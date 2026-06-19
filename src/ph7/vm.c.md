# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6721/8599 lines (78.16%)

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
|        - |  7757 | `							/* Use the returned map, not pBacking->x.pOther: PH7_HashmapDup` |
|        - |  7758 | `							 * inside CowSeparate can reallocate (move) pVm->aMemObj and leave` |
|        - |  7759 | `							 * pBacking dangling. The return value is the post-separation map. */` |
|        9 |  7760 | `							pTos->x.pOther = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  7761 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  7762 | `						}` |
|        4 |  7763 | `					}` |
|        4 |  7764 | `				}` |
|    12330 |  7765 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7766 | `				/* Reset the internal loop cursor */` |
|    12330 |  7767 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7768 | `				/* Mark the step */` |
|    12330 |  7769 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    12330 |  7770 | `				pStep->xIter.pMap = pMap;` |
|    12330 |  7771 | `				pMap->iRef++;` |
|     6166 |  7772 | `			}else{` |
|       36 |  7773 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7774 | `				ph7_class *pIteratorClass;` |
|        - |  7775 | `				/* Check if the object implements Iterator */` |
|       36 |  7776 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       47 |  7777 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  7778 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  7779 | `					ph7_class_method *pRewind;` |
|       24 |  7780 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  7781 | `					pStep->xIter.pThis = pThis;` |
|       24 |  7782 | `					pThis->iRef++;` |
|       24 |  7783 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  7784 | `					if( pRewind ){` |
|       24 |  7785 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  7786 | `					}` |
|       13 |  7787 | `				}else{` |
|        - |  7788 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  7789 | `					ph7_class *pIterAggClass;` |
|       14 |  7790 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  7791 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       15 |  7792 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  7793 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  7794 | `						ph7_class_method *pGetIter;` |
|        3 |  7795 | `						int iterAggOk = 0;` |
|        3 |  7796 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  7797 | `						if( pGetIter ){` |
|        - |  7798 | `							ph7_value sResult;` |
|        3 |  7799 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  7800 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  7801 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  7802 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  7803 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  7804 | `									ph7_class_method *pRewind;` |
|        3 |  7805 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  7806 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  7807 | `									pIterObj->iRef++;` |
|        - |  7808 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  7809 | `									pStep->pOwner = pThis;` |
|        3 |  7810 | `									pThis->iRef++;` |
|        3 |  7811 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  7812 | `									if( pRewind ){` |
|        3 |  7813 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  7814 | `									}` |
|        3 |  7815 | `									iterAggOk = 1;` |
|        1 |  7816 | `								}` |
|        1 |  7817 | `							}` |
|        3 |  7818 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  7819 | `						}` |
|        3 |  7820 | `						if( !iterAggOk ){` |
|        - |  7821 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  7822 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7823 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  7824 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  7825 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  7826 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  7827 | `						}` |
|        2 |  7828 | `					}else{` |
|        - |  7829 | `						/* Plain object iteration via hAttr */` |
|       12 |  7830 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|       12 |  7831 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|       12 |  7832 | `						pStep->xIter.pThis = pThis;` |
|       12 |  7833 | `						pThis->iRef++;` |
|        - |  7834 | `					}` |
|        - |  7835 | `				}` |
|        - |  7836 | `			}` |
|        - |  7837 | `		}` |
|    12364 |  7838 | `		if( pStep ){` |
|    12364 |  7839 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  7840 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  7841 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  7842 | `				/* Jump out of the loop */` |
|      ! 0 |  7843 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  7844 | `			}` |
|     6181 |  7845 | `		}` |
|        - |  7846 | `	}` |
|    12364 |  7847 | `	VmPopOperand(&pTos,1);` |
|    12364 |  7848 | `	break;` |
|        - |  7849 | `						  }` |
|        - |  7850 | `/*` |
|        - |  7851 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  7852 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  7853 | ` */` |
|   101558 |  7854 | `case PH7_OP_FOREACH_STEP: {` |
|   203118 |  7855 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7856 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  7857 | `	ph7_value *pValue;` |
|        - |  7858 | `	VmFrame *pFrameLocal;` |
|        - |  7859 | `	/* Peek the last step */` |
|   203118 |  7860 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   203118 |  7861 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   203118 |  7862 | `	pFrameLocal = pVm->pFrame;` |
|   203118 |  7863 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   203118 |  7864 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   202984 |  7865 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  7866 | `		ph7_hashmap_node *pNode;` |
|        - |  7867 | `		/* Extract the current node value */` |
|   202984 |  7868 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   202984 |  7869 | `		if( pNode == 0 ){` |
|        - |  7870 | `			/* No more entry to process */` |
|    12328 |  7871 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    12328 |  7872 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7873 | `				/* Break the reference with the last element */` |
|        7 |  7874 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  7875 | `			}` |
|        - |  7876 | `			/* Automatically reset the loop cursor */` |
|    12328 |  7877 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7878 | `			/* Cleanup the mess left behind */` |
|    12328 |  7879 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    12328 |  7880 | `			SySetPop(&pInfo->aStep);` |
|    12328 |  7881 | `			PH7_HashmapUnref(pMap);` |
|     6165 |  7882 | `		}else{` |
|   190658 |  7883 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      528 |  7884 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      528 |  7885 | `				if( pKey ){` |
|      528 |  7886 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      263 |  7887 | `				}` |
|      263 |  7888 | `			}` |
|   190658 |  7889 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7890 | `				SyHashEntry *pEntry;` |
|        - |  7891 | `				/* Pass by reference */` |
|       23 |  7892 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  7893 | `				if( pEntry ){` |
|       21 |  7894 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  7895 | `				}else{` |
|        4 |  7896 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  7897 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  7898 | `				}` |
|       12 |  7899 | `			}else{` |
|        - |  7900 | `				/* Make a copy of the entry value */` |
|   190636 |  7901 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   190636 |  7902 | `				if( pValue ){` |
|   190636 |  7903 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    95317 |  7904 | `				}` |
|        - |  7905 | `			}` |
|        2 |  7906 | `		}` |
|   101627 |  7907 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  7908 | `		/* Iterator-based iteration.` |
|        - |  7909 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  7910 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  7911 | `		 */` |
|      106 |  7912 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  7913 | `		ph7_class_method *pMethod;` |
|        - |  7914 | `		ph7_value sResult;` |
|      106 |  7915 | `		int isValid = 0;` |
|        - |  7916 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  7917 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  7918 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  7919 | `		}else{` |
|       82 |  7920 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  7921 | `			if( pMethod ){` |
|       82 |  7922 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  7923 | `			}` |
|        - |  7924 | `		}` |
|        - |  7925 | `		/* Call valid() */` |
|      106 |  7926 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  7927 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  7928 | `		if( pMethod ){` |
|      106 |  7929 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  7930 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  7931 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  7932 | `		}` |
|      106 |  7933 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  7934 | `		if( !isValid ){` |
|        - |  7935 | `			/* Iterator exhausted */` |
|       24 |  7936 | `			pc = pInstr->iP2 - 1;` |
|        - |  7937 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  7938 | `			if( pStep->pOwner ){` |
|        3 |  7939 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  7940 | `			}` |
|       24 |  7941 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  7942 | `			SySetPop(&pInfo->aStep);` |
|       24 |  7943 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  7944 | `		}else{` |
|        - |  7945 | `			/* Call current() to get value */` |
|       84 |  7946 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  7947 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  7948 | `			if( pMethod ){` |
|       84 |  7949 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  7950 | `			}` |
|       84 |  7951 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  7952 | `			if( pValue ){` |
|       84 |  7953 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  7954 | `			}` |
|       84 |  7955 | `			PH7_MemObjRelease(&sResult);` |
|        - |  7956 | `			/* Call key() if needed */` |
|       84 |  7957 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  7958 | `				ph7_value sKey;` |
|       35 |  7959 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  7960 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  7961 | `				if( pMethod ){` |
|       35 |  7962 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  7963 | `				}` |
|       35 |  7964 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  7965 | `				if( pValue ){` |
|       35 |  7966 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  7967 | `				}` |
|       35 |  7968 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  7969 | `			}` |
|        - |  7970 | `		}` |
|       54 |  7971 | `	}else{` |
|       32 |  7972 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       32 |  7973 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  7974 | `		SyHashEntry *pEntry;` |
|        - |  7975 | `		/* Point to the next attribute */` |
|       36 |  7976 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       26 |  7977 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  7978 | `			/* Check access permission */` |
|       38 |  7979 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       24 |  7980 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       22 |  7981 | `					break; /* Access is granted */` |
|        - |  7982 | `			}` |
|        1 |  7983 | `		}` |
|       32 |  7984 | `		if( pEntry == 0 ){` |
|        - |  7985 | `			/* Clean up the mess left behind */` |
|       12 |  7986 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|       12 |  7987 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7988 | `				/* Break the reference with the last element */` |
|        3 |  7989 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  7990 | `			}` |
|       12 |  7991 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       12 |  7992 | `			SySetPop(&pInfo->aStep);` |
|       12 |  7993 | `			PH7_ClassInstanceUnref(pThis);` |
|        7 |  7994 | `		}else{` |
|       22 |  7995 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  7996 | `			ph7_value *pAttrValue;` |
|       22 |  7997 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  7998 | `				/* Fill with the current attribute name */` |
|       22 |  7999 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       22 |  8000 | `				if( pKey ){` |
|       22 |  8001 | `					SyBlobReset(&pKey->sBlob);` |
|       22 |  8002 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       22 |  8003 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|       10 |  8004 | `				}` |
|       10 |  8005 | `			}` |
|        - |  8006 | `			/* Extract attribute value */` |
|       22 |  8007 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       22 |  8008 | `			if( pAttrValue ){` |
|       22 |  8009 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  8010 | `					/* Pass by reference */` |
|        3 |  8011 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  8012 | `					if( pEntry ){` |
|        3 |  8013 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  8014 | `					}else{` |
|      ! 0 |  8015 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  8016 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  8017 | `					}` |
|        2 |  8018 | `				}else{` |
|        - |  8019 | `					/* Make a copy of the attribute value */` |
|       20 |  8020 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       20 |  8021 | `					if( pValue ){` |
|       20 |  8022 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        9 |  8023 | `					}` |
|        - |  8024 | `				}` |
|       10 |  8025 | `			}` |
|        - |  8026 | `		}` |
|        - |  8027 | `	}` |
|   203118 |  8028 | `	break;` |
|        - |  8029 | `						  }` |
|        - |  8030 | `/*` |
|        - |  8031 | ` * OP_MEMBER P1 P2` |
|        - |  8032 | ` * Load class attribute/method on the stack.` |
|        - |  8033 | ` */` |
|     4051 |  8034 | `case PH7_OP_MEMBER: {` |
|        - |  8035 | `	ph7_class_instance *pThis;` |
|        - |  8036 | `	ph7_value *pNos;` |
|        - |  8037 | `	SyString sName;` |
|     8104 |  8038 | `	if( !pInstr->iP1 ){` |
|     7864 |  8039 | `		pNos = &pTos[-1];` |
|        - |  8040 | `#ifdef UNTRUST` |
|        - |  8041 | `		if( pNos < pStack ){` |
|        - |  8042 | `			goto Abort;` |
|        - |  8043 | `		}` |
|        - |  8044 | `#endif` |
|     7864 |  8045 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8046 | `			ph7_class *pClass;` |
|        - |  8047 | `			/* Class already instantiated */` |
|     7862 |  8048 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  8049 | `			/* Point to the instantiated class */` |
|     7862 |  8050 | `			pClass = pThis->pClass;` |
|        - |  8051 | `			/* Extract attribute name first */` |
|     7862 |  8052 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     7862 |  8053 | `			if( pInstr->iP2 ){` |
|        - |  8054 | `				/* Method call */` |
|      786 |  8055 | `				ph7_class_method *pMeth = 0;` |
|      786 |  8056 | `				if( sName.nByte > 0 ){` |
|        - |  8057 | `					/* Extract the target method */` |
|      786 |  8058 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      392 |  8059 | `				}` |
|      786 |  8060 | `				if( pMeth == 0 ){` |
|      ! 0 |  8061 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  8062 | `						&pClass->sName,&sName` |
|        - |  8063 | `						);` |
|        - |  8064 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  8065 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  8066 | `					/* Pop the method name from the stack */` |
|      ! 0 |  8067 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  8068 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  8069 | `				}else{` |
|        - |  8070 | `					/* Push method name on the stack */` |
|      786 |  8071 | `					PH7_MemObjRelease(pTos);` |
|      786 |  8072 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      786 |  8073 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  8074 | `				}` |
|      786 |  8075 | `				pTos->nIdx = SXU32_HIGH;` |
|      394 |  8076 | `			}else{` |
|        - |  8077 | `				/* Attribute access */` |
|     7078 |  8078 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  8079 | `				SyHashEntry *pEntry;` |
|        - |  8080 | `				/* Extract the target attribute */` |
|     7078 |  8081 | `				if( sName.nByte > 0 ){` |
|     7078 |  8082 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     7078 |  8083 | `					if( pEntry ){` |
|        - |  8084 | `						/* Point to the attribute value */` |
|     7076 |  8085 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     3537 |  8086 | `					}` |
|     3538 |  8087 | `				}` |
|     7078 |  8088 | `				if( pObjAttr == 0 ){` |
|        - |  8089 | `					/* No such attribute,load null */` |
|        4 |  8090 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  8091 | `						&pClass->sName,&sName);` |
|        - |  8092 | `					/* Call the __get magic method if available */` |
|        3 |  8093 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  8094 | `				}` |
|     7078 |  8095 | `				VmPopOperand(&pTos,1);` |
|        - |  8096 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  8097 | `				 * This is due to the following case:` |
|        - |  8098 | `				 *     (new TestClass())->foo;` |
|        - |  8099 | `				 */` |
|     7078 |  8100 | `				pThis->iRef++;` |
|     7078 |  8101 | `				PH7_MemObjRelease(pTos);` |
|     7078 |  8102 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     7078 |  8103 | `				if( pObjAttr ){` |
|     7076 |  8104 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  8105 | `					/* Check attribute access */` |
|     7076 |  8106 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  8107 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  8108 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  8109 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  8110 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  8111 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     7074 |  8112 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     3579 |  8113 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       82 |  8114 | `							VmInstr *pNext = pInstr + 1;` |
|       82 |  8115 | `							int bIsLhs = 0;` |
|       82 |  8116 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       80 |  8117 | `								bIsLhs = 1;` |
|       39 |  8118 | `							}` |
|       82 |  8119 | `							if( !bIsLhs ){` |
|        3 |  8120 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  8121 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  8122 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  8123 | `									goto Abort;` |
|        - |  8124 | `								}` |
|        - |  8125 | `								{` |
|        3 |  8126 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  8127 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  8128 | `										pc = pFrm2->iExceptionJump - 1;` |
|     4051 |  8129 | `										break;` |
|        - |  8130 | `									}` |
|        - |  8131 | `								}` |
|      ! 0 |  8132 | `								goto Exception;` |
|        - |  8133 | `							}` |
|       39 |  8134 | `						}` |
|        - |  8135 | `						/* Load attribute */` |
|     7074 |  8136 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     7074 |  8137 | `						if( pValue ){` |
|     7074 |  8138 | `							if( pThis->iRef < 2 ){` |
|        - |  8139 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  8140 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  8141 | `								 */` |
|        7 |  8142 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  8143 | `							}else{` |
|        - |  8144 | `								/* Simple load */` |
|     7068 |  8145 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  8146 | `							}` |
|     7074 |  8147 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     7072 |  8148 | `								if( pThis->iRef > 1 ){` |
|        - |  8149 | `									/* Load attribute index */` |
|     7066 |  8150 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     3532 |  8151 | `								}` |
|     3535 |  8152 | `							}` |
|     3536 |  8153 | `						}` |
|     3538 |  8154 | `					}else{` |
|        - |  8155 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  8156 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  8157 | `						char zMsg[256];` |
|      ! 0 |  8158 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  8159 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  8160 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  8161 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  8162 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8163 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  8164 | `						goto Abort;` |
|        - |  8165 | `					}` |
|     3536 |  8166 | `				}` |
|        - |  8167 | `				/* Safely unreference the object */` |
|     7076 |  8168 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  8169 | `			}` |
|     3931 |  8170 | `		}else{` |
|        3 |  8171 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  8172 | `			VmPopOperand(&pTos,1);` |
|        3 |  8173 | `			PH7_MemObjRelease(pTos);` |
|        3 |  8174 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  8175 | `		}` |
|     3932 |  8176 | `	}else{` |
|        - |  8177 | `		/* Static member access using class name */` |
|      242 |  8178 | `		pNos = pTos;` |
|      242 |  8179 | `		pThis = 0;` |
|      242 |  8180 | `		if( !pInstr->p3 ){` |
|      192 |  8181 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      192 |  8182 | `			pNos--;` |
|        - |  8183 | `#ifdef UNTRUST` |
|        - |  8184 | `			if( pNos < pStack ){` |
|        - |  8185 | `				goto Abort;` |
|        - |  8186 | `			}` |
|        - |  8187 | `#endif` |
|       97 |  8188 | `		}else{` |
|        - |  8189 | `			/* Attribute name already computed */` |
|       52 |  8190 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  8191 | `		}` |
|      242 |  8192 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      242 |  8193 | `			ph7_class *pClass = 0;` |
|      242 |  8194 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8195 | `				/* Class already instantiated */` |
|        5 |  8196 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  8197 | `				pClass = pThis->pClass;` |
|        5 |  8198 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  8199 | `			}else{` |
|        - |  8200 | `				/* Try to extract the target class */` |
|      238 |  8201 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      238 |  8202 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      238 |  8203 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  8204 | `					/* Handle self/static/parent keywords */` |
|      238 |  8205 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  8206 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  8207 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  8208 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  8209 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  8210 | `						}` |
|      208 |  8211 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  8212 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      178 |  8213 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       28 |  8214 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       28 |  8215 | `						if( pSelf && pSelf->pBase ){` |
|       28 |  8216 | `							pClass = pSelf->pBase;` |
|       13 |  8217 | `						}` |
|       15 |  8218 | `					}else{` |
|      126 |  8219 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  8220 | `					}` |
|      118 |  8221 | `				}` |
|        - |  8222 | `			}` |
|      242 |  8223 | `			if( pClass == 0 ){` |
|        - |  8224 | `				/* Undefined class */` |
|      ! 0 |  8225 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  8226 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  8227 | `					);` |
|      ! 0 |  8228 | `				if( !pInstr->p3 ){` |
|      ! 0 |  8229 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  8230 | `				}` |
|      ! 0 |  8231 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  8232 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  8233 | `			}else{` |
|      242 |  8234 | `				if( pInstr->iP2 ){` |
|        - |  8235 | `					/* Method call */` |
|       86 |  8236 | `					ph7_class_method *pMeth = 0;` |
|       86 |  8237 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  8238 | `						/* Extract the target method */` |
|       86 |  8239 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  8240 | `					}` |
|       86 |  8241 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  8242 | `						if( pMeth ){` |
|      ! 0 |  8243 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  8244 | `								&pClass->sName,&sName` |
|        - |  8245 | `								);` |
|      ! 0 |  8246 | `						}else{` |
|      ! 0 |  8247 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8248 | `								&pClass->sName,&sName` |
|        - |  8249 | `								);` |
|        - |  8250 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  8251 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  8252 | `						}` |
|        - |  8253 | `						/* Pop the method name from the stack */` |
|      ! 0 |  8254 | `						if( !pInstr->p3 ){` |
|      ! 0 |  8255 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  8256 | `						}` |
|      ! 0 |  8257 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  8258 | `					}else{` |
|        - |  8259 | `						/* Push method name on the stack */` |
|       86 |  8260 | `						PH7_MemObjRelease(pTos);` |
|       86 |  8261 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       86 |  8262 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  8263 | `					}` |
|       86 |  8264 | `					pTos->nIdx = SXU32_HIGH;` |
|       44 |  8265 | `				}else{` |
|        - |  8266 | `					/* Attribute access */` |
|      158 |  8267 | `					ph7_class_attr *pAttr = 0;` |
|        - |  8268 | `					/* Check for special ::class pseudo-constant */` |
|      204 |  8269 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  8270 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  8271 | `						/* ::class returns the fully qualified class name */` |
|        - |  8272 | `						/* Pop the attribute name from the stack */` |
|       60 |  8273 | `						if( !pInstr->p3 ){` |
|       60 |  8274 | `							VmPopOperand(&pTos,1);` |
|       29 |  8275 | `						}` |
|       60 |  8276 | `						PH7_MemObjRelease(pTos);` |
|        - |  8277 | `						/* Load the class name */` |
|       60 |  8278 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  8279 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  8280 | `					}else{` |
|        - |  8281 | `						/* Extract the target attribute */` |
|      100 |  8282 | `						if( sName.nByte > 0 ){` |
|      100 |  8283 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       49 |  8284 | `						}` |
|      100 |  8285 | `						if( pAttr == 0 ){` |
|        - |  8286 | `							/* No such attribute,load null */` |
|      ! 0 |  8287 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8288 | `								&pClass->sName,&sName);` |
|        - |  8289 | `							/* Call the __get magic method if available */` |
|      ! 0 |  8290 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  8291 | `						}` |
|        - |  8292 | `						/* Pop the attribute name from the stack */` |
|      100 |  8293 | `						if( !pInstr->p3 ){` |
|       50 |  8294 | `							VmPopOperand(&pTos,1);` |
|       24 |  8295 | `						}` |
|      100 |  8296 | `						PH7_MemObjRelease(pTos);` |
|      100 |  8297 | `						pTos->nIdx = SXU32_HIGH;` |
|      100 |  8298 | `						if( pAttr ){` |
|      100 |  8299 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  8300 | `								/* Access to a non static attribute */` |
|      ! 0 |  8301 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  8302 | `									&pClass->sName,&pAttr->sName` |
|        - |  8303 | `									);` |
|      ! 0 |  8304 | `							}else{` |
|        - |  8305 | `								ph7_value *pValue;` |
|        - |  8306 | `								/* Check if the access to the attribute is allowed */` |
|      100 |  8307 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  8308 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  8309 | `									 * Same LHS-of-store peek as the instance path. */` |
|       94 |  8310 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       68 |  8311 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       59 |  8312 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       38 |  8313 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       40 |  8314 | `										if( pS ){` |
|       40 |  8315 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       40 |  8316 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  8317 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  8318 | `												int bIsLhs = 0;` |
|        8 |  8319 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  8320 | `													bIsLhs = 1;` |
|        2 |  8321 | `												}` |
|        8 |  8322 | `												if( !bIsLhs ){` |
|        3 |  8323 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  8324 | `													if( pThis ){` |
|      ! 0 |  8325 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8326 | `													}` |
|        3 |  8327 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  8328 | `														goto Abort;` |
|        - |  8329 | `													}` |
|        - |  8330 | `													{` |
|        3 |  8331 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  8332 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  8333 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  8334 | `															break;` |
|        - |  8335 | `														}` |
|        - |  8336 | `													}` |
|      ! 0 |  8337 | `													goto Exception;` |
|        - |  8338 | `												}` |
|        2 |  8339 | `											}` |
|       18 |  8340 | `										}` |
|       18 |  8341 | `									}` |
|        - |  8342 | `									/* Load the desired attribute */` |
|       94 |  8343 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       94 |  8344 | `									if( pValue ){` |
|       94 |  8345 | `										PH7_MemObjLoad(pValue,pTos);` |
|       94 |  8346 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  8347 | `											/* Load index number */` |
|       50 |  8348 | `											pTos->nIdx = pAttr->nIdx;` |
|       24 |  8349 | `										}` |
|       46 |  8350 | `									}` |
|       48 |  8351 | `								}else{` |
|        - |  8352 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  8353 | `									char zMsg[256];` |
|        5 |  8354 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  8355 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  8356 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  8357 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  8358 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  8359 | `									}else{` |
|      ! 0 |  8360 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  8361 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  8362 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  8363 | `									}` |
|        5 |  8364 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  8365 | `									goto Abort;` |
|        - |  8366 | `								}` |
|        - |  8367 | `							}` |
|       46 |  8368 | `						}` |
|        - |  8369 | `					}` |
|        - |  8370 | `				}` |
|      236 |  8371 | `				if( pThis ){` |
|        - |  8372 | `					/* Safely unreference the object */` |
|        5 |  8373 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  8374 | `				}` |
|        - |  8375 | `			}` |
|      119 |  8376 | `		}else{` |
|        - |  8377 | `			/* Pop operands */` |
|      ! 0 |  8378 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  8379 | `			if( !pInstr->p3 ){` |
|      ! 0 |  8380 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  8381 | `			}` |
|      ! 0 |  8382 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8383 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  8384 | `		}` |
|        - |  8385 | `	}` |
|     8096 |  8386 | `	break;` |
|        - |  8387 | `					}` |
|        - |  8388 | `/*` |
|        - |  8389 | ` * OP_NEW P1 * * *` |
|        - |  8390 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  8391 | ` */` |
|      664 |  8392 | `case PH7_OP_NEW: {` |
|     1330 |  8393 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1330 |  8394 | `	ph7_class *pClass = 0;` |
|        - |  8395 | `	ph7_class_instance *pNew;` |
|     1330 |  8396 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  8397 | `		/* Try to extract the desired class */` |
|     1994 |  8398 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1328 |  8399 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      664 |  8400 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  8401 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  8402 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  8403 | `	}` |
|     1330 |  8404 | `	if( pClass == 0 ){` |
|        - |  8405 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  8406 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  8407 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  8408 | `			);` |
|        - |  8409 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  8410 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8411 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8412 | `			/* Pop given arguments */` |
|      ! 0 |  8413 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8414 | `		}` |
|      ! 0 |  8415 | `		goto Abort;` |
|      ! 0 |  8416 | `	}else{` |
|        - |  8417 | `		ph7_class_method *pCons;` |
|        - |  8418 | `		/* Create a new class instance */` |
|     1330 |  8419 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1330 |  8420 | `		if( pNew == 0 ){` |
|      ! 0 |  8421 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8422 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  8423 | `				&pClass->sName` |
|        - |  8424 | `			);` |
|      ! 0 |  8425 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8426 | `			if( pInstr->iP1 > 0 ){` |
|        - |  8427 | `				/* Pop given arguments */` |
|      ! 0 |  8428 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8429 | `			}` |
|      ! 0 |  8430 | `			break;` |
|        - |  8431 | `		}` |
|        - |  8432 | `		/* Check if a constructor is available */` |
|     1330 |  8433 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1330 |  8434 | `		if( pCons == 0 ){` |
|      934 |  8435 | `			SyString *pName = &pClass->sName;` |
|        - |  8436 | `			/* Check for a constructor with the same base class name */` |
|      934 |  8437 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      466 |  8438 | `		}` |
|     1330 |  8439 | `		if( pCons ){` |
|        - |  8440 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  8441 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  8442 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  8443 | `			 * (including variadic string-key packing). */` |
|      398 |  8444 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|        - |  8445 | `			sxi32 rcCons;` |
|      398 |  8446 | `			SySetReset(&aArg);` |
|      778 |  8447 | `			while( pArg < pTos ){` |
|      382 |  8448 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      382 |  8449 | `				pArg++;` |
|        2 |  8450 | `			}` |
|      398 |  8451 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  8452 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  8453 | `				sxu32 n;` |
|      114 |  8454 | `				n = SySetUsed(&aArg);` |
|        - |  8455 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  8456 | `				 * for named args the missing-arg check happens downstream` |
|        - |  8457 | `				 * after resolution). */` |
|      222 |  8458 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|      110 |  8459 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|      110 |  8460 | `					if( pFuncArg ){` |
|      110 |  8461 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  8462 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  8463 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  8464 | `						}` |
|       54 |  8465 | `					}` |
|      110 |  8466 | `					n++;` |
|        2 |  8467 | `				}` |
|       56 |  8468 | `			}` |
|      398 |  8469 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  8470 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      398 |  8471 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  8472 | `				pNew->iRef = 1;` |
|      ! 0 |  8473 | `			}` |
|      398 |  8474 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|        - |  8475 | `				/* The constructor raised: the half-constructed object must not` |
|        - |  8476 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|        - |  8477 | `				 * The class-name operand (and any leftover args) are released by` |
|        - |  8478 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|        - |  8479 | `				sxi32 iResumePc;` |
|        5 |  8480 | `				PH7_ClassInstanceUnref(pNew);` |
|        5 |  8481 | `				if( rcCons == PH7_ABORT ){` |
|      ! 0 |  8482 | `					goto Abort;` |
|        - |  8483 | `				}` |
|        5 |  8484 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        - |  8485 | `					/* This frame's own try caught it in-place: tidy the stack` |
|        - |  8486 | `					 * (pop ctor args + release the class-name slot) and resume. */` |
|        5 |  8487 | `					if( pInstr->iP1 > 0 ){` |
|        3 |  8488 | `						VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  8489 | `					}` |
|        5 |  8490 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8491 | `					pc = iResumePc;` |
|        5 |  8492 | `					break;` |
|        - |  8493 | `				}` |
|      ! 0 |  8494 | `				goto Exception;` |
|        - |  8495 | `			}` |
|      196 |  8496 | `		}` |
|     1326 |  8497 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8498 | `			/* Pop given arguments */` |
|      312 |  8499 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      155 |  8500 | `		}` |
|     1326 |  8501 | `		PH7_MemObjRelease(pTos);` |
|     1326 |  8502 | `		pTos->x.pOther = pNew;` |
|     1326 |  8503 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8504 | `	}` |
|     1326 |  8505 | `	break;` |
|        - |  8506 | `				 }` |
|        - |  8507 | `/*` |
|        - |  8508 | ` * OP_CLONE * * *` |
|        - |  8509 | ` * Perfome a clone operation.` |
|        - |  8510 | ` */` |
|       24 |  8511 | `case PH7_OP_CLONE: {` |
|        - |  8512 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  8513 | `#ifdef UNTRUST` |
|        - |  8514 | `	if( pTos < pStack ){` |
|        - |  8515 | `		goto Abort;` |
|        - |  8516 | `	}` |
|        - |  8517 | `#endif` |
|        - |  8518 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  8519 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  8520 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8521 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  8522 | `		PH7_MemObjRelease(pTos);` |
|        5 |  8523 | `		break;` |
|        - |  8524 | `	}` |
|        - |  8525 | `	/* Point to the source */` |
|       46 |  8526 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8527 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  8528 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  8529 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8530 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  8531 | `			&pSrc->pClass->sName);` |
|      ! 0 |  8532 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8533 | `		break;` |
|        - |  8534 | `	}` |
|        - |  8535 | `	/* Perform the clone operation */` |
|       46 |  8536 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  8537 | `	PH7_MemObjRelease(pTos);` |
|       46 |  8538 | `	if( pClone == 0 ){` |
|      ! 0 |  8539 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8540 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  8541 | `	}else{` |
|        - |  8542 | `		/* Load the cloned object */` |
|       46 |  8543 | `		pTos->x.pOther = pClone;` |
|       46 |  8544 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8545 | `	}` |
|       46 |  8546 | `	break;` |
|        - |  8547 | `				   }` |
|        - |  8548 | `/*` |
|        - |  8549 | ` * OP_SWITCH * * P3` |
|        - |  8550 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  8551 | ` */` |
|       26 |  8552 | `case PH7_OP_SWITCH: {` |
|       54 |  8553 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  8554 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  8555 | `	ph7_value sValue,sCaseValue;` |
|        - |  8556 | `	sxu32 n,nEntry;` |
|        - |  8557 | `#ifdef UNTRUST` |
|        - |  8558 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  8559 | `		goto Abort;` |
|        - |  8560 | `	}` |
|        - |  8561 | `#endif` |
|        - |  8562 | `	/* Point to the case table  */` |
|       54 |  8563 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  8564 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  8565 | `	/* Select the appropriate case block to execute */` |
|       54 |  8566 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  8567 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  8568 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  8569 | `		pCase = &aCase[n];` |
|      130 |  8570 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  8571 | `		/* Execute the case expression first */` |
|      130 |  8572 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  8573 | `		/* Compare the two expression */` |
|      130 |  8574 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  8575 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  8576 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  8577 | `		if( rc == 0 ){` |
|        - |  8578 | `			/* Value match,jump to this block */` |
|       52 |  8579 | `			pc = pCase->nStart - 1;` |
|       52 |  8580 | `			break;` |
|        - |  8581 | `		}` |
|       41 |  8582 | `	}` |
|       54 |  8583 | `	VmPopOperand(&pTos,1);` |
|       54 |  8584 | `	if( n >= nEntry ){` |
|        - |  8585 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  8586 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  8587 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  8588 | `		}else{` |
|        - |  8589 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  8590 | `			pc = pSwitch->nOut - 1;` |
|        - |  8591 | `		}` |
|        1 |  8592 | `	}` |
|       54 |  8593 | `	break;` |
|        - |  8594 | `					}` |
|        - |  8595 | `/*` |
|        - |  8596 | ` * OP_MATCH * * P3` |
|        - |  8597 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  8598 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  8599 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  8600 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  8601 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  8602 | ` */` |
|       54 |  8603 | `case PH7_OP_MATCH: {` |
|      110 |  8604 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      110 |  8605 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  8606 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  8607 | `	sxu32 i,j,nArm,nCond;` |
|      110 |  8608 | `	int matched = 0;` |
|        - |  8609 | `#ifdef UNTRUST` |
|        - |  8610 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  8611 | `		goto Abort;` |
|        - |  8612 | `	}` |
|        - |  8613 | `#endif` |
|      110 |  8614 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      110 |  8615 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      110 |  8616 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      110 |  8617 | `	PH7_MemObjInit(pVm,&sCond);` |
|      110 |  8618 | `	PH7_MemObjInit(pVm,&sResult);` |
|      110 |  8619 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      348 |  8620 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  8621 | `		pArm = &aArm[i];` |
|      240 |  8622 | `		if( pArm->bDefault ){` |
|       13 |  8623 | `			pDefault = pArm;` |
|       13 |  8624 | `			continue;` |
|        - |  8625 | `		}` |
|      228 |  8626 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  8627 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  8628 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  8629 | `			if( pCondBc == 0 ){` |
|      ! 0 |  8630 | `				continue;` |
|        - |  8631 | `			}` |
|      260 |  8632 | `			VmLocalExec(pVm,pCondBc,&sCond);` |
|      260 |  8633 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  8634 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  8635 | `			if( rc == 0 ){` |
|       93 |  8636 | `				VmLocalExec(pVm,&pArm->aResult,&sResult);` |
|       93 |  8637 | `				matched = 1;` |
|       93 |  8638 | `				break;` |
|        - |  8639 | `			}` |
|       85 |  8640 | `		}` |
|      115 |  8641 | `	}` |
|      110 |  8642 | `	if( !matched && pDefault ){` |
|       13 |  8643 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult);` |
|       13 |  8644 | `		matched = 1;` |
|        6 |  8645 | `	}` |
|      110 |  8646 | `	if( !matched ){` |
|        5 |  8647 | `		const char *zType = "unknown";` |
|        - |  8648 | `		char zMsg[128];` |
|        - |  8649 | `		sxu32 nMsg;` |
|        5 |  8650 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  8651 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  8652 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        5 |  8653 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  8654 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  8655 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  8656 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  8657 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  8658 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  8659 | `		default: break;` |
|        - |  8660 | `		}` |
|        7 |  8661 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  8662 | `			"Unhandled match case of type %s",zType);` |
|        7 |  8663 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  8664 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        5 |  8665 | `		PH7_MemObjRelease(&sSubject);` |
|        5 |  8666 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  8667 | `		goto Abort;` |
|        - |  8668 | `	}` |
|      105 |  8669 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  8670 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  8671 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  8672 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  8673 | `	break;` |
|        - |  8674 | `					}` |
|        - |  8675 | `/*` |
|        - |  8676 | ` * OP_YIELD P1 P2 *` |
|        - |  8677 | ` *  Yield a value from a generator function.` |
|        - |  8678 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  8679 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  8680 | ` */` |
|       34 |  8681 | `case PH7_OP_YIELD: {` |
|        - |  8682 | `	ph7_generator *pGen;` |
|       70 |  8683 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  8684 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  8685 | `		goto Abort;` |
|        - |  8686 | `	}` |
|       70 |  8687 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       70 |  8688 | `	if( pInstr->iP2 ){` |
|        - |  8689 | `		/* yield $key => $value: value on top, key below */` |
|        - |  8690 | `#ifdef UNTRUST` |
|        - |  8691 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  8692 | `#endif` |
|        7 |  8693 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  8694 | `		VmPopOperand(&pTos, 1);` |
|        7 |  8695 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  8696 | `		VmPopOperand(&pTos, 1);` |
|        - |  8697 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  8698 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  8699 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  8700 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  8701 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  8702 | `			}` |
|        1 |  8703 | `		}` |
|       67 |  8704 | `	}else if( pInstr->iP1 ){` |
|        - |  8705 | `		/* yield $value */` |
|        - |  8706 | `#ifdef UNTRUST` |
|        - |  8707 | `		if( pTos < pStack ) goto Abort;` |
|        - |  8708 | `#endif` |
|       64 |  8709 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       64 |  8710 | `		VmPopOperand(&pTos, 1);` |
|        - |  8711 | `		/* Auto-increment key */` |
|       64 |  8712 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       64 |  8713 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       64 |  8714 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       33 |  8715 | `	}else{` |
|        - |  8716 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  8717 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  8718 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  8719 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  8720 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  8721 | `	}` |
|        - |  8722 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       70 |  8723 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       70 |  8724 | `	goto Suspend;` |
|        - |  8725 |  |
|        - |  8726 | `/*` |
|        - |  8727 | ` * OP_CALL P1 * *` |
|        - |  8728 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  8729 | ` *  function on the stack.` |
|        - |  8730 | ` */` |
|   357792 |  8731 | `case PH7_OP_CALL: {` |
|   715630 |  8732 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  8733 | `	ph7_value *pArg;` |
|   715630 |  8734 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   715630 |  8735 | `	pArg = &pTos[-nCallArgs];` |
|        - |  8736 | `	SyHashEntry *pEntry;` |
|        - |  8737 | `	SyString sName;` |
|        - |  8738 | `	/* Extract function name */` |
|   715630 |  8739 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       86 |  8740 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8741 | `			ph7_value sResult;` |
|        - |  8742 | `			sxi32 rcArr;` |
|        3 |  8743 | `			SySetReset(&aArg);` |
|        3 |  8744 | `			while( pArg < pTos ){` |
|      ! 0 |  8745 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  8746 | `				pArg++;` |
|      ! 0 |  8747 | `			}` |
|        3 |  8748 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  8749 | `			/* May be a class instance and it's static method */` |
|        3 |  8750 | `			rcArr = PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|        3 |  8751 | `			SySetReset(&aArg);` |
|        - |  8752 | `			/* Pop given arguments */` |
|        3 |  8753 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8754 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8755 | `			}` |
|        3 |  8756 | `			if( rcArr == PH7_ABORT ){` |
|      ! 0 |  8757 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8758 | `				goto Abort;` |
|        - |  8759 | `			}` |
|        3 |  8760 | `			if( rcArr == PH7_EXCEPTION ){` |
|        - |  8761 | `				/* An array callable ([$obj,'m']()) raised: resume after this frame's` |
|        - |  8762 | `				 * try if it caught the exception in-place, otherwise propagate. */` |
|        - |  8763 | `				sxi32 iResumePc;` |
|        3 |  8764 | `				PH7_MemObjRelease(&sResult);` |
|        3 |  8765 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        3 |  8766 | `					PH7_MemObjRelease(pTos);` |
|        3 |  8767 | `					pc = iResumePc;` |
|        3 |  8768 | `					break;` |
|        - |  8769 | `				}` |
|      ! 0 |  8770 | `				goto Exception;` |
|        - |  8771 | `			}` |
|        - |  8772 | `			/* Copy result */` |
|      ! 0 |  8773 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  8774 | `			PH7_MemObjRelease(&sResult);` |
|       84 |  8775 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       84 |  8776 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8777 | `			ph7_value sResult;` |
|        - |  8778 | `			sxi32 rcInv;` |
|       84 |  8779 | `			SySetReset(&aArg);` |
|      200 |  8780 | `			while( pArg < pTos ){` |
|      118 |  8781 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      118 |  8782 | `				pArg++;` |
|        2 |  8783 | `			}` |
|       84 |  8784 | `			PH7_MemObjInit(pVm,&sResult);` |
|      125 |  8785 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|       82 |  8786 | `				(int)SySetUsed(&aArg),` |
|       82 |  8787 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - |  8788 | `				&sResult,` |
|       82 |  8789 | `				(VmCallArgMap *)pInstr->p3);` |
|       84 |  8790 | `			SySetReset(&aArg);` |
|       84 |  8791 | `			if( nCallArgs > 0 ){` |
|       76 |  8792 | `				VmPopOperand(&pTos,nCallArgs);` |
|       37 |  8793 | `			}` |
|       84 |  8794 | `			if( rcInv == SXERR_INVALID ){` |
|        - |  8795 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - |  8796 | `				 * sResult was already released by VmCallObjectInvoke.` |
|        - |  8797 | `				 * Pin pThis: the release below would otherwise drop the last ref` |
|        - |  8798 | `				 * to a temporary callable like (new Plain())(...), freeing the` |
|        - |  8799 | `				 * instance before VmRaiseNotCallable reads its class name. */` |
|       13 |  8800 | `				pThis->iRef++;` |
|       13 |  8801 | `				PH7_MemObjRelease(pTos);` |
|       13 |  8802 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|       13 |  8803 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 |  8804 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8805 | `					goto Abort;` |
|        - |  8806 | `				}` |
|        - |  8807 | `				{` |
|       13 |  8808 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       12 |  8809 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION)` |
|       13 |  8810 | `					 && pFrm2->iExceptionJump > 0 ){` |
|       13 |  8811 | `						pc = pFrm2->iExceptionJump - 1;` |
|       15 |  8812 | `						break;` |
|        - |  8813 | `					}` |
|        - |  8814 | `				}` |
|      ! 0 |  8815 | `				goto Exception;` |
|        - |  8816 | `			}` |
|       72 |  8817 | `			if( rcInv == PH7_ABORT ){` |
|      ! 0 |  8818 | `				PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8819 | `				goto Abort;` |
|        - |  8820 | `			}` |
|       72 |  8821 | `			if( rcInv == PH7_EXCEPTION ){` |
|        - |  8822 | `				/* __invoke raised. The catch body (if any) already ran in-place` |
|        - |  8823 | `				 * inside VmThrowException. If THIS frame's own try caught it,` |
|        - |  8824 | `				 * resume after the try/catch; otherwise propagate so the` |
|        - |  8825 | `				 * exception unwinds through intermediate frames with no handler. */` |
|        - |  8826 | `				sxi32 iResumePc;` |
|        7 |  8827 | `				PH7_MemObjRelease(&sResult);` |
|        7 |  8828 | `				if( VmCalleeExceptionResume(pVm,&iResumePc) ){` |
|        5 |  8829 | `					PH7_MemObjRelease(pTos);` |
|        5 |  8830 | `					pc = iResumePc;` |
|        5 |  8831 | `					break;` |
|        - |  8832 | `				}` |
|        3 |  8833 | `				goto Exception;` |
|        - |  8834 | `			}` |
|       66 |  8835 | `			PH7_MemObjStore(&sResult,pTos);` |
|       66 |  8836 | `			PH7_MemObjRelease(&sResult);` |
|       34 |  8837 | `		}else{` |
|        - |  8838 | `			/* Raise exception: Invalid function name */` |
|      ! 0 |  8839 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  8840 | `			/* Pop given arguments */` |
|      ! 0 |  8841 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8842 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8843 | `			}` |
|        - |  8844 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8845 | `			PH7_MemObjRelease(pTos);` |
|        - |  8846 | `		}` |
|       66 |  8847 | `		break;` |
|        - |  8848 | `	}` |
|   715546 |  8849 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  8850 | `	/* Check for a compiled function first.` |
|        - |  8851 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  8852 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   715546 |  8853 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  8854 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  8855 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  8856 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  8857 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  8858 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  8859 | `	{` |
|   715546 |  8860 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   715546 |  8861 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  8862 | `		const char *zFunc;` |
|        - |  8863 | `		const char *zEnd;` |
|        - |  8864 | `		const char *z;` |
|        - |  8865 | `		SyString sGlobal;` |
|       22 |  8866 | `		zFunc = sName.zString;` |
|       22 |  8867 | `		zEnd  = zFunc + sName.nByte;` |
|       22 |  8868 | `		z = zEnd;` |
|        - |  8869 | `		/* Find last namespace separator */` |
|      194 |  8870 | `		while( z > zFunc ){` |
|      194 |  8871 | `			if( z[-1] == '\\' ){` |
|       22 |  8872 | `				break;` |
|        - |  8873 | `			}` |
|      174 |  8874 | `			z--;` |
|        2 |  8875 | `		}` |
|       22 |  8876 | `		if( z > zFunc && z < zEnd ){` |
|        - |  8877 | `			/* Retry lookup using the unqualified/global function name */` |
|       22 |  8878 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       22 |  8879 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  8880 | `		}` |
|       10 |  8881 | `	}` |
|        - |  8882 | `	} /* end VmCallArgMap namespace scope */` |
|   715546 |  8883 | `	if( pEntry ){` |
|        - |  8884 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  8885 | `		ph7_class_instance *pThis;` |
|        - |  8886 | `		ph7_value *pFrameStack;` |
|        - |  8887 | `		ph7_vm_func *pVmFunc;` |
|        - |  8888 | `		ph7_class *pSelf;` |
|        - |  8889 | `		VmFrame *pFrame;` |
|        - |  8890 | `		ph7_value *pObj;` |
|        - |  8891 | `		VmSlot sArg;` |
|        - |  8892 | `		sxu32 n;` |
|        - |  8893 | `		/* initialize fields */` |
|    18546 |  8894 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    18546 |  8895 | `		pThis = 0;` |
|    18546 |  8896 | `		pSelf = 0;` |
|    18546 |  8897 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  8898 | `			ph7_class_method *pMeth;` |
|        - |  8899 | `			/* Class method call */` |
|     3352 |  8900 | `			ph7_value *pTarget = &pTos[-1];` |
|     3352 |  8901 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  8902 | `				/* Extract the 'this' pointer */` |
|     3352 |  8903 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  8904 | `					/* Instance already loaded */` |
|     3262 |  8905 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     3262 |  8906 | `					pThis->iRef++;` |
|     3262 |  8907 | `					pSelf = pThis->pClass;` |
|     1630 |  8908 | `				}` |
|     3352 |  8909 | `				if( pSelf == 0 ){` |
|       92 |  8910 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  8911 | `						/* "Late Static Binding" class name */` |
|      128 |  8912 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  8913 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  8914 | `					}` |
|       92 |  8915 | `					if( pSelf == 0 ){` |
|       21 |  8916 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  8917 | `					}` |
|       45 |  8918 | `				}` |
|     3352 |  8919 | `				if( pThis == 0  ){` |
|       92 |  8920 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       92 |  8921 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       92 |  8922 | `					if( pFrameLocal->pParent ){` |
|        - |  8923 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       66 |  8924 | `						pThis = pFrameLocal->pThis;` |
|       66 |  8925 | `						if( pThis ){` |
|       21 |  8926 | `							pThis->iRef++;` |
|       10 |  8927 | `						}` |
|       32 |  8928 | `					}` |
|       45 |  8929 | `				}` |
|     3352 |  8930 | `				VmPopOperand(&pTos,1);` |
|     3352 |  8931 | `				PH7_MemObjRelease(pTos);` |
|        - |  8932 | `				/* Synchronize pointers */` |
|     3352 |  8933 | `				pArg = &pTos[-nCallArgs];` |
|        - |  8934 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  8935 | `				 * user have already computed the random generated unique class method name` |
|        - |  8936 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  8937 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  8938 | `				 */` |
|     3352 |  8939 | `				while( pArg < pStack ){` |
|      ! 0 |  8940 | `					pArg++;` |
|      ! 0 |  8941 | `				}` |
|     3352 |  8942 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  8943 | `					/* Check if the call is allowed */` |
|     3352 |  8944 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     3352 |  8945 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  8946 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  8947 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  8948 | `							char zMsg[256];` |
|      ! 0 |  8949 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  8950 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  8951 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  8952 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  8953 | `							/* Pop given arguments */` |
|      ! 0 |  8954 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  8955 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8956 | `							}` |
|      ! 0 |  8957 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  8958 | `							goto Abort;` |
|        - |  8959 | `						}` |
|        6 |  8960 | `					}` |
|     1675 |  8961 | `				}` |
|     1675 |  8962 | `			}` |
|     1675 |  8963 | `		}` |
|        - |  8964 | `		/* Check The recursion limit */` |
|    18546 |  8965 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  8966 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8967 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  8968 | `				&pVmFunc->sName);` |
|        - |  8969 | `			/* Pop given arguments */` |
|        3 |  8970 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8971 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8972 | `			}` |
|        - |  8973 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  8974 | `			PH7_MemObjRelease(pTos);` |
|       14 |  8975 | `			break;` |
|        - |  8976 | `		}` |
|    18544 |  8977 | `		if( pVmFunc->pNextName ){` |
|        - |  8978 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  8979 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  8980 | `		}` |
|    18544 |  8981 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  8982 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  8983 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  8984 | `			ph7_generator *pGenerator;` |
|        - |  8985 | `			ph7_class_instance *pGenObj;` |
|        - |  8986 | `			ph7_value *pCtxAttr;` |
|        - |  8987 | `			SyString sAttrName;` |
|        - |  8988 | `			ph7_value **apCallArgs;` |
|        - |  8989 | `			int nGenArgs, iArg;` |
|        - |  8990 | `			/* Collect arguments from the operand stack */` |
|       24 |  8991 | `			nGenArgs = (int)(pTos - pArg);` |
|       24 |  8992 | `			apCallArgs = 0;` |
|       24 |  8993 | `			if( nGenArgs > 0 ){` |
|       14 |  8994 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  8995 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  8996 | `				if( apCallArgs == 0 ){` |
|        - |  8997 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  8998 | `					nGenArgs = 0;` |
|      ! 0 |  8999 | `				}else{` |
|       10 |  9000 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  9001 | `					int didReorder = 0;` |
|       10 |  9002 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  9003 | `						/* Named-argument reordering for generator */` |
|        5 |  9004 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  9005 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  9006 | `						sxu32 nNV = nF;` |
|        5 |  9007 | `						sxi32 iVIdx = -1;` |
|        - |  9008 | `						sxi32 *aGSlot;` |
|        - |  9009 | `						sxu8 *aGUsed;` |
|        - |  9010 | `						sxu32 gi;` |
|       13 |  9011 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  9012 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  9013 | `						}` |
|        7 |  9014 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  9015 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  9016 | `						if( aGSlot ){` |
|        5 |  9017 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  9018 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  9019 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  9020 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9021 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  9022 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9023 | `								goto Abort;` |
|        - |  9024 | `							}` |
|        - |  9025 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  9026 | `							 * append overflow (variadic / positional beyond` |
|        - |  9027 | `							 * formals) so downstream sees every argument. */` |
|        - |  9028 | `							{` |
|        5 |  9029 | `								int nOut = 0;` |
|       13 |  9030 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  9031 | `									sxu32 gj;` |
|       13 |  9032 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  9033 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  9034 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  9035 | `											break;` |
|        - |  9036 | `										}` |
|        3 |  9037 | `									}` |
|        5 |  9038 | `								}` |
|       13 |  9039 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  9040 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  9041 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  9042 | `									}` |
|        5 |  9043 | `								}` |
|        5 |  9044 | `								nGenArgs = nOut;` |
|        - |  9045 | `							}` |
|        5 |  9046 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  9047 | `							didReorder = 1;` |
|        2 |  9048 | `						}` |
|        - |  9049 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  9050 | `						 * positional fill below — preserves arg order rather` |
|        - |  9051 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  9052 | `					}` |
|       10 |  9053 | `					if( !didReorder ){` |
|       12 |  9054 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  9055 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  9056 | `						}` |
|        2 |  9057 | `					}` |
|        - |  9058 | `				}` |
|        4 |  9059 | `			}` |
|        - |  9060 | `			/* Create execution context and generator wrapper */` |
|       24 |  9061 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       24 |  9062 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  9063 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9064 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  9065 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  9066 | `				break;` |
|        - |  9067 | `			}` |
|       24 |  9068 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       24 |  9069 | `			if( pGenerator == 0 ){` |
|      ! 0 |  9070 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  9071 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  9072 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  9073 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  9074 | `				break;` |
|        - |  9075 | `			}` |
|        - |  9076 | `			/* Set up the frame with arguments, closure env, $this */` |
|       24 |  9077 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       24 |  9078 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       24 |  9079 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       24 |  9080 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       24 |  9081 | `			pExecCtx->pFrame->pParent = 0;` |
|       24 |  9082 | `			if( apCallArgs ){` |
|       10 |  9083 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  9084 | `			}` |
|       24 |  9085 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9086 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  9087 | `				if( pThis ){` |
|      ! 0 |  9088 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  9089 | `				}` |
|      ! 0 |  9090 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9091 | `					goto Abort;` |
|        - |  9092 | `				}` |
|      ! 0 |  9093 | `				break;` |
|        - |  9094 | `			}` |
|        - |  9095 | `			/* Create Generator class instance */` |
|       24 |  9096 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       24 |  9097 | `			if( pGenObj == 0 ){` |
|      ! 0 |  9098 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  9099 | `				break;` |
|        - |  9100 | `			}` |
|        - |  9101 | `			/* Store generator in __ctx attribute */` |
|       24 |  9102 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       24 |  9103 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       24 |  9104 | `			if( pCtxAttr ){` |
|       24 |  9105 | `				pCtxAttr->x.pOther = pGenerator;` |
|       24 |  9106 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       11 |  9107 | `			}` |
|        - |  9108 | `			/* Pop args and function name, push Generator object */` |
|       24 |  9109 | `			PH7_MemObjRelease(pTos);` |
|       24 |  9110 | `			pTos = &pTos[-nCallArgs];` |
|       24 |  9111 | `			pTos->x.pOther = pGenObj;` |
|       24 |  9112 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       24 |  9113 | `			pGenObj->iRef++;` |
|       24 |  9114 | `			if( pThis ){` |
|      ! 0 |  9115 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  9116 | `			}` |
|       24 |  9117 | `			break;` |
|        - |  9118 | `		}` |
|        - |  9119 | `		/* Extract the formal argument set */` |
|    18522 |  9120 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  9121 | `		/* Create a new VM frame  */` |
|    18522 |  9122 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    18522 |  9123 | `		if( rc != SXRET_OK ){` |
|        - |  9124 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9125 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9126 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  9127 | `				&pVmFunc->sName);` |
|        - |  9128 | `			/* Pop given arguments */` |
|      ! 0 |  9129 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9130 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9131 | `			}` |
|        - |  9132 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  9133 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  9134 | `			break;` |
|        - |  9135 | `		}` |
|    18522 |  9136 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  9137 | `			/* Install the '$this' variable */` |
|        - |  9138 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     3280 |  9139 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     3280 |  9140 | `			if( pObj ){` |
|        - |  9141 | `				/* Reflect the change */` |
|     3280 |  9142 | `				pObj->x.pOther = pThis;` |
|     3280 |  9143 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1639 |  9144 | `			}` |
|     1639 |  9145 | `		}` |
|    18522 |  9146 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  9147 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  9148 | `			/* Install static variables */` |
|        6 |  9149 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|       12 |  9150 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|        6 |  9151 | `				pStatic = &aStatic[n];` |
|        6 |  9152 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  9153 | `					/* Initialize the static variables */` |
|        6 |  9154 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|        6 |  9155 | `					if( pObj ){` |
|        - |  9156 | `						/* Assume a NULL initialization value */` |
|        6 |  9157 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|        6 |  9158 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  9159 | `							/* Evaluate initialization expression (Any complex expression) */` |
|        6 |  9160 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|        3 |  9161 | `						}` |
|        6 |  9162 | `						pObj->nIdx = pStatic->nIdx;` |
|        3 |  9163 | `					}else{` |
|      ! 0 |  9164 | `						continue;` |
|        - |  9165 | `					}` |
|        3 |  9166 | `				}` |
|        - |  9167 | `				/* Install in the current frame */` |
|        9 |  9168 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|        6 |  9169 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|        3 |  9170 | `			}` |
|        3 |  9171 | `		}` |
|        - |  9172 | `		/* Push arguments in the local frame */` |
|        - |  9173 | `		{` |
|    18522 |  9174 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|        - |  9175 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - |  9176 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    18522 |  9177 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    18522 |  9178 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  9179 | `			/* ============================================================` |
|        - |  9180 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  9181 | `			 *` |
|        - |  9182 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  9183 | `			 * or position, then install them in the frame.` |
|        - |  9184 | `			 * ============================================================ */` |
|       96 |  9185 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       96 |  9186 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       96 |  9187 | `			sxi32 iVariadicIdx = -1;` |
|        - |  9188 | `			sxu32 nNonVariadic;` |
|        - |  9189 | `			sxi32 *aSlot;` |
|        - |  9190 | `			sxu8  *aUsed;` |
|        - |  9191 | `			sxu32 i;` |
|        - |  9192 | `			/* Find variadic parameter index */` |
|      292 |  9193 | `			for( i = 0; i < nFormal; i++ ){` |
|      206 |  9194 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  9195 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  9196 | `					break;` |
|        - |  9197 | `				}` |
|      100 |  9198 | `			}` |
|       96 |  9199 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  9200 | `			/* Allocate mapping arrays */` |
|      143 |  9201 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       94 |  9202 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       96 |  9203 | `			if( aSlot == 0 ){` |
|      ! 0 |  9204 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  9205 | `				goto Abort;` |
|        - |  9206 | `			}` |
|       96 |  9207 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  9208 | `			/* Resolve named arguments to formal parameters */` |
|      143 |  9209 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       47 |  9210 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       96 |  9211 | `			if( rc == PH7_ABORT ){` |
|        7 |  9212 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  9213 | `				goto Abort;` |
|        - |  9214 | `			}` |
|        - |  9215 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      275 |  9216 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  9217 | `				/* Find the stack arg mapped to formal n */` |
|      187 |  9218 | `				sxi32 iSrc = -1;` |
|      309 |  9219 | `				for( i = 0; i < nActual; i++ ){` |
|      291 |  9220 | `					if( aSlot[i] == (sxi32)n ){` |
|      169 |  9221 | `						iSrc = (sxi32)i;` |
|      169 |  9222 | `						break;` |
|        - |  9223 | `					}` |
|       62 |  9224 | `				}` |
|      187 |  9225 | `				if( iSrc >= 0 ){` |
|        - |  9226 | `					/* Argument was provided — install with type checking */` |
|      169 |  9227 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  9228 | `					/* NULL-to-default redirect (existing behavior) */` |
|      168 |  9229 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  9230 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  9231 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  9232 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9233 | `					}` |
|        - |  9234 | `					/* Type checking: union types */` |
|      169 |  9235 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  9236 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  9237 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 |  9238 | `							bCallIsStrict);` |
|       13 |  9239 | `						if( rcU != SXRET_OK ){` |
|        - |  9240 | `							const char *zGiven;` |
|      ! 0 |  9241 | `							const char *zExpected = "union";` |
|        - |  9242 | `							char zBuf[128];` |
|        - |  9243 | `							char zTypeBuf[128];` |
|      ! 0 |  9244 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9245 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  9246 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9247 | `								zGiven = "null";` |
|      ! 0 |  9248 | `							}else{` |
|      ! 0 |  9249 | `								zGiven = ph7_type_name(pVal);` |
|        - |  9250 | `							}` |
|      ! 0 |  9251 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 |  9252 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  9253 | `							}` |
|      ! 0 |  9254 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9255 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 |  9256 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9257 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9258 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  9259 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9260 | `							pFrameStack = 0;` |
|      ! 0 |  9261 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  9262 | `							goto SkipFuncBody;` |
|        - |  9263 | `						}` |
|      171 |  9264 | `					}else if( aFormalArg[n].nType > 0` |
|       91 |  9265 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  9266 | `						/* Scalar/class type checking */` |
|       17 |  9267 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  9268 | `							SyString *pName = &aFormalArg[n].sClass;` |
|      ! 0 |  9269 | `							ph7_class *pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  9270 | `							if( pClass ){` |
|      ! 0 |  9271 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9272 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9273 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9274 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9275 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9276 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  9277 | `									}` |
|      ! 0 |  9278 | `								}else{` |
|      ! 0 |  9279 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  9280 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  9281 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9282 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9283 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9284 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  9285 | `									}` |
|        - |  9286 | `								}` |
|      ! 0 |  9287 | `							}` |
|       17 |  9288 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  9289 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  9290 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9291 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  9292 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9293 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9294 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9295 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9296 | `								pFrameStack = 0;` |
|      ! 0 |  9297 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9298 | `								goto SkipFuncBody;` |
|        7 |  9299 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9300 | `								char zTypeBuf[128];` |
|      ! 0 |  9301 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9302 | `									&aFormalArg[n].sName,` |
|      ! 0 |  9303 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9304 | `									ph7_type_name(pVal));` |
|      ! 0 |  9305 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  9306 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  9307 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  9308 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9309 | `								pFrameStack = 0;` |
|      ! 0 |  9310 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  9311 | `								goto SkipFuncBody;` |
|        - |  9312 | `							}` |
|        3 |  9313 | `						}` |
|        8 |  9314 | `					}` |
|        - |  9315 | `					/* Install: by reference or by value */` |
|      169 |  9316 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  9317 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9318 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  9319 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9320 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9321 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9322 | `							}` |
|      ! 0 |  9323 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9324 | `						}else{` |
|        7 |  9325 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  9326 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  9327 | `							if( pRefEntry == 0 ){` |
|        7 |  9328 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  9329 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  9330 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  9331 | `								sArg.pUserData = 0;` |
|        5 |  9332 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  9333 | `							}` |
|        5 |  9334 | `							pObj = 0;` |
|        - |  9335 | `						}` |
|        3 |  9336 | `					}else{` |
|      165 |  9337 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9338 | `					}` |
|      169 |  9339 | `					if( pObj ){` |
|      165 |  9340 | `						PH7_MemObjStore(pVal,pObj);` |
|      165 |  9341 | `						sArg.nIdx = pObj->nIdx;` |
|      165 |  9342 | `						sArg.pUserData = 0;` |
|      165 |  9343 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       82 |  9344 | `					}` |
|       85 |  9345 | `				}else{` |
|        - |  9346 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  9347 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9348 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  9349 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  9350 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  9351 | `						if( pObj ){` |
|       19 |  9352 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  9353 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  9354 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  9355 | `							sArg.pUserData = 0;` |
|       19 |  9356 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  9357 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  9358 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9359 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9360 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  9361 | `							}` |
|        9 |  9362 | `						}` |
|        9 |  9363 | `					}` |
|        - |  9364 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  9365 | `				}` |
|       94 |  9366 | `			}` |
|        - |  9367 | `			/* Handle variadic parameter */` |
|       89 |  9368 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  9369 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  9370 | `				if( pObj ){` |
|        9 |  9371 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9372 | `					{` |
|        9 |  9373 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  9374 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  9375 | `							if( aSlot[i] == -1 ){` |
|       16 |  9376 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  9377 | `									/* Named variadic entry: insert with string key */` |
|        - |  9378 | `									ph7_value sKey;` |
|       11 |  9379 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  9380 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  9381 | `										pCallMap3->aNames[i].zString,` |
|       10 |  9382 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  9383 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  9384 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  9385 | `								}else{` |
|        - |  9386 | `									/* Positional variadic entry */` |
|      ! 0 |  9387 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  9388 | `								}` |
|        5 |  9389 | `							}` |
|       12 |  9390 | `						}` |
|        - |  9391 | `					}` |
|        9 |  9392 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  9393 | `					sArg.pUserData = 0;` |
|        9 |  9394 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  9395 | `				}` |
|        5 |  9396 | `			}else{` |
|        - |  9397 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  9398 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  9399 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  9400 | `				 * the positional-only path's behavior. */` |
|       81 |  9401 | `				sxu32 nAnon = nNonVariadic;` |
|      237 |  9402 | `				for( i = 0; i < nActual; i++ ){` |
|      157 |  9403 | `					if( aSlot[i] == -2 ){` |
|        - |  9404 | `						char zAnonBuf[32];` |
|        - |  9405 | `						SyString sAnonName;` |
|      ! 0 |  9406 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  9407 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  9408 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  9409 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  9410 | `						if( pObj ){` |
|      ! 0 |  9411 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  9412 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  9413 | `							sArg.pUserData = 0;` |
|      ! 0 |  9414 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  9415 | `						}` |
|      ! 0 |  9416 | `						nAnon++;` |
|      ! 0 |  9417 | `					}` |
|       79 |  9418 | `				}` |
|        - |  9419 | `			}` |
|        - |  9420 | `			/* Release all stack arguments */` |
|      267 |  9421 | `			for( i = 0; i < nActual; i++ ){` |
|      179 |  9422 | `				PH7_MemObjRelease(&pArg[i]);` |
|       90 |  9423 | `			}` |
|       89 |  9424 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  9425 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       89 |  9426 | `			n = nFormal;` |
|       45 |  9427 | `		}else{` |
|        - |  9428 | `		/* ============================================================` |
|        - |  9429 | `		 * Positional-only matching path (original)` |
|        - |  9430 | `		 * ============================================================ */` |
|    18428 |  9431 | `		n = 0;` |
|    49060 |  9432 | `		while( pArg < pTos ){` |
|    30706 |  9433 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  9434 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       40 |  9435 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       40 |  9436 | `				if( pObj ){` |
|        - |  9437 | `					/* Initialize as empty array */` |
|       40 |  9438 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  9439 | `					{` |
|       40 |  9440 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      150 |  9441 | `						while( pArg < pTos ){` |
|        - |  9442 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  9443 | `							 *` |
|        - |  9444 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  9445 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  9446 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  9447 | `							 * non-union variadic path below has the same limitation;` |
|        - |  9448 | `							 * fixing both wants a separate counter for elements` |
|        - |  9449 | `							 * already packed into the variadic array. */` |
|      114 |  9450 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  9451 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  9452 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 |  9453 | `									bCallIsStrict);` |
|       16 |  9454 | `								if( rcU != SXRET_OK ){` |
|        - |  9455 | `									const char *zGiven;` |
|        3 |  9456 | `									const char *zExpected = "union";` |
|        - |  9457 | `									char zBuf[128];` |
|        - |  9458 | `									char zTypeBuf[128];` |
|        3 |  9459 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9460 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  9461 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  9462 | `										zGiven = "null";` |
|      ! 0 |  9463 | `									}else{` |
|        3 |  9464 | `										zGiven = ph7_type_name(pArg);` |
|        - |  9465 | `									}` |
|        3 |  9466 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 |  9467 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 |  9468 | `									}` |
|        4 |  9469 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  9470 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 |  9471 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9472 | `										goto Abort;` |
|        - |  9473 | `									}` |
|        3 |  9474 | `									PH7_MemObjRelease(pTos);` |
|        3 |  9475 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  9476 | `									pFrameStack = 0;` |
|        3 |  9477 | `									rc = PH7_EXCEPTION;` |
|        3 |  9478 | `									goto SkipFuncBody;` |
|        - |  9479 | `								}` |
|       14 |  9480 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  9481 | `								pArg++;` |
|       14 |  9482 | `								continue;` |
|        - |  9483 | `							}` |
|        - |  9484 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  9485 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      114 |  9486 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  9487 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  9488 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  9489 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9490 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  9491 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9492 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  9493 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9494 | `										goto Abort;` |
|        - |  9495 | `									}` |
|        - |  9496 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  9497 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9498 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9499 | `									pFrameStack = 0;` |
|      ! 0 |  9500 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9501 | `									goto SkipFuncBody;` |
|       13 |  9502 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9503 | `									char zTypeBuf[128];` |
|      ! 0 |  9504 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9505 | `										&aFormalArg[n].sName,` |
|      ! 0 |  9506 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9507 | `										ph7_type_name(pArg));` |
|      ! 0 |  9508 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9509 | `										goto Abort;` |
|        - |  9510 | `									}` |
|      ! 0 |  9511 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9512 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9513 | `									pFrameStack = 0;` |
|      ! 0 |  9514 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9515 | `									goto SkipFuncBody;` |
|        - |  9516 | `								}` |
|        6 |  9517 | `							}` |
|      100 |  9518 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|      100 |  9519 | `							pArg++;` |
|        2 |  9520 | `						}` |
|        - |  9521 | `					}` |
|       38 |  9522 | `					sArg.nIdx = pObj->nIdx;` |
|       38 |  9523 | `					sArg.pUserData = 0;` |
|       38 |  9524 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  9525 | `				}` |
|       38 |  9526 | `				break; /* All remaining args consumed */` |
|        - |  9527 | `			}` |
|    30668 |  9528 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    30450 |  9529 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       39 |  9530 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  9531 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  9532 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  9533 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9534 | `						goto Abort;` |
|        - |  9535 | `					}` |
|      ! 0 |  9536 | `				}` |
|        - |  9537 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    30452 |  9538 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       89 |  9539 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       58 |  9540 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       29 |  9541 | `						bCallIsStrict);` |
|       60 |  9542 | `					if( rcU != SXRET_OK ){` |
|        - |  9543 | `						const char *zGiven;` |
|       19 |  9544 | `						const char *zExpected = "union";` |
|        - |  9545 | `						char zBuf[128];` |
|        - |  9546 | `						char zTypeBuf[128];` |
|       19 |  9547 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  9548 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  9549 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  9550 | `							zGiven = "null";` |
|        5 |  9551 | `						}else{` |
|        5 |  9552 | `							zGiven = ph7_type_name(pArg);` |
|        - |  9553 | `						}` |
|       19 |  9554 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       19 |  9555 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        9 |  9556 | `						}` |
|       28 |  9557 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  9558 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       19 |  9559 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  9560 | `							goto Abort;` |
|        - |  9561 | `						}` |
|       19 |  9562 | `						PH7_MemObjRelease(pTos);` |
|       19 |  9563 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  9564 | `						pFrameStack = 0;` |
|       19 |  9565 | `						rc = PH7_EXCEPTION;` |
|       19 |  9566 | `						goto SkipFuncBody;` |
|        - |  9567 | `					}` |
|       21 |  9568 | `				}else` |
|        - |  9569 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  9570 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    30418 |  9571 | `				if( aFormalArg[n].nType > 0` |
|    15913 |  9572 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1406 |  9573 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  9574 | `						/* Argument must be a class instance [i.e: object] */` |
|       26 |  9575 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  9576 | `						ph7_class *pClass;` |
|        - |  9577 | `						/* Try to extract the desired class */` |
|       26 |  9578 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       26 |  9579 | `						if( pClass ){` |
|       22 |  9580 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9581 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9582 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9583 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9584 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9585 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9586 | `								}` |
|      ! 0 |  9587 | `							}else{` |
|        - |  9588 | `								/* reuse pThis declared in outer scope */` |
|       22 |  9589 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  9590 | `								/* Make sure the object is an instance of the given class */` |
|       22 |  9591 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  9592 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9593 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9594 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9595 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9596 | `								}` |
|        - |  9597 | `							}` |
|       12 |  9598 | `						}` |
|     1394 |  9599 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       26 |  9600 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9601 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  9602 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  9603 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  9604 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9605 | `								goto Abort;` |
|        - |  9606 | `							}` |
|        - |  9607 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  9608 | `							PH7_MemObjRelease(pTos);` |
|       11 |  9609 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  9610 | `							pFrameStack = 0;` |
|       11 |  9611 | `							rc = PH7_EXCEPTION;` |
|       11 |  9612 | `							goto SkipFuncBody;` |
|       16 |  9613 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9614 | `							char zTypeBuf[128];` |
|       11 |  9615 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        6 |  9616 | `								&aFormalArg[n].sName,` |
|        6 |  9617 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        3 |  9618 | `								ph7_type_name(pArg));` |
|        8 |  9619 | `							if( rc == PH7_ABORT ){` |
|        5 |  9620 | `								goto Abort;` |
|        - |  9621 | `							}` |
|        3 |  9622 | `							PH7_MemObjRelease(pTos);` |
|        3 |  9623 | `							pTos = &pTos[-nCallArgs];` |
|        3 |  9624 | `							pFrameStack = 0;` |
|        3 |  9625 | `							rc = PH7_EXCEPTION;` |
|        3 |  9626 | `							goto SkipFuncBody;` |
|        - |  9627 | `						}` |
|        4 |  9628 | `					}` |
|      694 |  9629 | `				}` |
|    30418 |  9630 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  9631 | `					/* Pass by reference */` |
|       58 |  9632 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  9633 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  9634 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  9635 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9636 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9637 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9638 | `						}` |
|        - |  9639 | `						/* Switch to pass by value */` |
|      ! 0 |  9640 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9641 | `					}else{` |
|        - |  9642 | `						SyHashEntry *pRefEntry;` |
|        - |  9643 | `						/* Install the referenced variable in the private function frame */` |
|       58 |  9644 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       58 |  9645 | `						if( pRefEntry == 0 ){` |
|       86 |  9646 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       56 |  9647 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       58 |  9648 | `							sArg.nIdx = pArg->nIdx;` |
|       58 |  9649 | `							sArg.pUserData = 0;` |
|       58 |  9650 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       28 |  9651 | `						}` |
|       58 |  9652 | `						pObj = 0;` |
|        - |  9653 | `					}` |
|       30 |  9654 | `				}else{` |
|        - |  9655 | `					/* Pass by value,make a copy of the given argument */` |
|    30362 |  9656 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9657 | `				}` |
|    15210 |  9658 | `			}else{` |
|        - |  9659 | `				char zName[32];` |
|        - |  9660 | `				SyString sArgName;` |
|        - |  9661 | `				/* Set a dummy name */` |
|      218 |  9662 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      218 |  9663 | `				sArgName.zString = zName;` |
|        - |  9664 | `				/* Annonymous argument */` |
|      218 |  9665 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  9666 | `			}` |
|    30634 |  9667 | `			if( pObj ){` |
|    30578 |  9668 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  9669 | `				/* Insert argument index  */` |
|    30578 |  9670 | `				sArg.nIdx = pObj->nIdx;` |
|    30578 |  9671 | `				sArg.pUserData = 0;` |
|    30578 |  9672 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    15288 |  9673 | `			}` |
|    30634 |  9674 | `			PH7_MemObjRelease(pArg);` |
|    30634 |  9675 | `			pArg++;` |
|    30634 |  9676 | `			++n;` |
|        2 |  9677 | `		}` |
|        - |  9678 | `		} /* end named vs positional branch */` |
|        - |  9679 | `		/* Set up closure environment */` |
|    18480 |  9680 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  9681 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  9682 | `			ph7_value *pValue;` |
|        - |  9683 | `			sxu32 iEnv;` |
|      184 |  9684 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      434 |  9685 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      252 |  9686 | `				pEnv = &aEnv[iEnv];` |
|      252 |  9687 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  9688 | `					/* Do not install null value */` |
|      178 |  9689 | `					continue;` |
|        - |  9690 | `				}` |
|       76 |  9691 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       76 |  9692 | `				if( pValue == 0 ){` |
|      ! 0 |  9693 | `					continue;` |
|        - |  9694 | `				}` |
|        - |  9695 | `				/* Invalidate any prior representation */` |
|       76 |  9696 | `				PH7_MemObjRelease(pValue);` |
|        - |  9697 | `				/* Duplicate bound variable value */` |
|       76 |  9698 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       39 |  9699 | `			}` |
|       91 |  9700 | `		}` |
|        - |  9701 | `		/* Process default values for remaining formal parameters */` |
|    21372 |  9702 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2940 |  9703 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9704 | `				/* Variadic parameter with no extra args — create empty array */` |
|       48 |  9705 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       48 |  9706 | `				if( pObj ){` |
|       48 |  9707 | `					PH7_MemObjToHashmap(pObj);` |
|       48 |  9708 | `					sArg.nIdx = pObj->nIdx;` |
|       48 |  9709 | `					sArg.pUserData = 0;` |
|       48 |  9710 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  9711 | `				}` |
|       48 |  9712 | `				n++;` |
|       48 |  9713 | `				break; /* Variadic is always last */` |
|        - |  9714 | `			}` |
|     2894 |  9715 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2888 |  9716 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2888 |  9717 | `				if( pObj ){` |
|        - |  9718 | `					/* Evaluate the default value and extract it's result */` |
|     2888 |  9719 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2888 |  9720 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9721 | `						goto Abort;` |
|        - |  9722 | `					}` |
|        - |  9723 | `					/* Insert argument index */` |
|     2888 |  9724 | `					sArg.nIdx = pObj->nIdx;` |
|     2888 |  9725 | `					sArg.pUserData = 0;` |
|     2888 |  9726 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  9727 | `					/* Make sure the default argument is of the correct type */` |
|     2886 |  9728 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1866 |  9729 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 |  9730 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  9731 | `						/* Cast to the desired type */` |
|        3 |  9732 | `						xCast(pObj);` |
|        1 |  9733 | `					}` |
|     1443 |  9734 | `				}` |
|     1443 |  9735 | `			}` |
|     2894 |  9736 | `			++n;` |
|        2 |  9737 | `		}` |
|        - |  9738 | `		} /* end VmCallArgMap scope */` |
|        - |  9739 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  9740 | `		 * does not return anything.` |
|        - |  9741 | `		 */` |
|    18480 |  9742 | `		PH7_MemObjRelease(pTos);` |
|    18480 |  9743 | `		pTos = &pTos[-nCallArgs];` |
|        - |  9744 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    18480 |  9745 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    18480 |  9746 | `		if( pFrameStack == 0 ){` |
|        - |  9747 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9748 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  9749 | `				&pVmFunc->sName);` |
|      ! 0 |  9750 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9751 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9752 | `			}` |
|      ! 0 |  9753 | `			break;` |
|        - |  9754 | `		}` |
|     9239 |  9755 | `SkipFuncBody:` |
|    18512 |  9756 | `		if( pSelf ){` |
|        - |  9757 | `			/* Push class name */` |
|     3350 |  9758 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1674 |  9759 | `		}` |
|        - |  9760 | `		/* Increment nesting level */` |
|    18512 |  9761 | `		pVm->nRecursionDepth++;` |
|    18512 |  9762 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  9763 | `			/* Execute function body */` |
|    27719 |  9764 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0,` |
|    18478 |  9765 | `				pVmFunc->nReturnType > 0 ? pVmFunc : 0);` |
|     9239 |  9766 | `		}` |
|        - |  9767 | `		/* Decrement nesting level */` |
|    18512 |  9768 | `		pVm->nRecursionDepth--;` |
|    18512 |  9769 | `		if( pSelf ){` |
|        - |  9770 | `			/* Pop class name */` |
|     3350 |  9771 | `			(void)SySetPop(&pVm->aSelf);` |
|     1674 |  9772 | `		}` |
|        - |  9773 | `		/* Cleanup the mess left behind */` |
|    18512 |  9774 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  9775 | `			/* Return by reference,reflect that */` |
|        9 |  9776 | `			if( n != SXU32_HIGH ){` |
|        9 |  9777 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  9778 | `				sxu32 i;` |
|        - |  9779 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  9780 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  9781 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  9782 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  9783 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9784 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9785 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  9786 | `								&pVmFunc->sName);` |
|      ! 0 |  9787 | `						}` |
|      ! 0 |  9788 | `						n = SXU32_HIGH;` |
|      ! 0 |  9789 | `						break;` |
|        - |  9790 | `					}` |
|        3 |  9791 | `				}` |
|        5 |  9792 | `			}else{` |
|      ! 0 |  9793 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9794 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9795 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  9796 | `						&pVmFunc->sName);` |
|      ! 0 |  9797 | `				}` |
|        - |  9798 | `			}` |
|        9 |  9799 | `			pTos->nIdx = n;` |
|        4 |  9800 | `		}` |
|        - |  9801 | `		/* Cleanup the mess left behind */` |
|    18512 |  9802 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  9803 | `			/* An exception was throw in this frame */` |
|      100 |  9804 | `			pFrame = pFrame->pParent;` |
|      100 |  9805 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  9806 | `				/* Pop the resutlt */` |
|       62 |  9807 | `				VmPopOperand(&pTos,1);` |
|        - |  9808 | `				/* Jump to this destination */` |
|       62 |  9809 | `				pc = pFrame->iExceptionJump - 1;` |
|       62 |  9810 | `				rc = PH7_OK;` |
|       32 |  9811 | `			}else{` |
|       39 |  9812 | `				if( pFrame->pParent ){` |
|       39 |  9813 | `					rc = PH7_EXCEPTION;` |
|       20 |  9814 | `				}else{` |
|        - |  9815 | `					/* Continue normal execution */` |
|      ! 0 |  9816 | `					rc = PH7_OK;` |
|        - |  9817 | `				}` |
|        - |  9818 | `			}` |
|       49 |  9819 | `		}` |
|        - |  9820 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    18512 |  9821 | `		if( pFrameStack ){` |
|    18480 |  9822 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     9239 |  9823 | `		}` |
|        - |  9824 | `		/* Leave the frame */` |
|    18512 |  9825 | `		VmLeaveFrame(&(*pVm));` |
|    18512 |  9826 | `		if( rc == PH7_ABORT ){` |
|        - |  9827 | `			/* Abort processing immeditaley */` |
|       17 |  9828 | `			goto Abort;` |
|    18496 |  9829 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9830 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  9831 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  9832 | `			 * overwriting the state saved by the inner level.` |
|        - |  9833 | `			 * pTos points to the result slot (not yet written).` |
|        - |  9834 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  9835 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  9836 | `			goto Suspend;` |
|    18458 |  9837 | `		}else if( rc == PH7_EXCEPTION ){` |
|       39 |  9838 | `			goto Exception;` |
|        - |  9839 | `		}` |
|     9211 |  9840 | `	}else{` |
|        - |  9841 | `		ph7_user_func *pFunc;` |
|        - |  9842 | `		ph7_context sCtx;` |
|        - |  9843 | `		ph7_value sRet;` |
|        - |  9844 | `		/* Look for an installed foreign function.` |
|        - |  9845 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  9846 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - |  9847 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - |  9848 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   697002 |  9849 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  9850 | `		{` |
|   697002 |  9851 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   697002 |  9852 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - |  9853 | `			/* Compiler-qualified: try short name as global fallback */` |
|       22 |  9854 | `			const char *zShort = sName.zString;` |
|        - |  9855 | `			sxu32 i;` |
|      334 |  9856 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      314 |  9857 | `				if( sName.zString[i] == '\\' ){` |
|       28 |  9858 | `					zShort = &sName.zString[i + 1];` |
|       13 |  9859 | `				}` |
|      158 |  9860 | `			}` |
|       22 |  9861 | `			if( zShort != sName.zString ){` |
|       22 |  9862 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       22 |  9863 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 |  9864 | `			}` |
|       10 |  9865 | `		}` |
|        - |  9866 | `		} /* end VmCallArgMap namespace scope */` |
|   697002 |  9867 | `		if( pEntry == 0 ){` |
|        - |  9868 | `			/* Call to undefined function */` |
|        5 |  9869 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  9870 | `			/* Pop given arguments */` |
|        5 |  9871 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  9872 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  9873 | `			}` |
|        - |  9874 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  9875 | `			PH7_MemObjRelease(pTos);` |
|       58 |  9876 | `			break;` |
|        - |  9877 | `		}` |
|   696998 |  9878 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  9879 | `		/* Start collecting function arguments */` |
|   696998 |  9880 | `		SySetReset(&aArg);` |
|  1879326 |  9881 | `		while( pArg < pTos ){` |
|  1182330 |  9882 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1182330 |  9883 | `			pArg++;` |
|        2 |  9884 | `		}` |
|        - |  9885 | `		/* Assume a null return value */` |
|   696998 |  9886 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  9887 | `		/* Init the call context */` |
|   696998 |  9888 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  9889 | `		/* Call the foreign function */` |
|   696998 |  9890 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  9891 | `		/* Release the call context */` |
|   696998 |  9892 | `		VmReleaseCallContext(&sCtx);` |
|   696998 |  9893 | `		if( rc == PH7_ABORT ){` |
|        - |  9894 | `			/* Release the (possibly partially-built) result slot before unwinding;` |
|        - |  9895 | `			 * the Abort: label only frees the operand stack, not this local` |
|        - |  9896 | `			 * (mirrors the PH7_EXCEPTION branch below). */` |
|      497 |  9897 | `			PH7_MemObjRelease(&sRet);` |
|      497 |  9898 | `			goto Abort;` |
|   696502 |  9899 | `		}else if( rc == PH7_EXCEPTION ){` |
|      112 |  9900 | `			VmFrame *pFrm = pVm->pFrame;` |
|      112 |  9901 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|      112 |  9902 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  9903 | `				/* Exception was NOT caught, propagate */` |
|        5 |  9904 | `				goto Exception;` |
|        - |  9905 | `			}` |
|        - |  9906 | `			/* Exception was caught: pop args and the result slot */` |
|      108 |  9907 | `			PH7_MemObjRelease(&sRet);` |
|      108 |  9908 | `			if( pInstr->iP1 > 0 ){` |
|       92 |  9909 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       45 |  9910 | `			}` |
|        - |  9911 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|      108 |  9912 | `			VmPopOperand(&pTos,1);` |
|        - |  9913 | `			/* Jump past the try/catch block via the exception frame */` |
|      108 |  9914 | `			pFrm = pVm->pFrame;` |
|      108 |  9915 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|      108 |  9916 | `				pc = pFrm->iExceptionJump - 1;` |
|       53 |  9917 | `			}` |
|      108 |  9918 | `			break;` |
|        - |  9919 | `		}` |
|   696392 |  9920 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9921 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  9922 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  9923 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  9924 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  9925 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  9926 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  9927 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  9928 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  9929 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  9930 | `			}` |
|        - |  9931 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  9932 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  9933 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  9934 | `			goto Suspend;` |
|        - |  9935 | `		}` |
|   696354 |  9936 | `		if( pInstr->iP1 > 0 ){` |
|        - |  9937 | `			/* Pop function name and arguments */` |
|   674358 |  9938 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   337200 |  9939 | `		}` |
|        - |  9940 | `		/* Save foreign function return value */` |
|   696354 |  9941 | `		PH7_MemObjStore(&sRet,pTos);` |
|   696354 |  9942 | `		PH7_MemObjRelease(&sRet);` |
|        - |  9943 | `	}` |
|   714772 |  9944 | `	break;` |
|        - |  9945 | `				  }` |
|        - |  9946 | `/*` |
|        - |  9947 | ` * OP_CONSUME: P1 * *` |
|        - |  9948 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  9949 | ` */` |
|    15999 |  9950 | `case PH7_OP_CONSUME: {` |
|    32000 |  9951 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    32000 |  9952 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  9953 |  |
|    32000 |  9954 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    32000 |  9955 | `	pCur = pOut;` |
|        - |  9956 | `	/* Start the consume process  */` |
|    64040 |  9957 | `	while( pOut <= pTos ){` |
|        - |  9958 | `		/* Force a string cast */` |
|    32042 |  9959 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1052 |  9960 | `			PH7_MemObjToString(pOut);` |
|      525 |  9961 | `		}` |
|    32042 |  9962 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  9963 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  9964 | `			/* Invoke the output consumer callback */` |
|    19624 |  9965 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    19624 |  9966 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    19624 |  9967 | `			SyBlobRelease(&pOut->sBlob);` |
|    19624 |  9968 | `			if( rc == SXERR_ABORT ){` |
|        - |  9969 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  9970 | `				goto Abort;` |
|        - |  9971 | `			}` |
|     9811 |  9972 | `		}` |
|    32042 |  9973 | `		pOut++;` |
|        2 |  9974 | `	}` |
|    32000 |  9975 | `	pTos = &pCur[-1];` |
|    31998 |  9976 | `	break;` |
|        - |  9977 | `					 }` |
|        - |  9978 |  |
|        - |  9979 | `		} /* Switch() */` |
| 11769082 |  9980 | `		pc++; /* Next instruction in the stream */` |
|        2 |  9981 | `	} /* For(;;) */` |
|    22188 |  9982 | `Done:` |
|    44378 |  9983 | `	SySetRelease(&aArg);` |
|    44378 |  9984 | `	return SXRET_OK;` |
|       72 |  9985 | `Suspend:` |
|      146 |  9986 | `	SySetRelease(&aArg);` |
|      146 |  9987 | `	return PH7_SUSPEND;` |
|      280 |  9988 | `Abort:` |
|      561 |  9989 | `	SySetRelease(&aArg);` |
|     1875 |  9990 | `	while( pTos >= pStack ){` |
|     1315 |  9991 | `		PH7_MemObjRelease(pTos);` |
|     1315 |  9992 | `		pTos--;` |
|        1 |  9993 | `	}` |
|      561 |  9994 | `	return PH7_ABORT;` |
|       29 |  9995 | `Exception:` |
|       60 |  9996 | `	SySetRelease(&aArg);` |
|      112 |  9997 | `	while( pTos >= pStack ){` |
|       54 |  9998 | `		PH7_MemObjRelease(pTos);` |
|       54 |  9999 | `		pTos--;` |
|        2 | 10000 | `	}` |
|       60 | 10001 | `	return PH7_EXCEPTION;` |
|    22571 | 10002 |  |
|        - | 10003 | `/*` |
|        - | 10004 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - | 10005 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 10006 | ` * See block-comment on that function for additional information.` |
|        - | 10007 | ` */` |
|    20606 | 10008 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 | 10009 |  |
|        - | 10010 | `	ph7_value *pStack;` |
|        - | 10011 | `	sxi32 rc;` |
|        - | 10012 | `	/* Allocate a new operand stack */` |
|    20608 | 10013 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    20608 | 10014 | `	if( pStack == 0 ){` |
|      ! 0 | 10015 | `		return SXERR_MEM;` |
|        - | 10016 | `	}` |
|        - | 10017 | `	/* Execute the program */` |
|    20608 | 10018 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0);` |
|        - | 10019 | `	/* Free the operand stack */` |
|    20608 | 10020 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - | 10021 | `	/* Execution result */` |
|    20608 | 10022 | `	return rc;` |
|    10305 | 10023 |  |
|        - | 10024 | `/*` |
|        - | 10025 | ` * Invoke any installed shutdown callbacks.` |
|        - | 10026 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - | 10027 | ` * or more calls to [register_shutdown_function()].` |
|        - | 10028 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - | 10029 | ` * execution ends.` |
|        - | 10030 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - | 10031 | ` * additional information.` |
|        - | 10032 | ` */` |
|     2832 | 10033 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 | 10034 |  |
|        - | 10035 | `	VmShutdownCB *pEntry;` |
|        - | 10036 | `	ph7_value *apArg[10];` |
|        - | 10037 | `	sxu32 n,nEntry;` |
|        - | 10038 | `	int i;` |
|        - | 10039 | `	/* Point to the stack of registered callbacks */` |
|     2834 | 10040 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    31154 | 10041 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    28322 | 10042 | `		apArg[i] = 0;` |
|    14162 | 10043 | `	}` |
|        - | 10044 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|        - | 10045 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|        - | 10046 | `	 * callbacks, mirroring PHP.` |
|        - | 10047 | `	 */` |
|     2834 | 10048 | `	pVm->bHaltRequested = 0;` |
|     2844 | 10049 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       12 | 10050 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       12 | 10051 | `		if( pEntry ){` |
|        - | 10052 | `			/* Prepare callback arguments if any */` |
|       12 | 10053 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 | 10054 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 | 10055 | `					break;` |
|        - | 10056 | `				}` |
|      ! 0 | 10057 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 | 10058 | `			}` |
|        - | 10059 | `			/* Invoke the callback */` |
|       12 | 10060 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - | 10061 | `			/*` |
|        - | 10062 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - | 10063 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - | 10064 | `			 */` |
|       12 | 10065 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       12 | 10066 | `			if( pEntry ){` |
|       12 | 10067 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|       12 | 10068 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 | 10069 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 | 10070 | `				}` |
|        5 | 10071 | `			}` |
|       12 | 10072 | `			if( pVm->bHaltRequested ){` |
|        - | 10073 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|      ! 0 | 10074 | `				break;` |
|        - | 10075 | `			}` |
|        5 | 10076 | `		}` |
|        7 | 10077 | `	}` |
|     2834 | 10078 | `	SySetReset(&pVm->aShutdown);` |
|     2834 | 10079 |  |
|        - | 10080 | `/*` |
|        - | 10081 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - | 10082 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - | 10083 | ` * See block-comment on that function for additional information.` |
|        - | 10084 | ` */` |
|     2832 | 10085 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 | 10086 |  |
|        - | 10087 | `	/* Make sure we are ready to execute this program */` |
|     2834 | 10088 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 | 10089 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - | 10090 | `	}` |
|        - | 10091 | `	/* Set the execution magic number  */` |
|     2834 | 10092 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - | 10093 | `	/* Execute the program */` |
|     2834 | 10094 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0);` |
|        - | 10095 | `	/* Invoke any shutdown callbacks */` |
|     2834 | 10096 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - | 10097 | `	/*` |
|        - | 10098 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - | 10099 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - | 10100 | `	 * [ph7_vm_reset()] first would fail.` |
|        - | 10101 | `	 */` |
|     2834 | 10102 | `	return SXRET_OK;` |
|     1418 | 10103 |  |
|        - | 10104 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - | 10105 | `/*` |
|        - | 10106 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - | 10107 | ` * The context is in CREATED state and ready to be started.` |
|        - | 10108 | ` */` |
|       46 | 10109 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 | 10110 |  |
|        - | 10111 | `	ph7_exec_ctx *pCtx;` |
|        - | 10112 | `	ph7_value *pStack;` |
|        - | 10113 | `	VmFrame *pFrame;` |
|       48 | 10114 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       48 | 10115 | `	if( pCtx == 0 ){` |
|      ! 0 | 10116 | `		return 0;` |
|        - | 10117 | `	}` |
|       48 | 10118 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       48 | 10119 | `	pCtx->pVm = pVm;` |
|       48 | 10120 | `	pCtx->pFunc = pFunc;` |
|       48 | 10121 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       48 | 10122 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       48 | 10123 | `	pCtx->pc = 0;` |
|       48 | 10124 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       48 | 10125 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - | 10126 | `	/* Allocate a private operand stack */` |
|       48 | 10127 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       48 | 10128 | `	if( pStack == 0 ){` |
|      ! 0 | 10129 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 | 10130 | `		return 0;` |
|        - | 10131 | `	}` |
|       48 | 10132 | `	pCtx->pStack = pStack;` |
|        - | 10133 | `	/* Create a detached frame for the fiber */` |
|       48 | 10134 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       48 | 10135 | `	if( pFrame == 0 ){` |
|      ! 0 | 10136 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 | 10137 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 | 10138 | `		return 0;` |
|        - | 10139 | `	}` |
|       48 | 10140 | `	pCtx->pFrame = pFrame;` |
|       48 | 10141 | `	return pCtx;` |
|       25 | 10142 |  |
|        - | 10143 | `/*` |
|        - | 10144 | ` * Start executing a fiber context for the first time.` |
|        - | 10145 | ` */` |
|       46 | 10146 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 | 10147 |  |
|        - | 10148 | `	ph7_exec_ctx *pOldCtx;` |
|        - | 10149 | `	sxi32 rc;` |
|       48 | 10150 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10151 | `		return SXERR_INVALID;` |
|        - | 10152 | `	}` |
|        - | 10153 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       48 | 10154 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       48 | 10155 | `	pVm->pFrame = pCtx->pFrame;` |
|        - | 10156 | `	/* Save and set the active context */` |
|       48 | 10157 | `	pOldCtx = pVm->pActiveCtx;` |
|       48 | 10158 | `	pVm->pActiveCtx = pCtx;` |
|       48 | 10159 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       48 | 10160 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       48 | 10161 | `	pVm->nRecursionDepth++;` |
|        - | 10162 | `	/* Execute from the beginning */` |
|       48 | 10163 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       23 | 10164 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|       46 | 10165 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|       48 | 10166 | `	pVm->nRecursionDepth--;` |
|        - | 10167 | `	/* Restore the previous context */` |
|       48 | 10168 | `	pVm->pActiveCtx = pOldCtx;` |
|       48 | 10169 | `	if( rc == PH7_SUSPEND ){` |
|        - | 10170 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       46 | 10171 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       46 | 10172 | `		pCtx->pFrame->pParent = 0;` |
|       46 | 10173 | `		if( pResult ){` |
|       24 | 10174 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 | 10175 | `		}` |
|       46 | 10176 | `		return SXRET_OK;` |
|        - | 10177 | `	}` |
|        - | 10178 | `	/* Detach frame */` |
|        3 | 10179 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 | 10180 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 | 10181 | `		pCtx->pFrame->pParent = 0;` |
|        1 | 10182 | `	}` |
|        3 | 10183 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10184 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10185 | `		return PH7_ABORT;` |
|        - | 10186 | `	}` |
|        3 | 10187 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10188 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10189 | `		return PH7_EXCEPTION;` |
|        - | 10190 | `	}` |
|        - | 10191 | `	/* Normal completion */` |
|        3 | 10192 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 | 10193 | `	if( pResult ){` |
|        3 | 10194 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 | 10195 | `	}` |
|        3 | 10196 | `	return SXRET_OK;` |
|       25 | 10197 |  |
|        - | 10198 | `/*` |
|        - | 10199 | ` * Resume a suspended fiber context.` |
|        - | 10200 | ` */` |
|       98 | 10201 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 | 10202 |  |
|        - | 10203 | `	ph7_exec_ctx *pOldCtx;` |
|        - | 10204 | `	sxi32 rc;` |
|      100 | 10205 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 | 10206 | `		return SXERR_INVALID;` |
|        - | 10207 | `	}` |
|        - | 10208 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - | 10209 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - | 10210 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      100 | 10211 | `	if( pResumeValue ){` |
|       40 | 10212 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 | 10213 | `	}else{` |
|       62 | 10214 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - | 10215 | `	}` |
|      100 | 10216 | `	pCtx->nTos++;` |
|        - | 10217 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      100 | 10218 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      100 | 10219 | `	pVm->pFrame = pCtx->pFrame;` |
|        - | 10220 | `	/* Save and set the active context */` |
|      100 | 10221 | `	pOldCtx = pVm->pActiveCtx;` |
|      100 | 10222 | `	pVm->pActiveCtx = pCtx;` |
|      100 | 10223 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      100 | 10224 | `	pVm->nRecursionDepth++;` |
|        - | 10225 | `	/* Resume execution from saved PC */` |
|      100 | 10226 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       49 | 10227 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|       98 | 10228 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|      100 | 10229 | `	pVm->nRecursionDepth--;` |
|        - | 10230 | `	/* Restore the previous context */` |
|      100 | 10231 | `	pVm->pActiveCtx = pOldCtx;` |
|      100 | 10232 | `	if( rc == PH7_SUSPEND ){` |
|        - | 10233 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       64 | 10234 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       64 | 10235 | `		pCtx->pFrame->pParent = 0;` |
|       64 | 10236 | `		if( pResult ){` |
|       18 | 10237 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 | 10238 | `		}` |
|       64 | 10239 | `		return SXRET_OK;` |
|        - | 10240 | `	}` |
|        - | 10241 | `	/* Detach frame */` |
|       38 | 10242 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       38 | 10243 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       38 | 10244 | `		pCtx->pFrame->pParent = 0;` |
|       18 | 10245 | `	}` |
|       38 | 10246 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10247 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10248 | `		return PH7_ABORT;` |
|        - | 10249 | `	}` |
|       38 | 10250 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10251 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10252 | `		return PH7_EXCEPTION;` |
|        - | 10253 | `	}` |
|        - | 10254 | `	/* Normal completion */` |
|       38 | 10255 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       38 | 10256 | `	if( pResult ){` |
|       20 | 10257 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 | 10258 | `	}` |
|       38 | 10259 | `	return SXRET_OK;` |
|       51 | 10260 |  |
|        - | 10261 | `/*` |
|        - | 10262 | ` * Release an execution context and all its resources.` |
|        - | 10263 | ` */` |
|        4 | 10264 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 | 10265 |  |
|        5 | 10266 | `	if( pCtx == 0 ){` |
|      ! 0 | 10267 | `		return;` |
|        - | 10268 | `	}` |
|        5 | 10269 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - | 10270 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 | 10271 | `		return;` |
|        - | 10272 | `	}` |
|        5 | 10273 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - | 10274 | `	/* Release values */` |
|        5 | 10275 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 | 10276 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - | 10277 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 | 10278 | `	if( pCtx->pFrame ){` |
|        - | 10279 | `		VmSlot *aSlot;` |
|        - | 10280 | `		sxu32 n;` |
|        - | 10281 | `		/* Free local variables */` |
|        5 | 10282 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 | 10283 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 | 10284 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 | 10285 | `		}` |
|        - | 10286 | `		/* Remove local references */` |
|        5 | 10287 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 | 10288 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 | 10289 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 | 10290 | `		}` |
|        5 | 10291 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 | 10292 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 | 10293 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 | 10294 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 | 10295 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 | 10296 | `		pCtx->pFrame = 0;` |
|        2 | 10297 | `	}` |
|        - | 10298 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - | 10299 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - | 10300 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 | 10301 | `	if( pCtx->pStack ){` |
|        5 | 10302 | `		if( pCtx->nTos >= 0 ){` |
|        5 | 10303 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 | 10304 | `			while( pTos >= pCtx->pStack ){` |
|        5 | 10305 | `				PH7_MemObjRelease(pTos);` |
|        5 | 10306 | `				pTos--;` |
|        1 | 10307 | `			}` |
|        2 | 10308 | `		}` |
|        5 | 10309 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 | 10310 | `		pCtx->pStack = 0;` |
|        2 | 10311 | `	}` |
|        - | 10312 | `	/* Free the context itself */` |
|        5 | 10313 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 | 10314 |  |
|        - | 10315 | `/*` |
|        - | 10316 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - | 10317 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - | 10318 | ` */` |
|       90 | 10319 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 | 10320 |  |
|        - | 10321 | `	ph7_class_instance *pThis;` |
|        - | 10322 | `	SyString sAttr;` |
|        - | 10323 | `	ph7_value *pAttr;` |
|       92 | 10324 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10325 | `		return 0;` |
|        - | 10326 | `	}` |
|       92 | 10327 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 | 10328 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 | 10329 | `		return 0;` |
|        - | 10330 | `	}` |
|       92 | 10331 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 | 10332 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 | 10333 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 | 10334 | `		return 0;` |
|        - | 10335 | `	}` |
|       62 | 10336 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 | 10337 |  |
|        - | 10338 | `/*` |
|        - | 10339 | ` * Fiber::suspend($value = null) — static method.` |
|        - | 10340 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - | 10341 | ` */` |
|       38 | 10342 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10343 |  |
|       40 | 10344 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 | 10345 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 | 10346 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10347 | `			"Cannot suspend outside of a fiber");` |
|        - | 10348 | `	}` |
|       40 | 10349 | `	if( nArg > 0 ){` |
|       40 | 10350 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 | 10351 | `	}else{` |
|      ! 0 | 10352 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - | 10353 | `	}` |
|       40 | 10354 | `	return PH7_SUSPEND;` |
|       21 | 10355 |  |
|        - | 10356 | `/*` |
|        - | 10357 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - | 10358 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - | 10359 | ` * and closure-environment binding happen with the correct argument context.` |
|        - | 10360 | ` */` |
|       24 | 10361 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10362 |  |
|        - | 10363 | `	ph7_class_instance *pThis;` |
|        - | 10364 | `	ph7_value *pAttr;` |
|        - | 10365 | `	SyString sAttrName;` |
|       26 | 10366 | `	if( nArg < 2 ){` |
|      ! 0 | 10367 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10368 | `			"Fiber::__construct() expects a callable argument");` |
|        - | 10369 | `	}` |
|       26 | 10370 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10371 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10372 | `			"Fiber::__construct(): invalid $this");` |
|        - | 10373 | `	}` |
|       26 | 10374 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 | 10375 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 | 10376 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10377 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - | 10378 | `	}` |
|        - | 10379 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 | 10380 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10381 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10382 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - | 10383 | `	}` |
|        - | 10384 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 | 10385 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 | 10386 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 | 10387 | `	if( pAttr ){` |
|       26 | 10388 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 | 10389 | `	}` |
|       26 | 10390 | `	return PH7_OK;` |
|       14 | 10391 |  |
|        - | 10392 | `/*` |
|        - | 10393 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - | 10394 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - | 10395 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - | 10396 | ` * so that start() can bind it as $this for the closure environment.` |
|        - | 10397 | ` */` |
|       24 | 10398 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - | 10399 | `	ph7_class_instance **ppThis)` |
|        2 | 10400 |  |
|       26 | 10401 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10402 | `	ph7_value *pCallable;` |
|        - | 10403 | `	SyString sAttrName;` |
|       26 | 10404 | `	*ppThis = 0;` |
|       26 | 10405 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 | 10406 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 | 10407 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 | 10408 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 | 10409 | `		return 0;` |
|        - | 10410 | `	}` |
|       26 | 10411 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10412 | `		/* String callable — look up in user functions with overload support */` |
|        - | 10413 | `		SyString sName;` |
|        - | 10414 | `		SyHashEntry *pEntry;` |
|        - | 10415 | `		ph7_vm_func *pFunc;` |
|       26 | 10416 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 | 10417 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 | 10418 | `		if( pEntry == 0 ){` |
|      ! 0 | 10419 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 | 10420 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 | 10421 | `			return 0;` |
|        - | 10422 | `		}` |
|       26 | 10423 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 | 10424 | `		return pFunc;` |
|      ! 0 | 10425 | `	}else{` |
|        - | 10426 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 | 10427 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10428 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10429 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10430 | `		if( pMethod == 0 ){` |
|      ! 0 | 10431 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10432 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 | 10433 | `			return 0;` |
|        - | 10434 | `		}` |
|      ! 0 | 10435 | `		*ppThis = pClosure;` |
|      ! 0 | 10436 | `		return &pMethod->sFunc;` |
|        - | 10437 | `	}` |
|       14 | 10438 |  |
|        - | 10439 | `/*` |
|        - | 10440 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - | 10441 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - | 10442 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - | 10443 | ` */` |
|       46 | 10444 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - | 10445 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 | 10446 |  |
|       48 | 10447 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - | 10448 | `	ph7_vm_func_arg *aFormalArg;` |
|        - | 10449 | `	sxu32 nFormal, n;` |
|        - | 10450 | `	VmSlot sSlot;` |
|        - | 10451 | `	sxi32 rc;` |
|        - | 10452 | `	/* Install $this for closure/method callables */` |
|       48 | 10453 | `	if( pClosureThis ){` |
|        - | 10454 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 | 10455 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 | 10456 | `		if( pObj ){` |
|      ! 0 | 10457 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 | 10458 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 | 10459 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 | 10460 | `		}` |
|      ! 0 | 10461 | `	}` |
|        - | 10462 | `	/* Install static variables */` |
|       48 | 10463 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - | 10464 | `		ph7_vm_func_static_var *aStatic;` |
|        - | 10465 | `		ph7_value *pVal;` |
|      ! 0 | 10466 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 | 10467 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 | 10468 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 | 10469 | `			if( pVal ){` |
|      ! 0 | 10470 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10471 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 | 10472 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 | 10473 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 | 10474 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 | 10475 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 | 10476 | `				}` |
|      ! 0 | 10477 | `			}` |
|      ! 0 | 10478 | `		}` |
|      ! 0 | 10479 | `	}` |
|        - | 10480 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       48 | 10481 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       48 | 10482 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       66 | 10483 | `	for( n = 0; n < nFormal; n++ ){` |
|        - | 10484 | `		ph7_value *pObj;` |
|       20 | 10485 | `		if( n < (sxu32)nArg ){` |
|        - | 10486 | `			/* Argument provided — install with type casting */` |
|       20 | 10487 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 | 10488 | `			if( pObj ){` |
|       20 | 10489 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - | 10490 | `				/* Type casting */` |
|       20 | 10491 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10492 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10493 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10494 | `						if( xCast ){` |
|      ! 0 | 10495 | `							xCast(pObj);` |
|      ! 0 | 10496 | `						}` |
|      ! 0 | 10497 | `					}` |
|      ! 0 | 10498 | `				}` |
|       20 | 10499 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 | 10500 | `				sSlot.pUserData = 0;` |
|       20 | 10501 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 | 10502 | `			}` |
|        9 | 10503 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - | 10504 | `			/* Default value */` |
|      ! 0 | 10505 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 | 10506 | `			if( pObj ){` |
|      ! 0 | 10507 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 | 10508 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10509 | `					return rc;` |
|        - | 10510 | `				}` |
|      ! 0 | 10511 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10512 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10513 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10514 | `						if( xCast ){` |
|      ! 0 | 10515 | `							xCast(pObj);` |
|      ! 0 | 10516 | `						}` |
|      ! 0 | 10517 | `					}` |
|      ! 0 | 10518 | `				}` |
|      ! 0 | 10519 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 | 10520 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10521 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 | 10522 | `			}` |
|      ! 0 | 10523 | `		}` |
|       11 | 10524 | `	}` |
|        - | 10525 | `	/* Install closure environment (captured variables) */` |
|       48 | 10526 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 10527 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - | 10528 | `		ph7_value *pValue;` |
|        - | 10529 | `		sxu32 iEnv;` |
|        3 | 10530 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 | 10531 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 | 10532 | `			pEnv = &aEnv[iEnv];` |
|        7 | 10533 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 | 10534 | `				continue;` |
|        - | 10535 | `			}` |
|        5 | 10536 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 | 10537 | `			if( pValue == 0 ){` |
|      ! 0 | 10538 | `				continue;` |
|        - | 10539 | `			}` |
|        5 | 10540 | `			PH7_MemObjRelease(pValue);` |
|        5 | 10541 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 | 10542 | `		}` |
|        1 | 10543 | `	}` |
|       48 | 10544 | `	return SXRET_OK;` |
|       25 | 10545 |  |
|        - | 10546 | `/*` |
|        - | 10547 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - | 10548 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - | 10549 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - | 10550 | ` */` |
|       26 | 10551 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10552 |  |
|       28 | 10553 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10554 | `	ph7_class_instance *pThis;` |
|        - | 10555 | `	ph7_class_instance *pClosureThis;` |
|        - | 10556 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10557 | `	ph7_vm_func *pFunc;` |
|        - | 10558 | `	ph7_value sResult;` |
|        - | 10559 | `	ph7_value *pCtxAttr;` |
|        - | 10560 | `	SyString sAttrName;` |
|        - | 10561 | `	sxi32 rc;` |
|       28 | 10562 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10563 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - | 10564 | `	}` |
|       28 | 10565 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10566 | `	/* Check if already started (has a __ctx) */` |
|       28 | 10567 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 | 10568 | `	if( pExecCtx != 0 ){` |
|        3 | 10569 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10570 | `			"Cannot start a fiber that has already been started");` |
|        - | 10571 | `	}` |
|        - | 10572 | `	/* Resolve callable */` |
|       26 | 10573 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 | 10574 | `	if( pFunc == 0 ){` |
|      ! 0 | 10575 | `		return PH7_EXCEPTION;` |
|        - | 10576 | `	}` |
|        - | 10577 | `	/* Create execution context now that we know the function */` |
|       26 | 10578 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 | 10579 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10580 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10581 | `			"Fiber::start(): out of memory");` |
|        - | 10582 | `	}` |
|        - | 10583 | `	/* Store context in $this->__ctx */` |
|       26 | 10584 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 | 10585 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 | 10586 | `	if( pCtxAttr ){` |
|       26 | 10587 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 | 10588 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 | 10589 | `	}` |
|        - | 10590 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - | 10591 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - | 10592 | `	 * into the fiber's frame, not the caller's. */` |
|       26 | 10593 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 | 10594 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - | 10595 | `	/* Unpack the args array and install into the frame */` |
|        - | 10596 | `	{` |
|       26 | 10597 | `		ph7_value **apValues = 0;` |
|       26 | 10598 | `		ph7_value *aStore = 0;` |
|       26 | 10599 | `		int nActual = 0;` |
|       26 | 10600 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 | 10601 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - | 10602 | `			ph7_hashmap_node *pNode;` |
|       26 | 10603 | `			sxu32 nCount = pMap->nEntry;` |
|       26 | 10604 | `			if( nCount > 0 ){` |
|        3 | 10605 | `				sxu32 idx = 0;` |
|        4 | 10606 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 10607 | `					nCount * sizeof(ph7_value *));` |
|        4 | 10608 | `				aStore = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 10609 | `					nCount * sizeof(ph7_value));` |
|        3 | 10610 | `				if( apValues && aStore ){` |
|        3 | 10611 | `					pNode = pMap->pFirst;` |
|        7 | 10612 | `					while( pNode && idx < nCount ){` |
|        - | 10613 | `						/* Snapshot each source into stable storage: VmFiberSetupFrame reserves` |
|        - | 10614 | `						 * memory objects (VmExtractMemObj) before reading the args, which can` |
|        - | 10615 | `						 * reallocate (move) pVm->aMemObj and dangle a raw pool pointer. A` |
|        - | 10616 | `						 * shallow copy is a safe source — the referent and the heap-resident` |
|        - | 10617 | `						 * blob data survive the move (same sSafeVal idiom the hashmap inserters` |
|        - | 10618 | `						 * use); it owns nothing independently, so it needs no release. */` |
|        5 | 10619 | `						ph7_value *pSrc = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 | 10620 | `						if( pSrc ){` |
|        5 | 10621 | `							aStore[idx] = *pSrc;` |
|        3 | 10622 | `						}else{` |
|      ! 0 | 10623 | `							PH7_MemObjInit(pVm, &aStore[idx]);` |
|        - | 10624 | `						}` |
|        5 | 10625 | `						apValues[idx] = &aStore[idx];` |
|        5 | 10626 | `						idx++;` |
|        5 | 10627 | `						pNode = pNode->pPrev;` |
|        1 | 10628 | `					}` |
|        3 | 10629 | `					nActual = (int)idx;` |
|        1 | 10630 | `				}` |
|        1 | 10631 | `			}` |
|       12 | 10632 | `		}` |
|       26 | 10633 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 | 10634 | `		if( aStore ){` |
|        3 | 10635 | `			SyMemBackendFree(&pVm->sAllocator, aStore);` |
|        1 | 10636 | `		}` |
|       26 | 10637 | `		if( apValues ){` |
|        3 | 10638 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 | 10639 | `		}` |
|        - | 10640 | `	}` |
|        - | 10641 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 | 10642 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 | 10643 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 | 10644 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10645 | `		return PH7_ABORT;` |
|        - | 10646 | `	}` |
|       26 | 10647 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 | 10648 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 | 10649 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10650 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10651 | `		return PH7_ABORT;` |
|        - | 10652 | `	}` |
|       26 | 10653 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10654 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10655 | `		return PH7_EXCEPTION;` |
|        - | 10656 | `	}` |
|       26 | 10657 | `	ph7_result_value(pCtx, &sResult);` |
|       26 | 10658 | `	PH7_MemObjRelease(&sResult);` |
|       26 | 10659 | `	return PH7_OK;` |
|       15 | 10660 |  |
|        - | 10661 | `/*` |
|        - | 10662 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - | 10663 | ` */` |
|       36 | 10664 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10665 |  |
|       38 | 10666 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10667 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10668 | `	ph7_value sResult;` |
|        - | 10669 | `	ph7_value *pResumeVal;` |
|        - | 10670 | `	sxi32 rc;` |
|       38 | 10671 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10672 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 | 10673 | `		return PH7_OK;` |
|        - | 10674 | `	}` |
|       38 | 10675 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 | 10676 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10677 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 | 10678 | `		return PH7_OK;` |
|        - | 10679 | `	}` |
|       38 | 10680 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10681 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10682 | `			"Cannot resume a fiber that is not suspended");` |
|        - | 10683 | `	}` |
|       36 | 10684 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 | 10685 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 | 10686 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 | 10687 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10688 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10689 | `		return PH7_ABORT;` |
|        - | 10690 | `	}` |
|       36 | 10691 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10692 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10693 | `		return PH7_EXCEPTION;` |
|        - | 10694 | `	}` |
|       36 | 10695 | `	ph7_result_value(pCtx, &sResult);` |
|       36 | 10696 | `	PH7_MemObjRelease(&sResult);` |
|       36 | 10697 | `	return PH7_OK;` |
|       20 | 10698 |  |
|        - | 10699 | `/*` |
|        - | 10700 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - | 10701 | ` */` |
|        6 | 10702 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10703 |  |
|        8 | 10704 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10705 | `	ph7_exec_ctx *pExecCtx;` |
|        8 | 10706 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10707 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10708 | `		return PH7_OK;` |
|        - | 10709 | `	}` |
|        8 | 10710 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 | 10711 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10712 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10713 | `		return PH7_OK;` |
|        - | 10714 | `	}` |
|        8 | 10715 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10716 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10717 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10718 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - | 10719 | `		}` |
|      ! 0 | 10720 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10721 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - | 10722 | `	}` |
|        8 | 10723 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 | 10724 | `	return PH7_OK;` |
|        5 | 10725 |  |
|        - | 10726 | `/*` |
|        - | 10727 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - | 10728 | ` */` |
|        6 | 10729 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10730 |  |
|        - | 10731 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10732 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10733 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10734 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 | 10735 | `	return PH7_OK;` |
|        4 | 10736 |  |
|      ! 0 | 10737 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10738 |  |
|        - | 10739 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 | 10740 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 | 10741 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10742 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 | 10743 | `	return PH7_OK;` |
|      ! 0 | 10744 |  |
|        6 | 10745 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10746 |  |
|        - | 10747 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10748 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10749 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10750 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 | 10751 | `	return PH7_OK;` |
|        4 | 10752 |  |
|        6 | 10753 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10754 |  |
|        - | 10755 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10756 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10757 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10758 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 | 10759 | `	return PH7_OK;` |
|        4 | 10760 |  |
|        - | 10761 | `/*` |
|        - | 10762 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - | 10763 | ` */` |
|        4 | 10764 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10765 |  |
|        5 | 10766 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10767 | `	ph7_exec_ctx *pExecCtx;` |
|        5 | 10768 | `	if( nArg < 1 ){` |
|      ! 0 | 10769 | `		return PH7_OK;` |
|        - | 10770 | `	}` |
|        5 | 10771 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 | 10772 | `	if( pExecCtx ){` |
|        5 | 10773 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - | 10774 | `		/* Clear the attribute so double-free is prevented */` |
|        5 | 10775 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 | 10776 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10777 | `			SyString sAttrName;` |
|        - | 10778 | `			ph7_value *pAttr;` |
|        5 | 10779 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 | 10780 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 | 10781 | `			if( pAttr ){` |
|        5 | 10782 | `				PH7_MemObjRelease(pAttr);` |
|        2 | 10783 | `			}` |
|        2 | 10784 | `		}` |
|        2 | 10785 | `	}` |
|        5 | 10786 | `	return PH7_OK;` |
|        3 | 10787 |  |
|        - | 10788 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 | 10789 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 | 10790 |  |
|        - | 10791 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10792 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 | 10793 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 | 10794 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 | 10795 |  |
|      ! 0 | 10796 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 | 10797 |  |
|        - | 10798 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10799 | `	ph7_class_instance *pClosureThis = 0;` |
|        - | 10800 | `	ph7_exec_ctx *pCtx;` |
|        - | 10801 | `	ph7_vm_func *pFunc;` |
|        - | 10802 | `	ph7_value *pCallable;` |
|        - | 10803 | `	ph7_value *pCtxAttr;` |
|        - | 10804 | `	SyString sAttrName;` |
|        - | 10805 | `	/* Must not already be started */` |
|      ! 0 | 10806 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10807 | `	if( pCtx != 0 ){` |
|      ! 0 | 10808 | `		return SXERR_INVALID;` |
|        - | 10809 | `	}` |
|      ! 0 | 10810 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10811 | `		return SXERR_INVALID;` |
|        - | 10812 | `	}` |
|      ! 0 | 10813 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - | 10814 | `	/* Get the callable */` |
|      ! 0 | 10815 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 | 10816 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10817 | `	if( pCallable == 0 ){` |
|      ! 0 | 10818 | `		return SXERR_INVALID;` |
|        - | 10819 | `	}` |
|        - | 10820 | `	/* Resolve callable */` |
|      ! 0 | 10821 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10822 | `		SyString sName;` |
|        - | 10823 | `		SyHashEntry *pEntry;` |
|      ! 0 | 10824 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 | 10825 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 | 10826 | `		if( pEntry == 0 ){` |
|      ! 0 | 10827 | `			return SXERR_NOTFOUND;` |
|        - | 10828 | `		}` |
|      ! 0 | 10829 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 | 10830 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10831 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10832 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10833 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10834 | `		if( pMethod == 0 ){` |
|      ! 0 | 10835 | `			return SXERR_INVALID;` |
|        - | 10836 | `		}` |
|      ! 0 | 10837 | `		pClosureThis = pClosure;` |
|      ! 0 | 10838 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 | 10839 | `	}else{` |
|      ! 0 | 10840 | `		return SXERR_INVALID;` |
|        - | 10841 | `	}` |
|        - | 10842 | `	/* Create context */` |
|      ! 0 | 10843 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 | 10844 | `	if( pCtx == 0 ){` |
|      ! 0 | 10845 | `		return SXERR_MEM;` |
|        - | 10846 | `	}` |
|        - | 10847 | `	/* Store in __ctx */` |
|      ! 0 | 10848 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 10849 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10850 | `	if( pCtxAttr ){` |
|      ! 0 | 10851 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 | 10852 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 | 10853 | `	}` |
|        - | 10854 | `	/* Set up frame with args */` |
|      ! 0 | 10855 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 | 10856 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 | 10857 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 | 10858 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 | 10859 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 | 10860 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 | 10861 |  |
|      ! 0 | 10862 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 | 10863 |  |
|      ! 0 | 10864 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10865 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 | 10866 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 | 10867 |  |
|      ! 0 | 10868 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10869 |  |
|      ! 0 | 10870 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10871 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 | 10872 |  |
|      ! 0 | 10873 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10874 |  |
|      ! 0 | 10875 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10876 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 | 10877 |  |
|      ! 0 | 10878 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10879 |  |
|      ! 0 | 10880 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10881 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 | 10882 | `	return &pCtx->sRetValue;` |
|      ! 0 | 10883 |  |
|        - | 10884 | `/* ======================== Generator Infrastructure ======================== */` |
|        - | 10885 | `/*` |
|        - | 10886 | ` * Allocate a new generator wrapper around an execution context.` |
|        - | 10887 | ` */` |
|       22 | 10888 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 | 10889 |  |
|        - | 10890 | `	ph7_generator *pGen;` |
|       24 | 10891 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       24 | 10892 | `	if( pGen == 0 ){` |
|      ! 0 | 10893 | `		return 0;` |
|        - | 10894 | `	}` |
|       24 | 10895 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       24 | 10896 | `	pGen->pCtx = pCtx;` |
|       24 | 10897 | `	pGen->iImplicitKey = 0;` |
|       24 | 10898 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       24 | 10899 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - | 10900 | `	/* Link the generator back to the exec context */` |
|       24 | 10901 | `	pCtx->pPrivate = pGen;` |
|       24 | 10902 | `	return pGen;` |
|       13 | 10903 |  |
|        - | 10904 | `/*` |
|        - | 10905 | ` * Release a generator and its execution context.` |
|        - | 10906 | ` */` |
|      ! 0 | 10907 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 | 10908 |  |
|      ! 0 | 10909 | `	if( pGen == 0 ){` |
|      ! 0 | 10910 | `		return;` |
|        - | 10911 | `	}` |
|      ! 0 | 10912 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 10913 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 10914 | `	if( pGen->pCtx ){` |
|      ! 0 | 10915 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 | 10916 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 | 10917 | `		pGen->pCtx = 0;` |
|      ! 0 | 10918 | `	}` |
|      ! 0 | 10919 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 | 10920 |  |
|        - | 10921 | `/*` |
|        - | 10922 | ` * Extract ph7_generator from a Generator class instance.` |
|        - | 10923 | ` */` |
|      236 | 10924 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 | 10925 |  |
|        - | 10926 | `	ph7_class_instance *pThis;` |
|        - | 10927 | `	SyString sAttr;` |
|        - | 10928 | `	ph7_value *pAttr;` |
|      238 | 10929 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10930 | `		return 0;` |
|        - | 10931 | `	}` |
|      238 | 10932 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      238 | 10933 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 | 10934 | `		return 0;` |
|        - | 10935 | `	}` |
|      238 | 10936 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      238 | 10937 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      238 | 10938 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 | 10939 | `		return 0;` |
|        - | 10940 | `	}` |
|      238 | 10941 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      120 | 10942 |  |
|        - | 10943 | `/*` |
|        - | 10944 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - | 10945 | ` */` |
|       22 | 10946 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10947 |  |
|        - | 10948 | `	ph7_generator *pGen;` |
|        - | 10949 | `	sxi32 rc;` |
|       24 | 10950 | `	if( nArg < 1 ) return PH7_OK;` |
|       24 | 10951 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       24 | 10952 | `	if( pGen == 0 ) return PH7_OK;` |
|       24 | 10953 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       24 | 10954 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       24 | 10955 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       24 | 10956 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       11 | 10957 | `	}` |
|       24 | 10958 | `	return PH7_OK;` |
|       13 | 10959 |  |
|        - | 10960 | `/*` |
|        - | 10961 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - | 10962 | ` */` |
|       68 | 10963 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10964 |  |
|        - | 10965 | `	ph7_generator *pGen;` |
|       70 | 10966 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       70 | 10967 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 10968 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       70 | 10969 | `	return PH7_OK;` |
|       36 | 10970 |  |
|        - | 10971 | `/*` |
|        - | 10972 | ` * Generator::current() — return the last yielded value.` |
|        - | 10973 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 10974 | ` */` |
|       68 | 10975 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10976 |  |
|        - | 10977 | `	ph7_generator *pGen;` |
|        - | 10978 | `	sxi32 rc;` |
|       70 | 10979 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 10980 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 10981 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 10982 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10983 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 10984 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 10985 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 10986 | `	}` |
|       70 | 10987 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 | 10988 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       36 | 10989 | `	}else{` |
|      ! 0 | 10990 | `		ph7_result_null(pCtx);` |
|        - | 10991 | `	}` |
|       70 | 10992 | `	return PH7_OK;` |
|       36 | 10993 |  |
|        - | 10994 | `/*` |
|        - | 10995 | ` * Generator::key() — return the last yielded key.` |
|        - | 10996 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 10997 | ` */` |
|       12 | 10998 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10999 |  |
|        - | 11000 | `	ph7_generator *pGen;` |
|        - | 11001 | `	sxi32 rc;` |
|       13 | 11002 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 11003 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 | 11004 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 11005 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11006 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 11007 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 11008 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 11009 | `	}` |
|       13 | 11010 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 | 11011 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 | 11012 | `	}else{` |
|      ! 0 | 11013 | `		ph7_result_null(pCtx);` |
|        - | 11014 | `	}` |
|       13 | 11015 | `	return PH7_OK;` |
|        7 | 11016 |  |
|        - | 11017 | `/*` |
|        - | 11018 | ` * Generator::next() — advance to the next yield point.` |
|        - | 11019 | ` */` |
|       60 | 11020 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 11021 |  |
|        - | 11022 | `	ph7_generator *pGen;` |
|        - | 11023 | `	sxi32 rc;` |
|       62 | 11024 | `	if( nArg < 1 ) return PH7_OK;` |
|       62 | 11025 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       62 | 11026 | `	if( pGen == 0 ) return PH7_OK;` |
|       62 | 11027 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 11028 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       62 | 11029 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       62 | 11030 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       32 | 11031 | `	}else{` |
|      ! 0 | 11032 | `		return PH7_OK;` |
|        - | 11033 | `	}` |
|       62 | 11034 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       62 | 11035 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       62 | 11036 | `	return PH7_OK;` |
|       32 | 11037 |  |
|        - | 11038 | `/*` |
|        - | 11039 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - | 11040 | ` */` |
|        4 | 11041 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11042 |  |
|        - | 11043 | `	ph7_generator *pGen;` |
|        - | 11044 | `	ph7_value *pSendVal;` |
|        - | 11045 | `	sxi32 rc;` |
|        5 | 11046 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 | 11047 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 | 11048 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 | 11049 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 | 11050 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - | 11051 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 | 11052 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 | 11053 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 | 11054 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 | 11055 | `	}else{` |
|      ! 0 | 11056 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11057 | `		return PH7_OK;` |
|        - | 11058 | `	}` |
|        5 | 11059 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 | 11060 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 | 11061 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 11062 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 | 11063 | `	}else{` |
|        3 | 11064 | `		ph7_result_null(pCtx);` |
|        - | 11065 | `	}` |
|        5 | 11066 | `	return PH7_OK;` |
|        3 | 11067 |  |
|        - | 11068 | `/*` |
|        - | 11069 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - | 11070 | ` *` |
|        - | 11071 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - | 11072 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - | 11073 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - | 11074 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - | 11075 | ` * the exception to the caller.` |
|        - | 11076 | ` */` |
|      ! 0 | 11077 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11078 |  |
|        - | 11079 | `	ph7_generator *pGen;` |
|        - | 11080 | `	const char *zMsg;` |
|        - | 11081 | `	int nLen;` |
|      ! 0 | 11082 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 | 11083 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11084 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 | 11085 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 | 11086 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 | 11087 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 11088 | `			"Cannot throw into a closed generator");` |
|        - | 11089 | `	}` |
|        - | 11090 | `	/* Close the generator. Re-throw the exception properly via` |
|        - | 11091 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - | 11092 | `	 * exception dispatch path works correctly. Extract the message` |
|        - | 11093 | `	 * from the passed exception object if possible. */` |
|      ! 0 | 11094 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 11095 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 | 11096 | `	nLen = 0;` |
|      ! 0 | 11097 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - | 11098 | `		/* Try to get the exception's message */` |
|        - | 11099 | `		SyString sAttr;` |
|        - | 11100 | `		ph7_value *pMsgAttr;` |
|      ! 0 | 11101 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 | 11102 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 | 11103 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 | 11104 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 | 11105 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 | 11106 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 | 11107 | `		}` |
|      ! 0 | 11108 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 11109 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 | 11110 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 | 11111 | `	}` |
|      ! 0 | 11112 | `	(void)nLen;` |
|      ! 0 | 11113 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 | 11114 |  |
|        - | 11115 | `/*` |
|        - | 11116 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - | 11117 | ` */` |
|        2 | 11118 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 11119 |  |
|        - | 11120 | `	ph7_generator *pGen;` |
|        3 | 11121 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 11122 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 | 11123 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 11124 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 11125 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 11126 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - | 11127 | `	}` |
|        3 | 11128 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 | 11129 | `	return PH7_OK;` |
|        2 | 11130 |  |
|        - | 11131 | `/*` |
|        - | 11132 | ` * Generator::__destruct() — clean up.` |
|        - | 11133 | ` */` |
|      ! 0 | 11134 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 11135 |  |
|        - | 11136 | `	ph7_generator *pGen;` |
|      ! 0 | 11137 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 | 11138 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 11139 | `	if( pGen ){` |
|      ! 0 | 11140 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 | 11141 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 11142 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 11143 | `			SyString sAttrName;` |
|        - | 11144 | `			ph7_value *pAttr;` |
|      ! 0 | 11145 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 11146 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 11147 | `			if( pAttr ){` |
|      ! 0 | 11148 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 | 11149 | `			}` |
|      ! 0 | 11150 | `		}` |
|      ! 0 | 11151 | `	}` |
|      ! 0 | 11152 | `	return PH7_OK;` |
|      ! 0 | 11153 |  |
|        - | 11154 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - | 11155 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - | 11156 | `/*` |
|        - | 11157 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 11158 | ` * the desired message.` |
|        - | 11159 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 11160 | ` * in 'api.c' for additional information.` |
|        - | 11161 | ` */` |
|      370 | 11162 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 11163 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 11164 | `	SyString *pString /* Message to output */` |
|        - | 11165 | `	)` |
|        2 | 11166 |  |
|      372 | 11167 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 | 11168 | `	sxi32 rc = SXRET_OK;` |
|        - | 11169 | `	/* Call the output consumer */` |
|      372 | 11170 | `	if( pString->nByte > 0 ){` |
|      372 | 11171 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 | 11172 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 | 11173 | `	}` |
|      372 | 11174 | `	return rc;` |
|        2 | 11175 |  |
|        - | 11176 | `/*` |
|        - | 11177 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 11178 | ` * callback to consume the formatted message.` |
|        - | 11179 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 11180 | ` * in 'api.c' for additional information.` |
|        - | 11181 | ` */` |
|        2 | 11182 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 11183 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 11184 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 11185 | `	va_list ap           /* Variable list of arguments */` |
|        - | 11186 | `	)` |
|        1 | 11187 |  |
|        3 | 11188 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 11189 | `	sxi32 rc = SXRET_OK;` |
|        - | 11190 | `	SyBlob sWorker;` |
|        - | 11191 | `	/* Format the message and call the output consumer */` |
|        3 | 11192 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 11193 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 11194 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 11195 | `		/* Consume the formatted message */` |
|        3 | 11196 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 11197 | `	}` |
|        3 | 11198 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 11199 | `	/* Release the working buffer */` |
|        3 | 11200 | `	SyBlobRelease(&sWorker);` |
|        3 | 11201 | `	return rc;` |
|        1 | 11202 |  |
|        - | 11203 | `/*` |
|        - | 11204 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 11205 | ` * This function never fail and always return a pointer` |
|        - | 11206 | ` * to a null terminated string.` |
|        - | 11207 | ` */` |
|       12 | 11208 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 11209 |  |
|       13 | 11210 | `	const char *zOp = "Unknown     ";` |
|       13 | 11211 | `	switch(nOp){` |
|        3 | 11212 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 11213 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 11214 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 11215 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 11216 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 11217 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 11218 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 11219 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 11220 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 11221 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 11222 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 11223 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 11224 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 11225 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 11226 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 11227 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 11228 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 11229 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 11230 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 11231 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 11232 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 11233 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 11234 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 11235 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 11236 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 11237 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 11238 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 11239 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 11240 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 11241 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 11242 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 11243 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 11244 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 11245 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 11246 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 11247 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 11248 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 11249 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 11250 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 11251 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 11252 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 11253 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 11254 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 11255 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 11256 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 11257 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 11258 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 11259 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 11260 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 11261 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 11262 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 11263 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 11264 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 11265 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 11266 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 11267 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 11268 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 11269 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 11270 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 11271 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 11272 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 | 11273 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 11274 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 11275 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 11276 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 11277 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 11278 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 11279 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 11280 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 | 11281 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 | 11282 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 11283 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 11284 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 11285 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 11286 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 11287 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 11288 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 11289 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 11290 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 11291 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 11292 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 11293 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 11294 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 11295 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 11296 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 11297 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 11298 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 11299 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 11300 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 11301 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 11302 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 11303 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 11304 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 11305 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 11306 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 11307 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 11308 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 11309 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 11310 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 11311 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 11312 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 11313 | `	default:` |
|      ! 0 | 11314 | `		break;` |
|        - | 11315 | `	}` |
|       13 | 11316 | `	return zOp;` |
|        1 | 11317 |  |
|        - | 11318 | `/*` |
|        - | 11319 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 11320 | ` * The xConsumer() callback which is an used defined function` |
|        - | 11321 | ` * is responsible of consuming the generated dump.` |
|        - | 11322 | ` */` |
|        2 | 11323 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 11324 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 11325 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 11326 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 11327 | `	)` |
|        1 | 11328 |  |
|        - | 11329 | `	sxi32 rc;` |
|        3 | 11330 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 11331 | `	return rc;` |
|        1 | 11332 |  |
|        - | 11333 | `/*` |
|        - | 11334 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 11335 | ` * outside a class body [i.e: global or function scope].` |
|        - | 11336 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 11337 | ` * in 'compile.c' for additional information.` |
|        - | 11338 | ` */` |
|       14 | 11339 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 | 11340 |  |
|       15 | 11341 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 11342 | `	/* Evaluate and expand constant value */` |
|       15 | 11343 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 | 11344 |  |
|        - | 11345 | `/*` |
|        - | 11346 | ` * Section:` |
|        - | 11347 | ` *  Function handling functions.` |
|        - | 11348 | ` * Status:` |
|        - | 11349 | ` *    Stable.` |
|        - | 11350 | ` */` |
|        - | 11351 | `/*` |
|        - | 11352 | ` * int func_num_args(void)` |
|        - | 11353 | ` *   Returns the number of arguments passed to the function.` |
|        - | 11354 | ` * Parameters` |
|        - | 11355 | ` *   None.` |
|        - | 11356 | ` * Return` |
|        - | 11357 | ` *  Total number of arguments passed into the current user-defined function` |
|        - | 11358 | ` *  or -1 if called from the globe scope.` |
|        - | 11359 | ` */` |
|      980 | 11360 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11361 |  |
|        - | 11362 | `	VmFrame *pFrame;` |
|        - | 11363 | `	ph7_vm *pVm;` |
|        - | 11364 | `	/* Point to the target VM */` |
|      982 | 11365 | `	pVm = pCtx->pVm;` |
|        - | 11366 | `	/* Current frame */` |
|      982 | 11367 | `	pFrame = pVm->pFrame;` |
|      982 | 11368 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      982 | 11369 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 | 11370 | `		SXUNUSED(nArg);` |
|      ! 0 | 11371 | `		SXUNUSED(apArg);` |
|        - | 11372 | `		/* Global frame,return -1 */` |
|      ! 0 | 11373 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 | 11374 | `		return SXRET_OK;` |
|        - | 11375 | `	}` |
|        - | 11376 | `	/* Total number of arguments passed to the enclosing function */` |
|      982 | 11377 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      982 | 11378 | `	ph7_result_int(pCtx,nArg);` |
|      982 | 11379 | `	return SXRET_OK;` |
|      492 | 11380 |  |
|        - | 11381 | `/*` |
|        - | 11382 | ` * value func_get_arg(int $arg_num)` |
|        - | 11383 | ` *   Return an item from the argument list.` |
|        - | 11384 | ` * Parameters` |
|        - | 11385 | ` *  Argument number(index start from zero).` |
|        - | 11386 | ` * Return` |
|        - | 11387 | ` *  Returns the specified argument or FALSE on error.` |
|        - | 11388 | ` */` |
|       22 | 11389 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11390 |  |
|       24 | 11391 | `	ph7_value *pObj = 0;` |
|       24 | 11392 | `	VmSlot *pSlot = 0;` |
|        - | 11393 | `	VmFrame *pFrame;` |
|        - | 11394 | `	ph7_vm *pVm;` |
|        - | 11395 | `	/* Point to the target VM */` |
|       24 | 11396 | `	pVm = pCtx->pVm;` |
|        - | 11397 | `	/* Current frame */` |
|       24 | 11398 | `	pFrame = pVm->pFrame;` |
|       24 | 11399 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 | 11400 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - | 11401 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 | 11402 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 | 11403 | `		ph7_result_bool(pCtx,0);` |
|        3 | 11404 | `		return SXRET_OK;` |
|        - | 11405 | `	}` |
|        - | 11406 | `	/* Extract the desired index */` |
|       21 | 11407 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 | 11408 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - | 11409 | `		/* Invalid index,return FALSE */` |
|      ! 0 | 11410 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11411 | `		return SXRET_OK;` |
|        - | 11412 | `	}` |
|        - | 11413 | `	/* Extract the desired argument */` |
|       21 | 11414 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 | 11415 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - | 11416 | `			/* Return the desired argument */` |
|       21 | 11417 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 | 11418 | `		}else{` |
|        - | 11419 | `			/* No such argument,return false */` |
|      ! 0 | 11420 | `			ph7_result_bool(pCtx,0);` |
|        - | 11421 | `		}` |
|       11 | 11422 | `	}else{` |
|        - | 11423 | `		/* CAN'T HAPPEN */` |
|      ! 0 | 11424 | `		ph7_result_bool(pCtx,0);` |
|        - | 11425 | `	}` |
|       21 | 11426 | `	return SXRET_OK;` |
|       13 | 11427 |  |
|        - | 11428 | `/*` |
|        - | 11429 | ` * array func_get_args_byref(void)` |
|        - | 11430 | ` *   Returns an array comprising a function's argument list.` |
|        - | 11431 | ` * Parameters` |
|        - | 11432 | ` *  None.` |
|        - | 11433 | ` * Return` |
|        - | 11434 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - | 11435 | ` *  member of the current user-defined function's argument list.` |
|        - | 11436 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11437 | ` * NOTE:` |
|        - | 11438 | ` *  Arguments are returned to the array by reference.` |
|        - | 11439 | ` */` |
|        2 | 11440 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11441 |  |
|        - | 11442 | `	ph7_value *pArray;` |
|        - | 11443 | `	VmFrame *pFrame;` |
|        - | 11444 | `	VmSlot *aSlot;` |
|        - | 11445 | `	sxu32 n;` |
|        - | 11446 | `	/* Point to the current frame */` |
|        3 | 11447 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 | 11448 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 | 11449 | `	if( pFrame->pParent == 0 ){` |
|        - | 11450 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11451 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11452 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11453 | `		return SXRET_OK;` |
|        - | 11454 | `	}` |
|        - | 11455 | `	/* Create a new array */` |
|        3 | 11456 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11457 | `	if( pArray == 0 ){` |
|      ! 0 | 11458 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11459 | `		SXUNUSED(apArg);` |
|      ! 0 | 11460 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11461 | `		return SXRET_OK;` |
|        - | 11462 | `	}` |
|        - | 11463 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 | 11464 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 | 11465 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 | 11466 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 | 11467 | `	}` |
|        - | 11468 | `	/* Return the freshly created array */` |
|        3 | 11469 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11470 | `	return SXRET_OK;` |
|        2 | 11471 |  |
|        - | 11472 | `/*` |
|        - | 11473 | ` * array func_get_args(void)` |
|        - | 11474 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - | 11475 | ` * Parameters` |
|        - | 11476 | ` *  None.` |
|        - | 11477 | ` * Return` |
|        - | 11478 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - | 11479 | ` *  member of the current user-defined function's argument list.` |
|        - | 11480 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 11481 | ` */` |
|       88 | 11482 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11483 |  |
|       90 | 11484 | `	ph7_value *pObj = 0;` |
|        - | 11485 | `	ph7_value *pArray;` |
|        - | 11486 | `	VmFrame *pFrame;` |
|        - | 11487 | `	VmSlot *aSlot;` |
|        - | 11488 | `	sxu32 n;` |
|        - | 11489 | `	/* Point to the current frame */` |
|       90 | 11490 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 | 11491 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 | 11492 | `	if( pFrame->pParent == 0 ){` |
|        - | 11493 | `		/* Global frame,return FALSE */` |
|      ! 0 | 11494 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 11495 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11496 | `		return SXRET_OK;` |
|        - | 11497 | `	}` |
|        - | 11498 | `	/* Create a new array */` |
|       90 | 11499 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 | 11500 | `	if( pArray == 0 ){` |
|      ! 0 | 11501 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11502 | `		SXUNUSED(apArg);` |
|      ! 0 | 11503 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11504 | `		return SXRET_OK;` |
|        - | 11505 | `	}` |
|        - | 11506 | `	/* Start filling the array with the given arguments */` |
|       90 | 11507 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 | 11508 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 | 11509 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 | 11510 | `		if( pObj ){` |
|      134 | 11511 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 | 11512 | `		}` |
|       68 | 11513 | `	}` |
|        - | 11514 | `	/* Return the freshly created array */` |
|       90 | 11515 | `	ph7_result_value(pCtx,pArray);` |
|       90 | 11516 | `	return SXRET_OK;` |
|       46 | 11517 |  |
|        - | 11518 | `/*` |
|        - | 11519 | ` * bool function_exists(string $name)` |
|        - | 11520 | ` *  Return TRUE if the given function has been defined.` |
|        - | 11521 | ` * Parameters` |
|        - | 11522 | ` *  The name of the desired function.` |
|        - | 11523 | ` * Return` |
|        - | 11524 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - | 11525 | ` */` |
|     1742 | 11526 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11527 |  |
|        - | 11528 | `	const char *zName;` |
|        - | 11529 | `	ph7_vm *pVm;` |
|        - | 11530 | `	int nLen;` |
|        - | 11531 | `	int res;` |
|     1744 | 11532 | `	if( nArg < 1 ){` |
|        - | 11533 | `		/* Missing argument,return FALSE */` |
|      ! 0 | 11534 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11535 | `		return SXRET_OK;` |
|        - | 11536 | `	}` |
|        - | 11537 | `	/* Point to the target VM */` |
|     1744 | 11538 | `	pVm = pCtx->pVm;` |
|        - | 11539 | `	/* Extract the function name */` |
|     1744 | 11540 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11541 | `	/* Assume the function is not defined */` |
|     1744 | 11542 | `	res = 0;` |
|        - | 11543 | `	/* Perform the lookup */` |
|     2613 | 11544 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1738 | 11545 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11546 | `			/* Function is defined */` |
|      266 | 11547 | `			res = 1;` |
|      132 | 11548 | `	}` |
|     1744 | 11549 | `	ph7_result_bool(pCtx,res);` |
|     1744 | 11550 | `	return SXRET_OK;` |
|      873 | 11551 |  |
|        - | 11552 | `/*` |
|        - | 11553 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11554 | ` * [i.e: Whether it is callable or not].` |
|        - | 11555 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - | 11556 | ` */` |
|    23864 | 11557 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 | 11558 |  |
|    23866 | 11559 | `	int res = 0;` |
|    23866 | 11560 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11561 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|        - | 11562 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|        - | 11563 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|        - | 11564 | `		 * standard PHP behavior. */` |
|       20 | 11565 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|       20 | 11566 | `		if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|       18 | 11567 | `			res = 1;` |
|       10 | 11568 | `		}` |
|        9 | 11569 | `		(void)CallInvoke;` |
|    23857 | 11570 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 | 11571 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 | 11572 | `		if( pMap->nEntry == 2 ){` |
|        - | 11573 | `			ph7_class *pClass;` |
|        - | 11574 | `			ph7_value *pV;` |
|        - | 11575 | `			/* Extract the target class */` |
|       12 | 11576 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 | 11577 | `			if( pV ){` |
|       12 | 11578 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 | 11579 | `				if( pClass ){` |
|        - | 11580 | `					ph7_class_method *pMethod;` |
|        - | 11581 | `					/* Extract the target method */` |
|       10 | 11582 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 | 11583 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - | 11584 | `						/* Perform the lookup */` |
|       10 | 11585 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 | 11586 | `						if( pMethod ){` |
|        - | 11587 | `							/* Method is callable */` |
|        5 | 11588 | `							res = 1;` |
|        2 | 11589 | `						}` |
|        4 | 11590 | `					}` |
|        4 | 11591 | `				}` |
|        5 | 11592 | `			}` |
|        7 | 11593 | `		}` |
|    23835 | 11594 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 11595 | `		const char *zName;` |
|        - | 11596 | `		int nLen;` |
|        - | 11597 | `		/* Extract the name */` |
|     5872 | 11598 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - | 11599 | `		/* Perform the lookup */` |
|     5887 | 11600 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 | 11601 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11602 | `				/* Function is callable */` |
|     5854 | 11603 | `				res = 1;` |
|     2926 | 11604 | `		}` |
|     2935 | 11605 | `	}` |
|    23866 | 11606 | `	return res;` |
|        2 | 11607 |  |
|        - | 11608 | `/*` |
|        - | 11609 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - | 11610 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11611 | ` * Parameters` |
|        - | 11612 | ` * $name` |
|        - | 11613 | ` *    The callback function to check` |
|        - | 11614 | ` * $syntax_only` |
|        - | 11615 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - | 11616 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - | 11617 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - | 11618 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - | 11619 | ` *    a string.` |
|        - | 11620 | ` * Return` |
|        - | 11621 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - | 11622 | ` */` |
|       20 | 11623 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11624 |  |
|        - | 11625 | `	ph7_vm *pVm;` |
|        - | 11626 | `	int res;` |
|       21 | 11627 | `	if( nArg < 1 ){` |
|        - | 11628 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 11629 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11630 | `		return SXRET_OK;` |
|        - | 11631 | `	}` |
|        - | 11632 | `	/* Point to the target VM */` |
|       21 | 11633 | `	pVm = pCtx->pVm;` |
|        - | 11634 | `	/* Perform the requested operation */` |
|       21 | 11635 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       21 | 11636 | `	ph7_result_bool(pCtx,res);` |
|       21 | 11637 | `	return SXRET_OK;` |
|       11 | 11638 |  |
|        - | 11639 | `/*` |
|        - | 11640 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - | 11641 | ` * defined below.` |
|        - | 11642 | ` */` |
|     1306 | 11643 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11644 |  |
|     1307 | 11645 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11646 | `	ph7_value sName;` |
|        - | 11647 | `	sxi32 rc;` |
|        - | 11648 | `	/* Prepare the function name for insertion */` |
|     1307 | 11649 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1307 | 11650 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11651 | `	/* Perform the insertion */` |
|     1307 | 11652 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1307 | 11653 | `	PH7_MemObjRelease(&sName);` |
|     1307 | 11654 | `	return rc;` |
|        1 | 11655 |  |
|        - | 11656 | `/*` |
|        - | 11657 | ` * array get_defined_functions(void)` |
|        - | 11658 | ` *  Returns an array of all defined functions.` |
|        - | 11659 | ` * Parameter` |
|        - | 11660 | ` *  None.` |
|        - | 11661 | ` * Return` |
|        - | 11662 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 11663 | ` *  both built-in (internal) and user-defined.` |
|        - | 11664 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 11665 | ` *  defined ones using $arr["user"].` |
|        - | 11666 | ` * Note:` |
|        - | 11667 | ` *  NULL is returned on failure.` |
|        - | 11668 | ` */` |
|        2 | 11669 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11670 |  |
|        - | 11671 | `	ph7_value *pArray,*pEntry;` |
|        - | 11672 | `	/* NOTE:` |
|        - | 11673 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 11674 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 11675 | `	 */` |
|        3 | 11676 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11677 | ` 	if( pArray == 0 ){` |
|      ! 0 | 11678 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11679 | `		SXUNUSED(apArg);` |
|        - | 11680 | `		/* Return NULL */` |
|      ! 0 | 11681 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11682 | `		return SXRET_OK;` |
|        - | 11683 | `	}` |
|        3 | 11684 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11685 | `	if( pEntry == 0 ){` |
|        - | 11686 | `		/* Return NULL */` |
|      ! 0 | 11687 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11688 | `		return SXRET_OK;` |
|        - | 11689 | `	}` |
|        - | 11690 | `	/* Fill with the appropriate information */` |
|        3 | 11691 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 11692 | `	/* Create the 'internal' index */` |
|        3 | 11693 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 11694 | `	/* Create the user-func array */` |
|        3 | 11695 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11696 | `	if( pEntry == 0 ){` |
|        - | 11697 | `		/* Return NULL */` |
|      ! 0 | 11698 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11699 | `		return SXRET_OK;` |
|        - | 11700 | `	}` |
|        - | 11701 | `	/* Fill with the appropriate information */` |
|        3 | 11702 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 11703 | `	/* Create the 'user' index */` |
|        3 | 11704 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 11705 | `	/* Return the multi-dimensional array */` |
|        3 | 11706 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11707 | `	return SXRET_OK;` |
|        2 | 11708 |  |
|        - | 11709 | `/*` |
|        - | 11710 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 11711 | ` *  Register a function for execution on shutdown.` |
|        - | 11712 | ` * Note` |
|        - | 11713 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 11714 | ` *  be called in the same order as they were registered.` |
|        - | 11715 | ` * Parameters` |
|        - | 11716 | ` *  $callback` |
|        - | 11717 | ` *   The shutdown callback to register.` |
|        - | 11718 | ` * $param` |
|        - | 11719 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 11720 | ` * Return` |
|        - | 11721 | ` *  Nothing.` |
|        - | 11722 | ` */` |
|       10 | 11723 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11724 |  |
|        - | 11725 | `	VmShutdownCB sEntry;` |
|        - | 11726 | `	int i,j;` |
|       12 | 11727 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11728 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 11729 | `		return PH7_OK;` |
|        - | 11730 | `	}` |
|        - | 11731 | `	/* Zero the Entry */` |
|       12 | 11732 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 11733 | `	/* Initialize fields */` |
|       12 | 11734 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 11735 | `	/* Save the callback name for later invocation name */` |
|       12 | 11736 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|      112 | 11737 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|      102 | 11738 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       52 | 11739 | `	}` |
|        - | 11740 | `	/* Copy arguments */` |
|       12 | 11741 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 11742 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 11743 | `			/* Limit reached */` |
|      ! 0 | 11744 | `			break;` |
|        - | 11745 | `		}` |
|      ! 0 | 11746 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 11747 | `	}` |
|       12 | 11748 | `	sEntry.nArg = j;` |
|        - | 11749 | `	/* Install the callback */` |
|       12 | 11750 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|       12 | 11751 | `	return PH7_OK;` |
|        7 | 11752 |  |
|        - | 11753 | `/*` |
|        - | 11754 | ` * Section:` |
|        - | 11755 | ` *  Class handling functions.` |
|        - | 11756 | ` * Status:` |
|        - | 11757 | ` *    Stable.` |
|        - | 11758 | ` */` |
|        - | 11759 | `/*` |
|        - | 11760 | ` * Extract the top active class. NULL is returned` |
|        - | 11761 | ` * if the class stack is empty.` |
|        - | 11762 | ` */` |
|      986 | 11763 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 | 11764 |  |
|      988 | 11765 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 11766 | `	ph7_class **apClass;` |
|      988 | 11767 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 11768 | `		/* Empty stack,return NULL */` |
|       15 | 11769 | `		return 0;` |
|        - | 11770 | `	}` |
|        - | 11771 | `	/* Peek the last entry */` |
|      974 | 11772 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      974 | 11773 | `	return apClass[pSet->nUsed - 1];` |
|      495 | 11774 |  |
|        - | 11775 | `/*` |
|        - | 11776 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 11777 | ` *   Get the class that declared the currently executing method.` |
|        - | 11778 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 11779 | ` *` |
|        - | 11780 | ` * Parameters` |
|        - | 11781 | ` *   pVm: Target VM` |
|        - | 11782 | ` *` |
|        - | 11783 | ` * Return` |
|        - | 11784 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 11785 | ` *   - Not executing within a class method` |
|        - | 11786 | ` *` |
|        - | 11787 | ` * Note` |
|        - | 11788 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 11789 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 11790 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 11791 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 11792 | ` *   declaring class.` |
|        - | 11793 | ` */` |
|       98 | 11794 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 | 11795 |  |
|      100 | 11796 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 11797 | `	ph7_vm_func *pVmFunc;` |
|        - | 11798 |  |
|        - | 11799 | `	/* Skip exception frames to find the actual method frame */` |
|      100 | 11800 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 11801 |  |
|        - | 11802 | `	/* Check if we're in a method context */` |
|      100 | 11803 | `	if( pFrame->pParent ){` |
|       96 | 11804 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       96 | 11805 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 11806 | `			/* Return the declaring class */` |
|       96 | 11807 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 11808 | `		}` |
|      ! 0 | 11809 | `	}` |
|        - | 11810 |  |
|        5 | 11811 | `	return 0;` |
|       51 | 11812 |  |
|        - | 11813 |  |
|        - | 11814 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 11815 | `/*` |
|        - | 11816 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 11817 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 11818 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 11819 | ` * return value indicates failure.` |
|        - | 11820 | ` */` |
|        - | 11821 | `/*` |
|        - | 11822 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 11823 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 11824 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 11825 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 11826 | ` */` |
|     2482 | 11827 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 11828 | `	ph7_vm *pVm,` |
|        - | 11829 | `	ph7_class_instance *pThis,` |
|        - | 11830 | `	ph7_class_method *pMethod,` |
|        - | 11831 | `	ph7_value *pResult,` |
|        - | 11832 | `	int nArg,` |
|        - | 11833 | `	ph7_value **apArg,` |
|        - | 11834 | `	VmCallArgMap *pMap` |
|        - | 11835 | `	)` |
|        2 | 11836 |  |
|        - | 11837 | `	ph7_value *aStack;` |
|        - | 11838 | `	VmInstr aInstr[2];` |
|        - | 11839 | `	int iCursor;` |
|        - | 11840 | `	int i;` |
|        - | 11841 | `	sxi32 rc;` |
|     2484 | 11842 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     2484 | 11843 | `	if( aStack == 0 ){` |
|      ! 0 | 11844 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 11845 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 11846 | `		return SXERR_MEM;` |
|        - | 11847 | `	}` |
|     4028 | 11848 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1546 | 11849 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     1546 | 11850 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      774 | 11851 | `	}` |
|     2484 | 11852 | `	iCursor = nArg + 1;` |
|     2484 | 11853 | `	if( pThis ){` |
|     2478 | 11854 | `		pThis->iRef++;` |
|     2478 | 11855 | `		aStack[i].x.pOther = pThis;` |
|     2478 | 11856 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|     1238 | 11857 | `	}` |
|     2484 | 11858 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2484 | 11859 | `	i++;` |
|     2484 | 11860 | `	SyBlobReset(&aStack[i].sBlob);` |
|     2484 | 11861 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     2484 | 11862 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     2484 | 11863 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2484 | 11864 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     2484 | 11865 | `	aInstr[0].iP1 = nArg;` |
|     2484 | 11866 | `	aInstr[0].iP2 = 0;` |
|     2484 | 11867 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     2484 | 11868 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     2484 | 11869 | `	aInstr[1].iP1 = 1;` |
|     2484 | 11870 | `	aInstr[1].iP2 = 0;` |
|     2484 | 11871 | `	aInstr[1].p3  = 0;` |
|     2484 | 11872 | `	rc = VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0);` |
|     2484 | 11873 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 11874 | `	/* Propagate the real exec status (PH7_EXCEPTION / PH7_ABORT) so callers` |
|        - | 11875 | `	 * can unwind instead of continuing past a method that raised. */` |
|     2484 | 11876 | `	return rc;` |
|     1243 | 11877 |  |
|     1924 | 11878 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 11879 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 11880 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 11881 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 11882 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 11883 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 11884 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 11885 | `	)` |
|        2 | 11886 |  |
|     1926 | 11887 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 11888 |  |
|        - | 11889 | `/*` |
|        - | 11890 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|        - | 11891 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|        - | 11892 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|        - | 11893 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|        - | 11894 | ` *` |
|        - | 11895 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|        - | 11896 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|        - | 11897 | ` * is_callable / closure-invoke paths follow the same rule.` |
|        - | 11898 | ` *` |
|        - | 11899 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|        - | 11900 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|        - | 11901 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|        - | 11902 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|        - | 11903 | ` *` |
|        - | 11904 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|        - | 11905 | ` */` |
|      174 | 11906 | `static sxi32 VmCallObjectInvoke(` |
|        - | 11907 | `	ph7_vm *pVm,` |
|        - | 11908 | `	ph7_class_instance *pThis,` |
|        - | 11909 | `	int nArg,` |
|        - | 11910 | `	ph7_value **apArg,` |
|        - | 11911 | `	ph7_value *pResult,` |
|        - | 11912 | `	VmCallArgMap *pMap` |
|        - | 11913 | `	)` |
|        2 | 11914 |  |
|        - | 11915 | `	ph7_class_method *pMethod;` |
|      176 | 11916 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      176 | 11917 | `	if( pMethod == 0 ){` |
|       13 | 11918 | `		if( pResult ){` |
|       13 | 11919 | `			PH7_MemObjRelease(pResult);` |
|        6 | 11920 | `		}` |
|       13 | 11921 | `		return SXERR_INVALID;` |
|        - | 11922 | `	}` |
|      164 | 11923 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       89 | 11924 |  |
|        - | 11925 | `/*` |
|        - | 11926 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|        - | 11927 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|        - | 11928 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|        - | 11929 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|        - | 11930 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|        - | 11931 | ` * lookup or 'goto Exception').` |
|        - | 11932 | ` *` |
|        - | 11933 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|        - | 11934 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|        - | 11935 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|        - | 11936 | ` * reported.` |
|        - | 11937 | ` */` |
|       12 | 11938 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        1 | 11939 |  |
|        - | 11940 | `	ph7_class *pErrorClass;` |
|       13 | 11941 | `	ph7_class_instance *pErrInst = 0;` |
|        - | 11942 | `	ph7_class_method *pCons;` |
|        - | 11943 | `	VmFrame *pThrowFrame;` |
|        - | 11944 | `	char zMsg[256];` |
|        - | 11945 | `	int nMsg;` |
|        - | 11946 | `	sxi32 rc;` |
|       25 | 11947 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - | 11948 | `		"Object of type %.*s is not callable",` |
|       12 | 11949 | `		(int)pThis->pClass->sName.nByte,` |
|       12 | 11950 | `		pThis->pClass->sName.zString);` |
|       13 | 11951 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|       13 | 11952 | `	if( pErrorClass ){` |
|       13 | 11953 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|        6 | 11954 | `	}` |
|       13 | 11955 | `	if( pErrInst == 0 ){` |
|        - | 11956 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|        - | 11957 | `		 * should always be available, so this branch is effectively unreachable.` |
|        - | 11958 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|        - | 11959 | `		 * visible to the user. */` |
|      ! 0 | 11960 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|      ! 0 | 11961 | `		return SXERR_ABORT;` |
|        - | 11962 | `	}` |
|       13 | 11963 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|       13 | 11964 | `	if( pCons ){` |
|        - | 11965 | `		ph7_value sArg;` |
|        - | 11966 | `		ph7_value *apMsg[1];` |
|        - | 11967 | `		SyString sMsgStr;` |
|       13 | 11968 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|       13 | 11969 | `		PH7_MemObjInit(pVm,&sArg);` |
|       13 | 11970 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|       13 | 11971 | `		apMsg[0] = &sArg;` |
|       13 | 11972 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|       13 | 11973 | `		PH7_MemObjRelease(&sArg);` |
|        6 | 11974 | `	}` |
|        - | 11975 | `	/* Else: Error::__construct is part of the built-in library and should` |
|        - | 11976 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|        - | 11977 | `	 * with an empty getMessage() rather than crashing. */` |
|       13 | 11978 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       13 | 11979 | `	if( pThrowFrame ){` |
|       13 | 11980 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 | 11981 | `	}` |
|       13 | 11982 | `	rc = VmThrowException(pVm,pErrInst);` |
|       13 | 11983 | `	PH7_ClassInstanceUnref(pErrInst);` |
|       13 | 11984 | `	return rc;` |
|        7 | 11985 |  |
|        - | 11986 | `/*` |
|        - | 11987 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 11988 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 11989 | ` * in the apArg[] array.` |
|        - | 11990 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 11991 | ` * return value indicates failure.` |
|        - | 11992 | ` */` |
|     1212 | 11993 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 11994 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 11995 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 11996 | `	int nArg,          /* Total number of given arguments */` |
|        - | 11997 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 11998 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 11999 | `	)` |
|        2 | 12000 |  |
|        - | 12001 | `	ph7_value *aStack;` |
|        - | 12002 | `	VmInstr aInstr[2];` |
|        - | 12003 | `	int i;` |
|     1214 | 12004 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|        - | 12005 | `		/* Object callable: dispatch through __invoke when available.` |
|        - | 12006 | `		 * No VmCallArgMap: call_user_func / array_map / usort and the C API` |
|        - | 12007 | `		 * pass arguments positionally and inherit no strict-types context. */` |
|      140 | 12008 | `		return VmCallObjectInvoke(&(*pVm),` |
|       92 | 12009 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|       46 | 12010 | `			nArg,apArg,pResult,0);` |
|        - | 12011 | `	}` |
|     1122 | 12012 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 12013 | `		/* Don't bother processing,it's invalid anyway */` |
|      511 | 12014 | `		if( pResult ){` |
|        - | 12015 | `			/* Assume a null return value */` |
|      ! 0 | 12016 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 12017 | `		}` |
|      511 | 12018 | `		return SXERR_INVALID;` |
|        - | 12019 | `	}` |
|      612 | 12020 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 12021 | `		/* Class method */` |
|       15 | 12022 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       15 | 12023 | `		ph7_class_method *pMethod = 0;` |
|       15 | 12024 | `		ph7_class_instance *pThis = 0;` |
|       15 | 12025 | `		ph7_class *pClass = 0;` |
|        - | 12026 | `		ph7_value *pValue;` |
|        - | 12027 | `		sxi32 rc;` |
|       15 | 12028 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 12029 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 12030 | `			if( pResult ){` |
|        - | 12031 | `				/* Assume a null return value */` |
|      ! 0 | 12032 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12033 | `			}` |
|      ! 0 | 12034 | `			return SXRET_OK;` |
|        - | 12035 | `		}` |
|        - | 12036 | `		/* Extract the class name or an instance of it */` |
|       15 | 12037 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       15 | 12038 | `		if( pValue ){` |
|       15 | 12039 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        7 | 12040 | `		}` |
|       15 | 12041 | `		if( pClass == 0 ){` |
|        - | 12042 | `			/* No such class,return NULL */` |
|      ! 0 | 12043 | `			if( pResult ){` |
|      ! 0 | 12044 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12045 | `			}` |
|      ! 0 | 12046 | `			return SXRET_OK;` |
|        - | 12047 | `		}` |
|       15 | 12048 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 12049 | `			/* Point to the class instance */` |
|        9 | 12050 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        4 | 12051 | `		}` |
|        - | 12052 | `		/* Try to extract the method */` |
|       15 | 12053 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       15 | 12054 | `		if( pValue ){` |
|       15 | 12055 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       22 | 12056 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        7 | 12057 | `					SyBlobLength(&pValue->sBlob));` |
|        7 | 12058 | `			}` |
|        7 | 12059 | `		}` |
|       15 | 12060 | `		if( pMethod == 0 ){` |
|        - | 12061 | `			/* No such method,return NULL */` |
|      ! 0 | 12062 | `			if( pResult ){` |
|      ! 0 | 12063 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 12064 | `			}` |
|      ! 0 | 12065 | `			return SXRET_OK;` |
|        - | 12066 | `		}` |
|        - | 12067 | `		/* Call the class method */` |
|       15 | 12068 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       15 | 12069 | `		return rc;` |
|        - | 12070 | `	}` |
|        - | 12071 | `	/* Create a new operand stack */` |
|      598 | 12072 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      598 | 12073 | `	if( aStack == 0 ){` |
|      ! 0 | 12074 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 12075 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 12076 | `		if( pResult ){` |
|        - | 12077 | `			/* Assume a null return value */` |
|      ! 0 | 12078 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 12079 | `		}` |
|      ! 0 | 12080 | `		return SXERR_MEM;` |
|        - | 12081 | `	}` |
|        - | 12082 | `	/* Fill the operand stack with the given arguments */` |
|     1900 | 12083 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1304 | 12084 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 12085 | `		/*` |
|        - | 12086 | `		 * Symisc eXtension:` |
|        - | 12087 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 12088 | `		 */` |
|     1304 | 12089 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      653 | 12090 | `	}` |
|        - | 12091 | `	/* Push the function name */` |
|      598 | 12092 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      598 | 12093 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 12094 | `	/* Emit the CALL istruction */` |
|      598 | 12095 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      598 | 12096 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      598 | 12097 | `	aInstr[0].iP2 = 0;` |
|      598 | 12098 | `	aInstr[0].p3  = 0;` |
|        - | 12099 | `	/* Emit the DONE instruction */` |
|      598 | 12100 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      598 | 12101 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      598 | 12102 | `	aInstr[1].iP2 = 0;` |
|      598 | 12103 | `	aInstr[1].p3  = 0;` |
|        - | 12104 | `	/* Execute the function body (if available) */` |
|        - | 12105 | `	{` |
|        - | 12106 | `		sxi32 rcExec;` |
|      598 | 12107 | `		rcExec = VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0);` |
|        - | 12108 | `		/* Clean up the mess left behind */` |
|      598 | 12109 | `		SyMemBackendFree(&pVm->sAllocator,aStack);` |
|        - | 12110 | `		/* Propagate PH7_EXCEPTION/PH7_ABORT so a callback that raised unwinds. */` |
|      598 | 12111 | `		return rcExec;` |
|        - | 12112 | `	}` |
|      608 | 12113 |  |
|        - | 12114 | `/*` |
|        - | 12115 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 12116 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 12117 | ` * parameter.` |
|        - | 12118 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 12119 | ` * return value indicates failure.` |
|        - | 12120 | ` */` |
|      240 | 12121 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 12122 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 12123 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 12124 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 12125 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 12126 | `	)` |
|        1 | 12127 |  |
|        - | 12128 | `	ph7_value *pArg;` |
|        - | 12129 | `	SySet aArg;` |
|        - | 12130 | `	va_list ap;` |
|        - | 12131 | `	sxi32 rc;` |
|      241 | 12132 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 12133 | `	/* Copy arguments one after one */` |
|      241 | 12134 | `	va_start(ap,pResult);` |
|      399 | 12135 | `	for(;;){` |
|      799 | 12136 | `		pArg = va_arg(ap,ph7_value *);` |
|      799 | 12137 | `		if( pArg == 0 ){` |
|      241 | 12138 | `			break;` |
|        - | 12139 | `		}` |
|      559 | 12140 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 12141 | `	}` |
|        - | 12142 | `	/* Call the core routine */` |
|      241 | 12143 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 12144 | `	/* Cleanup */` |
|      241 | 12145 | `	SySetRelease(&aArg);` |
|      241 | 12146 | `	return rc;` |
|        1 | 12147 |  |
|        - | 12148 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 12149 | `/*` |
|        - | 12150 | ` * bool defined(string $name)` |
|        - | 12151 | ` *  Checks whether a given named constant exists.` |
|        - | 12152 | ` * Parameter:` |
|        - | 12153 | ` *  Name of the desired constant.` |
|        - | 12154 | ` * Return` |
|        - | 12155 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 12156 | ` */` |
|       26 | 12157 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12158 |  |
|        - | 12159 | `	const char *zName;` |
|       28 | 12160 | `	int nLen = 0;` |
|       28 | 12161 | `	int res = 0;` |
|       28 | 12162 | `	if( nArg < 1 ){` |
|        - | 12163 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 12164 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 12165 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12166 | `		return SXRET_OK;` |
|        - | 12167 | `	}` |
|        - | 12168 | `	/* Extract constant name */` |
|       28 | 12169 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 12170 | `	/* Perform the lookup */` |
|       28 | 12171 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 12172 | `		/* Already defined */` |
|       26 | 12173 | `		res = 1;` |
|       12 | 12174 | `	}` |
|       28 | 12175 | `	ph7_result_bool(pCtx,res);` |
|       28 | 12176 | `	return SXRET_OK;` |
|       15 | 12177 |  |
|        - | 12178 | `/*` |
|        - | 12179 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 12180 | ` * below.` |
|        - | 12181 | ` */` |
|       16 | 12182 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 12183 |  |
|       18 | 12184 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 12185 | `	/* Expand constant value */` |
|       18 | 12186 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       18 | 12187 |  |
|        - | 12188 | `/*` |
|        - | 12189 | ` * bool define(string $constant_name,expression value)` |
|        - | 12190 | ` *  Defines a named constant at runtime.` |
|        - | 12191 | ` * Parameter:` |
|        - | 12192 | ` *  $constant_name` |
|        - | 12193 | ` *   The name of the constant` |
|        - | 12194 | ` *  $value` |
|        - | 12195 | ` *   Constant value` |
|        - | 12196 | ` * Return:` |
|        - | 12197 | ` *   TRUE on success,FALSE on failure.` |
|        - | 12198 | ` */` |
|       14 | 12199 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12200 |  |
|        - | 12201 | `	const char *zName;  /* Constant name */` |
|        - | 12202 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       16 | 12203 | `	int nLen = 0;       /* Name length */` |
|        - | 12204 | `	sxi32 rc;` |
|       16 | 12205 | `	if( nArg < 2 ){` |
|        - | 12206 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 12207 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 12208 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12209 | `		return SXRET_OK;` |
|        - | 12210 | `	}` |
|       16 | 12211 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 12212 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 12213 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12214 | `		return SXRET_OK;` |
|        - | 12215 | `	}` |
|        - | 12216 | `	/* Extract constant name */` |
|       16 | 12217 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       16 | 12218 | `	if( nLen < 1 ){` |
|      ! 0 | 12219 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 12220 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12221 | `		return SXRET_OK;` |
|        - | 12222 | `	}` |
|        - | 12223 | `	/* Duplicate constant value */` |
|       16 | 12224 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       16 | 12225 | `	if( pValue == 0 ){` |
|      ! 0 | 12226 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 12227 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12228 | `		return SXRET_OK;` |
|        - | 12229 | `	}` |
|        - | 12230 | `	/* Initialize the memory object */` |
|       16 | 12231 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 12232 | `	/* Register the constant */` |
|       16 | 12233 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       16 | 12234 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 12235 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 12236 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 12237 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12238 | `		return SXRET_OK;` |
|        - | 12239 | `	}` |
|        - | 12240 | `	/* Duplicate constant value */` |
|       16 | 12241 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       16 | 12242 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 12243 | `		/* Lower case the constant name */` |
|      ! 0 | 12244 | `		char *zCur = (char *)zName;` |
|      ! 0 | 12245 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 12246 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 12247 | `				/* UTF-8 stream */` |
|      ! 0 | 12248 | `				zCur++;` |
|      ! 0 | 12249 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 12250 | `					zCur++;` |
|      ! 0 | 12251 | `				}` |
|      ! 0 | 12252 | `				continue;` |
|        - | 12253 | `			}` |
|      ! 0 | 12254 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 12255 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 12256 | `				zCur[0] = (char)c;` |
|      ! 0 | 12257 | `			}` |
|      ! 0 | 12258 | `			zCur++;` |
|      ! 0 | 12259 | `		}` |
|        - | 12260 | `		/* Register the lowercase alias with its OWN value copy (not the same` |
|        - | 12261 | `		 * pValue) so the two entries don't share one object — otherwise freeing` |
|        - | 12262 | `		 * one on a later overwrite would dangle the other. */` |
|        - | 12263 | `		{` |
|      ! 0 | 12264 | `			ph7_value *pAlias = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|      ! 0 | 12265 | `			if( pAlias ){` |
|      ! 0 | 12266 | `				PH7_MemObjInit(pCtx->pVm,pAlias);` |
|      ! 0 | 12267 | `				PH7_MemObjStore(apArg[1],pAlias);` |
|      ! 0 | 12268 | `				ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pAlias);` |
|      ! 0 | 12269 | `			}` |
|        - | 12270 | `		}` |
|      ! 0 | 12271 | `	}` |
|        - | 12272 | `	/* All done,return TRUE */` |
|       16 | 12273 | `	ph7_result_bool(pCtx,1);` |
|       16 | 12274 | `	return SXRET_OK;` |
|        9 | 12275 |  |
|        - | 12276 | `/*` |
|        - | 12277 | ` * value constant(string $name)` |
|        - | 12278 | ` *  Returns the value of a constant` |
|        - | 12279 | ` * Parameter` |
|        - | 12280 | ` *  $name` |
|        - | 12281 | ` *    Name of the constant.` |
|        - | 12282 | ` * Return` |
|        - | 12283 | ` *  Constant value or NULL if not defined.` |
|        - | 12284 | ` */` |
|        8 | 12285 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12286 |  |
|        - | 12287 | `	SyHashEntry *pEntry;` |
|        - | 12288 | `	ph7_constant *pCons;` |
|        - | 12289 | `	const char *zName; /* Constant name */` |
|        - | 12290 | `	ph7_value sVal;    /* Constant value */` |
|        - | 12291 | `	int nLen;` |
|       10 | 12292 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12293 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 12294 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 12295 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12296 | `		return SXRET_OK;` |
|        - | 12297 | `	}` |
|        - | 12298 | `	/* Extract the constant name */` |
|       10 | 12299 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 12300 | `	/* Perform the query */` |
|       10 | 12301 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 12302 | `	if( pEntry == 0 ){` |
|        3 | 12303 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 12304 | `		ph7_result_null(pCtx);` |
|        3 | 12305 | `		return SXRET_OK;` |
|        - | 12306 | `	}` |
|        8 | 12307 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 12308 | `	/* Point to the structure that describe the constant */` |
|        8 | 12309 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 12310 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 12311 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 12312 | `	/* Return that value */` |
|        8 | 12313 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 12314 | `	/* Cleanup */` |
|        8 | 12315 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 12316 | `	return SXRET_OK;` |
|        6 | 12317 |  |
|        - | 12318 | `/*` |
|        - | 12319 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 12320 | ` * defined below.` |
|        - | 12321 | ` */` |
|      466 | 12322 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12323 |  |
|      467 | 12324 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 12325 | `	ph7_value sName;` |
|        - | 12326 | `	sxi32 rc;` |
|        - | 12327 | `	/* Prepare the constant name for insertion */` |
|      467 | 12328 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      467 | 12329 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 12330 | `	/* Perform the insertion */` |
|      467 | 12331 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      467 | 12332 | `	PH7_MemObjRelease(&sName);` |
|      467 | 12333 | `	return rc;` |
|        1 | 12334 |  |
|        - | 12335 | `/*` |
|        - | 12336 | ` * array get_defined_constants(void)` |
|        - | 12337 | ` *  Returns an associative array with the names of all defined` |
|        - | 12338 | ` *  constants.` |
|        - | 12339 | ` * Parameters` |
|        - | 12340 | ` *  NONE.` |
|        - | 12341 | ` * Returns` |
|        - | 12342 | ` *  Returns the names of all the constants currently defined.` |
|        - | 12343 | ` */` |
|        2 | 12344 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12345 |  |
|        - | 12346 | `	ph7_value *pArray;` |
|        - | 12347 | `	/* Create the array first*/` |
|        3 | 12348 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12349 | `	if( pArray == 0 ){` |
|      ! 0 | 12350 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12351 | `		SXUNUSED(apArg);` |
|        - | 12352 | `		/* Return NULL */` |
|      ! 0 | 12353 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12354 | `		return SXRET_OK;` |
|        - | 12355 | `	}` |
|        - | 12356 | `	/* Fill the array with the defined constants */` |
|        3 | 12357 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 12358 | `	/* Return the created array */` |
|        3 | 12359 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12360 | `	return SXRET_OK;` |
|        2 | 12361 |  |
|        - | 12362 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 12363 | `/*` |
|        - | 12364 | ` * Section:` |
|        - | 12365 | ` *  Random numbers/string generators.` |
|        - | 12366 | ` * Status:` |
|        - | 12367 | ` *    Stable.` |
|        - | 12368 | ` */` |
|        - | 12369 | `/*` |
|        - | 12370 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 12371 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 12372 | ` * implemented in src/sx/sxrand.c).` |
|        - | 12373 | ` */` |
|     2907 | 12374 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 12375 |  |
|        - | 12376 | `	sxu32 iNum;` |
|     2909 | 12377 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2909 | 12378 | `	return iNum;` |
|        2 | 12379 |  |
|        - | 12380 | `/*` |
|        - | 12381 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 12382 | ` * Note that the generated string is NOT null terminated.` |
|        - | 12383 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - | 12384 | ` * implemented in src/sx/sxrand.c).` |
|        - | 12385 | ` */` |
|   236484 | 12386 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 12387 |  |
|        - | 12388 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 12389 | `	int i;` |
|        - | 12390 | `	/* Generate a binary string first */` |
|   236486 | 12391 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 12392 | `	/* Turn the binary string into english based alphabet */` |
|  2601494 | 12393 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  2365010 | 12394 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|  1182506 | 12395 | `	 }` |
|   236486 | 12396 |  |
|        - | 12397 | `/*` |
|        - | 12398 | ` * int rand()` |
|        - | 12399 | ` * int mt_rand()` |
|        - | 12400 | ` * int rand(int $min,int $max)` |
|        - | 12401 | ` * int mt_rand(int $min,int $max)` |
|        - | 12402 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 12403 | ` * Parameter` |
|        - | 12404 | ` *  $min` |
|        - | 12405 | ` *    The lowest value to return (default: 0)` |
|        - | 12406 | ` *  $max` |
|        - | 12407 | ` *   The highest value to return (default: getrandmax())` |
|        - | 12408 | ` * Return` |
|        - | 12409 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 12410 | ` * Note:` |
|        - | 12411 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12412 | ` *  by te SQLite3 library.` |
|        - | 12413 | ` */` |
|       20 | 12414 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12415 |  |
|        - | 12416 | `	sxu32 iNum;` |
|        - | 12417 | `	/* Generate the random number */` |
|       21 | 12418 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 12419 | `	if( nArg > 1 ){` |
|        - | 12420 | `		sxu32 iMin,iMax;` |
|        3 | 12421 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 12422 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 12423 | `		if( iMin < iMax ){` |
|        3 | 12424 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 12425 | `			if( iDiv > 0 ){` |
|        3 | 12426 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 12427 | `			}` |
|        1 | 12428 | `		}else if(iMax > 0 ){` |
|      ! 0 | 12429 | `			iNum %= iMax;` |
|      ! 0 | 12430 | `		}` |
|        1 | 12431 | `	}` |
|        - | 12432 | `	/* Return the number */` |
|       21 | 12433 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 12434 | `	return SXRET_OK;` |
|        1 | 12435 |  |
|        - | 12436 | `/*` |
|        - | 12437 | ` * int getrandmax(void)` |
|        - | 12438 | ` * int mt_getrandmax(void)` |
|        - | 12439 | ` * int rc4_getrandmax(void)` |
|        - | 12440 | ` *   Show largest possible random value` |
|        - | 12441 | ` * Return` |
|        - | 12442 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 12443 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 12444 | ` * Note:` |
|        - | 12445 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12446 | ` *  by te SQLite3 library.` |
|        - | 12447 | ` */` |
|        4 | 12448 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12449 |  |
|        2 | 12450 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 12451 | `	SXUNUSED(apArg);` |
|        5 | 12452 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 12453 | `	return SXRET_OK;` |
|        1 | 12454 |  |
|        - | 12455 | `/*` |
|        - | 12456 | ` * string rand_str()` |
|        - | 12457 | ` * string rand_str(int $len)` |
|        - | 12458 | ` *  Generate a random string (English alphabet).` |
|        - | 12459 | ` * Parameter` |
|        - | 12460 | ` *  $len` |
|        - | 12461 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 12462 | ` * Return` |
|        - | 12463 | ` *   A pseudo random string.` |
|        - | 12464 | ` * Note:` |
|        - | 12465 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 12466 | ` *  by te SQLite3 library.` |
|        - | 12467 | ` *  This function is a symisc extension.` |
|        - | 12468 | ` */` |
|      120 | 12469 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12470 |  |
|        - | 12471 | `	char zString[1024];` |
|      122 | 12472 | `	int iLen = 0x10;` |
|      122 | 12473 | `	if( nArg > 0 ){` |
|        - | 12474 | `		/* Get the desired length */` |
|      122 | 12475 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 12476 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 12477 | `			/* Default length */` |
|        3 | 12478 | `			iLen = 0x10;` |
|        1 | 12479 | `		}` |
|       60 | 12480 | `	}` |
|        - | 12481 | `	/* Generate the random string */` |
|      122 | 12482 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 12483 | `	/* Return the generated string */` |
|      122 | 12484 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 12485 | `	return SXRET_OK;` |
|        2 | 12486 |  |
|        - | 12487 | `/*` |
|        - | 12488 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - | 12489 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - | 12490 | ` * an int (PHP coerces float and numeric string silently).` |
|        - | 12491 | ` */` |
|      488 | 12492 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        1 | 12493 |  |
|      488 | 12494 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      483 | 12495 | `		\|\| ph7_value_is_resource(pArg) ){` |
|       10 | 12496 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12497 | `			"TypeError",` |
|        - | 12498 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|        3 | 12499 | `			zFunc,iArgPos,zParamName,` |
|        3 | 12500 | `			ph7_type_name(pArg)` |
|        - | 12501 | `			);` |
|        - | 12502 | `	}` |
|      483 | 12503 | `	if( ph7_value_is_string(pArg) ){` |
|        - | 12504 | `		int len;` |
|        9 | 12505 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 | 12506 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 | 12507 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12508 | `				"TypeError",` |
|        - | 12509 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 | 12510 | `				zFunc,iArgPos,zParamName` |
|        - | 12511 | `				);` |
|        - | 12512 | `		}` |
|        2 | 12513 | `	}` |
|      479 | 12514 | `	return SXRET_OK;` |
|      245 | 12515 |  |
|        - | 12516 | `/*` |
|        - | 12517 | ` * int random_int(int $min, int $max)` |
|        - | 12518 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - | 12519 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - | 12520 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - | 12521 | ` *  power-of-two mask covering the range.` |
|        - | 12522 | ` */` |
|      242 | 12523 | `static int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12524 |  |
|        - | 12525 | `	sxi64 iMin,iMax;` |
|        - | 12526 | `	sxu64 uRange,uMask,uResult;` |
|        - | 12527 | `	unsigned int nAttempt;` |
|        - | 12528 | `	int rc;` |
|      243 | 12529 | `	if( nArg != 2 ){` |
|       10 | 12530 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12531 | `			"ArgumentCountError",` |
|        - | 12532 | `			"random_int() expects exactly 2 arguments, %d given",` |
|        3 | 12533 | `			nArg` |
|        - | 12534 | `			);` |
|        - | 12535 | `	}` |
|      237 | 12536 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      237 | 12537 | `	if( rc != SXRET_OK ){ return rc; }` |
|      233 | 12538 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      233 | 12539 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 | 12540 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 | 12541 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 | 12542 | `	if( iMin > iMax ){` |
|        3 | 12543 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12544 | `			"ValueError",` |
|        - | 12545 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - | 12546 | `			);` |
|        - | 12547 | `	}` |
|      229 | 12548 | `	if( iMin == iMax ){` |
|        5 | 12549 | `		ph7_result_int64(pCtx,iMin);` |
|        5 | 12550 | `		return SXRET_OK;` |
|        - | 12551 | `	}` |
|      225 | 12552 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 | 12553 | `	uMask = uRange;` |
|      225 | 12554 | `	uMask \|= uMask >> 1;` |
|      225 | 12555 | `	uMask \|= uMask >> 2;` |
|      225 | 12556 | `	uMask \|= uMask >> 4;` |
|      225 | 12557 | `	uMask \|= uMask >> 8;` |
|      225 | 12558 | `	uMask \|= uMask >> 16;` |
|      225 | 12559 | `	uMask \|= uMask >> 32;` |
|      225 | 12560 | `	uResult = 0;` |
|      336 | 12561 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - | 12562 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - | 12563 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - | 12564 | `		 * and the low-half mask would always read 0). */` |
|        - | 12565 | `		sxu64 uDraw;` |
|      336 | 12566 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|        - | 12567 | `			/* PHP 8.2+ would throw Random\RandomException here; that class` |
|        - | 12568 | `			 * is not yet registered in PHL (see PLAN.md item 6.13). */` |
|      ! 0 | 12569 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12570 | `				"Exception",` |
|        - | 12571 | `				"Cannot gather sufficient random data"` |
|        - | 12572 | `				);` |
|        - | 12573 | `		}` |
|      336 | 12574 | `		uDraw &= uMask;` |
|      336 | 12575 | `		if( uDraw <= uRange ){` |
|      225 | 12576 | `			uResult = uDraw;` |
|      225 | 12577 | `			break;` |
|        - | 12578 | `		}` |
|       64 | 12579 | `	}` |
|      225 | 12580 | `	if( nAttempt >= 50 ){` |
|      ! 0 | 12581 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12582 | `			"Exception",` |
|        - | 12583 | `			"Cannot gather sufficient random data"` |
|        - | 12584 | `			);` |
|        - | 12585 | `	}` |
|      225 | 12586 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 | 12587 | `	return SXRET_OK;` |
|      122 | 12588 |  |
|        - | 12589 | `/*` |
|        - | 12590 | ` * string random_bytes(int $length)` |
|        - | 12591 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - | 12592 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - | 12593 | ` */` |
|       24 | 12594 | `static int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12595 |  |
|        - | 12596 | `	sxi64 iLen;` |
|        - | 12597 | `	unsigned char zStack[256];` |
|        - | 12598 | `	void *pBuf;` |
|        - | 12599 | `	int rc;` |
|       25 | 12600 | `	int bHeap = 0;` |
|       25 | 12601 | `	if( nArg != 1 ){` |
|        7 | 12602 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12603 | `			"ArgumentCountError",` |
|        - | 12604 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|        2 | 12605 | `			nArg` |
|        - | 12606 | `			);` |
|        - | 12607 | `	}` |
|       21 | 12608 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       21 | 12609 | `	if( rc != SXRET_OK ){ return rc; }` |
|       17 | 12610 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       17 | 12611 | `	if( iLen < 1 ){` |
|        5 | 12612 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12613 | `			"ValueError",` |
|        - | 12614 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - | 12615 | `			);` |
|        - | 12616 | `	}` |
|        - | 12617 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - | 12618 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - | 12619 | `	 * silently truncating via the (sxu32) cast below. */` |
|       13 | 12620 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 | 12621 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12622 | `			"ValueError",` |
|        - | 12623 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - | 12624 | `			);` |
|        - | 12625 | `	}` |
|       13 | 12626 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       13 | 12627 | `		pBuf = zStack;` |
|        7 | 12628 | `	}else{` |
|      ! 0 | 12629 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 | 12630 | `		if( pBuf == 0 ){` |
|      ! 0 | 12631 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12632 | `				"Exception",` |
|        - | 12633 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 | 12634 | `				iLen` |
|        - | 12635 | `				);` |
|        - | 12636 | `		}` |
|      ! 0 | 12637 | `		bHeap = 1;` |
|        - | 12638 | `	}` |
|       13 | 12639 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 | 12640 | `		if( bHeap ){` |
|      ! 0 | 12641 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12642 | `		}` |
|      ! 0 | 12643 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12644 | `			"Exception",` |
|        - | 12645 | `			"Cannot gather sufficient random data"` |
|        - | 12646 | `			);` |
|        - | 12647 | `	}` |
|       13 | 12648 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       13 | 12649 | `	if( bHeap ){` |
|      ! 0 | 12650 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12651 | `	}` |
|       13 | 12652 | `	return SXRET_OK;` |
|       13 | 12653 |  |
|        - | 12654 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12655 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 12656 | `/* Unique ID private data */` |
|        - | 12657 | `struct unique_id_data` |
|        - | 12658 |  |
|        - | 12659 | `	ph7_context *pCtx; /* Call context */` |
|        - | 12660 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 12661 | `};` |
|        - | 12662 | `/*` |
|        - | 12663 | ` * Binary to hex consumer callback.` |
|        - | 12664 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 12665 | ` * defined below.` |
|        - | 12666 | ` */` |
|      192 | 12667 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 12668 |  |
|      193 | 12669 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 12670 | `	sxu32 nBuflen;` |
|        - | 12671 | `	/* Extract result buffer length */` |
|      193 | 12672 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 12673 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 12674 | `			/*` |
|        - | 12675 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 12676 | `			 * string will be 13 characters long` |
|        - | 12677 | `			 */` |
|       25 | 12678 | `		return SXERR_ABORT;` |
|        - | 12679 | `	}` |
|      169 | 12680 | `	if( nBuflen > 22 ){` |
|      ! 0 | 12681 | `		return SXERR_ABORT;` |
|        - | 12682 | `	}` |
|        - | 12683 | `	/* Safely Consume the hex stream */` |
|      169 | 12684 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 12685 | `	return SXRET_OK;` |
|       97 | 12686 |  |
|        - | 12687 | `/*` |
|        - | 12688 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 12689 | ` *  Generate a unique ID` |
|        - | 12690 | ` * Parameter` |
|        - | 12691 | ` * $prefix` |
|        - | 12692 | ` *  Append this prefix to the generated unique ID.` |
|        - | 12693 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 12694 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 12695 | ` * $more_entropy` |
|        - | 12696 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 12697 | ` *  that the result will be unique.` |
|        - | 12698 | ` * Return` |
|        - | 12699 | ` *  Returns the unique identifier, as a string.` |
|        - | 12700 | ` */` |
|       24 | 12701 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12702 |  |
|        - | 12703 | `	struct unique_id_data sUniq;` |
|        - | 12704 | `	unsigned char zDigest[20];` |
|       25 | 12705 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12706 | `	const char *zPrefix;` |
|        - | 12707 | `	SHA1Context sCtx;` |
|        - | 12708 | `	char zRandom[7];` |
|        - | 12709 | `	int nPrefix;` |
|        - | 12710 | `	int entropy;` |
|        - | 12711 | `	/* Generate a random string first */` |
|       25 | 12712 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 12713 | `	/* Initialize fields */` |
|       25 | 12714 | `	zPrefix = 0;` |
|       25 | 12715 | `	nPrefix = 0;` |
|       25 | 12716 | `	entropy = 0;` |
|       25 | 12717 | `	if( nArg > 0 ){` |
|        - | 12718 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 12719 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 12720 | `		if( nArg > 1 ){` |
|      ! 0 | 12721 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 12722 | `		}` |
|      ! 0 | 12723 | `	}` |
|       25 | 12724 | `	SHA1Init(&sCtx);` |
|        - | 12725 | `	/* Generate the random ID */` |
|       25 | 12726 | `	if( nPrefix > 0 ){` |
|      ! 0 | 12727 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 12728 | `	}` |
|        - | 12729 | `	/* Append the random ID */` |
|       25 | 12730 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 12731 | `	/* Append the random string */` |
|       25 | 12732 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 12733 | `	/* Increment the number */` |
|       25 | 12734 | `	pVm->unique_id++;` |
|       25 | 12735 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 12736 | `	/* Hexify the digest */` |
|       25 | 12737 | `	sUniq.pCtx = pCtx;` |
|       25 | 12738 | `	sUniq.entropy = entropy;` |
|       25 | 12739 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 12740 | `	/* All done */` |
|       25 | 12741 | `	return PH7_OK;` |
|        1 | 12742 |  |
|        - | 12743 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 12744 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12745 | `/*` |
|        - | 12746 | ` * Section:` |
|        - | 12747 | ` *  Language construct implementation as foreign functions.` |
|        - | 12748 | ` * Status:` |
|        - | 12749 | ` *    Stable.` |
|        - | 12750 | ` */` |
|        - | 12751 | `/*` |
|        - | 12752 | ` * void echo($string...)` |
|        - | 12753 | ` *  Output one or more messages.` |
|        - | 12754 | ` * Parameters` |
|        - | 12755 | ` *  $string` |
|        - | 12756 | ` *   Message to output.` |
|        - | 12757 | ` * Return` |
|        - | 12758 | ` *  NULL.` |
|        - | 12759 | ` */` |
|      ! 0 | 12760 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12761 |  |
|        - | 12762 | `	const char *zData;` |
|      ! 0 | 12763 | `	int nDataLen = 0;` |
|        - | 12764 | `	ph7_vm *pVm;` |
|        - | 12765 | `	int i,rc;` |
|        - | 12766 | `	/* Point to the target VM */` |
|      ! 0 | 12767 | `	pVm = pCtx->pVm;` |
|        - | 12768 | `	/* Output */` |
|      ! 0 | 12769 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 12770 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 12771 | `		if( nDataLen > 0 ){` |
|      ! 0 | 12772 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 12773 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 12774 | `			if( rc == SXERR_ABORT ){` |
|        - | 12775 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12776 | `				return PH7_ABORT;` |
|        - | 12777 | `			}` |
|      ! 0 | 12778 | `		}` |
|      ! 0 | 12779 | `	}` |
|      ! 0 | 12780 | `	return SXRET_OK;` |
|      ! 0 | 12781 |  |
|        - | 12782 | `/*` |
|        - | 12783 | ` * int print($string...)` |
|        - | 12784 | ` *  Output one or more messages.` |
|        - | 12785 | ` * Parameters` |
|        - | 12786 | ` *  $string` |
|        - | 12787 | ` *   Message to output.` |
|        - | 12788 | ` * Return` |
|        - | 12789 | ` *  1 always.` |
|        - | 12790 | ` */` |
|        2 | 12791 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12792 |  |
|        - | 12793 | `	const char *zData;` |
|        3 | 12794 | `	int nDataLen = 0;` |
|        - | 12795 | `	ph7_vm *pVm;` |
|        - | 12796 | `	int i,rc;` |
|        - | 12797 | `	/* Point to the target VM */` |
|        3 | 12798 | `	pVm = pCtx->pVm;` |
|        - | 12799 | `	/* Output */` |
|        5 | 12800 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 12801 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 12802 | `		if( nDataLen > 0 ){` |
|        3 | 12803 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 12804 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 12805 | `			if( rc == SXERR_ABORT ){` |
|        - | 12806 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12807 | `				return PH7_ABORT;` |
|        - | 12808 | `			}` |
|        1 | 12809 | `		}` |
|        2 | 12810 | `	}` |
|        - | 12811 | `	/* Return 1 */` |
|        3 | 12812 | `	ph7_result_int(pCtx,1);` |
|        3 | 12813 | `	return SXRET_OK;` |
|        2 | 12814 |  |
|        - | 12815 | `/*` |
|        - | 12816 | ` * void exit(string $msg)` |
|        - | 12817 | ` * void exit(int $status)` |
|        - | 12818 | ` * void die(string $ms)` |
|        - | 12819 | ` * void die(int $status)` |
|        - | 12820 | ` *   Output a message and terminate program execution.` |
|        - | 12821 | ` * Parameter` |
|        - | 12822 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 12823 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 12824 | ` *  and not printed` |
|        - | 12825 | ` * Return` |
|        - | 12826 | ` *  NULL` |
|        - | 12827 | ` */` |
|      ! 0 | 12828 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12829 |  |
|      ! 0 | 12830 | `	if( nArg > 0 ){` |
|      ! 0 | 12831 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 12832 | `			const char *zData;` |
|      ! 0 | 12833 | `			int iLen = 0;` |
|        - | 12834 | `			/* Print exit message */` |
|      ! 0 | 12835 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 12836 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 12837 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 12838 | `			sxi32 iExitStatus;` |
|        - | 12839 | `			/* Record exit status code */` |
|      ! 0 | 12840 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 12841 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 12842 | `		}` |
|      ! 0 | 12843 | `	}` |
|        - | 12844 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|        - | 12845 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|        - | 12846 | `	 */` |
|      ! 0 | 12847 | `	pCtx->pVm->bHaltRequested = 1;` |
|      ! 0 | 12848 | `	return PH7_ABORT;` |
|      ! 0 | 12849 |  |
|        - | 12850 | `/*` |
|        - | 12851 | ` * bool isset($var,...)` |
|        - | 12852 | ` *  Finds out whether a variable is set.` |
|        - | 12853 | ` * Parameters` |
|        - | 12854 | ` *  One or more variable to check.` |
|        - | 12855 | ` * Return` |
|        - | 12856 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 12857 | ` */` |
|    92744 | 12858 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12859 |  |
|        - | 12860 | `	ph7_value *pObj;` |
|    92746 | 12861 | `	int res = 0;` |
|        - | 12862 | `	int i;` |
|    92746 | 12863 | `	if( nArg < 1 ){` |
|        - | 12864 | `		/* Missing arguments,return false */` |
|      ! 0 | 12865 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 12866 | `		return SXRET_OK;` |
|        - | 12867 | `	}` |
|        - | 12868 | `	/* Iterate over available arguments */` |
|   121226 | 12869 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    92756 | 12870 | `		pObj = apArg[i];` |
|    92756 | 12871 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - | 12872 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - | 12873 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - | 12874 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|    63300 | 12875 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - | 12876 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 12877 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 12878 | `			}` |
|    31649 | 12879 | `		}` |
|    92756 | 12880 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    92756 | 12881 | `		if( !res ){` |
|        - | 12882 | `			/* Variable not set,return FALSE */` |
|    64276 | 12883 | `			ph7_result_bool(pCtx,0);` |
|    64276 | 12884 | `			return SXRET_OK;` |
|        - | 12885 | `		}` |
|    14242 | 12886 | `	}` |
|        - | 12887 | `	/* All given variable are set,return TRUE */` |
|    28472 | 12888 | `	ph7_result_bool(pCtx,1);` |
|    28472 | 12889 | `	return SXRET_OK;` |
|    46374 | 12890 |  |
|        - | 12891 | `/*` |
|        - | 12892 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 12893 | ` * frame,the reference table and discard it's contents.` |
|        - | 12894 | ` * This function never fail and always return SXRET_OK.` |
|        - | 12895 | ` */` |
|  3161648 | 12896 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 12897 |  |
|        - | 12898 | `	ph7_value *pObj;` |
|        - | 12899 | `	VmRefObj *pRef;` |
|  3161650 | 12900 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3161650 | 12901 | `	if( pObj ){` |
|        - | 12902 | `		/* Release the object */` |
|  3161650 | 12903 | `		PH7_MemObjRelease(pObj);` |
|  1580824 | 12904 | `	}` |
|        - | 12905 | `	/* Remove old reference links */` |
|  3161650 | 12906 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3161650 | 12907 | `	if( pRef ){` |
|  3161644 | 12908 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 12909 | `		/* Unlink from the reference table */` |
|  3161644 | 12910 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3161644 | 12911 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 12912 | `			VmSlot sFree;` |
|        - | 12913 | `			/* Restore to the free list */` |
|  3161636 | 12914 | `			sFree.nIdx = nObjIdx;` |
|  3161636 | 12915 | `			sFree.pUserData = 0;` |
|  3161636 | 12916 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1580817 | 12917 | `		}` |
|  1580821 | 12918 | `	}` |
|  3161650 | 12919 | `	return SXRET_OK;` |
|        2 | 12920 |  |
|        - | 12921 | `/*` |
|        - | 12922 | ` * void unset($var,...)` |
|        - | 12923 | ` *   Unset one or more given variable.` |
|        - | 12924 | ` * Parameters` |
|        - | 12925 | ` *  One or more variable to unset.` |
|        - | 12926 | ` * Return` |
|        - | 12927 | ` *  Nothing.` |
|        - | 12928 | ` */` |
|     7554 | 12929 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12930 |  |
|        - | 12931 | `	ph7_value *pObj;` |
|        - | 12932 | `	ph7_vm *pVm;` |
|        - | 12933 | `	int i;` |
|        - | 12934 | `	/* Point to the target VM */` |
|     7556 | 12935 | `	pVm = pCtx->pVm;` |
|        - | 12936 | `	/* Iterate and unset */` |
|    15110 | 12937 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7556 | 12938 | `		pObj = apArg[i];` |
|     7556 | 12939 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      840 | 12940 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 12941 | `				/* Throw an error */` |
|      ! 0 | 12942 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 12943 | `			}` |
|      421 | 12944 | `		}else{` |
|     6718 | 12945 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 12946 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6718 | 12947 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6712 | 12948 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3355 | 12949 | `			}` |
|        - | 12950 | `		}` |
|     3779 | 12951 | `	}` |
|     7556 | 12952 | `	return SXRET_OK;` |
|        2 | 12953 |  |
|        - | 12954 | `/*` |
|        - | 12955 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 12956 | ` */` |
|      116 | 12957 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12958 |  |
|      117 | 12959 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      117 | 12960 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 12961 | `	ph7_value *pObj;` |
|        - | 12962 | `	sxu32 nIdx;` |
|        - | 12963 | `	/* Extract the memory object */` |
|      117 | 12964 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      117 | 12965 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      117 | 12966 | `	if( pObj ){` |
|      117 | 12967 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      115 | 12968 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 12969 | `				SyString sName;` |
|        - | 12970 | `				ph7_value sKey;` |
|        - | 12971 | `				/* Perform the insertion (pObj may point into pVm->aMemObj; the` |
|        - | 12972 | `				 * inserter snapshots the source before reserving, so the pool may` |
|        - | 12973 | `				 * safely move underneath it — see HashmapInsertIntKey/BlobKey). */` |
|      115 | 12974 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      115 | 12975 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      115 | 12976 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      115 | 12977 | `				PH7_MemObjRelease(&sKey);` |
|       57 | 12978 | `			}` |
|       57 | 12979 | `		}` |
|       58 | 12980 | `	}` |
|      117 | 12981 | `	return SXRET_OK;` |
|        1 | 12982 |  |
|        - | 12983 | `/*` |
|        - | 12984 | ` * array get_defined_vars(void)` |
|        - | 12985 | ` *  Returns an array of all defined variables.` |
|        - | 12986 | ` * Parameter` |
|        - | 12987 | ` *  None` |
|        - | 12988 | ` * Return` |
|        - | 12989 | ` *  An array with all the variables defined in the current scope.` |
|        - | 12990 | ` */` |
|        2 | 12991 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12992 |  |
|        3 | 12993 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12994 | `	ph7_value *pArray;` |
|        - | 12995 | `	/* Create a new array */` |
|        3 | 12996 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12997 | ` 	if( pArray == 0 ){` |
|      ! 0 | 12998 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12999 | `		SXUNUSED(apArg);` |
|        - | 13000 | `		/* Return NULL */` |
|      ! 0 | 13001 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13002 | `		return SXRET_OK;` |
|        - | 13003 | `	}` |
|        - | 13004 | `	/* Superglobals first */` |
|        3 | 13005 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 13006 | `	/* Then variable defined in the current frame */` |
|        3 | 13007 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 13008 | `	/* Finally,return the created array */` |
|        3 | 13009 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 13010 | `	return SXRET_OK;` |
|        2 | 13011 |  |
|        - | 13012 | `/*` |
|        - | 13013 | ` * bool gettype($var)` |
|        - | 13014 | ` *  Get the type of a variable` |
|        - | 13015 | ` * Parameters` |
|        - | 13016 | ` *   $var` |
|        - | 13017 | ` *    The variable being type checked.` |
|        - | 13018 | ` * Return` |
|        - | 13019 | ` *   String representation of the given variable type.` |
|        - | 13020 | ` */` |
|       32 | 13021 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13022 |  |
|       34 | 13023 | `	const char *zType = "Empty";` |
|       34 | 13024 | `	if( nArg > 0 ){` |
|       34 | 13025 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 13026 | `	}` |
|        - | 13027 | `	/* Return the variable type */` |
|       34 | 13028 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 13029 | `	return SXRET_OK;` |
|        2 | 13030 |  |
|        - | 13031 | `/*` |
|        - | 13032 | ` * string get_resource_type(resource $handle)` |
|        - | 13033 | ` *  This function gets the type of the given resource.` |
|        - | 13034 | ` * Parameters` |
|        - | 13035 | ` *  $handle` |
|        - | 13036 | ` *  The evaluated resource handle.` |
|        - | 13037 | ` * Return` |
|        - | 13038 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 13039 | ` *  representing its type. If the type is not identified by this function` |
|        - | 13040 | ` *  the return value will be the string Unknown.` |
|        - | 13041 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 13042 | ` *  is not a resource.` |
|        - | 13043 | ` */` |
|        2 | 13044 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13045 |  |
|        3 | 13046 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13047 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 13048 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13049 | `		return PH7_OK;` |
|        - | 13050 | `	}` |
|        3 | 13051 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 13052 | `	return SXRET_OK;` |
|        2 | 13053 |  |
|        - | 13054 | `/*` |
|        - | 13055 | ` * void var_dump(expression,....)` |
|        - | 13056 | ` *   var_dump � Dumps information about a variable` |
|        - | 13057 | ` * Parameters` |
|        - | 13058 | ` *   One or more expression to dump.` |
|        - | 13059 | ` * Returns` |
|        - | 13060 | ` *  Nothing.` |
|        - | 13061 | ` */` |
|      218 | 13062 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13063 |  |
|        - | 13064 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 13065 | `	int i;` |
|      220 | 13066 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 13067 | `	/* Dump one or more expressions */` |
|      444 | 13068 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 13069 | `		ph7_value *pObj = apArg[i];` |
|        - | 13070 | `		/* Reset the working buffer */` |
|      226 | 13071 | `		SyBlobReset(&sDump);` |
|        - | 13072 | `		/* Dump the given expression */` |
|      226 | 13073 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 13074 | `		/* Output */` |
|      226 | 13075 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 13076 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 13077 | `		}` |
|      114 | 13078 | `	}` |
|        - | 13079 | `	/* Release the working buffer */` |
|      220 | 13080 | `	SyBlobRelease(&sDump);` |
|      220 | 13081 | `	return SXRET_OK;` |
|        2 | 13082 |  |
|        - | 13083 | `/*` |
|        - | 13084 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 13085 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 13086 | ` * Parameters` |
|        - | 13087 | ` *   expression: Expression to dump` |
|        - | 13088 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 13089 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 13090 | ` *            print_r() will return the information rather than print it.` |
|        - | 13091 | ` * Return` |
|        - | 13092 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 13093 | ` *  Otherwise, the return value is TRUE.` |
|        - | 13094 | ` */` |
|       16 | 13095 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13096 |  |
|       17 | 13097 | `	int ret_string = 0;` |
|        - | 13098 | `	SyBlob sDump;` |
|       17 | 13099 | `	if( nArg < 1 ){` |
|        - | 13100 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 13101 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13102 | `		return SXRET_OK;` |
|        - | 13103 | `	}` |
|       17 | 13104 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 13105 | `	if ( nArg > 1 ){` |
|        - | 13106 | `		/* Where to redirect output */` |
|       11 | 13107 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 13108 | `	}` |
|        - | 13109 | `	/* Generate dump */` |
|       17 | 13110 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 13111 | `	if( !ret_string ){` |
|        - | 13112 | `		/* Output dump */` |
|        7 | 13113 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13114 | `		/* Return true */` |
|        7 | 13115 | `		ph7_result_bool(pCtx,1);` |
|        4 | 13116 | `	}else{` |
|        - | 13117 | `		/* Generated dump as return value */` |
|       11 | 13118 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13119 | `	}` |
|        - | 13120 | `	/* Release the working buffer */` |
|       17 | 13121 | `	SyBlobRelease(&sDump);` |
|       17 | 13122 | `	return SXRET_OK;` |
|        9 | 13123 |  |
|        - | 13124 | `/*` |
|        - | 13125 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 13126 | ` * Same job as print_r. (see coment above)` |
|        - | 13127 | ` */` |
|        2 | 13128 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13129 |  |
|        3 | 13130 | `	int ret_string = 0;` |
|        - | 13131 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 13132 | `	if( nArg < 1 ){` |
|        - | 13133 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 13134 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13135 | `		return SXRET_OK;` |
|        - | 13136 | `	}` |
|        3 | 13137 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 13138 | `	if ( nArg > 1 ){` |
|        - | 13139 | `		/* Where to redirect output */` |
|        3 | 13140 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 13141 | `	}` |
|        - | 13142 | `	/* Generate dump */` |
|        3 | 13143 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 13144 | `	if( !ret_string ){` |
|        - | 13145 | `		/* Output dump */` |
|      ! 0 | 13146 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13147 | `		/* Return NULL */` |
|      ! 0 | 13148 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13149 | `	}else{` |
|        - | 13150 | `		/* Generated dump as return value */` |
|        3 | 13151 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13152 | `	}` |
|        - | 13153 | `	/* Release the working buffer */` |
|        3 | 13154 | `	SyBlobRelease(&sDump);` |
|        3 | 13155 | `	return SXRET_OK;` |
|        2 | 13156 |  |
|        - | 13157 | `/*` |
|        - | 13158 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 13159 | ` *  Set/get the various assert flags.` |
|        - | 13160 | ` * Parameter` |
|        - | 13161 | ` * $what` |
|        - | 13162 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 13163 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 13164 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 13165 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 13166 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 13167 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 13168 | ` * $value` |
|        - | 13169 | ` *   An optional new value for the option.` |
|        - | 13170 | ` * Return` |
|        - | 13171 | ` *  Old setting on success or FALSE on failure.` |
|        - | 13172 | ` */` |
|       28 | 13173 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13174 |  |
|       30 | 13175 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13176 | `	int iOption;` |
|        - | 13177 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 13178 | `	if( nArg < 1 ){` |
|        3 | 13179 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13180 | `			"ArgumentCountError",` |
|        - | 13181 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 13182 | `			);` |
|        - | 13183 | `	}` |
|        - | 13184 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 13185 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 13186 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 13187 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13188 | `			"TypeError",` |
|        - | 13189 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 13190 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 13191 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 13192 | `			);` |
|        - | 13193 | `	}` |
|       28 | 13194 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 13195 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 13196 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 13197 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 13198 | `	switch( iOption ){` |
|        5 | 13199 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 13200 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 13201 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 13202 | `		if( nArg > 1 ){` |
|        5 | 13203 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 13204 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 13205 | `			}else{` |
|        3 | 13206 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 13207 | `			}` |
|        2 | 13208 | `		}` |
|       12 | 13209 | `		break;` |
|        1 | 13210 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 13211 | `		/* Return old callback or null */` |
|        3 | 13212 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 13213 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 13214 | `		}else{` |
|        3 | 13215 | `			ph7_result_null(pCtx);` |
|        - | 13216 | `		}` |
|        3 | 13217 | `		if( nArg > 1 ){` |
|      ! 0 | 13218 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 13219 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 13220 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 13221 | `			}else{` |
|      ! 0 | 13222 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 13223 | `			}` |
|      ! 0 | 13224 | `		}` |
|        3 | 13225 | `		break;` |
|        5 | 13226 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 13227 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 13228 | `		if( nArg > 1 ){` |
|        5 | 13229 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 13230 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 13231 | `			}else{` |
|        3 | 13232 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 13233 | `			}` |
|        2 | 13234 | `		}` |
|       11 | 13235 | `		break;` |
|      ! 0 | 13236 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 13237 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 13238 | `		break;` |
|        1 | 13239 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 13240 | `		ph7_result_int(pCtx, 1);` |
|        3 | 13241 | `		break;` |
|      ! 0 | 13242 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 13243 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 13244 | `		break;` |
|        1 | 13245 | `	default:` |
|        - | 13246 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 13247 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13248 | `			"ValueError",` |
|        - | 13249 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 13250 | `			);` |
|        - | 13251 | `	}` |
|       26 | 13252 | `	return PH7_OK;` |
|       16 | 13253 |  |
|        - | 13254 | `/*` |
|        - | 13255 | ` * bool assert(mixed $assertion)` |
|        - | 13256 | ` *  Checks if assertion is FALSE.` |
|        - | 13257 | ` * Parameter` |
|        - | 13258 | ` *  $assertion` |
|        - | 13259 | ` *    The assertion to test.` |
|        - | 13260 | ` * Return` |
|        - | 13261 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 13262 | ` */` |
|       24 | 13263 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13264 |  |
|       26 | 13265 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13266 | `	int iFlags,iResult;` |
|        - | 13267 | `	const char *zDesc;` |
|        - | 13268 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 13269 | `	if( nArg < 1 ){` |
|        3 | 13270 | `		return PH7_VmThrowException(pCtx,` |
|        - | 13271 | `			"ArgumentCountError",` |
|        - | 13272 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 13273 | `			);` |
|        - | 13274 | `	}` |
|       24 | 13275 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 13276 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 13277 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 13278 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 13279 | `		return PH7_OK;` |
|        - | 13280 | `	}` |
|        - | 13281 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 13282 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 13283 | `	if( !iResult ){` |
|        - | 13284 | `		/* Assertion failed */` |
|        - | 13285 | `		/* Extract optional description */` |
|       13 | 13286 | `		zDesc = 0;` |
|       13 | 13287 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 13288 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 13289 | `		}` |
|       13 | 13290 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 13291 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 13292 | `			ph7_value sFile,sLine;` |
|        - | 13293 | `			ph7_value *apCbArg[3];` |
|        - | 13294 | `			SyString *pFile;` |
|        - | 13295 | `			/* Extract the processed script */` |
|      ! 0 | 13296 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 13297 | `			if( pFile == 0 ){` |
|      ! 0 | 13298 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 13299 | `			}` |
|        - | 13300 | `			/* Invoke the callback */` |
|      ! 0 | 13301 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 13302 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 13303 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 13304 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 13305 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 13306 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 13307 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 13308 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 13309 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 13310 | `		}` |
|       13 | 13311 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 13312 | `			/* Abort VM execution immediately */` |
|      ! 0 | 13313 | `			return PH7_ABORT;` |
|        - | 13314 | `		}` |
|        - | 13315 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 13316 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 13317 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13318 | `				"AssertionError",` |
|        - | 13319 | `				"%s",` |
|        1 | 13320 | `				zDesc` |
|        - | 13321 | `				);` |
|      ! 0 | 13322 | `		}else{` |
|       11 | 13323 | `			return PH7_VmThrowException(pCtx,` |
|        - | 13324 | `				"AssertionError",` |
|        - | 13325 | `				"assert(false)"` |
|        - | 13326 | `				);` |
|        - | 13327 | `		}` |
|        - | 13328 | `	}` |
|        - | 13329 | `	/* Assertion passed */` |
|       11 | 13330 | `	ph7_result_bool(pCtx,1);` |
|       11 | 13331 | `	return PH7_OK;` |
|       14 | 13332 |  |
|        - | 13333 | `/*` |
|        - | 13334 | ` * Section:` |
|        - | 13335 | ` *  Error reporting functions.` |
|        - | 13336 | ` * Status:` |
|        - | 13337 | ` *    Stable.` |
|        - | 13338 | ` */` |
|        - | 13339 | `/*` |
|        - | 13340 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 13341 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 13342 | ` * Parameters` |
|        - | 13343 | ` *  $error_msg` |
|        - | 13344 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 13345 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 13346 | ` * $error_type` |
|        - | 13347 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 13348 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 13349 | ` * Return` |
|        - | 13350 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 13351 | ` */` |
|       12 | 13352 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13353 |  |
|       14 | 13354 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 13355 | `	int rc = PH7_OK;` |
|       14 | 13356 | `	if( nArg > 0 ){` |
|        - | 13357 | `		const char *zErr;` |
|        - | 13358 | `		int nLen;` |
|        - | 13359 | `		/* Extract the error message */` |
|       12 | 13360 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 13361 | `		if( nArg > 1 ){` |
|        - | 13362 | `			/* Extract the error type */` |
|       12 | 13363 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 13364 | `			switch( nErr ){` |
|        1 | 13365 | `			case 1:   /* E_ERROR */` |
|        - | 13366 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 13367 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 13368 | `			case 256: /* E_USER_ERROR */` |
|        3 | 13369 | `				nErr = PH7_CTX_ERR;` |
|        3 | 13370 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 13371 | `				break;` |
|        1 | 13372 | `			case 2:   /* E_WARNING */` |
|        - | 13373 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 13374 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 13375 | `			case 512: /* E_USER_WARNING */` |
|        3 | 13376 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 13377 | `				break;` |
|        3 | 13378 | `			default:` |
|        8 | 13379 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 13380 | `				break;` |
|        - | 13381 | `			}` |
|        5 | 13382 | `		}` |
|        - | 13383 | `		/* Report error */` |
|       12 | 13384 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 13385 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 13386 | `			return rc;` |
|        - | 13387 | `		}` |
|        - | 13388 | `		/* Return true */` |
|       12 | 13389 | `		ph7_result_bool(pCtx,1);` |
|        7 | 13390 | `	}else{` |
|        - | 13391 | `		/* Missing arguments,return FALSE */` |
|        3 | 13392 | `		ph7_result_bool(pCtx,0);` |
|        - | 13393 | `	}` |
|       14 | 13394 | `	return rc;` |
|        8 | 13395 |  |
|        - | 13396 | `/*` |
|        - | 13397 | ` * int error_reporting([int $level])` |
|        - | 13398 | ` *  Sets which PHP errors are reported.` |
|        - | 13399 | ` * Parameters` |
|        - | 13400 | ` *  $level` |
|        - | 13401 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 13402 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 13403 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 13404 | ` *   levels will not always behave as expected.` |
|        - | 13405 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 13406 | ` *   in the predefined constants.` |
|        - | 13407 | ` * Return` |
|        - | 13408 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 13409 | ` *   parameter is given.` |
|        - | 13410 | ` */` |
|       32 | 13411 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13412 |  |
|       34 | 13413 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13414 | `	int nOld;` |
|        - | 13415 | `	/* Extract the old reporting level */` |
|       34 | 13416 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       34 | 13417 | `	if( nArg > 0 ){` |
|        - | 13418 | `		int nNew;` |
|        - | 13419 | `		/* Extract the desired error reporting level */` |
|       28 | 13420 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       28 | 13421 | `		if( !nNew ){` |
|        - | 13422 | `			/* Do not report errors at all */` |
|        5 | 13423 | `			pVm->bErrReport = 0;` |
|        3 | 13424 | `		}else{` |
|        - | 13425 | `			/* Report all errors */` |
|       24 | 13426 | `			pVm->bErrReport = 1;` |
|        - | 13427 | `		}` |
|       13 | 13428 | `	}` |
|        - | 13429 | `	/* Return the old level */` |
|       34 | 13430 | `	ph7_result_int(pCtx,nOld);` |
|       34 | 13431 | `	return PH7_OK;` |
|        2 | 13432 |  |
|        - | 13433 | `/*` |
|        - | 13434 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 13435 | ` *  Send an error message somewhere.` |
|        - | 13436 | ` * Parameter` |
|        - | 13437 | ` *  $message` |
|        - | 13438 | ` *   The error message that should be logged.` |
|        - | 13439 | ` *  $message_type` |
|        - | 13440 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 13441 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 13442 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 13443 | ` *       This is the default option.` |
|        - | 13444 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 13445 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 13446 | ` *    2  No longer an option.` |
|        - | 13447 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 13448 | ` *       to the end of the message string.` |
|        - | 13449 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 13450 | ` *  $destination` |
|        - | 13451 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 13452 | ` *  $extra_headers` |
|        - | 13453 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 13454 | ` * Return` |
|        - | 13455 | ` *  TRUE on success or FALSE on failure.` |
|        - | 13456 | ` * NOTE:` |
|        - | 13457 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 13458 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 13459 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 13460 | ` *  Otherwise this function is no-op.` |
|        - | 13461 | ` */` |
|        4 | 13462 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13463 |  |
|        - | 13464 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 13465 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 13466 | `	int iType = 0;` |
|        5 | 13467 | `	if( nArg < 1 ){` |
|        - | 13468 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 13469 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13470 | `		return PH7_OK;` |
|        - | 13471 | `	}` |
|        5 | 13472 | `	if( pVm->xErrLog  ){` |
|        - | 13473 | `		/* Invoke the user callback */` |
|      ! 0 | 13474 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 13475 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 13476 | `		if( nArg > 1 ){` |
|      ! 0 | 13477 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 13478 | `			if( nArg > 2 ){` |
|      ! 0 | 13479 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 13480 | `				if( nArg > 3 ){` |
|      ! 0 | 13481 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 13482 | `				}` |
|      ! 0 | 13483 | `			}` |
|      ! 0 | 13484 | `		}` |
|      ! 0 | 13485 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 13486 | `	}` |
|        - | 13487 | `	/* Retun TRUE */` |
|        5 | 13488 | `	ph7_result_bool(pCtx,1);` |
|        5 | 13489 | `	return PH7_OK;` |
|        3 | 13490 |  |
|        - | 13491 | `/*` |
|        - | 13492 | ` * bool restore_exception_handler(void)` |
|        - | 13493 | ` *  Restores the previously defined exception handler function.` |
|        - | 13494 | ` * Parameter` |
|        - | 13495 | ` *  None` |
|        - | 13496 | ` * Return` |
|        - | 13497 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 13498 | ` */` |
|        4 | 13499 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13500 |  |
|        5 | 13501 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13502 | `	ph7_value *pOld,*pNew;` |
|        - | 13503 | `	/* Point to the old and the new handler */` |
|        5 | 13504 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 13505 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 13506 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 13507 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 13508 | `		SXUNUSED(apArg);` |
|        - | 13509 | `		/* No installed handler,return FALSE */` |
|        5 | 13510 | `		ph7_result_bool(pCtx,0);` |
|        5 | 13511 | `		return PH7_OK;` |
|        - | 13512 | `	}` |
|        - | 13513 | `	/* Copy the old handler */` |
|      ! 0 | 13514 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13515 | `	PH7_MemObjRelease(pOld);` |
|        - | 13516 | `	/* Return TRUE */` |
|      ! 0 | 13517 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13518 | `	return PH7_OK;` |
|        3 | 13519 |  |
|        - | 13520 | `/*` |
|        - | 13521 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 13522 | ` *  Sets a user-defined exception handler function.` |
|        - | 13523 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 13524 | ` * NOTE` |
|        - | 13525 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 13526 | ` *  the satndard PHP engine.` |
|        - | 13527 | ` * Parameters` |
|        - | 13528 | ` *  $exception_handler` |
|        - | 13529 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 13530 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 13531 | ` *   that was thrown.` |
|        - | 13532 | ` *  Note:` |
|        - | 13533 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13534 | ` * Return` |
|        - | 13535 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 13536 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13537 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13538 | ` */` |
|        4 | 13539 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13540 |  |
|        6 | 13541 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13542 | `	ph7_value *pOld,*pNew;` |
|        - | 13543 | `	/* Point to the old and the new handler */` |
|        6 | 13544 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 13545 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 13546 | `	/* Return the old handler */` |
|        6 | 13547 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 13548 | `	if( nArg > 0 ){` |
|        6 | 13549 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13550 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 13551 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 13552 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 13553 | `		}else{` |
|        6 | 13554 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13555 | `			/* Install the new handler */` |
|        6 | 13556 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13557 | `		}` |
|        2 | 13558 | `	}` |
|        6 | 13559 | `	return PH7_OK;` |
|        2 | 13560 |  |
|        - | 13561 | `/*` |
|        - | 13562 | ` * bool restore_error_handler(void)` |
|        - | 13563 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13564 | ` * Parameters:` |
|        - | 13565 | ` *  None.` |
|        - | 13566 | ` * Return` |
|        - | 13567 | ` *  Always TRUE.` |
|        - | 13568 | ` */` |
|        6 | 13569 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13570 |  |
|        7 | 13571 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13572 | `	ph7_value *pOld,*pNew;` |
|        - | 13573 | `	/* Point to the old and the new handler */` |
|        7 | 13574 | `	pOld = &pVm->aErrCB[0];` |
|        7 | 13575 | `	pNew = &pVm->aErrCB[1];` |
|        7 | 13576 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 13577 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 13578 | `		SXUNUSED(apArg);` |
|        - | 13579 | `		/* No installed callback,return FALSE */` |
|        7 | 13580 | `		ph7_result_bool(pCtx,0);` |
|        7 | 13581 | `		return PH7_OK;` |
|        - | 13582 | `	}` |
|        - | 13583 | `	/* Copy the old callback */` |
|      ! 0 | 13584 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13585 | `	PH7_MemObjRelease(pOld);` |
|        - | 13586 | `	/* Return TRUE */` |
|      ! 0 | 13587 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13588 | `	return PH7_OK;` |
|        4 | 13589 |  |
|        - | 13590 | `/*` |
|        - | 13591 | ` * value set_error_handler(callable $error_handler)` |
|        - | 13592 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13593 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13594 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13595 | ` *  Sets a user-defined error handler function.` |
|        - | 13596 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 13597 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 13598 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 13599 | ` *  conditions (using trigger_error()).` |
|        - | 13600 | ` * Parameters` |
|        - | 13601 | ` *  $error_handler` |
|        - | 13602 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 13603 | ` *   describing the error.` |
|        - | 13604 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 13605 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 13606 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 13607 | ` *   The function can be shown as:` |
|        - | 13608 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 13609 | ` *     errno` |
|        - | 13610 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 13611 | ` *   errstr` |
|        - | 13612 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 13613 | ` *   errfile` |
|        - | 13614 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 13615 | ` *     was raised in, as a string.` |
|        - | 13616 | ` *  Note:` |
|        - | 13617 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13618 | ` * Return` |
|        - | 13619 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 13620 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13621 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13622 | ` */` |
|    10860 | 13623 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13624 |  |
|    10862 | 13625 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13626 | `	ph7_value *pOld,*pNew;` |
|        - | 13627 | `	/* Point to the old and the new handler */` |
|    10862 | 13628 | `	pOld = &pVm->aErrCB[0];` |
|    10862 | 13629 | `	pNew = &pVm->aErrCB[1];` |
|        - | 13630 | `	/* Return the old handler */` |
|    10862 | 13631 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    10862 | 13632 | `	if( nArg > 0 ){` |
|    10862 | 13633 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13634 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5425 | 13635 | `			PH7_MemObjRelease(pNew);` |
|     5425 | 13636 | `			ph7_result_bool(pCtx,1);` |
|     2713 | 13637 | `		}else{` |
|     5438 | 13638 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13639 | `			/* Install the new handler */` |
|     5438 | 13640 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13641 | `		}` |
|     5430 | 13642 | `	}` |
|    10862 | 13643 | `	return PH7_OK;` |
|        2 | 13644 |  |
|        - | 13645 | `/*` |
|        - | 13646 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 13647 | ` *  Generates a backtrace.` |
|        - | 13648 | ` * Paramaeter` |
|        - | 13649 | ` *  $options` |
|        - | 13650 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 13651 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 13652 | ` *   all the function/method arguments, to save memory.` |
|        - | 13653 | ` * $limit` |
|        - | 13654 | ` *   (Not Used)` |
|        - | 13655 | ` * Return` |
|        - | 13656 | ` *  An array.The possible returned elements are as follows:` |
|        - | 13657 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 13658 | ` *          Name        Type      Description` |
|        - | 13659 | ` *          ------      ------     -----------` |
|        - | 13660 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 13661 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 13662 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 13663 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 13664 | ` *          object      object    The current object.` |
|        - | 13665 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 13666 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 13667 | ` */` |
|      928 | 13668 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13669 |  |
|      930 | 13670 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13671 | `	ph7_value *pArray;` |
|        - | 13672 | `	ph7_class *pClass;` |
|        - | 13673 | `	ph7_value *pValue;` |
|        - | 13674 | `	SyString *pFile;` |
|        - | 13675 | `	/* Create a new array */` |
|      930 | 13676 | `	pArray = ph7_context_new_array(pCtx);` |
|      930 | 13677 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      930 | 13678 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 13679 | `		/* Out of memory,return NULL */` |
|      ! 0 | 13680 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13681 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13682 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13683 | `		SXUNUSED(apArg);` |
|      ! 0 | 13684 | `		return PH7_OK;` |
|        - | 13685 | `	}` |
|        - | 13686 | `	/* Dump running function name and it's arguments  */` |
|      930 | 13687 | `	if( pVm->pFrame->pParent ){` |
|      930 | 13688 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 13689 | `		ph7_vm_func *pFunc;` |
|        - | 13690 | `		ph7_value *pArg;` |
|      930 | 13691 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      930 | 13692 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      930 | 13693 | `		if( pFrame->pParent && pFunc ){` |
|      930 | 13694 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      930 | 13695 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      930 | 13696 | `			ph7_value_reset_string_cursor(pValue);` |
|      464 | 13697 | `		}` |
|        - | 13698 | `		/* Function arguments */` |
|      930 | 13699 | `		pArg = ph7_context_new_array(pCtx);` |
|      930 | 13700 | `		if( pArg  ){` |
|        - | 13701 | `			ph7_value *pObj;` |
|        - | 13702 | `			VmSlot *aSlot;` |
|        - | 13703 | `			sxu32 n;` |
|        - | 13704 | `			/* Start filling the array with the given arguments */` |
|      930 | 13705 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     3718 | 13706 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     2790 | 13707 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     2790 | 13708 | `				if( pObj ){` |
|     2790 | 13709 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1394 | 13710 | `				}` |
|     1396 | 13711 | `			}` |
|        - | 13712 | `			/* Save the array */` |
|      930 | 13713 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      464 | 13714 | `		}` |
|      464 | 13715 | `	}` |
|      930 | 13716 | `	ph7_value_int(pValue,1);` |
|        - | 13717 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 13718 | `	 * line numbers at run-time. )` |
|        - | 13719 | `	 */` |
|      930 | 13720 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 13721 | `	/* Current processed script */` |
|      930 | 13722 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      930 | 13723 | `	if( pFile ){` |
|      930 | 13724 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      930 | 13725 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      930 | 13726 | `		ph7_value_reset_string_cursor(pValue);` |
|      464 | 13727 | `	}` |
|        - | 13728 | `	/* Top class */` |
|      930 | 13729 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      930 | 13730 | `	if( pClass ){` |
|      926 | 13731 | `		ph7_value_reset_string_cursor(pValue);` |
|      926 | 13732 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      926 | 13733 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      462 | 13734 | `	}` |
|        - | 13735 | `	/* Return the freshly created array */` |
|      930 | 13736 | `	ph7_result_value(pCtx,pArray);` |
|        - | 13737 | `	/*` |
|        - | 13738 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 13739 | `	 * as soon we return from this function.` |
|        - | 13740 | `	 */` |
|      930 | 13741 | `	return PH7_OK;` |
|      466 | 13742 |  |
|        - | 13743 | `/*` |
|        - | 13744 | ` * Generate a small backtrace.` |
|        - | 13745 | ` * Store the generated dump in the given BLOB` |
|        - | 13746 | ` */` |
|        4 | 13747 | `static int VmMiniBacktrace(` |
|        - | 13748 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13749 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 13750 | `	)` |
|        1 | 13751 |  |
|        5 | 13752 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 13753 | `	ph7_vm_func *pFunc;` |
|        - | 13754 | `	ph7_class *pClass;` |
|        - | 13755 | `	SyString *pFile;` |
|        - | 13756 | `	/* Called function */` |
|        5 | 13757 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 13758 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 13759 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13760 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 13761 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 13762 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 13763 | `	}else{` |
|      ! 0 | 13764 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 13765 | `	}` |
|        5 | 13766 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 13767 | `	/* Current processed script */` |
|        5 | 13768 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 13769 | `	if( pFile ){` |
|        5 | 13770 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13771 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 13772 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 13773 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 13774 | `	}` |
|        - | 13775 | `	/* Top class */` |
|        5 | 13776 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 13777 | `	if( pClass ){` |
|      ! 0 | 13778 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 13779 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 13780 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 13781 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 13782 | `	}` |
|        5 | 13783 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 13784 | `	/* All done */` |
|        5 | 13785 | `	return SXRET_OK;` |
|        1 | 13786 |  |
|        - | 13787 | `/*` |
|        - | 13788 | ` * void debug_print_backtrace()` |
|        - | 13789 | ` *  Prints a backtrace` |
|        - | 13790 | ` * Parameters` |
|        - | 13791 | ` * None` |
|        - | 13792 | ` * Return` |
|        - | 13793 | ` * NULL` |
|        - | 13794 | ` */` |
|        2 | 13795 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13796 |  |
|        3 | 13797 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13798 | `	SyBlob sDump;` |
|        3 | 13799 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13800 | `	/* Generate the backtrace */` |
|        3 | 13801 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13802 | `	/* Output backtrace */` |
|        3 | 13803 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13804 | `	/* All done,cleanup */` |
|        3 | 13805 | `	SyBlobRelease(&sDump);` |
|        1 | 13806 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13807 | `	SXUNUSED(apArg);` |
|        3 | 13808 | `	return PH7_OK;` |
|        1 | 13809 |  |
|        - | 13810 | `/*` |
|        - | 13811 | ` * string debug_string_backtrace()` |
|        - | 13812 | ` *  Generate a backtrace` |
|        - | 13813 | ` * Parameters` |
|        - | 13814 | ` * None` |
|        - | 13815 | ` * Return` |
|        - | 13816 | ` *  A mini backtrace().` |
|        - | 13817 | ` * Note that this is a symisc extension.` |
|        - | 13818 | ` */` |
|        2 | 13819 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13820 |  |
|        3 | 13821 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13822 | `	SyBlob sDump;` |
|        3 | 13823 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13824 | `	/* Generate the backtrace */` |
|        3 | 13825 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13826 | `	/* Return the backtrace */` |
|        3 | 13827 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 13828 | `	/* All done,cleanup */` |
|        3 | 13829 | `	SyBlobRelease(&sDump);` |
|        1 | 13830 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13831 | `	SXUNUSED(apArg);` |
|        3 | 13832 | `	return PH7_OK;` |
|        1 | 13833 |  |
|        - | 13834 | `/*` |
|        - | 13835 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 13836 | ` * exception is triggered.` |
|        - | 13837 | ` */` |
|      512 | 13838 | `static sxi32 VmUncaughtException(` |
|        - | 13839 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13840 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13841 | `	)` |
|        1 | 13842 |  |
|        - | 13843 | `	ph7_value *apArg[2],sArg;` |
|      513 | 13844 | `	int nArg = 1;` |
|        - | 13845 | `	sxi32 rc;` |
|      513 | 13846 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 13847 | `		/* Nesting limit reached */` |
|      ! 0 | 13848 | `		return SXRET_OK;` |
|        - | 13849 | `	}` |
|        - | 13850 | `	/* Call any exception handler if available */` |
|      513 | 13851 | `	PH7_MemObjInit(pVm,&sArg);` |
|      513 | 13852 | `	if( pThis ){` |
|        - | 13853 | `		/* Load the exception instance */` |
|      513 | 13854 | `		sArg.x.pOther = pThis;` |
|      513 | 13855 | `		pThis->iRef++;` |
|      513 | 13856 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      257 | 13857 | `	}else{` |
|      ! 0 | 13858 | `		nArg = 0;` |
|        - | 13859 | `	}` |
|      513 | 13860 | `	apArg[0] = &sArg;` |
|        - | 13861 | `	/* Call the exception handler if available */` |
|      513 | 13862 | `	pVm->nExceptDepth++;` |
|      513 | 13863 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      513 | 13864 | `	pVm->nExceptDepth--;` |
|      513 | 13865 | `	if( rc != SXRET_OK ){` |
|        - | 13866 | `		SyBlob sMsgBuf;` |
|      511 | 13867 | `		const char *zClass = "Exception";` |
|      511 | 13868 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 13869 | `		const char *zMsg;` |
|        - | 13870 | `		sxu32 nMsg;` |
|        - | 13871 | `		const char *zFuncName;` |
|        - | 13872 | `		int nFuncLen;` |
|      511 | 13873 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      511 | 13874 | `		if( pThis ){` |
|        - | 13875 | `			ph7_class_method *pGetMessage;` |
|        - | 13876 | `			ph7_value sMsg;` |
|        - | 13877 | `			const char *zTmp;` |
|        - | 13878 | `			int nTmp;` |
|      511 | 13879 | `			zClass = pThis->pClass->sName.zString;` |
|      511 | 13880 | `			nClass = pThis->pClass->sName.nByte;` |
|      511 | 13881 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      511 | 13882 | `			if( pGetMessage ){` |
|      511 | 13883 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      511 | 13884 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      511 | 13885 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      511 | 13886 | `					if( zTmp && nTmp > 0 ){` |
|      511 | 13887 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      255 | 13888 | `					}` |
|      255 | 13889 | `				}` |
|      511 | 13890 | `				PH7_MemObjRelease(&sMsg);` |
|      255 | 13891 | `			}` |
|      255 | 13892 | `		}` |
|      511 | 13893 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      511 | 13894 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      511 | 13895 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      511 | 13896 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      511 | 13897 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 13898 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      511 | 13899 | `		rc = SXERR_ABORT;` |
|      255 | 13900 | `	}` |
|      513 | 13901 | `	PH7_MemObjRelease(&sArg);` |
|      513 | 13902 | `	return rc;` |
|      257 | 13903 |  |
|        - | 13904 | `/*` |
|        - | 13905 | ` * Throw a user exception.` |
|        - | 13906 | ` *` |
|        - | 13907 | ` * Exception dispatch follows this sequence:` |
|        - | 13908 | ` *` |
|        - | 13909 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 13910 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 13911 | ` *` |
|        - | 13912 | ` * 2. If NO catch matches:` |
|        - | 13913 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 13914 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 13915 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 13916 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 13917 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 13918 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 13919 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 13920 | ` *` |
|        - | 13921 | ` * 3. If a catch DOES match:` |
|        - | 13922 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 13923 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 13924 | ` *       inside the catch body from immediately propagating past our` |
|        - | 13925 | ` *       finally block.` |
|        - | 13926 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 13927 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 13928 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 13929 | ` *       in pPendingException (step 2c).` |
|        - | 13930 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 13931 | ` *    d. Run finally (if present).` |
|        - | 13932 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 13933 | ` *       that handlers are restored and finally has run.` |
|        - | 13934 | ` */` |
|      858 | 13935 | `static sxi32 VmThrowException(` |
|        - | 13936 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 13937 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13938 | `	)` |
|        2 | 13939 |  |
|        - | 13940 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 13941 | `	ph7_exception **apException;` |
|        - | 13942 | `	ph7_exception *pException;` |
|        - | 13943 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|        - | 13944 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|        - | 13945 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|      860 | 13946 | `	VmCoalesceDisarm(pVm);` |
|        - | 13947 | `	/* Point to the stack of loaded exceptions */` |
|      860 | 13948 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      860 | 13949 | `	pException = 0;` |
|      860 | 13950 | `	pCatch = 0;` |
|      860 | 13951 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 13952 | `		ph7_exception_block *aCatch;` |
|        - | 13953 | `		ph7_class *pClass;` |
|        - | 13954 | `		SyString *aNames;` |
|        - | 13955 | `		sxu32 nNames;` |
|        - | 13956 | `		int matched;` |
|        - | 13957 | `		sxu32 j,k;` |
|        - | 13958 | `		/* Locate the appropriate block to execute */` |
|      340 | 13959 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      340 | 13960 | `		(void)SySetPop(&pVm->aException);` |
|      340 | 13961 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      348 | 13962 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 13963 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      346 | 13964 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      346 | 13965 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      346 | 13966 | `			matched = 0;` |
|      372 | 13967 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 13968 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 13969 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 13970 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      364 | 13971 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      364 | 13972 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 13973 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 13974 | `					continue;` |
|        - | 13975 | `				}` |
|      364 | 13976 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      338 | 13977 | `					matched = 1;` |
|      338 | 13978 | `					break;` |
|        - | 13979 | `				}` |
|       14 | 13980 | `			}` |
|      346 | 13981 | `			if( matched ){` |
|        - | 13982 | `				/* Catch block found,break immediately */` |
|      338 | 13983 | `				pCatch = &aCatch[j];` |
|      338 | 13984 | `				break;` |
|        - | 13985 | `			}` |
|        5 | 13986 | `		}` |
|      169 | 13987 | `	}` |
|        - | 13988 | `	/* Execute the cached block if available */` |
|      860 | 13989 | `	if( pCatch == 0 ){` |
|        - | 13990 | `		sxi32 rc;` |
|        - | 13991 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      524 | 13992 | `		if( pException && pException->iHasFinally ){` |
|        3 | 13993 | `			pException->iFinallyDone = 1;` |
|        3 | 13994 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 13995 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13996 | `				return SXERR_ABORT;` |
|        - | 13997 | `			}` |
|        1 | 13998 | `		}` |
|        - | 13999 | `		/* Check if there is an outer exception handler on the stack */` |
|      524 | 14000 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 14001 | `			/* Re-throw to the outer handler */` |
|        3 | 14002 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 14003 | `		}` |
|        - | 14004 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 14005 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 14006 | `		 * exception instead of reporting it uncaught.` |
|        - | 14007 | `		 */` |
|      522 | 14008 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 14009 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 14010 | `			 * by looking for a catch frame on the stack.` |
|        - | 14011 | `			 */` |
|      522 | 14012 | `			VmFrame *pF = pVm->pFrame;` |
|      522 | 14013 | `			int inCatch = 0;` |
|     1050 | 14014 | `			while( pF ){` |
|      538 | 14015 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        9 | 14016 | `					inCatch = 1;` |
|        9 | 14017 | `					break;` |
|        - | 14018 | `				}` |
|      529 | 14019 | `				pF = pF->pParent;` |
|        1 | 14020 | `			}` |
|      522 | 14021 | `			if( inCatch ){` |
|        - | 14022 | `				/* Defer — will be re-thrown after finally runs */` |
|        9 | 14023 | `				pThis->iRef++;` |
|        9 | 14024 | `				pVm->pPendingException = pThis;` |
|        9 | 14025 | `				return SXRET_OK;` |
|        - | 14026 | `			}` |
|      256 | 14027 | `		}` |
|        - | 14028 | `		/* Truly uncaught */` |
|      513 | 14029 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      513 | 14030 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 14031 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 14032 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 14033 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 14034 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 14035 | `			}` |
|      ! 0 | 14036 | `		}` |
|      513 | 14037 | `		return rc;` |
|      ! 0 | 14038 | `	}else{` |
|      338 | 14039 | `		VmFrame *pFrame = pVm->pFrame;` |
|      338 | 14040 | `		ph7_exception **apSaved = 0;` |
|        - | 14041 | `		sxu32 nSavedCount;` |
|        - | 14042 | `		sxi32 rc;` |
|      338 | 14043 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      338 | 14044 | `		if( pException->pFrame == pFrame ){` |
|      238 | 14045 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      118 | 14046 | `		}` |
|        - | 14047 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 14048 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 14049 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 14050 | `		 */` |
|      338 | 14051 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      338 | 14052 | `		if( nSavedCount > 0 ){` |
|       16 | 14053 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        5 | 14054 | `				nSavedCount * sizeof(ph7_exception *));` |
|       11 | 14055 | `			if( apSaved ){` |
|       16 | 14056 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        5 | 14057 | `					nSavedCount * sizeof(ph7_exception *));` |
|       11 | 14058 | `				SySetReset(&pVm->aException);` |
|        5 | 14059 | `			}` |
|        5 | 14060 | `		}` |
|        - | 14061 | `		/* Create a private frame first */` |
|      338 | 14062 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      338 | 14063 | `		if( rc == SXRET_OK ){` |
|      338 | 14064 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      338 | 14065 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      338 | 14066 | `			if( pObj ){` |
|      338 | 14067 | `				pThis->iRef++;` |
|      338 | 14068 | `				pObj->x.pOther = pThis;` |
|      338 | 14069 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      168 | 14070 | `			}` |
|        - | 14071 | `			/* Execute the catch block */` |
|      338 | 14072 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 14073 | `			/* Leave the frame */` |
|      338 | 14074 | `			VmLeaveFrame(&(*pVm));` |
|      168 | 14075 | `		}` |
|        - | 14076 | `		/* Restore the outer exception handlers */` |
|      338 | 14077 | `		if( apSaved ){` |
|        - | 14078 | `			sxu32 k;` |
|        - | 14079 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 14080 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 14081 | `			 * Restore the original outer entries.` |
|        - | 14082 | `			 */` |
|       11 | 14083 | `			SySetReset(&pVm->aException);` |
|       21 | 14084 | `			for(k = 0; k < nSavedCount; k++){` |
|       11 | 14085 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        6 | 14086 | `			}` |
|       11 | 14087 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        5 | 14088 | `		}` |
|        - | 14089 | `		/* Execute the finally block after catch */` |
|      338 | 14090 | `		if( pException->iHasFinally ){` |
|       16 | 14091 | `			pException->iFinallyDone = 1;` |
|        - | 14092 | `			{` |
|       16 | 14093 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 14094 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 14095 | `					return SXERR_ABORT;` |
|        - | 14096 | `				}` |
|        - | 14097 | `			}` |
|        7 | 14098 | `		}` |
|      338 | 14099 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 14100 | `			return SXERR_ABORT;` |
|        - | 14101 | `		}` |
|        - | 14102 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 14103 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 14104 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 14105 | `		 */` |
|      338 | 14106 | `		if( pVm->pPendingException ){` |
|        9 | 14107 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 14108 | `			pVm->pPendingException = 0;` |
|        9 | 14109 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 14110 | `		}` |
|        - | 14111 | `	}` |
|        - | 14112 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 14113 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 14114 | `	 */` |
|      330 | 14115 | `	return SXRET_OK;` |
|      431 | 14116 |  |
|        - | 14117 | `/*` |
|        - | 14118 | ` * Section:` |
|        - | 14119 | ` *  Version,Credits and Copyright related functions.` |
|        - | 14120 | ` * Status:` |
|        - | 14121 | ` *    Stable.` |
|        - | 14122 | ` */` |
|        - | 14123 | `/*` |
|        - | 14124 | ` * string ph7version(void)` |
|        - | 14125 | ` *  Returns the running version of the PH7 version.` |
|        - | 14126 | ` * Parameters` |
|        - | 14127 | ` *  None` |
|        - | 14128 | ` * Return` |
|        - | 14129 | ` * Current PH7 version.` |
|        - | 14130 | ` */` |
|        2 | 14131 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14132 |  |
|        1 | 14133 | `	SXUNUSED(nArg);` |
|        1 | 14134 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 14135 | `	/* Current engine version */` |
|        3 | 14136 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 14137 | `	return PH7_OK;` |
|        1 | 14138 |  |
|        - | 14139 | `/*` |
|        - | 14140 | ` * string phpversion([ string $extension ])` |
|        - | 14141 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|        - | 14142 | ` * Parameters` |
|        - | 14143 | ` *  $extension (optional): an extension name. PHL has no extension registry, so any` |
|        - | 14144 | ` *  argument yields NULL (PHP returns FALSE for an unknown extension).` |
|        - | 14145 | ` * Return` |
|        - | 14146 | ` *  The PHP-compat version string, or NULL when called with an extension argument.` |
|        - | 14147 | ` */` |
|        4 | 14148 | `static int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14149 |  |
|        2 | 14150 | `	SXUNUSED(apArg); /* cc warning */` |
|        5 | 14151 | `	if( nArg > 0 ){` |
|      ! 0 | 14152 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14153 | `		return PH7_OK;` |
|        - | 14154 | `	}` |
|        5 | 14155 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|        5 | 14156 | `	return PH7_OK;` |
|        3 | 14157 |  |
|        - | 14158 | `/*` |
|        - | 14159 | ` * string php_sapi_name(void)` |
|        - | 14160 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|        - | 14161 | ` * Parameters` |
|        - | 14162 | ` *  None` |
|        - | 14163 | ` * Return` |
|        - | 14164 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|        - | 14165 | ` */` |
|        2 | 14166 | `static int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14167 |  |
|        3 | 14168 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|        1 | 14169 | `	SXUNUSED(nArg);` |
|        1 | 14170 | `	SXUNUSED(apArg); /* cc warning */` |
|        3 | 14171 | `	ph7_result_string(pCtx,zSapi,-1);` |
|        3 | 14172 | `	return PH7_OK;` |
|        1 | 14173 |  |
|        - | 14174 | `/*` |
|        - | 14175 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 14176 | ` */` |
|        - | 14177 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 14178 | ` "<html><head>"\` |
|        - | 14179 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 14180 | ` "<style type=\"text/css\">"\` |
|        - | 14181 | ` "div {"\` |
|        - | 14182 | `     "border: 1px solid #cccccc;"\` |
|        - | 14183 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 14184 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 14185 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 14186 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 14187 | `     "-webkit-border-radius: 10px;"\` |
|        - | 14188 | `     "-o-border-radius: 10px;"\` |
|        - | 14189 | `     "border-radius: 10px;"\` |
|        - | 14190 | `     "padding-left: 2em;"\` |
|        - | 14191 | `     "background-color: white;"\` |
|        - | 14192 | `     "margin-left: auto;"\` |
|        - | 14193 | `     "font-family: verdana;"\` |
|        - | 14194 | `     "padding-right: 2em;"\` |
|        - | 14195 | `     "margin-right: auto;"\` |
|        - | 14196 | `     "}"\` |
|        - | 14197 | `     "body {"\` |
|        - | 14198 | `     "padding: 0.2em;"\` |
|        - | 14199 | `     "font-style: normal;"\` |
|        - | 14200 | `     "font-size: medium;"\` |
|        - | 14201 | `     "background-color: #f2f2f2;"\` |
|        - | 14202 | `     "}"\` |
|        - | 14203 | `     "hr {"\` |
|        - | 14204 | `     "border-style: solid none none;"\` |
|        - | 14205 | `     "border-width: 1px medium medium;"\` |
|        - | 14206 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 14207 | `     "height: 1px;"\` |
|        - | 14208 | `     "}"\` |
|        - | 14209 | `     "a {"\` |
|        - | 14210 | `     "color: #3366cc;"\` |
|        - | 14211 | `     "text-decoration: none;"\` |
|        - | 14212 | `     "}"\` |
|        - | 14213 | `     "a:hover {"\` |
|        - | 14214 | `     "color: #999999;"\` |
|        - | 14215 | `     "}"\` |
|        - | 14216 | `     "a:active {"\` |
|        - | 14217 | `     "color: #663399;"\` |
|        - | 14218 | `     "}"\` |
|        - | 14219 | `     "h1 {"\` |
|        - | 14220 | `     "margin: 0;"\` |
|        - | 14221 | `     "padding: 0;"\` |
|        - | 14222 | `     "font-family: Verdana;"\` |
|        - | 14223 | `     "font-weight: bold;"\` |
|        - | 14224 | `     "font-style: normal;"\` |
|        - | 14225 | `     "font-size: medium;"\` |
|        - | 14226 | `     "text-transform: capitalize;"\` |
|        - | 14227 | `     "color: #0a328c;"\` |
|        - | 14228 | `     "}"\` |
|        - | 14229 | `     "p {"\` |
|        - | 14230 | `     "margin: 0 auto;"\` |
|        - | 14231 | `     "font-size: medium;"\` |
|        - | 14232 | `     "font-style: normal;"\` |
|        - | 14233 | `     "font-family: verdana;"\` |
|        - | 14234 | `     "}"\` |
|        - | 14235 | `"</style></head><body>"\` |
|        - | 14236 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 14237 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 14238 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 14239 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 14240 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 14241 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 14242 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 14243 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 14244 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 14245 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 14246 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 14247 |  |
|        - | 14248 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14249 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 14250 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 14251 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 14252 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14253 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 14254 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 14255 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 14256 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 14257 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 14258 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 14259 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 14260 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 14261 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 14262 |  |
|        - | 14263 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 14264 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 14265 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 14266 | `"&nbsp;*<br>"\` |
|        - | 14267 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 14268 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 14269 | `"&nbsp;* are met:<br>"\` |
|        - | 14270 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 14271 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 14272 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 14273 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 14274 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 14275 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 14276 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 14277 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 14278 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 14279 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 14280 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 14281 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 14282 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 14283 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 14284 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 14285 | `"&nbsp;*<br>"\` |
|        - | 14286 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 14287 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 14288 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 14289 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 14290 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 14291 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 14292 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 14293 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 14294 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 14295 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 14296 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 14297 | `"&nbsp;*/<br>"\` |
|        - | 14298 | `"</span></small></small></p>"\` |
|        - | 14299 | `"</div></body></html>"` |
|        - | 14300 | `/*` |
|        - | 14301 | ` * bool ph7credits(void)` |
|        - | 14302 | ` * bool ph7info(void)` |
|        - | 14303 | ` * bool ph7copyright(void)` |
|        - | 14304 | ` *  Prints out the credits for PH7 engine` |
|        - | 14305 | ` * Parameters` |
|        - | 14306 | ` *  None` |
|        - | 14307 | ` * Return` |
|        - | 14308 | ` *  Always TRUE` |
|        - | 14309 | ` */` |
|        2 | 14310 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14311 |  |
|        3 | 14312 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 14313 | `	/* Expand the HTML page above*/` |
|        3 | 14314 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 14315 | `	ph7_context_output_format(` |
|        1 | 14316 | `		pCtx,` |
|        - | 14317 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 14318 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 14319 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 14320 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 14321 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 14322 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 14323 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 14324 | `#ifdef __WINNT__` |
|        - | 14325 | `		"Windows NT"` |
|        - | 14326 | `#elif defined(__UNIXES__)` |
|        - | 14327 | `		"UNIX-Like"` |
|        - | 14328 | `#else` |
|        - | 14329 | `		"Other OS"` |
|        - | 14330 | `#endif` |
|        - | 14331 | `		);` |
|        3 | 14332 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 14333 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14334 | `	SXUNUSED(apArg);` |
|        - | 14335 | `	/* Return TRUE */` |
|        - | 14336 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 14337 | `	return PH7_OK;` |
|        1 | 14338 |  |
|        - | 14339 | `/*` |
|        - | 14340 | ` * Section:` |
|        - | 14341 | ` *    URL related routines.` |
|        - | 14342 | ` * Status:` |
|        - | 14343 | ` *    Stable.` |
|        - | 14344 | ` */` |
|        - | 14345 | `/*` |
|        - | 14346 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 14347 | ` *  Parse a URL and return its fields.` |
|        - | 14348 | ` * Parameters` |
|        - | 14349 | ` *  $url` |
|        - | 14350 | ` *   The URL to parse.` |
|        - | 14351 | ` * $component` |
|        - | 14352 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 14353 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 14354 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 14355 | ` *  in which case the return value will be an integer).` |
|        - | 14356 | ` * Return` |
|        - | 14357 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 14358 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 14359 | ` *  this array are:` |
|        - | 14360 | ` *   scheme - e.g. http` |
|        - | 14361 | ` *   host` |
|        - | 14362 | ` *   port` |
|        - | 14363 | ` *   user` |
|        - | 14364 | ` *   pass` |
|        - | 14365 | ` *   path` |
|        - | 14366 | ` *   query - after the question mark ?` |
|        - | 14367 | ` *   fragment - after the hashmark #` |
|        - | 14368 | ` * Note:` |
|        - | 14369 | ` *  FALSE is returned on failure.` |
|        - | 14370 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 14371 | ` *  with the standard PHP engine.` |
|        - | 14372 | ` */` |
|       28 | 14373 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14374 |  |
|        - | 14375 | `	const char *zStr; /* Input string */` |
|        - | 14376 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 14377 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 14378 | `	int nLen;` |
|        - | 14379 | `	sxi32 rc;` |
|       29 | 14380 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 14381 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 14382 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14383 | `		return PH7_OK;` |
|        - | 14384 | `	}` |
|        - | 14385 | `	/* Extract the given URI */` |
|       29 | 14386 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 14387 | `	if( nLen < 1 ){` |
|        - | 14388 | `		/* Nothing to process,return FALSE */` |
|        3 | 14389 | `		ph7_result_bool(pCtx,0);` |
|        3 | 14390 | `		return PH7_OK;` |
|        - | 14391 | `	}` |
|        - | 14392 | `	/* Get a parse */` |
|       27 | 14393 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 14394 | `	if( rc != SXRET_OK ){` |
|        - | 14395 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 14396 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14397 | `		return PH7_OK;` |
|        - | 14398 | `	}` |
|       27 | 14399 | `	if( nArg > 1 ){` |
|      ! 0 | 14400 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 14401 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 14402 | `		switch(nComponent){` |
|      ! 0 | 14403 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 14404 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 14405 | `			if( pComp->nByte < 1 ){` |
|        - | 14406 | `				/* No available value,return NULL */` |
|      ! 0 | 14407 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14408 | `			}else{` |
|      ! 0 | 14409 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14410 | `			}` |
|      ! 0 | 14411 | `			break;` |
|      ! 0 | 14412 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 14413 | `			pComp = &sURI.sHost;` |
|      ! 0 | 14414 | `			if( pComp->nByte < 1 ){` |
|        - | 14415 | `				/* No available value,return NULL */` |
|      ! 0 | 14416 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14417 | `			}else{` |
|      ! 0 | 14418 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14419 | `			}` |
|      ! 0 | 14420 | `			break;` |
|      ! 0 | 14421 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 14422 | `			pComp = &sURI.sPort;` |
|      ! 0 | 14423 | `			if( pComp->nByte < 1 ){` |
|        - | 14424 | `				/* No available value,return NULL */` |
|      ! 0 | 14425 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14426 | `			}else{` |
|      ! 0 | 14427 | `				int iPort = 0;` |
|        - | 14428 | `				/* Cast the value to integer */` |
|      ! 0 | 14429 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 14430 | `				ph7_result_int(pCtx,iPort);` |
|        - | 14431 | `			}` |
|      ! 0 | 14432 | `			break;` |
|      ! 0 | 14433 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 14434 | `			pComp = &sURI.sUser;` |
|      ! 0 | 14435 | `			if( pComp->nByte < 1 ){` |
|        - | 14436 | `				/* No available value,return NULL */` |
|      ! 0 | 14437 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14438 | `			}else{` |
|      ! 0 | 14439 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14440 | `			}` |
|      ! 0 | 14441 | `			break;` |
|      ! 0 | 14442 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 14443 | `			pComp = &sURI.sPass;` |
|      ! 0 | 14444 | `			if( pComp->nByte < 1 ){` |
|        - | 14445 | `				/* No available value,return NULL */` |
|      ! 0 | 14446 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14447 | `			}else{` |
|      ! 0 | 14448 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14449 | `			}` |
|      ! 0 | 14450 | `			break;` |
|      ! 0 | 14451 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 14452 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 14453 | `			if( pComp->nByte < 1 ){` |
|        - | 14454 | `				/* No available value,return NULL */` |
|      ! 0 | 14455 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14456 | `			}else{` |
|      ! 0 | 14457 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14458 | `			}` |
|      ! 0 | 14459 | `			break;` |
|      ! 0 | 14460 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 14461 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 14462 | `			if( pComp->nByte < 1 ){` |
|        - | 14463 | `				/* No available value,return NULL */` |
|      ! 0 | 14464 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14465 | `			}else{` |
|      ! 0 | 14466 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14467 | `			}` |
|      ! 0 | 14468 | `			break;` |
|      ! 0 | 14469 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 14470 | `			pComp = &sURI.sPath;` |
|      ! 0 | 14471 | `			if( pComp->nByte < 1 ){` |
|        - | 14472 | `				/* No available value,return NULL */` |
|      ! 0 | 14473 | `				ph7_result_null(pCtx);` |
|      ! 0 | 14474 | `			}else{` |
|      ! 0 | 14475 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 14476 | `			}` |
|      ! 0 | 14477 | `			break;` |
|      ! 0 | 14478 | `		default:` |
|        - | 14479 | `			/* No such entry,return NULL */` |
|      ! 0 | 14480 | `			ph7_result_null(pCtx);` |
|      ! 0 | 14481 | `			break;` |
|        - | 14482 | `		}` |
|      ! 0 | 14483 | `	}else{` |
|        - | 14484 | `		ph7_value *pArray,*pValue;` |
|        - | 14485 | `		/* Return an associative array */` |
|       27 | 14486 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 14487 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 14488 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 14489 | `			/* Out of memory */` |
|      ! 0 | 14490 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14491 | `			/* Return false */` |
|      ! 0 | 14492 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 14493 | `			return PH7_OK;` |
|        - | 14494 | `		}` |
|        - | 14495 | `		/* Fill the array */` |
|       27 | 14496 | `		pComp = &sURI.sScheme;` |
|       27 | 14497 | `		if( pComp->nByte > 0 ){` |
|       19 | 14498 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 14499 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 14500 | `		}` |
|        - | 14501 | `		/* Reset the string cursor */` |
|       27 | 14502 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14503 | `		pComp = &sURI.sHost;` |
|       27 | 14504 | `		if( pComp->nByte > 0 ){` |
|       25 | 14505 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 14506 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 14507 | `		}` |
|        - | 14508 | `		/* Reset the string cursor */` |
|       27 | 14509 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14510 | `		pComp = &sURI.sPort;` |
|       27 | 14511 | `		if( pComp->nByte > 0 ){` |
|       11 | 14512 | `			int iPort = 0;/* cc warning */` |
|        - | 14513 | `			/* Convert to integer */` |
|       11 | 14514 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 14515 | `			ph7_value_int(pValue,iPort);` |
|       11 | 14516 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 14517 | `		}` |
|        - | 14518 | `		/* Reset the string cursor */` |
|       27 | 14519 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14520 | `		pComp = &sURI.sUser;` |
|       27 | 14521 | `		if( pComp->nByte > 0 ){` |
|        7 | 14522 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 14523 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 14524 | `		}` |
|        - | 14525 | `		/* Reset the string cursor */` |
|       27 | 14526 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14527 | `		pComp = &sURI.sPass;` |
|       27 | 14528 | `		if( pComp->nByte > 0 ){` |
|        7 | 14529 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 14530 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 14531 | `		}` |
|        - | 14532 | `		/* Reset the string cursor */` |
|       27 | 14533 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14534 | `		pComp = &sURI.sPath;` |
|       27 | 14535 | `		if( pComp->nByte > 0 ){` |
|       17 | 14536 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 14537 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 14538 | `		}` |
|        - | 14539 | `		/* Reset the string cursor */` |
|       27 | 14540 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14541 | `		pComp = &sURI.sQuery;` |
|       27 | 14542 | `		if( pComp->nByte > 0 ){` |
|        5 | 14543 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 14544 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 14545 | `		}` |
|        - | 14546 | `		/* Reset the string cursor */` |
|       27 | 14547 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 14548 | `		pComp = &sURI.sFragment;` |
|       27 | 14549 | `		if( pComp->nByte > 0 ){` |
|        5 | 14550 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 14551 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 14552 | `		}` |
|        - | 14553 | `		/* Return the created array */` |
|       27 | 14554 | `		ph7_result_value(pCtx,pArray);` |
|        - | 14555 | `		/* NOTE:` |
|        - | 14556 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 14557 | `		 * automatically as soon we return from this function.` |
|        - | 14558 | `		 */` |
|        - | 14559 | `	}` |
|        - | 14560 | `	/* All done */` |
|       27 | 14561 | `	return PH7_OK;` |
|       15 | 14562 |  |
|        - | 14563 | `/*` |
|        - | 14564 | ` * Section:` |
|        - | 14565 | ` *   Array related routines.` |
|        - | 14566 | ` * Status:` |
|        - | 14567 | ` *    Stable.` |
|        - | 14568 | ` * Note 2012-5-21 01:04:15:` |
|        - | 14569 | ` *  Array related functions that need access to the underlying` |
|        - | 14570 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 14571 | ` */` |
|        - | 14572 | `/*` |
|        - | 14573 | ` * The [compact()] function store it's state information in an instance` |
|        - | 14574 | ` * of the following structure.` |
|        - | 14575 | ` */` |
|        - | 14576 | `struct compact_data` |
|        - | 14577 |  |
|        - | 14578 | `	ph7_value *pArray;  /* Target array */` |
|        - | 14579 | `	int nRecCount;      /* Recursion count */` |
|        - | 14580 | `};` |
|        - | 14581 | `/*` |
|        - | 14582 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 14583 | ` */` |
|      ! 0 | 14584 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 14585 |  |
|      ! 0 | 14586 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 14587 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 14588 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 14589 | `	/* Act according to the hashmap value */` |
|      ! 0 | 14590 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 14591 | `		SyString sVar;` |
|      ! 0 | 14592 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 14593 | `		if( sVar.nByte > 0 ){` |
|        - | 14594 | `			/* Query the current frame */` |
|      ! 0 | 14595 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 14596 | `			/* ^` |
|        - | 14597 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 14598 | `			 */` |
|      ! 0 | 14599 | `			if( pKey ){` |
|        - | 14600 | `				/* Perform the insertion */` |
|      ! 0 | 14601 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 14602 | `			}` |
|      ! 0 | 14603 | `		}` |
|      ! 0 | 14604 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 14605 | `		int rc;` |
|        - | 14606 | `		/* Recursively traverse this array */` |
|      ! 0 | 14607 | `		pData->nRecCount++;` |
|      ! 0 | 14608 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 14609 | `		pData->nRecCount--;` |
|      ! 0 | 14610 | `		return rc;` |
|        - | 14611 | `	}` |
|      ! 0 | 14612 | `	return SXRET_OK;` |
|      ! 0 | 14613 |  |
|        - | 14614 | `/*` |
|        - | 14615 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 14616 | ` *  Create array containing variables and their values.` |
|        - | 14617 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 14618 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 14619 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 14620 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 14621 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 14622 | ` * Parameters` |
|        - | 14623 | ` *  $varname` |
|        - | 14624 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 14625 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 14626 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 14627 | ` *   it recursively.` |
|        - | 14628 | ` * Return` |
|        - | 14629 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 14630 | ` */` |
|        2 | 14631 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14632 |  |
|        - | 14633 | `	ph7_value *pArray,*pObj;` |
|        3 | 14634 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14635 | `	const char *zName;` |
|        - | 14636 | `	SyString sVar;` |
|        - | 14637 | `	int i,nLen;` |
|        3 | 14638 | `	if( nArg < 1 ){` |
|        - | 14639 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 14640 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14641 | `		return PH7_OK;` |
|        - | 14642 | `	}` |
|        - | 14643 | `	/* Create the array */` |
|        3 | 14644 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 14645 | `	if( pArray == 0 ){` |
|        - | 14646 | `		/* Out of memory */` |
|      ! 0 | 14647 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14648 | `		/* Return NULL */` |
|      ! 0 | 14649 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14650 | `		return PH7_OK;` |
|        - | 14651 | `	}` |
|        - | 14652 | `	/* Perform the requested operation */` |
|        7 | 14653 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 14654 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 14655 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 14656 | `				struct compact_data sData;` |
|      ! 0 | 14657 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 14658 | `				/* Recursively walk the array */` |
|      ! 0 | 14659 | `				sData.nRecCount = 0;` |
|      ! 0 | 14660 | `				sData.pArray = pArray;` |
|      ! 0 | 14661 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 14662 | `			}` |
|      ! 0 | 14663 | `		}else{` |
|        - | 14664 | `			/* Extract variable name */` |
|        5 | 14665 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 14666 | `			if( nLen > 0 ){` |
|        5 | 14667 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 14668 | `				/* Check if the variable is available in the current frame */` |
|        5 | 14669 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 14670 | `				if( pObj ){` |
|        5 | 14671 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 14672 | `				}` |
|        2 | 14673 | `			}` |
|        - | 14674 | `		}` |
|        3 | 14675 | `	}` |
|        - | 14676 | `	/* Return the array */` |
|        3 | 14677 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 14678 | `	return PH7_OK;` |
|        2 | 14679 |  |
|        - | 14680 | `/*` |
|        - | 14681 | ` * The [extract()] function store it's state information in an instance` |
|        - | 14682 | ` * of the following structure.` |
|        - | 14683 | ` */` |
|        - | 14684 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 14685 | `struct extract_aux_data` |
|        - | 14686 |  |
|        - | 14687 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 14688 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 14689 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 14690 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 14691 | `	int iFlags;           /* Control flags */` |
|        - | 14692 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 14693 | `};` |
|        - | 14694 | `/* Forward declaration */` |
|        - | 14695 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 14696 | `/*` |
|        - | 14697 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 14698 | ` *   Import variables into the current symbol table from an array.` |
|        - | 14699 | ` * Parameters` |
|        - | 14700 | ` * $var_array` |
|        - | 14701 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 14702 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 14703 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 14704 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 14705 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 14706 | ` * $extract_type` |
|        - | 14707 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 14708 | ` *  It can be one of the following values:` |
|        - | 14709 | ` *   EXTR_OVERWRITE` |
|        - | 14710 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 14711 | ` *   EXTR_SKIP` |
|        - | 14712 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 14713 | ` *   EXTR_PREFIX_SAME` |
|        - | 14714 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 14715 | ` *   EXTR_PREFIX_ALL` |
|        - | 14716 | ` *       Prefix all variable names with prefix.` |
|        - | 14717 | ` *   EXTR_PREFIX_INVALID` |
|        - | 14718 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 14719 | ` *   EXTR_IF_EXISTS` |
|        - | 14720 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 14721 | ` *       otherwise do nothing.` |
|        - | 14722 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 14723 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 14724 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 14725 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 14726 | ` *      the current symbol table.` |
|        - | 14727 | ` * $prefix` |
|        - | 14728 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 14729 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 14730 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 14731 | ` *  underscore character.` |
|        - | 14732 | ` * Return` |
|        - | 14733 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 14734 | ` */` |
|        4 | 14735 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14736 |  |
|        - | 14737 | `	extract_aux_data sAux;` |
|        - | 14738 | `	ph7_hashmap *pMap;` |
|        5 | 14739 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 14740 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 14741 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14742 | `		return PH7_OK;` |
|        - | 14743 | `	}` |
|        - | 14744 | `	/* Point to the target hashmap */` |
|        5 | 14745 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 14746 | `	if( pMap->nEntry < 1 ){` |
|        - | 14747 | `		/* Empty map,return  0 */` |
|      ! 0 | 14748 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14749 | `		return PH7_OK;` |
|        - | 14750 | `	}` |
|        - | 14751 | `	/* Prepare the aux data */` |
|        5 | 14752 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 14753 | `	if( nArg > 1 ){` |
|        3 | 14754 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 14755 | `		if( nArg > 2 ){` |
|      ! 0 | 14756 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 14757 | `		}` |
|        1 | 14758 | `	}` |
|        5 | 14759 | `	sAux.pVm = pCtx->pVm;` |
|        - | 14760 | `	/* Invoke the worker callback */` |
|        5 | 14761 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 14762 | `	/* Number of variables successfully imported */` |
|        5 | 14763 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 14764 | `	return PH7_OK;` |
|        3 | 14765 |  |
|        - | 14766 | `/*` |
|        - | 14767 | ` * Worker callback for the [extract()] function defined` |
|        - | 14768 | ` * below.` |
|        - | 14769 | ` */` |
|        8 | 14770 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14771 |  |
|        9 | 14772 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 14773 | `	int iFlags = pAux->iFlags;` |
|        9 | 14774 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14775 | `	ph7_value *pObj;` |
|        - | 14776 | `	SyString sVar;` |
|        9 | 14777 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 14778 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 14779 | `	}` |
|        - | 14780 | `	/* Perform a string cast */` |
|        9 | 14781 | `	PH7_MemObjToString(pKey);` |
|        9 | 14782 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14783 | `		/* Unavailable variable name */` |
|      ! 0 | 14784 | `		return SXRET_OK;` |
|        - | 14785 | `	}` |
|        9 | 14786 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 14787 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 14788 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14789 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14790 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14791 | `			);` |
|      ! 0 | 14792 | `	}else{` |
|       13 | 14793 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 14794 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14795 | `	}` |
|        9 | 14796 | `	sVar.zString = pAux->zWorker;` |
|        - | 14797 | `	/* Try to extract the variable */` |
|        9 | 14798 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 14799 | `	if( pObj ){` |
|        - | 14800 | `		/* Collision */` |
|        5 | 14801 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 14802 | `			return SXRET_OK;` |
|        - | 14803 | `		}` |
|        5 | 14804 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 14805 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 14806 | `				/* Already prefixed */` |
|      ! 0 | 14807 | `				return SXRET_OK;` |
|        - | 14808 | `			}` |
|      ! 0 | 14809 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14810 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14811 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14812 | `				);` |
|      ! 0 | 14813 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 14814 | `		}` |
|        3 | 14815 | `	}else{` |
|        - | 14816 | `		/* Create the variable */` |
|        5 | 14817 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 14818 | `	}` |
|        9 | 14819 | `	if( pObj ){` |
|        - | 14820 | `		/* Overwrite the old value */` |
|        9 | 14821 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 14822 | `		/* Increment counter */` |
|        9 | 14823 | `		pAux->iCount++;` |
|        4 | 14824 | `	}` |
|        9 | 14825 | `	return SXRET_OK;` |
|        5 | 14826 |  |
|        - | 14827 | `/*` |
|        - | 14828 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 14829 | ` * defined below.` |
|        - | 14830 | ` */` |
|        2 | 14831 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14832 |  |
|        3 | 14833 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 14834 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14835 | `	ph7_value *pObj;` |
|        - | 14836 | `	SyString sVar;` |
|        - | 14837 | `	/* Perform a string cast */` |
|        3 | 14838 | `	PH7_MemObjToString(pKey);` |
|        3 | 14839 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14840 | `		/* Unavailable variable name */` |
|      ! 0 | 14841 | `		return SXRET_OK;` |
|        - | 14842 | `	}` |
|        3 | 14843 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 14844 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 14845 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 14846 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 14847 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14848 | `			);` |
|        2 | 14849 | `	}else{` |
|      ! 0 | 14850 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 14851 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14852 | `	}` |
|        3 | 14853 | `	sVar.zString = pAux->zWorker;` |
|        - | 14854 | `	/* Extract the variable */` |
|        3 | 14855 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 14856 | `	if( pObj ){` |
|        3 | 14857 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 14858 | `	}` |
|        3 | 14859 | `	return SXRET_OK;` |
|        2 | 14860 |  |
|        - | 14861 | `/*` |
|        - | 14862 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 14863 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 14864 | ` * Parameters` |
|        - | 14865 | ` * $types` |
|        - | 14866 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 14867 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 14868 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 14869 | ` *  POST includes the POST uploaded file information.` |
|        - | 14870 | ` *  Note:` |
|        - | 14871 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 14872 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 14873 | ` * $prefix` |
|        - | 14874 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 14875 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 14876 | ` *  variable named $pref_userid.` |
|        - | 14877 | ` * Return` |
|        - | 14878 | ` *  TRUE on success or FALSE on failure.` |
|        - | 14879 | ` */` |
|        2 | 14880 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14881 |  |
|        - | 14882 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 14883 | `	extract_aux_data sAux;` |
|        - | 14884 | `	int nLen,nPrefixLen;` |
|        - | 14885 | `	ph7_value *pSuper;` |
|        - | 14886 | `	ph7_vm *pVm;` |
|        - | 14887 | `	/* By default import only $_GET variables  */` |
|        3 | 14888 | `	zImport = "G";` |
|        3 | 14889 | `	nLen = (int)sizeof(char);` |
|        3 | 14890 | `	zPrefix = 0;` |
|        3 | 14891 | `	nPrefixLen = 0;` |
|        3 | 14892 | `	if( nArg > 0 ){` |
|        3 | 14893 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 14894 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 14895 | `		}` |
|        3 | 14896 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 14897 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 14898 | `		}` |
|        1 | 14899 | `	}` |
|        - | 14900 | `	/* Point to the underlying VM */` |
|        3 | 14901 | `	pVm = pCtx->pVm;` |
|        - | 14902 | `	/* Initialize the aux data */` |
|        3 | 14903 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 14904 | `	sAux.zPrefix = zPrefix;` |
|        3 | 14905 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 14906 | `	sAux.pVm = pVm;` |
|        - | 14907 | `	/* Extract */` |
|        3 | 14908 | `	zEnd = &zImport[nLen];` |
|        5 | 14909 | `	while( zImport < zEnd ){` |
|        3 | 14910 | `		int c = zImport[0];` |
|        3 | 14911 | `		pSuper = 0;` |
|        3 | 14912 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 14913 | `			/* Import $_GET variables */` |
|        3 | 14914 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 14915 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 14916 | `			/* Import $_POST variables */` |
|      ! 0 | 14917 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 14918 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 14919 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 14920 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 14921 | `		}` |
|        3 | 14922 | `		if( pSuper ){` |
|        - | 14923 | `			/* Iterate throw array entries */` |
|        3 | 14924 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 14925 | `		}` |
|        - | 14926 | `		/* Advance the cursor */` |
|        3 | 14927 | `		zImport++;` |
|        1 | 14928 | `	}` |
|        - | 14929 | `	/* All done,return TRUE*/` |
|        3 | 14930 | `	ph7_result_bool(pCtx,0);` |
|        3 | 14931 | `	return PH7_OK;` |
|        1 | 14932 |  |
|        - | 14933 | `/*` |
|        - | 14934 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 14935 | ` * Refer to the eval() language construct implementation for more` |
|        - | 14936 | ` * information.` |
|        - | 14937 | ` */` |
|    12730 | 14938 | `static sxi32 VmEvalChunk(` |
|        - | 14939 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 14940 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 14941 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 14942 | `	int iFlags,         /* Compile flag */` |
|        - | 14943 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 14944 | `	)` |
|        2 | 14945 |  |
|        - | 14946 | `	SySet *pByteCode,aByteCode;` |
|        - | 14947 | `	SyBlob sSavedNs;` |
|    12732 | 14948 | `	ProcConsumer xErr = 0;` |
|    12732 | 14949 | `	void *pErrData = 0;` |
|        - | 14950 | `	/* Initialize bytecode container */` |
|    12732 | 14951 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    12732 | 14952 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 14953 | `	/* Reset the code generator */` |
|    12732 | 14954 | `	if( bTrueReturn ){` |
|        - | 14955 | `		/* Included file,log compile-time errors */` |
|     9570 | 14956 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9570 | 14957 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4784 | 14958 | `	}` |
|    12732 | 14959 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 14960 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 14961 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 14962 | `	 * the caller's namespace is restored. */` |
|    12732 | 14963 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    12732 | 14964 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    12732 | 14965 | `	if( bTrueReturn ){` |
|        - | 14966 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9570 | 14967 | `		SyBlobReset(&pVm->sNamespace);` |
|     4784 | 14968 | `	}` |
|        - | 14969 | `	/* Swap bytecode container */` |
|    12732 | 14970 | `	pByteCode = pVm->pByteContainer;` |
|    12732 | 14971 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 14972 | `	/* Compile the chunk */` |
|    12732 | 14973 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    19097 | 14974 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 14975 | `		/* Compilation error,return false */` |
|        3 | 14976 | `		if( pCtx ){` |
|        3 | 14977 | `			ph7_result_bool(pCtx,0);` |
|        1 | 14978 | `		}` |
|        2 | 14979 | `	}else{` |
|        - | 14980 | `		/* Mount any newly defined classes */` |
|        - | 14981 | `		SyHashEntry *pEntry;` |
|        - | 14982 | `		ph7_class *pClass;` |
|        - | 14983 | `		ph7_value sResult; /* Return value */` |
|        - | 14984 | `		sxi32 rc;` |
|    12730 | 14985 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   967816 | 14986 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   948724 | 14987 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 14988 | `			/* Only mount classes that haven't been mounted yet */` |
|   948724 | 14989 | `			if( !pClass->bMounted ){` |
|   245982 | 14990 | `				rc = VmMountUserClass(pVm,pClass);` |
|   245982 | 14991 | `				if( rc != SXRET_OK ){` |
|        - | 14992 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 14993 | `					if( pCtx ){` |
|      ! 0 | 14994 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 14995 | `					}` |
|      ! 0 | 14996 | `					goto Cleanup;` |
|        - | 14997 | `				}` |
|   122990 | 14998 | `			}` |
|        2 | 14999 | `		}` |
|    12730 | 15000 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 15001 | `			/* Out of memory */` |
|      ! 0 | 15002 | `			if( pCtx ){` |
|      ! 0 | 15003 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 15004 | `			}` |
|      ! 0 | 15005 | `			goto Cleanup;` |
|        - | 15006 | `		}` |
|    12730 | 15007 | `		if( bTrueReturn ){` |
|        - | 15008 | `			/* Assume a boolean true return value */` |
|     9570 | 15009 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4786 | 15010 | `		}else{` |
|        - | 15011 | `			/* Assume a null return value */` |
|     3162 | 15012 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 15013 | `		}` |
|        - | 15014 | `		/* Execute the compiled chunk */` |
|    12730 | 15015 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    12730 | 15016 | `		if( pCtx ){` |
|        - | 15017 | `			/* Set the execution result */` |
|     9590 | 15018 | `			ph7_result_value(pCtx,&sResult);` |
|     4794 | 15019 | `		}` |
|    12730 | 15020 | `		PH7_MemObjRelease(&sResult);` |
|        - | 15021 | `	}` |
|     6365 | 15022 | `Cleanup:` |
|        - | 15023 | `	/* Cleanup the mess left behind */` |
|    12732 | 15024 | `	pVm->pByteContainer = pByteCode;` |
|    12732 | 15025 | `	SySetRelease(&aByteCode);` |
|        - | 15026 | `	/* Restore caller's namespace state */` |
|    12732 | 15027 | `	SyBlobReset(&pVm->sNamespace);` |
|    12732 | 15028 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    12732 | 15029 | `	SyBlobRelease(&sSavedNs);` |
|    12732 | 15030 | `	return SXRET_OK;` |
|        2 | 15031 |  |
|        - | 15032 | `/*` |
|        - | 15033 | ` * value eval(string $code)` |
|        - | 15034 | ` *   Evaluate a string as PHP code.` |
|        - | 15035 | ` * Parameter` |
|        - | 15036 | ` *  code: PHP code to evaluate.` |
|        - | 15037 | ` * Return` |
|        - | 15038 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 15039 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 15040 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 15041 | ` */` |
|       24 | 15042 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15043 |  |
|        - | 15044 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       26 | 15045 | `	if( nArg < 1 ){` |
|        - | 15046 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15047 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15048 | `		return SXRET_OK;` |
|        - | 15049 | `	}` |
|        - | 15050 | `	/* Chunk to evaluate */` |
|       26 | 15051 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       26 | 15052 | `	if( sChunk.nByte < 1 ){` |
|        - | 15053 | `		/* Empty string,return NULL */` |
|        3 | 15054 | `		ph7_result_null(pCtx);` |
|        3 | 15055 | `		return SXRET_OK;` |
|        - | 15056 | `	}` |
|        - | 15057 | `	/* Eval the chunk */` |
|       24 | 15058 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       24 | 15059 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15060 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|        3 | 15061 | `		return PH7_ABORT;` |
|        - | 15062 | `	}` |
|       22 | 15063 | `	return SXRET_OK;` |
|       14 | 15064 |  |
|        - | 15065 | `/*` |
|        - | 15066 | ` * Check if a file path is already included.` |
|        - | 15067 | ` */` |
|    19134 | 15068 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 15069 |  |
|        - | 15070 | `	SyString *aEntries;` |
|        - | 15071 | `	sxu32 n;` |
|    19136 | 15072 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 15073 | `	/* Perform a linear search */` |
| 91337154 | 15074 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 91318030 | 15075 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 15076 | `			/* Already included */` |
|       11 | 15077 | `			return TRUE;` |
|        - | 15078 | `		}` |
| 45659011 | 15079 | `	}` |
|    19126 | 15080 | `	return FALSE;` |
|     9569 | 15081 |  |
|        - | 15082 | `/*` |
|        - | 15083 | ` * Push a file path in the appropriate VM container.` |
|        - | 15084 | ` */` |
|    22266 | 15085 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 15086 |  |
|        - | 15087 | `	SyString sPath;` |
|        - | 15088 | `	char *zDup;` |
|        - | 15089 | `#ifdef __WINNT__` |
|        - | 15090 | `	char *zCur;` |
|        - | 15091 | `#endif` |
|        - | 15092 | `	sxi32 rc;` |
|    22268 | 15093 | `	if( nLen < 0 ){` |
|     3134 | 15094 | `		nLen = SyStrlen(zPath);` |
|     1566 | 15095 | `	}` |
|        - | 15096 | `	/* Duplicate the file path first */` |
|    22268 | 15097 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    22268 | 15098 | `	if( zDup == 0 ){` |
|      ! 0 | 15099 | `		return SXERR_MEM;` |
|        - | 15100 | `	}` |
|        - | 15101 | `#ifdef __WINNT__` |
|        - | 15102 | `	/* Normalize path on windows` |
|        - | 15103 | `	 * Example:` |
|        - | 15104 | `	 *    Path/To/File.php` |
|        - | 15105 | `	 * becomes` |
|        - | 15106 | `	 *   path\to\file.php` |
|        - | 15107 | `	 */` |
|        2 | 15108 | `	zCur = zDup;` |
|        2 | 15109 | `	while( zCur[0] != 0 ){` |
|        2 | 15110 | `		if( zCur[0] == '/' ){` |
|        2 | 15111 | `			zCur[0] = '\\';` |
|        2 | 15112 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 15113 | `			int c = SyToLower(zCur[0]);` |
|        1 | 15114 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 15115 | `		}` |
|        2 | 15116 | `		zCur++;` |
|        2 | 15117 | `	}` |
|        - | 15118 | `#endif` |
|        - | 15119 | `	/* Install the file path */` |
|    22268 | 15120 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    22268 | 15121 | `	if( !bMain ){` |
|    19136 | 15122 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 15123 | `			/* Already included */` |
|       11 | 15124 | `			*pNew = 0;` |
|        6 | 15125 | `		}else{` |
|        - | 15126 | `			/* Insert in the corresponding container */` |
|    19126 | 15127 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    19126 | 15128 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 15129 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 15130 | `				return rc;` |
|        - | 15131 | `			}` |
|    19126 | 15132 | `			*pNew = 1;` |
|        - | 15133 | `		}` |
|     9567 | 15134 | `	}` |
|    22268 | 15135 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    22268 | 15136 | `	return SXRET_OK;` |
|    11135 | 15137 |  |
|        - | 15138 | `/*` |
|        - | 15139 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 15140 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 15141 | ` * indicates failure.` |
|        - | 15142 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 15143 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 15144 | ` * operations.` |
|        - | 15145 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 15146 | ` * this function is a no-op.` |
|        - | 15147 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 15148 | ` * constructs for more information.` |
|        - | 15149 | ` */` |
|     9582 | 15150 | `static sxi32 VmExecIncludedFile(` |
|        - | 15151 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 15152 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 15153 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 15154 | `	 )` |
|        2 | 15155 |  |
|        - | 15156 | `	sxi32 rc;` |
|        - | 15157 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15158 | `	const ph7_io_stream *pStream;` |
|        - | 15159 | `	SyBlob sContents;` |
|        - | 15160 | `	void *pHandle;` |
|        - | 15161 | `	ph7_vm *pVm;` |
|        - | 15162 | `	int isNew;` |
|        - | 15163 | `	/* Initialize fields */` |
|     9584 | 15164 | `	pVm = pCtx->pVm;` |
|     9584 | 15165 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9584 | 15166 | `	isNew = 0;` |
|        - | 15167 | `	/* Extract the associated stream */` |
|     9584 | 15168 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 15169 | `	/*` |
|        - | 15170 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 15171 | `	 * in a read-only mode.` |
|        - | 15172 | `	 */` |
|     9584 | 15173 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9584 | 15174 | `	if( pHandle == 0 ){` |
|        8 | 15175 | `		return SXERR_IO;` |
|        - | 15176 | `	}` |
|     9578 | 15177 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9578 | 15178 | `	if( IncludeOnce && !isNew ){` |
|        - | 15179 | `		/* Already included */` |
|        9 | 15180 | `		rc = SXERR_EXISTS;` |
|        5 | 15181 | `	}else{` |
|        - | 15182 | `		/* Read the whole file contents */` |
|     9570 | 15183 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9570 | 15184 | `		if( rc == SXRET_OK ){` |
|        - | 15185 | `			SyString sScript;` |
|        - | 15186 | `			/* Compile and execute the script */` |
|     9570 | 15187 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9570 | 15188 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4784 | 15189 | `		}` |
|        - | 15190 | `	}` |
|        - | 15191 | `	/* Pop from the set of included file */` |
|     9578 | 15192 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 15193 | `	/* Close the handle */` |
|     9578 | 15194 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 15195 | `	/* Release the working buffer */` |
|     9578 | 15196 | `	SyBlobRelease(&sContents);` |
|        - | 15197 | `#else` |
|        - | 15198 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 15199 | `	SXUNUSED(pPath);` |
|        - | 15200 | `	SXUNUSED(IncludeOnce);` |
|        - | 15201 | `	rc = SXERR_IO;` |
|        - | 15202 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9578 | 15203 | `	return rc;` |
|     4793 | 15204 |  |
|        - | 15205 | `/*` |
|        - | 15206 | ` * string get_include_path(void)` |
|        - | 15207 | ` *  Gets the current include_path configuration option.` |
|        - | 15208 | ` * Parameter` |
|        - | 15209 | ` *  None` |
|        - | 15210 | ` * Return` |
|        - | 15211 | ` *  Included paths as a string` |
|        - | 15212 | ` */` |
|        2 | 15213 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15214 |  |
|        3 | 15215 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15216 | `	SyString *aEntry;` |
|        - | 15217 | `	int dir_sep;` |
|        - | 15218 | `	sxu32 n;` |
|        - | 15219 | `#ifdef __WINNT__` |
|        1 | 15220 | `	dir_sep = ';';` |
|        - | 15221 | `#else` |
|        - | 15222 | `	/* Assume UNIX path separator */` |
|        2 | 15223 | `	dir_sep = ':';` |
|        - | 15224 | `#endif` |
|        1 | 15225 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 15226 | `	SXUNUSED(apArg);` |
|        - | 15227 | `	/* Point to the list of import paths */` |
|        3 | 15228 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 15229 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 15230 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 15231 | `		if( n > 0 ){` |
|        - | 15232 | `			/* Append dir seprator */` |
|      ! 0 | 15233 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 15234 | `		}` |
|        - | 15235 | `		/* Append path */` |
|        3 | 15236 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 15237 | `	}` |
|        3 | 15238 | `	return PH7_OK;` |
|        1 | 15239 |  |
|        - | 15240 | `/*` |
|        - | 15241 | ` * string get_get_included_files(void)` |
|        - | 15242 | ` *  Gets the current include_path configuration option.` |
|        - | 15243 | ` * Parameter` |
|        - | 15244 | ` *  None` |
|        - | 15245 | ` * Return` |
|        - | 15246 | ` *  Included paths as a string` |
|        - | 15247 | ` */` |
|        2 | 15248 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15249 |  |
|        3 | 15250 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 15251 | `	ph7_value *pArray,*pWorker;` |
|        - | 15252 | `	SyString *pEntry;` |
|        - | 15253 | `	int c,d;` |
|        - | 15254 | `	/* Create an array and a working value */` |
|        3 | 15255 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 15256 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 15257 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 15258 | `		/* Out of memory,return null */` |
|      ! 0 | 15259 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15260 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 15261 | `		SXUNUSED(apArg);` |
|      ! 0 | 15262 | `		return PH7_OK;` |
|        - | 15263 | `	}` |
|        3 | 15264 | `	c = d = '/';` |
|        - | 15265 | `#ifdef __WINNT__` |
|        1 | 15266 | `	d = '\\';` |
|        - | 15267 | `#endif` |
|        - | 15268 | `	/* Iterate throw entries */` |
|        3 | 15269 | `	SySetResetCursor(pFiles);` |
|     3917 | 15270 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 15271 | `		const char *zBase,*zEnd;` |
|        - | 15272 | `		int iLen;` |
|        - | 15273 | `		/* reset the string cursor */` |
|     3915 | 15274 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 15275 | `		/* Extract base name */` |
|     3915 | 15276 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 15277 | `		/* Ignore trailing '/' */` |
|     5872 | 15278 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 15279 | `			zEnd--;` |
|      ! 0 | 15280 | `		}` |
|     3915 | 15281 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   120668 | 15282 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   114797 | 15283 | `			zEnd--;` |
|        1 | 15284 | `		}` |
|     3915 | 15285 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3915 | 15286 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 15287 | `		/* Copy entry name */` |
|     3915 | 15288 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 15289 | `		/* Perform the insertion */` |
|     3915 | 15290 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 15291 | `	}` |
|        - | 15292 | `	/* All done,return the created array */` |
|        3 | 15293 | `	ph7_result_value(pCtx,pArray);` |
|        - | 15294 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 15295 | `	 * by the engine as soon we return from this foreign` |
|        - | 15296 | `	 * function.` |
|        - | 15297 | `	 */` |
|        3 | 15298 | `	return PH7_OK;` |
|        2 | 15299 |  |
|        - | 15300 | `/*` |
|        - | 15301 | ` * include:` |
|        - | 15302 | ` * According to the PHP reference manual.` |
|        - | 15303 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 15304 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 15305 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 15306 | ` *  include() will finally check in the calling script's own directory` |
|        - | 15307 | ` *  and the current working directory before failing. The include()` |
|        - | 15308 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 15309 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 15310 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 15311 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 15312 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 15313 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 15314 | ` *  directory to find the requested file.` |
|        - | 15315 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 15316 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 15317 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 15318 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 15319 | ` */` |
|     9558 | 15320 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15321 |  |
|        - | 15322 | `	SyString sFile;` |
|        - | 15323 | `	sxi32 rc;` |
|     9560 | 15324 | `	if( nArg < 1 ){` |
|        - | 15325 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15326 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15327 | `		return SXRET_OK;` |
|        - | 15328 | `	}` |
|        - | 15329 | `	/* File to include */` |
|     9560 | 15330 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9560 | 15331 | `	if( sFile.nByte < 1 ){` |
|        - | 15332 | `		/* Empty string,return NULL */` |
|      ! 0 | 15333 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15334 | `		return SXRET_OK;` |
|        - | 15335 | `	}` |
|        - | 15336 | `	/* Open,compile and execute the desired script */` |
|     9560 | 15337 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9560 | 15338 | `	if( rc != SXRET_OK ){` |
|        - | 15339 | `		/* Emit a warning and return false */` |
|        3 | 15340 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 15341 | `		ph7_result_bool(pCtx,0);` |
|        1 | 15342 | `	}` |
|     9560 | 15343 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15344 | `		/* exit/die inside the included file: cascade the halt */` |
|        5 | 15345 | `		return PH7_ABORT;` |
|        - | 15346 | `	}` |
|     9556 | 15347 | `	return SXRET_OK;` |
|     4781 | 15348 |  |
|        - | 15349 | `/*` |
|        - | 15350 | ` * include_once:` |
|        - | 15351 | ` *  According to the PHP reference manual.` |
|        - | 15352 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 15353 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 15354 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 15355 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 15356 | ` *   just once.` |
|        - | 15357 | ` */` |
|       10 | 15358 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15359 |  |
|        - | 15360 | `	SyString sFile;` |
|        - | 15361 | `	sxi32 rc;` |
|       11 | 15362 | `	if( nArg < 1 ){` |
|        - | 15363 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15364 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15365 | `		return SXRET_OK;` |
|        - | 15366 | `	}` |
|        - | 15367 | `	/* File to include */` |
|       11 | 15368 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|       11 | 15369 | `	if( sFile.nByte < 1 ){` |
|        - | 15370 | `		/* Empty string,return NULL */` |
|      ! 0 | 15371 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15372 | `		return SXRET_OK;` |
|        - | 15373 | `	}` |
|        - | 15374 | `	/* Open,compile and execute the desired script */` |
|       11 | 15375 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|       11 | 15376 | `	if( rc == SXERR_EXISTS ){` |
|        - | 15377 | `		/* File already included,return TRUE */` |
|        7 | 15378 | `		ph7_result_bool(pCtx,1);` |
|        7 | 15379 | `		return SXRET_OK;` |
|        - | 15380 | `	}` |
|        5 | 15381 | `	if( rc != SXRET_OK ){` |
|        - | 15382 | `		/* Emit a warning and return false */` |
|      ! 0 | 15383 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15384 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15385 | ` 	}` |
|        5 | 15386 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15387 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15388 | `		return PH7_ABORT;` |
|        - | 15389 | `	}` |
|        5 | 15390 | `	return SXRET_OK;` |
|        6 | 15391 |  |
|        - | 15392 | `/*` |
|        - | 15393 | ` * require.` |
|        - | 15394 | ` *  According to the PHP reference manual.` |
|        - | 15395 | ` *   require() is identical to include() except upon failure it will` |
|        - | 15396 | ` *   also produce a fatal level error.` |
|        - | 15397 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 15398 | ` *   emits a warning  which allows the script to continue.` |
|        - | 15399 | ` */` |
|        6 | 15400 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15401 |  |
|        - | 15402 | `	SyString sFile;` |
|        - | 15403 | `	sxi32 rc;` |
|        8 | 15404 | `	if( nArg < 1 ){` |
|        - | 15405 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15406 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15407 | `		return SXRET_OK;` |
|        - | 15408 | `	}` |
|        - | 15409 | `	/* File to include */` |
|        8 | 15410 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 15411 | `	if( sFile.nByte < 1 ){` |
|        - | 15412 | `		/* Empty string,return NULL */` |
|      ! 0 | 15413 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15414 | `		return SXRET_OK;` |
|        - | 15415 | `	}` |
|        - | 15416 | `	/* Open,compile and execute the desired script */` |
|        8 | 15417 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 15418 | `	if( rc != SXRET_OK ){` |
|        - | 15419 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 15420 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15421 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15422 | `		return PH7_ABORT;` |
|        - | 15423 | `	}` |
|        8 | 15424 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15425 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15426 | `		return PH7_ABORT;` |
|        - | 15427 | `	}` |
|        8 | 15428 | `	return SXRET_OK;` |
|        5 | 15429 |  |
|        - | 15430 | `/*` |
|        - | 15431 | ` * require_once:` |
|        - | 15432 | ` *  According to the PHP reference manual.` |
|        - | 15433 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 15434 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 15435 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 15436 | ` *   and how it differs from its non _once siblings.` |
|        - | 15437 | ` */` |
|        4 | 15438 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15439 |  |
|        - | 15440 | `	SyString sFile;` |
|        - | 15441 | `	sxi32 rc;` |
|        5 | 15442 | `	if( nArg < 1 ){` |
|        - | 15443 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 15444 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15445 | `		return SXRET_OK;` |
|        - | 15446 | `	}` |
|        - | 15447 | `	/* File to include */` |
|        5 | 15448 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 15449 | `	if( sFile.nByte < 1 ){` |
|        - | 15450 | `		/* Empty string,return NULL */` |
|      ! 0 | 15451 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15452 | `		return SXRET_OK;` |
|        - | 15453 | `	}` |
|        - | 15454 | `	/* Open,compile and execute the desired script */` |
|        5 | 15455 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 15456 | `	if( rc == SXERR_EXISTS ){` |
|        - | 15457 | `		/* File already included,return TRUE */` |
|        3 | 15458 | `		ph7_result_bool(pCtx,1);` |
|        3 | 15459 | `		return SXRET_OK;` |
|        - | 15460 | `	}` |
|        3 | 15461 | `	if( rc != SXRET_OK ){` |
|        - | 15462 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 15463 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 15464 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15465 | `		return PH7_ABORT;` |
|        - | 15466 | `	}` |
|        3 | 15467 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 15468 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 15469 | `		return PH7_ABORT;` |
|        - | 15470 | `	}` |
|        3 | 15471 | `	return SXRET_OK;` |
|        3 | 15472 |  |
|        - | 15473 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 15474 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 15475 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 15476 | `/*` |
|        - | 15477 | ` * Section:` |
|        - | 15478 | ` *  SPL Autoloading functions.` |
|        - | 15479 | ` * Status:` |
|        - | 15480 | ` *  Stable.` |
|        - | 15481 | ` */` |
|        - | 15482 | `/*` |
|        - | 15483 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 15484 | ` *  Register given function as __autoload() implementation.` |
|        - | 15485 | ` * Parameters` |
|        - | 15486 | ` *  callback` |
|        - | 15487 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 15488 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 15489 | ` *  throw` |
|        - | 15490 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 15491 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 15492 | ` *  prepend` |
|        - | 15493 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 15494 | ` *   autoload stack instead of appending it.` |
|        - | 15495 | ` * Return` |
|        - | 15496 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15497 | ` */` |
|       34 | 15498 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15499 |  |
|        - | 15500 | `	VmAutoloadCB sEntry;` |
|       36 | 15501 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 15502 | `	int iPrepend = 0;` |
|        - | 15503 | `	sxu32 n;` |
|       36 | 15504 | `	if( nArg < 1 ){` |
|        - | 15505 | `		/* No callback provided — register default spl_autoload.` |
|        - | 15506 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 15507 | `		/* Check for duplicates first */` |
|        9 | 15508 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 15509 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 15510 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 15511 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 15512 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 15513 | `				ph7_result_bool(pCtx,1);` |
|        5 | 15514 | `				return SXRET_OK;` |
|        - | 15515 | `			}` |
|      ! 0 | 15516 | `		}` |
|        5 | 15517 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 15518 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 15519 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 15520 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 15521 | `		ph7_result_bool(pCtx,1);` |
|        5 | 15522 | `		return SXRET_OK;` |
|        - | 15523 | `	}` |
|        - | 15524 | `	/* Validate that the callback is callable */` |
|       28 | 15525 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 15526 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 15527 | `		if( nArg >= 2 ){` |
|      ! 0 | 15528 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 15529 | `		}` |
|      ! 0 | 15530 | `		if( iThrow ){` |
|      ! 0 | 15531 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 15532 | `				"Argument is not callable");` |
|      ! 0 | 15533 | `		}` |
|      ! 0 | 15534 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15535 | `		return SXRET_OK;` |
|        - | 15536 | `	}` |
|        - | 15537 | `	/* Check for duplicates */` |
|       46 | 15538 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 15539 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 15540 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15541 | `			/* Already registered */` |
|      ! 0 | 15542 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 15543 | `			return SXRET_OK;` |
|        - | 15544 | `		}` |
|       11 | 15545 | `	}` |
|        - | 15546 | `	/* Check prepend flag */` |
|       28 | 15547 | `	if( nArg >= 3 ){` |
|        3 | 15548 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 15549 | `	}` |
|        - | 15550 | `	/* Store the callback */` |
|       28 | 15551 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 15552 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 15553 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 15554 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 15555 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 15556 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 15557 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 15558 | `		VmAutoloadCB *aBase;` |
|        3 | 15559 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 15560 | `		/* Rotate: move last entry to front */` |
|        3 | 15561 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 15562 | `		if( aBase ){` |
|        - | 15563 | `			VmAutoloadCB sTemp;` |
|        - | 15564 | `			sxu32 i;` |
|        3 | 15565 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 15566 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 15567 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 15568 | `			}` |
|        3 | 15569 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 15570 | `		}` |
|        2 | 15571 | `	}else{` |
|       26 | 15572 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 15573 | `	}` |
|       28 | 15574 | `	ph7_result_bool(pCtx,1);` |
|       28 | 15575 | `	return SXRET_OK;` |
|       19 | 15576 |  |
|        - | 15577 | `/*` |
|        - | 15578 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 15579 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 15580 | ` * Parameters` |
|        - | 15581 | ` *  callback` |
|        - | 15582 | ` *   The autoload function being unregistered.` |
|        - | 15583 | ` * Return` |
|        - | 15584 | ` *  TRUE on success, FALSE on failure.` |
|        - | 15585 | ` */` |
|       32 | 15586 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 15587 |  |
|       34 | 15588 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15589 | `	sxu32 n,nEntry;` |
|       34 | 15590 | `	if( nArg < 1 ){` |
|      ! 0 | 15591 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 15592 | `		return SXRET_OK;` |
|        - | 15593 | `	}` |
|       34 | 15594 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 15595 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 15596 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 15597 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15598 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 15599 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 15600 | `			sxu32 i;` |
|       32 | 15601 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 15602 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 15603 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 15604 | `			}` |
|        - | 15605 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 15606 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 15607 | `			ph7_result_bool(pCtx,1);` |
|       32 | 15608 | `			return SXRET_OK;` |
|        - | 15609 | `		}` |
|        3 | 15610 | `	}` |
|        3 | 15611 | `	ph7_result_bool(pCtx,0);` |
|        3 | 15612 | `	return SXRET_OK;` |
|       18 | 15613 |  |
|        - | 15614 | `/*` |
|        - | 15615 | ` * array spl_autoload_functions(void)` |
|        - | 15616 | ` *  Return all registered __autoload() functions.` |
|        - | 15617 | ` * Return` |
|        - | 15618 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 15619 | ` *  an empty array is returned.` |
|        - | 15620 | ` */` |
|       20 | 15621 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15622 |  |
|       21 | 15623 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15624 | `	ph7_value *pArray;` |
|        - | 15625 | `	sxu32 n,nEntry;` |
|       10 | 15626 | `	SXUNUSED(nArg);` |
|       10 | 15627 | `	SXUNUSED(apArg);` |
|       21 | 15628 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 15629 | `	if( pArray == 0 ){` |
|      ! 0 | 15630 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15631 | `		return SXRET_OK;` |
|        - | 15632 | `	}` |
|       21 | 15633 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 15634 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 15635 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 15636 | `		if( pEntry ){` |
|       15 | 15637 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 15638 | `		}` |
|        8 | 15639 | `	}` |
|       21 | 15640 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 15641 | `	return SXRET_OK;` |
|       11 | 15642 |  |
|        - | 15643 | `/*` |
|        - | 15644 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 15645 | ` *  Default implementation of __autoload().` |
|        - | 15646 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 15647 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 15648 | ` * Parameters` |
|        - | 15649 | ` *  class` |
|        - | 15650 | ` *   The class name being searched.` |
|        - | 15651 | ` *  file_extensions` |
|        - | 15652 | ` *   Comma-separated list of file extensions to try.` |
|        - | 15653 | ` */` |
|        2 | 15654 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15655 |  |
|        - | 15656 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 15657 | `	SyBlob sPath;` |
|        - | 15658 | `	int nClass;` |
|        - | 15659 | `	sxi32 rc;` |
|        3 | 15660 | `	if( nArg < 1 ){` |
|      ! 0 | 15661 | `		return SXRET_OK;` |
|        - | 15662 | `	}` |
|        3 | 15663 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 15664 | `	if( nClass < 1 ){` |
|      ! 0 | 15665 | `		return SXRET_OK;` |
|        - | 15666 | `	}` |
|        - | 15667 | `	/* Default extensions */` |
|        3 | 15668 | `	zExt = ".php,.inc";` |
|        3 | 15669 | `	if( nArg >= 2 ){` |
|        - | 15670 | `		int nExt;` |
|      ! 0 | 15671 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 15672 | `		if( nExt < 1 ){` |
|      ! 0 | 15673 | `			zExt = ".php,.inc";` |
|      ! 0 | 15674 | `		}` |
|      ! 0 | 15675 | `	}` |
|        3 | 15676 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 15677 | `	/* Iterate over comma-separated extensions */` |
|        3 | 15678 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 15679 | `	zCur = zExt;` |
|        7 | 15680 | `	while( zCur < zEnd ){` |
|        - | 15681 | `		const char *zComma;` |
|        - | 15682 | `		SyString sFile;` |
|        - | 15683 | `		int i;` |
|        - | 15684 | `		/* Find next comma or end */` |
|        5 | 15685 | `		zComma = zCur;` |
|       21 | 15686 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 15687 | `			zComma++;` |
|        1 | 15688 | `		}` |
|        - | 15689 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 15690 | `		SyBlobReset(&sPath);` |
|       69 | 15691 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 15692 | `			char c = zClass[i];` |
|       65 | 15693 | `			if( c == '\\' ){` |
|      ! 0 | 15694 | `				c = '/';` |
|       65 | 15695 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 15696 | `				c = c + ('a' - 'A');` |
|        6 | 15697 | `			}` |
|       65 | 15698 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 15699 | `		}` |
|        - | 15700 | `		/* Append extension */` |
|        5 | 15701 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 15702 | `		/* Try to include the file */` |
|        5 | 15703 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 15704 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 15705 | `		if( rc == SXRET_OK ){` |
|        - | 15706 | `			/* File included successfully */` |
|      ! 0 | 15707 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 15708 | `			return SXRET_OK;` |
|        - | 15709 | `		}` |
|        - | 15710 | `		/* Move past the comma */` |
|        5 | 15711 | `		zCur = zComma;` |
|        5 | 15712 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 15713 | `			zCur++;` |
|        1 | 15714 | `		}` |
|        1 | 15715 | `	}` |
|        3 | 15716 | `	SyBlobRelease(&sPath);` |
|        3 | 15717 | `	return SXRET_OK;` |
|        2 | 15718 |  |
|        - | 15719 | `/* Table of built-in VM functions. */` |
|        - | 15720 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 15721 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 15722 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 15723 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 15724 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 15725 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 15726 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 15727 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 15728 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 15729 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 15730 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 15731 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 15732 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 15733 | `	    /* Constants management */` |
|        - | 15734 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 15735 | `	{ "define",   vm_builtin_define               },` |
|        - | 15736 | `	{ "constant", vm_builtin_constant             },` |
|        - | 15737 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 15738 | `	   /* Class/Object functions */` |
|        - | 15739 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 15740 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 15741 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 15742 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 15743 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 15744 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 15745 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 15746 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 15747 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 15748 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 15749 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 15750 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 15751 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 15752 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 15753 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 15754 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 15755 | `	   /* SPL Autoloading */` |
|        - | 15756 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 15757 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 15758 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 15759 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 15760 | `	   /* Random numbers/strings generators */` |
|        - | 15761 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 15762 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 15763 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 15764 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 15765 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 15766 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 15767 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 15768 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15769 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 15770 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 15771 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 15772 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15773 | `	   /* Language constructs functions */` |
|        - | 15774 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 15775 | `	{ "print", vm_builtin_print                   },` |
|        - | 15776 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 15777 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 15778 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 15779 | `	  /* Variable handling functions */` |
|        - | 15780 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 15781 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 15782 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 15783 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 15784 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 15785 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 15786 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 15787 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 15788 | `	  /* Ouput control functions */` |
|        - | 15789 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 15790 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 15791 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 15792 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 15793 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 15794 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 15795 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 15796 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 15797 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 15798 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 15799 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 15800 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 15801 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 15802 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 15803 | `	  /* Assertion functions */` |
|        - | 15804 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 15805 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 15806 | `	  /* Error reporting functions */` |
|        - | 15807 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 15808 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 15809 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 15810 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 15811 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 15812 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 15813 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 15814 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 15815 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 15816 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 15817 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 15818 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 15819 | `	  /* Release info */` |
|        - | 15820 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 15821 | `	{"phpversion",       vm_builtin_phpversion    },` |
|        - | 15822 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|        - | 15823 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 15824 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 15825 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 15826 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 15827 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 15828 | `	  /* hashmap */` |
|        - | 15829 | `	{"compact",          vm_builtin_compact       },` |
|        - | 15830 | `	{"extract",          vm_builtin_extract       },` |
|        - | 15831 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 15832 | `	  /* URL related function */` |
|        - | 15833 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 15834 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 15835 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15836 | `	   /* XML processing functions */` |
|        - | 15837 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 15838 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 15839 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 15840 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 15841 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 15842 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 15843 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 15844 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 15845 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 15846 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 15847 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 15848 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 15849 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 15850 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 15851 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 15852 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 15853 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 15854 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 15855 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 15856 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 15857 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 15858 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15859 | `	   /* UTF-8 encoding/decoding */` |
|        - | 15860 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 15861 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 15862 | `	   /* Command line processing */` |
|        - | 15863 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 15864 | `	   /* JSON encoding/decoding */` |
|        - | 15865 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 15866 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 15867 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|        - | 15868 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 15869 | `	{"json_validate",  vm_builtin_json_validate },` |
|        - | 15870 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 15871 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 15872 | `	   /* Files/URI inclusion facility */` |
|        - | 15873 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 15874 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 15875 | `	{ "include",      vm_builtin_include          },` |
|        - | 15876 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 15877 | `	{ "require",      vm_builtin_require          },` |
|        - | 15878 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 15879 | `};` |
|        - | 15880 | `/*` |
|        - | 15881 | ` * Register the built-in VM functions defined above.` |
|        - | 15882 | ` */` |
|     2826 | 15883 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 15884 |  |
|        - | 15885 | `	sxi32 rc;` |
|        - | 15886 | `	sxu32 n;` |
|   381512 | 15887 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 15888 | `		/* Note that these special functions have access` |
|        - | 15889 | `		 * to the underlying virtual machine as their` |
|        - | 15890 | `		 * private data.` |
|        - | 15891 | `		 */` |
|   378686 | 15892 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   378686 | 15893 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 15894 | `			return rc;` |
|        - | 15895 | `		}` |
|   189344 | 15896 | `	}` |
|     2828 | 15897 | `	return SXRET_OK;` |
|     1415 | 15898 |  |
|        - | 15899 | `/*` |
|        - | 15900 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 15901 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 15902 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 15903 | ` */` |
|   182382 | 15904 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 15905 |  |
|   182384 | 15906 | `	if( !iLoadable ){` |
|   180296 | 15907 | `		return pClass;` |
|        - | 15908 | `	}` |
|     2094 | 15909 | `	while(pClass){` |
|     2090 | 15910 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     2086 | 15911 | `			return pClass;` |
|        - | 15912 | `		}` |
|        5 | 15913 | `		pClass = pClass->pNextName;` |
|        1 | 15914 | `	}` |
|        5 | 15915 | `	return 0;` |
|    91193 | 15916 |  |
|        - | 15917 | `/*` |
|        - | 15918 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 15919 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 15920 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 15921 | ` * registered in the VM's class table.` |
|        - | 15922 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 15923 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 15924 | ` */` |
|       38 | 15925 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 15926 |  |
|        - | 15927 | `	VmAutoloadCB *pEntry;` |
|        - | 15928 | `	ph7_value sArg,sResult;` |
|        - | 15929 | `	SyHashEntry *pHashEntry;` |
|        - | 15930 | `	ph7_class *pClass;` |
|        - | 15931 | `	sxu32 n,nEntry;` |
|       40 | 15932 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       40 | 15933 | `	if( nEntry < 1 ){` |
|       26 | 15934 | `		return 0;` |
|        - | 15935 | `	}` |
|        - | 15936 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 15937 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 15938 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 15939 | `	}` |
|        - | 15940 | `	/* Mark this class as being autoloaded */` |
|       14 | 15941 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 15942 | `	/* Prepare the class name argument */` |
|       14 | 15943 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 15944 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 15945 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 15946 | `	pClass = 0;` |
|       28 | 15947 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 15948 | `		ph7_value *apArg[1];` |
|       24 | 15949 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 15950 | `		if( pEntry == 0 ){` |
|      ! 0 | 15951 | `			continue;` |
|        - | 15952 | `		}` |
|       24 | 15953 | `		apArg[0] = &sArg;` |
|       24 | 15954 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 15955 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 15956 | `			continue;` |
|        - | 15957 | `		}` |
|        - | 15958 | `		/* Check if the class is now available */` |
|       24 | 15959 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 15960 | `		if( pHashEntry ){` |
|       10 | 15961 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 15962 | `			if( pClass ){` |
|       10 | 15963 | `				break;` |
|        - | 15964 | `			}` |
|      ! 0 | 15965 | `		}` |
|        9 | 15966 | `	}` |
|       14 | 15967 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 15968 | `	PH7_MemObjRelease(&sResult);` |
|        - | 15969 | `	/* Remove reentrancy guard */` |
|       14 | 15970 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 15971 | `	return pClass;` |
|       21 | 15972 |  |
|        - | 15973 | `/*` |
|        - | 15974 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 15975 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 15976 | ` */` |
|       18 | 15977 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 15978 |  |
|       20 | 15979 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 15980 |  |
|        - | 15981 | `/*` |
|        - | 15982 | ` * Check if the given name refer to an installed class.` |
|        - | 15983 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 15984 | ` */` |
|   182394 | 15985 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 15986 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 15987 | `	const char *zName,  /* Name of the target class */` |
|        - | 15988 | `	sxu32 nByte,        /* zName length */` |
|        - | 15989 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 15990 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 15991 | `						 */` |
|        - | 15992 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 15993 | `	)` |
|        2 | 15994 |  |
|        - | 15995 | `	SyHashEntry *pEntry;` |
|        - | 15996 | `	ph7_class *pClass;` |
|    91197 | 15997 | `	SXUNUSED(iNest);` |
|        - | 15998 | `	/* Exact class lookup.` |
|        - | 15999 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 16000 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   182396 | 16001 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|   182396 | 16002 | `	if( pEntry == 0 ){` |
|        - | 16003 | `		/* Class not found in hash table — try autoload before giving up */` |
|       22 | 16004 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 16005 | `	}` |
|   182376 | 16006 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   182376 | 16007 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    91199 | 16008 |  |
|        - | 16009 | `/*` |
|        - | 16010 | ` * Reference Table Implementation` |
|        - | 16011 | ` * Status: stable <chm@symisc.net>` |
|        - | 16012 | ` * Intro` |
|        - | 16013 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 16014 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 16015 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 16016 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 16017 | ` *  Refer to the official for more information on this powerful` |
|        - | 16018 | ` *  extension.` |
|        - | 16019 | ` */` |
|        - | 16020 | `/*` |
|        - | 16021 | ` * Allocate a new reference entry.` |
|        - | 16022 | ` */` |
|  3203096 | 16023 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 16024 |  |
|        - | 16025 | `	VmRefObj *pRef;` |
|        - | 16026 | `	/* Allocate a new instance */` |
|  3203098 | 16027 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3203098 | 16028 | `	if( pRef == 0 ){` |
|      ! 0 | 16029 | `		return 0;` |
|        - | 16030 | `	}` |
|        - | 16031 | `	/* Zero the structure */` |
|  3203098 | 16032 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 16033 | `	/* Initialize fields */` |
|  3203098 | 16034 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3203098 | 16035 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3203098 | 16036 | `	pRef->nIdx = nIdx;` |
|  3203098 | 16037 | `	return pRef;` |
|  1601550 | 16038 |  |
|        - | 16039 | `/*` |
|        - | 16040 | ` * Default hash function used by the reference table` |
|        - | 16041 | ` * for lookup/insertion operations.` |
|        - | 16042 | ` */` |
| 17544138 | 16043 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 16044 |  |
|        - | 16045 | `	/* Calculate the hash based on the memory object index */` |
| 17544140 | 16046 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 16047 |  |
|        - | 16048 | `/*` |
|        - | 16049 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 16050 | ` * in the reference table.` |
|        - | 16051 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 16052 | ` * otherwise.` |
|        - | 16053 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16054 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16055 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16056 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16057 | ` * Refer to the official for more information on this powerful` |
|        - | 16058 | ` * extension.` |
|        - | 16059 | ` */` |
|  9549328 | 16060 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 16061 |  |
|        - | 16062 | `	VmRefObj *pRef;` |
|        - | 16063 | `	sxu32 nBucket;` |
|        - | 16064 | `	/* Point to the appropriate bucket */` |
|  9549330 | 16065 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 16066 | `	/* Perform the lookup */` |
|  9549330 | 16067 | `	pRef = pVm->apRefObj[nBucket];` |
| 20995179 | 16068 | `	for(;;){` |
| 41980859 | 16069 | `		if( pRef == 0 ){` |
|  3308482 | 16070 | `			break;` |
|        - | 16071 | `		}` |
| 38672379 | 16072 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 16073 | `			/* Entry found */` |
|  6240850 | 16074 | `			return pRef;` |
|        - | 16075 | `		}` |
|        - | 16076 | `		/* Point to the next entry */` |
| 32431531 | 16077 | `		pRef = pRef->pNextCollide;` |
|        2 | 16078 | `	}` |
|        - | 16079 | `	/* No such entry,return NULL */` |
|  3308482 | 16080 | `	return 0;` |
|  4774666 | 16081 |  |
|        - | 16082 | `/*` |
|        - | 16083 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 16084 | ` *` |
|        - | 16085 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16086 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16087 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16088 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16089 | ` * Refer to the official for more information on this powerful` |
|        - | 16090 | ` * extension.` |
|        - | 16091 | ` */` |
|  3203096 | 16092 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 16093 |  |
|        - | 16094 | `	sxu32 nBucket;` |
|  3203098 | 16095 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 16096 | `		VmRefObj **apNew;` |
|        - | 16097 | `		sxu32 nNew;` |
|        - | 16098 | `		/* Allocate a larger table */` |
|     4484 | 16099 | `		nNew = pVm->nRefSize << 1;` |
|     4484 | 16100 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4484 | 16101 | `		if( apNew ){` |
|     4484 | 16102 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 16103 | `			sxu32 n;` |
|        - | 16104 | `			/* Zero the structure */` |
|     4484 | 16105 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 16106 | `			/* Rehash all referenced entries */` |
|  2848110 | 16107 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 16108 | `				/* Remove old collision links */` |
|  2843628 | 16109 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 16110 | `				/* Point to the appropriate bucket */` |
|  2843628 | 16111 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 16112 | `				/* Insert the entry  */` |
|  2843628 | 16113 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2843628 | 16114 | `				if( apNew[nBucket] ){` |
|  2301116 | 16115 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1150557 | 16116 | `				}` |
|  2843628 | 16117 | `				apNew[nBucket] = pEntry;` |
|        - | 16118 | `				/* Point to the next entry */` |
|  2843628 | 16119 | `				pEntry = pEntry->pNext;` |
|  1421815 | 16120 | `			}` |
|        - | 16121 | `			/* Release the old table */` |
|     4484 | 16122 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 16123 | `			/* Install the new one */` |
|     4484 | 16124 | `			pVm->apRefObj = apNew;` |
|     4484 | 16125 | `			pVm->nRefSize = nNew;` |
|     2241 | 16126 | `		}` |
|     2241 | 16127 | `	}` |
|        - | 16128 | `	/* Point to the appropriate bucket */` |
|  3203098 | 16129 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 16130 | `	/* Insert the entry */` |
|  3203098 | 16131 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3203098 | 16132 | `	if( pVm->apRefObj[nBucket] ){` |
|  2614677 | 16133 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1307346 | 16134 | `	}` |
|  3203098 | 16135 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3203098 | 16136 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3203098 | 16137 | `	pVm->nRefUsed++;` |
|  3203098 | 16138 | `	return SXRET_OK;` |
|        2 | 16139 |  |
|        - | 16140 | `/*` |
|        - | 16141 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 16142 | ` * the reference table.` |
|        - | 16143 | ` * This function is invoked when the user perform an unset` |
|        - | 16144 | ` * call [i.e: unset($var); ].` |
|        - | 16145 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16146 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16147 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16148 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16149 | ` * Refer to the official for more information on this powerful` |
|        - | 16150 | ` * extension.` |
|        - | 16151 | ` */` |
|  3161840 | 16152 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 16153 |  |
|        - | 16154 | `	ph7_hashmap_node **apNode;` |
|        - | 16155 | `	SyHashEntry **apEntry;` |
|        - | 16156 | `	sxu32 n;` |
|        - | 16157 | `	/* Point to the reference table */` |
|  3161842 | 16158 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3161842 | 16159 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 16160 | `	/* Unlink the entry from the reference table */` |
|  3273194 | 16161 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   111354 | 16162 | `		if( apEntry[n] ){` |
|   111304 | 16163 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    55651 | 16164 | `		}` |
|    55678 | 16165 | `	}` |
|  6212404 | 16166 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3050564 | 16167 | `		if( apNode[n] ){` |
|     7042 | 16168 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3520 | 16169 | `		}` |
|  1525283 | 16170 | `	}` |
|  3161842 | 16171 | `	if( pRef->pPrevCollide ){` |
|  1213754 | 16172 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   606598 | 16173 | `	}else{` |
|  1948090 | 16174 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 16175 | `	}` |
|  3161842 | 16176 | `	if( pRef->pNextCollide ){` |
|  1801708 | 16177 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   900850 | 16178 | `	}` |
|  3161842 | 16179 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 16180 | `	/* Release the node */` |
|  3161842 | 16181 | `	SySetRelease(&pRef->aReference);` |
|  3161842 | 16182 | `	SySetRelease(&pRef->aArrEntries);` |
|  3161842 | 16183 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3161842 | 16184 | `	pVm->nRefUsed--;` |
|  3161842 | 16185 | `	return SXRET_OK;` |
|        2 | 16186 |  |
|        - | 16187 | `/*` |
|        - | 16188 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 16189 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16190 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16191 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16192 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16193 | ` * Refer to the official for more information on this powerful` |
|        - | 16194 | ` * extension.` |
|        - | 16195 | ` */` |
|  3238702 | 16196 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 16197 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 16198 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 16199 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 16200 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 16201 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 16202 | `	)` |
|        2 | 16203 |  |
|  3238704 | 16204 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 16205 | `	VmRefObj *pRef;` |
|        - | 16206 | `	/* Check if the referenced object already exists */` |
|  3238704 | 16207 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3238704 | 16208 | `	if( pRef == 0 ){` |
|        - | 16209 | `		/* Create a new entry */` |
|  3203098 | 16210 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3203098 | 16211 | `		if( pRef == 0 ){` |
|      ! 0 | 16212 | `			return SXERR_MEM;` |
|        - | 16213 | `		}` |
|  3203098 | 16214 | `		pRef->iFlags = iFlags;` |
|        - | 16215 | `		/* Install the entry */` |
|  3203098 | 16216 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1601548 | 16217 | `	}` |
|  3238704 | 16218 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3238704 | 16219 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 16220 | `		VmSlot sRef;` |
|        - | 16221 | `		/* Local frame,record referenced entry so that it can` |
|        - | 16222 | `		 * be deleted when we leave this frame.` |
|        - | 16223 | `		 */` |
|   105494 | 16224 | `		sRef.nIdx = nIdx;` |
|   105494 | 16225 | `		sRef.pUserData = pEntry;` |
|   105494 | 16226 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 16227 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 16228 | `		}` |
|    52746 | 16229 | `	}` |
|  3238704 | 16230 | `	if( pEntry ){` |
|        - | 16231 | `		/* Address of the hash-entry */` |
|   140872 | 16232 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    70435 | 16233 | `	}` |
|  3238704 | 16234 | `	if( pMapEntry ){` |
|        - | 16235 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3089416 | 16236 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1544707 | 16237 | `	}` |
|  3238704 | 16238 | `	return SXRET_OK;` |
|  1619353 | 16239 |  |
|        - | 16240 | `/*` |
|        - | 16241 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 16242 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 16243 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 16244 | ` * the reference implementation is consistent,solid and it's` |
|        - | 16245 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 16246 | ` * Refer to the official for more information on this powerful` |
|        - | 16247 | ` * extension.` |
|        - | 16248 | ` */` |
|  3148978 | 16249 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 16250 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 16251 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 16252 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 16253 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 16254 | `	)` |
|        2 | 16255 |  |
|        - | 16256 | `	VmRefObj *pRef;` |
|        - | 16257 | `	sxu32 n;` |
|        - | 16258 | `	/* Check if the referenced object already exists */` |
|  3148980 | 16259 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3148980 | 16260 | `	if( pRef == 0 ){` |
|        - | 16261 | `		/* Not such entry */` |
|   105380 | 16262 | `		return SXERR_NOTFOUND;` |
|        - | 16263 | `	}` |
|        - | 16264 | `	/* Remove the desired entry */` |
|  3043602 | 16265 | `	if( pEntry ){` |
|        - | 16266 | `		SyHashEntry **apEntry;` |
|       74 | 16267 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      264 | 16268 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      192 | 16269 | `			if( apEntry[n] == pEntry ){` |
|        - | 16270 | `				/* Nullify the entry */` |
|       74 | 16271 | `				apEntry[n] = 0;` |
|        - | 16272 | `				/*` |
|        - | 16273 | `				 * NOTE:` |
|        - | 16274 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 16275 | `				 * we avoid wasting spaces.` |
|        - | 16276 | `				 */` |
|       36 | 16277 | `			}` |
|       97 | 16278 | `		}` |
|       36 | 16279 | `	}` |
|  3043602 | 16280 | `	if( pMapEntry ){` |
|        - | 16281 | `		ph7_hashmap_node **apNode;` |
|  3043530 | 16282 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  6087152 | 16283 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3043624 | 16284 | `			if( apNode[n] == pMapEntry ){` |
|        - | 16285 | `				/* nullify the entry */` |
|  3043530 | 16286 | `				apNode[n] = 0;` |
|  1521764 | 16287 | `			}` |
|  1521813 | 16288 | `		}` |
|  1521764 | 16289 | `	}` |
|  3043602 | 16290 | `	return SXRET_OK;` |
|  1574491 | 16291 |  |
|        - | 16292 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 16293 | `/*` |
|        - | 16294 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 16295 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 16296 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 16297 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 16298 | ` * For more information on how to register IO stream devices,please` |
|        - | 16299 | ` * refer to the official documentation.` |
|        - | 16300 | ` */` |
|    29116 | 16301 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 16302 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 16303 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 16304 | `	int nByte              /* *pzDevice length*/` |
|        - | 16305 | `	)` |
|        2 | 16306 |  |
|        - | 16307 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 16308 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 16309 | `	SyString sDev,sCur;` |
|        - | 16310 | `	sxu32 n,nEntry;` |
|        - | 16311 | `	int rc;` |
|        - | 16312 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    29118 | 16313 | `	zNext = zCur = zIn = *pzDevice;` |
|    29118 | 16314 | `	zEnd = &zIn[nByte];` |
|  1859823 | 16315 | `	while( zIn < zEnd ){` |
|  1830709 | 16316 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 16317 | `			/* Got one */` |
|        3 | 16318 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 16319 | `			break;` |
|        - | 16320 | `		}` |
|        - | 16321 | `		/* Advance the cursor */` |
|  1830707 | 16322 | `		zIn++;` |
|        2 | 16323 | `	}` |
|    29118 | 16324 | `	if( zIn >= zEnd ){` |
|        - | 16325 | `		/* No such scheme,return the default stream */` |
|    29116 | 16326 | `		return pVm->pDefStream;` |
|        - | 16327 | `	}` |
|        3 | 16328 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 16329 | `	/* Remove leading and trailing white spaces */` |
|        3 | 16330 | `	SyStringFullTrim(&sDev);` |
|        - | 16331 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 16332 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 16333 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 16334 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 16335 | `		pStream = apStream[n];` |
|        3 | 16336 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 16337 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 16338 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 16339 | `		if( rc == 0 ){` |
|        - | 16340 | `			/* Stream device found */` |
|        3 | 16341 | `			*pzDevice = zNext;` |
|        3 | 16342 | `			return pStream;` |
|        - | 16343 | `		}` |
|      ! 0 | 16344 | `	}` |
|        - | 16345 | `	/* No such stream,return NULL */` |
|      ! 0 | 16346 | `	return 0;` |
|    14560 | 16347 |  |
|        - | 16348 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 16349 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 16350 |  |
