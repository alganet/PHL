# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5236/7401 lines (70.75%)

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
|        - |     9 | `/*` |
|        - |    10 | ` * The code in this file implements execution method of the PH7 Virtual Machine.` |
|        - |    11 | ` * The PH7 compiler (implemented in 'compiler.c' and 'parse.c') generates a bytecode program` |
|        - |    12 | ` * which is then executed by the virtual machine implemented here to do the work of the PHP` |
|        - |    13 | ` * statements.` |
|        - |    14 | ` * PH7 bytecode programs are similar in form to assembly language. The program consists` |
|        - |    15 | ` * of a linear sequence of operations .Each operation has an opcode and 3 operands.` |
|        - |    16 | ` * Operands P1 and P2 are integers where the first is signed while the second is unsigned.` |
|        - |    17 | ` * Operand P3 is an arbitrary pointer specific to each instruction. The P2 operand is usually` |
|        - |    18 | ` * the jump destination used by the OP_JMP,OP_JZ,OP_JNZ,... instructions.` |
|        - |    19 | ` * Opcodes will typically ignore one or more operands. Many opcodes ignore all three operands.` |
|        - |    20 | ` * Computation results are stored on a stack. Each entry on the stack is of type ph7_value.` |
|        - |    21 | ` * PH7 uses the ph7_value object to represent all values that can be stored in a PHP variable.` |
|        - |    22 | ` * Since PHP uses dynamic typing for the values it stores. Values stored in ph7_value objects` |
|        - |    23 | ` * can be integers,floating point values,strings,arrays,class instances (object in the PHP jargon)` |
|        - |    24 | ` * and so on.` |
|        - |    25 | ` * Internally,the PH7 virtual machine manipulates nearly all PHP values as ph7_values structures.` |
|        - |    26 | ` * Each ph7_value may cache multiple representations(string,integer etc.) of the same value.` |
|        - |    27 | ` * An implicit conversion from one type to the other occurs as necessary.` |
|        - |    28 | ` * Most of the code in this file is taken up by the [VmByteCodeExec()] function which does` |
|        - |    29 | ` * the work of interpreting a PH7 bytecode program. But other routines are also provided` |
|        - |    30 | ` * to help in building up a program instruction by instruction. Also note that sepcial` |
|        - |    31 | ` * functions that need access to the underlying virtual machine details such as [die()],` |
|        - |    32 | ` * [func_get_args()],[call_user_func()],[ob_start()] and many more are implemented here.` |
|        - |    33 | ` */` |
|        - |    34 | `/*` |
|        - |    35 | ` * Each active virtual machine frame is represented by an instance` |
|        - |    36 | ` * of the following structure.` |
|        - |    37 | ` * VM Frame hold local variables and other stuff related to function call.` |
|        - |    38 | ` */` |
|        - |    39 | `struct VmFrame` |
|        - |    40 |  |
|        - |    41 | `	VmFrame *pParent; /* Parent frame or NULL if global scope */` |
|        - |    42 | `	void *pUserData;  /* Upper layer private data associated with this frame */` |
|        - |    43 | `	ph7_class_instance *pThis; /* Current class instance [i.e: the '$this' variable].NULL otherwise */` |
|        - |    44 | `	SySet sLocal;     /* Local variables container (VmSlot instance) */` |
|        - |    45 | `	ph7_vm *pVm;      /* VM that own this frame */` |
|        - |    46 | `	SyHash hVar;      /* Variable hashtable for fast lookup */` |
|        - |    47 | `	SySet sArg;       /* Function arguments container */` |
|        - |    48 | `	SySet sRef;       /* Local reference table (VmSlot instance) */` |
|        - |    49 | `	sxi32 iFlags;     /* Frame configuration flags (See below)*/` |
|        - |    50 | `	sxu32 iExceptionJump; /* Exception jump destination */` |
|        - |    51 | `};` |
|        - |    52 | `#define VM_FRAME_EXCEPTION  0x01 /* Special Exception frame */` |
|        - |    53 | `#define VM_FRAME_THROW      0x02 /* An exception was thrown */` |
|        - |    54 | `#define VM_FRAME_CATCH      0x04 /* Catch frame */` |
|        - |    55 | `/*` |
|        - |    56 | ` * When a user defined variable is released (via manual unset($x) or garbage collected)` |
|        - |    57 | ` * memory object index is stored in an instance of the following structure and put` |
|        - |    58 | ` * in the free object table so that it can be reused again without allocating` |
|        - |    59 | ` * a new memory object.` |
|        - |    60 | ` */` |
|        - |    61 | `typedef struct VmSlot VmSlot;` |
|        - |    62 | `struct VmSlot` |
|        - |    63 |  |
|        - |    64 | `	sxu32 nIdx;      /* Index in pVm->aMemObj[] */` |
|        - |    65 | `	void *pUserData; /* Upper-layer private data */` |
|        - |    66 | `};` |
|        - |    67 | `/*` |
|        - |    68 | ` * An entry in the reference table is represented by an instance of the` |
|        - |    69 | ` * follwoing table.` |
|        - |    70 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - |    71 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - |    72 | ` * the reference implementation is consistent,solid and it's` |
|        - |    73 | ` * behavior resemble the C++ reference mechanism.` |
|        - |    74 | ` * Refer to the official for more information on this powerful` |
|        - |    75 | ` * extension.` |
|        - |    76 | ` */` |
|        - |    77 | `struct VmRefObj` |
|        - |    78 |  |
|        - |    79 | `	SySet aReference;  /* Table of references to this memory object */` |
|        - |    80 | `	SySet aArrEntries; /* Foreign hashmap entries [i.e: array(&$a) ] */` |
|        - |    81 | `	sxu32 nIdx;        /* Referenced object index */` |
|        - |    82 | `	sxi32 iFlags;      /* Configuration flags */` |
|        - |    83 | `	VmRefObj *pNextCollide,*pPrevCollide; /* Collision link */` |
|        - |    84 | `	VmRefObj *pNext,*pPrev;               /* List of all referenced objects */` |
|        - |    85 | `};` |
|        - |    86 | `#define VM_REF_IDX_KEEP  0x001 /* Do not restore the memory object to the free list */` |
|        - |    87 | `/*` |
|        - |    88 | ` * Output control buffer entry.` |
|        - |    89 | ` * Refer to the implementation of [ob_start()] for more information.` |
|        - |    90 | ` */` |
|        - |    91 | `typedef struct VmObEntry VmObEntry;` |
|        - |    92 | `struct VmObEntry` |
|        - |    93 |  |
|        - |    94 | `	ph7_value sCallback; /* User defined callback */` |
|        - |    95 | `	SyBlob sOB;          /* Output buffer consumer */` |
|        - |    96 | `};` |
|        - |    97 | `/*` |
|        - |    98 | ` * Each installed shutdown callback (registered using [register_shutdown_function()] )` |
|        - |    99 | ` * is stored in an instance of the following structure.` |
|        - |   100 | ` * Refer to the implementation of [register_shutdown_function(()] for more information.` |
|        - |   101 | ` */` |
|        - |   102 | `typedef struct VmShutdownCB VmShutdownCB;` |
|        - |   103 | `struct VmShutdownCB` |
|        - |   104 |  |
|        - |   105 | `	ph7_value sCallback; /* Shutdown callback */` |
|        - |   106 | `	ph7_value aArg[10];   /* Callback arguments (10 maximum arguments) */` |
|        - |   107 | `	int nArg;             /* Total number of given arguments */` |
|        - |   108 | `};` |
|        - |   109 | `/* Uncaught exception code value */` |
|        - |   110 | `#define PH7_EXCEPTION -255` |
|        - |   111 |  |
|        - |   112 | `/*` |
|        - |   113 | ` * Return TRUE if either operand is a NaN real value.` |
|        - |   114 | ` */` |
|   669216 |   115 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |   116 |  |
|   669218 |   117 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       23 |   118 | `		return TRUE;` |
|        - |   119 | `	}` |
|   669196 |   120 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |   121 | `		return TRUE;` |
|        - |   122 | `	}` |
|   669188 |   123 | `	return FALSE;` |
|   334632 |   124 |  |
|        - |   125 | `/*` |
|        - |   126 | ` * Each parsed URI is recorded and stored in an instance of the following structure.` |
|        - |   127 | ` * This structure and it's related routines are taken verbatim from the xHT project` |
|        - |   128 | ` * [A modern embeddable HTTP engine implementing all the RFC2616 methods]` |
|        - |   129 | ` * the xHT project is developed internally by Symisc Systems.` |
|        - |   130 | ` */` |
|        - |   131 | `typedef struct SyhttpUri SyhttpUri;` |
|        - |   132 | `struct SyhttpUri` |
|        - |   133 |  |
|        - |   134 | `	SyString sHost;     /* Hostname or IP address */` |
|        - |   135 | `	SyString sPort;     /* Port number */` |
|        - |   136 | `	SyString sPath;     /* Mandatory resource path passed verbatim (Not decoded) */` |
|        - |   137 | `	SyString sQuery;    /* Query part */` |
|        - |   138 | `	SyString sFragment; /* Fragment part */` |
|        - |   139 | `	SyString sScheme;   /* Scheme */` |
|        - |   140 | `	SyString sUser;     /* Username */` |
|        - |   141 | `	SyString sPass;     /* Password */` |
|        - |   142 | `	SyString sRaw;      /* Raw URI */` |
|        - |   143 | `};` |
|        - |   144 | `/*` |
|        - |   145 | ` * An instance of the following structure is used to record all MIME headers seen` |
|        - |   146 | ` * during a HTTP interaction.` |
|        - |   147 | ` * This structure and it's related routines are taken verbatim from the xHT project` |
|        - |   148 | ` * [A modern embeddable HTTP engine implementing all the RFC2616 methods]` |
|        - |   149 | ` * the xHT project is developed internally by Symisc Systems.` |
|        - |   150 | ` */` |
|        - |   151 | `typedef struct SyhttpHeader SyhttpHeader;` |
|        - |   152 | `struct SyhttpHeader` |
|        - |   153 |  |
|        - |   154 | `	SyString sName;    /* Header name [i.e:"Content-Type","Host","User-Agent"]. NOT NUL TERMINATED */` |
|        - |   155 | `	SyString sValue;   /* Header values [i.e: "text/html"]. NOT NUL TERMINATED */` |
|        - |   156 | `};` |
|        - |   157 | `/*` |
|        - |   158 | ` * Supported HTTP methods.` |
|        - |   159 | ` */` |
|        - |   160 | `#define HTTP_METHOD_GET  1 /* GET */` |
|        - |   161 | `#define HTTP_METHOD_HEAD 2 /* HEAD */` |
|        - |   162 | `#define HTTP_METHOD_POST 3 /* POST */` |
|        - |   163 | `#define HTTP_METHOD_PUT  4 /* PUT */` |
|        - |   164 | `#define HTTP_METHOD_OTHR 5 /* Other HTTP methods [i.e: DELETE,TRACE,OPTIONS...]*/` |
|        - |   165 | `/*` |
|        - |   166 | ` * Supported HTTP protocol version.` |
|        - |   167 | ` */` |
|        - |   168 | `#define HTTP_PROTO_10 1 /* HTTP/1.0 */` |
|        - |   169 | `#define HTTP_PROTO_11 2 /* HTTP/1.1 */` |
|        - |   170 | `/*` |
|        - |   171 | ` * Register a constant and it's associated expansion callback so that` |
|        - |   172 | ` * it can be expanded from the target PHP program.` |
|        - |   173 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|        - |   174 | ` * simple and work as follows:` |
|        - |   175 | ` * Each registered constant have a C procedure associated with it.` |
|        - |   176 | ` * This procedure known as the constant expansion callback is responsible` |
|        - |   177 | ` * of expanding the invoked constant to the desired value,for example:` |
|        - |   178 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|        - |   179 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|        - |   180 | ` * (Windows,Linux,...) and so on.` |
|        - |   181 | ` * Please refer to the official documentation for additional information.` |
|        - |   182 | ` */` |
|   293322 |   183 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |   184 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |   185 | `	const SyString *pName,  /* Constant name */` |
|        - |   186 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |   187 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |   188 | `	)` |
|        2 |   189 |  |
|        - |   190 | `	ph7_constant *pCons;` |
|        - |   191 | `	SyHashEntry *pEntry;` |
|        - |   192 | `	char *zDupName;` |
|        - |   193 | `	sxi32 rc;` |
|   293324 |   194 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   293324 |   195 | `	if( pEntry ){` |
|        - |   196 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   197 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   198 | `		pCons->xExpand = xExpand;` |
|        6 |   199 | `		pCons->pUserData = pUserData;` |
|        6 |   200 | `		return SXRET_OK;` |
|        - |   201 | `	}` |
|        - |   202 | `	/* Allocate a new constant instance */` |
|   293320 |   203 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   293320 |   204 | `	if( pCons == 0 ){` |
|      ! 0 |   205 | `		return 0;` |
|        - |   206 | `	}` |
|        - |   207 | `	/* Duplicate constant name */` |
|   293320 |   208 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   293320 |   209 | `	if( zDupName == 0 ){` |
|      ! 0 |   210 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   211 | `		return 0;` |
|        - |   212 | `	}` |
|        - |   213 | `	/* Install the constant */` |
|   293320 |   214 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   293320 |   215 | `	pCons->xExpand = xExpand;` |
|   293320 |   216 | `	pCons->pUserData = pUserData;` |
|   293320 |   217 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   293320 |   218 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   219 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   220 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   221 | `		return rc;` |
|        - |   222 | `	}` |
|        - |   223 | `	/* All done,constant can be invoked from PHP code */` |
|   293320 |   224 | `	return SXRET_OK;` |
|   146663 |   225 |  |
|        - |   226 | `/*` |
|        - |   227 | ` * Allocate a new foreign function instance.` |
|        - |   228 | ` * This function return SXRET_OK on success. Any other` |
|        - |   229 | ` * return value indicates failure.` |
|        - |   230 | ` * Please refer to the official documentation for an introduction to` |
|        - |   231 | ` * the foreign function mechanism.` |
|        - |   232 | ` */` |
|   631620 |   233 | `static sxi32 PH7_NewForeignFunction(` |
|        - |   234 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   235 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   236 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   237 | `	void *pUserData,          /* Foreign function private data */` |
|        - |   238 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|        - |   239 | `	)` |
|        2 |   240 |  |
|        - |   241 | `	ph7_user_func *pFunc;` |
|        - |   242 | `	char *zDup;` |
|        - |   243 | `	/* Allocate a new user function */` |
|   631622 |   244 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   631622 |   245 | `	if( pFunc == 0 ){` |
|      ! 0 |   246 | `		return SXERR_MEM;` |
|        - |   247 | `	}` |
|        - |   248 | `	/* Duplicate function name */` |
|   631622 |   249 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   631622 |   250 | `	if( zDup == 0 ){` |
|      ! 0 |   251 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   252 | `		return SXERR_MEM;` |
|        - |   253 | `	}` |
|        - |   254 | `	/* Zero the structure */` |
|   631622 |   255 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   256 | `	/* Initialize structure fields */` |
|   631622 |   257 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   631622 |   258 | `	pFunc->pVm   = pVm;` |
|   631622 |   259 | `	pFunc->xFunc = xFunc;` |
|   631622 |   260 | `	pFunc->pUserData = pUserData;` |
|   631622 |   261 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   262 | `	/* Write a pointer to the new function */` |
|   631622 |   263 | `	*ppOut = pFunc;` |
|   631622 |   264 | `	return SXRET_OK;` |
|   315812 |   265 |  |
|        - |   266 | `/*` |
|        - |   267 | ` * Install a foreign function and it's associated callback so that` |
|        - |   268 | ` * it can be invoked from the target PHP code.` |
|        - |   269 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   270 | ` * return value indicates failure.` |
|        - |   271 | ` * Please refer to the official documentation for an introduction to` |
|        - |   272 | ` * the foreign function mechanism.` |
|        - |   273 | ` */` |
|   633072 |   274 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|        - |   275 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   276 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   277 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   278 | `	void *pUserData           /* Foreign function private data */` |
|        - |   279 | `	)` |
|        2 |   280 |  |
|        - |   281 | `	ph7_user_func *pFunc;` |
|        - |   282 | `	SyHashEntry *pEntry;` |
|        - |   283 | `	sxi32 rc;` |
|        - |   284 | `	/* Overwrite any previously registered function with the same name */` |
|   633074 |   285 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   633074 |   286 | `	if( pEntry ){` |
|     1454 |   287 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     1454 |   288 | `		pFunc->pUserData = pUserData;` |
|     1454 |   289 | `		pFunc->xFunc = xFunc;` |
|     1454 |   290 | `		SySetReset(&pFunc->aAux);` |
|     1454 |   291 | `		return SXRET_OK;` |
|        - |   292 | `	}` |
|        - |   293 | `	/* Create a new user function */` |
|   631622 |   294 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   631622 |   295 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   296 | `		return rc;` |
|        - |   297 | `	}` |
|        - |   298 | `	/* Install the function in the corresponding hashtable */` |
|   631622 |   299 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   631622 |   300 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   301 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   302 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   303 | `		return rc;` |
|        - |   304 | `	}` |
|        - |   305 | `	/* User function successfully installed */` |
|   631622 |   306 | `	return SXRET_OK;` |
|   316538 |   307 |  |
|        - |   308 | `/*` |
|        - |   309 | ` * Initialize a VM function.` |
|        - |   310 | ` */` |
|    72442 |   311 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   312 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   313 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   314 | `	const char *zName,  /* Function name */` |
|        - |   315 | `	sxu32 nByte,        /* zName length */` |
|        - |   316 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   317 | `	void *pUserData     /* Function private data */` |
|        - |   318 | `	)` |
|        2 |   319 |  |
|        - |   320 | `	/* Zero the structure */` |
|    72444 |   321 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   322 | `	/* Initialize structure fields */` |
|        - |   323 | `	/* Arguments container */` |
|    72444 |   324 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   325 | `	/* Static variable container */` |
|    72444 |   326 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   327 | `	/* Bytecode container */` |
|    72444 |   328 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   329 | `    /* Preallocate some instruction slots */` |
|    72444 |   330 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   331 | `	/* Closure environment */` |
|    72444 |   332 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|    72444 |   333 | `	pFunc->iFlags = iFlags;` |
|    72444 |   334 | `	pFunc->pUserData = pUserData;` |
|    72444 |   335 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|    72444 |   336 | `	return SXRET_OK;` |
|        2 |   337 |  |
|        - |   338 | `/*` |
|        - |   339 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   340 | ` */` |
|   228400 |   341 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   342 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   343 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   344 | `	SyString *pName     /* Function name */` |
|        - |   345 | `	)` |
|        2 |   346 |  |
|        - |   347 | `	SyHashEntry *pEntry;` |
|        - |   348 | `	sxi32 rc;` |
|   228402 |   349 | `	if( pName == 0 ){` |
|        - |   350 | `		/* Use the built-in name */` |
|    22642 |   351 | `		pName = &pFunc->sName;` |
|    11320 |   352 | `	}` |
|        - |   353 | `	/* Check for duplicates (functions with the same name) first */` |
|   228402 |   354 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   228402 |   355 | `	if( pEntry ){` |
|   169846 |   356 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   169846 |   357 | `		if( pLink != pFunc ){` |
|        - |   358 | `			/* Link */` |
|      179 |   359 | `			pFunc->pNextName = pLink;` |
|      179 |   360 | `			pEntry->pUserData = pFunc;` |
|       89 |   361 | `		}` |
|   169846 |   362 | `		return SXRET_OK;` |
|        - |   363 | `	}` |
|        - |   364 | `	/* First time seen */` |
|    58558 |   365 | `	pFunc->pNextName = 0;` |
|    58558 |   366 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    58558 |   367 | `	return rc;` |
|   114202 |   368 |  |
|        - |   369 | `/*` |
|        - |   370 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   371 | ` */` |
|    19064 |   372 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   373 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   374 | `	ph7_class *pClass /* Target Class */` |
|        - |   375 | `	)` |
|        2 |   376 |  |
|    19066 |   377 | `	SyString *pName = &pClass->sName;` |
|        - |   378 | `	SyHashEntry *pEntry;` |
|        - |   379 | `	sxi32 rc;` |
|        - |   380 | `	/* Check for duplicates */` |
|    19066 |   381 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    19066 |   382 | `	if( pEntry ){` |
|       31 |   383 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   384 | `		/* Link entry with the same name */` |
|       31 |   385 | `		pClass->pNextName = pLink;` |
|       31 |   386 | `		pEntry->pUserData = pClass;` |
|       31 |   387 | `		return SXRET_OK;` |
|        - |   388 | `	}` |
|    19036 |   389 | `	pClass->pNextName = 0;` |
|        - |   390 | `	/* Perform a simple hashtable insertion */` |
|    19036 |   391 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    19036 |   392 | `	return rc;` |
|     9534 |   393 |  |
|        - |   394 | `/*` |
|        - |   395 | ` * Instruction builder interface.` |
|        - |   396 | ` */` |
|  1817890 |   397 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   398 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   399 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   400 | `	sxi32 iP1,    /* First operand */` |
|        - |   401 | `	sxu32 iP2,    /* Second operand */` |
|        - |   402 | `	void *p3,     /* Third operand */` |
|        - |   403 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   404 | `	)` |
|        2 |   405 |  |
|        - |   406 | `	VmInstr sInstr;` |
|        - |   407 | `	sxi32 rc;` |
|        - |   408 | `	/* Fill the VM instruction */` |
|  1817892 |   409 | `	sInstr.iOp = (sxu8)iOp;` |
|  1817892 |   410 | `	sInstr.iP1 = iP1;` |
|  1817892 |   411 | `	sInstr.iP2 = iP2;` |
|  1817892 |   412 | `	sInstr.p3  = p3;` |
|  1817892 |   413 | `	if( pIndex ){` |
|        - |   414 | `		/* Instruction index in the bytecode array */` |
|   110824 |   415 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    55411 |   416 | `	}` |
|        - |   417 | `	/* Finally,record the instruction */` |
|  1817892 |   418 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  1817892 |   419 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   420 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   421 | `		/* Fall throw */` |
|      ! 0 |   422 | `	}` |
|  1817892 |   423 | `	return rc;` |
|        2 |   424 |  |
|        - |   425 | `/*` |
|        - |   426 | ` * Swap the current bytecode container with the given one.` |
|        - |   427 | ` */` |
|   176160 |   428 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   429 |  |
|   176162 |   430 | `	if( pContainer == 0 ){` |
|        - |   431 | `		/* Point to the default container */` |
|      ! 0 |   432 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   433 | `	}else{` |
|        - |   434 | `		/* Change container */` |
|   176162 |   435 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   436 | `	}` |
|   176162 |   437 | `	return SXRET_OK;` |
|        2 |   438 |  |
|        - |   439 | `/*` |
|        - |   440 | ` * Return the current bytecode container.` |
|        - |   441 | ` */` |
|    88080 |   442 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   443 |  |
|    88082 |   444 | `	return pVm->pByteContainer;` |
|        2 |   445 |  |
|        - |   446 | `/*` |
|        - |   447 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   448 | ` */` |
|   109036 |   449 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   450 |  |
|        - |   451 | `	VmInstr *pInstr;` |
|   109038 |   452 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   109038 |   453 | `	return pInstr;` |
|        2 |   454 |  |
|        - |   455 | `/*` |
|        - |   456 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   457 | ` */` |
|   527158 |   458 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   459 |  |
|   527160 |   460 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   461 |  |
|        - |   462 | `/*` |
|        - |   463 | ` * Pop the last VM instruction.` |
|        - |   464 | ` */` |
|   105760 |   465 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   466 |  |
|   105762 |   467 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   468 |  |
|        - |   469 | `/*` |
|        - |   470 | ` * Peek the last VM instruction.` |
|        - |   471 | ` */` |
|   281714 |   472 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   473 |  |
|   281716 |   474 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   475 |  |
|     7062 |   476 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   477 |  |
|        - |   478 | `	VmInstr *aInstr;` |
|        - |   479 | `	sxu32 n;` |
|     7064 |   480 | `	n = SySetUsed(pVm->pByteContainer);` |
|     7064 |   481 | `	if( n < 2 ){` |
|      ! 0 |   482 | `		return 0;` |
|        - |   483 | `	}` |
|     7064 |   484 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|     7064 |   485 | `	return &aInstr[n - 2];` |
|     3533 |   486 |  |
|        - |   487 | `/*` |
|        - |   488 | ` * Allocate a new virtual machine frame.` |
|        - |   489 | ` */` |
|    11546 |   490 | `static VmFrame * VmNewFrame(` |
|        - |   491 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   492 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   493 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   494 | `	)` |
|        2 |   495 |  |
|        - |   496 | `	VmFrame *pFrame;` |
|        - |   497 | `	/* Allocate a new vm frame */` |
|    11548 |   498 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    11548 |   499 | `	if( pFrame == 0 ){` |
|      ! 0 |   500 | `		return 0;` |
|        - |   501 | `	}` |
|        - |   502 | `	/* Zero the structure */` |
|    11548 |   503 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   504 | `	/* Initialize frame fields */` |
|    11548 |   505 | `	pFrame->pUserData = pUserData;` |
|    11548 |   506 | `	pFrame->pThis = pThis;` |
|    11548 |   507 | `	pFrame->pVm = pVm;` |
|    11548 |   508 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    11548 |   509 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    11548 |   510 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    11548 |   511 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    11548 |   512 | `	return pFrame;` |
|     5775 |   513 |  |
|        - |   514 | `/*` |
|        - |   515 | ` * Enter a VM frame.` |
|        - |   516 | ` */` |
|    11546 |   517 | `static sxi32 VmEnterFrame(` |
|        - |   518 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   519 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   520 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   521 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   522 | `	)` |
|        2 |   523 |  |
|        - |   524 | `	VmFrame *pFrame;` |
|        - |   525 | `	/* Allocate a new frame */` |
|    11548 |   526 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    11548 |   527 | `	if( pFrame == 0 ){` |
|      ! 0 |   528 | `		return SXERR_MEM;` |
|        - |   529 | `	}` |
|        - |   530 | `	/* Link to the list of active VM frame */` |
|    11548 |   531 | `	pFrame->pParent = pVm->pFrame;` |
|    11548 |   532 | `	pVm->pFrame = pFrame;` |
|    11548 |   533 | `	if( ppFrame ){` |
|        - |   534 | `		/* Write a pointer to the new VM frame */` |
|     9838 |   535 | `		*ppFrame = pFrame;` |
|     4918 |   536 | `	}` |
|    11548 |   537 | `	return SXRET_OK;` |
|     5775 |   538 |  |
|        - |   539 | `/*` |
|        - |   540 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   541 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   542 | ` * information.` |
|        - |   543 | ` */` |
|       30 |   544 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        1 |   545 |  |
|        - |   546 | `	VmFrame *pTarget,*pFrame;` |
|       31 |   547 | `	SyHashEntry *pEntry = 0;` |
|        - |   548 | `	sxi32 rc;` |
|        - |   549 | `	/* Point to the upper frame */` |
|       31 |   550 | `	pFrame = pVm->pFrame;` |
|       31 |   551 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |   552 | `		/* Safely ignore the exception frame */` |
|      ! 0 |   553 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   554 | `	}` |
|       31 |   555 | `	pTarget = pFrame;` |
|       31 |   556 | `	pFrame = pTarget->pParent;` |
|       45 |   557 | `	while( pFrame ){` |
|       45 |   558 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   559 | `			/* Query the current frame */` |
|       31 |   560 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       31 |   561 | `			if( pEntry ){` |
|        - |   562 | `				/* Variable found */` |
|       31 |   563 | `				break;` |
|        - |   564 | `			}` |
|      ! 0 |   565 | `		}` |
|        - |   566 | `		/* Point to the upper frame */` |
|       15 |   567 | `		pFrame = pFrame->pParent;` |
|        1 |   568 | `	}` |
|       31 |   569 | `	if( pEntry == 0 ){` |
|        - |   570 | `		/* Inexistant variable */` |
|      ! 0 |   571 | `		return SXERR_NOTFOUND;` |
|        - |   572 | `	}` |
|        - |   573 | `	/* Link to the current frame */` |
|       31 |   574 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       31 |   575 | `	if( rc == SXRET_OK ){` |
|        - |   576 | `		sxu32 nIdx;` |
|       31 |   577 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       31 |   578 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       15 |   579 | `	}` |
|       31 |   580 | `	return rc;` |
|       16 |   581 |  |
|        - |   582 | `/*` |
|        - |   583 | ` * Leave the top-most active frame.` |
|        - |   584 | ` */` |
|     9834 |   585 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   586 |  |
|     9836 |   587 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|     9836 |   588 | `	if( pCurFrame ){` |
|        - |   589 | `		/* Unlink from the list of active VM frame */` |
|     9836 |   590 | `		pVm->pFrame = pCurFrame->pParent;` |
|     9836 |   591 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   592 | `			VmSlot  *aSlot;` |
|        - |   593 | `			sxu32 n;` |
|        - |   594 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|     9818 |   595 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    72000 |   596 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   597 | `				/* Unset the local variable */` |
|    62184 |   598 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    31093 |   599 | `			}` |
|        - |   600 | `			/* Remove local reference */` |
|     9818 |   601 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    72034 |   602 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    62218 |   603 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    31110 |   604 | `			}` |
|     4908 |   605 | `		}` |
|        - |   606 | `		/* Release internal containers */` |
|     9836 |   607 | `		SyHashRelease(&pCurFrame->hVar);` |
|     9836 |   608 | `		SySetRelease(&pCurFrame->sArg);` |
|     9836 |   609 | `		SySetRelease(&pCurFrame->sLocal);` |
|     9836 |   610 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   611 | `		/* Release the whole structure */` |
|     9836 |   612 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     4917 |   613 | `	}` |
|     9836 |   614 |  |
|        - |   615 | `/*` |
|        - |   616 | ` * Compare two functions signature and return the comparison result.` |
|        - |   617 | ` */` |
|      818 |   618 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   619 |  |
|      819 |   620 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      819 |   621 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      819 |   622 | `	const char *zSin = pSecond->zString;` |
|      819 |   623 | `	const char *zFin = pFirst->zString;` |
|      819 |   624 | `	const char *zPtr = zFin;` |
|      409 |   625 | `	for(;;){` |
|      819 |   626 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      410 |   627 | `			break;` |
|        - |   628 | `		}` |
|      ! 0 |   629 | `		if( zFin[0] != zSin[0] ){` |
|        - |   630 | `			/* mismatch */` |
|      ! 0 |   631 | `			break;` |
|        - |   632 | `		}` |
|      ! 0 |   633 | `		zFin++;` |
|      ! 0 |   634 | `		zSin++;` |
|      ! 0 |   635 | `	}` |
|      819 |   636 | `	return (int)(zFin-zPtr);` |
|        1 |   637 |  |
|        - |   638 | `/*` |
|        - |   639 | ` * Select the appropriate VM function for the current call context.` |
|        - |   640 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   641 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   642 | ` * Refer to the official documentation for more information.` |
|        - |   643 | ` */` |
|      122 |   644 | `static ph7_vm_func * VmOverload(` |
|        - |   645 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   646 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   647 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   648 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   649 | `	)` |
|        1 |   650 |  |
|        - |   651 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   652 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   653 | `	ph7_vm_func *pLink;` |
|        - |   654 | `	SyString sArgSig;` |
|        - |   655 | `	SyBlob sSig;` |
|        - |   656 |  |
|      123 |   657 | `	pLink = pList;` |
|      123 |   658 | `	i = 0;` |
|        - |   659 | `	/* Put functions expecting the same number of passed arguments */` |
|     1031 |   660 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|      969 |   661 | `		if( pLink == 0 ){` |
|       61 |   662 | `			break;` |
|        - |   663 | `		}` |
|      909 |   664 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   665 | `			/* Candidate for overloading */` |
|      863 |   666 | `			apSet[i++] = pLink;` |
|      431 |   667 | `		}` |
|        - |   668 | `		/* Point to the next entry */` |
|      909 |   669 | `		pLink = pLink->pNextName;` |
|        1 |   670 | `	}` |
|      123 |   671 | `	if( i < 1 ){` |
|        - |   672 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   673 | `		return pList;` |
|        - |   674 | `	}` |
|      123 |   675 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   676 | `		/* Return the only candidate */` |
|       21 |   677 | `		return apSet[0];` |
|        - |   678 | `	}` |
|        - |   679 | `	/* Calculate function signature */` |
|      103 |   680 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      355 |   681 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      253 |   682 | `		int c = 'n'; /* null */` |
|      253 |   683 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   684 | `			/* Hashmap */` |
|       45 |   685 | `			c = 'h';` |
|      231 |   686 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   687 | `			/* bool */` |
|      ! 0 |   688 | `			c = 'b';` |
|      209 |   689 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   690 | `			/* int */` |
|        5 |   691 | `			c = 'i';` |
|      207 |   692 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   693 | `			/* String */` |
|      105 |   694 | `			c = 's';` |
|      153 |   695 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   696 | `			/* Float */` |
|      ! 0 |   697 | `			c = 'f';` |
|      101 |   698 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   699 | `			/* Class instance */` |
|      ! 0 |   700 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|      ! 0 |   701 | `			SyString *pName = &pClass->sName;` |
|      ! 0 |   702 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|      ! 0 |   703 | `			c = -1;` |
|      ! 0 |   704 | `		}` |
|      253 |   705 | `		if( c > 0 ){` |
|      253 |   706 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      126 |   707 | `		}` |
|      127 |   708 | `	}` |
|      103 |   709 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      103 |   710 | `	iTarget = 0;` |
|      103 |   711 | `	iMax = -1;` |
|        - |   712 | `	/* Select the appropriate function */` |
|      921 |   713 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   714 | `		/* Compare the two signatures */` |
|      819 |   715 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      819 |   716 | `		if( iCur > iMax ){` |
|      103 |   717 | `			iMax = iCur;` |
|      103 |   718 | `			iTarget = j;` |
|       51 |   719 | `		}` |
|      410 |   720 | `	}` |
|      103 |   721 | `	SyBlobRelease(&sSig);` |
|        - |   722 | `	/* Appropriate function for the current call context */` |
|      103 |   723 | `	return apSet[iTarget];` |
|       62 |   724 |  |
|        - |   725 | `/* Forward declaration */` |
|        - |   726 | `static sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult);` |
|        - |   727 | `static sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...);` |
|        - |   728 | `/*` |
|        - |   729 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   730 | ` * it can be instanciated from the executed PHP script.` |
|        - |   731 | ` */` |
|    65798 |   732 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   733 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   734 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   735 | `	)` |
|        2 |   736 |  |
|        - |   737 | `	ph7_class_method *pMeth;` |
|        - |   738 | `	ph7_class_attr *pAttr;` |
|        - |   739 | `	SyHashEntry *pEntry;` |
|        - |   740 | `	sxi32 rc;` |
|        - |   741 | `	/* Reset the loop cursor */` |
|    65800 |   742 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   743 | `	/* Process only static and constant attribute */` |
|   226655 |   744 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   745 | `		/* Extract the current attribute */` |
|   127958 |   746 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   127958 |   747 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   748 | `			ph7_value *pMemObj;` |
|        - |   749 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1290 |   750 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1290 |   751 | `			if( pMemObj == 0 ){` |
|      ! 0 |   752 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   753 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   754 | `					&pClass->sName,&pAttr->sName` |
|        - |   755 | `					);` |
|      ! 0 |   756 | `				return SXERR_MEM;` |
|        - |   757 | `			}` |
|     1290 |   758 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   759 | `				/* Initialize attribute default value (any complex expression) */` |
|     1290 |   760 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      644 |   761 | `			}` |
|        - |   762 | `			/* Record attribute index */` |
|     1290 |   763 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   764 | `			/* Install static attribute in the reference table */` |
|     1290 |   765 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      644 |   766 | `		}` |
|        2 |   767 | `	}` |
|        - |   768 | `	/* Install class methods */` |
|    65800 |   769 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |   770 | `		/* Do not mount interface methods since they are signatures only.` |
|        - |   771 | `		 */` |
|    40274 |   772 | `		return SXRET_OK;` |
|        - |   773 | `	}` |
|        - |   774 | `	/* Create constructor alias if not yet done */` |
|    25528 |   775 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   776 | `		/* User constructor with the same base class name */` |
|      202 |   777 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      202 |   778 | `		if( pEntry ){` |
|      ! 0 |   779 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   780 | `			/* Create the alias */` |
|      ! 0 |   781 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   782 | `		}` |
|      100 |   783 | `	}` |
|        - |   784 | `	/* Install the methods now */` |
|    25528 |   785 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   244057 |   786 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   205768 |   787 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   205768 |   788 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   205762 |   789 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   205762 |   790 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   791 | `				return rc;` |
|        - |   792 | `			}` |
|   102880 |   793 | `		}` |
|        2 |   794 | `	}` |
|        - |   795 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    25528 |   796 | `	pClass->bMounted = TRUE;` |
|    25528 |   797 | `	return SXRET_OK;` |
|    32901 |   798 |  |
|        - |   799 | `/*` |
|        - |   800 | ` * Allocate a private frame for attributes of the given` |
|        - |   801 | ` * class instance (Object in the PHP jargon).` |
|        - |   802 | ` */` |
|      812 |   803 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   804 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   805 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   806 | `	)` |
|        2 |   807 |  |
|      814 |   808 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   809 | `	ph7_class_attr *pAttr;` |
|        - |   810 | `	SyHashEntry *pEntry;` |
|        - |   811 | `	sxi32 rc;` |
|        - |   812 | `	/* Install class attribute in the private frame associated with this instance */` |
|      814 |   813 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     3000 |   814 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   815 | `		VmClassAttr *pVmAttr;` |
|        - |   816 | `		/* Extract the current attribute */` |
|     2188 |   817 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     2188 |   818 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     2188 |   819 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   820 | `			return SXERR_MEM;` |
|        - |   821 | `		}` |
|     2188 |   822 | `		pVmAttr->pAttr = pAttr;` |
|     2188 |   823 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   824 | `			ph7_value *pMemObj;` |
|        - |   825 | `			/* Reserve a memory object for this attribute */` |
|     2182 |   826 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     2182 |   827 | `			if( pMemObj == 0 ){` |
|      ! 0 |   828 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   829 | `				return SXERR_MEM;` |
|        - |   830 | `			}` |
|     2182 |   831 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     2182 |   832 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   833 | `				/* Initialize attribute default value (any complex expression) */` |
|      704 |   834 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      351 |   835 | `			}` |
|     2182 |   836 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     2182 |   837 | `			if( rc != SXRET_OK ){` |
|        - |   838 | `				VmSlot sSlot;` |
|        - |   839 | `				/* Restore memory object */` |
|      ! 0 |   840 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   841 | `				sSlot.pUserData = 0;` |
|      ! 0 |   842 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   843 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   844 | `				return SXERR_MEM;` |
|        - |   845 | `			}` |
|        - |   846 | `			/* Install attribute in the reference table */` |
|     2182 |   847 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1092 |   848 | `		}else{` |
|        - |   849 | `			/* Install static/constant attribute */` |
|        8 |   850 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   851 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   852 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   853 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   854 | `				return SXERR_MEM;` |
|        - |   855 | `			}` |
|        - |   856 | `		}` |
|        2 |   857 | `	}` |
|      814 |   858 | `	return SXRET_OK;` |
|      408 |   859 |  |
|        - |   860 | `/* Forward declaration */` |
|        - |   861 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   862 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   863 | `/*` |
|        - |   864 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   865 | ` */` |
|        - |   866 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   867 | `/*` |
|        - |   868 | ` * Reserve a constant memory object.` |
|        - |   869 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   870 | ` */` |
|   211342 |   871 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   872 |  |
|        - |   873 | `	ph7_value *pObj;` |
|        - |   874 | `	sxi32 rc;` |
|   211344 |   875 | `	if( pIndex ){` |
|        - |   876 | `		/* Object index in the object table */` |
|   206214 |   877 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   103106 |   878 | `	}` |
|        - |   879 | `	/* Reserve a slot for the new object */` |
|   211344 |   880 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   211344 |   881 | `	if( rc != SXRET_OK ){` |
|        - |   882 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   883 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   884 | `		 */` |
|      ! 0 |   885 | `		return 0;` |
|        - |   886 | `	}` |
|   211344 |   887 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   211344 |   888 | `	return pObj;` |
|   105673 |   889 |  |
|        - |   890 | `/*` |
|        - |   891 | ` * Reserve a memory object.` |
|        - |   892 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   893 | ` */` |
|  2124686 |   894 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   895 |  |
|        - |   896 | `	ph7_value *pObj;` |
|        - |   897 | `	sxi32 rc;` |
|  2124688 |   898 | `	if( pIndex ){` |
|        - |   899 | `		/* Object index in the object table */` |
|  2124688 |   900 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1062343 |   901 | `	}` |
|        - |   902 | `	/* Reserve a slot for the new object */` |
|  2124688 |   903 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2124688 |   904 | `	if( rc != SXRET_OK ){` |
|        - |   905 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   906 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   907 | `		 */` |
|      ! 0 |   908 | `		return 0;` |
|        - |   909 | `	}` |
|  2124688 |   910 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2124688 |   911 | `	return pObj;` |
|  1062345 |   912 |  |
|        - |   913 | `/* Forward declaration */` |
|        - |   914 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   915 | `/*` |
|        - |   916 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   917 | ` * directly as foreign functions.` |
|        - |   918 | ` */` |
|        - |   919 | `#define PH7_BUILTIN_LIB \` |
|        - |   920 | `	"class Exception { "\` |
|        - |   921 | `    "protected $message = 'Unknown exception';"\` |
|        - |   922 | `    "protected $code = 0;"\` |
|        - |   923 | `    "protected $file;"\` |
|        - |   924 | `    "protected $line;"\` |
|        - |   925 | `    "protected $trace;"\` |
|        - |   926 | `    "protected $previous;"\` |
|        - |   927 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   928 | `	"   if( isset($message) ){"\` |
|        - |   929 | `	"	  $this->message = $message;"\` |
|        - |   930 | `	"   }"\` |
|        - |   931 | `	"   $this->code = $code;"\` |
|        - |   932 | `	"   $this->file = __FILE__;"\` |
|        - |   933 | `	"   $this->line = __LINE__;"\` |
|        - |   934 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   935 | `	"   if( isset($previous) ){"\` |
|        - |   936 | `	"     $this->previous = $previous;"\` |
|        - |   937 | `	"   }"\` |
|        - |   938 | `	"}"\` |
|        - |   939 | `	"public function getMessage(){"\` |
|        - |   940 | `	"   return $this->message;"\` |
|        - |   941 | `	"}"\` |
|        - |   942 | `	" public function getCode(){"\` |
|        - |   943 | `	"  return $this->code;"\` |
|        - |   944 | `	"}"\` |
|        - |   945 | `	"public function getFile(){"\` |
|        - |   946 | `	"  return $this->file;"\` |
|        - |   947 | `	"}"\` |
|        - |   948 | `	"public function getLine(){"\` |
|        - |   949 | `	"  return $this->line;"\` |
|        - |   950 | `	"}"\` |
|        - |   951 | `	"public function getTrace(){"\` |
|        - |   952 | `	"   return $this->trace;"\` |
|        - |   953 | `	"}"\` |
|        - |   954 | `	"public function getTraceAsString(){"\` |
|        - |   955 | `	"  return debug_string_backtrace();"\` |
|        - |   956 | `	"}"\` |
|        - |   957 | `	"public function getPrevious(){"\` |
|        - |   958 | `	"    return $this->previous;"\` |
|        - |   959 | `	"}"\` |
|        - |   960 | `	"public function __toString(){"\` |
|        - |   961 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   962 | `    "}"\` |
|        - |   963 | `	"}"\` |
|        - |   964 | `	"class Error extends Exception { }"\` |
|        - |   965 | `	"class TypeError extends Error { }"\` |
|        - |   966 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   967 | `	"class ValueError extends Error { }"\` |
|        - |   968 | `	"class ErrorException extends Exception { "\` |
|        - |   969 | `	"protected $severity;"\` |
|        - |   970 | `	"public function __construct(string $message = null,"\` |
|        - |   971 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   972 | `	"   if( isset($message) ){"\` |
|        - |   973 | `	"	  $this->message = $message;"\` |
|        - |   974 | `	"   }"\` |
|        - |   975 | `	"   $this->severity = $severity;"\` |
|        - |   976 | `	"   $this->code = $code;"\` |
|        - |   977 | `	"   $this->file = $filename;"\` |
|        - |   978 | `	"   $this->line = $lineno;"\` |
|        - |   979 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   980 | `	"   if( isset($previous) ){"\` |
|        - |   981 | `	"     $this->previous = $previous;"\` |
|        - |   982 | `	"   }"\` |
|        - |   983 | `	"}"\` |
|        - |   984 | `	"public function getSeverity(){"\` |
|        - |   985 | `	"   return $this->severity;"\` |
|        - |   986 | `    "}"\` |
|        - |   987 | `	"}"\` |
|        - |   988 | `	"interface Iterator {"\` |
|        - |   989 | `	"public function current();"\` |
|        - |   990 | `	"public function key();"\` |
|        - |   991 | `	"public function next();"\` |
|        - |   992 | `	"public function rewind();"\` |
|        - |   993 | `	"public function valid();"\` |
|        - |   994 | `	"}"\` |
|        - |   995 | `	"interface IteratorAggregate {"\` |
|        - |   996 | `	"public function getIterator();"\` |
|        - |   997 | `	"}"\` |
|        - |   998 | `	"interface Serializable {"\` |
|        - |   999 | `	"public function serialize();"\` |
|        - |  1000 | `	"public function unserialize(string $serialized);"\` |
|        - |  1001 | `	"}"\` |
|        - |  1002 | `	"/* Directory releated IO */"\` |
|        - |  1003 | `	"class Directory {"\` |
|        - |  1004 | `	"public $handle = null;"\` |
|        - |  1005 | `	"public $path  = null;"\` |
|        - |  1006 | `	"public function __construct(string $path)"\` |
|        - |  1007 | `	"{"\` |
|        - |  1008 | `	"   $this->handle = opendir($path);"\` |
|        - |  1009 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1010 | `	"      $this->path = $path;"\` |
|        - |  1011 | `	"   }"\` |
|        - |  1012 | `	"}"\` |
|        - |  1013 | `	"public function __destruct()"\` |
|        - |  1014 | `	"{"\` |
|        - |  1015 | `	"  if( $this->handle != null ){"\` |
|        - |  1016 | `	"       closedir($this->handle);"\` |
|        - |  1017 | `	"  }"\` |
|        - |  1018 | `	"}"\` |
|        - |  1019 | `	"public function read()"\` |
|        - |  1020 | `	"{"\` |
|        - |  1021 | `	"    return readdir($this->handle);"\` |
|        - |  1022 | `	"}"\` |
|        - |  1023 | `	"public function rewind()"\` |
|        - |  1024 | `	"{"\` |
|        - |  1025 | `	"    rewinddir($this->handle);"\` |
|        - |  1026 | `	"}"\` |
|        - |  1027 | `	"public function close()"\` |
|        - |  1028 | `	"{"\` |
|        - |  1029 | `	"    closedir($this->handle);"\` |
|        - |  1030 | `	"    $this->handle = null;"\` |
|        - |  1031 | `	"}"\` |
|        - |  1032 | `	"}"\` |
|        - |  1033 | `	"class stdClass{"\` |
|        - |  1034 | `	"  public $value;"\` |
|        - |  1035 | `	" /* Magic methods */"\` |
|        - |  1036 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1037 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1038 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1039 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1040 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1041 | `	"}"\` |
|        - |  1042 | `	"function dir(string $path){"\` |
|        - |  1043 | `	"   return new Directory($path);"\` |
|        - |  1044 | `	"}"\` |
|        - |  1045 | `	"function Dir(string $path){"\` |
|        - |  1046 | `	"   return new Directory($path);"\` |
|        - |  1047 | `	"}"\` |
|        - |  1048 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1049 | `    "{"\` |
|        - |  1050 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1051 | `	"  $aDir = array();"\` |
|        - |  1052 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1053 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1054 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1055 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1056 | `	"   }"\` |
|        - |  1057 | `	"  closedir($pHandle);"\` |
|        - |  1058 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1059 | `	"      rsort($aDir);"\` |
|        - |  1060 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1061 | `	"      sort($aDir);"\` |
|        - |  1062 | `	"  }"\` |
|        - |  1063 | `	"  return $aDir;"\` |
|        - |  1064 | `	"}"\` |
|        - |  1065 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1066 | `	"/* Open the target directory */"\` |
|        - |  1067 | `	"$zDir = dirname($pattern);"\` |
|        - |  1068 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1069 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1070 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1071 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1072 | `	"	return FALSE;"\` |
|        - |  1073 | `	"}"\` |
|        - |  1074 | `	"$pattern = basename($pattern);"\` |
|        - |  1075 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1076 | `	"/* Loop throw available entries */"\` |
|        - |  1077 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1078 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1079 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1080 | `	"	if( $rc ){"\` |
|        - |  1081 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1082 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1083 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1084 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1085 | `	"		  }"\` |
|        - |  1086 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1087 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1088 | `	"		 continue;"\` |
|        - |  1089 | `	"	   }"\` |
|        - |  1090 | `	"	   /* Add the entry */"\` |
|        - |  1091 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1092 | `	"	}"\` |
|        - |  1093 | `	" }"\` |
|        - |  1094 | `	"/* Close the handle */"\` |
|        - |  1095 | `	"closedir($pHandle);"\` |
|        - |  1096 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1097 | `	"  /* Sort the array */"\` |
|        - |  1098 | `	"  sort($pArray);"\` |
|        - |  1099 | `	"}"\` |
|        - |  1100 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1101 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1102 | `	"  $pArray[] = $pattern;"\` |
|        - |  1103 | `	"}"\` |
|        - |  1104 | `	"/* Return the created array */"\` |
|        - |  1105 | `	"return $pArray;"\` |
|        - |  1106 | `   "}"\` |
|        - |  1107 | `   "/* Creates a temporary file */"\` |
|        - |  1108 | `   "function tmpfile(){"\` |
|        - |  1109 | `   "  /* Extract the temp directory */"\` |
|        - |  1110 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1111 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1112 | `   "    /* Use the current dir */"\` |
|        - |  1113 | `   "    $zTempDir = '.';"\` |
|        - |  1114 | `   "  }"\` |
|        - |  1115 | `   "  /* Create the file */"\` |
|        - |  1116 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1117 | `   "  return $pHandle;"\` |
|        - |  1118 | `   "}"\` |
|        - |  1119 | `   "/* Creates a temporary filename */"\` |
|        - |  1120 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1121 | `   "{"\` |
|        - |  1122 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1123 | `   "}"\` |
|        - |  1124 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1125 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1126 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1127 | `   "/* Copy arguments */"\` |
|        - |  1128 | `   "$nArgs = func_num_args();"\` |
|        - |  1129 | `   "$pNew = array();"\` |
|        - |  1130 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1131 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1132 | `    "}"\` |
|        - |  1133 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1134 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1135 | `	"/* Erase */"\` |
|        - |  1136 | `	"array_erase($pArray);"\` |
|        - |  1137 | `	"/* Unshift */"\` |
|        - |  1138 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1139 | `	"return sizeof($pArray);"\` |
|        - |  1140 | `    "}"\` |
|        - |  1141 | `	"function array_merge_recursive($array1, $array2){"\` |
|        - |  1142 | `	"if( func_num_args() < 1 ){ return NULL; }"\` |
|        - |  1143 | `    "$arrays = func_get_args();"\` |
|        - |  1144 | `    "$narrays = count($arrays);"\` |
|        - |  1145 | `    "$ret = $arrays[0];"\` |
|        - |  1146 | `    "for ($i = 1; $i < $narrays; $i++) {"\` |
|        - |  1147 | `	 " if( array_same($ret,$arrays[$i]) ){ /* Same instance */continue;}"\` |
|        - |  1148 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1149 | `     "  if (((string) $key) === ((string) intval($key))) {"\` |
|        - |  1150 | `     "   $ret[] = $value;"\` |
|        - |  1151 | `     "  }else{"\` |
|        - |  1152 | `     "  if (is_array($value) && isset($ret[$key]) ) {"\` |
|        - |  1153 | `     "   $ret[$key] = array_merge_recursive($ret[$key], $value);"\` |
|        - |  1154 | `     " }else {"\` |
|        - |  1155 | `     "   $ret[$key] = $value;"\` |
|        - |  1156 | `     "  }"\` |
|        - |  1157 | `     " }"\` |
|        - |  1158 | `     " }"\` |
|        - |  1159 | `	 "}"\` |
|        - |  1160 | `	 " return $ret;"\` |
|        - |  1161 | `    "}"\` |
|        - |  1162 | `	"function max(){"\` |
|        - |  1163 | `    "  $pArgs = func_get_args();"\` |
|        - |  1164 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1165 | `	"  return null;"\` |
|        - |  1166 | `    " }"\` |
|        - |  1167 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1168 | `    " $pArg = $pArgs[0];"\` |
|        - |  1169 | `	" if( !is_array($pArg) ){"\` |
|        - |  1170 | `	"   return $pArg; "\` |
|        - |  1171 | `	" }"\` |
|        - |  1172 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1173 | `	"   return null;"\` |
|        - |  1174 | `	" }"\` |
|        - |  1175 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1176 | `	" reset($pArg);"\` |
|        - |  1177 | `	" $max = current($pArg);"\` |
|        - |  1178 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1179 | `	"   if( $val > $max ){"\` |
|        - |  1180 | `	"     $max = $val;"\` |
|        - |  1181 | `    " }"\` |
|        - |  1182 | `	" }"\` |
|        - |  1183 | `	" return $max;"\` |
|        - |  1184 | `    " }"\` |
|        - |  1185 | `    " $max = $pArgs[0];"\` |
|        - |  1186 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1187 | `    " $val = $pArgs[$i];"\` |
|        - |  1188 | `	"if( $val > $max ){"\` |
|        - |  1189 | `	" $max = $val;"\` |
|        - |  1190 | `	"}"\` |
|        - |  1191 | `    " }"\` |
|        - |  1192 | `	" return $max;"\` |
|        - |  1193 | `    "}"\` |
|        - |  1194 | `	"function min(){"\` |
|        - |  1195 | `    "  $pArgs = func_get_args();"\` |
|        - |  1196 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1197 | `	"  return null;"\` |
|        - |  1198 | `    " }"\` |
|        - |  1199 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1200 | `    " $pArg = $pArgs[0];"\` |
|        - |  1201 | `	" if( !is_array($pArg) ){"\` |
|        - |  1202 | `	"   return $pArg; "\` |
|        - |  1203 | `	" }"\` |
|        - |  1204 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1205 | `	"   return null;"\` |
|        - |  1206 | `	" }"\` |
|        - |  1207 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1208 | `	" reset($pArg);"\` |
|        - |  1209 | `	" $min = current($pArg);"\` |
|        - |  1210 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1211 | `	"   if( $val < $min ){"\` |
|        - |  1212 | `	"     $min = $val;"\` |
|        - |  1213 | `    " }"\` |
|        - |  1214 | `	" }"\` |
|        - |  1215 | `	" return $min;"\` |
|        - |  1216 | `    " }"\` |
|        - |  1217 | `    " $min = $pArgs[0];"\` |
|        - |  1218 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1219 | `    " $val = $pArgs[$i];"\` |
|        - |  1220 | `	"if( $val < $min ){"\` |
|        - |  1221 | `	" $min = $val;"\` |
|        - |  1222 | `	" }"\` |
|        - |  1223 | `    " }"\` |
|        - |  1224 | `	" return $min;"\` |
|        - |  1225 | `	"}"\` |
|        - |  1226 | `	"function fileowner(string $file){"\` |
|        - |  1227 | `    " $a = stat($file);"\` |
|        - |  1228 | `	" if( !is_array($a) ){"\` |
|        - |  1229 | `	"	return false;"\` |
|        - |  1230 | `	" }"\` |
|        - |  1231 | `	" return $a['uid'];"\` |
|        - |  1232 | `    "}"\` |
|        - |  1233 | `    "function filegroup(string $file){"\` |
|        - |  1234 | `	" $a = stat($file);"\` |
|        - |  1235 | `	" if( !is_array($a) ){"\` |
|        - |  1236 | `	"	return false;"\` |
|        - |  1237 | `	" }"\` |
|        - |  1238 | `	" return $a['gid'];"\` |
|        - |  1239 | `    "}"\` |
|        - |  1240 | `	 "function fileinode(string $file){"\` |
|        - |  1241 | `	" $a = stat($file);"\` |
|        - |  1242 | `	" if( !is_array($a) ){"\` |
|        - |  1243 | `	"	return false;"\` |
|        - |  1244 | `	" }"\` |
|        - |  1245 | `	" return $a['ino'];"\` |
|        - |  1246 | `    "}"` |
|        - |  1247 |  |
|        - |  1248 | `/*` |
|        - |  1249 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1250 | ` * start compiling the target PHP program.` |
|        - |  1251 | ` */` |
|     1710 |  1252 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1253 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1254 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1255 | `	 )` |
|        2 |  1256 |  |
|        - |  1257 | `	SyString sBuiltin;` |
|        - |  1258 | `	ph7_value *pObj;` |
|        - |  1259 | `	sxi32 rc;` |
|        - |  1260 | `	/* Zero the structure */` |
|     1712 |  1261 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1262 | `	/* Initialize VM fields */` |
|     1712 |  1263 | `	pVm->pEngine = &(*pEngine);` |
|     1712 |  1264 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1265 | `	/* Instructions containers */` |
|     1712 |  1266 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     1712 |  1267 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     1712 |  1268 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1269 | `	/* Object containers */` |
|     1712 |  1270 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1712 |  1271 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1272 | `	/* Virtual machine internal containers */` |
|     1712 |  1273 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     1712 |  1274 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     1712 |  1275 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     1712 |  1276 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1712 |  1277 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     1712 |  1278 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     1712 |  1279 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     1712 |  1280 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     1712 |  1281 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     1712 |  1282 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     1712 |  1283 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     1712 |  1284 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     1712 |  1285 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     1712 |  1286 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     1712 |  1287 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|        - |  1288 | `	/* Configuration containers */` |
|     1712 |  1289 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     1712 |  1290 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     1712 |  1291 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     1712 |  1292 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     1712 |  1293 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1294 | `	/* Error callbacks containers */` |
|     1712 |  1295 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     1712 |  1296 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     1712 |  1297 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     1712 |  1298 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     1712 |  1299 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1300 | `	/* Set a default recursion limit */` |
|        - |  1301 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     1712 |  1302 | `	pVm->nMaxDepth = 32;` |
|        - |  1303 | `#else` |
|        - |  1304 | `	pVm->nMaxDepth = 16;` |
|        - |  1305 | `#endif` |
|        - |  1306 | `	/* Default assertion flags */` |
|     1712 |  1307 | `	pVm->iAssertFlags = PH7_ASSERT_WARNING; /* Issue a warning for each failed assertion */` |
|        - |  1308 | `	/* JSON return status */` |
|     1712 |  1309 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1310 | `	/* PRNG context */` |
|     1712 |  1311 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1312 | `	/* Install the null constant */` |
|     1712 |  1313 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1712 |  1314 | `	if( pObj == 0 ){` |
|      ! 0 |  1315 | `		rc = SXERR_MEM;` |
|      ! 0 |  1316 | `		goto Err;` |
|        - |  1317 | `	}` |
|     1712 |  1318 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1319 | `	/* Install the boolean TRUE constant */` |
|     1712 |  1320 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1712 |  1321 | `	if( pObj == 0 ){` |
|      ! 0 |  1322 | `		rc = SXERR_MEM;` |
|      ! 0 |  1323 | `		goto Err;` |
|        - |  1324 | `	}` |
|     1712 |  1325 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1326 | `	/* Install the boolean FALSE constant */` |
|     1712 |  1327 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1712 |  1328 | `	if( pObj == 0 ){` |
|      ! 0 |  1329 | `		rc = SXERR_MEM;` |
|      ! 0 |  1330 | `		goto Err;` |
|        - |  1331 | `	}` |
|     1712 |  1332 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1333 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1334 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1335 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     1712 |  1336 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     1712 |  1337 | `	if( pObj == 0 ){` |
|      ! 0 |  1338 | `		rc = SXERR_MEM;` |
|      ! 0 |  1339 | `		goto Err;` |
|        - |  1340 | `	}` |
|     1712 |  1341 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1342 | `	/* Create the global frame */` |
|     1712 |  1343 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     1712 |  1344 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1345 | `		goto Err;` |
|        - |  1346 | `	}` |
|        - |  1347 | `	/* Initialize the code generator */` |
|     1712 |  1348 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1712 |  1349 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1350 | `		goto Err;` |
|        - |  1351 | `	}` |
|        - |  1352 | `	/* VM correctly initialized,set the magic number */` |
|     1712 |  1353 | `	pVm->nMagic = PH7_VM_INIT;` |
|     1712 |  1354 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1355 | `	/* Compile the built-in library */` |
|     1712 |  1356 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1357 | `	/* Reset the code generator */` |
|     1712 |  1358 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1712 |  1359 | `	return SXRET_OK;` |
|      ! 0 |  1360 | `Err:` |
|      ! 0 |  1361 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1362 | `	return rc;` |
|      857 |  1363 |  |
|        - |  1364 | `/*` |
|        - |  1365 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1366 | ` * routine which store the output in an internal blob.` |
|        - |  1367 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1368 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1369 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1370 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1371 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1372 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1373 | ` * to finish executing and extracting the output.` |
|        - |  1374 | ` */` |
|      ! 0 |  1375 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1376 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1377 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1378 | `	void *pUserData     /* User private data */` |
|        - |  1379 | `	)` |
|      ! 0 |  1380 |  |
|        - |  1381 | `	 sxi32 rc;` |
|        - |  1382 | `	 /* Store the output in an internal BLOB */` |
|      ! 0 |  1383 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|      ! 0 |  1384 | `	 return rc;` |
|      ! 0 |  1385 |  |
|        - |  1386 | `#define VM_STACK_GUARD 16` |
|        - |  1387 | `/*` |
|        - |  1388 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1389 | ` * our compiled PHP program.` |
|        - |  1390 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1391 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1392 | ` */` |
|    24706 |  1393 | `static ph7_value * VmNewOperandStack(` |
|        - |  1394 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1395 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1396 | `	)` |
|        2 |  1397 |  |
|        - |  1398 | `	ph7_value *pStack;` |
|        - |  1399 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1400 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1401 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1402 | `  ** on the maximum stack depth required.` |
|        - |  1403 | `  **` |
|        - |  1404 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1405 | `  */` |
|    24708 |  1406 | `	nInstr += VM_STACK_GUARD;` |
|    24708 |  1407 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    24708 |  1408 | `	if( pStack == 0 ){` |
|      ! 0 |  1409 | `		return 0;` |
|        - |  1410 | `	}` |
|        - |  1411 | `	/* Initialize the operand stack */` |
|  1564534 |  1412 | `	while( nInstr > 0 ){` |
|  1539828 |  1413 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1539828 |  1414 | `		--nInstr;` |
|        2 |  1415 | `	}` |
|        - |  1416 | `	/* Ready for bytecode execution */` |
|    24708 |  1417 | `	return pStack;` |
|    12355 |  1418 |  |
|        - |  1419 | `/* Forward declaration */` |
|        - |  1420 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1421 | `static int VmInstanceOf(ph7_class *pThis,ph7_class *pClass);` |
|        - |  1422 | `static int VmClassMemberAccess(ph7_vm *pVm,ph7_class *pClass,const SyString *pAttrName,sxi32 iProtection,int bLog);` |
|        - |  1423 | `/*` |
|        - |  1424 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1425 | ` * This routine gets called by the PH7 engine after` |
|        - |  1426 | ` * successful compilation of the target PHP program.` |
|        - |  1427 | ` */` |
|     1452 |  1428 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1429 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1430 | `	)` |
|        2 |  1431 |  |
|        - |  1432 | `	SyHashEntry *pEntry;` |
|        - |  1433 | `	sxi32 rc;` |
|     1454 |  1434 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1435 | `		/* Initialize your VM first */` |
|      ! 0 |  1436 | `		return SXERR_CORRUPT;` |
|        - |  1437 | `	}` |
|        - |  1438 | `	/* Mark the VM ready for byte-code execution */` |
|     1454 |  1439 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1440 | `	/* Release the code generator now we have compiled our program */` |
|     1454 |  1441 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1442 | `	/* Emit the DONE instruction */` |
|     1454 |  1443 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     1454 |  1444 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1445 | `		return SXERR_MEM;` |
|        - |  1446 | `	}` |
|        - |  1447 | `	/* Script return value */` |
|     1454 |  1448 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1449 | `	/* Allocate a new operand stack */` |
|     1454 |  1450 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     1454 |  1451 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1452 | `		return SXERR_MEM;` |
|        - |  1453 | `	}` |
|        - |  1454 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1455 | `	 * private data. */` |
|     1454 |  1456 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     1454 |  1457 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1458 | `	/* Allocate the reference table */` |
|     1454 |  1459 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     1454 |  1460 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     1454 |  1461 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1462 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1463 | `		return SXERR_MEM;` |
|        - |  1464 | `	}` |
|        - |  1465 | `	/* Zero the reference table */` |
|     1454 |  1466 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1467 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     1454 |  1468 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     1454 |  1469 | `	if( rc != SXRET_OK ){` |
|        - |  1470 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1471 | `		return rc;` |
|        - |  1472 | `	}` |
|        - |  1473 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     1454 |  1474 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     1454 |  1475 | `	if( rc != SXRET_OK ){` |
|        - |  1476 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1477 | `		return rc;` |
|        - |  1478 | `	}` |
|        - |  1479 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     1454 |  1480 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1481 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     1454 |  1482 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1483 | `	/* Initialize and install static and constants class attributes */` |
|     1454 |  1484 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    17452 |  1485 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    16000 |  1486 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    16000 |  1487 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1488 | `			return rc;` |
|        - |  1489 | `		}` |
|        2 |  1490 | `	}` |
|        - |  1491 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     1454 |  1492 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1493 | `	/* VM is ready for bytecode execution */` |
|     1454 |  1494 | `	return SXRET_OK;` |
|      728 |  1495 |  |
|        - |  1496 | `/*` |
|        - |  1497 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1498 | ` */` |
|      ! 0 |  1499 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1500 |  |
|      ! 0 |  1501 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1502 | `		return SXERR_CORRUPT;` |
|        - |  1503 | `	}` |
|        - |  1504 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1505 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1506 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1507 | `	/* Set the ready flag */` |
|      ! 0 |  1508 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1509 | `	return SXRET_OK;` |
|      ! 0 |  1510 |  |
|        - |  1511 | `/*` |
|        - |  1512 | ` * Release a Virtual Machine.` |
|        - |  1513 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1514 | ` */` |
|     1444 |  1515 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1516 |  |
|        - |  1517 | `	/* Set the stale magic number */` |
|     1446 |  1518 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1519 | `	/* Release the private memory subsystem */` |
|     1446 |  1520 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     1446 |  1521 | `	return SXRET_OK;` |
|        2 |  1522 |  |
|        - |  1523 | `/*` |
|        - |  1524 | ` * Initialize a foreign function call context.` |
|        - |  1525 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1526 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1527 | ` * functions.` |
|        - |  1528 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1529 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1530 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1531 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1532 | ` */` |
|   494492 |  1533 | `static sxi32 VmInitCallContext(` |
|        - |  1534 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1535 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1536 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1537 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1538 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1539 | `	)` |
|        2 |  1540 |  |
|   494494 |  1541 | `	pOut->pFunc = pFunc;` |
|   494494 |  1542 | `	pOut->pVm   = pVm;` |
|   494494 |  1543 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   494494 |  1544 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1545 | `	/* Assume a null return value */` |
|   494494 |  1546 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   494494 |  1547 | `	pOut->pRet = pRet;` |
|   494494 |  1548 | `	pOut->iFlags = iFlags;` |
|   494494 |  1549 | `	return SXRET_OK;` |
|        2 |  1550 |  |
|        - |  1551 | `/*` |
|        - |  1552 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1553 | ` * left behind.` |
|        - |  1554 | ` */` |
|   494492 |  1555 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1556 |  |
|        - |  1557 | `	sxu32 n;` |
|   494494 |  1558 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     5740 |  1559 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    16134 |  1560 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    10396 |  1561 | `			if( apObj[n] == 0 ){` |
|        - |  1562 | `				/* Already released */` |
|      250 |  1563 | `				continue;` |
|        - |  1564 | `			}` |
|    10148 |  1565 | `			PH7_MemObjRelease(apObj[n]);` |
|    10148 |  1566 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     5075 |  1567 | `		}` |
|     5740 |  1568 | `		SySetRelease(&pCtx->sVar);` |
|     2869 |  1569 | `	}` |
|   494494 |  1570 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1571 | `		ph7_aux_data *aAux;` |
|        - |  1572 | `		void *pChunk;` |
|        - |  1573 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1574 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1575 | `		 */` |
|        9 |  1576 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1577 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1578 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1579 | `			/* Release the chunk */` |
|       25 |  1580 | `			if( pChunk ){` |
|       25 |  1581 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1582 | `			}` |
|       13 |  1583 | `		}` |
|        9 |  1584 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1585 | `	}` |
|   494494 |  1586 |  |
|        - |  1587 | `/*` |
|        - |  1588 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1589 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1590 | ` */` |
|      248 |  1591 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1592 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1593 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1594 | `	)` |
|        2 |  1595 |  |
|      250 |  1596 | `	if( pValue == 0 ){` |
|        - |  1597 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1598 | `		return;` |
|        - |  1599 | `	}` |
|      250 |  1600 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      250 |  1601 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1602 | `		sxu32 n;` |
|      936 |  1603 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      936 |  1604 | `			if( apObj[n] == pValue ){` |
|      250 |  1605 | `				PH7_MemObjRelease(pValue);` |
|      250 |  1606 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1607 | `				/* Mark as released */` |
|      250 |  1608 | `				apObj[n] = 0;` |
|      250 |  1609 | `				break;` |
|        - |  1610 | `			}` |
|      345 |  1611 | `		}` |
|      124 |  1612 | `	}` |
|      126 |  1613 |  |
|        - |  1614 | `/*` |
|        - |  1615 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1616 | ` */` |
|  2866570 |  1617 | `static void VmPopOperand(` |
|        - |  1618 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1619 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1620 | `	)` |
|        2 |  1621 |  |
|  2866572 |  1622 | `	ph7_value *pTos = *ppTos;` |
|  6087674 |  1623 | `	while( nPop > 0 ){` |
|  3221104 |  1624 | `		PH7_MemObjRelease(pTos);` |
|  3221104 |  1625 | `		pTos--;` |
|  3221104 |  1626 | `		nPop--;` |
|        2 |  1627 | `	}` |
|        - |  1628 | `	/* Top of the stack */` |
|  2866572 |  1629 | `	*ppTos = pTos;` |
|  2866572 |  1630 |  |
|        - |  1631 | `/*` |
|        - |  1632 | ` * Reserve a memory object.` |
|        - |  1633 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1634 | ` */` |
|  2879128 |  1635 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1636 |  |
|  2879130 |  1637 | `	ph7_value *pObj = 0;` |
|        - |  1638 | `	VmSlot *pSlot;` |
|        - |  1639 | `	sxu32 nIdx;` |
|        - |  1640 | `	/* Check for a free slot */` |
|  2879130 |  1641 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  2879130 |  1642 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  2879130 |  1643 | `	if( pSlot ){` |
|   754444 |  1644 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   754444 |  1645 | `		nIdx = pSlot->nIdx;` |
|   377221 |  1646 | `	}` |
|  2879130 |  1647 | `	if( pObj == 0 ){` |
|        - |  1648 | `		/* Reserve a new memory object */` |
|  2124688 |  1649 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2124688 |  1650 | `		if( pObj == 0 ){` |
|      ! 0 |  1651 | `			return 0;` |
|        - |  1652 | `		}` |
|  1062343 |  1653 | `	}` |
|        - |  1654 | `	/* Set a null default value */` |
|  2879130 |  1655 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  2879130 |  1656 | `	pObj->nIdx = nIdx;` |
|  2879130 |  1657 | `	return pObj;` |
|  1439566 |  1658 |  |
|        - |  1659 | `/*` |
|        - |  1660 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1661 | ` */` |
|    19756 |  1662 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1663 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1664 | `	const char *zKey,  /* Entry key */` |
|        - |  1665 | `	sxu32 nByte,       /* Key length */` |
|        - |  1666 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1667 | `	)` |
|        2 |  1668 |  |
|        - |  1669 | `	ph7_value sKey;` |
|        - |  1670 | `	sxi32 rc;` |
|    19758 |  1671 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    19758 |  1672 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1673 | `	/* Perform the insertion */` |
|    19758 |  1674 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    19758 |  1675 | `	PH7_MemObjRelease(&sKey);` |
|    19758 |  1676 | `	return rc;` |
|        2 |  1677 |  |
|        - |  1678 | `/*` |
|        - |  1679 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1680 | ` * Return a pointer to the variable value on success.` |
|        - |  1681 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1682 | ` */` |
|  2669268 |  1683 | `static ph7_value * VmExtractMemObj(` |
|        - |  1684 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1685 | `	const SyString *pName, /* Variable name */` |
|        - |  1686 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1687 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1688 | `	)` |
|        2 |  1689 |  |
|  2669270 |  1690 | `	int bNullify = FALSE;` |
|        - |  1691 | `	SyHashEntry *pEntry;` |
|        - |  1692 | `	VmFrame *pFrame;` |
|        - |  1693 | `	ph7_value *pObj;` |
|        - |  1694 | `	sxu32 nIdx;` |
|        - |  1695 | `	sxi32 rc;` |
|        - |  1696 | `	/* Point to the top active frame */` |
|  2669270 |  1697 | `	pFrame = pVm->pFrame;` |
|  2718622 |  1698 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1699 | `		/* Safely ignore the exception frame */` |
|    49353 |  1700 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        1 |  1701 | `	}` |
|        - |  1702 | `	/* Perform the lookup */` |
|  2669270 |  1703 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1704 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1705 | `		pName = &sAnnon;` |
|        - |  1706 | `		/* Always nullify the object */` |
|      ! 0 |  1707 | `		bNullify = TRUE;` |
|      ! 0 |  1708 | `		bDup = FALSE;` |
|      ! 0 |  1709 | `	}` |
|        - |  1710 | `	/* Check the superglobals table first */` |
|  2669270 |  1711 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  2669270 |  1712 | `	if( pEntry == 0 ){` |
|        - |  1713 | `		/* Query the top active frame */` |
|  2669234 |  1714 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  2669234 |  1715 | `		if( pEntry == 0 ){` |
|    67888 |  1716 | `			char *zName = (char *)pName->zString;` |
|        - |  1717 | `			VmSlot sLocal;` |
|    67888 |  1718 | `			if( !bCreate ){` |
|        - |  1719 | `				/* Do not create the variable,return NULL instead */` |
|      494 |  1720 | `				return 0;` |
|        - |  1721 | `			}` |
|        - |  1722 | `			/* No such variable,automatically create a new one and install` |
|        - |  1723 | `			 * it in the current frame.` |
|        - |  1724 | `			 */` |
|    67396 |  1725 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    67396 |  1726 | `			if( pObj == 0 ){` |
|      ! 0 |  1727 | `				return 0;` |
|        - |  1728 | `			}` |
|    67396 |  1729 | `			nIdx = pObj->nIdx;` |
|    67396 |  1730 | `			if( bDup ){` |
|        - |  1731 | `				/* Duplicate name */` |
|      134 |  1732 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      134 |  1733 | `				if( zName == 0 ){` |
|      ! 0 |  1734 | `					return 0;` |
|        - |  1735 | `				}` |
|       66 |  1736 | `			}` |
|        - |  1737 | `			/* Link to the top active VM frame */` |
|    67396 |  1738 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    67396 |  1739 | `			if( rc != SXRET_OK ){` |
|        - |  1740 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1741 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1742 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1743 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1744 | `				return 0;` |
|        - |  1745 | `			}` |
|    67396 |  1746 | `			if( pFrame->pParent != 0 ){` |
|        - |  1747 | `				/* Local variable */` |
|    62184 |  1748 | `				sLocal.nIdx = nIdx;` |
|    62184 |  1749 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    31093 |  1750 | `			}else{` |
|        - |  1751 | `				/* Register in the $GLOBALS array */` |
|     5214 |  1752 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1753 | `			}` |
|        - |  1754 | `			/* Install in the reference table */` |
|    67396 |  1755 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1756 | `			/* Save object index */` |
|    67396 |  1757 | `			pObj->nIdx = nIdx;` |
|    33699 |  1758 | `		}else{` |
|        - |  1759 | `			/* Extract variable contents */` |
|  2601348 |  1760 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  2601348 |  1761 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  2601348 |  1762 | `			if( bNullify && pObj ){` |
|      ! 0 |  1763 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1764 | `			}` |
|        - |  1765 | `		}` |
|  1334482 |  1766 | `	}else{` |
|        - |  1767 | `		/* Superglobal */` |
|       38 |  1768 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1769 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1770 | `	}` |
|  2668778 |  1771 | `	return pObj;` |
|  1334746 |  1772 |  |
|        - |  1773 | `/*` |
|        - |  1774 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1775 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1776 | ` */` |
|     1478 |  1777 | `static ph7_value * VmExtractSuper(` |
|        - |  1778 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1779 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1780 | `	sxu32 nByte        /* zName length */` |
|        - |  1781 | `	)` |
|        2 |  1782 |  |
|        - |  1783 | `	SyHashEntry *pEntry;` |
|        - |  1784 | `	ph7_value *pValue;` |
|        - |  1785 | `	sxu32 nIdx;` |
|        - |  1786 | `	/* Query the superglobal table */` |
|     1480 |  1787 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     1480 |  1788 | `	if( pEntry == 0 ){` |
|        - |  1789 | `		/* No such entry */` |
|      ! 0 |  1790 | `		return 0;` |
|        - |  1791 | `	}` |
|        - |  1792 | `	/* Extract the superglobal index in the global object pool */` |
|     1480 |  1793 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1794 | `	/* Extract the variable value  */` |
|     1480 |  1795 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     1480 |  1796 | `	return pValue;` |
|      741 |  1797 |  |
|        - |  1798 | `/*` |
|        - |  1799 | ` * Perform a raw hashmap insertion.` |
|        - |  1800 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1801 | ` */` |
|     1476 |  1802 | `static sxi32 VmHashmapInsert(` |
|        - |  1803 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1804 | `	const char *zKey,   /* Entry key */` |
|        - |  1805 | `	int nKeylen,        /* zKey length*/` |
|        - |  1806 | `	const char *zData,  /* Entry data */` |
|        - |  1807 | `	int nLen            /* zData length */` |
|        - |  1808 | `	)` |
|        2 |  1809 |  |
|        - |  1810 | `	ph7_value sKey,sValue;` |
|        - |  1811 | `	sxi32 rc;` |
|     1478 |  1812 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     1478 |  1813 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     1478 |  1814 | `	if( zKey ){` |
|     1456 |  1815 | `		if( nKeylen < 0 ){` |
|     1456 |  1816 | `			nKeylen = (int)SyStrlen(zKey);` |
|      727 |  1817 | `		}` |
|     1456 |  1818 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|      727 |  1819 | `	}` |
|     1478 |  1820 | `	if( zData ){` |
|     1478 |  1821 | `		if( nLen < 0 ){` |
|        - |  1822 | `			/* Compute length automatically */` |
|      ! 0 |  1823 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1824 | `		}` |
|     1478 |  1825 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|      738 |  1826 | `	}` |
|        - |  1827 | `	/* Perform the insertion */` |
|     1478 |  1828 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     1478 |  1829 | `	PH7_MemObjRelease(&sKey);` |
|     1478 |  1830 | `	PH7_MemObjRelease(&sValue);` |
|     1478 |  1831 | `	return rc;` |
|        2 |  1832 |  |
|        - |  1833 | `/* Forward declaration */` |
|        - |  1834 | `static sxi32 VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte);` |
|        - |  1835 | `/*` |
|        - |  1836 | ` * Configure a working virtual machine instance.` |
|        - |  1837 | ` *` |
|        - |  1838 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1839 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1840 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1841 | ` * The second argument to this function is an integer configuration option` |
|        - |  1842 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1843 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1844 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1845 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1846 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1847 | ` */` |
|    23256 |  1848 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1849 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1850 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1851 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1852 | `	)` |
|        2 |  1853 |  |
|    23258 |  1854 | `	sxi32 rc = SXRET_OK;` |
|    23258 |  1855 | `	switch(nOp){` |
|      726 |  1856 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     1454 |  1857 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     1454 |  1858 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1859 | `		/* VM output consumer callback */` |
|        - |  1860 | `#ifdef UNTRUST` |
|        - |  1861 | `		if( xConsumer == 0 ){` |
|        - |  1862 | `			rc = SXERR_CORRUPT;` |
|        - |  1863 | `			break;` |
|        - |  1864 | `		}` |
|        - |  1865 | `#endif` |
|        - |  1866 | `		/* Install the output consumer */` |
|     1454 |  1867 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     1454 |  1868 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     1454 |  1869 | `		break;` |
|        - |  1870 | `							   }` |
|      726 |  1871 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1872 | `		/* Import path */` |
|        - |  1873 | `		  const char *zPath;` |
|        - |  1874 | `		  SyString sPath;` |
|     1454 |  1875 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1876 | `#if defined(UNTRUST)` |
|        - |  1877 | `		  if( zPath == 0 ){` |
|        - |  1878 | `			  rc = SXERR_EMPTY;` |
|        - |  1879 | `			  break;` |
|        - |  1880 | `		  }` |
|        - |  1881 | `#endif` |
|     1454 |  1882 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1883 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1884 | `#ifdef __WINNT__` |
|        2 |  1885 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1886 | `#endif` |
|     2906 |  1887 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1888 | `		  /* Remove leading and trailing white spaces */` |
|     1454 |  1889 | `		  SyStringFullTrim(&sPath);` |
|     1454 |  1890 | `		  if( sPath.nByte > 0 ){` |
|        - |  1891 | `			  /* Store the path in the corresponding conatiner */` |
|     1454 |  1892 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      726 |  1893 | `		  }` |
|     1454 |  1894 | `		  break;` |
|        - |  1895 | `									 }` |
|      726 |  1896 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1897 | `		/* Run-Time Error report */` |
|     1454 |  1898 | `		pVm->bErrReport = 1;` |
|     1454 |  1899 | `		break;` |
|      ! 0 |  1900 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1901 | `		/* Recursion depth */` |
|      ! 0 |  1902 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1903 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1904 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1905 | `		}` |
|      ! 0 |  1906 | `		break;` |
|        - |  1907 | `									   }` |
|      ! 0 |  1908 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1909 | `		/* VM output length in bytes */` |
|      ! 0 |  1910 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1911 | `#ifdef UNTRUST` |
|        - |  1912 | `		if( pOut == 0 ){` |
|        - |  1913 | `			rc = SXERR_CORRUPT;` |
|        - |  1914 | `			break;` |
|        - |  1915 | `		}` |
|        - |  1916 | `#endif` |
|      ! 0 |  1917 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1918 | `		break;` |
|        - |  1919 | `							   }` |
|        - |  1920 |  |
|     7260 |  1921 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1922 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1923 | `		/* Create a new superglobal/global variable */` |
|    14522 |  1924 | `		const char *zName = va_arg(ap,const char *);` |
|    14522 |  1925 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1926 | `		SyHashEntry *pEntry;` |
|        - |  1927 | `		ph7_value *pObj;` |
|        - |  1928 | `		sxu32 nByte;` |
|        - |  1929 | `		sxu32 nIdx;` |
|        - |  1930 | `#ifdef UNTRUST` |
|        - |  1931 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  1932 | `			rc = SXERR_CORRUPT;` |
|        - |  1933 | `			break;` |
|        - |  1934 | `		}` |
|        - |  1935 | `#endif` |
|    14522 |  1936 | `		nByte = SyStrlen(zName);` |
|    14522 |  1937 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1938 | `			/* Check if the superglobal is already installed */` |
|    14522 |  1939 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     7262 |  1940 | `		}else{` |
|        - |  1941 | `			/* Query the top active VM frame */` |
|      ! 0 |  1942 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1943 | `		}` |
|    14522 |  1944 | `		if( pEntry ){` |
|        - |  1945 | `			/* Variable already installed */` |
|      ! 0 |  1946 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1947 | `			/* Extract contents */` |
|      ! 0 |  1948 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  1949 | `			if( pObj ){` |
|        - |  1950 | `				/* Overwrite old contents */` |
|      ! 0 |  1951 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  1952 | `			}` |
|      ! 0 |  1953 | `		}else{` |
|        - |  1954 | `			/* Install a new variable */` |
|    14522 |  1955 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    14522 |  1956 | `			if( pObj == 0 ){` |
|      ! 0 |  1957 | `				rc = SXERR_MEM;` |
|      ! 0 |  1958 | `				break;` |
|        - |  1959 | `			}` |
|    14522 |  1960 | `			nIdx = pObj->nIdx;` |
|        - |  1961 | `			/* Copy value */` |
|    14522 |  1962 | `			PH7_MemObjStore(pValue,pObj);` |
|    14522 |  1963 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1964 | `				/* Install the superglobal */` |
|    14522 |  1965 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     7262 |  1966 | `			}else{` |
|        - |  1967 | `				/* Install in the current frame */` |
|      ! 0 |  1968 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1969 | `			}` |
|    14522 |  1970 | `			if( rc == SXRET_OK ){` |
|        - |  1971 | `				SyHashEntry *pRef;` |
|    14522 |  1972 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    14522 |  1973 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     7262 |  1974 | `				}else{` |
|      ! 0 |  1975 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1976 | `				}` |
|        - |  1977 | `				/* Install in the reference table */` |
|    14522 |  1978 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    14522 |  1979 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1980 | `					/* Register in the $GLOBALS array */` |
|    14522 |  1981 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     7260 |  1982 | `				}` |
|     7260 |  1983 | `			}` |
|        - |  1984 | `		}` |
|    14522 |  1985 | `		break;` |
|        - |  1986 | `									}` |
|      727 |  1987 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1988 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1989 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1990 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1991 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1992 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1993 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     1456 |  1994 | `		const char *zKey   = va_arg(ap,const char *);` |
|     1456 |  1995 | `		const char *zValue = va_arg(ap,const char *);` |
|     1456 |  1996 | `		int nLen = va_arg(ap,int);` |
|        - |  1997 | `		ph7_hashmap *pMap;` |
|        - |  1998 | `		ph7_value *pValue;` |
|     1456 |  1999 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2000 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2001 | `			pValue = VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     1455 |  2002 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2003 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2004 | `			pValue = VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     1454 |  2005 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2006 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2007 | `			pValue = VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     1454 |  2008 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2009 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2010 | `			pValue = VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     1454 |  2011 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2012 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2013 | `			pValue = VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     1454 |  2014 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2015 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2016 | `			pValue = VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2017 | `		}else{` |
|        - |  2018 | `			/* Extract the $_SERVER superglobal */` |
|     1454 |  2019 | `			pValue = VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2020 | `		}` |
|     1456 |  2021 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2022 | `			/* No such entry */` |
|      ! 0 |  2023 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2024 | `			break;` |
|        - |  2025 | `		}` |
|        - |  2026 | `		/* Point to the hashmap */` |
|     1456 |  2027 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2028 | `		/* Perform the insertion */` |
|     1456 |  2029 | `		rc = VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     1456 |  2030 | `		break;` |
|        - |  2031 | `								   }` |
|       11 |  2032 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2033 | `		/* Script arguments */` |
|       24 |  2034 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2035 | `		ph7_hashmap *pMap;` |
|        - |  2036 | `		ph7_value *pValue;` |
|        - |  2037 | `		sxu32 n;` |
|       24 |  2038 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2039 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2040 | `			break;` |
|        - |  2041 | `		}` |
|        - |  2042 | `		/* Extract the $argv array */` |
|       24 |  2043 | `		pValue = VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2044 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2045 | `			/* No such entry */` |
|      ! 0 |  2046 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2047 | `			break;` |
|        - |  2048 | `		}` |
|        - |  2049 | `		/* Point to the hashmap */` |
|       24 |  2050 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2051 | `		/* Perform the insertion */` |
|       24 |  2052 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2053 | `		rc = VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2054 | `		if( rc == SXRET_OK ){` |
|       24 |  2055 | `			if( pMap->nEntry > 1 ){` |
|        - |  2056 | `				/* Append space separator first */` |
|       18 |  2057 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2058 | `			}` |
|       24 |  2059 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2060 | `		}` |
|       24 |  2061 | `		break;` |
|        - |  2062 | `								  }` |
|      ! 0 |  2063 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2064 | `		/* error_log() consumer */` |
|      ! 0 |  2065 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2066 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2067 | `		break;` |
|        - |  2068 | `										}` |
|      ! 0 |  2069 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2070 | `		/* Script return value */` |
|      ! 0 |  2071 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2072 | `#ifdef UNTRUST` |
|        - |  2073 | `		if( ppValue == 0 ){` |
|        - |  2074 | `			rc = SXERR_CORRUPT;` |
|        - |  2075 | `			break;` |
|        - |  2076 | `		}` |
|        - |  2077 | `#endif` |
|      ! 0 |  2078 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2079 | `		break;` |
|        - |  2080 | `								   }` |
|     1452 |  2081 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2082 | `		/* Register an IO stream device */` |
|     2906 |  2083 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2084 | `		/* Make sure we are dealing with a valid IO stream */` |
|     4356 |  2085 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     2906 |  2086 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2087 | `				/* Invalid stream */` |
|      ! 0 |  2088 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2089 | `				break;` |
|        - |  2090 | `		}` |
|     2906 |  2091 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2092 | `			/* Make the 'file://' stream the defaut stream device */` |
|     1454 |  2093 | `			pVm->pDefStream = pStream;` |
|      726 |  2094 | `		}` |
|        - |  2095 | `		/* Insert in the appropriate container */` |
|     2906 |  2096 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     2906 |  2097 | `		break;` |
|        - |  2098 | `								  }` |
|      ! 0 |  2099 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2100 | `		/* Point to the VM internal output consumer buffer */` |
|      ! 0 |  2101 | `		const void **ppOut = va_arg(ap,const void **);` |
|      ! 0 |  2102 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2103 | `#ifdef UNTRUST` |
|        - |  2104 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2105 | `			rc = SXERR_CORRUPT;` |
|        - |  2106 | `			break;` |
|        - |  2107 | `		}` |
|        - |  2108 | `#endif` |
|      ! 0 |  2109 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|      ! 0 |  2110 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|      ! 0 |  2111 | `		break;` |
|        - |  2112 | `									   }` |
|      ! 0 |  2113 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2114 | `		/* Raw HTTP request*/` |
|      ! 0 |  2115 | `		const char *zRequest = va_arg(ap,const char *);` |
|      ! 0 |  2116 | `		int nByte = va_arg(ap,int);` |
|      ! 0 |  2117 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2118 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2119 | `			break;` |
|        - |  2120 | `		}` |
|      ! 0 |  2121 | `		if( nByte < 0 ){` |
|        - |  2122 | `			/* Compute length automatically */` |
|      ! 0 |  2123 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2124 | `		}` |
|        - |  2125 | `		/* Process the request */` |
|      ! 0 |  2126 | `		rc = VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|      ! 0 |  2127 | `		break;` |
|        - |  2128 | `									}` |
|      ! 0 |  2129 | `	default:` |
|        - |  2130 | `		/* Unknown configuration option */` |
|      ! 0 |  2131 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2132 | `		break;` |
|        - |  2133 | `	}` |
|    23258 |  2134 | `	return rc;` |
|        2 |  2135 |  |
|        - |  2136 | `/* Forward declaration */` |
|        - |  2137 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2138 | `/*` |
|        - |  2139 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2140 | ` * format.` |
|        - |  2141 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2142 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2143 | ` * (STDOUT).` |
|        - |  2144 | ` */` |
|        2 |  2145 | `static sxi32 VmByteCodeDump(` |
|        - |  2146 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2147 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2148 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2149 | `	)` |
|        1 |  2150 |  |
|        - |  2151 | `	static const char zDump[] = {` |
|        - |  2152 | `		"====================================================\n"` |
|        - |  2153 | `		"PH7 VM Dump\n"` |
|        - |  2154 | `		"====================================================\n"` |
|        - |  2155 | `	};` |
|        - |  2156 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2157 | `	sxi32 rc = SXRET_OK;` |
|        - |  2158 | `	sxu32 n;` |
|        - |  2159 | `	/* Point to the PH7 instructions */` |
|        3 |  2160 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2161 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2162 | `	n = 0;` |
|        3 |  2163 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2164 | `	/* Dump instructions */` |
|        6 |  2165 | `	for(;;){` |
|       13 |  2166 | `		if( pInstr >= pEnd ){` |
|        - |  2167 | `			/* No more instructions */` |
|        3 |  2168 | `			break;` |
|        - |  2169 | `		}` |
|        - |  2170 | `		/* Format and call the consumer callback */` |
|       16 |  2171 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       10 |  2172 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       10 |  2173 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       11 |  2174 | `		if( rc != SXRET_OK ){` |
|        - |  2175 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2176 | `			return rc;` |
|        - |  2177 | `		}` |
|       11 |  2178 | `		++n;` |
|       11 |  2179 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2180 | `	}` |
|        3 |  2181 | `	return rc;` |
|        2 |  2182 |  |
|        - |  2183 | `/* Forward declaration */` |
|        - |  2184 | `static int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData);` |
|        - |  2185 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2186 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2187 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2188 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2189 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2190 | `/*` |
|        - |  2191 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2192 | ` * consumer callback.` |
|        - |  2193 | ` */` |
|      336 |  2194 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2195 |  |
|      337 |  2196 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      337 |  2197 | `	sxi32 rc = SXRET_OK;` |
|        - |  2198 | `	/* Append a new line */` |
|        - |  2199 | `#ifdef __WINNT__` |
|        1 |  2200 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2201 | `#else` |
|      336 |  2202 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2203 | `#endif` |
|        - |  2204 | `	/* Invoke the output consumer callback */` |
|      337 |  2205 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      337 |  2206 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  2207 | `		/* Increment output length */` |
|      337 |  2208 | `		pVm->nOutputLen += SyBlobLength(pMsg);` |
|      168 |  2209 | `	}` |
|      337 |  2210 | `	return rc;` |
|        1 |  2211 |  |
|        - |  2212 | `/*` |
|        - |  2213 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2214 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2215 | ` * information.` |
|        - |  2216 | ` */` |
|      118 |  2217 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2218 |  |
|      120 |  2219 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2220 | `		ph7_value apArg[4];` |
|        - |  2221 | `		ph7_value *apArgPtr[4];` |
|        - |  2222 | `		ph7_value sResult;` |
|        - |  2223 | `		SyString sErr;` |
|        - |  2224 | `		/* Prepare arguments */` |
|       41 |  2225 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2226 | `			/* use explicit message length to avoid reading past buffer */` |
|       41 |  2227 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       41 |  2228 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       41 |  2229 | `		if( pFile ){` |
|       41 |  2230 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       41 |  2231 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       21 |  2232 | `		}else{` |
|      ! 0 |  2233 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2234 | `		}` |
|       41 |  2235 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       41 |  2236 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2237 | `		/* Set up pointer array */` |
|       41 |  2238 | `		apArgPtr[0] = &apArg[0];` |
|       41 |  2239 | `		apArgPtr[1] = &apArg[1];` |
|       41 |  2240 | `		apArgPtr[2] = &apArg[2];` |
|       41 |  2241 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2242 | `		/* Call the handler */` |
|       41 |  2243 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2244 | `		/* Check return value */` |
|       41 |  2245 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2246 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2247 | `		}` |
|        - |  2248 | `		/* Release */` |
|       41 |  2249 | `		PH7_MemObjRelease(&apArg[0]);` |
|       41 |  2250 | `		PH7_MemObjRelease(&apArg[1]);` |
|       41 |  2251 | `		PH7_MemObjRelease(&apArg[2]);` |
|       41 |  2252 | `		PH7_MemObjRelease(&apArg[3]);` |
|       41 |  2253 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2254 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2255 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       41 |  2256 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2257 | `	}` |
|        - |  2258 | `	/* No handler, always call error handler */` |
|       79 |  2259 | `	return TRUE;` |
|       61 |  2260 |  |
|       82 |  2261 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2262 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2263 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2264 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2265 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2266 | `	)` |
|        2 |  2267 |  |
|       84 |  2268 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2269 | `	SyString *pFile;` |
|        - |  2270 | `	char *zErr;` |
|       84 |  2271 | `	sxi32 rc = SXRET_OK;` |
|       84 |  2272 | `	if( !pVm->bErrReport ){` |
|        - |  2273 | `		/* Don't bother reporting errors */` |
|        3 |  2274 | `		return SXRET_OK;` |
|        - |  2275 | `	}` |
|        - |  2276 | `	/* Reset the working buffer */` |
|       82 |  2277 | `	SyBlobReset(pWorker);` |
|        - |  2278 | `	/* Peek the processed file if available */` |
|       82 |  2279 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       82 |  2280 | `	if( pFile ){` |
|        - |  2281 | `		/* Append file name */` |
|       82 |  2282 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       82 |  2283 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       40 |  2284 | `	}` |
|        - |  2285 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2286 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2287 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2288 | `	 * E_DEPRECATED). */` |
|       82 |  2289 | `	zErr = "Error:  ";` |
|       82 |  2290 | `	switch(iErr){` |
|       20 |  2291 | `	case PH7_CTX_WARNING:` |
|       42 |  2292 | `		zErr = "Warning:  ";` |
|       42 |  2293 | `		break;` |
|        6 |  2294 | `	case PH7_CTX_NOTICE:` |
|       14 |  2295 | `		zErr = "Notice:  ";` |
|       12 |  2296 | `		break;` |
|       14 |  2297 | `	default:` |
|        - |  2298 | `		/* keep iErr unchanged */` |
|       28 |  2299 | `		break;` |
|        - |  2300 | `	}` |
|       82 |  2301 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       82 |  2302 | `	if( pFuncName ){` |
|        - |  2303 | `		/* Append function name first */` |
|       29 |  2304 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       29 |  2305 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       14 |  2306 | `	}` |
|       82 |  2307 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2308 | `	/* Check for user error handler.  compute length of C string */` |
|       82 |  2309 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       53 |  2310 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       26 |  2311 | `	}` |
|       82 |  2312 | `	return rc;` |
|       43 |  2313 |  |
|        - |  2314 | `/*` |
|        - |  2315 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2316 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2317 | ` * information.` |
|        - |  2318 | ` */` |
|       38 |  2319 | `static sxi32 VmThrowErrorAp(` |
|        - |  2320 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2321 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2322 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2323 | `	const char *zFormat, /* Format message */` |
|        - |  2324 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2325 | `	)` |
|        2 |  2326 |  |
|       40 |  2327 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2328 | `	SyBlob sMsg;` |
|        - |  2329 | `	SyString *pFile;` |
|        - |  2330 | `	char *zErr;` |
|       40 |  2331 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2332 | `	if( !pVm->bErrReport ){` |
|        - |  2333 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2334 | `		return SXRET_OK;` |
|        - |  2335 | `	}` |
|        - |  2336 | `	/* Reset the working buffer */` |
|       40 |  2337 | `	SyBlobReset(pWorker);` |
|        - |  2338 | `	/* Peek the processed file if available */` |
|       40 |  2339 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2340 | `	if( pFile ){` |
|        - |  2341 | `		/* Append file name */` |
|       40 |  2342 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2343 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2344 | `	}` |
|        - |  2345 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2346 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2347 | `	 * the correct errno value. */` |
|       40 |  2348 | `	zErr = "Error:  ";` |
|       40 |  2349 | `	switch(iErr){` |
|        4 |  2350 | `	case PH7_CTX_WARNING:` |
|        9 |  2351 | `		zErr = "Warning:  ";` |
|        9 |  2352 | `		break;` |
|        3 |  2353 | `	case PH7_CTX_NOTICE:` |
|        7 |  2354 | `		zErr = "Notice:  ";` |
|        6 |  2355 | `		break;` |
|       12 |  2356 | `	default:` |
|        - |  2357 | `		/* do not change iErr */` |
|       24 |  2358 | `		break;` |
|        - |  2359 | `	}` |
|       40 |  2360 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2361 | `	if( pFuncName ){` |
|        - |  2362 | `		/* Append function name first */` |
|       26 |  2363 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2364 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2365 | `	}` |
|        - |  2366 | `	/* Format the raw message */` |
|       40 |  2367 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2368 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2369 | `	/* Check if a user error handler is installed */` |
|       40 |  2370 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2371 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2372 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2373 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2374 | `	}` |
|       40 |  2375 | `	SyBlobRelease(&sMsg);` |
|       40 |  2376 | `	return rc;` |
|       21 |  2377 |  |
|        - |  2378 | `/*` |
|        - |  2379 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2380 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2381 | ` * information.` |
|        - |  2382 | ` * ------------------------------------` |
|        - |  2383 | ` * Simple boring wrapper function.` |
|        - |  2384 | ` * ------------------------------------` |
|        - |  2385 | ` */` |
|       14 |  2386 | `static sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2387 |  |
|        - |  2388 | `	va_list ap;` |
|        - |  2389 | `	sxi32 rc;` |
|       15 |  2390 | `	va_start(ap,zFormat);` |
|       15 |  2391 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2392 | `	va_end(ap);` |
|       15 |  2393 | `	return rc;` |
|        1 |  2394 |  |
|        - |  2395 | `/*` |
|        - |  2396 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2397 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2398 | ` * information.` |
|        - |  2399 | ` * ------------------------------------` |
|        - |  2400 | ` * Simple boring wrapper function.` |
|        - |  2401 | ` * ------------------------------------` |
|        - |  2402 | ` */` |
|       24 |  2403 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2404 |  |
|        - |  2405 | `	sxi32 rc;` |
|       26 |  2406 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2407 | `	return rc;` |
|        2 |  2408 |  |
|        - |  2409 | `/*` |
|        - |  2410 | ` * Resolve function context from the current frame.` |
|        - |  2411 | ` */` |
|      512 |  2412 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2413 |  |
|        - |  2414 | `	VmFrame *pFrame;` |
|        - |  2415 | `	ph7_vm_func *pFunc;` |
|      513 |  2416 | `	*pzFuncName = 0;` |
|      513 |  2417 | `	*pnFuncLen = 0;` |
|      513 |  2418 | `	pFrame = pVm->pFrame;` |
|      513 |  2419 | `	if( pFrame == 0 ){` |
|      ! 0 |  2420 | `		return;` |
|        - |  2421 | `	}` |
|      513 |  2422 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  2423 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  2424 | `	}` |
|      513 |  2425 | `	if( pFrame->pParent == 0 ){` |
|      509 |  2426 | `		return;` |
|        - |  2427 | `	}` |
|        5 |  2428 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  2429 | `	if( pFunc == 0 ){` |
|      ! 0 |  2430 | `		return;` |
|        - |  2431 | `	}` |
|        5 |  2432 | `	*pzFuncName = pFunc->sName.zString;` |
|        5 |  2433 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      257 |  2434 |  |
|        - |  2435 | `/*` |
|        - |  2436 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2437 | ` */` |
|      258 |  2438 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2439 |  |
|        - |  2440 | `	SyBlob sOut;` |
|        - |  2441 | `	SyString *pFile;` |
|      259 |  2442 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2443 | `		return PH7_OK;` |
|        - |  2444 | `	}` |
|      259 |  2445 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2446 | `		zClass = "Exception";` |
|      ! 0 |  2447 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2448 | `	}` |
|      259 |  2449 | `	if( zMsg == 0 ){` |
|      ! 0 |  2450 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2451 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2452 | `	}` |
|      259 |  2453 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      255 |  2454 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      127 |  2455 | `	}` |
|      259 |  2456 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      259 |  2457 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      259 |  2458 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      259 |  2459 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      259 |  2460 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      259 |  2461 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      259 |  2462 | `	if( pFile ){` |
|      259 |  2463 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      259 |  2464 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      259 |  2465 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      129 |  2466 | `	}` |
|      259 |  2467 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      259 |  2468 | `	if( pFile ){` |
|      259 |  2469 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      259 |  2470 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      259 |  2471 | `		if( zFuncName && nFuncLen > 0 ){` |
|        5 |  2472 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        3 |  2473 | `		}else{` |
|      255 |  2474 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2475 | `		}` |
|      129 |  2476 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2477 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2478 | `	}else{` |
|      ! 0 |  2479 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2480 | `	}` |
|      259 |  2481 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      259 |  2482 | `	if( pFile ){` |
|      259 |  2483 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      259 |  2484 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      259 |  2485 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      259 |  2486 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      129 |  2487 | `	}` |
|      259 |  2488 | `	VmCallErrorHandler(pVm,&sOut);` |
|      259 |  2489 | `	SyBlobRelease(&sOut);` |
|      259 |  2490 | `	return PH7_ABORT;` |
|      130 |  2491 |  |
|        - |  2492 | `/*` |
|        - |  2493 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2494 | ` */` |
|      254 |  2495 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2496 |  |
|        - |  2497 | `	ph7_vm *pVm;` |
|        - |  2498 | `	ph7_class *pClass;` |
|        - |  2499 | `	ph7_class_instance *pThis;` |
|        - |  2500 | `	ph7_class_method *pCons;` |
|        - |  2501 | `	ph7_value sArg;` |
|        - |  2502 | `	ph7_value *apArg[1];` |
|        - |  2503 | `	SyBlob sMsg;` |
|        - |  2504 | `	SyString sMsgStr;` |
|        - |  2505 | `	VmFrame *pFrame;` |
|        - |  2506 | `	va_list ap;` |
|        - |  2507 | `	sxi32 rc;` |
|        - |  2508 |  |
|      256 |  2509 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2510 | `		return PH7_ABORT;` |
|        - |  2511 | `	}` |
|      256 |  2512 | `	pVm = pCtx->pVm;` |
|      256 |  2513 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2514 | `		zClass = "Error";` |
|      ! 0 |  2515 | `	}` |
|      256 |  2516 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      256 |  2517 | `	if( pClass == 0 ){` |
|      ! 0 |  2518 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2519 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2520 | `			zClass` |
|        - |  2521 | `			);` |
|        - |  2522 | `	}` |
|      256 |  2523 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      256 |  2524 | `	if( pThis == 0 ){` |
|      ! 0 |  2525 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2526 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2527 | `			);` |
|        - |  2528 | `	}` |
|        - |  2529 |  |
|      256 |  2530 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      256 |  2531 | `	va_start(ap,zFormat);` |
|      256 |  2532 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      256 |  2533 | `	va_end(ap);` |
|        - |  2534 |  |
|      256 |  2535 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      256 |  2536 | `	if( pCons ){` |
|      256 |  2537 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      256 |  2538 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      256 |  2539 | `		apArg[0] = &sArg;` |
|      256 |  2540 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      256 |  2541 | `		PH7_MemObjRelease(&sArg);` |
|      127 |  2542 | `	}` |
|      256 |  2543 | `	SyBlobRelease(&sMsg);` |
|        - |  2544 |  |
|      256 |  2545 | `	pFrame = pVm->pFrame;` |
|      256 |  2546 | `	if( pFrame ){` |
|      258 |  2547 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        3 |  2548 | `			pFrame = pFrame->pParent;` |
|        1 |  2549 | `		}` |
|      256 |  2550 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      127 |  2551 | `	}` |
|      256 |  2552 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      256 |  2553 | `	PH7_ClassInstanceUnref(pThis);` |
|      256 |  2554 | `	if( rc == SXERR_ABORT ){` |
|      253 |  2555 | `		return PH7_ABORT;` |
|        - |  2556 | `	}` |
|        3 |  2557 | `	return PH7_EXCEPTION;` |
|      129 |  2558 |  |
|        - |  2559 | `/*` |
|        - |  2560 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2561 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2562 | ` */` |
|      ! 0 |  2563 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2564 |  |
|        - |  2565 | `	ph7_vm *pVm;` |
|        - |  2566 | `	SyBlob sMsg;` |
|      ! 0 |  2567 | `	const char *zFuncName = 0;` |
|      ! 0 |  2568 | `	int nFuncLen = 0;` |
|        - |  2569 | `	va_list ap;` |
|        - |  2570 | `	sxi32 rc;` |
|        - |  2571 |  |
|      ! 0 |  2572 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2573 | `		return PH7_OK;` |
|        - |  2574 | `	}` |
|      ! 0 |  2575 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2576 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2577 | `		zClass = "Error";` |
|      ! 0 |  2578 | `	}` |
|        - |  2579 |  |
|      ! 0 |  2580 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2581 |  |
|      ! 0 |  2582 | `	va_start(ap,zFormat);` |
|      ! 0 |  2583 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2584 | `	va_end(ap);` |
|        - |  2585 |  |
|      ! 0 |  2586 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2587 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2588 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2589 | `	}` |
|      ! 0 |  2590 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2591 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2592 | `	}` |
|      ! 0 |  2593 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2594 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2595 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2596 | `	return rc;` |
|      ! 0 |  2597 |  |
|        - |  2598 | `/*` |
|        - |  2599 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2600 | ` *` |
|        - |  2601 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2602 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2603 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2604 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2605 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2606 | ` * then the program execution is halted.` |
|        - |  2607 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2608 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2609 | ` * or to reset the VM to it's initial state.` |
|        - |  2610 | ` */` |
|    24706 |  2611 | `static sxi32 VmByteCodeExec(` |
|        - |  2612 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2613 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2614 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2615 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2616 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2617 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2618 | `	int is_callback      /* TRUE if we are executing a callback */` |
|        - |  2619 | `	)` |
|        2 |  2620 |  |
|        - |  2621 | `	VmInstr *pInstr;` |
|        - |  2622 | `	ph7_value *pTos;` |
|        - |  2623 | `	SySet aArg;` |
|        - |  2624 | `	sxi32 pc;` |
|        - |  2625 | `	sxi32 rc;` |
|        - |  2626 | `	/* Argument container */` |
|    24708 |  2627 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    24708 |  2628 | `	if( nTos < 0 ){` |
|    23634 |  2629 | `		pTos = &pStack[-1];` |
|    11818 |  2630 | `	}else{` |
|     1076 |  2631 | `		pTos = &pStack[nTos];` |
|        - |  2632 | `	}` |
|    24708 |  2633 | `	pc = 0;` |
|        - |  2634 | `	/* Execute as much as we can */` |
|  4294319 |  2635 | `	for(;;){` |
|        - |  2636 | `		/* Fetch the instruction to execute */` |
|  8587936 |  2637 | `		pInstr = &aInstr[pc];` |
|  8587936 |  2638 | `		rc = SXRET_OK;` |
|        - |  2639 | `/*` |
|        - |  2640 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2641 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2642 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2643 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2644 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2645 | ` */` |
|  8587936 |  2646 | `		switch(pInstr->iOp){` |
|        - |  2647 | `/*` |
|        - |  2648 | ` * DONE: P1 * *` |
|        - |  2649 | ` *` |
|        - |  2650 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2651 | ` * and return immediately.` |
|        - |  2652 | ` */` |
|    12215 |  2653 | `case PH7_OP_DONE:` |
|    24432 |  2654 | `	if( pInstr->iP1 ){` |
|        - |  2655 | `#ifdef UNTRUST` |
|        - |  2656 | `		if( pTos < pStack ){` |
|        - |  2657 | `			goto Abort;` |
|        - |  2658 | `		}` |
|        - |  2659 | `#endif` |
|    13706 |  2660 | `		if( pLastRef ){` |
|     9172 |  2661 | `			*pLastRef = pTos->nIdx;` |
|     4585 |  2662 | `		}` |
|    13706 |  2663 | `		if( pResult ){` |
|        - |  2664 | `			/* Execution result */` |
|    13162 |  2665 | `			PH7_MemObjStore(pTos,pResult);` |
|     6580 |  2666 | `		}` |
|    13706 |  2667 | `		VmPopOperand(&pTos,1);` |
|    17580 |  2668 | `	}else if( pLastRef ){` |
|        - |  2669 | `		/* Nothing referenced */` |
|      630 |  2670 | `		*pLastRef = SXU32_HIGH;` |
|      314 |  2671 | `	}` |
|    24432 |  2672 | `	goto Done;` |
|        - |  2673 | `/*` |
|        - |  2674 | ` * HALT: P1 * *` |
|        - |  2675 | ` *` |
|        - |  2676 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2677 | ` * and abort immediately.` |
|        - |  2678 | ` */` |
|        4 |  2679 | `case PH7_OP_HALT:` |
|        9 |  2680 | `	if( pInstr->iP1 ){` |
|        - |  2681 | `#ifdef UNTRUST` |
|        - |  2682 | `		if( pTos < pStack ){` |
|        - |  2683 | `			goto Abort;` |
|        - |  2684 | `		}` |
|        - |  2685 | `#endif` |
|        9 |  2686 | `		if( pLastRef ){` |
|      ! 0 |  2687 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2688 | `		}` |
|        9 |  2689 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2690 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2691 | `				/* Output the exit message */` |
|        7 |  2692 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2693 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2694 | `				if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  2695 | `					/* Increment output length */` |
|        5 |  2696 | `					pVm->nOutputLen += SyBlobLength(&pTos->sBlob);` |
|        2 |  2697 | `				}` |
|        3 |  2698 | `			}` |
|        7 |  2699 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2700 | `			/* Record exit status */` |
|        5 |  2701 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2702 | `		}` |
|        9 |  2703 | `		VmPopOperand(&pTos,1);` |
|        4 |  2704 | `	}else if( pLastRef ){` |
|        - |  2705 | `		/* Nothing referenced */` |
|      ! 0 |  2706 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2707 | `	}` |
|        - |  2708 | `	/* Check if we're in an included file context */` |
|        9 |  2709 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2710 | `		/* Terminate the entire process */` |
|        9 |  2711 | `		exit(pVm->iExitStatus);` |
|        - |  2712 | `	}` |
|      ! 0 |  2713 | `	goto Abort;` |
|        - |  2714 | `/*` |
|        - |  2715 | ` * JMP: * P2 *` |
|        - |  2716 | ` *` |
|        - |  2717 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2718 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2719 | ` */` |
|   189829 |  2720 | `case PH7_OP_JMP:` |
|   379704 |  2721 | `	pc = pInstr->iP2 - 1;` |
|   379704 |  2722 | `	break;` |
|        - |  2723 | `/*` |
|        - |  2724 | ` * JZ: P1 P2 *` |
|        - |  2725 | ` *` |
|        - |  2726 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2727 | ` * entry in the stack if P1 is zero.` |
|        - |  2728 | ` */` |
|   434610 |  2729 | `case PH7_OP_JZ:` |
|        - |  2730 | `#ifdef UNTRUST` |
|        - |  2731 | `	if( pTos < pStack ){` |
|        - |  2732 | `		goto Abort;` |
|        - |  2733 | `	}` |
|        - |  2734 | `#endif` |
|        - |  2735 | `	/* Get a boolean value */` |
|   869310 |  2736 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       77 |  2737 | `		PH7_MemObjToBool(pTos);` |
|       38 |  2738 | `	}` |
|   869310 |  2739 | `	if( !pTos->x.iVal ){` |
|        - |  2740 | `		/* Take the jump */` |
|   416424 |  2741 | `		pc = pInstr->iP2 - 1;` |
|   208211 |  2742 | `	}` |
|   869310 |  2743 | `	if( !pInstr->iP1 ){` |
|   681794 |  2744 | `		VmPopOperand(&pTos,1);` |
|   340918 |  2745 | `	}` |
|   869310 |  2746 | `	break;` |
|        - |  2747 | `/*` |
|        - |  2748 | ` * JNZ: P1 P2 *` |
|        - |  2749 | ` *` |
|        - |  2750 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2751 | ` * entry in the stack if P1 is zero.` |
|        - |  2752 | ` */` |
|    43690 |  2753 | `case PH7_OP_JNZ:` |
|        - |  2754 | `#ifdef UNTRUST` |
|        - |  2755 | `	if( pTos < pStack ){` |
|        - |  2756 | `		goto Abort;` |
|        - |  2757 | `	}` |
|        - |  2758 | `#endif` |
|        - |  2759 | `	/* Get a boolean value */` |
|    87382 |  2760 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2761 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2762 | `	}` |
|    87382 |  2763 | `	if( pTos->x.iVal ){` |
|        - |  2764 | `		/* Take the jump */` |
|     3700 |  2765 | `		pc = pInstr->iP2 - 1;` |
|     1849 |  2766 | `	}` |
|    87382 |  2767 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2768 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2769 | `	}` |
|    87382 |  2770 | `	break;` |
|        - |  2771 | `/*` |
|        - |  2772 | ` * NOOP: * * *` |
|        - |  2773 | ` *` |
|        - |  2774 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2775 | ` * destination.` |
|        - |  2776 | ` */` |
|      ! 0 |  2777 | `case PH7_OP_NOOP:` |
|      ! 0 |  2778 | `	break;` |
|        - |  2779 | `/*` |
|        - |  2780 | ` * POP: P1 * *` |
|        - |  2781 | ` *` |
|        - |  2782 | ` * Pop P1 elements from the operand stack.` |
|        - |  2783 | ` */` |
|   339371 |  2784 | `case PH7_OP_POP: {` |
|   678788 |  2785 | `	sxi32 n = pInstr->iP1;` |
|   678788 |  2786 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2787 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2788 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2789 | `	}` |
|   678788 |  2790 | `	VmPopOperand(&pTos,n);` |
|   678788 |  2791 | `	break;` |
|        - |  2792 | `				 }` |
|        - |  2793 | `/*` |
|        - |  2794 | ` * CVT_INT: * * *` |
|        - |  2795 | ` *` |
|        - |  2796 | ` * Force the top of the stack to be an integer.` |
|        - |  2797 | ` */` |
|       35 |  2798 | `case PH7_OP_CVT_INT:` |
|        - |  2799 | `#ifdef UNTRUST` |
|        - |  2800 | `	if( pTos < pStack ){` |
|        - |  2801 | `		goto Abort;` |
|        - |  2802 | `	}` |
|        - |  2803 | `#endif` |
|       72 |  2804 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2805 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2806 | `	}` |
|        - |  2807 | `	/* Invalidate any prior representation */` |
|       72 |  2808 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2809 | `	break;` |
|        - |  2810 | `/*` |
|        - |  2811 | ` * CVT_REAL: * * *` |
|        - |  2812 | ` *` |
|        - |  2813 | ` * Force the top of the stack to be a real.` |
|        - |  2814 | ` */` |
|        4 |  2815 | `case PH7_OP_CVT_REAL:` |
|        - |  2816 | `#ifdef UNTRUST` |
|        - |  2817 | `	if( pTos < pStack ){` |
|        - |  2818 | `		goto Abort;` |
|        - |  2819 | `	}` |
|        - |  2820 | `#endif` |
|        9 |  2821 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2822 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2823 | `	}` |
|        - |  2824 | `	/* Invalidate any prior representation */` |
|        9 |  2825 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2826 | `	break;` |
|        - |  2827 | `/*` |
|        - |  2828 | ` * CVT_STR: * * *` |
|        - |  2829 | ` *` |
|        - |  2830 | ` * Force the top of the stack to be a string.` |
|        - |  2831 | ` */` |
|      136 |  2832 | `case PH7_OP_CVT_STR:` |
|        - |  2833 | `#ifdef UNTRUST` |
|        - |  2834 | `	if( pTos < pStack ){` |
|        - |  2835 | `		goto Abort;` |
|        - |  2836 | `	}` |
|        - |  2837 | `#endif` |
|      274 |  2838 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      274 |  2839 | `		PH7_MemObjToString(pTos);` |
|      136 |  2840 | `	}` |
|      274 |  2841 | `	break;` |
|        - |  2842 | `/*` |
|        - |  2843 | ` * CVT_BOOL: * * *` |
|        - |  2844 | ` *` |
|        - |  2845 | ` * Force the top of the stack to be a boolean.` |
|        - |  2846 | ` */` |
|        5 |  2847 | `case PH7_OP_CVT_BOOL:` |
|        - |  2848 | `#ifdef UNTRUST` |
|        - |  2849 | `	if( pTos < pStack ){` |
|        - |  2850 | `		goto Abort;` |
|        - |  2851 | `	}` |
|        - |  2852 | `#endif` |
|       11 |  2853 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  2854 | `		PH7_MemObjToBool(pTos);` |
|        3 |  2855 | `	}` |
|       11 |  2856 | `	break;` |
|        - |  2857 | `/*` |
|        - |  2858 | ` * CVT_NULL: * * *` |
|        - |  2859 | ` *` |
|        - |  2860 | ` * Nullify the top of the stack.` |
|        - |  2861 | ` */` |
|        3 |  2862 | `case PH7_OP_CVT_NULL:` |
|        - |  2863 | `#ifdef UNTRUST` |
|        - |  2864 | `	if( pTos < pStack ){` |
|        - |  2865 | `		goto Abort;` |
|        - |  2866 | `	}` |
|        - |  2867 | `#endif` |
|        7 |  2868 | `	PH7_MemObjRelease(pTos);` |
|        7 |  2869 | `	break;` |
|        - |  2870 | `/*` |
|        - |  2871 | ` * CVT_NUMC: * * *` |
|        - |  2872 | ` *` |
|        - |  2873 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  2874 | ` */` |
|      ! 0 |  2875 | `case PH7_OP_CVT_NUMC:` |
|        - |  2876 | `#ifdef UNTRUST` |
|        - |  2877 | `	if( pTos < pStack ){` |
|        - |  2878 | `		goto Abort;` |
|        - |  2879 | `	}` |
|        - |  2880 | `#endif` |
|        - |  2881 | `	/* Force a numeric cast */` |
|      ! 0 |  2882 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  2883 | `	break;` |
|        - |  2884 | `/*` |
|        - |  2885 | ` * CVT_ARRAY: * * *` |
|        - |  2886 | ` *` |
|        - |  2887 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  2888 | ` */` |
|       10 |  2889 | `case PH7_OP_CVT_ARRAY:` |
|        - |  2890 | `#ifdef UNTRUST` |
|        - |  2891 | `	if( pTos < pStack ){` |
|        - |  2892 | `		goto Abort;` |
|        - |  2893 | `	}` |
|        - |  2894 | `#endif` |
|        - |  2895 | `	/* Force a hashmap cast */` |
|       21 |  2896 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  2897 | `	if( rc != SXRET_OK ){` |
|        - |  2898 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  2899 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  2900 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  2901 | `	}` |
|       21 |  2902 | `	break;` |
|        - |  2903 | `/*` |
|        - |  2904 | ` * CVT_OBJ: * * *` |
|        - |  2905 | ` *` |
|        - |  2906 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  2907 | ` */` |
|        8 |  2908 | `case PH7_OP_CVT_OBJ:` |
|        - |  2909 | `#ifdef UNTRUST` |
|        - |  2910 | `	if( pTos < pStack ){` |
|        - |  2911 | `		goto Abort;` |
|        - |  2912 | `	}` |
|        - |  2913 | `#endif` |
|       17 |  2914 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  2915 | `		/* Force a 'stdClass()' cast */` |
|       17 |  2916 | `		PH7_MemObjToObject(pTos);` |
|        8 |  2917 | `	}` |
|       17 |  2918 | `	break;` |
|        - |  2919 | `/*` |
|        - |  2920 | ` * ERR_CTRL * * *` |
|        - |  2921 | ` *` |
|        - |  2922 | ` * Error control operator.` |
|        - |  2923 | ` */` |
|    10765 |  2924 | `case PH7_OP_ERR_CTRL:` |
|        - |  2925 | `	/*` |
|        - |  2926 | `	 * TICKET 1433-038:` |
|        - |  2927 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2928 | `	 * use the public API,to control error output.` |
|        - |  2929 | `	 */` |
|    21530 |  2930 | `	break;` |
|        - |  2931 | `/*` |
|        - |  2932 | ` * IS_A * * *` |
|        - |  2933 | ` *` |
|        - |  2934 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  2935 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  2936 | ` * holding a class name or an object).` |
|        - |  2937 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  2938 | ` */` |
|       11 |  2939 | `case PH7_OP_IS_A:{` |
|       23 |  2940 | `	ph7_value *pNos = &pTos[-1];` |
|       23 |  2941 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  2942 | `#ifdef UNTRUST` |
|        - |  2943 | `	if( pNos < pStack ){` |
|        - |  2944 | `		goto Abort;` |
|        - |  2945 | `	}` |
|        - |  2946 | `#endif` |
|       23 |  2947 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       21 |  2948 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       21 |  2949 | `		ph7_class *pClass = 0;` |
|        - |  2950 | `		/* Extract the target class */` |
|       21 |  2951 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  2952 | `			/* Instance already loaded */` |
|      ! 0 |  2953 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       21 |  2954 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2955 | `			/* Perform the query */` |
|       31 |  2956 | `			pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|       20 |  2957 | `				SyBlobLength(&pTos->sBlob),FALSE,0);` |
|       10 |  2958 | `		}` |
|       21 |  2959 | `		if( pClass ){` |
|        - |  2960 | `			/* Perform the query */` |
|       21 |  2961 | `			iRes = VmInstanceOf(pThis->pClass,pClass);` |
|       10 |  2962 | `		}` |
|       10 |  2963 | `	}` |
|        - |  2964 | `	/* Push result */` |
|       23 |  2965 | `	VmPopOperand(&pTos,1);` |
|       23 |  2966 | `	PH7_MemObjRelease(pTos);` |
|       23 |  2967 | `	pTos->x.iVal = iRes;` |
|       23 |  2968 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       23 |  2969 | `	break;` |
|        - |  2970 | `				 }` |
|        - |  2971 |  |
|        - |  2972 | `/*` |
|        - |  2973 | ` * LOADC P1 P2 *` |
|        - |  2974 | ` *` |
|        - |  2975 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  2976 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  2977 | ` */` |
|   712707 |  2978 | `case PH7_OP_LOADC: {` |
|        - |  2979 | `	ph7_value *pObj;` |
|        - |  2980 | `	/* Reserve a room */` |
|  1425460 |  2981 | `	pTos++;` |
|  1425460 |  2982 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1425460 |  2983 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2984 | `			SyHashEntry *pEntry;` |
|        - |  2985 | `			/* Candidate for expansion via user defined callbacks */` |
|    15840 |  2986 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    15840 |  2987 | `			if( pEntry ){` |
|    13376 |  2988 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2989 | `				/* Set a NULL default value */` |
|    13376 |  2990 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    13376 |  2991 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  2992 | `				/* Invoke the callback and deal with the expanded value */` |
|    13376 |  2993 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  2994 | `				/* Mark as constant */` |
|    13376 |  2995 | `				pTos->nIdx = SXU32_HIGH;` |
|    13376 |  2996 | `				break;` |
|        - |  2997 | `			}` |
|     1232 |  2998 | `		}` |
|  1412086 |  2999 | `		PH7_MemObjLoad(pObj,pTos);` |
|   706066 |  3000 | `	}else{` |
|        - |  3001 | `		/* Set a NULL value */` |
|      ! 0 |  3002 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3003 | `	}` |
|        - |  3004 | `	/* Mark as constant */` |
|  1412086 |  3005 | `	pTos->nIdx = SXU32_HIGH;` |
|  1412086 |  3006 | `	break;` |
|        - |  3007 | `				  }` |
|        - |  3008 | `/*` |
|        - |  3009 | ` * LOAD: P1 * P3` |
|        - |  3010 | ` *` |
|        - |  3011 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3012 | ` * from the P3 operand.` |
|        - |  3013 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3014 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3015 | ` */` |
|  1167668 |  3016 | `case PH7_OP_LOAD:{` |
|        - |  3017 | `	ph7_value *pObj;` |
|        - |  3018 | `	SyString sName;` |
|  2335558 |  3019 | `	if( pInstr->p3 == 0 ){` |
|        - |  3020 | `		/* Take the variable name from the top of the stack */` |
|        - |  3021 | `#ifdef UNTRUST` |
|        - |  3022 | `		if( pTos < pStack ){` |
|        - |  3023 | `			goto Abort;` |
|        - |  3024 | `		}` |
|        - |  3025 | `#endif` |
|        - |  3026 | `		/* Force a string cast */` |
|       19 |  3027 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3028 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3029 | `		}` |
|       19 |  3030 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3031 | `	}else{` |
|  2335540 |  3032 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3033 | `		/* Reserve a room for the target object */` |
|  2335540 |  3034 | `		pTos++;` |
|        - |  3035 | `	}` |
|        - |  3036 | `	/* Extract the requested memory object */` |
|  2335558 |  3037 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2335558 |  3038 | `	if( pObj == 0 ){` |
|      484 |  3039 | `		if( pInstr->iP1 ){` |
|        - |  3040 | `			/* Variable not found,load NULL */` |
|      484 |  3041 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3042 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3043 | `			}else{` |
|      484 |  3044 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3045 | `			}` |
|      484 |  3046 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1167911 |  3047 | `			break;` |
|      ! 0 |  3048 | `		}else{` |
|        - |  3049 | `			/* Fatal error */` |
|      ! 0 |  3050 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3051 | `			goto Abort;` |
|        - |  3052 | `		}` |
|        - |  3053 | `	}` |
|        - |  3054 | `	/* Load variable contents */` |
|  2335076 |  3055 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2335076 |  3056 | `	pTos->nIdx = pObj->nIdx;` |
|  2335076 |  3057 | `	break;` |
|        - |  3058 | `				   }` |
|        - |  3059 | `/*` |
|        - |  3060 | ` * LOAD_MAP P1 * *` |
|        - |  3061 | ` *` |
|        - |  3062 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3063 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3064 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3065 | ` */` |
|    15547 |  3066 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3067 | `	ph7_hashmap *pMap;` |
|        - |  3068 | `	/* Allocate a new hashmap instance */` |
|    31096 |  3069 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    31096 |  3070 | `	if( pMap == 0 ){` |
|      ! 0 |  3071 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3072 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3073 | `		goto Abort;` |
|        - |  3074 | `	}` |
|    31096 |  3075 | `	if( pInstr->iP1 > 0 ){` |
|     1866 |  3076 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3077 | `		/* Perform the insertion */` |
|     5550 |  3078 | `		while( pEntry < pTos ){` |
|     3686 |  3079 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3080 | `				/* Insertion by reference */` |
|      142 |  3081 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3082 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3083 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3084 | `					);` |
|       48 |  3085 | `			}else{` |
|        - |  3086 | `				/* Standard insertion */` |
|     5387 |  3087 | `				PH7_HashmapInsert(pMap,` |
|     3590 |  3088 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     1795 |  3089 | `					&pEntry[1]` |
|        - |  3090 | `				);` |
|        - |  3091 | `			}` |
|        - |  3092 | `			/* Next pair on the stack */` |
|     3686 |  3093 | `			pEntry += 2;` |
|        2 |  3094 | `		}` |
|        - |  3095 | `		/* Pop P1 elements */` |
|     1866 |  3096 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|      932 |  3097 | `	}` |
|        - |  3098 | `	/* Push the hashmap */` |
|    31096 |  3099 | `	pTos++;` |
|    31096 |  3100 | `	pTos->nIdx = SXU32_HIGH;` |
|    31096 |  3101 | `	pTos->x.pOther = pMap;` |
|    31096 |  3102 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    31096 |  3103 | `	break;` |
|        - |  3104 | `					  }` |
|        - |  3105 | `/*` |
|        - |  3106 | ` * LOAD_LIST: P1 * *` |
|        - |  3107 | ` *` |
|        - |  3108 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3109 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3110 | ` * Caveats:` |
|        - |  3111 | ` *  This implementation support only a single nesting level.` |
|        - |  3112 | ` */` |
|       17 |  3113 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3114 | `	ph7_value *pEntry;` |
|       35 |  3115 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3116 | `		/* Empty list,break immediately */` |
|      ! 0 |  3117 | `		break;` |
|        - |  3118 | `	}` |
|       35 |  3119 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3120 | `#ifdef UNTRUST` |
|        - |  3121 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3122 | `		goto Abort;` |
|        - |  3123 | `	}` |
|        - |  3124 | `#endif` |
|       35 |  3125 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       31 |  3126 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3127 | `		ph7_hashmap_node *pNode;` |
|        - |  3128 | `		ph7_value sKey,*pObj;` |
|        - |  3129 | `		/* Start Copying */` |
|       31 |  3130 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|       99 |  3131 | `		while( pEntry <= pTos ){` |
|       69 |  3132 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       65 |  3133 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       65 |  3134 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       65 |  3135 | `					if( rc == SXRET_OK ){` |
|        - |  3136 | `						/* Store node value */` |
|       65 |  3137 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       33 |  3138 | `					}else{` |
|        - |  3139 | `						/* Nullify the variable */` |
|      ! 0 |  3140 | `						PH7_MemObjRelease(pObj);` |
|        - |  3141 | `					}` |
|       32 |  3142 | `				}` |
|       32 |  3143 | `			}` |
|       69 |  3144 | `			sKey.x.iVal++; /* Next numeric index */` |
|       69 |  3145 | `			pEntry++;` |
|        1 |  3146 | `		}` |
|       15 |  3147 | `	}` |
|       35 |  3148 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       35 |  3149 | `	break;` |
|        - |  3150 | `					   }` |
|        - |  3151 | `/*` |
|        - |  3152 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3153 | ` *` |
|        - |  3154 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3155 | ` * from the stack.` |
|        - |  3156 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3157 | ` * instead.` |
|        - |  3158 | ` */` |
|   185285 |  3159 | `case PH7_OP_LOAD_IDX: {` |
|   370616 |  3160 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   370616 |  3161 | `	ph7_hashmap *pMap = 0;` |
|        - |  3162 | `	ph7_value *pIdx;` |
|   370616 |  3163 | `	pIdx = 0;` |
|   370616 |  3164 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3165 | `		if( !pInstr->iP2){` |
|        - |  3166 | `			/* No available index,load NULL */` |
|      ! 0 |  3167 | `			if( pTos >= pStack ){` |
|      ! 0 |  3168 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3169 | `			}else{` |
|        - |  3170 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3171 | `				pTos++;` |
|      ! 0 |  3172 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3173 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3174 | `			}` |
|        - |  3175 | `			/* Emit a notice */` |
|      ! 0 |  3176 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3177 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3178 | `			break;` |
|        - |  3179 | `		}` |
|      ! 0 |  3180 | `	}else{` |
|   370616 |  3181 | `		pIdx = pTos;` |
|   370616 |  3182 | `		pTos--;` |
|        - |  3183 | `	}` |
|   370616 |  3184 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3185 | `		/* String access */` |
|   293684 |  3186 | `		if( pIdx ){` |
|        - |  3187 | `			sxu32 nOfft;` |
|   293684 |  3188 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3189 | `				/* Force an int cast */` |
|      ! 0 |  3190 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3191 | `			}` |
|   293684 |  3192 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   293684 |  3193 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3194 | `				/* Invalid offset,load null */` |
|      ! 0 |  3195 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3196 | `			}else{` |
|   293684 |  3197 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   293684 |  3198 | `				int c = zData[nOfft];` |
|   293684 |  3199 | `				PH7_MemObjRelease(pTos);` |
|   293684 |  3200 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   293684 |  3201 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3202 | `			}` |
|   146865 |  3203 | `		}else{` |
|        - |  3204 | `			/* No available index,load NULL */` |
|      ! 0 |  3205 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3206 | `		}` |
|   293684 |  3207 | `		break;` |
|        - |  3208 | `	}` |
|    76934 |  3209 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3210 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3211 | `			ph7_value *pObj;` |
|      ! 0 |  3212 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3213 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3214 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3215 | `			}` |
|      ! 0 |  3216 | `		}` |
|      ! 0 |  3217 | `	}` |
|    76934 |  3218 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    76934 |  3219 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3220 | `		/* Point to the hashmap */` |
|    76934 |  3221 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    76934 |  3222 | `		if( pIdx ){` |
|        - |  3223 | `			/* Load the desired entry */` |
|    76934 |  3224 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    38466 |  3225 | `		}` |
|    76934 |  3226 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3227 | `			/* Create a new empty entry */` |
|      ! 0 |  3228 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3229 | `			if( rc == SXRET_OK ){` |
|        - |  3230 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3231 | `				pNode = pMap->pLast;` |
|      ! 0 |  3232 | `			}` |
|      ! 0 |  3233 | `		}` |
|    38466 |  3234 | `	}` |
|    76934 |  3235 | `	if( pIdx ){` |
|    76934 |  3236 | `		PH7_MemObjRelease(pIdx);` |
|    38466 |  3237 | `	}` |
|    76934 |  3238 | `	if( rc == SXRET_OK ){` |
|        - |  3239 | `		/* Load entry contents */` |
|    35938 |  3240 | `		if( pMap->iRef < 2 ){` |
|        - |  3241 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3242 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3243 | `			 */` |
|        7 |  3244 | `			pTos->nIdx = SXU32_HIGH;` |
|        7 |  3245 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|        4 |  3246 | `		}else{` |
|    35932 |  3247 | `			pTos->nIdx = pNode->nValIdx;` |
|    35932 |  3248 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    35932 |  3249 | `			PH7_HashmapUnref(pMap);` |
|        - |  3250 | `		}` |
|    17970 |  3251 | `	}else{` |
|        - |  3252 | `		/* No such entry,load NULL */` |
|    40998 |  3253 | `		PH7_MemObjRelease(pTos);` |
|    40998 |  3254 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3255 | `	}` |
|    76934 |  3256 | `	break;` |
|        - |  3257 | `					  }` |
|        - |  3258 | `/*` |
|        - |  3259 | ` * LOAD_CLOSURE * * P3` |
|        - |  3260 | ` *` |
|        - |  3261 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3262 | ` * name in the stack.` |
|        - |  3263 | ` */` |
|        2 |  3264 | `case PH7_OP_LOAD_CLOSURE:{` |
|        5 |  3265 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        5 |  3266 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3267 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3268 | `		ph7_vm_func *pClosure;` |
|        - |  3269 | `		char *zName;` |
|        - |  3270 | `		sxu32 mLen;` |
|        - |  3271 | `		sxu32 n;` |
|        - |  3272 | `		/* Create a new VM function */` |
|        5 |  3273 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3274 | `		/* Generate an unique closure name */` |
|        5 |  3275 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        5 |  3276 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3277 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3278 | `			goto Abort;` |
|        - |  3279 | `		}` |
|        5 |  3280 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        5 |  3281 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3282 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3283 | `		}` |
|        - |  3284 | `		/* Zero the stucture */` |
|        5 |  3285 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3286 | `		/* Perform a structure assignment on read-only items */` |
|        5 |  3287 | `		pClosure->aArgs = pFunc->aArgs;` |
|        5 |  3288 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        5 |  3289 | `		pClosure->aStatic = pFunc->aStatic;` |
|        5 |  3290 | `		pClosure->iFlags = pFunc->iFlags;` |
|        5 |  3291 | `		pClosure->pUserData = pFunc->pUserData;` |
|        5 |  3292 | `		pClosure->sSignature = pFunc->sSignature;` |
|        5 |  3293 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3294 | `		/* Register the closure */` |
|        5 |  3295 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3296 | `		/* Set up closure environment */` |
|        5 |  3297 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        5 |  3298 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       13 |  3299 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3300 | `			ph7_value *pValue;` |
|        9 |  3301 | `			pEnv = &aEnv[n];` |
|        9 |  3302 | `			sEnv.sName  = pEnv->sName;` |
|        9 |  3303 | `			sEnv.iFlags = pEnv->iFlags;` |
|        9 |  3304 | `			sEnv.nIdx = SXU32_HIGH;` |
|        9 |  3305 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|        9 |  3306 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3307 | `				/* Pass by reference */` |
|      ! 0 |  3308 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3309 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3310 | `					);` |
|      ! 0 |  3311 | `			}` |
|        - |  3312 | `			/* Standard pass by value */` |
|        9 |  3313 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|        9 |  3314 | `			if( pValue ){` |
|        - |  3315 | `				/* Copy imported value */` |
|        5 |  3316 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        2 |  3317 | `			}` |
|        - |  3318 | `			/* Insert the imported variable */` |
|        9 |  3319 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|        5 |  3320 | `		}` |
|        - |  3321 | `		/* Finally,load the closure name on the stack */` |
|        5 |  3322 | `		pTos++;` |
|        5 |  3323 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        2 |  3324 | `	}` |
|        5 |  3325 | `	break;` |
|        - |  3326 | `						 }` |
|        - |  3327 | `/*` |
|        - |  3328 | ` * STORE * P2 P3` |
|        - |  3329 | ` *` |
|        - |  3330 | ` * Perform a store (Assignment) operation.` |
|        - |  3331 | ` */` |
|    94158 |  3332 | `case PH7_OP_STORE: {` |
|        - |  3333 | `	ph7_value *pObj;` |
|        - |  3334 | `	SyString sName;` |
|        - |  3335 | `#ifdef UNTRUST` |
|        - |  3336 | `	if( pTos < pStack ){` |
|        - |  3337 | `		goto Abort;` |
|        - |  3338 | `	}` |
|        - |  3339 | `#endif` |
|   188318 |  3340 | `	if( pInstr->iP2 ){` |
|        - |  3341 | `		sxu32 nIdx;` |
|        - |  3342 | `		/* Member store operation */` |
|     1758 |  3343 | `		nIdx = pTos->nIdx;` |
|     1758 |  3344 | `		VmPopOperand(&pTos,1);` |
|     1758 |  3345 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3346 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3347 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3348 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3349 | `		}else{` |
|        - |  3350 | `			/* Point to the desired memory object */` |
|     1754 |  3351 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     1754 |  3352 | `			if( pObj ){` |
|        - |  3353 | `				/* Perform the store operation */` |
|     1754 |  3354 | `				PH7_MemObjStore(pTos,pObj);` |
|      876 |  3355 | `			}` |
|        - |  3356 | `		}` |
|    95038 |  3357 | `		break;` |
|   186562 |  3358 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3359 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3360 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3361 | `			/* Force a string cast */` |
|      ! 0 |  3362 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3363 | `		}` |
|        7 |  3364 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3365 | `		pTos--;` |
|        - |  3366 | `#ifdef UNTRUST` |
|        - |  3367 | `		if( pTos < pStack  ){` |
|        - |  3368 | `			goto Abort;` |
|        - |  3369 | `		}` |
|        - |  3370 | `#endif` |
|        4 |  3371 | `	}else{` |
|   186556 |  3372 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3373 | `	}` |
|        - |  3374 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   186562 |  3375 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   186562 |  3376 | `	if( pObj == 0 ){` |
|      ! 0 |  3377 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3378 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3379 | `		goto Abort;` |
|        - |  3380 | `	}` |
|   186562 |  3381 | `	if( !pInstr->p3 ){` |
|        7 |  3382 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3383 | `	}` |
|        - |  3384 | `	/* Perform the store operation */` |
|   186562 |  3385 | `	PH7_MemObjStore(pTos,pObj);` |
|   186562 |  3386 | `	break;` |
|        - |  3387 | `				   }` |
|        - |  3388 | `/*` |
|        - |  3389 | ` * STORE_IDX:   P1 * P3` |
|        - |  3390 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3391 | ` *` |
|        - |  3392 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3393 | ` */` |
|    73828 |  3394 | `case PH7_OP_STORE_IDX:` |
|        - |  3395 | `case PH7_OP_STORE_IDX_REF: {` |
|   147658 |  3396 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3397 | `	ph7_value *pKey;` |
|        - |  3398 | `	sxu32 nIdx;` |
|   147658 |  3399 | `	if( pInstr->iP1 ){` |
|        - |  3400 | `		/* Key is next on stack */` |
|    53758 |  3401 | `		pKey = pTos;` |
|    53758 |  3402 | `		pTos--;` |
|    26880 |  3403 | `	}else{` |
|    93902 |  3404 | `		pKey = 0;` |
|        - |  3405 | `	}` |
|   147658 |  3406 | `	nIdx = pTos->nIdx;` |
|   147658 |  3407 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3408 | `		/* Hashmap already loaded */` |
|   147606 |  3409 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   147606 |  3410 | `		if( pMap->iRef < 2 ){` |
|        - |  3411 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3412 | `			pMap->iRef = 2;` |
|      ! 0 |  3413 | `		}` |
|    73804 |  3414 | `	}else{` |
|        - |  3415 | `		ph7_value *pObj;` |
|       53 |  3416 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3417 | `		if( pObj == 0 ){` |
|      ! 0 |  3418 | `			if( pKey ){` |
|      ! 0 |  3419 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3420 | `			}` |
|      ! 0 |  3421 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3422 | `			break;` |
|        - |  3423 | `		}` |
|        - |  3424 | `		/* Phase#1: Load the array */` |
|       53 |  3425 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3426 | `			VmPopOperand(&pTos,1);` |
|       53 |  3427 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3428 | `				/* Force a string cast */` |
|      ! 0 |  3429 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3430 | `			}` |
|       53 |  3431 | `			if( pKey == 0 ){` |
|        - |  3432 | `				/* Append string */` |
|        3 |  3433 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3434 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3435 | `				}` |
|        2 |  3436 | `			}else{` |
|        - |  3437 | `				sxu32 nOfft;` |
|       51 |  3438 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3439 | `					/* Force an int cast */` |
|       51 |  3440 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3441 | `				}` |
|       51 |  3442 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3443 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3444 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3445 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3446 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3447 | `				}else{` |
|      ! 0 |  3448 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3449 | `						/* Perform an append operation */` |
|      ! 0 |  3450 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3451 | `					}` |
|        - |  3452 | `				}` |
|        - |  3453 | `			}` |
|       53 |  3454 | `			if( pKey ){` |
|       51 |  3455 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3456 | `			}` |
|       53 |  3457 | `			break;` |
|      ! 0 |  3458 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3459 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3460 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3461 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3462 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3463 | `				goto Abort;` |
|        - |  3464 | `			}` |
|      ! 0 |  3465 | `		}` |
|      ! 0 |  3466 | `		pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - |  3467 | `	}` |
|   147606 |  3468 | `	VmPopOperand(&pTos,1);` |
|        - |  3469 | `	/* Phase#2: Perform the insertion */` |
|   147606 |  3470 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3471 | `		/* Insertion by reference */` |
|       13 |  3472 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        7 |  3473 | `	}else{` |
|   147594 |  3474 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3475 | `	}` |
|   147606 |  3476 | `	if( pKey ){` |
|    53708 |  3477 | `		PH7_MemObjRelease(pKey);` |
|    26853 |  3478 | `	}` |
|   147606 |  3479 | `	break;` |
|        - |  3480 | `					   }` |
|        - |  3481 | `/*` |
|        - |  3482 | ` * INCR: P1 * *` |
|        - |  3483 | ` *` |
|        - |  3484 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3485 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3486 | ` * the stack and increment after that.` |
|        - |  3487 | ` */` |
|   136680 |  3488 | `case PH7_OP_INCR:` |
|        - |  3489 | `#ifdef UNTRUST` |
|        - |  3490 | `	if( pTos < pStack ){` |
|        - |  3491 | `		goto Abort;` |
|        - |  3492 | `	}` |
|        - |  3493 | `#endif` |
|   273406 |  3494 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   273406 |  3495 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3496 | `			ph7_value *pObj;` |
|   273406 |  3497 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3498 | `				/* Force a numeric cast */` |
|   273406 |  3499 | `				PH7_MemObjToNumeric(pObj);` |
|   273406 |  3500 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3501 | `					pObj->rVal++;` |
|        - |  3502 | `					/* Try to get an integer representation */` |
|      ! 0 |  3503 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3504 | `				}else{` |
|   273406 |  3505 | `					pObj->x.iVal++;` |
|   273406 |  3506 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3507 | `				}` |
|   273406 |  3508 | `				if( pInstr->iP1 ){` |
|        - |  3509 | `					/* Pre-icrement */` |
|       71 |  3510 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3511 | `				}` |
|   136724 |  3512 | `			}` |
|   136726 |  3513 | `		}else{` |
|      ! 0 |  3514 | `			if( pInstr->iP1 ){` |
|        - |  3515 | `				/* Force a numeric cast */` |
|      ! 0 |  3516 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3517 | `				/* Pre-increment */` |
|      ! 0 |  3518 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3519 | `					pTos->rVal++;` |
|        - |  3520 | `					/* Try to get an integer representation */` |
|      ! 0 |  3521 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3522 | `				}else{` |
|      ! 0 |  3523 | `					pTos->x.iVal++;` |
|      ! 0 |  3524 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3525 | `				}` |
|      ! 0 |  3526 | `			}` |
|        - |  3527 | `		}` |
|   136724 |  3528 | `	}` |
|   273406 |  3529 | `	break;` |
|        - |  3530 | `/*` |
|        - |  3531 | ` * DECR: P1 * *` |
|        - |  3532 | ` *` |
|        - |  3533 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3534 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3535 | ` * and decrement after that.` |
|        - |  3536 | ` */` |
|        2 |  3537 | `case PH7_OP_DECR:` |
|        - |  3538 | `#ifdef UNTRUST` |
|        - |  3539 | `	if( pTos < pStack ){` |
|        - |  3540 | `		goto Abort;` |
|        - |  3541 | `	}` |
|        - |  3542 | `#endif` |
|        5 |  3543 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3544 | `		/* Force a numeric cast */` |
|        5 |  3545 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3546 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3547 | `			ph7_value *pObj;` |
|        5 |  3548 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3549 | `				/* Force a numeric cast */` |
|        5 |  3550 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3551 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3552 | `					pObj->rVal--;` |
|        - |  3553 | `					/* Try to get an integer representation */` |
|      ! 0 |  3554 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3555 | `				}else{` |
|        5 |  3556 | `					pObj->x.iVal--;` |
|        5 |  3557 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3558 | `				}` |
|        5 |  3559 | `				if( pInstr->iP1 ){` |
|        - |  3560 | `					/* Pre-icrement */` |
|      ! 0 |  3561 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3562 | `				}` |
|        2 |  3563 | `			}` |
|        3 |  3564 | `		}else{` |
|      ! 0 |  3565 | `			if( pInstr->iP1 ){` |
|        - |  3566 | `				/* Pre-increment */` |
|      ! 0 |  3567 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3568 | `					pTos->rVal--;` |
|        - |  3569 | `					/* Try to get an integer representation */` |
|      ! 0 |  3570 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3571 | `				}else{` |
|      ! 0 |  3572 | `					pTos->x.iVal--;` |
|      ! 0 |  3573 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3574 | `				}` |
|      ! 0 |  3575 | `			}` |
|        - |  3576 | `		}` |
|        2 |  3577 | `	}` |
|        5 |  3578 | `	break;` |
|        - |  3579 | `/*` |
|        - |  3580 | ` * UMINUS: * * *` |
|        - |  3581 | ` *` |
|        - |  3582 | ` * Perform a unary minus operation.` |
|        - |  3583 | ` */` |
|    20095 |  3584 | `case PH7_OP_UMINUS:` |
|        - |  3585 | `#ifdef UNTRUST` |
|        - |  3586 | `	if( pTos < pStack ){` |
|        - |  3587 | `		goto Abort;` |
|        - |  3588 | `	}` |
|        - |  3589 | `#endif` |
|        - |  3590 | `	/* Force a numeric (integer,real or both) cast */` |
|    40192 |  3591 | `	PH7_MemObjToNumeric(pTos);` |
|    40192 |  3592 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       25 |  3593 | `		pTos->rVal = -pTos->rVal;` |
|       12 |  3594 | `	}` |
|    40192 |  3595 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    40168 |  3596 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    20083 |  3597 | `	}` |
|    40192 |  3598 | `	break;` |
|        - |  3599 | `/*` |
|        - |  3600 | ` * UPLUS: * * *` |
|        - |  3601 | ` *` |
|        - |  3602 | ` * Perform a unary plus operation.` |
|        - |  3603 | ` */` |
|       16 |  3604 | `case PH7_OP_UPLUS:` |
|        - |  3605 | `#ifdef UNTRUST` |
|        - |  3606 | `	if( pTos < pStack ){` |
|        - |  3607 | `		goto Abort;` |
|        - |  3608 | `	}` |
|        - |  3609 | `#endif` |
|        - |  3610 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3611 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3612 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3613 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3614 | `	}` |
|       33 |  3615 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3616 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3617 | `	}` |
|       33 |  3618 | `	break;` |
|        - |  3619 | `/*` |
|        - |  3620 | ` * OP_LNOT: * * *` |
|        - |  3621 | ` *` |
|        - |  3622 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3623 | ` * with its complement.` |
|        - |  3624 | ` */` |
|    41028 |  3625 | `case PH7_OP_LNOT:` |
|        - |  3626 | `#ifdef UNTRUST` |
|        - |  3627 | `	if( pTos < pStack ){` |
|        - |  3628 | `		goto Abort;` |
|        - |  3629 | `	}` |
|        - |  3630 | `#endif` |
|        - |  3631 | `	/* Force a boolean cast */` |
|    82102 |  3632 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3633 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3634 | `	}` |
|    82102 |  3635 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    82102 |  3636 | `	break;` |
|        - |  3637 | `/*` |
|        - |  3638 | ` * OP_BITNOT: * * *` |
|        - |  3639 | ` *` |
|        - |  3640 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3641 | ` * with its ones-complement.` |
|        - |  3642 | ` */` |
|        3 |  3643 | `case PH7_OP_BITNOT:` |
|        - |  3644 | `#ifdef UNTRUST` |
|        - |  3645 | `	if( pTos < pStack ){` |
|        - |  3646 | `		goto Abort;` |
|        - |  3647 | `	}` |
|        - |  3648 | `#endif` |
|        - |  3649 | `	/* Force an integer cast */` |
|        7 |  3650 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3651 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3652 | `	}` |
|        7 |  3653 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|        7 |  3654 | `	break;` |
|        - |  3655 | `/* OP_MUL * * *` |
|        - |  3656 | ` * OP_MUL_STORE * * *` |
|        - |  3657 | ` *` |
|        - |  3658 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3659 | ` * and push the result back onto the stack.` |
|        - |  3660 | ` */` |
|     1231 |  3661 | `case PH7_OP_MUL:` |
|        - |  3662 | `case PH7_OP_MUL_STORE: {` |
|     2464 |  3663 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3664 | `	/* Force the operand to be numeric */` |
|        - |  3665 | `#ifdef UNTRUST` |
|        - |  3666 | `	if( pNos < pStack ){` |
|        - |  3667 | `		goto Abort;` |
|        - |  3668 | `	}` |
|        - |  3669 | `#endif` |
|     2464 |  3670 | `	PH7_MemObjToNumeric(pTos);` |
|     2464 |  3671 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3672 | `	/* Perform the requested operation */` |
|     2464 |  3673 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3674 | `		/* Floating point arithemic */` |
|        - |  3675 | `		ph7_real a,b,r;` |
|       17 |  3676 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3677 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3678 | `		}` |
|       17 |  3679 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3680 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3681 | `		}` |
|       17 |  3682 | `		a = pNos->rVal;` |
|       17 |  3683 | `		b = pTos->rVal;` |
|       17 |  3684 | `		r = a * b;` |
|        - |  3685 | `		/* Push the result */` |
|       17 |  3686 | `		pNos->rVal = r;` |
|       17 |  3687 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3688 | `		/* Try to get an integer representation */` |
|       17 |  3689 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3690 | `	}else{` |
|        - |  3691 | `		/* Integer arithmetic */` |
|        - |  3692 | `		sxi64 a,b,r;` |
|     2448 |  3693 | `		a = pNos->x.iVal;` |
|     2448 |  3694 | `		b = pTos->x.iVal;` |
|     2448 |  3695 | `		r = a * b;` |
|        - |  3696 | `		/* Push the result */` |
|     2448 |  3697 | `		pNos->x.iVal = r;` |
|     2448 |  3698 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3699 | `	}` |
|     2464 |  3700 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3701 | `		ph7_value *pObj;` |
|       19 |  3702 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3703 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  3704 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  3705 | `			PH7_MemObjStore(pNos,pObj);` |
|        9 |  3706 | `		}` |
|        9 |  3707 | `	}` |
|     2464 |  3708 | `	VmPopOperand(&pTos,1);` |
|     2464 |  3709 | `	break;` |
|        - |  3710 | `				 }` |
|        - |  3711 | `/* OP_ADD * * *` |
|        - |  3712 | ` *` |
|        - |  3713 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3714 | ` * and push the result back onto the stack.` |
|        - |  3715 | ` */` |
|      420 |  3716 | `case PH7_OP_ADD:{` |
|      842 |  3717 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3718 | `#ifdef UNTRUST` |
|        - |  3719 | `	if( pNos < pStack ){` |
|        - |  3720 | `		goto Abort;` |
|        - |  3721 | `	}` |
|        - |  3722 | `#endif` |
|        - |  3723 | `	/* Perform the addition */` |
|      842 |  3724 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      842 |  3725 | `	VmPopOperand(&pTos,1);` |
|      842 |  3726 | `	break;` |
|        - |  3727 | `				}` |
|        - |  3728 | `/*` |
|        - |  3729 | ` * OP_ADD_STORE * * *` |
|        - |  3730 | ` *` |
|        - |  3731 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3732 | ` * and push the result back onto the stack.` |
|        - |  3733 | ` */` |
|      481 |  3734 | `case PH7_OP_ADD_STORE:{` |
|      963 |  3735 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3736 | `	ph7_value *pObj;` |
|        - |  3737 | `	sxu32 nIdx;` |
|        - |  3738 | `#ifdef UNTRUST` |
|        - |  3739 | `	if( pNos < pStack ){` |
|        - |  3740 | `		goto Abort;` |
|        - |  3741 | `	}` |
|        - |  3742 | `#endif` |
|        - |  3743 | `	/* Perform the addition */` |
|      963 |  3744 | `	nIdx = pTos->nIdx;` |
|      963 |  3745 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3746 | `	/* Peform the store operation */` |
|      963 |  3747 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3748 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      963 |  3749 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      963 |  3750 | `		PH7_MemObjStore(pTos,pObj);` |
|      481 |  3751 | `	}` |
|        - |  3752 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      963 |  3753 | `	PH7_MemObjStore(pTos,pNos);` |
|      963 |  3754 | `	VmPopOperand(&pTos,1);` |
|      963 |  3755 | `	break;` |
|        - |  3756 | `				}` |
|        - |  3757 | `/* OP_SUB * * *` |
|        - |  3758 | ` *` |
|        - |  3759 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3760 | ` * first (what was next on the stack) from the second (the` |
|        - |  3761 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3762 | ` */` |
|      294 |  3763 | `case PH7_OP_SUB: {` |
|      589 |  3764 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3765 | `#ifdef UNTRUST` |
|        - |  3766 | `	if( pNos < pStack ){` |
|        - |  3767 | `		goto Abort;` |
|        - |  3768 | `	}` |
|        - |  3769 | `#endif` |
|      589 |  3770 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3771 | `		/* Floating point arithemic */` |
|        - |  3772 | `		ph7_real a,b,r;` |
|       95 |  3773 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3774 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3775 | `		}` |
|       95 |  3776 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3777 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3778 | `		}` |
|       95 |  3779 | `		a = pNos->rVal;` |
|       95 |  3780 | `		b = pTos->rVal;` |
|       95 |  3781 | `		r = a - b;` |
|        - |  3782 | `		/* Push the result */` |
|       95 |  3783 | `		pNos->rVal = r;` |
|       95 |  3784 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3785 | `		/* Try to get an integer representation */` |
|       95 |  3786 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  3787 | `	}else{` |
|        - |  3788 | `		/* Integer arithmetic */` |
|        - |  3789 | `		sxi64 a,b,r;` |
|      495 |  3790 | `		a = pNos->x.iVal;` |
|      495 |  3791 | `		b = pTos->x.iVal;` |
|      495 |  3792 | `		r = a - b;` |
|        - |  3793 | `		/* Push the result */` |
|      495 |  3794 | `		pNos->x.iVal = r;` |
|      495 |  3795 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3796 | `	}` |
|      589 |  3797 | `	VmPopOperand(&pTos,1);` |
|      589 |  3798 | `	break;` |
|        - |  3799 | `				 }` |
|        - |  3800 | `/* OP_SUB_STORE * * *` |
|        - |  3801 | ` *` |
|        - |  3802 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3803 | ` * first (what was next on the stack) from the second (the` |
|        - |  3804 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3805 | ` */` |
|        1 |  3806 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3807 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3808 | `	ph7_value *pObj;` |
|        - |  3809 | `#ifdef UNTRUST` |
|        - |  3810 | `	if( pNos < pStack ){` |
|        - |  3811 | `		goto Abort;` |
|        - |  3812 | `	}` |
|        - |  3813 | `#endif` |
|        3 |  3814 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3815 | `		/* Floating point arithemic */` |
|        - |  3816 | `		ph7_real a,b,r;` |
|      ! 0 |  3817 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3818 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3819 | `		}` |
|      ! 0 |  3820 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3821 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  3822 | `		}` |
|      ! 0 |  3823 | `		a = pTos->rVal;` |
|      ! 0 |  3824 | `		b = pNos->rVal;` |
|      ! 0 |  3825 | `		r = a - b;` |
|        - |  3826 | `		/* Push the result */` |
|      ! 0 |  3827 | `		pNos->rVal = r;` |
|      ! 0 |  3828 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3829 | `		/* Try to get an integer representation */` |
|      ! 0 |  3830 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  3831 | `	}else{` |
|        - |  3832 | `		/* Integer arithmetic */` |
|        - |  3833 | `		sxi64 a,b,r;` |
|        3 |  3834 | `		a = pTos->x.iVal;` |
|        3 |  3835 | `		b = pNos->x.iVal;` |
|        3 |  3836 | `		r = a - b;` |
|        - |  3837 | `		/* Push the result */` |
|        3 |  3838 | `		pNos->x.iVal = r;` |
|        3 |  3839 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3840 | `	}` |
|        3 |  3841 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3842 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3843 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3844 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3845 | `	}` |
|        3 |  3846 | `	VmPopOperand(&pTos,1);` |
|        3 |  3847 | `	break;` |
|        - |  3848 | `				 }` |
|        - |  3849 |  |
|        - |  3850 | `/*` |
|        - |  3851 | ` * OP_MOD * * *` |
|        - |  3852 | ` *` |
|        - |  3853 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3854 | ` * first (what was next on the stack) from the second (the` |
|        - |  3855 | ` * top of the stack) and push the remainder after division` |
|        - |  3856 | ` * onto the stack.` |
|        - |  3857 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3858 | ` */` |
|      296 |  3859 | `case PH7_OP_MOD:{` |
|      594 |  3860 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3861 | `	sxi64 a,b,r;` |
|        - |  3862 | `#ifdef UNTRUST` |
|        - |  3863 | `	if( pNos < pStack ){` |
|        - |  3864 | `		goto Abort;` |
|        - |  3865 | `	}` |
|        - |  3866 | `#endif` |
|        - |  3867 | `	/* Force the operands to be integer */` |
|      594 |  3868 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3869 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3870 | `	}` |
|      594 |  3871 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  3872 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  3873 | `	}` |
|        - |  3874 | `	/* Perform the requested operation */` |
|      594 |  3875 | `	a = pNos->x.iVal;` |
|      594 |  3876 | `	b = pTos->x.iVal;` |
|      594 |  3877 | `	if( b == 0 ){` |
|        3 |  3878 | `		r = 0;` |
|        3 |  3879 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3880 | `		/* goto Abort; */` |
|        2 |  3881 | `	}else{` |
|      591 |  3882 | `		r = a%b;` |
|        - |  3883 | `	}` |
|        - |  3884 | `	/* Push the result */` |
|      594 |  3885 | `	pNos->x.iVal = r;` |
|      594 |  3886 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      594 |  3887 | `	VmPopOperand(&pTos,1);` |
|      594 |  3888 | `	break;` |
|        - |  3889 | `				}` |
|        - |  3890 | `/*` |
|        - |  3891 | ` * OP_MOD_STORE * * *` |
|        - |  3892 | ` *` |
|        - |  3893 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3894 | ` * first (what was next on the stack) from the second (the` |
|        - |  3895 | ` * top of the stack) and push the remainder after division` |
|        - |  3896 | ` * onto the stack.` |
|        - |  3897 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3898 | ` */` |
|        1 |  3899 | `case PH7_OP_MOD_STORE: {` |
|        3 |  3900 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3901 | `	ph7_value *pObj;` |
|        - |  3902 | `	sxi64 a,b,r;` |
|        - |  3903 | `#ifdef UNTRUST` |
|        - |  3904 | `	if( pNos < pStack ){` |
|        - |  3905 | `		goto Abort;` |
|        - |  3906 | `	}` |
|        - |  3907 | `#endif` |
|        - |  3908 | `	/* Force the operands to be integer */` |
|        3 |  3909 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3910 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3911 | `	}` |
|        3 |  3912 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3913 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3914 | `	}` |
|        - |  3915 | `	/* Perform the requested operation */` |
|        3 |  3916 | `	a = pTos->x.iVal;` |
|        3 |  3917 | `	b = pNos->x.iVal;` |
|        3 |  3918 | `	if( b == 0 ){` |
|      ! 0 |  3919 | `		r = 0;` |
|      ! 0 |  3920 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3921 | `		/* goto Abort; */` |
|      ! 0 |  3922 | `	}else{` |
|        3 |  3923 | `		r = a%b;` |
|        - |  3924 | `	}` |
|        - |  3925 | `	/* Push the result */` |
|        3 |  3926 | `	pNos->x.iVal = r;` |
|        3 |  3927 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  3928 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3929 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3930 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3931 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3932 | `	}` |
|        3 |  3933 | `	VmPopOperand(&pTos,1);` |
|        3 |  3934 | `	break;` |
|        - |  3935 | `				}` |
|        - |  3936 | `/*` |
|        - |  3937 | ` * OP_DIV * * *` |
|        - |  3938 | ` *` |
|        - |  3939 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3940 | ` * first (what was next on the stack) from the second (the` |
|        - |  3941 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3942 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3943 | ` */` |
|       28 |  3944 | `case PH7_OP_DIV:{` |
|       58 |  3945 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3946 | `	ph7_real a,b,r;` |
|        - |  3947 | `#ifdef UNTRUST` |
|        - |  3948 | `	if( pNos < pStack ){` |
|        - |  3949 | `		goto Abort;` |
|        - |  3950 | `	}` |
|        - |  3951 | `#endif` |
|        - |  3952 | `	/* Force the operands to be real */` |
|       58 |  3953 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  3954 | `		PH7_MemObjToReal(pTos);` |
|       26 |  3955 | `	}` |
|       58 |  3956 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  3957 | `		PH7_MemObjToReal(pNos);` |
|        9 |  3958 | `	}` |
|        - |  3959 | `	/* Perform the requested operation */` |
|       58 |  3960 | `	a = pNos->rVal;` |
|       58 |  3961 | `	b = pTos->rVal;` |
|       58 |  3962 | `	if( b == 0 ){` |
|        - |  3963 | `		/* Division by zero */` |
|        3 |  3964 | `		pNos->rVal = 0;` |
|        3 |  3965 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  3966 | `		/* goto Abort; */` |
|        2 |  3967 | `	}else{` |
|       55 |  3968 | `		r = a/b;` |
|        - |  3969 | `		/* Push the result */` |
|       55 |  3970 | `		pNos->rVal = r;` |
|       55 |  3971 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3972 | `		/* Try to get an integer representation */` |
|       55 |  3973 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3974 | `	}` |
|       58 |  3975 | `	VmPopOperand(&pTos,1);` |
|       58 |  3976 | `	break;` |
|        - |  3977 | `				}` |
|        - |  3978 | `/*` |
|        - |  3979 | ` * OP_DIV_STORE * * *` |
|        - |  3980 | ` *` |
|        - |  3981 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3982 | ` * first (what was next on the stack) from the second (the` |
|        - |  3983 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3984 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3985 | ` */` |
|        1 |  3986 | `case PH7_OP_DIV_STORE:{` |
|        3 |  3987 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3988 | `	ph7_value *pObj;` |
|        - |  3989 | `	ph7_real a,b,r;` |
|        - |  3990 | `#ifdef UNTRUST` |
|        - |  3991 | `	if( pNos < pStack ){` |
|        - |  3992 | `		goto Abort;` |
|        - |  3993 | `	}` |
|        - |  3994 | `#endif` |
|        - |  3995 | `	/* Force the operands to be real */` |
|        3 |  3996 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3997 | `		PH7_MemObjToReal(pTos);` |
|        1 |  3998 | `	}` |
|        3 |  3999 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4000 | `		PH7_MemObjToReal(pNos);` |
|        1 |  4001 | `	}` |
|        - |  4002 | `	/* Perform the requested operation */` |
|        3 |  4003 | `	a = pTos->rVal;` |
|        3 |  4004 | `	b = pNos->rVal;` |
|        3 |  4005 | `	if( b == 0 ){` |
|        - |  4006 | `		/* Division by zero */` |
|      ! 0 |  4007 | `		r = 0;` |
|      ! 0 |  4008 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4009 | `		/* goto Abort; */` |
|      ! 0 |  4010 | `	}else{` |
|        3 |  4011 | `		r = a/b;` |
|        - |  4012 | `		/* Push the result */` |
|        3 |  4013 | `		pNos->rVal = r;` |
|        3 |  4014 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4015 | `		/* Try to get an integer representation */` |
|        3 |  4016 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4017 | `	}` |
|        3 |  4018 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4019 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4020 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4021 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4022 | `	}` |
|        3 |  4023 | `	VmPopOperand(&pTos,1);` |
|        3 |  4024 | `	break;` |
|        - |  4025 | `				}` |
|        - |  4026 | `/* OP_BAND * * *` |
|        - |  4027 | ` *` |
|        - |  4028 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4029 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4030 | ` * two elements.` |
|        - |  4031 | `*/` |
|        - |  4032 | `/* OP_BOR * * *` |
|        - |  4033 | ` *` |
|        - |  4034 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4035 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4036 | ` * two elements.` |
|        - |  4037 | ` */` |
|        - |  4038 | `/* OP_BXOR * * *` |
|        - |  4039 | ` *` |
|        - |  4040 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4041 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4042 | ` * two elements.` |
|        - |  4043 | ` */` |
|       19 |  4044 | `case PH7_OP_BAND:` |
|        - |  4045 | `case PH7_OP_BOR:` |
|        - |  4046 | `case PH7_OP_BXOR:{` |
|       39 |  4047 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4048 | `	sxi64 a,b,r;` |
|        - |  4049 | `#ifdef UNTRUST` |
|        - |  4050 | `	if( pNos < pStack ){` |
|        - |  4051 | `		goto Abort;` |
|        - |  4052 | `	}` |
|        - |  4053 | `#endif` |
|        - |  4054 | `	/* Force the operands to be integer */` |
|       39 |  4055 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4056 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4057 | `	}` |
|       39 |  4058 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4059 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4060 | `	}` |
|        - |  4061 | `	/* Perform the requested operation */` |
|       39 |  4062 | `	a = pNos->x.iVal;` |
|       39 |  4063 | `	b = pTos->x.iVal;` |
|       39 |  4064 | `	switch(pInstr->iOp){` |
|        6 |  4065 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4066 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4067 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4068 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        7 |  4069 | `	case PH7_OP_BAND_STORE:` |
|        7 |  4070 | `	case PH7_OP_BAND:` |
|       15 |  4071 | `	default:          r = a&b; break;` |
|        - |  4072 | `	}` |
|        - |  4073 | `	/* Push the result */` |
|       39 |  4074 | `	pNos->x.iVal = r;` |
|       39 |  4075 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       39 |  4076 | `	VmPopOperand(&pTos,1);` |
|       39 |  4077 | `	break;` |
|        - |  4078 | `				 }` |
|        - |  4079 | `/* OP_BAND_STORE * * *` |
|        - |  4080 | ` *` |
|        - |  4081 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4082 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4083 | ` * two elements.` |
|        - |  4084 | `*/` |
|        - |  4085 | `/* OP_BOR_STORE * * *` |
|        - |  4086 | ` *` |
|        - |  4087 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4088 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4089 | ` * two elements.` |
|        - |  4090 | ` */` |
|        - |  4091 | `/* OP_BXOR_STORE * * *` |
|        - |  4092 | ` *` |
|        - |  4093 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4094 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4095 | ` * two elements.` |
|        - |  4096 | ` */` |
|        7 |  4097 | `case PH7_OP_BAND_STORE:` |
|        - |  4098 | `case PH7_OP_BOR_STORE:` |
|        - |  4099 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4100 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4101 | `	ph7_value *pObj;` |
|        - |  4102 | `	sxi64 a,b,r;` |
|        - |  4103 | `#ifdef UNTRUST` |
|        - |  4104 | `	if( pNos < pStack ){` |
|        - |  4105 | `		goto Abort;` |
|        - |  4106 | `	}` |
|        - |  4107 | `#endif` |
|        - |  4108 | `	/* Force the operands to be integer */` |
|       15 |  4109 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4110 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4111 | `	}` |
|       15 |  4112 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4113 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4114 | `	}` |
|        - |  4115 | `	/* Perform the requested operation */` |
|       15 |  4116 | `	a = pTos->x.iVal;` |
|       15 |  4117 | `	b = pNos->x.iVal;` |
|       15 |  4118 | `	switch(pInstr->iOp){` |
|        2 |  4119 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4120 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4121 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4122 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4123 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4124 | `	case PH7_OP_BAND:` |
|        5 |  4125 | `	default:          r = a&b; break;` |
|        - |  4126 | `	}` |
|        - |  4127 | `	/* Push the result */` |
|       15 |  4128 | `	pNos->x.iVal = r;` |
|       15 |  4129 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4130 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4131 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4132 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4133 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4134 | `	}` |
|       15 |  4135 | `	VmPopOperand(&pTos,1);` |
|       15 |  4136 | `	break;` |
|        - |  4137 | `				 }` |
|        - |  4138 | `/* OP_SHL * * *` |
|        - |  4139 | ` *` |
|        - |  4140 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4141 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4142 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4143 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4144 | ` */` |
|        - |  4145 | `/* OP_SHR * * *` |
|        - |  4146 | ` *` |
|        - |  4147 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4148 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4149 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4150 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4151 | ` */` |
|        9 |  4152 | `case PH7_OP_SHL:` |
|        - |  4153 | `case PH7_OP_SHR: {` |
|       19 |  4154 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4155 | `	sxi64 a,r;` |
|        - |  4156 | `	sxi32 b;` |
|        - |  4157 | `#ifdef UNTRUST` |
|        - |  4158 | `	if( pNos < pStack ){` |
|        - |  4159 | `		goto Abort;` |
|        - |  4160 | `	}` |
|        - |  4161 | `#endif` |
|        - |  4162 | `	/* Force the operands to be integer */` |
|       19 |  4163 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4164 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4165 | `	}` |
|       19 |  4166 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4167 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4168 | `	}` |
|        - |  4169 | `	/* Perform the requested operation */` |
|       19 |  4170 | `	a = pNos->x.iVal;` |
|       19 |  4171 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4172 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4173 | `		r = a << b;` |
|        6 |  4174 | `	}else{` |
|        9 |  4175 | `		r = a >> b;` |
|        - |  4176 | `	}` |
|        - |  4177 | `	/* Push the result */` |
|       19 |  4178 | `	pNos->x.iVal = r;` |
|       19 |  4179 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4180 | `	VmPopOperand(&pTos,1);` |
|       19 |  4181 | `	break;` |
|        - |  4182 | `				 }` |
|        - |  4183 | `/*  OP_SHL_STORE * * *` |
|        - |  4184 | ` *` |
|        - |  4185 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4186 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4187 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4188 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4189 | ` */` |
|        - |  4190 | `/* OP_SHR_STORE * * *` |
|        - |  4191 | ` *` |
|        - |  4192 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4193 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4194 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4195 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4196 | ` */` |
|        7 |  4197 | `case PH7_OP_SHL_STORE:` |
|        - |  4198 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4199 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4200 | `	ph7_value *pObj;` |
|        - |  4201 | `	sxi64 a,r;` |
|        - |  4202 | `	sxi32 b;` |
|        - |  4203 | `#ifdef UNTRUST` |
|        - |  4204 | `	if( pNos < pStack ){` |
|        - |  4205 | `		goto Abort;` |
|        - |  4206 | `	}` |
|        - |  4207 | `#endif` |
|        - |  4208 | `	/* Force the operands to be integer */` |
|       15 |  4209 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4210 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4211 | `	}` |
|       15 |  4212 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4213 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4214 | `	}` |
|        - |  4215 | `	/* Perform the requested operation */` |
|       15 |  4216 | `	a = pTos->x.iVal;` |
|       15 |  4217 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4218 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4219 | `		r = a << b;` |
|        4 |  4220 | `	}else{` |
|        9 |  4221 | `		r = a >> b;` |
|        - |  4222 | `	}` |
|        - |  4223 | `	/* Push the result */` |
|       15 |  4224 | `	pNos->x.iVal = r;` |
|       15 |  4225 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4226 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4227 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4228 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4229 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4230 | `	}` |
|       15 |  4231 | `	VmPopOperand(&pTos,1);` |
|       15 |  4232 | `	break;` |
|        - |  4233 | `				 }` |
|        - |  4234 | `/* CAT:  P1 * *` |
|        - |  4235 | ` *` |
|        - |  4236 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4237 | ` * back.` |
|        - |  4238 | ` */` |
|    55394 |  4239 | `case PH7_OP_CAT:{` |
|        - |  4240 | `	ph7_value *pNos,*pCur;` |
|   110790 |  4241 | `	if( pInstr->iP1 < 1 ){` |
|    84018 |  4242 | `		pNos = &pTos[-1];` |
|    42010 |  4243 | `	}else{` |
|    26774 |  4244 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4245 | `	}` |
|        - |  4246 | `#ifdef UNTRUST` |
|        - |  4247 | `	if( pNos < pStack ){` |
|        - |  4248 | `		goto Abort;` |
|        - |  4249 | `	}` |
|        - |  4250 | `#endif` |
|        - |  4251 | `	/* Force a string cast */` |
|   110790 |  4252 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      786 |  4253 | `		PH7_MemObjToString(pNos);` |
|      392 |  4254 | `	}` |
|   110790 |  4255 | `	pCur = &pNos[1];` |
|   223110 |  4256 | `	while( pCur <= pTos ){` |
|   112322 |  4257 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50310 |  4258 | `			PH7_MemObjToString(pCur);` |
|    25154 |  4259 | `		}` |
|        - |  4260 | `		/* Perform the concatenation */` |
|   112322 |  4261 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   112284 |  4262 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    56141 |  4263 | `		}` |
|   112322 |  4264 | `		SyBlobRelease(&pCur->sBlob);` |
|   112322 |  4265 | `		pCur++;` |
|        2 |  4266 | `	}` |
|   110790 |  4267 | `	pTos = pNos;` |
|   110790 |  4268 | `	break;` |
|        - |  4269 | `				}` |
|        - |  4270 | `/*  CAT_STORE: * * *` |
|        - |  4271 | ` *` |
|        - |  4272 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4273 | ` * back.` |
|        - |  4274 | ` */` |
|     2067 |  4275 | `case PH7_OP_CAT_STORE:{` |
|     4135 |  4276 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4277 | `	ph7_value *pObj;` |
|        - |  4278 | `#ifdef UNTRUST` |
|        - |  4279 | `	if( pNos < pStack ){` |
|        - |  4280 | `		goto Abort;` |
|        - |  4281 | `	}` |
|        - |  4282 | `#endif` |
|     4135 |  4283 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4284 | `		/* Force a string cast */` |
|      ! 0 |  4285 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4286 | `	}` |
|     4135 |  4287 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4288 | `		/* Force a string cast */` |
|      ! 0 |  4289 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4290 | `	}` |
|        - |  4291 | `	/* Perform the concatenation (Reverse order) */` |
|     4135 |  4292 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     4135 |  4293 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     2067 |  4294 | `	}` |
|        - |  4295 | `	/* Perform the store operation */` |
|     4135 |  4296 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4297 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     4135 |  4298 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     4135 |  4299 | `		PH7_MemObjStore(pTos,pObj);` |
|     2067 |  4300 | `	}` |
|     4135 |  4301 | `	PH7_MemObjStore(pTos,pNos);` |
|     4135 |  4302 | `	VmPopOperand(&pTos,1);` |
|     4135 |  4303 | `	break;` |
|        - |  4304 | `				}` |
|        - |  4305 | `/* OP_AND: * * *` |
|        - |  4306 | ` *` |
|        - |  4307 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4308 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4309 | ` * stack.` |
|        - |  4310 | ` */` |
|        - |  4311 | `/* OP_OR: * * *` |
|        - |  4312 | ` *` |
|        - |  4313 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4314 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4315 | ` * stack.` |
|        - |  4316 | ` */` |
|    86257 |  4317 | `case PH7_OP_LAND:` |
|        - |  4318 | `case PH7_OP_LOR: {` |
|   172560 |  4319 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4320 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4321 | `#ifdef UNTRUST` |
|        - |  4322 | `	if( pNos < pStack ){` |
|        - |  4323 | `		goto Abort;` |
|        - |  4324 | `	}` |
|        - |  4325 | `#endif` |
|        - |  4326 | `	/* Force a boolean cast */` |
|   172560 |  4327 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4328 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4329 | `	}` |
|   172560 |  4330 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4331 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4332 | `	}` |
|   172560 |  4333 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   172560 |  4334 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   172560 |  4335 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4336 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    88878 |  4337 | `		v1 = and_logic[v1*3+v2];` |
|    44462 |  4338 | `	}else{` |
|        - |  4339 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|    83684 |  4340 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4341 | `	}` |
|   172560 |  4342 | `	if( v1 == 2 ){` |
|      ! 0 |  4343 | `		v1 = 1;` |
|      ! 0 |  4344 | `	}` |
|   172560 |  4345 | `	VmPopOperand(&pTos,1);` |
|   172560 |  4346 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   172560 |  4347 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   172560 |  4348 | `	break;` |
|        - |  4349 | `				 }` |
|        - |  4350 | `/* OP_LXOR: * * *` |
|        - |  4351 | ` *` |
|        - |  4352 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4353 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4354 | ` * stack.` |
|        - |  4355 | ` * According to the PHP language reference manual:` |
|        - |  4356 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4357 | ` *  TRUE,but not both.` |
|        - |  4358 | ` */` |
|        5 |  4359 | `case PH7_OP_LXOR:{` |
|       11 |  4360 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4361 | `	sxi32 v = 0;` |
|        - |  4362 | `#ifdef UNTRUST` |
|        - |  4363 | `	if( pNos < pStack ){` |
|        - |  4364 | `		goto Abort;` |
|        - |  4365 | `	}` |
|        - |  4366 | `#endif` |
|        - |  4367 | `	/* Force a boolean cast */` |
|       11 |  4368 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4369 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4370 | `	}` |
|       11 |  4371 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4372 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4373 | `	}` |
|       11 |  4374 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4375 | `		v = 1;` |
|        3 |  4376 | `	}` |
|       11 |  4377 | `	VmPopOperand(&pTos,1);` |
|       11 |  4378 | `	pTos->x.iVal = v;` |
|       11 |  4379 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4380 | `	break;` |
|        - |  4381 | `				 }` |
|        - |  4382 | `/* OP_EQ P1 P2 P3` |
|        - |  4383 | ` *` |
|        - |  4384 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4385 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4386 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4387 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4388 | ` */` |
|        - |  4389 | `/* OP_NEQ P1 P2 P3` |
|        - |  4390 | ` *` |
|        - |  4391 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4392 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4393 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4394 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4395 | ` */` |
|     3451 |  4396 | `case PH7_OP_EQ:` |
|        - |  4397 | `case PH7_OP_NEQ: {` |
|     6904 |  4398 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4399 | `	/* Perform the comparison and act accordingly */` |
|        - |  4400 | `#ifdef UNTRUST` |
|        - |  4401 | `	if( pNos < pStack ){` |
|        - |  4402 | `		goto Abort;` |
|        - |  4403 | `	}` |
|        - |  4404 | `#endif` |
|     6904 |  4405 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     6904 |  4406 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       11 |  4407 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     6899 |  4408 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     6868 |  4409 | `		rc = rc == 0;` |
|     3435 |  4410 | `	}else{` |
|       28 |  4411 | `		rc = rc != 0;` |
|        - |  4412 | `	}` |
|     6904 |  4413 | `	VmPopOperand(&pTos,1);` |
|     6904 |  4414 | `	if( !pInstr->iP2 ){` |
|        - |  4415 | `		/* Push comparison result without taking the jump */` |
|     6904 |  4416 | `		PH7_MemObjRelease(pTos);` |
|     6904 |  4417 | `		pTos->x.iVal = rc;` |
|        - |  4418 | `		/* Invalidate any prior representation */` |
|     6904 |  4419 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3453 |  4420 | `	}else{` |
|      ! 0 |  4421 | `		if( rc ){` |
|        - |  4422 | `			/* Jump to the desired location */` |
|      ! 0 |  4423 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4424 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4425 | `		}` |
|        - |  4426 | `	}` |
|     6904 |  4427 | `	break;` |
|        - |  4428 | `				 }` |
|        - |  4429 | `/* OP_TEQ P1 P2 *` |
|        - |  4430 | ` *` |
|        - |  4431 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4432 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4433 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4434 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4435 | ` */` |
|   108615 |  4436 | `case PH7_OP_TEQ: {` |
|   217232 |  4437 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4438 | `	/* Perform the comparison and act accordingly */` |
|        - |  4439 | `#ifdef UNTRUST` |
|        - |  4440 | `	if( pNos < pStack ){` |
|        - |  4441 | `		goto Abort;` |
|        - |  4442 | `	}` |
|        - |  4443 | `#endif` |
|   217232 |  4444 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   217232 |  4445 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4446 | `		rc = 0;` |
|        2 |  4447 | `	}else{` |
|   217230 |  4448 | `		rc = rc == 0;` |
|        - |  4449 | `	}` |
|   217232 |  4450 | `	VmPopOperand(&pTos,1);` |
|   217232 |  4451 | `	if( !pInstr->iP2 ){` |
|        - |  4452 | `		/* Push comparison result without taking the jump */` |
|   217232 |  4453 | `		PH7_MemObjRelease(pTos);` |
|   217232 |  4454 | `		pTos->x.iVal = rc;` |
|        - |  4455 | `		/* Invalidate any prior representation */` |
|   217232 |  4456 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   108617 |  4457 | `	}else{` |
|      ! 0 |  4458 | `		if( rc ){` |
|        - |  4459 | `			/* Jump to the desired location */` |
|      ! 0 |  4460 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4461 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4462 | `		}` |
|        - |  4463 | `	}` |
|   217232 |  4464 | `	break;` |
|        - |  4465 | `				 }` |
|        - |  4466 | `/* OP_TNE P1 P2 *` |
|        - |  4467 | ` *` |
|        - |  4468 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4469 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4470 | ` * instruction.` |
|        - |  4471 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4472 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4473 | ` *` |
|        - |  4474 | ` */` |
|    85999 |  4475 | `case PH7_OP_TNE: {` |
|   172000 |  4476 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4477 | `	/* Perform the comparison and act accordingly */` |
|        - |  4478 | `#ifdef UNTRUST` |
|        - |  4479 | `	if( pNos < pStack ){` |
|        - |  4480 | `		goto Abort;` |
|        - |  4481 | `	}` |
|        - |  4482 | `#endif` |
|   172000 |  4483 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   172000 |  4484 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4485 | `		rc = 1;` |
|        2 |  4486 | `	}else{` |
|   171998 |  4487 | `		rc = rc != 0;` |
|        - |  4488 | `	}` |
|   172000 |  4489 | `	VmPopOperand(&pTos,1);` |
|   172000 |  4490 | `	if( !pInstr->iP2 ){` |
|        - |  4491 | `		/* Push comparison result without taking the jump */` |
|   172000 |  4492 | `		PH7_MemObjRelease(pTos);` |
|   172000 |  4493 | `		pTos->x.iVal = rc;` |
|        - |  4494 | `		/* Invalidate any prior representation */` |
|   172000 |  4495 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    86001 |  4496 | `	}else{` |
|      ! 0 |  4497 | `		if( rc ){` |
|        - |  4498 | `			/* Jump to the desired location */` |
|      ! 0 |  4499 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4500 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4501 | `		}` |
|        - |  4502 | `	}` |
|   172000 |  4503 | `	break;` |
|        - |  4504 | `				 }` |
|        - |  4505 | `/* OP_LT P1 P2 P3` |
|        - |  4506 | ` *` |
|        - |  4507 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4508 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4509 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4510 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4511 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4512 | ` *` |
|        - |  4513 | ` */` |
|        - |  4514 | `/* OP_LE P1 P2 P3` |
|        - |  4515 | ` *` |
|        - |  4516 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4517 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4518 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4519 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4520 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4521 | ` *` |
|        - |  4522 | ` */` |
|    97012 |  4523 | `case PH7_OP_LT:` |
|        - |  4524 | `case PH7_OP_LE: {` |
|   194070 |  4525 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4526 | `	/* Perform the comparison and act accordingly */` |
|        - |  4527 | `#ifdef UNTRUST` |
|        - |  4528 | `	if( pNos < pStack ){` |
|        - |  4529 | `		goto Abort;` |
|        - |  4530 | `	}` |
|        - |  4531 | `#endif` |
|   194070 |  4532 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   194070 |  4533 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4534 | `		rc = 0;` |
|   194066 |  4535 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      395 |  4536 | `		rc = rc < 1;` |
|      198 |  4537 | `	}else{` |
|   193668 |  4538 | `		rc = rc < 0;` |
|        - |  4539 | `	}` |
|   194070 |  4540 | `	VmPopOperand(&pTos,1);` |
|   194070 |  4541 | `	if( !pInstr->iP2 ){` |
|        - |  4542 | `		/* Push comparison result without taking the jump */` |
|   194070 |  4543 | `		PH7_MemObjRelease(pTos);` |
|   194070 |  4544 | `		pTos->x.iVal = rc;` |
|        - |  4545 | `		/* Invalidate any prior representation */` |
|   194070 |  4546 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    97058 |  4547 | `	}else{` |
|      ! 0 |  4548 | `		if( rc ){` |
|        - |  4549 | `			/* Jump to the desired location */` |
|      ! 0 |  4550 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4551 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4552 | `		}` |
|        - |  4553 | `	}` |
|   194070 |  4554 | `	break;` |
|        - |  4555 | `				}` |
|        - |  4556 | `/* OP_GT P1 P2 P3` |
|        - |  4557 | ` *` |
|        - |  4558 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4559 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4560 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4561 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4562 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4563 | ` *` |
|        - |  4564 | ` */` |
|        - |  4565 | `/* OP_GE P1 P2 P3` |
|        - |  4566 | ` *` |
|        - |  4567 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4568 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4569 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4570 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4571 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4572 | ` *` |
|        - |  4573 | ` */` |
|    39509 |  4574 | `case PH7_OP_GT:` |
|        - |  4575 | `case PH7_OP_GE: {` |
|    79020 |  4576 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4577 | `	/* Perform the comparison and act accordingly */` |
|        - |  4578 | `#ifdef UNTRUST` |
|        - |  4579 | `	if( pNos < pStack ){` |
|        - |  4580 | `		goto Abort;` |
|        - |  4581 | `	}` |
|        - |  4582 | `#endif` |
|    79020 |  4583 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    79020 |  4584 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4585 | `		rc = 0;` |
|    79016 |  4586 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    78864 |  4587 | `		rc = rc >= 0;` |
|    39433 |  4588 | `	}else{` |
|      150 |  4589 | `		rc = rc > 0;` |
|        - |  4590 | `	}` |
|    79020 |  4591 | `	VmPopOperand(&pTos,1);` |
|    79020 |  4592 | `	if( !pInstr->iP2 ){` |
|        - |  4593 | `		/* Push comparison result without taking the jump */` |
|    79020 |  4594 | `		PH7_MemObjRelease(pTos);` |
|    79020 |  4595 | `		pTos->x.iVal = rc;` |
|        - |  4596 | `		/* Invalidate any prior representation */` |
|    79020 |  4597 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    39511 |  4598 | `	}else{` |
|      ! 0 |  4599 | `		if( rc ){` |
|        - |  4600 | `			/* Jump to the desired location */` |
|      ! 0 |  4601 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4602 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4603 | `		}` |
|        - |  4604 | `	}` |
|    79020 |  4605 | `	break;` |
|        - |  4606 | `				}` |
|        - |  4607 | `/* OP_SEQ P1 P2 *` |
|        - |  4608 | ` * Strict string comparison.` |
|        - |  4609 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4610 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4611 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4612 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4613 | ` * use PH7_OP_EQ.` |
|        - |  4614 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4615 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4616 | ` */` |
|        - |  4617 | `/* OP_SNE P1 P2 *` |
|        - |  4618 | ` * Strict string comparison.` |
|        - |  4619 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4620 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4621 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4622 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4623 | ` * use PH7_OP_EQ.` |
|        - |  4624 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4625 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4626 | ` */` |
|       18 |  4627 | `case PH7_OP_SEQ:` |
|        - |  4628 | `case PH7_OP_SNE: {` |
|       38 |  4629 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4630 | `	SyString s1,s2;` |
|        - |  4631 | `	/* Perform the comparison and act accordingly */` |
|        - |  4632 | `#ifdef UNTRUST` |
|        - |  4633 | `	if( pNos < pStack ){` |
|        - |  4634 | `		goto Abort;` |
|        - |  4635 | `	}` |
|        - |  4636 | `#endif` |
|        - |  4637 | `	/* Force a string cast */` |
|       38 |  4638 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4639 | `		PH7_MemObjToString(pTos);` |
|        2 |  4640 | `	}` |
|       38 |  4641 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4642 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4643 | `	}` |
|       38 |  4644 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4645 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4646 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4647 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4648 | `		rc = rc != 0;` |
|      ! 0 |  4649 | `	}else{` |
|       38 |  4650 | `		rc = rc == 0;` |
|        - |  4651 | `	}` |
|       38 |  4652 | `	VmPopOperand(&pTos,1);` |
|       38 |  4653 | `	if( !pInstr->iP2 ){` |
|        - |  4654 | `		/* Push comparison result without taking the jump */` |
|       38 |  4655 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4656 | `		pTos->x.iVal = rc;` |
|        - |  4657 | `		/* Invalidate any prior representation */` |
|       38 |  4658 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4659 | `	}else{` |
|      ! 0 |  4660 | `		if( rc ){` |
|        - |  4661 | `			/* Jump to the desired location */` |
|      ! 0 |  4662 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4663 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4664 | `		}` |
|        - |  4665 | `	}` |
|       38 |  4666 | `	break;` |
|        - |  4667 | `				 }` |
|        - |  4668 | `/*` |
|        - |  4669 | ` * OP_LOAD_REF * * *` |
|        - |  4670 | ` * Push the index of a referenced object on the stack.` |
|        - |  4671 | ` */` |
|       57 |  4672 | `case PH7_OP_LOAD_REF: {` |
|        - |  4673 | `	sxu32 nIdx;` |
|        - |  4674 | `#ifdef UNTRUST` |
|        - |  4675 | `	if( pTos < pStack ){` |
|        - |  4676 | `		goto Abort;` |
|        - |  4677 | `	}` |
|        - |  4678 | `#endif` |
|        - |  4679 | `	/* Extract memory object index */` |
|      115 |  4680 | `	nIdx = pTos->nIdx;` |
|      115 |  4681 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4682 | `		/* Nullify the object */` |
|       95 |  4683 | `		PH7_MemObjRelease(pTos);` |
|        - |  4684 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4685 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4686 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4687 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4688 | `	}` |
|      115 |  4689 | `	break;` |
|        - |  4690 | `					  }` |
|        - |  4691 | `/*` |
|        - |  4692 | ` * OP_STORE_REF * * P3` |
|        - |  4693 | ` * Perform an assignment operation by reference.` |
|        - |  4694 | ` */` |
|       14 |  4695 | ` case PH7_OP_STORE_REF: {` |
|       30 |  4696 | `	 SyString sName = { 0 , 0 };` |
|        - |  4697 | `	 VmFrame *pFrameLocal;` |
|        - |  4698 | `	SyHashEntry *pEntry;` |
|        - |  4699 | `	sxu32 nIdx;` |
|        - |  4700 | `#ifdef UNTRUST` |
|        - |  4701 | `	if( pTos < pStack ){` |
|        - |  4702 | `		goto Abort;` |
|        - |  4703 | `	}` |
|        - |  4704 | `#endif` |
|       30 |  4705 | `	if( pInstr->p3 == 0 ){` |
|        - |  4706 | `		char *zName;` |
|        - |  4707 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4708 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4709 | `			/* Force a string cast */` |
|      ! 0 |  4710 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4711 | `		}` |
|      ! 0 |  4712 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4713 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4714 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4715 | `			if( zName ){` |
|      ! 0 |  4716 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4717 | `			}` |
|      ! 0 |  4718 | `		}` |
|      ! 0 |  4719 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4720 | `		pTos--;` |
|      ! 0 |  4721 | `	}else{` |
|       30 |  4722 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4723 | `	}` |
|       30 |  4724 | `	nIdx = pTos->nIdx;` |
|       30 |  4725 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4726 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4727 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4728 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4729 | `		}else{` |
|        - |  4730 | `			ph7_value *pObj;` |
|        - |  4731 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4732 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4733 | `			if( pObj == 0 ){` |
|      ! 0 |  4734 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4735 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4736 | `				goto Abort;` |
|        - |  4737 | `			}` |
|        - |  4738 | `			/* Perform the store operation */` |
|      ! 0 |  4739 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4740 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4741 | `		}` |
|       30 |  4742 | `	}else if( sName.nByte > 0){` |
|       30 |  4743 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4744 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4745 | `		}else{` |
|       30 |  4746 | `			pFrameLocal = pVm->pFrame;` |
|       50 |  4747 | `			while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4748 | `				/* Safely ignore the exception frame */` |
|       21 |  4749 | `				pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4750 | `			}` |
|        - |  4751 | `			/* Query the local frame */` |
|       30 |  4752 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       30 |  4753 | `			if( pEntry ){` |
|      ! 0 |  4754 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4755 | `			}else{` |
|       30 |  4756 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       30 |  4757 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4758 | `					/* Insert in the $GLOBALS array */` |
|       26 |  4759 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       12 |  4760 | `				}` |
|       30 |  4761 | `				if( rc == SXRET_OK ){` |
|       30 |  4762 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       14 |  4763 | `				}` |
|        - |  4764 | `			}` |
|        - |  4765 | `		}` |
|       14 |  4766 | `	}` |
|       30 |  4767 | `	break;` |
|        - |  4768 | `				 }` |
|        - |  4769 | `/*` |
|        - |  4770 | ` * OP_UPLINK P1 * *` |
|        - |  4771 | ` * Link a variable to the top active VM frame.` |
|        - |  4772 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4773 | ` */` |
|       14 |  4774 | `case PH7_OP_UPLINK: {` |
|       29 |  4775 | `	if( pVm->pFrame->pParent ){` |
|       29 |  4776 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4777 | `		SyString sName;` |
|        - |  4778 | `		/* Perform the link */` |
|       59 |  4779 | `		while( pLink <= pTos ){` |
|       31 |  4780 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4781 | `				/* Force a string cast */` |
|      ! 0 |  4782 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4783 | `			}` |
|       31 |  4784 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       31 |  4785 | `			if( sName.nByte > 0 ){` |
|       31 |  4786 | `				VmFrameLink(&(*pVm),&sName);` |
|       15 |  4787 | `			}` |
|       31 |  4788 | `			pLink++;` |
|        1 |  4789 | `		}` |
|       14 |  4790 | `	}` |
|       29 |  4791 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       29 |  4792 | `	break;` |
|        - |  4793 | `					}` |
|        - |  4794 | `/*` |
|        - |  4795 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4796 | ` * Push an exception in the corresponding container so that` |
|        - |  4797 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4798 | ` */` |
|       10 |  4799 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       22 |  4800 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4801 | `	VmFrame *pFrameLocal;` |
|       22 |  4802 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4803 | `	/* Create the exception frame */` |
|       22 |  4804 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       22 |  4805 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4806 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4807 | `		goto Abort;` |
|        - |  4808 | `	}` |
|        - |  4809 | `	/* Mark the special frame */` |
|       22 |  4810 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       22 |  4811 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4812 | `	/* Point to the frame that trigger the exception */` |
|       22 |  4813 | `	pFrameLocal = pFrameLocal->pParent;` |
|       34 |  4814 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|       13 |  4815 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4816 | `	}` |
|       22 |  4817 | `	pException->pFrame = pFrameLocal;` |
|       22 |  4818 | `	break;` |
|        - |  4819 | `							}` |
|        - |  4820 | `/*` |
|        - |  4821 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4822 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4823 | ` */` |
|        9 |  4824 | `case PH7_OP_POP_EXCEPTION: {` |
|       20 |  4825 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       20 |  4826 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4827 | `		ph7_exception **apException;` |
|        - |  4828 | `		/* Pop the loaded exception */` |
|        7 |  4829 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        7 |  4830 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|        7 |  4831 | `			(void)SySetPop(&pVm->aException);` |
|        3 |  4832 | `		}` |
|        3 |  4833 | `	}` |
|       20 |  4834 | `	pException->pFrame = 0;` |
|        - |  4835 | `	/* Leave the exception frame */` |
|       20 |  4836 | `	VmLeaveFrame(&(*pVm));` |
|       20 |  4837 | `	break;` |
|        - |  4838 | `							}` |
|        - |  4839 |  |
|        - |  4840 | `/*` |
|        - |  4841 | ` * OP_THROW * P2 *` |
|        - |  4842 | ` * Throw an user exception.` |
|        - |  4843 | ` */` |
|       10 |  4844 | `case PH7_OP_THROW: {` |
|       22 |  4845 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       22 |  4846 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  4847 | `#ifdef UNTRUST` |
|        - |  4848 | `	if( pTos < pStack ){` |
|        - |  4849 | `		goto Abort;` |
|        - |  4850 | `	}` |
|        - |  4851 | `#endif` |
|       28 |  4852 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4853 | `		/* Safely ignore the exception frame */` |
|        8 |  4854 | `		pFrameLocal = pFrameLocal->pParent;` |
|        2 |  4855 | `	}` |
|        - |  4856 | `	/* Tell the upper layer that an exception was thrown */` |
|       22 |  4857 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       22 |  4858 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       22 |  4859 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4860 | `		ph7_class *pException;` |
|        - |  4861 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  4862 | `		 */` |
|       22 |  4863 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       22 |  4864 | `		if( pException == 0 \|\| !VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  4865 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  4866 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  4867 | `			if( rc == SXERR_ABORT ){` |
|        - |  4868 | `				/* Abort processing immediately */` |
|      ! 0 |  4869 | `				goto Abort;` |
|        - |  4870 | `			}` |
|      ! 0 |  4871 | `		}else{` |
|        - |  4872 | `			/* Throw the exception */` |
|       22 |  4873 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       22 |  4874 | `			if( rc == SXERR_ABORT ){` |
|        - |  4875 | `				/* Abort processing immediately */` |
|        7 |  4876 | `				goto Abort;` |
|        - |  4877 | `			}` |
|        - |  4878 | `		}` |
|        9 |  4879 | `	}else{` |
|        - |  4880 | `		/* Expecting a class instance */` |
|      ! 0 |  4881 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  4882 | `		if( rc == SXERR_ABORT ){` |
|        - |  4883 | `			/* Abort processing immediately */` |
|      ! 0 |  4884 | `			goto Abort;` |
|        - |  4885 | `		}` |
|        - |  4886 | `	}` |
|        - |  4887 | `	/* Pop the top entry */` |
|       16 |  4888 | `	VmPopOperand(&pTos,1);` |
|        - |  4889 | `	/* Perform an unconditional jump */` |
|       16 |  4890 | `	pc = nJump - 1;` |
|       16 |  4891 | `	break;` |
|        - |  4892 | `				   }` |
|        - |  4893 | `/*` |
|        - |  4894 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  4895 | ` * Prepare a foreach step.` |
|        - |  4896 | ` */` |
|     4138 |  4897 | `case PH7_OP_FOREACH_INIT: {` |
|     8278 |  4898 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4899 | `	void *pName;` |
|        - |  4900 | `#ifdef UNTRUST` |
|        - |  4901 | `	if( pTos < pStack ){` |
|        - |  4902 | `		goto Abort;` |
|        - |  4903 | `	}` |
|        - |  4904 | `#endif` |
|     8278 |  4905 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4906 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  4907 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4908 | `			/* Force a string cast */` |
|      ! 0 |  4909 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4910 | `		}` |
|        - |  4911 | `		/* Duplicate name */` |
|      ! 0 |  4912 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4913 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4914 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4915 | `		}` |
|      ! 0 |  4916 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4917 | `	}` |
|     8278 |  4918 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  4919 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4920 | `			/* Force a string cast */` |
|      ! 0 |  4921 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4922 | `		}` |
|        - |  4923 | `		/* Duplicate name */` |
|      ! 0 |  4924 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4925 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4926 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4927 | `		}` |
|      ! 0 |  4928 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4929 | `	}` |
|        - |  4930 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     8278 |  4931 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4932 | `		/* Jump out of the loop */` |
|      ! 0 |  4933 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4934 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4935 | `		}` |
|      ! 0 |  4936 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4937 | `	}else{` |
|        - |  4938 | `		ph7_foreach_step *pStep;` |
|     8278 |  4939 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     8278 |  4940 | `		if( pStep == 0 ){` |
|      ! 0 |  4941 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4942 | `			/* Jump out of the loop */` |
|      ! 0 |  4943 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4944 | `		}else{` |
|        - |  4945 | `			/* Zero the structure */` |
|     8278 |  4946 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4947 | `			/* Prepare the step */` |
|     8278 |  4948 | `			pStep->iFlags = pInfo->iFlags;` |
|     8278 |  4949 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     8270 |  4950 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4951 | `				/* Reset the internal loop cursor */` |
|     8270 |  4952 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4953 | `				/* Mark the step */` |
|     8270 |  4954 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     8270 |  4955 | `				pStep->xIter.pMap = pMap;` |
|     8270 |  4956 | `				pMap->iRef++;` |
|     4136 |  4957 | `			}else{` |
|        9 |  4958 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4959 | `				/* Reset the loop cursor */` |
|        9 |  4960 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|        - |  4961 | `				/* Mark the step */` |
|        9 |  4962 | `				pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  4963 | `				pStep->xIter.pThis = pThis;` |
|        9 |  4964 | `				pThis->iRef++;` |
|        - |  4965 | `			}` |
|        - |  4966 | `		}` |
|     8278 |  4967 | `		if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  4968 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  4969 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  4970 | `			/* Jump out of the loop */` |
|      ! 0 |  4971 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4972 | `		}` |
|        - |  4973 | `	}` |
|     8278 |  4974 | `	VmPopOperand(&pTos,1);` |
|     8278 |  4975 | `	break;` |
|        - |  4976 | `						  }` |
|        - |  4977 | `/*` |
|        - |  4978 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  4979 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  4980 | ` */` |
|    67548 |  4981 | `case PH7_OP_FOREACH_STEP: {` |
|   135098 |  4982 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4983 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  4984 | `	ph7_value *pValue;` |
|        - |  4985 | `	VmFrame *pFrameLocal;` |
|        - |  4986 | `	/* Peek the last step */` |
|   135098 |  4987 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   135098 |  4988 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   135098 |  4989 | `	pFrameLocal = pVm->pFrame;` |
|   140130 |  4990 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4991 | `		/* Safely ignore the exception frame */` |
|     5033 |  4992 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4993 | `	}` |
|   135098 |  4994 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   135074 |  4995 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  4996 | `		ph7_hashmap_node *pNode;` |
|        - |  4997 | `		/* Extract the current node value */` |
|   135074 |  4998 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   135074 |  4999 | `		if( pNode == 0 ){` |
|        - |  5000 | `			/* No more entry to process */` |
|     8270 |  5001 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     8270 |  5002 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5003 | `				/* Break the reference with the last element */` |
|        5 |  5004 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  5005 | `			}` |
|        - |  5006 | `			/* Automatically reset the loop cursor */` |
|     8270 |  5007 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5008 | `			/* Cleanup the mess left behind */` |
|     8270 |  5009 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     8270 |  5010 | `			SySetPop(&pInfo->aStep);` |
|     8270 |  5011 | `			PH7_HashmapUnref(pMap);` |
|     4136 |  5012 | `		}else{` |
|   126806 |  5013 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      259 |  5014 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      259 |  5015 | `				if( pKey ){` |
|      259 |  5016 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      129 |  5017 | `				}` |
|      129 |  5018 | `			}` |
|   126806 |  5019 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5020 | `				SyHashEntry *pEntry;` |
|        - |  5021 | `				/* Pass by reference */` |
|       13 |  5022 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       13 |  5023 | `				if( pEntry ){` |
|       13 |  5024 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|        7 |  5025 | `				}else{` |
|      ! 0 |  5026 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5027 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5028 | `				}` |
|        7 |  5029 | `			}else{` |
|        - |  5030 | `				/* Make a copy of the entry value */` |
|   126794 |  5031 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   126794 |  5032 | `				if( pValue ){` |
|   126794 |  5033 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    63396 |  5034 | `				}` |
|        - |  5035 | `			}` |
|        - |  5036 | `		}` |
|    67538 |  5037 | `	}else{` |
|       25 |  5038 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5039 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5040 | `		SyHashEntry *pEntry;` |
|        - |  5041 | `		/* Point to the next attribute */` |
|       29 |  5042 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5043 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5044 | `			/* Check access permission */` |
|       31 |  5045 | `			if( VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5046 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5047 | `					break; /* Access is granted */` |
|        - |  5048 | `			}` |
|        1 |  5049 | `		}` |
|       25 |  5050 | `		if( pEntry == 0 ){` |
|        - |  5051 | `			/* Clean up the mess left behind */` |
|        9 |  5052 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5053 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5054 | `				/* Break the reference with the last element */` |
|        3 |  5055 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5056 | `			}` |
|        9 |  5057 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5058 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5059 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5060 | `		}else{` |
|       17 |  5061 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5062 | `			ph7_value *pAttrValue;` |
|       17 |  5063 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5064 | `				/* Fill with the current attribute name */` |
|       17 |  5065 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5066 | `				if( pKey ){` |
|       17 |  5067 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5068 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5069 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5070 | `				}` |
|        8 |  5071 | `			}` |
|        - |  5072 | `			/* Extract attribute value */` |
|       17 |  5073 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5074 | `			if( pAttrValue ){` |
|       17 |  5075 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5076 | `					/* Pass by reference */` |
|        3 |  5077 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5078 | `					if( pEntry ){` |
|        3 |  5079 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5080 | `					}else{` |
|      ! 0 |  5081 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5082 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5083 | `					}` |
|        2 |  5084 | `				}else{` |
|        - |  5085 | `					/* Make a copy of the attribute value */` |
|       15 |  5086 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5087 | `					if( pValue ){` |
|       15 |  5088 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5089 | `					}` |
|        - |  5090 | `				}` |
|        8 |  5091 | `			}` |
|        - |  5092 | `		}` |
|        - |  5093 | `	}` |
|   135098 |  5094 | `	break;` |
|        - |  5095 | `						  }` |
|        - |  5096 | `/*` |
|        - |  5097 | ` * OP_MEMBER P1 P2` |
|        - |  5098 | ` * Load class attribute/method on the stack.` |
|        - |  5099 | ` */` |
|     1188 |  5100 | `case PH7_OP_MEMBER: {` |
|        - |  5101 | `	ph7_class_instance *pThis;` |
|        - |  5102 | `	ph7_value *pNos;` |
|        - |  5103 | `	SyString sName;` |
|     2378 |  5104 | `	if( !pInstr->iP1 ){` |
|     2320 |  5105 | `		pNos = &pTos[-1];` |
|        - |  5106 | `#ifdef UNTRUST` |
|        - |  5107 | `		if( pNos < pStack ){` |
|        - |  5108 | `			goto Abort;` |
|        - |  5109 | `		}` |
|        - |  5110 | `#endif` |
|     2320 |  5111 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5112 | `			ph7_class *pClass;` |
|        - |  5113 | `			/* Class already instantiated */` |
|     2320 |  5114 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5115 | `			/* Point to the instantiated class */` |
|     2320 |  5116 | `			pClass = pThis->pClass;` |
|        - |  5117 | `			/* Extract attribute name first */` |
|     2320 |  5118 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     2320 |  5119 | `			if( pInstr->iP2 ){` |
|        - |  5120 | `				/* Method call */` |
|      120 |  5121 | `				ph7_class_method *pMeth = 0;` |
|      120 |  5122 | `				if( sName.nByte > 0 ){` |
|        - |  5123 | `					/* Extract the target method */` |
|      120 |  5124 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       59 |  5125 | `				}` |
|      120 |  5126 | `				if( pMeth == 0 ){` |
|      ! 0 |  5127 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5128 | `						&pClass->sName,&sName` |
|        - |  5129 | `						);` |
|        - |  5130 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5131 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5132 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5133 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5134 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5135 | `				}else{` |
|        - |  5136 | `					/* Push method name on the stack */` |
|      120 |  5137 | `					PH7_MemObjRelease(pTos);` |
|      120 |  5138 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      120 |  5139 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5140 | `				}` |
|      120 |  5141 | `				pTos->nIdx = SXU32_HIGH;` |
|       61 |  5142 | `			}else{` |
|        - |  5143 | `				/* Attribute access */` |
|     2202 |  5144 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5145 | `				SyHashEntry *pEntry;` |
|        - |  5146 | `				/* Extract the target attribute */` |
|     2202 |  5147 | `				if( sName.nByte > 0 ){` |
|     2202 |  5148 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     2202 |  5149 | `					if( pEntry ){` |
|        - |  5150 | `						/* Point to the attribute value */` |
|     2200 |  5151 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1099 |  5152 | `					}` |
|     1100 |  5153 | `				}` |
|     2202 |  5154 | `				if( pObjAttr == 0 ){` |
|        - |  5155 | `					/* No such attribute,load null */` |
|        4 |  5156 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5157 | `						&pClass->sName,&sName);` |
|        - |  5158 | `					/* Call the __get magic method if available */` |
|        3 |  5159 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5160 | `				}` |
|     2202 |  5161 | `				VmPopOperand(&pTos,1);` |
|        - |  5162 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5163 | `				 * This is due to the following case:` |
|        - |  5164 | `				 *     (new TestClass())->foo;` |
|        - |  5165 | `				 */` |
|     2202 |  5166 | `				pThis->iRef++;` |
|     2202 |  5167 | `				PH7_MemObjRelease(pTos);` |
|     2202 |  5168 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     2202 |  5169 | `				if( pObjAttr ){` |
|     2200 |  5170 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5171 | `					/* Check attribute access */` |
|     2200 |  5172 | `					if( VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5173 | `						/* Load attribute */` |
|     2200 |  5174 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     2200 |  5175 | `						if( pValue ){` |
|     2200 |  5176 | `							if( pThis->iRef < 2 ){` |
|        - |  5177 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5178 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5179 | `								 */` |
|        3 |  5180 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5181 | `							}else{` |
|        - |  5182 | `								/* Simple load */` |
|     2198 |  5183 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5184 | `							}` |
|     2200 |  5185 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     2198 |  5186 | `								if( pThis->iRef > 1 ){` |
|        - |  5187 | `									/* Load attribute index */` |
|     2196 |  5188 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1097 |  5189 | `								}` |
|     1098 |  5190 | `							}` |
|     1099 |  5191 | `						}` |
|     1099 |  5192 | `					}` |
|     1099 |  5193 | `				}` |
|        - |  5194 | `				/* Safely unreference the object */` |
|     2202 |  5195 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5196 | `			}` |
|     1161 |  5197 | `		}else{` |
|      ! 0 |  5198 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5199 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5200 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5201 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5202 | `		}` |
|     1161 |  5203 | `	}else{` |
|        - |  5204 | `		/* Static member access using class name */` |
|       59 |  5205 | `		pNos = pTos;` |
|       59 |  5206 | `		pThis = 0;` |
|       59 |  5207 | `		if( !pInstr->p3 ){` |
|       57 |  5208 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       57 |  5209 | `			pNos--;` |
|        - |  5210 | `#ifdef UNTRUST` |
|        - |  5211 | `			if( pNos < pStack ){` |
|        - |  5212 | `				goto Abort;` |
|        - |  5213 | `			}` |
|        - |  5214 | `#endif` |
|       29 |  5215 | `		}else{` |
|        - |  5216 | `			/* Attribute name already computed */` |
|        3 |  5217 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5218 | `		}` |
|       59 |  5219 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|       59 |  5220 | `			ph7_class *pClass = 0;` |
|       59 |  5221 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5222 | `				/* Class already instantiated */` |
|      ! 0 |  5223 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5224 | `				pClass = pThis->pClass;` |
|      ! 0 |  5225 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5226 | `			}else{` |
|        - |  5227 | `				/* Try to extract the target class */` |
|       59 |  5228 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       88 |  5229 | `					pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pNos->sBlob),` |
|       29 |  5230 | `						SyBlobLength(&pNos->sBlob),FALSE,0);` |
|       29 |  5231 | `				}` |
|        - |  5232 | `			}` |
|       59 |  5233 | `			if( pClass == 0 ){` |
|        - |  5234 | `				/* Undefined class */` |
|      ! 0 |  5235 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5236 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5237 | `					);` |
|      ! 0 |  5238 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5239 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5240 | `				}` |
|      ! 0 |  5241 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5242 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5243 | `			}else{` |
|       59 |  5244 | `				if( pInstr->iP2 ){` |
|        - |  5245 | `					/* Method call */` |
|       25 |  5246 | `					ph7_class_method *pMeth = 0;` |
|       25 |  5247 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5248 | `						/* Extract the target method */` |
|       25 |  5249 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       12 |  5250 | `					}` |
|       25 |  5251 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5252 | `						if( pMeth ){` |
|      ! 0 |  5253 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5254 | `								&pClass->sName,&sName` |
|        - |  5255 | `								);` |
|      ! 0 |  5256 | `						}else{` |
|      ! 0 |  5257 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5258 | `								&pClass->sName,&sName` |
|        - |  5259 | `								);` |
|        - |  5260 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5261 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5262 | `						}` |
|        - |  5263 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5264 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5265 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5266 | `						}` |
|      ! 0 |  5267 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5268 | `					}else{` |
|        - |  5269 | `						/* Push method name on the stack */` |
|       25 |  5270 | `						PH7_MemObjRelease(pTos);` |
|       25 |  5271 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       25 |  5272 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5273 | `					}` |
|       25 |  5274 | `					pTos->nIdx = SXU32_HIGH;` |
|       13 |  5275 | `				}else{` |
|        - |  5276 | `					/* Attribute access */` |
|       35 |  5277 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5278 | `					/* Check for special ::class pseudo-constant */` |
|       49 |  5279 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       28 |  5280 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5281 | `						/* ::class returns the fully qualified class name */` |
|        - |  5282 | `						/* Pop the attribute name from the stack */` |
|       27 |  5283 | `						if( !pInstr->p3 ){` |
|       27 |  5284 | `							VmPopOperand(&pTos,1);` |
|       13 |  5285 | `						}` |
|       27 |  5286 | `						PH7_MemObjRelease(pTos);` |
|        - |  5287 | `						/* Load the class name */` |
|       27 |  5288 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       27 |  5289 | `						pTos->nIdx = SXU32_HIGH;` |
|       14 |  5290 | `					}else{` |
|        - |  5291 | `						/* Extract the target attribute */` |
|        9 |  5292 | `						if( sName.nByte > 0 ){` |
|        9 |  5293 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        4 |  5294 | `						}` |
|        9 |  5295 | `						if( pAttr == 0 ){` |
|        - |  5296 | `							/* No such attribute,load null */` |
|      ! 0 |  5297 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5298 | `								&pClass->sName,&sName);` |
|        - |  5299 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5300 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5301 | `						}` |
|        - |  5302 | `						/* Pop the attribute name from the stack */` |
|        9 |  5303 | `						if( !pInstr->p3 ){` |
|        7 |  5304 | `							VmPopOperand(&pTos,1);` |
|        3 |  5305 | `						}` |
|        9 |  5306 | `						PH7_MemObjRelease(pTos);` |
|        9 |  5307 | `						pTos->nIdx = SXU32_HIGH;` |
|        9 |  5308 | `						if( pAttr ){` |
|        9 |  5309 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5310 | `								/* Access to a non static attribute */` |
|      ! 0 |  5311 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5312 | `									&pClass->sName,&pAttr->sName` |
|        - |  5313 | `									);` |
|      ! 0 |  5314 | `							}else{` |
|        - |  5315 | `								ph7_value *pValue;` |
|        - |  5316 | `								/* Check if the access to the attribute is allowed */` |
|        9 |  5317 | `								if( VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5318 | `									/* Load the desired attribute */` |
|        9 |  5319 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|        9 |  5320 | `									if( pValue ){` |
|        9 |  5321 | `										PH7_MemObjLoad(pValue,pTos);` |
|        9 |  5322 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5323 | `											/* Load index number */` |
|        3 |  5324 | `											pTos->nIdx = pAttr->nIdx;` |
|        1 |  5325 | `										}` |
|        4 |  5326 | `									}` |
|        4 |  5327 | `								}` |
|        - |  5328 | `							}` |
|        4 |  5329 | `						}` |
|        - |  5330 | `					}` |
|        - |  5331 | `				}` |
|       59 |  5332 | `				if( pThis ){` |
|        - |  5333 | `					/* Safely unreference the object */` |
|      ! 0 |  5334 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5335 | `				}` |
|        - |  5336 | `			}` |
|       30 |  5337 | `		}else{` |
|        - |  5338 | `			/* Pop operands */` |
|      ! 0 |  5339 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5340 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5341 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5342 | `			}` |
|      ! 0 |  5343 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5344 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5345 | `		}` |
|        - |  5346 | `	}` |
|     2378 |  5347 | `	break;` |
|        - |  5348 | `					}` |
|        - |  5349 | `/*` |
|        - |  5350 | ` * OP_NEW P1 * * *` |
|        - |  5351 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5352 | ` */` |
|      250 |  5353 | `case PH7_OP_NEW: {` |
|      502 |  5354 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      502 |  5355 | `	ph7_class *pClass = 0;` |
|        - |  5356 | `	ph7_class_instance *pNew;` |
|      502 |  5357 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5358 | `		/* Try to extract the desired class */` |
|      752 |  5359 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      500 |  5360 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      250 |  5361 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5362 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5363 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5364 | `	}` |
|      502 |  5365 | `	if( pClass == 0 ){` |
|        - |  5366 | `		/* No such class */` |
|      ! 0 |  5367 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined,PH7 is loading NULL",` |
|      ! 0 |  5368 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5369 | `			);` |
|      ! 0 |  5370 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5371 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5372 | `			/* Pop given arguments */` |
|      ! 0 |  5373 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5374 | `		}` |
|      ! 0 |  5375 | `	}else{` |
|        - |  5376 | `		ph7_class_method *pCons;` |
|        - |  5377 | `		/* Create a new class instance */` |
|      502 |  5378 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      502 |  5379 | `		if( pNew == 0 ){` |
|      ! 0 |  5380 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5381 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5382 | `				&pClass->sName` |
|        - |  5383 | `			);` |
|      ! 0 |  5384 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5385 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5386 | `				/* Pop given arguments */` |
|      ! 0 |  5387 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5388 | `			}` |
|      ! 0 |  5389 | `			break;` |
|        - |  5390 | `		}` |
|        - |  5391 | `		/* Check if a constructor is available */` |
|      502 |  5392 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      502 |  5393 | `		if( pCons == 0 ){` |
|      446 |  5394 | `			SyString *pName = &pClass->sName;` |
|        - |  5395 | `			/* Check for a constructor with the same base class name */` |
|      446 |  5396 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      222 |  5397 | `		}` |
|      502 |  5398 | `		if( pCons ){` |
|        - |  5399 | `			/* Call the class constructor */` |
|       58 |  5400 | `			SySetReset(&aArg);` |
|      104 |  5401 | `			while( pArg < pTos ){` |
|       48 |  5402 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       48 |  5403 | `				pArg++;` |
|        2 |  5404 | `			}` |
|       58 |  5405 | `			if( pVm->bErrReport ){` |
|        - |  5406 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5407 | `				sxu32 n;` |
|       15 |  5408 | `				n = SySetUsed(&aArg);` |
|        - |  5409 | `				/* Emit a notice for missing arguments */` |
|       39 |  5410 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       25 |  5411 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       25 |  5412 | `					if( pFuncArg ){` |
|       25 |  5413 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5414 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5415 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5416 | `						}` |
|       12 |  5417 | `					}` |
|       25 |  5418 | `					n++;` |
|        1 |  5419 | `				}` |
|        7 |  5420 | `			}` |
|       58 |  5421 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5422 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       58 |  5423 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5424 | `				pNew->iRef = 1;` |
|      ! 0 |  5425 | `			}` |
|       28 |  5426 | `		}` |
|      502 |  5427 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5428 | `			/* Pop given arguments */` |
|       42 |  5429 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       20 |  5430 | `		}` |
|      502 |  5431 | `		PH7_MemObjRelease(pTos);` |
|      502 |  5432 | `		pTos->x.pOther = pNew;` |
|      502 |  5433 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5434 | `	}` |
|      502 |  5435 | `	break;` |
|        - |  5436 | `				 }` |
|        - |  5437 | `/*` |
|        - |  5438 | ` * OP_CLONE * * *` |
|        - |  5439 | ` * Perfome a clone operation.` |
|        - |  5440 | ` */` |
|       23 |  5441 | `case PH7_OP_CLONE: {` |
|        - |  5442 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5443 | `#ifdef UNTRUST` |
|        - |  5444 | `	if( pTos < pStack ){` |
|        - |  5445 | `		goto Abort;` |
|        - |  5446 | `	}` |
|        - |  5447 | `#endif` |
|        - |  5448 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5449 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5450 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5451 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5452 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5453 | `		break;` |
|        - |  5454 | `	}` |
|        - |  5455 | `	/* Point to the source */` |
|       44 |  5456 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5457 | `	/* Perform the clone operation */` |
|       44 |  5458 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5459 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5460 | `	if( pClone == 0 ){` |
|      ! 0 |  5461 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5462 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5463 | `	}else{` |
|        - |  5464 | `		/* Load the cloned object */` |
|       44 |  5465 | `		pTos->x.pOther = pClone;` |
|       44 |  5466 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5467 | `	}` |
|       44 |  5468 | `	break;` |
|        - |  5469 | `				   }` |
|        - |  5470 | `/*` |
|        - |  5471 | ` * OP_SWITCH * * P3` |
|        - |  5472 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5473 | ` */` |
|       18 |  5474 | `case PH7_OP_SWITCH: {` |
|       38 |  5475 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5476 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5477 | `	ph7_value sValue,sCaseValue;` |
|        - |  5478 | `	sxu32 n,nEntry;` |
|        - |  5479 | `#ifdef UNTRUST` |
|        - |  5480 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5481 | `		goto Abort;` |
|        - |  5482 | `	}` |
|        - |  5483 | `#endif` |
|        - |  5484 | `	/* Point to the case table  */` |
|       38 |  5485 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5486 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5487 | `	/* Select the appropriate case block to execute */` |
|       38 |  5488 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5489 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5490 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5491 | `		pCase = &aCase[n];` |
|       92 |  5492 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5493 | `		/* Execute the case expression first */` |
|       92 |  5494 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5495 | `		/* Compare the two expression */` |
|       92 |  5496 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5497 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5498 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5499 | `		if( rc == 0 ){` |
|        - |  5500 | `			/* Value match,jump to this block */` |
|       38 |  5501 | `			pc = pCase->nStart - 1;` |
|       38 |  5502 | `			break;` |
|        - |  5503 | `		}` |
|       29 |  5504 | `	}` |
|       38 |  5505 | `	VmPopOperand(&pTos,1);` |
|       38 |  5506 | `	if( n >= nEntry ){` |
|        - |  5507 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5508 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5509 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5510 | `		}else{` |
|        - |  5511 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5512 | `			pc = pSwitch->nOut - 1;` |
|        - |  5513 | `		}` |
|      ! 0 |  5514 | `	}` |
|       38 |  5515 | `	break;` |
|        - |  5516 | `					}` |
|        - |  5517 | `/*` |
|        - |  5518 | ` * OP_CALL P1 * *` |
|        - |  5519 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5520 | ` *  function on the stack.` |
|        - |  5521 | ` */` |
|   252129 |  5522 | `case PH7_OP_CALL: {` |
|   504304 |  5523 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5524 | `	SyHashEntry *pEntry;` |
|        - |  5525 | `	SyString sName;` |
|        - |  5526 | `	/* Extract function name */` |
|   504304 |  5527 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5528 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5529 | `			ph7_value sResult;` |
|      ! 0 |  5530 | `			SySetReset(&aArg);` |
|      ! 0 |  5531 | `			while( pArg < pTos ){` |
|      ! 0 |  5532 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5533 | `				pArg++;` |
|      ! 0 |  5534 | `			}` |
|      ! 0 |  5535 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5536 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5537 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5538 | `			SySetReset(&aArg);` |
|        - |  5539 | `			/* Pop given arguments */` |
|      ! 0 |  5540 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5541 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5542 | `			}` |
|        - |  5543 | `			/* Copy result */` |
|      ! 0 |  5544 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5545 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5546 | `		}else{` |
|        3 |  5547 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5548 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5549 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5550 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5551 | `			}else{` |
|        - |  5552 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5553 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5554 | `			}` |
|        - |  5555 | `			/* Pop given arguments */` |
|        3 |  5556 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5557 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5558 | `			}` |
|        - |  5559 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5560 | `			PH7_MemObjRelease(pTos);` |
|        - |  5561 | `		}` |
|   252000 |  5562 | `		break;` |
|        - |  5563 | `	}` |
|   504302 |  5564 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5565 | `	/* Check for a compiled function first */` |
|   504302 |  5566 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|   504302 |  5567 | `	if( pEntry ){` |
|        - |  5568 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5569 | `		ph7_class_instance *pThis;` |
|        - |  5570 | `		ph7_value *pFrameStack;` |
|        - |  5571 | `		ph7_vm_func *pVmFunc;` |
|        - |  5572 | `		ph7_class *pSelf;` |
|        - |  5573 | `		VmFrame *pFrame;` |
|        - |  5574 | `		ph7_value *pObj;` |
|        - |  5575 | `		VmSlot sArg;` |
|        - |  5576 | `		sxu32 n;` |
|        - |  5577 | `		/* initialize fields */` |
|     9806 |  5578 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     9806 |  5579 | `		pThis = 0;` |
|     9806 |  5580 | `		pSelf = 0;` |
|     9806 |  5581 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5582 | `			ph7_class_method *pMeth;` |
|        - |  5583 | `			/* Class method call */` |
|      862 |  5584 | `			ph7_value *pTarget = &pTos[-1];` |
|      862 |  5585 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5586 | `				/* Extract the 'this' pointer */` |
|      862 |  5587 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5588 | `					/* Instance already loaded */` |
|      832 |  5589 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|      832 |  5590 | `					pThis->iRef++;` |
|      832 |  5591 | `					pSelf = pThis->pClass;` |
|      415 |  5592 | `				}` |
|      862 |  5593 | `				if( pSelf == 0 ){` |
|       31 |  5594 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5595 | `						/* "Late Static Binding" class name */` |
|       37 |  5596 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       12 |  5597 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       12 |  5598 | `					}` |
|       31 |  5599 | `					if( pSelf == 0 ){` |
|        7 |  5600 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        3 |  5601 | `					}` |
|       15 |  5602 | `				}` |
|      862 |  5603 | `				if( pThis == 0  ){` |
|       31 |  5604 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       33 |  5605 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5606 | `						/* Safely ignore the exception frame */` |
|        3 |  5607 | `						pFrameLocal = pFrameLocal->pParent;` |
|        1 |  5608 | `					}` |
|       31 |  5609 | `					if( pFrameLocal->pParent ){` |
|        - |  5610 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5611 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5612 | `						if( pThis ){` |
|       13 |  5613 | `							pThis->iRef++;` |
|        6 |  5614 | `						}` |
|        9 |  5615 | `					}` |
|       15 |  5616 | `				}` |
|      862 |  5617 | `				VmPopOperand(&pTos,1);` |
|      862 |  5618 | `				PH7_MemObjRelease(pTos);` |
|        - |  5619 | `				/* Synchronize pointers */` |
|      862 |  5620 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5621 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5622 | `				 * user have already computed the random generated unique class method name` |
|        - |  5623 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5624 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5625 | `				 */` |
|      862 |  5626 | `				while( pArg < pStack ){` |
|      ! 0 |  5627 | `					pArg++;` |
|      ! 0 |  5628 | `				}` |
|      862 |  5629 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5630 | `					/* Check if the call is allowed */` |
|      862 |  5631 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|      862 |  5632 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        5 |  5633 | `						if( !VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5634 | `							/* Pop given arguments */` |
|      ! 0 |  5635 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5636 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5637 | `							}` |
|        - |  5638 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5639 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5640 | `							break;` |
|        - |  5641 | `						}` |
|        2 |  5642 | `					}` |
|      430 |  5643 | `				}` |
|      430 |  5644 | `			}` |
|      430 |  5645 | `		}` |
|        - |  5646 | `		/* Check The recursion limit */` |
|     9806 |  5647 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5648 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5649 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5650 | `				&pVmFunc->sName);` |
|        - |  5651 | `			/* Pop given arguments */` |
|        3 |  5652 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5653 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5654 | `			}` |
|        - |  5655 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5656 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5657 | `			break;` |
|        - |  5658 | `		}` |
|     9804 |  5659 | `		if( pVmFunc->pNextName ){` |
|        - |  5660 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      123 |  5661 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       61 |  5662 | `		}` |
|        - |  5663 | `		/* Extract the formal argument set */` |
|     9804 |  5664 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5665 | `		/* Create a new VM frame  */` |
|     9804 |  5666 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|     9804 |  5667 | `		if( rc != SXRET_OK ){` |
|        - |  5668 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5669 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5670 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5671 | `				&pVmFunc->sName);` |
|        - |  5672 | `			/* Pop given arguments */` |
|      ! 0 |  5673 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5674 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5675 | `			}` |
|        - |  5676 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5677 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5678 | `			break;` |
|        - |  5679 | `		}` |
|     9804 |  5680 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5681 | `			/* Install the '$this' variable */` |
|        - |  5682 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|      842 |  5683 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|      842 |  5684 | `			if( pObj ){` |
|        - |  5685 | `				/* Reflect the change */` |
|      842 |  5686 | `				pObj->x.pOther = pThis;` |
|      842 |  5687 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      420 |  5688 | `			}` |
|      420 |  5689 | `		}` |
|     9804 |  5690 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5691 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5692 | `			/* Install static variables */` |
|      ! 0 |  5693 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5694 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5695 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5696 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5697 | `					/* Initialize the static variables */` |
|      ! 0 |  5698 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5699 | `					if( pObj ){` |
|        - |  5700 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5701 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5702 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5703 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5704 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5705 | `						}` |
|      ! 0 |  5706 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5707 | `					}else{` |
|      ! 0 |  5708 | `						continue;` |
|        - |  5709 | `					}` |
|      ! 0 |  5710 | `				}` |
|        - |  5711 | `				/* Install in the current frame */` |
|      ! 0 |  5712 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5713 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5714 | `			}` |
|      ! 0 |  5715 | `		}` |
|        - |  5716 | `		/* Push arguments in the local frame */` |
|     9804 |  5717 | `		n = 0;` |
|    27666 |  5718 | `		while( pArg < pTos ){` |
|    17864 |  5719 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    17746 |  5720 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5721 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  5722 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  5723 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5724 | `						goto Abort;` |
|        - |  5725 | `					}` |
|      ! 0 |  5726 | `				}` |
|        - |  5727 | `				/* Make sure the given arguments are of the correct type */` |
|    17746 |  5728 | `				if( aFormalArg[n].nType > 0 ){` |
|     1048 |  5729 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5730 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5731 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5732 | `						ph7_class *pClass;` |
|        - |  5733 | `						/* Try to extract the desired class */` |
|      ! 0 |  5734 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5735 | `						if( pClass ){` |
|      ! 0 |  5736 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5737 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5738 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5739 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5740 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5741 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5742 | `								}` |
|      ! 0 |  5743 | `							}else{` |
|        - |  5744 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  5745 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  5746 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  5747 | `								if( ! VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  5748 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5749 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5750 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5751 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5752 | `								}` |
|        - |  5753 | `							}` |
|      ! 0 |  5754 | `						}` |
|     1048 |  5755 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5756 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5757 | `						/* Cast to the desired type */` |
|      ! 0 |  5758 | `						xCast(pArg);` |
|      ! 0 |  5759 | `					}` |
|      523 |  5760 | `				}` |
|    17746 |  5761 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5762 | `					/* Pass by reference */` |
|       42 |  5763 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  5764 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  5765 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  5766 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5767 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  5768 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  5769 | `						}` |
|        - |  5770 | `						/* Switch to pass by value */` |
|      ! 0 |  5771 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  5772 | `					}else{` |
|        - |  5773 | `						SyHashEntry *pRefEntry;` |
|        - |  5774 | `						/* Install the referenced variable in the private function frame */` |
|       42 |  5775 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       42 |  5776 | `						if( pRefEntry == 0 ){` |
|       62 |  5777 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       40 |  5778 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       42 |  5779 | `							sArg.nIdx = pArg->nIdx;` |
|       42 |  5780 | `							sArg.pUserData = 0;` |
|       42 |  5781 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       20 |  5782 | `						}` |
|       42 |  5783 | `						pObj = 0;` |
|        - |  5784 | `					}` |
|       22 |  5785 | `				}else{` |
|        - |  5786 | `					/* Pass by value,make a copy of the given argument */` |
|    17706 |  5787 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5788 | `				}` |
|     8874 |  5789 | `			}else{` |
|        - |  5790 | `				char zName[32];` |
|        - |  5791 | `				SyString sArgName;` |
|        - |  5792 | `				/* Set a dummy name */` |
|      120 |  5793 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      120 |  5794 | `				sArgName.zString = zName;` |
|        - |  5795 | `				/* Annonymous argument */` |
|      120 |  5796 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  5797 | `			}` |
|    17864 |  5798 | `			if( pObj ){` |
|    17824 |  5799 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  5800 | `				/* Insert argument index  */` |
|    17824 |  5801 | `				sArg.nIdx = pObj->nIdx;` |
|    17824 |  5802 | `				sArg.pUserData = 0;` |
|    17824 |  5803 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|     8911 |  5804 | `			}` |
|    17864 |  5805 | `			PH7_MemObjRelease(pArg);` |
|    17864 |  5806 | `			pArg++;` |
|    17864 |  5807 | `			++n;` |
|        2 |  5808 | `		}` |
|        - |  5809 | `		/* Set up closure environment */` |
|     9804 |  5810 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5811 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  5812 | `			ph7_value *pValue;` |
|        - |  5813 | `			sxu32 iEnv;` |
|        9 |  5814 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       25 |  5815 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       17 |  5816 | `				pEnv = &aEnv[iEnv];` |
|       17 |  5817 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  5818 | `					/* Do not install null value */` |
|        9 |  5819 | `					continue;` |
|        - |  5820 | `				}` |
|        9 |  5821 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|        9 |  5822 | `				if( pValue == 0 ){` |
|      ! 0 |  5823 | `					continue;` |
|        - |  5824 | `				}` |
|        - |  5825 | `				/* Invalidate any prior representation */` |
|        9 |  5826 | `				PH7_MemObjRelease(pValue);` |
|        - |  5827 | `				/* Duplicate bound variable value */` |
|        9 |  5828 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        5 |  5829 | `			}` |
|        4 |  5830 | `		}` |
|        - |  5831 | `		/* Process default values */` |
|    11174 |  5832 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1372 |  5833 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1362 |  5834 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1362 |  5835 | `				if( pObj ){` |
|        - |  5836 | `					/* Evaluate the default value and extract it's result */` |
|     1362 |  5837 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1362 |  5838 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5839 | `						goto Abort;` |
|        - |  5840 | `					}` |
|        - |  5841 | `					/* Insert argument index */` |
|     1362 |  5842 | `					sArg.nIdx = pObj->nIdx;` |
|     1362 |  5843 | `					sArg.pUserData = 0;` |
|     1362 |  5844 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  5845 | `					/* Make sure the default argument is of the correct type */` |
|     1362 |  5846 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5847 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5848 | `						/* Cast to the desired type */` |
|      ! 0 |  5849 | `						xCast(pObj);` |
|      ! 0 |  5850 | `					}` |
|      680 |  5851 | `				}` |
|      680 |  5852 | `			}` |
|     1372 |  5853 | `			++n;` |
|        2 |  5854 | `		}` |
|        - |  5855 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  5856 | `		 * does not return anything.` |
|        - |  5857 | `		 */` |
|     9804 |  5858 | `		PH7_MemObjRelease(pTos);` |
|     9804 |  5859 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  5860 | `		/* Allocate a new operand stack and evaluate the function body */` |
|     9804 |  5861 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|     9804 |  5862 | `		if( pFrameStack == 0 ){` |
|        - |  5863 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5864 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5865 | `				&pVmFunc->sName);` |
|      ! 0 |  5866 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5867 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5868 | `			}` |
|      ! 0 |  5869 | `			break;` |
|        - |  5870 | `		}` |
|     9804 |  5871 | `		if( pSelf ){` |
|        - |  5872 | `			/* Push class name */` |
|      860 |  5873 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      429 |  5874 | `		}` |
|        - |  5875 | `		/* Increment nesting level */` |
|     9804 |  5876 | `		pVm->nRecursionDepth++;` |
|        - |  5877 | `		/* Execute function body */` |
|     9804 |  5878 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  5879 | `		/* Decrement nesting level */` |
|     9804 |  5880 | `		pVm->nRecursionDepth--;` |
|     9804 |  5881 | `		if( pSelf ){` |
|        - |  5882 | `			/* Pop class name */` |
|      860 |  5883 | `			(void)SySetPop(&pVm->aSelf);` |
|      429 |  5884 | `		}` |
|        - |  5885 | `		/* Cleanup the mess left behind */` |
|     9804 |  5886 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  5887 | `			/* Return by reference,reflect that */` |
|        9 |  5888 | `			if( n != SXU32_HIGH ){` |
|        9 |  5889 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  5890 | `				sxu32 i;` |
|        - |  5891 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  5892 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  5893 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  5894 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  5895 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5896 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5897 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  5898 | `								&pVmFunc->sName);` |
|      ! 0 |  5899 | `						}` |
|      ! 0 |  5900 | `						n = SXU32_HIGH;` |
|      ! 0 |  5901 | `						break;` |
|        - |  5902 | `					}` |
|        3 |  5903 | `				}` |
|        5 |  5904 | `			}else{` |
|      ! 0 |  5905 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5906 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5907 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  5908 | `						&pVmFunc->sName);` |
|      ! 0 |  5909 | `				}` |
|        - |  5910 | `			}` |
|        9 |  5911 | `			pTos->nIdx = n;` |
|        4 |  5912 | `		}` |
|        - |  5913 | `		/* Cleanup the mess left behind */` |
|     9804 |  5914 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  5915 | `			/* An exception was throw in this frame */` |
|        7 |  5916 | `			pFrame = pFrame->pParent;` |
|        7 |  5917 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  5918 | `				/* Pop the resutlt */` |
|        5 |  5919 | `				VmPopOperand(&pTos,1);` |
|        - |  5920 | `				/* Jump to this destination */` |
|        5 |  5921 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  5922 | `				rc = PH7_OK;` |
|        3 |  5923 | `			}else{` |
|        3 |  5924 | `				if( pFrame->pParent ){` |
|        3 |  5925 | `					rc = PH7_EXCEPTION;` |
|        2 |  5926 | `				}else{` |
|        - |  5927 | `					/* Continue normal execution */` |
|      ! 0 |  5928 | `					rc = PH7_OK;` |
|        - |  5929 | `				}` |
|        - |  5930 | `			}` |
|        3 |  5931 | `		}` |
|        - |  5932 | `		/* Free the operand stack */` |
|     9804 |  5933 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  5934 | `		/* Leave the frame */` |
|     9804 |  5935 | `		VmLeaveFrame(&(*pVm));` |
|     9804 |  5936 | `		if( rc == PH7_ABORT ){` |
|        - |  5937 | `			/* Abort processing immeditaley */` |
|        5 |  5938 | `			goto Abort;` |
|     9800 |  5939 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5940 | `			goto Exception;` |
|        - |  5941 | `		}` |
|     4900 |  5942 | `	}else{` |
|        - |  5943 | `		ph7_user_func *pFunc;` |
|        - |  5944 | `		ph7_context sCtx;` |
|        - |  5945 | `		ph7_value sRet;` |
|        - |  5946 | `		/* Look for an installed foreign function */` |
|   494498 |  5947 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   494498 |  5948 | `		if( pEntry == 0 ){` |
|        - |  5949 | `			/* Call to undefined function */` |
|        5 |  5950 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  5951 | `			/* Pop given arguments */` |
|        5 |  5952 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5953 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5954 | `			}` |
|        - |  5955 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  5956 | `			PH7_MemObjRelease(pTos);` |
|        5 |  5957 | `			break;` |
|        - |  5958 | `		}` |
|   494494 |  5959 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  5960 | `		/* Start collecting function arguments */` |
|   494494 |  5961 | `		SySetReset(&aArg);` |
|  1321770 |  5962 | `		while( pArg < pTos ){` |
|   827278 |  5963 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   827278 |  5964 | `			pArg++;` |
|        2 |  5965 | `		}` |
|        - |  5966 | `		/* Assume a null return value */` |
|   494494 |  5967 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  5968 | `		/* Init the call context */` |
|   494494 |  5969 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  5970 | `		/* Call the foreign function */` |
|   494494 |  5971 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5972 | `		/* Release the call context */` |
|   494494 |  5973 | `		VmReleaseCallContext(&sCtx);` |
|   494494 |  5974 | `		if( rc == PH7_ABORT ){` |
|      255 |  5975 | `			goto Abort;` |
|   494240 |  5976 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5977 | `			goto Exception;` |
|        - |  5978 | `		}` |
|   494238 |  5979 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5980 | `			/* Pop function name and arguments */` |
|   477916 |  5981 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   238979 |  5982 | `		}` |
|        - |  5983 | `		/* Save foreign function return value */` |
|   494238 |  5984 | `		PH7_MemObjStore(&sRet,pTos);` |
|   494238 |  5985 | `		PH7_MemObjRelease(&sRet);` |
|        - |  5986 | `	}` |
|   504034 |  5987 | `	break;` |
|        - |  5988 | `				  }` |
|        - |  5989 | `/*` |
|        - |  5990 | ` * OP_CONSUME: P1 * *` |
|        - |  5991 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  5992 | ` */` |
|     9353 |  5993 | `case PH7_OP_CONSUME: {` |
|    18708 |  5994 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    18708 |  5995 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  5996 |  |
|    18708 |  5997 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    18708 |  5998 | `	pCur = pOut;` |
|        - |  5999 | `	/* Start the consume process  */` |
|    37414 |  6000 | `	while( pOut <= pTos ){` |
|        - |  6001 | `		/* Force a string cast */` |
|    18708 |  6002 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      112 |  6003 | `			PH7_MemObjToString(pOut);` |
|       55 |  6004 | `		}` |
|    18708 |  6005 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6006 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6007 | `			/* Invoke the output consumer callback */` |
|    10060 |  6008 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    10060 |  6009 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6010 | `				/* Increment output length */` |
|     3910 |  6011 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     1954 |  6012 | `			}` |
|    10060 |  6013 | `			SyBlobRelease(&pOut->sBlob);` |
|    10060 |  6014 | `			if( rc == SXERR_ABORT ){` |
|        - |  6015 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6016 | `				goto Abort;` |
|        - |  6017 | `			}` |
|     5029 |  6018 | `		}` |
|    18708 |  6019 | `		pOut++;` |
|        2 |  6020 | `	}` |
|    18708 |  6021 | `	pTos = &pCur[-1];` |
|    18706 |  6022 | `	break;` |
|        - |  6023 | `					 }` |
|        - |  6024 |  |
|        - |  6025 | `		} /* Switch() */` |
|  8563230 |  6026 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6027 | `	} /* For(;;) */` |
|    12215 |  6028 | `Done:` |
|    24432 |  6029 | `	SySetRelease(&aArg);` |
|    24432 |  6030 | `	return SXRET_OK;` |
|      132 |  6031 | `Abort:` |
|      265 |  6032 | `	SySetRelease(&aArg);` |
|      913 |  6033 | `	while( pTos >= pStack ){` |
|      649 |  6034 | `		PH7_MemObjRelease(pTos);` |
|      649 |  6035 | `		pTos--;` |
|        1 |  6036 | `	}` |
|      265 |  6037 | `	return PH7_ABORT;` |
|        2 |  6038 | `Exception:` |
|        5 |  6039 | `	SySetRelease(&aArg);` |
|        9 |  6040 | `	while( pTos >= pStack ){` |
|        5 |  6041 | `		PH7_MemObjRelease(pTos);` |
|        5 |  6042 | `		pTos--;` |
|        1 |  6043 | `	}` |
|        5 |  6044 | `	return PH7_EXCEPTION;` |
|    12351 |  6045 |  |
|        - |  6046 | `/*` |
|        - |  6047 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6048 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6049 | ` * See block-comment on that function for additional information.` |
|        - |  6050 | ` */` |
|    12378 |  6051 | `static sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6052 |  |
|        - |  6053 | `	ph7_value *pStack;` |
|        - |  6054 | `	sxi32 rc;` |
|        - |  6055 | `	/* Allocate a new operand stack */` |
|    12380 |  6056 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    12380 |  6057 | `	if( pStack == 0 ){` |
|      ! 0 |  6058 | `		return SXERR_MEM;` |
|        - |  6059 | `	}` |
|        - |  6060 | `	/* Execute the program */` |
|    12380 |  6061 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  6062 | `	/* Free the operand stack */` |
|    12380 |  6063 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6064 | `	/* Execution result */` |
|    12380 |  6065 | `	return rc;` |
|     6191 |  6066 |  |
|        - |  6067 | `/*` |
|        - |  6068 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6069 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6070 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6071 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6072 | ` * execution ends.` |
|        - |  6073 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6074 | ` * additional information.` |
|        - |  6075 | ` */` |
|     1444 |  6076 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6077 |  |
|        - |  6078 | `	VmShutdownCB *pEntry;` |
|        - |  6079 | `	ph7_value *apArg[10];` |
|        - |  6080 | `	sxu32 n,nEntry;` |
|        - |  6081 | `	int i;` |
|        - |  6082 | `	/* Point to the stack of registered callbacks */` |
|     1446 |  6083 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    15886 |  6084 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    14442 |  6085 | `		apArg[i] = 0;` |
|     7222 |  6086 | `	}` |
|     1448 |  6087 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6088 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6089 | `		if( pEntry ){` |
|        - |  6090 | `			/* Prepare callback arguments if any */` |
|        3 |  6091 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6092 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6093 | `					break;` |
|        - |  6094 | `				}` |
|      ! 0 |  6095 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6096 | `			}` |
|        - |  6097 | `			/* Invoke the callback */` |
|        3 |  6098 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6099 | `			/*` |
|        - |  6100 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6101 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6102 | `			 */` |
|        3 |  6103 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6104 | `			if( pEntry ){` |
|        3 |  6105 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6106 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6107 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6108 | `				}` |
|        1 |  6109 | `			}` |
|        1 |  6110 | `		}` |
|        2 |  6111 | `	}` |
|     1446 |  6112 | `	SySetReset(&pVm->aShutdown);` |
|     1446 |  6113 |  |
|        - |  6114 | `/*` |
|        - |  6115 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6116 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6117 | ` * See block-comment on that function for additional information.` |
|        - |  6118 | ` */` |
|     1452 |  6119 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6120 |  |
|        - |  6121 | `	/* Make sure we are ready to execute this program */` |
|     1454 |  6122 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6123 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6124 | `	}` |
|        - |  6125 | `	/* Set the execution magic number  */` |
|     1454 |  6126 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6127 | `	/* Execute the program */` |
|     1454 |  6128 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6129 | `	/* Invoke any shutdown callbacks */` |
|     1450 |  6130 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6131 | `	/*` |
|        - |  6132 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6133 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6134 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6135 | `	 */` |
|     1450 |  6136 | `	return SXRET_OK;` |
|      728 |  6137 |  |
|        - |  6138 | `/*` |
|        - |  6139 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6140 | ` * the desired message.` |
|        - |  6141 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6142 | ` * in 'api.c' for additional information.` |
|        - |  6143 | ` */` |
|      352 |  6144 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6145 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6146 | `	SyString *pString /* Message to output */` |
|        - |  6147 | `	)` |
|        2 |  6148 |  |
|      354 |  6149 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      354 |  6150 | `	sxi32 rc = SXRET_OK;` |
|        - |  6151 | `	/* Call the output consumer */` |
|      354 |  6152 | `	if( pString->nByte > 0 ){` |
|      354 |  6153 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      354 |  6154 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6155 | `			/* Increment output length */` |
|       17 |  6156 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6157 | `		}` |
|      176 |  6158 | `	}` |
|      354 |  6159 | `	return rc;` |
|        2 |  6160 |  |
|        - |  6161 | `/*` |
|        - |  6162 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6163 | ` * callback to consume the formatted message.` |
|        - |  6164 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6165 | ` * in 'api.c' for additional information.` |
|        - |  6166 | ` */` |
|        2 |  6167 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6168 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6169 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6170 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6171 | `	)` |
|        1 |  6172 |  |
|        3 |  6173 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6174 | `	sxi32 rc = SXRET_OK;` |
|        - |  6175 | `	SyBlob sWorker;` |
|        - |  6176 | `	/* Format the message and call the output consumer */` |
|        3 |  6177 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6178 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6179 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6180 | `		/* Consume the formatted message */` |
|        3 |  6181 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6182 | `	}` |
|        3 |  6183 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6184 | `		/* Increment output length */` |
|      ! 0 |  6185 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6186 | `	}` |
|        - |  6187 | `	/* Release the working buffer */` |
|        3 |  6188 | `	SyBlobRelease(&sWorker);` |
|        3 |  6189 | `	return rc;` |
|        1 |  6190 |  |
|        - |  6191 | `/*` |
|        - |  6192 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6193 | ` * This function never fail and always return a pointer` |
|        - |  6194 | ` * to a null terminated string.` |
|        - |  6195 | ` */` |
|       10 |  6196 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6197 |  |
|       11 |  6198 | `	const char *zOp = "Unknown     ";` |
|       11 |  6199 | `	switch(nOp){` |
|        3 |  6200 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6201 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6202 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6203 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6204 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6205 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6206 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6207 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6208 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6209 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6210 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6211 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6212 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6213 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6214 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6215 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6216 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6217 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6218 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6219 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6220 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6221 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6222 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6223 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6224 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6225 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6226 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6227 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6228 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6229 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6230 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6231 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6232 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6233 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6234 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6235 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6236 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6237 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6238 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6239 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6240 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6241 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6242 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6243 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6244 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6245 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6246 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6247 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6248 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6249 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6250 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6251 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6252 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6253 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6254 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6255 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6256 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6257 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6258 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6259 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6260 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6261 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6262 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6263 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6264 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6265 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6266 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6267 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6268 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6269 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6270 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6271 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6272 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6273 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6274 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6275 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6276 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6277 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6278 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6279 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6280 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6281 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6282 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6283 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6284 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6285 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6286 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6287 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6288 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6289 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6290 | `	default:` |
|      ! 0 |  6291 | `		break;` |
|        - |  6292 | `	}` |
|       11 |  6293 | `	return zOp;` |
|        1 |  6294 |  |
|        - |  6295 | `/*` |
|        - |  6296 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6297 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6298 | ` * is responsible of consuming the generated dump.` |
|        - |  6299 | ` */` |
|        2 |  6300 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6301 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6302 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6303 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6304 | `	)` |
|        1 |  6305 |  |
|        - |  6306 | `	sxi32 rc;` |
|        3 |  6307 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6308 | `	return rc;` |
|        1 |  6309 |  |
|        - |  6310 | `/*` |
|        - |  6311 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6312 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6313 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6314 | ` * in 'compile.c' for additional information.` |
|        - |  6315 | ` */` |
|        8 |  6316 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6317 |  |
|        9 |  6318 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6319 | `	/* Evaluate and expand constant value */` |
|        9 |  6320 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6321 |  |
|        - |  6322 | `/*` |
|        - |  6323 | ` * Section:` |
|        - |  6324 | ` *  Function handling functions.` |
|        - |  6325 | ` * Status:` |
|        - |  6326 | ` *    Stable.` |
|        - |  6327 | ` */` |
|        - |  6328 | `/*` |
|        - |  6329 | ` * int func_num_args(void)` |
|        - |  6330 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6331 | ` * Parameters` |
|        - |  6332 | ` *   None.` |
|        - |  6333 | ` * Return` |
|        - |  6334 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6335 | ` *  or -1 if called from the globe scope.` |
|        - |  6336 | ` */` |
|      850 |  6337 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6338 |  |
|        - |  6339 | `	VmFrame *pFrame;` |
|        - |  6340 | `	ph7_vm *pVm;` |
|        - |  6341 | `	/* Point to the target VM */` |
|      852 |  6342 | `	pVm = pCtx->pVm;` |
|        - |  6343 | `	/* Current frame */` |
|      852 |  6344 | `	pFrame = pVm->pFrame;` |
|      852 |  6345 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6346 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6347 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6348 | `	}` |
|      852 |  6349 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6350 | `		SXUNUSED(nArg);` |
|      ! 0 |  6351 | `		SXUNUSED(apArg);` |
|        - |  6352 | `		/* Global frame,return -1 */` |
|      ! 0 |  6353 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6354 | `		return SXRET_OK;` |
|        - |  6355 | `	}` |
|        - |  6356 | `	/* Total number of arguments passed to the enclosing function */` |
|      852 |  6357 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      852 |  6358 | `	ph7_result_int(pCtx,nArg);` |
|      852 |  6359 | `	return SXRET_OK;` |
|      427 |  6360 |  |
|        - |  6361 | `/*` |
|        - |  6362 | ` * value func_get_arg(int $arg_num)` |
|        - |  6363 | ` *   Return an item from the argument list.` |
|        - |  6364 | ` * Parameters` |
|        - |  6365 | ` *  Argument number(index start from zero).` |
|        - |  6366 | ` * Return` |
|        - |  6367 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6368 | ` */` |
|       22 |  6369 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6370 |  |
|       24 |  6371 | `	ph7_value *pObj = 0;` |
|       24 |  6372 | `	VmSlot *pSlot = 0;` |
|        - |  6373 | `	VmFrame *pFrame;` |
|        - |  6374 | `	ph7_vm *pVm;` |
|        - |  6375 | `	/* Point to the target VM */` |
|       24 |  6376 | `	pVm = pCtx->pVm;` |
|        - |  6377 | `	/* Current frame */` |
|       24 |  6378 | `	pFrame = pVm->pFrame;` |
|       24 |  6379 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6380 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6381 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6382 | `	}` |
|       24 |  6383 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6384 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6385 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6386 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6387 | `		return SXRET_OK;` |
|        - |  6388 | `	}` |
|        - |  6389 | `	/* Extract the desired index */` |
|       21 |  6390 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  6391 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6392 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6393 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6394 | `		return SXRET_OK;` |
|        - |  6395 | `	}` |
|        - |  6396 | `	/* Extract the desired argument */` |
|       21 |  6397 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  6398 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6399 | `			/* Return the desired argument */` |
|       21 |  6400 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  6401 | `		}else{` |
|        - |  6402 | `			/* No such argument,return false */` |
|      ! 0 |  6403 | `			ph7_result_bool(pCtx,0);` |
|        - |  6404 | `		}` |
|       11 |  6405 | `	}else{` |
|        - |  6406 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6407 | `		ph7_result_bool(pCtx,0);` |
|        - |  6408 | `	}` |
|       21 |  6409 | `	return SXRET_OK;` |
|       13 |  6410 |  |
|        - |  6411 | `/*` |
|        - |  6412 | ` * array func_get_args_byref(void)` |
|        - |  6413 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6414 | ` * Parameters` |
|        - |  6415 | ` *  None.` |
|        - |  6416 | ` * Return` |
|        - |  6417 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6418 | ` *  member of the current user-defined function's argument list.` |
|        - |  6419 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6420 | ` * NOTE:` |
|        - |  6421 | ` *  Arguments are returned to the array by reference.` |
|        - |  6422 | ` */` |
|        2 |  6423 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6424 |  |
|        - |  6425 | `	ph7_value *pArray;` |
|        - |  6426 | `	VmFrame *pFrame;` |
|        - |  6427 | `	VmSlot *aSlot;` |
|        - |  6428 | `	sxu32 n;` |
|        - |  6429 | `	/* Point to the current frame */` |
|        3 |  6430 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6431 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6432 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6433 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6434 | `	}` |
|        3 |  6435 | `	if( pFrame->pParent == 0 ){` |
|        - |  6436 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6437 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6438 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6439 | `		return SXRET_OK;` |
|        - |  6440 | `	}` |
|        - |  6441 | `	/* Create a new array */` |
|        3 |  6442 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6443 | `	if( pArray == 0 ){` |
|      ! 0 |  6444 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6445 | `		SXUNUSED(apArg);` |
|      ! 0 |  6446 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6447 | `		return SXRET_OK;` |
|        - |  6448 | `	}` |
|        - |  6449 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6450 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6451 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6452 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6453 | `	}` |
|        - |  6454 | `	/* Return the freshly created array */` |
|        3 |  6455 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6456 | `	return SXRET_OK;` |
|        2 |  6457 |  |
|        - |  6458 | `/*` |
|        - |  6459 | ` * array func_get_args(void)` |
|        - |  6460 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6461 | ` * Parameters` |
|        - |  6462 | ` *  None.` |
|        - |  6463 | ` * Return` |
|        - |  6464 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6465 | ` *  member of the current user-defined function's argument list.` |
|        - |  6466 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6467 | ` */` |
|       46 |  6468 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6469 |  |
|       47 |  6470 | `	ph7_value *pObj = 0;` |
|        - |  6471 | `	ph7_value *pArray;` |
|        - |  6472 | `	VmFrame *pFrame;` |
|        - |  6473 | `	VmSlot *aSlot;` |
|        - |  6474 | `	sxu32 n;` |
|        - |  6475 | `	/* Point to the current frame */` |
|       47 |  6476 | `	pFrame = pCtx->pVm->pFrame;` |
|       47 |  6477 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6478 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6479 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6480 | `	}` |
|       47 |  6481 | `	if( pFrame->pParent == 0 ){` |
|        - |  6482 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6483 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6484 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6485 | `		return SXRET_OK;` |
|        - |  6486 | `	}` |
|        - |  6487 | `	/* Create a new array */` |
|       47 |  6488 | `	pArray = ph7_context_new_array(pCtx);` |
|       47 |  6489 | `	if( pArray == 0 ){` |
|      ! 0 |  6490 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6491 | `		SXUNUSED(apArg);` |
|      ! 0 |  6492 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6493 | `		return SXRET_OK;` |
|        - |  6494 | `	}` |
|        - |  6495 | `	/* Start filling the array with the given arguments */` |
|       47 |  6496 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      143 |  6497 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|       97 |  6498 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|       97 |  6499 | `		if( pObj ){` |
|       97 |  6500 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       48 |  6501 | `		}` |
|       49 |  6502 | `	}` |
|        - |  6503 | `	/* Return the freshly created array */` |
|       47 |  6504 | `	ph7_result_value(pCtx,pArray);` |
|       47 |  6505 | `	return SXRET_OK;` |
|       24 |  6506 |  |
|        - |  6507 | `/*` |
|        - |  6508 | ` * bool function_exists(string $name)` |
|        - |  6509 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6510 | ` * Parameters` |
|        - |  6511 | ` *  The name of the desired function.` |
|        - |  6512 | ` * Return` |
|        - |  6513 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6514 | ` */` |
|     1664 |  6515 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6516 |  |
|        - |  6517 | `	const char *zName;` |
|        - |  6518 | `	ph7_vm *pVm;` |
|        - |  6519 | `	int nLen;` |
|        - |  6520 | `	int res;` |
|     1666 |  6521 | `	if( nArg < 1 ){` |
|        - |  6522 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6523 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6524 | `		return SXRET_OK;` |
|        - |  6525 | `	}` |
|        - |  6526 | `	/* Point to the target VM */` |
|     1666 |  6527 | `	pVm = pCtx->pVm;` |
|        - |  6528 | `	/* Extract the function name */` |
|     1666 |  6529 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6530 | `	/* Assume the function is not defined */` |
|     1666 |  6531 | `	res = 0;` |
|        - |  6532 | `	/* Perform the lookup */` |
|     2496 |  6533 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1660 |  6534 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6535 | `			/* Function is defined */` |
|      212 |  6536 | `			res = 1;` |
|      105 |  6537 | `	}` |
|     1666 |  6538 | `	ph7_result_bool(pCtx,res);` |
|     1666 |  6539 | `	return SXRET_OK;` |
|      834 |  6540 |  |
|        - |  6541 | `/* Forward declaration */` |
|        - |  6542 | `static ph7_class * VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg);` |
|        - |  6543 | `/*` |
|        - |  6544 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6545 | ` * [i.e: Whether it is callable or not].` |
|        - |  6546 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6547 | ` */` |
|    15246 |  6548 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6549 |  |
|    15248 |  6550 | `	int res = 0;` |
|    15248 |  6551 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6552 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6553 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6554 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6555 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6556 | `		if( pMethod && CallInvoke ){` |
|        - |  6557 | `			ph7_value sResult;` |
|        - |  6558 | `			sxi32 rc;` |
|        - |  6559 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6560 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6561 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6562 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6563 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6564 | `			}` |
|      ! 0 |  6565 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6566 | `		}` |
|    15248 |  6567 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       10 |  6568 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       10 |  6569 | `		if( pMap->nEntry > 1 ){` |
|        - |  6570 | `			ph7_class *pClass;` |
|        - |  6571 | `			ph7_value *pV;` |
|        - |  6572 | `			/* Extract the target class */` |
|       10 |  6573 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       10 |  6574 | `			if( pV ){` |
|       10 |  6575 | `				pClass = VmExtractClassFromValue(pVm,pV);` |
|       10 |  6576 | `				if( pClass ){` |
|        - |  6577 | `					ph7_class_method *pMethod;` |
|        - |  6578 | `					/* Extract the target method */` |
|        7 |  6579 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|        7 |  6580 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6581 | `						/* Perform the lookup */` |
|        7 |  6582 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|        7 |  6583 | `						if( pMethod ){` |
|        - |  6584 | `							/* Method is callable */` |
|        5 |  6585 | `							res = 1;` |
|        2 |  6586 | `						}` |
|        3 |  6587 | `					}` |
|        3 |  6588 | `				}` |
|        4 |  6589 | `			}` |
|        6 |  6590 | `		}` |
|    15244 |  6591 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6592 | `		const char *zName;` |
|        - |  6593 | `		int nLen;` |
|        - |  6594 | `		/* Extract the name */` |
|     4432 |  6595 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6596 | `		/* Perform the lookup */` |
|     4438 |  6597 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       12 |  6598 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6599 | `				/* Function is callable */` |
|     4426 |  6600 | `				res = 1;` |
|     2212 |  6601 | `		}` |
|     2215 |  6602 | `	}` |
|    15248 |  6603 | `	return res;` |
|        2 |  6604 |  |
|        - |  6605 | `/*` |
|        - |  6606 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6607 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6608 | ` * Parameters` |
|        - |  6609 | ` * $name` |
|        - |  6610 | ` *    The callback function to check` |
|        - |  6611 | ` * $syntax_only` |
|        - |  6612 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6613 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6614 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6615 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6616 | ` *    a string.` |
|        - |  6617 | ` * Return` |
|        - |  6618 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6619 | ` */` |
|       14 |  6620 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6621 |  |
|        - |  6622 | `	ph7_vm *pVm;` |
|        - |  6623 | `	int res;` |
|       15 |  6624 | `	if( nArg < 1 ){` |
|        - |  6625 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6626 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6627 | `		return SXRET_OK;` |
|        - |  6628 | `	}` |
|        - |  6629 | `	/* Point to the target VM */` |
|       15 |  6630 | `	pVm = pCtx->pVm;` |
|        - |  6631 | `	/* Perform the requested operation */` |
|       15 |  6632 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6633 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6634 | `	return SXRET_OK;` |
|        8 |  6635 |  |
|        - |  6636 | `/*` |
|        - |  6637 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6638 | ` * defined below.` |
|        - |  6639 | ` */` |
|     1046 |  6640 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6641 |  |
|     1047 |  6642 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6643 | `	ph7_value sName;` |
|        - |  6644 | `	sxi32 rc;` |
|        - |  6645 | `	/* Prepare the function name for insertion */` |
|     1047 |  6646 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1047 |  6647 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6648 | `	/* Perform the insertion */` |
|     1047 |  6649 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1047 |  6650 | `	PH7_MemObjRelease(&sName);` |
|     1047 |  6651 | `	return rc;` |
|        1 |  6652 |  |
|        - |  6653 | `/*` |
|        - |  6654 | ` * array get_defined_functions(void)` |
|        - |  6655 | ` *  Returns an array of all defined functions.` |
|        - |  6656 | ` * Parameter` |
|        - |  6657 | ` *  None.` |
|        - |  6658 | ` * Return` |
|        - |  6659 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6660 | ` *  both built-in (internal) and user-defined.` |
|        - |  6661 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6662 | ` *  defined ones using $arr["user"].` |
|        - |  6663 | ` * Note:` |
|        - |  6664 | ` *  NULL is returned on failure.` |
|        - |  6665 | ` */` |
|        2 |  6666 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6667 |  |
|        - |  6668 | `	ph7_value *pArray,*pEntry;` |
|        - |  6669 | `	/* NOTE:` |
|        - |  6670 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6671 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6672 | `	 */` |
|        3 |  6673 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6674 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6675 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6676 | `		SXUNUSED(apArg);` |
|        - |  6677 | `		/* Return NULL */` |
|      ! 0 |  6678 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6679 | `		return SXRET_OK;` |
|        - |  6680 | `	}` |
|        3 |  6681 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6682 | `	if( pEntry == 0 ){` |
|        - |  6683 | `		/* Return NULL */` |
|      ! 0 |  6684 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6685 | `		return SXRET_OK;` |
|        - |  6686 | `	}` |
|        - |  6687 | `	/* Fill with the appropriate information */` |
|        3 |  6688 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6689 | `	/* Create the 'internal' index */` |
|        3 |  6690 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6691 | `	/* Create the user-func array */` |
|        3 |  6692 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6693 | `	if( pEntry == 0 ){` |
|        - |  6694 | `		/* Return NULL */` |
|      ! 0 |  6695 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6696 | `		return SXRET_OK;` |
|        - |  6697 | `	}` |
|        - |  6698 | `	/* Fill with the appropriate information */` |
|        3 |  6699 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6700 | `	/* Create the 'user' index */` |
|        3 |  6701 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6702 | `	/* Return the multi-dimensional array */` |
|        3 |  6703 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6704 | `	return SXRET_OK;` |
|        2 |  6705 |  |
|        - |  6706 | `/*` |
|        - |  6707 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6708 | ` *  Register a function for execution on shutdown.` |
|        - |  6709 | ` * Note` |
|        - |  6710 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6711 | ` *  be called in the same order as they were registered.` |
|        - |  6712 | ` * Parameters` |
|        - |  6713 | ` *  $callback` |
|        - |  6714 | ` *   The shutdown callback to register.` |
|        - |  6715 | ` * $param` |
|        - |  6716 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6717 | ` * Return` |
|        - |  6718 | ` *  Nothing.` |
|        - |  6719 | ` */` |
|        2 |  6720 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6721 |  |
|        - |  6722 | `	VmShutdownCB sEntry;` |
|        - |  6723 | `	int i,j;` |
|        3 |  6724 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6725 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6726 | `		return PH7_OK;` |
|        - |  6727 | `	}` |
|        - |  6728 | `	/* Zero the Entry */` |
|        3 |  6729 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6730 | `	/* Initialize fields */` |
|        3 |  6731 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6732 | `	/* Save the callback name for later invocation name */` |
|        3 |  6733 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6734 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6735 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6736 | `	}` |
|        - |  6737 | `	/* Copy arguments */` |
|        3 |  6738 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6739 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6740 | `			/* Limit reached */` |
|      ! 0 |  6741 | `			break;` |
|        - |  6742 | `		}` |
|      ! 0 |  6743 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  6744 | `	}` |
|        3 |  6745 | `	sEntry.nArg = j;` |
|        - |  6746 | `	/* Install the callback */` |
|        3 |  6747 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  6748 | `	return PH7_OK;` |
|        2 |  6749 |  |
|        - |  6750 | `/*` |
|        - |  6751 | ` * Section:` |
|        - |  6752 | ` *  Class handling functions.` |
|        - |  6753 | ` * Status:` |
|        - |  6754 | ` *    Stable.` |
|        - |  6755 | ` */` |
|        - |  6756 | `/*` |
|        - |  6757 | ` * Extract the top active class. NULL is returned` |
|        - |  6758 | ` * if the class stack is empty.` |
|        - |  6759 | ` */` |
|      300 |  6760 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6761 |  |
|      302 |  6762 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6763 | `	ph7_class **apClass;` |
|      302 |  6764 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6765 | `		/* Empty stack,return NULL */` |
|       15 |  6766 | `		return 0;` |
|        - |  6767 | `	}` |
|        - |  6768 | `	/* Peek the last entry */` |
|      288 |  6769 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      288 |  6770 | `	return apClass[pSet->nUsed - 1];` |
|      152 |  6771 |  |
|        - |  6772 | `/*` |
|        - |  6773 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  6774 | ` *   Get the class that declared the currently executing method.` |
|        - |  6775 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  6776 | ` *` |
|        - |  6777 | ` * Parameters` |
|        - |  6778 | ` *   pVm: Target VM` |
|        - |  6779 | ` *` |
|        - |  6780 | ` * Return` |
|        - |  6781 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  6782 | ` *   - Not executing within a class method` |
|        - |  6783 | ` *` |
|        - |  6784 | ` * Note` |
|        - |  6785 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  6786 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  6787 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  6788 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  6789 | ` *   declaring class.` |
|        - |  6790 | ` */` |
|       18 |  6791 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        1 |  6792 |  |
|       19 |  6793 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  6794 | `	ph7_vm_func *pVmFunc;` |
|        - |  6795 |  |
|        - |  6796 | `	/* Skip exception frames to find the actual method frame */` |
|       19 |  6797 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  6798 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6799 | `	}` |
|        - |  6800 |  |
|        - |  6801 | `	/* Check if we're in a method context */` |
|       19 |  6802 | `	if( pFrame->pParent ){` |
|       15 |  6803 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       15 |  6804 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  6805 | `			/* Return the declaring class */` |
|       15 |  6806 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  6807 | `		}` |
|      ! 0 |  6808 | `	}` |
|        - |  6809 |  |
|        5 |  6810 | `	return 0;` |
|       10 |  6811 |  |
|        - |  6812 |  |
|        - |  6813 | `/*` |
|        - |  6814 | ` * string get_class ([ object $object = NULL ] )` |
|        - |  6815 | ` *   Returns the name of the class of an object` |
|        - |  6816 | ` * Parameters` |
|        - |  6817 | ` *  object` |
|        - |  6818 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|        - |  6819 | ` * Return` |
|        - |  6820 | ` *  The name of the class of which object is an instance.` |
|        - |  6821 | ` *  Returns FALSE if object is not an object.` |
|        - |  6822 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|        - |  6823 | ` */` |
|       18 |  6824 | `static int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6825 |  |
|        - |  6826 | `	ph7_class *pClass;` |
|        - |  6827 | `	SyString *pName;` |
|       20 |  6828 | `	if( nArg < 1 ){` |
|        - |  6829 | `		/* Check if we are inside a class */` |
|      ! 0 |  6830 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|      ! 0 |  6831 | `		if( pClass ){` |
|        - |  6832 | `			/* Point to the class name */` |
|      ! 0 |  6833 | `			pName = &pClass->sName;` |
|      ! 0 |  6834 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|      ! 0 |  6835 | `		}else{` |
|        - |  6836 | `			/* Not inside class,return FALSE */` |
|      ! 0 |  6837 | `			ph7_result_bool(pCtx,0);` |
|        - |  6838 | `		}` |
|      ! 0 |  6839 | `	}else{` |
|        - |  6840 | `		/* Extract the target class */` |
|       20 |  6841 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       20 |  6842 | `		if( pClass ){` |
|       18 |  6843 | `			pName = &pClass->sName;` |
|        - |  6844 | `			/* Return the class name */` |
|       18 |  6845 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|       10 |  6846 | `		}else{` |
|        - |  6847 | `			/* Not a class instance,return FALSE */` |
|        3 |  6848 | `			ph7_result_bool(pCtx,0);` |
|        - |  6849 | `		}` |
|        - |  6850 | `	}` |
|       20 |  6851 | `	return PH7_OK;` |
|        2 |  6852 |  |
|        - |  6853 | `/*` |
|        - |  6854 | ` * string get_parent_class([object $object = NULL ] )` |
|        - |  6855 | ` *   Returns the name of the parent class of an object` |
|        - |  6856 | ` * Parameters` |
|        - |  6857 | ` *  object` |
|        - |  6858 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|        - |  6859 | ` * Return` |
|        - |  6860 | ` *  The name of the parent class of which object is an instance.` |
|        - |  6861 | ` *  Returns FALSE if object is not an object or if the object does` |
|        - |  6862 | ` *  not have a parent.` |
|        - |  6863 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|        - |  6864 | ` */` |
|        8 |  6865 | `static int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6866 |  |
|        - |  6867 | `	ph7_class *pClass;` |
|        - |  6868 | `	SyString *pName;` |
|        9 |  6869 | `	if( nArg < 1 ){` |
|        - |  6870 | `		/* Check if we are inside a class [i.e: a method call]*/` |
|        3 |  6871 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|        3 |  6872 | `		if( pClass && pClass->pBase ){` |
|        - |  6873 | `			/* Point to the class name */` |
|        3 |  6874 | `			pName = &pClass->pBase->sName;` |
|        3 |  6875 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        2 |  6876 | `		}else{` |
|        - |  6877 | `			/* Not inside class,return FALSE */` |
|      ! 0 |  6878 | `			ph7_result_bool(pCtx,0);` |
|        - |  6879 | `		}` |
|        2 |  6880 | `	}else{` |
|        - |  6881 | `		/* Extract the target class */` |
|        7 |  6882 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        7 |  6883 | `		if( pClass ){` |
|        7 |  6884 | `			if( pClass->pBase ){` |
|        5 |  6885 | `				pName = &pClass->pBase->sName;` |
|        - |  6886 | `				/* Return the parent class name */` |
|        5 |  6887 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        3 |  6888 | `			}else{` |
|        - |  6889 | `				/* Object does not have a parent class */` |
|        3 |  6890 | `				ph7_result_bool(pCtx,0);` |
|        - |  6891 | `			}` |
|        4 |  6892 | `		}else{` |
|        - |  6893 | `			/* Not a class instance,return FALSE */` |
|      ! 0 |  6894 | `			ph7_result_bool(pCtx,0);` |
|        - |  6895 | `		}` |
|        - |  6896 | `	}` |
|        9 |  6897 | `	return PH7_OK;` |
|        1 |  6898 |  |
|        - |  6899 | `/*` |
|        - |  6900 | ` * string get_called_class(void)` |
|        - |  6901 | ` *   Gets the name of the class the static method is called in.` |
|        - |  6902 | ` * Parameters` |
|        - |  6903 | ` *  None.` |
|        - |  6904 | ` * Return` |
|        - |  6905 | ` *  Returns the class name. Returns FALSE if called from outside a class.` |
|        - |  6906 | ` */` |
|        4 |  6907 | `static int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6908 |  |
|        - |  6909 | `	ph7_class *pClass;` |
|        - |  6910 | `	/* Check if we are inside a class [i.e: a method call] */` |
|        5 |  6911 | `	pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|        5 |  6912 | `	if( pClass ){` |
|        - |  6913 | `		SyString *pName;` |
|        - |  6914 | `		/* Point to the class name */` |
|        5 |  6915 | `		pName = &pClass->sName;` |
|        5 |  6916 | `		ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        3 |  6917 | `	}else{` |
|      ! 0 |  6918 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6919 | `		SXUNUSED(apArg);` |
|        - |  6920 | `		/* Not inside class,return FALSE */` |
|      ! 0 |  6921 | `		ph7_result_bool(pCtx,0);` |
|        - |  6922 | `	}` |
|        5 |  6923 | `	return PH7_OK;` |
|        1 |  6924 |  |
|        - |  6925 | `/*` |
|        - |  6926 | ` * Extract a ph7_class from the given ph7_value.` |
|        - |  6927 | ` * The given value must be of type object [i.e: class instance] or` |
|        - |  6928 | ` * string which hold the class name.` |
|        - |  6929 | ` */` |
|       80 |  6930 | `static ph7_class * VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|        2 |  6931 |  |
|       82 |  6932 | `	ph7_class *pClass = 0;` |
|       82 |  6933 | `	if( ph7_value_is_object(pArg) ){` |
|        - |  6934 | `		/* Class instance already loaded,no need to perform a lookup */` |
|       44 |  6935 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|       61 |  6936 | `	}else if( ph7_value_is_string(pArg) ){` |
|        - |  6937 | `		const char *zClass;` |
|        - |  6938 | `		int nLen;` |
|        - |  6939 | `		/* Extract class name */` |
|       38 |  6940 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|       38 |  6941 | `		if( nLen > 0 ){` |
|        - |  6942 | `			SyHashEntry *pEntry;` |
|        - |  6943 | `			/* Perform a lookup */` |
|       38 |  6944 | `			pEntry = SyHashGet(&pVm->hClass,(const void *)zClass,(sxu32)nLen);` |
|       38 |  6945 | `			if( pEntry ){` |
|        - |  6946 | `				/* Point to the desired class */` |
|       31 |  6947 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|       15 |  6948 | `			}` |
|       18 |  6949 | `		}` |
|       18 |  6950 | `	}` |
|       82 |  6951 | `	return pClass;` |
|        2 |  6952 |  |
|        - |  6953 | `/*` |
|        - |  6954 | ` * bool property_exists(mixed $class,string $property)` |
|        - |  6955 | ` *   Checks if the object or class has a property.` |
|        - |  6956 | ` * Parameters` |
|        - |  6957 | ` *  class` |
|        - |  6958 | ` *   The class name or an object of the class to test for` |
|        - |  6959 | ` * property` |
|        - |  6960 | ` *  The name of the property` |
|        - |  6961 | ` * Return` |
|        - |  6962 | ` *   Returns TRUE if the property exists,FALSE otherwise.` |
|        - |  6963 | ` */` |
|       12 |  6964 | `static int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6965 |  |
|       13 |  6966 | `	int res = 0; /* Assume attribute does not exists */` |
|       13 |  6967 | `	if( nArg > 1 ){` |
|        - |  6968 | `		ph7_class *pClass;` |
|       13 |  6969 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       13 |  6970 | `		if( pClass ){` |
|        - |  6971 | `			const char *zName;` |
|        - |  6972 | `			int nLen;` |
|        - |  6973 | `			/* Extract attribute name */` |
|       13 |  6974 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|       13 |  6975 | `			if( nLen > 0 ){` |
|        - |  6976 | `				/* Perform the lookup in the attribute and method table */` |
|       12 |  6977 | `				if( SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen) != 0` |
|        8 |  6978 | `					\|\| SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6979 | `						/* property exists,flag that */` |
|       11 |  6980 | `						res = 1;` |
|        5 |  6981 | `				}` |
|        6 |  6982 | `			}` |
|        6 |  6983 | `		}` |
|        6 |  6984 | `	}` |
|       13 |  6985 | `	ph7_result_bool(pCtx,res);` |
|       13 |  6986 | `	return PH7_OK;` |
|        1 |  6987 |  |
|        - |  6988 | `/*` |
|        - |  6989 | ` * bool method_exists(mixed $class,string $method)` |
|        - |  6990 | ` *   Checks if the given method is a class member.` |
|        - |  6991 | ` * Parameters` |
|        - |  6992 | ` *  class` |
|        - |  6993 | ` *   The class name or an object of the class to test for` |
|        - |  6994 | ` * property` |
|        - |  6995 | ` *  The name of the method` |
|        - |  6996 | ` * Return` |
|        - |  6997 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|        - |  6998 | ` */` |
|        4 |  6999 | `static int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7000 |  |
|        5 |  7001 | `	int res = 0; /* Assume method does not exists */` |
|        5 |  7002 | `	if( nArg > 1 ){` |
|        - |  7003 | `		ph7_class *pClass;` |
|        5 |  7004 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        5 |  7005 | `		if( pClass ){` |
|        - |  7006 | `			const char *zName;` |
|        - |  7007 | `			int nLen;` |
|        - |  7008 | `			/* Extract method name */` |
|        5 |  7009 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|        5 |  7010 | `			if( nLen > 0 ){` |
|        - |  7011 | `				/* Perform the lookup in the method table */` |
|        5 |  7012 | `				if( SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7013 | `					/* method exists,flag that */` |
|        3 |  7014 | `					res = 1;` |
|        1 |  7015 | `				}` |
|        2 |  7016 | `			}` |
|        2 |  7017 | `		}` |
|        2 |  7018 | `	}` |
|        5 |  7019 | `	ph7_result_bool(pCtx,res);` |
|        5 |  7020 | `	return PH7_OK;` |
|        1 |  7021 |  |
|        - |  7022 | `/*` |
|        - |  7023 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|        - |  7024 | ` *   Checks if the class has been defined.` |
|        - |  7025 | ` * Parameters` |
|        - |  7026 | ` *  class_name` |
|        - |  7027 | ` *   The class name. The name is matched in a case-sensitive manner` |
|        - |  7028 | ` *   unlinke the standard PHP engine.` |
|        - |  7029 | ` *  autoload` |
|        - |  7030 | ` *   Whether or not to call __autoload by default.` |
|        - |  7031 | ` * Return` |
|        - |  7032 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|        - |  7033 | ` */` |
|       12 |  7034 | `static int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7035 |  |
|       14 |  7036 | `	int res = 0; /* Assume class does not exists */` |
|       14 |  7037 | `	if( nArg > 0 ){` |
|        - |  7038 | `		const char *zName;` |
|        - |  7039 | `		int nLen;` |
|        - |  7040 | `		/* Extract given name */` |
|       14 |  7041 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7042 | `		/* Perform a hashlookup */` |
|       14 |  7043 | `		if( nLen > 0 && SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7044 | `			/* class is available */` |
|       10 |  7045 | `			res = 1;` |
|        4 |  7046 | `		}` |
|        6 |  7047 | `	}` |
|       14 |  7048 | `	ph7_result_bool(pCtx,res);` |
|       14 |  7049 | `	return PH7_OK;` |
|        2 |  7050 |  |
|        - |  7051 | `/*` |
|        - |  7052 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|        - |  7053 | ` *   Checks if the interface has been defined.` |
|        - |  7054 | ` * Parameters` |
|        - |  7055 | ` *  class_name` |
|        - |  7056 | ` *   The class name. The name is matched in a case-sensitive manner` |
|        - |  7057 | ` *   unlinke the standard PHP engine.` |
|        - |  7058 | ` *  autoload` |
|        - |  7059 | ` *   Whether or not to call __autoload by default.` |
|        - |  7060 | ` * Return` |
|        - |  7061 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|        - |  7062 | ` */` |
|        6 |  7063 | `static int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7064 |  |
|        7 |  7065 | `	int res = 0; /* Assume class does not exists */` |
|        7 |  7066 | `	if( nArg > 0 ){` |
|        7 |  7067 | `		SyHashEntry *pEntry = 0;` |
|        - |  7068 | `		const char *zName;` |
|        - |  7069 | `		int nLen;` |
|        - |  7070 | `		/* Extract given name */` |
|        7 |  7071 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7072 | `		/* Perform a hashlookup */` |
|        7 |  7073 | `		if( nLen > 0 ){` |
|        7 |  7074 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|        3 |  7075 | `		}` |
|        7 |  7076 | `		if( pEntry ){` |
|        5 |  7077 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        5 |  7078 | `			while( pClass ){` |
|        5 |  7079 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |  7080 | `					/* interface is available */` |
|        5 |  7081 | `					res = 1;` |
|        5 |  7082 | `					break;` |
|        - |  7083 | `				}` |
|        - |  7084 | `				/* Next with the same name */` |
|      ! 0 |  7085 | `				pClass = pClass->pNextName;` |
|      ! 0 |  7086 | `			}` |
|        2 |  7087 | `		}` |
|        3 |  7088 | `	}` |
|        7 |  7089 | `	ph7_result_bool(pCtx,res);` |
|        7 |  7090 | `	return PH7_OK;` |
|        1 |  7091 |  |
|        - |  7092 | `/*` |
|        - |  7093 | ` * bool class_alias([string $original[,string $alias ]])` |
|        - |  7094 | ` *   Creates an alias for a class.` |
|        - |  7095 | ` * Parameters` |
|        - |  7096 | ` *  original` |
|        - |  7097 | ` *    The original class.` |
|        - |  7098 | ` *  alias` |
|        - |  7099 | ` *   The alias name for the class.` |
|        - |  7100 | ` * Return` |
|        - |  7101 | ` *   Returns TRUE on success or FALSE on failure.` |
|        - |  7102 | ` */` |
|        2 |  7103 | `static int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7104 |  |
|        - |  7105 | `	const char *zOld,*zNew;` |
|        - |  7106 | `	int nOldLen,nNewLen;` |
|        - |  7107 | `	SyHashEntry *pEntry;` |
|        - |  7108 | `	ph7_class *pClass;` |
|        - |  7109 | `	char *zDup;` |
|        - |  7110 | `	sxi32 rc;` |
|        3 |  7111 | `	if( nArg < 2 ){` |
|        - |  7112 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  7113 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7114 | `		return PH7_OK;` |
|        - |  7115 | `	}` |
|        - |  7116 | `	/* Extract old class name */` |
|        3 |  7117 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|        - |  7118 | `	/* Extract alias name */` |
|        3 |  7119 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|        3 |  7120 | `	if( nNewLen < 1 ){` |
|        - |  7121 | `		/* Invalid alias name,return FALSE */` |
|      ! 0 |  7122 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7123 | `		return PH7_OK;` |
|        - |  7124 | `	}` |
|        - |  7125 | `	/* Perform a hash lookup */` |
|        3 |  7126 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,(sxu32)nOldLen);` |
|        3 |  7127 | `	if( pEntry ==  0 ){` |
|        - |  7128 | `		/* No such class,return FALSE */` |
|      ! 0 |  7129 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7130 | `		return PH7_OK;` |
|        - |  7131 | `	}` |
|        - |  7132 | `	/* Point to the class */` |
|        3 |  7133 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7134 | `	/* Duplicate alias name */` |
|        3 |  7135 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,(sxu32)nNewLen);` |
|        3 |  7136 | `	if( zDup == 0 ){` |
|        - |  7137 | `		/* Out of memory,return FALSE */` |
|      ! 0 |  7138 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7139 | `		return PH7_OK;` |
|        - |  7140 | `	}` |
|        - |  7141 | `	/* Create the alias */` |
|        3 |  7142 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,(sxu32)nNewLen,pClass);` |
|        3 |  7143 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7144 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|      ! 0 |  7145 | `	}` |
|        3 |  7146 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|        3 |  7147 | `	return PH7_OK;` |
|        2 |  7148 |  |
|        - |  7149 | `/*` |
|        - |  7150 | ` * array get_declared_classes(void)` |
|        - |  7151 | ` *   Returns an array with the name of the defined classes` |
|        - |  7152 | ` * Parameters` |
|        - |  7153 | ` *  None` |
|        - |  7154 | ` * Return` |
|        - |  7155 | ` *   Returns an array of the names of the declared classes` |
|        - |  7156 | ` *   in the current script.` |
|        - |  7157 | ` * Note:` |
|        - |  7158 | ` *   NULL is returned on failure.` |
|        - |  7159 | ` */` |
|        2 |  7160 | `static int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7161 |  |
|        - |  7162 | `	ph7_value *pName,*pArray;` |
|        - |  7163 | `	SyHashEntry *pEntry;` |
|        - |  7164 | `	/* Create a new array first */` |
|        3 |  7165 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7166 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7167 | `	if( pArray == 0 \|\| pName == 0){` |
|      ! 0 |  7168 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7169 | `		SXUNUSED(apArg);` |
|        - |  7170 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7171 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7172 | `		return PH7_OK;` |
|        - |  7173 | `	}` |
|        - |  7174 | `	/* Fill the array with the defined classes */` |
|        3 |  7175 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|       52 |  7176 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|       49 |  7177 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7178 | `		/* Do not register classes defined as interfaces */` |
|       49 |  7179 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       43 |  7180 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|        - |  7181 | `			/* insert class name */` |
|       43 |  7182 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7183 | `			/* Reset the cursor */` |
|       43 |  7184 | `			ph7_value_reset_string_cursor(pName);` |
|       21 |  7185 | `		}` |
|        1 |  7186 | `	}` |
|        - |  7187 | `	/* Return the created array */` |
|        3 |  7188 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7189 | `	return PH7_OK;` |
|        2 |  7190 |  |
|        - |  7191 | `/*` |
|        - |  7192 | ` * array get_declared_interfaces(void)` |
|        - |  7193 | ` *   Returns an array with the name of the defined interfaces` |
|        - |  7194 | ` * Parameters` |
|        - |  7195 | ` *  None` |
|        - |  7196 | ` * Return` |
|        - |  7197 | ` *   Returns an array of the names of the declared interfaces` |
|        - |  7198 | ` *   in the current script.` |
|        - |  7199 | ` * Note:` |
|        - |  7200 | ` *   NULL is returned on failure.` |
|        - |  7201 | ` */` |
|        2 |  7202 | `static int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7203 |  |
|        - |  7204 | `	ph7_value *pName,*pArray;` |
|        - |  7205 | `	SyHashEntry *pEntry;` |
|        - |  7206 | `	/* Create a new array first */` |
|        3 |  7207 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7208 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7209 | `	if( pArray == 0 \|\| pName == 0 ){` |
|      ! 0 |  7210 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7211 | `		SXUNUSED(apArg);` |
|        - |  7212 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7213 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7214 | `		return PH7_OK;` |
|        - |  7215 | `	}` |
|        - |  7216 | `	/* Fill the array with the defined classes */` |
|        3 |  7217 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|       54 |  7218 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|       51 |  7219 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7220 | `		/* Register classes defined as interfaces only */` |
|       51 |  7221 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        9 |  7222 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|        - |  7223 | `			/* insert interface name */` |
|        9 |  7224 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7225 | `			/* Reset the cursor */` |
|        9 |  7226 | `			ph7_value_reset_string_cursor(pName);` |
|        4 |  7227 | `		}` |
|        1 |  7228 | `	}` |
|        - |  7229 | `	/* Return the created array */` |
|        3 |  7230 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7231 | `	return PH7_OK;` |
|        2 |  7232 |  |
|        - |  7233 | `/*` |
|        - |  7234 | ` * array get_class_methods(string/object $class_name)` |
|        - |  7235 | ` *   Returns an array with the name of the class methods` |
|        - |  7236 | ` * Parameters` |
|        - |  7237 | ` *  class_name` |
|        - |  7238 | ` *  The class name or class instance` |
|        - |  7239 | ` * Return` |
|        - |  7240 | ` *  Returns an array of method names defined for the class specified by class_name.` |
|        - |  7241 | ` *  In case of an error, it returns NULL.` |
|        - |  7242 | ` * Note:` |
|        - |  7243 | ` *   NULL is returned on failure.` |
|        - |  7244 | ` */` |
|        6 |  7245 | `static int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7246 |  |
|        - |  7247 | `	ph7_value *pName,*pArray;` |
|        - |  7248 | `	SyHashEntry *pEntry;` |
|        - |  7249 | `	ph7_class *pClass;` |
|        - |  7250 | `	/* Extract the target class first */` |
|        7 |  7251 | `	pClass = 0;` |
|        7 |  7252 | `	if( nArg > 0 ){` |
|        7 |  7253 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        3 |  7254 | `	}` |
|        7 |  7255 | `	if( pClass == 0 ){` |
|        - |  7256 | `		/* No such class,return NULL */` |
|        3 |  7257 | `		ph7_result_null(pCtx);` |
|        3 |  7258 | `		return PH7_OK;` |
|        - |  7259 | `	}` |
|        - |  7260 | `	/* Create a new array  */` |
|        5 |  7261 | `	pArray = ph7_context_new_array(pCtx);` |
|        5 |  7262 | `	pName = ph7_context_new_scalar(pCtx);` |
|        5 |  7263 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7264 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7265 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7266 | `		return PH7_OK;` |
|        - |  7267 | `	}` |
|        - |  7268 | `	/* Fill the array with the defined methods */` |
|        5 |  7269 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|       17 |  7270 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       13 |  7271 | `		ph7_class_method *pMethod = (ph7_class_method *)pEntry->pUserData;` |
|        - |  7272 | `		/* Insert method name */` |
|       13 |  7273 | `		ph7_value_string(pName,SyStringData(&pMethod->sFunc.sName),(int)SyStringLength(&pMethod->sFunc.sName));` |
|       13 |  7274 | `		ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7275 | `		/* Reset the cursor */` |
|       13 |  7276 | `		ph7_value_reset_string_cursor(pName);` |
|        1 |  7277 | `	}` |
|        - |  7278 | `	/* Return the created array */` |
|        5 |  7279 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7280 | `	/*` |
|        - |  7281 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7282 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7283 | `	 */` |
|        5 |  7284 | `	return PH7_OK;` |
|        4 |  7285 |  |
|        - |  7286 | `/*` |
|        - |  7287 | ` * This function return TRUE(1) if the given class attribute stored` |
|        - |  7288 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|        - |  7289 | ` * from the current scope.Otherwise FALSE is returned.` |
|        - |  7290 | ` */` |
|     2240 |  7291 | `static int VmClassMemberAccess(` |
|        - |  7292 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7293 | `	ph7_class *pClass,         /* Target Class */` |
|        - |  7294 | `	const SyString *pAttrName, /* Attribute name */` |
|        - |  7295 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|        - |  7296 | `	int bLog                   /* TRUE to log forbidden access. */` |
|        - |  7297 | `	)` |
|        2 |  7298 |  |
|     2242 |  7299 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     1678 |  7300 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  7301 | `		ph7_vm_func *pVmFunc;` |
|     1682 |  7302 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|        - |  7303 | `			/* Safely ignore the exception frame */` |
|        5 |  7304 | `			pFrame = pFrame->pParent;` |
|        1 |  7305 | `		}` |
|     1678 |  7306 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     1678 |  7307 | `		if( pVmFunc == 0 \|\| (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|        9 |  7308 | `			goto dis; /* Access is forbidden */` |
|        - |  7309 | `		}` |
|     1670 |  7310 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - |  7311 | `			/* Must be the same instance */` |
|        7 |  7312 | `			if( (ph7_class *)pVmFunc->pUserData != pClass ){` |
|      ! 0 |  7313 | `				goto dis; /* Access is forbidden */` |
|        - |  7314 | `			}` |
|        4 |  7315 | `		}else{` |
|        - |  7316 | `			/* Protected */` |
|     1664 |  7317 | `			ph7_class *pBase = (ph7_class *)pVmFunc->pUserData;` |
|        - |  7318 | `			/* Must be a derived class */` |
|     1664 |  7319 | `			if( !VmInstanceOf(pClass,pBase) ){` |
|      ! 0 |  7320 | `				goto dis; /* Access is forbidden */` |
|        - |  7321 | `			}` |
|        - |  7322 | `		}` |
|      834 |  7323 | `	}` |
|     2234 |  7324 | `	return 1; /* Access is granted */` |
|        4 |  7325 | `dis:` |
|        9 |  7326 | `	if( bLog ){` |
|      ! 0 |  7327 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7328 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|      ! 0 |  7329 | `			&pClass->sName,pAttrName);` |
|      ! 0 |  7330 | `	}` |
|        9 |  7331 | `	return 0; /* Access is forbidden */` |
|     1122 |  7332 |  |
|        - |  7333 | `/*` |
|        - |  7334 | ` * array get_class_vars(string/object $class_name)` |
|        - |  7335 | ` *   Get the default properties of the class` |
|        - |  7336 | ` * Parameters` |
|        - |  7337 | ` *  class_name` |
|        - |  7338 | ` *   The class name or class instance` |
|        - |  7339 | ` * Return` |
|        - |  7340 | ` *  Returns an associative array of declared properties visible from the current scope` |
|        - |  7341 | ` *  with their default value. The resulting array elements are in the form` |
|        - |  7342 | ` *  of varname => value.` |
|        - |  7343 | ` * Note:` |
|        - |  7344 | ` *   NULL is returned on failure.` |
|        - |  7345 | ` */` |
|        2 |  7346 | `static int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7347 |  |
|        - |  7348 | `	ph7_value *pName,*pArray,sValue;` |
|        - |  7349 | `	SyHashEntry *pEntry;` |
|        - |  7350 | `	ph7_class *pClass;` |
|        - |  7351 | `	/* Extract the target class first */` |
|        3 |  7352 | `	pClass = 0;` |
|        3 |  7353 | `	if( nArg > 0 ){` |
|        3 |  7354 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        1 |  7355 | `	}` |
|        3 |  7356 | `	if( pClass == 0 ){` |
|        - |  7357 | `		/* No such class,return NULL */` |
|      ! 0 |  7358 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7359 | `		return PH7_OK;` |
|        - |  7360 | `	}` |
|        - |  7361 | `	/* Create a new array  */` |
|        3 |  7362 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7363 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7364 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|        3 |  7365 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7366 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7367 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7368 | `		return PH7_OK;` |
|        - |  7369 | `	}` |
|        - |  7370 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|        3 |  7371 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        8 |  7372 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        5 |  7373 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|        - |  7374 | `		/* Check if the access is allowed */` |
|        5 |  7375 | `		if( VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        5 |  7376 | `			SyString *pAttrName = &pAttr->sName;` |
|        5 |  7377 | `			ph7_value *pValue = 0;` |
|        5 |  7378 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |  7379 | `				/* Extract static attribute value which is always computed */` |
|        5 |  7380 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|        3 |  7381 | `			}else{` |
|      ! 0 |  7382 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      ! 0 |  7383 | `					PH7_MemObjRelease(&sValue);` |
|        - |  7384 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|      ! 0 |  7385 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue);` |
|      ! 0 |  7386 | `					pValue = &sValue;` |
|      ! 0 |  7387 | `				}` |
|        - |  7388 | `			}` |
|        - |  7389 | `			/* Fill in the array */` |
|        5 |  7390 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|        5 |  7391 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|        - |  7392 | `			/* Reset the cursor */` |
|        5 |  7393 | `			ph7_value_reset_string_cursor(pName);` |
|        2 |  7394 | `		}` |
|        1 |  7395 | `	}` |
|        3 |  7396 | `	PH7_MemObjRelease(&sValue);` |
|        - |  7397 | `	/* Return the created array */` |
|        3 |  7398 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7399 | `	/*` |
|        - |  7400 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7401 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7402 | `	 */` |
|        3 |  7403 | `	return PH7_OK;` |
|        2 |  7404 |  |
|        - |  7405 | `/*` |
|        - |  7406 | ` * array get_object_vars(object $this)` |
|        - |  7407 | ` *   Gets the properties of the given object` |
|        - |  7408 | ` * Parameters` |
|        - |  7409 | ` *  this` |
|        - |  7410 | ` *   A class instance` |
|        - |  7411 | ` * Return` |
|        - |  7412 | ` *  Returns an associative array of defined object accessible non-static properties` |
|        - |  7413 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|        - |  7414 | ` *  it will be returned with a NULL value.` |
|        - |  7415 | ` * Note:` |
|        - |  7416 | ` *   NULL is returned on failure.` |
|        - |  7417 | ` */` |
|        2 |  7418 | `static int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7419 |  |
|        3 |  7420 | `	ph7_class_instance *pThis = 0;` |
|        - |  7421 | `	ph7_value *pName,*pArray;` |
|        - |  7422 | `	SyHashEntry *pEntry;` |
|        3 |  7423 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|        - |  7424 | `		/* Extract the target instance */` |
|        3 |  7425 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        1 |  7426 | `	}` |
|        3 |  7427 | `	if( pThis == 0 ){` |
|        - |  7428 | `		/* No such instance,return NULL */` |
|      ! 0 |  7429 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7430 | `		return PH7_OK;` |
|        - |  7431 | `	}` |
|        - |  7432 | `	/* Create a new array  */` |
|        3 |  7433 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7434 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7435 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7436 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7437 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7438 | `		return PH7_OK;` |
|        - |  7439 | `	}` |
|        - |  7440 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|        3 |  7441 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  7442 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        7 |  7443 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  7444 | `		SyString *pAttrName;` |
|        7 |  7445 | `		if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        - |  7446 | `			/* Only non-static/constant attributes are extracted */` |
|      ! 0 |  7447 | `			continue;` |
|        - |  7448 | `		}` |
|        7 |  7449 | `		pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  7450 | `		/* Check if the access is allowed */` |
|        7 |  7451 | `		if( VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|        3 |  7452 | `			ph7_value *pValue = 0;` |
|        - |  7453 | `			/* Extract attribute */` |
|        3 |  7454 | `			pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|        3 |  7455 | `			if( pValue ){` |
|        - |  7456 | `				/* Insert attribute name in the array */` |
|        3 |  7457 | `				ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|        3 |  7458 | `				ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|        1 |  7459 | `			}` |
|        - |  7460 | `			/* Reset the cursor */` |
|        3 |  7461 | `			ph7_value_reset_string_cursor(pName);` |
|        1 |  7462 | `		}` |
|        1 |  7463 | `	}` |
|        - |  7464 | `	/* Return the created array */` |
|        3 |  7465 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7466 | `	/*` |
|        - |  7467 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7468 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7469 | `	 */` |
|        3 |  7470 | `	return PH7_OK;` |
|        2 |  7471 |  |
|        - |  7472 | `/*` |
|        - |  7473 | ` * This function returns TRUE if the given class is an implemented` |
|        - |  7474 | ` * interface.Otherwise FALSE is returned.` |
|        - |  7475 | ` */` |
|     3708 |  7476 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|        2 |  7477 |  |
|        - |  7478 | `	ph7_class **apInterface;` |
|        - |  7479 | `	sxu32 n;` |
|     3710 |  7480 | `	if( SySetUsed(pSet) < 1 ){` |
|        - |  7481 | `		/* Empty interface container */` |
|     3708 |  7482 | `		return FALSE;` |
|        - |  7483 | `	}` |
|        - |  7484 | `	/* Point to the set of implemented interfaces */` |
|        3 |  7485 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|        - |  7486 | `	/* Perform the lookup */` |
|        3 |  7487 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
|        3 |  7488 | `		if( apInterface[n] == pClass ){` |
|        3 |  7489 | `			return TRUE;` |
|        - |  7490 | `		}` |
|      ! 0 |  7491 | `	}` |
|      ! 0 |  7492 | `	return FALSE;` |
|     1856 |  7493 |  |
|        - |  7494 | `/*` |
|        - |  7495 | ` * This function returns TRUE if the given class (first argument)` |
|        - |  7496 | ` * is an instance of the main class (second argument).` |
|        - |  7497 | ` * Otherwise FALSE is returned.` |
|        - |  7498 | ` */` |
|     1718 |  7499 | `static int VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|        2 |  7500 |  |
|        - |  7501 | `	ph7_class *pParent;` |
|        - |  7502 | `	sxi32 rc;` |
|     1720 |  7503 | `	if( pThis == pClass ){` |
|        - |  7504 | `		/* Instance of the same class */` |
|      140 |  7505 | `		return TRUE;` |
|        - |  7506 | `	}` |
|        - |  7507 | `	/* Check implemented interfaces */` |
|     1582 |  7508 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
|     1582 |  7509 | `	if( rc ){` |
|        3 |  7510 | `		return TRUE;` |
|        - |  7511 | `	}` |
|        - |  7512 | `	/* Check parent classes */` |
|     1580 |  7513 | `	pParent = pThis->pBase;` |
|     3708 |  7514 | `	while( pParent ){` |
|     3706 |  7515 | `		if( pParent == pClass ){` |
|        - |  7516 | `			/* Same instance */` |
|     1578 |  7517 | `			return TRUE;` |
|        - |  7518 | `		}` |
|        - |  7519 | `		/* Check the implemented interfaces */` |
|     2130 |  7520 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|     2130 |  7521 | `		if( rc ){` |
|      ! 0 |  7522 | `			return TRUE;` |
|        - |  7523 | `		}` |
|        - |  7524 | `		/* Point to the parent class */` |
|     2130 |  7525 | `		pParent = pParent->pBase;` |
|        2 |  7526 | `	}` |
|        - |  7527 | `	/* Not an instance of the the given class */` |
|        3 |  7528 | `	return FALSE;` |
|      861 |  7529 |  |
|        - |  7530 | `/*` |
|        - |  7531 | ` * This function returns TRUE if the given class (first argument)` |
|        - |  7532 | ` * is a subclass of the main class (second argument).` |
|        - |  7533 | ` * Otherwise FALSE is returned.` |
|        - |  7534 | ` */` |
|        4 |  7535 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|        1 |  7536 |  |
|        5 |  7537 | `	SySet *pInterface = &pClass->aInterface;` |
|        - |  7538 | `	SyHashEntry *pEntry;` |
|        - |  7539 | `	SyString *pName;` |
|        - |  7540 | `	sxi32 rc;` |
|        5 |  7541 | `	while( pClass ){` |
|        5 |  7542 | `		pName = &pClass->sName;` |
|        - |  7543 | `		/* Query the derived hashtable */` |
|        5 |  7544 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|        5 |  7545 | `		if( pEntry ){` |
|        5 |  7546 | `			return TRUE;` |
|        - |  7547 | `		}` |
|      ! 0 |  7548 | `		pClass = pClass->pBase;` |
|      ! 0 |  7549 | `	}` |
|      ! 0 |  7550 | `	rc = VmQueryInterfaceSet(pBase,pInterface);` |
|      ! 0 |  7551 | `	if( rc ){` |
|      ! 0 |  7552 | `		return TRUE;` |
|        - |  7553 | `	}` |
|        - |  7554 | `	/* Not a subclass */` |
|      ! 0 |  7555 | `	return FALSE;` |
|        3 |  7556 |  |
|        - |  7557 | `/*` |
|        - |  7558 | ` * bool is_a(object $object,string $class_name)` |
|        - |  7559 | ` *   Checks if the object is of this class or has this class as one of its parents.` |
|        - |  7560 | ` * Parameters` |
|        - |  7561 | ` *  object` |
|        - |  7562 | ` *   The tested object` |
|        - |  7563 | ` * class_name` |
|        - |  7564 | ` *  The class name` |
|        - |  7565 | ` * Return` |
|        - |  7566 | ` *   Returns TRUE if the object is of this class or has this class as one of its` |
|        - |  7567 | ` *   parents, FALSE otherwise.` |
|        - |  7568 | ` */` |
|        2 |  7569 | `static int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7570 |  |
|        3 |  7571 | `	int res = 0; /* Assume FALSE by default */` |
|        3 |  7572 | `	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){` |
|        3 |  7573 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7574 | `		ph7_class *pClass;` |
|        - |  7575 | `		/* Extract the given class */` |
|        3 |  7576 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|        3 |  7577 | `		if( pClass ){` |
|        - |  7578 | `			/* Perform the query */` |
|        3 |  7579 | `			res = VmInstanceOf(pThis->pClass,pClass);` |
|        1 |  7580 | `		}` |
|        1 |  7581 | `	}` |
|        - |  7582 | `	/* Query result */` |
|        3 |  7583 | `	ph7_result_bool(pCtx,res);` |
|        3 |  7584 | `	return PH7_OK;` |
|        1 |  7585 |  |
|        - |  7586 | `/*` |
|        - |  7587 | ` * bool is_subclass_of(object/string $object,object/string $class_name)` |
|        - |  7588 | ` *   Checks if the object has this class as one of its parents.` |
|        - |  7589 | ` * Parameters` |
|        - |  7590 | ` *  object` |
|        - |  7591 | ` *   The tested object` |
|        - |  7592 | ` * class_name` |
|        - |  7593 | ` *  The class name` |
|        - |  7594 | ` * Return` |
|        - |  7595 | ` *  This function returns TRUE if the object , belongs to a class` |
|        - |  7596 | ` *  which is a subclass of class_name, FALSE otherwise.` |
|        - |  7597 | ` */` |
|        6 |  7598 | `static int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7599 |  |
|        7 |  7600 | `	int res = 0; /* Assume FALSE by default */` |
|        7 |  7601 | `	if( nArg > 1 ){` |
|        - |  7602 | `		ph7_class *pClass,*pMain;` |
|        - |  7603 | `		/* Extract the given classes */` |
|        7 |  7604 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        7 |  7605 | `		pMain = VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|        7 |  7606 | `		if( pClass && pMain ){` |
|        - |  7607 | `			/* Perform the query */` |
|        5 |  7608 | `			res = VmSubclassOf(pClass,pMain);` |
|        2 |  7609 | `		}` |
|        3 |  7610 | `	}` |
|        - |  7611 | `	/* Query result */` |
|        7 |  7612 | `	ph7_result_bool(pCtx,res);` |
|        7 |  7613 | `	return PH7_OK;` |
|        1 |  7614 |  |
|        - |  7615 | `/*` |
|        - |  7616 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  7617 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  7618 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  7619 | ` * return value indicates failure.` |
|        - |  7620 | ` */` |
|      718 |  7621 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  7622 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7623 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  7624 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  7625 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  7626 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  7627 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  7628 | `	)` |
|        2 |  7629 |  |
|        - |  7630 | `	ph7_value *aStack;` |
|        - |  7631 | `	VmInstr aInstr[2];` |
|        - |  7632 | `	int iCursor;` |
|        - |  7633 | `	int i;` |
|        - |  7634 | `	/* Create a new operand stack */` |
|      720 |  7635 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|      720 |  7636 | `	if( aStack == 0 ){` |
|      ! 0 |  7637 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7638 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  7639 | `		return SXERR_MEM;` |
|        - |  7640 | `	}` |
|        - |  7641 | `	/* Fill the operand stack with the given arguments */` |
|     1050 |  7642 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      332 |  7643 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7644 | `		/*` |
|        - |  7645 | `		 * Symisc eXtension:` |
|        - |  7646 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7647 | `		 */` |
|      332 |  7648 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      167 |  7649 | `	}` |
|      720 |  7650 | `	iCursor = nArg + 1;` |
|      720 |  7651 | `	if( pThis ){` |
|        - |  7652 | `		/*` |
|        - |  7653 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  7654 | `		 */` |
|      714 |  7655 | `		pThis->iRef++; /* Increment reference count */` |
|      714 |  7656 | `		aStack[i].x.pOther = pThis;` |
|      714 |  7657 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      356 |  7658 | `	}` |
|      720 |  7659 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|      720 |  7660 | `	i++;` |
|        - |  7661 | `	/* Push method name */` |
|      720 |  7662 | `	SyBlobReset(&aStack[i].sBlob);` |
|      720 |  7663 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|      720 |  7664 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|      720 |  7665 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  7666 | `	/* Emit the CALL istruction */` |
|      720 |  7667 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      720 |  7668 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      720 |  7669 | `	aInstr[0].iP2 = 0;` |
|      720 |  7670 | `	aInstr[0].p3  = 0;` |
|        - |  7671 | `	/* Emit the DONE instruction */` |
|      720 |  7672 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      720 |  7673 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|      720 |  7674 | `	aInstr[1].iP2 = 0;` |
|      720 |  7675 | `	aInstr[1].p3  = 0;` |
|        - |  7676 | `	/* Execute the method body (if available) */` |
|      720 |  7677 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  7678 | `	/* Clean up the mess left behind */` |
|      720 |  7679 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      720 |  7680 | `	return PH7_OK;` |
|      361 |  7681 |  |
|        - |  7682 | `/*` |
|        - |  7683 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  7684 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  7685 | ` * in the apArg[] array.` |
|        - |  7686 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7687 | ` * return value indicates failure.` |
|        - |  7688 | ` */` |
|      624 |  7689 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  7690 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7691 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7692 | `	int nArg,          /* Total number of given arguments */` |
|        - |  7693 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  7694 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  7695 | `	)` |
|        2 |  7696 |  |
|        - |  7697 | `	ph7_value *aStack;` |
|        - |  7698 | `	VmInstr aInstr[2];` |
|        - |  7699 | `	int i;` |
|      626 |  7700 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  7701 | `		/* Don't bother processing,it's invalid anyway */` |
|      259 |  7702 | `		if( pResult ){` |
|        - |  7703 | `			/* Assume a null return value */` |
|      ! 0 |  7704 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7705 | `		}` |
|      259 |  7706 | `		return SXERR_INVALID;` |
|        - |  7707 | `	}` |
|      368 |  7708 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7709 | `		/* Class method */` |
|       11 |  7710 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  7711 | `		ph7_class_method *pMethod = 0;` |
|       11 |  7712 | `		ph7_class_instance *pThis = 0;` |
|       11 |  7713 | `		ph7_class *pClass = 0;` |
|        - |  7714 | `		ph7_value *pValue;` |
|        - |  7715 | `		sxi32 rc;` |
|       11 |  7716 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  7717 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  7718 | `			if( pResult ){` |
|        - |  7719 | `				/* Assume a null return value */` |
|      ! 0 |  7720 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7721 | `			}` |
|      ! 0 |  7722 | `			return SXRET_OK;` |
|        - |  7723 | `		}` |
|        - |  7724 | `		/* Extract the class name or an instance of it */` |
|       11 |  7725 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  7726 | `		if( pValue ){` |
|       11 |  7727 | `			pClass = VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  7728 | `		}` |
|       11 |  7729 | `		if( pClass == 0 ){` |
|        - |  7730 | `			/* No such class,return NULL */` |
|      ! 0 |  7731 | `			if( pResult ){` |
|      ! 0 |  7732 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7733 | `			}` |
|      ! 0 |  7734 | `			return SXRET_OK;` |
|        - |  7735 | `		}` |
|       11 |  7736 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  7737 | `			/* Point to the class instance */` |
|        5 |  7738 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  7739 | `		}` |
|        - |  7740 | `		/* Try to extract the method */` |
|       11 |  7741 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  7742 | `		if( pValue ){` |
|       11 |  7743 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  7744 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  7745 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  7746 | `			}` |
|        5 |  7747 | `		}` |
|       11 |  7748 | `		if( pMethod == 0 ){` |
|        - |  7749 | `			/* No such method,return NULL */` |
|      ! 0 |  7750 | `			if( pResult ){` |
|      ! 0 |  7751 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7752 | `			}` |
|      ! 0 |  7753 | `			return SXRET_OK;` |
|        - |  7754 | `		}` |
|        - |  7755 | `		/* Call the class method */` |
|       11 |  7756 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  7757 | `		return rc;` |
|        - |  7758 | `	}` |
|        - |  7759 | `	/* Create a new operand stack */` |
|      358 |  7760 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      358 |  7761 | `	if( aStack == 0 ){` |
|      ! 0 |  7762 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7763 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  7764 | `		if( pResult ){` |
|        - |  7765 | `			/* Assume a null return value */` |
|      ! 0 |  7766 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7767 | `		}` |
|      ! 0 |  7768 | `		return SXERR_MEM;` |
|        - |  7769 | `	}` |
|        - |  7770 | `	/* Fill the operand stack with the given arguments */` |
|     1160 |  7771 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      804 |  7772 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7773 | `		/*` |
|        - |  7774 | `		 * Symisc eXtension:` |
|        - |  7775 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7776 | `		 */` |
|      804 |  7777 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      403 |  7778 | `	}` |
|        - |  7779 | `	/* Push the function name */` |
|      358 |  7780 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      358 |  7781 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7782 | `	/* Emit the CALL istruction */` |
|      358 |  7783 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      358 |  7784 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      358 |  7785 | `	aInstr[0].iP2 = 0;` |
|      358 |  7786 | `	aInstr[0].p3  = 0;` |
|        - |  7787 | `	/* Emit the DONE instruction */` |
|      358 |  7788 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      358 |  7789 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      358 |  7790 | `	aInstr[1].iP2 = 0;` |
|      358 |  7791 | `	aInstr[1].p3  = 0;` |
|        - |  7792 | `	/* Execute the function body (if available) */` |
|      358 |  7793 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  7794 | `	/* Clean up the mess left behind */` |
|      358 |  7795 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      358 |  7796 | `	return PH7_OK;` |
|      314 |  7797 |  |
|        - |  7798 | `/*` |
|        - |  7799 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  7800 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  7801 | ` * parameter.` |
|        - |  7802 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7803 | ` * return value indicates failure.` |
|        - |  7804 | ` */` |
|      190 |  7805 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  7806 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7807 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7808 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  7809 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  7810 | `	)` |
|        1 |  7811 |  |
|        - |  7812 | `	ph7_value *pArg;` |
|        - |  7813 | `	SySet aArg;` |
|        - |  7814 | `	va_list ap;` |
|        - |  7815 | `	sxi32 rc;` |
|      191 |  7816 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7817 | `	/* Copy arguments one after one */` |
|      191 |  7818 | `	va_start(ap,pResult);` |
|      319 |  7819 | `	for(;;){` |
|      639 |  7820 | `		pArg = va_arg(ap,ph7_value *);` |
|      639 |  7821 | `		if( pArg == 0 ){` |
|      191 |  7822 | `			break;` |
|        - |  7823 | `		}` |
|      449 |  7824 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  7825 | `	}` |
|        - |  7826 | `	/* Call the core routine */` |
|      191 |  7827 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  7828 | `	/* Cleanup */` |
|      191 |  7829 | `	SySetRelease(&aArg);` |
|      191 |  7830 | `	return rc;` |
|        1 |  7831 |  |
|        - |  7832 | `/*` |
|        - |  7833 | ` * value call_user_func(callable $callback[,value $parameter[, value $... ]])` |
|        - |  7834 | ` *  Call the callback given by the first parameter.` |
|        - |  7835 | ` * Parameter` |
|        - |  7836 | ` *  $callback` |
|        - |  7837 | ` *   The callable to be called.` |
|        - |  7838 | ` *  ...` |
|        - |  7839 | ` *    Zero or more parameters to be passed to the callback.` |
|        - |  7840 | ` * Return` |
|        - |  7841 | ` *  Th return value of the callback, or FALSE on error.` |
|        - |  7842 | ` */` |
|       14 |  7843 | `static int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7844 |  |
|        - |  7845 | `	ph7_value sResult; /* Store callback return value here */` |
|        - |  7846 | `	sxi32 rc;` |
|       15 |  7847 | `	if( nArg < 1 ){` |
|        - |  7848 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  7849 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7850 | `		return PH7_OK;` |
|        - |  7851 | `	}` |
|       15 |  7852 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|       15 |  7853 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7854 | `	/* Try to invoke the callback */` |
|       15 |  7855 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|       15 |  7856 | `	if( rc != SXRET_OK ){` |
|        - |  7857 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|      ! 0 |  7858 | `		ph7_result_bool(pCtx,0); /* return false */` |
|      ! 0 |  7859 | `	}else{` |
|        - |  7860 | `		/* Callback result */` |
|       15 |  7861 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        - |  7862 | `	}` |
|       15 |  7863 | `	PH7_MemObjRelease(&sResult);` |
|       15 |  7864 | `	return PH7_OK;` |
|        8 |  7865 |  |
|        - |  7866 | `/*` |
|        - |  7867 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|        - |  7868 | ` *  Call a callback with an array of parameters.` |
|        - |  7869 | ` * Parameter` |
|        - |  7870 | ` *  $callback` |
|        - |  7871 | ` *   The callable to be called.` |
|        - |  7872 | ` * $param_arr` |
|        - |  7873 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|        - |  7874 | ` * Return` |
|        - |  7875 | ` *  Returns the return value of the callback, or FALSE on error.` |
|        - |  7876 | ` */` |
|       10 |  7877 | `static int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7878 |  |
|        - |  7879 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|        - |  7880 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|        - |  7881 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|        - |  7882 | `	SySet aArg;               /* Arguments containers */` |
|        - |  7883 | `	sxi32 rc;` |
|        - |  7884 | `	sxu32 n;` |
|       11 |  7885 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|        - |  7886 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  7887 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7888 | `		return PH7_OK;` |
|        - |  7889 | `	}` |
|       11 |  7890 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|       11 |  7891 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7892 | `	/* Initialize the arguments container */` |
|       11 |  7893 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7894 | `	/* Turn hashmap entries into callback arguments */` |
|       11 |  7895 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       11 |  7896 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|       23 |  7897 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        - |  7898 | `		/* Extract node value */` |
|       13 |  7899 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|       13 |  7900 | `			SySetPut(&aArg,(const void *)&pValue);` |
|        6 |  7901 | `		}` |
|        - |  7902 | `		/* Point to the next entry */` |
|       13 |  7903 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        7 |  7904 | `	}` |
|        - |  7905 | `	/* Try to invoke the callback */` |
|       11 |  7906 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|       11 |  7907 | `	if( rc != SXRET_OK ){` |
|        - |  7908 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|      ! 0 |  7909 | `		ph7_result_bool(pCtx,0); /* return false */` |
|      ! 0 |  7910 | `	}else{` |
|        - |  7911 | `		/* Callback result */` |
|       11 |  7912 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        - |  7913 | `	}` |
|        - |  7914 | `	/* Cleanup the mess left behind */` |
|       11 |  7915 | `	PH7_MemObjRelease(&sResult);` |
|       11 |  7916 | `	SySetRelease(&aArg);` |
|       11 |  7917 | `	return PH7_OK;` |
|        6 |  7918 |  |
|        - |  7919 | `/*` |
|        - |  7920 | ` * bool defined(string $name)` |
|        - |  7921 | ` *  Checks whether a given named constant exists.` |
|        - |  7922 | ` * Parameter:` |
|        - |  7923 | ` *  Name of the desired constant.` |
|        - |  7924 | ` * Return` |
|        - |  7925 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  7926 | ` */` |
|       14 |  7927 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7928 |  |
|        - |  7929 | `	const char *zName;` |
|       16 |  7930 | `	int nLen = 0;` |
|       16 |  7931 | `	int res = 0;` |
|       16 |  7932 | `	if( nArg < 1 ){` |
|        - |  7933 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  7934 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  7935 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7936 | `		return SXRET_OK;` |
|        - |  7937 | `	}` |
|        - |  7938 | `	/* Extract constant name */` |
|       16 |  7939 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7940 | `	/* Perform the lookup */` |
|       16 |  7941 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7942 | `		/* Already defined */` |
|       10 |  7943 | `		res = 1;` |
|        4 |  7944 | `	}` |
|       16 |  7945 | `	ph7_result_bool(pCtx,res);` |
|       16 |  7946 | `	return SXRET_OK;` |
|        9 |  7947 |  |
|        - |  7948 | `/*` |
|        - |  7949 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  7950 | ` * below.` |
|        - |  7951 | ` */` |
|        8 |  7952 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  7953 |  |
|       10 |  7954 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  7955 | `	/* Expand constant value */` |
|       10 |  7956 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7957 |  |
|        - |  7958 | `/*` |
|        - |  7959 | ` * bool define(string $constant_name,expression value)` |
|        - |  7960 | ` *  Defines a named constant at runtime.` |
|        - |  7961 | ` * Parameter:` |
|        - |  7962 | ` *  $constant_name` |
|        - |  7963 | ` *   The name of the constant` |
|        - |  7964 | ` *  $value` |
|        - |  7965 | ` *   Constant value` |
|        - |  7966 | ` * Return:` |
|        - |  7967 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7968 | ` */` |
|       10 |  7969 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7970 |  |
|        - |  7971 | `	const char *zName;  /* Constant name */` |
|        - |  7972 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7973 | `	int nLen = 0;       /* Name length */` |
|        - |  7974 | `	sxi32 rc;` |
|       12 |  7975 | `	if( nArg < 2 ){` |
|        - |  7976 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7977 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7978 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7979 | `		return SXRET_OK;` |
|        - |  7980 | `	}` |
|       12 |  7981 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7982 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7983 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7984 | `		return SXRET_OK;` |
|        - |  7985 | `	}` |
|        - |  7986 | `	/* Extract constant name */` |
|       12 |  7987 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7988 | `	if( nLen < 1 ){` |
|      ! 0 |  7989 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7990 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7991 | `		return SXRET_OK;` |
|        - |  7992 | `	}` |
|        - |  7993 | `	/* Duplicate constant value */` |
|       12 |  7994 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7995 | `	if( pValue == 0 ){` |
|      ! 0 |  7996 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7997 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7998 | `		return SXRET_OK;` |
|        - |  7999 | `	}` |
|        - |  8000 | `	/* Initialize the memory object */` |
|       12 |  8001 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  8002 | `	/* Register the constant */` |
|       12 |  8003 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  8004 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8005 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  8006 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  8007 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8008 | `		return SXRET_OK;` |
|        - |  8009 | `	}` |
|        - |  8010 | `	/* Duplicate constant value */` |
|       12 |  8011 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  8012 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  8013 | `		/* Lower case the constant name */` |
|      ! 0 |  8014 | `		char *zCur = (char *)zName;` |
|      ! 0 |  8015 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  8016 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  8017 | `				/* UTF-8 stream */` |
|      ! 0 |  8018 | `				zCur++;` |
|      ! 0 |  8019 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  8020 | `					zCur++;` |
|      ! 0 |  8021 | `				}` |
|      ! 0 |  8022 | `				continue;` |
|        - |  8023 | `			}` |
|      ! 0 |  8024 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  8025 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  8026 | `				zCur[0] = (char)c;` |
|      ! 0 |  8027 | `			}` |
|      ! 0 |  8028 | `			zCur++;` |
|      ! 0 |  8029 | `		}` |
|        - |  8030 | `		/* Finally,register the constant */` |
|      ! 0 |  8031 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  8032 | `	}` |
|        - |  8033 | `	/* All done,return TRUE */` |
|       12 |  8034 | `	ph7_result_bool(pCtx,1);` |
|       12 |  8035 | `	return SXRET_OK;` |
|        7 |  8036 |  |
|        - |  8037 | `/*` |
|        - |  8038 | ` * value constant(string $name)` |
|        - |  8039 | ` *  Returns the value of a constant` |
|        - |  8040 | ` * Parameter` |
|        - |  8041 | ` *  $name` |
|        - |  8042 | ` *    Name of the constant.` |
|        - |  8043 | ` * Return` |
|        - |  8044 | ` *  Constant value or NULL if not defined.` |
|        - |  8045 | ` */` |
|        8 |  8046 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8047 |  |
|        - |  8048 | `	SyHashEntry *pEntry;` |
|        - |  8049 | `	ph7_constant *pCons;` |
|        - |  8050 | `	const char *zName; /* Constant name */` |
|        - |  8051 | `	ph7_value sVal;    /* Constant value */` |
|        - |  8052 | `	int nLen;` |
|       10 |  8053 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  8054 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  8055 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  8056 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8057 | `		return SXRET_OK;` |
|        - |  8058 | `	}` |
|        - |  8059 | `	/* Extract the constant name */` |
|       10 |  8060 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8061 | `	/* Perform the query */` |
|       10 |  8062 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  8063 | `	if( pEntry == 0 ){` |
|        3 |  8064 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  8065 | `		ph7_result_null(pCtx);` |
|        3 |  8066 | `		return SXRET_OK;` |
|        - |  8067 | `	}` |
|        8 |  8068 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  8069 | `	/* Point to the structure that describe the constant */` |
|        8 |  8070 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  8071 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  8072 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  8073 | `	/* Return that value */` |
|        8 |  8074 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  8075 | `	/* Cleanup */` |
|        8 |  8076 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  8077 | `	return SXRET_OK;` |
|        6 |  8078 |  |
|        - |  8079 | `/*` |
|        - |  8080 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  8081 | ` * defined below.` |
|        - |  8082 | ` */` |
|      414 |  8083 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8084 |  |
|      415 |  8085 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8086 | `	ph7_value sName;` |
|        - |  8087 | `	sxi32 rc;` |
|        - |  8088 | `	/* Prepare the constant name for insertion */` |
|      415 |  8089 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      415 |  8090 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8091 | `	/* Perform the insertion */` |
|      415 |  8092 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      415 |  8093 | `	PH7_MemObjRelease(&sName);` |
|      415 |  8094 | `	return rc;` |
|        1 |  8095 |  |
|        - |  8096 | `/*` |
|        - |  8097 | ` * array get_defined_constants(void)` |
|        - |  8098 | ` *  Returns an associative array with the names of all defined` |
|        - |  8099 | ` *  constants.` |
|        - |  8100 | ` * Parameters` |
|        - |  8101 | ` *  NONE.` |
|        - |  8102 | ` * Returns` |
|        - |  8103 | ` *  Returns the names of all the constants currently defined.` |
|        - |  8104 | ` */` |
|        2 |  8105 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8106 |  |
|        - |  8107 | `	ph7_value *pArray;` |
|        - |  8108 | `	/* Create the array first*/` |
|        3 |  8109 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8110 | `	if( pArray == 0 ){` |
|      ! 0 |  8111 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8112 | `		SXUNUSED(apArg);` |
|        - |  8113 | `		/* Return NULL */` |
|      ! 0 |  8114 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8115 | `		return SXRET_OK;` |
|        - |  8116 | `	}` |
|        - |  8117 | `	/* Fill the array with the defined constants */` |
|        3 |  8118 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  8119 | `	/* Return the created array */` |
|        3 |  8120 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8121 | `	return SXRET_OK;` |
|        2 |  8122 |  |
|        - |  8123 | `/*` |
|        - |  8124 | ` * Section:` |
|        - |  8125 | ` *  Output Control (OB) functions.` |
|        - |  8126 | ` * Status:` |
|        - |  8127 | ` *    Stable.` |
|        - |  8128 | ` */` |
|        - |  8129 | `/* Forward declaration */` |
|        - |  8130 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry);` |
|        - |  8131 | `/*` |
|        - |  8132 | ` * void ob_clean(void)` |
|        - |  8133 | ` *  This function discards the contents of the output buffer.` |
|        - |  8134 | ` *  This function does not destroy the output buffer like ob_end_clean() does.` |
|        - |  8135 | ` * Parameter` |
|        - |  8136 | ` *  None` |
|        - |  8137 | ` * Return` |
|        - |  8138 | ` *  No value is returned.` |
|        - |  8139 | ` */` |
|        2 |  8140 | `static int vm_builtin_ob_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8141 |  |
|        3 |  8142 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8143 | `	VmObEntry *pOb;` |
|        1 |  8144 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8145 | `	SXUNUSED(apArg);` |
|        - |  8146 | `	/* Peek the top most OB */` |
|        3 |  8147 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8148 | `	if( pOb ){` |
|        3 |  8149 | `		SyBlobRelease(&pOb->sOB);` |
|        1 |  8150 | `	}` |
|        3 |  8151 | `	return PH7_OK;` |
|        1 |  8152 |  |
|        - |  8153 | `/*` |
|        - |  8154 | ` * bool ob_end_clean(void)` |
|        - |  8155 | ` *  Clean (erase) the output buffer and turn off output buffering` |
|        - |  8156 | ` *  This function discards the contents of the topmost output buffer and turns` |
|        - |  8157 | ` *  off this output buffering. If you want to further process the buffer's contents` |
|        - |  8158 | ` *  you have to call ob_get_contents() before ob_end_clean() as the buffer contents` |
|        - |  8159 | ` *  are discarded when ob_end_clean() is called.` |
|        - |  8160 | ` * Parameter` |
|        - |  8161 | ` *  None` |
|        - |  8162 | ` * Return` |
|        - |  8163 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first that you called` |
|        - |  8164 | ` *  the function without an active buffer or that for some reason a buffer could not be deleted` |
|        - |  8165 | ` * (possible for special buffer)` |
|        - |  8166 | ` */` |
|     3002 |  8167 | `static int vm_builtin_ob_end_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8168 |  |
|     3004 |  8169 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8170 | `	VmObEntry *pOb;` |
|        - |  8171 | `	/* Pop the top most OB */` |
|     3004 |  8172 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|     3004 |  8173 | `	if( pOb == 0){` |
|        - |  8174 | `		/* No such OB,return FALSE */` |
|      ! 0 |  8175 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8176 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8177 | `		SXUNUSED(apArg);` |
|      ! 0 |  8178 | `	}else{` |
|        - |  8179 | `		/* Release */` |
|     3004 |  8180 | `		VmObRestore(pVm,pOb);` |
|        - |  8181 | `		/* Return true */` |
|     3004 |  8182 | `		ph7_result_bool(pCtx,1);` |
|        - |  8183 | `	}` |
|     3004 |  8184 | `	return PH7_OK;` |
|        2 |  8185 |  |
|        - |  8186 | `/*` |
|        - |  8187 | ` * string ob_get_contents(void)` |
|        - |  8188 | ` *  Gets the contents of the output buffer without clearing it.` |
|        - |  8189 | ` * Parameter` |
|        - |  8190 | ` *  None` |
|        - |  8191 | ` * Return` |
|        - |  8192 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|        - |  8193 | ` */` |
|        6 |  8194 | `static int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8195 |  |
|        7 |  8196 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8197 | `	VmObEntry *pOb;` |
|        - |  8198 | `	/* Peek the top most OB */` |
|        7 |  8199 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        7 |  8200 | `	if( pOb == 0 ){` |
|        - |  8201 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8202 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8203 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8204 | `		SXUNUSED(apArg);` |
|      ! 0 |  8205 | `	}else{` |
|        - |  8206 | `		/* Return contents */` |
|        7 |  8207 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB));` |
|        - |  8208 | `	}` |
|        7 |  8209 | `	return PH7_OK;` |
|        1 |  8210 |  |
|        - |  8211 | `/*` |
|        - |  8212 | ` * string ob_get_clean(void)` |
|        - |  8213 | ` * string ob_get_flush(void)` |
|        - |  8214 | ` *  Get current buffer contents and delete current output buffer.` |
|        - |  8215 | ` * Parameter` |
|        - |  8216 | ` *  None` |
|        - |  8217 | ` * Return` |
|        - |  8218 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|        - |  8219 | ` */` |
|     4206 |  8220 | `static int vm_builtin_ob_get_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8221 |  |
|     4208 |  8222 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8223 | `	VmObEntry *pOb;` |
|        - |  8224 | `	/* Pop the top most OB */` |
|     4208 |  8225 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|     4208 |  8226 | `	if( pOb == 0 ){` |
|        - |  8227 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8228 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8229 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8230 | `		SXUNUSED(apArg);` |
|      ! 0 |  8231 | `	}else{` |
|        - |  8232 | `		/* Return contents */` |
|     4208 |  8233 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB)); /* Will make it's own copy */` |
|        - |  8234 | `		/* Release */` |
|     4208 |  8235 | `		VmObRestore(pVm,pOb);` |
|        - |  8236 | `	}` |
|     4208 |  8237 | `	return PH7_OK;` |
|        2 |  8238 |  |
|        - |  8239 | `/*` |
|        - |  8240 | ` * int ob_get_length(void)` |
|        - |  8241 | ` *  Return the length of the output buffer.` |
|        - |  8242 | ` * Parameter` |
|        - |  8243 | ` *  None` |
|        - |  8244 | ` * Return` |
|        - |  8245 | ` *  Returns the length of the output buffer contents or FALSE if no buffering is active.` |
|        - |  8246 | ` */` |
|        2 |  8247 | `static int vm_builtin_ob_get_length(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8248 |  |
|        3 |  8249 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8250 | `	VmObEntry *pOb;` |
|        - |  8251 | `	/* Peek the top most OB */` |
|        3 |  8252 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8253 | `	if( pOb == 0 ){` |
|        - |  8254 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8255 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8256 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8257 | `		SXUNUSED(apArg);` |
|      ! 0 |  8258 | `	}else{` |
|        - |  8259 | `		/* Return OB length */` |
|        3 |  8260 | `		ph7_result_int64(pCtx,(ph7_int64)SyBlobLength(&pOb->sOB));` |
|        - |  8261 | `	}` |
|        3 |  8262 | `	return PH7_OK;` |
|        1 |  8263 |  |
|        - |  8264 | `/*` |
|        - |  8265 | ` * int ob_get_level(void)` |
|        - |  8266 | ` *  Returns the nesting level of the output buffering mechanism.` |
|        - |  8267 | ` * Parameter` |
|        - |  8268 | ` *  None` |
|        - |  8269 | ` * Return` |
|        - |  8270 | ` *  Returns the level of nested output buffering handlers or zero if output buffering is not active.` |
|        - |  8271 | ` */` |
|        6 |  8272 | `static int vm_builtin_ob_get_level(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8273 |  |
|        7 |  8274 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8275 | `	int iNest;` |
|        3 |  8276 | `	SXUNUSED(nArg); /* cc warning */` |
|        3 |  8277 | `	SXUNUSED(apArg);` |
|        - |  8278 | `	/* Nesting level */` |
|        7 |  8279 | `	iNest = (int)SySetUsed(&pVm->aOB);` |
|        - |  8280 | `	/* Return the nesting value */` |
|        7 |  8281 | `	ph7_result_int(pCtx,iNest);` |
|        7 |  8282 | `	return PH7_OK;` |
|        1 |  8283 |  |
|        - |  8284 | `/*` |
|        - |  8285 | ` * Output Buffer(OB) default VM consumer routine.All VM output is now redirected` |
|        - |  8286 | ` * to a stackable internal buffer,until the user call [ob_get_clean(),ob_end_clean(),...].` |
|        - |  8287 | ` * Refer to the implementation of [ob_start()] for more information.` |
|        - |  8288 | ` */` |
|     6490 |  8289 | `static int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData)` |
|        2 |  8290 |  |
|     6492 |  8291 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|        - |  8292 | `	VmObEntry *pEntry;` |
|        - |  8293 | `	ph7_value sResult;` |
|        - |  8294 | `	/* Peek the top most entry */` |
|     6492 |  8295 | `	pEntry = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|     6492 |  8296 | `	if( pEntry == 0 ){` |
|        - |  8297 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8298 | `		return PH7_OK;` |
|        - |  8299 | `	}` |
|     6492 |  8300 | `	PH7_MemObjInit(pVm,&sResult);` |
|     6492 |  8301 | `	if( ph7_value_is_callable(&pEntry->sCallback) && pVm->nObDepth < 15 ){` |
|        - |  8302 | `		ph7_value sArg,*apArg[2];` |
|        - |  8303 | `		/* Fill the first argument */` |
|      ! 0 |  8304 | `		PH7_MemObjInitFromString(pVm,&sArg,0);` |
|      ! 0 |  8305 | `		PH7_MemObjStringAppend(&sArg,(const char *)pData,nDataLen);` |
|      ! 0 |  8306 | `		apArg[0] = &sArg;` |
|        - |  8307 | `		/* Call the 'filter' callback */` |
|      ! 0 |  8308 | `		pVm->nObDepth++;` |
|      ! 0 |  8309 | `		PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult);` |
|      ! 0 |  8310 | `		pVm->nObDepth--;` |
|      ! 0 |  8311 | `		if( sResult.iFlags & MEMOBJ_STRING ){` |
|        - |  8312 | `			/* Extract the function result */` |
|      ! 0 |  8313 | `			pData = SyBlobData(&sResult.sBlob);` |
|      ! 0 |  8314 | `			nDataLen = SyBlobLength(&sResult.sBlob);` |
|      ! 0 |  8315 | `		}` |
|      ! 0 |  8316 | `		PH7_MemObjRelease(&sArg);` |
|      ! 0 |  8317 | `	}` |
|     6492 |  8318 | `	if( nDataLen > 0 ){` |
|        - |  8319 | `		/* Redirect the VM output to the internal buffer */` |
|     6492 |  8320 | `		SyBlobAppend(&pEntry->sOB,pData,nDataLen);` |
|     3245 |  8321 | `	}` |
|        - |  8322 | `	/* Release */` |
|     6492 |  8323 | `	PH7_MemObjRelease(&sResult);` |
|     6492 |  8324 | `	return PH7_OK;` |
|     3247 |  8325 |  |
|        - |  8326 | `/*` |
|        - |  8327 | ` * Restore the default consumer.` |
|        - |  8328 | ` * Refer to the implementation of [ob_end_clean()] for more` |
|        - |  8329 | ` * information.` |
|        - |  8330 | ` */` |
|     7210 |  8331 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry)` |
|        2 |  8332 |  |
|     7212 |  8333 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|     7212 |  8334 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|        - |  8335 | `		/* No more stackable OB */` |
|     7194 |  8336 | `		pCons->xConsumer = pCons->xDef;` |
|     7194 |  8337 | `		pCons->pUserData = pCons->pDefData;` |
|     3596 |  8338 | `	}` |
|        - |  8339 | `	/* Release OB data */` |
|     7212 |  8340 | `	PH7_MemObjRelease(&pEntry->sCallback);` |
|     7212 |  8341 | `	SyBlobRelease(&pEntry->sOB);` |
|     7212 |  8342 |  |
|        - |  8343 | `/*` |
|        - |  8344 | ` * bool ob_start([ callback $output_callback] )` |
|        - |  8345 | ` * This function will turn output buffering on. While output buffering is active no output` |
|        - |  8346 | ` *  is sent from the script (other than headers), instead the output is stored in an internal` |
|        - |  8347 | ` *  buffer.` |
|        - |  8348 | ` * Parameter` |
|        - |  8349 | ` *  $output_callback` |
|        - |  8350 | ` *   An optional output_callback function may be specified. This function takes a string` |
|        - |  8351 | ` *   as a parameter and should return a string. The function will be called when the output` |
|        - |  8352 | ` *   buffer is flushed (sent) or cleaned (with ob_flush(), ob_clean() or similar function)` |
|        - |  8353 | ` *   or when the output buffer is flushed to the browser at the end of the request.` |
|        - |  8354 | ` *   When output_callback is called, it will receive the contents of the output buffer` |
|        - |  8355 | ` *   as its parameter and is expected to return a new output buffer as a result, which will` |
|        - |  8356 | ` *   be sent to the browser. If the output_callback is not a callable function, this function` |
|        - |  8357 | ` *   will return FALSE.` |
|        - |  8358 | ` *   If the callback function has two parameters, the second parameter is filled with` |
|        - |  8359 | ` *   a bit-field consisting of PHP_OUTPUT_HANDLER_START, PHP_OUTPUT_HANDLER_CONT` |
|        - |  8360 | ` *   and PHP_OUTPUT_HANDLER_END.` |
|        - |  8361 | ` *   If output_callback returns FALSE original input is sent to the browser.` |
|        - |  8362 | ` *   The output_callback parameter may be bypassed by passing a NULL value.` |
|        - |  8363 | ` * Return` |
|        - |  8364 | ` *   Returns TRUE on success or FALSE on failure.` |
|        - |  8365 | ` */` |
|     7210 |  8366 | `static int vm_builtin_ob_start(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8367 |  |
|     7212 |  8368 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8369 | `	VmObEntry sOb;` |
|        - |  8370 | `	sxi32 rc;` |
|        - |  8371 | `	/* Initialize the OB entry */` |
|     7212 |  8372 | `	PH7_MemObjInit(pCtx->pVm,&sOb.sCallback);` |
|     7212 |  8373 | `	SyBlobInit(&sOb.sOB,&pVm->sAllocator);` |
|     7212 |  8374 | `	if( nArg > 0 && (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) ){` |
|        - |  8375 | `		/* Save the callback name for later invocation */` |
|      ! 0 |  8376 | `		PH7_MemObjStore(apArg[0],&sOb.sCallback);` |
|      ! 0 |  8377 | `	}` |
|        - |  8378 | `	/* Push in the stack */` |
|     7212 |  8379 | `	rc = SySetPut(&pVm->aOB,(const void *)&sOb);` |
|     7212 |  8380 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8381 | `		PH7_MemObjRelease(&sOb.sCallback);` |
|      ! 0 |  8382 | `	}else{` |
|     7212 |  8383 | `		ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        - |  8384 | `		/* Substitute the default VM consumer */` |
|     7212 |  8385 | `		if( pCons->xConsumer != VmObConsumer ){` |
|     7194 |  8386 | `			pCons->xDef = pCons->xConsumer;` |
|     7194 |  8387 | `			pCons->pDefData = pCons->pUserData;` |
|        - |  8388 | `			/* Install the new consumer */` |
|     7194 |  8389 | `			pCons->xConsumer = VmObConsumer;` |
|     7194 |  8390 | `			pCons->pUserData = pVm;` |
|     3596 |  8391 | `		}` |
|        - |  8392 | `	}` |
|     7212 |  8393 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     7212 |  8394 | `	return PH7_OK;` |
|        2 |  8395 |  |
|        - |  8396 | `/*` |
|        - |  8397 | ` * Flush Output buffer to the default VM output consumer.` |
|        - |  8398 | ` * Refer to the implementation of [ob_flush()] for more` |
|        - |  8399 | ` * information.` |
|        - |  8400 | ` */` |
|        4 |  8401 | `static sxi32 VmObFlush(ph7_vm *pVm,VmObEntry *pEntry,int bRelease)` |
|        1 |  8402 |  |
|        5 |  8403 | `	SyBlob *pBlob = &pEntry->sOB;` |
|        - |  8404 | `	sxi32 rc;` |
|        - |  8405 | `	/* Flush contents */` |
|        5 |  8406 | `	rc = PH7_OK;` |
|        5 |  8407 | `	if( SyBlobLength(pBlob) > 0 ){` |
|        - |  8408 | `		/* Call the VM output consumer */` |
|        5 |  8409 | `		rc = pVm->sVmConsumer.xDef(SyBlobData(pBlob),SyBlobLength(pBlob),pVm->sVmConsumer.pDefData);` |
|        - |  8410 | `		/* Increment VM output counter */` |
|        5 |  8411 | `		pVm->nOutputLen += SyBlobLength(pBlob);` |
|        5 |  8412 | `		if( rc != PH7_ABORT ){` |
|        5 |  8413 | `			rc = PH7_OK;` |
|        2 |  8414 | `		}` |
|        2 |  8415 | `	}` |
|        5 |  8416 | `	if( bRelease ){` |
|        3 |  8417 | `		VmObRestore(&(*pVm),pEntry);` |
|        2 |  8418 | `	}else{` |
|        - |  8419 | `		/* Reset the blob */` |
|        3 |  8420 | `		SyBlobReset(pBlob);` |
|        - |  8421 | `	}` |
|        5 |  8422 | `	return rc;` |
|        1 |  8423 |  |
|        - |  8424 | `/*` |
|        - |  8425 | ` * void ob_flush(void)` |
|        - |  8426 | ` * void flush(void)` |
|        - |  8427 | ` *  Flush (send) the output buffer.` |
|        - |  8428 | ` * Parameter` |
|        - |  8429 | ` *  None` |
|        - |  8430 | ` * Return` |
|        - |  8431 | ` *  No return value.` |
|        - |  8432 | ` */` |
|        2 |  8433 | `static int vm_builtin_ob_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8434 |  |
|        3 |  8435 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8436 | `	VmObEntry *pOb;` |
|        - |  8437 | `	sxi32 rc;` |
|        - |  8438 | `	/* Peek the top most OB entry */` |
|        3 |  8439 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8440 | `	if( pOb == 0 ){` |
|        - |  8441 | `		/* Empty stack,return immediately */` |
|      ! 0 |  8442 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8443 | `		SXUNUSED(apArg);` |
|      ! 0 |  8444 | `		return PH7_OK;` |
|        - |  8445 | `	}` |
|        - |  8446 | `	/* Flush contents */` |
|        3 |  8447 | `	rc = VmObFlush(pVm,pOb,FALSE);` |
|        3 |  8448 | `	return rc;` |
|        2 |  8449 |  |
|        - |  8450 | `/*` |
|        - |  8451 | ` * bool ob_end_flush(void)` |
|        - |  8452 | ` *  Flush (send) the output buffer and turn off output buffering.` |
|        - |  8453 | ` * Parameter` |
|        - |  8454 | ` *  None` |
|        - |  8455 | ` * Return` |
|        - |  8456 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first` |
|        - |  8457 | ` *  that you called the function without an active buffer or that for some reason` |
|        - |  8458 | ` *  a buffer could not be deleted (possible for special buffer).` |
|        - |  8459 | ` */` |
|        2 |  8460 | `static int vm_builtin_ob_end_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8461 |  |
|        3 |  8462 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8463 | `	VmObEntry *pOb;` |
|        - |  8464 | `	sxi32 rc;` |
|        - |  8465 | `	/* Pop the top most OB entry */` |
|        3 |  8466 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|        3 |  8467 | `	if( pOb == 0 ){` |
|        - |  8468 | `		/* Empty stack,return FALSE */` |
|      ! 0 |  8469 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8470 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8471 | `		SXUNUSED(apArg);` |
|      ! 0 |  8472 | `		return PH7_OK;` |
|        - |  8473 | `	}` |
|        - |  8474 | `	/* Flush contents */` |
|        3 |  8475 | `	rc = VmObFlush(pVm,pOb,TRUE);` |
|        - |  8476 | `	/* Return true */` |
|        3 |  8477 | `	ph7_result_bool(pCtx,1);` |
|        3 |  8478 | `	return rc;` |
|        2 |  8479 |  |
|        - |  8480 | `/*` |
|        - |  8481 | ` * void ob_implicit_flush([int $flag = true ])` |
|        - |  8482 | ` *  ob_implicit_flush() will turn implicit flushing on or off.` |
|        - |  8483 | ` *  Implicit flushing will result in a flush operation after every` |
|        - |  8484 | ` *  output call, so that explicit calls to flush() will no longer be needed.` |
|        - |  8485 | ` * Parameter` |
|        - |  8486 | ` *  $flag` |
|        - |  8487 | ` *   TRUE to turn implicit flushing on, FALSE otherwise.` |
|        - |  8488 | ` * Return` |
|        - |  8489 | ` *   Nothing` |
|        - |  8490 | ` */` |
|        4 |  8491 | `static int vm_builtin_ob_implicit_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8492 |  |
|        - |  8493 | `	/* NOTE: As of this version,this function is a no-op.` |
|        - |  8494 | `	 * PH7 is smart enough to flush it's internal buffer when appropriate.` |
|        - |  8495 | `	 */` |
|        2 |  8496 | `	SXUNUSED(pCtx);` |
|        2 |  8497 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  8498 | `	SXUNUSED(apArg);` |
|        5 |  8499 | `	return PH7_OK;` |
|        1 |  8500 |  |
|        - |  8501 | `/*` |
|        - |  8502 | ` * array ob_list_handlers(void)` |
|        - |  8503 | ` *  Lists all output handlers in use.` |
|        - |  8504 | ` * Parameter` |
|        - |  8505 | ` *  None` |
|        - |  8506 | ` * Return` |
|        - |  8507 | ` *  This will return an array with the output handlers in use (if any).` |
|        - |  8508 | ` */` |
|        2 |  8509 | `static int vm_builtin_ob_list_handlers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8510 |  |
|        3 |  8511 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8512 | `	ph7_value *pArray;` |
|        - |  8513 | `	VmObEntry *aEntry;` |
|        - |  8514 | `	ph7_value sVal;` |
|        - |  8515 | `	sxu32 n;` |
|        3 |  8516 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|        - |  8517 | `		/* Empty stack,return null */` |
|      ! 0 |  8518 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8519 | `		return PH7_OK;` |
|        - |  8520 | `	}` |
|        - |  8521 | `	/* Create a new array */` |
|        3 |  8522 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8523 | `	if( pArray == 0 ){` |
|        - |  8524 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8525 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8526 | `		SXUNUSED(apArg);` |
|      ! 0 |  8527 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8528 | `		return PH7_OK;` |
|        - |  8529 | `	}` |
|        3 |  8530 | `	PH7_MemObjInit(pVm,&sVal);` |
|        - |  8531 | `	/* Point to the installed OB entries */` |
|        3 |  8532 | `	aEntry = (VmObEntry *)SySetBasePtr(&pVm->aOB);` |
|        - |  8533 | `	/* Perform the requested operation */` |
|        5 |  8534 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; n++ ){` |
|        3 |  8535 | `		VmObEntry *pEntry = &aEntry[n];` |
|        - |  8536 | `		/* Extract handler name */` |
|        3 |  8537 | `		SyBlobReset(&sVal.sBlob);` |
|        3 |  8538 | `		if( pEntry->sCallback.iFlags & MEMOBJ_STRING ){` |
|        - |  8539 | `			/* Callback,dup it's name */` |
|      ! 0 |  8540 | `			SyBlobDup(&pEntry->sCallback.sBlob,&sVal.sBlob);` |
|        3 |  8541 | `		}else if( pEntry->sCallback.iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  8542 | `			SyBlobAppend(&sVal.sBlob,"Class Method",sizeof("Class Method")-1);` |
|      ! 0 |  8543 | `		}else{` |
|        3 |  8544 | `			SyBlobAppend(&sVal.sBlob,"default output handler",sizeof("default output handler")-1);` |
|        - |  8545 | `		}` |
|        3 |  8546 | `		sVal.iFlags = MEMOBJ_STRING;` |
|        - |  8547 | `		/* Perform the insertion */` |
|        3 |  8548 | `		ph7_array_add_elem(pArray,0/* Automatic index assign */,&sVal /* Will make it's own copy */);` |
|        2 |  8549 | `	}` |
|        3 |  8550 | `	PH7_MemObjRelease(&sVal);` |
|        - |  8551 | `	/* Return the freshly created array */` |
|        3 |  8552 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8553 | `	return PH7_OK;` |
|        2 |  8554 |  |
|        - |  8555 | `/*` |
|        - |  8556 | ` * Section:` |
|        - |  8557 | ` *  Random numbers/string generators.` |
|        - |  8558 | ` * Status:` |
|        - |  8559 | ` *    Stable.` |
|        - |  8560 | ` */` |
|        - |  8561 | `/*` |
|        - |  8562 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  8563 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  8564 | ` * used by te SQLite3 library.` |
|        - |  8565 | ` */` |
|     1525 |  8566 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  8567 |  |
|        - |  8568 | `	sxu32 iNum;` |
|     1527 |  8569 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     1527 |  8570 | `	return iNum;` |
|        2 |  8571 |  |
|        - |  8572 | `/*` |
|        - |  8573 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  8574 | ` * Note that the generated string is NOT null terminated.` |
|        - |  8575 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  8576 | ` * by te SQLite3 library.` |
|        - |  8577 | ` */` |
|    49940 |  8578 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  8579 |  |
|        - |  8580 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  8581 | `	int i;` |
|        - |  8582 | `	/* Generate a binary string first */` |
|    49942 |  8583 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  8584 | `	/* Turn the binary string into english based alphabet */` |
|   549510 |  8585 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   499570 |  8586 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   249786 |  8587 | `	 }` |
|    49942 |  8588 |  |
|        - |  8589 | `/*` |
|        - |  8590 | ` * int rand()` |
|        - |  8591 | ` * int mt_rand()` |
|        - |  8592 | ` * int rand(int $min,int $max)` |
|        - |  8593 | ` * int mt_rand(int $min,int $max)` |
|        - |  8594 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  8595 | ` * Parameter` |
|        - |  8596 | ` *  $min` |
|        - |  8597 | ` *    The lowest value to return (default: 0)` |
|        - |  8598 | ` *  $max` |
|        - |  8599 | ` *   The highest value to return (default: getrandmax())` |
|        - |  8600 | ` * Return` |
|        - |  8601 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  8602 | ` * Note:` |
|        - |  8603 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8604 | ` *  by te SQLite3 library.` |
|        - |  8605 | ` */` |
|       20 |  8606 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8607 |  |
|        - |  8608 | `	sxu32 iNum;` |
|        - |  8609 | `	/* Generate the random number */` |
|       21 |  8610 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  8611 | `	if( nArg > 1 ){` |
|        - |  8612 | `		sxu32 iMin,iMax;` |
|        3 |  8613 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  8614 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  8615 | `		if( iMin < iMax ){` |
|        3 |  8616 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  8617 | `			if( iDiv > 0 ){` |
|        3 |  8618 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  8619 | `			}` |
|        1 |  8620 | `		}else if(iMax > 0 ){` |
|      ! 0 |  8621 | `			iNum %= iMax;` |
|      ! 0 |  8622 | `		}` |
|        1 |  8623 | `	}` |
|        - |  8624 | `	/* Return the number */` |
|       21 |  8625 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  8626 | `	return SXRET_OK;` |
|        1 |  8627 |  |
|        - |  8628 | `/*` |
|        - |  8629 | ` * int getrandmax(void)` |
|        - |  8630 | ` * int mt_getrandmax(void)` |
|        - |  8631 | ` * int rc4_getrandmax(void)` |
|        - |  8632 | ` *   Show largest possible random value` |
|        - |  8633 | ` * Return` |
|        - |  8634 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  8635 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  8636 | ` * Note:` |
|        - |  8637 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8638 | ` *  by te SQLite3 library.` |
|        - |  8639 | ` */` |
|        4 |  8640 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8641 |  |
|        2 |  8642 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  8643 | `	SXUNUSED(apArg);` |
|        5 |  8644 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  8645 | `	return SXRET_OK;` |
|        1 |  8646 |  |
|        - |  8647 | `/*` |
|        - |  8648 | ` * string rand_str()` |
|        - |  8649 | ` * string rand_str(int $len)` |
|        - |  8650 | ` *  Generate a random string (English alphabet).` |
|        - |  8651 | ` * Parameter` |
|        - |  8652 | ` *  $len` |
|        - |  8653 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  8654 | ` * Return` |
|        - |  8655 | ` *   A pseudo random string.` |
|        - |  8656 | ` * Note:` |
|        - |  8657 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8658 | ` *  by te SQLite3 library.` |
|        - |  8659 | ` *  This function is a symisc extension.` |
|        - |  8660 | ` */` |
|      120 |  8661 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8662 |  |
|        - |  8663 | `	char zString[1024];` |
|      122 |  8664 | `	int iLen = 0x10;` |
|      122 |  8665 | `	if( nArg > 0 ){` |
|        - |  8666 | `		/* Get the desired length */` |
|      122 |  8667 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  8668 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  8669 | `			/* Default length */` |
|        3 |  8670 | `			iLen = 0x10;` |
|        1 |  8671 | `		}` |
|       60 |  8672 | `	}` |
|        - |  8673 | `	/* Generate the random string */` |
|      122 |  8674 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  8675 | `	/* Return the generated string */` |
|      122 |  8676 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  8677 | `	return SXRET_OK;` |
|        2 |  8678 |  |
|        - |  8679 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  8680 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  8681 | `/* Unique ID private data */` |
|        - |  8682 | `struct unique_id_data` |
|        - |  8683 |  |
|        - |  8684 | `	ph7_context *pCtx; /* Call context */` |
|        - |  8685 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  8686 | `};` |
|        - |  8687 | `/*` |
|        - |  8688 | ` * Binary to hex consumer callback.` |
|        - |  8689 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  8690 | ` * defined below.` |
|        - |  8691 | ` */` |
|      192 |  8692 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  8693 |  |
|      193 |  8694 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  8695 | `	sxu32 nBuflen;` |
|        - |  8696 | `	/* Extract result buffer length */` |
|      193 |  8697 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  8698 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  8699 | `			/*` |
|        - |  8700 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  8701 | `			 * string will be 13 characters long` |
|        - |  8702 | `			 */` |
|       25 |  8703 | `		return SXERR_ABORT;` |
|        - |  8704 | `	}` |
|      169 |  8705 | `	if( nBuflen > 22 ){` |
|      ! 0 |  8706 | `		return SXERR_ABORT;` |
|        - |  8707 | `	}` |
|        - |  8708 | `	/* Safely Consume the hex stream */` |
|      169 |  8709 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  8710 | `	return SXRET_OK;` |
|       97 |  8711 |  |
|        - |  8712 | `/*` |
|        - |  8713 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  8714 | ` *  Generate a unique ID` |
|        - |  8715 | ` * Parameter` |
|        - |  8716 | ` * $prefix` |
|        - |  8717 | ` *  Append this prefix to the generated unique ID.` |
|        - |  8718 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  8719 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  8720 | ` * $more_entropy` |
|        - |  8721 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  8722 | ` *  that the result will be unique.` |
|        - |  8723 | ` * Return` |
|        - |  8724 | ` *  Returns the unique identifier, as a string.` |
|        - |  8725 | ` */` |
|       24 |  8726 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8727 |  |
|        - |  8728 | `	struct unique_id_data sUniq;` |
|        - |  8729 | `	unsigned char zDigest[20];` |
|       25 |  8730 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8731 | `	const char *zPrefix;` |
|        - |  8732 | `	SHA1Context sCtx;` |
|        - |  8733 | `	char zRandom[7];` |
|        - |  8734 | `	int nPrefix;` |
|        - |  8735 | `	int entropy;` |
|        - |  8736 | `	/* Generate a random string first */` |
|       25 |  8737 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  8738 | `	/* Initialize fields */` |
|       25 |  8739 | `	zPrefix = 0;` |
|       25 |  8740 | `	nPrefix = 0;` |
|       25 |  8741 | `	entropy = 0;` |
|       25 |  8742 | `	if( nArg > 0 ){` |
|        - |  8743 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  8744 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  8745 | `		if( nArg > 1 ){` |
|      ! 0 |  8746 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  8747 | `		}` |
|      ! 0 |  8748 | `	}` |
|       25 |  8749 | `	SHA1Init(&sCtx);` |
|        - |  8750 | `	/* Generate the random ID */` |
|       25 |  8751 | `	if( nPrefix > 0 ){` |
|      ! 0 |  8752 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  8753 | `	}` |
|        - |  8754 | `	/* Append the random ID */` |
|       25 |  8755 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  8756 | `	/* Append the random string */` |
|       25 |  8757 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  8758 | `	/* Increment the number */` |
|       25 |  8759 | `	pVm->unique_id++;` |
|       25 |  8760 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  8761 | `	/* Hexify the digest */` |
|       25 |  8762 | `	sUniq.pCtx = pCtx;` |
|       25 |  8763 | `	sUniq.entropy = entropy;` |
|       25 |  8764 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  8765 | `	/* All done */` |
|       25 |  8766 | `	return PH7_OK;` |
|        1 |  8767 |  |
|        - |  8768 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  8769 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  8770 | `/*` |
|        - |  8771 | ` * Section:` |
|        - |  8772 | ` *  Language construct implementation as foreign functions.` |
|        - |  8773 | ` * Status:` |
|        - |  8774 | ` *    Stable.` |
|        - |  8775 | ` */` |
|        - |  8776 | `/*` |
|        - |  8777 | ` * void echo($string...)` |
|        - |  8778 | ` *  Output one or more messages.` |
|        - |  8779 | ` * Parameters` |
|        - |  8780 | ` *  $string` |
|        - |  8781 | ` *   Message to output.` |
|        - |  8782 | ` * Return` |
|        - |  8783 | ` *  NULL.` |
|        - |  8784 | ` */` |
|      ! 0 |  8785 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  8786 |  |
|        - |  8787 | `	const char *zData;` |
|      ! 0 |  8788 | `	int nDataLen = 0;` |
|        - |  8789 | `	ph7_vm *pVm;` |
|        - |  8790 | `	int i,rc;` |
|        - |  8791 | `	/* Point to the target VM */` |
|      ! 0 |  8792 | `	pVm = pCtx->pVm;` |
|        - |  8793 | `	/* Output */` |
|      ! 0 |  8794 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  8795 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  8796 | `		if( nDataLen > 0 ){` |
|      ! 0 |  8797 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  8798 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  8799 | `				/* Increment output length */` |
|      ! 0 |  8800 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  8801 | `			}` |
|      ! 0 |  8802 | `			if( rc == SXERR_ABORT ){` |
|        - |  8803 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  8804 | `				return PH7_ABORT;` |
|        - |  8805 | `			}` |
|      ! 0 |  8806 | `		}` |
|      ! 0 |  8807 | `	}` |
|      ! 0 |  8808 | `	return SXRET_OK;` |
|      ! 0 |  8809 |  |
|        - |  8810 | `/*` |
|        - |  8811 | ` * int print($string...)` |
|        - |  8812 | ` *  Output one or more messages.` |
|        - |  8813 | ` * Parameters` |
|        - |  8814 | ` *  $string` |
|        - |  8815 | ` *   Message to output.` |
|        - |  8816 | ` * Return` |
|        - |  8817 | ` *  1 always.` |
|        - |  8818 | ` */` |
|        2 |  8819 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8820 |  |
|        - |  8821 | `	const char *zData;` |
|        3 |  8822 | `	int nDataLen = 0;` |
|        - |  8823 | `	ph7_vm *pVm;` |
|        - |  8824 | `	int i,rc;` |
|        - |  8825 | `	/* Point to the target VM */` |
|        3 |  8826 | `	pVm = pCtx->pVm;` |
|        - |  8827 | `	/* Output */` |
|        5 |  8828 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  8829 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  8830 | `		if( nDataLen > 0 ){` |
|        3 |  8831 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  8832 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  8833 | `				/* Increment output length */` |
|        3 |  8834 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  8835 | `			}` |
|        3 |  8836 | `			if( rc == SXERR_ABORT ){` |
|        - |  8837 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  8838 | `				return PH7_ABORT;` |
|        - |  8839 | `			}` |
|        1 |  8840 | `		}` |
|        2 |  8841 | `	}` |
|        - |  8842 | `	/* Return 1 */` |
|        3 |  8843 | `	ph7_result_int(pCtx,1);` |
|        3 |  8844 | `	return SXRET_OK;` |
|        2 |  8845 |  |
|        - |  8846 | `/*` |
|        - |  8847 | ` * void exit(string $msg)` |
|        - |  8848 | ` * void exit(int $status)` |
|        - |  8849 | ` * void die(string $ms)` |
|        - |  8850 | ` * void die(int $status)` |
|        - |  8851 | ` *   Output a message and terminate program execution.` |
|        - |  8852 | ` * Parameter` |
|        - |  8853 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  8854 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  8855 | ` *  and not printed` |
|        - |  8856 | ` * Return` |
|        - |  8857 | ` *  NULL` |
|        - |  8858 | ` */` |
|      ! 0 |  8859 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  8860 |  |
|      ! 0 |  8861 | `	if( nArg > 0 ){` |
|      ! 0 |  8862 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  8863 | `			const char *zData;` |
|      ! 0 |  8864 | `			int iLen = 0;` |
|        - |  8865 | `			/* Print exit message */` |
|      ! 0 |  8866 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  8867 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  8868 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  8869 | `			sxi32 iExitStatus;` |
|        - |  8870 | `			/* Record exit status code */` |
|      ! 0 |  8871 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  8872 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  8873 | `		}` |
|      ! 0 |  8874 | `	}` |
|        - |  8875 | `	/* Check if we are in an included file */` |
|      ! 0 |  8876 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  8877 | `		/* Exit the entire process */` |
|      ! 0 |  8878 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  8879 | `	}` |
|        - |  8880 | `	/* Abort processing immediately */` |
|      ! 0 |  8881 | `	return PH7_ABORT;` |
|      ! 0 |  8882 |  |
|        - |  8883 | `/*` |
|        - |  8884 | ` * bool isset($var,...)` |
|        - |  8885 | ` *  Finds out whether a variable is set.` |
|        - |  8886 | ` * Parameters` |
|        - |  8887 | ` *  One or more variable to check.` |
|        - |  8888 | ` * Return` |
|        - |  8889 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  8890 | ` */` |
|    61258 |  8891 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8892 |  |
|        - |  8893 | `	ph7_value *pObj;` |
|    61260 |  8894 | `	int res = 0;` |
|        - |  8895 | `	int i;` |
|    61260 |  8896 | `	if( nArg < 1 ){` |
|        - |  8897 | `		/* Missing arguments,return false */` |
|      ! 0 |  8898 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  8899 | `		return SXRET_OK;` |
|        - |  8900 | `	}` |
|        - |  8901 | `	/* Iterate over available arguments */` |
|    81516 |  8902 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    61260 |  8903 | `		pObj = apArg[i];` |
|    61260 |  8904 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    40724 |  8905 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  8906 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  8907 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  8908 | `			}` |
|    20361 |  8909 | `		}` |
|    61260 |  8910 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    61260 |  8911 | `		if( !res ){` |
|        - |  8912 | `			/* Variable not set,return FALSE */` |
|    41004 |  8913 | `			ph7_result_bool(pCtx,0);` |
|    41004 |  8914 | `			return SXRET_OK;` |
|        - |  8915 | `		}` |
|    10130 |  8916 | `	}` |
|        - |  8917 | `	/* All given variable are set,return TRUE */` |
|    20258 |  8918 | `	ph7_result_bool(pCtx,1);` |
|    20258 |  8919 | `	return SXRET_OK;` |
|    30631 |  8920 |  |
|        - |  8921 | `/*` |
|        - |  8922 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  8923 | ` * frame,the reference table and discard it's contents.` |
|        - |  8924 | ` * This function never fail and always return SXRET_OK.` |
|        - |  8925 | ` */` |
|  2855814 |  8926 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  8927 |  |
|        - |  8928 | `	ph7_value *pObj;` |
|        - |  8929 | `	VmRefObj *pRef;` |
|  2855816 |  8930 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2855816 |  8931 | `	if( pObj ){` |
|        - |  8932 | `		/* Release the object */` |
|  2855816 |  8933 | `		PH7_MemObjRelease(pObj);` |
|  1427907 |  8934 | `	}` |
|        - |  8935 | `	/* Remove old reference links */` |
|  2855816 |  8936 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2855816 |  8937 | `	if( pRef ){` |
|  2855796 |  8938 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  8939 | `		/* Unlink from the reference table */` |
|  2855796 |  8940 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2855796 |  8941 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  8942 | `			VmSlot sFree;` |
|        - |  8943 | `			/* Restore to the free list */` |
|  2855790 |  8944 | `			sFree.nIdx = nObjIdx;` |
|  2855790 |  8945 | `			sFree.pUserData = 0;` |
|  2855790 |  8946 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1427894 |  8947 | `		}` |
|  1427897 |  8948 | `	}` |
|  2855816 |  8949 | `	return SXRET_OK;` |
|        2 |  8950 |  |
|        - |  8951 | `/*` |
|        - |  8952 | ` * void unset($var,...)` |
|        - |  8953 | ` *   Unset one or more given variable.` |
|        - |  8954 | ` * Parameters` |
|        - |  8955 | ` *  One or more variable to unset.` |
|        - |  8956 | ` * Return` |
|        - |  8957 | ` *  Nothing.` |
|        - |  8958 | ` */` |
|     3012 |  8959 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8960 |  |
|        - |  8961 | `	ph7_value *pObj;` |
|        - |  8962 | `	ph7_vm *pVm;` |
|        - |  8963 | `	int i;` |
|        - |  8964 | `	/* Point to the target VM */` |
|     3014 |  8965 | `	pVm = pCtx->pVm;` |
|        - |  8966 | `	/* Iterate and unset */` |
|     9116 |  8967 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6104 |  8968 | `		pObj = apArg[i];` |
|     6104 |  8969 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      728 |  8970 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  8971 | `				/* Throw an error */` |
|      ! 0 |  8972 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  8973 | `			}` |
|      365 |  8974 | `		}else{` |
|     5377 |  8975 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  8976 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     5377 |  8977 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     5371 |  8978 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2685 |  8979 | `			}` |
|        - |  8980 | `		}` |
|     3053 |  8981 | `	}` |
|     3014 |  8982 | `	return SXRET_OK;` |
|        2 |  8983 |  |
|        - |  8984 | `/*` |
|        - |  8985 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  8986 | ` */` |
|      108 |  8987 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8988 |  |
|      109 |  8989 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      109 |  8990 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  8991 | `	ph7_value *pObj;` |
|        - |  8992 | `	sxu32 nIdx;` |
|        - |  8993 | `	/* Extract the memory object */` |
|      109 |  8994 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      109 |  8995 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      109 |  8996 | `	if( pObj ){` |
|      109 |  8997 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      107 |  8998 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  8999 | `				SyString sName;` |
|        - |  9000 | `				ph7_value sKey;` |
|        - |  9001 | `				/* Perform the insertion */` |
|      107 |  9002 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      107 |  9003 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      107 |  9004 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      107 |  9005 | `				PH7_MemObjRelease(&sKey);` |
|       53 |  9006 | `			}` |
|       53 |  9007 | `		}` |
|       54 |  9008 | `	}` |
|      109 |  9009 | `	return SXRET_OK;` |
|        1 |  9010 |  |
|        - |  9011 | `/*` |
|        - |  9012 | ` * array get_defined_vars(void)` |
|        - |  9013 | ` *  Returns an array of all defined variables.` |
|        - |  9014 | ` * Parameter` |
|        - |  9015 | ` *  None` |
|        - |  9016 | ` * Return` |
|        - |  9017 | ` *  An array with all the variables defined in the current scope.` |
|        - |  9018 | ` */` |
|        2 |  9019 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9020 |  |
|        3 |  9021 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9022 | `	ph7_value *pArray;` |
|        - |  9023 | `	/* Create a new array */` |
|        3 |  9024 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9025 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9026 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9027 | `		SXUNUSED(apArg);` |
|        - |  9028 | `		/* Return NULL */` |
|      ! 0 |  9029 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9030 | `		return SXRET_OK;` |
|        - |  9031 | `	}` |
|        - |  9032 | `	/* Superglobals first */` |
|        3 |  9033 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  9034 | `	/* Then variable defined in the current frame */` |
|        3 |  9035 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  9036 | `	/* Finally,return the created array */` |
|        3 |  9037 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9038 | `	return SXRET_OK;` |
|        2 |  9039 |  |
|        - |  9040 | `/*` |
|        - |  9041 | ` * bool gettype($var)` |
|        - |  9042 | ` *  Get the type of a variable` |
|        - |  9043 | ` * Parameters` |
|        - |  9044 | ` *   $var` |
|        - |  9045 | ` *    The variable being type checked.` |
|        - |  9046 | ` * Return` |
|        - |  9047 | ` *   String representation of the given variable type.` |
|        - |  9048 | ` */` |
|       30 |  9049 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9050 |  |
|       32 |  9051 | `	const char *zType = "Empty";` |
|       32 |  9052 | `	if( nArg > 0 ){` |
|       32 |  9053 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       15 |  9054 | `	}` |
|        - |  9055 | `	/* Return the variable type */` |
|       32 |  9056 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       32 |  9057 | `	return SXRET_OK;` |
|        2 |  9058 |  |
|        - |  9059 | `/*` |
|        - |  9060 | ` * string get_resource_type(resource $handle)` |
|        - |  9061 | ` *  This function gets the type of the given resource.` |
|        - |  9062 | ` * Parameters` |
|        - |  9063 | ` *  $handle` |
|        - |  9064 | ` *  The evaluated resource handle.` |
|        - |  9065 | ` * Return` |
|        - |  9066 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  9067 | ` *  representing its type. If the type is not identified by this function` |
|        - |  9068 | ` *  the return value will be the string Unknown.` |
|        - |  9069 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  9070 | ` *  is not a resource.` |
|        - |  9071 | ` */` |
|        2 |  9072 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9073 |  |
|        3 |  9074 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  9075 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  9076 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9077 | `		return PH7_OK;` |
|        - |  9078 | `	}` |
|        3 |  9079 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  9080 | `	return SXRET_OK;` |
|        2 |  9081 |  |
|        - |  9082 | `/*` |
|        - |  9083 | ` * void var_dump(expression,....)` |
|        - |  9084 | ` *   var_dump � Dumps information about a variable` |
|        - |  9085 | ` * Parameters` |
|        - |  9086 | ` *   One or more expression to dump.` |
|        - |  9087 | ` * Returns` |
|        - |  9088 | ` *  Nothing.` |
|        - |  9089 | ` */` |
|      220 |  9090 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9091 |  |
|        - |  9092 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  9093 | `	int i;` |
|      222 |  9094 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  9095 | `	/* Dump one or more expressions */` |
|      448 |  9096 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      228 |  9097 | `		ph7_value *pObj = apArg[i];` |
|        - |  9098 | `		/* Reset the working buffer */` |
|      228 |  9099 | `		SyBlobReset(&sDump);` |
|        - |  9100 | `		/* Dump the given expression */` |
|      228 |  9101 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  9102 | `		/* Output */` |
|      228 |  9103 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      228 |  9104 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      113 |  9105 | `		}` |
|      115 |  9106 | `	}` |
|        - |  9107 | `	/* Release the working buffer */` |
|      222 |  9108 | `	SyBlobRelease(&sDump);` |
|      222 |  9109 | `	return SXRET_OK;` |
|        2 |  9110 |  |
|        - |  9111 | `/*` |
|        - |  9112 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  9113 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  9114 | ` * Parameters` |
|        - |  9115 | ` *   expression: Expression to dump` |
|        - |  9116 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  9117 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  9118 | ` *            print_r() will return the information rather than print it.` |
|        - |  9119 | ` * Return` |
|        - |  9120 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  9121 | ` *  Otherwise, the return value is TRUE.` |
|        - |  9122 | ` */` |
|       16 |  9123 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9124 |  |
|       17 |  9125 | `	int ret_string = 0;` |
|        - |  9126 | `	SyBlob sDump;` |
|       17 |  9127 | `	if( nArg < 1 ){` |
|        - |  9128 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9129 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9130 | `		return SXRET_OK;` |
|        - |  9131 | `	}` |
|       17 |  9132 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  9133 | `	if ( nArg > 1 ){` |
|        - |  9134 | `		/* Where to redirect output */` |
|       11 |  9135 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  9136 | `	}` |
|        - |  9137 | `	/* Generate dump */` |
|       17 |  9138 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  9139 | `	if( !ret_string ){` |
|        - |  9140 | `		/* Output dump */` |
|        7 |  9141 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9142 | `		/* Return true */` |
|        7 |  9143 | `		ph7_result_bool(pCtx,1);` |
|        4 |  9144 | `	}else{` |
|        - |  9145 | `		/* Generated dump as return value */` |
|       11 |  9146 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9147 | `	}` |
|        - |  9148 | `	/* Release the working buffer */` |
|       17 |  9149 | `	SyBlobRelease(&sDump);` |
|       17 |  9150 | `	return SXRET_OK;` |
|        9 |  9151 |  |
|        - |  9152 | `/*` |
|        - |  9153 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  9154 | ` * Same job as print_r. (see coment above)` |
|        - |  9155 | ` */` |
|        2 |  9156 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9157 |  |
|        3 |  9158 | `	int ret_string = 0;` |
|        - |  9159 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  9160 | `	if( nArg < 1 ){` |
|        - |  9161 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9162 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9163 | `		return SXRET_OK;` |
|        - |  9164 | `	}` |
|        3 |  9165 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  9166 | `	if ( nArg > 1 ){` |
|        - |  9167 | `		/* Where to redirect output */` |
|        3 |  9168 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  9169 | `	}` |
|        - |  9170 | `	/* Generate dump */` |
|        3 |  9171 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  9172 | `	if( !ret_string ){` |
|        - |  9173 | `		/* Output dump */` |
|      ! 0 |  9174 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9175 | `		/* Return NULL */` |
|      ! 0 |  9176 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9177 | `	}else{` |
|        - |  9178 | `		/* Generated dump as return value */` |
|        3 |  9179 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9180 | `	}` |
|        - |  9181 | `	/* Release the working buffer */` |
|        3 |  9182 | `	SyBlobRelease(&sDump);` |
|        3 |  9183 | `	return SXRET_OK;` |
|        2 |  9184 |  |
|        - |  9185 | `/*` |
|        - |  9186 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  9187 | ` *  Set/get the various assert flags.` |
|        - |  9188 | ` * Parameter` |
|        - |  9189 | ` * $what` |
|        - |  9190 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  9191 | ` *   ASSERT_WARNING         Issue a warning for each failed assertion` |
|        - |  9192 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  9193 | ` *   ASSERT_QUIET_EVAL      Not used` |
|        - |  9194 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  9195 | ` * $value` |
|        - |  9196 | ` *   An optional new value for the option.` |
|        - |  9197 | ` * Return` |
|        - |  9198 | ` *  Old setting on success or FALSE on failure.` |
|        - |  9199 | ` */` |
|        8 |  9200 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9201 |  |
|        9 |  9202 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9203 | `	int iOld,iNew,iValue;` |
|        9 |  9204 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|        - |  9205 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  9206 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9207 | `		return PH7_OK;` |
|        - |  9208 | `	}` |
|        - |  9209 | `	/* Save old assertion flags */` |
|        9 |  9210 | `	iOld = pVm->iAssertFlags;` |
|        - |  9211 | `	/* Extract the new flags */` |
|        9 |  9212 | `	iNew = ph7_value_to_int(apArg[0]);` |
|        9 |  9213 | `	if( iNew == PH7_ASSERT_DISABLE ){` |
|        7 |  9214 | `		pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        7 |  9215 | `		if( nArg > 1 ){` |
|        5 |  9216 | `			iValue = !ph7_value_to_bool(apArg[1]);` |
|        5 |  9217 | `			if( iValue ){` |
|        - |  9218 | `				/* Disable assertion */` |
|        3 |  9219 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        1 |  9220 | `			}` |
|        3 |  9221 | `		}` |
|        6 |  9222 | `	}else if( iNew == PH7_ASSERT_WARNING ){` |
|      ! 0 |  9223 | `		pVm->iAssertFlags &= ~PH7_ASSERT_WARNING;` |
|      ! 0 |  9224 | `		if( nArg > 1 ){` |
|      ! 0 |  9225 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  9226 | `			if( iValue ){` |
|        - |  9227 | `				/* Issue a warning for each failed assertion */` |
|      ! 0 |  9228 | `				pVm->iAssertFlags \|= PH7_ASSERT_WARNING;` |
|      ! 0 |  9229 | `			}` |
|      ! 0 |  9230 | `		}` |
|        3 |  9231 | `	}else if( iNew == PH7_ASSERT_BAIL ){` |
|        3 |  9232 | `		pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        3 |  9233 | `		if( nArg > 1 ){` |
|        3 |  9234 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|        3 |  9235 | `			if( iValue ){` |
|        - |  9236 | `				/* Terminate execution on failed assertions */` |
|        3 |  9237 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        1 |  9238 | `			}` |
|        2 |  9239 | `		}` |
|        1 |  9240 | `	}else if( iNew == PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  9241 | `		pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9242 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|        - |  9243 | `			/* Callback to call on failed assertions */` |
|      ! 0 |  9244 | `			PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  9245 | `			pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9246 | `		}` |
|      ! 0 |  9247 | `	}` |
|        - |  9248 | `	/* Return the old flags */` |
|        9 |  9249 | `	ph7_result_int(pCtx,iOld);` |
|        9 |  9250 | `	return PH7_OK;` |
|        5 |  9251 |  |
|        - |  9252 | `/*` |
|        - |  9253 | ` * bool assert(mixed $assertion)` |
|        - |  9254 | ` *  Checks if assertion is FALSE.` |
|        - |  9255 | ` * Parameter` |
|        - |  9256 | ` *  $assertion` |
|        - |  9257 | ` *    The assertion to test.` |
|        - |  9258 | ` * Return` |
|        - |  9259 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  9260 | ` */` |
|       14 |  9261 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9262 |  |
|       15 |  9263 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9264 | `	ph7_value *pAssert;` |
|        - |  9265 | `	int iFlags,iResult;` |
|       15 |  9266 | `	if( nArg < 1 ){` |
|        - |  9267 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  9268 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9269 | `		return PH7_OK;` |
|        - |  9270 | `	}` |
|       15 |  9271 | `	iFlags = pVm->iAssertFlags;` |
|       15 |  9272 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  9273 | `		/* Assertion is disabled,return FALSE */` |
|      ! 0 |  9274 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9275 | `		return PH7_OK;` |
|        - |  9276 | `	}` |
|       15 |  9277 | `	pAssert = apArg[0];` |
|       15 |  9278 | `	iResult = 1; /* cc warning */` |
|       15 |  9279 | `	if( pAssert->iFlags & MEMOBJ_STRING ){` |
|        - |  9280 | `		SyString sChunk;` |
|        7 |  9281 | `		SyStringInitFromBuf(&sChunk,SyBlobData(&pAssert->sBlob),SyBlobLength(&pAssert->sBlob));` |
|        7 |  9282 | `		if( sChunk.nByte > 0 ){` |
|        5 |  9283 | `			VmEvalChunk(pVm,pCtx,&sChunk,PH7_PHP_ONLY\|PH7_PHP_EXPR,FALSE);` |
|        - |  9284 | `			/* Extract evaluation result */` |
|        5 |  9285 | `			iResult = ph7_value_to_bool(pCtx->pRet);` |
|        3 |  9286 | `		}else{` |
|        3 |  9287 | `			iResult = 0;` |
|        - |  9288 | `		}` |
|        4 |  9289 | `	}else{` |
|        - |  9290 | `		/* Perform a boolean cast */` |
|        9 |  9291 | `		iResult = ph7_value_to_bool(apArg[0]);` |
|        - |  9292 | `	}` |
|       15 |  9293 | `	if( !iResult ){` |
|        - |  9294 | `		/* Assertion failed */` |
|        9 |  9295 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  9296 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  9297 | `			ph7_value sFile,sLine;` |
|        - |  9298 | `			ph7_value *apCbArg[3];` |
|        - |  9299 | `			SyString *pFile;` |
|        - |  9300 | `			/* Extract the processed script */` |
|      ! 0 |  9301 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  9302 | `			if( pFile == 0 ){` |
|      ! 0 |  9303 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  9304 | `			}` |
|        - |  9305 | `			/* Invoke the callback */` |
|      ! 0 |  9306 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  9307 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  9308 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  9309 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  9310 | `			apCbArg[2] = pAssert;` |
|      ! 0 |  9311 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  9312 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  9313 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  9314 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  9315 | `		}` |
|        9 |  9316 | `		if( iFlags & PH7_ASSERT_WARNING ){` |
|        - |  9317 | `			/* Emit a warning */` |
|        9 |  9318 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Assertion failed");` |
|        4 |  9319 | `		}` |
|        9 |  9320 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  9321 | `			/* Abort VM execution immediately */` |
|        3 |  9322 | `			return PH7_ABORT;` |
|        - |  9323 | `		}` |
|        3 |  9324 | `	}` |
|        - |  9325 | `	/* Assertion result */` |
|       13 |  9326 | `	ph7_result_bool(pCtx,iResult);` |
|       13 |  9327 | `	return PH7_OK;` |
|        8 |  9328 |  |
|        - |  9329 | `/*` |
|        - |  9330 | ` * Section:` |
|        - |  9331 | ` *  Error reporting functions.` |
|        - |  9332 | ` * Status:` |
|        - |  9333 | ` *    Stable.` |
|        - |  9334 | ` */` |
|        - |  9335 | `/*` |
|        - |  9336 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  9337 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  9338 | ` * Parameters` |
|        - |  9339 | ` *  $error_msg` |
|        - |  9340 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  9341 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  9342 | ` * $error_type` |
|        - |  9343 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  9344 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  9345 | ` * Return` |
|        - |  9346 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  9347 | ` */` |
|       12 |  9348 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9349 |  |
|       14 |  9350 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  9351 | `	int rc = PH7_OK;` |
|       14 |  9352 | `	if( nArg > 0 ){` |
|        - |  9353 | `		const char *zErr;` |
|        - |  9354 | `		int nLen;` |
|        - |  9355 | `		/* Extract the error message */` |
|       12 |  9356 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  9357 | `		if( nArg > 1 ){` |
|        - |  9358 | `			/* Extract the error type */` |
|       12 |  9359 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  9360 | `			switch( nErr ){` |
|        1 |  9361 | `			case 1:   /* E_ERROR */` |
|        - |  9362 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  9363 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  9364 | `			case 256: /* E_USER_ERROR */` |
|        3 |  9365 | `				nErr = PH7_CTX_ERR;` |
|        3 |  9366 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  9367 | `				break;` |
|        1 |  9368 | `			case 2:   /* E_WARNING */` |
|        - |  9369 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  9370 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  9371 | `			case 512: /* E_USER_WARNING */` |
|        3 |  9372 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  9373 | `				break;` |
|        3 |  9374 | `			default:` |
|        8 |  9375 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  9376 | `				break;` |
|        - |  9377 | `			}` |
|        5 |  9378 | `		}` |
|        - |  9379 | `		/* Report error */` |
|       12 |  9380 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  9381 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  9382 | `			return rc;` |
|        - |  9383 | `		}` |
|        - |  9384 | `		/* Return true */` |
|       12 |  9385 | `		ph7_result_bool(pCtx,1);` |
|        7 |  9386 | `	}else{` |
|        - |  9387 | `		/* Missing arguments,return FALSE */` |
|        3 |  9388 | `		ph7_result_bool(pCtx,0);` |
|        - |  9389 | `	}` |
|       14 |  9390 | `	return rc;` |
|        8 |  9391 |  |
|        - |  9392 | `/*` |
|        - |  9393 | ` * int error_reporting([int $level])` |
|        - |  9394 | ` *  Sets which PHP errors are reported.` |
|        - |  9395 | ` * Parameters` |
|        - |  9396 | ` *  $level` |
|        - |  9397 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  9398 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  9399 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  9400 | ` *   levels will not always behave as expected.` |
|        - |  9401 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  9402 | ` *   in the predefined constants.` |
|        - |  9403 | ` * Return` |
|        - |  9404 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  9405 | ` *   parameter is given.` |
|        - |  9406 | ` */` |
|       18 |  9407 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9408 |  |
|       19 |  9409 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9410 | `	int nOld;` |
|        - |  9411 | `	/* Extract the old reporting level */` |
|       19 |  9412 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       19 |  9413 | `	if( nArg > 0 ){` |
|        - |  9414 | `		int nNew;` |
|        - |  9415 | `		/* Extract the desired error reporting level */` |
|       11 |  9416 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       11 |  9417 | `		if( !nNew ){` |
|        - |  9418 | `			/* Do not report errors at all */` |
|        5 |  9419 | `			pVm->bErrReport = 0;` |
|        3 |  9420 | `		}else{` |
|        - |  9421 | `			/* Report all errors */` |
|        7 |  9422 | `			pVm->bErrReport = 1;` |
|        - |  9423 | `		}` |
|        5 |  9424 | `	}` |
|        - |  9425 | `	/* Return the old level */` |
|       19 |  9426 | `	ph7_result_int(pCtx,nOld);` |
|       19 |  9427 | `	return PH7_OK;` |
|        1 |  9428 |  |
|        - |  9429 | `/*` |
|        - |  9430 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  9431 | ` *  Send an error message somewhere.` |
|        - |  9432 | ` * Parameter` |
|        - |  9433 | ` *  $message` |
|        - |  9434 | ` *   The error message that should be logged.` |
|        - |  9435 | ` *  $message_type` |
|        - |  9436 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  9437 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  9438 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  9439 | ` *       This is the default option.` |
|        - |  9440 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  9441 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  9442 | ` *    2  No longer an option.` |
|        - |  9443 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  9444 | ` *       to the end of the message string.` |
|        - |  9445 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  9446 | ` *  $destination` |
|        - |  9447 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  9448 | ` *  $extra_headers` |
|        - |  9449 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  9450 | ` * Return` |
|        - |  9451 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9452 | ` * NOTE:` |
|        - |  9453 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  9454 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  9455 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  9456 | ` *  Otherwise this function is no-op.` |
|        - |  9457 | ` */` |
|        4 |  9458 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9459 |  |
|        - |  9460 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  9461 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  9462 | `	int iType = 0;` |
|        5 |  9463 | `	if( nArg < 1 ){` |
|        - |  9464 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  9465 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9466 | `		return PH7_OK;` |
|        - |  9467 | `	}` |
|        5 |  9468 | `	if( pVm->xErrLog  ){` |
|        - |  9469 | `		/* Invoke the user callback */` |
|      ! 0 |  9470 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  9471 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  9472 | `		if( nArg > 1 ){` |
|      ! 0 |  9473 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  9474 | `			if( nArg > 2 ){` |
|      ! 0 |  9475 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  9476 | `				if( nArg > 3 ){` |
|      ! 0 |  9477 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  9478 | `				}` |
|      ! 0 |  9479 | `			}` |
|      ! 0 |  9480 | `		}` |
|      ! 0 |  9481 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  9482 | `	}` |
|        - |  9483 | `	/* Retun TRUE */` |
|        5 |  9484 | `	ph7_result_bool(pCtx,1);` |
|        5 |  9485 | `	return PH7_OK;` |
|        3 |  9486 |  |
|        - |  9487 | `/*` |
|        - |  9488 | ` * bool restore_exception_handler(void)` |
|        - |  9489 | ` *  Restores the previously defined exception handler function.` |
|        - |  9490 | ` * Parameter` |
|        - |  9491 | ` *  None` |
|        - |  9492 | ` * Return` |
|        - |  9493 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  9494 | ` */` |
|        4 |  9495 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9496 |  |
|        5 |  9497 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9498 | `	ph7_value *pOld,*pNew;` |
|        - |  9499 | `	/* Point to the old and the new handler */` |
|        5 |  9500 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  9501 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  9502 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9503 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9504 | `		SXUNUSED(apArg);` |
|        - |  9505 | `		/* No installed handler,return FALSE */` |
|        5 |  9506 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9507 | `		return PH7_OK;` |
|        - |  9508 | `	}` |
|        - |  9509 | `	/* Copy the old handler */` |
|      ! 0 |  9510 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9511 | `	PH7_MemObjRelease(pOld);` |
|        - |  9512 | `	/* Return TRUE */` |
|      ! 0 |  9513 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9514 | `	return PH7_OK;` |
|        3 |  9515 |  |
|        - |  9516 | `/*` |
|        - |  9517 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  9518 | ` *  Sets a user-defined exception handler function.` |
|        - |  9519 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  9520 | ` * NOTE` |
|        - |  9521 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  9522 | ` *  the satndard PHP engine.` |
|        - |  9523 | ` * Parameters` |
|        - |  9524 | ` *  $exception_handler` |
|        - |  9525 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  9526 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  9527 | ` *   that was thrown.` |
|        - |  9528 | ` *  Note:` |
|        - |  9529 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9530 | ` * Return` |
|        - |  9531 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  9532 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9533 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9534 | ` */` |
|        4 |  9535 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9536 |  |
|        6 |  9537 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9538 | `	ph7_value *pOld,*pNew;` |
|        - |  9539 | `	/* Point to the old and the new handler */` |
|        6 |  9540 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  9541 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  9542 | `	/* Return the old handler */` |
|        6 |  9543 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  9544 | `	if( nArg > 0 ){` |
|        6 |  9545 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9546 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  9547 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  9548 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  9549 | `		}else{` |
|        6 |  9550 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9551 | `			/* Install the new handler */` |
|        6 |  9552 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9553 | `		}` |
|        2 |  9554 | `	}` |
|        6 |  9555 | `	return PH7_OK;` |
|        2 |  9556 |  |
|        - |  9557 | `/*` |
|        - |  9558 | ` * bool restore_error_handler(void)` |
|        - |  9559 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9560 | ` * Parameters:` |
|        - |  9561 | ` *  None.` |
|        - |  9562 | ` * Return` |
|        - |  9563 | ` *  Always TRUE.` |
|        - |  9564 | ` */` |
|        4 |  9565 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9566 |  |
|        5 |  9567 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9568 | `	ph7_value *pOld,*pNew;` |
|        - |  9569 | `	/* Point to the old and the new handler */` |
|        5 |  9570 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  9571 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  9572 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9573 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9574 | `		SXUNUSED(apArg);` |
|        - |  9575 | `		/* No installed callback,return FALSE */` |
|        5 |  9576 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9577 | `		return PH7_OK;` |
|        - |  9578 | `	}` |
|        - |  9579 | `	/* Copy the old callback */` |
|      ! 0 |  9580 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9581 | `	PH7_MemObjRelease(pOld);` |
|        - |  9582 | `	/* Return TRUE */` |
|      ! 0 |  9583 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9584 | `	return PH7_OK;` |
|        3 |  9585 |  |
|        - |  9586 | `/*` |
|        - |  9587 | ` * value set_error_handler(callable $error_handler)` |
|        - |  9588 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9589 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9590 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9591 | ` *  Sets a user-defined error handler function.` |
|        - |  9592 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  9593 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  9594 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  9595 | ` *  conditions (using trigger_error()).` |
|        - |  9596 | ` * Parameters` |
|        - |  9597 | ` *  $error_handler` |
|        - |  9598 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  9599 | ` *   describing the error.` |
|        - |  9600 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  9601 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  9602 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  9603 | ` *   The function can be shown as:` |
|        - |  9604 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  9605 | ` *     errno` |
|        - |  9606 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  9607 | ` *   errstr` |
|        - |  9608 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  9609 | ` *   errfile` |
|        - |  9610 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  9611 | ` *     was raised in, as a string.` |
|        - |  9612 | ` *  Note:` |
|        - |  9613 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9614 | ` * Return` |
|        - |  9615 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  9616 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9617 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9618 | ` */` |
|     8390 |  9619 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9620 |  |
|     8392 |  9621 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9622 | `	ph7_value *pOld,*pNew;` |
|        - |  9623 | `	/* Point to the old and the new handler */` |
|     8392 |  9624 | `	pOld = &pVm->aErrCB[0];` |
|     8392 |  9625 | `	pNew = &pVm->aErrCB[1];` |
|        - |  9626 | `	/* Return the old handler */` |
|     8392 |  9627 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8392 |  9628 | `	if( nArg > 0 ){` |
|     8392 |  9629 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9630 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4195 |  9631 | `			PH7_MemObjRelease(pNew);` |
|     4195 |  9632 | `			ph7_result_bool(pCtx,1);` |
|     2098 |  9633 | `		}else{` |
|     4198 |  9634 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9635 | `			/* Install the new handler */` |
|     4198 |  9636 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9637 | `		}` |
|     4195 |  9638 | `	}` |
|     8392 |  9639 | `	return PH7_OK;` |
|        2 |  9640 |  |
|        - |  9641 | `/*` |
|        - |  9642 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  9643 | ` *  Generates a backtrace.` |
|        - |  9644 | ` * Paramaeter` |
|        - |  9645 | ` *  $options` |
|        - |  9646 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  9647 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  9648 | ` *   all the function/method arguments, to save memory.` |
|        - |  9649 | ` * $limit` |
|        - |  9650 | ` *   (Not Used)` |
|        - |  9651 | ` * Return` |
|        - |  9652 | ` *  An array.The possible returned elements are as follows:` |
|        - |  9653 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  9654 | ` *          Name        Type      Description` |
|        - |  9655 | ` *          ------      ------     -----------` |
|        - |  9656 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  9657 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  9658 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  9659 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  9660 | ` *          object      object    The current object.` |
|        - |  9661 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  9662 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  9663 | ` */` |
|      276 |  9664 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9665 |  |
|      278 |  9666 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9667 | `	ph7_value *pArray;` |
|        - |  9668 | `	ph7_class *pClass;` |
|        - |  9669 | `	ph7_value *pValue;` |
|        - |  9670 | `	SyString *pFile;` |
|        - |  9671 | `	/* Create a new array */` |
|      278 |  9672 | `	pArray = ph7_context_new_array(pCtx);` |
|      278 |  9673 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      278 |  9674 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9675 | `		/* Out of memory,return NULL */` |
|      ! 0 |  9676 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  9677 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9678 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9679 | `		SXUNUSED(apArg);` |
|      ! 0 |  9680 | `		return PH7_OK;` |
|        - |  9681 | `	}` |
|        - |  9682 | `	/* Dump running function name and it's arguments  */` |
|      278 |  9683 | `	if( pVm->pFrame->pParent ){` |
|      278 |  9684 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  9685 | `		ph7_vm_func *pFunc;` |
|        - |  9686 | `		ph7_value *pArg;` |
|      278 |  9687 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9688 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  9689 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  9690 | `		}` |
|      278 |  9691 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      278 |  9692 | `		if( pFrame->pParent && pFunc ){` |
|      278 |  9693 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      278 |  9694 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      278 |  9695 | `			ph7_value_reset_string_cursor(pValue);` |
|      138 |  9696 | `		}` |
|        - |  9697 | `		/* Function arguments */` |
|      278 |  9698 | `		pArg = ph7_context_new_array(pCtx);` |
|      278 |  9699 | `		if( pArg  ){` |
|        - |  9700 | `			ph7_value *pObj;` |
|        - |  9701 | `			VmSlot *aSlot;` |
|        - |  9702 | `			sxu32 n;` |
|        - |  9703 | `			/* Start filling the array with the given arguments */` |
|      278 |  9704 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     1098 |  9705 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      822 |  9706 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      822 |  9707 | `				if( pObj ){` |
|      822 |  9708 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      410 |  9709 | `				}` |
|      412 |  9710 | `			}` |
|        - |  9711 | `			/* Save the array */` |
|      278 |  9712 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      138 |  9713 | `		}` |
|      138 |  9714 | `	}` |
|      278 |  9715 | `	ph7_value_int(pValue,1);` |
|        - |  9716 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  9717 | `	 * line numbers at run-time. )` |
|        - |  9718 | `	 */` |
|      278 |  9719 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  9720 | `	/* Current processed script */` |
|      278 |  9721 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      278 |  9722 | `	if( pFile ){` |
|      278 |  9723 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      278 |  9724 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      278 |  9725 | `		ph7_value_reset_string_cursor(pValue);` |
|      138 |  9726 | `	}` |
|        - |  9727 | `	/* Top class */` |
|      278 |  9728 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      278 |  9729 | `	if( pClass ){` |
|      274 |  9730 | `		ph7_value_reset_string_cursor(pValue);` |
|      274 |  9731 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      274 |  9732 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      136 |  9733 | `	}` |
|        - |  9734 | `	/* Return the freshly created array */` |
|      278 |  9735 | `	ph7_result_value(pCtx,pArray);` |
|        - |  9736 | `	/*` |
|        - |  9737 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  9738 | `	 * as soon we return from this function.` |
|        - |  9739 | `	 */` |
|      278 |  9740 | `	return PH7_OK;` |
|      140 |  9741 |  |
|        - |  9742 | `/*` |
|        - |  9743 | ` * Generate a small backtrace.` |
|        - |  9744 | ` * Store the generated dump in the given BLOB` |
|        - |  9745 | ` */` |
|        4 |  9746 | `static int VmMiniBacktrace(` |
|        - |  9747 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  9748 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  9749 | `	)` |
|        1 |  9750 |  |
|        5 |  9751 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  9752 | `	ph7_vm_func *pFunc;` |
|        - |  9753 | `	ph7_class *pClass;` |
|        - |  9754 | `	SyString *pFile;` |
|        - |  9755 | `	/* Called function */` |
|        5 |  9756 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9757 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  9758 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  9759 | `	}` |
|        5 |  9760 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  9761 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  9762 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  9763 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  9764 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  9765 | `	}else{` |
|      ! 0 |  9766 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  9767 | `	}` |
|        5 |  9768 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  9769 | `	/* Current processed script */` |
|        5 |  9770 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  9771 | `	if( pFile ){` |
|        5 |  9772 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  9773 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  9774 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  9775 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  9776 | `	}` |
|        - |  9777 | `	/* Top class */` |
|        5 |  9778 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  9779 | `	if( pClass ){` |
|      ! 0 |  9780 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  9781 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  9782 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  9783 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  9784 | `	}` |
|        5 |  9785 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  9786 | `	/* All done */` |
|        5 |  9787 | `	return SXRET_OK;` |
|        1 |  9788 |  |
|        - |  9789 | `/*` |
|        - |  9790 | ` * void debug_print_backtrace()` |
|        - |  9791 | ` *  Prints a backtrace` |
|        - |  9792 | ` * Parameters` |
|        - |  9793 | ` * None` |
|        - |  9794 | ` * Return` |
|        - |  9795 | ` * NULL` |
|        - |  9796 | ` */` |
|        2 |  9797 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9798 |  |
|        3 |  9799 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9800 | `	SyBlob sDump;` |
|        3 |  9801 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  9802 | `	/* Generate the backtrace */` |
|        3 |  9803 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  9804 | `	/* Output backtrace */` |
|        3 |  9805 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9806 | `	/* All done,cleanup */` |
|        3 |  9807 | `	SyBlobRelease(&sDump);` |
|        1 |  9808 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9809 | `	SXUNUSED(apArg);` |
|        3 |  9810 | `	return PH7_OK;` |
|        1 |  9811 |  |
|        - |  9812 | `/*` |
|        - |  9813 | ` * string debug_string_backtrace()` |
|        - |  9814 | ` *  Generate a backtrace` |
|        - |  9815 | ` * Parameters` |
|        - |  9816 | ` * None` |
|        - |  9817 | ` * Return` |
|        - |  9818 | ` *  A mini backtrace().` |
|        - |  9819 | ` * Note that this is a symisc extension.` |
|        - |  9820 | ` */` |
|        2 |  9821 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9822 |  |
|        3 |  9823 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9824 | `	SyBlob sDump;` |
|        3 |  9825 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  9826 | `	/* Generate the backtrace */` |
|        3 |  9827 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  9828 | `	/* Return the backtrace */` |
|        3 |  9829 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  9830 | `	/* All done,cleanup */` |
|        3 |  9831 | `	SyBlobRelease(&sDump);` |
|        1 |  9832 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9833 | `	SXUNUSED(apArg);` |
|        3 |  9834 | `	return PH7_OK;` |
|        1 |  9835 |  |
|        - |  9836 | `/*` |
|        - |  9837 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  9838 | ` * exception is triggered.` |
|        - |  9839 | ` */` |
|      260 |  9840 | `static sxi32 VmUncaughtException(` |
|        - |  9841 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  9842 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  9843 | `	)` |
|        1 |  9844 |  |
|        - |  9845 | `	ph7_value *apArg[2],sArg;` |
|      261 |  9846 | `	int nArg = 1;` |
|        - |  9847 | `	sxi32 rc;` |
|      261 |  9848 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  9849 | `		/* Nesting limit reached */` |
|      ! 0 |  9850 | `		return SXRET_OK;` |
|        - |  9851 | `	}` |
|        - |  9852 | `	/* Call any exception handler if available */` |
|      261 |  9853 | `	PH7_MemObjInit(pVm,&sArg);` |
|      261 |  9854 | `	if( pThis ){` |
|        - |  9855 | `		/* Load the exception instance */` |
|      261 |  9856 | `		sArg.x.pOther = pThis;` |
|      261 |  9857 | `		pThis->iRef++;` |
|      261 |  9858 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      131 |  9859 | `	}else{` |
|      ! 0 |  9860 | `		nArg = 0;` |
|        - |  9861 | `	}` |
|      261 |  9862 | `	apArg[0] = &sArg;` |
|        - |  9863 | `	/* Call the exception handler if available */` |
|      261 |  9864 | `	pVm->nExceptDepth++;` |
|      261 |  9865 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      261 |  9866 | `	pVm->nExceptDepth--;` |
|      261 |  9867 | `	if( rc != SXRET_OK ){` |
|        - |  9868 | `		SyBlob sMsgBuf;` |
|      259 |  9869 | `		const char *zClass = "Exception";` |
|      259 |  9870 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  9871 | `		const char *zMsg;` |
|        - |  9872 | `		sxu32 nMsg;` |
|        - |  9873 | `		const char *zFuncName;` |
|        - |  9874 | `		int nFuncLen;` |
|      259 |  9875 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      259 |  9876 | `		if( pThis ){` |
|        - |  9877 | `			ph7_class_method *pGetMessage;` |
|        - |  9878 | `			ph7_value sMsg;` |
|        - |  9879 | `			const char *zTmp;` |
|        - |  9880 | `			int nTmp;` |
|      259 |  9881 | `			zClass = pThis->pClass->sName.zString;` |
|      259 |  9882 | `			nClass = pThis->pClass->sName.nByte;` |
|      259 |  9883 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      259 |  9884 | `			if( pGetMessage ){` |
|      259 |  9885 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      259 |  9886 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      259 |  9887 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      259 |  9888 | `					if( zTmp && nTmp > 0 ){` |
|      259 |  9889 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      129 |  9890 | `					}` |
|      129 |  9891 | `				}` |
|      259 |  9892 | `				PH7_MemObjRelease(&sMsg);` |
|      129 |  9893 | `			}` |
|      129 |  9894 | `		}` |
|      259 |  9895 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  9896 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  9897 | `		}` |
|      259 |  9898 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      259 |  9899 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      259 |  9900 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      259 |  9901 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      259 |  9902 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  9903 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      259 |  9904 | `		rc = SXERR_ABORT;` |
|      129 |  9905 | `	}` |
|      261 |  9906 | `	PH7_MemObjRelease(&sArg);` |
|      261 |  9907 | `	return rc;` |
|      131 |  9908 |  |
|        - |  9909 | `/*` |
|        - |  9910 | ` * Throw an user exception.` |
|        - |  9911 | ` */` |
|      274 |  9912 | `static sxi32 VmThrowException(` |
|        - |  9913 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  9914 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  9915 | `	)` |
|        2 |  9916 |  |
|        - |  9917 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  9918 | `	ph7_exception **apException;` |
|        - |  9919 | `	ph7_exception *pException;` |
|        - |  9920 | `	/* Point to the stack of loaded exceptions */` |
|      276 |  9921 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      276 |  9922 | `	pException = 0;` |
|      276 |  9923 | `	pCatch = 0;` |
|      276 |  9924 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  9925 | `		ph7_exception_block *aCatch;` |
|        - |  9926 | `		ph7_class *pClass;` |
|        - |  9927 | `		sxu32 j;` |
|        - |  9928 | `		/* Locate the appropriate block to execute */` |
|       16 |  9929 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       16 |  9930 | `		(void)SySetPop(&pVm->aException);` |
|       16 |  9931 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       16 |  9932 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       16 |  9933 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  9934 | `			/* Extract the target class */` |
|       16 |  9935 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       16 |  9936 | `			if( pClass == 0 ){` |
|        - |  9937 | `				/* No such class */` |
|      ! 0 |  9938 | `				continue;` |
|        - |  9939 | `			}` |
|       16 |  9940 | `			if( VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  9941 | `				/* Catch block found,break immeditaley */` |
|       16 |  9942 | `				pCatch = &aCatch[j];` |
|       16 |  9943 | `				break;` |
|        - |  9944 | `			}` |
|      ! 0 |  9945 | `		}` |
|        7 |  9946 | `	}` |
|        - |  9947 | `	/* Execute the cached block if available */` |
|      276 |  9948 | `	if( pCatch == 0 ){` |
|        - |  9949 | `		sxi32 rc;` |
|      261 |  9950 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      261 |  9951 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  9952 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  9953 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9954 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  9955 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  9956 | `			}` |
|      ! 0 |  9957 | `			if( pException->pFrame == pFrame ){` |
|        - |  9958 | `				/* Tell the upper layer that the exception was caught */` |
|      ! 0 |  9959 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  9960 | `			}` |
|      ! 0 |  9961 | `		}` |
|      261 |  9962 | `		return rc;` |
|      ! 0 |  9963 | `	}else{` |
|       16 |  9964 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  9965 | `		sxi32 rc;` |
|       24 |  9966 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9967 | `			/* Safely ignore the exception frame */` |
|       10 |  9968 | `			pFrame = pFrame->pParent;` |
|        2 |  9969 | `		}` |
|       16 |  9970 | `		if( pException->pFrame == pFrame ){` |
|        - |  9971 | `			/* Tell the upper layer that the exception was caught */` |
|        8 |  9972 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        3 |  9973 | `		}` |
|        - |  9974 | `		/* Create a private frame first */` |
|       16 |  9975 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       16 |  9976 | `		if( rc == SXRET_OK ){` |
|        - |  9977 | `			/* Mark as catch frame */` |
|       16 |  9978 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       16 |  9979 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       16 |  9980 | `			if( pObj ){` |
|        - |  9981 | `				/* Install the exception instance */` |
|       16 |  9982 | `				pThis->iRef++; /* Increment reference count */` |
|       16 |  9983 | `				pObj->x.pOther = pThis;` |
|       16 |  9984 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        7 |  9985 | `			}` |
|        - |  9986 | `			/* Exceute the block */` |
|       16 |  9987 | `			VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  9988 | `			/* Leave the frame */` |
|       16 |  9989 | `			VmLeaveFrame(&(*pVm));` |
|        7 |  9990 | `		}` |
|        - |  9991 | `	}` |
|        - |  9992 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  9993 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  9994 | `	 */` |
|       16 |  9995 | `	return SXRET_OK;` |
|      139 |  9996 |  |
|        - |  9997 | `/*` |
|        - |  9998 | ` * Section:` |
|        - |  9999 | ` *  Version,Credits and Copyright related functions.` |
|        - | 10000 | ` * Status:` |
|        - | 10001 | ` *    Stable.` |
|        - | 10002 | ` */` |
|        - | 10003 | `/*` |
|        - | 10004 | ` * string ph7version(void)` |
|        - | 10005 | ` *  Returns the running version of the PH7 version.` |
|        - | 10006 | ` * Parameters` |
|        - | 10007 | ` *  None` |
|        - | 10008 | ` * Return` |
|        - | 10009 | ` * Current PH7 version.` |
|        - | 10010 | ` */` |
|        2 | 10011 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10012 |  |
|        1 | 10013 | `	SXUNUSED(nArg);` |
|        1 | 10014 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 10015 | `	/* Current engine version */` |
|        3 | 10016 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 10017 | `	return PH7_OK;` |
|        1 | 10018 |  |
|        - | 10019 | `/*` |
|        - | 10020 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 10021 | ` */` |
|        - | 10022 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 10023 | ` "<html><head>"\` |
|        - | 10024 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 10025 | ` "<style type=\"text/css\">"\` |
|        - | 10026 | ` "div {"\` |
|        - | 10027 | `     "border: 1px solid #cccccc;"\` |
|        - | 10028 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 10029 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 10030 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 10031 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 10032 | `     "-webkit-border-radius: 10px;"\` |
|        - | 10033 | `     "-o-border-radius: 10px;"\` |
|        - | 10034 | `     "border-radius: 10px;"\` |
|        - | 10035 | `     "padding-left: 2em;"\` |
|        - | 10036 | `     "background-color: white;"\` |
|        - | 10037 | `     "margin-left: auto;"\` |
|        - | 10038 | `     "font-family: verdana;"\` |
|        - | 10039 | `     "padding-right: 2em;"\` |
|        - | 10040 | `     "margin-right: auto;"\` |
|        - | 10041 | `     "}"\` |
|        - | 10042 | `     "body {"\` |
|        - | 10043 | `     "padding: 0.2em;"\` |
|        - | 10044 | `     "font-style: normal;"\` |
|        - | 10045 | `     "font-size: medium;"\` |
|        - | 10046 | `     "background-color: #f2f2f2;"\` |
|        - | 10047 | `     "}"\` |
|        - | 10048 | `     "hr {"\` |
|        - | 10049 | `     "border-style: solid none none;"\` |
|        - | 10050 | `     "border-width: 1px medium medium;"\` |
|        - | 10051 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 10052 | `     "height: 1px;"\` |
|        - | 10053 | `     "}"\` |
|        - | 10054 | `     "a {"\` |
|        - | 10055 | `     "color: #3366cc;"\` |
|        - | 10056 | `     "text-decoration: none;"\` |
|        - | 10057 | `     "}"\` |
|        - | 10058 | `     "a:hover {"\` |
|        - | 10059 | `     "color: #999999;"\` |
|        - | 10060 | `     "}"\` |
|        - | 10061 | `     "a:active {"\` |
|        - | 10062 | `     "color: #663399;"\` |
|        - | 10063 | `     "}"\` |
|        - | 10064 | `     "h1 {"\` |
|        - | 10065 | `     "margin: 0;"\` |
|        - | 10066 | `     "padding: 0;"\` |
|        - | 10067 | `     "font-family: Verdana;"\` |
|        - | 10068 | `     "font-weight: bold;"\` |
|        - | 10069 | `     "font-style: normal;"\` |
|        - | 10070 | `     "font-size: medium;"\` |
|        - | 10071 | `     "text-transform: capitalize;"\` |
|        - | 10072 | `     "color: #0a328c;"\` |
|        - | 10073 | `     "}"\` |
|        - | 10074 | `     "p {"\` |
|        - | 10075 | `     "margin: 0 auto;"\` |
|        - | 10076 | `     "font-size: medium;"\` |
|        - | 10077 | `     "font-style: normal;"\` |
|        - | 10078 | `     "font-family: verdana;"\` |
|        - | 10079 | `     "}"\` |
|        - | 10080 | `"</style></head><body>"\` |
|        - | 10081 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 10082 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 10083 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 10084 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 10085 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 10086 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 10087 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 10088 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 10089 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 10090 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 10091 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 10092 |  |
|        - | 10093 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10094 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 10095 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 10096 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 10097 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10098 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 10099 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10100 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 10101 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10102 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 10103 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10104 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 10105 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 10106 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 10107 |  |
|        - | 10108 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 10109 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 10110 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 10111 | `"&nbsp;*<br>"\` |
|        - | 10112 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 10113 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 10114 | `"&nbsp;* are met:<br>"\` |
|        - | 10115 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 10116 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 10117 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 10118 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 10119 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 10120 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 10121 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 10122 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 10123 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 10124 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 10125 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 10126 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 10127 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 10128 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 10129 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 10130 | `"&nbsp;*<br>"\` |
|        - | 10131 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 10132 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 10133 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 10134 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 10135 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 10136 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 10137 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 10138 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 10139 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 10140 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 10141 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 10142 | `"&nbsp;*/<br>"\` |
|        - | 10143 | `"</span></small></small></p>"\` |
|        - | 10144 | `"</div></body></html>"` |
|        - | 10145 | `/*` |
|        - | 10146 | ` * bool ph7credits(void)` |
|        - | 10147 | ` * bool ph7info(void)` |
|        - | 10148 | ` * bool ph7copyright(void)` |
|        - | 10149 | ` *  Prints out the credits for PH7 engine` |
|        - | 10150 | ` * Parameters` |
|        - | 10151 | ` *  None` |
|        - | 10152 | ` * Return` |
|        - | 10153 | ` *  Always TRUE` |
|        - | 10154 | ` */` |
|        2 | 10155 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10156 |  |
|        3 | 10157 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 10158 | `	/* Expand the HTML page above*/` |
|        3 | 10159 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 10160 | `	ph7_context_output_format(` |
|        1 | 10161 | `		pCtx,` |
|        - | 10162 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 10163 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 10164 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 10165 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 10166 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 10167 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 10168 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 10169 | `#ifdef __WINNT__` |
|        - | 10170 | `		"Windows NT"` |
|        - | 10171 | `#elif defined(__UNIXES__)` |
|        - | 10172 | `		"UNIX-Like"` |
|        - | 10173 | `#else` |
|        - | 10174 | `		"Other OS"` |
|        - | 10175 | `#endif` |
|        - | 10176 | `		);` |
|        3 | 10177 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 10178 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10179 | `	SXUNUSED(apArg);` |
|        - | 10180 | `	/* Return TRUE */` |
|        - | 10181 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 10182 | `	return PH7_OK;` |
|        1 | 10183 |  |
|        - | 10184 | `/*` |
|        - | 10185 | ` * Section:` |
|        - | 10186 | ` *    URL related routines.` |
|        - | 10187 | ` * Status:` |
|        - | 10188 | ` *    Stable.` |
|        - | 10189 | ` */` |
|        - | 10190 | `/* Forward declaration */` |
|        - | 10191 | `static sxi32 VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen);` |
|        - | 10192 | `/*` |
|        - | 10193 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 10194 | ` *  Parse a URL and return its fields.` |
|        - | 10195 | ` * Parameters` |
|        - | 10196 | ` *  $url` |
|        - | 10197 | ` *   The URL to parse.` |
|        - | 10198 | ` * $component` |
|        - | 10199 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 10200 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 10201 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 10202 | ` *  in which case the return value will be an integer).` |
|        - | 10203 | ` * Return` |
|        - | 10204 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 10205 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 10206 | ` *  this array are:` |
|        - | 10207 | ` *   scheme - e.g. http` |
|        - | 10208 | ` *   host` |
|        - | 10209 | ` *   port` |
|        - | 10210 | ` *   user` |
|        - | 10211 | ` *   pass` |
|        - | 10212 | ` *   path` |
|        - | 10213 | ` *   query - after the question mark ?` |
|        - | 10214 | ` *   fragment - after the hashmark #` |
|        - | 10215 | ` * Note:` |
|        - | 10216 | ` *  FALSE is returned on failure.` |
|        - | 10217 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 10218 | ` *  with the standard PHP engine.` |
|        - | 10219 | ` */` |
|       28 | 10220 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10221 |  |
|        - | 10222 | `	const char *zStr; /* Input string */` |
|        - | 10223 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 10224 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 10225 | `	int nLen;` |
|        - | 10226 | `	sxi32 rc;` |
|       29 | 10227 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10228 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 10229 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10230 | `		return PH7_OK;` |
|        - | 10231 | `	}` |
|        - | 10232 | `	/* Extract the given URI */` |
|       29 | 10233 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 10234 | `	if( nLen < 1 ){` |
|        - | 10235 | `		/* Nothing to process,return FALSE */` |
|        3 | 10236 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10237 | `		return PH7_OK;` |
|        - | 10238 | `	}` |
|        - | 10239 | `	/* Get a parse */` |
|       27 | 10240 | `	rc = VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 10241 | `	if( rc != SXRET_OK ){` |
|        - | 10242 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 10243 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10244 | `		return PH7_OK;` |
|        - | 10245 | `	}` |
|       27 | 10246 | `	if( nArg > 1 ){` |
|      ! 0 | 10247 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 10248 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 10249 | `		switch(nComponent){` |
|      ! 0 | 10250 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 10251 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 10252 | `			if( pComp->nByte < 1 ){` |
|        - | 10253 | `				/* No available value,return NULL */` |
|      ! 0 | 10254 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10255 | `			}else{` |
|      ! 0 | 10256 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10257 | `			}` |
|      ! 0 | 10258 | `			break;` |
|      ! 0 | 10259 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 10260 | `			pComp = &sURI.sHost;` |
|      ! 0 | 10261 | `			if( pComp->nByte < 1 ){` |
|        - | 10262 | `				/* No available value,return NULL */` |
|      ! 0 | 10263 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10264 | `			}else{` |
|      ! 0 | 10265 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10266 | `			}` |
|      ! 0 | 10267 | `			break;` |
|      ! 0 | 10268 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 10269 | `			pComp = &sURI.sPort;` |
|      ! 0 | 10270 | `			if( pComp->nByte < 1 ){` |
|        - | 10271 | `				/* No available value,return NULL */` |
|      ! 0 | 10272 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10273 | `			}else{` |
|      ! 0 | 10274 | `				int iPort = 0;` |
|        - | 10275 | `				/* Cast the value to integer */` |
|      ! 0 | 10276 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 10277 | `				ph7_result_int(pCtx,iPort);` |
|        - | 10278 | `			}` |
|      ! 0 | 10279 | `			break;` |
|      ! 0 | 10280 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 10281 | `			pComp = &sURI.sUser;` |
|      ! 0 | 10282 | `			if( pComp->nByte < 1 ){` |
|        - | 10283 | `				/* No available value,return NULL */` |
|      ! 0 | 10284 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10285 | `			}else{` |
|      ! 0 | 10286 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10287 | `			}` |
|      ! 0 | 10288 | `			break;` |
|      ! 0 | 10289 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 10290 | `			pComp = &sURI.sPass;` |
|      ! 0 | 10291 | `			if( pComp->nByte < 1 ){` |
|        - | 10292 | `				/* No available value,return NULL */` |
|      ! 0 | 10293 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10294 | `			}else{` |
|      ! 0 | 10295 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10296 | `			}` |
|      ! 0 | 10297 | `			break;` |
|      ! 0 | 10298 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 10299 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 10300 | `			if( pComp->nByte < 1 ){` |
|        - | 10301 | `				/* No available value,return NULL */` |
|      ! 0 | 10302 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10303 | `			}else{` |
|      ! 0 | 10304 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10305 | `			}` |
|      ! 0 | 10306 | `			break;` |
|      ! 0 | 10307 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 10308 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 10309 | `			if( pComp->nByte < 1 ){` |
|        - | 10310 | `				/* No available value,return NULL */` |
|      ! 0 | 10311 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10312 | `			}else{` |
|      ! 0 | 10313 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10314 | `			}` |
|      ! 0 | 10315 | `			break;` |
|      ! 0 | 10316 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 10317 | `			pComp = &sURI.sPath;` |
|      ! 0 | 10318 | `			if( pComp->nByte < 1 ){` |
|        - | 10319 | `				/* No available value,return NULL */` |
|      ! 0 | 10320 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10321 | `			}else{` |
|      ! 0 | 10322 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10323 | `			}` |
|      ! 0 | 10324 | `			break;` |
|      ! 0 | 10325 | `		default:` |
|        - | 10326 | `			/* No such entry,return NULL */` |
|      ! 0 | 10327 | `			ph7_result_null(pCtx);` |
|      ! 0 | 10328 | `			break;` |
|        - | 10329 | `		}` |
|      ! 0 | 10330 | `	}else{` |
|        - | 10331 | `		ph7_value *pArray,*pValue;` |
|        - | 10332 | `		/* Return an associative array */` |
|       27 | 10333 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 10334 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 10335 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10336 | `			/* Out of memory */` |
|      ! 0 | 10337 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10338 | `			/* Return false */` |
|      ! 0 | 10339 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 10340 | `			return PH7_OK;` |
|        - | 10341 | `		}` |
|        - | 10342 | `		/* Fill the array */` |
|       27 | 10343 | `		pComp = &sURI.sScheme;` |
|       27 | 10344 | `		if( pComp->nByte > 0 ){` |
|       19 | 10345 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 10346 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 10347 | `		}` |
|        - | 10348 | `		/* Reset the string cursor */` |
|       27 | 10349 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10350 | `		pComp = &sURI.sHost;` |
|       27 | 10351 | `		if( pComp->nByte > 0 ){` |
|       25 | 10352 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 10353 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 10354 | `		}` |
|        - | 10355 | `		/* Reset the string cursor */` |
|       27 | 10356 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10357 | `		pComp = &sURI.sPort;` |
|       27 | 10358 | `		if( pComp->nByte > 0 ){` |
|       11 | 10359 | `			int iPort = 0;/* cc warning */` |
|        - | 10360 | `			/* Convert to integer */` |
|       11 | 10361 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 10362 | `			ph7_value_int(pValue,iPort);` |
|       11 | 10363 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 10364 | `		}` |
|        - | 10365 | `		/* Reset the string cursor */` |
|       27 | 10366 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10367 | `		pComp = &sURI.sUser;` |
|       27 | 10368 | `		if( pComp->nByte > 0 ){` |
|        7 | 10369 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10370 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 10371 | `		}` |
|        - | 10372 | `		/* Reset the string cursor */` |
|       27 | 10373 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10374 | `		pComp = &sURI.sPass;` |
|       27 | 10375 | `		if( pComp->nByte > 0 ){` |
|        7 | 10376 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10377 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 10378 | `		}` |
|        - | 10379 | `		/* Reset the string cursor */` |
|       27 | 10380 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10381 | `		pComp = &sURI.sPath;` |
|       27 | 10382 | `		if( pComp->nByte > 0 ){` |
|       17 | 10383 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 10384 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 10385 | `		}` |
|        - | 10386 | `		/* Reset the string cursor */` |
|       27 | 10387 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10388 | `		pComp = &sURI.sQuery;` |
|       27 | 10389 | `		if( pComp->nByte > 0 ){` |
|        5 | 10390 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10391 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 10392 | `		}` |
|        - | 10393 | `		/* Reset the string cursor */` |
|       27 | 10394 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10395 | `		pComp = &sURI.sFragment;` |
|       27 | 10396 | `		if( pComp->nByte > 0 ){` |
|        5 | 10397 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10398 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 10399 | `		}` |
|        - | 10400 | `		/* Return the created array */` |
|       27 | 10401 | `		ph7_result_value(pCtx,pArray);` |
|        - | 10402 | `		/* NOTE:` |
|        - | 10403 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 10404 | `		 * automatically as soon we return from this function.` |
|        - | 10405 | `		 */` |
|        - | 10406 | `	}` |
|        - | 10407 | `	/* All done */` |
|       27 | 10408 | `	return PH7_OK;` |
|       15 | 10409 |  |
|        - | 10410 | `/*` |
|        - | 10411 | ` * Section:` |
|        - | 10412 | ` *   Array related routines.` |
|        - | 10413 | ` * Status:` |
|        - | 10414 | ` *    Stable.` |
|        - | 10415 | ` * Note 2012-5-21 01:04:15:` |
|        - | 10416 | ` *  Array related functions that need access to the underlying` |
|        - | 10417 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 10418 | ` */` |
|        - | 10419 | `/*` |
|        - | 10420 | ` * The [compact()] function store it's state information in an instance` |
|        - | 10421 | ` * of the following structure.` |
|        - | 10422 | ` */` |
|        - | 10423 | `struct compact_data` |
|        - | 10424 |  |
|        - | 10425 | `	ph7_value *pArray;  /* Target array */` |
|        - | 10426 | `	int nRecCount;      /* Recursion count */` |
|        - | 10427 | `};` |
|        - | 10428 | `/*` |
|        - | 10429 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 10430 | ` */` |
|      ! 0 | 10431 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 10432 |  |
|      ! 0 | 10433 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 10434 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 10435 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 10436 | `	/* Act according to the hashmap value */` |
|      ! 0 | 10437 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 10438 | `		SyString sVar;` |
|      ! 0 | 10439 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 10440 | `		if( sVar.nByte > 0 ){` |
|        - | 10441 | `			/* Query the current frame */` |
|      ! 0 | 10442 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 10443 | `			/* ^` |
|        - | 10444 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 10445 | `			 */` |
|      ! 0 | 10446 | `			if( pKey ){` |
|        - | 10447 | `				/* Perform the insertion */` |
|      ! 0 | 10448 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 10449 | `			}` |
|      ! 0 | 10450 | `		}` |
|      ! 0 | 10451 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 10452 | `		int rc;` |
|        - | 10453 | `		/* Recursively traverse this array */` |
|      ! 0 | 10454 | `		pData->nRecCount++;` |
|      ! 0 | 10455 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 10456 | `		pData->nRecCount--;` |
|      ! 0 | 10457 | `		return rc;` |
|        - | 10458 | `	}` |
|      ! 0 | 10459 | `	return SXRET_OK;` |
|      ! 0 | 10460 |  |
|        - | 10461 | `/*` |
|        - | 10462 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 10463 | ` *  Create array containing variables and their values.` |
|        - | 10464 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 10465 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 10466 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 10467 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 10468 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 10469 | ` * Parameters` |
|        - | 10470 | ` *  $varname` |
|        - | 10471 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 10472 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 10473 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 10474 | ` *   it recursively.` |
|        - | 10475 | ` * Return` |
|        - | 10476 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 10477 | ` */` |
|        2 | 10478 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10479 |  |
|        - | 10480 | `	ph7_value *pArray,*pObj;` |
|        3 | 10481 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10482 | `	const char *zName;` |
|        - | 10483 | `	SyString sVar;` |
|        - | 10484 | `	int i,nLen;` |
|        3 | 10485 | `	if( nArg < 1 ){` |
|        - | 10486 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 10487 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10488 | `		return PH7_OK;` |
|        - | 10489 | `	}` |
|        - | 10490 | `	/* Create the array */` |
|        3 | 10491 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10492 | `	if( pArray == 0 ){` |
|        - | 10493 | `		/* Out of memory */` |
|      ! 0 | 10494 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10495 | `		/* Return NULL */` |
|      ! 0 | 10496 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10497 | `		return PH7_OK;` |
|        - | 10498 | `	}` |
|        - | 10499 | `	/* Perform the requested operation */` |
|        7 | 10500 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 10501 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 10502 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 10503 | `				struct compact_data sData;` |
|      ! 0 | 10504 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 10505 | `				/* Recursively walk the array */` |
|      ! 0 | 10506 | `				sData.nRecCount = 0;` |
|      ! 0 | 10507 | `				sData.pArray = pArray;` |
|      ! 0 | 10508 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 10509 | `			}` |
|      ! 0 | 10510 | `		}else{` |
|        - | 10511 | `			/* Extract variable name */` |
|        5 | 10512 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 10513 | `			if( nLen > 0 ){` |
|        5 | 10514 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 10515 | `				/* Check if the variable is available in the current frame */` |
|        5 | 10516 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 10517 | `				if( pObj ){` |
|        5 | 10518 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 10519 | `				}` |
|        2 | 10520 | `			}` |
|        - | 10521 | `		}` |
|        3 | 10522 | `	}` |
|        - | 10523 | `	/* Return the array */` |
|        3 | 10524 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10525 | `	return PH7_OK;` |
|        2 | 10526 |  |
|        - | 10527 | `/*` |
|        - | 10528 | ` * The [extract()] function store it's state information in an instance` |
|        - | 10529 | ` * of the following structure.` |
|        - | 10530 | ` */` |
|        - | 10531 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 10532 | `struct extract_aux_data` |
|        - | 10533 |  |
|        - | 10534 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 10535 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 10536 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 10537 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 10538 | `	int iFlags;           /* Control flags */` |
|        - | 10539 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 10540 | `};` |
|        - | 10541 | `/* Forward declaration */` |
|        - | 10542 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 10543 | `/*` |
|        - | 10544 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 10545 | ` *   Import variables into the current symbol table from an array.` |
|        - | 10546 | ` * Parameters` |
|        - | 10547 | ` * $var_array` |
|        - | 10548 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 10549 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 10550 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 10551 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 10552 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 10553 | ` * $extract_type` |
|        - | 10554 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 10555 | ` *  It can be one of the following values:` |
|        - | 10556 | ` *   EXTR_OVERWRITE` |
|        - | 10557 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 10558 | ` *   EXTR_SKIP` |
|        - | 10559 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 10560 | ` *   EXTR_PREFIX_SAME` |
|        - | 10561 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 10562 | ` *   EXTR_PREFIX_ALL` |
|        - | 10563 | ` *       Prefix all variable names with prefix.` |
|        - | 10564 | ` *   EXTR_PREFIX_INVALID` |
|        - | 10565 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 10566 | ` *   EXTR_IF_EXISTS` |
|        - | 10567 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 10568 | ` *       otherwise do nothing.` |
|        - | 10569 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 10570 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 10571 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 10572 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 10573 | ` *      the current symbol table.` |
|        - | 10574 | ` * $prefix` |
|        - | 10575 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 10576 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 10577 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 10578 | ` *  underscore character.` |
|        - | 10579 | ` * Return` |
|        - | 10580 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 10581 | ` */` |
|        4 | 10582 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10583 |  |
|        - | 10584 | `	extract_aux_data sAux;` |
|        - | 10585 | `	ph7_hashmap *pMap;` |
|        5 | 10586 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 10587 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 10588 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 10589 | `		return PH7_OK;` |
|        - | 10590 | `	}` |
|        - | 10591 | `	/* Point to the target hashmap */` |
|        5 | 10592 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 10593 | `	if( pMap->nEntry < 1 ){` |
|        - | 10594 | `		/* Empty map,return  0 */` |
|      ! 0 | 10595 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 10596 | `		return PH7_OK;` |
|        - | 10597 | `	}` |
|        - | 10598 | `	/* Prepare the aux data */` |
|        5 | 10599 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 10600 | `	if( nArg > 1 ){` |
|        3 | 10601 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 10602 | `		if( nArg > 2 ){` |
|      ! 0 | 10603 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 10604 | `		}` |
|        1 | 10605 | `	}` |
|        5 | 10606 | `	sAux.pVm = pCtx->pVm;` |
|        - | 10607 | `	/* Invoke the worker callback */` |
|        5 | 10608 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 10609 | `	/* Number of variables successfully imported */` |
|        5 | 10610 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 10611 | `	return PH7_OK;` |
|        3 | 10612 |  |
|        - | 10613 | `/*` |
|        - | 10614 | ` * Worker callback for the [extract()] function defined` |
|        - | 10615 | ` * below.` |
|        - | 10616 | ` */` |
|        8 | 10617 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 10618 |  |
|        9 | 10619 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 10620 | `	int iFlags = pAux->iFlags;` |
|        9 | 10621 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 10622 | `	ph7_value *pObj;` |
|        - | 10623 | `	SyString sVar;` |
|        9 | 10624 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 10625 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 10626 | `	}` |
|        - | 10627 | `	/* Perform a string cast */` |
|        9 | 10628 | `	PH7_MemObjToString(pKey);` |
|        9 | 10629 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 10630 | `		/* Unavailable variable name */` |
|      ! 0 | 10631 | `		return SXRET_OK;` |
|        - | 10632 | `	}` |
|        9 | 10633 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 10634 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 10635 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 10636 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 10637 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10638 | `			);` |
|      ! 0 | 10639 | `	}else{` |
|       13 | 10640 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 10641 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 10642 | `	}` |
|        9 | 10643 | `	sVar.zString = pAux->zWorker;` |
|        - | 10644 | `	/* Try to extract the variable */` |
|        9 | 10645 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 10646 | `	if( pObj ){` |
|        - | 10647 | `		/* Collision */` |
|        3 | 10648 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 10649 | `			return SXRET_OK;` |
|        - | 10650 | `		}` |
|        3 | 10651 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 10652 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 10653 | `				/* Already prefixed */` |
|      ! 0 | 10654 | `				return SXRET_OK;` |
|        - | 10655 | `			}` |
|      ! 0 | 10656 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 10657 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 10658 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10659 | `				);` |
|      ! 0 | 10660 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 10661 | `		}` |
|        2 | 10662 | `	}else{` |
|        - | 10663 | `		/* Create the variable */` |
|        7 | 10664 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 10665 | `	}` |
|        9 | 10666 | `	if( pObj ){` |
|        - | 10667 | `		/* Overwrite the old value */` |
|        9 | 10668 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 10669 | `		/* Increment counter */` |
|        9 | 10670 | `		pAux->iCount++;` |
|        4 | 10671 | `	}` |
|        9 | 10672 | `	return SXRET_OK;` |
|        5 | 10673 |  |
|        - | 10674 | `/*` |
|        - | 10675 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 10676 | ` * defined below.` |
|        - | 10677 | ` */` |
|        2 | 10678 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 10679 |  |
|        3 | 10680 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 10681 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 10682 | `	ph7_value *pObj;` |
|        - | 10683 | `	SyString sVar;` |
|        - | 10684 | `	/* Perform a string cast */` |
|        3 | 10685 | `	PH7_MemObjToString(pKey);` |
|        3 | 10686 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 10687 | `		/* Unavailable variable name */` |
|      ! 0 | 10688 | `		return SXRET_OK;` |
|        - | 10689 | `	}` |
|        3 | 10690 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 10691 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 10692 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 10693 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 10694 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10695 | `			);` |
|        2 | 10696 | `	}else{` |
|      ! 0 | 10697 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 10698 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 10699 | `	}` |
|        3 | 10700 | `	sVar.zString = pAux->zWorker;` |
|        - | 10701 | `	/* Extract the variable */` |
|        3 | 10702 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 10703 | `	if( pObj ){` |
|        3 | 10704 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 10705 | `	}` |
|        3 | 10706 | `	return SXRET_OK;` |
|        2 | 10707 |  |
|        - | 10708 | `/*` |
|        - | 10709 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 10710 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 10711 | ` * Parameters` |
|        - | 10712 | ` * $types` |
|        - | 10713 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 10714 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 10715 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 10716 | ` *  POST includes the POST uploaded file information.` |
|        - | 10717 | ` *  Note:` |
|        - | 10718 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 10719 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 10720 | ` * $prefix` |
|        - | 10721 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 10722 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 10723 | ` *  variable named $pref_userid.` |
|        - | 10724 | ` * Return` |
|        - | 10725 | ` *  TRUE on success or FALSE on failure.` |
|        - | 10726 | ` */` |
|        2 | 10727 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10728 |  |
|        - | 10729 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 10730 | `	extract_aux_data sAux;` |
|        - | 10731 | `	int nLen,nPrefixLen;` |
|        - | 10732 | `	ph7_value *pSuper;` |
|        - | 10733 | `	ph7_vm *pVm;` |
|        - | 10734 | `	/* By default import only $_GET variables  */` |
|        3 | 10735 | `	zImport = "G";` |
|        3 | 10736 | `	nLen = (int)sizeof(char);` |
|        3 | 10737 | `	zPrefix = 0;` |
|        3 | 10738 | `	nPrefixLen = 0;` |
|        3 | 10739 | `	if( nArg > 0 ){` |
|        3 | 10740 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 10741 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 10742 | `		}` |
|        3 | 10743 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 10744 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 10745 | `		}` |
|        1 | 10746 | `	}` |
|        - | 10747 | `	/* Point to the underlying VM */` |
|        3 | 10748 | `	pVm = pCtx->pVm;` |
|        - | 10749 | `	/* Initialize the aux data */` |
|        3 | 10750 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 10751 | `	sAux.zPrefix = zPrefix;` |
|        3 | 10752 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 10753 | `	sAux.pVm = pVm;` |
|        - | 10754 | `	/* Extract */` |
|        3 | 10755 | `	zEnd = &zImport[nLen];` |
|        5 | 10756 | `	while( zImport < zEnd ){` |
|        3 | 10757 | `		int c = zImport[0];` |
|        3 | 10758 | `		pSuper = 0;` |
|        3 | 10759 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 10760 | `			/* Import $_GET variables */` |
|        3 | 10761 | `			pSuper = VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 10762 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 10763 | `			/* Import $_POST variables */` |
|      ! 0 | 10764 | `			pSuper = VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 10765 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 10766 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 10767 | `			pSuper = VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 10768 | `		}` |
|        3 | 10769 | `		if( pSuper ){` |
|        - | 10770 | `			/* Iterate throw array entries */` |
|        3 | 10771 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 10772 | `		}` |
|        - | 10773 | `		/* Advance the cursor */` |
|        3 | 10774 | `		zImport++;` |
|        1 | 10775 | `	}` |
|        - | 10776 | `	/* All done,return TRUE*/` |
|        3 | 10777 | `	ph7_result_bool(pCtx,0);` |
|        3 | 10778 | `	return PH7_OK;` |
|        1 | 10779 |  |
|        - | 10780 | `/*` |
|        - | 10781 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 10782 | ` * Refer to the eval() language construct implementation for more` |
|        - | 10783 | ` * information.` |
|        - | 10784 | ` */` |
|     8918 | 10785 | `static sxi32 VmEvalChunk(` |
|        - | 10786 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 10787 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 10788 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 10789 | `	int iFlags,         /* Compile flag */` |
|        - | 10790 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 10791 | `	)` |
|        2 | 10792 |  |
|        - | 10793 | `	SySet *pByteCode,aByteCode;` |
|     8920 | 10794 | `	ProcConsumer xErr = 0;` |
|     8920 | 10795 | `	void *pErrData = 0;` |
|        - | 10796 | `	/* Initialize bytecode container */` |
|     8920 | 10797 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     8920 | 10798 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 10799 | `	/* Reset the code generator */` |
|     8920 | 10800 | `	if( bTrueReturn ){` |
|        - | 10801 | `		/* Included file,log compile-time errors */` |
|     7191 | 10802 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7191 | 10803 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3595 | 10804 | `	}` |
|     8920 | 10805 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 10806 | `	/* Swap bytecode container */` |
|     8920 | 10807 | `	pByteCode = pVm->pByteContainer;` |
|     8920 | 10808 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 10809 | `	/* Compile the chunk */` |
|     8920 | 10810 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    13379 | 10811 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 10812 | `		/* Compilation error,return false */` |
|        3 | 10813 | `		if( pCtx ){` |
|        3 | 10814 | `			ph7_result_bool(pCtx,0);` |
|        1 | 10815 | `		}` |
|        2 | 10816 | `	}else{` |
|        - | 10817 | `		/* Mount any newly defined classes */` |
|        - | 10818 | `		SyHashEntry *pEntry;` |
|        - | 10819 | `		ph7_class *pClass;` |
|        - | 10820 | `		ph7_value sResult; /* Return value */` |
|        - | 10821 | `		sxi32 rc;` |
|     8918 | 10822 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   249862 | 10823 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   236488 | 10824 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 10825 | `			/* Only mount classes that haven't been mounted yet */` |
|   236488 | 10826 | `			if( !pClass->bMounted ){` |
|    49802 | 10827 | `				rc = VmMountUserClass(pVm,pClass);` |
|    49802 | 10828 | `				if( rc != SXRET_OK ){` |
|        - | 10829 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 10830 | `					if( pCtx ){` |
|      ! 0 | 10831 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 10832 | `					}` |
|      ! 0 | 10833 | `					goto Cleanup;` |
|        - | 10834 | `				}` |
|    24900 | 10835 | `			}` |
|        2 | 10836 | `		}` |
|     8918 | 10837 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 10838 | `			/* Out of memory */` |
|      ! 0 | 10839 | `			if( pCtx ){` |
|      ! 0 | 10840 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 10841 | `			}` |
|      ! 0 | 10842 | `			goto Cleanup;` |
|        - | 10843 | `		}` |
|     8918 | 10844 | `		if( bTrueReturn ){` |
|        - | 10845 | `			/* Assume a boolean true return value */` |
|     7191 | 10846 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3596 | 10847 | `		}else{` |
|        - | 10848 | `			/* Assume a null return value */` |
|     1728 | 10849 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 10850 | `		}` |
|        - | 10851 | `		/* Execute the compiled chunk */` |
|     8918 | 10852 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|     8918 | 10853 | `		if( pCtx ){` |
|        - | 10854 | `			/* Set the execution result */` |
|     7208 | 10855 | `			ph7_result_value(pCtx,&sResult);` |
|     3603 | 10856 | `		}` |
|     8918 | 10857 | `		PH7_MemObjRelease(&sResult);` |
|        - | 10858 | `	}` |
|     4459 | 10859 | `Cleanup:` |
|        - | 10860 | `	/* Cleanup the mess left behind */` |
|     8920 | 10861 | `	pVm->pByteContainer = pByteCode;` |
|     8920 | 10862 | `	SySetRelease(&aByteCode);` |
|     8920 | 10863 | `	return SXRET_OK;` |
|        2 | 10864 |  |
|        - | 10865 | `/*` |
|        - | 10866 | ` * value eval(string $code)` |
|        - | 10867 | ` *   Evaluate a string as PHP code.` |
|        - | 10868 | ` * Parameter` |
|        - | 10869 | ` *  code: PHP code to evaluate.` |
|        - | 10870 | ` * Return` |
|        - | 10871 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 10872 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 10873 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 10874 | ` */` |
|       16 | 10875 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10876 |  |
|        - | 10877 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 | 10878 | `	if( nArg < 1 ){` |
|        - | 10879 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10880 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10881 | `		return SXRET_OK;` |
|        - | 10882 | `	}` |
|        - | 10883 | `	/* Chunk to evaluate */` |
|       18 | 10884 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 | 10885 | `	if( sChunk.nByte < 1 ){` |
|        - | 10886 | `		/* Empty string,return NULL */` |
|        3 | 10887 | `		ph7_result_null(pCtx);` |
|        3 | 10888 | `		return SXRET_OK;` |
|        - | 10889 | `	}` |
|        - | 10890 | `	/* Eval the chunk */` |
|       16 | 10891 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 | 10892 | `	return SXRET_OK;` |
|       10 | 10893 |  |
|        - | 10894 | `/*` |
|        - | 10895 | ` * Check if a file path is already included.` |
|        - | 10896 | ` */` |
|    14376 | 10897 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 | 10898 |  |
|        - | 10899 | `	SyString *aEntries;` |
|        - | 10900 | `	sxu32 n;` |
|    14377 | 10901 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 10902 | `	/* Perform a linear search */` |
| 51656715 | 10903 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 51642345 | 10904 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 10905 | `			/* Already included */` |
|        7 | 10906 | `			return TRUE;` |
|        - | 10907 | `		}` |
| 25821170 | 10908 | `	}` |
|    14371 | 10909 | `	return FALSE;` |
|     7189 | 10910 |  |
|        - | 10911 | `/*` |
|        - | 10912 | ` * Push a file path in the appropriate VM container.` |
|        - | 10913 | ` */` |
|    16078 | 10914 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 10915 |  |
|        - | 10916 | `	SyString sPath;` |
|        - | 10917 | `	char *zDup;` |
|        - | 10918 | `#ifdef __WINNT__` |
|        - | 10919 | `	char *zCur;` |
|        - | 10920 | `#endif` |
|        - | 10921 | `	sxi32 rc;` |
|    16080 | 10922 | `	if( nLen < 0 ){` |
|     1704 | 10923 | `		nLen = SyStrlen(zPath);` |
|      851 | 10924 | `	}` |
|        - | 10925 | `	/* Duplicate the file path first */` |
|    16080 | 10926 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    16080 | 10927 | `	if( zDup == 0 ){` |
|      ! 0 | 10928 | `		return SXERR_MEM;` |
|        - | 10929 | `	}` |
|        - | 10930 | `#ifdef __WINNT__` |
|        - | 10931 | `	/* Normalize path on windows` |
|        - | 10932 | `	 * Example:` |
|        - | 10933 | `	 *    Path/To/File.php` |
|        - | 10934 | `	 * becomes` |
|        - | 10935 | `	 *   path\to\file.php` |
|        - | 10936 | `	 */` |
|        2 | 10937 | `	zCur = zDup;` |
|        2 | 10938 | `	while( zCur[0] != 0 ){` |
|        2 | 10939 | `		if( zCur[0] == '/' ){` |
|        2 | 10940 | `			zCur[0] = '\\';` |
|        2 | 10941 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 10942 | `			int c = SyToLower(zCur[0]);` |
|        1 | 10943 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 10944 | `		}` |
|        2 | 10945 | `		zCur++;` |
|        2 | 10946 | `	}` |
|        - | 10947 | `#endif` |
|        - | 10948 | `	/* Install the file path */` |
|    16080 | 10949 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    16080 | 10950 | `	if( !bMain ){` |
|    14377 | 10951 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 10952 | `			/* Already included */` |
|        7 | 10953 | `			*pNew = 0;` |
|        4 | 10954 | `		}else{` |
|        - | 10955 | `			/* Insert in the corresponding container */` |
|    14371 | 10956 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    14371 | 10957 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10958 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 10959 | `				return rc;` |
|        - | 10960 | `			}` |
|    14371 | 10961 | `			*pNew = 1;` |
|        - | 10962 | `		}` |
|     7188 | 10963 | `	}` |
|    16080 | 10964 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    16080 | 10965 | `	return SXRET_OK;` |
|     8041 | 10966 |  |
|        - | 10967 | `/*` |
|        - | 10968 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 10969 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 10970 | ` * indicates failure.` |
|        - | 10971 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 10972 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 10973 | ` * operations.` |
|        - | 10974 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 10975 | ` * this function is a no-op.` |
|        - | 10976 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 10977 | ` * constructs for more information.` |
|        - | 10978 | ` */` |
|     7196 | 10979 | `static sxi32 VmExecIncludedFile(` |
|        - | 10980 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 10981 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 10982 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 10983 | `	 )` |
|        2 | 10984 |  |
|        - | 10985 | `	sxi32 rc;` |
|        - | 10986 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10987 | `	const ph7_io_stream *pStream;` |
|        - | 10988 | `	SyBlob sContents;` |
|        - | 10989 | `	void *pHandle;` |
|        - | 10990 | `	ph7_vm *pVm;` |
|        - | 10991 | `	int isNew;` |
|        - | 10992 | `	/* Initialize fields */` |
|     7198 | 10993 | `	pVm = pCtx->pVm;` |
|     7198 | 10994 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7198 | 10995 | `	isNew = 0;` |
|        - | 10996 | `	/* Extract the associated stream */` |
|     7198 | 10997 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 10998 | `	/*` |
|        - | 10999 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 11000 | `	 * in a read-only mode.` |
|        - | 11001 | `	 */` |
|     7198 | 11002 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7198 | 11003 | `	if( pHandle == 0 ){` |
|        3 | 11004 | `		return SXERR_IO;` |
|        - | 11005 | `	}` |
|     7195 | 11006 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7195 | 11007 | `	if( IncludeOnce && !isNew ){` |
|        - | 11008 | `		/* Already included */` |
|        5 | 11009 | `		rc = SXERR_EXISTS;` |
|        3 | 11010 | `	}else{` |
|        - | 11011 | `		/* Read the whole file contents */` |
|     7191 | 11012 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7191 | 11013 | `		if( rc == SXRET_OK ){` |
|        - | 11014 | `			SyString sScript;` |
|        - | 11015 | `			/* Compile and execute the script */` |
|     7191 | 11016 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7191 | 11017 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3595 | 11018 | `		}` |
|        - | 11019 | `	}` |
|        - | 11020 | `	/* Pop from the set of included file */` |
|     7195 | 11021 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 11022 | `	/* Close the handle */` |
|     7195 | 11023 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 11024 | `	/* Release the working buffer */` |
|     7195 | 11025 | `	SyBlobRelease(&sContents);` |
|        - | 11026 | `#else` |
|        - | 11027 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 11028 | `	SXUNUSED(pPath);` |
|        - | 11029 | `	SXUNUSED(IncludeOnce);` |
|        - | 11030 | `	rc = SXERR_IO;` |
|        - | 11031 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7195 | 11032 | `	return rc;` |
|     3600 | 11033 |  |
|        - | 11034 | `/*` |
|        - | 11035 | ` * string get_include_path(void)` |
|        - | 11036 | ` *  Gets the current include_path configuration option.` |
|        - | 11037 | ` * Parameter` |
|        - | 11038 | ` *  None` |
|        - | 11039 | ` * Return` |
|        - | 11040 | ` *  Included paths as a string` |
|        - | 11041 | ` */` |
|        2 | 11042 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11043 |  |
|        3 | 11044 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11045 | `	SyString *aEntry;` |
|        - | 11046 | `	int dir_sep;` |
|        - | 11047 | `	sxu32 n;` |
|        - | 11048 | `#ifdef __WINNT__` |
|        1 | 11049 | `	dir_sep = ';';` |
|        - | 11050 | `#else` |
|        - | 11051 | `	/* Assume UNIX path separator */` |
|        2 | 11052 | `	dir_sep = ':';` |
|        - | 11053 | `#endif` |
|        1 | 11054 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11055 | `	SXUNUSED(apArg);` |
|        - | 11056 | `	/* Point to the list of import paths */` |
|        3 | 11057 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 11058 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 11059 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 11060 | `		if( n > 0 ){` |
|        - | 11061 | `			/* Append dir seprator */` |
|      ! 0 | 11062 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 11063 | `		}` |
|        - | 11064 | `		/* Append path */` |
|        3 | 11065 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 11066 | `	}` |
|        3 | 11067 | `	return PH7_OK;` |
|        1 | 11068 |  |
|        - | 11069 | `/*` |
|        - | 11070 | ` * string get_get_included_files(void)` |
|        - | 11071 | ` *  Gets the current include_path configuration option.` |
|        - | 11072 | ` * Parameter` |
|        - | 11073 | ` *  None` |
|        - | 11074 | ` * Return` |
|        - | 11075 | ` *  Included paths as a string` |
|        - | 11076 | ` */` |
|        2 | 11077 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11078 |  |
|        3 | 11079 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 11080 | `	ph7_value *pArray,*pWorker;` |
|        - | 11081 | `	SyString *pEntry;` |
|        - | 11082 | `	int c,d;` |
|        - | 11083 | `	/* Create an array and a working value */` |
|        3 | 11084 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 11085 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11086 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 11087 | `		/* Out of memory,return null */` |
|      ! 0 | 11088 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11089 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11090 | `		SXUNUSED(apArg);` |
|      ! 0 | 11091 | `		return PH7_OK;` |
|        - | 11092 | `	}` |
|        3 | 11093 | `	c = d = '/';` |
|        - | 11094 | `#ifdef __WINNT__` |
|        1 | 11095 | `	d = '\\';` |
|        - | 11096 | `#endif` |
|        - | 11097 | `	/* Iterate throw entries */` |
|        3 | 11098 | `	SySetResetCursor(pFiles);` |
|     3387 | 11099 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 11100 | `		const char *zBase,*zEnd;` |
|        - | 11101 | `		int iLen;` |
|        - | 11102 | `		/* reset the string cursor */` |
|     3385 | 11103 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 11104 | `		/* Extract base name */` |
|     3385 | 11105 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 11106 | `		/* Ignore trailing '/' */` |
|     5077 | 11107 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 11108 | `			zEnd--;` |
|      ! 0 | 11109 | `		}` |
|     3385 | 11110 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   101927 | 11111 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    96851 | 11112 | `			zEnd--;` |
|        1 | 11113 | `		}` |
|     3385 | 11114 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3385 | 11115 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 11116 | `		/* Copy entry name */` |
|     3385 | 11117 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 11118 | `		/* Perform the insertion */` |
|     3385 | 11119 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 11120 | `	}` |
|        - | 11121 | `	/* All done,return the created array */` |
|        3 | 11122 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11123 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 11124 | `	 * by the engine as soon we return from this foreign` |
|        - | 11125 | `	 * function.` |
|        - | 11126 | `	 */` |
|        3 | 11127 | `	return PH7_OK;` |
|        2 | 11128 |  |
|        - | 11129 | `/*` |
|        - | 11130 | ` * include:` |
|        - | 11131 | ` * According to the PHP reference manual.` |
|        - | 11132 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 11133 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 11134 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 11135 | ` *  include() will finally check in the calling script's own directory` |
|        - | 11136 | ` *  and the current working directory before failing. The include()` |
|        - | 11137 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 11138 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 11139 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 11140 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 11141 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 11142 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 11143 | ` *  directory to find the requested file.` |
|        - | 11144 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 11145 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 11146 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 11147 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 11148 | ` */` |
|     7184 | 11149 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11150 |  |
|        - | 11151 | `	SyString sFile;` |
|        - | 11152 | `	sxi32 rc;` |
|     7186 | 11153 | `	if( nArg < 1 ){` |
|        - | 11154 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11155 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11156 | `		return SXRET_OK;` |
|        - | 11157 | `	}` |
|        - | 11158 | `	/* File to include */` |
|     7186 | 11159 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7186 | 11160 | `	if( sFile.nByte < 1 ){` |
|        - | 11161 | `		/* Empty string,return NULL */` |
|      ! 0 | 11162 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11163 | `		return SXRET_OK;` |
|        - | 11164 | `	}` |
|        - | 11165 | `	/* Open,compile and execute the desired script */` |
|     7186 | 11166 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7186 | 11167 | `	if( rc != SXRET_OK ){` |
|        - | 11168 | `		/* Emit a warning and return false */` |
|        3 | 11169 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 11170 | `		ph7_result_bool(pCtx,0);` |
|        1 | 11171 | `	}` |
|     7186 | 11172 | `	return SXRET_OK;` |
|     3594 | 11173 |  |
|        - | 11174 | `/*` |
|        - | 11175 | ` * include_once:` |
|        - | 11176 | ` *  According to the PHP reference manual.` |
|        - | 11177 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 11178 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 11179 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 11180 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 11181 | ` *   just once.` |
|        - | 11182 | ` */` |
|        4 | 11183 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11184 |  |
|        - | 11185 | `	SyString sFile;` |
|        - | 11186 | `	sxi32 rc;` |
|        5 | 11187 | `	if( nArg < 1 ){` |
|        - | 11188 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11189 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11190 | `		return SXRET_OK;` |
|        - | 11191 | `	}` |
|        - | 11192 | `	/* File to include */` |
|        5 | 11193 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11194 | `	if( sFile.nByte < 1 ){` |
|        - | 11195 | `		/* Empty string,return NULL */` |
|      ! 0 | 11196 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11197 | `		return SXRET_OK;` |
|        - | 11198 | `	}` |
|        - | 11199 | `	/* Open,compile and execute the desired script */` |
|        5 | 11200 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11201 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11202 | `		/* File already included,return TRUE */` |
|        3 | 11203 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11204 | `		return SXRET_OK;` |
|        - | 11205 | `	}` |
|        3 | 11206 | `	if( rc != SXRET_OK ){` |
|        - | 11207 | `		/* Emit a warning and return false */` |
|      ! 0 | 11208 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11209 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11210 | ` 	}` |
|        3 | 11211 | `	return SXRET_OK;` |
|        3 | 11212 |  |
|        - | 11213 | `/*` |
|        - | 11214 | ` * require.` |
|        - | 11215 | ` *  According to the PHP reference manual.` |
|        - | 11216 | ` *   require() is identical to include() except upon failure it will` |
|        - | 11217 | ` *   also produce a fatal level error.` |
|        - | 11218 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 11219 | ` *   emits a warning  which allows the script to continue.` |
|        - | 11220 | ` */` |
|        4 | 11221 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11222 |  |
|        - | 11223 | `	SyString sFile;` |
|        - | 11224 | `	sxi32 rc;` |
|        5 | 11225 | `	if( nArg < 1 ){` |
|        - | 11226 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11227 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11228 | `		return SXRET_OK;` |
|        - | 11229 | `	}` |
|        - | 11230 | `	/* File to include */` |
|        5 | 11231 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11232 | `	if( sFile.nByte < 1 ){` |
|        - | 11233 | `		/* Empty string,return NULL */` |
|      ! 0 | 11234 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11235 | `		return SXRET_OK;` |
|        - | 11236 | `	}` |
|        - | 11237 | `	/* Open,compile and execute the desired script */` |
|        5 | 11238 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 11239 | `	if( rc != SXRET_OK ){` |
|        - | 11240 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11241 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11242 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11243 | `		return PH7_ABORT;` |
|        - | 11244 | `	}` |
|        5 | 11245 | `	return SXRET_OK;` |
|        3 | 11246 |  |
|        - | 11247 | `/*` |
|        - | 11248 | ` * require_once:` |
|        - | 11249 | ` *  According to the PHP reference manual.` |
|        - | 11250 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 11251 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 11252 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 11253 | ` *   and how it differs from its non _once siblings.` |
|        - | 11254 | ` */` |
|        4 | 11255 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11256 |  |
|        - | 11257 | `	SyString sFile;` |
|        - | 11258 | `	sxi32 rc;` |
|        5 | 11259 | `	if( nArg < 1 ){` |
|        - | 11260 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11261 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11262 | `		return SXRET_OK;` |
|        - | 11263 | `	}` |
|        - | 11264 | `	/* File to include */` |
|        5 | 11265 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11266 | `	if( sFile.nByte < 1 ){` |
|        - | 11267 | `		/* Empty string,return NULL */` |
|      ! 0 | 11268 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11269 | `		return SXRET_OK;` |
|        - | 11270 | `	}` |
|        - | 11271 | `	/* Open,compile and execute the desired script */` |
|        5 | 11272 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11273 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11274 | `		/* File already included,return TRUE */` |
|        3 | 11275 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11276 | `		return SXRET_OK;` |
|        - | 11277 | `	}` |
|        3 | 11278 | `	if( rc != SXRET_OK ){` |
|        - | 11279 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11280 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11281 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11282 | `		return PH7_ABORT;` |
|        - | 11283 | `	}` |
|        3 | 11284 | `	return SXRET_OK;` |
|        3 | 11285 |  |
|        - | 11286 | `/*` |
|        - | 11287 | ` * Section:` |
|        - | 11288 | ` *  Command line arguments processing.` |
|        - | 11289 | ` * Status:` |
|        - | 11290 | ` *    Stable.` |
|        - | 11291 | ` */` |
|        - | 11292 | `/*` |
|        - | 11293 | ` * Check if a short option argument [i.e: -c] is available in the command` |
|        - | 11294 | ` * line string. Return a pointer to the start of the stream on success.` |
|        - | 11295 | ` * NULL otherwise.` |
|        - | 11296 | ` */` |
|        6 | 11297 | `static const char * VmFindShortOpt(int c,const char *zIn,const char *zEnd)` |
|        1 | 11298 |  |
|      319 | 11299 | `	while( zIn < zEnd ){` |
|      313 | 11300 | `		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == c ){` |
|        - | 11301 | `			/* Got one */` |
|      ! 0 | 11302 | `			return &zIn[1];` |
|        - | 11303 | `		}` |
|        - | 11304 | `		/* Advance the cursor */` |
|      313 | 11305 | `		zIn++;` |
|        1 | 11306 | `	}` |
|        - | 11307 | `	/* No such option */` |
|        7 | 11308 | `	return 0;` |
|        4 | 11309 |  |
|        - | 11310 | `/*` |
|        - | 11311 | ` * Check if a long option argument [i.e: --opt] is available in the command` |
|        - | 11312 | ` * line string. Return a pointer to the start of the stream on success.` |
|        - | 11313 | ` * NULL otherwise.` |
|        - | 11314 | ` */` |
|      ! 0 | 11315 | `static const char * VmFindLongOpt(const char *zLong,int nByte,const char *zIn,const char *zEnd)` |
|      ! 0 | 11316 |  |
|        - | 11317 | `	const char *zOpt;` |
|      ! 0 | 11318 | `	while( zIn < zEnd ){` |
|      ! 0 | 11319 | `		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == '-' ){` |
|      ! 0 | 11320 | `			zIn += 2;` |
|      ! 0 | 11321 | `			zOpt = zIn;` |
|      ! 0 | 11322 | `			while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 11323 | `				if( zIn[0] == '=' /* --opt=val */){` |
|      ! 0 | 11324 | `					break;` |
|        - | 11325 | `				}` |
|      ! 0 | 11326 | `				zIn++;` |
|      ! 0 | 11327 | `			}` |
|        - | 11328 | `			/* Test */` |
|      ! 0 | 11329 | `			if( (int)(zIn-zOpt) == nByte && SyMemcmp(zOpt,zLong,nByte) == 0 ){` |
|        - | 11330 | `				/* Got one,return it's value */` |
|      ! 0 | 11331 | `				return zIn;` |
|        - | 11332 | `			}` |
|        - | 11333 |  |
|      ! 0 | 11334 | `		}else{` |
|      ! 0 | 11335 | `			zIn++;` |
|        - | 11336 | `		}` |
|      ! 0 | 11337 | `	}` |
|        - | 11338 | `	/* No such option */` |
|      ! 0 | 11339 | `	return 0;` |
|      ! 0 | 11340 |  |
|        - | 11341 | `/*` |
|        - | 11342 | ` * Long option [i.e: --opt] arguments private data structure.` |
|        - | 11343 | ` */` |
|        - | 11344 | `struct getopt_long_opt` |
|        - | 11345 |  |
|        - | 11346 | `	const char *zArgIn,*zArgEnd; /* Command line arguments */` |
|        - | 11347 | `	ph7_value *pWorker;  /* Worker variable*/` |
|        - | 11348 | `	ph7_value *pArray;   /* getopt() return value */` |
|        - | 11349 | `	ph7_context *pCtx;   /* Call Context */` |
|        - | 11350 | `};` |
|        - | 11351 | `/* Forward declaration */` |
|        - | 11352 | `static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11353 | `/*` |
|        - | 11354 | ` * Extract short or long argument option values.` |
|        - | 11355 | ` */` |
|      ! 0 | 11356 | `static void VmExtractOptArgValue(` |
|        - | 11357 | `	ph7_value *pArray,  /* getopt() return value */` |
|        - | 11358 | `	ph7_value *pWorker, /* Worker variable */` |
|        - | 11359 | `	const char *zArg,   /* Argument stream */` |
|        - | 11360 | `	const char *zArgEnd,/* End of the argument stream  */` |
|        - | 11361 | `	int need_val,       /* TRUE to fetch option argument */` |
|        - | 11362 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 11363 | `	const char *zName   /* Option name */)` |
|      ! 0 | 11364 |  |
|      ! 0 | 11365 | `	ph7_value_bool(pWorker,0);` |
|      ! 0 | 11366 | `	if( !need_val ){` |
|        - | 11367 | `		/*` |
|        - | 11368 | `		 * Option does not need arguments.` |
|        - | 11369 | `		 * Insert the option name and a boolean FALSE.` |
|        - | 11370 | `		 */` |
|      ! 0 | 11371 | `		ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11372 | `	}else{` |
|        - | 11373 | `		const char *zCur;` |
|        - | 11374 | `		/* Extract option argument */` |
|      ! 0 | 11375 | `		zArg++;` |
|      ! 0 | 11376 | `		if( zArg < zArgEnd && zArg[0] == '=' ){` |
|      ! 0 | 11377 | `			zArg++;` |
|      ! 0 | 11378 | `		}` |
|      ! 0 | 11379 | `		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11380 | `			zArg++;` |
|      ! 0 | 11381 | `		}` |
|      ! 0 | 11382 | `		if( zArg >= zArgEnd \|\| zArg[0] == '-' ){` |
|        - | 11383 | `			/*` |
|        - | 11384 | `			 * Argument not found.` |
|        - | 11385 | `			 * Insert the option name and a boolean FALSE.` |
|        - | 11386 | `			 */` |
|      ! 0 | 11387 | `			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11388 | `			return;` |
|        - | 11389 | `		}` |
|        - | 11390 | `		/* Delimit the value */` |
|      ! 0 | 11391 | `		zCur = zArg;` |
|      ! 0 | 11392 | `		if( zArg[0] == '\'' \|\| zArg[0] == '"' ){` |
|      ! 0 | 11393 | `			int d = zArg[0];` |
|        - | 11394 | `			/* Delimt the argument */` |
|      ! 0 | 11395 | `			zArg++;` |
|      ! 0 | 11396 | `			zCur = zArg;` |
|      ! 0 | 11397 | `			while( zArg < zArgEnd ){` |
|      ! 0 | 11398 | `				if( zArg[0] == d && zArg[-1] != '\\' ){` |
|        - | 11399 | `					/* Delimiter found,exit the loop  */` |
|      ! 0 | 11400 | `					break;` |
|        - | 11401 | `				}` |
|      ! 0 | 11402 | `				zArg++;` |
|      ! 0 | 11403 | `			}` |
|        - | 11404 | `			/* Save the value */` |
|      ! 0 | 11405 | `			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|      ! 0 | 11406 | `			if( zArg < zArgEnd ){ zArg++; }` |
|      ! 0 | 11407 | `		}else{` |
|      ! 0 | 11408 | `			while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){` |
|      ! 0 | 11409 | `				zArg++;` |
|      ! 0 | 11410 | `			}` |
|        - | 11411 | `			/* Save the value */` |
|      ! 0 | 11412 | `			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|        - | 11413 | `		}` |
|        - | 11414 | `		/*` |
|        - | 11415 | `		 * Check if we are dealing with multiple values.` |
|        - | 11416 | `		 * If so,create an array to hold them,rather than a scalar variable.` |
|        - | 11417 | `		 */` |
|      ! 0 | 11418 | `		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11419 | `			zArg++;` |
|      ! 0 | 11420 | `		}` |
|      ! 0 | 11421 | `		if( zArg < zArgEnd && zArg[0] != '-' ){` |
|        - | 11422 | `			ph7_value *pOptArg; /* Array of option arguments */` |
|      ! 0 | 11423 | `			pOptArg = ph7_context_new_array(pCtx);` |
|      ! 0 | 11424 | `			if( pOptArg == 0 ){` |
|      ! 0 | 11425 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11426 | `			}else{` |
|        - | 11427 | `				/* Insert the first value */` |
|      ! 0 | 11428 | `				ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11429 | `				for(;;){` |
|      ! 0 | 11430 | `					if( zArg >= zArgEnd \|\| zArg[0] == '-' ){` |
|        - | 11431 | `						/* No more value */` |
|      ! 0 | 11432 | `						break;` |
|        - | 11433 | `					}` |
|        - | 11434 | `					/* Delimit the value */` |
|      ! 0 | 11435 | `					zCur = zArg;` |
|      ! 0 | 11436 | `					if( zArg < zArgEnd && zArg[0] == '\\' ){` |
|      ! 0 | 11437 | `						zArg++;` |
|      ! 0 | 11438 | `						zCur = zArg;` |
|      ! 0 | 11439 | `					}` |
|      ! 0 | 11440 | `					while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){` |
|      ! 0 | 11441 | `						zArg++;` |
|      ! 0 | 11442 | `					}` |
|        - | 11443 | `					/* Reset the string cursor */` |
|      ! 0 | 11444 | `					ph7_value_reset_string_cursor(pWorker);` |
|        - | 11445 | `					/* Save the value */` |
|      ! 0 | 11446 | `					ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|        - | 11447 | `					/* Insert */` |
|      ! 0 | 11448 | `					ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */` |
|        - | 11449 | `					/* Jump trailing white spaces */` |
|      ! 0 | 11450 | `					while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11451 | `						zArg++;` |
|      ! 0 | 11452 | `					}` |
|      ! 0 | 11453 | `				}` |
|        - | 11454 | `				/* Insert the option arg array */` |
|      ! 0 | 11455 | `				ph7_array_add_strkey_elem(pArray,(const char *)zName,pOptArg); /* Will make it's own copy */` |
|        - | 11456 | `				/* Safely release */` |
|      ! 0 | 11457 | `				ph7_context_release_value(pCtx,pOptArg);` |
|        - | 11458 | `			}` |
|      ! 0 | 11459 | `		}else{` |
|        - | 11460 | `			/* Single value */` |
|      ! 0 | 11461 | `			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|        - | 11462 | `		}` |
|        - | 11463 | `	}` |
|      ! 0 | 11464 |  |
|        - | 11465 | `/*` |
|        - | 11466 | ` * array getopt(string $options[,array $longopts ])` |
|        - | 11467 | ` *   Gets options from the command line argument list.` |
|        - | 11468 | ` * Parameters` |
|        - | 11469 | ` *  $options` |
|        - | 11470 | ` *   Each character in this string will be used as option characters` |
|        - | 11471 | ` *   and matched against options passed to the script starting with` |
|        - | 11472 | ` *   a single hyphen (-). For example, an option string "x" recognizes` |
|        - | 11473 | ` *   an option -x. Only a-z, A-Z and 0-9 are allowed.` |
|        - | 11474 | ` *  $longopts` |
|        - | 11475 | ` *   An array of options. Each element in this array will be used as option` |
|        - | 11476 | ` *   strings and matched against options passed to the script starting with` |
|        - | 11477 | ` *   two hyphens (--). For example, an longopts element "opt" recognizes an` |
|        - | 11478 | ` *   option --opt.` |
|        - | 11479 | ` * Return` |
|        - | 11480 | ` *  This function will return an array of option / argument pairs or FALSE` |
|        - | 11481 | ` *  on failure.` |
|        - | 11482 | ` */` |
|        2 | 11483 | `static int vm_builtin_getopt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11484 |  |
|        - | 11485 | `	const char *zIn,*zEnd,*zArg,*zArgIn,*zArgEnd;` |
|        - | 11486 | `	struct getopt_long_opt sLong;` |
|        - | 11487 | `	ph7_value *pArray,*pWorker;` |
|        - | 11488 | `	SyBlob *pArg;` |
|        - | 11489 | `	int nByte;` |
|        3 | 11490 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11491 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 11492 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Missing/Invalid option arguments");` |
|      ! 0 | 11493 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11494 | `		return PH7_OK;` |
|        - | 11495 | `	}` |
|        - | 11496 | `	/* Extract option arguments */` |
|        3 | 11497 | `	zIn  = ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 11498 | `	zEnd = &zIn[nByte];` |
|        - | 11499 | `	/* Point to the string representation of the $argv[] array */` |
|        3 | 11500 | `	pArg = &pCtx->pVm->sArgv;` |
|        - | 11501 | `	/* Create a new empty array and a worker variable */` |
|        3 | 11502 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11503 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11504 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|      ! 0 | 11505 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11506 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11507 | `		return PH7_OK;` |
|        - | 11508 | `	}` |
|        3 | 11509 | `	if( SyBlobLength(pArg) < 1 ){` |
|        - | 11510 | `		/* Empty command line,return the empty array*/` |
|      ! 0 | 11511 | `		ph7_result_value(pCtx,pArray);` |
|        - | 11512 | `		/* Everything will be released automatically when we return` |
|        - | 11513 | `		 * from this function.` |
|        - | 11514 | `		 */` |
|      ! 0 | 11515 | `		return PH7_OK;` |
|        - | 11516 | `	}` |
|        3 | 11517 | `	zArgIn = (const char *)SyBlobData(pArg);` |
|        3 | 11518 | `	zArgEnd = &zArgIn[SyBlobLength(pArg)];` |
|        - | 11519 | `	/* Fill the long option structure */` |
|        3 | 11520 | `	sLong.pArray = pArray;` |
|        3 | 11521 | `	sLong.pWorker = pWorker;` |
|        3 | 11522 | `	sLong.zArgIn =  zArgIn;` |
|        3 | 11523 | `	sLong.zArgEnd = zArgEnd;` |
|        3 | 11524 | `	sLong.pCtx = pCtx;` |
|        - | 11525 | `	/* Start processing */` |
|        9 | 11526 | `	while( zIn < zEnd ){` |
|        7 | 11527 | `		int c = zIn[0];` |
|        7 | 11528 | `		int need_val = 0;` |
|        - | 11529 | `		/* Advance the stream cursor */` |
|        7 | 11530 | `		zIn++;` |
|        - | 11531 | `		/* Ignore non-alphanum characters */` |
|        7 | 11532 | `		if( !SyisAlphaNum(c) ){` |
|      ! 0 | 11533 | `			continue;` |
|        - | 11534 | `		}` |
|        7 | 11535 | `		if( zIn < zEnd && zIn[0] == ':' ){` |
|        5 | 11536 | `			zIn++;` |
|        5 | 11537 | `			need_val = 1;` |
|        5 | 11538 | `			if( zIn < zEnd && zIn[0] == ':' ){` |
|      ! 0 | 11539 | `				zIn++;` |
|      ! 0 | 11540 | `			}` |
|        2 | 11541 | `		}` |
|        - | 11542 | `		/* Find option */` |
|        7 | 11543 | `		zArg = VmFindShortOpt(c,zArgIn,zArgEnd);` |
|        7 | 11544 | `		if( zArg == 0 ){` |
|        - | 11545 | `			/* No such option */` |
|        7 | 11546 | `			continue;` |
|        - | 11547 | `		}` |
|        - | 11548 | `		/* Extract option argument value */` |
|      ! 0 | 11549 | `		VmExtractOptArgValue(pArray,pWorker,zArg,zArgEnd,need_val,pCtx,(const char *)&c);` |
|      ! 0 | 11550 | `	}` |
|        3 | 11551 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) && ph7_array_count(apArg[1]) > 0 ){` |
|        - | 11552 | `		/* Process long options */` |
|      ! 0 | 11553 | `		ph7_array_walk(apArg[1],VmProcessLongOpt,&sLong);` |
|      ! 0 | 11554 | `	}` |
|        - | 11555 | `	/* Return the option array */` |
|        3 | 11556 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11557 | `	/*` |
|        - | 11558 | `	 * Don't worry about freeing memory, everything will be released` |
|        - | 11559 | `	 * automatically as soon we return from this foreign function.` |
|        - | 11560 | `	 */` |
|        3 | 11561 | `	return PH7_OK;` |
|        2 | 11562 |  |
|        - | 11563 | `/*` |
|        - | 11564 | ` * Array walker callback used for processing long options values.` |
|        - | 11565 | ` */` |
|      ! 0 | 11566 | `static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11567 |  |
|      ! 0 | 11568 | `	struct getopt_long_opt *pOpt = (struct getopt_long_opt *)pUserData;` |
|        - | 11569 | `	const char *zArg,*zOpt,*zEnd;` |
|      ! 0 | 11570 | `	int need_value = 0;` |
|        - | 11571 | `	int nByte;` |
|        - | 11572 | `	/* Value must be of type string */` |
|      ! 0 | 11573 | `	if( !ph7_value_is_string(pValue) ){` |
|        - | 11574 | `		/* Simply ignore */` |
|      ! 0 | 11575 | `		return PH7_OK;` |
|        - | 11576 | `	}` |
|      ! 0 | 11577 | `	zOpt = ph7_value_to_string(pValue,&nByte);` |
|      ! 0 | 11578 | `	if( nByte < 1 ){` |
|        - | 11579 | `		/* Empty string,ignore */` |
|      ! 0 | 11580 | `		return PH7_OK;` |
|        - | 11581 | `	}` |
|      ! 0 | 11582 | `	zEnd = &zOpt[nByte - 1];` |
|      ! 0 | 11583 | `	if( zEnd[0] == ':' ){` |
|        - | 11584 | `		char *zTerm;` |
|        - | 11585 | `		/* Try to extract a value */` |
|      ! 0 | 11586 | `		need_value = 1;` |
|      ! 0 | 11587 | `		while( zEnd >= zOpt && zEnd[0] == ':' ){` |
|      ! 0 | 11588 | `			zEnd--;` |
|      ! 0 | 11589 | `		}` |
|      ! 0 | 11590 | `		if( zOpt >= zEnd ){` |
|        - | 11591 | `			/* Empty string,ignore */` |
|      ! 0 | 11592 | `			SXUNUSED(pKey);` |
|      ! 0 | 11593 | `			return PH7_OK;` |
|        - | 11594 | `		}` |
|      ! 0 | 11595 | `		zEnd++;` |
|      ! 0 | 11596 | `		zTerm = (char *)zEnd;` |
|      ! 0 | 11597 | `		zTerm[0] = 0;` |
|      ! 0 | 11598 | `	}else{` |
|      ! 0 | 11599 | `		zEnd = &zOpt[nByte];` |
|        - | 11600 | `	}` |
|        - | 11601 | `	/* Find the option */` |
|      ! 0 | 11602 | `	zArg = VmFindLongOpt(zOpt,(int)(zEnd-zOpt),pOpt->zArgIn,pOpt->zArgEnd);` |
|      ! 0 | 11603 | `	if( zArg == 0 ){` |
|        - | 11604 | `		/* No such option,return immediately */` |
|      ! 0 | 11605 | `		return PH7_OK;` |
|        - | 11606 | `	}` |
|        - | 11607 | `	/* Try to extract a value */` |
|      ! 0 | 11608 | `	VmExtractOptArgValue(pOpt->pArray,pOpt->pWorker,zArg,pOpt->zArgEnd,need_value,pOpt->pCtx,zOpt);` |
|      ! 0 | 11609 | `	return PH7_OK;` |
|      ! 0 | 11610 |  |
|        - | 11611 | `/*` |
|        - | 11612 | ` * Section:` |
|        - | 11613 | ` *  JSON encoding/decoding routines.` |
|        - | 11614 | ` * Status:` |
|        - | 11615 | ` *    Devel.` |
|        - | 11616 | ` */` |
|        - | 11617 | `/* Forward reference */` |
|        - | 11618 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11619 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData);` |
|        - | 11620 | `/*` |
|        - | 11621 | ` * JSON encoder state is stored in an instance` |
|        - | 11622 | ` * of the following structure.` |
|        - | 11623 | ` */` |
|        - | 11624 | `typedef struct json_private_data json_private_data;` |
|        - | 11625 | `struct json_private_data` |
|        - | 11626 |  |
|        - | 11627 | `	ph7_context *pCtx; /* Call context */` |
|        - | 11628 | `	int isFirst;       /* True if first encoded entry */` |
|        - | 11629 | `	int iFlags;        /* JSON encoding flags */` |
|        - | 11630 | `	int nRecCount;     /* Recursion count */` |
|        - | 11631 | `};` |
|        - | 11632 | `/*` |
|        - | 11633 | ` * Returns the JSON representation of a value.In other word perform a JSON encoding operation.` |
|        - | 11634 | ` * According to wikipedia` |
|        - | 11635 | ` * JSON's basic types are:` |
|        - | 11636 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|        - | 11637 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|        - | 11638 | ` *   Boolean (true or false)` |
|        - | 11639 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|        - | 11640 | ` *    do not need to be of the same type)` |
|        - | 11641 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|        - | 11642 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|        - | 11643 | ` *     be distinct from each other)` |
|        - | 11644 | ` *   null (empty)` |
|        - | 11645 | ` * Non-significant white space may be added freely around the "structural characters"` |
|        - | 11646 | ` * (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|        - | 11647 | ` */` |
|        8 | 11648 | `static sxi32 VmJsonEncode(` |
|        - | 11649 | `	ph7_value *pIn,          /* Encode this value */` |
|        - | 11650 | `	json_private_data *pData /* Context data */` |
|        1 | 11651 | `	){` |
|        9 | 11652 | `		ph7_context *pCtx = pData->pCtx;` |
|        9 | 11653 | `		int iFlags = pData->iFlags;` |
|        - | 11654 | `		int nByte;` |
|        9 | 11655 | `		if( ph7_value_is_null(pIn) \|\| ph7_value_is_resource(pIn)){` |
|        - | 11656 | `			/* null */` |
|      ! 0 | 11657 | `			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|        9 | 11658 | `		}else if( ph7_value_is_bool(pIn) ){` |
|      ! 0 | 11659 | `			int iBool = ph7_value_to_bool(pIn);` |
|        - | 11660 | `			int iLen;` |
|        - | 11661 | `			/* true/false */` |
|      ! 0 | 11662 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|      ! 0 | 11663 | `			ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1);` |
|       12 | 11664 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|        - | 11665 | `			const char *zNum;` |
|        - | 11666 | `			/* Get a string representation of the number */` |
|        7 | 11667 | `			zNum = ph7_value_to_string(pIn,&nByte);` |
|        7 | 11668 | `			ph7_result_string(pCtx,zNum,nByte);` |
|        6 | 11669 | `		}else if( ph7_value_is_string(pIn) ){` |
|      ! 0 | 11670 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|        - | 11671 | `				const char *zNum;` |
|        - | 11672 | `				/* Encodes numeric strings as numbers. */` |
|      ! 0 | 11673 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|        - | 11674 | `				/* Get a string representation of the number */` |
|      ! 0 | 11675 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|      ! 0 | 11676 | `				ph7_result_string(pCtx,zNum,nByte);` |
|      ! 0 | 11677 | `			}else{` |
|        - | 11678 | `				const char *zIn,*zEnd;` |
|        - | 11679 | `				int c;` |
|        - | 11680 | `				/* Encode the string */` |
|      ! 0 | 11681 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|      ! 0 | 11682 | `				zEnd = &zIn[nByte];` |
|        - | 11683 | `				/* Append the double quote */` |
|      ! 0 | 11684 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      ! 0 | 11685 | `				for(;;){` |
|      ! 0 | 11686 | `					if( zIn >= zEnd ){` |
|        - | 11687 | `						/* No more input to process */` |
|      ! 0 | 11688 | `						break;` |
|        - | 11689 | `					}` |
|      ! 0 | 11690 | `					c = zIn[0];` |
|        - | 11691 | `					/* Advance the stream cursor */` |
|      ! 0 | 11692 | `					zIn++;` |
|      ! 0 | 11693 | `					if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|        - | 11694 | `						/* All < and > are converted to \u003C and \u003E */` |
|      ! 0 | 11695 | `						if( c == '<' ){` |
|      ! 0 | 11696 | `							ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1);` |
|      ! 0 | 11697 | `						}else{` |
|      ! 0 | 11698 | `							ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1);` |
|        - | 11699 | `						}` |
|      ! 0 | 11700 | `						continue;` |
|      ! 0 | 11701 | `					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|        - | 11702 | `						/* All &s are converted to \u0026.  */` |
|      ! 0 | 11703 | `						ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1);` |
|      ! 0 | 11704 | `						continue;` |
|      ! 0 | 11705 | `					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|        - | 11706 | `						/* All ' are converted to \u0027.   */` |
|      ! 0 | 11707 | `						ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1);` |
|      ! 0 | 11708 | `						continue;` |
|      ! 0 | 11709 | `					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|        - | 11710 | `						/* All " are converted to \u0022. */` |
|      ! 0 | 11711 | `						ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1);` |
|      ! 0 | 11712 | `						continue;` |
|        - | 11713 | `					}` |
|      ! 0 | 11714 | `					if( c == '"' \|\| (c == '\\' && ((iFlags & JSON_UNESCAPED_SLASHES)==0)) ){` |
|        - | 11715 | `						/* Unescape the character */` |
|      ! 0 | 11716 | `						ph7_result_string(pCtx,"\\",(int)sizeof(char));` |
|      ! 0 | 11717 | `					}` |
|        - | 11718 | `					/* Append character verbatim */` |
|      ! 0 | 11719 | `					ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      ! 0 | 11720 | `				}` |
|        - | 11721 | `				/* Append the double quote */` |
|      ! 0 | 11722 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      ! 0 | 11723 | `			}` |
|        3 | 11724 | `		}else if( ph7_value_is_array(pIn) ){` |
|        3 | 11725 | `			int c = '[',d = ']';` |
|        - | 11726 | `			/* Encode the array */` |
|        3 | 11727 | `			pData->isFirst = 1;` |
|        3 | 11728 | `			if( iFlags & JSON_FORCE_OBJECT ){` |
|        - | 11729 | `				/* Outputs an object rather than an array */` |
|      ! 0 | 11730 | `				c = '{';` |
|      ! 0 | 11731 | `				d = '}';` |
|      ! 0 | 11732 | `			}` |
|        - | 11733 | `			/* Append the square bracket or curly braces */` |
|        3 | 11734 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|        - | 11735 | `			/* Iterate throw array entries */` |
|        3 | 11736 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|        - | 11737 | `			/* Append the closing square bracket or curly braces */` |
|        3 | 11738 | `			ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char));` |
|        1 | 11739 | `		}else if( ph7_value_is_object(pIn) ){` |
|        - | 11740 | `			/* Encode the class instance */` |
|      ! 0 | 11741 | `			pData->isFirst = 1;` |
|        - | 11742 | `			/* Append the curly braces */` |
|      ! 0 | 11743 | `			ph7_result_string(pCtx,"{",(int)sizeof(char));` |
|        - | 11744 | `			/* Iterate throw class attribute */` |
|      ! 0 | 11745 | `			ph7_object_walk(pIn,VmJsonObjectEncode,pData);` |
|        - | 11746 | `			/* Append the closing curly braces  */` |
|      ! 0 | 11747 | `			ph7_result_string(pCtx,"}",(int)sizeof(char));` |
|      ! 0 | 11748 | `		}else{` |
|        - | 11749 | `			/* Can't happen */` |
|      ! 0 | 11750 | `			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|        - | 11751 | `		}` |
|        - | 11752 | `		/* All done */` |
|        9 | 11753 | `		return PH7_OK;` |
|        1 | 11754 |  |
|        - | 11755 | `/*` |
|        - | 11756 | ` * The following walker callback is invoked each time we need` |
|        - | 11757 | ` * to encode an array to JSON.` |
|        - | 11758 | ` */` |
|        6 | 11759 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11760 |  |
|        7 | 11761 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|        7 | 11762 | `	if( pJson->nRecCount > 31 ){` |
|        - | 11763 | `		/* Recursion limit reached,return immediately */` |
|      ! 0 | 11764 | `		return PH7_OK;` |
|        - | 11765 | `	}` |
|        7 | 11766 | `	if( !pJson->isFirst ){` |
|        - | 11767 | `		/* Append the colon first */` |
|        5 | 11768 | `		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));` |
|        2 | 11769 | `	}` |
|        7 | 11770 | `	if( pJson->iFlags & JSON_FORCE_OBJECT ){` |
|        - | 11771 | `		/* Outputs an object rather than an array */` |
|        - | 11772 | `		const char *zKey;` |
|        - | 11773 | `		int nByte;` |
|        - | 11774 | `		/* Extract a string representation of the key */` |
|      ! 0 | 11775 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|        - | 11776 | `		/* Append the key and the double colon */` |
|      ! 0 | 11777 | `		ph7_result_string_format(pJson->pCtx,"\"%.*s\":",nByte,zKey);` |
|      ! 0 | 11778 | `	}` |
|        - | 11779 | `	/* Encode the value */` |
|        7 | 11780 | `	pJson->nRecCount++;` |
|        7 | 11781 | `	VmJsonEncode(pValue,pJson);` |
|        7 | 11782 | `	pJson->nRecCount--;` |
|        7 | 11783 | `	pJson->isFirst = 0;` |
|        7 | 11784 | `	return PH7_OK;` |
|        4 | 11785 |  |
|        - | 11786 | `/*` |
|        - | 11787 | ` * The following walker callback is invoked each time we need to encode` |
|        - | 11788 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|        - | 11789 | ` */` |
|      ! 0 | 11790 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11791 |  |
|      ! 0 | 11792 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|      ! 0 | 11793 | `	if( pJson->nRecCount > 31 ){` |
|        - | 11794 | `		/* Recursion limit reached,return immediately */` |
|      ! 0 | 11795 | `		return PH7_OK;` |
|        - | 11796 | `	}` |
|      ! 0 | 11797 | `	if( !pJson->isFirst ){` |
|        - | 11798 | `		/* Append the colon first */` |
|      ! 0 | 11799 | `		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));` |
|      ! 0 | 11800 | `	}` |
|        - | 11801 | `	/* Append the attribute name and the double colon first */` |
|      ! 0 | 11802 | `	ph7_result_string_format(pJson->pCtx,"\"%s\":",zAttr);` |
|        - | 11803 | `	/* Encode the value */` |
|      ! 0 | 11804 | `	pJson->nRecCount++;` |
|      ! 0 | 11805 | `	VmJsonEncode(pValue,pJson);` |
|      ! 0 | 11806 | `	pJson->nRecCount--;` |
|      ! 0 | 11807 | `	pJson->isFirst = 0;` |
|      ! 0 | 11808 | `	return PH7_OK;` |
|      ! 0 | 11809 |  |
|        - | 11810 | `/*` |
|        - | 11811 | ` * string json_encode(mixed $value [, int $options = 0 ])` |
|        - | 11812 | ` *  Returns a string containing the JSON representation of value.` |
|        - | 11813 | ` * Parameters` |
|        - | 11814 | ` *  $value` |
|        - | 11815 | ` *  The value being encoded. Can be any type except a resource.` |
|        - | 11816 | ` * $options` |
|        - | 11817 | ` *  Bitmask consisting of:` |
|        - | 11818 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|        - | 11819 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|        - | 11820 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|        - | 11821 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|        - | 11822 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|        - | 11823 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|        - | 11824 | ` *  JSON_BIGINT_AS_STRING   Not used` |
|        - | 11825 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|        - | 11826 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|        - | 11827 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|        - | 11828 | ` * Return` |
|        - | 11829 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|        - | 11830 | ` */` |
|        2 | 11831 | `static int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11832 |  |
|        - | 11833 | `	json_private_data sJson;` |
|        3 | 11834 | `	if( nArg < 1 ){` |
|        - | 11835 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 11836 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11837 | `		return PH7_OK;` |
|        - | 11838 | `	}` |
|        - | 11839 | `	/* Prepare the JSON data */` |
|        3 | 11840 | `	sJson.nRecCount = 0;` |
|        3 | 11841 | `	sJson.pCtx = pCtx;` |
|        3 | 11842 | `	sJson.isFirst = 1;` |
|        3 | 11843 | `	sJson.iFlags = 0;` |
|        3 | 11844 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|        - | 11845 | `		/* Extract option flags */` |
|      ! 0 | 11846 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 11847 | `	}` |
|        - | 11848 | `	/* Perform the encoding operation */` |
|        3 | 11849 | `	VmJsonEncode(apArg[0],&sJson);` |
|        - | 11850 | `	/* All done */` |
|        3 | 11851 | `	return PH7_OK;` |
|        2 | 11852 |  |
|        - | 11853 | `/*` |
|        - | 11854 | ` * int json_last_error(void)` |
|        - | 11855 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|        - | 11856 | ` * Parameters` |
|        - | 11857 | ` *  None` |
|        - | 11858 | ` * Return` |
|        - | 11859 | ` *  Returns an integer, the value can be one of the following constants:` |
|        - | 11860 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|        - | 11861 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|        - | 11862 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|        - | 11863 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|        - | 11864 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|        - | 11865 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|        - | 11866 | ` */` |
|        8 | 11867 | `static int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11868 |  |
|       10 | 11869 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11870 | `	/* Return the error code */` |
|       10 | 11871 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|        4 | 11872 | `	SXUNUSED(nArg); /* cc warning */` |
|        4 | 11873 | `	SXUNUSED(apArg);` |
|       10 | 11874 | `	return PH7_OK;` |
|        2 | 11875 |  |
|        - | 11876 | `/* Possible tokens from the JSON tokenization process */` |
|        - | 11877 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|        - | 11878 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|        - | 11879 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|        - | 11880 | `#define JSON_TK_NULL    0x008 /* null */` |
|        - | 11881 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|        - | 11882 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|        - | 11883 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|        - | 11884 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|        - | 11885 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|        - | 11886 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|        - | 11887 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|        - | 11888 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|        - | 11889 | `/*` |
|        - | 11890 | ` * Tokenize an entire JSON input.` |
|        - | 11891 | ` * Get a single low-level token from the input file.` |
|        - | 11892 | ` * Update the stream pointer so that it points to the first` |
|        - | 11893 | ` * character beyond the extracted token.` |
|        - | 11894 | ` */` |
|       60 | 11895 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        2 | 11896 |  |
|       62 | 11897 | `	int *pJsonErr = (int *)pUserData;` |
|        - | 11898 | `	SyString *pStr;` |
|        - | 11899 | `	int c;` |
|        - | 11900 | `	/* Ignore leading white spaces */` |
|       66 | 11901 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - | 11902 | `		/* Advance the stream cursor */` |
|        6 | 11903 | `		if( pStream->zText[0] == '\n' ){` |
|        - | 11904 | `			/* Update line counter */` |
|      ! 0 | 11905 | `			pStream->nLine++;` |
|      ! 0 | 11906 | `		}` |
|        6 | 11907 | `		pStream->zText++;` |
|        2 | 11908 | `	}` |
|       62 | 11909 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - | 11910 | `		/* End of input reached */` |
|      ! 0 | 11911 | `		SXUNUSED(pCtxData); /* cc warning */` |
|      ! 0 | 11912 | `		return SXERR_EOF;` |
|        - | 11913 | `	}` |
|        - | 11914 | `	/* Record token starting position and line */` |
|       62 | 11915 | `	pToken->nLine = pStream->nLine;` |
|       62 | 11916 | `	pToken->pUserData = 0;` |
|       62 | 11917 | `	pStr = &pToken->sData;` |
|       62 | 11918 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|       77 | 11919 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|       44 | 11920 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|        - | 11921 | `			/* Single character */` |
|       36 | 11922 | `			c = pStream->zText[0];` |
|        - | 11923 | `			/* Set token type */` |
|       36 | 11924 | `			switch(c){` |
|        5 | 11925 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|       10 | 11926 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|        6 | 11927 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|        5 | 11928 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|        8 | 11929 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|        9 | 11930 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|      ! 0 | 11931 | `			default:` |
|      ! 0 | 11932 | `				break;` |
|        - | 11933 | `			}` |
|        - | 11934 | `			/* Advance the stream cursor */` |
|       36 | 11935 | `			pStream->zText++;` |
|       45 | 11936 | `	}else if( pStream->zText[0] == '"') {` |
|        - | 11937 | `		/* JSON string */` |
|       10 | 11938 | `		pStream->zText++;` |
|       10 | 11939 | `		pStr->zString++;` |
|        - | 11940 | `		/* Delimit the string */` |
|       32 | 11941 | `		while( pStream->zText < pStream->zEnd ){` |
|       32 | 11942 | `			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){` |
|       10 | 11943 | `				break;` |
|        - | 11944 | `			}` |
|       24 | 11945 | `			if( pStream->zText[0] == '\n' ){` |
|        - | 11946 | `				/* Update line counter */` |
|      ! 0 | 11947 | `				pStream->nLine++;` |
|      ! 0 | 11948 | `			}` |
|       24 | 11949 | `			pStream->zText++;` |
|        2 | 11950 | `		}` |
|       10 | 11951 | `		if( pStream->zText >= pStream->zEnd ){` |
|        - | 11952 | `			/* Missing closing '"' */` |
|      ! 0 | 11953 | `			pToken->nType = JSON_TK_INVALID;` |
|      ! 0 | 11954 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 11955 | `		}else{` |
|       10 | 11956 | `			pToken->nType = JSON_TK_STR;` |
|       10 | 11957 | `			pStream->zText++; /* Jump the closing double quotes */` |
|        2 | 11958 | `		}` |
|       24 | 11959 | `	}else if( pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|        - | 11960 | `		/* Number */` |
|       13 | 11961 | `		pStream->zText++;` |
|       13 | 11962 | `		pToken->nType = JSON_TK_NUM;` |
|       13 | 11963 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11964 | `			pStream->zText++;` |
|      ! 0 | 11965 | `		}` |
|       13 | 11966 | `		if( pStream->zText < pStream->zEnd ){` |
|       13 | 11967 | `			c = pStream->zText[0];` |
|       13 | 11968 | `			if( c == '.' ){` |
|        - | 11969 | `					/* Real number */` |
|      ! 0 | 11970 | `					pStream->zText++;` |
|      ! 0 | 11971 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11972 | `						pStream->zText++;` |
|      ! 0 | 11973 | `					}` |
|      ! 0 | 11974 | `					if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11975 | `						c = pStream->zText[0];` |
|      ! 0 | 11976 | `						if( c=='e' \|\| c=='E' ){` |
|      ! 0 | 11977 | `							pStream->zText++;` |
|      ! 0 | 11978 | `							if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11979 | `								c = pStream->zText[0];` |
|      ! 0 | 11980 | `								if( c =='+' \|\| c=='-' ){` |
|      ! 0 | 11981 | `									pStream->zText++;` |
|      ! 0 | 11982 | `								}` |
|      ! 0 | 11983 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11984 | `									pStream->zText++;` |
|      ! 0 | 11985 | `								}` |
|      ! 0 | 11986 | `							}` |
|      ! 0 | 11987 | `						}` |
|      ! 0 | 11988 | `					}` |
|       13 | 11989 | `				}else if( c=='e' \|\| c=='E' ){` |
|        - | 11990 | `					/* Real number */` |
|      ! 0 | 11991 | `					pStream->zText++;` |
|      ! 0 | 11992 | `					if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11993 | `						c = pStream->zText[0];` |
|      ! 0 | 11994 | `						if( c =='+' \|\| c=='-' ){` |
|      ! 0 | 11995 | `							pStream->zText++;` |
|      ! 0 | 11996 | `						}` |
|      ! 0 | 11997 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11998 | `							pStream->zText++;` |
|      ! 0 | 11999 | `						}` |
|      ! 0 | 12000 | `					}` |
|      ! 0 | 12001 | `				}` |
|        7 | 12002 | `			}` |
|       17 | 12003 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|        6 | 12004 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|        - | 12005 | `			/* boolean true */` |
|      ! 0 | 12006 | `			pToken->nType = JSON_TK_TRUE;` |
|        - | 12007 | `			/* Advance the stream cursor */` |
|      ! 0 | 12008 | `			pStream->zText += sizeof("true")-1;` |
|       11 | 12009 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|        6 | 12010 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|        - | 12011 | `			/* boolean false */` |
|      ! 0 | 12012 | `			pToken->nType = JSON_TK_FALSE;` |
|        - | 12013 | `			/* Advance the stream cursor */` |
|      ! 0 | 12014 | `			pStream->zText += sizeof("false")-1;` |
|       11 | 12015 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|        6 | 12016 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|        - | 12017 | `			/* NULL */` |
|      ! 0 | 12018 | `			pToken->nType = JSON_TK_NULL;` |
|        - | 12019 | `			/* Advance the stream cursor */` |
|      ! 0 | 12020 | `			pStream->zText += sizeof("null")-1;` |
|      ! 0 | 12021 | `	}else{` |
|        - | 12022 | `		/* Unexpected token */` |
|        8 | 12023 | `		pToken->nType = JSON_TK_INVALID;` |
|        - | 12024 | `		/* Advance the stream cursor */` |
|        8 | 12025 | `		pStream->zText++;` |
|        8 | 12026 | `		*pJsonErr = JSON_ERROR_SYNTAX;` |
|        - | 12027 | `		/* Abort processing immediatley */` |
|        8 | 12028 | `		return SXERR_ABORT;` |
|        - | 12029 | `	}` |
|        - | 12030 | `	/* record token length */` |
|       56 | 12031 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|       56 | 12032 | `	if( pToken->nType == JSON_TK_STR ){` |
|       10 | 12033 | `		pStr->nByte--;` |
|        4 | 12034 | `	}` |
|        - | 12035 | `	/* Return to the lexer */` |
|       56 | 12036 | `	return SXRET_OK;` |
|       32 | 12037 |  |
|        - | 12038 | `/*` |
|        - | 12039 | ` * JSON decoded input consumer callback signature.` |
|        - | 12040 | ` */` |
|        - | 12041 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|        - | 12042 | `/*` |
|        - | 12043 | ` * JSON decoder state is kept in the following structure.` |
|        - | 12044 | ` */` |
|        - | 12045 | `typedef struct json_decoder json_decoder;` |
|        - | 12046 | `struct json_decoder` |
|        - | 12047 |  |
|        - | 12048 | `	ph7_context *pCtx; /* Call context */` |
|        - | 12049 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|        - | 12050 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|        - | 12051 | `	int iFlags;        /* Configuration flags */` |
|        - | 12052 | `	SyToken *pIn;      /* Token stream */` |
|        - | 12053 | `	SyToken *pEnd;     /* End of the token stream */` |
|        - | 12054 | `	int rec_depth;     /* Recursion limit */` |
|        - | 12055 | `	int rec_count;     /* Current nesting level */` |
|        - | 12056 | `	int *pErr;         /* JSON decoding error if any */` |
|        - | 12057 | `};` |
|        - | 12058 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|        - | 12059 | `/* Forward declaration */` |
|        - | 12060 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|        - | 12061 | `/*` |
|        - | 12062 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|        - | 12063 | ` * the result in the given ph7_value.` |
|        - | 12064 | ` */` |
|        8 | 12065 | `static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)` |
|        2 | 12066 |  |
|       10 | 12067 | `	const char *zIn = pStr->zString;` |
|       10 | 12068 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|        - | 12069 | `	const char *zCur;` |
|        - | 12070 | `	int c;` |
|        - | 12071 | `	/* Mark the value as a string */` |
|       10 | 12072 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|        4 | 12073 | `	for(;;){` |
|       10 | 12074 | `		zCur = zIn;` |
|       32 | 12075 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|       24 | 12076 | `			zIn++;` |
|        2 | 12077 | `		}` |
|       10 | 12078 | `		if( zIn > zCur ){` |
|        - | 12079 | `			/* Append chunk verbatim */` |
|       10 | 12080 | `			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));` |
|        4 | 12081 | `		}` |
|       10 | 12082 | `		zIn++;` |
|       10 | 12083 | `		if( zIn >= zEnd ){` |
|        - | 12084 | `			/* End of the input reached */` |
|       10 | 12085 | `			break;` |
|        - | 12086 | `		}` |
|      ! 0 | 12087 | `		c = zIn[0];` |
|        - | 12088 | `		/* Unescape the character */` |
|      ! 0 | 12089 | `		switch(c){` |
|      ! 0 | 12090 | `		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|      ! 0 | 12091 | `		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|      ! 0 | 12092 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|      ! 0 | 12093 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|      ! 0 | 12094 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|      ! 0 | 12095 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|      ! 0 | 12096 | `		default:` |
|      ! 0 | 12097 | `			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));` |
|      ! 0 | 12098 | `			break;` |
|        - | 12099 | `		}` |
|        - | 12100 | `		/* Advance the stream cursor */` |
|      ! 0 | 12101 | `		zIn++;` |
|      ! 0 | 12102 | `	}` |
|       10 | 12103 |  |
|        - | 12104 | `/*` |
|        - | 12105 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|        - | 12106 | ` * According to wikipedia` |
|        - | 12107 | ` * JSON's basic types are:` |
|        - | 12108 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|        - | 12109 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|        - | 12110 | ` *   Boolean (true or false)` |
|        - | 12111 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|        - | 12112 | ` *    do not need to be of the same type)` |
|        - | 12113 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|        - | 12114 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|        - | 12115 | ` *     be distinct from each other)` |
|        - | 12116 | ` *   null (empty)` |
|        - | 12117 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|        - | 12118 | ` */` |
|       24 | 12119 | `static sxi32 VmJsonDecode(` |
|        - | 12120 | `	json_decoder *pDecoder, /* JSON decoder */` |
|        - | 12121 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|        2 | 12122 | `	){` |
|        - | 12123 | `	ph7_value *pWorker; /* Worker variable */` |
|        - | 12124 | `	sxi32 rc;` |
|        - | 12125 | `	/* Check if we do not nest to much */` |
|       26 | 12126 | `	if( pDecoder->rec_count >= pDecoder->rec_depth ){` |
|        - | 12127 | `		/* Nesting limit reached,abort decoding immediately */` |
|      ! 0 | 12128 | `		*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|      ! 0 | 12129 | `		return SXERR_ABORT;` |
|        - | 12130 | `	}` |
|       26 | 12131 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|        - | 12132 | `		/* Scalar value */` |
|       16 | 12133 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|       16 | 12134 | `		if( pWorker == 0 ){` |
|      ! 0 | 12135 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12136 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 12137 | `			return SXERR_ABORT;` |
|        - | 12138 | `		}` |
|        - | 12139 | `		/* Reflect the JSON image */` |
|       16 | 12140 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|        - | 12141 | `			/* Nullify the value.*/` |
|      ! 0 | 12142 | `			ph7_value_null(pWorker);` |
|       16 | 12143 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|        - | 12144 | `			/* Boolean value */` |
|      ! 0 | 12145 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|       16 | 12146 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|       13 | 12147 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|        - | 12148 | `			/*` |
|        - | 12149 | `			 * Numeric value.` |
|        - | 12150 | `			 * Get a string representation first then try to get a numeric` |
|        - | 12151 | `			 * value.` |
|        - | 12152 | `			 */` |
|       13 | 12153 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|        - | 12154 | `			/* Obtain a numeric representation */` |
|       13 | 12155 | `			PH7_MemObjToNumeric(pWorker);` |
|        7 | 12156 | `		}else{` |
|        - | 12157 | `			/* Dequote the string */` |
|        3 | 12158 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);` |
|        - | 12159 | `		}` |
|        - | 12160 | `		/* Invoke the consumer callback */` |
|       16 | 12161 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|       16 | 12162 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12163 | `			return SXERR_ABORT;` |
|        - | 12164 | `		}` |
|        - | 12165 | `		/* All done,advance the stream cursor */` |
|       16 | 12166 | `		pDecoder->pIn++;` |
|       19 | 12167 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|        - | 12168 | `		ProcJsonConsumer xOld;` |
|        - | 12169 | `		void *pOld;` |
|        - | 12170 | `		/* Array representation*/` |
|        5 | 12171 | `		pDecoder->pIn++;` |
|        - | 12172 | `		/* Create a working array */` |
|        5 | 12173 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|        5 | 12174 | `		if( pWorker == 0 ){` |
|      ! 0 | 12175 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12176 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 12177 | `			return SXERR_ABORT;` |
|        - | 12178 | `		}` |
|        - | 12179 | `		/* Save the old consumer */` |
|        5 | 12180 | `		xOld = pDecoder->xConsumer;` |
|        5 | 12181 | `		pOld = pDecoder->pUserData;` |
|        - | 12182 | `		/* Set the new consumer */` |
|        5 | 12183 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|        5 | 12184 | `		pDecoder->pUserData = pWorker;` |
|        - | 12185 | `		/* Decode the array */` |
|        7 | 12186 | `		for(;;){` |
|        - | 12187 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|        - | 12188 | `			 * do this.` |
|        - | 12189 | `			 */` |
|       21 | 12190 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|        7 | 12191 | `				pDecoder->pIn++;` |
|        1 | 12192 | `			}` |
|       15 | 12193 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CSB) /*']'*/ ){` |
|        5 | 12194 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|        5 | 12195 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|        2 | 12196 | `				}` |
|        5 | 12197 | `				break;` |
|        - | 12198 | `			}` |
|        - | 12199 | `			/* Recurse and decode the entry */` |
|       11 | 12200 | `			pDecoder->rec_count++;` |
|       11 | 12201 | `			rc = VmJsonDecode(pDecoder,0);` |
|       11 | 12202 | `			pDecoder->rec_count--;` |
|       11 | 12203 | `			if( rc == SXERR_ABORT ){` |
|        - | 12204 | `				/* Abort processing immediately */` |
|      ! 0 | 12205 | `				return SXERR_ABORT;` |
|        - | 12206 | `			}` |
|        - | 12207 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|       11 | 12208 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|       10 | 12209 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|        - | 12210 | `					/* Unexpected token,abort immediatley */` |
|      ! 0 | 12211 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 12212 | `					return SXERR_ABORT;` |
|        - | 12213 | `			}` |
|        1 | 12214 | `		}` |
|        - | 12215 | `		/* Restore the old consumer */` |
|        5 | 12216 | `		pDecoder->xConsumer = xOld;` |
|        5 | 12217 | `		pDecoder->pUserData = pOld;` |
|        - | 12218 | `		/* Invoke the old consumer on the decoded array */` |
|        5 | 12219 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|       10 | 12220 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|        - | 12221 | `		ProcJsonConsumer xOld;` |
|        - | 12222 | `		ph7_value *pKey;` |
|        - | 12223 | `		void *pOld;` |
|        - | 12224 | `		/* Object representation*/` |
|        8 | 12225 | `		pDecoder->pIn++;` |
|        - | 12226 | `		/* Return the object as an associative array */` |
|        8 | 12227 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|        3 | 12228 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_WARNING,` |
|        - | 12229 | `				"JSON Objects are always returned as an associative array"` |
|        - | 12230 | `				);` |
|        1 | 12231 | `		}` |
|        - | 12232 | `		/* Create a working array */` |
|        8 | 12233 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|        8 | 12234 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|        8 | 12235 | `		if( pWorker == 0 \|\| pKey == 0){` |
|      ! 0 | 12236 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12237 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 12238 | `			return SXERR_ABORT;` |
|        - | 12239 | `		}` |
|        - | 12240 | `		/* Save the old consumer */` |
|        8 | 12241 | `		xOld = pDecoder->xConsumer;` |
|        8 | 12242 | `		pOld = pDecoder->pUserData;` |
|        - | 12243 | `		/* Set the new consumer */` |
|        8 | 12244 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|        8 | 12245 | `		pDecoder->pUserData = pWorker;` |
|        - | 12246 | `		/* Decode the object */` |
|        6 | 12247 | `		for(;;){` |
|        - | 12248 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|        - | 12249 | `			 * do this.` |
|        - | 12250 | `			 */` |
|       16 | 12251 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|        3 | 12252 | `				pDecoder->pIn++;` |
|        1 | 12253 | `			}` |
|       14 | 12254 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CCB) /*'}'*/ ){` |
|        8 | 12255 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|        6 | 12256 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|        2 | 12257 | `				}` |
|        8 | 12258 | `				break;` |
|        - | 12259 | `			}` |
|        6 | 12260 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|        8 | 12261 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|        - | 12262 | `					/* Syntax error,return immediately */` |
|      ! 0 | 12263 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 12264 | `					return SXERR_ABORT;` |
|        - | 12265 | `			}` |
|        - | 12266 | `			/* Dequote the key */` |
|        8 | 12267 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);` |
|        - | 12268 | `			/* Jump the key and the colon */` |
|        8 | 12269 | `			pDecoder->pIn += 2;` |
|        - | 12270 | `			/* Recurse and decode the value */` |
|        8 | 12271 | `			pDecoder->rec_count++;` |
|        8 | 12272 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|        8 | 12273 | `			pDecoder->rec_count--;` |
|        8 | 12274 | `			if( rc == SXERR_ABORT ){` |
|        - | 12275 | `				/* Abort processing immediately */` |
|      ! 0 | 12276 | `				return SXERR_ABORT;` |
|        - | 12277 | `			}` |
|        - | 12278 | `			/* Reset the internal buffer of the key */` |
|        8 | 12279 | `			ph7_value_reset_string_cursor(pKey);` |
|        - | 12280 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|        2 | 12281 | `		}` |
|        - | 12282 | `		/* Restore the old consumer */` |
|        8 | 12283 | `		pDecoder->xConsumer = xOld;` |
|        8 | 12284 | `		pDecoder->pUserData = pOld;` |
|        - | 12285 | `		/* Invoke the old consumer on the decoded object*/` |
|        8 | 12286 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|        - | 12287 | `		/* Release the key */` |
|        8 | 12288 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|        5 | 12289 | `	}else{` |
|        - | 12290 | `		/* Unexpected token */` |
|      ! 0 | 12291 | `		return SXERR_ABORT; /* Abort immediately */` |
|        - | 12292 | `	}` |
|        - | 12293 | `	/* Release the worker variable */` |
|       26 | 12294 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|       26 | 12295 | `	return SXRET_OK;` |
|       14 | 12296 |  |
|        - | 12297 | `/*` |
|        - | 12298 | ` * The following JSON decoder callback is invoked each time` |
|        - | 12299 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|        - | 12300 | ` * is being decoded.` |
|        - | 12301 | ` */` |
|       16 | 12302 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|        2 | 12303 |  |
|       18 | 12304 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 12305 | `	/* Insert the entry */` |
|       18 | 12306 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|        8 | 12307 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 12308 | `	/* All done */` |
|       18 | 12309 | `	return SXRET_OK;` |
|        2 | 12310 |  |
|        - | 12311 | `/*` |
|        - | 12312 | ` * Standard JSON decoder callback.` |
|        - | 12313 | ` */` |
|        8 | 12314 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|        2 | 12315 |  |
|        - | 12316 | `	/* Return the value directly */` |
|       10 | 12317 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|        4 | 12318 | `	SXUNUSED(pKey); /* cc warning */` |
|        4 | 12319 | `	SXUNUSED(pUserData);` |
|        - | 12320 | `	/* All done */` |
|       10 | 12321 | `	return SXRET_OK;` |
|        2 | 12322 |  |
|        - | 12323 | `/*` |
|        - | 12324 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])` |
|        - | 12325 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|        - | 12326 | ` * Parameters` |
|        - | 12327 | ` *  $json` |
|        - | 12328 | ` *    The json string being decoded.` |
|        - | 12329 | ` * $assoc` |
|        - | 12330 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|        - | 12331 | ` * $depth` |
|        - | 12332 | ` *   User specified recursion depth.` |
|        - | 12333 | ` * $options` |
|        - | 12334 | ` *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported` |
|        - | 12335 | ` * (default is to cast large integers as floats)` |
|        - | 12336 | ` * Return` |
|        - | 12337 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|        - | 12338 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|        - | 12339 | ` *  or if the encoded data is deeper than the recursion limit.` |
|        - | 12340 | ` */` |
|       16 | 12341 | `static int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12342 |  |
|       18 | 12343 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12344 | `	json_decoder sDecoder;` |
|        - | 12345 | `	const char *zIn;` |
|        - | 12346 | `	SySet sToken;` |
|        - | 12347 | `	SyLex sLex;` |
|        - | 12348 | `	int nByte;` |
|        - | 12349 | `	sxi32 rc;` |
|       18 | 12350 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12351 | `		/* Missing/Invalid arguments, return NULL */` |
|      ! 0 | 12352 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12353 | `		return PH7_OK;` |
|        - | 12354 | `	}` |
|        - | 12355 | `	/* Extract the JSON string */` |
|       18 | 12356 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|       18 | 12357 | `	if( nByte < 1 ){` |
|        - | 12358 | `		/* Empty string,return NULL */` |
|        3 | 12359 | `		ph7_result_null(pCtx);` |
|        3 | 12360 | `		return PH7_OK;` |
|        - | 12361 | `	}` |
|        - | 12362 | `	/* Clear JSON error code */` |
|       16 | 12363 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - | 12364 | `	/* Tokenize the input */` |
|       16 | 12365 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|       16 | 12366 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|       16 | 12367 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|       16 | 12368 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|        - | 12369 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|        8 | 12370 | `		SyLexRelease(&sLex);` |
|        8 | 12371 | `		SySetRelease(&sToken);` |
|        - | 12372 | `		/* return NULL */` |
|        8 | 12373 | `		ph7_result_null(pCtx);` |
|        8 | 12374 | `		return PH7_OK;` |
|        - | 12375 | `	}` |
|        - | 12376 | `	/* Fill the decoder */` |
|       10 | 12377 | `	sDecoder.pCtx = pCtx;` |
|       10 | 12378 | `	sDecoder.pErr = &pVm->json_rc;` |
|       10 | 12379 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       10 | 12380 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|       10 | 12381 | `	sDecoder.iFlags = 0;` |
|       10 | 12382 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){` |
|        - | 12383 | `		/* Returned objects will be converted into associative arrays */` |
|        8 | 12384 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|        3 | 12385 | `	}` |
|       10 | 12386 | `	sDecoder.rec_depth = 32;` |
|       10 | 12387 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      ! 0 | 12388 | `		int nDepth = ph7_value_to_int(apArg[2]);` |
|      ! 0 | 12389 | `		if( nDepth > 1 && nDepth < 32 ){` |
|      ! 0 | 12390 | `			sDecoder.rec_depth = nDepth;` |
|      ! 0 | 12391 | `		}` |
|      ! 0 | 12392 | `	}` |
|       10 | 12393 | `	sDecoder.rec_count = 0;` |
|        - | 12394 | `	/* Set a default consumer */` |
|       10 | 12395 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|       10 | 12396 | `	sDecoder.pUserData = 0;` |
|        - | 12397 | `	/* Decode the raw JSON input */` |
|       10 | 12398 | `	rc = VmJsonDecode(&sDecoder,0);` |
|       10 | 12399 | `	if( rc == SXERR_ABORT \|\|  pVm->json_rc != JSON_ERROR_NONE ){` |
|        - | 12400 | `		/*` |
|        - | 12401 | `		 * Something goes wrong while decoding JSON input.Return NULL.` |
|        - | 12402 | `		 */` |
|      ! 0 | 12403 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12404 | `	}` |
|        - | 12405 | `	/* Clean-up the mess left behind */` |
|       10 | 12406 | `	SyLexRelease(&sLex);` |
|       10 | 12407 | `	SySetRelease(&sToken);` |
|        - | 12408 | `	/* All done */` |
|       10 | 12409 | `	return PH7_OK;` |
|       10 | 12410 |  |
|        - | 12411 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12412 | `/*` |
|        - | 12413 | ` * XML processing Functions.` |
|        - | 12414 | ` * Status:` |
|        - | 12415 | ` *    Devel.` |
|        - | 12416 | ` */` |
|        - | 12417 | `enum ph7_xml_handler_id{` |
|        - | 12418 | `	PH7_XML_START_TAG = 0, /* Start element handlers ID */` |
|        - | 12419 | `	PH7_XML_END_TAG,       /* End element handler ID*/` |
|        - | 12420 | `	PH7_XML_CDATA,         /* Character data handler ID*/` |
|        - | 12421 | `	PH7_XML_PI,            /* Processing instruction (PI) handler ID*/` |
|        - | 12422 | `	PH7_XML_DEF,           /* Default handler ID */` |
|        - | 12423 | `	PH7_XML_UNPED,         /* Unparsed entity declaration handler */` |
|        - | 12424 | `	PH7_XML_ND,            /* Notation declaration handler ID*/` |
|        - | 12425 | `	PH7_XML_EER,           /* External entity reference handler */` |
|        - | 12426 | `	PH7_XML_NS_START,      /* Start namespace declaration handler */` |
|        - | 12427 | `	PH7_XML_NS_END         /* End namespace declaration handler */` |
|        - | 12428 | `};` |
|        - | 12429 | `#define XML_TOTAL_HANDLER (PH7_XML_NS_END + 1)` |
|        - | 12430 | `/* An instance of the following structure describe a working` |
|        - | 12431 | ` * XML engine instance.` |
|        - | 12432 | ` */` |
|        - | 12433 | `typedef struct ph7_xml_engine ph7_xml_engine;` |
|        - | 12434 | `struct ph7_xml_engine` |
|        - | 12435 |  |
|        - | 12436 | `	ph7_vm *pVm;         /* VM that own this instance */` |
|        - | 12437 | `	ph7_context *pCtx;   /* Call context */` |
|        - | 12438 | `	SyXMLParser sParser; /* Underlying XML parser */` |
|        - | 12439 | `	ph7_value aCB[XML_TOTAL_HANDLER]; /* User-defined callbacks */` |
|        - | 12440 | `	ph7_value sParserValue; /* ph7_value holding this instance which is forwarded` |
|        - | 12441 | `							  * as the first argument to the user callbacks.` |
|        - | 12442 | `							  */` |
|        - | 12443 | `	int ns_sep;      /* Namespace separator */` |
|        - | 12444 | `	SyBlob sErr;     /* Error message consumer */` |
|        - | 12445 | `	sxi32 iErrCode;  /* Last error code */` |
|        - | 12446 | `	sxi32 iNest;     /* Nesting level */` |
|        - | 12447 | `	sxu32 nLine;     /* Last processed line */` |
|        - | 12448 | `	sxu32 nMagic;    /* Magic number so that we avoid misuse  */` |
|        - | 12449 | `};` |
|        - | 12450 | `#define XML_ENGINE_MAGIC 0x851EFC52` |
|        - | 12451 | `#define IS_INVALID_XML_ENGINE(XML) (XML == 0 \|\| (XML)->nMagic != XML_ENGINE_MAGIC)` |
|        - | 12452 | `/*` |
|        - | 12453 | ` * Allocate and initialize an XML engine.` |
|        - | 12454 | ` */` |
|       84 | 12455 | `static ph7_xml_engine * VmCreateXMLEngine(ph7_context *pCtx,int process_ns,int ns_sep)` |
|        1 | 12456 |  |
|        - | 12457 | `	ph7_xml_engine *pEngine;` |
|       85 | 12458 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12459 | `	ph7_value *pValue;` |
|        - | 12460 | `	sxu32 n;` |
|        - | 12461 | `	/* Allocate a new instance */` |
|       85 | 12462 | `	pEngine = (ph7_xml_engine *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_xml_engine));` |
|       85 | 12463 | `	if( pEngine == 0 ){` |
|        - | 12464 | `		/* Out of memory */` |
|      ! 0 | 12465 | `		return 0;` |
|        - | 12466 | `	}` |
|        - | 12467 | `	/* Zero the structure */` |
|       85 | 12468 | `	SyZero(pEngine,sizeof(ph7_xml_engine));` |
|        - | 12469 | `	/* Initialize fields */` |
|       85 | 12470 | `	pEngine->pVm = pVm;` |
|       85 | 12471 | `	pEngine->pCtx = 0;` |
|       85 | 12472 | `	pEngine->ns_sep = ns_sep;` |
|       85 | 12473 | `	SyXMLParserInit(&pEngine->sParser,&pVm->sAllocator,process_ns ? SXML_ENABLE_NAMESPACE : 0);` |
|       85 | 12474 | `	SyBlobInit(&pEngine->sErr,&pVm->sAllocator);` |
|       85 | 12475 | `	PH7_MemObjInit(pVm,&pEngine->sParserValue);` |
|      925 | 12476 | `	for( n = 0 ; n < SX_ARRAYSIZE(pEngine->aCB) ; ++n ){` |
|      841 | 12477 | `		pValue = &pEngine->aCB[n];` |
|        - | 12478 | `		/* NULLIFY the array entries,until someone register an event handler */` |
|      841 | 12479 | `		PH7_MemObjInit(&(*pVm),pValue);` |
|      421 | 12480 | `	}` |
|       85 | 12481 | `	ph7_value_resource(&pEngine->sParserValue,pEngine);` |
|       85 | 12482 | `	pEngine->iErrCode = SXML_ERROR_NONE;` |
|        - | 12483 | `	/* Finally set the magic number */` |
|       85 | 12484 | `	pEngine->nMagic = XML_ENGINE_MAGIC;` |
|       85 | 12485 | `	return pEngine;` |
|       43 | 12486 |  |
|        - | 12487 | `/*` |
|        - | 12488 | ` * Release an XML engine.` |
|        - | 12489 | ` */` |
|       84 | 12490 | `static void VmReleaseXMLEngine(ph7_xml_engine *pEngine)` |
|        1 | 12491 |  |
|       85 | 12492 | `	ph7_vm *pVm = pEngine->pVm;` |
|        - | 12493 | `	ph7_value *pValue;` |
|        - | 12494 | `	sxu32 n;` |
|        - | 12495 | `	/* Release fields */` |
|       85 | 12496 | `	SyBlobRelease(&pEngine->sErr);` |
|       85 | 12497 | `	SyXMLParserRelease(&pEngine->sParser);` |
|       85 | 12498 | `	PH7_MemObjRelease(&pEngine->sParserValue);` |
|      925 | 12499 | `	for( n = 0 ; n < SX_ARRAYSIZE(pEngine->aCB) ; ++n ){` |
|      841 | 12500 | `		pValue = &pEngine->aCB[n];` |
|      841 | 12501 | `		PH7_MemObjRelease(pValue);` |
|      421 | 12502 | `	}` |
|       85 | 12503 | `	pEngine->nMagic = 0x2621;` |
|        - | 12504 | `	/* Finally,release the whole instance */` |
|       85 | 12505 | `	SyMemBackendFree(&pVm->sAllocator,pEngine);` |
|       85 | 12506 |  |
|        - | 12507 | `/*` |
|        - | 12508 | ` * resource xml_parser_create([ string $encoding ])` |
|        - | 12509 | ` *  Create an UTF-8 XML parser.` |
|        - | 12510 | ` * Parameter` |
|        - | 12511 | ` *  $encoding` |
|        - | 12512 | ` *   (Only UTF-8 encoding is used)` |
|        - | 12513 | ` * Return` |
|        - | 12514 | ` *  Returns a resource handle for the new XML parser.` |
|        - | 12515 | ` */` |
|       80 | 12516 | `static int vm_builtin_xml_parser_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12517 |  |
|        - | 12518 | `	ph7_xml_engine *pEngine;` |
|        - | 12519 | `	/* Allocate a new instance */` |
|       81 | 12520 | `	pEngine = VmCreateXMLEngine(&(*pCtx),0,':');` |
|       81 | 12521 | `	if( pEngine == 0 ){` |
|      ! 0 | 12522 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12523 | `		/* Return null */` |
|      ! 0 | 12524 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12525 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12526 | `		SXUNUSED(apArg);` |
|      ! 0 | 12527 | `		return PH7_OK;` |
|        - | 12528 | `	}` |
|        - | 12529 | `	/* Return the engine as a resource */` |
|       81 | 12530 | `	ph7_result_resource(pCtx,pEngine);` |
|       81 | 12531 | `	return PH7_OK;` |
|       41 | 12532 |  |
|        - | 12533 | `/*` |
|        - | 12534 | ` * resource xml_parser_create_ns([ string $encoding[,string $separator = ':']])` |
|        - | 12535 | ` *  Create an UTF-8 XML parser with namespace support.` |
|        - | 12536 | ` * Parameter` |
|        - | 12537 | ` *  $encoding` |
|        - | 12538 | ` *   (Only UTF-8 encoding is supported)` |
|        - | 12539 | ` *  $separtor` |
|        - | 12540 | ` *   Namespace separator (a single character)` |
|        - | 12541 | ` * Return` |
|        - | 12542 | ` *  Returns a resource handle for the new XML parser.` |
|        - | 12543 | ` */` |
|        4 | 12544 | `static int vm_builtin_xml_parser_create_ns(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12545 |  |
|        - | 12546 | `	ph7_xml_engine *pEngine;` |
|        5 | 12547 | `	int ns_sep = ':';` |
|        5 | 12548 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      ! 0 | 12549 | `		const char *zSep = ph7_value_to_string(apArg[1],0);` |
|      ! 0 | 12550 | `		if( zSep[0] != 0 ){` |
|      ! 0 | 12551 | `			ns_sep = zSep[0];` |
|      ! 0 | 12552 | `		}` |
|      ! 0 | 12553 | `	}` |
|        - | 12554 | `	/* Allocate a new instance */` |
|        5 | 12555 | `	pEngine = VmCreateXMLEngine(&(*pCtx),TRUE,ns_sep);` |
|        5 | 12556 | `	if( pEngine == 0 ){` |
|      ! 0 | 12557 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12558 | `		/* Return null */` |
|      ! 0 | 12559 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12560 | `		return PH7_OK;` |
|        - | 12561 | `	}` |
|        - | 12562 | `	/* Return the engine as a resource */` |
|        5 | 12563 | `	ph7_result_resource(pCtx,pEngine);` |
|        5 | 12564 | `	return PH7_OK;` |
|        3 | 12565 |  |
|        - | 12566 | `/*` |
|        - | 12567 | ` * bool xml_parser_free(resource $parser)` |
|        - | 12568 | ` *  Release an XML engine.` |
|        - | 12569 | ` * Parameter` |
|        - | 12570 | ` *  $parser` |
|        - | 12571 | ` *   A reference to the XML parser to free.` |
|        - | 12572 | ` * Return` |
|        - | 12573 | ` *  This function returns FALSE if parser does not refer` |
|        - | 12574 | ` *  to a valid parser, or else it frees the parser and returns TRUE.` |
|        - | 12575 | ` */` |
|       84 | 12576 | `static int vm_builtin_xml_parser_free(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12577 |  |
|        - | 12578 | `	ph7_xml_engine *pEngine;` |
|       85 | 12579 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12580 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12581 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12582 | `		return PH7_OK;` |
|        - | 12583 | `	}` |
|        - | 12584 | `	/* Point to the XML engine */` |
|       85 | 12585 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       85 | 12586 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12587 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12588 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12589 | `		return PH7_OK;` |
|        - | 12590 | `	}` |
|        - | 12591 | `	/* Safely release the engine */` |
|       85 | 12592 | `	VmReleaseXMLEngine(pEngine);` |
|        - | 12593 | `	/* Return TRUE */` |
|       85 | 12594 | `	ph7_result_bool(pCtx,1);` |
|       85 | 12595 | `	return PH7_OK;` |
|       43 | 12596 |  |
|        - | 12597 | `/*` |
|        - | 12598 | ` * bool xml_set_element_handler(resource $parser,callback $start_element_handler,[callback $end_element_handler])` |
|        - | 12599 | ` * Sets the element handler functions for the XML parser. start_element_handler and end_element_handler` |
|        - | 12600 | ` * are strings containing the names of functions.` |
|        - | 12601 | ` * Parameters` |
|        - | 12602 | ` *  $parser` |
|        - | 12603 | ` *   A reference to the XML parser to set up start and end element handler functions.` |
|        - | 12604 | ` *  $start_element_handler` |
|        - | 12605 | ` *    The function named by start_element_handler must accept three parameters:` |
|        - | 12606 | ` *    start_element_handler(resource $parser,string $name,array $attribs)` |
|        - | 12607 | ` *    $parser` |
|        - | 12608 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12609 | ` *   $name` |
|        - | 12610 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 12611 | ` *		is called.If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 12612 | ` *  $attribs` |
|        - | 12613 | ` *      The third parameter, attribs, contains an associative array with the element's attributes (if any).` |
|        - | 12614 | ` *		The keys of this array are the attribute names, the values are the attribute values.` |
|        - | 12615 | ` *      Attribute names are case-folded on the same criteria as element names.Attribute values are not case-folded.` |
|        - | 12616 | ` *      The original order of the attributes can be retrieved by walking through attribs the normal way, using each().` |
|        - | 12617 | ` *      The first key in the array was the first attribute, and so on.` |
|        - | 12618 | ` *      Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 12619 | ` * $end_element_handler` |
|        - | 12620 | ` *     The function named by end_element_handler must accept two parameters:` |
|        - | 12621 | ` *     end_element_handler(resource $parser,string $name)` |
|        - | 12622 | ` *    $parser` |
|        - | 12623 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12624 | ` *   $name` |
|        - | 12625 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 12626 | ` *      is called.If case-folding is in effect for this parser, the element name will be in uppercase` |
|        - | 12627 | ` *      letters.` |
|        - | 12628 | ` *      If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 12629 | ` * Return` |
|        - | 12630 | ` * TRUE on success or FALSE on failure.` |
|        - | 12631 | ` */` |
|       66 | 12632 | `static int vm_builtin_xml_set_element_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12633 |  |
|        - | 12634 | `	ph7_xml_engine *pEngine;` |
|       67 | 12635 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12636 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12637 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12638 | `		return PH7_OK;` |
|        - | 12639 | `	}` |
|        - | 12640 | `	/* Point to the XML engine */` |
|       67 | 12641 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       67 | 12642 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12643 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12644 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12645 | `		return PH7_OK;` |
|        - | 12646 | `	}` |
|       67 | 12647 | `	if( nArg > 1 ){` |
|        - | 12648 | `		/* Save the start_element_handler callback for later invocation */` |
|       67 | 12649 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_START_TAG]);` |
|       67 | 12650 | `		if( nArg > 2 ){` |
|        - | 12651 | `			/* Save the end_element_handler callback for later invocation */` |
|       67 | 12652 | `			PH7_MemObjStore(apArg[2]/* User callback*/,&pEngine->aCB[PH7_XML_END_TAG]);` |
|       33 | 12653 | `		}` |
|       33 | 12654 | `	}` |
|        - | 12655 | `	/* All done,return TRUE */` |
|       67 | 12656 | `	ph7_result_bool(pCtx,1);` |
|       67 | 12657 | `	return PH7_OK;` |
|       34 | 12658 |  |
|        - | 12659 | `/*` |
|        - | 12660 | ` * bool xml_set_character_data_handler(resource $parser,callback $handler)` |
|        - | 12661 | ` *  Sets the character data handler function for the XML parser parser.` |
|        - | 12662 | ` * Parameters` |
|        - | 12663 | ` * $parser` |
|        - | 12664 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12665 | ` * $handler` |
|        - | 12666 | ` *  handler is a string containing the name of the callback.` |
|        - | 12667 | ` *  The function named by handler must accept two parameters:` |
|        - | 12668 | ` *   handler(resource $parser,string $data)` |
|        - | 12669 | ` *  $parser` |
|        - | 12670 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12671 | ` *  $data` |
|        - | 12672 | ` *   The second parameter, data, contains the character data as a string.` |
|        - | 12673 | ` *   Character data handler is called for every piece of a text in the XML document.` |
|        - | 12674 | ` *   It can be called multiple times inside each fragment (e.g. for non-ASCII strings).` |
|        - | 12675 | ` *   If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 12676 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12677 | ` *   can also be supplied.` |
|        - | 12678 | ` * Return` |
|        - | 12679 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12680 | ` */` |
|       40 | 12681 | `static int vm_builtin_xml_set_character_data_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12682 |  |
|        - | 12683 | `	ph7_xml_engine *pEngine;` |
|       41 | 12684 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12685 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12686 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12687 | `		return PH7_OK;` |
|        - | 12688 | `	}` |
|        - | 12689 | `	/* Point to the XML engine */` |
|       41 | 12690 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       41 | 12691 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12692 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12693 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12694 | `		return PH7_OK;` |
|        - | 12695 | `	}` |
|       41 | 12696 | `	if( nArg > 1 ){` |
|        - | 12697 | `		/* Save the user callback for later invocation */` |
|       41 | 12698 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_CDATA]);` |
|       20 | 12699 | `	}` |
|        - | 12700 | `	/* All done,return TRUE */` |
|       41 | 12701 | `	ph7_result_bool(pCtx,1);` |
|       41 | 12702 | `	return PH7_OK;` |
|       21 | 12703 |  |
|        - | 12704 | `/*` |
|        - | 12705 | ` * bool xml_set_default_handler(resource $parser,callback $handler)` |
|        - | 12706 | ` *  Set up default handler.` |
|        - | 12707 | ` * Parameters` |
|        - | 12708 | ` * $parser` |
|        - | 12709 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12710 | ` * $handler` |
|        - | 12711 | ` *  handler is a string containing the name of the callback.` |
|        - | 12712 | ` *  The function named by handler must accept two parameters:` |
|        - | 12713 | ` *   handler(resource $parser,string $data)` |
|        - | 12714 | ` *  $parser` |
|        - | 12715 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12716 | ` *  $data` |
|        - | 12717 | ` *   The second parameter, data, contains the character data.This may be the XML declaration` |
|        - | 12718 | ` *   document type declaration, entities or other data for which no other handler exists.` |
|        - | 12719 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12720 | ` *   can also be supplied.` |
|        - | 12721 | ` * Return` |
|        - | 12722 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12723 | ` */` |
|        2 | 12724 | `static int vm_builtin_xml_set_default_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12725 |  |
|        - | 12726 | `	ph7_xml_engine *pEngine;` |
|        3 | 12727 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12728 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12729 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12730 | `		return PH7_OK;` |
|        - | 12731 | `	}` |
|        - | 12732 | `	/* Point to the XML engine */` |
|        3 | 12733 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12734 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12735 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12736 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12737 | `		return PH7_OK;` |
|        - | 12738 | `	}` |
|        3 | 12739 | `	if( nArg > 1 ){` |
|        - | 12740 | `		/* Save the user callback for later invocation */` |
|        3 | 12741 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_DEF]);` |
|        1 | 12742 | `	}` |
|        - | 12743 | `	/* All done,return TRUE */` |
|        3 | 12744 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12745 | `	return PH7_OK;` |
|        2 | 12746 |  |
|        - | 12747 | `/*` |
|        - | 12748 | ` * bool xml_set_end_namespace_decl_handler(resource $parser,callback $handler)` |
|        - | 12749 | ` *  Set up end namespace declaration handler.` |
|        - | 12750 | ` * Parameters` |
|        - | 12751 | ` * $parser` |
|        - | 12752 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12753 | ` * $handler` |
|        - | 12754 | ` *  handler is a string containing the name of the callback.` |
|        - | 12755 | ` *  The function named by handler must accept two parameters:` |
|        - | 12756 | ` *   handler(resource $parser,string $prefix)` |
|        - | 12757 | ` *  $parser` |
|        - | 12758 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12759 | ` *  $prefix` |
|        - | 12760 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 12761 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12762 | ` *   can also be supplied.` |
|        - | 12763 | ` * Return` |
|        - | 12764 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12765 | ` */` |
|        2 | 12766 | `static int vm_builtin_xml_set_end_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12767 |  |
|        - | 12768 | `	ph7_xml_engine *pEngine;` |
|        3 | 12769 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12770 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12771 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12772 | `		return PH7_OK;` |
|        - | 12773 | `	}` |
|        - | 12774 | `	/* Point to the XML engine */` |
|        3 | 12775 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12776 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12777 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12778 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12779 | `		return PH7_OK;` |
|        - | 12780 | `	}` |
|        3 | 12781 | `	if( nArg > 1 ){` |
|        - | 12782 | `		/* Save the user callback for later invocation */` |
|        3 | 12783 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_NS_END]);` |
|        1 | 12784 | `	}` |
|        - | 12785 | `	/* All done,return TRUE */` |
|        3 | 12786 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12787 | `	return PH7_OK;` |
|        2 | 12788 |  |
|        - | 12789 | `/*` |
|        - | 12790 | ` * bool xml_set_start_namespace_decl_handler(resource $parser,callback $handler)` |
|        - | 12791 | ` *  Set up start namespace declaration handler.` |
|        - | 12792 | ` * Parameters` |
|        - | 12793 | ` * $parser` |
|        - | 12794 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12795 | ` * $handler` |
|        - | 12796 | ` *  handler is a string containing the name of the callback.` |
|        - | 12797 | ` *  The function named by handler must accept two parameters:` |
|        - | 12798 | ` *   handler(resource $parser,string $prefix,string $uri)` |
|        - | 12799 | ` *  $parser` |
|        - | 12800 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12801 | ` *  $prefix` |
|        - | 12802 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 12803 | ` *  $uri` |
|        - | 12804 | ` *    Uniform Resource Identifier (URI) of namespace.` |
|        - | 12805 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12806 | ` *   can also be supplied.` |
|        - | 12807 | ` * Return` |
|        - | 12808 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12809 | ` */` |
|        2 | 12810 | `static int vm_builtin_xml_set_start_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12811 |  |
|        - | 12812 | `	ph7_xml_engine *pEngine;` |
|        3 | 12813 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12814 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12815 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12816 | `		return PH7_OK;` |
|        - | 12817 | `	}` |
|        - | 12818 | `	/* Point to the XML engine */` |
|        3 | 12819 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12820 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12821 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12822 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12823 | `		return PH7_OK;` |
|        - | 12824 | `	}` |
|        3 | 12825 | `	if( nArg > 1 ){` |
|        - | 12826 | `		/* Save the user callback for later invocation */` |
|        3 | 12827 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_NS_START]);` |
|        1 | 12828 | `	}` |
|        - | 12829 | `	/* All done,return TRUE */` |
|        3 | 12830 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12831 | `	return PH7_OK;` |
|        2 | 12832 |  |
|        - | 12833 | `/*` |
|        - | 12834 | ` * bool xml_set_processing_instruction_handler(resource $parser,callback $handler)` |
|        - | 12835 | ` *  Set up processing instruction (PI) handler.` |
|        - | 12836 | ` * Parameters` |
|        - | 12837 | ` * $parser` |
|        - | 12838 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12839 | ` * $handler` |
|        - | 12840 | ` *  handler is a string containing the name of the callback.` |
|        - | 12841 | ` *  The function named by handler must accept three parameters:` |
|        - | 12842 | ` *   handler(resource $parser,string $target,string $data)` |
|        - | 12843 | ` *  $parser` |
|        - | 12844 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12845 | ` *  $target` |
|        - | 12846 | ` *   The second parameter, target, contains the PI target.` |
|        - | 12847 | ` *  $data` |
|        - | 12848 | `     The third parameter, data, contains the PI data.` |
|        - | 12849 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12850 | ` *   can also be supplied.` |
|        - | 12851 | ` * Return` |
|        - | 12852 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12853 | ` */` |
|        8 | 12854 | `static int vm_builtin_xml_set_processing_instruction_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12855 |  |
|        - | 12856 | `	ph7_xml_engine *pEngine;` |
|        9 | 12857 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12858 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12859 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12860 | `		return PH7_OK;` |
|        - | 12861 | `	}` |
|        - | 12862 | `	/* Point to the XML engine */` |
|        9 | 12863 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        9 | 12864 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12865 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12866 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12867 | `		return PH7_OK;` |
|        - | 12868 | `	}` |
|        9 | 12869 | `	if( nArg > 1 ){` |
|        - | 12870 | `		/* Save the user callback for later invocation */` |
|        9 | 12871 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_PI]);` |
|        4 | 12872 | `	}` |
|        - | 12873 | `	/* All done,return TRUE */` |
|        9 | 12874 | `	ph7_result_bool(pCtx,1);` |
|        9 | 12875 | `	return PH7_OK;` |
|        5 | 12876 |  |
|        - | 12877 | `/*` |
|        - | 12878 | ` * bool xml_set_unparsed_entity_decl_handler(resource $parser,callback $handler)` |
|        - | 12879 | ` *  Set up unparsed entity declaration handler.` |
|        - | 12880 | ` * Parameters` |
|        - | 12881 | ` * $parser` |
|        - | 12882 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12883 | ` * $handler` |
|        - | 12884 | ` *  handler is a string containing the name of the callback.` |
|        - | 12885 | ` *  The function named by handler must accept six parameters:` |
|        - | 12886 | ` *  handler(resource $parser,string $entity_name,string $base,string $system_id,string $public_id,string $notation_name)` |
|        - | 12887 | ` *  $parser` |
|        - | 12888 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12889 | ` *  $entity_name` |
|        - | 12890 | ` *   The name of the entity that is about to be defined.` |
|        - | 12891 | ` *  $base` |
|        - | 12892 | ` *   This is the base for resolving the system identifier (systemId) of the external entity.` |
|        - | 12893 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12894 | ` *  $system_id` |
|        - | 12895 | ` *   System identifier for the external entity.` |
|        - | 12896 | ` *  $public_id` |
|        - | 12897 | ` *    Public identifier for the external entity.` |
|        - | 12898 | ` *  $notation_name` |
|        - | 12899 | ` *    Name of the notation of this entity (see xml_set_notation_decl_handler()).` |
|        - | 12900 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12901 | ` *   can also be supplied.` |
|        - | 12902 | ` * Return` |
|        - | 12903 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12904 | ` */` |
|        2 | 12905 | `static int vm_builtin_xml_set_unparsed_entity_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12906 |  |
|        - | 12907 | `	ph7_xml_engine *pEngine;` |
|        3 | 12908 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12909 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12910 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12911 | `		return PH7_OK;` |
|        - | 12912 | `	}` |
|        - | 12913 | `	/* Point to the XML engine */` |
|        3 | 12914 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12915 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12916 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12917 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12918 | `		return PH7_OK;` |
|        - | 12919 | `	}` |
|        3 | 12920 | `	if( nArg > 1 ){` |
|        - | 12921 | `		/* Save the user callback for later invocation */` |
|        3 | 12922 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_UNPED]);` |
|        1 | 12923 | `	}` |
|        - | 12924 | `	/* All done,return TRUE */` |
|        3 | 12925 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12926 | `	return PH7_OK;` |
|        2 | 12927 |  |
|        - | 12928 | `/*` |
|        - | 12929 | ` * bool xml_set_notation_decl_handler(resource $parser,callback $handler)` |
|        - | 12930 | ` *  Set up notation declaration handler.` |
|        - | 12931 | ` * Parameters` |
|        - | 12932 | ` * $parser` |
|        - | 12933 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12934 | ` * $handler` |
|        - | 12935 | ` *  handler is a string containing the name of the callback.` |
|        - | 12936 | ` *  The function named by handler must accept five parameters:` |
|        - | 12937 | ` *  handler(resource $parser,string $entity_name,string $base,string $system_id,string $public_id)` |
|        - | 12938 | ` *  $parser` |
|        - | 12939 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12940 | ` *  $entity_name` |
|        - | 12941 | ` *   The name of the entity that is about to be defined.` |
|        - | 12942 | ` *  $base` |
|        - | 12943 | ` *   This is the base for resolving the system identifier (systemId) of the external entity.` |
|        - | 12944 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12945 | ` *  $system_id` |
|        - | 12946 | ` *   System identifier for the external entity.` |
|        - | 12947 | ` *  $public_id` |
|        - | 12948 | ` *    Public identifier for the external entity.` |
|        - | 12949 | ` *  Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12950 | ` *  can also be supplied.` |
|        - | 12951 | ` * Return` |
|        - | 12952 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12953 | ` */` |
|        2 | 12954 | `static int vm_builtin_xml_set_notation_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12955 |  |
|        - | 12956 | `	ph7_xml_engine *pEngine;` |
|        3 | 12957 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12958 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12959 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12960 | `		return PH7_OK;` |
|        - | 12961 | `	}` |
|        - | 12962 | `	/* Point to the XML engine */` |
|        3 | 12963 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12964 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12965 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12966 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12967 | `		return PH7_OK;` |
|        - | 12968 | `	}` |
|        3 | 12969 | `	if( nArg > 1 ){` |
|        - | 12970 | `		/* Save the user callback for later invocation */` |
|        3 | 12971 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_ND]);` |
|        1 | 12972 | `	}` |
|        - | 12973 | `	/* All done,return TRUE */` |
|        3 | 12974 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12975 | `	return PH7_OK;` |
|        2 | 12976 |  |
|        - | 12977 | `/*` |
|        - | 12978 | ` * bool xml_set_external_entity_ref_handler(resource $parser,callback $handler)` |
|        - | 12979 | ` *  Set up external entity reference handler.` |
|        - | 12980 | ` * Parameters` |
|        - | 12981 | ` * $parser` |
|        - | 12982 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12983 | ` * $handler` |
|        - | 12984 | ` *  handler is a string containing the name of the callback.` |
|        - | 12985 | ` *  The function named by handler must accept five parameters:` |
|        - | 12986 | ` *   handler(resource $parser,string $open_entity_names,string $base,string $system_id,string $public_id)` |
|        - | 12987 | ` *  $parser` |
|        - | 12988 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12989 | ` *  $open_entity_names` |
|        - | 12990 | ` *   The second parameter, open_entity_names, is a space-separated list of the names` |
|        - | 12991 | ` *   of the entities that are open for the parse of this entity (including the name of the referenced entity).` |
|        - | 12992 | ` *  $base` |
|        - | 12993 | ` *   This is the base for resolving the system identifier (system_id) of the external entity.` |
|        - | 12994 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12995 | ` *  $system_id` |
|        - | 12996 | ` *   The fourth parameter, system_id, is the system identifier as specified in the entity declaration.` |
|        - | 12997 | ` *  $public_id` |
|        - | 12998 | ` *   The fifth parameter, public_id, is the public identifier as specified in the entity declaration` |
|        - | 12999 | ` *   or an empty string if none was specified; the whitespace in the public identifier will have been` |
|        - | 13000 | ` *   normalized as required by the XML spec.` |
|        - | 13001 | ` * Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 13002 | ` * can also be supplied.` |
|        - | 13003 | ` * Return` |
|        - | 13004 | ` *  TRUE on success or FALSE on failure.` |
|        - | 13005 | ` */` |
|        2 | 13006 | `static int vm_builtin_xml_set_external_entity_ref_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13007 |  |
|        - | 13008 | `	ph7_xml_engine *pEngine;` |
|        3 | 13009 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13010 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13011 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13012 | `		return PH7_OK;` |
|        - | 13013 | `	}` |
|        - | 13014 | `	/* Point to the XML engine */` |
|        3 | 13015 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 13016 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13017 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13018 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13019 | `		return PH7_OK;` |
|        - | 13020 | `	}` |
|        3 | 13021 | `	if( nArg > 1 ){` |
|        - | 13022 | `		/* Save the user callback for later invocation */` |
|        3 | 13023 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_EER]);` |
|        1 | 13024 | `	}` |
|        - | 13025 | `	/* All done,return TRUE */` |
|        3 | 13026 | `	ph7_result_bool(pCtx,1);` |
|        3 | 13027 | `	return PH7_OK;` |
|        2 | 13028 |  |
|        - | 13029 | `/*` |
|        - | 13030 | ` * int xml_get_current_line_number(resource $parser)` |
|        - | 13031 | ` *  Gets the current line number for the given XML parser.` |
|        - | 13032 | ` * Parameters` |
|        - | 13033 | ` * $parser` |
|        - | 13034 | ` *   A reference to the XML parser.` |
|        - | 13035 | ` * Return` |
|        - | 13036 | ` *  This function returns FALSE if parser does not refer` |
|        - | 13037 | ` *  to a valid parser, or else it returns which line the parser` |
|        - | 13038 | ` *  is currently at in its data buffer.` |
|        - | 13039 | ` */` |
|        8 | 13040 | `static int vm_builtin_xml_get_current_line_number(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13041 |  |
|        - | 13042 | `	ph7_xml_engine *pEngine;` |
|        9 | 13043 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13044 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13045 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13046 | `		return PH7_OK;` |
|        - | 13047 | `	}` |
|        - | 13048 | `	/* Point to the XML engine */` |
|        9 | 13049 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        9 | 13050 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13051 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13052 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13053 | `		return PH7_OK;` |
|        - | 13054 | `	}` |
|        - | 13055 | `	/* Return the line number */` |
|        9 | 13056 | `	ph7_result_int(pCtx,(int)pEngine->nLine);` |
|        9 | 13057 | `	return PH7_OK;` |
|        5 | 13058 |  |
|        - | 13059 | `/*` |
|        - | 13060 | ` * int xml_get_current_byte_index(resource $parser)` |
|        - | 13061 | ` *  Gets the current byte index of the given XML parser.` |
|        - | 13062 | ` * Parameters` |
|        - | 13063 | ` * $parser` |
|        - | 13064 | ` *   A reference to the XML parser.` |
|        - | 13065 | ` * Return` |
|        - | 13066 | ` *  This function returns FALSE if parser does not refer to a valid` |
|        - | 13067 | ` *  parser, or else it returns which byte index the parser is currently` |
|        - | 13068 | ` *  at in its data buffer (starting at 0).` |
|        - | 13069 | ` */` |
|        4 | 13070 | `static int vm_builtin_xml_get_current_byte_index(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13071 |  |
|        - | 13072 | `	ph7_xml_engine *pEngine;` |
|        - | 13073 | `	SyStream *pStream;` |
|        - | 13074 | `	SyToken *pToken;` |
|        5 | 13075 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13076 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13077 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13078 | `		return PH7_OK;` |
|        - | 13079 | `	}` |
|        - | 13080 | `	/* Point to the XML engine */` |
|        5 | 13081 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        5 | 13082 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13083 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13084 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13085 | `		return PH7_OK;` |
|        - | 13086 | `	}` |
|        - | 13087 | `	/* Point to the current processed token */` |
|        5 | 13088 | `	pToken = (SyToken *)SySetPeekCurrentEntry(&pEngine->sParser.sToken);` |
|        5 | 13089 | `	if( pToken == 0 ){` |
|        - | 13090 | `		/* Stream not yet processed */` |
|        3 | 13091 | `		ph7_result_int(pCtx,0);` |
|        3 | 13092 | `		return 0;` |
|        - | 13093 | `	}` |
|        - | 13094 | `	/* Point to the input stream */` |
|        3 | 13095 | `	pStream = &pEngine->sParser.sLex.sStream;` |
|        - | 13096 | `	/* Return the byte index */` |
|        3 | 13097 | `	ph7_result_int64(pCtx,(ph7_int64)(pToken->sData.zString-(const char *)pStream->zInput));` |
|        3 | 13098 | `	return PH7_OK;` |
|        3 | 13099 |  |
|        - | 13100 | `/*` |
|        - | 13101 | ` * bool xml_set_object(resource $parser,object &$object)` |
|        - | 13102 | ` *  Use XML Parser within an object.` |
|        - | 13103 | ` * NOTE` |
|        - | 13104 | ` *  This function is depreceated and is a no-op.` |
|        - | 13105 | ` * Parameters` |
|        - | 13106 | ` * $parser` |
|        - | 13107 | ` *   A reference to the XML parser.` |
|        - | 13108 | ` * $object` |
|        - | 13109 | ` *  The object where to use the XML parser.` |
|        - | 13110 | ` * Return` |
|        - | 13111 | ` * Always FALSE.` |
|        - | 13112 | ` */` |
|        2 | 13113 | `static int vm_builtin_xml_set_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13114 |  |
|        - | 13115 | `	ph7_xml_engine *pEngine;` |
|        3 | 13116 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_object(apArg[1]) ){` |
|        - | 13117 | `		/* Missing/Ivalid argument,return FALSE */` |
|        3 | 13118 | `		ph7_result_bool(pCtx,0);` |
|        3 | 13119 | `		return PH7_OK;` |
|        - | 13120 | `	}` |
|        - | 13121 | `	/* Point to the XML engine */` |
|      ! 0 | 13122 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|      ! 0 | 13123 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13124 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13125 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13126 | `		return PH7_OK;` |
|        - | 13127 | `	}` |
|        - | 13128 | `	/*  Throw a notice and return */` |
|      ! 0 | 13129 | `	ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"This function is depreceated and is a no-op."` |
|        - | 13130 | `		"In order to mimic this behaviour,you can supply instead of a function name an array "` |
|        - | 13131 | `		"containing an object reference and a method name."` |
|        - | 13132 | `		);` |
|        - | 13133 | `	/* Return FALSE */` |
|      ! 0 | 13134 | `	ph7_result_bool(pCtx,0);` |
|      ! 0 | 13135 | `	return PH7_OK;` |
|        2 | 13136 |  |
|        - | 13137 | `/*` |
|        - | 13138 | ` * int xml_get_current_column_number(resource $parser)` |
|        - | 13139 | ` *  Gets the current column number of the given XML parser.` |
|        - | 13140 | ` * Parameters` |
|        - | 13141 | ` * $parser` |
|        - | 13142 | ` *   A reference to the XML parser.` |
|        - | 13143 | ` * Return` |
|        - | 13144 | ` *  This function returns FALSE if parser does not refer to a valid parser, or else it returns` |
|        - | 13145 | ` *  which column on the current line (as given by xml_get_current_line_number()) the parser` |
|        - | 13146 | ` *  is currently at.` |
|        - | 13147 | ` */` |
|        4 | 13148 | `static int vm_builtin_xml_get_current_column_number(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13149 |  |
|        - | 13150 | `	ph7_xml_engine *pEngine;` |
|        - | 13151 | `	SyStream *pStream;` |
|        - | 13152 | `	SyToken *pToken;` |
|        5 | 13153 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13154 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13155 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13156 | `		return PH7_OK;` |
|        - | 13157 | `	}` |
|        - | 13158 | `	/* Point to the XML engine */` |
|        5 | 13159 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        5 | 13160 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13161 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13162 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13163 | `		return PH7_OK;` |
|        - | 13164 | `	}` |
|        - | 13165 | `	/* Point to the current processed token */` |
|        5 | 13166 | `	pToken = (SyToken *)SySetPeekCurrentEntry(&pEngine->sParser.sToken);` |
|        5 | 13167 | `	if( pToken == 0 ){` |
|        - | 13168 | `		/* Stream not yet processed */` |
|      ! 0 | 13169 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 13170 | `		return 0;` |
|        - | 13171 | `	}` |
|        - | 13172 | `	/* Point to the input stream */` |
|        5 | 13173 | `	pStream = &pEngine->sParser.sLex.sStream;` |
|        - | 13174 | `	/* Return the byte index */` |
|        5 | 13175 | `	ph7_result_int64(pCtx,(ph7_int64)(pToken->sData.zString-(const char *)pStream->zInput)/80);` |
|        5 | 13176 | `	return PH7_OK;` |
|        3 | 13177 |  |
|        - | 13178 | `/*` |
|        - | 13179 | ` * int xml_get_error_code(resource $parser)` |
|        - | 13180 | ` *  Get XML parser error code.` |
|        - | 13181 | ` * Parameters` |
|        - | 13182 | ` * $parser` |
|        - | 13183 | ` *   A reference to the XML parser.` |
|        - | 13184 | ` * Return` |
|        - | 13185 | ` *  This function returns FALSE if parser does not refer to a valid` |
|        - | 13186 | ` *  parser, or else it returns one of the error codes listed in the error` |
|        - | 13187 | ` *  codes section.` |
|        - | 13188 | ` */` |
|       32 | 13189 | `static int vm_builtin_xml_get_error_code(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13190 |  |
|        - | 13191 | `	ph7_xml_engine *pEngine;` |
|       33 | 13192 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13193 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13194 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13195 | `		return PH7_OK;` |
|        - | 13196 | `	}` |
|        - | 13197 | `	/* Point to the XML engine */` |
|       33 | 13198 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       33 | 13199 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13200 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13201 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13202 | `		return PH7_OK;` |
|        - | 13203 | `	}` |
|        - | 13204 | `	/* Return the error code if any */` |
|       33 | 13205 | `	ph7_result_int(pCtx,pEngine->iErrCode);` |
|       33 | 13206 | `	return PH7_OK;` |
|       17 | 13207 |  |
|        - | 13208 | `/*` |
|        - | 13209 | ` * XML parser event callbacks` |
|        - | 13210 | ` * Each time the unserlying XML parser extract a single token` |
|        - | 13211 | ` * from the input,one of the following callbacks are invoked.` |
|        - | 13212 | ` * IMP-XML-ENGINE-07-07-2012 22:02 FreeBSD [chm@symisc.net]` |
|        - | 13213 | ` */` |
|        - | 13214 | `/*` |
|        - | 13215 | ` * Create a scalar ph7_value holding the value` |
|        - | 13216 | ` * of an XML tag/attribute/CDATA and so on.` |
|        - | 13217 | ` */` |
|      148 | 13218 | `static ph7_value * VmXMLValue(ph7_xml_engine *pEngine,SyXMLRawStr *pXML,SyXMLRawStr *pNsUri)` |
|        1 | 13219 |  |
|        - | 13220 | `	ph7_value *pValue;` |
|        - | 13221 | `	/* Allocate a new scalar variable */` |
|      149 | 13222 | `	pValue = ph7_context_new_scalar(pEngine->pCtx);` |
|      149 | 13223 | `	if( pValue == 0 ){` |
|      ! 0 | 13224 | `		ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13225 | `		return 0;` |
|        - | 13226 | `	}` |
|      149 | 13227 | `	if( pNsUri && pNsUri->nByte > 0 ){` |
|        - | 13228 | `		/* Append namespace URI and the separator */` |
|        9 | 13229 | `		ph7_value_string_format(pValue,"%.*s%c",pNsUri->nByte,pNsUri->zString,pEngine->ns_sep);` |
|        4 | 13230 | `	}` |
|        - | 13231 | `	/* Copy the tag value */` |
|      149 | 13232 | `	ph7_value_string(pValue,pXML->zString,(int)pXML->nByte);` |
|      149 | 13233 | `	return pValue;` |
|       75 | 13234 |  |
|        - | 13235 | `/*` |
|        - | 13236 | ` * Create a 'ph7_value' of type array holding the values` |
|        - | 13237 | ` * of an XML tag attributes.` |
|        - | 13238 | ` */` |
|       62 | 13239 | `static ph7_value * VmXMLAttrValue(ph7_xml_engine *pEngine,SyXMLRawStr *aAttr,sxu32 nAttr)` |
|        1 | 13240 |  |
|        - | 13241 | `	ph7_value *pArray;` |
|        - | 13242 | `	/* Create an empty array */` |
|       63 | 13243 | `	pArray = ph7_context_new_array(pEngine->pCtx);` |
|       63 | 13244 | `	if( pArray == 0 ){` |
|      ! 0 | 13245 | `		ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13246 | `		return 0;` |
|        - | 13247 | `	}` |
|       63 | 13248 | `	if( nAttr > 0 ){` |
|        - | 13249 | `		ph7_value *pKey,*pValue;` |
|        - | 13250 | `		sxu32 n;` |
|        - | 13251 | `		/* Create worker variables */` |
|        5 | 13252 | `		pKey = ph7_context_new_scalar(pEngine->pCtx);` |
|        5 | 13253 | `		pValue = ph7_context_new_scalar(pEngine->pCtx);` |
|        5 | 13254 | `		if( pKey == 0 \|\| pValue == 0 ){` |
|      ! 0 | 13255 | `			ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13256 | `			return 0;` |
|        - | 13257 | `		}` |
|        - | 13258 | `		/* Copy attributes */` |
|        9 | 13259 | `		for( n = 0 ; n < nAttr ; n += 2 ){` |
|        - | 13260 | `			/* Reset string cursors */` |
|        5 | 13261 | `			ph7_value_reset_string_cursor(pKey);` |
|        5 | 13262 | `			ph7_value_reset_string_cursor(pValue);` |
|        - | 13263 | `			/* Copy attribute name and it's associated value */` |
|        5 | 13264 | `			ph7_value_string(pKey,aAttr[n].zString,(int)aAttr[n].nByte); /* Attribute name */` |
|        5 | 13265 | `			ph7_value_string(pValue,aAttr[n+1].zString,(int)aAttr[n+1].nByte); /* Attribute value */` |
|        - | 13266 | `			/* Insert in the array */` |
|        5 | 13267 | `			ph7_array_add_elem(pArray,pKey,pValue); /* Will make it's own copy */` |
|        3 | 13268 | `		}` |
|        - | 13269 | `		/* Release the worker variables */` |
|        5 | 13270 | `		ph7_context_release_value(pEngine->pCtx,pKey);` |
|        5 | 13271 | `		ph7_context_release_value(pEngine->pCtx,pValue);` |
|        2 | 13272 | `	}` |
|        - | 13273 | `	/* Return the freshly created array */` |
|       63 | 13274 | `	return pArray;` |
|       32 | 13275 |  |
|        - | 13276 | `/*` |
|        - | 13277 | ` * Start element handler.` |
|        - | 13278 | ` * The user defined callback must accept three parameters:` |
|        - | 13279 | ` *    start_element_handler(resource $parser,string $name,array $attribs )` |
|        - | 13280 | ` *    $parser` |
|        - | 13281 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13282 | ` *    $name` |
|        - | 13283 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 13284 | ` *		is called.If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 13285 | ` *    $attribs` |
|        - | 13286 | ` *      The third parameter, attribs, contains an associative array with the element's attributes (if any).` |
|        - | 13287 | ` *		The keys of this array are the attribute names, the values are the attribute values.` |
|        - | 13288 | ` *      Attribute names are case-folded on the same criteria as element names.Attribute values are not case-folded.` |
|        - | 13289 | ` *      The original order of the attributes can be retrieved by walking through attribs the normal way, using each().` |
|        - | 13290 | ` *      The first key in the array was the first attribute, and so on.` |
|        - | 13291 | ` *      Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 13292 | ` */` |
|       78 | 13293 | `static sxi32 VmXMLStartElementHandler(SyXMLRawStr *pStart,SyXMLRawStr *pNS,sxu32 nAttr,SyXMLRawStr *aAttr,void *pUserData)` |
|        1 | 13294 |  |
|       79 | 13295 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13296 | `	ph7_value *pCallback,*pTag,*pAttr;` |
|        - | 13297 | `	/* Point to the target user defined callback */` |
|       79 | 13298 | `	pCallback = &pEngine->aCB[PH7_XML_START_TAG];` |
|        - | 13299 | `	/* Make sure the given callback is callable */` |
|       79 | 13300 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13301 | `		/* Not callable,return immediately*/` |
|       17 | 13302 | `		return SXRET_OK;` |
|        - | 13303 | `	}` |
|        - | 13304 | `	/* Create a ph7_value holding the tag name */` |
|       63 | 13305 | `	pTag = VmXMLValue(pEngine,pStart,pNS);` |
|        - | 13306 | `	/* Create a ph7_value holding the tag attributes */` |
|       63 | 13307 | `	pAttr = VmXMLAttrValue(pEngine,aAttr,nAttr);` |
|       63 | 13308 | `	if( pTag == 0  \|\| pAttr == 0 ){` |
|      ! 0 | 13309 | `		SXUNUSED(pNS); /* cc warning */` |
|        - | 13310 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13311 | `		return SXRET_OK;` |
|        - | 13312 | `	}` |
|        - | 13313 | `	/* Invoke the user callback */` |
|       63 | 13314 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTag,pAttr,(ph7_value*)0);` |
|        - | 13315 | `	/* Clean-up the mess left behind */` |
|       63 | 13316 | `	ph7_context_release_value(pEngine->pCtx,pTag);` |
|       63 | 13317 | `	ph7_context_release_value(pEngine->pCtx,pAttr);` |
|       63 | 13318 | `	return SXRET_OK;` |
|       40 | 13319 |  |
|        - | 13320 | `/*` |
|        - | 13321 | ` * End element handler.` |
|        - | 13322 | ` * The user defined callback must accept two parameters:` |
|        - | 13323 | ` *  end_element_handler(resource $parser,string $name)` |
|        - | 13324 | ` *  $parser` |
|        - | 13325 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13326 | ` *  $name` |
|        - | 13327 | ` *   The second parameter, name, contains the name of the element for which this handler is called.` |
|        - | 13328 | ` *   If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 13329 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 13330 | ` *   can also be supplied.` |
|        - | 13331 | ` */` |
|       62 | 13332 | `static sxi32 VmXMLEndElementHandler(SyXMLRawStr *pEnd,SyXMLRawStr *pNS,void *pUserData)` |
|        1 | 13333 |  |
|       63 | 13334 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13335 | `	ph7_value *pCallback,*pTag;` |
|        - | 13336 | `	/* Point to the target user defined callback */` |
|       63 | 13337 | `	pCallback = &pEngine->aCB[PH7_XML_END_TAG];` |
|        - | 13338 | `	/* Make sure the given callback is callable */` |
|       63 | 13339 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13340 | `		/* Not callable,return immediately*/` |
|        9 | 13341 | `		return SXRET_OK;` |
|        - | 13342 | `	}` |
|        - | 13343 | `	/* Create a ph7_value holding the tag name */` |
|       55 | 13344 | `	pTag = VmXMLValue(pEngine,pEnd,pNS);` |
|       55 | 13345 | `	if( pTag == 0  ){` |
|      ! 0 | 13346 | `		SXUNUSED(pNS); /* cc warning */` |
|        - | 13347 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13348 | `		return SXRET_OK;` |
|        - | 13349 | `	}` |
|        - | 13350 | `	/* Invoke the user callback */` |
|       55 | 13351 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTag,(ph7_value*)0);` |
|        - | 13352 | `	/* Clean-up the mess left behind */` |
|       55 | 13353 | `	ph7_context_release_value(pEngine->pCtx,pTag);` |
|       55 | 13354 | `	return SXRET_OK;` |
|       32 | 13355 |  |
|        - | 13356 | `/*` |
|        - | 13357 | ` * Character data handler.` |
|        - | 13358 | ` *  The user defined callback must accept two parameters:` |
|        - | 13359 | ` *  handler(resource $parser,string $data)` |
|        - | 13360 | ` *  $parser` |
|        - | 13361 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13362 | ` *  $data` |
|        - | 13363 | ` *   The second parameter, data, contains the character data as a string.` |
|        - | 13364 | ` *   Character data handler is called for every piece of a text in the XML document.` |
|        - | 13365 | ` *   It can be called multiple times inside each fragment (e.g. for non-ASCII strings).` |
|        - | 13366 | ` *   If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 13367 | ` *   Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 13368 | ` */` |
|       28 | 13369 | `static sxi32 VmXMLTextHandler(SyXMLRawStr *pText,void *pUserData)` |
|        1 | 13370 |  |
|       29 | 13371 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13372 | `	ph7_value *pCallback,*pData;` |
|        - | 13373 | `	/* Point to the target user defined callback */` |
|       29 | 13374 | `	pCallback = &pEngine->aCB[PH7_XML_CDATA];` |
|        - | 13375 | `	/* Make sure the given callback is callable */` |
|       29 | 13376 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13377 | `		/* Not callable,return immediately*/` |
|       11 | 13378 | `		return SXRET_OK;` |
|        - | 13379 | `	}` |
|        - | 13380 | `	/* Create a ph7_value holding the data */` |
|       19 | 13381 | `	pData = VmXMLValue(pEngine,&(*pText),0);` |
|       19 | 13382 | `	if( pData == 0  ){` |
|        - | 13383 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13384 | `		return SXRET_OK;` |
|        - | 13385 | `	}` |
|        - | 13386 | `	/* Invoke the user callback */` |
|       19 | 13387 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pData,(ph7_value*)0);` |
|        - | 13388 | `	/* Clean-up the mess left behind */` |
|       19 | 13389 | `	ph7_context_release_value(pEngine->pCtx,pData);` |
|       19 | 13390 | `	return SXRET_OK;` |
|       15 | 13391 |  |
|        - | 13392 | `/*` |
|        - | 13393 | ` * Processing instruction (PI) handler.` |
|        - | 13394 | ` * The user defined callback must accept two parameters:` |
|        - | 13395 | ` *   handler(resource $parser,string $target,string $data)` |
|        - | 13396 | ` *  $parser` |
|        - | 13397 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13398 | ` *  $target` |
|        - | 13399 | ` *   The second parameter, target, contains the PI target.` |
|        - | 13400 | ` *  $data` |
|        - | 13401 | ` *    The third parameter, data, contains the PI data.` |
|        - | 13402 | ` *    Note: Instead of a function name, an array containing an object reference` |
|        - | 13403 | ` *    and a method name can also be supplied.` |
|        - | 13404 | ` */` |
|        8 | 13405 | `static sxi32 VmXMLPIHandler(SyXMLRawStr *pTargetStr,SyXMLRawStr *pDataStr,void *pUserData)` |
|        1 | 13406 |  |
|        9 | 13407 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13408 | `	ph7_value *pCallback,*pTarget,*pData;` |
|        - | 13409 | `	/* Point to the target user defined callback */` |
|        9 | 13410 | `	pCallback = &pEngine->aCB[PH7_XML_PI];` |
|        - | 13411 | `	/* Make sure the given callback is callable */` |
|        9 | 13412 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13413 | `		/* Not callable,return immediately*/` |
|        5 | 13414 | `		return SXRET_OK;` |
|        - | 13415 | `	}` |
|        - | 13416 | `	/* Get a ph7_value holding the data */` |
|        5 | 13417 | `	pTarget = VmXMLValue(pEngine,&(*pTargetStr),0);` |
|        5 | 13418 | `	pData = VmXMLValue(pEngine,&(*pDataStr),0);` |
|        5 | 13419 | `	if( pTarget == 0 \|\| pData == 0  ){` |
|        - | 13420 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13421 | `		return SXRET_OK;` |
|        - | 13422 | `	}` |
|        - | 13423 | `	/* Invoke the user callback */` |
|        5 | 13424 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTarget,pData,(ph7_value*)0);` |
|        - | 13425 | `	/* Clean-up the mess left behind */` |
|        5 | 13426 | `	ph7_context_release_value(pEngine->pCtx,pTarget);` |
|        5 | 13427 | `	ph7_context_release_value(pEngine->pCtx,pData);` |
|        5 | 13428 | `	return SXRET_OK;` |
|        5 | 13429 |  |
|        - | 13430 | `/*` |
|        - | 13431 | ` * Namespace declaration handler.` |
|        - | 13432 | ` * The user defined callback must accept two parameters:` |
|        - | 13433 | ` *    handler(resource $parser,string $prefix,string $uri)` |
|        - | 13434 | ` * $parser` |
|        - | 13435 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13436 | ` * $prefix` |
|        - | 13437 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 13438 | ` * $uri` |
|        - | 13439 | ` *   Uniform Resource Identifier (URI) of namespace.` |
|        - | 13440 | ` *   Note: Instead of a function name, an array containing an object reference` |
|        - | 13441 | ` *   and a method name can also be supplied.` |
|        - | 13442 | ` */` |
|        4 | 13443 | `static sxi32 VmXMLNSStartHandler(SyXMLRawStr *pUriStr,SyXMLRawStr *pPrefixStr,void *pUserData)` |
|        1 | 13444 |  |
|        5 | 13445 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13446 | `	ph7_value *pCallback,*pUri,*pPrefix;` |
|        - | 13447 | `	/* Point to the target user defined callback */` |
|        5 | 13448 | `	pCallback = &pEngine->aCB[PH7_XML_NS_START];` |
|        - | 13449 | `	/* Make sure the given callback is callable */` |
|        5 | 13450 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13451 | `		/* Not callable,return immediately*/` |
|        3 | 13452 | `		return SXRET_OK;` |
|        - | 13453 | `	}` |
|        - | 13454 | `	/* Get a ph7_value holding the PREFIX/URI */` |
|        3 | 13455 | `	pUri = VmXMLValue(pEngine,pUriStr,0);` |
|        3 | 13456 | `	pPrefix = VmXMLValue(pEngine,pPrefixStr,0);` |
|        3 | 13457 | `	if( pUri == 0 \|\| pPrefix == 0  ){` |
|        - | 13458 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13459 | `		return SXRET_OK;` |
|        - | 13460 | `	}` |
|        - | 13461 | `	/* Invoke the user callback */` |
|        3 | 13462 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pUri,pPrefix,(ph7_value*)0);` |
|        - | 13463 | `	/* Clean-up the mess left behind */` |
|        3 | 13464 | `	ph7_context_release_value(pEngine->pCtx,pUri);` |
|        3 | 13465 | `	ph7_context_release_value(pEngine->pCtx,pPrefix);` |
|        3 | 13466 | `	return SXRET_OK;` |
|        3 | 13467 |  |
|        - | 13468 | `/*` |
|        - | 13469 | ` * Namespace end declaration handler.` |
|        - | 13470 | ` * The user defined callback must accept two parameters:` |
|        - | 13471 | ` *    handler(resource $parser,string $prefix)` |
|        - | 13472 | ` * $parser` |
|        - | 13473 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13474 | ` * $prefix` |
|        - | 13475 | ` *  The prefix is a string used to reference the namespace within an XML object.` |
|        - | 13476 | ` *   Note: Instead of a function name, an array containing an object reference` |
|        - | 13477 | ` *   and a method name can also be supplied.` |
|        - | 13478 | ` */` |
|        4 | 13479 | `static sxi32 VmXMLNSEndHandler(SyXMLRawStr *pPrefixStr,void *pUserData)` |
|        1 | 13480 |  |
|        5 | 13481 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13482 | `	ph7_value *pCallback,*pPrefix;` |
|        - | 13483 | `	/* Point to the target user defined callback */` |
|        5 | 13484 | `	pCallback = &pEngine->aCB[PH7_XML_NS_END];` |
|        - | 13485 | `	/* Make sure the given callback is callable */` |
|        5 | 13486 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13487 | `		/* Not callable,return immediately*/` |
|        3 | 13488 | `		return SXRET_OK;` |
|        - | 13489 | `	}` |
|        - | 13490 | `	/* Get a ph7_value holding the prefix */` |
|        3 | 13491 | `	pPrefix = VmXMLValue(pEngine,pPrefixStr,0);` |
|        3 | 13492 | `	if( pPrefix == 0 ){` |
|        - | 13493 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13494 | `		return SXRET_OK;` |
|        - | 13495 | `	}` |
|        - | 13496 | `	/* Invoke the user callback */` |
|        3 | 13497 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pPrefix,(ph7_value*)0);` |
|        - | 13498 | `	/* Clean-up the mess left behind */` |
|        3 | 13499 | `	ph7_context_release_value(pEngine->pCtx,pPrefix);` |
|        3 | 13500 | `	return SXRET_OK;` |
|        3 | 13501 |  |
|        - | 13502 | `/*` |
|        - | 13503 | ` * Error Message consumer handler.` |
|        - | 13504 | ` * Each time the XML parser encounter a syntaxt error or any other error` |
|        - | 13505 | ` * related to XML processing,the following callback is invoked by the` |
|        - | 13506 | ` * underlying XML parser.` |
|        - | 13507 | ` */` |
|       34 | 13508 | `static sxi32 VmXMLErrorHandler(const char *zMessage,sxi32 iErrCode,SyToken *pToken,void *pUserData)` |
|        1 | 13509 |  |
|       35 | 13510 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13511 | `	/* Save the error code */` |
|       35 | 13512 | `	pEngine->iErrCode = iErrCode;` |
|       17 | 13513 | `	SXUNUSED(zMessage); /* cc warning */` |
|       35 | 13514 | `	if( pToken ){` |
|       35 | 13515 | `		pEngine->nLine = pToken->nLine;` |
|       17 | 13516 | `	}` |
|        - | 13517 | `	/* Abort XML processing immediately */` |
|       35 | 13518 | `	return SXERR_ABORT;` |
|        1 | 13519 |  |
|        - | 13520 | `/*` |
|        - | 13521 | ` * int xml_parse(resource $parser,string $data[,bool $is_final = false ])` |
|        - | 13522 | ` *  Parses an XML document. The handlers for the configured events are called` |
|        - | 13523 | ` *  as many times as necessary.` |
|        - | 13524 | ` * Parameters` |
|        - | 13525 | ` *  $parser` |
|        - | 13526 | ` *   A reference to the XML parser.` |
|        - | 13527 | ` *  $data` |
|        - | 13528 | ` *   Chunk of data to parse. A document may be parsed piece-wise by calling` |
|        - | 13529 | ` *   xml_parse() several times with new data, as long as the is_final parameter` |
|        - | 13530 | ` *   is set and TRUE when the last data is parsed.` |
|        - | 13531 | ` * $is_final` |
|        - | 13532 | ` *   NOT USED. This implementation require that all the processed input be` |
|        - | 13533 | ` *   entirely loaded in memory.` |
|        - | 13534 | ` * Return` |
|        - | 13535 | ` *  Returns 1 on success or 0 on failure.` |
|        - | 13536 | ` */` |
|       74 | 13537 | `static int vm_builtin_xml_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13538 |  |
|        - | 13539 | `	ph7_xml_engine *pEngine;` |
|        - | 13540 | `	SyXMLParser *pParser;` |
|        - | 13541 | `	const char *zData;` |
|        - | 13542 | `	int nByte;` |
|       75 | 13543 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|        - | 13544 | `		/* Missing/Ivalid arguments,return FALSE */` |
|      ! 0 | 13545 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13546 | `		return PH7_OK;` |
|        - | 13547 | `	}` |
|        - | 13548 | `	/* Point to the XML engine */` |
|       75 | 13549 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       75 | 13550 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13551 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13552 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13553 | `		return PH7_OK;` |
|        - | 13554 | `	}` |
|       75 | 13555 | `	if( pEngine->iNest > 0 ){` |
|        - | 13556 | `		/* This can happen when the user callback call xml_parse() again` |
|        - | 13557 | `		 * in it's body which is forbidden.` |
|        - | 13558 | `		 */` |
|      ! 0 | 13559 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|        - | 13560 | `			"Recursive call to %s,PH7 is returning false",` |
|      ! 0 | 13561 | `			ph7_function_name(pCtx)` |
|        - | 13562 | `			);` |
|        - | 13563 | `		/* Return FALSE */` |
|      ! 0 | 13564 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13565 | `		return PH7_OK;` |
|        - | 13566 | `	}` |
|       75 | 13567 | `	pEngine->pCtx = pCtx;` |
|        - | 13568 | `	/* Point to the underlying XML parser */` |
|       75 | 13569 | `	pParser = &pEngine->sParser;` |
|        - | 13570 | `	/* Register elements handler */` |
|       75 | 13571 | `	SyXMLParserSetEventHandler(pParser,pEngine,` |
|        - | 13572 | `		VmXMLStartElementHandler,` |
|        - | 13573 | `		VmXMLTextHandler,` |
|        - | 13574 | `		VmXMLErrorHandler,` |
|        - | 13575 |  |
|        - | 13576 | `		VmXMLEndElementHandler,` |
|        - | 13577 | `		VmXMLPIHandler,` |
|        - | 13578 |  |
|        - | 13579 |  |
|        - | 13580 | `		VmXMLNSStartHandler,` |
|        - | 13581 | `		VmXMLNSEndHandler` |
|        - | 13582 | `		);` |
|       75 | 13583 | `	pEngine->iErrCode = SXML_ERROR_NONE;` |
|        - | 13584 | `	/* Extract the raw XML input */` |
|       75 | 13585 | `	zData = ph7_value_to_string(apArg[1],&nByte);` |
|        - | 13586 | `	/* Start the parse process */` |
|       75 | 13587 | `	pEngine->iNest++;` |
|       75 | 13588 | `	SyXMLProcess(pParser,zData,(sxu32)nByte);` |
|       75 | 13589 | `	pEngine->iNest--;` |
|        - | 13590 | `	/* Return the parse result */` |
|       75 | 13591 | `	ph7_result_int(pCtx,pEngine->iErrCode == SXML_ERROR_NONE ? 1 : 0);` |
|       75 | 13592 | `	return PH7_OK;` |
|       38 | 13593 |  |
|        - | 13594 | `/*` |
|        - | 13595 | ` * bool xml_parser_set_option(resource $parser,int $option,mixed $value)` |
|        - | 13596 | ` *  Sets an option in an XML parser.` |
|        - | 13597 | ` * Parameters` |
|        - | 13598 | ` *  $parser` |
|        - | 13599 | ` *   A reference to the XML parser to set an option in.` |
|        - | 13600 | ` *  $option` |
|        - | 13601 | ` *    Which option to set. See below.` |
|        - | 13602 | ` *   The following options are available:` |
|        - | 13603 | ` *   XML_OPTION_CASE_FOLDING 	integer  Controls whether case-folding is enabled for this XML parser.` |
|        - | 13604 | ` *   XML_OPTION_SKIP_TAGSTART 	integer  Specify how many characters should be skipped in the beginning of a tag name.` |
|        - | 13605 | ` *   XML_OPTION_SKIP_WHITE 	    integer  Whether to skip values consisting of whitespace characters.` |
|        - | 13606 | ` *   XML_OPTION_TARGET_ENCODING string 	 Sets which target encoding to use in this XML parser.` |
|        - | 13607 | ` * $value` |
|        - | 13608 | ` *   The option's new value.` |
|        - | 13609 | ` * Return` |
|        - | 13610 | ` *  Returns 1 on success or 0 on failure.` |
|        - | 13611 | ` * Note:` |
|        - | 13612 | ` *  Well,none of these options have meaning under the built-in XML parser so a call to this` |
|        - | 13613 | ` *  function is a no-op.` |
|        - | 13614 | ` */` |
|        6 | 13615 | `static int vm_builtin_xml_parser_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13616 |  |
|        - | 13617 | `	ph7_xml_engine *pEngine;` |
|        7 | 13618 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13619 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13620 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13621 | `		return PH7_OK;` |
|        - | 13622 | `	}` |
|        - | 13623 | `	/* Point to the XML engine */` |
|        7 | 13624 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        7 | 13625 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13626 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13627 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13628 | `		return PH7_OK;` |
|        - | 13629 | `	}` |
|        - | 13630 | `	/* Always return FALSE */` |
|        7 | 13631 | `	ph7_result_bool(pCtx,0);` |
|        7 | 13632 | `	return PH7_OK;` |
|        4 | 13633 |  |
|        - | 13634 | `/*` |
|        - | 13635 | ` * mixed xml_parser_get_option(resource $parser,int $option)` |
|        - | 13636 | ` *  Get options from an XML parser.` |
|        - | 13637 | ` * Parameters` |
|        - | 13638 | ` *  $parser` |
|        - | 13639 | ` *   A reference to the XML parser to set an option in.` |
|        - | 13640 | ` * $option` |
|        - | 13641 | ` *   Which option to fetch.` |
|        - | 13642 | ` * Return` |
|        - | 13643 | ` *  This function returns FALSE if parser does not refer to a valid parser` |
|        - | 13644 | ` *  or if option isn't valid.Else the option's value is returned.` |
|        - | 13645 | ` */` |
|        2 | 13646 | `static int vm_builtin_xml_parser_get_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13647 |  |
|        - | 13648 | `	ph7_xml_engine *pEngine;` |
|        - | 13649 | `	int nOp;` |
|        3 | 13650 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13651 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13652 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13653 | `		return PH7_OK;` |
|        - | 13654 | `	}` |
|        - | 13655 | `	/* Point to the XML engine */` |
|        3 | 13656 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 13657 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13658 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13659 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13660 | `		return PH7_OK;` |
|        - | 13661 | `	}` |
|        - | 13662 | `	/* Extract the option */` |
|        3 | 13663 | `	nOp = ph7_value_to_int(apArg[1]);` |
|        3 | 13664 | `	switch(nOp){` |
|      ! 0 | 13665 | `	case SXML_OPTION_SKIP_TAGSTART:` |
|        - | 13666 | `	case SXML_OPTION_SKIP_WHITE:` |
|        - | 13667 | `	case SXML_OPTION_CASE_FOLDING:` |
|      ! 0 | 13668 | `		ph7_result_int(pCtx,0); break;` |
|      ! 0 | 13669 | `	case SXML_OPTION_TARGET_ENCODING:` |
|      ! 0 | 13670 | `		ph7_result_string(pCtx,"UTF-8",(int)sizeof("UTF-8")-1);` |
|      ! 0 | 13671 | `		break;` |
|        1 | 13672 | `	default:` |
|        - | 13673 | `		/* Unknown option,return FALSE*/` |
|        3 | 13674 | `		ph7_result_bool(pCtx,0);` |
|        2 | 13675 | `		break;` |
|        - | 13676 | `	}` |
|        3 | 13677 | `	return PH7_OK;` |
|        2 | 13678 |  |
|        - | 13679 | `/*` |
|        - | 13680 | ` * string xml_error_string(int $code)` |
|        - | 13681 | ` *  Gets the XML parser error string associated with the given code.` |
|        - | 13682 | ` * Parameters` |
|        - | 13683 | ` *  $code` |
|        - | 13684 | ` *   An error code from xml_get_error_code().` |
|        - | 13685 | ` * Return` |
|        - | 13686 | ` *  Returns a string with a textual description of the error` |
|        - | 13687 | ` *  code, or FALSE if no description was found.` |
|        - | 13688 | ` */` |
|       30 | 13689 | `static int vm_builtin_xml_error_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13690 |  |
|       31 | 13691 | `	int nErr = -1;` |
|       31 | 13692 | `	if( nArg > 0 ){` |
|       31 | 13693 | `		nErr = ph7_value_to_int(apArg[0]);` |
|       15 | 13694 | `	}` |
|       31 | 13695 | `	switch(nErr){` |
|        1 | 13696 | `	case SXML_ERROR_DUPLICATE_ATTRIBUTE:` |
|        3 | 13697 | `		ph7_result_string(pCtx,"Duplicate attribute",-1/*Compute length automatically*/);` |
|        3 | 13698 | `		break;` |
|      ! 0 | 13699 | `	case SXML_ERROR_INCORRECT_ENCODING:` |
|      ! 0 | 13700 | `		ph7_result_string(pCtx,"Incorrect encoding",-1);` |
|      ! 0 | 13701 | `		break;` |
|      ! 0 | 13702 | `	case SXML_ERROR_INVALID_TOKEN:` |
|      ! 0 | 13703 | `		ph7_result_string(pCtx,"Unexpected token",-1);` |
|      ! 0 | 13704 | `		break;` |
|        3 | 13705 | `	case SXML_ERROR_MISPLACED_XML_PI:` |
|        7 | 13706 | `		ph7_result_string(pCtx,"Misplaced processing instruction",-1);` |
|        7 | 13707 | `		break;` |
|      ! 0 | 13708 | `	case SXML_ERROR_NO_MEMORY:` |
|      ! 0 | 13709 | `		ph7_result_string(pCtx,"Out of memory",-1);` |
|      ! 0 | 13710 | `		break;` |
|        1 | 13711 | `	case SXML_ERROR_NONE:` |
|        3 | 13712 | `		ph7_result_string(pCtx,"Not an error",-1);` |
|        3 | 13713 | `		break;` |
|        1 | 13714 | `	case SXML_ERROR_TAG_MISMATCH:` |
|        3 | 13715 | `		ph7_result_string(pCtx,"Tag mismatch",-1);` |
|        3 | 13716 | `		break;` |
|      ! 0 | 13717 | `	case -1:` |
|      ! 0 | 13718 | `		ph7_result_string(pCtx,"Unknown error code",-1);` |
|      ! 0 | 13719 | `		break;` |
|        9 | 13720 | `	default:` |
|       19 | 13721 | `		ph7_result_string(pCtx,"Syntax error",-1);` |
|       18 | 13722 | `		break;` |
|        - | 13723 | `	}` |
|       31 | 13724 | `	return PH7_OK;` |
|        1 | 13725 |  |
|        - | 13726 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13727 | `/*` |
|        - | 13728 | ` * int utf8_encode(string $input)` |
|        - | 13729 | ` *  UTF-8 encoding.` |
|        - | 13730 | ` *  This function encodes the string data to UTF-8, and returns the encoded version.` |
|        - | 13731 | ` *  UTF-8 is a standard mechanism used by Unicode for encoding wide character values` |
|        - | 13732 | ` * into a byte stream. UTF-8 is transparent to plain ASCII characters, is self-synchronized` |
|        - | 13733 | ` * (meaning it is possible for a program to figure out where in the bytestream characters start)` |
|        - | 13734 | ` * and can be used with normal string comparison functions for sorting and such.` |
|        - | 13735 | ` *  Notes on UTF-8 (According to SQLite3 authors):` |
|        - | 13736 | ` *  Byte-0    Byte-1    Byte-2    Byte-3    Value` |
|        - | 13737 | ` *  0xxxxxxx                                 00000000 00000000 0xxxxxxx` |
|        - | 13738 | ` *  110yyyyy  10xxxxxx                       00000000 00000yyy yyxxxxxx` |
|        - | 13739 | ` *  1110zzzz  10yyyyyy  10xxxxxx             00000000 zzzzyyyy yyxxxxxx` |
|        - | 13740 | ` *  11110uuu  10uuzzzz  10yyyyyy  10xxxxxx   000uuuuu zzzzyyyy yyxxxxxx` |
|        - | 13741 | ` * Parameters` |
|        - | 13742 | ` * $input` |
|        - | 13743 | ` *   String to encode or NULL on failure.` |
|        - | 13744 | ` * Return` |
|        - | 13745 | ` *  An UTF-8 encoded string.` |
|        - | 13746 | ` */` |
|        2 | 13747 | `static int vm_builtin_utf8_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13748 |  |
|        - | 13749 | `	const unsigned char *zIn,*zEnd;` |
|        - | 13750 | `	int nByte,c,e;` |
|        3 | 13751 | `	if( nArg < 1 ){` |
|        - | 13752 | `		/* Missing arguments,return null */` |
|      ! 0 | 13753 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13754 | `		return PH7_OK;` |
|        - | 13755 | `	}` |
|        - | 13756 | `	/* Extract the target string */` |
|        3 | 13757 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 13758 | `	if( nByte < 1 ){` |
|        - | 13759 | `		/* Empty string,return null */` |
|      ! 0 | 13760 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13761 | `		return PH7_OK;` |
|        - | 13762 | `	}` |
|        3 | 13763 | `	zEnd = &zIn[nByte];` |
|        - | 13764 | `	/* Start the encoding process */` |
|        2 | 13765 | `	for(;;){` |
|        5 | 13766 | `		if( zIn >= zEnd ){` |
|        - | 13767 | `			/* End of input */` |
|        3 | 13768 | `			break;` |
|        - | 13769 | `		}` |
|        3 | 13770 | `		c = zIn[0];` |
|        - | 13771 | `		/* Advance the stream cursor */` |
|        3 | 13772 | `		zIn++;` |
|        - | 13773 | `		/* Encode */` |
|        3 | 13774 | `		if( c<0x00080 ){` |
|      ! 0 | 13775 | `			e = (c&0xFF);` |
|      ! 0 | 13776 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        3 | 13777 | `		}else if( c<0x00800 ){` |
|        3 | 13778 | `			e = 0xC0 + ((c>>6)&0x1F);` |
|        3 | 13779 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        3 | 13780 | `			e = 0x80 + (c & 0x3F);` |
|        3 | 13781 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        1 | 13782 | `		}else if( c<0x10000 ){` |
|      ! 0 | 13783 | `			e = 0xE0 + ((c>>12)&0x0F);` |
|      ! 0 | 13784 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13785 | `			e = 0x80 + ((c>>6) & 0x3F);` |
|      ! 0 | 13786 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13787 | `			e = 0x80 + (c & 0x3F);` |
|      ! 0 | 13788 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13789 | `		}else{` |
|      ! 0 | 13790 | `			e = 0xF0 + ((c>>18) & 0x07);` |
|      ! 0 | 13791 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13792 | `			e = 0x80 + ((c>>12) & 0x3F);` |
|      ! 0 | 13793 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13794 | `			e = 0x80 + ((c>>6) & 0x3F);` |
|      ! 0 | 13795 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13796 | `			e = 0x80 + (c & 0x3F);` |
|      ! 0 | 13797 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        - | 13798 | `		}` |
|        1 | 13799 | `	}` |
|        - | 13800 | `	/* All done */` |
|        3 | 13801 | `	return PH7_OK;` |
|        2 | 13802 |  |
|        - | 13803 | `/*` |
|        - | 13804 | ` * UTF-8 decoding routine extracted from the sqlite3 source tree.` |
|        - | 13805 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|        - | 13806 | ` * Status: Public Domain` |
|        - | 13807 | ` */` |
|        - | 13808 | `/*` |
|        - | 13809 | `** This lookup table is used to help decode the first byte of` |
|        - | 13810 | `** a multi-byte UTF8 character.` |
|        - | 13811 | `*/` |
|        - | 13812 | `static const unsigned char UtfTrans1[] = {` |
|        - | 13813 |  |
|        - | 13814 |  |
|        - | 13815 |  |
|        - | 13816 |  |
|        - | 13817 |  |
|        - | 13818 |  |
|        - | 13819 |  |
|        - | 13820 |  |
|        - | 13821 | `};` |
|        - | 13822 | `/*` |
|        - | 13823 | `** Translate a single UTF-8 character.  Return the unicode value.` |
|        - | 13824 | `**` |
|        - | 13825 | `** During translation, assume that the byte that zTerm points` |
|        - | 13826 | `** is a 0x00.` |
|        - | 13827 | `**` |
|        - | 13828 | `** Write a pointer to the next unread byte back into *pzNext.` |
|        - | 13829 | `**` |
|        - | 13830 | `** Notes On Invalid UTF-8:` |
|        - | 13831 | `**` |
|        - | 13832 | `**  *  This routine never allows a 7-bit character (0x00 through 0x7f) to` |
|        - | 13833 | `**     be encoded as a multi-byte character.  Any multi-byte character that` |
|        - | 13834 | `**     attempts to encode a value between 0x00 and 0x7f is rendered as 0xfffd.` |
|        - | 13835 | `**` |
|        - | 13836 | `**  *  This routine never allows a UTF16 surrogate value to be encoded.` |
|        - | 13837 | `**     If a multi-byte character attempts to encode a value between` |
|        - | 13838 | `**     0xd800 and 0xe000 then it is rendered as 0xfffd.` |
|        - | 13839 | `**` |
|        - | 13840 | `**  *  Bytes in the range of 0x80 through 0xbf which occur as the first` |
|        - | 13841 | `**     byte of a character are interpreted as single-byte characters` |
|        - | 13842 | `**     and rendered as themselves even though they are technically` |
|        - | 13843 | `**     invalid characters.` |
|        - | 13844 | `**` |
|        - | 13845 | `**  *  This routine accepts an infinite number of different UTF8 encodings` |
|        - | 13846 | `**     for unicode values 0x80 and greater.  It do not change over-length` |
|        - | 13847 | `**     encodings to 0xfffd as some systems recommend.` |
|        - | 13848 | `*/` |
|        - | 13849 | `#define READ_UTF8(zIn, zTerm, c)                           \` |
|        - | 13850 | `  c = *(zIn++);                                            \` |
|        - | 13851 | `  if( c>=0xc0 ){                                           \` |
|        - | 13852 | `    c = UtfTrans1[c-0xc0];                                 \` |
|        - | 13853 | `    while( zIn!=zTerm && (*zIn & 0xc0)==0x80 ){            \` |
|        - | 13854 | `      c = (c<<6) + (0x3f & *(zIn++));                      \` |
|        - | 13855 | `    }                                                      \` |
|        - | 13856 | `    if( c<0x80                                             \` |
|        - | 13857 | `        \|\| (c&0xFFFFF800)==0xD800                          \` |
|        - | 13858 | `        \|\| (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }        \` |
|        - | 13859 | `  }` |
|      150 | 13860 | `PH7_PRIVATE int PH7_Utf8Read(` |
|        - | 13861 | `  const unsigned char *z,         /* First byte of UTF-8 character */` |
|        - | 13862 | `  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */` |
|        - | 13863 | `  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */` |
|        1 | 13864 | `){` |
|        - | 13865 | `  int c;` |
|      153 | 13866 | `  READ_UTF8(z, zTerm, c);` |
|      151 | 13867 | `  *pzNext = z;` |
|      151 | 13868 | `  return c;` |
|        1 | 13869 |  |
|        - | 13870 | `/*` |
|        - | 13871 | ` * string utf8_decode(string $data)` |
|        - | 13872 | ` *  This function decodes data, assumed to be UTF-8 encoded, to unicode.` |
|        - | 13873 | ` * Parameters` |
|        - | 13874 | ` * data` |
|        - | 13875 | ` *  An UTF-8 encoded string.` |
|        - | 13876 | ` * Return` |
|        - | 13877 | ` *  Unicode decoded string or NULL on failure.` |
|        - | 13878 | ` */` |
|        2 | 13879 | `static int vm_builtin_utf8_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13880 |  |
|        - | 13881 | `	const unsigned char *zIn,*zEnd;` |
|        - | 13882 | `	int nByte,c;` |
|        3 | 13883 | `	if( nArg < 1 ){` |
|        - | 13884 | `		/* Missing arguments,return null */` |
|      ! 0 | 13885 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13886 | `		return PH7_OK;` |
|        - | 13887 | `	}` |
|        - | 13888 | `	/* Extract the target string */` |
|        3 | 13889 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 13890 | `	if( nByte < 1 ){` |
|        - | 13891 | `		/* Empty string,return null */` |
|      ! 0 | 13892 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13893 | `		return PH7_OK;` |
|        - | 13894 | `	}` |
|        3 | 13895 | `	zEnd = &zIn[nByte];` |
|        - | 13896 | `	/* Start the decoding process */` |
|        5 | 13897 | `	while( zIn < zEnd ){` |
|        3 | 13898 | `		c = PH7_Utf8Read(zIn,zEnd,&zIn);` |
|        3 | 13899 | `		if( c == 0x0 ){` |
|      ! 0 | 13900 | `			break;` |
|        - | 13901 | `		}` |
|        3 | 13902 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|        1 | 13903 | `	}` |
|        3 | 13904 | `	return PH7_OK;` |
|        2 | 13905 |  |
|        - | 13906 | `/* Table of built-in VM functions. */` |
|        - | 13907 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 13908 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 13909 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 13910 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 13911 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 13912 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 13913 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 13914 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 13915 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 13916 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 13917 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 13918 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 13919 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 13920 | `	    /* Constants management */` |
|        - | 13921 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 13922 | `	{ "define",   vm_builtin_define               },` |
|        - | 13923 | `	{ "constant", vm_builtin_constant             },` |
|        - | 13924 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 13925 | `	   /* Class/Object functions */` |
|        - | 13926 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 13927 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 13928 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 13929 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 13930 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 13931 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 13932 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 13933 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 13934 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 13935 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 13936 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 13937 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 13938 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 13939 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 13940 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 13941 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 13942 | `	   /* Random numbers/strings generators */` |
|        - | 13943 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 13944 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 13945 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 13946 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 13947 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 13948 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13949 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 13950 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 13951 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 13952 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13953 | `	   /* Language constructs functions */` |
|        - | 13954 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 13955 | `	{ "print", vm_builtin_print                   },` |
|        - | 13956 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 13957 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 13958 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 13959 | `	  /* Variable handling functions */` |
|        - | 13960 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 13961 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 13962 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 13963 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 13964 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 13965 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 13966 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 13967 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 13968 | `	  /* Ouput control functions */` |
|        - | 13969 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 13970 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 13971 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 13972 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 13973 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 13974 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 13975 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 13976 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 13977 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 13978 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 13979 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 13980 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 13981 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 13982 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 13983 | `	  /* Assertion functions */` |
|        - | 13984 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 13985 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 13986 | `	  /* Error reporting functions */` |
|        - | 13987 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 13988 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 13989 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 13990 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 13991 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 13992 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 13993 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 13994 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 13995 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 13996 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 13997 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 13998 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 13999 | `	  /* Release info */` |
|        - | 14000 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 14001 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 14002 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 14003 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 14004 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 14005 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 14006 | `	  /* hashmap */` |
|        - | 14007 | `	{"compact",          vm_builtin_compact       },` |
|        - | 14008 | `	{"extract",          vm_builtin_extract       },` |
|        - | 14009 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 14010 | `	  /* URL related function */` |
|        - | 14011 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 14012 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 14013 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14014 | `	   /* XML processing functions */` |
|        - | 14015 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 14016 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 14017 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 14018 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 14019 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 14020 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 14021 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 14022 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 14023 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 14024 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 14025 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 14026 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 14027 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 14028 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 14029 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 14030 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 14031 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 14032 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 14033 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 14034 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 14035 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 14036 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 14037 | `	   /* UTF-8 encoding/decoding */` |
|        - | 14038 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 14039 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 14040 | `	   /* Command line processing */` |
|        - | 14041 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 14042 | `	   /* JSON encoding/decoding */` |
|        - | 14043 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 14044 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 14045 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 14046 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 14047 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 14048 | `	   /* Files/URI inclusion facility */` |
|        - | 14049 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 14050 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 14051 | `	{ "include",      vm_builtin_include          },` |
|        - | 14052 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 14053 | `	{ "require",      vm_builtin_require          },` |
|        - | 14054 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 14055 | `};` |
|        - | 14056 | `/*` |
|        - | 14057 | ` * Register the built-in VM functions defined above.` |
|        - | 14058 | ` */` |
|     1452 | 14059 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 14060 |  |
|        - | 14061 | `	sxi32 rc;` |
|        - | 14062 | `	sxu32 n;` |
|   181502 | 14063 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 14064 | `		/* Note that these special functions have access` |
|        - | 14065 | `		 * to the underlying virtual machine as their` |
|        - | 14066 | `		 * private data.` |
|        - | 14067 | `		 */` |
|   180050 | 14068 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   180050 | 14069 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 14070 | `			return rc;` |
|        - | 14071 | `		}` |
|    90026 | 14072 | `	}` |
|     1454 | 14073 | `	return SXRET_OK;` |
|      728 | 14074 |  |
|        - | 14075 | `/*` |
|        - | 14076 | ` * Check if the given name refer to an installed class.` |
|        - | 14077 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 14078 | ` */` |
|     9500 | 14079 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 14080 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 14081 | `	const char *zName,  /* Name of the target class */` |
|        - | 14082 | `	sxu32 nByte,        /* zName length */` |
|        - | 14083 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 14084 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 14085 | `						 */` |
|        - | 14086 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 14087 | `	)` |
|        2 | 14088 |  |
|        - | 14089 | `	SyHashEntry *pEntry;` |
|        - | 14090 | `	ph7_class *pClass;` |
|     4750 | 14091 | `		SXUNUSED(iNest);` |
|        - | 14092 | `	/* Perform a hash lookup */` |
|     9502 | 14093 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|        - | 14094 |  |
|     9502 | 14095 | `	if( pEntry == 0 ){` |
|        - | 14096 | `		/* No such entry,return NULL */` |
|      ! 0 | 14097 | `		return 0;` |
|        - | 14098 | `	}` |
|     9502 | 14099 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|     9502 | 14100 | `	if( !iLoadable ){` |
|        - | 14101 | `		/* Return the first class seen */` |
|     8714 | 14102 | `		return pClass;` |
|      ! 0 | 14103 | `	}else{` |
|        - | 14104 | `		/* Check the collision list */` |
|      790 | 14105 | `		while(pClass){` |
|      790 | 14106 | `			if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) == 0 ){` |
|        - | 14107 | `				/* Class is loadable */` |
|      790 | 14108 | `				return pClass;` |
|        - | 14109 | `			}` |
|        - | 14110 | `			/* Point to the next entry */` |
|      ! 0 | 14111 | `			pClass = pClass->pNextName;` |
|      ! 0 | 14112 | `		}` |
|        - | 14113 | `	}` |
|        - | 14114 | `	/* No such loadable class */` |
|      ! 0 | 14115 | `	return 0;` |
|     4752 | 14116 |  |
|        - | 14117 | `/*` |
|        - | 14118 | ` * Reference Table Implementation` |
|        - | 14119 | ` * Status: stable <chm@symisc.net>` |
|        - | 14120 | ` * Intro` |
|        - | 14121 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 14122 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 14123 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 14124 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 14125 | ` *  Refer to the official for more information on this powerful` |
|        - | 14126 | ` *  extension.` |
|        - | 14127 | ` */` |
|        - | 14128 | `/*` |
|        - | 14129 | ` * Allocate a new reference entry.` |
|        - | 14130 | ` */` |
|  2877676 | 14131 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 14132 |  |
|        - | 14133 | `	VmRefObj *pRef;` |
|        - | 14134 | `	/* Allocate a new instance */` |
|  2877678 | 14135 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  2877678 | 14136 | `	if( pRef == 0 ){` |
|      ! 0 | 14137 | `		return 0;` |
|        - | 14138 | `	}` |
|        - | 14139 | `	/* Zero the structure */` |
|  2877678 | 14140 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 14141 | `	/* Initialize fields */` |
|  2877678 | 14142 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  2877678 | 14143 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  2877678 | 14144 | `	pRef->nIdx = nIdx;` |
|  2877678 | 14145 | `	return pRef;` |
|  1438840 | 14146 |  |
|        - | 14147 | `/*` |
|        - | 14148 | ` * Default hash function used by the reference table` |
|        - | 14149 | ` * for lookup/insertion operations.` |
|        - | 14150 | ` */` |
| 16114153 | 14151 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 14152 |  |
|        - | 14153 | `	/* Calculate the hash based on the memory object index */` |
| 16114155 | 14154 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 14155 |  |
|        - | 14156 | `/*` |
|        - | 14157 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 14158 | ` * in the reference table.` |
|        - | 14159 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 14160 | ` * otherwise.` |
|        - | 14161 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14162 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14163 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14164 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14165 | ` * Refer to the official for more information on this powerful` |
|        - | 14166 | ` * extension.` |
|        - | 14167 | ` */` |
|  8601984 | 14168 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 14169 |  |
|        - | 14170 | `	VmRefObj *pRef;` |
|        - | 14171 | `	sxu32 nBucket;` |
|        - | 14172 | `	/* Point to the appropriate bucket */` |
|  8601986 | 14173 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 14174 | `	/* Perform the lookup */` |
|  8601986 | 14175 | `	pRef = pVm->apRefObj[nBucket];` |
| 17987598 | 14176 | `	for(;;){` |
| 35975595 | 14177 | `		if( pRef == 0 ){` |
|  2939882 | 14178 | `			break;` |
|        - | 14179 | `		}` |
| 33035715 | 14180 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 14181 | `			/* Entry found */` |
|  5662106 | 14182 | `			return pRef;` |
|        - | 14183 | `		}` |
|        - | 14184 | `		/* Point to the next entry */` |
| 27373611 | 14185 | `		pRef = pRef->pNextCollide;` |
|        2 | 14186 | `	}` |
|        - | 14187 | `	/* No such entry,return NULL */` |
|  2939882 | 14188 | `	return 0;` |
|  4300994 | 14189 |  |
|        - | 14190 | `/*` |
|        - | 14191 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14192 | ` *` |
|        - | 14193 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14194 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14195 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14196 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14197 | ` * Refer to the official for more information on this powerful` |
|        - | 14198 | ` * extension.` |
|        - | 14199 | ` */` |
|  2877676 | 14200 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14201 |  |
|        - | 14202 | `	sxu32 nBucket;` |
|  2877678 | 14203 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 14204 | `		VmRefObj **apNew;` |
|        - | 14205 | `		sxu32 nNew;` |
|        - | 14206 | `		/* Allocate a larger table */` |
|     2152 | 14207 | `		nNew = pVm->nRefSize << 1;` |
|     2152 | 14208 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     2152 | 14209 | `		if( apNew ){` |
|     2152 | 14210 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 14211 | `			sxu32 n;` |
|        - | 14212 | `			/* Zero the structure */` |
|     2152 | 14213 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 14214 | `			/* Rehash all referenced entries */` |
|  2820304 | 14215 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 14216 | `				/* Remove old collision links */` |
|  2818154 | 14217 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 14218 | `				/* Point to the appropriate bucket */` |
|  2818154 | 14219 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 14220 | `				/* Insert the entry  */` |
|  2818154 | 14221 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2818154 | 14222 | `				if( apNew[nBucket] ){` |
|  2298896 | 14223 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 14224 | `				}` |
|  2818154 | 14225 | `				apNew[nBucket] = pEntry;` |
|        - | 14226 | `				/* Point to the next entry */` |
|  2818154 | 14227 | `				pEntry = pEntry->pNext;` |
|  1409078 | 14228 | `			}` |
|        - | 14229 | `			/* Release the old table */` |
|     2152 | 14230 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 14231 | `			/* Install the new one */` |
|     2152 | 14232 | `			pVm->apRefObj = apNew;` |
|     2152 | 14233 | `			pVm->nRefSize = nNew;` |
|     1075 | 14234 | `		}` |
|     1075 | 14235 | `	}` |
|        - | 14236 | `	/* Point to the appropriate bucket */` |
|  2877678 | 14237 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 14238 | `	/* Insert the entry */` |
|  2877678 | 14239 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  2877678 | 14240 | `	if( pVm->apRefObj[nBucket] ){` |
|  2382813 | 14241 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1191459 | 14242 | `	}` |
|  2877678 | 14243 | `	pVm->apRefObj[nBucket] = pRef;` |
|  2877678 | 14244 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  2877678 | 14245 | `	pVm->nRefUsed++;` |
|  2877678 | 14246 | `	return SXRET_OK;` |
|        2 | 14247 |  |
|        - | 14248 | `/*` |
|        - | 14249 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 14250 | ` * the reference table.` |
|        - | 14251 | ` * This function is invoked when the user perform an unset` |
|        - | 14252 | ` * call [i.e: unset($var); ].` |
|        - | 14253 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14254 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14255 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14256 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14257 | ` * Refer to the official for more information on this powerful` |
|        - | 14258 | ` * extension.` |
|        - | 14259 | ` */` |
|  2855794 | 14260 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14261 |  |
|        - | 14262 | `	ph7_hashmap_node **apNode;` |
|        - | 14263 | `	SyHashEntry **apEntry;` |
|        - | 14264 | `	sxu32 n;` |
|        - | 14265 | `	/* Point to the reference table */` |
|  2855796 | 14266 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2855796 | 14267 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 14268 | `	/* Unlink the entry from the reference table */` |
|  2922784 | 14269 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    66990 | 14270 | `		if( apEntry[n] ){` |
|    66958 | 14271 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    33478 | 14272 | `		}` |
|    33496 | 14273 | `	}` |
|  5647618 | 14274 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2791824 | 14275 | `		if( apNode[n] ){` |
|     5473 | 14276 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2736 | 14277 | `		}` |
|  1395913 | 14278 | `	}` |
|  2855796 | 14279 | `	if( pRef->pPrevCollide ){` |
|  1039455 | 14280 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   519705 | 14281 | `	}else{` |
|  1816343 | 14282 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 14283 | `	}` |
|  2855796 | 14284 | `	if( pRef->pNextCollide ){` |
|  1579820 | 14285 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   789887 | 14286 | `	}` |
|  2855796 | 14287 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 14288 | `	/* Release the node */` |
|  2855796 | 14289 | `	SySetRelease(&pRef->aReference);` |
|  2855796 | 14290 | `	SySetRelease(&pRef->aArrEntries);` |
|  2855796 | 14291 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2855796 | 14292 | `	pVm->nRefUsed--;` |
|  2855796 | 14293 | `	return SXRET_OK;` |
|        2 | 14294 |  |
|        - | 14295 | `/*` |
|        - | 14296 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14297 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14298 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14299 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14300 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14301 | ` * Refer to the official for more information on this powerful` |
|        - | 14302 | ` * extension.` |
|        - | 14303 | ` */` |
|  2897598 | 14304 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 14305 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14306 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14307 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14308 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 14309 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 14310 | `	)` |
|        2 | 14311 |  |
|  2897600 | 14312 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 14313 | `	VmRefObj *pRef;` |
|        - | 14314 | `	/* Check if the referenced object already exists */` |
|  2897600 | 14315 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2897600 | 14316 | `	if( pRef == 0 ){` |
|        - | 14317 | `		/* Create a new entry */` |
|  2877678 | 14318 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  2877678 | 14319 | `		if( pRef == 0 ){` |
|      ! 0 | 14320 | `			return SXERR_MEM;` |
|        - | 14321 | `		}` |
|  2877678 | 14322 | `		pRef->iFlags = iFlags;` |
|        - | 14323 | `		/* Install the entry */` |
|  2877678 | 14324 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1438838 | 14325 | `	}` |
|  2902512 | 14326 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 14327 | `		/* Safely ignore the exception frame */` |
|     4914 | 14328 | `		pFrame = pFrame->pParent;` |
|        2 | 14329 | `	}` |
|  2897600 | 14330 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 14331 | `		VmSlot sRef;` |
|        - | 14332 | `		/* Local frame,record referenced entry so that it can` |
|        - | 14333 | `		 * be deleted when we leave this frame.` |
|        - | 14334 | `		 */` |
|    62218 | 14335 | `		sRef.nIdx = nIdx;` |
|    62218 | 14336 | `		sRef.pUserData = pEntry;` |
|    62218 | 14337 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 14338 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 14339 | `		}` |
|    31108 | 14340 | `	}` |
|  2897600 | 14341 | `	if( pEntry ){` |
|        - | 14342 | `		/* Address of the hash-entry */` |
|    81974 | 14343 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    40986 | 14344 | `	}` |
|  2897600 | 14345 | `	if( pMapEntry ){` |
|        - | 14346 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2812160 | 14347 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1406079 | 14348 | `	}` |
|  2897600 | 14349 | `	return SXRET_OK;` |
|  1448801 | 14350 |  |
|        - | 14351 | `/*` |
|        - | 14352 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 14353 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14354 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14355 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14356 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14357 | ` * Refer to the official for more information on this powerful` |
|        - | 14358 | ` * extension.` |
|        - | 14359 | ` */` |
|  2848572 | 14360 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 14361 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14362 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14363 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14364 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 14365 | `	)` |
|        2 | 14366 |  |
|        - | 14367 | `	VmRefObj *pRef;` |
|        - | 14368 | `	sxu32 n;` |
|        - | 14369 | `	/* Check if the referenced object already exists */` |
|  2848574 | 14370 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2848574 | 14371 | `	if( pRef == 0 ){` |
|        - | 14372 | `		/* Not such entry */` |
|    62186 | 14373 | `		return SXERR_NOTFOUND;` |
|        - | 14374 | `	}` |
|        - | 14375 | `	/* Remove the desired entry */` |
|  2786390 | 14376 | `	if( pEntry ){` |
|        - | 14377 | `		SyHashEntry **apEntry;` |
|       33 | 14378 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      129 | 14379 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|       97 | 14380 | `			if( apEntry[n] == pEntry ){` |
|        - | 14381 | `				/* Nullify the entry */` |
|       33 | 14382 | `				apEntry[n] = 0;` |
|        - | 14383 | `				/*` |
|        - | 14384 | `				 * NOTE:` |
|        - | 14385 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 14386 | `				 * we avoid wasting spaces.` |
|        - | 14387 | `				 */` |
|       16 | 14388 | `			}` |
|       49 | 14389 | `		}` |
|       16 | 14390 | `	}` |
|  2786390 | 14391 | `	if( pMapEntry ){` |
|        - | 14392 | `		ph7_hashmap_node **apNode;` |
|  2786358 | 14393 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5572802 | 14394 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2786446 | 14395 | `			if( apNode[n] == pMapEntry ){` |
|        - | 14396 | `				/* nullify the entry */` |
|  2786358 | 14397 | `				apNode[n] = 0;` |
|  1393178 | 14398 | `			}` |
|  1393224 | 14399 | `		}` |
|  1393178 | 14400 | `	}` |
|  2786390 | 14401 | `	return SXRET_OK;` |
|  1424288 | 14402 |  |
|        - | 14403 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 14404 | `/*` |
|        - | 14405 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 14406 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 14407 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 14408 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 14409 | ` * For more information on how to register IO stream devices,please` |
|        - | 14410 | ` * refer to the official documentation.` |
|        - | 14411 | ` */` |
|    20916 | 14412 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 14413 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 14414 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 14415 | `	int nByte              /* *pzDevice length*/` |
|        - | 14416 | `	)` |
|        2 | 14417 |  |
|        - | 14418 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 14419 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 14420 | `	SyString sDev,sCur;` |
|        - | 14421 | `	sxu32 n,nEntry;` |
|        - | 14422 | `	int rc;` |
|        - | 14423 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    20918 | 14424 | `	zNext = zCur = zIn = *pzDevice;` |
|    20918 | 14425 | `	zEnd = &zIn[nByte];` |
|  1317385 | 14426 | `	while( zIn < zEnd ){` |
|  1296471 | 14427 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 14428 | `			/* Got one */` |
|        3 | 14429 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 14430 | `			break;` |
|        - | 14431 | `		}` |
|        - | 14432 | `		/* Advance the cursor */` |
|  1296469 | 14433 | `		zIn++;` |
|        2 | 14434 | `	}` |
|    20918 | 14435 | `	if( zIn >= zEnd ){` |
|        - | 14436 | `		/* No such scheme,return the default stream */` |
|    20916 | 14437 | `		return pVm->pDefStream;` |
|        - | 14438 | `	}` |
|        3 | 14439 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 14440 | `	/* Remove leading and trailing white spaces */` |
|        3 | 14441 | `	SyStringFullTrim(&sDev);` |
|        - | 14442 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 14443 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 14444 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 14445 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 14446 | `		pStream = apStream[n];` |
|        3 | 14447 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 14448 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 14449 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 14450 | `		if( rc == 0 ){` |
|        - | 14451 | `			/* Stream device found */` |
|        3 | 14452 | `			*pzDevice = zNext;` |
|        3 | 14453 | `			return pStream;` |
|        - | 14454 | `		}` |
|      ! 0 | 14455 | `	}` |
|        - | 14456 | `	/* No such stream,return NULL */` |
|      ! 0 | 14457 | `	return 0;` |
|    10460 | 14458 |  |
|        - | 14459 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 14460 | `/*` |
|        - | 14461 | ` * Section:` |
|        - | 14462 | ` *    HTTP/URI related routines.` |
|        - | 14463 | ` * Status:` |
|        - | 14464 | ` *    Stable.` |
|        - | 14465 | ` */` |
|        - | 14466 | ` /*` |
|        - | 14467 | `  * URI Parser: Split an URI into components [i.e: Host,Path,Query,...].` |
|        - | 14468 | `  * URI syntax: [method:/][/[user[:pwd]@]host[:port]/][document]` |
|        - | 14469 | `  * This almost, but not quite, RFC1738 URI syntax.` |
|        - | 14470 | `  * This routine is not a validator,it does not check for validity` |
|        - | 14471 | `  * nor decode URI parts,the only thing this routine does is splitting` |
|        - | 14472 | `  * the input to its fields.` |
|        - | 14473 | `  * Upper layer are responsible of decoding and validating URI parts.` |
|        - | 14474 | `  * On success,this function populate the "SyhttpUri" structure passed` |
|        - | 14475 | `  * as the first argument. Otherwise SXERR_* is returned when a malformed` |
|        - | 14476 | `  * input is encountered.` |
|        - | 14477 | `  */` |
|       26 | 14478 | ` static sxi32 VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen)` |
|        1 | 14479 | ` {` |
|       27 | 14480 | `	 const char *zEnd = &zUri[nLen];` |
|       27 | 14481 | `	 sxu8 bHostOnly = FALSE;` |
|       27 | 14482 | `	 sxu8 bIPv6 = FALSE	;` |
|        - | 14483 | `	 const char *zCur;` |
|        - | 14484 | `	 SyString *pComp;` |
|       27 | 14485 | `	 sxu32 nPos = 0;` |
|        - | 14486 | `	 sxi32 rc;` |
|        - | 14487 | `	 /* Zero the structure first */` |
|       27 | 14488 | `	 SyZero(pOut,sizeof(SyhttpUri));` |
|        - | 14489 | `	 /* Remove leading and trailing white spaces  */` |
|       27 | 14490 | `	 SyStringInitFromBuf(&pOut->sRaw,zUri,nLen);` |
|       27 | 14491 | `	 SyStringFullTrim(&pOut->sRaw);` |
|        - | 14492 | `	 /* Find the first '/' separator */` |
|       27 | 14493 | `	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);` |
|       27 | 14494 | `	 if( rc != SXRET_OK ){` |
|        - | 14495 | `		 /* Assume a host name only */` |
|        7 | 14496 | `		 zCur = zEnd;` |
|        7 | 14497 | `		 bHostOnly = TRUE;` |
|        7 | 14498 | `		 goto ProcessHost;` |
|        - | 14499 | `	 }` |
|       21 | 14500 | `	 zCur = &zUri[nPos];` |
|       21 | 14501 | `	 if( zUri != zCur && zCur[-1] == ':' ){` |
|        - | 14502 | `		 /* Extract a scheme:` |
|        - | 14503 | `		  * Not that we can get an invalid scheme here.` |
|        - | 14504 | `		  * Fortunately the caller can discard any URI by comparing this scheme with its` |
|        - | 14505 | `		  * registered schemes and will report the error as soon as his comparison function` |
|        - | 14506 | `		  * fail.` |
|        - | 14507 | `		  */` |
|       19 | 14508 | `	 	pComp = &pOut->sScheme;` |
|       19 | 14509 | `		SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri - 1));` |
|       19 | 14510 | `		SyStringLeftTrim(pComp);` |
|        9 | 14511 | `	 }` |
|       21 | 14512 | `	 if( zCur[1] != '/' ){` |
|      ! 0 | 14513 | `		 if( zCur == zUri \|\| zCur[-1] == ':' ){` |
|        - | 14514 | `		  /* No authority */` |
|      ! 0 | 14515 | `		  goto PathSplit;` |
|        - | 14516 | `		}` |
|        - | 14517 | `		 /* There is something here , we will assume its an authority` |
|        - | 14518 | `		  * and someone has forgot the two prefix slashes "//",` |
|        - | 14519 | `		  * sooner or later we will detect if we are dealing with a malicious` |
|        - | 14520 | `		  * user or not,but now assume we are dealing with an authority` |
|        - | 14521 | `		  * and let the caller handle all the validation process.` |
|        - | 14522 | `		  */` |
|      ! 0 | 14523 | `		 goto ProcessHost;` |
|        - | 14524 | `	 }` |
|       21 | 14525 | `	 zUri = &zCur[2];` |
|       21 | 14526 | `	 zCur = zEnd;` |
|       21 | 14527 | `	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);` |
|       29 | 14528 | `	 if( rc == SXRET_OK ){` |
|       17 | 14529 | `		 zCur = &zUri[nPos];` |
|        8 | 14530 | `	 }` |
|        2 | 14531 | ` ProcessHost:` |
|        - | 14532 | `	 /* Extract user information if present */` |
|       27 | 14533 | `	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),'@',&nPos);` |
|       27 | 14534 | `	 if( rc == SXRET_OK ){` |
|        7 | 14535 | `		 if( nPos > 0 ){` |
|        - | 14536 | `			 sxu32 nPassOfft; /* Password offset */` |
|        7 | 14537 | `			 pComp = &pOut->sUser;` |
|        7 | 14538 | `			 SyStringInitFromBuf(pComp,zUri,nPos);` |
|        - | 14539 | `			 /* Extract the password if available */` |
|        7 | 14540 | `			 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPassOfft);` |
|        7 | 14541 | `			 if( rc == SXRET_OK && nPassOfft < nPos){` |
|        7 | 14542 | `				 pComp->nByte = nPassOfft;` |
|        7 | 14543 | `				 pComp = &pOut->sPass;` |
|        7 | 14544 | `				 pComp->zString = &zUri[nPassOfft+sizeof(char)];` |
|        7 | 14545 | `				 pComp->nByte = nPos - nPassOfft - 1;` |
|        3 | 14546 | `			 }` |
|        - | 14547 | `			 /* Update the cursor */` |
|        7 | 14548 | `			 zUri = &zUri[nPos+1];` |
|        4 | 14549 | `		 }else{` |
|      ! 0 | 14550 | `			 zUri++;` |
|        - | 14551 | `		 }` |
|        3 | 14552 | `	 }` |
|       27 | 14553 | `	 pComp = &pOut->sHost;` |
|       27 | 14554 | `	 while( zUri < zCur && SyisSpace(zUri[0])){` |
|      ! 0 | 14555 | `		 zUri++;` |
|      ! 0 | 14556 | `	 }` |
|       27 | 14557 | `	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri));` |
|       27 | 14558 | `	 if( pComp->zString[0] == '[' ){` |
|        - | 14559 | `		 /* An IPv6 Address: Make a simple naive test` |
|        - | 14560 | `		  */` |
|        3 | 14561 | `		 zUri++; pComp->zString++; pComp->nByte = 0;` |
|        9 | 14562 | `		 while( ((unsigned char)zUri[0] < 0xc0 && SyisHex(zUri[0])) \|\| zUri[0] == ':' ){` |
|        7 | 14563 | `			 zUri++; pComp->nByte++;` |
|        1 | 14564 | `		 }` |
|        3 | 14565 | `		 if( zUri[0] != ']' ){` |
|      ! 0 | 14566 | `			 return SXERR_CORRUPT; /* Malformed IPv6 address */` |
|        - | 14567 | `		 }` |
|        3 | 14568 | `		 zUri++;` |
|        3 | 14569 | `		 bIPv6 = TRUE;` |
|        1 | 14570 | `	 }` |
|        - | 14571 | `	 /* Extract a port number if available */` |
|       27 | 14572 | `	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPos);` |
|       27 | 14573 | `	 if( rc == SXRET_OK ){` |
|       11 | 14574 | `		 if( bIPv6 == FALSE ){` |
|       11 | 14575 | `			 pComp->nByte = (sxu32)(&zUri[nPos] - zUri);` |
|        5 | 14576 | `		 }` |
|       11 | 14577 | `		 pComp = &pOut->sPort;` |
|       11 | 14578 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zCur - &zUri[nPos+1]));` |
|        5 | 14579 | `	 }` |
|       27 | 14580 | `	 if( bHostOnly == TRUE ){` |
|        7 | 14581 | `		 return SXRET_OK;` |
|        - | 14582 | `	 }` |
|       10 | 14583 | `PathSplit:` |
|       21 | 14584 | `	 zUri = zCur;` |
|       21 | 14585 | `	 pComp = &pOut->sPath;` |
|       21 | 14586 | `	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zEnd-zUri));` |
|       21 | 14587 | `	 if( pComp->nByte == 0 ){` |
|        5 | 14588 | `		 return SXRET_OK; /* Empty path */` |
|        - | 14589 | `	 }` |
|       17 | 14590 | `	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'?',&nPos) ){` |
|        5 | 14591 | `		 pComp->nByte = nPos; /* Update path length */` |
|        5 | 14592 | `		 pComp = &pOut->sQuery;` |
|        5 | 14593 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]));` |
|        2 | 14594 | `	 }` |
|       17 | 14595 | `	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'#',&nPos) ){` |
|        - | 14596 | `		 /* Update path or query length */` |
|        5 | 14597 | `		 if( pComp == &pOut->sPath ){` |
|      ! 0 | 14598 | `			 pComp->nByte = nPos;` |
|      ! 0 | 14599 | `		 }else{` |
|        5 | 14600 | `			 if( &zUri[nPos] < (char *)SyStringData(pComp) ){` |
|        - | 14601 | `				 /* Malformed syntax : Query must be present before fragment */` |
|      ! 0 | 14602 | `				 return SXERR_SYNTAX;` |
|        - | 14603 | `			 }` |
|        5 | 14604 | `			 pComp->nByte -= (sxu32)(zEnd - &zUri[nPos]);` |
|        - | 14605 | `		 }` |
|        5 | 14606 | `		 pComp = &pOut->sFragment;` |
|        5 | 14607 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]))` |
|        2 | 14608 | `	 }` |
|       17 | 14609 | `	 return SXRET_OK;` |
|       14 | 14610 | ` }` |
|        - | 14611 | ` /*` |
|        - | 14612 | ` * Extract a single line from a raw HTTP request.` |
|        - | 14613 | ` * Return SXRET_OK on success,SXERR_EOF when end of input` |
|        - | 14614 | ` * and SXERR_MORE when more input is needed.` |
|        - | 14615 | ` */` |
|      ! 0 | 14616 | `static sxi32 VmGetNextLine(SyString *pCursor,SyString *pCurrent)` |
|      ! 0 | 14617 |  |
|        - | 14618 | `  	const char *zIn;` |
|        - | 14619 | `  	sxu32 nPos;` |
|        - | 14620 | `	/* Jump leading white spaces */` |
|      ! 0 | 14621 | `	SyStringLeftTrim(pCursor);` |
|      ! 0 | 14622 | `	if( pCursor->nByte < 1 ){` |
|      ! 0 | 14623 | `		SyStringInitFromBuf(pCurrent,0,0);` |
|      ! 0 | 14624 | `		return SXERR_EOF; /* End of input */` |
|        - | 14625 | `	}` |
|      ! 0 | 14626 | `	zIn = SyStringData(pCursor);` |
|      ! 0 | 14627 | `	if( SXRET_OK != SyByteListFind(pCursor->zString,pCursor->nByte,"\r\n",&nPos) ){` |
|        - | 14628 | `		/* Line not found,tell the caller to read more input from source */` |
|      ! 0 | 14629 | `		SyStringDupPtr(pCurrent,pCursor);` |
|      ! 0 | 14630 | `		return SXERR_MORE;` |
|        - | 14631 | `	}` |
|      ! 0 | 14632 | `  	pCurrent->zString = zIn;` |
|      ! 0 | 14633 | `  	pCurrent->nByte	= nPos;` |
|        - | 14634 | `  	/* advance the cursor so we can call this routine again */` |
|      ! 0 | 14635 | `  	pCursor->zString = &zIn[nPos];` |
|      ! 0 | 14636 | `  	pCursor->nByte -= nPos;` |
|      ! 0 | 14637 | `  	return SXRET_OK;` |
|      ! 0 | 14638 | ` }` |
|        - | 14639 | ` /*` |
|        - | 14640 | `  * Split a single MIME header into a name value pair.` |
|        - | 14641 | `  * This function return SXRET_OK,SXERR_CONTINUE on success.` |
|        - | 14642 | `  * Otherwise SXERR_NEXT is returned when a malformed header` |
|        - | 14643 | `  * is encountered.` |
|        - | 14644 | `  * Note: This function handle also mult-line headers.` |
|        - | 14645 | `  */` |
|      ! 0 | 14646 | ` static sxi32 VmHttpProcessOneHeader(SyhttpHeader *pHdr,SyhttpHeader *pLast,const char *zLine,sxu32 nLen)` |
|      ! 0 | 14647 | ` {` |
|        - | 14648 | `	 SyString *pName;` |
|        - | 14649 | `	 sxu32 nPos;` |
|        - | 14650 | `	 sxi32 rc;` |
|      ! 0 | 14651 | `	 if( nLen < 1 ){` |
|      ! 0 | 14652 | `		 return SXERR_NEXT;` |
|        - | 14653 | `	 }` |
|        - | 14654 | `	 /* Check for multi-line header */` |
|      ! 0 | 14655 | `	if( pLast && (zLine[-1] == ' ' \|\| zLine[-1] == '\t') ){` |
|      ! 0 | 14656 | `		SyString *pTmp = &pLast->sValue;` |
|      ! 0 | 14657 | `		SyStringFullTrim(pTmp);` |
|      ! 0 | 14658 | `		if( pTmp->nByte == 0 ){` |
|      ! 0 | 14659 | `			SyStringInitFromBuf(pTmp,zLine,nLen);` |
|      ! 0 | 14660 | `		}else{` |
|        - | 14661 | `			/* Update header value length */` |
|      ! 0 | 14662 | `			pTmp->nByte = (sxu32)(&zLine[nLen] - pTmp->zString);` |
|        - | 14663 | `		}` |
|        - | 14664 | `		 /* Simply tell the caller to reset its states and get another line */` |
|      ! 0 | 14665 | `		 return SXERR_CONTINUE;` |
|        - | 14666 | `	 }` |
|        - | 14667 | `	/* Split the header */` |
|      ! 0 | 14668 | `	pName = &pHdr->sName;` |
|      ! 0 | 14669 | `	rc = SyByteFind(zLine,nLen,':',&nPos);` |
|      ! 0 | 14670 | `	if(rc != SXRET_OK ){` |
|      ! 0 | 14671 | `		return SXERR_NEXT; /* Malformed header;Check the next entry */` |
|        - | 14672 | `	}` |
|      ! 0 | 14673 | `	SyStringInitFromBuf(pName,zLine,nPos);` |
|      ! 0 | 14674 | `	SyStringFullTrim(pName);` |
|        - | 14675 | `	/* Extract a header value */` |
|      ! 0 | 14676 | `	SyStringInitFromBuf(&pHdr->sValue,&zLine[nPos + 1],nLen - nPos - 1);` |
|        - | 14677 | `	/* Remove leading and trailing whitespaces */` |
|      ! 0 | 14678 | `	SyStringFullTrim(&pHdr->sValue);` |
|      ! 0 | 14679 | `	return SXRET_OK;` |
|      ! 0 | 14680 | ` }` |
|        - | 14681 | ` /*` |
|        - | 14682 | `  * Extract all MIME headers associated with a HTTP request.` |
|        - | 14683 | `  * After processing the first line of a HTTP request,the following` |
|        - | 14684 | `  * routine is called in order to extract MIME headers.` |
|        - | 14685 | `  * This function return SXRET_OK on success,SXERR_MORE when it needs` |
|        - | 14686 | `  * more inputs.` |
|        - | 14687 | `  * Note: Any malformed header is simply discarded.` |
|        - | 14688 | `  */` |
|      ! 0 | 14689 | ` static sxi32 VmHttpExtractHeaders(SyString *pRequest,SySet *pOut)` |
|      ! 0 | 14690 | ` {` |
|      ! 0 | 14691 | `	 SyhttpHeader *pLast = 0;` |
|        - | 14692 | `	 SyString sCurrent;` |
|        - | 14693 | `	 SyhttpHeader sHdr;` |
|        - | 14694 | `	 sxu8 bEol;` |
|        - | 14695 | `	 sxi32 rc;` |
|      ! 0 | 14696 | `	 if( SySetUsed(pOut) > 0 ){` |
|      ! 0 | 14697 | `		 pLast = (SyhttpHeader *)SySetAt(pOut,SySetUsed(pOut)-1);` |
|      ! 0 | 14698 | `	 }` |
|      ! 0 | 14699 | `	 bEol = FALSE;` |
|      ! 0 | 14700 | `	 for(;;){` |
|      ! 0 | 14701 | `		 SyZero(&sHdr,sizeof(SyhttpHeader));` |
|        - | 14702 | `		 /* Extract a single line from the raw HTTP request */` |
|      ! 0 | 14703 | `		 rc = VmGetNextLine(pRequest,&sCurrent);` |
|      ! 0 | 14704 | `		 if(rc != SXRET_OK ){` |
|      ! 0 | 14705 | `			 if( sCurrent.nByte < 1 ){` |
|      ! 0 | 14706 | `				 break;` |
|        - | 14707 | `			 }` |
|      ! 0 | 14708 | `			 bEol = TRUE;` |
|      ! 0 | 14709 | `		 }` |
|        - | 14710 | `		 /* Process the header */` |
|      ! 0 | 14711 | `		 if( SXRET_OK == VmHttpProcessOneHeader(&sHdr,pLast,sCurrent.zString,sCurrent.nByte)){` |
|      ! 0 | 14712 | `			 if( SXRET_OK != SySetPut(pOut,(const void *)&sHdr) ){` |
|      ! 0 | 14713 | `				 break;` |
|        - | 14714 | `			 }` |
|        - | 14715 | `			 /* Retrieve the last parsed header so we can handle multi-line header` |
|        - | 14716 | `			  * in case we face one of them.` |
|        - | 14717 | `			  */` |
|      ! 0 | 14718 | `			 pLast = (SyhttpHeader *)SySetPeek(pOut);` |
|      ! 0 | 14719 | `		 }` |
|      ! 0 | 14720 | `		 if( bEol ){` |
|      ! 0 | 14721 | `			 break;` |
|        - | 14722 | `		 }` |
|      ! 0 | 14723 | `	 } /* for(;;) */` |
|      ! 0 | 14724 | `	 return SXRET_OK;` |
|      ! 0 | 14725 | ` }` |
|        - | 14726 | ` /*` |
|        - | 14727 | `  * Process the first line of a HTTP request.` |
|        - | 14728 | `  * This routine perform the following operations` |
|        - | 14729 | `  *  1) Extract the HTTP method.` |
|        - | 14730 | `  *  2) Split the request URI to it's fields [ie: host,path,query,...].` |
|        - | 14731 | `  *  3) Extract the HTTP protocol version.` |
|        - | 14732 | `  */` |
|      ! 0 | 14733 | ` static sxi32 VmHttpProcessFirstLine(` |
|        - | 14734 | `	 SyString *pRequest, /* Raw HTTP request */` |
|        - | 14735 | `	 sxi32 *pMethod,     /* OUT: HTTP method */` |
|        - | 14736 | `	 SyhttpUri *pUri,    /* OUT: Parse of the URI */` |
|        - | 14737 | `	 sxi32 *pProto       /* OUT: HTTP protocol */` |
|        - | 14738 | `	 )` |
|      ! 0 | 14739 | ` {` |
|        - | 14740 | `	 static const char *azMethods[] = { "get","post","head","put"};` |
|        - | 14741 | `	 static const sxi32 aMethods[]  = { HTTP_METHOD_GET,HTTP_METHOD_POST,HTTP_METHOD_HEAD,HTTP_METHOD_PUT};` |
|        - | 14742 | `	 const char *zIn,*zEnd,*zPtr;` |
|        - | 14743 | `	 SyString sLine;` |
|        - | 14744 | `	 sxu32 nLen;` |
|        - | 14745 | `	 sxi32 rc;` |
|        - | 14746 | `	 /* Extract the first line and update the pointer */` |
|      ! 0 | 14747 | `	 rc = VmGetNextLine(pRequest,&sLine);` |
|      ! 0 | 14748 | `	 if( rc != SXRET_OK ){` |
|      ! 0 | 14749 | `		 return rc;` |
|        - | 14750 | `	 }` |
|      ! 0 | 14751 | `	 if ( sLine.nByte < 1 ){` |
|        - | 14752 | `		 /* Empty HTTP request */` |
|      ! 0 | 14753 | `		 return SXERR_EMPTY;` |
|        - | 14754 | `	 }` |
|        - | 14755 | `	 /* Delimit the line and ignore trailing and leading white spaces */` |
|      ! 0 | 14756 | `	 zIn = sLine.zString;` |
|      ! 0 | 14757 | `	 zEnd = &zIn[sLine.nByte];` |
|      ! 0 | 14758 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14759 | `		 zIn++;` |
|      ! 0 | 14760 | `	 }` |
|        - | 14761 | `	 /* Extract the HTTP method */` |
|      ! 0 | 14762 | `	 zPtr = zIn;` |
|      ! 0 | 14763 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14764 | `		 zIn++;` |
|      ! 0 | 14765 | `	 }` |
|      ! 0 | 14766 | `	 *pMethod = HTTP_METHOD_OTHR;` |
|      ! 0 | 14767 | `	 if( zIn > zPtr ){` |
|        - | 14768 | `		 sxu32 i;` |
|      ! 0 | 14769 | `		 nLen = (sxu32)(zIn-zPtr);` |
|      ! 0 | 14770 | `		 for( i = 0 ; i < SX_ARRAYSIZE(azMethods) ; ++i ){` |
|      ! 0 | 14771 | `			 if( SyStrnicmp(azMethods[i],zPtr,nLen) == 0 ){` |
|      ! 0 | 14772 | `				 *pMethod = aMethods[i];` |
|      ! 0 | 14773 | `				 break;` |
|        - | 14774 | `			 }` |
|      ! 0 | 14775 | `		 }` |
|      ! 0 | 14776 | `	 }` |
|        - | 14777 | `	 /* Jump trailing white spaces */` |
|      ! 0 | 14778 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14779 | `		 zIn++;` |
|      ! 0 | 14780 | `	 }` |
|        - | 14781 | `	  /* Extract the request URI */` |
|      ! 0 | 14782 | `	 zPtr = zIn;` |
|      ! 0 | 14783 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14784 | `		 zIn++;` |
|      ! 0 | 14785 | `	 }` |
|      ! 0 | 14786 | `	 if( zIn > zPtr ){` |
|      ! 0 | 14787 | `		 nLen = (sxu32)(zIn-zPtr);` |
|        - | 14788 | `		 /* Split raw URI to it's fields */` |
|      ! 0 | 14789 | `		 VmHttpSplitURI(pUri,zPtr,nLen);` |
|      ! 0 | 14790 | `	 }` |
|        - | 14791 | `	 /* Jump trailing white spaces */` |
|      ! 0 | 14792 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14793 | `		 zIn++;` |
|      ! 0 | 14794 | `	 }` |
|        - | 14795 | `	 /* Extract the HTTP version */` |
|      ! 0 | 14796 | `	 zPtr = zIn;` |
|      ! 0 | 14797 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14798 | `		 zIn++;` |
|      ! 0 | 14799 | `	 }` |
|      ! 0 | 14800 | `	 *pProto = HTTP_PROTO_11; /* HTTP/1.1 */` |
|      ! 0 | 14801 | `	 rc = 1;` |
|      ! 0 | 14802 | `	 if( zIn > zPtr ){` |
|      ! 0 | 14803 | `		 rc = SyStrnicmp(zPtr,"http/1.0",(sxu32)(zIn-zPtr));` |
|      ! 0 | 14804 | `	 }` |
|      ! 0 | 14805 | `	 if( !rc ){` |
|      ! 0 | 14806 | `		 *pProto = HTTP_PROTO_10; /* HTTP/1.0 */` |
|      ! 0 | 14807 | `	 }` |
|      ! 0 | 14808 | `	 return SXRET_OK;` |
|      ! 0 | 14809 | ` }` |
|        - | 14810 | ` /*` |
|        - | 14811 | `  * Tokenize,decode and split a raw query encoded as: "x-www-form-urlencoded"` |
|        - | 14812 | `  * into a name value pair.` |
|        - | 14813 | `  * Note that this encoding is implicit in GET based requests.` |
|        - | 14814 | `  * After the tokenization process,register the decoded queries` |
|        - | 14815 | `  * in the $_GET/$_POST/$_REQUEST superglobals arrays.` |
|        - | 14816 | `  */` |
|      ! 0 | 14817 | ` static sxi32 VmHttpSplitEncodedQuery(` |
|        - | 14818 | `	 ph7_vm *pVm,       /* Target VM */` |
|        - | 14819 | `	 SyString *pQuery,  /* Raw query to decode */` |
|        - | 14820 | `	 SyBlob *pWorker,   /* Working buffer */` |
|        - | 14821 | `	 int is_post        /* TRUE if we are dealing with a POST request */` |
|        - | 14822 | `	 )` |
|      ! 0 | 14823 | ` {` |
|      ! 0 | 14824 | `	 const char *zEnd = &pQuery->zString[pQuery->nByte];` |
|      ! 0 | 14825 | `	 const char *zIn = pQuery->zString;` |
|        - | 14826 | `	 ph7_value *pGet,*pRequest;` |
|        - | 14827 | `	 SyString sName,sValue;` |
|        - | 14828 | `	 const char *zPtr;` |
|        - | 14829 | `	 sxu32 nBlobOfft;` |
|        - | 14830 | `	 /* Extract superglobals */` |
|      ! 0 | 14831 | `	 if( is_post ){` |
|        - | 14832 | `		 /* $_POST superglobal */` |
|      ! 0 | 14833 | `		 pGet = VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|      ! 0 | 14834 | `	 }else{` |
|        - | 14835 | `		 /* $_GET superglobal */` |
|      ! 0 | 14836 | `		 pGet = VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|        - | 14837 | `	 }` |
|      ! 0 | 14838 | `	 pRequest = VmExtractSuper(&(*pVm),"_REQUEST",sizeof("_REQUEST")-1);` |
|        - | 14839 | `	 /* Split up the raw query */` |
|      ! 0 | 14840 | `	 for(;;){` |
|        - | 14841 | `		 /* Jump leading white spaces */` |
|      ! 0 | 14842 | `		 while(zIn < zEnd  && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14843 | `			 zIn++;` |
|      ! 0 | 14844 | `		 }` |
|      ! 0 | 14845 | `		 if( zIn >= zEnd ){` |
|      ! 0 | 14846 | `			 break;` |
|        - | 14847 | `		 }` |
|      ! 0 | 14848 | `		 zPtr = zIn;` |
|      ! 0 | 14849 | `		 while( zPtr < zEnd && zPtr[0] != '=' && zPtr[0] != '&' && zPtr[0] != ';' ){` |
|      ! 0 | 14850 | `			 zPtr++;` |
|      ! 0 | 14851 | `		 }` |
|        - | 14852 | `		 /* Reset the working buffer */` |
|      ! 0 | 14853 | `		 SyBlobReset(pWorker);` |
|        - | 14854 | `		 /* Decode the entry */` |
|      ! 0 | 14855 | `		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|        - | 14856 | `		 /* Save the entry */` |
|      ! 0 | 14857 | `		 sName.nByte = SyBlobLength(pWorker);` |
|      ! 0 | 14858 | `		 sValue.zString = 0;` |
|      ! 0 | 14859 | `		 sValue.nByte = 0;` |
|      ! 0 | 14860 | `		 if( zPtr < zEnd && zPtr[0] == '=' ){` |
|      ! 0 | 14861 | `			 zPtr++;` |
|      ! 0 | 14862 | `			 zIn = zPtr;` |
|        - | 14863 | `			 /* Store field value */` |
|      ! 0 | 14864 | `			 while( zPtr < zEnd && zPtr[0] != '&' && zPtr[0] != ';' ){` |
|      ! 0 | 14865 | `				 zPtr++;` |
|      ! 0 | 14866 | `			 }` |
|      ! 0 | 14867 | `			 if( zPtr > zIn ){` |
|        - | 14868 | `				 /* Decode the value */` |
|      ! 0 | 14869 | `				  nBlobOfft = SyBlobLength(pWorker);` |
|      ! 0 | 14870 | `				  SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14871 | `				  sValue.zString = (const char *)SyBlobDataAt(pWorker,nBlobOfft);` |
|      ! 0 | 14872 | `				  sValue.nByte = SyBlobLength(pWorker) - nBlobOfft;` |
|        - | 14873 |  |
|      ! 0 | 14874 | `			 }` |
|        - | 14875 | `			 /* Synchronize pointers */` |
|      ! 0 | 14876 | `			 zIn = zPtr;` |
|      ! 0 | 14877 | `		 }` |
|      ! 0 | 14878 | `		 sName.zString = (const char *)SyBlobData(pWorker);` |
|        - | 14879 | `		 /* Install the decoded query in the $_GET/$_REQUEST array */` |
|      ! 0 | 14880 | `		 if( pGet && (pGet->iFlags & MEMOBJ_HASHMAP) ){` |
|      ! 0 | 14881 | `			 VmHashmapInsert((ph7_hashmap *)pGet->x.pOther,` |
|      ! 0 | 14882 | `				 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14883 | `				 sValue.zString,(int)sValue.nByte` |
|        - | 14884 | `				 );` |
|      ! 0 | 14885 | `		 }` |
|      ! 0 | 14886 | `		 if( pRequest && (pRequest->iFlags & MEMOBJ_HASHMAP) ){` |
|      ! 0 | 14887 | `			 VmHashmapInsert((ph7_hashmap *)pRequest->x.pOther,` |
|      ! 0 | 14888 | `				 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14889 | `				 sValue.zString,(int)sValue.nByte` |
|        - | 14890 | `					 );` |
|      ! 0 | 14891 | `		 }` |
|        - | 14892 | `		 /* Advance the pointer */` |
|      ! 0 | 14893 | `		 zIn = &zPtr[1];` |
|      ! 0 | 14894 | `	 }` |
|        - | 14895 | `	/* All done*/` |
|      ! 0 | 14896 | `	return SXRET_OK;` |
|      ! 0 | 14897 | ` }` |
|        - | 14898 | ` /*` |
|        - | 14899 | `  * Extract MIME header value from the given set.` |
|        - | 14900 | `  * Return header value on success. NULL otherwise.` |
|        - | 14901 | `  */` |
|      ! 0 | 14902 | ` static SyString * VmHttpExtractHeaderValue(SySet *pSet,const char *zMime,sxu32 nByte)` |
|      ! 0 | 14903 | ` {` |
|        - | 14904 | `	 SyhttpHeader *aMime,*pMime;` |
|        - | 14905 | `	 SyString sMime;` |
|        - | 14906 | `	 sxu32 n;` |
|      ! 0 | 14907 | `	 SyStringInitFromBuf(&sMime,zMime,nByte);` |
|        - | 14908 | `	 /* Point to the MIME entries */` |
|      ! 0 | 14909 | `	 aMime = (SyhttpHeader *)SySetBasePtr(pSet);` |
|        - | 14910 | `	 /* Perform the lookup */` |
|      ! 0 | 14911 | `	 for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|      ! 0 | 14912 | `		 pMime = &aMime[n];` |
|      ! 0 | 14913 | `		 if( SyStringCmp(&sMime,&pMime->sName,SyStrnicmp) == 0 ){` |
|        - | 14914 | `			 /* Header found,return it's associated value */` |
|      ! 0 | 14915 | `			 return &pMime->sValue;` |
|        - | 14916 | `		 }` |
|      ! 0 | 14917 | `	 }` |
|        - | 14918 | `	 /* No such MIME header */` |
|      ! 0 | 14919 | `	 return 0;` |
|      ! 0 | 14920 | ` }` |
|        - | 14921 | ` /*` |
|        - | 14922 | `  * Tokenize and decode a raw "Cookie:" MIME header into a name value pair` |
|        - | 14923 | `  * and insert it's fields [i.e name,value] in the $_COOKIE superglobal.` |
|        - | 14924 | `  */` |
|      ! 0 | 14925 | ` static sxi32 VmHttpPorcessCookie(ph7_vm *pVm,SyBlob *pWorker,const char *zIn,sxu32 nByte)` |
|      ! 0 | 14926 | ` {` |
|      ! 0 | 14927 | `	 const char *zPtr,*zDelimiter,*zEnd = &zIn[nByte];` |
|        - | 14928 | `	 SyString sName,sValue;` |
|        - | 14929 | `	 ph7_value *pCookie;` |
|        - | 14930 | `	 sxu32 nOfft;` |
|        - | 14931 | `	 /* Make sure the $_COOKIE superglobal is available */` |
|      ! 0 | 14932 | `	 pCookie = VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 14933 | `	 if( pCookie == 0 \|\| (pCookie->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 14934 | `		 /* $_COOKIE superglobal not available */` |
|      ! 0 | 14935 | `		 return SXERR_NOTFOUND;` |
|        - | 14936 | `	 }` |
|      ! 0 | 14937 | `	 for(;;){` |
|        - | 14938 | `		  /* Jump leading white spaces */` |
|      ! 0 | 14939 | `		 while( zIn < zEnd && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14940 | `			 zIn++;` |
|      ! 0 | 14941 | `		 }` |
|      ! 0 | 14942 | `		 if( zIn >= zEnd ){` |
|      ! 0 | 14943 | `			 break;` |
|        - | 14944 | `		 }` |
|        - | 14945 | `		  /* Reset the working buffer */` |
|      ! 0 | 14946 | `		 SyBlobReset(pWorker);` |
|      ! 0 | 14947 | `		 zDelimiter = zIn;` |
|        - | 14948 | `		 /* Delimit the name[=value]; pair */` |
|      ! 0 | 14949 | `		 while( zDelimiter < zEnd && zDelimiter[0] != ';' ){` |
|      ! 0 | 14950 | `			 zDelimiter++;` |
|      ! 0 | 14951 | `		 }` |
|      ! 0 | 14952 | `		 zPtr = zIn;` |
|      ! 0 | 14953 | `		 while( zPtr < zDelimiter && zPtr[0] != '=' ){` |
|      ! 0 | 14954 | `			 zPtr++;` |
|      ! 0 | 14955 | `		 }` |
|        - | 14956 | `		 /* Decode the cookie */` |
|      ! 0 | 14957 | `		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14958 | `		 sName.nByte = SyBlobLength(pWorker);` |
|      ! 0 | 14959 | `		 zPtr++;` |
|      ! 0 | 14960 | `		 sValue.zString = 0;` |
|      ! 0 | 14961 | `		 sValue.nByte = 0;` |
|      ! 0 | 14962 | `		 if( zPtr < zDelimiter ){` |
|        - | 14963 | `			 /* Got a Cookie value */` |
|      ! 0 | 14964 | `			 nOfft = SyBlobLength(pWorker);` |
|      ! 0 | 14965 | `			 SyUriDecode(zPtr,(sxu32)(zDelimiter-zPtr),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14966 | `			 SyStringInitFromBuf(&sValue,SyBlobDataAt(pWorker,nOfft),SyBlobLength(pWorker)-nOfft);` |
|      ! 0 | 14967 | `		 }` |
|        - | 14968 | `		 /* Synchronize pointers */` |
|      ! 0 | 14969 | `		 zIn = &zDelimiter[1];` |
|        - | 14970 | `		 /* Perform the insertion */` |
|      ! 0 | 14971 | `		 sName.zString = (const char *)SyBlobData(pWorker);` |
|      ! 0 | 14972 | `		 VmHashmapInsert((ph7_hashmap *)pCookie->x.pOther,` |
|      ! 0 | 14973 | `			 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14974 | `			 sValue.zString,(int)sValue.nByte` |
|        - | 14975 | `			 );` |
|      ! 0 | 14976 | `	 }` |
|      ! 0 | 14977 | `	 return SXRET_OK;` |
|      ! 0 | 14978 | ` }` |
|        - | 14979 | ` /*` |
|        - | 14980 | `  * Process a full HTTP request and populate the appropriate arrays` |
|        - | 14981 | `  * such as $_SERVER,$_GET,$_POST,$_COOKIE,$_REQUEST,... with the information` |
|        - | 14982 | `  * extracted from the raw HTTP request. As an extension Symisc introduced` |
|        - | 14983 | `  * the $_HEADER array which hold a copy of the processed HTTP MIME headers` |
|        - | 14984 | `  * and their associated values. [i.e: $_HEADER['Server'],$_HEADER['User-Agent'],...].` |
|        - | 14985 | `  * This function return SXRET_OK on success. Any other return value indicates` |
|        - | 14986 | `  * a malformed HTTP request.` |
|        - | 14987 | `  */` |
|      ! 0 | 14988 | ` static sxi32 VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte)` |
|      ! 0 | 14989 | ` {` |
|        - | 14990 | `	 SyString *pName,*pValue,sRequest; /* Raw HTTP request */` |
|        - | 14991 | `	 ph7_value *pHeaderArray;          /* $_HEADER superglobal (Symisc eXtension to the PHP specification)*/` |
|        - | 14992 | `	 SyhttpHeader *pHeader;            /* MIME header */` |
|        - | 14993 | `	 SyhttpUri sUri;     /* Parse of the raw URI*/` |
|        - | 14994 | `	 SyBlob sWorker;     /* General purpose working buffer */` |
|        - | 14995 | `	 SySet sHeader;      /* MIME headers set */` |
|        - | 14996 | `	 sxi32 iMethod;      /* HTTP method [i.e: GET,POST,HEAD...]*/` |
|        - | 14997 | `	 sxi32 iVer;         /* HTTP protocol version */` |
|        - | 14998 | `	 sxi32 rc;` |
|      ! 0 | 14999 | `	 SyStringInitFromBuf(&sRequest,zRequest,nByte);` |
|      ! 0 | 15000 | `	 SySetInit(&sHeader,&pVm->sAllocator,sizeof(SyhttpHeader));` |
|      ! 0 | 15001 | `	 SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        - | 15002 | `	 /* Ignore leading and trailing white spaces*/` |
|      ! 0 | 15003 | `	 SyStringFullTrim(&sRequest);` |
|        - | 15004 | `	 /* Process the first line */` |
|      ! 0 | 15005 | `	 rc = VmHttpProcessFirstLine(&sRequest,&iMethod,&sUri,&iVer);` |
|      ! 0 | 15006 | `	 if( rc != SXRET_OK ){` |
|      ! 0 | 15007 | `		 return rc;` |
|        - | 15008 | `	 }` |
|        - | 15009 | `	 /* Process MIME headers */` |
|      ! 0 | 15010 | `	 VmHttpExtractHeaders(&sRequest,&sHeader);` |
|        - | 15011 | `	 /*` |
|        - | 15012 | `	  * Setup $_SERVER environments` |
|        - | 15013 | `	  */` |
|        - | 15014 | `	 /* 'SERVER_PROTOCOL': Name and revision of the information protocol via which the page was requested */` |
|      ! 0 | 15015 | `	 ph7_vm_config(pVm,` |
|        - | 15016 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15017 | `		 "SERVER_PROTOCOL",` |
|      ! 0 | 15018 | `		 iVer == HTTP_PROTO_10 ? "HTTP/1.0" : "HTTP/1.1",` |
|        - | 15019 | `		 sizeof("HTTP/1.1")-1` |
|        - | 15020 | `		 );` |
|        - | 15021 | `	 /* 'REQUEST_METHOD':  Which request method was used to access the page */` |
|      ! 0 | 15022 | `	 ph7_vm_config(pVm,` |
|        - | 15023 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15024 | `		 "REQUEST_METHOD",` |
|      ! 0 | 15025 | `		 iMethod == HTTP_METHOD_GET ?   "GET" :` |
|      ! 0 | 15026 | `		 (iMethod == HTTP_METHOD_POST ? "POST":` |
|      ! 0 | 15027 | `		 (iMethod == HTTP_METHOD_PUT  ? "PUT" :` |
|      ! 0 | 15028 | `		 (iMethod == HTTP_METHOD_HEAD ?  "HEAD" : "OTHER"))),` |
|        - | 15029 | `		 -1 /* Compute attribute length automatically */` |
|        - | 15030 | `		 );` |
|      ! 0 | 15031 | `	 if( SyStringLength(&sUri.sQuery) > 0 && iMethod == HTTP_METHOD_GET ){` |
|      ! 0 | 15032 | `		 pValue = &sUri.sQuery;` |
|        - | 15033 | `		 /* 'QUERY_STRING': The query string, if any, via which the page was accessed */` |
|      ! 0 | 15034 | `		 ph7_vm_config(pVm,` |
|        - | 15035 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15036 | `			 "QUERY_STRING",` |
|      ! 0 | 15037 | `			 pValue->zString,` |
|      ! 0 | 15038 | `			 pValue->nByte` |
|        - | 15039 | `			 );` |
|        - | 15040 | `		 /* Decoded the raw query */` |
|      ! 0 | 15041 | `		 VmHttpSplitEncodedQuery(&(*pVm),pValue,&sWorker,FALSE);` |
|      ! 0 | 15042 | `	 }` |
|        - | 15043 | `	 /* REQUEST_URI: The URI which was given in order to access this page; for instance, '/index.html' */` |
|      ! 0 | 15044 | `	 pValue = &sUri.sRaw;` |
|      ! 0 | 15045 | `	 ph7_vm_config(pVm,` |
|        - | 15046 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15047 | `		 "REQUEST_URI",` |
|      ! 0 | 15048 | `		 pValue->zString,` |
|      ! 0 | 15049 | `		 pValue->nByte` |
|        - | 15050 | `		 );` |
|        - | 15051 | `	 /*` |
|        - | 15052 | `	  * 'PATH_INFO'` |
|        - | 15053 | `	  * 'ORIG_PATH_INFO'` |
|        - | 15054 | `      * Contains any client-provided pathname information trailing the actual script filename but preceding` |
|        - | 15055 | `	  * the query string, if available. For instance, if the current script was accessed via the URL` |
|        - | 15056 | `	  * http://www.example.com/php/path_info.php/some/stuff?foo=bar, then $_SERVER['PATH_INFO'] would contain` |
|        - | 15057 | `	  * /some/stuff.` |
|        - | 15058 | `	  */` |
|      ! 0 | 15059 | `	 pValue = &sUri.sPath;` |
|      ! 0 | 15060 | `	 ph7_vm_config(pVm,` |
|        - | 15061 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15062 | `		 "PATH_INFO",` |
|      ! 0 | 15063 | `		 pValue->zString,` |
|      ! 0 | 15064 | `		 pValue->nByte` |
|        - | 15065 | `		 );` |
|      ! 0 | 15066 | `	 ph7_vm_config(pVm,` |
|        - | 15067 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15068 | `		 "ORIG_PATH_INFO",` |
|      ! 0 | 15069 | `		 pValue->zString,` |
|      ! 0 | 15070 | `		 pValue->nByte` |
|        - | 15071 | `		 );` |
|        - | 15072 | `	 /* 'HTTP_ACCEPT': Contents of the Accept: header from the current request, if there is one */` |
|      ! 0 | 15073 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept",sizeof("Accept")-1);` |
|      ! 0 | 15074 | `	 if( pValue ){` |
|      ! 0 | 15075 | `		 ph7_vm_config(pVm,` |
|        - | 15076 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15077 | `			 "HTTP_ACCEPT",` |
|      ! 0 | 15078 | `			 pValue->zString,` |
|      ! 0 | 15079 | `			 pValue->nByte` |
|        - | 15080 | `		 );` |
|      ! 0 | 15081 | `	 }` |
|        - | 15082 | `	 /* 'HTTP_ACCEPT_CHARSET': Contents of the Accept-Charset: header from the current request, if there is one. */` |
|      ! 0 | 15083 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Charset",sizeof("Accept-Charset")-1);` |
|      ! 0 | 15084 | `	 if( pValue ){` |
|      ! 0 | 15085 | `		 ph7_vm_config(pVm,` |
|        - | 15086 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15087 | `			 "HTTP_ACCEPT_CHARSET",` |
|      ! 0 | 15088 | `			 pValue->zString,` |
|      ! 0 | 15089 | `			 pValue->nByte` |
|        - | 15090 | `		 );` |
|      ! 0 | 15091 | `	 }` |
|        - | 15092 | `	 /* 'HTTP_ACCEPT_ENCODING': Contents of the Accept-Encoding: header from the current request, if there is one. */` |
|      ! 0 | 15093 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Encoding",sizeof("Accept-Encoding")-1);` |
|      ! 0 | 15094 | `	 if( pValue ){` |
|      ! 0 | 15095 | `		 ph7_vm_config(pVm,` |
|        - | 15096 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15097 | `			 "HTTP_ACCEPT_ENCODING",` |
|      ! 0 | 15098 | `			 pValue->zString,` |
|      ! 0 | 15099 | `			 pValue->nByte` |
|        - | 15100 | `		 );` |
|      ! 0 | 15101 | `	 }` |
|        - | 15102 | `	  /* 'HTTP_ACCEPT_LANGUAGE': Contents of the Accept-Language: header from the current request, if there is one */` |
|      ! 0 | 15103 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Language",sizeof("Accept-Language")-1);` |
|      ! 0 | 15104 | `	 if( pValue ){` |
|      ! 0 | 15105 | `		 ph7_vm_config(pVm,` |
|        - | 15106 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15107 | `			 "HTTP_ACCEPT_LANGUAGE",` |
|      ! 0 | 15108 | `			 pValue->zString,` |
|      ! 0 | 15109 | `			 pValue->nByte` |
|        - | 15110 | `		 );` |
|      ! 0 | 15111 | `	 }` |
|        - | 15112 | `	 /* 'HTTP_CONNECTION': Contents of the Connection: header from the current request, if there is one. */` |
|      ! 0 | 15113 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Connection",sizeof("Connection")-1);` |
|      ! 0 | 15114 | `	 if( pValue ){` |
|      ! 0 | 15115 | `		 ph7_vm_config(pVm,` |
|        - | 15116 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15117 | `			 "HTTP_CONNECTION",` |
|      ! 0 | 15118 | `			 pValue->zString,` |
|      ! 0 | 15119 | `			 pValue->nByte` |
|        - | 15120 | `		 );` |
|      ! 0 | 15121 | `	 }` |
|        - | 15122 | `	 /* 'HTTP_HOST': Contents of the Host: header from the current request, if there is one. */` |
|      ! 0 | 15123 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Host",sizeof("Host")-1);` |
|      ! 0 | 15124 | `	 if( pValue ){` |
|      ! 0 | 15125 | `		 ph7_vm_config(pVm,` |
|        - | 15126 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15127 | `			 "HTTP_HOST",` |
|      ! 0 | 15128 | `			 pValue->zString,` |
|      ! 0 | 15129 | `			 pValue->nByte` |
|        - | 15130 | `		 );` |
|      ! 0 | 15131 | `	 }` |
|        - | 15132 | `	 /* 'HTTP_REFERER': Contents of the Referer: header from the current request, if there is one. */` |
|      ! 0 | 15133 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Referer",sizeof("Referer")-1);` |
|      ! 0 | 15134 | `	 if( pValue ){` |
|      ! 0 | 15135 | `		 ph7_vm_config(pVm,` |
|        - | 15136 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15137 | `			 "HTTP_REFERER",` |
|      ! 0 | 15138 | `			 pValue->zString,` |
|      ! 0 | 15139 | `			 pValue->nByte` |
|        - | 15140 | `		 );` |
|      ! 0 | 15141 | `	 }` |
|        - | 15142 | `	 /* 'HTTP_USER_AGENT': Contents of the Referer: header from the current request, if there is one. */` |
|      ! 0 | 15143 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"User-Agent",sizeof("User-Agent")-1);` |
|      ! 0 | 15144 | `	 if( pValue ){` |
|      ! 0 | 15145 | `		 ph7_vm_config(pVm,` |
|        - | 15146 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15147 | `			 "HTTP_USER_AGENT",` |
|      ! 0 | 15148 | `			 pValue->zString,` |
|      ! 0 | 15149 | `			 pValue->nByte` |
|        - | 15150 | `		 );` |
|      ! 0 | 15151 | `	 }` |
|        - | 15152 | `	  /* 'PHP_AUTH_DIGEST': When doing Digest HTTP authentication this variable is set to the 'Authorization'` |
|        - | 15153 | `	   * header sent by the client (which you should then use to make the appropriate validation).` |
|        - | 15154 | `	   */` |
|      ! 0 | 15155 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Authorization",sizeof("Authorization")-1);` |
|      ! 0 | 15156 | `	 if( pValue ){` |
|      ! 0 | 15157 | `		 ph7_vm_config(pVm,` |
|        - | 15158 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15159 | `			 "PHP_AUTH_DIGEST",` |
|      ! 0 | 15160 | `			 pValue->zString,` |
|      ! 0 | 15161 | `			 pValue->nByte` |
|        - | 15162 | `		 );` |
|      ! 0 | 15163 | `		 ph7_vm_config(pVm,` |
|        - | 15164 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15165 | `			 "PHP_AUTH",` |
|      ! 0 | 15166 | `			 pValue->zString,` |
|      ! 0 | 15167 | `			 pValue->nByte` |
|        - | 15168 | `		 );` |
|      ! 0 | 15169 | `	 }` |
|        - | 15170 | `	 /* Install all clients HTTP headers in the $_HEADER superglobal */` |
|      ! 0 | 15171 | `	 pHeaderArray = VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|        - | 15172 | `	 /* Iterate throw the available MIME headers*/` |
|      ! 0 | 15173 | `	 SySetResetCursor(&sHeader);` |
|      ! 0 | 15174 | `	 pHeader = 0; /* stupid cc warning */` |
|      ! 0 | 15175 | `	 while( SXRET_OK == SySetGetNextEntry(&sHeader,(void **)&pHeader) ){` |
|      ! 0 | 15176 | `		 pName  = &pHeader->sName;` |
|      ! 0 | 15177 | `		 pValue = &pHeader->sValue;` |
|      ! 0 | 15178 | `		 if( pHeaderArray && (pHeaderArray->iFlags & MEMOBJ_HASHMAP)){` |
|        - | 15179 | `			 /* Insert the MIME header and it's associated value */` |
|      ! 0 | 15180 | `			 VmHashmapInsert((ph7_hashmap *)pHeaderArray->x.pOther,` |
|      ! 0 | 15181 | `				 pName->zString,(int)pName->nByte,` |
|      ! 0 | 15182 | `				 pValue->zString,(int)pValue->nByte` |
|        - | 15183 | `				 );` |
|      ! 0 | 15184 | `		 }` |
|      ! 0 | 15185 | `		 if( pName->nByte == sizeof("Cookie")-1 && SyStrnicmp(pName->zString,"Cookie",sizeof("Cookie")-1) == 0` |
|      ! 0 | 15186 | `			 && pValue->nByte > 0){` |
|        - | 15187 | `				 /* Process the name=value pair and insert them in the $_COOKIE superglobal array */` |
|      ! 0 | 15188 | `				 VmHttpPorcessCookie(&(*pVm),&sWorker,pValue->zString,pValue->nByte);` |
|      ! 0 | 15189 | `		 }` |
|      ! 0 | 15190 | `	 }` |
|      ! 0 | 15191 | `	 if( iMethod == HTTP_METHOD_POST ){` |
|        - | 15192 | `		 /* Extract raw POST data */` |
|      ! 0 | 15193 | `		 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Type",sizeof("Content-Type") - 1);` |
|      ! 0 | 15194 | `		 if( pValue && pValue->nByte >= sizeof("application/x-www-form-urlencoded") - 1 &&` |
|      ! 0 | 15195 | `			 SyMemcmp("application/x-www-form-urlencoded",pValue->zString,pValue->nByte) == 0 ){` |
|        - | 15196 | `				 /* Extract POST data length */` |
|      ! 0 | 15197 | `				 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Length",sizeof("Content-Length") - 1);` |
|      ! 0 | 15198 | `				 if( pValue ){` |
|      ! 0 | 15199 | `					 sxi32 iLen = 0; /* POST data length */` |
|      ! 0 | 15200 | `					 SyStrToInt32(pValue->zString,pValue->nByte,(void *)&iLen,0);` |
|      ! 0 | 15201 | `					 if( iLen > 0 ){` |
|        - | 15202 | `						 /* Remove leading and trailing white spaces */` |
|      ! 0 | 15203 | `						 SyStringFullTrim(&sRequest);` |
|      ! 0 | 15204 | `						 if( (int)sRequest.nByte > iLen ){` |
|      ! 0 | 15205 | `							 sRequest.nByte = (sxu32)iLen;` |
|      ! 0 | 15206 | `						 }` |
|        - | 15207 | `						 /* Decode POST data now */` |
|      ! 0 | 15208 | `						 VmHttpSplitEncodedQuery(&(*pVm),&sRequest,&sWorker,TRUE);` |
|      ! 0 | 15209 | `					 }` |
|      ! 0 | 15210 | `				 }` |
|      ! 0 | 15211 | `		 }` |
|      ! 0 | 15212 | `	 }` |
|        - | 15213 | `	 /* All done,clean-up the mess left behind */` |
|      ! 0 | 15214 | `	 SySetRelease(&sHeader);` |
|      ! 0 | 15215 | `	 SyBlobRelease(&sWorker);` |
|      ! 0 | 15216 | `	 return SXRET_OK;` |
|      ! 0 | 15217 | ` }` |
|        - | 15218 |  |
