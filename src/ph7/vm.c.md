# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5223/7395 lines (70.63%)

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
|   578074 |   115 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |   116 |  |
|   578076 |   117 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       23 |   118 | `		return TRUE;` |
|        - |   119 | `	}` |
|   578054 |   120 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |   121 | `		return TRUE;` |
|        - |   122 | `	}` |
|   578046 |   123 | `	return FALSE;` |
|   289061 |   124 |  |
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
|   250498 |   183 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   250500 |   194 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   250500 |   195 | `	if( pEntry ){` |
|        - |   196 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   197 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   198 | `		pCons->xExpand = xExpand;` |
|        6 |   199 | `		pCons->pUserData = pUserData;` |
|        6 |   200 | `		return SXRET_OK;` |
|        - |   201 | `	}` |
|        - |   202 | `	/* Allocate a new constant instance */` |
|   250496 |   203 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   250496 |   204 | `	if( pCons == 0 ){` |
|      ! 0 |   205 | `		return 0;` |
|        - |   206 | `	}` |
|        - |   207 | `	/* Duplicate constant name */` |
|   250496 |   208 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   250496 |   209 | `	if( zDupName == 0 ){` |
|      ! 0 |   210 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   211 | `		return 0;` |
|        - |   212 | `	}` |
|        - |   213 | `	/* Install the constant */` |
|   250496 |   214 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   250496 |   215 | `	pCons->xExpand = xExpand;` |
|   250496 |   216 | `	pCons->pUserData = pUserData;` |
|   250496 |   217 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   250496 |   218 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   219 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   220 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   221 | `		return rc;` |
|        - |   222 | `	}` |
|        - |   223 | `	/* All done,constant can be invoked from PHP code */` |
|   250496 |   224 | `	return SXRET_OK;` |
|   125251 |   225 |  |
|        - |   226 | `/*` |
|        - |   227 | ` * Allocate a new foreign function instance.` |
|        - |   228 | ` * This function return SXRET_OK on success. Any other` |
|        - |   229 | ` * return value indicates failure.` |
|        - |   230 | ` * Please refer to the official documentation for an introduction to` |
|        - |   231 | ` * the foreign function mechanism.` |
|        - |   232 | ` */` |
|   539400 |   233 | `static sxi32 PH7_NewForeignFunction(` |
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
|   539402 |   244 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   539402 |   245 | `	if( pFunc == 0 ){` |
|      ! 0 |   246 | `		return SXERR_MEM;` |
|        - |   247 | `	}` |
|        - |   248 | `	/* Duplicate function name */` |
|   539402 |   249 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   539402 |   250 | `	if( zDup == 0 ){` |
|      ! 0 |   251 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   252 | `		return SXERR_MEM;` |
|        - |   253 | `	}` |
|        - |   254 | `	/* Zero the structure */` |
|   539402 |   255 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   256 | `	/* Initialize structure fields */` |
|   539402 |   257 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   539402 |   258 | `	pFunc->pVm   = pVm;` |
|   539402 |   259 | `	pFunc->xFunc = xFunc;` |
|   539402 |   260 | `	pFunc->pUserData = pUserData;` |
|   539402 |   261 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   262 | `	/* Write a pointer to the new function */` |
|   539402 |   263 | `	*ppOut = pFunc;` |
|   539402 |   264 | `	return SXRET_OK;` |
|   269702 |   265 |  |
|        - |   266 | `/*` |
|        - |   267 | ` * Install a foreign function and it's associated callback so that` |
|        - |   268 | ` * it can be invoked from the target PHP code.` |
|        - |   269 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   270 | ` * return value indicates failure.` |
|        - |   271 | ` * Please refer to the official documentation for an introduction to` |
|        - |   272 | ` * the foreign function mechanism.` |
|        - |   273 | ` */` |
|   540640 |   274 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|   540642 |   285 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   540642 |   286 | `	if( pEntry ){` |
|     1242 |   287 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     1242 |   288 | `		pFunc->pUserData = pUserData;` |
|     1242 |   289 | `		pFunc->xFunc = xFunc;` |
|     1242 |   290 | `		SySetReset(&pFunc->aAux);` |
|     1242 |   291 | `		return SXRET_OK;` |
|        - |   292 | `	}` |
|        - |   293 | `	/* Create a new user function */` |
|   539402 |   294 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   539402 |   295 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   296 | `		return rc;` |
|        - |   297 | `	}` |
|        - |   298 | `	/* Install the function in the corresponding hashtable */` |
|   539402 |   299 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   539402 |   300 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   301 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   302 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   303 | `		return rc;` |
|        - |   304 | `	}` |
|        - |   305 | `	/* User function successfully installed */` |
|   539402 |   306 | `	return SXRET_OK;` |
|   270322 |   307 |  |
|        - |   308 | `/*` |
|        - |   309 | ` * Initialize a VM function.` |
|        - |   310 | ` */` |
|    63538 |   311 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   312 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   313 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   314 | `	const char *zName,  /* Function name */` |
|        - |   315 | `	sxu32 nByte,        /* zName length */` |
|        - |   316 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   317 | `	void *pUserData     /* Function private data */` |
|        - |   318 | `	)` |
|        2 |   319 |  |
|        - |   320 | `	/* Zero the structure */` |
|    63540 |   321 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   322 | `	/* Initialize structure fields */` |
|        - |   323 | `	/* Arguments container */` |
|    63540 |   324 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   325 | `	/* Static variable container */` |
|    63540 |   326 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   327 | `	/* Bytecode container */` |
|    63540 |   328 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   329 | `    /* Preallocate some instruction slots */` |
|    63540 |   330 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   331 | `	/* Closure environment */` |
|    63540 |   332 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|    63540 |   333 | `	pFunc->iFlags = iFlags;` |
|    63540 |   334 | `	pFunc->pUserData = pUserData;` |
|    63540 |   335 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|    63540 |   336 | `	return SXRET_OK;` |
|        2 |   337 |  |
|        - |   338 | `/*` |
|        - |   339 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   340 | ` */` |
|   198084 |   341 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   342 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   343 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   344 | `	SyString *pName     /* Function name */` |
|        - |   345 | `	)` |
|        2 |   346 |  |
|        - |   347 | `	SyHashEntry *pEntry;` |
|        - |   348 | `	sxi32 rc;` |
|   198086 |   349 | `	if( pName == 0 ){` |
|        - |   350 | `		/* Use the built-in name */` |
|    19886 |   351 | `		pName = &pFunc->sName;` |
|     9942 |   352 | `	}` |
|        - |   353 | `	/* Check for duplicates (functions with the same name) first */` |
|   198086 |   354 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   198086 |   355 | `	if( pEntry ){` |
|   146738 |   356 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   146738 |   357 | `		if( pLink != pFunc ){` |
|        - |   358 | `			/* Link */` |
|      179 |   359 | `			pFunc->pNextName = pLink;` |
|      179 |   360 | `			pEntry->pUserData = pFunc;` |
|       89 |   361 | `		}` |
|   146738 |   362 | `		return SXRET_OK;` |
|        - |   363 | `	}` |
|        - |   364 | `	/* First time seen */` |
|    51350 |   365 | `	pFunc->pNextName = 0;` |
|    51350 |   366 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    51350 |   367 | `	return rc;` |
|    99044 |   368 |  |
|        - |   369 | `/*` |
|        - |   370 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   371 | ` */` |
|    16732 |   372 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   373 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   374 | `	ph7_class *pClass /* Target Class */` |
|        - |   375 | `	)` |
|        2 |   376 |  |
|    16734 |   377 | `	SyString *pName = &pClass->sName;` |
|        - |   378 | `	SyHashEntry *pEntry;` |
|        - |   379 | `	sxi32 rc;` |
|        - |   380 | `	/* Check for duplicates */` |
|    16734 |   381 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    16734 |   382 | `	if( pEntry ){` |
|       31 |   383 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   384 | `		/* Link entry with the same name */` |
|       31 |   385 | `		pClass->pNextName = pLink;` |
|       31 |   386 | `		pEntry->pUserData = pClass;` |
|       31 |   387 | `		return SXRET_OK;` |
|        - |   388 | `	}` |
|    16704 |   389 | `	pClass->pNextName = 0;` |
|        - |   390 | `	/* Perform a simple hashtable insertion */` |
|    16704 |   391 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    16704 |   392 | `	return rc;` |
|     8368 |   393 |  |
|        - |   394 | `/*` |
|        - |   395 | ` * Instruction builder interface.` |
|        - |   396 | ` */` |
|  1581692 |   397 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  1581694 |   409 | `	sInstr.iOp = (sxu8)iOp;` |
|  1581694 |   410 | `	sInstr.iP1 = iP1;` |
|  1581694 |   411 | `	sInstr.iP2 = iP2;` |
|  1581694 |   412 | `	sInstr.p3  = p3;` |
|  1581694 |   413 | `	if( pIndex ){` |
|        - |   414 | `		/* Instruction index in the bytecode array */` |
|    94950 |   415 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    47474 |   416 | `	}` |
|        - |   417 | `	/* Finally,record the instruction */` |
|  1581694 |   418 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  1581694 |   419 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   420 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   421 | `		/* Fall throw */` |
|      ! 0 |   422 | `	}` |
|  1581694 |   423 | `	return rc;` |
|        2 |   424 |  |
|        - |   425 | `/*` |
|        - |   426 | ` * Swap the current bytecode container with the given one.` |
|        - |   427 | ` */` |
|   154536 |   428 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   429 |  |
|   154538 |   430 | `	if( pContainer == 0 ){` |
|        - |   431 | `		/* Point to the default container */` |
|      ! 0 |   432 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   433 | `	}else{` |
|        - |   434 | `		/* Change container */` |
|   154538 |   435 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   436 | `	}` |
|   154538 |   437 | `	return SXRET_OK;` |
|        2 |   438 |  |
|        - |   439 | `/*` |
|        - |   440 | ` * Return the current bytecode container.` |
|        - |   441 | ` */` |
|    77268 |   442 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   443 |  |
|    77270 |   444 | `	return pVm->pByteContainer;` |
|        2 |   445 |  |
|        - |   446 | `/*` |
|        - |   447 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   448 | ` */` |
|    93374 |   449 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   450 |  |
|        - |   451 | `	VmInstr *pInstr;` |
|    93376 |   452 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|    93376 |   453 | `	return pInstr;` |
|        2 |   454 |  |
|        - |   455 | `/*` |
|        - |   456 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   457 | ` */` |
|   459822 |   458 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   459 |  |
|   459824 |   460 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   461 |  |
|        - |   462 | `/*` |
|        - |   463 | ` * Pop the last VM instruction.` |
|        - |   464 | ` */` |
|    89944 |   465 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   466 |  |
|    89946 |   467 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   468 |  |
|        - |   469 | `/*` |
|        - |   470 | ` * Peek the last VM instruction.` |
|        - |   471 | ` */` |
|   240468 |   472 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   473 |  |
|   240470 |   474 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   475 |  |
|     3218 |   476 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   477 |  |
|        - |   478 | `	VmInstr *aInstr;` |
|        - |   479 | `	sxu32 n;` |
|     3220 |   480 | `	n = SySetUsed(pVm->pByteContainer);` |
|     3220 |   481 | `	if( n < 2 ){` |
|      ! 0 |   482 | `		return 0;` |
|        - |   483 | `	}` |
|     3220 |   484 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|     3220 |   485 | `	return &aInstr[n - 2];` |
|     1611 |   486 |  |
|        - |   487 | `/*` |
|        - |   488 | ` * Allocate a new virtual machine frame.` |
|        - |   489 | ` */` |
|    10268 |   490 | `static VmFrame * VmNewFrame(` |
|        - |   491 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   492 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   493 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   494 | `	)` |
|        2 |   495 |  |
|        - |   496 | `	VmFrame *pFrame;` |
|        - |   497 | `	/* Allocate a new vm frame */` |
|    10270 |   498 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    10270 |   499 | `	if( pFrame == 0 ){` |
|      ! 0 |   500 | `		return 0;` |
|        - |   501 | `	}` |
|        - |   502 | `	/* Zero the structure */` |
|    10270 |   503 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   504 | `	/* Initialize frame fields */` |
|    10270 |   505 | `	pFrame->pUserData = pUserData;` |
|    10270 |   506 | `	pFrame->pThis = pThis;` |
|    10270 |   507 | `	pFrame->pVm = pVm;` |
|    10270 |   508 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    10270 |   509 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    10270 |   510 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    10270 |   511 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    10270 |   512 | `	return pFrame;` |
|     5136 |   513 |  |
|        - |   514 | `/*` |
|        - |   515 | ` * Enter a VM frame.` |
|        - |   516 | ` */` |
|    10268 |   517 | `static sxi32 VmEnterFrame(` |
|        - |   518 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   519 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   520 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   521 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   522 | `	)` |
|        2 |   523 |  |
|        - |   524 | `	VmFrame *pFrame;` |
|        - |   525 | `	/* Allocate a new frame */` |
|    10270 |   526 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    10270 |   527 | `	if( pFrame == 0 ){` |
|      ! 0 |   528 | `		return SXERR_MEM;` |
|        - |   529 | `	}` |
|        - |   530 | `	/* Link to the list of active VM frame */` |
|    10270 |   531 | `	pFrame->pParent = pVm->pFrame;` |
|    10270 |   532 | `	pVm->pFrame = pFrame;` |
|    10270 |   533 | `	if( ppFrame ){` |
|        - |   534 | `		/* Write a pointer to the new VM frame */` |
|     8772 |   535 | `		*ppFrame = pFrame;` |
|     4385 |   536 | `	}` |
|    10270 |   537 | `	return SXRET_OK;` |
|     5136 |   538 |  |
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
|     8768 |   585 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   586 |  |
|     8770 |   587 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|     8770 |   588 | `	if( pCurFrame ){` |
|        - |   589 | `		/* Unlink from the list of active VM frame */` |
|     8770 |   590 | `		pVm->pFrame = pCurFrame->pParent;` |
|     8770 |   591 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   592 | `			VmSlot  *aSlot;` |
|        - |   593 | `			sxu32 n;` |
|        - |   594 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|     8752 |   595 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    64288 |   596 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   597 | `				/* Unset the local variable */` |
|    55538 |   598 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    27770 |   599 | `			}` |
|        - |   600 | `			/* Remove local reference */` |
|     8752 |   601 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    64322 |   602 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    55572 |   603 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    27787 |   604 | `			}` |
|     4375 |   605 | `		}` |
|        - |   606 | `		/* Release internal containers */` |
|     8770 |   607 | `		SyHashRelease(&pCurFrame->hVar);` |
|     8770 |   608 | `		SySetRelease(&pCurFrame->sArg);` |
|     8770 |   609 | `		SySetRelease(&pCurFrame->sLocal);` |
|     8770 |   610 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   611 | `		/* Release the whole structure */` |
|     8770 |   612 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     4384 |   613 | `	}` |
|     8770 |   614 |  |
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
|    59520 |   732 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   733 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   734 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   735 | `	)` |
|        2 |   736 |  |
|        - |   737 | `	ph7_class_method *pMeth;` |
|        - |   738 | `	ph7_class_attr *pAttr;` |
|        - |   739 | `	SyHashEntry *pEntry;` |
|        - |   740 | `	sxi32 rc;` |
|        - |   741 | `	/* Reset the loop cursor */` |
|    59522 |   742 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   743 | `	/* Process only static and constant attribute */` |
|   200278 |   744 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   745 | `		/* Extract the current attribute */` |
|   110998 |   746 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   110998 |   747 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|    59522 |   769 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |   770 | `		/* Do not mount interface methods since they are signatures only.` |
|        - |   771 | `		 */` |
|    37388 |   772 | `		return SXRET_OK;` |
|        - |   773 | `	}` |
|        - |   774 | `	/* Create constructor alias if not yet done */` |
|    22136 |   775 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   776 | `		/* User constructor with the same base class name */` |
|      202 |   777 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      202 |   778 | `		if( pEntry ){` |
|      ! 0 |   779 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   780 | `			/* Create the alias */` |
|      ! 0 |   781 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   782 | `		}` |
|      100 |   783 | `	}` |
|        - |   784 | `	/* Install the methods now */` |
|    22136 |   785 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   211409 |   786 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   178208 |   787 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   178208 |   788 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   178202 |   789 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   178202 |   790 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   791 | `				return rc;` |
|        - |   792 | `			}` |
|    89100 |   793 | `		}` |
|        2 |   794 | `	}` |
|        - |   795 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    22136 |   796 | `	pClass->bMounted = TRUE;` |
|    22136 |   797 | `	return SXRET_OK;` |
|    29762 |   798 |  |
|        - |   799 | `/*` |
|        - |   800 | ` * Allocate a private frame for attributes of the given` |
|        - |   801 | ` * class instance (Object in the PHP jargon).` |
|        - |   802 | ` */` |
|      700 |   803 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   804 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   805 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   806 | `	)` |
|        2 |   807 |  |
|      702 |   808 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   809 | `	ph7_class_attr *pAttr;` |
|        - |   810 | `	SyHashEntry *pEntry;` |
|        - |   811 | `	sxi32 rc;` |
|        - |   812 | `	/* Install class attribute in the private frame associated with this instance */` |
|      702 |   813 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     2216 |   814 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   815 | `		VmClassAttr *pVmAttr;` |
|        - |   816 | `		/* Extract the current attribute */` |
|     1516 |   817 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     1516 |   818 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     1516 |   819 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   820 | `			return SXERR_MEM;` |
|        - |   821 | `		}` |
|     1516 |   822 | `		pVmAttr->pAttr = pAttr;` |
|     1516 |   823 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   824 | `			ph7_value *pMemObj;` |
|        - |   825 | `			/* Reserve a memory object for this attribute */` |
|     1510 |   826 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1510 |   827 | `			if( pMemObj == 0 ){` |
|      ! 0 |   828 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   829 | `				return SXERR_MEM;` |
|        - |   830 | `			}` |
|     1510 |   831 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     1510 |   832 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   833 | `				/* Initialize attribute default value (any complex expression) */` |
|      480 |   834 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      239 |   835 | `			}` |
|     1510 |   836 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     1510 |   837 | `			if( rc != SXRET_OK ){` |
|        - |   838 | `				VmSlot sSlot;` |
|        - |   839 | `				/* Restore memory object */` |
|      ! 0 |   840 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   841 | `				sSlot.pUserData = 0;` |
|      ! 0 |   842 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   843 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   844 | `				return SXERR_MEM;` |
|        - |   845 | `			}` |
|        - |   846 | `			/* Install attribute in the reference table */` |
|     1510 |   847 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      756 |   848 | `		}else{` |
|        - |   849 | `			/* Install static/constant attribute */` |
|        8 |   850 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   851 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   852 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   853 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   854 | `				return SXERR_MEM;` |
|        - |   855 | `			}` |
|        - |   856 | `		}` |
|        2 |   857 | `	}` |
|      702 |   858 | `	return SXRET_OK;` |
|      352 |   859 |  |
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
|   179806 |   871 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   872 |  |
|        - |   873 | `	ph7_value *pObj;` |
|        - |   874 | `	sxi32 rc;` |
|   179808 |   875 | `	if( pIndex ){` |
|        - |   876 | `		/* Object index in the object table */` |
|   175314 |   877 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|    87656 |   878 | `	}` |
|        - |   879 | `	/* Reserve a slot for the new object */` |
|   179808 |   880 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   179808 |   881 | `	if( rc != SXRET_OK ){` |
|        - |   882 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   883 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   884 | `		 */` |
|      ! 0 |   885 | `		return 0;` |
|        - |   886 | `	}` |
|   179808 |   887 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   179808 |   888 | `	return pObj;` |
|    89905 |   889 |  |
|        - |   890 | `/*` |
|        - |   891 | ` * Reserve a memory object.` |
|        - |   892 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   893 | ` */` |
|  2119496 |   894 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   895 |  |
|        - |   896 | `	ph7_value *pObj;` |
|        - |   897 | `	sxi32 rc;` |
|  2119498 |   898 | `	if( pIndex ){` |
|        - |   899 | `		/* Object index in the object table */` |
|  2119498 |   900 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1059748 |   901 | `	}` |
|        - |   902 | `	/* Reserve a slot for the new object */` |
|  2119498 |   903 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2119498 |   904 | `	if( rc != SXRET_OK ){` |
|        - |   905 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   906 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   907 | `		 */` |
|      ! 0 |   908 | `		return 0;` |
|        - |   909 | `	}` |
|  2119498 |   910 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2119498 |   911 | `	return pObj;` |
|  1059750 |   912 |  |
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
|        - |  1125 | `   " if( func_num_args() < 1 \|\| !is_array($pArray) ){  return 0; }"\` |
|        - |  1126 | `   "/* Copy arguments */"\` |
|        - |  1127 | `   "$nArgs = func_num_args();"\` |
|        - |  1128 | `   "$pNew = array();"\` |
|        - |  1129 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1130 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1131 | `    "}"\` |
|        - |  1132 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1133 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1134 | `	"/* Erase */"\` |
|        - |  1135 | `	"array_erase($pArray);"\` |
|        - |  1136 | `	"/* Unshift */"\` |
|        - |  1137 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1138 | `	"return sizeof($pArray);"\` |
|        - |  1139 | `    "}"\` |
|        - |  1140 | `	"function array_merge_recursive($array1, $array2){"\` |
|        - |  1141 | `	"if( func_num_args() < 1 ){ return NULL; }"\` |
|        - |  1142 | `    "$arrays = func_get_args();"\` |
|        - |  1143 | `    "$narrays = count($arrays);"\` |
|        - |  1144 | `    "$ret = $arrays[0];"\` |
|        - |  1145 | `    "for ($i = 1; $i < $narrays; $i++) {"\` |
|        - |  1146 | `	 " if( array_same($ret,$arrays[$i]) ){ /* Same instance */continue;}"\` |
|        - |  1147 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1148 | `     "  if (((string) $key) === ((string) intval($key))) {"\` |
|        - |  1149 | `     "   $ret[] = $value;"\` |
|        - |  1150 | `     "  }else{"\` |
|        - |  1151 | `     "  if (is_array($value) && isset($ret[$key]) ) {"\` |
|        - |  1152 | `     "   $ret[$key] = array_merge_recursive($ret[$key], $value);"\` |
|        - |  1153 | `     " }else {"\` |
|        - |  1154 | `     "   $ret[$key] = $value;"\` |
|        - |  1155 | `     "  }"\` |
|        - |  1156 | `     " }"\` |
|        - |  1157 | `     " }"\` |
|        - |  1158 | `	 "}"\` |
|        - |  1159 | `	 " return $ret;"\` |
|        - |  1160 | `    "}"\` |
|        - |  1161 | `	"function max(){"\` |
|        - |  1162 | `    "  $pArgs = func_get_args();"\` |
|        - |  1163 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1164 | `	"  return null;"\` |
|        - |  1165 | `    " }"\` |
|        - |  1166 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1167 | `    " $pArg = $pArgs[0];"\` |
|        - |  1168 | `	" if( !is_array($pArg) ){"\` |
|        - |  1169 | `	"   return $pArg; "\` |
|        - |  1170 | `	" }"\` |
|        - |  1171 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1172 | `	"   return null;"\` |
|        - |  1173 | `	" }"\` |
|        - |  1174 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1175 | `	" reset($pArg);"\` |
|        - |  1176 | `	" $max = current($pArg);"\` |
|        - |  1177 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1178 | `	"   if( $val > $max ){"\` |
|        - |  1179 | `	"     $max = $val;"\` |
|        - |  1180 | `    " }"\` |
|        - |  1181 | `	" }"\` |
|        - |  1182 | `	" return $max;"\` |
|        - |  1183 | `    " }"\` |
|        - |  1184 | `    " $max = $pArgs[0];"\` |
|        - |  1185 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1186 | `    " $val = $pArgs[$i];"\` |
|        - |  1187 | `	"if( $val > $max ){"\` |
|        - |  1188 | `	" $max = $val;"\` |
|        - |  1189 | `	"}"\` |
|        - |  1190 | `    " }"\` |
|        - |  1191 | `	" return $max;"\` |
|        - |  1192 | `    "}"\` |
|        - |  1193 | `	"function min(){"\` |
|        - |  1194 | `    "  $pArgs = func_get_args();"\` |
|        - |  1195 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1196 | `	"  return null;"\` |
|        - |  1197 | `    " }"\` |
|        - |  1198 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1199 | `    " $pArg = $pArgs[0];"\` |
|        - |  1200 | `	" if( !is_array($pArg) ){"\` |
|        - |  1201 | `	"   return $pArg; "\` |
|        - |  1202 | `	" }"\` |
|        - |  1203 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1204 | `	"   return null;"\` |
|        - |  1205 | `	" }"\` |
|        - |  1206 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1207 | `	" reset($pArg);"\` |
|        - |  1208 | `	" $min = current($pArg);"\` |
|        - |  1209 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1210 | `	"   if( $val < $min ){"\` |
|        - |  1211 | `	"     $min = $val;"\` |
|        - |  1212 | `    " }"\` |
|        - |  1213 | `	" }"\` |
|        - |  1214 | `	" return $min;"\` |
|        - |  1215 | `    " }"\` |
|        - |  1216 | `    " $min = $pArgs[0];"\` |
|        - |  1217 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1218 | `    " $val = $pArgs[$i];"\` |
|        - |  1219 | `	"if( $val < $min ){"\` |
|        - |  1220 | `	" $min = $val;"\` |
|        - |  1221 | `	" }"\` |
|        - |  1222 | `    " }"\` |
|        - |  1223 | `	" return $min;"\` |
|        - |  1224 | `	"}"\` |
|        - |  1225 | `	"function fileowner(string $file){"\` |
|        - |  1226 | `    " $a = stat($file);"\` |
|        - |  1227 | `	" if( !is_array($a) ){"\` |
|        - |  1228 | `	"	return false;"\` |
|        - |  1229 | `	" }"\` |
|        - |  1230 | `	" return $a['uid'];"\` |
|        - |  1231 | `    "}"\` |
|        - |  1232 | `    "function filegroup(string $file){"\` |
|        - |  1233 | `	" $a = stat($file);"\` |
|        - |  1234 | `	" if( !is_array($a) ){"\` |
|        - |  1235 | `	"	return false;"\` |
|        - |  1236 | `	" }"\` |
|        - |  1237 | `	" return $a['gid'];"\` |
|        - |  1238 | `    "}"\` |
|        - |  1239 | `	 "function fileinode(string $file){"\` |
|        - |  1240 | `	" $a = stat($file);"\` |
|        - |  1241 | `	" if( !is_array($a) ){"\` |
|        - |  1242 | `	"	return false;"\` |
|        - |  1243 | `	" }"\` |
|        - |  1244 | `	" return $a['ino'];"\` |
|        - |  1245 | `    "}"` |
|        - |  1246 |  |
|        - |  1247 | `/*` |
|        - |  1248 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1249 | ` * start compiling the target PHP program.` |
|        - |  1250 | ` */` |
|     1498 |  1251 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1252 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1253 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1254 | `	 )` |
|        2 |  1255 |  |
|        - |  1256 | `	SyString sBuiltin;` |
|        - |  1257 | `	ph7_value *pObj;` |
|        - |  1258 | `	sxi32 rc;` |
|        - |  1259 | `	/* Zero the structure */` |
|     1500 |  1260 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1261 | `	/* Initialize VM fields */` |
|     1500 |  1262 | `	pVm->pEngine = &(*pEngine);` |
|     1500 |  1263 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1264 | `	/* Instructions containers */` |
|     1500 |  1265 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     1500 |  1266 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     1500 |  1267 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1268 | `	/* Object containers */` |
|     1500 |  1269 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1500 |  1270 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1271 | `	/* Virtual machine internal containers */` |
|     1500 |  1272 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     1500 |  1273 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     1500 |  1274 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     1500 |  1275 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1500 |  1276 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     1500 |  1277 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     1500 |  1278 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     1500 |  1279 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     1500 |  1280 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     1500 |  1281 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     1500 |  1282 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     1500 |  1283 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     1500 |  1284 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     1500 |  1285 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     1500 |  1286 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|        - |  1287 | `	/* Configuration containers */` |
|     1500 |  1288 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     1500 |  1289 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     1500 |  1290 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     1500 |  1291 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     1500 |  1292 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1293 | `	/* Error callbacks containers */` |
|     1500 |  1294 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     1500 |  1295 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     1500 |  1296 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     1500 |  1297 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     1500 |  1298 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1299 | `	/* Set a default recursion limit */` |
|        - |  1300 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     1500 |  1301 | `	pVm->nMaxDepth = 32;` |
|        - |  1302 | `#else` |
|        - |  1303 | `	pVm->nMaxDepth = 16;` |
|        - |  1304 | `#endif` |
|        - |  1305 | `	/* Default assertion flags */` |
|     1500 |  1306 | `	pVm->iAssertFlags = PH7_ASSERT_WARNING; /* Issue a warning for each failed assertion */` |
|        - |  1307 | `	/* JSON return status */` |
|     1500 |  1308 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1309 | `	/* PRNG context */` |
|     1500 |  1310 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1311 | `	/* Install the null constant */` |
|     1500 |  1312 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1500 |  1313 | `	if( pObj == 0 ){` |
|      ! 0 |  1314 | `		rc = SXERR_MEM;` |
|      ! 0 |  1315 | `		goto Err;` |
|        - |  1316 | `	}` |
|     1500 |  1317 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1318 | `	/* Install the boolean TRUE constant */` |
|     1500 |  1319 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1500 |  1320 | `	if( pObj == 0 ){` |
|      ! 0 |  1321 | `		rc = SXERR_MEM;` |
|      ! 0 |  1322 | `		goto Err;` |
|        - |  1323 | `	}` |
|     1500 |  1324 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1325 | `	/* Install the boolean FALSE constant */` |
|     1500 |  1326 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1500 |  1327 | `	if( pObj == 0 ){` |
|      ! 0 |  1328 | `		rc = SXERR_MEM;` |
|      ! 0 |  1329 | `		goto Err;` |
|        - |  1330 | `	}` |
|     1500 |  1331 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1332 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1333 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1334 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     1500 |  1335 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     1500 |  1336 | `	if( pObj == 0 ){` |
|      ! 0 |  1337 | `		rc = SXERR_MEM;` |
|      ! 0 |  1338 | `		goto Err;` |
|        - |  1339 | `	}` |
|     1500 |  1340 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1341 | `	/* Create the global frame */` |
|     1500 |  1342 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     1500 |  1343 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1344 | `		goto Err;` |
|        - |  1345 | `	}` |
|        - |  1346 | `	/* Initialize the code generator */` |
|     1500 |  1347 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1500 |  1348 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1349 | `		goto Err;` |
|        - |  1350 | `	}` |
|        - |  1351 | `	/* VM correctly initialized,set the magic number */` |
|     1500 |  1352 | `	pVm->nMagic = PH7_VM_INIT;` |
|     1500 |  1353 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1354 | `	/* Compile the built-in library */` |
|     1500 |  1355 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1356 | `	/* Reset the code generator */` |
|     1500 |  1357 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1500 |  1358 | `	return SXRET_OK;` |
|      ! 0 |  1359 | `Err:` |
|      ! 0 |  1360 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1361 | `	return rc;` |
|      751 |  1362 |  |
|        - |  1363 | `/*` |
|        - |  1364 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1365 | ` * routine which store the output in an internal blob.` |
|        - |  1366 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1367 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1368 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1369 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1370 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1371 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1372 | ` * to finish executing and extracting the output.` |
|        - |  1373 | ` */` |
|      ! 0 |  1374 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1375 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1376 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1377 | `	void *pUserData     /* User private data */` |
|        - |  1378 | `	)` |
|      ! 0 |  1379 |  |
|        - |  1380 | `	 sxi32 rc;` |
|        - |  1381 | `	 /* Store the output in an internal BLOB */` |
|      ! 0 |  1382 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|      ! 0 |  1383 | `	 return rc;` |
|      ! 0 |  1384 |  |
|        - |  1385 | `#define VM_STACK_GUARD 16` |
|        - |  1386 | `/*` |
|        - |  1387 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1388 | ` * our compiled PHP program.` |
|        - |  1389 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1390 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1391 | ` */` |
|    21960 |  1392 | `static ph7_value * VmNewOperandStack(` |
|        - |  1393 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1394 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1395 | `	)` |
|        2 |  1396 |  |
|        - |  1397 | `	ph7_value *pStack;` |
|        - |  1398 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1399 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1400 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1401 | `  ** on the maximum stack depth required.` |
|        - |  1402 | `  **` |
|        - |  1403 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1404 | `  */` |
|    21962 |  1405 | `	nInstr += VM_STACK_GUARD;` |
|    21962 |  1406 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    21962 |  1407 | `	if( pStack == 0 ){` |
|      ! 0 |  1408 | `		return 0;` |
|        - |  1409 | `	}` |
|        - |  1410 | `	/* Initialize the operand stack */` |
|  1393744 |  1411 | `	while( nInstr > 0 ){` |
|  1371784 |  1412 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1371784 |  1413 | `		--nInstr;` |
|        2 |  1414 | `	}` |
|        - |  1415 | `	/* Ready for bytecode execution */` |
|    21962 |  1416 | `	return pStack;` |
|    10982 |  1417 |  |
|        - |  1418 | `/* Forward declaration */` |
|        - |  1419 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1420 | `static int VmInstanceOf(ph7_class *pThis,ph7_class *pClass);` |
|        - |  1421 | `static int VmClassMemberAccess(ph7_vm *pVm,ph7_class *pClass,const SyString *pAttrName,sxi32 iProtection,int bLog);` |
|        - |  1422 | `/*` |
|        - |  1423 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1424 | ` * This routine gets called by the PH7 engine after` |
|        - |  1425 | ` * successful compilation of the target PHP program.` |
|        - |  1426 | ` */` |
|     1240 |  1427 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1428 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1429 | `	)` |
|        2 |  1430 |  |
|        - |  1431 | `	SyHashEntry *pEntry;` |
|        - |  1432 | `	sxi32 rc;` |
|     1242 |  1433 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1434 | `		/* Initialize your VM first */` |
|      ! 0 |  1435 | `		return SXERR_CORRUPT;` |
|        - |  1436 | `	}` |
|        - |  1437 | `	/* Mark the VM ready for byte-code execution */` |
|     1242 |  1438 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1439 | `	/* Release the code generator now we have compiled our program */` |
|     1242 |  1440 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1441 | `	/* Emit the DONE instruction */` |
|     1242 |  1442 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     1242 |  1443 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1444 | `		return SXERR_MEM;` |
|        - |  1445 | `	}` |
|        - |  1446 | `	/* Script return value */` |
|     1242 |  1447 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1448 | `	/* Allocate a new operand stack */` |
|     1242 |  1449 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     1242 |  1450 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1451 | `		return SXERR_MEM;` |
|        - |  1452 | `	}` |
|        - |  1453 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1454 | `	 * private data. */` |
|     1242 |  1455 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     1242 |  1456 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1457 | `	/* Allocate the reference table */` |
|     1242 |  1458 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     1242 |  1459 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     1242 |  1460 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1461 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1462 | `		return SXERR_MEM;` |
|        - |  1463 | `	}` |
|        - |  1464 | `	/* Zero the reference table */` |
|     1242 |  1465 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1466 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     1242 |  1467 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     1242 |  1468 | `	if( rc != SXRET_OK ){` |
|        - |  1469 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1470 | `		return rc;` |
|        - |  1471 | `	}` |
|        - |  1472 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     1242 |  1473 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     1242 |  1474 | `	if( rc != SXRET_OK ){` |
|        - |  1475 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1476 | `		return rc;` |
|        - |  1477 | `	}` |
|        - |  1478 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     1242 |  1479 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1480 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     1242 |  1481 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1482 | `	/* Initialize and install static and constants class attributes */` |
|     1242 |  1483 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    14908 |  1484 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    13668 |  1485 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    13668 |  1486 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1487 | `			return rc;` |
|        - |  1488 | `		}` |
|        2 |  1489 | `	}` |
|        - |  1490 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     1242 |  1491 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1492 | `	/* VM is ready for bytecode execution */` |
|     1242 |  1493 | `	return SXRET_OK;` |
|      622 |  1494 |  |
|        - |  1495 | `/*` |
|        - |  1496 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1497 | ` */` |
|      ! 0 |  1498 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1499 |  |
|      ! 0 |  1500 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1501 | `		return SXERR_CORRUPT;` |
|        - |  1502 | `	}` |
|        - |  1503 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1504 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1505 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1506 | `	/* Set the ready flag */` |
|      ! 0 |  1507 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1508 | `	return SXRET_OK;` |
|      ! 0 |  1509 |  |
|        - |  1510 | `/*` |
|        - |  1511 | ` * Release a Virtual Machine.` |
|        - |  1512 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1513 | ` */` |
|     1232 |  1514 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1515 |  |
|        - |  1516 | `	/* Set the stale magic number */` |
|     1234 |  1517 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1518 | `	/* Release the private memory subsystem */` |
|     1234 |  1519 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     1234 |  1520 | `	return SXRET_OK;` |
|        2 |  1521 |  |
|        - |  1522 | `/*` |
|        - |  1523 | ` * Initialize a foreign function call context.` |
|        - |  1524 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1525 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1526 | ` * functions.` |
|        - |  1527 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1528 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1529 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1530 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1531 | ` */` |
|   444670 |  1532 | `static sxi32 VmInitCallContext(` |
|        - |  1533 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1534 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1535 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1536 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1537 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1538 | `	)` |
|        2 |  1539 |  |
|   444672 |  1540 | `	pOut->pFunc = pFunc;` |
|   444672 |  1541 | `	pOut->pVm   = pVm;` |
|   444672 |  1542 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   444672 |  1543 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1544 | `	/* Assume a null return value */` |
|   444672 |  1545 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   444672 |  1546 | `	pOut->pRet = pRet;` |
|   444672 |  1547 | `	pOut->iFlags = iFlags;` |
|   444672 |  1548 | `	return SXRET_OK;` |
|        2 |  1549 |  |
|        - |  1550 | `/*` |
|        - |  1551 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1552 | ` * left behind.` |
|        - |  1553 | ` */` |
|   444670 |  1554 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1555 |  |
|        - |  1556 | `	sxu32 n;` |
|   444672 |  1557 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     4978 |  1558 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    13976 |  1559 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     9000 |  1560 | `			if( apObj[n] == 0 ){` |
|        - |  1561 | `				/* Already released */` |
|      250 |  1562 | `				continue;` |
|        - |  1563 | `			}` |
|     8752 |  1564 | `			PH7_MemObjRelease(apObj[n]);` |
|     8752 |  1565 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     4377 |  1566 | `		}` |
|     4978 |  1567 | `		SySetRelease(&pCtx->sVar);` |
|     2488 |  1568 | `	}` |
|   444672 |  1569 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1570 | `		ph7_aux_data *aAux;` |
|        - |  1571 | `		void *pChunk;` |
|        - |  1572 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1573 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1574 | `		 */` |
|        9 |  1575 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1576 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1577 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1578 | `			/* Release the chunk */` |
|       25 |  1579 | `			if( pChunk ){` |
|       25 |  1580 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1581 | `			}` |
|       13 |  1582 | `		}` |
|        9 |  1583 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1584 | `	}` |
|   444672 |  1585 |  |
|        - |  1586 | `/*` |
|        - |  1587 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1588 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1589 | ` */` |
|      248 |  1590 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1591 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1592 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1593 | `	)` |
|        2 |  1594 |  |
|      250 |  1595 | `	if( pValue == 0 ){` |
|        - |  1596 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1597 | `		return;` |
|        - |  1598 | `	}` |
|      250 |  1599 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      250 |  1600 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1601 | `		sxu32 n;` |
|      936 |  1602 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      936 |  1603 | `			if( apObj[n] == pValue ){` |
|      250 |  1604 | `				PH7_MemObjRelease(pValue);` |
|      250 |  1605 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1606 | `				/* Mark as released */` |
|      250 |  1607 | `				apObj[n] = 0;` |
|      250 |  1608 | `				break;` |
|        - |  1609 | `			}` |
|      345 |  1610 | `		}` |
|      124 |  1611 | `	}` |
|      126 |  1612 |  |
|        - |  1613 | `/*` |
|        - |  1614 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1615 | ` */` |
|  2524810 |  1616 | `static void VmPopOperand(` |
|        - |  1617 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1618 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1619 | `	)` |
|        2 |  1620 |  |
|  2524812 |  1621 | `	ph7_value *pTos = *ppTos;` |
|  5375104 |  1622 | `	while( nPop > 0 ){` |
|  2850294 |  1623 | `		PH7_MemObjRelease(pTos);` |
|  2850294 |  1624 | `		pTos--;` |
|  2850294 |  1625 | `		nPop--;` |
|        2 |  1626 | `	}` |
|        - |  1627 | `	/* Top of the stack */` |
|  2524812 |  1628 | `	*ppTos = pTos;` |
|  2524812 |  1629 |  |
|        - |  1630 | `/*` |
|        - |  1631 | ` * Reserve a memory object.` |
|        - |  1632 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1633 | ` */` |
|  2764470 |  1634 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1635 |  |
|  2764472 |  1636 | `	ph7_value *pObj = 0;` |
|        - |  1637 | `	VmSlot *pSlot;` |
|        - |  1638 | `	sxu32 nIdx;` |
|        - |  1639 | `	/* Check for a free slot */` |
|  2764472 |  1640 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  2764472 |  1641 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  2764472 |  1642 | `	if( pSlot ){` |
|   644976 |  1643 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   644976 |  1644 | `		nIdx = pSlot->nIdx;` |
|   322487 |  1645 | `	}` |
|  2764472 |  1646 | `	if( pObj == 0 ){` |
|        - |  1647 | `		/* Reserve a new memory object */` |
|  2119498 |  1648 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2119498 |  1649 | `		if( pObj == 0 ){` |
|      ! 0 |  1650 | `			return 0;` |
|        - |  1651 | `		}` |
|  1059748 |  1652 | `	}` |
|        - |  1653 | `	/* Set a null default value */` |
|  2764472 |  1654 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  2764472 |  1655 | `	pObj->nIdx = nIdx;` |
|  2764472 |  1656 | `	return pObj;` |
|  1382237 |  1657 |  |
|        - |  1658 | `/*` |
|        - |  1659 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1660 | ` */` |
|    17194 |  1661 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1662 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1663 | `	const char *zKey,  /* Entry key */` |
|        - |  1664 | `	sxu32 nByte,       /* Key length */` |
|        - |  1665 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1666 | `	)` |
|        2 |  1667 |  |
|        - |  1668 | `	ph7_value sKey;` |
|        - |  1669 | `	sxi32 rc;` |
|    17196 |  1670 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    17196 |  1671 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1672 | `	/* Perform the insertion */` |
|    17196 |  1673 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    17196 |  1674 | `	PH7_MemObjRelease(&sKey);` |
|    17196 |  1675 | `	return rc;` |
|        2 |  1676 |  |
|        - |  1677 | `/*` |
|        - |  1678 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1679 | ` * Return a pointer to the variable value on success.` |
|        - |  1680 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1681 | ` */` |
|  2321726 |  1682 | `static ph7_value * VmExtractMemObj(` |
|        - |  1683 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1684 | `	const SyString *pName, /* Variable name */` |
|        - |  1685 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1686 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1687 | `	)` |
|        2 |  1688 |  |
|  2321728 |  1689 | `	int bNullify = FALSE;` |
|        - |  1690 | `	SyHashEntry *pEntry;` |
|        - |  1691 | `	VmFrame *pFrame;` |
|        - |  1692 | `	ph7_value *pObj;` |
|        - |  1693 | `	sxu32 nIdx;` |
|        - |  1694 | `	sxi32 rc;` |
|        - |  1695 | `	/* Point to the top active frame */` |
|  2321728 |  1696 | `	pFrame = pVm->pFrame;` |
|  2371080 |  1697 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1698 | `		/* Safely ignore the exception frame */` |
|    49353 |  1699 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        1 |  1700 | `	}` |
|        - |  1701 | `	/* Perform the lookup */` |
|  2321728 |  1702 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1703 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1704 | `		pName = &sAnnon;` |
|        - |  1705 | `		/* Always nullify the object */` |
|      ! 0 |  1706 | `		bNullify = TRUE;` |
|      ! 0 |  1707 | `		bDup = FALSE;` |
|      ! 0 |  1708 | `	}` |
|        - |  1709 | `	/* Check the superglobals table first */` |
|  2321728 |  1710 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  2321728 |  1711 | `	if( pEntry == 0 ){` |
|        - |  1712 | `		/* Query the top active frame */` |
|  2321692 |  1713 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  2321692 |  1714 | `		if( pEntry == 0 ){` |
|    60792 |  1715 | `			char *zName = (char *)pName->zString;` |
|        - |  1716 | `			VmSlot sLocal;` |
|    60792 |  1717 | `			if( !bCreate ){` |
|        - |  1718 | `				/* Do not create the variable,return NULL instead */` |
|      486 |  1719 | `				return 0;` |
|        - |  1720 | `			}` |
|        - |  1721 | `			/* No such variable,automatically create a new one and install` |
|        - |  1722 | `			 * it in the current frame.` |
|        - |  1723 | `			 */` |
|    60308 |  1724 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    60308 |  1725 | `			if( pObj == 0 ){` |
|      ! 0 |  1726 | `				return 0;` |
|        - |  1727 | `			}` |
|    60308 |  1728 | `			nIdx = pObj->nIdx;` |
|    60308 |  1729 | `			if( bDup ){` |
|        - |  1730 | `				/* Duplicate name */` |
|      115 |  1731 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      115 |  1732 | `				if( zName == 0 ){` |
|      ! 0 |  1733 | `					return 0;` |
|        - |  1734 | `				}` |
|       57 |  1735 | `			}` |
|        - |  1736 | `			/* Link to the top active VM frame */` |
|    60308 |  1737 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    60308 |  1738 | `			if( rc != SXRET_OK ){` |
|        - |  1739 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1740 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1741 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1742 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1743 | `				return 0;` |
|        - |  1744 | `			}` |
|    60308 |  1745 | `			if( pFrame->pParent != 0 ){` |
|        - |  1746 | `				/* Local variable */` |
|    55538 |  1747 | `				sLocal.nIdx = nIdx;` |
|    55538 |  1748 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    27770 |  1749 | `			}else{` |
|        - |  1750 | `				/* Register in the $GLOBALS array */` |
|     4772 |  1751 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1752 | `			}` |
|        - |  1753 | `			/* Install in the reference table */` |
|    60308 |  1754 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1755 | `			/* Save object index */` |
|    60308 |  1756 | `			pObj->nIdx = nIdx;` |
|    30155 |  1757 | `		}else{` |
|        - |  1758 | `			/* Extract variable contents */` |
|  2260902 |  1759 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  2260902 |  1760 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  2260902 |  1761 | `			if( bNullify && pObj ){` |
|      ! 0 |  1762 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1763 | `			}` |
|        - |  1764 | `		}` |
|  1160715 |  1765 | `	}else{` |
|        - |  1766 | `		/* Superglobal */` |
|       38 |  1767 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1768 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1769 | `	}` |
|  2321244 |  1770 | `	return pObj;` |
|  1160975 |  1771 |  |
|        - |  1772 | `/*` |
|        - |  1773 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1774 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1775 | ` */` |
|     1266 |  1776 | `static ph7_value * VmExtractSuper(` |
|        - |  1777 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1778 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1779 | `	sxu32 nByte        /* zName length */` |
|        - |  1780 | `	)` |
|        2 |  1781 |  |
|        - |  1782 | `	SyHashEntry *pEntry;` |
|        - |  1783 | `	ph7_value *pValue;` |
|        - |  1784 | `	sxu32 nIdx;` |
|        - |  1785 | `	/* Query the superglobal table */` |
|     1268 |  1786 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     1268 |  1787 | `	if( pEntry == 0 ){` |
|        - |  1788 | `		/* No such entry */` |
|      ! 0 |  1789 | `		return 0;` |
|        - |  1790 | `	}` |
|        - |  1791 | `	/* Extract the superglobal index in the global object pool */` |
|     1268 |  1792 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1793 | `	/* Extract the variable value  */` |
|     1268 |  1794 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     1268 |  1795 | `	return pValue;` |
|      635 |  1796 |  |
|        - |  1797 | `/*` |
|        - |  1798 | ` * Perform a raw hashmap insertion.` |
|        - |  1799 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1800 | ` */` |
|     1264 |  1801 | `static sxi32 VmHashmapInsert(` |
|        - |  1802 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1803 | `	const char *zKey,   /* Entry key */` |
|        - |  1804 | `	int nKeylen,        /* zKey length*/` |
|        - |  1805 | `	const char *zData,  /* Entry data */` |
|        - |  1806 | `	int nLen            /* zData length */` |
|        - |  1807 | `	)` |
|        2 |  1808 |  |
|        - |  1809 | `	ph7_value sKey,sValue;` |
|        - |  1810 | `	sxi32 rc;` |
|     1266 |  1811 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     1266 |  1812 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     1266 |  1813 | `	if( zKey ){` |
|     1244 |  1814 | `		if( nKeylen < 0 ){` |
|     1244 |  1815 | `			nKeylen = (int)SyStrlen(zKey);` |
|      621 |  1816 | `		}` |
|     1244 |  1817 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|      621 |  1818 | `	}` |
|     1266 |  1819 | `	if( zData ){` |
|     1266 |  1820 | `		if( nLen < 0 ){` |
|        - |  1821 | `			/* Compute length automatically */` |
|      ! 0 |  1822 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1823 | `		}` |
|     1266 |  1824 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|      632 |  1825 | `	}` |
|        - |  1826 | `	/* Perform the insertion */` |
|     1266 |  1827 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     1266 |  1828 | `	PH7_MemObjRelease(&sKey);` |
|     1266 |  1829 | `	PH7_MemObjRelease(&sValue);` |
|     1266 |  1830 | `	return rc;` |
|        2 |  1831 |  |
|        - |  1832 | `/* Forward declaration */` |
|        - |  1833 | `static sxi32 VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte);` |
|        - |  1834 | `/*` |
|        - |  1835 | ` * Configure a working virtual machine instance.` |
|        - |  1836 | ` *` |
|        - |  1837 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1838 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1839 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1840 | ` * The second argument to this function is an integer configuration option` |
|        - |  1841 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1842 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1843 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1844 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1845 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1846 | ` */` |
|    19864 |  1847 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1848 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1849 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1850 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1851 | `	)` |
|        2 |  1852 |  |
|    19866 |  1853 | `	sxi32 rc = SXRET_OK;` |
|    19866 |  1854 | `	switch(nOp){` |
|      620 |  1855 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     1242 |  1856 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     1242 |  1857 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1858 | `		/* VM output consumer callback */` |
|        - |  1859 | `#ifdef UNTRUST` |
|        - |  1860 | `		if( xConsumer == 0 ){` |
|        - |  1861 | `			rc = SXERR_CORRUPT;` |
|        - |  1862 | `			break;` |
|        - |  1863 | `		}` |
|        - |  1864 | `#endif` |
|        - |  1865 | `		/* Install the output consumer */` |
|     1242 |  1866 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     1242 |  1867 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     1242 |  1868 | `		break;` |
|        - |  1869 | `							   }` |
|      620 |  1870 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1871 | `		/* Import path */` |
|        - |  1872 | `		  const char *zPath;` |
|        - |  1873 | `		  SyString sPath;` |
|     1242 |  1874 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1875 | `#if defined(UNTRUST)` |
|        - |  1876 | `		  if( zPath == 0 ){` |
|        - |  1877 | `			  rc = SXERR_EMPTY;` |
|        - |  1878 | `			  break;` |
|        - |  1879 | `		  }` |
|        - |  1880 | `#endif` |
|     1242 |  1881 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1882 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1883 | `#ifdef __WINNT__` |
|        2 |  1884 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1885 | `#endif` |
|     2482 |  1886 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1887 | `		  /* Remove leading and trailing white spaces */` |
|     1242 |  1888 | `		  SyStringFullTrim(&sPath);` |
|     1242 |  1889 | `		  if( sPath.nByte > 0 ){` |
|        - |  1890 | `			  /* Store the path in the corresponding conatiner */` |
|     1242 |  1891 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      620 |  1892 | `		  }` |
|     1242 |  1893 | `		  break;` |
|        - |  1894 | `									 }` |
|      620 |  1895 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1896 | `		/* Run-Time Error report */` |
|     1242 |  1897 | `		pVm->bErrReport = 1;` |
|     1242 |  1898 | `		break;` |
|      ! 0 |  1899 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1900 | `		/* Recursion depth */` |
|      ! 0 |  1901 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1902 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1903 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1904 | `		}` |
|      ! 0 |  1905 | `		break;` |
|        - |  1906 | `									   }` |
|      ! 0 |  1907 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1908 | `		/* VM output length in bytes */` |
|      ! 0 |  1909 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1910 | `#ifdef UNTRUST` |
|        - |  1911 | `		if( pOut == 0 ){` |
|        - |  1912 | `			rc = SXERR_CORRUPT;` |
|        - |  1913 | `			break;` |
|        - |  1914 | `		}` |
|        - |  1915 | `#endif` |
|      ! 0 |  1916 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1917 | `		break;` |
|        - |  1918 | `							   }` |
|        - |  1919 |  |
|     6200 |  1920 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1921 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1922 | `		/* Create a new superglobal/global variable */` |
|    12402 |  1923 | `		const char *zName = va_arg(ap,const char *);` |
|    12402 |  1924 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1925 | `		SyHashEntry *pEntry;` |
|        - |  1926 | `		ph7_value *pObj;` |
|        - |  1927 | `		sxu32 nByte;` |
|        - |  1928 | `		sxu32 nIdx;` |
|        - |  1929 | `#ifdef UNTRUST` |
|        - |  1930 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  1931 | `			rc = SXERR_CORRUPT;` |
|        - |  1932 | `			break;` |
|        - |  1933 | `		}` |
|        - |  1934 | `#endif` |
|    12402 |  1935 | `		nByte = SyStrlen(zName);` |
|    12402 |  1936 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1937 | `			/* Check if the superglobal is already installed */` |
|    12402 |  1938 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     6202 |  1939 | `		}else{` |
|        - |  1940 | `			/* Query the top active VM frame */` |
|      ! 0 |  1941 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1942 | `		}` |
|    12402 |  1943 | `		if( pEntry ){` |
|        - |  1944 | `			/* Variable already installed */` |
|      ! 0 |  1945 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1946 | `			/* Extract contents */` |
|      ! 0 |  1947 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  1948 | `			if( pObj ){` |
|        - |  1949 | `				/* Overwrite old contents */` |
|      ! 0 |  1950 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  1951 | `			}` |
|      ! 0 |  1952 | `		}else{` |
|        - |  1953 | `			/* Install a new variable */` |
|    12402 |  1954 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    12402 |  1955 | `			if( pObj == 0 ){` |
|      ! 0 |  1956 | `				rc = SXERR_MEM;` |
|      ! 0 |  1957 | `				break;` |
|        - |  1958 | `			}` |
|    12402 |  1959 | `			nIdx = pObj->nIdx;` |
|        - |  1960 | `			/* Copy value */` |
|    12402 |  1961 | `			PH7_MemObjStore(pValue,pObj);` |
|    12402 |  1962 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1963 | `				/* Install the superglobal */` |
|    12402 |  1964 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     6202 |  1965 | `			}else{` |
|        - |  1966 | `				/* Install in the current frame */` |
|      ! 0 |  1967 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1968 | `			}` |
|    12402 |  1969 | `			if( rc == SXRET_OK ){` |
|        - |  1970 | `				SyHashEntry *pRef;` |
|    12402 |  1971 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    12402 |  1972 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     6202 |  1973 | `				}else{` |
|      ! 0 |  1974 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1975 | `				}` |
|        - |  1976 | `				/* Install in the reference table */` |
|    12402 |  1977 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    12402 |  1978 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1979 | `					/* Register in the $GLOBALS array */` |
|    12402 |  1980 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     6200 |  1981 | `				}` |
|     6200 |  1982 | `			}` |
|        - |  1983 | `		}` |
|    12402 |  1984 | `		break;` |
|        - |  1985 | `									}` |
|      621 |  1986 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1987 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1988 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1989 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1990 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1991 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1992 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     1244 |  1993 | `		const char *zKey   = va_arg(ap,const char *);` |
|     1244 |  1994 | `		const char *zValue = va_arg(ap,const char *);` |
|     1244 |  1995 | `		int nLen = va_arg(ap,int);` |
|        - |  1996 | `		ph7_hashmap *pMap;` |
|        - |  1997 | `		ph7_value *pValue;` |
|     1244 |  1998 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1999 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2000 | `			pValue = VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     1243 |  2001 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2002 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2003 | `			pValue = VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     1242 |  2004 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2005 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2006 | `			pValue = VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     1242 |  2007 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2008 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2009 | `			pValue = VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     1242 |  2010 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2011 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2012 | `			pValue = VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     1242 |  2013 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2014 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2015 | `			pValue = VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2016 | `		}else{` |
|        - |  2017 | `			/* Extract the $_SERVER superglobal */` |
|     1242 |  2018 | `			pValue = VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2019 | `		}` |
|     1244 |  2020 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2021 | `			/* No such entry */` |
|      ! 0 |  2022 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2023 | `			break;` |
|        - |  2024 | `		}` |
|        - |  2025 | `		/* Point to the hashmap */` |
|     1244 |  2026 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2027 | `		/* Perform the insertion */` |
|     1244 |  2028 | `		rc = VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     1244 |  2029 | `		break;` |
|        - |  2030 | `								   }` |
|       11 |  2031 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2032 | `		/* Script arguments */` |
|       24 |  2033 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2034 | `		ph7_hashmap *pMap;` |
|        - |  2035 | `		ph7_value *pValue;` |
|        - |  2036 | `		sxu32 n;` |
|       24 |  2037 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2038 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2039 | `			break;` |
|        - |  2040 | `		}` |
|        - |  2041 | `		/* Extract the $argv array */` |
|       24 |  2042 | `		pValue = VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2043 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2044 | `			/* No such entry */` |
|      ! 0 |  2045 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2046 | `			break;` |
|        - |  2047 | `		}` |
|        - |  2048 | `		/* Point to the hashmap */` |
|       24 |  2049 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2050 | `		/* Perform the insertion */` |
|       24 |  2051 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2052 | `		rc = VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2053 | `		if( rc == SXRET_OK ){` |
|       24 |  2054 | `			if( pMap->nEntry > 1 ){` |
|        - |  2055 | `				/* Append space separator first */` |
|       18 |  2056 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2057 | `			}` |
|       24 |  2058 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2059 | `		}` |
|       24 |  2060 | `		break;` |
|        - |  2061 | `								  }` |
|      ! 0 |  2062 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2063 | `		/* error_log() consumer */` |
|      ! 0 |  2064 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2065 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2066 | `		break;` |
|        - |  2067 | `										}` |
|      ! 0 |  2068 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2069 | `		/* Script return value */` |
|      ! 0 |  2070 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2071 | `#ifdef UNTRUST` |
|        - |  2072 | `		if( ppValue == 0 ){` |
|        - |  2073 | `			rc = SXERR_CORRUPT;` |
|        - |  2074 | `			break;` |
|        - |  2075 | `		}` |
|        - |  2076 | `#endif` |
|      ! 0 |  2077 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2078 | `		break;` |
|        - |  2079 | `								   }` |
|     1240 |  2080 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2081 | `		/* Register an IO stream device */` |
|     2482 |  2082 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2083 | `		/* Make sure we are dealing with a valid IO stream */` |
|     3720 |  2084 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     2482 |  2085 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2086 | `				/* Invalid stream */` |
|      ! 0 |  2087 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2088 | `				break;` |
|        - |  2089 | `		}` |
|     2482 |  2090 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2091 | `			/* Make the 'file://' stream the defaut stream device */` |
|     1242 |  2092 | `			pVm->pDefStream = pStream;` |
|      620 |  2093 | `		}` |
|        - |  2094 | `		/* Insert in the appropriate container */` |
|     2482 |  2095 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     2482 |  2096 | `		break;` |
|        - |  2097 | `								  }` |
|      ! 0 |  2098 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2099 | `		/* Point to the VM internal output consumer buffer */` |
|      ! 0 |  2100 | `		const void **ppOut = va_arg(ap,const void **);` |
|      ! 0 |  2101 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2102 | `#ifdef UNTRUST` |
|        - |  2103 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2104 | `			rc = SXERR_CORRUPT;` |
|        - |  2105 | `			break;` |
|        - |  2106 | `		}` |
|        - |  2107 | `#endif` |
|      ! 0 |  2108 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|      ! 0 |  2109 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|      ! 0 |  2110 | `		break;` |
|        - |  2111 | `									   }` |
|      ! 0 |  2112 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2113 | `		/* Raw HTTP request*/` |
|      ! 0 |  2114 | `		const char *zRequest = va_arg(ap,const char *);` |
|      ! 0 |  2115 | `		int nByte = va_arg(ap,int);` |
|      ! 0 |  2116 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2117 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2118 | `			break;` |
|        - |  2119 | `		}` |
|      ! 0 |  2120 | `		if( nByte < 0 ){` |
|        - |  2121 | `			/* Compute length automatically */` |
|      ! 0 |  2122 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2123 | `		}` |
|        - |  2124 | `		/* Process the request */` |
|      ! 0 |  2125 | `		rc = VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|      ! 0 |  2126 | `		break;` |
|        - |  2127 | `									}` |
|      ! 0 |  2128 | `	default:` |
|        - |  2129 | `		/* Unknown configuration option */` |
|      ! 0 |  2130 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2131 | `		break;` |
|        - |  2132 | `	}` |
|    19866 |  2133 | `	return rc;` |
|        2 |  2134 |  |
|        - |  2135 | `/* Forward declaration */` |
|        - |  2136 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2137 | `/*` |
|        - |  2138 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2139 | ` * format.` |
|        - |  2140 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2141 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2142 | ` * (STDOUT).` |
|        - |  2143 | ` */` |
|        2 |  2144 | `static sxi32 VmByteCodeDump(` |
|        - |  2145 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2146 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2147 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2148 | `	)` |
|        1 |  2149 |  |
|        - |  2150 | `	static const char zDump[] = {` |
|        - |  2151 | `		"====================================================\n"` |
|        - |  2152 | `		"PH7 VM Dump\n"` |
|        - |  2153 | `		"====================================================\n"` |
|        - |  2154 | `	};` |
|        - |  2155 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2156 | `	sxi32 rc = SXRET_OK;` |
|        - |  2157 | `	sxu32 n;` |
|        - |  2158 | `	/* Point to the PH7 instructions */` |
|        3 |  2159 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2160 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2161 | `	n = 0;` |
|        3 |  2162 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2163 | `	/* Dump instructions */` |
|        6 |  2164 | `	for(;;){` |
|       13 |  2165 | `		if( pInstr >= pEnd ){` |
|        - |  2166 | `			/* No more instructions */` |
|        3 |  2167 | `			break;` |
|        - |  2168 | `		}` |
|        - |  2169 | `		/* Format and call the consumer callback */` |
|       16 |  2170 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       10 |  2171 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       10 |  2172 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       11 |  2173 | `		if( rc != SXRET_OK ){` |
|        - |  2174 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2175 | `			return rc;` |
|        - |  2176 | `		}` |
|       11 |  2177 | `		++n;` |
|       11 |  2178 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2179 | `	}` |
|        3 |  2180 | `	return rc;` |
|        2 |  2181 |  |
|        - |  2182 | `/* Forward declaration */` |
|        - |  2183 | `static int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData);` |
|        - |  2184 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2185 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2186 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2187 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2188 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2189 | `/*` |
|        - |  2190 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2191 | ` * consumer callback.` |
|        - |  2192 | ` */` |
|      224 |  2193 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        2 |  2194 |  |
|      226 |  2195 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      226 |  2196 | `	sxi32 rc = SXRET_OK;` |
|        - |  2197 | `	/* Append a new line */` |
|        - |  2198 | `#ifdef __WINNT__` |
|        2 |  2199 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2200 | `#else` |
|      224 |  2201 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2202 | `#endif` |
|        - |  2203 | `	/* Invoke the output consumer callback */` |
|      226 |  2204 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      226 |  2205 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  2206 | `		/* Increment output length */` |
|      223 |  2207 | `		pVm->nOutputLen += SyBlobLength(pMsg);` |
|      111 |  2208 | `	}` |
|      226 |  2209 | `	return rc;` |
|        2 |  2210 |  |
|        - |  2211 | `/*` |
|        - |  2212 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2213 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2214 | ` * information.` |
|        - |  2215 | ` */` |
|       96 |  2216 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, SyString *pFile, sxi32 iLine)` |
|        2 |  2217 |  |
|       98 |  2218 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2219 | `		ph7_value apArg[4];` |
|        - |  2220 | `		ph7_value *apArgPtr[4];` |
|        - |  2221 | `		ph7_value sResult;` |
|        - |  2222 | `		SyString sErr;` |
|        - |  2223 | `		/* Prepare arguments */` |
|       19 |  2224 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|       19 |  2225 | `		SyStringInitFromBuf(&sErr,zMessage,SyStrlen(zMessage));` |
|       19 |  2226 | `		PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       19 |  2227 | `		if( pFile ){` |
|       19 |  2228 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       19 |  2229 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       10 |  2230 | `		}else{` |
|      ! 0 |  2231 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2232 | `		}` |
|       19 |  2233 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       19 |  2234 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2235 | `		/* Set up pointer array */` |
|       19 |  2236 | `		apArgPtr[0] = &apArg[0];` |
|       19 |  2237 | `		apArgPtr[1] = &apArg[1];` |
|       19 |  2238 | `		apArgPtr[2] = &apArg[2];` |
|       19 |  2239 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2240 | `		/* Call the handler */` |
|       19 |  2241 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2242 | `		/* Check return value */` |
|       19 |  2243 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2244 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2245 | `		}` |
|        - |  2246 | `		/* Release */` |
|       19 |  2247 | `		PH7_MemObjRelease(&apArg[0]);` |
|       19 |  2248 | `		PH7_MemObjRelease(&apArg[1]);` |
|       19 |  2249 | `		PH7_MemObjRelease(&apArg[2]);` |
|       19 |  2250 | `		PH7_MemObjRelease(&apArg[3]);` |
|       19 |  2251 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2252 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2253 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       19 |  2254 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2255 | `	}` |
|        - |  2256 | `	/* No handler, always call error handler */` |
|       79 |  2257 | `	return TRUE;` |
|       50 |  2258 |  |
|       72 |  2259 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2260 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2261 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2262 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2263 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2264 | `	)` |
|        2 |  2265 |  |
|       74 |  2266 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2267 | `	SyString *pFile;` |
|        - |  2268 | `	char *zErr;` |
|       74 |  2269 | `	sxi32 rc = SXRET_OK;` |
|       74 |  2270 | `	if( !pVm->bErrReport ){` |
|        - |  2271 | `		/* Don't bother reporting errors */` |
|        3 |  2272 | `		return SXRET_OK;` |
|        - |  2273 | `	}` |
|        - |  2274 | `	/* Reset the working buffer */` |
|       72 |  2275 | `	SyBlobReset(pWorker);` |
|        - |  2276 | `	/* Peek the processed file if available */` |
|       72 |  2277 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       72 |  2278 | `	if( pFile ){` |
|        - |  2279 | `		/* Append file name */` |
|       72 |  2280 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       72 |  2281 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       35 |  2282 | `	}` |
|       72 |  2283 | `	zErr = "Error: ";` |
|       72 |  2284 | `	switch(iErr){` |
|       38 |  2285 | `	case PH7_CTX_WARNING: zErr = "Warning: "; break;` |
|       14 |  2286 | `	case PH7_CTX_NOTICE:  zErr = "Notice: ";  break;` |
|       11 |  2287 | `	default:` |
|       23 |  2288 | `		iErr = PH7_CTX_ERR;` |
|       22 |  2289 | `		break;` |
|        - |  2290 | `	}` |
|       72 |  2291 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       72 |  2292 | `	if( pFuncName ){` |
|        - |  2293 | `		/* Append function name first */` |
|       29 |  2294 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       29 |  2295 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       14 |  2296 | `	}` |
|       72 |  2297 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2298 | `	/* Check for user error handler */` |
|       72 |  2299 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, pFile, 0) ){` |
|       53 |  2300 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       26 |  2301 | `	}` |
|       72 |  2302 | `	return rc;` |
|       38 |  2303 |  |
|        - |  2304 | `/*` |
|        - |  2305 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2306 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2307 | ` * information.` |
|        - |  2308 | ` */` |
|       26 |  2309 | `static sxi32 VmThrowErrorAp(` |
|        - |  2310 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2311 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2312 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2313 | `	const char *zFormat, /* Format message */` |
|        - |  2314 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2315 | `	)` |
|        1 |  2316 |  |
|       27 |  2317 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2318 | `	SyBlob sMsg;` |
|        - |  2319 | `	SyString *pFile;` |
|        - |  2320 | `	char *zErr;` |
|       27 |  2321 | `	sxi32 rc = SXRET_OK;` |
|       27 |  2322 | `	if( !pVm->bErrReport ){` |
|        - |  2323 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2324 | `		return SXRET_OK;` |
|        - |  2325 | `	}` |
|        - |  2326 | `	/* Reset the working buffer */` |
|       27 |  2327 | `	SyBlobReset(pWorker);` |
|        - |  2328 | `	/* Peek the processed file if available */` |
|       27 |  2329 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       27 |  2330 | `	if( pFile ){` |
|        - |  2331 | `		/* Append file name */` |
|       27 |  2332 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       27 |  2333 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       13 |  2334 | `	}` |
|       27 |  2335 | `	zErr = "Error: ";` |
|       27 |  2336 | `	switch(iErr){` |
|        9 |  2337 | `	case PH7_CTX_WARNING: zErr = "Warning: "; break;` |
|        7 |  2338 | `	case PH7_CTX_NOTICE:  zErr = "Notice: ";  break;` |
|        6 |  2339 | `	default:` |
|       13 |  2340 | `		iErr = PH7_CTX_ERR;` |
|       12 |  2341 | `		break;` |
|        - |  2342 | `	}` |
|       27 |  2343 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       27 |  2344 | `	if( pFuncName ){` |
|        - |  2345 | `		/* Append function name first */` |
|       13 |  2346 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       13 |  2347 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|        6 |  2348 | `	}` |
|        - |  2349 | `	/* Format the raw message */` |
|       27 |  2350 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       27 |  2351 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2352 | `	/* Check if a user error handler is installed */` |
|       27 |  2353 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), pFile, 0) ){` |
|        - |  2354 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2355 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2356 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2357 | `	}` |
|       27 |  2358 | `	SyBlobRelease(&sMsg);` |
|       27 |  2359 | `	return rc;` |
|       14 |  2360 |  |
|        - |  2361 | `/*` |
|        - |  2362 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2363 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2364 | ` * information.` |
|        - |  2365 | ` * ------------------------------------` |
|        - |  2366 | ` * Simple boring wrapper function.` |
|        - |  2367 | ` * ------------------------------------` |
|        - |  2368 | ` */` |
|       14 |  2369 | `static sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2370 |  |
|        - |  2371 | `	va_list ap;` |
|        - |  2372 | `	sxi32 rc;` |
|       15 |  2373 | `	va_start(ap,zFormat);` |
|       15 |  2374 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2375 | `	va_end(ap);` |
|       15 |  2376 | `	return rc;` |
|        1 |  2377 |  |
|        - |  2378 | `/*` |
|        - |  2379 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2380 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2381 | ` * information.` |
|        - |  2382 | ` * ------------------------------------` |
|        - |  2383 | ` * Simple boring wrapper function.` |
|        - |  2384 | ` * ------------------------------------` |
|        - |  2385 | ` */` |
|       12 |  2386 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        1 |  2387 |  |
|        - |  2388 | `	sxi32 rc;` |
|       13 |  2389 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       13 |  2390 | `	return rc;` |
|        1 |  2391 |  |
|        - |  2392 | `/*` |
|        - |  2393 | ` * Resolve function context from the current frame.` |
|        - |  2394 | ` */` |
|      292 |  2395 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        2 |  2396 |  |
|        - |  2397 | `	VmFrame *pFrame;` |
|        - |  2398 | `	ph7_vm_func *pFunc;` |
|      294 |  2399 | `	*pzFuncName = 0;` |
|      294 |  2400 | `	*pnFuncLen = 0;` |
|      294 |  2401 | `	pFrame = pVm->pFrame;` |
|      294 |  2402 | `	if( pFrame == 0 ){` |
|      ! 0 |  2403 | `		return;` |
|        - |  2404 | `	}` |
|      294 |  2405 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  2406 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  2407 | `	}` |
|      294 |  2408 | `	if( pFrame->pParent == 0 ){` |
|      294 |  2409 | `		return;` |
|        - |  2410 | `	}` |
|      ! 0 |  2411 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      ! 0 |  2412 | `	if( pFunc == 0 ){` |
|      ! 0 |  2413 | `		return;` |
|        - |  2414 | `	}` |
|      ! 0 |  2415 | `	*pzFuncName = pFunc->sName.zString;` |
|      ! 0 |  2416 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      148 |  2417 |  |
|        - |  2418 | `/*` |
|        - |  2419 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2420 | ` */` |
|      146 |  2421 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        2 |  2422 |  |
|        - |  2423 | `	SyBlob sOut;` |
|        - |  2424 | `	SyString *pFile;` |
|      148 |  2425 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2426 | `		return PH7_OK;` |
|        - |  2427 | `	}` |
|      148 |  2428 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2429 | `		zClass = "Exception";` |
|      ! 0 |  2430 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2431 | `	}` |
|      148 |  2432 | `	if( zMsg == 0 ){` |
|      ! 0 |  2433 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2434 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2435 | `	}` |
|      148 |  2436 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      148 |  2437 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|       73 |  2438 | `	}` |
|      148 |  2439 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      148 |  2440 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      148 |  2441 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      148 |  2442 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      148 |  2443 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      148 |  2444 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      148 |  2445 | `	if( pFile ){` |
|      148 |  2446 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      148 |  2447 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      148 |  2448 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|       73 |  2449 | `	}` |
|      148 |  2450 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      148 |  2451 | `	if( pFile ){` |
|      148 |  2452 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      148 |  2453 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      148 |  2454 | `		if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2455 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2456 | `		}else{` |
|      148 |  2457 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        2 |  2458 | `		}` |
|       73 |  2459 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2460 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2461 | `	}else{` |
|      ! 0 |  2462 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2463 | `	}` |
|      148 |  2464 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      148 |  2465 | `	if( pFile ){` |
|      148 |  2466 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      148 |  2467 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      148 |  2468 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      148 |  2469 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|       73 |  2470 | `	}` |
|      148 |  2471 | `	VmCallErrorHandler(pVm,&sOut);` |
|      148 |  2472 | `	SyBlobRelease(&sOut);` |
|      148 |  2473 | `	return PH7_ABORT;` |
|       75 |  2474 |  |
|        - |  2475 | `/*` |
|        - |  2476 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2477 | ` */` |
|      146 |  2478 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2479 |  |
|        - |  2480 | `	ph7_vm *pVm;` |
|        - |  2481 | `	ph7_class *pClass;` |
|        - |  2482 | `	ph7_class_instance *pThis;` |
|        - |  2483 | `	ph7_class_method *pCons;` |
|        - |  2484 | `	ph7_value sArg;` |
|        - |  2485 | `	ph7_value *apArg[1];` |
|        - |  2486 | `	SyBlob sMsg;` |
|        - |  2487 | `	SyString sMsgStr;` |
|        - |  2488 | `	VmFrame *pFrame;` |
|        - |  2489 | `	va_list ap;` |
|        - |  2490 | `	sxi32 rc;` |
|        - |  2491 |  |
|      148 |  2492 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2493 | `		return PH7_ABORT;` |
|        - |  2494 | `	}` |
|      148 |  2495 | `	pVm = pCtx->pVm;` |
|      148 |  2496 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2497 | `		zClass = "Error";` |
|      ! 0 |  2498 | `	}` |
|      148 |  2499 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      148 |  2500 | `	if( pClass == 0 ){` |
|      ! 0 |  2501 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2502 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2503 | `			zClass` |
|        - |  2504 | `			);` |
|        - |  2505 | `	}` |
|      148 |  2506 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      148 |  2507 | `	if( pThis == 0 ){` |
|      ! 0 |  2508 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2509 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2510 | `			);` |
|        - |  2511 | `	}` |
|        - |  2512 |  |
|      148 |  2513 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      148 |  2514 | `	va_start(ap,zFormat);` |
|      148 |  2515 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      148 |  2516 | `	va_end(ap);` |
|        - |  2517 |  |
|      148 |  2518 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      148 |  2519 | `	if( pCons ){` |
|      148 |  2520 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      148 |  2521 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      148 |  2522 | `		apArg[0] = &sArg;` |
|      148 |  2523 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      148 |  2524 | `		PH7_MemObjRelease(&sArg);` |
|       73 |  2525 | `	}` |
|      148 |  2526 | `	SyBlobRelease(&sMsg);` |
|        - |  2527 |  |
|      148 |  2528 | `	pFrame = pVm->pFrame;` |
|      148 |  2529 | `	if( pFrame ){` |
|      150 |  2530 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        3 |  2531 | `			pFrame = pFrame->pParent;` |
|        1 |  2532 | `		}` |
|      148 |  2533 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       73 |  2534 | `	}` |
|      148 |  2535 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      148 |  2536 | `	PH7_ClassInstanceUnref(pThis);` |
|      148 |  2537 | `	if( rc == SXERR_ABORT ){` |
|      146 |  2538 | `		return PH7_ABORT;` |
|        - |  2539 | `	}` |
|        3 |  2540 | `	return PH7_EXCEPTION;` |
|       75 |  2541 |  |
|        - |  2542 | `/*` |
|        - |  2543 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2544 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2545 | ` */` |
|      ! 0 |  2546 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2547 |  |
|        - |  2548 | `	ph7_vm *pVm;` |
|        - |  2549 | `	SyBlob sMsg;` |
|      ! 0 |  2550 | `	const char *zFuncName = 0;` |
|      ! 0 |  2551 | `	int nFuncLen = 0;` |
|        - |  2552 | `	va_list ap;` |
|        - |  2553 | `	sxi32 rc;` |
|        - |  2554 |  |
|      ! 0 |  2555 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2556 | `		return PH7_OK;` |
|        - |  2557 | `	}` |
|      ! 0 |  2558 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2559 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2560 | `		zClass = "Error";` |
|      ! 0 |  2561 | `	}` |
|        - |  2562 |  |
|      ! 0 |  2563 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2564 |  |
|      ! 0 |  2565 | `	va_start(ap,zFormat);` |
|      ! 0 |  2566 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2567 | `	va_end(ap);` |
|        - |  2568 |  |
|      ! 0 |  2569 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2570 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2571 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2572 | `	}` |
|      ! 0 |  2573 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2574 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2575 | `	}` |
|      ! 0 |  2576 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2577 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2578 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2579 | `	return rc;` |
|      ! 0 |  2580 |  |
|        - |  2581 | `/*` |
|        - |  2582 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2583 | ` *` |
|        - |  2584 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2585 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2586 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2587 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2588 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2589 | ` * then the program execution is halted.` |
|        - |  2590 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2591 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2592 | ` * or to reset the VM to it's initial state.` |
|        - |  2593 | ` */` |
|    21960 |  2594 | `static sxi32 VmByteCodeExec(` |
|        - |  2595 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2596 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2597 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2598 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2599 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2600 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2601 | `	int is_callback      /* TRUE if we are executing a callback */` |
|        - |  2602 | `	)` |
|        2 |  2603 |  |
|        - |  2604 | `	VmInstr *pInstr;` |
|        - |  2605 | `	ph7_value *pTos;` |
|        - |  2606 | `	SySet aArg;` |
|        - |  2607 | `	sxi32 pc;` |
|        - |  2608 | `	sxi32 rc;` |
|        - |  2609 | `	/* Argument container */` |
|    21962 |  2610 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    21962 |  2611 | `	if( nTos < 0 ){` |
|    21134 |  2612 | `		pTos = &pStack[-1];` |
|    10568 |  2613 | `	}else{` |
|      830 |  2614 | `		pTos = &pStack[nTos];` |
|        - |  2615 | `	}` |
|    21962 |  2616 | `	pc = 0;` |
|        - |  2617 | `	/* Execute as much as we can */` |
|  3776099 |  2618 | `	for(;;){` |
|        - |  2619 | `		/* Fetch the instruction to execute */` |
|  7551496 |  2620 | `		pInstr = &aInstr[pc];` |
|  7551496 |  2621 | `		rc = SXRET_OK;` |
|        - |  2622 | `/*` |
|        - |  2623 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2624 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2625 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2626 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2627 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2628 | ` */` |
|  7551496 |  2629 | `		switch(pInstr->iOp){` |
|        - |  2630 | `/*` |
|        - |  2631 | ` * DONE: P1 * *` |
|        - |  2632 | ` *` |
|        - |  2633 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2634 | ` * and return immediately.` |
|        - |  2635 | ` */` |
|    10900 |  2636 | `case PH7_OP_DONE:` |
|    21802 |  2637 | `	if( pInstr->iP1 ){` |
|        - |  2638 | `#ifdef UNTRUST` |
|        - |  2639 | `		if( pTos < pStack ){` |
|        - |  2640 | `			goto Abort;` |
|        - |  2641 | `		}` |
|        - |  2642 | `#endif` |
|    12026 |  2643 | `		if( pLastRef ){` |
|     8222 |  2644 | `			*pLastRef = pTos->nIdx;` |
|     4110 |  2645 | `		}` |
|    12026 |  2646 | `		if( pResult ){` |
|        - |  2647 | `			/* Execution result */` |
|    11594 |  2648 | `			PH7_MemObjStore(pTos,pResult);` |
|     5796 |  2649 | `		}` |
|    12026 |  2650 | `		VmPopOperand(&pTos,1);` |
|    15790 |  2651 | `	}else if( pLastRef ){` |
|        - |  2652 | `		/* Nothing referenced */` |
|      518 |  2653 | `		*pLastRef = SXU32_HIGH;` |
|      258 |  2654 | `	}` |
|    21802 |  2655 | `	goto Done;` |
|        - |  2656 | `/*` |
|        - |  2657 | ` * HALT: P1 * *` |
|        - |  2658 | ` *` |
|        - |  2659 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2660 | ` * and abort immediately.` |
|        - |  2661 | ` */` |
|        4 |  2662 | `case PH7_OP_HALT:` |
|        9 |  2663 | `	if( pInstr->iP1 ){` |
|        - |  2664 | `#ifdef UNTRUST` |
|        - |  2665 | `		if( pTos < pStack ){` |
|        - |  2666 | `			goto Abort;` |
|        - |  2667 | `		}` |
|        - |  2668 | `#endif` |
|        9 |  2669 | `		if( pLastRef ){` |
|      ! 0 |  2670 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2671 | `		}` |
|        9 |  2672 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2673 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2674 | `				/* Output the exit message */` |
|        7 |  2675 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2676 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2677 | `				if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  2678 | `					/* Increment output length */` |
|        5 |  2679 | `					pVm->nOutputLen += SyBlobLength(&pTos->sBlob);` |
|        2 |  2680 | `				}` |
|        3 |  2681 | `			}` |
|        7 |  2682 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2683 | `			/* Record exit status */` |
|        5 |  2684 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2685 | `		}` |
|        9 |  2686 | `		VmPopOperand(&pTos,1);` |
|        4 |  2687 | `	}else if( pLastRef ){` |
|        - |  2688 | `		/* Nothing referenced */` |
|      ! 0 |  2689 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2690 | `	}` |
|        - |  2691 | `	/* Check if we're in an included file context */` |
|        9 |  2692 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2693 | `		/* Terminate the entire process */` |
|        9 |  2694 | `		exit(pVm->iExitStatus);` |
|        - |  2695 | `	}` |
|      ! 0 |  2696 | `	goto Abort;` |
|        - |  2697 | `/*` |
|        - |  2698 | ` * JMP: * P2 *` |
|        - |  2699 | ` *` |
|        - |  2700 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2701 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2702 | ` */` |
|   167925 |  2703 | `case PH7_OP_JMP:` |
|   335896 |  2704 | `	pc = pInstr->iP2 - 1;` |
|   335896 |  2705 | `	break;` |
|        - |  2706 | `/*` |
|        - |  2707 | ` * JZ: P1 P2 *` |
|        - |  2708 | ` *` |
|        - |  2709 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2710 | ` * entry in the stack if P1 is zero.` |
|        - |  2711 | ` */` |
|   380394 |  2712 | `case PH7_OP_JZ:` |
|        - |  2713 | `#ifdef UNTRUST` |
|        - |  2714 | `	if( pTos < pStack ){` |
|        - |  2715 | `		goto Abort;` |
|        - |  2716 | `	}` |
|        - |  2717 | `#endif` |
|        - |  2718 | `	/* Get a boolean value */` |
|   760878 |  2719 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       77 |  2720 | `		PH7_MemObjToBool(pTos);` |
|       38 |  2721 | `	}` |
|   760878 |  2722 | `	if( !pTos->x.iVal ){` |
|        - |  2723 | `		/* Take the jump */` |
|   364388 |  2724 | `		pc = pInstr->iP2 - 1;` |
|   182193 |  2725 | `	}` |
|   760878 |  2726 | `	if( !pInstr->iP1 ){` |
|   596006 |  2727 | `		VmPopOperand(&pTos,1);` |
|   298024 |  2728 | `	}` |
|   760878 |  2729 | `	break;` |
|        - |  2730 | `/*` |
|        - |  2731 | ` * JNZ: P1 P2 *` |
|        - |  2732 | ` *` |
|        - |  2733 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2734 | ` * entry in the stack if P1 is zero.` |
|        - |  2735 | ` */` |
|    35915 |  2736 | `case PH7_OP_JNZ:` |
|        - |  2737 | `#ifdef UNTRUST` |
|        - |  2738 | `	if( pTos < pStack ){` |
|        - |  2739 | `		goto Abort;` |
|        - |  2740 | `	}` |
|        - |  2741 | `#endif` |
|        - |  2742 | `	/* Get a boolean value */` |
|    71832 |  2743 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2744 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2745 | `	}` |
|    71832 |  2746 | `	if( pTos->x.iVal ){` |
|        - |  2747 | `		/* Take the jump */` |
|     3314 |  2748 | `		pc = pInstr->iP2 - 1;` |
|     1656 |  2749 | `	}` |
|    71832 |  2750 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2751 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2752 | `	}` |
|    71832 |  2753 | `	break;` |
|        - |  2754 | `/*` |
|        - |  2755 | ` * NOOP: * * *` |
|        - |  2756 | ` *` |
|        - |  2757 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2758 | ` * destination.` |
|        - |  2759 | ` */` |
|      ! 0 |  2760 | `case PH7_OP_NOOP:` |
|      ! 0 |  2761 | `	break;` |
|        - |  2762 | `/*` |
|        - |  2763 | ` * POP: P1 * *` |
|        - |  2764 | ` *` |
|        - |  2765 | ` * Pop P1 elements from the operand stack.` |
|        - |  2766 | ` */` |
|   302289 |  2767 | `case PH7_OP_POP: {` |
|   604624 |  2768 | `	sxi32 n = pInstr->iP1;` |
|   604624 |  2769 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2770 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2771 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2772 | `	}` |
|   604624 |  2773 | `	VmPopOperand(&pTos,n);` |
|   604624 |  2774 | `	break;` |
|        - |  2775 | `				 }` |
|        - |  2776 | `/*` |
|        - |  2777 | ` * CVT_INT: * * *` |
|        - |  2778 | ` *` |
|        - |  2779 | ` * Force the top of the stack to be an integer.` |
|        - |  2780 | ` */` |
|       35 |  2781 | `case PH7_OP_CVT_INT:` |
|        - |  2782 | `#ifdef UNTRUST` |
|        - |  2783 | `	if( pTos < pStack ){` |
|        - |  2784 | `		goto Abort;` |
|        - |  2785 | `	}` |
|        - |  2786 | `#endif` |
|       72 |  2787 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2788 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2789 | `	}` |
|        - |  2790 | `	/* Invalidate any prior representation */` |
|       72 |  2791 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2792 | `	break;` |
|        - |  2793 | `/*` |
|        - |  2794 | ` * CVT_REAL: * * *` |
|        - |  2795 | ` *` |
|        - |  2796 | ` * Force the top of the stack to be a real.` |
|        - |  2797 | ` */` |
|        4 |  2798 | `case PH7_OP_CVT_REAL:` |
|        - |  2799 | `#ifdef UNTRUST` |
|        - |  2800 | `	if( pTos < pStack ){` |
|        - |  2801 | `		goto Abort;` |
|        - |  2802 | `	}` |
|        - |  2803 | `#endif` |
|        9 |  2804 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2805 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2806 | `	}` |
|        - |  2807 | `	/* Invalidate any prior representation */` |
|        9 |  2808 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2809 | `	break;` |
|        - |  2810 | `/*` |
|        - |  2811 | ` * CVT_STR: * * *` |
|        - |  2812 | ` *` |
|        - |  2813 | ` * Force the top of the stack to be a string.` |
|        - |  2814 | ` */` |
|      136 |  2815 | `case PH7_OP_CVT_STR:` |
|        - |  2816 | `#ifdef UNTRUST` |
|        - |  2817 | `	if( pTos < pStack ){` |
|        - |  2818 | `		goto Abort;` |
|        - |  2819 | `	}` |
|        - |  2820 | `#endif` |
|      274 |  2821 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      274 |  2822 | `		PH7_MemObjToString(pTos);` |
|      136 |  2823 | `	}` |
|      274 |  2824 | `	break;` |
|        - |  2825 | `/*` |
|        - |  2826 | ` * CVT_BOOL: * * *` |
|        - |  2827 | ` *` |
|        - |  2828 | ` * Force the top of the stack to be a boolean.` |
|        - |  2829 | ` */` |
|        5 |  2830 | `case PH7_OP_CVT_BOOL:` |
|        - |  2831 | `#ifdef UNTRUST` |
|        - |  2832 | `	if( pTos < pStack ){` |
|        - |  2833 | `		goto Abort;` |
|        - |  2834 | `	}` |
|        - |  2835 | `#endif` |
|       11 |  2836 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  2837 | `		PH7_MemObjToBool(pTos);` |
|        3 |  2838 | `	}` |
|       11 |  2839 | `	break;` |
|        - |  2840 | `/*` |
|        - |  2841 | ` * CVT_NULL: * * *` |
|        - |  2842 | ` *` |
|        - |  2843 | ` * Nullify the top of the stack.` |
|        - |  2844 | ` */` |
|        3 |  2845 | `case PH7_OP_CVT_NULL:` |
|        - |  2846 | `#ifdef UNTRUST` |
|        - |  2847 | `	if( pTos < pStack ){` |
|        - |  2848 | `		goto Abort;` |
|        - |  2849 | `	}` |
|        - |  2850 | `#endif` |
|        7 |  2851 | `	PH7_MemObjRelease(pTos);` |
|        7 |  2852 | `	break;` |
|        - |  2853 | `/*` |
|        - |  2854 | ` * CVT_NUMC: * * *` |
|        - |  2855 | ` *` |
|        - |  2856 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  2857 | ` */` |
|      ! 0 |  2858 | `case PH7_OP_CVT_NUMC:` |
|        - |  2859 | `#ifdef UNTRUST` |
|        - |  2860 | `	if( pTos < pStack ){` |
|        - |  2861 | `		goto Abort;` |
|        - |  2862 | `	}` |
|        - |  2863 | `#endif` |
|        - |  2864 | `	/* Force a numeric cast */` |
|      ! 0 |  2865 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  2866 | `	break;` |
|        - |  2867 | `/*` |
|        - |  2868 | ` * CVT_ARRAY: * * *` |
|        - |  2869 | ` *` |
|        - |  2870 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  2871 | ` */` |
|       10 |  2872 | `case PH7_OP_CVT_ARRAY:` |
|        - |  2873 | `#ifdef UNTRUST` |
|        - |  2874 | `	if( pTos < pStack ){` |
|        - |  2875 | `		goto Abort;` |
|        - |  2876 | `	}` |
|        - |  2877 | `#endif` |
|        - |  2878 | `	/* Force a hashmap cast */` |
|       21 |  2879 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  2880 | `	if( rc != SXRET_OK ){` |
|        - |  2881 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  2882 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  2883 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  2884 | `	}` |
|       21 |  2885 | `	break;` |
|        - |  2886 | `/*` |
|        - |  2887 | ` * CVT_OBJ: * * *` |
|        - |  2888 | ` *` |
|        - |  2889 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  2890 | ` */` |
|        8 |  2891 | `case PH7_OP_CVT_OBJ:` |
|        - |  2892 | `#ifdef UNTRUST` |
|        - |  2893 | `	if( pTos < pStack ){` |
|        - |  2894 | `		goto Abort;` |
|        - |  2895 | `	}` |
|        - |  2896 | `#endif` |
|       17 |  2897 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  2898 | `		/* Force a 'stdClass()' cast */` |
|       17 |  2899 | `		PH7_MemObjToObject(pTos);` |
|        8 |  2900 | `	}` |
|       17 |  2901 | `	break;` |
|        - |  2902 | `/*` |
|        - |  2903 | ` * ERR_CTRL * * *` |
|        - |  2904 | ` *` |
|        - |  2905 | ` * Error control operator.` |
|        - |  2906 | ` */` |
|     9657 |  2907 | `case PH7_OP_ERR_CTRL:` |
|        - |  2908 | `	/*` |
|        - |  2909 | `	 * TICKET 1433-038:` |
|        - |  2910 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2911 | `	 * use the public API,to control error output.` |
|        - |  2912 | `	 */` |
|    19314 |  2913 | `	break;` |
|        - |  2914 | `/*` |
|        - |  2915 | ` * IS_A * * *` |
|        - |  2916 | ` *` |
|        - |  2917 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  2918 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  2919 | ` * holding a class name or an object).` |
|        - |  2920 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  2921 | ` */` |
|       11 |  2922 | `case PH7_OP_IS_A:{` |
|       23 |  2923 | `	ph7_value *pNos = &pTos[-1];` |
|       23 |  2924 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  2925 | `#ifdef UNTRUST` |
|        - |  2926 | `	if( pNos < pStack ){` |
|        - |  2927 | `		goto Abort;` |
|        - |  2928 | `	}` |
|        - |  2929 | `#endif` |
|       23 |  2930 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       21 |  2931 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       21 |  2932 | `		ph7_class *pClass = 0;` |
|        - |  2933 | `		/* Extract the target class */` |
|       21 |  2934 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  2935 | `			/* Instance already loaded */` |
|      ! 0 |  2936 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       21 |  2937 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2938 | `			/* Perform the query */` |
|       31 |  2939 | `			pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|       20 |  2940 | `				SyBlobLength(&pTos->sBlob),FALSE,0);` |
|       10 |  2941 | `		}` |
|       21 |  2942 | `		if( pClass ){` |
|        - |  2943 | `			/* Perform the query */` |
|       21 |  2944 | `			iRes = VmInstanceOf(pThis->pClass,pClass);` |
|       10 |  2945 | `		}` |
|       10 |  2946 | `	}` |
|        - |  2947 | `	/* Push result */` |
|       23 |  2948 | `	VmPopOperand(&pTos,1);` |
|       23 |  2949 | `	PH7_MemObjRelease(pTos);` |
|       23 |  2950 | `	pTos->x.iVal = iRes;` |
|       23 |  2951 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       23 |  2952 | `	break;` |
|        - |  2953 | `				 }` |
|        - |  2954 |  |
|        - |  2955 | `/*` |
|        - |  2956 | ` * LOADC P1 P2 *` |
|        - |  2957 | ` *` |
|        - |  2958 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  2959 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  2960 | ` */` |
|   649211 |  2961 | `case PH7_OP_LOADC: {` |
|        - |  2962 | `	ph7_value *pObj;` |
|        - |  2963 | `	/* Reserve a room */` |
|  1298468 |  2964 | `	pTos++;` |
|  1298468 |  2965 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1298468 |  2966 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2967 | `			SyHashEntry *pEntry;` |
|        - |  2968 | `			/* Candidate for expansion via user defined callbacks */` |
|    13668 |  2969 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    13668 |  2970 | `			if( pEntry ){` |
|    11876 |  2971 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2972 | `				/* Set a NULL default value */` |
|    11876 |  2973 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    11876 |  2974 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  2975 | `				/* Invoke the callback and deal with the expanded value */` |
|    11876 |  2976 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  2977 | `				/* Mark as constant */` |
|    11876 |  2978 | `				pTos->nIdx = SXU32_HIGH;` |
|    11876 |  2979 | `				break;` |
|        - |  2980 | `			}` |
|      896 |  2981 | `		}` |
|  1286594 |  2982 | `		PH7_MemObjLoad(pObj,pTos);` |
|   643320 |  2983 | `	}else{` |
|        - |  2984 | `		/* Set a NULL value */` |
|      ! 0 |  2985 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2986 | `	}` |
|        - |  2987 | `	/* Mark as constant */` |
|  1286594 |  2988 | `	pTos->nIdx = SXU32_HIGH;` |
|  1286594 |  2989 | `	break;` |
|        - |  2990 | `				  }` |
|        - |  2991 | `/*` |
|        - |  2992 | ` * LOAD: P1 * P3` |
|        - |  2993 | ` *` |
|        - |  2994 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  2995 | ` * from the P3 operand.` |
|        - |  2996 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  2997 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  2998 | ` */` |
|  1009612 |  2999 | `case PH7_OP_LOAD:{` |
|        - |  3000 | `	ph7_value *pObj;` |
|        - |  3001 | `	SyString sName;` |
|  2019446 |  3002 | `	if( pInstr->p3 == 0 ){` |
|        - |  3003 | `		/* Take the variable name from the top of the stack */` |
|        - |  3004 | `#ifdef UNTRUST` |
|        - |  3005 | `		if( pTos < pStack ){` |
|        - |  3006 | `			goto Abort;` |
|        - |  3007 | `		}` |
|        - |  3008 | `#endif` |
|        - |  3009 | `		/* Force a string cast */` |
|       19 |  3010 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3011 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3012 | `		}` |
|       19 |  3013 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3014 | `	}else{` |
|  2019428 |  3015 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3016 | `		/* Reserve a room for the target object */` |
|  2019428 |  3017 | `		pTos++;` |
|        - |  3018 | `	}` |
|        - |  3019 | `	/* Extract the requested memory object */` |
|  2019446 |  3020 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2019446 |  3021 | `	if( pObj == 0 ){` |
|      476 |  3022 | `		if( pInstr->iP1 ){` |
|        - |  3023 | `			/* Variable not found,load NULL */` |
|      476 |  3024 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3025 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3026 | `			}else{` |
|      476 |  3027 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3028 | `			}` |
|      476 |  3029 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1009851 |  3030 | `			break;` |
|      ! 0 |  3031 | `		}else{` |
|        - |  3032 | `			/* Fatal error */` |
|      ! 0 |  3033 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3034 | `			goto Abort;` |
|        - |  3035 | `		}` |
|        - |  3036 | `	}` |
|        - |  3037 | `	/* Load variable contents */` |
|  2018972 |  3038 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2018972 |  3039 | `	pTos->nIdx = pObj->nIdx;` |
|  2018972 |  3040 | `	break;` |
|        - |  3041 | `				   }` |
|        - |  3042 | `/*` |
|        - |  3043 | ` * LOAD_MAP P1 * *` |
|        - |  3044 | ` *` |
|        - |  3045 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3046 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3047 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3048 | ` */` |
|    13975 |  3049 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3050 | `	ph7_hashmap *pMap;` |
|        - |  3051 | `	/* Allocate a new hashmap instance */` |
|    27952 |  3052 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    27952 |  3053 | `	if( pMap == 0 ){` |
|      ! 0 |  3054 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3055 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3056 | `		goto Abort;` |
|        - |  3057 | `	}` |
|    27952 |  3058 | `	if( pInstr->iP1 > 0 ){` |
|     1534 |  3059 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3060 | `		/* Perform the insertion */` |
|     4408 |  3061 | `		while( pEntry < pTos ){` |
|     2876 |  3062 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3063 | `				/* Insertion by reference */` |
|      142 |  3064 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3065 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3066 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3067 | `					);` |
|       48 |  3068 | `			}else{` |
|        - |  3069 | `				/* Standard insertion */` |
|     4172 |  3070 | `				PH7_HashmapInsert(pMap,` |
|     2780 |  3071 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     1390 |  3072 | `					&pEntry[1]` |
|        - |  3073 | `				);` |
|        - |  3074 | `			}` |
|        - |  3075 | `			/* Next pair on the stack */` |
|     2876 |  3076 | `			pEntry += 2;` |
|        2 |  3077 | `		}` |
|        - |  3078 | `		/* Pop P1 elements */` |
|     1534 |  3079 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|      766 |  3080 | `	}` |
|        - |  3081 | `	/* Push the hashmap */` |
|    27952 |  3082 | `	pTos++;` |
|    27952 |  3083 | `	pTos->nIdx = SXU32_HIGH;` |
|    27952 |  3084 | `	pTos->x.pOther = pMap;` |
|    27952 |  3085 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    27952 |  3086 | `	break;` |
|        - |  3087 | `					  }` |
|        - |  3088 | `/*` |
|        - |  3089 | ` * LOAD_LIST: P1 * *` |
|        - |  3090 | ` *` |
|        - |  3091 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3092 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3093 | ` * Caveats:` |
|        - |  3094 | ` *  This implementation support only a single nesting level.` |
|        - |  3095 | ` */` |
|       17 |  3096 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3097 | `	ph7_value *pEntry;` |
|       35 |  3098 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3099 | `		/* Empty list,break immediately */` |
|      ! 0 |  3100 | `		break;` |
|        - |  3101 | `	}` |
|       35 |  3102 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3103 | `#ifdef UNTRUST` |
|        - |  3104 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3105 | `		goto Abort;` |
|        - |  3106 | `	}` |
|        - |  3107 | `#endif` |
|       35 |  3108 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       31 |  3109 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3110 | `		ph7_hashmap_node *pNode;` |
|        - |  3111 | `		ph7_value sKey,*pObj;` |
|        - |  3112 | `		/* Start Copying */` |
|       31 |  3113 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|       99 |  3114 | `		while( pEntry <= pTos ){` |
|       69 |  3115 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       65 |  3116 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       65 |  3117 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       65 |  3118 | `					if( rc == SXRET_OK ){` |
|        - |  3119 | `						/* Store node value */` |
|       65 |  3120 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       33 |  3121 | `					}else{` |
|        - |  3122 | `						/* Nullify the variable */` |
|      ! 0 |  3123 | `						PH7_MemObjRelease(pObj);` |
|        - |  3124 | `					}` |
|       32 |  3125 | `				}` |
|       32 |  3126 | `			}` |
|       69 |  3127 | `			sKey.x.iVal++; /* Next numeric index */` |
|       69 |  3128 | `			pEntry++;` |
|        1 |  3129 | `		}` |
|       15 |  3130 | `	}` |
|       35 |  3131 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       35 |  3132 | `	break;` |
|        - |  3133 | `					   }` |
|        - |  3134 | `/*` |
|        - |  3135 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3136 | ` *` |
|        - |  3137 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3138 | ` * from the stack.` |
|        - |  3139 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3140 | ` * instead.` |
|        - |  3141 | ` */` |
|   153238 |  3142 | `case PH7_OP_LOAD_IDX: {` |
|   306522 |  3143 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   306522 |  3144 | `	ph7_hashmap *pMap = 0;` |
|        - |  3145 | `	ph7_value *pIdx;` |
|   306522 |  3146 | `	pIdx = 0;` |
|   306522 |  3147 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3148 | `		if( !pInstr->iP2){` |
|        - |  3149 | `			/* No available index,load NULL */` |
|      ! 0 |  3150 | `			if( pTos >= pStack ){` |
|      ! 0 |  3151 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3152 | `			}else{` |
|        - |  3153 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3154 | `				pTos++;` |
|      ! 0 |  3155 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3156 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3157 | `			}` |
|        - |  3158 | `			/* Emit a notice */` |
|      ! 0 |  3159 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3160 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3161 | `			break;` |
|        - |  3162 | `		}` |
|      ! 0 |  3163 | `	}else{` |
|   306522 |  3164 | `		pIdx = pTos;` |
|   306522 |  3165 | `		pTos--;` |
|        - |  3166 | `	}` |
|   306522 |  3167 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3168 | `		/* String access */` |
|   237166 |  3169 | `		if( pIdx ){` |
|        - |  3170 | `			sxu32 nOfft;` |
|   237166 |  3171 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3172 | `				/* Force an int cast */` |
|      ! 0 |  3173 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3174 | `			}` |
|   237166 |  3175 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   237166 |  3176 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3177 | `				/* Invalid offset,load null */` |
|      ! 0 |  3178 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3179 | `			}else{` |
|   237166 |  3180 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   237166 |  3181 | `				int c = zData[nOfft];` |
|   237166 |  3182 | `				PH7_MemObjRelease(pTos);` |
|   237166 |  3183 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   237166 |  3184 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3185 | `			}` |
|   118606 |  3186 | `		}else{` |
|        - |  3187 | `			/* No available index,load NULL */` |
|      ! 0 |  3188 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3189 | `		}` |
|   237166 |  3190 | `		break;` |
|        - |  3191 | `	}` |
|    69358 |  3192 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3193 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3194 | `			ph7_value *pObj;` |
|      ! 0 |  3195 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3196 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3197 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3198 | `			}` |
|      ! 0 |  3199 | `		}` |
|      ! 0 |  3200 | `	}` |
|    69358 |  3201 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    69358 |  3202 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3203 | `		/* Point to the hashmap */` |
|    69358 |  3204 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    69358 |  3205 | `		if( pIdx ){` |
|        - |  3206 | `			/* Load the desired entry */` |
|    69358 |  3207 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    34678 |  3208 | `		}` |
|    69358 |  3209 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3210 | `			/* Create a new empty entry */` |
|      ! 0 |  3211 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3212 | `			if( rc == SXRET_OK ){` |
|        - |  3213 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3214 | `				pNode = pMap->pLast;` |
|      ! 0 |  3215 | `			}` |
|      ! 0 |  3216 | `		}` |
|    34678 |  3217 | `	}` |
|    69358 |  3218 | `	if( pIdx ){` |
|    69358 |  3219 | `		PH7_MemObjRelease(pIdx);` |
|    34678 |  3220 | `	}` |
|    69358 |  3221 | `	if( rc == SXRET_OK ){` |
|        - |  3222 | `		/* Load entry contents */` |
|    33100 |  3223 | `		if( pMap->iRef < 2 ){` |
|        - |  3224 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3225 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3226 | `			 */` |
|        5 |  3227 | `			pTos->nIdx = SXU32_HIGH;` |
|        5 |  3228 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|        3 |  3229 | `		}else{` |
|    33096 |  3230 | `			pTos->nIdx = pNode->nValIdx;` |
|    33096 |  3231 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    33096 |  3232 | `			PH7_HashmapUnref(pMap);` |
|        - |  3233 | `		}` |
|    16551 |  3234 | `	}else{` |
|        - |  3235 | `		/* No such entry,load NULL */` |
|    36260 |  3236 | `		PH7_MemObjRelease(pTos);` |
|    36260 |  3237 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3238 | `	}` |
|    69358 |  3239 | `	break;` |
|        - |  3240 | `					  }` |
|        - |  3241 | `/*` |
|        - |  3242 | ` * LOAD_CLOSURE * * P3` |
|        - |  3243 | ` *` |
|        - |  3244 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3245 | ` * name in the stack.` |
|        - |  3246 | ` */` |
|        2 |  3247 | `case PH7_OP_LOAD_CLOSURE:{` |
|        5 |  3248 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        5 |  3249 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3250 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3251 | `		ph7_vm_func *pClosure;` |
|        - |  3252 | `		char *zName;` |
|        - |  3253 | `		sxu32 mLen;` |
|        - |  3254 | `		sxu32 n;` |
|        - |  3255 | `		/* Create a new VM function */` |
|        5 |  3256 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3257 | `		/* Generate an unique closure name */` |
|        5 |  3258 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        5 |  3259 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3260 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3261 | `			goto Abort;` |
|        - |  3262 | `		}` |
|        5 |  3263 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        5 |  3264 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3265 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3266 | `		}` |
|        - |  3267 | `		/* Zero the stucture */` |
|        5 |  3268 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3269 | `		/* Perform a structure assignment on read-only items */` |
|        5 |  3270 | `		pClosure->aArgs = pFunc->aArgs;` |
|        5 |  3271 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        5 |  3272 | `		pClosure->aStatic = pFunc->aStatic;` |
|        5 |  3273 | `		pClosure->iFlags = pFunc->iFlags;` |
|        5 |  3274 | `		pClosure->pUserData = pFunc->pUserData;` |
|        5 |  3275 | `		pClosure->sSignature = pFunc->sSignature;` |
|        5 |  3276 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3277 | `		/* Register the closure */` |
|        5 |  3278 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3279 | `		/* Set up closure environment */` |
|        5 |  3280 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        5 |  3281 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       13 |  3282 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3283 | `			ph7_value *pValue;` |
|        9 |  3284 | `			pEnv = &aEnv[n];` |
|        9 |  3285 | `			sEnv.sName  = pEnv->sName;` |
|        9 |  3286 | `			sEnv.iFlags = pEnv->iFlags;` |
|        9 |  3287 | `			sEnv.nIdx = SXU32_HIGH;` |
|        9 |  3288 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|        9 |  3289 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3290 | `				/* Pass by reference */` |
|      ! 0 |  3291 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3292 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3293 | `					);` |
|      ! 0 |  3294 | `			}` |
|        - |  3295 | `			/* Standard pass by value */` |
|        9 |  3296 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|        9 |  3297 | `			if( pValue ){` |
|        - |  3298 | `				/* Copy imported value */` |
|        5 |  3299 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        2 |  3300 | `			}` |
|        - |  3301 | `			/* Insert the imported variable */` |
|        9 |  3302 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|        5 |  3303 | `		}` |
|        - |  3304 | `		/* Finally,load the closure name on the stack */` |
|        5 |  3305 | `		pTos++;` |
|        5 |  3306 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        2 |  3307 | `	}` |
|        5 |  3308 | `	break;` |
|        - |  3309 | `						 }` |
|        - |  3310 | `/*` |
|        - |  3311 | ` * STORE * P2 P3` |
|        - |  3312 | ` *` |
|        - |  3313 | ` * Perform a store (Assignment) operation.` |
|        - |  3314 | ` */` |
|    84743 |  3315 | `case PH7_OP_STORE: {` |
|        - |  3316 | `	ph7_value *pObj;` |
|        - |  3317 | `	SyString sName;` |
|        - |  3318 | `#ifdef UNTRUST` |
|        - |  3319 | `	if( pTos < pStack ){` |
|        - |  3320 | `		goto Abort;` |
|        - |  3321 | `	}` |
|        - |  3322 | `#endif` |
|   169488 |  3323 | `	if( pInstr->iP2 ){` |
|        - |  3324 | `		sxu32 nIdx;` |
|        - |  3325 | `		/* Member store operation */` |
|     1198 |  3326 | `		nIdx = pTos->nIdx;` |
|     1198 |  3327 | `		VmPopOperand(&pTos,1);` |
|     1198 |  3328 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3329 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3330 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3331 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3332 | `		}else{` |
|        - |  3333 | `			/* Point to the desired memory object */` |
|     1194 |  3334 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     1194 |  3335 | `			if( pObj ){` |
|        - |  3336 | `				/* Perform the store operation */` |
|     1194 |  3337 | `				PH7_MemObjStore(pTos,pObj);` |
|      596 |  3338 | `			}` |
|        - |  3339 | `		}` |
|    85343 |  3340 | `		break;` |
|   168292 |  3341 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3342 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3343 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3344 | `			/* Force a string cast */` |
|      ! 0 |  3345 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3346 | `		}` |
|        7 |  3347 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3348 | `		pTos--;` |
|        - |  3349 | `#ifdef UNTRUST` |
|        - |  3350 | `		if( pTos < pStack  ){` |
|        - |  3351 | `			goto Abort;` |
|        - |  3352 | `		}` |
|        - |  3353 | `#endif` |
|        4 |  3354 | `	}else{` |
|   168286 |  3355 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3356 | `	}` |
|        - |  3357 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   168292 |  3358 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   168292 |  3359 | `	if( pObj == 0 ){` |
|      ! 0 |  3360 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3361 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3362 | `		goto Abort;` |
|        - |  3363 | `	}` |
|   168292 |  3364 | `	if( !pInstr->p3 ){` |
|        7 |  3365 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3366 | `	}` |
|        - |  3367 | `	/* Perform the store operation */` |
|   168292 |  3368 | `	PH7_MemObjStore(pTos,pObj);` |
|   168292 |  3369 | `	break;` |
|        - |  3370 | `				   }` |
|        - |  3371 | `/*` |
|        - |  3372 | ` * STORE_IDX:   P1 * P3` |
|        - |  3373 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3374 | ` *` |
|        - |  3375 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3376 | ` */` |
|    70261 |  3377 | `case PH7_OP_STORE_IDX:` |
|        - |  3378 | `case PH7_OP_STORE_IDX_REF: {` |
|   140524 |  3379 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3380 | `	ph7_value *pKey;` |
|        - |  3381 | `	sxu32 nIdx;` |
|   140524 |  3382 | `	if( pInstr->iP1 ){` |
|        - |  3383 | `		/* Key is next on stack */` |
|    51860 |  3384 | `		pKey = pTos;` |
|    51860 |  3385 | `		pTos--;` |
|    25931 |  3386 | `	}else{` |
|    88666 |  3387 | `		pKey = 0;` |
|        - |  3388 | `	}` |
|   140524 |  3389 | `	nIdx = pTos->nIdx;` |
|   140524 |  3390 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3391 | `		/* Hashmap already loaded */` |
|   140472 |  3392 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   140472 |  3393 | `		if( pMap->iRef < 2 ){` |
|        - |  3394 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3395 | `			pMap->iRef = 2;` |
|      ! 0 |  3396 | `		}` |
|    70237 |  3397 | `	}else{` |
|        - |  3398 | `		ph7_value *pObj;` |
|       53 |  3399 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3400 | `		if( pObj == 0 ){` |
|      ! 0 |  3401 | `			if( pKey ){` |
|      ! 0 |  3402 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3403 | `			}` |
|      ! 0 |  3404 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3405 | `			break;` |
|        - |  3406 | `		}` |
|        - |  3407 | `		/* Phase#1: Load the array */` |
|       53 |  3408 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3409 | `			VmPopOperand(&pTos,1);` |
|       53 |  3410 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3411 | `				/* Force a string cast */` |
|      ! 0 |  3412 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3413 | `			}` |
|       53 |  3414 | `			if( pKey == 0 ){` |
|        - |  3415 | `				/* Append string */` |
|        3 |  3416 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3417 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3418 | `				}` |
|        2 |  3419 | `			}else{` |
|        - |  3420 | `				sxu32 nOfft;` |
|       51 |  3421 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3422 | `					/* Force an int cast */` |
|       51 |  3423 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3424 | `				}` |
|       51 |  3425 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3426 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3427 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3428 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3429 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3430 | `				}else{` |
|      ! 0 |  3431 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3432 | `						/* Perform an append operation */` |
|      ! 0 |  3433 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3434 | `					}` |
|        - |  3435 | `				}` |
|        - |  3436 | `			}` |
|       53 |  3437 | `			if( pKey ){` |
|       51 |  3438 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3439 | `			}` |
|       53 |  3440 | `			break;` |
|      ! 0 |  3441 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3442 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3443 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3444 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3445 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3446 | `				goto Abort;` |
|        - |  3447 | `			}` |
|      ! 0 |  3448 | `		}` |
|      ! 0 |  3449 | `		pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - |  3450 | `	}` |
|   140472 |  3451 | `	VmPopOperand(&pTos,1);` |
|        - |  3452 | `	/* Phase#2: Perform the insertion */` |
|   140472 |  3453 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3454 | `		/* Insertion by reference */` |
|       13 |  3455 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        7 |  3456 | `	}else{` |
|   140460 |  3457 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3458 | `	}` |
|   140472 |  3459 | `	if( pKey ){` |
|    51810 |  3460 | `		PH7_MemObjRelease(pKey);` |
|    25904 |  3461 | `	}` |
|   140472 |  3462 | `	break;` |
|        - |  3463 | `					   }` |
|        - |  3464 | `/*` |
|        - |  3465 | ` * INCR: P1 * *` |
|        - |  3466 | ` *` |
|        - |  3467 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3468 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3469 | ` * the stack and increment after that.` |
|        - |  3470 | ` */` |
|   115645 |  3471 | `case PH7_OP_INCR:` |
|        - |  3472 | `#ifdef UNTRUST` |
|        - |  3473 | `	if( pTos < pStack ){` |
|        - |  3474 | `		goto Abort;` |
|        - |  3475 | `	}` |
|        - |  3476 | `#endif` |
|   231336 |  3477 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   231336 |  3478 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3479 | `			ph7_value *pObj;` |
|   231336 |  3480 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3481 | `				/* Force a numeric cast */` |
|   231336 |  3482 | `				PH7_MemObjToNumeric(pObj);` |
|   231336 |  3483 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3484 | `					pObj->rVal++;` |
|        - |  3485 | `					/* Try to get an integer representation */` |
|      ! 0 |  3486 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3487 | `				}else{` |
|   231336 |  3488 | `					pObj->x.iVal++;` |
|   231336 |  3489 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3490 | `				}` |
|   231336 |  3491 | `				if( pInstr->iP1 ){` |
|        - |  3492 | `					/* Pre-icrement */` |
|       55 |  3493 | `					PH7_MemObjStore(pObj,pTos);` |
|       27 |  3494 | `				}` |
|   115689 |  3495 | `			}` |
|   115691 |  3496 | `		}else{` |
|      ! 0 |  3497 | `			if( pInstr->iP1 ){` |
|        - |  3498 | `				/* Force a numeric cast */` |
|      ! 0 |  3499 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3500 | `				/* Pre-increment */` |
|      ! 0 |  3501 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3502 | `					pTos->rVal++;` |
|        - |  3503 | `					/* Try to get an integer representation */` |
|      ! 0 |  3504 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3505 | `				}else{` |
|      ! 0 |  3506 | `					pTos->x.iVal++;` |
|      ! 0 |  3507 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3508 | `				}` |
|      ! 0 |  3509 | `			}` |
|        - |  3510 | `		}` |
|   115689 |  3511 | `	}` |
|   231336 |  3512 | `	break;` |
|        - |  3513 | `/*` |
|        - |  3514 | ` * DECR: P1 * *` |
|        - |  3515 | ` *` |
|        - |  3516 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3517 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3518 | ` * and decrement after that.` |
|        - |  3519 | ` */` |
|        2 |  3520 | `case PH7_OP_DECR:` |
|        - |  3521 | `#ifdef UNTRUST` |
|        - |  3522 | `	if( pTos < pStack ){` |
|        - |  3523 | `		goto Abort;` |
|        - |  3524 | `	}` |
|        - |  3525 | `#endif` |
|        5 |  3526 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3527 | `		/* Force a numeric cast */` |
|        5 |  3528 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3529 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3530 | `			ph7_value *pObj;` |
|        5 |  3531 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3532 | `				/* Force a numeric cast */` |
|        5 |  3533 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3534 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3535 | `					pObj->rVal--;` |
|        - |  3536 | `					/* Try to get an integer representation */` |
|      ! 0 |  3537 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3538 | `				}else{` |
|        5 |  3539 | `					pObj->x.iVal--;` |
|        5 |  3540 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3541 | `				}` |
|        5 |  3542 | `				if( pInstr->iP1 ){` |
|        - |  3543 | `					/* Pre-icrement */` |
|      ! 0 |  3544 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3545 | `				}` |
|        2 |  3546 | `			}` |
|        3 |  3547 | `		}else{` |
|      ! 0 |  3548 | `			if( pInstr->iP1 ){` |
|        - |  3549 | `				/* Pre-increment */` |
|      ! 0 |  3550 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3551 | `					pTos->rVal--;` |
|        - |  3552 | `					/* Try to get an integer representation */` |
|      ! 0 |  3553 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3554 | `				}else{` |
|      ! 0 |  3555 | `					pTos->x.iVal--;` |
|      ! 0 |  3556 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3557 | `				}` |
|      ! 0 |  3558 | `			}` |
|        - |  3559 | `		}` |
|        2 |  3560 | `	}` |
|        5 |  3561 | `	break;` |
|        - |  3562 | `/*` |
|        - |  3563 | ` * UMINUS: * * *` |
|        - |  3564 | ` *` |
|        - |  3565 | ` * Perform a unary minus operation.` |
|        - |  3566 | ` */` |
|    18183 |  3567 | `case PH7_OP_UMINUS:` |
|        - |  3568 | `#ifdef UNTRUST` |
|        - |  3569 | `	if( pTos < pStack ){` |
|        - |  3570 | `		goto Abort;` |
|        - |  3571 | `	}` |
|        - |  3572 | `#endif` |
|        - |  3573 | `	/* Force a numeric (integer,real or both) cast */` |
|    36368 |  3574 | `	PH7_MemObjToNumeric(pTos);` |
|    36368 |  3575 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       23 |  3576 | `		pTos->rVal = -pTos->rVal;` |
|       11 |  3577 | `	}` |
|    36368 |  3578 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    36346 |  3579 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    18172 |  3580 | `	}` |
|    36368 |  3581 | `	break;` |
|        - |  3582 | `/*` |
|        - |  3583 | ` * UPLUS: * * *` |
|        - |  3584 | ` *` |
|        - |  3585 | ` * Perform a unary plus operation.` |
|        - |  3586 | ` */` |
|       16 |  3587 | `case PH7_OP_UPLUS:` |
|        - |  3588 | `#ifdef UNTRUST` |
|        - |  3589 | `	if( pTos < pStack ){` |
|        - |  3590 | `		goto Abort;` |
|        - |  3591 | `	}` |
|        - |  3592 | `#endif` |
|        - |  3593 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3594 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3595 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3596 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3597 | `	}` |
|       33 |  3598 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3599 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3600 | `	}` |
|       33 |  3601 | `	break;` |
|        - |  3602 | `/*` |
|        - |  3603 | ` * OP_LNOT: * * *` |
|        - |  3604 | ` *` |
|        - |  3605 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3606 | ` * with its complement.` |
|        - |  3607 | ` */` |
|    34050 |  3608 | `case PH7_OP_LNOT:` |
|        - |  3609 | `#ifdef UNTRUST` |
|        - |  3610 | `	if( pTos < pStack ){` |
|        - |  3611 | `		goto Abort;` |
|        - |  3612 | `	}` |
|        - |  3613 | `#endif` |
|        - |  3614 | `	/* Force a boolean cast */` |
|    68146 |  3615 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3616 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3617 | `	}` |
|    68146 |  3618 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    68146 |  3619 | `	break;` |
|        - |  3620 | `/*` |
|        - |  3621 | ` * OP_BITNOT: * * *` |
|        - |  3622 | ` *` |
|        - |  3623 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3624 | ` * with its ones-complement.` |
|        - |  3625 | ` */` |
|        3 |  3626 | `case PH7_OP_BITNOT:` |
|        - |  3627 | `#ifdef UNTRUST` |
|        - |  3628 | `	if( pTos < pStack ){` |
|        - |  3629 | `		goto Abort;` |
|        - |  3630 | `	}` |
|        - |  3631 | `#endif` |
|        - |  3632 | `	/* Force an integer cast */` |
|        7 |  3633 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3634 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3635 | `	}` |
|        7 |  3636 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|        7 |  3637 | `	break;` |
|        - |  3638 | `/* OP_MUL * * *` |
|        - |  3639 | ` * OP_MUL_STORE * * *` |
|        - |  3640 | ` *` |
|        - |  3641 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3642 | ` * and push the result back onto the stack.` |
|        - |  3643 | ` */` |
|     1231 |  3644 | `case PH7_OP_MUL:` |
|        - |  3645 | `case PH7_OP_MUL_STORE: {` |
|     2464 |  3646 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3647 | `	/* Force the operand to be numeric */` |
|        - |  3648 | `#ifdef UNTRUST` |
|        - |  3649 | `	if( pNos < pStack ){` |
|        - |  3650 | `		goto Abort;` |
|        - |  3651 | `	}` |
|        - |  3652 | `#endif` |
|     2464 |  3653 | `	PH7_MemObjToNumeric(pTos);` |
|     2464 |  3654 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3655 | `	/* Perform the requested operation */` |
|     2464 |  3656 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3657 | `		/* Floating point arithemic */` |
|        - |  3658 | `		ph7_real a,b,r;` |
|       17 |  3659 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3660 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3661 | `		}` |
|       17 |  3662 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3663 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3664 | `		}` |
|       17 |  3665 | `		a = pNos->rVal;` |
|       17 |  3666 | `		b = pTos->rVal;` |
|       17 |  3667 | `		r = a * b;` |
|        - |  3668 | `		/* Push the result */` |
|       17 |  3669 | `		pNos->rVal = r;` |
|       17 |  3670 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3671 | `		/* Try to get an integer representation */` |
|       17 |  3672 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3673 | `	}else{` |
|        - |  3674 | `		/* Integer arithmetic */` |
|        - |  3675 | `		sxi64 a,b,r;` |
|     2448 |  3676 | `		a = pNos->x.iVal;` |
|     2448 |  3677 | `		b = pTos->x.iVal;` |
|     2448 |  3678 | `		r = a * b;` |
|        - |  3679 | `		/* Push the result */` |
|     2448 |  3680 | `		pNos->x.iVal = r;` |
|     2448 |  3681 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3682 | `	}` |
|     2464 |  3683 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3684 | `		ph7_value *pObj;` |
|       19 |  3685 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3686 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  3687 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  3688 | `			PH7_MemObjStore(pNos,pObj);` |
|        9 |  3689 | `		}` |
|        9 |  3690 | `	}` |
|     2464 |  3691 | `	VmPopOperand(&pTos,1);` |
|     2464 |  3692 | `	break;` |
|        - |  3693 | `				 }` |
|        - |  3694 | `/* OP_ADD * * *` |
|        - |  3695 | ` *` |
|        - |  3696 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3697 | ` * and push the result back onto the stack.` |
|        - |  3698 | ` */` |
|      420 |  3699 | `case PH7_OP_ADD:{` |
|      842 |  3700 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3701 | `#ifdef UNTRUST` |
|        - |  3702 | `	if( pNos < pStack ){` |
|        - |  3703 | `		goto Abort;` |
|        - |  3704 | `	}` |
|        - |  3705 | `#endif` |
|        - |  3706 | `	/* Perform the addition */` |
|      842 |  3707 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      842 |  3708 | `	VmPopOperand(&pTos,1);` |
|      842 |  3709 | `	break;` |
|        - |  3710 | `				}` |
|        - |  3711 | `/*` |
|        - |  3712 | ` * OP_ADD_STORE * * *` |
|        - |  3713 | ` *` |
|        - |  3714 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3715 | ` * and push the result back onto the stack.` |
|        - |  3716 | ` */` |
|      481 |  3717 | `case PH7_OP_ADD_STORE:{` |
|      963 |  3718 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3719 | `	ph7_value *pObj;` |
|        - |  3720 | `	sxu32 nIdx;` |
|        - |  3721 | `#ifdef UNTRUST` |
|        - |  3722 | `	if( pNos < pStack ){` |
|        - |  3723 | `		goto Abort;` |
|        - |  3724 | `	}` |
|        - |  3725 | `#endif` |
|        - |  3726 | `	/* Perform the addition */` |
|      963 |  3727 | `	nIdx = pTos->nIdx;` |
|      963 |  3728 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3729 | `	/* Peform the store operation */` |
|      963 |  3730 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3731 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      963 |  3732 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      963 |  3733 | `		PH7_MemObjStore(pTos,pObj);` |
|      481 |  3734 | `	}` |
|        - |  3735 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      963 |  3736 | `	PH7_MemObjStore(pTos,pNos);` |
|      963 |  3737 | `	VmPopOperand(&pTos,1);` |
|      963 |  3738 | `	break;` |
|        - |  3739 | `				}` |
|        - |  3740 | `/* OP_SUB * * *` |
|        - |  3741 | ` *` |
|        - |  3742 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3743 | ` * first (what was next on the stack) from the second (the` |
|        - |  3744 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3745 | ` */` |
|      283 |  3746 | `case PH7_OP_SUB: {` |
|      567 |  3747 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3748 | `#ifdef UNTRUST` |
|        - |  3749 | `	if( pNos < pStack ){` |
|        - |  3750 | `		goto Abort;` |
|        - |  3751 | `	}` |
|        - |  3752 | `#endif` |
|      567 |  3753 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3754 | `		/* Floating point arithemic */` |
|        - |  3755 | `		ph7_real a,b,r;` |
|       73 |  3756 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3757 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3758 | `		}` |
|       73 |  3759 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3760 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3761 | `		}` |
|       73 |  3762 | `		a = pNos->rVal;` |
|       73 |  3763 | `		b = pTos->rVal;` |
|       73 |  3764 | `		r = a - b;` |
|        - |  3765 | `		/* Push the result */` |
|       73 |  3766 | `		pNos->rVal = r;` |
|       73 |  3767 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3768 | `		/* Try to get an integer representation */` |
|       73 |  3769 | `		PH7_MemObjTryInteger(pNos);` |
|       37 |  3770 | `	}else{` |
|        - |  3771 | `		/* Integer arithmetic */` |
|        - |  3772 | `		sxi64 a,b,r;` |
|      495 |  3773 | `		a = pNos->x.iVal;` |
|      495 |  3774 | `		b = pTos->x.iVal;` |
|      495 |  3775 | `		r = a - b;` |
|        - |  3776 | `		/* Push the result */` |
|      495 |  3777 | `		pNos->x.iVal = r;` |
|      495 |  3778 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3779 | `	}` |
|      567 |  3780 | `	VmPopOperand(&pTos,1);` |
|      567 |  3781 | `	break;` |
|        - |  3782 | `				 }` |
|        - |  3783 | `/* OP_SUB_STORE * * *` |
|        - |  3784 | ` *` |
|        - |  3785 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3786 | ` * first (what was next on the stack) from the second (the` |
|        - |  3787 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3788 | ` */` |
|        1 |  3789 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3790 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3791 | `	ph7_value *pObj;` |
|        - |  3792 | `#ifdef UNTRUST` |
|        - |  3793 | `	if( pNos < pStack ){` |
|        - |  3794 | `		goto Abort;` |
|        - |  3795 | `	}` |
|        - |  3796 | `#endif` |
|        3 |  3797 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3798 | `		/* Floating point arithemic */` |
|        - |  3799 | `		ph7_real a,b,r;` |
|      ! 0 |  3800 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3801 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3802 | `		}` |
|      ! 0 |  3803 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3804 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  3805 | `		}` |
|      ! 0 |  3806 | `		a = pTos->rVal;` |
|      ! 0 |  3807 | `		b = pNos->rVal;` |
|      ! 0 |  3808 | `		r = a - b;` |
|        - |  3809 | `		/* Push the result */` |
|      ! 0 |  3810 | `		pNos->rVal = r;` |
|      ! 0 |  3811 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3812 | `		/* Try to get an integer representation */` |
|      ! 0 |  3813 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  3814 | `	}else{` |
|        - |  3815 | `		/* Integer arithmetic */` |
|        - |  3816 | `		sxi64 a,b,r;` |
|        3 |  3817 | `		a = pTos->x.iVal;` |
|        3 |  3818 | `		b = pNos->x.iVal;` |
|        3 |  3819 | `		r = a - b;` |
|        - |  3820 | `		/* Push the result */` |
|        3 |  3821 | `		pNos->x.iVal = r;` |
|        3 |  3822 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3823 | `	}` |
|        3 |  3824 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3825 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3826 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3827 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3828 | `	}` |
|        3 |  3829 | `	VmPopOperand(&pTos,1);` |
|        3 |  3830 | `	break;` |
|        - |  3831 | `				 }` |
|        - |  3832 |  |
|        - |  3833 | `/*` |
|        - |  3834 | ` * OP_MOD * * *` |
|        - |  3835 | ` *` |
|        - |  3836 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3837 | ` * first (what was next on the stack) from the second (the` |
|        - |  3838 | ` * top of the stack) and push the remainder after division` |
|        - |  3839 | ` * onto the stack.` |
|        - |  3840 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3841 | ` */` |
|      296 |  3842 | `case PH7_OP_MOD:{` |
|      594 |  3843 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3844 | `	sxi64 a,b,r;` |
|        - |  3845 | `#ifdef UNTRUST` |
|        - |  3846 | `	if( pNos < pStack ){` |
|        - |  3847 | `		goto Abort;` |
|        - |  3848 | `	}` |
|        - |  3849 | `#endif` |
|        - |  3850 | `	/* Force the operands to be integer */` |
|      594 |  3851 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3852 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3853 | `	}` |
|      594 |  3854 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  3855 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  3856 | `	}` |
|        - |  3857 | `	/* Perform the requested operation */` |
|      594 |  3858 | `	a = pNos->x.iVal;` |
|      594 |  3859 | `	b = pTos->x.iVal;` |
|      594 |  3860 | `	if( b == 0 ){` |
|        3 |  3861 | `		r = 0;` |
|        3 |  3862 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3863 | `		/* goto Abort; */` |
|        2 |  3864 | `	}else{` |
|      591 |  3865 | `		r = a%b;` |
|        - |  3866 | `	}` |
|        - |  3867 | `	/* Push the result */` |
|      594 |  3868 | `	pNos->x.iVal = r;` |
|      594 |  3869 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      594 |  3870 | `	VmPopOperand(&pTos,1);` |
|      594 |  3871 | `	break;` |
|        - |  3872 | `				}` |
|        - |  3873 | `/*` |
|        - |  3874 | ` * OP_MOD_STORE * * *` |
|        - |  3875 | ` *` |
|        - |  3876 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3877 | ` * first (what was next on the stack) from the second (the` |
|        - |  3878 | ` * top of the stack) and push the remainder after division` |
|        - |  3879 | ` * onto the stack.` |
|        - |  3880 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3881 | ` */` |
|        1 |  3882 | `case PH7_OP_MOD_STORE: {` |
|        3 |  3883 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3884 | `	ph7_value *pObj;` |
|        - |  3885 | `	sxi64 a,b,r;` |
|        - |  3886 | `#ifdef UNTRUST` |
|        - |  3887 | `	if( pNos < pStack ){` |
|        - |  3888 | `		goto Abort;` |
|        - |  3889 | `	}` |
|        - |  3890 | `#endif` |
|        - |  3891 | `	/* Force the operands to be integer */` |
|        3 |  3892 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3893 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3894 | `	}` |
|        3 |  3895 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3896 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3897 | `	}` |
|        - |  3898 | `	/* Perform the requested operation */` |
|        3 |  3899 | `	a = pTos->x.iVal;` |
|        3 |  3900 | `	b = pNos->x.iVal;` |
|        3 |  3901 | `	if( b == 0 ){` |
|      ! 0 |  3902 | `		r = 0;` |
|      ! 0 |  3903 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3904 | `		/* goto Abort; */` |
|      ! 0 |  3905 | `	}else{` |
|        3 |  3906 | `		r = a%b;` |
|        - |  3907 | `	}` |
|        - |  3908 | `	/* Push the result */` |
|        3 |  3909 | `	pNos->x.iVal = r;` |
|        3 |  3910 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  3911 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3912 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3913 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3914 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3915 | `	}` |
|        3 |  3916 | `	VmPopOperand(&pTos,1);` |
|        3 |  3917 | `	break;` |
|        - |  3918 | `				}` |
|        - |  3919 | `/*` |
|        - |  3920 | ` * OP_DIV * * *` |
|        - |  3921 | ` *` |
|        - |  3922 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3923 | ` * first (what was next on the stack) from the second (the` |
|        - |  3924 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3925 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3926 | ` */` |
|       28 |  3927 | `case PH7_OP_DIV:{` |
|       58 |  3928 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3929 | `	ph7_real a,b,r;` |
|        - |  3930 | `#ifdef UNTRUST` |
|        - |  3931 | `	if( pNos < pStack ){` |
|        - |  3932 | `		goto Abort;` |
|        - |  3933 | `	}` |
|        - |  3934 | `#endif` |
|        - |  3935 | `	/* Force the operands to be real */` |
|       58 |  3936 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  3937 | `		PH7_MemObjToReal(pTos);` |
|       26 |  3938 | `	}` |
|       58 |  3939 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  3940 | `		PH7_MemObjToReal(pNos);` |
|        9 |  3941 | `	}` |
|        - |  3942 | `	/* Perform the requested operation */` |
|       58 |  3943 | `	a = pNos->rVal;` |
|       58 |  3944 | `	b = pTos->rVal;` |
|       58 |  3945 | `	if( b == 0 ){` |
|        - |  3946 | `		/* Division by zero */` |
|        3 |  3947 | `		pNos->rVal = 0;` |
|        3 |  3948 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  3949 | `		/* goto Abort; */` |
|        2 |  3950 | `	}else{` |
|       55 |  3951 | `		r = a/b;` |
|        - |  3952 | `		/* Push the result */` |
|       55 |  3953 | `		pNos->rVal = r;` |
|       55 |  3954 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3955 | `		/* Try to get an integer representation */` |
|       55 |  3956 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3957 | `	}` |
|       58 |  3958 | `	VmPopOperand(&pTos,1);` |
|       58 |  3959 | `	break;` |
|        - |  3960 | `				}` |
|        - |  3961 | `/*` |
|        - |  3962 | ` * OP_DIV_STORE * * *` |
|        - |  3963 | ` *` |
|        - |  3964 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3965 | ` * first (what was next on the stack) from the second (the` |
|        - |  3966 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3967 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3968 | ` */` |
|        1 |  3969 | `case PH7_OP_DIV_STORE:{` |
|        3 |  3970 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3971 | `	ph7_value *pObj;` |
|        - |  3972 | `	ph7_real a,b,r;` |
|        - |  3973 | `#ifdef UNTRUST` |
|        - |  3974 | `	if( pNos < pStack ){` |
|        - |  3975 | `		goto Abort;` |
|        - |  3976 | `	}` |
|        - |  3977 | `#endif` |
|        - |  3978 | `	/* Force the operands to be real */` |
|        3 |  3979 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3980 | `		PH7_MemObjToReal(pTos);` |
|        1 |  3981 | `	}` |
|        3 |  3982 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3983 | `		PH7_MemObjToReal(pNos);` |
|        1 |  3984 | `	}` |
|        - |  3985 | `	/* Perform the requested operation */` |
|        3 |  3986 | `	a = pTos->rVal;` |
|        3 |  3987 | `	b = pNos->rVal;` |
|        3 |  3988 | `	if( b == 0 ){` |
|        - |  3989 | `		/* Division by zero */` |
|      ! 0 |  3990 | `		r = 0;` |
|      ! 0 |  3991 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  3992 | `		/* goto Abort; */` |
|      ! 0 |  3993 | `	}else{` |
|        3 |  3994 | `		r = a/b;` |
|        - |  3995 | `		/* Push the result */` |
|        3 |  3996 | `		pNos->rVal = r;` |
|        3 |  3997 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3998 | `		/* Try to get an integer representation */` |
|        3 |  3999 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4000 | `	}` |
|        3 |  4001 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4002 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4003 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4004 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4005 | `	}` |
|        3 |  4006 | `	VmPopOperand(&pTos,1);` |
|        3 |  4007 | `	break;` |
|        - |  4008 | `				}` |
|        - |  4009 | `/* OP_BAND * * *` |
|        - |  4010 | ` *` |
|        - |  4011 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4012 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4013 | ` * two elements.` |
|        - |  4014 | `*/` |
|        - |  4015 | `/* OP_BOR * * *` |
|        - |  4016 | ` *` |
|        - |  4017 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4018 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4019 | ` * two elements.` |
|        - |  4020 | ` */` |
|        - |  4021 | `/* OP_BXOR * * *` |
|        - |  4022 | ` *` |
|        - |  4023 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4024 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4025 | ` * two elements.` |
|        - |  4026 | ` */` |
|       19 |  4027 | `case PH7_OP_BAND:` |
|        - |  4028 | `case PH7_OP_BOR:` |
|        - |  4029 | `case PH7_OP_BXOR:{` |
|       39 |  4030 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4031 | `	sxi64 a,b,r;` |
|        - |  4032 | `#ifdef UNTRUST` |
|        - |  4033 | `	if( pNos < pStack ){` |
|        - |  4034 | `		goto Abort;` |
|        - |  4035 | `	}` |
|        - |  4036 | `#endif` |
|        - |  4037 | `	/* Force the operands to be integer */` |
|       39 |  4038 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4039 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4040 | `	}` |
|       39 |  4041 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4042 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4043 | `	}` |
|        - |  4044 | `	/* Perform the requested operation */` |
|       39 |  4045 | `	a = pNos->x.iVal;` |
|       39 |  4046 | `	b = pTos->x.iVal;` |
|       39 |  4047 | `	switch(pInstr->iOp){` |
|        6 |  4048 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4049 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4050 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4051 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        7 |  4052 | `	case PH7_OP_BAND_STORE:` |
|        7 |  4053 | `	case PH7_OP_BAND:` |
|       15 |  4054 | `	default:          r = a&b; break;` |
|        - |  4055 | `	}` |
|        - |  4056 | `	/* Push the result */` |
|       39 |  4057 | `	pNos->x.iVal = r;` |
|       39 |  4058 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       39 |  4059 | `	VmPopOperand(&pTos,1);` |
|       39 |  4060 | `	break;` |
|        - |  4061 | `				 }` |
|        - |  4062 | `/* OP_BAND_STORE * * *` |
|        - |  4063 | ` *` |
|        - |  4064 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4065 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4066 | ` * two elements.` |
|        - |  4067 | `*/` |
|        - |  4068 | `/* OP_BOR_STORE * * *` |
|        - |  4069 | ` *` |
|        - |  4070 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4071 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4072 | ` * two elements.` |
|        - |  4073 | ` */` |
|        - |  4074 | `/* OP_BXOR_STORE * * *` |
|        - |  4075 | ` *` |
|        - |  4076 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4077 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4078 | ` * two elements.` |
|        - |  4079 | ` */` |
|        7 |  4080 | `case PH7_OP_BAND_STORE:` |
|        - |  4081 | `case PH7_OP_BOR_STORE:` |
|        - |  4082 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4083 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4084 | `	ph7_value *pObj;` |
|        - |  4085 | `	sxi64 a,b,r;` |
|        - |  4086 | `#ifdef UNTRUST` |
|        - |  4087 | `	if( pNos < pStack ){` |
|        - |  4088 | `		goto Abort;` |
|        - |  4089 | `	}` |
|        - |  4090 | `#endif` |
|        - |  4091 | `	/* Force the operands to be integer */` |
|       15 |  4092 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4093 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4094 | `	}` |
|       15 |  4095 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4096 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4097 | `	}` |
|        - |  4098 | `	/* Perform the requested operation */` |
|       15 |  4099 | `	a = pTos->x.iVal;` |
|       15 |  4100 | `	b = pNos->x.iVal;` |
|       15 |  4101 | `	switch(pInstr->iOp){` |
|        2 |  4102 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4103 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4104 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4105 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4106 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4107 | `	case PH7_OP_BAND:` |
|        5 |  4108 | `	default:          r = a&b; break;` |
|        - |  4109 | `	}` |
|        - |  4110 | `	/* Push the result */` |
|       15 |  4111 | `	pNos->x.iVal = r;` |
|       15 |  4112 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4113 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4114 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4115 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4116 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4117 | `	}` |
|       15 |  4118 | `	VmPopOperand(&pTos,1);` |
|       15 |  4119 | `	break;` |
|        - |  4120 | `				 }` |
|        - |  4121 | `/* OP_SHL * * *` |
|        - |  4122 | ` *` |
|        - |  4123 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4124 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4125 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4126 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4127 | ` */` |
|        - |  4128 | `/* OP_SHR * * *` |
|        - |  4129 | ` *` |
|        - |  4130 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4131 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4132 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4133 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4134 | ` */` |
|        9 |  4135 | `case PH7_OP_SHL:` |
|        - |  4136 | `case PH7_OP_SHR: {` |
|       19 |  4137 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4138 | `	sxi64 a,r;` |
|        - |  4139 | `	sxi32 b;` |
|        - |  4140 | `#ifdef UNTRUST` |
|        - |  4141 | `	if( pNos < pStack ){` |
|        - |  4142 | `		goto Abort;` |
|        - |  4143 | `	}` |
|        - |  4144 | `#endif` |
|        - |  4145 | `	/* Force the operands to be integer */` |
|       19 |  4146 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4147 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4148 | `	}` |
|       19 |  4149 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4150 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4151 | `	}` |
|        - |  4152 | `	/* Perform the requested operation */` |
|       19 |  4153 | `	a = pNos->x.iVal;` |
|       19 |  4154 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4155 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4156 | `		r = a << b;` |
|        6 |  4157 | `	}else{` |
|        9 |  4158 | `		r = a >> b;` |
|        - |  4159 | `	}` |
|        - |  4160 | `	/* Push the result */` |
|       19 |  4161 | `	pNos->x.iVal = r;` |
|       19 |  4162 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4163 | `	VmPopOperand(&pTos,1);` |
|       19 |  4164 | `	break;` |
|        - |  4165 | `				 }` |
|        - |  4166 | `/*  OP_SHL_STORE * * *` |
|        - |  4167 | ` *` |
|        - |  4168 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4169 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4170 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4171 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4172 | ` */` |
|        - |  4173 | `/* OP_SHR_STORE * * *` |
|        - |  4174 | ` *` |
|        - |  4175 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4176 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4177 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4178 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4179 | ` */` |
|        7 |  4180 | `case PH7_OP_SHL_STORE:` |
|        - |  4181 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4182 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4183 | `	ph7_value *pObj;` |
|        - |  4184 | `	sxi64 a,r;` |
|        - |  4185 | `	sxi32 b;` |
|        - |  4186 | `#ifdef UNTRUST` |
|        - |  4187 | `	if( pNos < pStack ){` |
|        - |  4188 | `		goto Abort;` |
|        - |  4189 | `	}` |
|        - |  4190 | `#endif` |
|        - |  4191 | `	/* Force the operands to be integer */` |
|       15 |  4192 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4193 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4194 | `	}` |
|       15 |  4195 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4196 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4197 | `	}` |
|        - |  4198 | `	/* Perform the requested operation */` |
|       15 |  4199 | `	a = pTos->x.iVal;` |
|       15 |  4200 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4201 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4202 | `		r = a << b;` |
|        4 |  4203 | `	}else{` |
|        9 |  4204 | `		r = a >> b;` |
|        - |  4205 | `	}` |
|        - |  4206 | `	/* Push the result */` |
|       15 |  4207 | `	pNos->x.iVal = r;` |
|       15 |  4208 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4209 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4210 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4211 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4212 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4213 | `	}` |
|       15 |  4214 | `	VmPopOperand(&pTos,1);` |
|       15 |  4215 | `	break;` |
|        - |  4216 | `				 }` |
|        - |  4217 | `/* CAT:  P1 * *` |
|        - |  4218 | ` *` |
|        - |  4219 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4220 | ` * back.` |
|        - |  4221 | ` */` |
|    52435 |  4222 | `case PH7_OP_CAT:{` |
|        - |  4223 | `	ph7_value *pNos,*pCur;` |
|   104872 |  4224 | `	if( pInstr->iP1 < 1 ){` |
|    78234 |  4225 | `		pNos = &pTos[-1];` |
|    39118 |  4226 | `	}else{` |
|    26640 |  4227 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4228 | `	}` |
|        - |  4229 | `#ifdef UNTRUST` |
|        - |  4230 | `	if( pNos < pStack ){` |
|        - |  4231 | `		goto Abort;` |
|        - |  4232 | `	}` |
|        - |  4233 | `#endif` |
|        - |  4234 | `	/* Force a string cast */` |
|   104872 |  4235 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      562 |  4236 | `		PH7_MemObjToString(pNos);` |
|      280 |  4237 | `	}` |
|   104872 |  4238 | `	pCur = &pNos[1];` |
|   210892 |  4239 | `	while( pCur <= pTos ){` |
|   106022 |  4240 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50166 |  4241 | `			PH7_MemObjToString(pCur);` |
|    25082 |  4242 | `		}` |
|        - |  4243 | `		/* Perform the concatenation */` |
|   106022 |  4244 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   105984 |  4245 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    52991 |  4246 | `		}` |
|   106022 |  4247 | `		SyBlobRelease(&pCur->sBlob);` |
|   106022 |  4248 | `		pCur++;` |
|        2 |  4249 | `	}` |
|   104872 |  4250 | `	pTos = pNos;` |
|   104872 |  4251 | `	break;` |
|        - |  4252 | `				}` |
|        - |  4253 | `/*  CAT_STORE: * * *` |
|        - |  4254 | ` *` |
|        - |  4255 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4256 | ` * back.` |
|        - |  4257 | ` */` |
|     1677 |  4258 | `case PH7_OP_CAT_STORE:{` |
|     3355 |  4259 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4260 | `	ph7_value *pObj;` |
|        - |  4261 | `#ifdef UNTRUST` |
|        - |  4262 | `	if( pNos < pStack ){` |
|        - |  4263 | `		goto Abort;` |
|        - |  4264 | `	}` |
|        - |  4265 | `#endif` |
|     3355 |  4266 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4267 | `		/* Force a string cast */` |
|      ! 0 |  4268 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4269 | `	}` |
|     3355 |  4270 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4271 | `		/* Force a string cast */` |
|      ! 0 |  4272 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4273 | `	}` |
|        - |  4274 | `	/* Perform the concatenation (Reverse order) */` |
|     3355 |  4275 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     3355 |  4276 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     1677 |  4277 | `	}` |
|        - |  4278 | `	/* Perform the store operation */` |
|     3355 |  4279 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4280 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     3355 |  4281 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     3355 |  4282 | `		PH7_MemObjStore(pTos,pObj);` |
|     1677 |  4283 | `	}` |
|     3355 |  4284 | `	PH7_MemObjStore(pTos,pNos);` |
|     3355 |  4285 | `	VmPopOperand(&pTos,1);` |
|     3355 |  4286 | `	break;` |
|        - |  4287 | `				}` |
|        - |  4288 | `/* OP_AND: * * *` |
|        - |  4289 | ` *` |
|        - |  4290 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4291 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4292 | ` * stack.` |
|        - |  4293 | ` */` |
|        - |  4294 | `/* OP_OR: * * *` |
|        - |  4295 | ` *` |
|        - |  4296 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4297 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4298 | ` * stack.` |
|        - |  4299 | ` */` |
|    71306 |  4300 | `case PH7_OP_LAND:` |
|        - |  4301 | `case PH7_OP_LOR: {` |
|   142658 |  4302 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4303 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4304 | `#ifdef UNTRUST` |
|        - |  4305 | `	if( pNos < pStack ){` |
|        - |  4306 | `		goto Abort;` |
|        - |  4307 | `	}` |
|        - |  4308 | `#endif` |
|        - |  4309 | `	/* Force a boolean cast */` |
|   142658 |  4310 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4311 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4312 | `	}` |
|   142658 |  4313 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4314 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4315 | `	}` |
|   142658 |  4316 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   142658 |  4317 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   142658 |  4318 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4319 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    74140 |  4320 | `		v1 = and_logic[v1*3+v2];` |
|    37093 |  4321 | `	}else{` |
|        - |  4322 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|    68520 |  4323 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4324 | `	}` |
|   142658 |  4325 | `	if( v1 == 2 ){` |
|      ! 0 |  4326 | `		v1 = 1;` |
|      ! 0 |  4327 | `	}` |
|   142658 |  4328 | `	VmPopOperand(&pTos,1);` |
|   142658 |  4329 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   142658 |  4330 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   142658 |  4331 | `	break;` |
|        - |  4332 | `				 }` |
|        - |  4333 | `/* OP_LXOR: * * *` |
|        - |  4334 | ` *` |
|        - |  4335 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4336 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4337 | ` * stack.` |
|        - |  4338 | ` * According to the PHP language reference manual:` |
|        - |  4339 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4340 | ` *  TRUE,but not both.` |
|        - |  4341 | ` */` |
|        5 |  4342 | `case PH7_OP_LXOR:{` |
|       11 |  4343 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4344 | `	sxi32 v = 0;` |
|        - |  4345 | `#ifdef UNTRUST` |
|        - |  4346 | `	if( pNos < pStack ){` |
|        - |  4347 | `		goto Abort;` |
|        - |  4348 | `	}` |
|        - |  4349 | `#endif` |
|        - |  4350 | `	/* Force a boolean cast */` |
|       11 |  4351 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4352 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4353 | `	}` |
|       11 |  4354 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4355 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4356 | `	}` |
|       11 |  4357 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4358 | `		v = 1;` |
|        3 |  4359 | `	}` |
|       11 |  4360 | `	VmPopOperand(&pTos,1);` |
|       11 |  4361 | `	pTos->x.iVal = v;` |
|       11 |  4362 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4363 | `	break;` |
|        - |  4364 | `				 }` |
|        - |  4365 | `/* OP_EQ P1 P2 P3` |
|        - |  4366 | ` *` |
|        - |  4367 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4368 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4369 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4370 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4371 | ` */` |
|        - |  4372 | `/* OP_NEQ P1 P2 P3` |
|        - |  4373 | ` *` |
|        - |  4374 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4375 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4376 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4377 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4378 | ` */` |
|     3203 |  4379 | `case PH7_OP_EQ:` |
|        - |  4380 | `case PH7_OP_NEQ: {` |
|     6408 |  4381 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4382 | `	/* Perform the comparison and act accordingly */` |
|        - |  4383 | `#ifdef UNTRUST` |
|        - |  4384 | `	if( pNos < pStack ){` |
|        - |  4385 | `		goto Abort;` |
|        - |  4386 | `	}` |
|        - |  4387 | `#endif` |
|     6408 |  4388 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     6408 |  4389 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       11 |  4390 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     6403 |  4391 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     6372 |  4392 | `		rc = rc == 0;` |
|     3187 |  4393 | `	}else{` |
|       28 |  4394 | `		rc = rc != 0;` |
|        - |  4395 | `	}` |
|     6408 |  4396 | `	VmPopOperand(&pTos,1);` |
|     6408 |  4397 | `	if( !pInstr->iP2 ){` |
|        - |  4398 | `		/* Push comparison result without taking the jump */` |
|     6408 |  4399 | `		PH7_MemObjRelease(pTos);` |
|     6408 |  4400 | `		pTos->x.iVal = rc;` |
|        - |  4401 | `		/* Invalidate any prior representation */` |
|     6408 |  4402 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3205 |  4403 | `	}else{` |
|      ! 0 |  4404 | `		if( rc ){` |
|        - |  4405 | `			/* Jump to the desired location */` |
|      ! 0 |  4406 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4407 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4408 | `		}` |
|        - |  4409 | `	}` |
|     6408 |  4410 | `	break;` |
|        - |  4411 | `				 }` |
|        - |  4412 | `/* OP_TEQ P1 P2 *` |
|        - |  4413 | ` *` |
|        - |  4414 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4415 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4416 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4417 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4418 | ` */` |
|    95132 |  4419 | `case PH7_OP_TEQ: {` |
|   190266 |  4420 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4421 | `	/* Perform the comparison and act accordingly */` |
|        - |  4422 | `#ifdef UNTRUST` |
|        - |  4423 | `	if( pNos < pStack ){` |
|        - |  4424 | `		goto Abort;` |
|        - |  4425 | `	}` |
|        - |  4426 | `#endif` |
|   190266 |  4427 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   190266 |  4428 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4429 | `		rc = 0;` |
|        2 |  4430 | `	}else{` |
|   190264 |  4431 | `		rc = rc == 0;` |
|        - |  4432 | `	}` |
|   190266 |  4433 | `	VmPopOperand(&pTos,1);` |
|   190266 |  4434 | `	if( !pInstr->iP2 ){` |
|        - |  4435 | `		/* Push comparison result without taking the jump */` |
|   190266 |  4436 | `		PH7_MemObjRelease(pTos);` |
|   190266 |  4437 | `		pTos->x.iVal = rc;` |
|        - |  4438 | `		/* Invalidate any prior representation */` |
|   190266 |  4439 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    95134 |  4440 | `	}else{` |
|      ! 0 |  4441 | `		if( rc ){` |
|        - |  4442 | `			/* Jump to the desired location */` |
|      ! 0 |  4443 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4444 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4445 | `		}` |
|        - |  4446 | `	}` |
|   190266 |  4447 | `	break;` |
|        - |  4448 | `				 }` |
|        - |  4449 | `/* OP_TNE P1 P2 *` |
|        - |  4450 | ` *` |
|        - |  4451 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4452 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4453 | ` * instruction.` |
|        - |  4454 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4455 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4456 | ` *` |
|        - |  4457 | ` */` |
|    75184 |  4458 | `case PH7_OP_TNE: {` |
|   150370 |  4459 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4460 | `	/* Perform the comparison and act accordingly */` |
|        - |  4461 | `#ifdef UNTRUST` |
|        - |  4462 | `	if( pNos < pStack ){` |
|        - |  4463 | `		goto Abort;` |
|        - |  4464 | `	}` |
|        - |  4465 | `#endif` |
|   150370 |  4466 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   150370 |  4467 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4468 | `		rc = 1;` |
|        2 |  4469 | `	}else{` |
|   150368 |  4470 | `		rc = rc != 0;` |
|        - |  4471 | `	}` |
|   150370 |  4472 | `	VmPopOperand(&pTos,1);` |
|   150370 |  4473 | `	if( !pInstr->iP2 ){` |
|        - |  4474 | `		/* Push comparison result without taking the jump */` |
|   150370 |  4475 | `		PH7_MemObjRelease(pTos);` |
|   150370 |  4476 | `		pTos->x.iVal = rc;` |
|        - |  4477 | `		/* Invalidate any prior representation */` |
|   150370 |  4478 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    75186 |  4479 | `	}else{` |
|      ! 0 |  4480 | `		if( rc ){` |
|        - |  4481 | `			/* Jump to the desired location */` |
|      ! 0 |  4482 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4483 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4484 | `		}` |
|        - |  4485 | `	}` |
|   150370 |  4486 | `	break;` |
|        - |  4487 | `				 }` |
|        - |  4488 | `/* OP_LT P1 P2 P3` |
|        - |  4489 | ` *` |
|        - |  4490 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4491 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4492 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4493 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4494 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4495 | ` *` |
|        - |  4496 | ` */` |
|        - |  4497 | `/* OP_LE P1 P2 P3` |
|        - |  4498 | ` *` |
|        - |  4499 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4500 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4501 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4502 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4503 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4504 | ` *` |
|        - |  4505 | ` */` |
|    83491 |  4506 | `case PH7_OP_LT:` |
|        - |  4507 | `case PH7_OP_LE: {` |
|   167028 |  4508 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4509 | `	/* Perform the comparison and act accordingly */` |
|        - |  4510 | `#ifdef UNTRUST` |
|        - |  4511 | `	if( pNos < pStack ){` |
|        - |  4512 | `		goto Abort;` |
|        - |  4513 | `	}` |
|        - |  4514 | `#endif` |
|   167028 |  4515 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   167028 |  4516 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4517 | `		rc = 0;` |
|   167024 |  4518 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      395 |  4519 | `		rc = rc < 1;` |
|      198 |  4520 | `	}else{` |
|   166626 |  4521 | `		rc = rc < 0;` |
|        - |  4522 | `	}` |
|   167028 |  4523 | `	VmPopOperand(&pTos,1);` |
|   167028 |  4524 | `	if( !pInstr->iP2 ){` |
|        - |  4525 | `		/* Push comparison result without taking the jump */` |
|   167028 |  4526 | `		PH7_MemObjRelease(pTos);` |
|   167028 |  4527 | `		pTos->x.iVal = rc;` |
|        - |  4528 | `		/* Invalidate any prior representation */` |
|   167028 |  4529 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    83537 |  4530 | `	}else{` |
|      ! 0 |  4531 | `		if( rc ){` |
|        - |  4532 | `			/* Jump to the desired location */` |
|      ! 0 |  4533 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4534 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4535 | `		}` |
|        - |  4536 | `	}` |
|   167028 |  4537 | `	break;` |
|        - |  4538 | `				}` |
|        - |  4539 | `/* OP_GT P1 P2 P3` |
|        - |  4540 | ` *` |
|        - |  4541 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4542 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4543 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4544 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4545 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4546 | ` *` |
|        - |  4547 | ` */` |
|        - |  4548 | `/* OP_GE P1 P2 P3` |
|        - |  4549 | ` *` |
|        - |  4550 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4551 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4552 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4553 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4554 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4555 | ` *` |
|        - |  4556 | ` */` |
|    32005 |  4557 | `case PH7_OP_GT:` |
|        - |  4558 | `case PH7_OP_GE: {` |
|    64012 |  4559 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4560 | `	/* Perform the comparison and act accordingly */` |
|        - |  4561 | `#ifdef UNTRUST` |
|        - |  4562 | `	if( pNos < pStack ){` |
|        - |  4563 | `		goto Abort;` |
|        - |  4564 | `	}` |
|        - |  4565 | `#endif` |
|    64012 |  4566 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    64012 |  4567 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4568 | `		rc = 0;` |
|    64008 |  4569 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    63856 |  4570 | `		rc = rc >= 0;` |
|    31929 |  4571 | `	}else{` |
|      150 |  4572 | `		rc = rc > 0;` |
|        - |  4573 | `	}` |
|    64012 |  4574 | `	VmPopOperand(&pTos,1);` |
|    64012 |  4575 | `	if( !pInstr->iP2 ){` |
|        - |  4576 | `		/* Push comparison result without taking the jump */` |
|    64012 |  4577 | `		PH7_MemObjRelease(pTos);` |
|    64012 |  4578 | `		pTos->x.iVal = rc;` |
|        - |  4579 | `		/* Invalidate any prior representation */` |
|    64012 |  4580 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    32007 |  4581 | `	}else{` |
|      ! 0 |  4582 | `		if( rc ){` |
|        - |  4583 | `			/* Jump to the desired location */` |
|      ! 0 |  4584 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4585 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4586 | `		}` |
|        - |  4587 | `	}` |
|    64012 |  4588 | `	break;` |
|        - |  4589 | `				}` |
|        - |  4590 | `/* OP_SEQ P1 P2 *` |
|        - |  4591 | ` * Strict string comparison.` |
|        - |  4592 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4593 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4594 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4595 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4596 | ` * use PH7_OP_EQ.` |
|        - |  4597 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4598 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4599 | ` */` |
|        - |  4600 | `/* OP_SNE P1 P2 *` |
|        - |  4601 | ` * Strict string comparison.` |
|        - |  4602 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4603 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4604 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4605 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4606 | ` * use PH7_OP_EQ.` |
|        - |  4607 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4608 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4609 | ` */` |
|       18 |  4610 | `case PH7_OP_SEQ:` |
|        - |  4611 | `case PH7_OP_SNE: {` |
|       38 |  4612 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4613 | `	SyString s1,s2;` |
|        - |  4614 | `	/* Perform the comparison and act accordingly */` |
|        - |  4615 | `#ifdef UNTRUST` |
|        - |  4616 | `	if( pNos < pStack ){` |
|        - |  4617 | `		goto Abort;` |
|        - |  4618 | `	}` |
|        - |  4619 | `#endif` |
|        - |  4620 | `	/* Force a string cast */` |
|       38 |  4621 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4622 | `		PH7_MemObjToString(pTos);` |
|        2 |  4623 | `	}` |
|       38 |  4624 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4625 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4626 | `	}` |
|       38 |  4627 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4628 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4629 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4630 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4631 | `		rc = rc != 0;` |
|      ! 0 |  4632 | `	}else{` |
|       38 |  4633 | `		rc = rc == 0;` |
|        - |  4634 | `	}` |
|       38 |  4635 | `	VmPopOperand(&pTos,1);` |
|       38 |  4636 | `	if( !pInstr->iP2 ){` |
|        - |  4637 | `		/* Push comparison result without taking the jump */` |
|       38 |  4638 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4639 | `		pTos->x.iVal = rc;` |
|        - |  4640 | `		/* Invalidate any prior representation */` |
|       38 |  4641 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4642 | `	}else{` |
|      ! 0 |  4643 | `		if( rc ){` |
|        - |  4644 | `			/* Jump to the desired location */` |
|      ! 0 |  4645 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4646 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4647 | `		}` |
|        - |  4648 | `	}` |
|       38 |  4649 | `	break;` |
|        - |  4650 | `				 }` |
|        - |  4651 | `/*` |
|        - |  4652 | ` * OP_LOAD_REF * * *` |
|        - |  4653 | ` * Push the index of a referenced object on the stack.` |
|        - |  4654 | ` */` |
|       57 |  4655 | `case PH7_OP_LOAD_REF: {` |
|        - |  4656 | `	sxu32 nIdx;` |
|        - |  4657 | `#ifdef UNTRUST` |
|        - |  4658 | `	if( pTos < pStack ){` |
|        - |  4659 | `		goto Abort;` |
|        - |  4660 | `	}` |
|        - |  4661 | `#endif` |
|        - |  4662 | `	/* Extract memory object index */` |
|      115 |  4663 | `	nIdx = pTos->nIdx;` |
|      115 |  4664 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4665 | `		/* Nullify the object */` |
|       95 |  4666 | `		PH7_MemObjRelease(pTos);` |
|        - |  4667 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4668 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4669 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4670 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4671 | `	}` |
|      115 |  4672 | `	break;` |
|        - |  4673 | `					  }` |
|        - |  4674 | `/*` |
|        - |  4675 | ` * OP_STORE_REF * * P3` |
|        - |  4676 | ` * Perform an assignment operation by reference.` |
|        - |  4677 | ` */` |
|       14 |  4678 | ` case PH7_OP_STORE_REF: {` |
|       30 |  4679 | `	 SyString sName = { 0 , 0 };` |
|        - |  4680 | `	 VmFrame *pFrameLocal;` |
|        - |  4681 | `	SyHashEntry *pEntry;` |
|        - |  4682 | `	sxu32 nIdx;` |
|        - |  4683 | `#ifdef UNTRUST` |
|        - |  4684 | `	if( pTos < pStack ){` |
|        - |  4685 | `		goto Abort;` |
|        - |  4686 | `	}` |
|        - |  4687 | `#endif` |
|       30 |  4688 | `	if( pInstr->p3 == 0 ){` |
|        - |  4689 | `		char *zName;` |
|        - |  4690 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4691 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4692 | `			/* Force a string cast */` |
|      ! 0 |  4693 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4694 | `		}` |
|      ! 0 |  4695 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4696 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4697 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4698 | `			if( zName ){` |
|      ! 0 |  4699 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4700 | `			}` |
|      ! 0 |  4701 | `		}` |
|      ! 0 |  4702 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4703 | `		pTos--;` |
|      ! 0 |  4704 | `	}else{` |
|       30 |  4705 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4706 | `	}` |
|       30 |  4707 | `	nIdx = pTos->nIdx;` |
|       30 |  4708 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4709 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4710 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4711 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4712 | `		}else{` |
|        - |  4713 | `			ph7_value *pObj;` |
|        - |  4714 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4715 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4716 | `			if( pObj == 0 ){` |
|      ! 0 |  4717 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4718 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4719 | `				goto Abort;` |
|        - |  4720 | `			}` |
|        - |  4721 | `			/* Perform the store operation */` |
|      ! 0 |  4722 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4723 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4724 | `		}` |
|       30 |  4725 | `	}else if( sName.nByte > 0){` |
|       30 |  4726 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4727 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4728 | `		}else{` |
|       30 |  4729 | `			pFrameLocal = pVm->pFrame;` |
|       50 |  4730 | `			while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4731 | `				/* Safely ignore the exception frame */` |
|       21 |  4732 | `				pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4733 | `			}` |
|        - |  4734 | `			/* Query the local frame */` |
|       30 |  4735 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       30 |  4736 | `			if( pEntry ){` |
|      ! 0 |  4737 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4738 | `			}else{` |
|       30 |  4739 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       30 |  4740 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4741 | `					/* Insert in the $GLOBALS array */` |
|       26 |  4742 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       12 |  4743 | `				}` |
|       30 |  4744 | `				if( rc == SXRET_OK ){` |
|       30 |  4745 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       14 |  4746 | `				}` |
|        - |  4747 | `			}` |
|        - |  4748 | `		}` |
|       14 |  4749 | `	}` |
|       30 |  4750 | `	break;` |
|        - |  4751 | `				 }` |
|        - |  4752 | `/*` |
|        - |  4753 | ` * OP_UPLINK P1 * *` |
|        - |  4754 | ` * Link a variable to the top active VM frame.` |
|        - |  4755 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4756 | ` */` |
|       14 |  4757 | `case PH7_OP_UPLINK: {` |
|       29 |  4758 | `	if( pVm->pFrame->pParent ){` |
|       29 |  4759 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4760 | `		SyString sName;` |
|        - |  4761 | `		/* Perform the link */` |
|       59 |  4762 | `		while( pLink <= pTos ){` |
|       31 |  4763 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4764 | `				/* Force a string cast */` |
|      ! 0 |  4765 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4766 | `			}` |
|       31 |  4767 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       31 |  4768 | `			if( sName.nByte > 0 ){` |
|       31 |  4769 | `				VmFrameLink(&(*pVm),&sName);` |
|       15 |  4770 | `			}` |
|       31 |  4771 | `			pLink++;` |
|        1 |  4772 | `		}` |
|       14 |  4773 | `	}` |
|       29 |  4774 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       29 |  4775 | `	break;` |
|        - |  4776 | `					}` |
|        - |  4777 | `/*` |
|        - |  4778 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4779 | ` * Push an exception in the corresponding container so that` |
|        - |  4780 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4781 | ` */` |
|       10 |  4782 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       22 |  4783 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4784 | `	VmFrame *pFrameLocal;` |
|       22 |  4785 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4786 | `	/* Create the exception frame */` |
|       22 |  4787 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       22 |  4788 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4789 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4790 | `		goto Abort;` |
|        - |  4791 | `	}` |
|        - |  4792 | `	/* Mark the special frame */` |
|       22 |  4793 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       22 |  4794 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4795 | `	/* Point to the frame that trigger the exception */` |
|       22 |  4796 | `	pFrameLocal = pFrameLocal->pParent;` |
|       34 |  4797 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|       13 |  4798 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4799 | `	}` |
|       22 |  4800 | `	pException->pFrame = pFrameLocal;` |
|       22 |  4801 | `	break;` |
|        - |  4802 | `							}` |
|        - |  4803 | `/*` |
|        - |  4804 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4805 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4806 | ` */` |
|        9 |  4807 | `case PH7_OP_POP_EXCEPTION: {` |
|       20 |  4808 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       20 |  4809 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4810 | `		ph7_exception **apException;` |
|        - |  4811 | `		/* Pop the loaded exception */` |
|        7 |  4812 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        7 |  4813 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|        7 |  4814 | `			(void)SySetPop(&pVm->aException);` |
|        3 |  4815 | `		}` |
|        3 |  4816 | `	}` |
|       20 |  4817 | `	pException->pFrame = 0;` |
|        - |  4818 | `	/* Leave the exception frame */` |
|       20 |  4819 | `	VmLeaveFrame(&(*pVm));` |
|       20 |  4820 | `	break;` |
|        - |  4821 | `							}` |
|        - |  4822 |  |
|        - |  4823 | `/*` |
|        - |  4824 | ` * OP_THROW * P2 *` |
|        - |  4825 | ` * Throw an user exception.` |
|        - |  4826 | ` */` |
|        8 |  4827 | `case PH7_OP_THROW: {` |
|       18 |  4828 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       18 |  4829 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  4830 | `#ifdef UNTRUST` |
|        - |  4831 | `	if( pTos < pStack ){` |
|        - |  4832 | `		goto Abort;` |
|        - |  4833 | `	}` |
|        - |  4834 | `#endif` |
|       24 |  4835 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4836 | `		/* Safely ignore the exception frame */` |
|        8 |  4837 | `		pFrameLocal = pFrameLocal->pParent;` |
|        2 |  4838 | `	}` |
|        - |  4839 | `	/* Tell the upper layer that an exception was thrown */` |
|       18 |  4840 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       18 |  4841 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       18 |  4842 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4843 | `		ph7_class *pException;` |
|        - |  4844 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  4845 | `		 */` |
|       18 |  4846 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       18 |  4847 | `		if( pException == 0 \|\| !VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  4848 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  4849 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  4850 | `			if( rc == SXERR_ABORT ){` |
|        - |  4851 | `				/* Abort processing immediately */` |
|      ! 0 |  4852 | `				goto Abort;` |
|        - |  4853 | `			}` |
|      ! 0 |  4854 | `		}else{` |
|        - |  4855 | `			/* Throw the exception */` |
|       18 |  4856 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       18 |  4857 | `			if( rc == SXERR_ABORT ){` |
|        - |  4858 | `				/* Abort processing immediately */` |
|        3 |  4859 | `				goto Abort;` |
|        - |  4860 | `			}` |
|        - |  4861 | `		}` |
|        9 |  4862 | `	}else{` |
|        - |  4863 | `		/* Expecting a class instance */` |
|      ! 0 |  4864 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  4865 | `		if( rc == SXERR_ABORT ){` |
|        - |  4866 | `			/* Abort processing immediately */` |
|      ! 0 |  4867 | `			goto Abort;` |
|        - |  4868 | `		}` |
|        - |  4869 | `	}` |
|        - |  4870 | `	/* Pop the top entry */` |
|       16 |  4871 | `	VmPopOperand(&pTos,1);` |
|        - |  4872 | `	/* Perform an unconditional jump */` |
|       16 |  4873 | `	pc = nJump - 1;` |
|       16 |  4874 | `	break;` |
|        - |  4875 | `				   }` |
|        - |  4876 | `/*` |
|        - |  4877 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  4878 | ` * Prepare a foreach step.` |
|        - |  4879 | ` */` |
|     3707 |  4880 | `case PH7_OP_FOREACH_INIT: {` |
|     7416 |  4881 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4882 | `	void *pName;` |
|        - |  4883 | `#ifdef UNTRUST` |
|        - |  4884 | `	if( pTos < pStack ){` |
|        - |  4885 | `		goto Abort;` |
|        - |  4886 | `	}` |
|        - |  4887 | `#endif` |
|     7416 |  4888 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4889 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  4890 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4891 | `			/* Force a string cast */` |
|      ! 0 |  4892 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4893 | `		}` |
|        - |  4894 | `		/* Duplicate name */` |
|      ! 0 |  4895 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4896 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4897 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4898 | `		}` |
|      ! 0 |  4899 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4900 | `	}` |
|     7416 |  4901 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  4902 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4903 | `			/* Force a string cast */` |
|      ! 0 |  4904 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4905 | `		}` |
|        - |  4906 | `		/* Duplicate name */` |
|      ! 0 |  4907 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4908 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4909 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4910 | `		}` |
|      ! 0 |  4911 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4912 | `	}` |
|        - |  4913 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     7416 |  4914 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4915 | `		/* Jump out of the loop */` |
|      ! 0 |  4916 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4917 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4918 | `		}` |
|      ! 0 |  4919 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4920 | `	}else{` |
|        - |  4921 | `		ph7_foreach_step *pStep;` |
|     7416 |  4922 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     7416 |  4923 | `		if( pStep == 0 ){` |
|      ! 0 |  4924 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4925 | `			/* Jump out of the loop */` |
|      ! 0 |  4926 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4927 | `		}else{` |
|        - |  4928 | `			/* Zero the structure */` |
|     7416 |  4929 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4930 | `			/* Prepare the step */` |
|     7416 |  4931 | `			pStep->iFlags = pInfo->iFlags;` |
|     7416 |  4932 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     7408 |  4933 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4934 | `				/* Reset the internal loop cursor */` |
|     7408 |  4935 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4936 | `				/* Mark the step */` |
|     7408 |  4937 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     7408 |  4938 | `				pStep->xIter.pMap = pMap;` |
|     7408 |  4939 | `				pMap->iRef++;` |
|     3705 |  4940 | `			}else{` |
|        9 |  4941 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4942 | `				/* Reset the loop cursor */` |
|        9 |  4943 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|        - |  4944 | `				/* Mark the step */` |
|        9 |  4945 | `				pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  4946 | `				pStep->xIter.pThis = pThis;` |
|        9 |  4947 | `				pThis->iRef++;` |
|        - |  4948 | `			}` |
|        - |  4949 | `		}` |
|     7416 |  4950 | `		if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  4951 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  4952 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  4953 | `			/* Jump out of the loop */` |
|      ! 0 |  4954 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4955 | `		}` |
|        - |  4956 | `	}` |
|     7416 |  4957 | `	VmPopOperand(&pTos,1);` |
|     7416 |  4958 | `	break;` |
|        - |  4959 | `						  }` |
|        - |  4960 | `/*` |
|        - |  4961 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  4962 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  4963 | ` */` |
|    61752 |  4964 | `case PH7_OP_FOREACH_STEP: {` |
|   123506 |  4965 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4966 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  4967 | `	ph7_value *pValue;` |
|        - |  4968 | `	VmFrame *pFrameLocal;` |
|        - |  4969 | `	/* Peek the last step */` |
|   123506 |  4970 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   123506 |  4971 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   123506 |  4972 | `	pFrameLocal = pVm->pFrame;` |
|   128538 |  4973 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4974 | `		/* Safely ignore the exception frame */` |
|     5033 |  4975 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4976 | `	}` |
|   123506 |  4977 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   123482 |  4978 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  4979 | `		ph7_hashmap_node *pNode;` |
|        - |  4980 | `		/* Extract the current node value */` |
|   123482 |  4981 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   123482 |  4982 | `		if( pNode == 0 ){` |
|        - |  4983 | `			/* No more entry to process */` |
|     7408 |  4984 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     7408 |  4985 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4986 | `				/* Break the reference with the last element */` |
|        5 |  4987 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  4988 | `			}` |
|        - |  4989 | `			/* Automatically reset the loop cursor */` |
|     7408 |  4990 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4991 | `			/* Cleanup the mess left behind */` |
|     7408 |  4992 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     7408 |  4993 | `			SySetPop(&pInfo->aStep);` |
|     7408 |  4994 | `			PH7_HashmapUnref(pMap);` |
|     3705 |  4995 | `		}else{` |
|   116076 |  4996 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      139 |  4997 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      139 |  4998 | `				if( pKey ){` |
|      139 |  4999 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|       69 |  5000 | `				}` |
|       69 |  5001 | `			}` |
|   116076 |  5002 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5003 | `				SyHashEntry *pEntry;` |
|        - |  5004 | `				/* Pass by reference */` |
|       13 |  5005 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       13 |  5006 | `				if( pEntry ){` |
|       13 |  5007 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|        7 |  5008 | `				}else{` |
|      ! 0 |  5009 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5010 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5011 | `				}` |
|        7 |  5012 | `			}else{` |
|        - |  5013 | `				/* Make a copy of the entry value */` |
|   116064 |  5014 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   116064 |  5015 | `				if( pValue ){` |
|   116064 |  5016 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    58031 |  5017 | `				}` |
|        - |  5018 | `			}` |
|        - |  5019 | `		}` |
|    61742 |  5020 | `	}else{` |
|       25 |  5021 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5022 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5023 | `		SyHashEntry *pEntry;` |
|        - |  5024 | `		/* Point to the next attribute */` |
|       29 |  5025 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5026 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5027 | `			/* Check access permission */` |
|       31 |  5028 | `			if( VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5029 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5030 | `					break; /* Access is granted */` |
|        - |  5031 | `			}` |
|        1 |  5032 | `		}` |
|       25 |  5033 | `		if( pEntry == 0 ){` |
|        - |  5034 | `			/* Clean up the mess left behind */` |
|        9 |  5035 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5036 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5037 | `				/* Break the reference with the last element */` |
|        3 |  5038 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5039 | `			}` |
|        9 |  5040 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5041 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5042 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5043 | `		}else{` |
|       17 |  5044 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5045 | `			ph7_value *pAttrValue;` |
|       17 |  5046 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5047 | `				/* Fill with the current attribute name */` |
|       17 |  5048 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5049 | `				if( pKey ){` |
|       17 |  5050 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5051 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5052 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5053 | `				}` |
|        8 |  5054 | `			}` |
|        - |  5055 | `			/* Extract attribute value */` |
|       17 |  5056 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5057 | `			if( pAttrValue ){` |
|       17 |  5058 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5059 | `					/* Pass by reference */` |
|        3 |  5060 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5061 | `					if( pEntry ){` |
|        3 |  5062 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5063 | `					}else{` |
|      ! 0 |  5064 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5065 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5066 | `					}` |
|        2 |  5067 | `				}else{` |
|        - |  5068 | `					/* Make a copy of the attribute value */` |
|       15 |  5069 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5070 | `					if( pValue ){` |
|       15 |  5071 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5072 | `					}` |
|        - |  5073 | `				}` |
|        8 |  5074 | `			}` |
|        - |  5075 | `		}` |
|        - |  5076 | `	}` |
|   123506 |  5077 | `	break;` |
|        - |  5078 | `						  }` |
|        - |  5079 | `/*` |
|        - |  5080 | ` * OP_MEMBER P1 P2` |
|        - |  5081 | ` * Load class attribute/method on the stack.` |
|        - |  5082 | ` */` |
|      852 |  5083 | `case PH7_OP_MEMBER: {` |
|        - |  5084 | `	ph7_class_instance *pThis;` |
|        - |  5085 | `	ph7_value *pNos;` |
|        - |  5086 | `	SyString sName;` |
|     1706 |  5087 | `	if( !pInstr->iP1 ){` |
|     1648 |  5088 | `		pNos = &pTos[-1];` |
|        - |  5089 | `#ifdef UNTRUST` |
|        - |  5090 | `		if( pNos < pStack ){` |
|        - |  5091 | `			goto Abort;` |
|        - |  5092 | `		}` |
|        - |  5093 | `#endif` |
|     1648 |  5094 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5095 | `			ph7_class *pClass;` |
|        - |  5096 | `			/* Class already instantiated */` |
|     1648 |  5097 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5098 | `			/* Point to the instantiated class */` |
|     1648 |  5099 | `			pClass = pThis->pClass;` |
|        - |  5100 | `			/* Extract attribute name first */` |
|     1648 |  5101 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     1648 |  5102 | `			if( pInstr->iP2 ){` |
|        - |  5103 | `				/* Method call */` |
|      120 |  5104 | `				ph7_class_method *pMeth = 0;` |
|      120 |  5105 | `				if( sName.nByte > 0 ){` |
|        - |  5106 | `					/* Extract the target method */` |
|      120 |  5107 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       59 |  5108 | `				}` |
|      120 |  5109 | `				if( pMeth == 0 ){` |
|      ! 0 |  5110 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5111 | `						&pClass->sName,&sName` |
|        - |  5112 | `						);` |
|        - |  5113 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5114 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5115 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5116 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5117 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5118 | `				}else{` |
|        - |  5119 | `					/* Push method name on the stack */` |
|      120 |  5120 | `					PH7_MemObjRelease(pTos);` |
|      120 |  5121 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      120 |  5122 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5123 | `				}` |
|      120 |  5124 | `				pTos->nIdx = SXU32_HIGH;` |
|       61 |  5125 | `			}else{` |
|        - |  5126 | `				/* Attribute access */` |
|     1530 |  5127 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5128 | `				SyHashEntry *pEntry;` |
|        - |  5129 | `				/* Extract the target attribute */` |
|     1530 |  5130 | `				if( sName.nByte > 0 ){` |
|     1530 |  5131 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     1530 |  5132 | `					if( pEntry ){` |
|        - |  5133 | `						/* Point to the attribute value */` |
|     1528 |  5134 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|      763 |  5135 | `					}` |
|      764 |  5136 | `				}` |
|     1530 |  5137 | `				if( pObjAttr == 0 ){` |
|        - |  5138 | `					/* No such attribute,load null */` |
|        4 |  5139 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5140 | `						&pClass->sName,&sName);` |
|        - |  5141 | `					/* Call the __get magic method if available */` |
|        3 |  5142 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5143 | `				}` |
|     1530 |  5144 | `				VmPopOperand(&pTos,1);` |
|        - |  5145 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5146 | `				 * This is due to the following case:` |
|        - |  5147 | `				 *     (new TestClass())->foo;` |
|        - |  5148 | `				 */` |
|     1530 |  5149 | `				pThis->iRef++;` |
|     1530 |  5150 | `				PH7_MemObjRelease(pTos);` |
|     1530 |  5151 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     1530 |  5152 | `				if( pObjAttr ){` |
|     1528 |  5153 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5154 | `					/* Check attribute access */` |
|     1528 |  5155 | `					if( VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5156 | `						/* Load attribute */` |
|     1528 |  5157 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     1528 |  5158 | `						if( pValue ){` |
|     1528 |  5159 | `							if( pThis->iRef < 2 ){` |
|        - |  5160 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5161 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5162 | `								 */` |
|        3 |  5163 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5164 | `							}else{` |
|        - |  5165 | `								/* Simple load */` |
|     1526 |  5166 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5167 | `							}` |
|     1528 |  5168 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     1526 |  5169 | `								if( pThis->iRef > 1 ){` |
|        - |  5170 | `									/* Load attribute index */` |
|     1524 |  5171 | `									pTos->nIdx = pObjAttr->nIdx;` |
|      761 |  5172 | `								}` |
|      762 |  5173 | `							}` |
|      763 |  5174 | `						}` |
|      763 |  5175 | `					}` |
|      763 |  5176 | `				}` |
|        - |  5177 | `				/* Safely unreference the object */` |
|     1530 |  5178 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5179 | `			}` |
|      825 |  5180 | `		}else{` |
|      ! 0 |  5181 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5182 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5183 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5184 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5185 | `		}` |
|      825 |  5186 | `	}else{` |
|        - |  5187 | `		/* Static member access using class name */` |
|       59 |  5188 | `		pNos = pTos;` |
|       59 |  5189 | `		pThis = 0;` |
|       59 |  5190 | `		if( !pInstr->p3 ){` |
|       57 |  5191 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       57 |  5192 | `			pNos--;` |
|        - |  5193 | `#ifdef UNTRUST` |
|        - |  5194 | `			if( pNos < pStack ){` |
|        - |  5195 | `				goto Abort;` |
|        - |  5196 | `			}` |
|        - |  5197 | `#endif` |
|       29 |  5198 | `		}else{` |
|        - |  5199 | `			/* Attribute name already computed */` |
|        3 |  5200 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5201 | `		}` |
|       59 |  5202 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|       59 |  5203 | `			ph7_class *pClass = 0;` |
|       59 |  5204 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5205 | `				/* Class already instantiated */` |
|      ! 0 |  5206 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5207 | `				pClass = pThis->pClass;` |
|      ! 0 |  5208 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5209 | `			}else{` |
|        - |  5210 | `				/* Try to extract the target class */` |
|       59 |  5211 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       88 |  5212 | `					pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pNos->sBlob),` |
|       29 |  5213 | `						SyBlobLength(&pNos->sBlob),FALSE,0);` |
|       29 |  5214 | `				}` |
|        - |  5215 | `			}` |
|       59 |  5216 | `			if( pClass == 0 ){` |
|        - |  5217 | `				/* Undefined class */` |
|      ! 0 |  5218 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5219 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5220 | `					);` |
|      ! 0 |  5221 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5222 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5223 | `				}` |
|      ! 0 |  5224 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5225 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5226 | `			}else{` |
|       59 |  5227 | `				if( pInstr->iP2 ){` |
|        - |  5228 | `					/* Method call */` |
|       25 |  5229 | `					ph7_class_method *pMeth = 0;` |
|       25 |  5230 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5231 | `						/* Extract the target method */` |
|       25 |  5232 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       12 |  5233 | `					}` |
|       25 |  5234 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5235 | `						if( pMeth ){` |
|      ! 0 |  5236 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5237 | `								&pClass->sName,&sName` |
|        - |  5238 | `								);` |
|      ! 0 |  5239 | `						}else{` |
|      ! 0 |  5240 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5241 | `								&pClass->sName,&sName` |
|        - |  5242 | `								);` |
|        - |  5243 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5244 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5245 | `						}` |
|        - |  5246 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5247 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5248 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5249 | `						}` |
|      ! 0 |  5250 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5251 | `					}else{` |
|        - |  5252 | `						/* Push method name on the stack */` |
|       25 |  5253 | `						PH7_MemObjRelease(pTos);` |
|       25 |  5254 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       25 |  5255 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5256 | `					}` |
|       25 |  5257 | `					pTos->nIdx = SXU32_HIGH;` |
|       13 |  5258 | `				}else{` |
|        - |  5259 | `					/* Attribute access */` |
|       35 |  5260 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5261 | `					/* Check for special ::class pseudo-constant */` |
|       49 |  5262 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       28 |  5263 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5264 | `						/* ::class returns the fully qualified class name */` |
|        - |  5265 | `						/* Pop the attribute name from the stack */` |
|       27 |  5266 | `						if( !pInstr->p3 ){` |
|       27 |  5267 | `							VmPopOperand(&pTos,1);` |
|       13 |  5268 | `						}` |
|       27 |  5269 | `						PH7_MemObjRelease(pTos);` |
|        - |  5270 | `						/* Load the class name */` |
|       27 |  5271 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       27 |  5272 | `						pTos->nIdx = SXU32_HIGH;` |
|       14 |  5273 | `					}else{` |
|        - |  5274 | `						/* Extract the target attribute */` |
|        9 |  5275 | `						if( sName.nByte > 0 ){` |
|        9 |  5276 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        4 |  5277 | `						}` |
|        9 |  5278 | `						if( pAttr == 0 ){` |
|        - |  5279 | `							/* No such attribute,load null */` |
|      ! 0 |  5280 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5281 | `								&pClass->sName,&sName);` |
|        - |  5282 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5283 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5284 | `						}` |
|        - |  5285 | `						/* Pop the attribute name from the stack */` |
|        9 |  5286 | `						if( !pInstr->p3 ){` |
|        7 |  5287 | `							VmPopOperand(&pTos,1);` |
|        3 |  5288 | `						}` |
|        9 |  5289 | `						PH7_MemObjRelease(pTos);` |
|        9 |  5290 | `						pTos->nIdx = SXU32_HIGH;` |
|        9 |  5291 | `						if( pAttr ){` |
|        9 |  5292 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5293 | `								/* Access to a non static attribute */` |
|      ! 0 |  5294 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5295 | `									&pClass->sName,&pAttr->sName` |
|        - |  5296 | `									);` |
|      ! 0 |  5297 | `							}else{` |
|        - |  5298 | `								ph7_value *pValue;` |
|        - |  5299 | `								/* Check if the access to the attribute is allowed */` |
|        9 |  5300 | `								if( VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5301 | `									/* Load the desired attribute */` |
|        9 |  5302 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|        9 |  5303 | `									if( pValue ){` |
|        9 |  5304 | `										PH7_MemObjLoad(pValue,pTos);` |
|        9 |  5305 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5306 | `											/* Load index number */` |
|        3 |  5307 | `											pTos->nIdx = pAttr->nIdx;` |
|        1 |  5308 | `										}` |
|        4 |  5309 | `									}` |
|        4 |  5310 | `								}` |
|        - |  5311 | `							}` |
|        4 |  5312 | `						}` |
|        - |  5313 | `					}` |
|        - |  5314 | `				}` |
|       59 |  5315 | `				if( pThis ){` |
|        - |  5316 | `					/* Safely unreference the object */` |
|      ! 0 |  5317 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5318 | `				}` |
|        - |  5319 | `			}` |
|       30 |  5320 | `		}else{` |
|        - |  5321 | `			/* Pop operands */` |
|      ! 0 |  5322 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5323 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5324 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5325 | `			}` |
|      ! 0 |  5326 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5327 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5328 | `		}` |
|        - |  5329 | `	}` |
|     1706 |  5330 | `	break;` |
|        - |  5331 | `					}` |
|        - |  5332 | `/*` |
|        - |  5333 | ` * OP_NEW P1 * * *` |
|        - |  5334 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5335 | ` */` |
|      248 |  5336 | `case PH7_OP_NEW: {` |
|      498 |  5337 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      498 |  5338 | `	ph7_class *pClass = 0;` |
|        - |  5339 | `	ph7_class_instance *pNew;` |
|      498 |  5340 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5341 | `		/* Try to extract the desired class */` |
|      746 |  5342 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      496 |  5343 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      248 |  5344 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5345 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5346 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5347 | `	}` |
|      498 |  5348 | `	if( pClass == 0 ){` |
|        - |  5349 | `		/* No such class */` |
|      ! 0 |  5350 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined,PH7 is loading NULL",` |
|      ! 0 |  5351 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5352 | `			);` |
|      ! 0 |  5353 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5354 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5355 | `			/* Pop given arguments */` |
|      ! 0 |  5356 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5357 | `		}` |
|      ! 0 |  5358 | `	}else{` |
|        - |  5359 | `		ph7_class_method *pCons;` |
|        - |  5360 | `		/* Create a new class instance */` |
|      498 |  5361 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      498 |  5362 | `		if( pNew == 0 ){` |
|      ! 0 |  5363 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5364 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5365 | `				&pClass->sName` |
|        - |  5366 | `			);` |
|      ! 0 |  5367 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5368 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5369 | `				/* Pop given arguments */` |
|      ! 0 |  5370 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5371 | `			}` |
|      ! 0 |  5372 | `			break;` |
|        - |  5373 | `		}` |
|        - |  5374 | `		/* Check if a constructor is available */` |
|      498 |  5375 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      498 |  5376 | `		if( pCons == 0 ){` |
|      446 |  5377 | `			SyString *pName = &pClass->sName;` |
|        - |  5378 | `			/* Check for a constructor with the same base class name */` |
|      446 |  5379 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      222 |  5380 | `		}` |
|      498 |  5381 | `		if( pCons ){` |
|        - |  5382 | `			/* Call the class constructor */` |
|       54 |  5383 | `			SySetReset(&aArg);` |
|       96 |  5384 | `			while( pArg < pTos ){` |
|       44 |  5385 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       44 |  5386 | `				pArg++;` |
|        2 |  5387 | `			}` |
|       54 |  5388 | `			if( pVm->bErrReport ){` |
|        - |  5389 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5390 | `				sxu32 n;` |
|       11 |  5391 | `				n = SySetUsed(&aArg);` |
|        - |  5392 | `				/* Emit a notice for missing arguments */` |
|       27 |  5393 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       17 |  5394 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       17 |  5395 | `					if( pFuncArg ){` |
|       17 |  5396 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5397 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5398 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5399 | `						}` |
|        8 |  5400 | `					}` |
|       17 |  5401 | `					n++;` |
|        1 |  5402 | `				}` |
|        5 |  5403 | `			}` |
|       54 |  5404 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5405 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       54 |  5406 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5407 | `				pNew->iRef = 1;` |
|      ! 0 |  5408 | `			}` |
|       26 |  5409 | `		}` |
|      498 |  5410 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5411 | `			/* Pop given arguments */` |
|       38 |  5412 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       18 |  5413 | `		}` |
|      498 |  5414 | `		PH7_MemObjRelease(pTos);` |
|      498 |  5415 | `		pTos->x.pOther = pNew;` |
|      498 |  5416 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5417 | `	}` |
|      498 |  5418 | `	break;` |
|        - |  5419 | `				 }` |
|        - |  5420 | `/*` |
|        - |  5421 | ` * OP_CLONE * * *` |
|        - |  5422 | ` * Perfome a clone operation.` |
|        - |  5423 | ` */` |
|       23 |  5424 | `case PH7_OP_CLONE: {` |
|        - |  5425 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5426 | `#ifdef UNTRUST` |
|        - |  5427 | `	if( pTos < pStack ){` |
|        - |  5428 | `		goto Abort;` |
|        - |  5429 | `	}` |
|        - |  5430 | `#endif` |
|        - |  5431 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5432 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5433 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5434 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5435 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5436 | `		break;` |
|        - |  5437 | `	}` |
|        - |  5438 | `	/* Point to the source */` |
|       44 |  5439 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5440 | `	/* Perform the clone operation */` |
|       44 |  5441 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5442 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5443 | `	if( pClone == 0 ){` |
|      ! 0 |  5444 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5445 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5446 | `	}else{` |
|        - |  5447 | `		/* Load the cloned object */` |
|       44 |  5448 | `		pTos->x.pOther = pClone;` |
|       44 |  5449 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5450 | `	}` |
|       44 |  5451 | `	break;` |
|        - |  5452 | `				   }` |
|        - |  5453 | `/*` |
|        - |  5454 | ` * OP_SWITCH * * P3` |
|        - |  5455 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5456 | ` */` |
|       18 |  5457 | `case PH7_OP_SWITCH: {` |
|       38 |  5458 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5459 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5460 | `	ph7_value sValue,sCaseValue;` |
|        - |  5461 | `	sxu32 n,nEntry;` |
|        - |  5462 | `#ifdef UNTRUST` |
|        - |  5463 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5464 | `		goto Abort;` |
|        - |  5465 | `	}` |
|        - |  5466 | `#endif` |
|        - |  5467 | `	/* Point to the case table  */` |
|       38 |  5468 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5469 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5470 | `	/* Select the appropriate case block to execute */` |
|       38 |  5471 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5472 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5473 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5474 | `		pCase = &aCase[n];` |
|       92 |  5475 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5476 | `		/* Execute the case expression first */` |
|       92 |  5477 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5478 | `		/* Compare the two expression */` |
|       92 |  5479 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5480 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5481 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5482 | `		if( rc == 0 ){` |
|        - |  5483 | `			/* Value match,jump to this block */` |
|       38 |  5484 | `			pc = pCase->nStart - 1;` |
|       38 |  5485 | `			break;` |
|        - |  5486 | `		}` |
|       29 |  5487 | `	}` |
|       38 |  5488 | `	VmPopOperand(&pTos,1);` |
|       38 |  5489 | `	if( n >= nEntry ){` |
|        - |  5490 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5491 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5492 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5493 | `		}else{` |
|        - |  5494 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5495 | `			pc = pSwitch->nOut - 1;` |
|        - |  5496 | `		}` |
|      ! 0 |  5497 | `	}` |
|       38 |  5498 | `	break;` |
|        - |  5499 | `					}` |
|        - |  5500 | `/*` |
|        - |  5501 | ` * OP_CALL P1 * *` |
|        - |  5502 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5503 | ` *  function on the stack.` |
|        - |  5504 | ` */` |
|   226685 |  5505 | `case PH7_OP_CALL: {` |
|   453416 |  5506 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5507 | `	SyHashEntry *pEntry;` |
|        - |  5508 | `	SyString sName;` |
|        - |  5509 | `	/* Extract function name */` |
|   453416 |  5510 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5511 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5512 | `			ph7_value sResult;` |
|      ! 0 |  5513 | `			SySetReset(&aArg);` |
|      ! 0 |  5514 | `			while( pArg < pTos ){` |
|      ! 0 |  5515 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5516 | `				pArg++;` |
|      ! 0 |  5517 | `			}` |
|      ! 0 |  5518 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5519 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5520 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5521 | `			SySetReset(&aArg);` |
|        - |  5522 | `			/* Pop given arguments */` |
|      ! 0 |  5523 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5524 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5525 | `			}` |
|        - |  5526 | `			/* Copy result */` |
|      ! 0 |  5527 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5528 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5529 | `		}else{` |
|        3 |  5530 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5531 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5532 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5533 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5534 | `			}else{` |
|        - |  5535 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5536 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5537 | `			}` |
|        - |  5538 | `			/* Pop given arguments */` |
|        3 |  5539 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5540 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5541 | `			}` |
|        - |  5542 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5543 | `			PH7_MemObjRelease(pTos);` |
|        - |  5544 | `		}` |
|   226612 |  5545 | `		break;` |
|        - |  5546 | `	}` |
|   453414 |  5547 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5548 | `	/* Check for a compiled function first */` |
|   453414 |  5549 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|   453414 |  5550 | `	if( pEntry ){` |
|        - |  5551 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5552 | `		ph7_class_instance *pThis;` |
|        - |  5553 | `		ph7_value *pFrameStack;` |
|        - |  5554 | `		ph7_vm_func *pVmFunc;` |
|        - |  5555 | `		ph7_class *pSelf;` |
|        - |  5556 | `		VmFrame *pFrame;` |
|        - |  5557 | `		ph7_value *pObj;` |
|        - |  5558 | `		VmSlot sArg;` |
|        - |  5559 | `		sxu32 n;` |
|        - |  5560 | `		/* initialize fields */` |
|     8740 |  5561 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     8740 |  5562 | `		pThis = 0;` |
|     8740 |  5563 | `		pSelf = 0;` |
|     8740 |  5564 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5565 | `			ph7_class_method *pMeth;` |
|        - |  5566 | `			/* Class method call */` |
|      638 |  5567 | `			ph7_value *pTarget = &pTos[-1];` |
|      638 |  5568 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5569 | `				/* Extract the 'this' pointer */` |
|      638 |  5570 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5571 | `					/* Instance already loaded */` |
|      608 |  5572 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|      608 |  5573 | `					pThis->iRef++;` |
|      608 |  5574 | `					pSelf = pThis->pClass;` |
|      303 |  5575 | `				}` |
|      638 |  5576 | `				if( pSelf == 0 ){` |
|       31 |  5577 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5578 | `						/* "Late Static Binding" class name */` |
|       37 |  5579 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       12 |  5580 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       12 |  5581 | `					}` |
|       31 |  5582 | `					if( pSelf == 0 ){` |
|        7 |  5583 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        3 |  5584 | `					}` |
|       15 |  5585 | `				}` |
|      638 |  5586 | `				if( pThis == 0  ){` |
|       31 |  5587 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       33 |  5588 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5589 | `						/* Safely ignore the exception frame */` |
|        3 |  5590 | `						pFrameLocal = pFrameLocal->pParent;` |
|        1 |  5591 | `					}` |
|       31 |  5592 | `					if( pFrameLocal->pParent ){` |
|        - |  5593 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5594 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5595 | `						if( pThis ){` |
|       13 |  5596 | `							pThis->iRef++;` |
|        6 |  5597 | `						}` |
|        9 |  5598 | `					}` |
|       15 |  5599 | `				}` |
|      638 |  5600 | `				VmPopOperand(&pTos,1);` |
|      638 |  5601 | `				PH7_MemObjRelease(pTos);` |
|        - |  5602 | `				/* Synchronize pointers */` |
|      638 |  5603 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5604 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5605 | `				 * user have already computed the random generated unique class method name` |
|        - |  5606 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5607 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5608 | `				 */` |
|      638 |  5609 | `				while( pArg < pStack ){` |
|      ! 0 |  5610 | `					pArg++;` |
|      ! 0 |  5611 | `				}` |
|      638 |  5612 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5613 | `					/* Check if the call is allowed */` |
|      638 |  5614 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|      638 |  5615 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        5 |  5616 | `						if( !VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5617 | `							/* Pop given arguments */` |
|      ! 0 |  5618 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5619 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5620 | `							}` |
|        - |  5621 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5622 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5623 | `							break;` |
|        - |  5624 | `						}` |
|        2 |  5625 | `					}` |
|      318 |  5626 | `				}` |
|      318 |  5627 | `			}` |
|      318 |  5628 | `		}` |
|        - |  5629 | `		/* Check The recursion limit */` |
|     8740 |  5630 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5631 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5632 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5633 | `				&pVmFunc->sName);` |
|        - |  5634 | `			/* Pop given arguments */` |
|        3 |  5635 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5636 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5637 | `			}` |
|        - |  5638 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5639 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5640 | `			break;` |
|        - |  5641 | `		}` |
|     8738 |  5642 | `		if( pVmFunc->pNextName ){` |
|        - |  5643 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      123 |  5644 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       61 |  5645 | `		}` |
|        - |  5646 | `		/* Extract the formal argument set */` |
|     8738 |  5647 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5648 | `		/* Create a new VM frame  */` |
|     8738 |  5649 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|     8738 |  5650 | `		if( rc != SXRET_OK ){` |
|        - |  5651 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5652 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5653 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5654 | `				&pVmFunc->sName);` |
|        - |  5655 | `			/* Pop given arguments */` |
|      ! 0 |  5656 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5657 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5658 | `			}` |
|        - |  5659 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5660 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5661 | `			break;` |
|        - |  5662 | `		}` |
|     8738 |  5663 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5664 | `			/* Install the '$this' variable */` |
|        - |  5665 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|      618 |  5666 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|      618 |  5667 | `			if( pObj ){` |
|        - |  5668 | `				/* Reflect the change */` |
|      618 |  5669 | `				pObj->x.pOther = pThis;` |
|      618 |  5670 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      308 |  5671 | `			}` |
|      308 |  5672 | `		}` |
|     8738 |  5673 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5674 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5675 | `			/* Install static variables */` |
|      ! 0 |  5676 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5677 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5678 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5679 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5680 | `					/* Initialize the static variables */` |
|      ! 0 |  5681 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5682 | `					if( pObj ){` |
|        - |  5683 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5684 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5685 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5686 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5687 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5688 | `						}` |
|      ! 0 |  5689 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5690 | `					}else{` |
|      ! 0 |  5691 | `						continue;` |
|        - |  5692 | `					}` |
|      ! 0 |  5693 | `				}` |
|        - |  5694 | `				/* Install in the current frame */` |
|      ! 0 |  5695 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5696 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5697 | `			}` |
|      ! 0 |  5698 | `		}` |
|        - |  5699 | `		/* Push arguments in the local frame */` |
|     8738 |  5700 | `		n = 0;` |
|    24758 |  5701 | `		while( pArg < pTos ){` |
|    16022 |  5702 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    15922 |  5703 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5704 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  5705 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  5706 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5707 | `						goto Abort;` |
|        - |  5708 | `					}` |
|      ! 0 |  5709 | `				}` |
|        - |  5710 | `				/* Make sure the given arguments are of the correct type */` |
|    15922 |  5711 | `				if( aFormalArg[n].nType > 0 ){` |
|     1012 |  5712 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5713 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5714 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5715 | `						ph7_class *pClass;` |
|        - |  5716 | `						/* Try to extract the desired class */` |
|      ! 0 |  5717 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5718 | `						if( pClass ){` |
|      ! 0 |  5719 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5720 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5721 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5722 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5723 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5724 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5725 | `								}` |
|      ! 0 |  5726 | `							}else{` |
|        - |  5727 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  5728 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  5729 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  5730 | `								if( ! VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  5731 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5732 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5733 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5734 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5735 | `								}` |
|        - |  5736 | `							}` |
|      ! 0 |  5737 | `						}` |
|     1012 |  5738 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5739 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5740 | `						/* Cast to the desired type */` |
|      ! 0 |  5741 | `						xCast(pArg);` |
|      ! 0 |  5742 | `					}` |
|      505 |  5743 | `				}` |
|    15922 |  5744 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5745 | `					/* Pass by reference */` |
|       25 |  5746 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  5747 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  5748 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  5749 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5750 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  5751 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  5752 | `						}` |
|        - |  5753 | `						/* Switch to pass by value */` |
|      ! 0 |  5754 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  5755 | `					}else{` |
|        - |  5756 | `						SyHashEntry *pRefEntry;` |
|        - |  5757 | `						/* Install the referenced variable in the private function frame */` |
|       25 |  5758 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       25 |  5759 | `						if( pRefEntry == 0 ){` |
|       37 |  5760 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       24 |  5761 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       25 |  5762 | `							sArg.nIdx = pArg->nIdx;` |
|       25 |  5763 | `							sArg.pUserData = 0;` |
|       25 |  5764 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       12 |  5765 | `						}` |
|       25 |  5766 | `						pObj = 0;` |
|        - |  5767 | `					}` |
|       13 |  5768 | `				}else{` |
|        - |  5769 | `					/* Pass by value,make a copy of the given argument */` |
|    15898 |  5770 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5771 | `				}` |
|     7962 |  5772 | `			}else{` |
|        - |  5773 | `				char zName[32];` |
|        - |  5774 | `				SyString sArgName;` |
|        - |  5775 | `				/* Set a dummy name */` |
|      101 |  5776 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      101 |  5777 | `				sArgName.zString = zName;` |
|        - |  5778 | `				/* Annonymous argument */` |
|      101 |  5779 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  5780 | `			}` |
|    16022 |  5781 | `			if( pObj ){` |
|    15998 |  5782 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  5783 | `				/* Insert argument index  */` |
|    15998 |  5784 | `				sArg.nIdx = pObj->nIdx;` |
|    15998 |  5785 | `				sArg.pUserData = 0;` |
|    15998 |  5786 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|     7998 |  5787 | `			}` |
|    16022 |  5788 | `			PH7_MemObjRelease(pArg);` |
|    16022 |  5789 | `			pArg++;` |
|    16022 |  5790 | `			++n;` |
|        2 |  5791 | `		}` |
|        - |  5792 | `		/* Set up closure environment */` |
|     8738 |  5793 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5794 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  5795 | `			ph7_value *pValue;` |
|        - |  5796 | `			sxu32 iEnv;` |
|        9 |  5797 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       25 |  5798 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       17 |  5799 | `				pEnv = &aEnv[iEnv];` |
|       17 |  5800 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  5801 | `					/* Do not install null value */` |
|        9 |  5802 | `					continue;` |
|        - |  5803 | `				}` |
|        9 |  5804 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|        9 |  5805 | `				if( pValue == 0 ){` |
|      ! 0 |  5806 | `					continue;` |
|        - |  5807 | `				}` |
|        - |  5808 | `				/* Invalidate any prior representation */` |
|        9 |  5809 | `				PH7_MemObjRelease(pValue);` |
|        - |  5810 | `				/* Duplicate bound variable value */` |
|        9 |  5811 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        5 |  5812 | `			}` |
|        4 |  5813 | `		}` |
|        - |  5814 | `		/* Process default values */` |
|     9848 |  5815 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1112 |  5816 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1102 |  5817 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1102 |  5818 | `				if( pObj ){` |
|        - |  5819 | `					/* Evaluate the default value and extract it's result */` |
|     1102 |  5820 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1102 |  5821 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5822 | `						goto Abort;` |
|        - |  5823 | `					}` |
|        - |  5824 | `					/* Insert argument index */` |
|     1102 |  5825 | `					sArg.nIdx = pObj->nIdx;` |
|     1102 |  5826 | `					sArg.pUserData = 0;` |
|     1102 |  5827 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  5828 | `					/* Make sure the default argument is of the correct type */` |
|     1102 |  5829 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5830 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5831 | `						/* Cast to the desired type */` |
|      ! 0 |  5832 | `						xCast(pObj);` |
|      ! 0 |  5833 | `					}` |
|      550 |  5834 | `				}` |
|      550 |  5835 | `			}` |
|     1112 |  5836 | `			++n;` |
|        2 |  5837 | `		}` |
|        - |  5838 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  5839 | `		 * does not return anything.` |
|        - |  5840 | `		 */` |
|     8738 |  5841 | `		PH7_MemObjRelease(pTos);` |
|     8738 |  5842 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  5843 | `		/* Allocate a new operand stack and evaluate the function body */` |
|     8738 |  5844 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|     8738 |  5845 | `		if( pFrameStack == 0 ){` |
|        - |  5846 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5847 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5848 | `				&pVmFunc->sName);` |
|      ! 0 |  5849 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5850 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5851 | `			}` |
|      ! 0 |  5852 | `			break;` |
|        - |  5853 | `		}` |
|     8738 |  5854 | `		if( pSelf ){` |
|        - |  5855 | `			/* Push class name */` |
|      636 |  5856 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      317 |  5857 | `		}` |
|        - |  5858 | `		/* Increment nesting level */` |
|     8738 |  5859 | `		pVm->nRecursionDepth++;` |
|        - |  5860 | `		/* Execute function body */` |
|     8738 |  5861 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  5862 | `		/* Decrement nesting level */` |
|     8738 |  5863 | `		pVm->nRecursionDepth--;` |
|     8738 |  5864 | `		if( pSelf ){` |
|        - |  5865 | `			/* Pop class name */` |
|      636 |  5866 | `			(void)SySetPop(&pVm->aSelf);` |
|      317 |  5867 | `		}` |
|        - |  5868 | `		/* Cleanup the mess left behind */` |
|     8738 |  5869 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  5870 | `			/* Return by reference,reflect that */` |
|        9 |  5871 | `			if( n != SXU32_HIGH ){` |
|        9 |  5872 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  5873 | `				sxu32 i;` |
|        - |  5874 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  5875 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  5876 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  5877 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  5878 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5879 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5880 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  5881 | `								&pVmFunc->sName);` |
|      ! 0 |  5882 | `						}` |
|      ! 0 |  5883 | `						n = SXU32_HIGH;` |
|      ! 0 |  5884 | `						break;` |
|        - |  5885 | `					}` |
|        3 |  5886 | `				}` |
|        5 |  5887 | `			}else{` |
|      ! 0 |  5888 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5889 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5890 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  5891 | `						&pVmFunc->sName);` |
|      ! 0 |  5892 | `				}` |
|        - |  5893 | `			}` |
|        9 |  5894 | `			pTos->nIdx = n;` |
|        4 |  5895 | `		}` |
|        - |  5896 | `		/* Cleanup the mess left behind */` |
|     8738 |  5897 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  5898 | `			/* An exception was throw in this frame */` |
|        7 |  5899 | `			pFrame = pFrame->pParent;` |
|        7 |  5900 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  5901 | `				/* Pop the resutlt */` |
|        5 |  5902 | `				VmPopOperand(&pTos,1);` |
|        - |  5903 | `				/* Jump to this destination */` |
|        5 |  5904 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  5905 | `				rc = PH7_OK;` |
|        3 |  5906 | `			}else{` |
|        3 |  5907 | `				if( pFrame->pParent ){` |
|        3 |  5908 | `					rc = PH7_EXCEPTION;` |
|        2 |  5909 | `				}else{` |
|        - |  5910 | `					/* Continue normal execution */` |
|      ! 0 |  5911 | `					rc = PH7_OK;` |
|        - |  5912 | `				}` |
|        - |  5913 | `			}` |
|        3 |  5914 | `		}` |
|        - |  5915 | `		/* Free the operand stack */` |
|     8738 |  5916 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  5917 | `		/* Leave the frame */` |
|     8738 |  5918 | `		VmLeaveFrame(&(*pVm));` |
|     8738 |  5919 | `		if( rc == PH7_ABORT ){` |
|        - |  5920 | `			/* Abort processing immeditaley */` |
|      ! 0 |  5921 | `			goto Abort;` |
|     8738 |  5922 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5923 | `			goto Exception;` |
|        - |  5924 | `		}` |
|     4369 |  5925 | `	}else{` |
|        - |  5926 | `		ph7_user_func *pFunc;` |
|        - |  5927 | `		ph7_context sCtx;` |
|        - |  5928 | `		ph7_value sRet;` |
|        - |  5929 | `		/* Look for an installed foreign function */` |
|   444676 |  5930 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   444676 |  5931 | `		if( pEntry == 0 ){` |
|        - |  5932 | `			/* Call to undefined function */` |
|        5 |  5933 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  5934 | `			/* Pop given arguments */` |
|        5 |  5935 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5936 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5937 | `			}` |
|        - |  5938 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  5939 | `			PH7_MemObjRelease(pTos);` |
|        5 |  5940 | `			break;` |
|        - |  5941 | `		}` |
|   444672 |  5942 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  5943 | `		/* Start collecting function arguments */` |
|   444672 |  5944 | `		SySetReset(&aArg);` |
|  1195532 |  5945 | `		while( pArg < pTos ){` |
|   750862 |  5946 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   750862 |  5947 | `			pArg++;` |
|        2 |  5948 | `		}` |
|        - |  5949 | `		/* Assume a null return value */` |
|   444672 |  5950 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  5951 | `		/* Init the call context */` |
|   444672 |  5952 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  5953 | `		/* Call the foreign function */` |
|   444672 |  5954 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5955 | `		/* Release the call context */` |
|   444672 |  5956 | `		VmReleaseCallContext(&sCtx);` |
|   444672 |  5957 | `		if( rc == PH7_ABORT ){` |
|      148 |  5958 | `			goto Abort;` |
|   444526 |  5959 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5960 | `			goto Exception;` |
|        - |  5961 | `		}` |
|   444524 |  5962 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5963 | `			/* Pop function name and arguments */` |
|   429422 |  5964 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   214732 |  5965 | `		}` |
|        - |  5966 | `		/* Save foreign function return value */` |
|   444524 |  5967 | `		PH7_MemObjStore(&sRet,pTos);` |
|   444524 |  5968 | `		PH7_MemObjRelease(&sRet);` |
|        - |  5969 | `	}` |
|   453258 |  5970 | `	break;` |
|        - |  5971 | `				  }` |
|        - |  5972 | `/*` |
|        - |  5973 | ` * OP_CONSUME: P1 * *` |
|        - |  5974 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  5975 | ` */` |
|     8504 |  5976 | `case PH7_OP_CONSUME: {` |
|    17010 |  5977 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    17010 |  5978 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  5979 |  |
|    17010 |  5980 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    17010 |  5981 | `	pCur = pOut;` |
|        - |  5982 | `	/* Start the consume process  */` |
|    34018 |  5983 | `	while( pOut <= pTos ){` |
|        - |  5984 | `		/* Force a string cast */` |
|    17010 |  5985 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|       54 |  5986 | `			PH7_MemObjToString(pOut);` |
|       26 |  5987 | `		}` |
|    17010 |  5988 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  5989 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  5990 | `			/* Invoke the output consumer callback */` |
|     9100 |  5991 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|     9100 |  5992 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  5993 | `				/* Increment output length */` |
|     3524 |  5994 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     1761 |  5995 | `			}` |
|     9100 |  5996 | `			SyBlobRelease(&pOut->sBlob);` |
|     9100 |  5997 | `			if( rc == SXERR_ABORT ){` |
|        - |  5998 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  5999 | `				goto Abort;` |
|        - |  6000 | `			}` |
|     4549 |  6001 | `		}` |
|    17010 |  6002 | `		pOut++;` |
|        2 |  6003 | `	}` |
|    17010 |  6004 | `	pTos = &pCur[-1];` |
|    17008 |  6005 | `	break;` |
|        - |  6006 | `					 }` |
|        - |  6007 |  |
|        - |  6008 | `		} /* Switch() */` |
|  7529536 |  6009 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6010 | `	} /* For(;;) */` |
|    10900 |  6011 | `Done:` |
|    21802 |  6012 | `	SySetRelease(&aArg);` |
|    21802 |  6013 | `	return SXRET_OK;` |
|       74 |  6014 | `Abort:` |
|      150 |  6015 | `	SySetRelease(&aArg);` |
|      522 |  6016 | `	while( pTos >= pStack ){` |
|      374 |  6017 | `		PH7_MemObjRelease(pTos);` |
|      374 |  6018 | `		pTos--;` |
|        2 |  6019 | `	}` |
|      150 |  6020 | `	return PH7_ABORT;` |
|        2 |  6021 | `Exception:` |
|        5 |  6022 | `	SySetRelease(&aArg);` |
|        9 |  6023 | `	while( pTos >= pStack ){` |
|        5 |  6024 | `		PH7_MemObjRelease(pTos);` |
|        5 |  6025 | `		pTos--;` |
|        1 |  6026 | `	}` |
|        5 |  6027 | `	return PH7_EXCEPTION;` |
|    10978 |  6028 |  |
|        - |  6029 | `/*` |
|        - |  6030 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6031 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6032 | ` * See block-comment on that function for additional information.` |
|        - |  6033 | ` */` |
|    11156 |  6034 | `static sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6035 |  |
|        - |  6036 | `	ph7_value *pStack;` |
|        - |  6037 | `	sxi32 rc;` |
|        - |  6038 | `	/* Allocate a new operand stack */` |
|    11158 |  6039 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    11158 |  6040 | `	if( pStack == 0 ){` |
|      ! 0 |  6041 | `		return SXERR_MEM;` |
|        - |  6042 | `	}` |
|        - |  6043 | `	/* Execute the program */` |
|    11158 |  6044 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  6045 | `	/* Free the operand stack */` |
|    11158 |  6046 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6047 | `	/* Execution result */` |
|    11158 |  6048 | `	return rc;` |
|     5580 |  6049 |  |
|        - |  6050 | `/*` |
|        - |  6051 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6052 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6053 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6054 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6055 | ` * execution ends.` |
|        - |  6056 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6057 | ` * additional information.` |
|        - |  6058 | ` */` |
|     1232 |  6059 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6060 |  |
|        - |  6061 | `	VmShutdownCB *pEntry;` |
|        - |  6062 | `	ph7_value *apArg[10];` |
|        - |  6063 | `	sxu32 n,nEntry;` |
|        - |  6064 | `	int i;` |
|        - |  6065 | `	/* Point to the stack of registered callbacks */` |
|     1234 |  6066 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    13554 |  6067 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    12322 |  6068 | `		apArg[i] = 0;` |
|     6162 |  6069 | `	}` |
|     1236 |  6070 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6071 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6072 | `		if( pEntry ){` |
|        - |  6073 | `			/* Prepare callback arguments if any */` |
|        3 |  6074 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6075 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6076 | `					break;` |
|        - |  6077 | `				}` |
|      ! 0 |  6078 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6079 | `			}` |
|        - |  6080 | `			/* Invoke the callback */` |
|        3 |  6081 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6082 | `			/*` |
|        - |  6083 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6084 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6085 | `			 */` |
|        3 |  6086 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6087 | `			if( pEntry ){` |
|        3 |  6088 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6089 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6090 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6091 | `				}` |
|        1 |  6092 | `			}` |
|        1 |  6093 | `		}` |
|        2 |  6094 | `	}` |
|     1234 |  6095 | `	SySetReset(&pVm->aShutdown);` |
|     1234 |  6096 |  |
|        - |  6097 | `/*` |
|        - |  6098 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6099 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6100 | ` * See block-comment on that function for additional information.` |
|        - |  6101 | ` */` |
|     1240 |  6102 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6103 |  |
|        - |  6104 | `	/* Make sure we are ready to execute this program */` |
|     1242 |  6105 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6106 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6107 | `	}` |
|        - |  6108 | `	/* Set the execution magic number  */` |
|     1242 |  6109 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6110 | `	/* Execute the program */` |
|     1242 |  6111 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6112 | `	/* Invoke any shutdown callbacks */` |
|     1238 |  6113 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6114 | `	/*` |
|        - |  6115 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6116 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6117 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6118 | `	 */` |
|     1238 |  6119 | `	return SXRET_OK;` |
|      622 |  6120 |  |
|        - |  6121 | `/*` |
|        - |  6122 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6123 | ` * the desired message.` |
|        - |  6124 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6125 | ` * in 'api.c' for additional information.` |
|        - |  6126 | ` */` |
|      372 |  6127 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6128 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6129 | `	SyString *pString /* Message to output */` |
|        - |  6130 | `	)` |
|        2 |  6131 |  |
|      374 |  6132 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      374 |  6133 | `	sxi32 rc = SXRET_OK;` |
|        - |  6134 | `	/* Call the output consumer */` |
|      374 |  6135 | `	if( pString->nByte > 0 ){` |
|      374 |  6136 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      374 |  6137 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6138 | `			/* Increment output length */` |
|       17 |  6139 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6140 | `		}` |
|      186 |  6141 | `	}` |
|      374 |  6142 | `	return rc;` |
|        2 |  6143 |  |
|        - |  6144 | `/*` |
|        - |  6145 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6146 | ` * callback to consume the formatted message.` |
|        - |  6147 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6148 | ` * in 'api.c' for additional information.` |
|        - |  6149 | ` */` |
|        2 |  6150 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6151 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6152 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6153 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6154 | `	)` |
|        1 |  6155 |  |
|        3 |  6156 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6157 | `	sxi32 rc = SXRET_OK;` |
|        - |  6158 | `	SyBlob sWorker;` |
|        - |  6159 | `	/* Format the message and call the output consumer */` |
|        3 |  6160 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6161 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6162 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6163 | `		/* Consume the formatted message */` |
|        3 |  6164 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6165 | `	}` |
|        3 |  6166 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6167 | `		/* Increment output length */` |
|      ! 0 |  6168 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6169 | `	}` |
|        - |  6170 | `	/* Release the working buffer */` |
|        3 |  6171 | `	SyBlobRelease(&sWorker);` |
|        3 |  6172 | `	return rc;` |
|        1 |  6173 |  |
|        - |  6174 | `/*` |
|        - |  6175 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6176 | ` * This function never fail and always return a pointer` |
|        - |  6177 | ` * to a null terminated string.` |
|        - |  6178 | ` */` |
|       10 |  6179 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6180 |  |
|       11 |  6181 | `	const char *zOp = "Unknown     ";` |
|       11 |  6182 | `	switch(nOp){` |
|        3 |  6183 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6184 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6185 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6186 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6187 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6188 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6189 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6190 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6191 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6192 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6193 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6194 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6195 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6196 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6197 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6198 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6199 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6200 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6201 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6202 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6203 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6204 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6205 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6206 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6207 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6208 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6209 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6210 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6211 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6212 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6213 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6214 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6215 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6216 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6217 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6218 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6219 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6220 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6221 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6222 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6223 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6224 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6225 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6226 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6227 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6228 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6229 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6230 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6231 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6232 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6233 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6234 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6235 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6236 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6237 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6238 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6239 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6240 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6241 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6242 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6243 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6244 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6245 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6246 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6247 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6248 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6249 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6250 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6251 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6252 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6253 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6254 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6255 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6256 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6257 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6258 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6259 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6260 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6261 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6262 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6263 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6264 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6265 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6266 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6267 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6268 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6269 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6270 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6271 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6272 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6273 | `	default:` |
|      ! 0 |  6274 | `		break;` |
|        - |  6275 | `	}` |
|       11 |  6276 | `	return zOp;` |
|        1 |  6277 |  |
|        - |  6278 | `/*` |
|        - |  6279 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6280 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6281 | ` * is responsible of consuming the generated dump.` |
|        - |  6282 | ` */` |
|        2 |  6283 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6284 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6285 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6286 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6287 | `	)` |
|        1 |  6288 |  |
|        - |  6289 | `	sxi32 rc;` |
|        3 |  6290 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6291 | `	return rc;` |
|        1 |  6292 |  |
|        - |  6293 | `/*` |
|        - |  6294 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6295 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6296 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6297 | ` * in 'compile.c' for additional information.` |
|        - |  6298 | ` */` |
|        8 |  6299 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6300 |  |
|        9 |  6301 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6302 | `	/* Evaluate and expand constant value */` |
|        9 |  6303 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6304 |  |
|        - |  6305 | `/*` |
|        - |  6306 | ` * Section:` |
|        - |  6307 | ` *  Function handling functions.` |
|        - |  6308 | ` * Status:` |
|        - |  6309 | ` *    Stable.` |
|        - |  6310 | ` */` |
|        - |  6311 | `/*` |
|        - |  6312 | ` * int func_num_args(void)` |
|        - |  6313 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6314 | ` * Parameters` |
|        - |  6315 | ` *   None.` |
|        - |  6316 | ` * Return` |
|        - |  6317 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6318 | ` *  or -1 if called from the globe scope.` |
|        - |  6319 | ` */` |
|      784 |  6320 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6321 |  |
|        - |  6322 | `	VmFrame *pFrame;` |
|        - |  6323 | `	ph7_vm *pVm;` |
|        - |  6324 | `	/* Point to the target VM */` |
|      786 |  6325 | `	pVm = pCtx->pVm;` |
|        - |  6326 | `	/* Current frame */` |
|      786 |  6327 | `	pFrame = pVm->pFrame;` |
|      786 |  6328 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6329 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6330 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6331 | `	}` |
|      786 |  6332 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6333 | `		SXUNUSED(nArg);` |
|      ! 0 |  6334 | `		SXUNUSED(apArg);` |
|        - |  6335 | `		/* Global frame,return -1 */` |
|      ! 0 |  6336 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6337 | `		return SXRET_OK;` |
|        - |  6338 | `	}` |
|        - |  6339 | `	/* Total number of arguments passed to the enclosing function */` |
|      786 |  6340 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      786 |  6341 | `	ph7_result_int(pCtx,nArg);` |
|      786 |  6342 | `	return SXRET_OK;` |
|      394 |  6343 |  |
|        - |  6344 | `/*` |
|        - |  6345 | ` * value func_get_arg(int $arg_num)` |
|        - |  6346 | ` *   Return an item from the argument list.` |
|        - |  6347 | ` * Parameters` |
|        - |  6348 | ` *  Argument number(index start from zero).` |
|        - |  6349 | ` * Return` |
|        - |  6350 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6351 | ` */` |
|        6 |  6352 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6353 |  |
|        8 |  6354 | `	ph7_value *pObj = 0;` |
|        8 |  6355 | `	VmSlot *pSlot = 0;` |
|        - |  6356 | `	VmFrame *pFrame;` |
|        - |  6357 | `	ph7_vm *pVm;` |
|        - |  6358 | `	/* Point to the target VM */` |
|        8 |  6359 | `	pVm = pCtx->pVm;` |
|        - |  6360 | `	/* Current frame */` |
|        8 |  6361 | `	pFrame = pVm->pFrame;` |
|        8 |  6362 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6363 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6364 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6365 | `	}` |
|        8 |  6366 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6367 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6368 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6369 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6370 | `		return SXRET_OK;` |
|        - |  6371 | `	}` |
|        - |  6372 | `	/* Extract the desired index */` |
|        5 |  6373 | `	nArg = ph7_value_to_int(apArg[0]);` |
|        5 |  6374 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6375 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6376 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6377 | `		return SXRET_OK;` |
|        - |  6378 | `	}` |
|        - |  6379 | `	/* Extract the desired argument */` |
|        5 |  6380 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|        5 |  6381 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6382 | `			/* Return the desired argument */` |
|        5 |  6383 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|        3 |  6384 | `		}else{` |
|        - |  6385 | `			/* No such argument,return false */` |
|      ! 0 |  6386 | `			ph7_result_bool(pCtx,0);` |
|        - |  6387 | `		}` |
|        3 |  6388 | `	}else{` |
|        - |  6389 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6390 | `		ph7_result_bool(pCtx,0);` |
|        - |  6391 | `	}` |
|        5 |  6392 | `	return SXRET_OK;` |
|        5 |  6393 |  |
|        - |  6394 | `/*` |
|        - |  6395 | ` * array func_get_args_byref(void)` |
|        - |  6396 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6397 | ` * Parameters` |
|        - |  6398 | ` *  None.` |
|        - |  6399 | ` * Return` |
|        - |  6400 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6401 | ` *  member of the current user-defined function's argument list.` |
|        - |  6402 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6403 | ` * NOTE:` |
|        - |  6404 | ` *  Arguments are returned to the array by reference.` |
|        - |  6405 | ` */` |
|        2 |  6406 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6407 |  |
|        - |  6408 | `	ph7_value *pArray;` |
|        - |  6409 | `	VmFrame *pFrame;` |
|        - |  6410 | `	VmSlot *aSlot;` |
|        - |  6411 | `	sxu32 n;` |
|        - |  6412 | `	/* Point to the current frame */` |
|        3 |  6413 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6414 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6415 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6416 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6417 | `	}` |
|        3 |  6418 | `	if( pFrame->pParent == 0 ){` |
|        - |  6419 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6420 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6421 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6422 | `		return SXRET_OK;` |
|        - |  6423 | `	}` |
|        - |  6424 | `	/* Create a new array */` |
|        3 |  6425 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6426 | `	if( pArray == 0 ){` |
|      ! 0 |  6427 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6428 | `		SXUNUSED(apArg);` |
|      ! 0 |  6429 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6430 | `		return SXRET_OK;` |
|        - |  6431 | `	}` |
|        - |  6432 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6433 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6434 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6435 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6436 | `	}` |
|        - |  6437 | `	/* Return the freshly created array */` |
|        3 |  6438 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6439 | `	return SXRET_OK;` |
|        2 |  6440 |  |
|        - |  6441 | `/*` |
|        - |  6442 | ` * array func_get_args(void)` |
|        - |  6443 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6444 | ` * Parameters` |
|        - |  6445 | ` *  None.` |
|        - |  6446 | ` * Return` |
|        - |  6447 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6448 | ` *  member of the current user-defined function's argument list.` |
|        - |  6449 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6450 | ` */` |
|       46 |  6451 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6452 |  |
|       47 |  6453 | `	ph7_value *pObj = 0;` |
|        - |  6454 | `	ph7_value *pArray;` |
|        - |  6455 | `	VmFrame *pFrame;` |
|        - |  6456 | `	VmSlot *aSlot;` |
|        - |  6457 | `	sxu32 n;` |
|        - |  6458 | `	/* Point to the current frame */` |
|       47 |  6459 | `	pFrame = pCtx->pVm->pFrame;` |
|       47 |  6460 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6461 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6462 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6463 | `	}` |
|       47 |  6464 | `	if( pFrame->pParent == 0 ){` |
|        - |  6465 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6466 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6467 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6468 | `		return SXRET_OK;` |
|        - |  6469 | `	}` |
|        - |  6470 | `	/* Create a new array */` |
|       47 |  6471 | `	pArray = ph7_context_new_array(pCtx);` |
|       47 |  6472 | `	if( pArray == 0 ){` |
|      ! 0 |  6473 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6474 | `		SXUNUSED(apArg);` |
|      ! 0 |  6475 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6476 | `		return SXRET_OK;` |
|        - |  6477 | `	}` |
|        - |  6478 | `	/* Start filling the array with the given arguments */` |
|       47 |  6479 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      143 |  6480 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|       97 |  6481 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|       97 |  6482 | `		if( pObj ){` |
|       97 |  6483 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       48 |  6484 | `		}` |
|       49 |  6485 | `	}` |
|        - |  6486 | `	/* Return the freshly created array */` |
|       47 |  6487 | `	ph7_result_value(pCtx,pArray);` |
|       47 |  6488 | `	return SXRET_OK;` |
|       24 |  6489 |  |
|        - |  6490 | `/*` |
|        - |  6491 | ` * bool function_exists(string $name)` |
|        - |  6492 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6493 | ` * Parameters` |
|        - |  6494 | ` *  The name of the desired function.` |
|        - |  6495 | ` * Return` |
|        - |  6496 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6497 | ` */` |
|     1702 |  6498 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6499 |  |
|        - |  6500 | `	const char *zName;` |
|        - |  6501 | `	ph7_vm *pVm;` |
|        - |  6502 | `	int nLen;` |
|        - |  6503 | `	int res;` |
|     1704 |  6504 | `	if( nArg < 1 ){` |
|        - |  6505 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6506 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6507 | `		return SXRET_OK;` |
|        - |  6508 | `	}` |
|        - |  6509 | `	/* Point to the target VM */` |
|     1704 |  6510 | `	pVm = pCtx->pVm;` |
|        - |  6511 | `	/* Extract the function name */` |
|     1704 |  6512 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6513 | `	/* Assume the function is not defined */` |
|     1704 |  6514 | `	res = 0;` |
|        - |  6515 | `	/* Perform the lookup */` |
|     2553 |  6516 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1698 |  6517 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6518 | `			/* Function is defined */` |
|      212 |  6519 | `			res = 1;` |
|      105 |  6520 | `	}` |
|     1704 |  6521 | `	ph7_result_bool(pCtx,res);` |
|     1704 |  6522 | `	return SXRET_OK;` |
|      853 |  6523 |  |
|        - |  6524 | `/* Forward declaration */` |
|        - |  6525 | `static ph7_class * VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg);` |
|        - |  6526 | `/*` |
|        - |  6527 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6528 | ` * [i.e: Whether it is callable or not].` |
|        - |  6529 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6530 | ` */` |
|    14176 |  6531 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6532 |  |
|    14178 |  6533 | `	int res = 0;` |
|    14178 |  6534 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6535 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6536 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6537 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6538 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6539 | `		if( pMethod && CallInvoke ){` |
|        - |  6540 | `			ph7_value sResult;` |
|        - |  6541 | `			sxi32 rc;` |
|        - |  6542 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6543 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6544 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6545 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6546 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6547 | `			}` |
|      ! 0 |  6548 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6549 | `		}` |
|    14178 |  6550 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       10 |  6551 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       10 |  6552 | `		if( pMap->nEntry > 1 ){` |
|        - |  6553 | `			ph7_class *pClass;` |
|        - |  6554 | `			ph7_value *pV;` |
|        - |  6555 | `			/* Extract the target class */` |
|       10 |  6556 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       10 |  6557 | `			if( pV ){` |
|       10 |  6558 | `				pClass = VmExtractClassFromValue(pVm,pV);` |
|       10 |  6559 | `				if( pClass ){` |
|        - |  6560 | `					ph7_class_method *pMethod;` |
|        - |  6561 | `					/* Extract the target method */` |
|        7 |  6562 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|        7 |  6563 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6564 | `						/* Perform the lookup */` |
|        7 |  6565 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|        7 |  6566 | `						if( pMethod ){` |
|        - |  6567 | `							/* Method is callable */` |
|        5 |  6568 | `							res = 1;` |
|        2 |  6569 | `						}` |
|        3 |  6570 | `					}` |
|        3 |  6571 | `				}` |
|        4 |  6572 | `			}` |
|        6 |  6573 | `		}` |
|    14174 |  6574 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6575 | `		const char *zName;` |
|        - |  6576 | `		int nLen;` |
|        - |  6577 | `		/* Extract the name */` |
|     4164 |  6578 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6579 | `		/* Perform the lookup */` |
|     4172 |  6580 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       16 |  6581 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6582 | `				/* Function is callable */` |
|     4154 |  6583 | `				res = 1;` |
|     2076 |  6584 | `		}` |
|     2081 |  6585 | `	}` |
|    14178 |  6586 | `	return res;` |
|        2 |  6587 |  |
|        - |  6588 | `/*` |
|        - |  6589 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6590 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6591 | ` * Parameters` |
|        - |  6592 | ` * $name` |
|        - |  6593 | ` *    The callback function to check` |
|        - |  6594 | ` * $syntax_only` |
|        - |  6595 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6596 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6597 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6598 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6599 | ` *    a string.` |
|        - |  6600 | ` * Return` |
|        - |  6601 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6602 | ` */` |
|       14 |  6603 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6604 |  |
|        - |  6605 | `	ph7_vm *pVm;` |
|        - |  6606 | `	int res;` |
|       15 |  6607 | `	if( nArg < 1 ){` |
|        - |  6608 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6609 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6610 | `		return SXRET_OK;` |
|        - |  6611 | `	}` |
|        - |  6612 | `	/* Point to the target VM */` |
|       15 |  6613 | `	pVm = pCtx->pVm;` |
|        - |  6614 | `	/* Perform the requested operation */` |
|       15 |  6615 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6616 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6617 | `	return SXRET_OK;` |
|        8 |  6618 |  |
|        - |  6619 | `/*` |
|        - |  6620 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6621 | ` * defined below.` |
|        - |  6622 | ` */` |
|     1046 |  6623 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6624 |  |
|     1047 |  6625 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6626 | `	ph7_value sName;` |
|        - |  6627 | `	sxi32 rc;` |
|        - |  6628 | `	/* Prepare the function name for insertion */` |
|     1047 |  6629 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1047 |  6630 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6631 | `	/* Perform the insertion */` |
|     1047 |  6632 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1047 |  6633 | `	PH7_MemObjRelease(&sName);` |
|     1047 |  6634 | `	return rc;` |
|        1 |  6635 |  |
|        - |  6636 | `/*` |
|        - |  6637 | ` * array get_defined_functions(void)` |
|        - |  6638 | ` *  Returns an array of all defined functions.` |
|        - |  6639 | ` * Parameter` |
|        - |  6640 | ` *  None.` |
|        - |  6641 | ` * Return` |
|        - |  6642 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6643 | ` *  both built-in (internal) and user-defined.` |
|        - |  6644 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6645 | ` *  defined ones using $arr["user"].` |
|        - |  6646 | ` * Note:` |
|        - |  6647 | ` *  NULL is returned on failure.` |
|        - |  6648 | ` */` |
|        2 |  6649 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6650 |  |
|        - |  6651 | `	ph7_value *pArray,*pEntry;` |
|        - |  6652 | `	/* NOTE:` |
|        - |  6653 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6654 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6655 | `	 */` |
|        3 |  6656 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6657 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6658 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6659 | `		SXUNUSED(apArg);` |
|        - |  6660 | `		/* Return NULL */` |
|      ! 0 |  6661 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6662 | `		return SXRET_OK;` |
|        - |  6663 | `	}` |
|        3 |  6664 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6665 | `	if( pEntry == 0 ){` |
|        - |  6666 | `		/* Return NULL */` |
|      ! 0 |  6667 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6668 | `		return SXRET_OK;` |
|        - |  6669 | `	}` |
|        - |  6670 | `	/* Fill with the appropriate information */` |
|        3 |  6671 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6672 | `	/* Create the 'internal' index */` |
|        3 |  6673 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6674 | `	/* Create the user-func array */` |
|        3 |  6675 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6676 | `	if( pEntry == 0 ){` |
|        - |  6677 | `		/* Return NULL */` |
|      ! 0 |  6678 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6679 | `		return SXRET_OK;` |
|        - |  6680 | `	}` |
|        - |  6681 | `	/* Fill with the appropriate information */` |
|        3 |  6682 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6683 | `	/* Create the 'user' index */` |
|        3 |  6684 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6685 | `	/* Return the multi-dimensional array */` |
|        3 |  6686 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6687 | `	return SXRET_OK;` |
|        2 |  6688 |  |
|        - |  6689 | `/*` |
|        - |  6690 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6691 | ` *  Register a function for execution on shutdown.` |
|        - |  6692 | ` * Note` |
|        - |  6693 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6694 | ` *  be called in the same order as they were registered.` |
|        - |  6695 | ` * Parameters` |
|        - |  6696 | ` *  $callback` |
|        - |  6697 | ` *   The shutdown callback to register.` |
|        - |  6698 | ` * $param` |
|        - |  6699 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6700 | ` * Return` |
|        - |  6701 | ` *  Nothing.` |
|        - |  6702 | ` */` |
|        2 |  6703 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6704 |  |
|        - |  6705 | `	VmShutdownCB sEntry;` |
|        - |  6706 | `	int i,j;` |
|        3 |  6707 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6708 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6709 | `		return PH7_OK;` |
|        - |  6710 | `	}` |
|        - |  6711 | `	/* Zero the Entry */` |
|        3 |  6712 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6713 | `	/* Initialize fields */` |
|        3 |  6714 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6715 | `	/* Save the callback name for later invocation name */` |
|        3 |  6716 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6717 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6718 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6719 | `	}` |
|        - |  6720 | `	/* Copy arguments */` |
|        3 |  6721 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6722 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6723 | `			/* Limit reached */` |
|      ! 0 |  6724 | `			break;` |
|        - |  6725 | `		}` |
|      ! 0 |  6726 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  6727 | `	}` |
|        3 |  6728 | `	sEntry.nArg = j;` |
|        - |  6729 | `	/* Install the callback */` |
|        3 |  6730 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  6731 | `	return PH7_OK;` |
|        2 |  6732 |  |
|        - |  6733 | `/*` |
|        - |  6734 | ` * Section:` |
|        - |  6735 | ` *  Class handling functions.` |
|        - |  6736 | ` * Status:` |
|        - |  6737 | ` *    Stable.` |
|        - |  6738 | ` */` |
|        - |  6739 | `/*` |
|        - |  6740 | ` * Extract the top active class. NULL is returned` |
|        - |  6741 | ` * if the class stack is empty.` |
|        - |  6742 | ` */` |
|      188 |  6743 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6744 |  |
|      190 |  6745 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6746 | `	ph7_class **apClass;` |
|      190 |  6747 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6748 | `		/* Empty stack,return NULL */` |
|       15 |  6749 | `		return 0;` |
|        - |  6750 | `	}` |
|        - |  6751 | `	/* Peek the last entry */` |
|      176 |  6752 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      176 |  6753 | `	return apClass[pSet->nUsed - 1];` |
|       96 |  6754 |  |
|        - |  6755 | `/*` |
|        - |  6756 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  6757 | ` *   Get the class that declared the currently executing method.` |
|        - |  6758 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  6759 | ` *` |
|        - |  6760 | ` * Parameters` |
|        - |  6761 | ` *   pVm: Target VM` |
|        - |  6762 | ` *` |
|        - |  6763 | ` * Return` |
|        - |  6764 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  6765 | ` *   - Not executing within a class method` |
|        - |  6766 | ` *` |
|        - |  6767 | ` * Note` |
|        - |  6768 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  6769 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  6770 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  6771 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  6772 | ` *   declaring class.` |
|        - |  6773 | ` */` |
|       18 |  6774 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        1 |  6775 |  |
|       19 |  6776 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  6777 | `	ph7_vm_func *pVmFunc;` |
|        - |  6778 |  |
|        - |  6779 | `	/* Skip exception frames to find the actual method frame */` |
|       19 |  6780 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  6781 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6782 | `	}` |
|        - |  6783 |  |
|        - |  6784 | `	/* Check if we're in a method context */` |
|       19 |  6785 | `	if( pFrame->pParent ){` |
|       15 |  6786 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       15 |  6787 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  6788 | `			/* Return the declaring class */` |
|       15 |  6789 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  6790 | `		}` |
|      ! 0 |  6791 | `	}` |
|        - |  6792 |  |
|        5 |  6793 | `	return 0;` |
|       10 |  6794 |  |
|        - |  6795 |  |
|        - |  6796 | `/*` |
|        - |  6797 | ` * string get_class ([ object $object = NULL ] )` |
|        - |  6798 | ` *   Returns the name of the class of an object` |
|        - |  6799 | ` * Parameters` |
|        - |  6800 | ` *  object` |
|        - |  6801 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|        - |  6802 | ` * Return` |
|        - |  6803 | ` *  The name of the class of which object is an instance.` |
|        - |  6804 | ` *  Returns FALSE if object is not an object.` |
|        - |  6805 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|        - |  6806 | ` */` |
|       18 |  6807 | `static int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6808 |  |
|        - |  6809 | `	ph7_class *pClass;` |
|        - |  6810 | `	SyString *pName;` |
|       20 |  6811 | `	if( nArg < 1 ){` |
|        - |  6812 | `		/* Check if we are inside a class */` |
|      ! 0 |  6813 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|      ! 0 |  6814 | `		if( pClass ){` |
|        - |  6815 | `			/* Point to the class name */` |
|      ! 0 |  6816 | `			pName = &pClass->sName;` |
|      ! 0 |  6817 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|      ! 0 |  6818 | `		}else{` |
|        - |  6819 | `			/* Not inside class,return FALSE */` |
|      ! 0 |  6820 | `			ph7_result_bool(pCtx,0);` |
|        - |  6821 | `		}` |
|      ! 0 |  6822 | `	}else{` |
|        - |  6823 | `		/* Extract the target class */` |
|       20 |  6824 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       20 |  6825 | `		if( pClass ){` |
|       18 |  6826 | `			pName = &pClass->sName;` |
|        - |  6827 | `			/* Return the class name */` |
|       18 |  6828 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|       10 |  6829 | `		}else{` |
|        - |  6830 | `			/* Not a class instance,return FALSE */` |
|        3 |  6831 | `			ph7_result_bool(pCtx,0);` |
|        - |  6832 | `		}` |
|        - |  6833 | `	}` |
|       20 |  6834 | `	return PH7_OK;` |
|        2 |  6835 |  |
|        - |  6836 | `/*` |
|        - |  6837 | ` * string get_parent_class([object $object = NULL ] )` |
|        - |  6838 | ` *   Returns the name of the parent class of an object` |
|        - |  6839 | ` * Parameters` |
|        - |  6840 | ` *  object` |
|        - |  6841 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|        - |  6842 | ` * Return` |
|        - |  6843 | ` *  The name of the parent class of which object is an instance.` |
|        - |  6844 | ` *  Returns FALSE if object is not an object or if the object does` |
|        - |  6845 | ` *  not have a parent.` |
|        - |  6846 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|        - |  6847 | ` */` |
|        8 |  6848 | `static int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6849 |  |
|        - |  6850 | `	ph7_class *pClass;` |
|        - |  6851 | `	SyString *pName;` |
|        9 |  6852 | `	if( nArg < 1 ){` |
|        - |  6853 | `		/* Check if we are inside a class [i.e: a method call]*/` |
|        3 |  6854 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|        3 |  6855 | `		if( pClass && pClass->pBase ){` |
|        - |  6856 | `			/* Point to the class name */` |
|        3 |  6857 | `			pName = &pClass->pBase->sName;` |
|        3 |  6858 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        2 |  6859 | `		}else{` |
|        - |  6860 | `			/* Not inside class,return FALSE */` |
|      ! 0 |  6861 | `			ph7_result_bool(pCtx,0);` |
|        - |  6862 | `		}` |
|        2 |  6863 | `	}else{` |
|        - |  6864 | `		/* Extract the target class */` |
|        7 |  6865 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        7 |  6866 | `		if( pClass ){` |
|        7 |  6867 | `			if( pClass->pBase ){` |
|        5 |  6868 | `				pName = &pClass->pBase->sName;` |
|        - |  6869 | `				/* Return the parent class name */` |
|        5 |  6870 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        3 |  6871 | `			}else{` |
|        - |  6872 | `				/* Object does not have a parent class */` |
|        3 |  6873 | `				ph7_result_bool(pCtx,0);` |
|        - |  6874 | `			}` |
|        4 |  6875 | `		}else{` |
|        - |  6876 | `			/* Not a class instance,return FALSE */` |
|      ! 0 |  6877 | `			ph7_result_bool(pCtx,0);` |
|        - |  6878 | `		}` |
|        - |  6879 | `	}` |
|        9 |  6880 | `	return PH7_OK;` |
|        1 |  6881 |  |
|        - |  6882 | `/*` |
|        - |  6883 | ` * string get_called_class(void)` |
|        - |  6884 | ` *   Gets the name of the class the static method is called in.` |
|        - |  6885 | ` * Parameters` |
|        - |  6886 | ` *  None.` |
|        - |  6887 | ` * Return` |
|        - |  6888 | ` *  Returns the class name. Returns FALSE if called from outside a class.` |
|        - |  6889 | ` */` |
|        4 |  6890 | `static int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6891 |  |
|        - |  6892 | `	ph7_class *pClass;` |
|        - |  6893 | `	/* Check if we are inside a class [i.e: a method call] */` |
|        5 |  6894 | `	pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|        5 |  6895 | `	if( pClass ){` |
|        - |  6896 | `		SyString *pName;` |
|        - |  6897 | `		/* Point to the class name */` |
|        5 |  6898 | `		pName = &pClass->sName;` |
|        5 |  6899 | `		ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        3 |  6900 | `	}else{` |
|      ! 0 |  6901 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6902 | `		SXUNUSED(apArg);` |
|        - |  6903 | `		/* Not inside class,return FALSE */` |
|      ! 0 |  6904 | `		ph7_result_bool(pCtx,0);` |
|        - |  6905 | `	}` |
|        5 |  6906 | `	return PH7_OK;` |
|        1 |  6907 |  |
|        - |  6908 | `/*` |
|        - |  6909 | ` * Extract a ph7_class from the given ph7_value.` |
|        - |  6910 | ` * The given value must be of type object [i.e: class instance] or` |
|        - |  6911 | ` * string which hold the class name.` |
|        - |  6912 | ` */` |
|       80 |  6913 | `static ph7_class * VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|        2 |  6914 |  |
|       82 |  6915 | `	ph7_class *pClass = 0;` |
|       82 |  6916 | `	if( ph7_value_is_object(pArg) ){` |
|        - |  6917 | `		/* Class instance already loaded,no need to perform a lookup */` |
|       44 |  6918 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|       61 |  6919 | `	}else if( ph7_value_is_string(pArg) ){` |
|        - |  6920 | `		const char *zClass;` |
|        - |  6921 | `		int nLen;` |
|        - |  6922 | `		/* Extract class name */` |
|       38 |  6923 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|       38 |  6924 | `		if( nLen > 0 ){` |
|        - |  6925 | `			SyHashEntry *pEntry;` |
|        - |  6926 | `			/* Perform a lookup */` |
|       38 |  6927 | `			pEntry = SyHashGet(&pVm->hClass,(const void *)zClass,(sxu32)nLen);` |
|       38 |  6928 | `			if( pEntry ){` |
|        - |  6929 | `				/* Point to the desired class */` |
|       31 |  6930 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|       15 |  6931 | `			}` |
|       18 |  6932 | `		}` |
|       18 |  6933 | `	}` |
|       82 |  6934 | `	return pClass;` |
|        2 |  6935 |  |
|        - |  6936 | `/*` |
|        - |  6937 | ` * bool property_exists(mixed $class,string $property)` |
|        - |  6938 | ` *   Checks if the object or class has a property.` |
|        - |  6939 | ` * Parameters` |
|        - |  6940 | ` *  class` |
|        - |  6941 | ` *   The class name or an object of the class to test for` |
|        - |  6942 | ` * property` |
|        - |  6943 | ` *  The name of the property` |
|        - |  6944 | ` * Return` |
|        - |  6945 | ` *   Returns TRUE if the property exists,FALSE otherwise.` |
|        - |  6946 | ` */` |
|       12 |  6947 | `static int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6948 |  |
|       13 |  6949 | `	int res = 0; /* Assume attribute does not exists */` |
|       13 |  6950 | `	if( nArg > 1 ){` |
|        - |  6951 | `		ph7_class *pClass;` |
|       13 |  6952 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       13 |  6953 | `		if( pClass ){` |
|        - |  6954 | `			const char *zName;` |
|        - |  6955 | `			int nLen;` |
|        - |  6956 | `			/* Extract attribute name */` |
|       13 |  6957 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|       13 |  6958 | `			if( nLen > 0 ){` |
|        - |  6959 | `				/* Perform the lookup in the attribute and method table */` |
|       12 |  6960 | `				if( SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen) != 0` |
|        8 |  6961 | `					\|\| SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6962 | `						/* property exists,flag that */` |
|       11 |  6963 | `						res = 1;` |
|        5 |  6964 | `				}` |
|        6 |  6965 | `			}` |
|        6 |  6966 | `		}` |
|        6 |  6967 | `	}` |
|       13 |  6968 | `	ph7_result_bool(pCtx,res);` |
|       13 |  6969 | `	return PH7_OK;` |
|        1 |  6970 |  |
|        - |  6971 | `/*` |
|        - |  6972 | ` * bool method_exists(mixed $class,string $method)` |
|        - |  6973 | ` *   Checks if the given method is a class member.` |
|        - |  6974 | ` * Parameters` |
|        - |  6975 | ` *  class` |
|        - |  6976 | ` *   The class name or an object of the class to test for` |
|        - |  6977 | ` * property` |
|        - |  6978 | ` *  The name of the method` |
|        - |  6979 | ` * Return` |
|        - |  6980 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|        - |  6981 | ` */` |
|        4 |  6982 | `static int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6983 |  |
|        5 |  6984 | `	int res = 0; /* Assume method does not exists */` |
|        5 |  6985 | `	if( nArg > 1 ){` |
|        - |  6986 | `		ph7_class *pClass;` |
|        5 |  6987 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        5 |  6988 | `		if( pClass ){` |
|        - |  6989 | `			const char *zName;` |
|        - |  6990 | `			int nLen;` |
|        - |  6991 | `			/* Extract method name */` |
|        5 |  6992 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|        5 |  6993 | `			if( nLen > 0 ){` |
|        - |  6994 | `				/* Perform the lookup in the method table */` |
|        5 |  6995 | `				if( SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6996 | `					/* method exists,flag that */` |
|        3 |  6997 | `					res = 1;` |
|        1 |  6998 | `				}` |
|        2 |  6999 | `			}` |
|        2 |  7000 | `		}` |
|        2 |  7001 | `	}` |
|        5 |  7002 | `	ph7_result_bool(pCtx,res);` |
|        5 |  7003 | `	return PH7_OK;` |
|        1 |  7004 |  |
|        - |  7005 | `/*` |
|        - |  7006 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|        - |  7007 | ` *   Checks if the class has been defined.` |
|        - |  7008 | ` * Parameters` |
|        - |  7009 | ` *  class_name` |
|        - |  7010 | ` *   The class name. The name is matched in a case-sensitive manner` |
|        - |  7011 | ` *   unlinke the standard PHP engine.` |
|        - |  7012 | ` *  autoload` |
|        - |  7013 | ` *   Whether or not to call __autoload by default.` |
|        - |  7014 | ` * Return` |
|        - |  7015 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|        - |  7016 | ` */` |
|       12 |  7017 | `static int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7018 |  |
|       14 |  7019 | `	int res = 0; /* Assume class does not exists */` |
|       14 |  7020 | `	if( nArg > 0 ){` |
|        - |  7021 | `		const char *zName;` |
|        - |  7022 | `		int nLen;` |
|        - |  7023 | `		/* Extract given name */` |
|       14 |  7024 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7025 | `		/* Perform a hashlookup */` |
|       14 |  7026 | `		if( nLen > 0 && SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7027 | `			/* class is available */` |
|       10 |  7028 | `			res = 1;` |
|        4 |  7029 | `		}` |
|        6 |  7030 | `	}` |
|       14 |  7031 | `	ph7_result_bool(pCtx,res);` |
|       14 |  7032 | `	return PH7_OK;` |
|        2 |  7033 |  |
|        - |  7034 | `/*` |
|        - |  7035 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|        - |  7036 | ` *   Checks if the interface has been defined.` |
|        - |  7037 | ` * Parameters` |
|        - |  7038 | ` *  class_name` |
|        - |  7039 | ` *   The class name. The name is matched in a case-sensitive manner` |
|        - |  7040 | ` *   unlinke the standard PHP engine.` |
|        - |  7041 | ` *  autoload` |
|        - |  7042 | ` *   Whether or not to call __autoload by default.` |
|        - |  7043 | ` * Return` |
|        - |  7044 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|        - |  7045 | ` */` |
|        6 |  7046 | `static int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7047 |  |
|        7 |  7048 | `	int res = 0; /* Assume class does not exists */` |
|        7 |  7049 | `	if( nArg > 0 ){` |
|        7 |  7050 | `		SyHashEntry *pEntry = 0;` |
|        - |  7051 | `		const char *zName;` |
|        - |  7052 | `		int nLen;` |
|        - |  7053 | `		/* Extract given name */` |
|        7 |  7054 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7055 | `		/* Perform a hashlookup */` |
|        7 |  7056 | `		if( nLen > 0 ){` |
|        7 |  7057 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|        3 |  7058 | `		}` |
|        7 |  7059 | `		if( pEntry ){` |
|        5 |  7060 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        5 |  7061 | `			while( pClass ){` |
|        5 |  7062 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |  7063 | `					/* interface is available */` |
|        5 |  7064 | `					res = 1;` |
|        5 |  7065 | `					break;` |
|        - |  7066 | `				}` |
|        - |  7067 | `				/* Next with the same name */` |
|      ! 0 |  7068 | `				pClass = pClass->pNextName;` |
|      ! 0 |  7069 | `			}` |
|        2 |  7070 | `		}` |
|        3 |  7071 | `	}` |
|        7 |  7072 | `	ph7_result_bool(pCtx,res);` |
|        7 |  7073 | `	return PH7_OK;` |
|        1 |  7074 |  |
|        - |  7075 | `/*` |
|        - |  7076 | ` * bool class_alias([string $original[,string $alias ]])` |
|        - |  7077 | ` *   Creates an alias for a class.` |
|        - |  7078 | ` * Parameters` |
|        - |  7079 | ` *  original` |
|        - |  7080 | ` *    The original class.` |
|        - |  7081 | ` *  alias` |
|        - |  7082 | ` *   The alias name for the class.` |
|        - |  7083 | ` * Return` |
|        - |  7084 | ` *   Returns TRUE on success or FALSE on failure.` |
|        - |  7085 | ` */` |
|        2 |  7086 | `static int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7087 |  |
|        - |  7088 | `	const char *zOld,*zNew;` |
|        - |  7089 | `	int nOldLen,nNewLen;` |
|        - |  7090 | `	SyHashEntry *pEntry;` |
|        - |  7091 | `	ph7_class *pClass;` |
|        - |  7092 | `	char *zDup;` |
|        - |  7093 | `	sxi32 rc;` |
|        3 |  7094 | `	if( nArg < 2 ){` |
|        - |  7095 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  7096 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7097 | `		return PH7_OK;` |
|        - |  7098 | `	}` |
|        - |  7099 | `	/* Extract old class name */` |
|        3 |  7100 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|        - |  7101 | `	/* Extract alias name */` |
|        3 |  7102 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|        3 |  7103 | `	if( nNewLen < 1 ){` |
|        - |  7104 | `		/* Invalid alias name,return FALSE */` |
|      ! 0 |  7105 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7106 | `		return PH7_OK;` |
|        - |  7107 | `	}` |
|        - |  7108 | `	/* Perform a hash lookup */` |
|        3 |  7109 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,(sxu32)nOldLen);` |
|        3 |  7110 | `	if( pEntry ==  0 ){` |
|        - |  7111 | `		/* No such class,return FALSE */` |
|      ! 0 |  7112 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7113 | `		return PH7_OK;` |
|        - |  7114 | `	}` |
|        - |  7115 | `	/* Point to the class */` |
|        3 |  7116 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7117 | `	/* Duplicate alias name */` |
|        3 |  7118 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,(sxu32)nNewLen);` |
|        3 |  7119 | `	if( zDup == 0 ){` |
|        - |  7120 | `		/* Out of memory,return FALSE */` |
|      ! 0 |  7121 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7122 | `		return PH7_OK;` |
|        - |  7123 | `	}` |
|        - |  7124 | `	/* Create the alias */` |
|        3 |  7125 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,(sxu32)nNewLen,pClass);` |
|        3 |  7126 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7127 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|      ! 0 |  7128 | `	}` |
|        3 |  7129 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|        3 |  7130 | `	return PH7_OK;` |
|        2 |  7131 |  |
|        - |  7132 | `/*` |
|        - |  7133 | ` * array get_declared_classes(void)` |
|        - |  7134 | ` *   Returns an array with the name of the defined classes` |
|        - |  7135 | ` * Parameters` |
|        - |  7136 | ` *  None` |
|        - |  7137 | ` * Return` |
|        - |  7138 | ` *   Returns an array of the names of the declared classes` |
|        - |  7139 | ` *   in the current script.` |
|        - |  7140 | ` * Note:` |
|        - |  7141 | ` *   NULL is returned on failure.` |
|        - |  7142 | ` */` |
|        2 |  7143 | `static int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7144 |  |
|        - |  7145 | `	ph7_value *pName,*pArray;` |
|        - |  7146 | `	SyHashEntry *pEntry;` |
|        - |  7147 | `	/* Create a new array first */` |
|        3 |  7148 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7149 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7150 | `	if( pArray == 0 \|\| pName == 0){` |
|      ! 0 |  7151 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7152 | `		SXUNUSED(apArg);` |
|        - |  7153 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7154 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7155 | `		return PH7_OK;` |
|        - |  7156 | `	}` |
|        - |  7157 | `	/* Fill the array with the defined classes */` |
|        3 |  7158 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|       52 |  7159 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|       49 |  7160 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7161 | `		/* Do not register classes defined as interfaces */` |
|       49 |  7162 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       43 |  7163 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|        - |  7164 | `			/* insert class name */` |
|       43 |  7165 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7166 | `			/* Reset the cursor */` |
|       43 |  7167 | `			ph7_value_reset_string_cursor(pName);` |
|       21 |  7168 | `		}` |
|        1 |  7169 | `	}` |
|        - |  7170 | `	/* Return the created array */` |
|        3 |  7171 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7172 | `	return PH7_OK;` |
|        2 |  7173 |  |
|        - |  7174 | `/*` |
|        - |  7175 | ` * array get_declared_interfaces(void)` |
|        - |  7176 | ` *   Returns an array with the name of the defined interfaces` |
|        - |  7177 | ` * Parameters` |
|        - |  7178 | ` *  None` |
|        - |  7179 | ` * Return` |
|        - |  7180 | ` *   Returns an array of the names of the declared interfaces` |
|        - |  7181 | ` *   in the current script.` |
|        - |  7182 | ` * Note:` |
|        - |  7183 | ` *   NULL is returned on failure.` |
|        - |  7184 | ` */` |
|        2 |  7185 | `static int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7186 |  |
|        - |  7187 | `	ph7_value *pName,*pArray;` |
|        - |  7188 | `	SyHashEntry *pEntry;` |
|        - |  7189 | `	/* Create a new array first */` |
|        3 |  7190 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7191 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7192 | `	if( pArray == 0 \|\| pName == 0 ){` |
|      ! 0 |  7193 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7194 | `		SXUNUSED(apArg);` |
|        - |  7195 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7196 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7197 | `		return PH7_OK;` |
|        - |  7198 | `	}` |
|        - |  7199 | `	/* Fill the array with the defined classes */` |
|        3 |  7200 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|       54 |  7201 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|       51 |  7202 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7203 | `		/* Register classes defined as interfaces only */` |
|       51 |  7204 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        9 |  7205 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|        - |  7206 | `			/* insert interface name */` |
|        9 |  7207 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7208 | `			/* Reset the cursor */` |
|        9 |  7209 | `			ph7_value_reset_string_cursor(pName);` |
|        4 |  7210 | `		}` |
|        1 |  7211 | `	}` |
|        - |  7212 | `	/* Return the created array */` |
|        3 |  7213 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7214 | `	return PH7_OK;` |
|        2 |  7215 |  |
|        - |  7216 | `/*` |
|        - |  7217 | ` * array get_class_methods(string/object $class_name)` |
|        - |  7218 | ` *   Returns an array with the name of the class methods` |
|        - |  7219 | ` * Parameters` |
|        - |  7220 | ` *  class_name` |
|        - |  7221 | ` *  The class name or class instance` |
|        - |  7222 | ` * Return` |
|        - |  7223 | ` *  Returns an array of method names defined for the class specified by class_name.` |
|        - |  7224 | ` *  In case of an error, it returns NULL.` |
|        - |  7225 | ` * Note:` |
|        - |  7226 | ` *   NULL is returned on failure.` |
|        - |  7227 | ` */` |
|        6 |  7228 | `static int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7229 |  |
|        - |  7230 | `	ph7_value *pName,*pArray;` |
|        - |  7231 | `	SyHashEntry *pEntry;` |
|        - |  7232 | `	ph7_class *pClass;` |
|        - |  7233 | `	/* Extract the target class first */` |
|        7 |  7234 | `	pClass = 0;` |
|        7 |  7235 | `	if( nArg > 0 ){` |
|        7 |  7236 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        3 |  7237 | `	}` |
|        7 |  7238 | `	if( pClass == 0 ){` |
|        - |  7239 | `		/* No such class,return NULL */` |
|        3 |  7240 | `		ph7_result_null(pCtx);` |
|        3 |  7241 | `		return PH7_OK;` |
|        - |  7242 | `	}` |
|        - |  7243 | `	/* Create a new array  */` |
|        5 |  7244 | `	pArray = ph7_context_new_array(pCtx);` |
|        5 |  7245 | `	pName = ph7_context_new_scalar(pCtx);` |
|        5 |  7246 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7247 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7248 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7249 | `		return PH7_OK;` |
|        - |  7250 | `	}` |
|        - |  7251 | `	/* Fill the array with the defined methods */` |
|        5 |  7252 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|       17 |  7253 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       13 |  7254 | `		ph7_class_method *pMethod = (ph7_class_method *)pEntry->pUserData;` |
|        - |  7255 | `		/* Insert method name */` |
|       13 |  7256 | `		ph7_value_string(pName,SyStringData(&pMethod->sFunc.sName),(int)SyStringLength(&pMethod->sFunc.sName));` |
|       13 |  7257 | `		ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7258 | `		/* Reset the cursor */` |
|       13 |  7259 | `		ph7_value_reset_string_cursor(pName);` |
|        1 |  7260 | `	}` |
|        - |  7261 | `	/* Return the created array */` |
|        5 |  7262 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7263 | `	/*` |
|        - |  7264 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7265 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7266 | `	 */` |
|        5 |  7267 | `	return PH7_OK;` |
|        4 |  7268 |  |
|        - |  7269 | `/*` |
|        - |  7270 | ` * This function return TRUE(1) if the given class attribute stored` |
|        - |  7271 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|        - |  7272 | ` * from the current scope.Otherwise FALSE is returned.` |
|        - |  7273 | ` */` |
|     1568 |  7274 | `static int VmClassMemberAccess(` |
|        - |  7275 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7276 | `	ph7_class *pClass,         /* Target Class */` |
|        - |  7277 | `	const SyString *pAttrName, /* Attribute name */` |
|        - |  7278 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|        - |  7279 | `	int bLog                   /* TRUE to log forbidden access. */` |
|        - |  7280 | `	)` |
|        2 |  7281 |  |
|     1570 |  7282 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     1006 |  7283 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  7284 | `		ph7_vm_func *pVmFunc;` |
|     1010 |  7285 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|        - |  7286 | `			/* Safely ignore the exception frame */` |
|        5 |  7287 | `			pFrame = pFrame->pParent;` |
|        1 |  7288 | `		}` |
|     1006 |  7289 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     1006 |  7290 | `		if( pVmFunc == 0 \|\| (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|        9 |  7291 | `			goto dis; /* Access is forbidden */` |
|        - |  7292 | `		}` |
|      998 |  7293 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - |  7294 | `			/* Must be the same instance */` |
|        7 |  7295 | `			if( (ph7_class *)pVmFunc->pUserData != pClass ){` |
|      ! 0 |  7296 | `				goto dis; /* Access is forbidden */` |
|        - |  7297 | `			}` |
|        4 |  7298 | `		}else{` |
|        - |  7299 | `			/* Protected */` |
|      992 |  7300 | `			ph7_class *pBase = (ph7_class *)pVmFunc->pUserData;` |
|        - |  7301 | `			/* Must be a derived class */` |
|      992 |  7302 | `			if( !VmInstanceOf(pClass,pBase) ){` |
|      ! 0 |  7303 | `				goto dis; /* Access is forbidden */` |
|        - |  7304 | `			}` |
|        - |  7305 | `		}` |
|      498 |  7306 | `	}` |
|     1562 |  7307 | `	return 1; /* Access is granted */` |
|        4 |  7308 | `dis:` |
|        9 |  7309 | `	if( bLog ){` |
|      ! 0 |  7310 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7311 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|      ! 0 |  7312 | `			&pClass->sName,pAttrName);` |
|      ! 0 |  7313 | `	}` |
|        9 |  7314 | `	return 0; /* Access is forbidden */` |
|      786 |  7315 |  |
|        - |  7316 | `/*` |
|        - |  7317 | ` * array get_class_vars(string/object $class_name)` |
|        - |  7318 | ` *   Get the default properties of the class` |
|        - |  7319 | ` * Parameters` |
|        - |  7320 | ` *  class_name` |
|        - |  7321 | ` *   The class name or class instance` |
|        - |  7322 | ` * Return` |
|        - |  7323 | ` *  Returns an associative array of declared properties visible from the current scope` |
|        - |  7324 | ` *  with their default value. The resulting array elements are in the form` |
|        - |  7325 | ` *  of varname => value.` |
|        - |  7326 | ` * Note:` |
|        - |  7327 | ` *   NULL is returned on failure.` |
|        - |  7328 | ` */` |
|        2 |  7329 | `static int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7330 |  |
|        - |  7331 | `	ph7_value *pName,*pArray,sValue;` |
|        - |  7332 | `	SyHashEntry *pEntry;` |
|        - |  7333 | `	ph7_class *pClass;` |
|        - |  7334 | `	/* Extract the target class first */` |
|        3 |  7335 | `	pClass = 0;` |
|        3 |  7336 | `	if( nArg > 0 ){` |
|        3 |  7337 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        1 |  7338 | `	}` |
|        3 |  7339 | `	if( pClass == 0 ){` |
|        - |  7340 | `		/* No such class,return NULL */` |
|      ! 0 |  7341 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7342 | `		return PH7_OK;` |
|        - |  7343 | `	}` |
|        - |  7344 | `	/* Create a new array  */` |
|        3 |  7345 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7346 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7347 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|        3 |  7348 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7349 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7350 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7351 | `		return PH7_OK;` |
|        - |  7352 | `	}` |
|        - |  7353 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|        3 |  7354 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        8 |  7355 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        5 |  7356 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|        - |  7357 | `		/* Check if the access is allowed */` |
|        5 |  7358 | `		if( VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        5 |  7359 | `			SyString *pAttrName = &pAttr->sName;` |
|        5 |  7360 | `			ph7_value *pValue = 0;` |
|        5 |  7361 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |  7362 | `				/* Extract static attribute value which is always computed */` |
|        5 |  7363 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|        3 |  7364 | `			}else{` |
|      ! 0 |  7365 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      ! 0 |  7366 | `					PH7_MemObjRelease(&sValue);` |
|        - |  7367 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|      ! 0 |  7368 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue);` |
|      ! 0 |  7369 | `					pValue = &sValue;` |
|      ! 0 |  7370 | `				}` |
|        - |  7371 | `			}` |
|        - |  7372 | `			/* Fill in the array */` |
|        5 |  7373 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|        5 |  7374 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|        - |  7375 | `			/* Reset the cursor */` |
|        5 |  7376 | `			ph7_value_reset_string_cursor(pName);` |
|        2 |  7377 | `		}` |
|        1 |  7378 | `	}` |
|        3 |  7379 | `	PH7_MemObjRelease(&sValue);` |
|        - |  7380 | `	/* Return the created array */` |
|        3 |  7381 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7382 | `	/*` |
|        - |  7383 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7384 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7385 | `	 */` |
|        3 |  7386 | `	return PH7_OK;` |
|        2 |  7387 |  |
|        - |  7388 | `/*` |
|        - |  7389 | ` * array get_object_vars(object $this)` |
|        - |  7390 | ` *   Gets the properties of the given object` |
|        - |  7391 | ` * Parameters` |
|        - |  7392 | ` *  this` |
|        - |  7393 | ` *   A class instance` |
|        - |  7394 | ` * Return` |
|        - |  7395 | ` *  Returns an associative array of defined object accessible non-static properties` |
|        - |  7396 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|        - |  7397 | ` *  it will be returned with a NULL value.` |
|        - |  7398 | ` * Note:` |
|        - |  7399 | ` *   NULL is returned on failure.` |
|        - |  7400 | ` */` |
|        2 |  7401 | `static int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7402 |  |
|        3 |  7403 | `	ph7_class_instance *pThis = 0;` |
|        - |  7404 | `	ph7_value *pName,*pArray;` |
|        - |  7405 | `	SyHashEntry *pEntry;` |
|        3 |  7406 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|        - |  7407 | `		/* Extract the target instance */` |
|        3 |  7408 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        1 |  7409 | `	}` |
|        3 |  7410 | `	if( pThis == 0 ){` |
|        - |  7411 | `		/* No such instance,return NULL */` |
|      ! 0 |  7412 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7413 | `		return PH7_OK;` |
|        - |  7414 | `	}` |
|        - |  7415 | `	/* Create a new array  */` |
|        3 |  7416 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7417 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7418 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7419 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7420 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7421 | `		return PH7_OK;` |
|        - |  7422 | `	}` |
|        - |  7423 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|        3 |  7424 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  7425 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        7 |  7426 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  7427 | `		SyString *pAttrName;` |
|        7 |  7428 | `		if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        - |  7429 | `			/* Only non-static/constant attributes are extracted */` |
|      ! 0 |  7430 | `			continue;` |
|        - |  7431 | `		}` |
|        7 |  7432 | `		pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  7433 | `		/* Check if the access is allowed */` |
|        7 |  7434 | `		if( VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|        3 |  7435 | `			ph7_value *pValue = 0;` |
|        - |  7436 | `			/* Extract attribute */` |
|        3 |  7437 | `			pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|        3 |  7438 | `			if( pValue ){` |
|        - |  7439 | `				/* Insert attribute name in the array */` |
|        3 |  7440 | `				ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|        3 |  7441 | `				ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|        1 |  7442 | `			}` |
|        - |  7443 | `			/* Reset the cursor */` |
|        3 |  7444 | `			ph7_value_reset_string_cursor(pName);` |
|        1 |  7445 | `		}` |
|        1 |  7446 | `	}` |
|        - |  7447 | `	/* Return the created array */` |
|        3 |  7448 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7449 | `	/*` |
|        - |  7450 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7451 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7452 | `	 */` |
|        3 |  7453 | `	return PH7_OK;` |
|        2 |  7454 |  |
|        - |  7455 | `/*` |
|        - |  7456 | ` * This function returns TRUE if the given class is an implemented` |
|        - |  7457 | ` * interface.Otherwise FALSE is returned.` |
|        - |  7458 | ` */` |
|     2042 |  7459 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|        2 |  7460 |  |
|        - |  7461 | `	ph7_class **apInterface;` |
|        - |  7462 | `	sxu32 n;` |
|     2044 |  7463 | `	if( SySetUsed(pSet) < 1 ){` |
|        - |  7464 | `		/* Empty interface container */` |
|     2042 |  7465 | `		return FALSE;` |
|        - |  7466 | `	}` |
|        - |  7467 | `	/* Point to the set of implemented interfaces */` |
|        3 |  7468 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|        - |  7469 | `	/* Perform the lookup */` |
|        3 |  7470 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
|        3 |  7471 | `		if( apInterface[n] == pClass ){` |
|        3 |  7472 | `			return TRUE;` |
|        - |  7473 | `		}` |
|      ! 0 |  7474 | `	}` |
|      ! 0 |  7475 | `	return FALSE;` |
|     1023 |  7476 |  |
|        - |  7477 | `/*` |
|        - |  7478 | ` * This function returns TRUE if the given class (first argument)` |
|        - |  7479 | ` * is an instance of the main class (second argument).` |
|        - |  7480 | ` * Otherwise FALSE is returned.` |
|        - |  7481 | ` */` |
|     1042 |  7482 | `static int VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|        2 |  7483 |  |
|        - |  7484 | `	ph7_class *pParent;` |
|        - |  7485 | `	sxi32 rc;` |
|     1044 |  7486 | `	if( pThis == pClass ){` |
|        - |  7487 | `		/* Instance of the same class */` |
|      140 |  7488 | `		return TRUE;` |
|        - |  7489 | `	}` |
|        - |  7490 | `	/* Check implemented interfaces */` |
|      906 |  7491 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
|      906 |  7492 | `	if( rc ){` |
|        3 |  7493 | `		return TRUE;` |
|        - |  7494 | `	}` |
|        - |  7495 | `	/* Check parent classes */` |
|      904 |  7496 | `	pParent = pThis->pBase;` |
|     2042 |  7497 | `	while( pParent ){` |
|     2040 |  7498 | `		if( pParent == pClass ){` |
|        - |  7499 | `			/* Same instance */` |
|      902 |  7500 | `			return TRUE;` |
|        - |  7501 | `		}` |
|        - |  7502 | `		/* Check the implemented interfaces */` |
|     1140 |  7503 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|     1140 |  7504 | `		if( rc ){` |
|      ! 0 |  7505 | `			return TRUE;` |
|        - |  7506 | `		}` |
|        - |  7507 | `		/* Point to the parent class */` |
|     1140 |  7508 | `		pParent = pParent->pBase;` |
|        2 |  7509 | `	}` |
|        - |  7510 | `	/* Not an instance of the the given class */` |
|        3 |  7511 | `	return FALSE;` |
|      523 |  7512 |  |
|        - |  7513 | `/*` |
|        - |  7514 | ` * This function returns TRUE if the given class (first argument)` |
|        - |  7515 | ` * is a subclass of the main class (second argument).` |
|        - |  7516 | ` * Otherwise FALSE is returned.` |
|        - |  7517 | ` */` |
|        4 |  7518 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|        1 |  7519 |  |
|        5 |  7520 | `	SySet *pInterface = &pClass->aInterface;` |
|        - |  7521 | `	SyHashEntry *pEntry;` |
|        - |  7522 | `	SyString *pName;` |
|        - |  7523 | `	sxi32 rc;` |
|        5 |  7524 | `	while( pClass ){` |
|        5 |  7525 | `		pName = &pClass->sName;` |
|        - |  7526 | `		/* Query the derived hashtable */` |
|        5 |  7527 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|        5 |  7528 | `		if( pEntry ){` |
|        5 |  7529 | `			return TRUE;` |
|        - |  7530 | `		}` |
|      ! 0 |  7531 | `		pClass = pClass->pBase;` |
|      ! 0 |  7532 | `	}` |
|      ! 0 |  7533 | `	rc = VmQueryInterfaceSet(pBase,pInterface);` |
|      ! 0 |  7534 | `	if( rc ){` |
|      ! 0 |  7535 | `		return TRUE;` |
|        - |  7536 | `	}` |
|        - |  7537 | `	/* Not a subclass */` |
|      ! 0 |  7538 | `	return FALSE;` |
|        3 |  7539 |  |
|        - |  7540 | `/*` |
|        - |  7541 | ` * bool is_a(object $object,string $class_name)` |
|        - |  7542 | ` *   Checks if the object is of this class or has this class as one of its parents.` |
|        - |  7543 | ` * Parameters` |
|        - |  7544 | ` *  object` |
|        - |  7545 | ` *   The tested object` |
|        - |  7546 | ` * class_name` |
|        - |  7547 | ` *  The class name` |
|        - |  7548 | ` * Return` |
|        - |  7549 | ` *   Returns TRUE if the object is of this class or has this class as one of its` |
|        - |  7550 | ` *   parents, FALSE otherwise.` |
|        - |  7551 | ` */` |
|        2 |  7552 | `static int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7553 |  |
|        3 |  7554 | `	int res = 0; /* Assume FALSE by default */` |
|        3 |  7555 | `	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){` |
|        3 |  7556 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7557 | `		ph7_class *pClass;` |
|        - |  7558 | `		/* Extract the given class */` |
|        3 |  7559 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|        3 |  7560 | `		if( pClass ){` |
|        - |  7561 | `			/* Perform the query */` |
|        3 |  7562 | `			res = VmInstanceOf(pThis->pClass,pClass);` |
|        1 |  7563 | `		}` |
|        1 |  7564 | `	}` |
|        - |  7565 | `	/* Query result */` |
|        3 |  7566 | `	ph7_result_bool(pCtx,res);` |
|        3 |  7567 | `	return PH7_OK;` |
|        1 |  7568 |  |
|        - |  7569 | `/*` |
|        - |  7570 | ` * bool is_subclass_of(object/string $object,object/string $class_name)` |
|        - |  7571 | ` *   Checks if the object has this class as one of its parents.` |
|        - |  7572 | ` * Parameters` |
|        - |  7573 | ` *  object` |
|        - |  7574 | ` *   The tested object` |
|        - |  7575 | ` * class_name` |
|        - |  7576 | ` *  The class name` |
|        - |  7577 | ` * Return` |
|        - |  7578 | ` *  This function returns TRUE if the object , belongs to a class` |
|        - |  7579 | ` *  which is a subclass of class_name, FALSE otherwise.` |
|        - |  7580 | ` */` |
|        6 |  7581 | `static int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7582 |  |
|        7 |  7583 | `	int res = 0; /* Assume FALSE by default */` |
|        7 |  7584 | `	if( nArg > 1 ){` |
|        - |  7585 | `		ph7_class *pClass,*pMain;` |
|        - |  7586 | `		/* Extract the given classes */` |
|        7 |  7587 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        7 |  7588 | `		pMain = VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|        7 |  7589 | `		if( pClass && pMain ){` |
|        - |  7590 | `			/* Perform the query */` |
|        5 |  7591 | `			res = VmSubclassOf(pClass,pMain);` |
|        2 |  7592 | `		}` |
|        3 |  7593 | `	}` |
|        - |  7594 | `	/* Query result */` |
|        7 |  7595 | `	ph7_result_bool(pCtx,res);` |
|        7 |  7596 | `	return PH7_OK;` |
|        1 |  7597 |  |
|        - |  7598 | `/*` |
|        - |  7599 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  7600 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  7601 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  7602 | ` * return value indicates failure.` |
|        - |  7603 | ` */` |
|      494 |  7604 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  7605 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7606 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  7607 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  7608 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  7609 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  7610 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  7611 | `	)` |
|        2 |  7612 |  |
|        - |  7613 | `	ph7_value *aStack;` |
|        - |  7614 | `	VmInstr aInstr[2];` |
|        - |  7615 | `	int iCursor;` |
|        - |  7616 | `	int i;` |
|        - |  7617 | `	/* Create a new operand stack */` |
|      496 |  7618 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|      496 |  7619 | `	if( aStack == 0 ){` |
|      ! 0 |  7620 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7621 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  7622 | `		return SXERR_MEM;` |
|        - |  7623 | `	}` |
|        - |  7624 | `	/* Fill the operand stack with the given arguments */` |
|      714 |  7625 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      220 |  7626 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7627 | `		/*` |
|        - |  7628 | `		 * Symisc eXtension:` |
|        - |  7629 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7630 | `		 */` |
|      220 |  7631 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      111 |  7632 | `	}` |
|      496 |  7633 | `	iCursor = nArg + 1;` |
|      496 |  7634 | `	if( pThis ){` |
|        - |  7635 | `		/*` |
|        - |  7636 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  7637 | `		 */` |
|      490 |  7638 | `		pThis->iRef++; /* Increment reference count */` |
|      490 |  7639 | `		aStack[i].x.pOther = pThis;` |
|      490 |  7640 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      244 |  7641 | `	}` |
|      496 |  7642 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|      496 |  7643 | `	i++;` |
|        - |  7644 | `	/* Push method name */` |
|      496 |  7645 | `	SyBlobReset(&aStack[i].sBlob);` |
|      496 |  7646 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|      496 |  7647 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|      496 |  7648 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  7649 | `	/* Emit the CALL istruction */` |
|      496 |  7650 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      496 |  7651 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      496 |  7652 | `	aInstr[0].iP2 = 0;` |
|      496 |  7653 | `	aInstr[0].p3  = 0;` |
|        - |  7654 | `	/* Emit the DONE instruction */` |
|      496 |  7655 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      496 |  7656 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|      496 |  7657 | `	aInstr[1].iP2 = 0;` |
|      496 |  7658 | `	aInstr[1].p3  = 0;` |
|        - |  7659 | `	/* Execute the method body (if available) */` |
|      496 |  7660 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  7661 | `	/* Clean up the mess left behind */` |
|      496 |  7662 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      496 |  7663 | `	return PH7_OK;` |
|      249 |  7664 |  |
|        - |  7665 | `/*` |
|        - |  7666 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  7667 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  7668 | ` * in the apArg[] array.` |
|        - |  7669 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7670 | ` * return value indicates failure.` |
|        - |  7671 | ` */` |
|      490 |  7672 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  7673 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7674 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7675 | `	int nArg,          /* Total number of given arguments */` |
|        - |  7676 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  7677 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  7678 | `	)` |
|        2 |  7679 |  |
|        - |  7680 | `	ph7_value *aStack;` |
|        - |  7681 | `	VmInstr aInstr[2];` |
|        - |  7682 | `	int i;` |
|      492 |  7683 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  7684 | `		/* Don't bother processing,it's invalid anyway */` |
|      148 |  7685 | `		if( pResult ){` |
|        - |  7686 | `			/* Assume a null return value */` |
|      ! 0 |  7687 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7688 | `		}` |
|      148 |  7689 | `		return SXERR_INVALID;` |
|        - |  7690 | `	}` |
|      346 |  7691 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7692 | `		/* Class method */` |
|       11 |  7693 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  7694 | `		ph7_class_method *pMethod = 0;` |
|       11 |  7695 | `		ph7_class_instance *pThis = 0;` |
|       11 |  7696 | `		ph7_class *pClass = 0;` |
|        - |  7697 | `		ph7_value *pValue;` |
|        - |  7698 | `		sxi32 rc;` |
|       11 |  7699 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  7700 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  7701 | `			if( pResult ){` |
|        - |  7702 | `				/* Assume a null return value */` |
|      ! 0 |  7703 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7704 | `			}` |
|      ! 0 |  7705 | `			return SXRET_OK;` |
|        - |  7706 | `		}` |
|        - |  7707 | `		/* Extract the class name or an instance of it */` |
|       11 |  7708 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  7709 | `		if( pValue ){` |
|       11 |  7710 | `			pClass = VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  7711 | `		}` |
|       11 |  7712 | `		if( pClass == 0 ){` |
|        - |  7713 | `			/* No such class,return NULL */` |
|      ! 0 |  7714 | `			if( pResult ){` |
|      ! 0 |  7715 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7716 | `			}` |
|      ! 0 |  7717 | `			return SXRET_OK;` |
|        - |  7718 | `		}` |
|       11 |  7719 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  7720 | `			/* Point to the class instance */` |
|        5 |  7721 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  7722 | `		}` |
|        - |  7723 | `		/* Try to extract the method */` |
|       11 |  7724 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  7725 | `		if( pValue ){` |
|       11 |  7726 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  7727 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  7728 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  7729 | `			}` |
|        5 |  7730 | `		}` |
|       11 |  7731 | `		if( pMethod == 0 ){` |
|        - |  7732 | `			/* No such method,return NULL */` |
|      ! 0 |  7733 | `			if( pResult ){` |
|      ! 0 |  7734 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7735 | `			}` |
|      ! 0 |  7736 | `			return SXRET_OK;` |
|        - |  7737 | `		}` |
|        - |  7738 | `		/* Call the class method */` |
|       11 |  7739 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  7740 | `		return rc;` |
|        - |  7741 | `	}` |
|        - |  7742 | `	/* Create a new operand stack */` |
|      336 |  7743 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      336 |  7744 | `	if( aStack == 0 ){` |
|      ! 0 |  7745 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7746 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  7747 | `		if( pResult ){` |
|        - |  7748 | `			/* Assume a null return value */` |
|      ! 0 |  7749 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7750 | `		}` |
|      ! 0 |  7751 | `		return SXERR_MEM;` |
|        - |  7752 | `	}` |
|        - |  7753 | `	/* Fill the operand stack with the given arguments */` |
|     1050 |  7754 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      716 |  7755 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7756 | `		/*` |
|        - |  7757 | `		 * Symisc eXtension:` |
|        - |  7758 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7759 | `		 */` |
|      716 |  7760 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      359 |  7761 | `	}` |
|        - |  7762 | `	/* Push the function name */` |
|      336 |  7763 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      336 |  7764 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7765 | `	/* Emit the CALL istruction */` |
|      336 |  7766 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      336 |  7767 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      336 |  7768 | `	aInstr[0].iP2 = 0;` |
|      336 |  7769 | `	aInstr[0].p3  = 0;` |
|        - |  7770 | `	/* Emit the DONE instruction */` |
|      336 |  7771 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      336 |  7772 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      336 |  7773 | `	aInstr[1].iP2 = 0;` |
|      336 |  7774 | `	aInstr[1].p3  = 0;` |
|        - |  7775 | `	/* Execute the function body (if available) */` |
|      336 |  7776 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  7777 | `	/* Clean up the mess left behind */` |
|      336 |  7778 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      336 |  7779 | `	return PH7_OK;` |
|      247 |  7780 |  |
|        - |  7781 | `/*` |
|        - |  7782 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  7783 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  7784 | ` * parameter.` |
|        - |  7785 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7786 | ` * return value indicates failure.` |
|        - |  7787 | ` */` |
|      190 |  7788 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  7789 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7790 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7791 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  7792 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  7793 | `	)` |
|        1 |  7794 |  |
|        - |  7795 | `	ph7_value *pArg;` |
|        - |  7796 | `	SySet aArg;` |
|        - |  7797 | `	va_list ap;` |
|        - |  7798 | `	sxi32 rc;` |
|      191 |  7799 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7800 | `	/* Copy arguments one after one */` |
|      191 |  7801 | `	va_start(ap,pResult);` |
|      319 |  7802 | `	for(;;){` |
|      639 |  7803 | `		pArg = va_arg(ap,ph7_value *);` |
|      639 |  7804 | `		if( pArg == 0 ){` |
|      191 |  7805 | `			break;` |
|        - |  7806 | `		}` |
|      449 |  7807 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  7808 | `	}` |
|        - |  7809 | `	/* Call the core routine */` |
|      191 |  7810 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  7811 | `	/* Cleanup */` |
|      191 |  7812 | `	SySetRelease(&aArg);` |
|      191 |  7813 | `	return rc;` |
|        1 |  7814 |  |
|        - |  7815 | `/*` |
|        - |  7816 | ` * value call_user_func(callable $callback[,value $parameter[, value $... ]])` |
|        - |  7817 | ` *  Call the callback given by the first parameter.` |
|        - |  7818 | ` * Parameter` |
|        - |  7819 | ` *  $callback` |
|        - |  7820 | ` *   The callable to be called.` |
|        - |  7821 | ` *  ...` |
|        - |  7822 | ` *    Zero or more parameters to be passed to the callback.` |
|        - |  7823 | ` * Return` |
|        - |  7824 | ` *  Th return value of the callback, or FALSE on error.` |
|        - |  7825 | ` */` |
|       14 |  7826 | `static int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7827 |  |
|        - |  7828 | `	ph7_value sResult; /* Store callback return value here */` |
|        - |  7829 | `	sxi32 rc;` |
|       15 |  7830 | `	if( nArg < 1 ){` |
|        - |  7831 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  7832 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7833 | `		return PH7_OK;` |
|        - |  7834 | `	}` |
|       15 |  7835 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|       15 |  7836 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7837 | `	/* Try to invoke the callback */` |
|       15 |  7838 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|       15 |  7839 | `	if( rc != SXRET_OK ){` |
|        - |  7840 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|      ! 0 |  7841 | `		ph7_result_bool(pCtx,0); /* return false */` |
|      ! 0 |  7842 | `	}else{` |
|        - |  7843 | `		/* Callback result */` |
|       15 |  7844 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        - |  7845 | `	}` |
|       15 |  7846 | `	PH7_MemObjRelease(&sResult);` |
|       15 |  7847 | `	return PH7_OK;` |
|        8 |  7848 |  |
|        - |  7849 | `/*` |
|        - |  7850 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|        - |  7851 | ` *  Call a callback with an array of parameters.` |
|        - |  7852 | ` * Parameter` |
|        - |  7853 | ` *  $callback` |
|        - |  7854 | ` *   The callable to be called.` |
|        - |  7855 | ` * $param_arr` |
|        - |  7856 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|        - |  7857 | ` * Return` |
|        - |  7858 | ` *  Returns the return value of the callback, or FALSE on error.` |
|        - |  7859 | ` */` |
|       10 |  7860 | `static int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7861 |  |
|        - |  7862 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|        - |  7863 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|        - |  7864 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|        - |  7865 | `	SySet aArg;               /* Arguments containers */` |
|        - |  7866 | `	sxi32 rc;` |
|        - |  7867 | `	sxu32 n;` |
|       11 |  7868 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|        - |  7869 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  7870 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7871 | `		return PH7_OK;` |
|        - |  7872 | `	}` |
|       11 |  7873 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|       11 |  7874 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7875 | `	/* Initialize the arguments container */` |
|       11 |  7876 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7877 | `	/* Turn hashmap entries into callback arguments */` |
|       11 |  7878 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       11 |  7879 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|       23 |  7880 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        - |  7881 | `		/* Extract node value */` |
|       13 |  7882 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|       13 |  7883 | `			SySetPut(&aArg,(const void *)&pValue);` |
|        6 |  7884 | `		}` |
|        - |  7885 | `		/* Point to the next entry */` |
|       13 |  7886 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        7 |  7887 | `	}` |
|        - |  7888 | `	/* Try to invoke the callback */` |
|       11 |  7889 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|       11 |  7890 | `	if( rc != SXRET_OK ){` |
|        - |  7891 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|      ! 0 |  7892 | `		ph7_result_bool(pCtx,0); /* return false */` |
|      ! 0 |  7893 | `	}else{` |
|        - |  7894 | `		/* Callback result */` |
|       11 |  7895 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        - |  7896 | `	}` |
|        - |  7897 | `	/* Cleanup the mess left behind */` |
|       11 |  7898 | `	PH7_MemObjRelease(&sResult);` |
|       11 |  7899 | `	SySetRelease(&aArg);` |
|       11 |  7900 | `	return PH7_OK;` |
|        6 |  7901 |  |
|        - |  7902 | `/*` |
|        - |  7903 | ` * bool defined(string $name)` |
|        - |  7904 | ` *  Checks whether a given named constant exists.` |
|        - |  7905 | ` * Parameter:` |
|        - |  7906 | ` *  Name of the desired constant.` |
|        - |  7907 | ` * Return` |
|        - |  7908 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  7909 | ` */` |
|       14 |  7910 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7911 |  |
|        - |  7912 | `	const char *zName;` |
|       16 |  7913 | `	int nLen = 0;` |
|       16 |  7914 | `	int res = 0;` |
|       16 |  7915 | `	if( nArg < 1 ){` |
|        - |  7916 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  7917 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  7918 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7919 | `		return SXRET_OK;` |
|        - |  7920 | `	}` |
|        - |  7921 | `	/* Extract constant name */` |
|       16 |  7922 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7923 | `	/* Perform the lookup */` |
|       16 |  7924 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7925 | `		/* Already defined */` |
|       10 |  7926 | `		res = 1;` |
|        4 |  7927 | `	}` |
|       16 |  7928 | `	ph7_result_bool(pCtx,res);` |
|       16 |  7929 | `	return SXRET_OK;` |
|        9 |  7930 |  |
|        - |  7931 | `/*` |
|        - |  7932 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  7933 | ` * below.` |
|        - |  7934 | ` */` |
|        8 |  7935 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  7936 |  |
|       10 |  7937 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  7938 | `	/* Expand constant value */` |
|       10 |  7939 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7940 |  |
|        - |  7941 | `/*` |
|        - |  7942 | ` * bool define(string $constant_name,expression value)` |
|        - |  7943 | ` *  Defines a named constant at runtime.` |
|        - |  7944 | ` * Parameter:` |
|        - |  7945 | ` *  $constant_name` |
|        - |  7946 | ` *   The name of the constant` |
|        - |  7947 | ` *  $value` |
|        - |  7948 | ` *   Constant value` |
|        - |  7949 | ` * Return:` |
|        - |  7950 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7951 | ` */` |
|       10 |  7952 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7953 |  |
|        - |  7954 | `	const char *zName;  /* Constant name */` |
|        - |  7955 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7956 | `	int nLen = 0;       /* Name length */` |
|        - |  7957 | `	sxi32 rc;` |
|       12 |  7958 | `	if( nArg < 2 ){` |
|        - |  7959 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7960 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7961 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7962 | `		return SXRET_OK;` |
|        - |  7963 | `	}` |
|       12 |  7964 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7965 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7966 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7967 | `		return SXRET_OK;` |
|        - |  7968 | `	}` |
|        - |  7969 | `	/* Extract constant name */` |
|       12 |  7970 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7971 | `	if( nLen < 1 ){` |
|      ! 0 |  7972 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7973 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7974 | `		return SXRET_OK;` |
|        - |  7975 | `	}` |
|        - |  7976 | `	/* Duplicate constant value */` |
|       12 |  7977 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7978 | `	if( pValue == 0 ){` |
|      ! 0 |  7979 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7980 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7981 | `		return SXRET_OK;` |
|        - |  7982 | `	}` |
|        - |  7983 | `	/* Initialize the memory object */` |
|       12 |  7984 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7985 | `	/* Register the constant */` |
|       12 |  7986 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7987 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7988 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7989 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7990 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7991 | `		return SXRET_OK;` |
|        - |  7992 | `	}` |
|        - |  7993 | `	/* Duplicate constant value */` |
|       12 |  7994 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7995 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7996 | `		/* Lower case the constant name */` |
|      ! 0 |  7997 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7998 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7999 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  8000 | `				/* UTF-8 stream */` |
|      ! 0 |  8001 | `				zCur++;` |
|      ! 0 |  8002 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  8003 | `					zCur++;` |
|      ! 0 |  8004 | `				}` |
|      ! 0 |  8005 | `				continue;` |
|        - |  8006 | `			}` |
|      ! 0 |  8007 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  8008 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  8009 | `				zCur[0] = (char)c;` |
|      ! 0 |  8010 | `			}` |
|      ! 0 |  8011 | `			zCur++;` |
|      ! 0 |  8012 | `		}` |
|        - |  8013 | `		/* Finally,register the constant */` |
|      ! 0 |  8014 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  8015 | `	}` |
|        - |  8016 | `	/* All done,return TRUE */` |
|       12 |  8017 | `	ph7_result_bool(pCtx,1);` |
|       12 |  8018 | `	return SXRET_OK;` |
|        7 |  8019 |  |
|        - |  8020 | `/*` |
|        - |  8021 | ` * value constant(string $name)` |
|        - |  8022 | ` *  Returns the value of a constant` |
|        - |  8023 | ` * Parameter` |
|        - |  8024 | ` *  $name` |
|        - |  8025 | ` *    Name of the constant.` |
|        - |  8026 | ` * Return` |
|        - |  8027 | ` *  Constant value or NULL if not defined.` |
|        - |  8028 | ` */` |
|        8 |  8029 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8030 |  |
|        - |  8031 | `	SyHashEntry *pEntry;` |
|        - |  8032 | `	ph7_constant *pCons;` |
|        - |  8033 | `	const char *zName; /* Constant name */` |
|        - |  8034 | `	ph7_value sVal;    /* Constant value */` |
|        - |  8035 | `	int nLen;` |
|       10 |  8036 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  8037 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  8038 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  8039 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8040 | `		return SXRET_OK;` |
|        - |  8041 | `	}` |
|        - |  8042 | `	/* Extract the constant name */` |
|       10 |  8043 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8044 | `	/* Perform the query */` |
|       10 |  8045 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  8046 | `	if( pEntry == 0 ){` |
|        3 |  8047 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  8048 | `		ph7_result_null(pCtx);` |
|        3 |  8049 | `		return SXRET_OK;` |
|        - |  8050 | `	}` |
|        8 |  8051 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  8052 | `	/* Point to the structure that describe the constant */` |
|        8 |  8053 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  8054 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  8055 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  8056 | `	/* Return that value */` |
|        8 |  8057 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  8058 | `	/* Cleanup */` |
|        8 |  8059 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  8060 | `	return SXRET_OK;` |
|        6 |  8061 |  |
|        - |  8062 | `/*` |
|        - |  8063 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  8064 | ` * defined below.` |
|        - |  8065 | ` */` |
|      414 |  8066 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8067 |  |
|      415 |  8068 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8069 | `	ph7_value sName;` |
|        - |  8070 | `	sxi32 rc;` |
|        - |  8071 | `	/* Prepare the constant name for insertion */` |
|      415 |  8072 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      415 |  8073 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8074 | `	/* Perform the insertion */` |
|      415 |  8075 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      415 |  8076 | `	PH7_MemObjRelease(&sName);` |
|      415 |  8077 | `	return rc;` |
|        1 |  8078 |  |
|        - |  8079 | `/*` |
|        - |  8080 | ` * array get_defined_constants(void)` |
|        - |  8081 | ` *  Returns an associative array with the names of all defined` |
|        - |  8082 | ` *  constants.` |
|        - |  8083 | ` * Parameters` |
|        - |  8084 | ` *  NONE.` |
|        - |  8085 | ` * Returns` |
|        - |  8086 | ` *  Returns the names of all the constants currently defined.` |
|        - |  8087 | ` */` |
|        2 |  8088 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8089 |  |
|        - |  8090 | `	ph7_value *pArray;` |
|        - |  8091 | `	/* Create the array first*/` |
|        3 |  8092 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8093 | `	if( pArray == 0 ){` |
|      ! 0 |  8094 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8095 | `		SXUNUSED(apArg);` |
|        - |  8096 | `		/* Return NULL */` |
|      ! 0 |  8097 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8098 | `		return SXRET_OK;` |
|        - |  8099 | `	}` |
|        - |  8100 | `	/* Fill the array with the defined constants */` |
|        3 |  8101 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  8102 | `	/* Return the created array */` |
|        3 |  8103 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8104 | `	return SXRET_OK;` |
|        2 |  8105 |  |
|        - |  8106 | `/*` |
|        - |  8107 | ` * Section:` |
|        - |  8108 | ` *  Output Control (OB) functions.` |
|        - |  8109 | ` * Status:` |
|        - |  8110 | ` *    Stable.` |
|        - |  8111 | ` */` |
|        - |  8112 | `/* Forward declaration */` |
|        - |  8113 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry);` |
|        - |  8114 | `/*` |
|        - |  8115 | ` * void ob_clean(void)` |
|        - |  8116 | ` *  This function discards the contents of the output buffer.` |
|        - |  8117 | ` *  This function does not destroy the output buffer like ob_end_clean() does.` |
|        - |  8118 | ` * Parameter` |
|        - |  8119 | ` *  None` |
|        - |  8120 | ` * Return` |
|        - |  8121 | ` *  No value is returned.` |
|        - |  8122 | ` */` |
|        2 |  8123 | `static int vm_builtin_ob_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8124 |  |
|        3 |  8125 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8126 | `	VmObEntry *pOb;` |
|        1 |  8127 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8128 | `	SXUNUSED(apArg);` |
|        - |  8129 | `	/* Peek the top most OB */` |
|        3 |  8130 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8131 | `	if( pOb ){` |
|        3 |  8132 | `		SyBlobRelease(&pOb->sOB);` |
|        1 |  8133 | `	}` |
|        3 |  8134 | `	return PH7_OK;` |
|        1 |  8135 |  |
|        - |  8136 | `/*` |
|        - |  8137 | ` * bool ob_end_clean(void)` |
|        - |  8138 | ` *  Clean (erase) the output buffer and turn off output buffering` |
|        - |  8139 | ` *  This function discards the contents of the topmost output buffer and turns` |
|        - |  8140 | ` *  off this output buffering. If you want to further process the buffer's contents` |
|        - |  8141 | ` *  you have to call ob_get_contents() before ob_end_clean() as the buffer contents` |
|        - |  8142 | ` *  are discarded when ob_end_clean() is called.` |
|        - |  8143 | ` * Parameter` |
|        - |  8144 | ` *  None` |
|        - |  8145 | ` * Return` |
|        - |  8146 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first that you called` |
|        - |  8147 | ` *  the function without an active buffer or that for some reason a buffer could not be deleted` |
|        - |  8148 | ` * (possible for special buffer)` |
|        - |  8149 | ` */` |
|     2726 |  8150 | `static int vm_builtin_ob_end_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8151 |  |
|     2728 |  8152 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8153 | `	VmObEntry *pOb;` |
|        - |  8154 | `	/* Pop the top most OB */` |
|     2728 |  8155 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|     2728 |  8156 | `	if( pOb == 0){` |
|        - |  8157 | `		/* No such OB,return FALSE */` |
|      ! 0 |  8158 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8159 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8160 | `		SXUNUSED(apArg);` |
|      ! 0 |  8161 | `	}else{` |
|        - |  8162 | `		/* Release */` |
|     2728 |  8163 | `		VmObRestore(pVm,pOb);` |
|        - |  8164 | `		/* Return true */` |
|     2728 |  8165 | `		ph7_result_bool(pCtx,1);` |
|        - |  8166 | `	}` |
|     2728 |  8167 | `	return PH7_OK;` |
|        2 |  8168 |  |
|        - |  8169 | `/*` |
|        - |  8170 | ` * string ob_get_contents(void)` |
|        - |  8171 | ` *  Gets the contents of the output buffer without clearing it.` |
|        - |  8172 | ` * Parameter` |
|        - |  8173 | ` *  None` |
|        - |  8174 | ` * Return` |
|        - |  8175 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|        - |  8176 | ` */` |
|        6 |  8177 | `static int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8178 |  |
|        7 |  8179 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8180 | `	VmObEntry *pOb;` |
|        - |  8181 | `	/* Peek the top most OB */` |
|        7 |  8182 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        7 |  8183 | `	if( pOb == 0 ){` |
|        - |  8184 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8185 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8186 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8187 | `		SXUNUSED(apArg);` |
|      ! 0 |  8188 | `	}else{` |
|        - |  8189 | `		/* Return contents */` |
|        7 |  8190 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB));` |
|        - |  8191 | `	}` |
|        7 |  8192 | `	return PH7_OK;` |
|        1 |  8193 |  |
|        - |  8194 | `/*` |
|        - |  8195 | ` * string ob_get_clean(void)` |
|        - |  8196 | ` * string ob_get_flush(void)` |
|        - |  8197 | ` *  Get current buffer contents and delete current output buffer.` |
|        - |  8198 | ` * Parameter` |
|        - |  8199 | ` *  None` |
|        - |  8200 | ` * Return` |
|        - |  8201 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|        - |  8202 | ` */` |
|     3956 |  8203 | `static int vm_builtin_ob_get_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8204 |  |
|     3958 |  8205 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8206 | `	VmObEntry *pOb;` |
|        - |  8207 | `	/* Pop the top most OB */` |
|     3958 |  8208 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|     3958 |  8209 | `	if( pOb == 0 ){` |
|        - |  8210 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8211 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8212 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8213 | `		SXUNUSED(apArg);` |
|      ! 0 |  8214 | `	}else{` |
|        - |  8215 | `		/* Return contents */` |
|     3958 |  8216 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB)); /* Will make it's own copy */` |
|        - |  8217 | `		/* Release */` |
|     3958 |  8218 | `		VmObRestore(pVm,pOb);` |
|        - |  8219 | `	}` |
|     3958 |  8220 | `	return PH7_OK;` |
|        2 |  8221 |  |
|        - |  8222 | `/*` |
|        - |  8223 | ` * int ob_get_length(void)` |
|        - |  8224 | ` *  Return the length of the output buffer.` |
|        - |  8225 | ` * Parameter` |
|        - |  8226 | ` *  None` |
|        - |  8227 | ` * Return` |
|        - |  8228 | ` *  Returns the length of the output buffer contents or FALSE if no buffering is active.` |
|        - |  8229 | ` */` |
|        2 |  8230 | `static int vm_builtin_ob_get_length(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8231 |  |
|        3 |  8232 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8233 | `	VmObEntry *pOb;` |
|        - |  8234 | `	/* Peek the top most OB */` |
|        3 |  8235 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8236 | `	if( pOb == 0 ){` |
|        - |  8237 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8238 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8239 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8240 | `		SXUNUSED(apArg);` |
|      ! 0 |  8241 | `	}else{` |
|        - |  8242 | `		/* Return OB length */` |
|        3 |  8243 | `		ph7_result_int64(pCtx,(ph7_int64)SyBlobLength(&pOb->sOB));` |
|        - |  8244 | `	}` |
|        3 |  8245 | `	return PH7_OK;` |
|        1 |  8246 |  |
|        - |  8247 | `/*` |
|        - |  8248 | ` * int ob_get_level(void)` |
|        - |  8249 | ` *  Returns the nesting level of the output buffering mechanism.` |
|        - |  8250 | ` * Parameter` |
|        - |  8251 | ` *  None` |
|        - |  8252 | ` * Return` |
|        - |  8253 | ` *  Returns the level of nested output buffering handlers or zero if output buffering is not active.` |
|        - |  8254 | ` */` |
|        6 |  8255 | `static int vm_builtin_ob_get_level(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8256 |  |
|        7 |  8257 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8258 | `	int iNest;` |
|        3 |  8259 | `	SXUNUSED(nArg); /* cc warning */` |
|        3 |  8260 | `	SXUNUSED(apArg);` |
|        - |  8261 | `	/* Nesting level */` |
|        7 |  8262 | `	iNest = (int)SySetUsed(&pVm->aOB);` |
|        - |  8263 | `	/* Return the nesting value */` |
|        7 |  8264 | `	ph7_result_int(pCtx,iNest);` |
|        7 |  8265 | `	return PH7_OK;` |
|        1 |  8266 |  |
|        - |  8267 | `/*` |
|        - |  8268 | ` * Output Buffer(OB) default VM consumer routine.All VM output is now redirected` |
|        - |  8269 | ` * to a stackable internal buffer,until the user call [ob_get_clean(),ob_end_clean(),...].` |
|        - |  8270 | ` * Refer to the implementation of [ob_start()] for more information.` |
|        - |  8271 | ` */` |
|     5938 |  8272 | `static int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData)` |
|        2 |  8273 |  |
|     5940 |  8274 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|        - |  8275 | `	VmObEntry *pEntry;` |
|        - |  8276 | `	ph7_value sResult;` |
|        - |  8277 | `	/* Peek the top most entry */` |
|     5940 |  8278 | `	pEntry = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|     5940 |  8279 | `	if( pEntry == 0 ){` |
|        - |  8280 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8281 | `		return PH7_OK;` |
|        - |  8282 | `	}` |
|     5940 |  8283 | `	PH7_MemObjInit(pVm,&sResult);` |
|     5940 |  8284 | `	if( ph7_value_is_callable(&pEntry->sCallback) && pVm->nObDepth < 15 ){` |
|        - |  8285 | `		ph7_value sArg,*apArg[2];` |
|        - |  8286 | `		/* Fill the first argument */` |
|      ! 0 |  8287 | `		PH7_MemObjInitFromString(pVm,&sArg,0);` |
|      ! 0 |  8288 | `		PH7_MemObjStringAppend(&sArg,(const char *)pData,nDataLen);` |
|      ! 0 |  8289 | `		apArg[0] = &sArg;` |
|        - |  8290 | `		/* Call the 'filter' callback */` |
|      ! 0 |  8291 | `		pVm->nObDepth++;` |
|      ! 0 |  8292 | `		PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult);` |
|      ! 0 |  8293 | `		pVm->nObDepth--;` |
|      ! 0 |  8294 | `		if( sResult.iFlags & MEMOBJ_STRING ){` |
|        - |  8295 | `			/* Extract the function result */` |
|      ! 0 |  8296 | `			pData = SyBlobData(&sResult.sBlob);` |
|      ! 0 |  8297 | `			nDataLen = SyBlobLength(&sResult.sBlob);` |
|      ! 0 |  8298 | `		}` |
|      ! 0 |  8299 | `		PH7_MemObjRelease(&sArg);` |
|      ! 0 |  8300 | `	}` |
|     5940 |  8301 | `	if( nDataLen > 0 ){` |
|        - |  8302 | `		/* Redirect the VM output to the internal buffer */` |
|     5940 |  8303 | `		SyBlobAppend(&pEntry->sOB,pData,nDataLen);` |
|     2969 |  8304 | `	}` |
|        - |  8305 | `	/* Release */` |
|     5940 |  8306 | `	PH7_MemObjRelease(&sResult);` |
|     5940 |  8307 | `	return PH7_OK;` |
|     2971 |  8308 |  |
|        - |  8309 | `/*` |
|        - |  8310 | ` * Restore the default consumer.` |
|        - |  8311 | ` * Refer to the implementation of [ob_end_clean()] for more` |
|        - |  8312 | ` * information.` |
|        - |  8313 | ` */` |
|     6684 |  8314 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry)` |
|        2 |  8315 |  |
|     6686 |  8316 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|     6686 |  8317 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|        - |  8318 | `		/* No more stackable OB */` |
|     6668 |  8319 | `		pCons->xConsumer = pCons->xDef;` |
|     6668 |  8320 | `		pCons->pUserData = pCons->pDefData;` |
|     3333 |  8321 | `	}` |
|        - |  8322 | `	/* Release OB data */` |
|     6686 |  8323 | `	PH7_MemObjRelease(&pEntry->sCallback);` |
|     6686 |  8324 | `	SyBlobRelease(&pEntry->sOB);` |
|     6686 |  8325 |  |
|        - |  8326 | `/*` |
|        - |  8327 | ` * bool ob_start([ callback $output_callback] )` |
|        - |  8328 | ` * This function will turn output buffering on. While output buffering is active no output` |
|        - |  8329 | ` *  is sent from the script (other than headers), instead the output is stored in an internal` |
|        - |  8330 | ` *  buffer.` |
|        - |  8331 | ` * Parameter` |
|        - |  8332 | ` *  $output_callback` |
|        - |  8333 | ` *   An optional output_callback function may be specified. This function takes a string` |
|        - |  8334 | ` *   as a parameter and should return a string. The function will be called when the output` |
|        - |  8335 | ` *   buffer is flushed (sent) or cleaned (with ob_flush(), ob_clean() or similar function)` |
|        - |  8336 | ` *   or when the output buffer is flushed to the browser at the end of the request.` |
|        - |  8337 | ` *   When output_callback is called, it will receive the contents of the output buffer` |
|        - |  8338 | ` *   as its parameter and is expected to return a new output buffer as a result, which will` |
|        - |  8339 | ` *   be sent to the browser. If the output_callback is not a callable function, this function` |
|        - |  8340 | ` *   will return FALSE.` |
|        - |  8341 | ` *   If the callback function has two parameters, the second parameter is filled with` |
|        - |  8342 | ` *   a bit-field consisting of PHP_OUTPUT_HANDLER_START, PHP_OUTPUT_HANDLER_CONT` |
|        - |  8343 | ` *   and PHP_OUTPUT_HANDLER_END.` |
|        - |  8344 | ` *   If output_callback returns FALSE original input is sent to the browser.` |
|        - |  8345 | ` *   The output_callback parameter may be bypassed by passing a NULL value.` |
|        - |  8346 | ` * Return` |
|        - |  8347 | ` *   Returns TRUE on success or FALSE on failure.` |
|        - |  8348 | ` */` |
|     6684 |  8349 | `static int vm_builtin_ob_start(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8350 |  |
|     6686 |  8351 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8352 | `	VmObEntry sOb;` |
|        - |  8353 | `	sxi32 rc;` |
|        - |  8354 | `	/* Initialize the OB entry */` |
|     6686 |  8355 | `	PH7_MemObjInit(pCtx->pVm,&sOb.sCallback);` |
|     6686 |  8356 | `	SyBlobInit(&sOb.sOB,&pVm->sAllocator);` |
|     6686 |  8357 | `	if( nArg > 0 && (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) ){` |
|        - |  8358 | `		/* Save the callback name for later invocation */` |
|      ! 0 |  8359 | `		PH7_MemObjStore(apArg[0],&sOb.sCallback);` |
|      ! 0 |  8360 | `	}` |
|        - |  8361 | `	/* Push in the stack */` |
|     6686 |  8362 | `	rc = SySetPut(&pVm->aOB,(const void *)&sOb);` |
|     6686 |  8363 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8364 | `		PH7_MemObjRelease(&sOb.sCallback);` |
|      ! 0 |  8365 | `	}else{` |
|     6686 |  8366 | `		ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        - |  8367 | `		/* Substitute the default VM consumer */` |
|     6686 |  8368 | `		if( pCons->xConsumer != VmObConsumer ){` |
|     6668 |  8369 | `			pCons->xDef = pCons->xConsumer;` |
|     6668 |  8370 | `			pCons->pDefData = pCons->pUserData;` |
|        - |  8371 | `			/* Install the new consumer */` |
|     6668 |  8372 | `			pCons->xConsumer = VmObConsumer;` |
|     6668 |  8373 | `			pCons->pUserData = pVm;` |
|     3333 |  8374 | `		}` |
|        - |  8375 | `	}` |
|     6686 |  8376 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     6686 |  8377 | `	return PH7_OK;` |
|        2 |  8378 |  |
|        - |  8379 | `/*` |
|        - |  8380 | ` * Flush Output buffer to the default VM output consumer.` |
|        - |  8381 | ` * Refer to the implementation of [ob_flush()] for more` |
|        - |  8382 | ` * information.` |
|        - |  8383 | ` */` |
|        4 |  8384 | `static sxi32 VmObFlush(ph7_vm *pVm,VmObEntry *pEntry,int bRelease)` |
|        1 |  8385 |  |
|        5 |  8386 | `	SyBlob *pBlob = &pEntry->sOB;` |
|        - |  8387 | `	sxi32 rc;` |
|        - |  8388 | `	/* Flush contents */` |
|        5 |  8389 | `	rc = PH7_OK;` |
|        5 |  8390 | `	if( SyBlobLength(pBlob) > 0 ){` |
|        - |  8391 | `		/* Call the VM output consumer */` |
|        5 |  8392 | `		rc = pVm->sVmConsumer.xDef(SyBlobData(pBlob),SyBlobLength(pBlob),pVm->sVmConsumer.pDefData);` |
|        - |  8393 | `		/* Increment VM output counter */` |
|        5 |  8394 | `		pVm->nOutputLen += SyBlobLength(pBlob);` |
|        5 |  8395 | `		if( rc != PH7_ABORT ){` |
|        5 |  8396 | `			rc = PH7_OK;` |
|        2 |  8397 | `		}` |
|        2 |  8398 | `	}` |
|        5 |  8399 | `	if( bRelease ){` |
|        3 |  8400 | `		VmObRestore(&(*pVm),pEntry);` |
|        2 |  8401 | `	}else{` |
|        - |  8402 | `		/* Reset the blob */` |
|        3 |  8403 | `		SyBlobReset(pBlob);` |
|        - |  8404 | `	}` |
|        5 |  8405 | `	return rc;` |
|        1 |  8406 |  |
|        - |  8407 | `/*` |
|        - |  8408 | ` * void ob_flush(void)` |
|        - |  8409 | ` * void flush(void)` |
|        - |  8410 | ` *  Flush (send) the output buffer.` |
|        - |  8411 | ` * Parameter` |
|        - |  8412 | ` *  None` |
|        - |  8413 | ` * Return` |
|        - |  8414 | ` *  No return value.` |
|        - |  8415 | ` */` |
|        2 |  8416 | `static int vm_builtin_ob_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8417 |  |
|        3 |  8418 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8419 | `	VmObEntry *pOb;` |
|        - |  8420 | `	sxi32 rc;` |
|        - |  8421 | `	/* Peek the top most OB entry */` |
|        3 |  8422 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8423 | `	if( pOb == 0 ){` |
|        - |  8424 | `		/* Empty stack,return immediately */` |
|      ! 0 |  8425 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8426 | `		SXUNUSED(apArg);` |
|      ! 0 |  8427 | `		return PH7_OK;` |
|        - |  8428 | `	}` |
|        - |  8429 | `	/* Flush contents */` |
|        3 |  8430 | `	rc = VmObFlush(pVm,pOb,FALSE);` |
|        3 |  8431 | `	return rc;` |
|        2 |  8432 |  |
|        - |  8433 | `/*` |
|        - |  8434 | ` * bool ob_end_flush(void)` |
|        - |  8435 | ` *  Flush (send) the output buffer and turn off output buffering.` |
|        - |  8436 | ` * Parameter` |
|        - |  8437 | ` *  None` |
|        - |  8438 | ` * Return` |
|        - |  8439 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first` |
|        - |  8440 | ` *  that you called the function without an active buffer or that for some reason` |
|        - |  8441 | ` *  a buffer could not be deleted (possible for special buffer).` |
|        - |  8442 | ` */` |
|        2 |  8443 | `static int vm_builtin_ob_end_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8444 |  |
|        3 |  8445 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8446 | `	VmObEntry *pOb;` |
|        - |  8447 | `	sxi32 rc;` |
|        - |  8448 | `	/* Pop the top most OB entry */` |
|        3 |  8449 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|        3 |  8450 | `	if( pOb == 0 ){` |
|        - |  8451 | `		/* Empty stack,return FALSE */` |
|      ! 0 |  8452 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8453 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8454 | `		SXUNUSED(apArg);` |
|      ! 0 |  8455 | `		return PH7_OK;` |
|        - |  8456 | `	}` |
|        - |  8457 | `	/* Flush contents */` |
|        3 |  8458 | `	rc = VmObFlush(pVm,pOb,TRUE);` |
|        - |  8459 | `	/* Return true */` |
|        3 |  8460 | `	ph7_result_bool(pCtx,1);` |
|        3 |  8461 | `	return rc;` |
|        2 |  8462 |  |
|        - |  8463 | `/*` |
|        - |  8464 | ` * void ob_implicit_flush([int $flag = true ])` |
|        - |  8465 | ` *  ob_implicit_flush() will turn implicit flushing on or off.` |
|        - |  8466 | ` *  Implicit flushing will result in a flush operation after every` |
|        - |  8467 | ` *  output call, so that explicit calls to flush() will no longer be needed.` |
|        - |  8468 | ` * Parameter` |
|        - |  8469 | ` *  $flag` |
|        - |  8470 | ` *   TRUE to turn implicit flushing on, FALSE otherwise.` |
|        - |  8471 | ` * Return` |
|        - |  8472 | ` *   Nothing` |
|        - |  8473 | ` */` |
|        4 |  8474 | `static int vm_builtin_ob_implicit_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8475 |  |
|        - |  8476 | `	/* NOTE: As of this version,this function is a no-op.` |
|        - |  8477 | `	 * PH7 is smart enough to flush it's internal buffer when appropriate.` |
|        - |  8478 | `	 */` |
|        2 |  8479 | `	SXUNUSED(pCtx);` |
|        2 |  8480 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  8481 | `	SXUNUSED(apArg);` |
|        5 |  8482 | `	return PH7_OK;` |
|        1 |  8483 |  |
|        - |  8484 | `/*` |
|        - |  8485 | ` * array ob_list_handlers(void)` |
|        - |  8486 | ` *  Lists all output handlers in use.` |
|        - |  8487 | ` * Parameter` |
|        - |  8488 | ` *  None` |
|        - |  8489 | ` * Return` |
|        - |  8490 | ` *  This will return an array with the output handlers in use (if any).` |
|        - |  8491 | ` */` |
|        2 |  8492 | `static int vm_builtin_ob_list_handlers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8493 |  |
|        3 |  8494 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8495 | `	ph7_value *pArray;` |
|        - |  8496 | `	VmObEntry *aEntry;` |
|        - |  8497 | `	ph7_value sVal;` |
|        - |  8498 | `	sxu32 n;` |
|        3 |  8499 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|        - |  8500 | `		/* Empty stack,return null */` |
|      ! 0 |  8501 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8502 | `		return PH7_OK;` |
|        - |  8503 | `	}` |
|        - |  8504 | `	/* Create a new array */` |
|        3 |  8505 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8506 | `	if( pArray == 0 ){` |
|        - |  8507 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8508 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8509 | `		SXUNUSED(apArg);` |
|      ! 0 |  8510 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8511 | `		return PH7_OK;` |
|        - |  8512 | `	}` |
|        3 |  8513 | `	PH7_MemObjInit(pVm,&sVal);` |
|        - |  8514 | `	/* Point to the installed OB entries */` |
|        3 |  8515 | `	aEntry = (VmObEntry *)SySetBasePtr(&pVm->aOB);` |
|        - |  8516 | `	/* Perform the requested operation */` |
|        5 |  8517 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; n++ ){` |
|        3 |  8518 | `		VmObEntry *pEntry = &aEntry[n];` |
|        - |  8519 | `		/* Extract handler name */` |
|        3 |  8520 | `		SyBlobReset(&sVal.sBlob);` |
|        3 |  8521 | `		if( pEntry->sCallback.iFlags & MEMOBJ_STRING ){` |
|        - |  8522 | `			/* Callback,dup it's name */` |
|      ! 0 |  8523 | `			SyBlobDup(&pEntry->sCallback.sBlob,&sVal.sBlob);` |
|        3 |  8524 | `		}else if( pEntry->sCallback.iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  8525 | `			SyBlobAppend(&sVal.sBlob,"Class Method",sizeof("Class Method")-1);` |
|      ! 0 |  8526 | `		}else{` |
|        3 |  8527 | `			SyBlobAppend(&sVal.sBlob,"default output handler",sizeof("default output handler")-1);` |
|        - |  8528 | `		}` |
|        3 |  8529 | `		sVal.iFlags = MEMOBJ_STRING;` |
|        - |  8530 | `		/* Perform the insertion */` |
|        3 |  8531 | `		ph7_array_add_elem(pArray,0/* Automatic index assign */,&sVal /* Will make it's own copy */);` |
|        2 |  8532 | `	}` |
|        3 |  8533 | `	PH7_MemObjRelease(&sVal);` |
|        - |  8534 | `	/* Return the freshly created array */` |
|        3 |  8535 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8536 | `	return PH7_OK;` |
|        2 |  8537 |  |
|        - |  8538 | `/*` |
|        - |  8539 | ` * Section:` |
|        - |  8540 | ` *  Random numbers/string generators.` |
|        - |  8541 | ` * Status:` |
|        - |  8542 | ` *    Stable.` |
|        - |  8543 | ` */` |
|        - |  8544 | `/*` |
|        - |  8545 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  8546 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  8547 | ` * used by te SQLite3 library.` |
|        - |  8548 | ` */` |
|     1314 |  8549 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  8550 |  |
|        - |  8551 | `	sxu32 iNum;` |
|     1316 |  8552 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     1316 |  8553 | `	return iNum;` |
|        2 |  8554 |  |
|        - |  8555 | `/*` |
|        - |  8556 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  8557 | ` * Note that the generated string is NOT null terminated.` |
|        - |  8558 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  8559 | ` * by te SQLite3 library.` |
|        - |  8560 | ` */` |
|    43792 |  8561 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  8562 |  |
|        - |  8563 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  8564 | `	int i;` |
|        - |  8565 | `	/* Generate a binary string first */` |
|    43794 |  8566 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  8567 | `	/* Turn the binary string into english based alphabet */` |
|   481882 |  8568 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   438090 |  8569 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   219046 |  8570 | `	 }` |
|    43794 |  8571 |  |
|        - |  8572 | `/*` |
|        - |  8573 | ` * int rand()` |
|        - |  8574 | ` * int mt_rand()` |
|        - |  8575 | ` * int rand(int $min,int $max)` |
|        - |  8576 | ` * int mt_rand(int $min,int $max)` |
|        - |  8577 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  8578 | ` * Parameter` |
|        - |  8579 | ` *  $min` |
|        - |  8580 | ` *    The lowest value to return (default: 0)` |
|        - |  8581 | ` *  $max` |
|        - |  8582 | ` *   The highest value to return (default: getrandmax())` |
|        - |  8583 | ` * Return` |
|        - |  8584 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  8585 | ` * Note:` |
|        - |  8586 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8587 | ` *  by te SQLite3 library.` |
|        - |  8588 | ` */` |
|       20 |  8589 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8590 |  |
|        - |  8591 | `	sxu32 iNum;` |
|        - |  8592 | `	/* Generate the random number */` |
|       21 |  8593 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  8594 | `	if( nArg > 1 ){` |
|        - |  8595 | `		sxu32 iMin,iMax;` |
|        3 |  8596 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  8597 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  8598 | `		if( iMin < iMax ){` |
|        3 |  8599 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  8600 | `			if( iDiv > 0 ){` |
|        3 |  8601 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  8602 | `			}` |
|        1 |  8603 | `		}else if(iMax > 0 ){` |
|      ! 0 |  8604 | `			iNum %= iMax;` |
|      ! 0 |  8605 | `		}` |
|        1 |  8606 | `	}` |
|        - |  8607 | `	/* Return the number */` |
|       21 |  8608 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  8609 | `	return SXRET_OK;` |
|        1 |  8610 |  |
|        - |  8611 | `/*` |
|        - |  8612 | ` * int getrandmax(void)` |
|        - |  8613 | ` * int mt_getrandmax(void)` |
|        - |  8614 | ` * int rc4_getrandmax(void)` |
|        - |  8615 | ` *   Show largest possible random value` |
|        - |  8616 | ` * Return` |
|        - |  8617 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  8618 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  8619 | ` * Note:` |
|        - |  8620 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8621 | ` *  by te SQLite3 library.` |
|        - |  8622 | ` */` |
|        4 |  8623 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8624 |  |
|        2 |  8625 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  8626 | `	SXUNUSED(apArg);` |
|        5 |  8627 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  8628 | `	return SXRET_OK;` |
|        1 |  8629 |  |
|        - |  8630 | `/*` |
|        - |  8631 | ` * string rand_str()` |
|        - |  8632 | ` * string rand_str(int $len)` |
|        - |  8633 | ` *  Generate a random string (English alphabet).` |
|        - |  8634 | ` * Parameter` |
|        - |  8635 | ` *  $len` |
|        - |  8636 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  8637 | ` * Return` |
|        - |  8638 | ` *   A pseudo random string.` |
|        - |  8639 | ` * Note:` |
|        - |  8640 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8641 | ` *  by te SQLite3 library.` |
|        - |  8642 | ` *  This function is a symisc extension.` |
|        - |  8643 | ` */` |
|      120 |  8644 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8645 |  |
|        - |  8646 | `	char zString[1024];` |
|      122 |  8647 | `	int iLen = 0x10;` |
|      122 |  8648 | `	if( nArg > 0 ){` |
|        - |  8649 | `		/* Get the desired length */` |
|      122 |  8650 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  8651 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  8652 | `			/* Default length */` |
|        3 |  8653 | `			iLen = 0x10;` |
|        1 |  8654 | `		}` |
|       60 |  8655 | `	}` |
|        - |  8656 | `	/* Generate the random string */` |
|      122 |  8657 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  8658 | `	/* Return the generated string */` |
|      122 |  8659 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  8660 | `	return SXRET_OK;` |
|        2 |  8661 |  |
|        - |  8662 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  8663 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  8664 | `/* Unique ID private data */` |
|        - |  8665 | `struct unique_id_data` |
|        - |  8666 |  |
|        - |  8667 | `	ph7_context *pCtx; /* Call context */` |
|        - |  8668 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  8669 | `};` |
|        - |  8670 | `/*` |
|        - |  8671 | ` * Binary to hex consumer callback.` |
|        - |  8672 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  8673 | ` * defined below.` |
|        - |  8674 | ` */` |
|      192 |  8675 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  8676 |  |
|      193 |  8677 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  8678 | `	sxu32 nBuflen;` |
|        - |  8679 | `	/* Extract result buffer length */` |
|      193 |  8680 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  8681 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  8682 | `			/*` |
|        - |  8683 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  8684 | `			 * string will be 13 characters long` |
|        - |  8685 | `			 */` |
|       25 |  8686 | `		return SXERR_ABORT;` |
|        - |  8687 | `	}` |
|      169 |  8688 | `	if( nBuflen > 22 ){` |
|      ! 0 |  8689 | `		return SXERR_ABORT;` |
|        - |  8690 | `	}` |
|        - |  8691 | `	/* Safely Consume the hex stream */` |
|      169 |  8692 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  8693 | `	return SXRET_OK;` |
|       97 |  8694 |  |
|        - |  8695 | `/*` |
|        - |  8696 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  8697 | ` *  Generate a unique ID` |
|        - |  8698 | ` * Parameter` |
|        - |  8699 | ` * $prefix` |
|        - |  8700 | ` *  Append this prefix to the generated unique ID.` |
|        - |  8701 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  8702 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  8703 | ` * $more_entropy` |
|        - |  8704 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  8705 | ` *  that the result will be unique.` |
|        - |  8706 | ` * Return` |
|        - |  8707 | ` *  Returns the unique identifier, as a string.` |
|        - |  8708 | ` */` |
|       24 |  8709 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8710 |  |
|        - |  8711 | `	struct unique_id_data sUniq;` |
|        - |  8712 | `	unsigned char zDigest[20];` |
|       25 |  8713 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8714 | `	const char *zPrefix;` |
|        - |  8715 | `	SHA1Context sCtx;` |
|        - |  8716 | `	char zRandom[7];` |
|        - |  8717 | `	int nPrefix;` |
|        - |  8718 | `	int entropy;` |
|        - |  8719 | `	/* Generate a random string first */` |
|       25 |  8720 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  8721 | `	/* Initialize fields */` |
|       25 |  8722 | `	zPrefix = 0;` |
|       25 |  8723 | `	nPrefix = 0;` |
|       25 |  8724 | `	entropy = 0;` |
|       25 |  8725 | `	if( nArg > 0 ){` |
|        - |  8726 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  8727 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  8728 | `		if( nArg > 1 ){` |
|      ! 0 |  8729 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  8730 | `		}` |
|      ! 0 |  8731 | `	}` |
|       25 |  8732 | `	SHA1Init(&sCtx);` |
|        - |  8733 | `	/* Generate the random ID */` |
|       25 |  8734 | `	if( nPrefix > 0 ){` |
|      ! 0 |  8735 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  8736 | `	}` |
|        - |  8737 | `	/* Append the random ID */` |
|       25 |  8738 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  8739 | `	/* Append the random string */` |
|       25 |  8740 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  8741 | `	/* Increment the number */` |
|       25 |  8742 | `	pVm->unique_id++;` |
|       25 |  8743 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  8744 | `	/* Hexify the digest */` |
|       25 |  8745 | `	sUniq.pCtx = pCtx;` |
|       25 |  8746 | `	sUniq.entropy = entropy;` |
|       25 |  8747 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  8748 | `	/* All done */` |
|       25 |  8749 | `	return PH7_OK;` |
|        1 |  8750 |  |
|        - |  8751 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  8752 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  8753 | `/*` |
|        - |  8754 | ` * Section:` |
|        - |  8755 | ` *  Language construct implementation as foreign functions.` |
|        - |  8756 | ` * Status:` |
|        - |  8757 | ` *    Stable.` |
|        - |  8758 | ` */` |
|        - |  8759 | `/*` |
|        - |  8760 | ` * void echo($string...)` |
|        - |  8761 | ` *  Output one or more messages.` |
|        - |  8762 | ` * Parameters` |
|        - |  8763 | ` *  $string` |
|        - |  8764 | ` *   Message to output.` |
|        - |  8765 | ` * Return` |
|        - |  8766 | ` *  NULL.` |
|        - |  8767 | ` */` |
|      ! 0 |  8768 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  8769 |  |
|        - |  8770 | `	const char *zData;` |
|      ! 0 |  8771 | `	int nDataLen = 0;` |
|        - |  8772 | `	ph7_vm *pVm;` |
|        - |  8773 | `	int i,rc;` |
|        - |  8774 | `	/* Point to the target VM */` |
|      ! 0 |  8775 | `	pVm = pCtx->pVm;` |
|        - |  8776 | `	/* Output */` |
|      ! 0 |  8777 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  8778 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  8779 | `		if( nDataLen > 0 ){` |
|      ! 0 |  8780 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  8781 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  8782 | `				/* Increment output length */` |
|      ! 0 |  8783 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  8784 | `			}` |
|      ! 0 |  8785 | `			if( rc == SXERR_ABORT ){` |
|        - |  8786 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  8787 | `				return PH7_ABORT;` |
|        - |  8788 | `			}` |
|      ! 0 |  8789 | `		}` |
|      ! 0 |  8790 | `	}` |
|      ! 0 |  8791 | `	return SXRET_OK;` |
|      ! 0 |  8792 |  |
|        - |  8793 | `/*` |
|        - |  8794 | ` * int print($string...)` |
|        - |  8795 | ` *  Output one or more messages.` |
|        - |  8796 | ` * Parameters` |
|        - |  8797 | ` *  $string` |
|        - |  8798 | ` *   Message to output.` |
|        - |  8799 | ` * Return` |
|        - |  8800 | ` *  1 always.` |
|        - |  8801 | ` */` |
|        2 |  8802 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8803 |  |
|        - |  8804 | `	const char *zData;` |
|        3 |  8805 | `	int nDataLen = 0;` |
|        - |  8806 | `	ph7_vm *pVm;` |
|        - |  8807 | `	int i,rc;` |
|        - |  8808 | `	/* Point to the target VM */` |
|        3 |  8809 | `	pVm = pCtx->pVm;` |
|        - |  8810 | `	/* Output */` |
|        5 |  8811 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  8812 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  8813 | `		if( nDataLen > 0 ){` |
|        3 |  8814 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  8815 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  8816 | `				/* Increment output length */` |
|        3 |  8817 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  8818 | `			}` |
|        3 |  8819 | `			if( rc == SXERR_ABORT ){` |
|        - |  8820 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  8821 | `				return PH7_ABORT;` |
|        - |  8822 | `			}` |
|        1 |  8823 | `		}` |
|        2 |  8824 | `	}` |
|        - |  8825 | `	/* Return 1 */` |
|        3 |  8826 | `	ph7_result_int(pCtx,1);` |
|        3 |  8827 | `	return SXRET_OK;` |
|        2 |  8828 |  |
|        - |  8829 | `/*` |
|        - |  8830 | ` * void exit(string $msg)` |
|        - |  8831 | ` * void exit(int $status)` |
|        - |  8832 | ` * void die(string $ms)` |
|        - |  8833 | ` * void die(int $status)` |
|        - |  8834 | ` *   Output a message and terminate program execution.` |
|        - |  8835 | ` * Parameter` |
|        - |  8836 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  8837 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  8838 | ` *  and not printed` |
|        - |  8839 | ` * Return` |
|        - |  8840 | ` *  NULL` |
|        - |  8841 | ` */` |
|      ! 0 |  8842 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  8843 |  |
|      ! 0 |  8844 | `	if( nArg > 0 ){` |
|      ! 0 |  8845 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  8846 | `			const char *zData;` |
|      ! 0 |  8847 | `			int iLen = 0;` |
|        - |  8848 | `			/* Print exit message */` |
|      ! 0 |  8849 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  8850 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  8851 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  8852 | `			sxi32 iExitStatus;` |
|        - |  8853 | `			/* Record exit status code */` |
|      ! 0 |  8854 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  8855 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  8856 | `		}` |
|      ! 0 |  8857 | `	}` |
|        - |  8858 | `	/* Check if we are in an included file */` |
|      ! 0 |  8859 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  8860 | `		/* Exit the entire process */` |
|      ! 0 |  8861 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  8862 | `	}` |
|        - |  8863 | `	/* Abort processing immediately */` |
|      ! 0 |  8864 | `	return PH7_ABORT;` |
|      ! 0 |  8865 |  |
|        - |  8866 | `/*` |
|        - |  8867 | ` * bool isset($var,...)` |
|        - |  8868 | ` *  Finds out whether a variable is set.` |
|        - |  8869 | ` * Parameters` |
|        - |  8870 | ` *  One or more variable to check.` |
|        - |  8871 | ` * Return` |
|        - |  8872 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  8873 | ` */` |
|    54820 |  8874 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8875 |  |
|        - |  8876 | `	ph7_value *pObj;` |
|    54822 |  8877 | `	int res = 0;` |
|        - |  8878 | `	int i;` |
|    54822 |  8879 | `	if( nArg < 1 ){` |
|        - |  8880 | `		/* Missing arguments,return false */` |
|      ! 0 |  8881 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  8882 | `		return SXRET_OK;` |
|        - |  8883 | `	}` |
|        - |  8884 | `	/* Iterate over available arguments */` |
|    73490 |  8885 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    54822 |  8886 | `		pObj = apArg[i];` |
|    54822 |  8887 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    35986 |  8888 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  8889 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  8890 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  8891 | `			}` |
|    17992 |  8892 | `		}` |
|    54822 |  8893 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    54822 |  8894 | `		if( !res ){` |
|        - |  8895 | `			/* Variable not set,return FALSE */` |
|    36154 |  8896 | `			ph7_result_bool(pCtx,0);` |
|    36154 |  8897 | `			return SXRET_OK;` |
|        - |  8898 | `		}` |
|     9336 |  8899 | `	}` |
|        - |  8900 | `	/* All given variable are set,return TRUE */` |
|    18670 |  8901 | `	ph7_result_bool(pCtx,1);` |
|    18670 |  8902 | `	return SXRET_OK;` |
|    27412 |  8903 |  |
|        - |  8904 | `/*` |
|        - |  8905 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  8906 | ` * frame,the reference table and discard it's contents.` |
|        - |  8907 | ` * This function never fail and always return SXRET_OK.` |
|        - |  8908 | ` */` |
|  2744100 |  8909 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  8910 |  |
|        - |  8911 | `	ph7_value *pObj;` |
|        - |  8912 | `	VmRefObj *pRef;` |
|  2744102 |  8913 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2744102 |  8914 | `	if( pObj ){` |
|        - |  8915 | `		/* Release the object */` |
|  2744102 |  8916 | `		PH7_MemObjRelease(pObj);` |
|  1372050 |  8917 | `	}` |
|        - |  8918 | `	/* Remove old reference links */` |
|  2744102 |  8919 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2744102 |  8920 | `	if( pRef ){` |
|  2744082 |  8921 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  8922 | `		/* Unlink from the reference table */` |
|  2744082 |  8923 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2744082 |  8924 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  8925 | `			VmSlot sFree;` |
|        - |  8926 | `			/* Restore to the free list */` |
|  2744076 |  8927 | `			sFree.nIdx = nObjIdx;` |
|  2744076 |  8928 | `			sFree.pUserData = 0;` |
|  2744076 |  8929 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1372037 |  8930 | `		}` |
|  1372040 |  8931 | `	}` |
|  2744102 |  8932 | `	return SXRET_OK;` |
|        2 |  8933 |  |
|        - |  8934 | `/*` |
|        - |  8935 | ` * void unset($var,...)` |
|        - |  8936 | ` *   Unset one or more given variable.` |
|        - |  8937 | ` * Parameters` |
|        - |  8938 | ` *  One or more variable to unset.` |
|        - |  8939 | ` * Return` |
|        - |  8940 | ` *  Nothing.` |
|        - |  8941 | ` */` |
|     2744 |  8942 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8943 |  |
|        - |  8944 | `	ph7_value *pObj;` |
|        - |  8945 | `	ph7_vm *pVm;` |
|        - |  8946 | `	int i;` |
|        - |  8947 | `	/* Point to the target VM */` |
|     2746 |  8948 | `	pVm = pCtx->pVm;` |
|        - |  8949 | `	/* Iterate and unset */` |
|     8406 |  8950 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     5662 |  8951 | `		pObj = apArg[i];` |
|     5662 |  8952 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      720 |  8953 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  8954 | `				/* Throw an error */` |
|      ! 0 |  8955 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  8956 | `			}` |
|      361 |  8957 | `		}else{` |
|     4943 |  8958 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  8959 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     4943 |  8960 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     4937 |  8961 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2468 |  8962 | `			}` |
|        - |  8963 | `		}` |
|     2832 |  8964 | `	}` |
|     2746 |  8965 | `	return SXRET_OK;` |
|        2 |  8966 |  |
|        - |  8967 | `/*` |
|        - |  8968 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  8969 | ` */` |
|      108 |  8970 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8971 |  |
|      109 |  8972 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      109 |  8973 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  8974 | `	ph7_value *pObj;` |
|        - |  8975 | `	sxu32 nIdx;` |
|        - |  8976 | `	/* Extract the memory object */` |
|      109 |  8977 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      109 |  8978 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      109 |  8979 | `	if( pObj ){` |
|      109 |  8980 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      107 |  8981 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  8982 | `				SyString sName;` |
|        - |  8983 | `				ph7_value sKey;` |
|        - |  8984 | `				/* Perform the insertion */` |
|      107 |  8985 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      107 |  8986 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      107 |  8987 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      107 |  8988 | `				PH7_MemObjRelease(&sKey);` |
|       53 |  8989 | `			}` |
|       53 |  8990 | `		}` |
|       54 |  8991 | `	}` |
|      109 |  8992 | `	return SXRET_OK;` |
|        1 |  8993 |  |
|        - |  8994 | `/*` |
|        - |  8995 | ` * array get_defined_vars(void)` |
|        - |  8996 | ` *  Returns an array of all defined variables.` |
|        - |  8997 | ` * Parameter` |
|        - |  8998 | ` *  None` |
|        - |  8999 | ` * Return` |
|        - |  9000 | ` *  An array with all the variables defined in the current scope.` |
|        - |  9001 | ` */` |
|        2 |  9002 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9003 |  |
|        3 |  9004 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9005 | `	ph7_value *pArray;` |
|        - |  9006 | `	/* Create a new array */` |
|        3 |  9007 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9008 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9009 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9010 | `		SXUNUSED(apArg);` |
|        - |  9011 | `		/* Return NULL */` |
|      ! 0 |  9012 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9013 | `		return SXRET_OK;` |
|        - |  9014 | `	}` |
|        - |  9015 | `	/* Superglobals first */` |
|        3 |  9016 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  9017 | `	/* Then variable defined in the current frame */` |
|        3 |  9018 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  9019 | `	/* Finally,return the created array */` |
|        3 |  9020 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9021 | `	return SXRET_OK;` |
|        2 |  9022 |  |
|        - |  9023 | `/*` |
|        - |  9024 | ` * bool gettype($var)` |
|        - |  9025 | ` *  Get the type of a variable` |
|        - |  9026 | ` * Parameters` |
|        - |  9027 | ` *   $var` |
|        - |  9028 | ` *    The variable being type checked.` |
|        - |  9029 | ` * Return` |
|        - |  9030 | ` *   String representation of the given variable type.` |
|        - |  9031 | ` */` |
|       28 |  9032 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9033 |  |
|       29 |  9034 | `	const char *zType = "Empty";` |
|       29 |  9035 | `	if( nArg > 0 ){` |
|       29 |  9036 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       14 |  9037 | `	}` |
|        - |  9038 | `	/* Return the variable type */` |
|       29 |  9039 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       29 |  9040 | `	return SXRET_OK;` |
|        1 |  9041 |  |
|        - |  9042 | `/*` |
|        - |  9043 | ` * string get_resource_type(resource $handle)` |
|        - |  9044 | ` *  This function gets the type of the given resource.` |
|        - |  9045 | ` * Parameters` |
|        - |  9046 | ` *  $handle` |
|        - |  9047 | ` *  The evaluated resource handle.` |
|        - |  9048 | ` * Return` |
|        - |  9049 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  9050 | ` *  representing its type. If the type is not identified by this function` |
|        - |  9051 | ` *  the return value will be the string Unknown.` |
|        - |  9052 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  9053 | ` *  is not a resource.` |
|        - |  9054 | ` */` |
|        2 |  9055 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9056 |  |
|        3 |  9057 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  9058 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  9059 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9060 | `		return PH7_OK;` |
|        - |  9061 | `	}` |
|        3 |  9062 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  9063 | `	return SXRET_OK;` |
|        2 |  9064 |  |
|        - |  9065 | `/*` |
|        - |  9066 | ` * void var_dump(expression,....)` |
|        - |  9067 | ` *   var_dump � Dumps information about a variable` |
|        - |  9068 | ` * Parameters` |
|        - |  9069 | ` *   One or more expression to dump.` |
|        - |  9070 | ` * Returns` |
|        - |  9071 | ` *  Nothing.` |
|        - |  9072 | ` */` |
|      240 |  9073 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9074 |  |
|        - |  9075 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  9076 | `	int i;` |
|      242 |  9077 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  9078 | `	/* Dump one or more expressions */` |
|      488 |  9079 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      248 |  9080 | `		ph7_value *pObj = apArg[i];` |
|        - |  9081 | `		/* Reset the working buffer */` |
|      248 |  9082 | `		SyBlobReset(&sDump);` |
|        - |  9083 | `		/* Dump the given expression */` |
|      248 |  9084 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  9085 | `		/* Output */` |
|      248 |  9086 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      248 |  9087 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      123 |  9088 | `		}` |
|      125 |  9089 | `	}` |
|        - |  9090 | `	/* Release the working buffer */` |
|      242 |  9091 | `	SyBlobRelease(&sDump);` |
|      242 |  9092 | `	return SXRET_OK;` |
|        2 |  9093 |  |
|        - |  9094 | `/*` |
|        - |  9095 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  9096 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  9097 | ` * Parameters` |
|        - |  9098 | ` *   expression: Expression to dump` |
|        - |  9099 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  9100 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  9101 | ` *            print_r() will return the information rather than print it.` |
|        - |  9102 | ` * Return` |
|        - |  9103 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  9104 | ` *  Otherwise, the return value is TRUE.` |
|        - |  9105 | ` */` |
|       16 |  9106 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9107 |  |
|       17 |  9108 | `	int ret_string = 0;` |
|        - |  9109 | `	SyBlob sDump;` |
|       17 |  9110 | `	if( nArg < 1 ){` |
|        - |  9111 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9112 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9113 | `		return SXRET_OK;` |
|        - |  9114 | `	}` |
|       17 |  9115 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  9116 | `	if ( nArg > 1 ){` |
|        - |  9117 | `		/* Where to redirect output */` |
|       11 |  9118 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  9119 | `	}` |
|        - |  9120 | `	/* Generate dump */` |
|       17 |  9121 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  9122 | `	if( !ret_string ){` |
|        - |  9123 | `		/* Output dump */` |
|        7 |  9124 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9125 | `		/* Return true */` |
|        7 |  9126 | `		ph7_result_bool(pCtx,1);` |
|        4 |  9127 | `	}else{` |
|        - |  9128 | `		/* Generated dump as return value */` |
|       11 |  9129 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9130 | `	}` |
|        - |  9131 | `	/* Release the working buffer */` |
|       17 |  9132 | `	SyBlobRelease(&sDump);` |
|       17 |  9133 | `	return SXRET_OK;` |
|        9 |  9134 |  |
|        - |  9135 | `/*` |
|        - |  9136 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  9137 | ` * Same job as print_r. (see coment above)` |
|        - |  9138 | ` */` |
|        2 |  9139 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9140 |  |
|        3 |  9141 | `	int ret_string = 0;` |
|        - |  9142 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  9143 | `	if( nArg < 1 ){` |
|        - |  9144 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9145 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9146 | `		return SXRET_OK;` |
|        - |  9147 | `	}` |
|        3 |  9148 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  9149 | `	if ( nArg > 1 ){` |
|        - |  9150 | `		/* Where to redirect output */` |
|        3 |  9151 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  9152 | `	}` |
|        - |  9153 | `	/* Generate dump */` |
|        3 |  9154 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  9155 | `	if( !ret_string ){` |
|        - |  9156 | `		/* Output dump */` |
|      ! 0 |  9157 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9158 | `		/* Return NULL */` |
|      ! 0 |  9159 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9160 | `	}else{` |
|        - |  9161 | `		/* Generated dump as return value */` |
|        3 |  9162 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9163 | `	}` |
|        - |  9164 | `	/* Release the working buffer */` |
|        3 |  9165 | `	SyBlobRelease(&sDump);` |
|        3 |  9166 | `	return SXRET_OK;` |
|        2 |  9167 |  |
|        - |  9168 | `/*` |
|        - |  9169 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  9170 | ` *  Set/get the various assert flags.` |
|        - |  9171 | ` * Parameter` |
|        - |  9172 | ` * $what` |
|        - |  9173 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  9174 | ` *   ASSERT_WARNING         Issue a warning for each failed assertion` |
|        - |  9175 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  9176 | ` *   ASSERT_QUIET_EVAL      Not used` |
|        - |  9177 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  9178 | ` * $value` |
|        - |  9179 | ` *   An optional new value for the option.` |
|        - |  9180 | ` * Return` |
|        - |  9181 | ` *  Old setting on success or FALSE on failure.` |
|        - |  9182 | ` */` |
|        8 |  9183 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9184 |  |
|        9 |  9185 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9186 | `	int iOld,iNew,iValue;` |
|        9 |  9187 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|        - |  9188 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  9189 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9190 | `		return PH7_OK;` |
|        - |  9191 | `	}` |
|        - |  9192 | `	/* Save old assertion flags */` |
|        9 |  9193 | `	iOld = pVm->iAssertFlags;` |
|        - |  9194 | `	/* Extract the new flags */` |
|        9 |  9195 | `	iNew = ph7_value_to_int(apArg[0]);` |
|        9 |  9196 | `	if( iNew == PH7_ASSERT_DISABLE ){` |
|        7 |  9197 | `		pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        7 |  9198 | `		if( nArg > 1 ){` |
|        5 |  9199 | `			iValue = !ph7_value_to_bool(apArg[1]);` |
|        5 |  9200 | `			if( iValue ){` |
|        - |  9201 | `				/* Disable assertion */` |
|        3 |  9202 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        1 |  9203 | `			}` |
|        3 |  9204 | `		}` |
|        6 |  9205 | `	}else if( iNew == PH7_ASSERT_WARNING ){` |
|      ! 0 |  9206 | `		pVm->iAssertFlags &= ~PH7_ASSERT_WARNING;` |
|      ! 0 |  9207 | `		if( nArg > 1 ){` |
|      ! 0 |  9208 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  9209 | `			if( iValue ){` |
|        - |  9210 | `				/* Issue a warning for each failed assertion */` |
|      ! 0 |  9211 | `				pVm->iAssertFlags \|= PH7_ASSERT_WARNING;` |
|      ! 0 |  9212 | `			}` |
|      ! 0 |  9213 | `		}` |
|        3 |  9214 | `	}else if( iNew == PH7_ASSERT_BAIL ){` |
|        3 |  9215 | `		pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        3 |  9216 | `		if( nArg > 1 ){` |
|        3 |  9217 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|        3 |  9218 | `			if( iValue ){` |
|        - |  9219 | `				/* Terminate execution on failed assertions */` |
|        3 |  9220 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        1 |  9221 | `			}` |
|        2 |  9222 | `		}` |
|        1 |  9223 | `	}else if( iNew == PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  9224 | `		pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9225 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|        - |  9226 | `			/* Callback to call on failed assertions */` |
|      ! 0 |  9227 | `			PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  9228 | `			pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9229 | `		}` |
|      ! 0 |  9230 | `	}` |
|        - |  9231 | `	/* Return the old flags */` |
|        9 |  9232 | `	ph7_result_int(pCtx,iOld);` |
|        9 |  9233 | `	return PH7_OK;` |
|        5 |  9234 |  |
|        - |  9235 | `/*` |
|        - |  9236 | ` * bool assert(mixed $assertion)` |
|        - |  9237 | ` *  Checks if assertion is FALSE.` |
|        - |  9238 | ` * Parameter` |
|        - |  9239 | ` *  $assertion` |
|        - |  9240 | ` *    The assertion to test.` |
|        - |  9241 | ` * Return` |
|        - |  9242 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  9243 | ` */` |
|       14 |  9244 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9245 |  |
|       15 |  9246 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9247 | `	ph7_value *pAssert;` |
|        - |  9248 | `	int iFlags,iResult;` |
|       15 |  9249 | `	if( nArg < 1 ){` |
|        - |  9250 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  9251 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9252 | `		return PH7_OK;` |
|        - |  9253 | `	}` |
|       15 |  9254 | `	iFlags = pVm->iAssertFlags;` |
|       15 |  9255 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  9256 | `		/* Assertion is disabled,return FALSE */` |
|      ! 0 |  9257 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9258 | `		return PH7_OK;` |
|        - |  9259 | `	}` |
|       15 |  9260 | `	pAssert = apArg[0];` |
|       15 |  9261 | `	iResult = 1; /* cc warning */` |
|       15 |  9262 | `	if( pAssert->iFlags & MEMOBJ_STRING ){` |
|        - |  9263 | `		SyString sChunk;` |
|        7 |  9264 | `		SyStringInitFromBuf(&sChunk,SyBlobData(&pAssert->sBlob),SyBlobLength(&pAssert->sBlob));` |
|        7 |  9265 | `		if( sChunk.nByte > 0 ){` |
|        5 |  9266 | `			VmEvalChunk(pVm,pCtx,&sChunk,PH7_PHP_ONLY\|PH7_PHP_EXPR,FALSE);` |
|        - |  9267 | `			/* Extract evaluation result */` |
|        5 |  9268 | `			iResult = ph7_value_to_bool(pCtx->pRet);` |
|        3 |  9269 | `		}else{` |
|        3 |  9270 | `			iResult = 0;` |
|        - |  9271 | `		}` |
|        4 |  9272 | `	}else{` |
|        - |  9273 | `		/* Perform a boolean cast */` |
|        9 |  9274 | `		iResult = ph7_value_to_bool(apArg[0]);` |
|        - |  9275 | `	}` |
|       15 |  9276 | `	if( !iResult ){` |
|        - |  9277 | `		/* Assertion failed */` |
|        9 |  9278 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  9279 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  9280 | `			ph7_value sFile,sLine;` |
|        - |  9281 | `			ph7_value *apCbArg[3];` |
|        - |  9282 | `			SyString *pFile;` |
|        - |  9283 | `			/* Extract the processed script */` |
|      ! 0 |  9284 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  9285 | `			if( pFile == 0 ){` |
|      ! 0 |  9286 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  9287 | `			}` |
|        - |  9288 | `			/* Invoke the callback */` |
|      ! 0 |  9289 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  9290 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  9291 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  9292 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  9293 | `			apCbArg[2] = pAssert;` |
|      ! 0 |  9294 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  9295 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  9296 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  9297 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  9298 | `		}` |
|        9 |  9299 | `		if( iFlags & PH7_ASSERT_WARNING ){` |
|        - |  9300 | `			/* Emit a warning */` |
|        9 |  9301 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Assertion failed");` |
|        4 |  9302 | `		}` |
|        9 |  9303 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  9304 | `			/* Abort VM execution immediately */` |
|        3 |  9305 | `			return PH7_ABORT;` |
|        - |  9306 | `		}` |
|        3 |  9307 | `	}` |
|        - |  9308 | `	/* Assertion result */` |
|       13 |  9309 | `	ph7_result_bool(pCtx,iResult);` |
|       13 |  9310 | `	return PH7_OK;` |
|        8 |  9311 |  |
|        - |  9312 | `/*` |
|        - |  9313 | ` * Section:` |
|        - |  9314 | ` *  Error reporting functions.` |
|        - |  9315 | ` * Status:` |
|        - |  9316 | ` *    Stable.` |
|        - |  9317 | ` */` |
|        - |  9318 | `/*` |
|        - |  9319 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  9320 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  9321 | ` * Parameters` |
|        - |  9322 | ` *  $error_msg` |
|        - |  9323 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  9324 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  9325 | ` * $error_type` |
|        - |  9326 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  9327 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  9328 | ` * Return` |
|        - |  9329 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  9330 | ` */` |
|       12 |  9331 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9332 |  |
|       14 |  9333 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  9334 | `	int rc = PH7_OK;` |
|       14 |  9335 | `	if( nArg > 0 ){` |
|        - |  9336 | `		const char *zErr;` |
|        - |  9337 | `		int nLen;` |
|        - |  9338 | `		/* Extract the error message */` |
|       12 |  9339 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  9340 | `		if( nArg > 1 ){` |
|        - |  9341 | `			/* Extract the error type */` |
|       12 |  9342 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  9343 | `			switch( nErr ){` |
|        1 |  9344 | `			case 1:   /* E_ERROR */` |
|        - |  9345 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  9346 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  9347 | `			case 256: /* E_USER_ERROR */` |
|        3 |  9348 | `				nErr = PH7_CTX_ERR;` |
|        3 |  9349 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  9350 | `				break;` |
|        1 |  9351 | `			case 2:   /* E_WARNING */` |
|        - |  9352 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  9353 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  9354 | `			case 512: /* E_USER_WARNING */` |
|        3 |  9355 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  9356 | `				break;` |
|        3 |  9357 | `			default:` |
|        8 |  9358 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  9359 | `				break;` |
|        - |  9360 | `			}` |
|        5 |  9361 | `		}` |
|        - |  9362 | `		/* Report error */` |
|       12 |  9363 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  9364 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  9365 | `			return rc;` |
|        - |  9366 | `		}` |
|        - |  9367 | `		/* Return true */` |
|       12 |  9368 | `		ph7_result_bool(pCtx,1);` |
|        7 |  9369 | `	}else{` |
|        - |  9370 | `		/* Missing arguments,return FALSE */` |
|        3 |  9371 | `		ph7_result_bool(pCtx,0);` |
|        - |  9372 | `	}` |
|       14 |  9373 | `	return rc;` |
|        8 |  9374 |  |
|        - |  9375 | `/*` |
|        - |  9376 | ` * int error_reporting([int $level])` |
|        - |  9377 | ` *  Sets which PHP errors are reported.` |
|        - |  9378 | ` * Parameters` |
|        - |  9379 | ` *  $level` |
|        - |  9380 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  9381 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  9382 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  9383 | ` *   levels will not always behave as expected.` |
|        - |  9384 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  9385 | ` *   in the predefined constants.` |
|        - |  9386 | ` * Return` |
|        - |  9387 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  9388 | ` *   parameter is given.` |
|        - |  9389 | ` */` |
|       18 |  9390 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9391 |  |
|       19 |  9392 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9393 | `	int nOld;` |
|        - |  9394 | `	/* Extract the old reporting level */` |
|       19 |  9395 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       19 |  9396 | `	if( nArg > 0 ){` |
|        - |  9397 | `		int nNew;` |
|        - |  9398 | `		/* Extract the desired error reporting level */` |
|       11 |  9399 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       11 |  9400 | `		if( !nNew ){` |
|        - |  9401 | `			/* Do not report errors at all */` |
|        5 |  9402 | `			pVm->bErrReport = 0;` |
|        3 |  9403 | `		}else{` |
|        - |  9404 | `			/* Report all errors */` |
|        7 |  9405 | `			pVm->bErrReport = 1;` |
|        - |  9406 | `		}` |
|        5 |  9407 | `	}` |
|        - |  9408 | `	/* Return the old level */` |
|       19 |  9409 | `	ph7_result_int(pCtx,nOld);` |
|       19 |  9410 | `	return PH7_OK;` |
|        1 |  9411 |  |
|        - |  9412 | `/*` |
|        - |  9413 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  9414 | ` *  Send an error message somewhere.` |
|        - |  9415 | ` * Parameter` |
|        - |  9416 | ` *  $message` |
|        - |  9417 | ` *   The error message that should be logged.` |
|        - |  9418 | ` *  $message_type` |
|        - |  9419 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  9420 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  9421 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  9422 | ` *       This is the default option.` |
|        - |  9423 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  9424 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  9425 | ` *    2  No longer an option.` |
|        - |  9426 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  9427 | ` *       to the end of the message string.` |
|        - |  9428 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  9429 | ` *  $destination` |
|        - |  9430 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  9431 | ` *  $extra_headers` |
|        - |  9432 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  9433 | ` * Return` |
|        - |  9434 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9435 | ` * NOTE:` |
|        - |  9436 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  9437 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  9438 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  9439 | ` *  Otherwise this function is no-op.` |
|        - |  9440 | ` */` |
|        4 |  9441 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9442 |  |
|        - |  9443 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  9444 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  9445 | `	int iType = 0;` |
|        5 |  9446 | `	if( nArg < 1 ){` |
|        - |  9447 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  9448 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9449 | `		return PH7_OK;` |
|        - |  9450 | `	}` |
|        5 |  9451 | `	if( pVm->xErrLog  ){` |
|        - |  9452 | `		/* Invoke the user callback */` |
|      ! 0 |  9453 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  9454 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  9455 | `		if( nArg > 1 ){` |
|      ! 0 |  9456 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  9457 | `			if( nArg > 2 ){` |
|      ! 0 |  9458 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  9459 | `				if( nArg > 3 ){` |
|      ! 0 |  9460 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  9461 | `				}` |
|      ! 0 |  9462 | `			}` |
|      ! 0 |  9463 | `		}` |
|      ! 0 |  9464 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  9465 | `	}` |
|        - |  9466 | `	/* Retun TRUE */` |
|        5 |  9467 | `	ph7_result_bool(pCtx,1);` |
|        5 |  9468 | `	return PH7_OK;` |
|        3 |  9469 |  |
|        - |  9470 | `/*` |
|        - |  9471 | ` * bool restore_exception_handler(void)` |
|        - |  9472 | ` *  Restores the previously defined exception handler function.` |
|        - |  9473 | ` * Parameter` |
|        - |  9474 | ` *  None` |
|        - |  9475 | ` * Return` |
|        - |  9476 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  9477 | ` */` |
|        4 |  9478 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9479 |  |
|        5 |  9480 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9481 | `	ph7_value *pOld,*pNew;` |
|        - |  9482 | `	/* Point to the old and the new handler */` |
|        5 |  9483 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  9484 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  9485 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9486 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9487 | `		SXUNUSED(apArg);` |
|        - |  9488 | `		/* No installed handler,return FALSE */` |
|        5 |  9489 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9490 | `		return PH7_OK;` |
|        - |  9491 | `	}` |
|        - |  9492 | `	/* Copy the old handler */` |
|      ! 0 |  9493 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9494 | `	PH7_MemObjRelease(pOld);` |
|        - |  9495 | `	/* Return TRUE */` |
|      ! 0 |  9496 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9497 | `	return PH7_OK;` |
|        3 |  9498 |  |
|        - |  9499 | `/*` |
|        - |  9500 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  9501 | ` *  Sets a user-defined exception handler function.` |
|        - |  9502 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  9503 | ` * NOTE` |
|        - |  9504 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  9505 | ` *  the satndard PHP engine.` |
|        - |  9506 | ` * Parameters` |
|        - |  9507 | ` *  $exception_handler` |
|        - |  9508 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  9509 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  9510 | ` *   that was thrown.` |
|        - |  9511 | ` *  Note:` |
|        - |  9512 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9513 | ` * Return` |
|        - |  9514 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  9515 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9516 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9517 | ` */` |
|        4 |  9518 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9519 |  |
|        6 |  9520 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9521 | `	ph7_value *pOld,*pNew;` |
|        - |  9522 | `	/* Point to the old and the new handler */` |
|        6 |  9523 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  9524 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  9525 | `	/* Return the old handler */` |
|        6 |  9526 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  9527 | `	if( nArg > 0 ){` |
|        6 |  9528 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9529 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  9530 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  9531 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  9532 | `		}else{` |
|        6 |  9533 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9534 | `			/* Install the new handler */` |
|        6 |  9535 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9536 | `		}` |
|        2 |  9537 | `	}` |
|        6 |  9538 | `	return PH7_OK;` |
|        2 |  9539 |  |
|        - |  9540 | `/*` |
|        - |  9541 | ` * bool restore_error_handler(void)` |
|        - |  9542 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9543 | ` * Parameters:` |
|        - |  9544 | ` *  None.` |
|        - |  9545 | ` * Return` |
|        - |  9546 | ` *  Always TRUE.` |
|        - |  9547 | ` */` |
|        4 |  9548 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9549 |  |
|        5 |  9550 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9551 | `	ph7_value *pOld,*pNew;` |
|        - |  9552 | `	/* Point to the old and the new handler */` |
|        5 |  9553 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  9554 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  9555 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9556 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9557 | `		SXUNUSED(apArg);` |
|        - |  9558 | `		/* No installed callback,return FALSE */` |
|        5 |  9559 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9560 | `		return PH7_OK;` |
|        - |  9561 | `	}` |
|        - |  9562 | `	/* Copy the old callback */` |
|      ! 0 |  9563 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9564 | `	PH7_MemObjRelease(pOld);` |
|        - |  9565 | `	/* Return TRUE */` |
|      ! 0 |  9566 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9567 | `	return PH7_OK;` |
|        3 |  9568 |  |
|        - |  9569 | `/*` |
|        - |  9570 | ` * value set_error_handler(callable $error_handler)` |
|        - |  9571 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9572 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9573 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9574 | ` *  Sets a user-defined error handler function.` |
|        - |  9575 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  9576 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  9577 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  9578 | ` *  conditions (using trigger_error()).` |
|        - |  9579 | ` * Parameters` |
|        - |  9580 | ` *  $error_handler` |
|        - |  9581 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  9582 | ` *   describing the error.` |
|        - |  9583 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  9584 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  9585 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  9586 | ` *   The function can be shown as:` |
|        - |  9587 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  9588 | ` *     errno` |
|        - |  9589 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  9590 | ` *   errstr` |
|        - |  9591 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  9592 | ` *   errfile` |
|        - |  9593 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  9594 | ` *     was raised in, as a string.` |
|        - |  9595 | ` *  Note:` |
|        - |  9596 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9597 | ` * Return` |
|        - |  9598 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  9599 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9600 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9601 | ` */` |
|     7890 |  9602 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9603 |  |
|     7892 |  9604 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9605 | `	ph7_value *pOld,*pNew;` |
|        - |  9606 | `	/* Point to the old and the new handler */` |
|     7892 |  9607 | `	pOld = &pVm->aErrCB[0];` |
|     7892 |  9608 | `	pNew = &pVm->aErrCB[1];` |
|        - |  9609 | `	/* Return the old handler */` |
|     7892 |  9610 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     7892 |  9611 | `	if( nArg > 0 ){` |
|     7892 |  9612 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9613 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     3945 |  9614 | `			PH7_MemObjRelease(pNew);` |
|     3945 |  9615 | `			ph7_result_bool(pCtx,1);` |
|     1973 |  9616 | `		}else{` |
|     3948 |  9617 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9618 | `			/* Install the new handler */` |
|     3948 |  9619 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9620 | `		}` |
|     3945 |  9621 | `	}` |
|     7892 |  9622 | `	return PH7_OK;` |
|        2 |  9623 |  |
|        - |  9624 | `/*` |
|        - |  9625 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  9626 | ` *  Generates a backtrace.` |
|        - |  9627 | ` * Paramaeter` |
|        - |  9628 | ` *  $options` |
|        - |  9629 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  9630 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  9631 | ` *   all the function/method arguments, to save memory.` |
|        - |  9632 | ` * $limit` |
|        - |  9633 | ` *   (Not Used)` |
|        - |  9634 | ` * Return` |
|        - |  9635 | ` *  An array.The possible returned elements are as follows:` |
|        - |  9636 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  9637 | ` *          Name        Type      Description` |
|        - |  9638 | ` *          ------      ------     -----------` |
|        - |  9639 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  9640 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  9641 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  9642 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  9643 | ` *          object      object    The current object.` |
|        - |  9644 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  9645 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  9646 | ` */` |
|      164 |  9647 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9648 |  |
|      166 |  9649 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9650 | `	ph7_value *pArray;` |
|        - |  9651 | `	ph7_class *pClass;` |
|        - |  9652 | `	ph7_value *pValue;` |
|        - |  9653 | `	SyString *pFile;` |
|        - |  9654 | `	/* Create a new array */` |
|      166 |  9655 | `	pArray = ph7_context_new_array(pCtx);` |
|      166 |  9656 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      166 |  9657 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9658 | `		/* Out of memory,return NULL */` |
|      ! 0 |  9659 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  9660 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9661 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9662 | `		SXUNUSED(apArg);` |
|      ! 0 |  9663 | `		return PH7_OK;` |
|        - |  9664 | `	}` |
|        - |  9665 | `	/* Dump running function name and it's arguments  */` |
|      166 |  9666 | `	if( pVm->pFrame->pParent ){` |
|      166 |  9667 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  9668 | `		ph7_vm_func *pFunc;` |
|        - |  9669 | `		ph7_value *pArg;` |
|      166 |  9670 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9671 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  9672 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  9673 | `		}` |
|      166 |  9674 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      166 |  9675 | `		if( pFrame->pParent && pFunc ){` |
|      166 |  9676 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      166 |  9677 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      166 |  9678 | `			ph7_value_reset_string_cursor(pValue);` |
|       82 |  9679 | `		}` |
|        - |  9680 | `		/* Function arguments */` |
|      166 |  9681 | `		pArg = ph7_context_new_array(pCtx);` |
|      166 |  9682 | `		if( pArg  ){` |
|        - |  9683 | `			ph7_value *pObj;` |
|        - |  9684 | `			VmSlot *aSlot;` |
|        - |  9685 | `			sxu32 n;` |
|        - |  9686 | `			/* Start filling the array with the given arguments */` |
|      166 |  9687 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      650 |  9688 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      486 |  9689 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      486 |  9690 | `				if( pObj ){` |
|      486 |  9691 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      242 |  9692 | `				}` |
|      244 |  9693 | `			}` |
|        - |  9694 | `			/* Save the array */` |
|      166 |  9695 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|       82 |  9696 | `		}` |
|       82 |  9697 | `	}` |
|      166 |  9698 | `	ph7_value_int(pValue,1);` |
|        - |  9699 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  9700 | `	 * line numbers at run-time. )` |
|        - |  9701 | `	 */` |
|      166 |  9702 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  9703 | `	/* Current processed script */` |
|      166 |  9704 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      166 |  9705 | `	if( pFile ){` |
|      166 |  9706 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      166 |  9707 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      166 |  9708 | `		ph7_value_reset_string_cursor(pValue);` |
|       82 |  9709 | `	}` |
|        - |  9710 | `	/* Top class */` |
|      166 |  9711 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      166 |  9712 | `	if( pClass ){` |
|      162 |  9713 | `		ph7_value_reset_string_cursor(pValue);` |
|      162 |  9714 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      162 |  9715 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|       80 |  9716 | `	}` |
|        - |  9717 | `	/* Return the freshly created array */` |
|      166 |  9718 | `	ph7_result_value(pCtx,pArray);` |
|        - |  9719 | `	/*` |
|        - |  9720 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  9721 | `	 * as soon we return from this function.` |
|        - |  9722 | `	 */` |
|      166 |  9723 | `	return PH7_OK;` |
|       84 |  9724 |  |
|        - |  9725 | `/*` |
|        - |  9726 | ` * Generate a small backtrace.` |
|        - |  9727 | ` * Store the generated dump in the given BLOB` |
|        - |  9728 | ` */` |
|        4 |  9729 | `static int VmMiniBacktrace(` |
|        - |  9730 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  9731 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  9732 | `	)` |
|        1 |  9733 |  |
|        5 |  9734 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  9735 | `	ph7_vm_func *pFunc;` |
|        - |  9736 | `	ph7_class *pClass;` |
|        - |  9737 | `	SyString *pFile;` |
|        - |  9738 | `	/* Called function */` |
|        5 |  9739 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9740 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  9741 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  9742 | `	}` |
|        5 |  9743 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  9744 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  9745 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  9746 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  9747 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  9748 | `	}else{` |
|      ! 0 |  9749 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  9750 | `	}` |
|        5 |  9751 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  9752 | `	/* Current processed script */` |
|        5 |  9753 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  9754 | `	if( pFile ){` |
|        5 |  9755 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  9756 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  9757 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  9758 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  9759 | `	}` |
|        - |  9760 | `	/* Top class */` |
|        5 |  9761 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  9762 | `	if( pClass ){` |
|      ! 0 |  9763 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  9764 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  9765 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  9766 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  9767 | `	}` |
|        5 |  9768 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  9769 | `	/* All done */` |
|        5 |  9770 | `	return SXRET_OK;` |
|        1 |  9771 |  |
|        - |  9772 | `/*` |
|        - |  9773 | ` * void debug_print_backtrace()` |
|        - |  9774 | ` *  Prints a backtrace` |
|        - |  9775 | ` * Parameters` |
|        - |  9776 | ` * None` |
|        - |  9777 | ` * Return` |
|        - |  9778 | ` * NULL` |
|        - |  9779 | ` */` |
|        2 |  9780 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9781 |  |
|        3 |  9782 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9783 | `	SyBlob sDump;` |
|        3 |  9784 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  9785 | `	/* Generate the backtrace */` |
|        3 |  9786 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  9787 | `	/* Output backtrace */` |
|        3 |  9788 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9789 | `	/* All done,cleanup */` |
|        3 |  9790 | `	SyBlobRelease(&sDump);` |
|        1 |  9791 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9792 | `	SXUNUSED(apArg);` |
|        3 |  9793 | `	return PH7_OK;` |
|        1 |  9794 |  |
|        - |  9795 | `/*` |
|        - |  9796 | ` * string debug_string_backtrace()` |
|        - |  9797 | ` *  Generate a backtrace` |
|        - |  9798 | ` * Parameters` |
|        - |  9799 | ` * None` |
|        - |  9800 | ` * Return` |
|        - |  9801 | ` *  A mini backtrace().` |
|        - |  9802 | ` * Note that this is a symisc extension.` |
|        - |  9803 | ` */` |
|        2 |  9804 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9805 |  |
|        3 |  9806 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9807 | `	SyBlob sDump;` |
|        3 |  9808 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  9809 | `	/* Generate the backtrace */` |
|        3 |  9810 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  9811 | `	/* Return the backtrace */` |
|        3 |  9812 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  9813 | `	/* All done,cleanup */` |
|        3 |  9814 | `	SyBlobRelease(&sDump);` |
|        1 |  9815 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9816 | `	SXUNUSED(apArg);` |
|        3 |  9817 | `	return PH7_OK;` |
|        1 |  9818 |  |
|        - |  9819 | `/*` |
|        - |  9820 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  9821 | ` * exception is triggered.` |
|        - |  9822 | ` */` |
|      148 |  9823 | `static sxi32 VmUncaughtException(` |
|        - |  9824 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  9825 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  9826 | `	)` |
|        2 |  9827 |  |
|        - |  9828 | `	ph7_value *apArg[2],sArg;` |
|      150 |  9829 | `	int nArg = 1;` |
|        - |  9830 | `	sxi32 rc;` |
|      150 |  9831 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  9832 | `		/* Nesting limit reached */` |
|      ! 0 |  9833 | `		return SXRET_OK;` |
|        - |  9834 | `	}` |
|        - |  9835 | `	/* Call any exception handler if available */` |
|      150 |  9836 | `	PH7_MemObjInit(pVm,&sArg);` |
|      150 |  9837 | `	if( pThis ){` |
|        - |  9838 | `		/* Load the exception instance */` |
|      150 |  9839 | `		sArg.x.pOther = pThis;` |
|      150 |  9840 | `		pThis->iRef++;` |
|      150 |  9841 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|       76 |  9842 | `	}else{` |
|      ! 0 |  9843 | `		nArg = 0;` |
|        - |  9844 | `	}` |
|      150 |  9845 | `	apArg[0] = &sArg;` |
|        - |  9846 | `	/* Call the exception handler if available */` |
|      150 |  9847 | `	pVm->nExceptDepth++;` |
|      150 |  9848 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      150 |  9849 | `	pVm->nExceptDepth--;` |
|      150 |  9850 | `	if( rc != SXRET_OK ){` |
|        - |  9851 | `		SyBlob sMsgBuf;` |
|      148 |  9852 | `		const char *zClass = "Exception";` |
|      148 |  9853 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  9854 | `		const char *zMsg;` |
|        - |  9855 | `		sxu32 nMsg;` |
|        - |  9856 | `		const char *zFuncName;` |
|        - |  9857 | `		int nFuncLen;` |
|      148 |  9858 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      148 |  9859 | `		if( pThis ){` |
|        - |  9860 | `			ph7_class_method *pGetMessage;` |
|        - |  9861 | `			ph7_value sMsg;` |
|        - |  9862 | `			const char *zTmp;` |
|        - |  9863 | `			int nTmp;` |
|      148 |  9864 | `			zClass = pThis->pClass->sName.zString;` |
|      148 |  9865 | `			nClass = pThis->pClass->sName.nByte;` |
|      148 |  9866 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      148 |  9867 | `			if( pGetMessage ){` |
|      148 |  9868 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      148 |  9869 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      148 |  9870 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      148 |  9871 | `					if( zTmp && nTmp > 0 ){` |
|      148 |  9872 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|       73 |  9873 | `					}` |
|       73 |  9874 | `				}` |
|      148 |  9875 | `				PH7_MemObjRelease(&sMsg);` |
|       73 |  9876 | `			}` |
|       73 |  9877 | `		}` |
|      148 |  9878 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  9879 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  9880 | `		}` |
|      148 |  9881 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      148 |  9882 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      148 |  9883 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      148 |  9884 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      148 |  9885 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  9886 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      148 |  9887 | `		rc = SXERR_ABORT;` |
|       73 |  9888 | `	}` |
|      150 |  9889 | `	PH7_MemObjRelease(&sArg);` |
|      150 |  9890 | `	return rc;` |
|       76 |  9891 |  |
|        - |  9892 | `/*` |
|        - |  9893 | ` * Throw an user exception.` |
|        - |  9894 | ` */` |
|      162 |  9895 | `static sxi32 VmThrowException(` |
|        - |  9896 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  9897 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  9898 | `	)` |
|        2 |  9899 |  |
|        - |  9900 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  9901 | `	ph7_exception **apException;` |
|        - |  9902 | `	ph7_exception *pException;` |
|        - |  9903 | `	/* Point to the stack of loaded exceptions */` |
|      164 |  9904 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      164 |  9905 | `	pException = 0;` |
|      164 |  9906 | `	pCatch = 0;` |
|      164 |  9907 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  9908 | `		ph7_exception_block *aCatch;` |
|        - |  9909 | `		ph7_class *pClass;` |
|        - |  9910 | `		sxu32 j;` |
|        - |  9911 | `		/* Locate the appropriate block to execute */` |
|       16 |  9912 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       16 |  9913 | `		(void)SySetPop(&pVm->aException);` |
|       16 |  9914 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       16 |  9915 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       16 |  9916 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  9917 | `			/* Extract the target class */` |
|       16 |  9918 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       16 |  9919 | `			if( pClass == 0 ){` |
|        - |  9920 | `				/* No such class */` |
|      ! 0 |  9921 | `				continue;` |
|        - |  9922 | `			}` |
|       16 |  9923 | `			if( VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  9924 | `				/* Catch block found,break immeditaley */` |
|       16 |  9925 | `				pCatch = &aCatch[j];` |
|       16 |  9926 | `				break;` |
|        - |  9927 | `			}` |
|      ! 0 |  9928 | `		}` |
|        7 |  9929 | `	}` |
|        - |  9930 | `	/* Execute the cached block if available */` |
|      164 |  9931 | `	if( pCatch == 0 ){` |
|        - |  9932 | `		sxi32 rc;` |
|      150 |  9933 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      150 |  9934 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  9935 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  9936 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9937 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  9938 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  9939 | `			}` |
|      ! 0 |  9940 | `			if( pException->pFrame == pFrame ){` |
|        - |  9941 | `				/* Tell the upper layer that the exception was caught */` |
|      ! 0 |  9942 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  9943 | `			}` |
|      ! 0 |  9944 | `		}` |
|      150 |  9945 | `		return rc;` |
|      ! 0 |  9946 | `	}else{` |
|       16 |  9947 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  9948 | `		sxi32 rc;` |
|       24 |  9949 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9950 | `			/* Safely ignore the exception frame */` |
|       10 |  9951 | `			pFrame = pFrame->pParent;` |
|        2 |  9952 | `		}` |
|       16 |  9953 | `		if( pException->pFrame == pFrame ){` |
|        - |  9954 | `			/* Tell the upper layer that the exception was caught */` |
|        8 |  9955 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        3 |  9956 | `		}` |
|        - |  9957 | `		/* Create a private frame first */` |
|       16 |  9958 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       16 |  9959 | `		if( rc == SXRET_OK ){` |
|        - |  9960 | `			/* Mark as catch frame */` |
|       16 |  9961 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       16 |  9962 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       16 |  9963 | `			if( pObj ){` |
|        - |  9964 | `				/* Install the exception instance */` |
|       16 |  9965 | `				pThis->iRef++; /* Increment reference count */` |
|       16 |  9966 | `				pObj->x.pOther = pThis;` |
|       16 |  9967 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        7 |  9968 | `			}` |
|        - |  9969 | `			/* Exceute the block */` |
|       16 |  9970 | `			VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  9971 | `			/* Leave the frame */` |
|       16 |  9972 | `			VmLeaveFrame(&(*pVm));` |
|        7 |  9973 | `		}` |
|        - |  9974 | `	}` |
|        - |  9975 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  9976 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  9977 | `	 */` |
|       16 |  9978 | `	return SXRET_OK;` |
|       83 |  9979 |  |
|        - |  9980 | `/*` |
|        - |  9981 | ` * Section:` |
|        - |  9982 | ` *  Version,Credits and Copyright related functions.` |
|        - |  9983 | ` * Status:` |
|        - |  9984 | ` *    Stable.` |
|        - |  9985 | ` */` |
|        - |  9986 | `/*` |
|        - |  9987 | ` * string ph7version(void)` |
|        - |  9988 | ` *  Returns the running version of the PH7 version.` |
|        - |  9989 | ` * Parameters` |
|        - |  9990 | ` *  None` |
|        - |  9991 | ` * Return` |
|        - |  9992 | ` * Current PH7 version.` |
|        - |  9993 | ` */` |
|        2 |  9994 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9995 |  |
|        1 |  9996 | `	SXUNUSED(nArg);` |
|        1 |  9997 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  9998 | `	/* Current engine version */` |
|        3 |  9999 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 10000 | `	return PH7_OK;` |
|        1 | 10001 |  |
|        - | 10002 | `/*` |
|        - | 10003 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 10004 | ` */` |
|        - | 10005 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 10006 | ` "<html><head>"\` |
|        - | 10007 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 10008 | ` "<style type=\"text/css\">"\` |
|        - | 10009 | ` "div {"\` |
|        - | 10010 | `     "border: 1px solid #cccccc;"\` |
|        - | 10011 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 10012 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 10013 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 10014 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 10015 | `     "-webkit-border-radius: 10px;"\` |
|        - | 10016 | `     "-o-border-radius: 10px;"\` |
|        - | 10017 | `     "border-radius: 10px;"\` |
|        - | 10018 | `     "padding-left: 2em;"\` |
|        - | 10019 | `     "background-color: white;"\` |
|        - | 10020 | `     "margin-left: auto;"\` |
|        - | 10021 | `     "font-family: verdana;"\` |
|        - | 10022 | `     "padding-right: 2em;"\` |
|        - | 10023 | `     "margin-right: auto;"\` |
|        - | 10024 | `     "}"\` |
|        - | 10025 | `     "body {"\` |
|        - | 10026 | `     "padding: 0.2em;"\` |
|        - | 10027 | `     "font-style: normal;"\` |
|        - | 10028 | `     "font-size: medium;"\` |
|        - | 10029 | `     "background-color: #f2f2f2;"\` |
|        - | 10030 | `     "}"\` |
|        - | 10031 | `     "hr {"\` |
|        - | 10032 | `     "border-style: solid none none;"\` |
|        - | 10033 | `     "border-width: 1px medium medium;"\` |
|        - | 10034 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 10035 | `     "height: 1px;"\` |
|        - | 10036 | `     "}"\` |
|        - | 10037 | `     "a {"\` |
|        - | 10038 | `     "color: #3366cc;"\` |
|        - | 10039 | `     "text-decoration: none;"\` |
|        - | 10040 | `     "}"\` |
|        - | 10041 | `     "a:hover {"\` |
|        - | 10042 | `     "color: #999999;"\` |
|        - | 10043 | `     "}"\` |
|        - | 10044 | `     "a:active {"\` |
|        - | 10045 | `     "color: #663399;"\` |
|        - | 10046 | `     "}"\` |
|        - | 10047 | `     "h1 {"\` |
|        - | 10048 | `     "margin: 0;"\` |
|        - | 10049 | `     "padding: 0;"\` |
|        - | 10050 | `     "font-family: Verdana;"\` |
|        - | 10051 | `     "font-weight: bold;"\` |
|        - | 10052 | `     "font-style: normal;"\` |
|        - | 10053 | `     "font-size: medium;"\` |
|        - | 10054 | `     "text-transform: capitalize;"\` |
|        - | 10055 | `     "color: #0a328c;"\` |
|        - | 10056 | `     "}"\` |
|        - | 10057 | `     "p {"\` |
|        - | 10058 | `     "margin: 0 auto;"\` |
|        - | 10059 | `     "font-size: medium;"\` |
|        - | 10060 | `     "font-style: normal;"\` |
|        - | 10061 | `     "font-family: verdana;"\` |
|        - | 10062 | `     "}"\` |
|        - | 10063 | `"</style></head><body>"\` |
|        - | 10064 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 10065 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 10066 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 10067 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 10068 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 10069 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 10070 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 10071 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 10072 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 10073 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 10074 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 10075 |  |
|        - | 10076 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10077 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 10078 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 10079 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 10080 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10081 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 10082 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10083 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 10084 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10085 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 10086 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10087 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 10088 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 10089 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 10090 |  |
|        - | 10091 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 10092 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 10093 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 10094 | `"&nbsp;*<br>"\` |
|        - | 10095 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 10096 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 10097 | `"&nbsp;* are met:<br>"\` |
|        - | 10098 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 10099 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 10100 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 10101 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 10102 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 10103 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 10104 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 10105 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 10106 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 10107 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 10108 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 10109 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 10110 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 10111 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 10112 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 10113 | `"&nbsp;*<br>"\` |
|        - | 10114 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 10115 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 10116 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 10117 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 10118 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 10119 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 10120 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 10121 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 10122 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 10123 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 10124 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 10125 | `"&nbsp;*/<br>"\` |
|        - | 10126 | `"</span></small></small></p>"\` |
|        - | 10127 | `"</div></body></html>"` |
|        - | 10128 | `/*` |
|        - | 10129 | ` * bool ph7credits(void)` |
|        - | 10130 | ` * bool ph7info(void)` |
|        - | 10131 | ` * bool ph7copyright(void)` |
|        - | 10132 | ` *  Prints out the credits for PH7 engine` |
|        - | 10133 | ` * Parameters` |
|        - | 10134 | ` *  None` |
|        - | 10135 | ` * Return` |
|        - | 10136 | ` *  Always TRUE` |
|        - | 10137 | ` */` |
|        2 | 10138 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10139 |  |
|        3 | 10140 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 10141 | `	/* Expand the HTML page above*/` |
|        3 | 10142 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 10143 | `	ph7_context_output_format(` |
|        1 | 10144 | `		pCtx,` |
|        - | 10145 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 10146 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 10147 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 10148 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 10149 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 10150 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 10151 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 10152 | `#ifdef __WINNT__` |
|        - | 10153 | `		"Windows NT"` |
|        - | 10154 | `#elif defined(__UNIXES__)` |
|        - | 10155 | `		"UNIX-Like"` |
|        - | 10156 | `#else` |
|        - | 10157 | `		"Other OS"` |
|        - | 10158 | `#endif` |
|        - | 10159 | `		);` |
|        3 | 10160 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 10161 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10162 | `	SXUNUSED(apArg);` |
|        - | 10163 | `	/* Return TRUE */` |
|        - | 10164 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 10165 | `	return PH7_OK;` |
|        1 | 10166 |  |
|        - | 10167 | `/*` |
|        - | 10168 | ` * Section:` |
|        - | 10169 | ` *    URL related routines.` |
|        - | 10170 | ` * Status:` |
|        - | 10171 | ` *    Stable.` |
|        - | 10172 | ` */` |
|        - | 10173 | `/* Forward declaration */` |
|        - | 10174 | `static sxi32 VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen);` |
|        - | 10175 | `/*` |
|        - | 10176 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 10177 | ` *  Parse a URL and return its fields.` |
|        - | 10178 | ` * Parameters` |
|        - | 10179 | ` *  $url` |
|        - | 10180 | ` *   The URL to parse.` |
|        - | 10181 | ` * $component` |
|        - | 10182 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 10183 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 10184 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 10185 | ` *  in which case the return value will be an integer).` |
|        - | 10186 | ` * Return` |
|        - | 10187 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 10188 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 10189 | ` *  this array are:` |
|        - | 10190 | ` *   scheme - e.g. http` |
|        - | 10191 | ` *   host` |
|        - | 10192 | ` *   port` |
|        - | 10193 | ` *   user` |
|        - | 10194 | ` *   pass` |
|        - | 10195 | ` *   path` |
|        - | 10196 | ` *   query - after the question mark ?` |
|        - | 10197 | ` *   fragment - after the hashmark #` |
|        - | 10198 | ` * Note:` |
|        - | 10199 | ` *  FALSE is returned on failure.` |
|        - | 10200 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 10201 | ` *  with the standard PHP engine.` |
|        - | 10202 | ` */` |
|       28 | 10203 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10204 |  |
|        - | 10205 | `	const char *zStr; /* Input string */` |
|        - | 10206 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 10207 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 10208 | `	int nLen;` |
|        - | 10209 | `	sxi32 rc;` |
|       29 | 10210 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10211 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 10212 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10213 | `		return PH7_OK;` |
|        - | 10214 | `	}` |
|        - | 10215 | `	/* Extract the given URI */` |
|       29 | 10216 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 10217 | `	if( nLen < 1 ){` |
|        - | 10218 | `		/* Nothing to process,return FALSE */` |
|        3 | 10219 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10220 | `		return PH7_OK;` |
|        - | 10221 | `	}` |
|        - | 10222 | `	/* Get a parse */` |
|       27 | 10223 | `	rc = VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 10224 | `	if( rc != SXRET_OK ){` |
|        - | 10225 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 10226 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10227 | `		return PH7_OK;` |
|        - | 10228 | `	}` |
|       27 | 10229 | `	if( nArg > 1 ){` |
|      ! 0 | 10230 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 10231 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 10232 | `		switch(nComponent){` |
|      ! 0 | 10233 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 10234 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 10235 | `			if( pComp->nByte < 1 ){` |
|        - | 10236 | `				/* No available value,return NULL */` |
|      ! 0 | 10237 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10238 | `			}else{` |
|      ! 0 | 10239 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10240 | `			}` |
|      ! 0 | 10241 | `			break;` |
|      ! 0 | 10242 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 10243 | `			pComp = &sURI.sHost;` |
|      ! 0 | 10244 | `			if( pComp->nByte < 1 ){` |
|        - | 10245 | `				/* No available value,return NULL */` |
|      ! 0 | 10246 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10247 | `			}else{` |
|      ! 0 | 10248 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10249 | `			}` |
|      ! 0 | 10250 | `			break;` |
|      ! 0 | 10251 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 10252 | `			pComp = &sURI.sPort;` |
|      ! 0 | 10253 | `			if( pComp->nByte < 1 ){` |
|        - | 10254 | `				/* No available value,return NULL */` |
|      ! 0 | 10255 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10256 | `			}else{` |
|      ! 0 | 10257 | `				int iPort = 0;` |
|        - | 10258 | `				/* Cast the value to integer */` |
|      ! 0 | 10259 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 10260 | `				ph7_result_int(pCtx,iPort);` |
|        - | 10261 | `			}` |
|      ! 0 | 10262 | `			break;` |
|      ! 0 | 10263 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 10264 | `			pComp = &sURI.sUser;` |
|      ! 0 | 10265 | `			if( pComp->nByte < 1 ){` |
|        - | 10266 | `				/* No available value,return NULL */` |
|      ! 0 | 10267 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10268 | `			}else{` |
|      ! 0 | 10269 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10270 | `			}` |
|      ! 0 | 10271 | `			break;` |
|      ! 0 | 10272 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 10273 | `			pComp = &sURI.sPass;` |
|      ! 0 | 10274 | `			if( pComp->nByte < 1 ){` |
|        - | 10275 | `				/* No available value,return NULL */` |
|      ! 0 | 10276 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10277 | `			}else{` |
|      ! 0 | 10278 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10279 | `			}` |
|      ! 0 | 10280 | `			break;` |
|      ! 0 | 10281 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 10282 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 10283 | `			if( pComp->nByte < 1 ){` |
|        - | 10284 | `				/* No available value,return NULL */` |
|      ! 0 | 10285 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10286 | `			}else{` |
|      ! 0 | 10287 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10288 | `			}` |
|      ! 0 | 10289 | `			break;` |
|      ! 0 | 10290 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 10291 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 10292 | `			if( pComp->nByte < 1 ){` |
|        - | 10293 | `				/* No available value,return NULL */` |
|      ! 0 | 10294 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10295 | `			}else{` |
|      ! 0 | 10296 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10297 | `			}` |
|      ! 0 | 10298 | `			break;` |
|      ! 0 | 10299 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 10300 | `			pComp = &sURI.sPath;` |
|      ! 0 | 10301 | `			if( pComp->nByte < 1 ){` |
|        - | 10302 | `				/* No available value,return NULL */` |
|      ! 0 | 10303 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10304 | `			}else{` |
|      ! 0 | 10305 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10306 | `			}` |
|      ! 0 | 10307 | `			break;` |
|      ! 0 | 10308 | `		default:` |
|        - | 10309 | `			/* No such entry,return NULL */` |
|      ! 0 | 10310 | `			ph7_result_null(pCtx);` |
|      ! 0 | 10311 | `			break;` |
|        - | 10312 | `		}` |
|      ! 0 | 10313 | `	}else{` |
|        - | 10314 | `		ph7_value *pArray,*pValue;` |
|        - | 10315 | `		/* Return an associative array */` |
|       27 | 10316 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 10317 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 10318 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10319 | `			/* Out of memory */` |
|      ! 0 | 10320 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10321 | `			/* Return false */` |
|      ! 0 | 10322 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 10323 | `			return PH7_OK;` |
|        - | 10324 | `		}` |
|        - | 10325 | `		/* Fill the array */` |
|       27 | 10326 | `		pComp = &sURI.sScheme;` |
|       27 | 10327 | `		if( pComp->nByte > 0 ){` |
|       19 | 10328 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 10329 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 10330 | `		}` |
|        - | 10331 | `		/* Reset the string cursor */` |
|       27 | 10332 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10333 | `		pComp = &sURI.sHost;` |
|       27 | 10334 | `		if( pComp->nByte > 0 ){` |
|       25 | 10335 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 10336 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 10337 | `		}` |
|        - | 10338 | `		/* Reset the string cursor */` |
|       27 | 10339 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10340 | `		pComp = &sURI.sPort;` |
|       27 | 10341 | `		if( pComp->nByte > 0 ){` |
|       11 | 10342 | `			int iPort = 0;/* cc warning */` |
|        - | 10343 | `			/* Convert to integer */` |
|       11 | 10344 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 10345 | `			ph7_value_int(pValue,iPort);` |
|       11 | 10346 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 10347 | `		}` |
|        - | 10348 | `		/* Reset the string cursor */` |
|       27 | 10349 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10350 | `		pComp = &sURI.sUser;` |
|       27 | 10351 | `		if( pComp->nByte > 0 ){` |
|        7 | 10352 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10353 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 10354 | `		}` |
|        - | 10355 | `		/* Reset the string cursor */` |
|       27 | 10356 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10357 | `		pComp = &sURI.sPass;` |
|       27 | 10358 | `		if( pComp->nByte > 0 ){` |
|        7 | 10359 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10360 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 10361 | `		}` |
|        - | 10362 | `		/* Reset the string cursor */` |
|       27 | 10363 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10364 | `		pComp = &sURI.sPath;` |
|       27 | 10365 | `		if( pComp->nByte > 0 ){` |
|       17 | 10366 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 10367 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 10368 | `		}` |
|        - | 10369 | `		/* Reset the string cursor */` |
|       27 | 10370 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10371 | `		pComp = &sURI.sQuery;` |
|       27 | 10372 | `		if( pComp->nByte > 0 ){` |
|        5 | 10373 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10374 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 10375 | `		}` |
|        - | 10376 | `		/* Reset the string cursor */` |
|       27 | 10377 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10378 | `		pComp = &sURI.sFragment;` |
|       27 | 10379 | `		if( pComp->nByte > 0 ){` |
|        5 | 10380 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10381 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 10382 | `		}` |
|        - | 10383 | `		/* Return the created array */` |
|       27 | 10384 | `		ph7_result_value(pCtx,pArray);` |
|        - | 10385 | `		/* NOTE:` |
|        - | 10386 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 10387 | `		 * automatically as soon we return from this function.` |
|        - | 10388 | `		 */` |
|        - | 10389 | `	}` |
|        - | 10390 | `	/* All done */` |
|       27 | 10391 | `	return PH7_OK;` |
|       15 | 10392 |  |
|        - | 10393 | `/*` |
|        - | 10394 | ` * Section:` |
|        - | 10395 | ` *   Array related routines.` |
|        - | 10396 | ` * Status:` |
|        - | 10397 | ` *    Stable.` |
|        - | 10398 | ` * Note 2012-5-21 01:04:15:` |
|        - | 10399 | ` *  Array related functions that need access to the underlying` |
|        - | 10400 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 10401 | ` */` |
|        - | 10402 | `/*` |
|        - | 10403 | ` * The [compact()] function store it's state information in an instance` |
|        - | 10404 | ` * of the following structure.` |
|        - | 10405 | ` */` |
|        - | 10406 | `struct compact_data` |
|        - | 10407 |  |
|        - | 10408 | `	ph7_value *pArray;  /* Target array */` |
|        - | 10409 | `	int nRecCount;      /* Recursion count */` |
|        - | 10410 | `};` |
|        - | 10411 | `/*` |
|        - | 10412 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 10413 | ` */` |
|      ! 0 | 10414 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 10415 |  |
|      ! 0 | 10416 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 10417 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 10418 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 10419 | `	/* Act according to the hashmap value */` |
|      ! 0 | 10420 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 10421 | `		SyString sVar;` |
|      ! 0 | 10422 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 10423 | `		if( sVar.nByte > 0 ){` |
|        - | 10424 | `			/* Query the current frame */` |
|      ! 0 | 10425 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 10426 | `			/* ^` |
|        - | 10427 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 10428 | `			 */` |
|      ! 0 | 10429 | `			if( pKey ){` |
|        - | 10430 | `				/* Perform the insertion */` |
|      ! 0 | 10431 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 10432 | `			}` |
|      ! 0 | 10433 | `		}` |
|      ! 0 | 10434 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 10435 | `		int rc;` |
|        - | 10436 | `		/* Recursively traverse this array */` |
|      ! 0 | 10437 | `		pData->nRecCount++;` |
|      ! 0 | 10438 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 10439 | `		pData->nRecCount--;` |
|      ! 0 | 10440 | `		return rc;` |
|        - | 10441 | `	}` |
|      ! 0 | 10442 | `	return SXRET_OK;` |
|      ! 0 | 10443 |  |
|        - | 10444 | `/*` |
|        - | 10445 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 10446 | ` *  Create array containing variables and their values.` |
|        - | 10447 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 10448 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 10449 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 10450 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 10451 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 10452 | ` * Parameters` |
|        - | 10453 | ` *  $varname` |
|        - | 10454 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 10455 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 10456 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 10457 | ` *   it recursively.` |
|        - | 10458 | ` * Return` |
|        - | 10459 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 10460 | ` */` |
|        2 | 10461 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10462 |  |
|        - | 10463 | `	ph7_value *pArray,*pObj;` |
|        3 | 10464 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10465 | `	const char *zName;` |
|        - | 10466 | `	SyString sVar;` |
|        - | 10467 | `	int i,nLen;` |
|        3 | 10468 | `	if( nArg < 1 ){` |
|        - | 10469 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 10470 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10471 | `		return PH7_OK;` |
|        - | 10472 | `	}` |
|        - | 10473 | `	/* Create the array */` |
|        3 | 10474 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10475 | `	if( pArray == 0 ){` |
|        - | 10476 | `		/* Out of memory */` |
|      ! 0 | 10477 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10478 | `		/* Return NULL */` |
|      ! 0 | 10479 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10480 | `		return PH7_OK;` |
|        - | 10481 | `	}` |
|        - | 10482 | `	/* Perform the requested operation */` |
|        7 | 10483 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 10484 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 10485 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 10486 | `				struct compact_data sData;` |
|      ! 0 | 10487 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 10488 | `				/* Recursively walk the array */` |
|      ! 0 | 10489 | `				sData.nRecCount = 0;` |
|      ! 0 | 10490 | `				sData.pArray = pArray;` |
|      ! 0 | 10491 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 10492 | `			}` |
|      ! 0 | 10493 | `		}else{` |
|        - | 10494 | `			/* Extract variable name */` |
|        5 | 10495 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 10496 | `			if( nLen > 0 ){` |
|        5 | 10497 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 10498 | `				/* Check if the variable is available in the current frame */` |
|        5 | 10499 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 10500 | `				if( pObj ){` |
|        5 | 10501 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 10502 | `				}` |
|        2 | 10503 | `			}` |
|        - | 10504 | `		}` |
|        3 | 10505 | `	}` |
|        - | 10506 | `	/* Return the array */` |
|        3 | 10507 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10508 | `	return PH7_OK;` |
|        2 | 10509 |  |
|        - | 10510 | `/*` |
|        - | 10511 | ` * The [extract()] function store it's state information in an instance` |
|        - | 10512 | ` * of the following structure.` |
|        - | 10513 | ` */` |
|        - | 10514 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 10515 | `struct extract_aux_data` |
|        - | 10516 |  |
|        - | 10517 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 10518 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 10519 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 10520 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 10521 | `	int iFlags;           /* Control flags */` |
|        - | 10522 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 10523 | `};` |
|        - | 10524 | `/* Forward declaration */` |
|        - | 10525 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 10526 | `/*` |
|        - | 10527 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 10528 | ` *   Import variables into the current symbol table from an array.` |
|        - | 10529 | ` * Parameters` |
|        - | 10530 | ` * $var_array` |
|        - | 10531 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 10532 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 10533 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 10534 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 10535 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 10536 | ` * $extract_type` |
|        - | 10537 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 10538 | ` *  It can be one of the following values:` |
|        - | 10539 | ` *   EXTR_OVERWRITE` |
|        - | 10540 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 10541 | ` *   EXTR_SKIP` |
|        - | 10542 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 10543 | ` *   EXTR_PREFIX_SAME` |
|        - | 10544 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 10545 | ` *   EXTR_PREFIX_ALL` |
|        - | 10546 | ` *       Prefix all variable names with prefix.` |
|        - | 10547 | ` *   EXTR_PREFIX_INVALID` |
|        - | 10548 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 10549 | ` *   EXTR_IF_EXISTS` |
|        - | 10550 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 10551 | ` *       otherwise do nothing.` |
|        - | 10552 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 10553 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 10554 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 10555 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 10556 | ` *      the current symbol table.` |
|        - | 10557 | ` * $prefix` |
|        - | 10558 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 10559 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 10560 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 10561 | ` *  underscore character.` |
|        - | 10562 | ` * Return` |
|        - | 10563 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 10564 | ` */` |
|        4 | 10565 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10566 |  |
|        - | 10567 | `	extract_aux_data sAux;` |
|        - | 10568 | `	ph7_hashmap *pMap;` |
|        5 | 10569 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 10570 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 10571 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 10572 | `		return PH7_OK;` |
|        - | 10573 | `	}` |
|        - | 10574 | `	/* Point to the target hashmap */` |
|        5 | 10575 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 10576 | `	if( pMap->nEntry < 1 ){` |
|        - | 10577 | `		/* Empty map,return  0 */` |
|      ! 0 | 10578 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 10579 | `		return PH7_OK;` |
|        - | 10580 | `	}` |
|        - | 10581 | `	/* Prepare the aux data */` |
|        5 | 10582 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 10583 | `	if( nArg > 1 ){` |
|        3 | 10584 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 10585 | `		if( nArg > 2 ){` |
|      ! 0 | 10586 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 10587 | `		}` |
|        1 | 10588 | `	}` |
|        5 | 10589 | `	sAux.pVm = pCtx->pVm;` |
|        - | 10590 | `	/* Invoke the worker callback */` |
|        5 | 10591 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 10592 | `	/* Number of variables successfully imported */` |
|        5 | 10593 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 10594 | `	return PH7_OK;` |
|        3 | 10595 |  |
|        - | 10596 | `/*` |
|        - | 10597 | ` * Worker callback for the [extract()] function defined` |
|        - | 10598 | ` * below.` |
|        - | 10599 | ` */` |
|        8 | 10600 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 10601 |  |
|        9 | 10602 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 10603 | `	int iFlags = pAux->iFlags;` |
|        9 | 10604 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 10605 | `	ph7_value *pObj;` |
|        - | 10606 | `	SyString sVar;` |
|        9 | 10607 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 10608 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 10609 | `	}` |
|        - | 10610 | `	/* Perform a string cast */` |
|        9 | 10611 | `	PH7_MemObjToString(pKey);` |
|        9 | 10612 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 10613 | `		/* Unavailable variable name */` |
|      ! 0 | 10614 | `		return SXRET_OK;` |
|        - | 10615 | `	}` |
|        9 | 10616 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 10617 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 10618 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 10619 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 10620 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10621 | `			);` |
|      ! 0 | 10622 | `	}else{` |
|       13 | 10623 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 10624 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 10625 | `	}` |
|        9 | 10626 | `	sVar.zString = pAux->zWorker;` |
|        - | 10627 | `	/* Try to extract the variable */` |
|        9 | 10628 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 10629 | `	if( pObj ){` |
|        - | 10630 | `		/* Collision */` |
|        3 | 10631 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 10632 | `			return SXRET_OK;` |
|        - | 10633 | `		}` |
|        3 | 10634 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 10635 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 10636 | `				/* Already prefixed */` |
|      ! 0 | 10637 | `				return SXRET_OK;` |
|        - | 10638 | `			}` |
|      ! 0 | 10639 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 10640 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 10641 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10642 | `				);` |
|      ! 0 | 10643 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 10644 | `		}` |
|        2 | 10645 | `	}else{` |
|        - | 10646 | `		/* Create the variable */` |
|        7 | 10647 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 10648 | `	}` |
|        9 | 10649 | `	if( pObj ){` |
|        - | 10650 | `		/* Overwrite the old value */` |
|        9 | 10651 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 10652 | `		/* Increment counter */` |
|        9 | 10653 | `		pAux->iCount++;` |
|        4 | 10654 | `	}` |
|        9 | 10655 | `	return SXRET_OK;` |
|        5 | 10656 |  |
|        - | 10657 | `/*` |
|        - | 10658 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 10659 | ` * defined below.` |
|        - | 10660 | ` */` |
|        2 | 10661 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 10662 |  |
|        3 | 10663 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 10664 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 10665 | `	ph7_value *pObj;` |
|        - | 10666 | `	SyString sVar;` |
|        - | 10667 | `	/* Perform a string cast */` |
|        3 | 10668 | `	PH7_MemObjToString(pKey);` |
|        3 | 10669 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 10670 | `		/* Unavailable variable name */` |
|      ! 0 | 10671 | `		return SXRET_OK;` |
|        - | 10672 | `	}` |
|        3 | 10673 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 10674 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 10675 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 10676 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 10677 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10678 | `			);` |
|        2 | 10679 | `	}else{` |
|      ! 0 | 10680 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 10681 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 10682 | `	}` |
|        3 | 10683 | `	sVar.zString = pAux->zWorker;` |
|        - | 10684 | `	/* Extract the variable */` |
|        3 | 10685 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 10686 | `	if( pObj ){` |
|        3 | 10687 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 10688 | `	}` |
|        3 | 10689 | `	return SXRET_OK;` |
|        2 | 10690 |  |
|        - | 10691 | `/*` |
|        - | 10692 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 10693 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 10694 | ` * Parameters` |
|        - | 10695 | ` * $types` |
|        - | 10696 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 10697 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 10698 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 10699 | ` *  POST includes the POST uploaded file information.` |
|        - | 10700 | ` *  Note:` |
|        - | 10701 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 10702 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 10703 | ` * $prefix` |
|        - | 10704 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 10705 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 10706 | ` *  variable named $pref_userid.` |
|        - | 10707 | ` * Return` |
|        - | 10708 | ` *  TRUE on success or FALSE on failure.` |
|        - | 10709 | ` */` |
|        2 | 10710 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10711 |  |
|        - | 10712 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 10713 | `	extract_aux_data sAux;` |
|        - | 10714 | `	int nLen,nPrefixLen;` |
|        - | 10715 | `	ph7_value *pSuper;` |
|        - | 10716 | `	ph7_vm *pVm;` |
|        - | 10717 | `	/* By default import only $_GET variables  */` |
|        3 | 10718 | `	zImport = "G";` |
|        3 | 10719 | `	nLen = (int)sizeof(char);` |
|        3 | 10720 | `	zPrefix = 0;` |
|        3 | 10721 | `	nPrefixLen = 0;` |
|        3 | 10722 | `	if( nArg > 0 ){` |
|        3 | 10723 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 10724 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 10725 | `		}` |
|        3 | 10726 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 10727 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 10728 | `		}` |
|        1 | 10729 | `	}` |
|        - | 10730 | `	/* Point to the underlying VM */` |
|        3 | 10731 | `	pVm = pCtx->pVm;` |
|        - | 10732 | `	/* Initialize the aux data */` |
|        3 | 10733 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 10734 | `	sAux.zPrefix = zPrefix;` |
|        3 | 10735 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 10736 | `	sAux.pVm = pVm;` |
|        - | 10737 | `	/* Extract */` |
|        3 | 10738 | `	zEnd = &zImport[nLen];` |
|        5 | 10739 | `	while( zImport < zEnd ){` |
|        3 | 10740 | `		int c = zImport[0];` |
|        3 | 10741 | `		pSuper = 0;` |
|        3 | 10742 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 10743 | `			/* Import $_GET variables */` |
|        3 | 10744 | `			pSuper = VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 10745 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 10746 | `			/* Import $_POST variables */` |
|      ! 0 | 10747 | `			pSuper = VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 10748 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 10749 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 10750 | `			pSuper = VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 10751 | `		}` |
|        3 | 10752 | `		if( pSuper ){` |
|        - | 10753 | `			/* Iterate throw array entries */` |
|        3 | 10754 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 10755 | `		}` |
|        - | 10756 | `		/* Advance the cursor */` |
|        3 | 10757 | `		zImport++;` |
|        1 | 10758 | `	}` |
|        - | 10759 | `	/* All done,return TRUE*/` |
|        3 | 10760 | `	ph7_result_bool(pCtx,0);` |
|        3 | 10761 | `	return PH7_OK;` |
|        1 | 10762 |  |
|        - | 10763 | `/*` |
|        - | 10764 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 10765 | ` * Refer to the eval() language construct implementation for more` |
|        - | 10766 | ` * information.` |
|        - | 10767 | ` */` |
|     8180 | 10768 | `static sxi32 VmEvalChunk(` |
|        - | 10769 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 10770 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 10771 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 10772 | `	int iFlags,         /* Compile flag */` |
|        - | 10773 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 10774 | `	)` |
|        2 | 10775 |  |
|        - | 10776 | `	SySet *pByteCode,aByteCode;` |
|     8182 | 10777 | `	ProcConsumer xErr = 0;` |
|     8182 | 10778 | `	void *pErrData = 0;` |
|        - | 10779 | `	/* Initialize bytecode container */` |
|     8182 | 10780 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     8182 | 10781 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 10782 | `	/* Reset the code generator */` |
|     8182 | 10783 | `	if( bTrueReturn ){` |
|        - | 10784 | `		/* Included file,log compile-time errors */` |
|     6665 | 10785 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     6665 | 10786 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3332 | 10787 | `	}` |
|     8182 | 10788 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 10789 | `	/* Swap bytecode container */` |
|     8182 | 10790 | `	pByteCode = pVm->pByteContainer;` |
|     8182 | 10791 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 10792 | `	/* Compile the chunk */` |
|     8182 | 10793 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    12272 | 10794 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 10795 | `		/* Compilation error,return false */` |
|        3 | 10796 | `		if( pCtx ){` |
|        3 | 10797 | `			ph7_result_bool(pCtx,0);` |
|        1 | 10798 | `		}` |
|        2 | 10799 | `	}else{` |
|        - | 10800 | `		/* Mount any newly defined classes */` |
|        - | 10801 | `		SyHashEntry *pEntry;` |
|        - | 10802 | `		ph7_class *pClass;` |
|        - | 10803 | `		ph7_value sResult; /* Return value */` |
|        - | 10804 | `		sxi32 rc;` |
|     8180 | 10805 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   239287 | 10806 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   227020 | 10807 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 10808 | `			/* Only mount classes that haven't been mounted yet */` |
|   227020 | 10809 | `			if( !pClass->bMounted ){` |
|    45856 | 10810 | `				rc = VmMountUserClass(pVm,pClass);` |
|    45856 | 10811 | `				if( rc != SXRET_OK ){` |
|        - | 10812 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 10813 | `					if( pCtx ){` |
|      ! 0 | 10814 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 10815 | `					}` |
|      ! 0 | 10816 | `					goto Cleanup;` |
|        - | 10817 | `				}` |
|    22927 | 10818 | `			}` |
|        2 | 10819 | `		}` |
|     8180 | 10820 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 10821 | `			/* Out of memory */` |
|      ! 0 | 10822 | `			if( pCtx ){` |
|      ! 0 | 10823 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 10824 | `			}` |
|      ! 0 | 10825 | `			goto Cleanup;` |
|        - | 10826 | `		}` |
|     8180 | 10827 | `		if( bTrueReturn ){` |
|        - | 10828 | `			/* Assume a boolean true return value */` |
|     6665 | 10829 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3333 | 10830 | `		}else{` |
|        - | 10831 | `			/* Assume a null return value */` |
|     1516 | 10832 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 10833 | `		}` |
|        - | 10834 | `		/* Execute the compiled chunk */` |
|     8180 | 10835 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|     8180 | 10836 | `		if( pCtx ){` |
|        - | 10837 | `			/* Set the execution result */` |
|     6682 | 10838 | `			ph7_result_value(pCtx,&sResult);` |
|     3340 | 10839 | `		}` |
|     8180 | 10840 | `		PH7_MemObjRelease(&sResult);` |
|        - | 10841 | `	}` |
|     4090 | 10842 | `Cleanup:` |
|        - | 10843 | `	/* Cleanup the mess left behind */` |
|     8182 | 10844 | `	pVm->pByteContainer = pByteCode;` |
|     8182 | 10845 | `	SySetRelease(&aByteCode);` |
|     8182 | 10846 | `	return SXRET_OK;` |
|        2 | 10847 |  |
|        - | 10848 | `/*` |
|        - | 10849 | ` * value eval(string $code)` |
|        - | 10850 | ` *   Evaluate a string as PHP code.` |
|        - | 10851 | ` * Parameter` |
|        - | 10852 | ` *  code: PHP code to evaluate.` |
|        - | 10853 | ` * Return` |
|        - | 10854 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 10855 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 10856 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 10857 | ` */` |
|       16 | 10858 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10859 |  |
|        - | 10860 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 | 10861 | `	if( nArg < 1 ){` |
|        - | 10862 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10863 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10864 | `		return SXRET_OK;` |
|        - | 10865 | `	}` |
|        - | 10866 | `	/* Chunk to evaluate */` |
|       18 | 10867 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 | 10868 | `	if( sChunk.nByte < 1 ){` |
|        - | 10869 | `		/* Empty string,return NULL */` |
|        3 | 10870 | `		ph7_result_null(pCtx);` |
|        3 | 10871 | `		return SXRET_OK;` |
|        - | 10872 | `	}` |
|        - | 10873 | `	/* Eval the chunk */` |
|       16 | 10874 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 | 10875 | `	return SXRET_OK;` |
|       10 | 10876 |  |
|        - | 10877 | `/*` |
|        - | 10878 | ` * Check if a file path is already included.` |
|        - | 10879 | ` */` |
|    13324 | 10880 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 | 10881 |  |
|        - | 10882 | `	SyString *aEntries;` |
|        - | 10883 | `	sxu32 n;` |
|    13325 | 10884 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 10885 | `	/* Perform a linear search */` |
| 44371197 | 10886 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 44357879 | 10887 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 10888 | `			/* Already included */` |
|        7 | 10889 | `			return TRUE;` |
|        - | 10890 | `		}` |
| 22178937 | 10891 | `	}` |
|    13319 | 10892 | `	return FALSE;` |
|     6663 | 10893 |  |
|        - | 10894 | `/*` |
|        - | 10895 | ` * Push a file path in the appropriate VM container.` |
|        - | 10896 | ` */` |
|    14814 | 10897 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 10898 |  |
|        - | 10899 | `	SyString sPath;` |
|        - | 10900 | `	char *zDup;` |
|        - | 10901 | `#ifdef __WINNT__` |
|        - | 10902 | `	char *zCur;` |
|        - | 10903 | `#endif` |
|        - | 10904 | `	sxi32 rc;` |
|    14816 | 10905 | `	if( nLen < 0 ){` |
|     1492 | 10906 | `		nLen = SyStrlen(zPath);` |
|      745 | 10907 | `	}` |
|        - | 10908 | `	/* Duplicate the file path first */` |
|    14816 | 10909 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    14816 | 10910 | `	if( zDup == 0 ){` |
|      ! 0 | 10911 | `		return SXERR_MEM;` |
|        - | 10912 | `	}` |
|        - | 10913 | `#ifdef __WINNT__` |
|        - | 10914 | `	/* Normalize path on windows` |
|        - | 10915 | `	 * Example:` |
|        - | 10916 | `	 *    Path/To/File.php` |
|        - | 10917 | `	 * becomes` |
|        - | 10918 | `	 *   path\to\file.php` |
|        - | 10919 | `	 */` |
|        2 | 10920 | `	zCur = zDup;` |
|        2 | 10921 | `	while( zCur[0] != 0 ){` |
|        2 | 10922 | `		if( zCur[0] == '/' ){` |
|        2 | 10923 | `			zCur[0] = '\\';` |
|        2 | 10924 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 10925 | `			int c = SyToLower(zCur[0]);` |
|        1 | 10926 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 10927 | `		}` |
|        2 | 10928 | `		zCur++;` |
|        2 | 10929 | `	}` |
|        - | 10930 | `#endif` |
|        - | 10931 | `	/* Install the file path */` |
|    14816 | 10932 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    14816 | 10933 | `	if( !bMain ){` |
|    13325 | 10934 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 10935 | `			/* Already included */` |
|        7 | 10936 | `			*pNew = 0;` |
|        4 | 10937 | `		}else{` |
|        - | 10938 | `			/* Insert in the corresponding container */` |
|    13319 | 10939 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    13319 | 10940 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10941 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 10942 | `				return rc;` |
|        - | 10943 | `			}` |
|    13319 | 10944 | `			*pNew = 1;` |
|        - | 10945 | `		}` |
|     6662 | 10946 | `	}` |
|    14816 | 10947 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    14816 | 10948 | `	return SXRET_OK;` |
|     7409 | 10949 |  |
|        - | 10950 | `/*` |
|        - | 10951 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 10952 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 10953 | ` * indicates failure.` |
|        - | 10954 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 10955 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 10956 | ` * operations.` |
|        - | 10957 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 10958 | ` * this function is a no-op.` |
|        - | 10959 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 10960 | ` * constructs for more information.` |
|        - | 10961 | ` */` |
|     6670 | 10962 | `static sxi32 VmExecIncludedFile(` |
|        - | 10963 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 10964 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 10965 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 10966 | `	 )` |
|        2 | 10967 |  |
|        - | 10968 | `	sxi32 rc;` |
|        - | 10969 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10970 | `	const ph7_io_stream *pStream;` |
|        - | 10971 | `	SyBlob sContents;` |
|        - | 10972 | `	void *pHandle;` |
|        - | 10973 | `	ph7_vm *pVm;` |
|        - | 10974 | `	int isNew;` |
|        - | 10975 | `	/* Initialize fields */` |
|     6672 | 10976 | `	pVm = pCtx->pVm;` |
|     6672 | 10977 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     6672 | 10978 | `	isNew = 0;` |
|        - | 10979 | `	/* Extract the associated stream */` |
|     6672 | 10980 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 10981 | `	/*` |
|        - | 10982 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 10983 | `	 * in a read-only mode.` |
|        - | 10984 | `	 */` |
|     6672 | 10985 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     6672 | 10986 | `	if( pHandle == 0 ){` |
|        3 | 10987 | `		return SXERR_IO;` |
|        - | 10988 | `	}` |
|     6669 | 10989 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     6669 | 10990 | `	if( IncludeOnce && !isNew ){` |
|        - | 10991 | `		/* Already included */` |
|        5 | 10992 | `		rc = SXERR_EXISTS;` |
|        3 | 10993 | `	}else{` |
|        - | 10994 | `		/* Read the whole file contents */` |
|     6665 | 10995 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     6665 | 10996 | `		if( rc == SXRET_OK ){` |
|        - | 10997 | `			SyString sScript;` |
|        - | 10998 | `			/* Compile and execute the script */` |
|     6665 | 10999 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     6665 | 11000 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3332 | 11001 | `		}` |
|        - | 11002 | `	}` |
|        - | 11003 | `	/* Pop from the set of included file */` |
|     6669 | 11004 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 11005 | `	/* Close the handle */` |
|     6669 | 11006 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 11007 | `	/* Release the working buffer */` |
|     6669 | 11008 | `	SyBlobRelease(&sContents);` |
|        - | 11009 | `#else` |
|        - | 11010 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 11011 | `	SXUNUSED(pPath);` |
|        - | 11012 | `	SXUNUSED(IncludeOnce);` |
|        - | 11013 | `	rc = SXERR_IO;` |
|        - | 11014 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     6669 | 11015 | `	return rc;` |
|     3337 | 11016 |  |
|        - | 11017 | `/*` |
|        - | 11018 | ` * string get_include_path(void)` |
|        - | 11019 | ` *  Gets the current include_path configuration option.` |
|        - | 11020 | ` * Parameter` |
|        - | 11021 | ` *  None` |
|        - | 11022 | ` * Return` |
|        - | 11023 | ` *  Included paths as a string` |
|        - | 11024 | ` */` |
|        2 | 11025 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11026 |  |
|        3 | 11027 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11028 | `	SyString *aEntry;` |
|        - | 11029 | `	int dir_sep;` |
|        - | 11030 | `	sxu32 n;` |
|        - | 11031 | `#ifdef __WINNT__` |
|        1 | 11032 | `	dir_sep = ';';` |
|        - | 11033 | `#else` |
|        - | 11034 | `	/* Assume UNIX path separator */` |
|        2 | 11035 | `	dir_sep = ':';` |
|        - | 11036 | `#endif` |
|        1 | 11037 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11038 | `	SXUNUSED(apArg);` |
|        - | 11039 | `	/* Point to the list of import paths */` |
|        3 | 11040 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 11041 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 11042 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 11043 | `		if( n > 0 ){` |
|        - | 11044 | `			/* Append dir seprator */` |
|      ! 0 | 11045 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 11046 | `		}` |
|        - | 11047 | `		/* Append path */` |
|        3 | 11048 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 11049 | `	}` |
|        3 | 11050 | `	return PH7_OK;` |
|        1 | 11051 |  |
|        - | 11052 | `/*` |
|        - | 11053 | ` * string get_get_included_files(void)` |
|        - | 11054 | ` *  Gets the current include_path configuration option.` |
|        - | 11055 | ` * Parameter` |
|        - | 11056 | ` *  None` |
|        - | 11057 | ` * Return` |
|        - | 11058 | ` *  Included paths as a string` |
|        - | 11059 | ` */` |
|        2 | 11060 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11061 |  |
|        3 | 11062 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 11063 | `	ph7_value *pArray,*pWorker;` |
|        - | 11064 | `	SyString *pEntry;` |
|        - | 11065 | `	int c,d;` |
|        - | 11066 | `	/* Create an array and a working value */` |
|        3 | 11067 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 11068 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11069 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 11070 | `		/* Out of memory,return null */` |
|      ! 0 | 11071 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11072 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11073 | `		SXUNUSED(apArg);` |
|      ! 0 | 11074 | `		return PH7_OK;` |
|        - | 11075 | `	}` |
|        3 | 11076 | `	c = d = '/';` |
|        - | 11077 | `#ifdef __WINNT__` |
|        1 | 11078 | `	d = '\\';` |
|        - | 11079 | `#endif` |
|        - | 11080 | `	/* Iterate throw entries */` |
|        3 | 11081 | `	SySetResetCursor(pFiles);` |
|     2879 | 11082 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 11083 | `		const char *zBase,*zEnd;` |
|        - | 11084 | `		int iLen;` |
|        - | 11085 | `		/* reset the string cursor */` |
|     2877 | 11086 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 11087 | `		/* Extract base name */` |
|     2877 | 11088 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 11089 | `		/* Ignore trailing '/' */` |
|     4315 | 11090 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 11091 | `			zEnd--;` |
|      ! 0 | 11092 | `		}` |
|     2877 | 11093 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|    82405 | 11094 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    78091 | 11095 | `			zEnd--;` |
|        1 | 11096 | `		}` |
|     2877 | 11097 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     2877 | 11098 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 11099 | `		/* Copy entry name */` |
|     2877 | 11100 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 11101 | `		/* Perform the insertion */` |
|     2877 | 11102 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 11103 | `	}` |
|        - | 11104 | `	/* All done,return the created array */` |
|        3 | 11105 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11106 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 11107 | `	 * by the engine as soon we return from this foreign` |
|        - | 11108 | `	 * function.` |
|        - | 11109 | `	 */` |
|        3 | 11110 | `	return PH7_OK;` |
|        2 | 11111 |  |
|        - | 11112 | `/*` |
|        - | 11113 | ` * include:` |
|        - | 11114 | ` * According to the PHP reference manual.` |
|        - | 11115 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 11116 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 11117 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 11118 | ` *  include() will finally check in the calling script's own directory` |
|        - | 11119 | ` *  and the current working directory before failing. The include()` |
|        - | 11120 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 11121 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 11122 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 11123 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 11124 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 11125 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 11126 | ` *  directory to find the requested file.` |
|        - | 11127 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 11128 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 11129 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 11130 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 11131 | ` */` |
|     6658 | 11132 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11133 |  |
|        - | 11134 | `	SyString sFile;` |
|        - | 11135 | `	sxi32 rc;` |
|     6660 | 11136 | `	if( nArg < 1 ){` |
|        - | 11137 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11138 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11139 | `		return SXRET_OK;` |
|        - | 11140 | `	}` |
|        - | 11141 | `	/* File to include */` |
|     6660 | 11142 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     6660 | 11143 | `	if( sFile.nByte < 1 ){` |
|        - | 11144 | `		/* Empty string,return NULL */` |
|      ! 0 | 11145 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11146 | `		return SXRET_OK;` |
|        - | 11147 | `	}` |
|        - | 11148 | `	/* Open,compile and execute the desired script */` |
|     6660 | 11149 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     6660 | 11150 | `	if( rc != SXRET_OK ){` |
|        - | 11151 | `		/* Emit a warning and return false */` |
|        3 | 11152 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 11153 | `		ph7_result_bool(pCtx,0);` |
|        1 | 11154 | `	}` |
|     6660 | 11155 | `	return SXRET_OK;` |
|     3331 | 11156 |  |
|        - | 11157 | `/*` |
|        - | 11158 | ` * include_once:` |
|        - | 11159 | ` *  According to the PHP reference manual.` |
|        - | 11160 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 11161 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 11162 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 11163 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 11164 | ` *   just once.` |
|        - | 11165 | ` */` |
|        4 | 11166 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11167 |  |
|        - | 11168 | `	SyString sFile;` |
|        - | 11169 | `	sxi32 rc;` |
|        5 | 11170 | `	if( nArg < 1 ){` |
|        - | 11171 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11172 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11173 | `		return SXRET_OK;` |
|        - | 11174 | `	}` |
|        - | 11175 | `	/* File to include */` |
|        5 | 11176 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11177 | `	if( sFile.nByte < 1 ){` |
|        - | 11178 | `		/* Empty string,return NULL */` |
|      ! 0 | 11179 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11180 | `		return SXRET_OK;` |
|        - | 11181 | `	}` |
|        - | 11182 | `	/* Open,compile and execute the desired script */` |
|        5 | 11183 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11184 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11185 | `		/* File already included,return TRUE */` |
|        3 | 11186 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11187 | `		return SXRET_OK;` |
|        - | 11188 | `	}` |
|        3 | 11189 | `	if( rc != SXRET_OK ){` |
|        - | 11190 | `		/* Emit a warning and return false */` |
|      ! 0 | 11191 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11192 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11193 | ` 	}` |
|        3 | 11194 | `	return SXRET_OK;` |
|        3 | 11195 |  |
|        - | 11196 | `/*` |
|        - | 11197 | ` * require.` |
|        - | 11198 | ` *  According to the PHP reference manual.` |
|        - | 11199 | ` *   require() is identical to include() except upon failure it will` |
|        - | 11200 | ` *   also produce a fatal level error.` |
|        - | 11201 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 11202 | ` *   emits a warning  which allows the script to continue.` |
|        - | 11203 | ` */` |
|        4 | 11204 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11205 |  |
|        - | 11206 | `	SyString sFile;` |
|        - | 11207 | `	sxi32 rc;` |
|        5 | 11208 | `	if( nArg < 1 ){` |
|        - | 11209 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11210 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11211 | `		return SXRET_OK;` |
|        - | 11212 | `	}` |
|        - | 11213 | `	/* File to include */` |
|        5 | 11214 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11215 | `	if( sFile.nByte < 1 ){` |
|        - | 11216 | `		/* Empty string,return NULL */` |
|      ! 0 | 11217 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11218 | `		return SXRET_OK;` |
|        - | 11219 | `	}` |
|        - | 11220 | `	/* Open,compile and execute the desired script */` |
|        5 | 11221 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 11222 | `	if( rc != SXRET_OK ){` |
|        - | 11223 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11224 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11225 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11226 | `		return PH7_ABORT;` |
|        - | 11227 | `	}` |
|        5 | 11228 | `	return SXRET_OK;` |
|        3 | 11229 |  |
|        - | 11230 | `/*` |
|        - | 11231 | ` * require_once:` |
|        - | 11232 | ` *  According to the PHP reference manual.` |
|        - | 11233 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 11234 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 11235 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 11236 | ` *   and how it differs from its non _once siblings.` |
|        - | 11237 | ` */` |
|        4 | 11238 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11239 |  |
|        - | 11240 | `	SyString sFile;` |
|        - | 11241 | `	sxi32 rc;` |
|        5 | 11242 | `	if( nArg < 1 ){` |
|        - | 11243 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11244 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11245 | `		return SXRET_OK;` |
|        - | 11246 | `	}` |
|        - | 11247 | `	/* File to include */` |
|        5 | 11248 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11249 | `	if( sFile.nByte < 1 ){` |
|        - | 11250 | `		/* Empty string,return NULL */` |
|      ! 0 | 11251 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11252 | `		return SXRET_OK;` |
|        - | 11253 | `	}` |
|        - | 11254 | `	/* Open,compile and execute the desired script */` |
|        5 | 11255 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11256 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11257 | `		/* File already included,return TRUE */` |
|        3 | 11258 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11259 | `		return SXRET_OK;` |
|        - | 11260 | `	}` |
|        3 | 11261 | `	if( rc != SXRET_OK ){` |
|        - | 11262 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11263 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11264 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11265 | `		return PH7_ABORT;` |
|        - | 11266 | `	}` |
|        3 | 11267 | `	return SXRET_OK;` |
|        3 | 11268 |  |
|        - | 11269 | `/*` |
|        - | 11270 | ` * Section:` |
|        - | 11271 | ` *  Command line arguments processing.` |
|        - | 11272 | ` * Status:` |
|        - | 11273 | ` *    Stable.` |
|        - | 11274 | ` */` |
|        - | 11275 | `/*` |
|        - | 11276 | ` * Check if a short option argument [i.e: -c] is available in the command` |
|        - | 11277 | ` * line string. Return a pointer to the start of the stream on success.` |
|        - | 11278 | ` * NULL otherwise.` |
|        - | 11279 | ` */` |
|        6 | 11280 | `static const char * VmFindShortOpt(int c,const char *zIn,const char *zEnd)` |
|        1 | 11281 |  |
|      319 | 11282 | `	while( zIn < zEnd ){` |
|      313 | 11283 | `		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == c ){` |
|        - | 11284 | `			/* Got one */` |
|      ! 0 | 11285 | `			return &zIn[1];` |
|        - | 11286 | `		}` |
|        - | 11287 | `		/* Advance the cursor */` |
|      313 | 11288 | `		zIn++;` |
|        1 | 11289 | `	}` |
|        - | 11290 | `	/* No such option */` |
|        7 | 11291 | `	return 0;` |
|        4 | 11292 |  |
|        - | 11293 | `/*` |
|        - | 11294 | ` * Check if a long option argument [i.e: --opt] is available in the command` |
|        - | 11295 | ` * line string. Return a pointer to the start of the stream on success.` |
|        - | 11296 | ` * NULL otherwise.` |
|        - | 11297 | ` */` |
|      ! 0 | 11298 | `static const char * VmFindLongOpt(const char *zLong,int nByte,const char *zIn,const char *zEnd)` |
|      ! 0 | 11299 |  |
|        - | 11300 | `	const char *zOpt;` |
|      ! 0 | 11301 | `	while( zIn < zEnd ){` |
|      ! 0 | 11302 | `		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == '-' ){` |
|      ! 0 | 11303 | `			zIn += 2;` |
|      ! 0 | 11304 | `			zOpt = zIn;` |
|      ! 0 | 11305 | `			while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 11306 | `				if( zIn[0] == '=' /* --opt=val */){` |
|      ! 0 | 11307 | `					break;` |
|        - | 11308 | `				}` |
|      ! 0 | 11309 | `				zIn++;` |
|      ! 0 | 11310 | `			}` |
|        - | 11311 | `			/* Test */` |
|      ! 0 | 11312 | `			if( (int)(zIn-zOpt) == nByte && SyMemcmp(zOpt,zLong,nByte) == 0 ){` |
|        - | 11313 | `				/* Got one,return it's value */` |
|      ! 0 | 11314 | `				return zIn;` |
|        - | 11315 | `			}` |
|        - | 11316 |  |
|      ! 0 | 11317 | `		}else{` |
|      ! 0 | 11318 | `			zIn++;` |
|        - | 11319 | `		}` |
|      ! 0 | 11320 | `	}` |
|        - | 11321 | `	/* No such option */` |
|      ! 0 | 11322 | `	return 0;` |
|      ! 0 | 11323 |  |
|        - | 11324 | `/*` |
|        - | 11325 | ` * Long option [i.e: --opt] arguments private data structure.` |
|        - | 11326 | ` */` |
|        - | 11327 | `struct getopt_long_opt` |
|        - | 11328 |  |
|        - | 11329 | `	const char *zArgIn,*zArgEnd; /* Command line arguments */` |
|        - | 11330 | `	ph7_value *pWorker;  /* Worker variable*/` |
|        - | 11331 | `	ph7_value *pArray;   /* getopt() return value */` |
|        - | 11332 | `	ph7_context *pCtx;   /* Call Context */` |
|        - | 11333 | `};` |
|        - | 11334 | `/* Forward declaration */` |
|        - | 11335 | `static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11336 | `/*` |
|        - | 11337 | ` * Extract short or long argument option values.` |
|        - | 11338 | ` */` |
|      ! 0 | 11339 | `static void VmExtractOptArgValue(` |
|        - | 11340 | `	ph7_value *pArray,  /* getopt() return value */` |
|        - | 11341 | `	ph7_value *pWorker, /* Worker variable */` |
|        - | 11342 | `	const char *zArg,   /* Argument stream */` |
|        - | 11343 | `	const char *zArgEnd,/* End of the argument stream  */` |
|        - | 11344 | `	int need_val,       /* TRUE to fetch option argument */` |
|        - | 11345 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 11346 | `	const char *zName   /* Option name */)` |
|      ! 0 | 11347 |  |
|      ! 0 | 11348 | `	ph7_value_bool(pWorker,0);` |
|      ! 0 | 11349 | `	if( !need_val ){` |
|        - | 11350 | `		/*` |
|        - | 11351 | `		 * Option does not need arguments.` |
|        - | 11352 | `		 * Insert the option name and a boolean FALSE.` |
|        - | 11353 | `		 */` |
|      ! 0 | 11354 | `		ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11355 | `	}else{` |
|        - | 11356 | `		const char *zCur;` |
|        - | 11357 | `		/* Extract option argument */` |
|      ! 0 | 11358 | `		zArg++;` |
|      ! 0 | 11359 | `		if( zArg < zArgEnd && zArg[0] == '=' ){` |
|      ! 0 | 11360 | `			zArg++;` |
|      ! 0 | 11361 | `		}` |
|      ! 0 | 11362 | `		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11363 | `			zArg++;` |
|      ! 0 | 11364 | `		}` |
|      ! 0 | 11365 | `		if( zArg >= zArgEnd \|\| zArg[0] == '-' ){` |
|        - | 11366 | `			/*` |
|        - | 11367 | `			 * Argument not found.` |
|        - | 11368 | `			 * Insert the option name and a boolean FALSE.` |
|        - | 11369 | `			 */` |
|      ! 0 | 11370 | `			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11371 | `			return;` |
|        - | 11372 | `		}` |
|        - | 11373 | `		/* Delimit the value */` |
|      ! 0 | 11374 | `		zCur = zArg;` |
|      ! 0 | 11375 | `		if( zArg[0] == '\'' \|\| zArg[0] == '"' ){` |
|      ! 0 | 11376 | `			int d = zArg[0];` |
|        - | 11377 | `			/* Delimt the argument */` |
|      ! 0 | 11378 | `			zArg++;` |
|      ! 0 | 11379 | `			zCur = zArg;` |
|      ! 0 | 11380 | `			while( zArg < zArgEnd ){` |
|      ! 0 | 11381 | `				if( zArg[0] == d && zArg[-1] != '\\' ){` |
|        - | 11382 | `					/* Delimiter found,exit the loop  */` |
|      ! 0 | 11383 | `					break;` |
|        - | 11384 | `				}` |
|      ! 0 | 11385 | `				zArg++;` |
|      ! 0 | 11386 | `			}` |
|        - | 11387 | `			/* Save the value */` |
|      ! 0 | 11388 | `			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|      ! 0 | 11389 | `			if( zArg < zArgEnd ){ zArg++; }` |
|      ! 0 | 11390 | `		}else{` |
|      ! 0 | 11391 | `			while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){` |
|      ! 0 | 11392 | `				zArg++;` |
|      ! 0 | 11393 | `			}` |
|        - | 11394 | `			/* Save the value */` |
|      ! 0 | 11395 | `			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|        - | 11396 | `		}` |
|        - | 11397 | `		/*` |
|        - | 11398 | `		 * Check if we are dealing with multiple values.` |
|        - | 11399 | `		 * If so,create an array to hold them,rather than a scalar variable.` |
|        - | 11400 | `		 */` |
|      ! 0 | 11401 | `		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11402 | `			zArg++;` |
|      ! 0 | 11403 | `		}` |
|      ! 0 | 11404 | `		if( zArg < zArgEnd && zArg[0] != '-' ){` |
|        - | 11405 | `			ph7_value *pOptArg; /* Array of option arguments */` |
|      ! 0 | 11406 | `			pOptArg = ph7_context_new_array(pCtx);` |
|      ! 0 | 11407 | `			if( pOptArg == 0 ){` |
|      ! 0 | 11408 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11409 | `			}else{` |
|        - | 11410 | `				/* Insert the first value */` |
|      ! 0 | 11411 | `				ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11412 | `				for(;;){` |
|      ! 0 | 11413 | `					if( zArg >= zArgEnd \|\| zArg[0] == '-' ){` |
|        - | 11414 | `						/* No more value */` |
|      ! 0 | 11415 | `						break;` |
|        - | 11416 | `					}` |
|        - | 11417 | `					/* Delimit the value */` |
|      ! 0 | 11418 | `					zCur = zArg;` |
|      ! 0 | 11419 | `					if( zArg < zArgEnd && zArg[0] == '\\' ){` |
|      ! 0 | 11420 | `						zArg++;` |
|      ! 0 | 11421 | `						zCur = zArg;` |
|      ! 0 | 11422 | `					}` |
|      ! 0 | 11423 | `					while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){` |
|      ! 0 | 11424 | `						zArg++;` |
|      ! 0 | 11425 | `					}` |
|        - | 11426 | `					/* Reset the string cursor */` |
|      ! 0 | 11427 | `					ph7_value_reset_string_cursor(pWorker);` |
|        - | 11428 | `					/* Save the value */` |
|      ! 0 | 11429 | `					ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|        - | 11430 | `					/* Insert */` |
|      ! 0 | 11431 | `					ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */` |
|        - | 11432 | `					/* Jump trailing white spaces */` |
|      ! 0 | 11433 | `					while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11434 | `						zArg++;` |
|      ! 0 | 11435 | `					}` |
|      ! 0 | 11436 | `				}` |
|        - | 11437 | `				/* Insert the option arg array */` |
|      ! 0 | 11438 | `				ph7_array_add_strkey_elem(pArray,(const char *)zName,pOptArg); /* Will make it's own copy */` |
|        - | 11439 | `				/* Safely release */` |
|      ! 0 | 11440 | `				ph7_context_release_value(pCtx,pOptArg);` |
|        - | 11441 | `			}` |
|      ! 0 | 11442 | `		}else{` |
|        - | 11443 | `			/* Single value */` |
|      ! 0 | 11444 | `			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|        - | 11445 | `		}` |
|        - | 11446 | `	}` |
|      ! 0 | 11447 |  |
|        - | 11448 | `/*` |
|        - | 11449 | ` * array getopt(string $options[,array $longopts ])` |
|        - | 11450 | ` *   Gets options from the command line argument list.` |
|        - | 11451 | ` * Parameters` |
|        - | 11452 | ` *  $options` |
|        - | 11453 | ` *   Each character in this string will be used as option characters` |
|        - | 11454 | ` *   and matched against options passed to the script starting with` |
|        - | 11455 | ` *   a single hyphen (-). For example, an option string "x" recognizes` |
|        - | 11456 | ` *   an option -x. Only a-z, A-Z and 0-9 are allowed.` |
|        - | 11457 | ` *  $longopts` |
|        - | 11458 | ` *   An array of options. Each element in this array will be used as option` |
|        - | 11459 | ` *   strings and matched against options passed to the script starting with` |
|        - | 11460 | ` *   two hyphens (--). For example, an longopts element "opt" recognizes an` |
|        - | 11461 | ` *   option --opt.` |
|        - | 11462 | ` * Return` |
|        - | 11463 | ` *  This function will return an array of option / argument pairs or FALSE` |
|        - | 11464 | ` *  on failure.` |
|        - | 11465 | ` */` |
|        2 | 11466 | `static int vm_builtin_getopt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11467 |  |
|        - | 11468 | `	const char *zIn,*zEnd,*zArg,*zArgIn,*zArgEnd;` |
|        - | 11469 | `	struct getopt_long_opt sLong;` |
|        - | 11470 | `	ph7_value *pArray,*pWorker;` |
|        - | 11471 | `	SyBlob *pArg;` |
|        - | 11472 | `	int nByte;` |
|        3 | 11473 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11474 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 11475 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Missing/Invalid option arguments");` |
|      ! 0 | 11476 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11477 | `		return PH7_OK;` |
|        - | 11478 | `	}` |
|        - | 11479 | `	/* Extract option arguments */` |
|        3 | 11480 | `	zIn  = ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 11481 | `	zEnd = &zIn[nByte];` |
|        - | 11482 | `	/* Point to the string representation of the $argv[] array */` |
|        3 | 11483 | `	pArg = &pCtx->pVm->sArgv;` |
|        - | 11484 | `	/* Create a new empty array and a worker variable */` |
|        3 | 11485 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11486 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11487 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|      ! 0 | 11488 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11489 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11490 | `		return PH7_OK;` |
|        - | 11491 | `	}` |
|        3 | 11492 | `	if( SyBlobLength(pArg) < 1 ){` |
|        - | 11493 | `		/* Empty command line,return the empty array*/` |
|      ! 0 | 11494 | `		ph7_result_value(pCtx,pArray);` |
|        - | 11495 | `		/* Everything will be released automatically when we return` |
|        - | 11496 | `		 * from this function.` |
|        - | 11497 | `		 */` |
|      ! 0 | 11498 | `		return PH7_OK;` |
|        - | 11499 | `	}` |
|        3 | 11500 | `	zArgIn = (const char *)SyBlobData(pArg);` |
|        3 | 11501 | `	zArgEnd = &zArgIn[SyBlobLength(pArg)];` |
|        - | 11502 | `	/* Fill the long option structure */` |
|        3 | 11503 | `	sLong.pArray = pArray;` |
|        3 | 11504 | `	sLong.pWorker = pWorker;` |
|        3 | 11505 | `	sLong.zArgIn =  zArgIn;` |
|        3 | 11506 | `	sLong.zArgEnd = zArgEnd;` |
|        3 | 11507 | `	sLong.pCtx = pCtx;` |
|        - | 11508 | `	/* Start processing */` |
|        9 | 11509 | `	while( zIn < zEnd ){` |
|        7 | 11510 | `		int c = zIn[0];` |
|        7 | 11511 | `		int need_val = 0;` |
|        - | 11512 | `		/* Advance the stream cursor */` |
|        7 | 11513 | `		zIn++;` |
|        - | 11514 | `		/* Ignore non-alphanum characters */` |
|        7 | 11515 | `		if( !SyisAlphaNum(c) ){` |
|      ! 0 | 11516 | `			continue;` |
|        - | 11517 | `		}` |
|        7 | 11518 | `		if( zIn < zEnd && zIn[0] == ':' ){` |
|        5 | 11519 | `			zIn++;` |
|        5 | 11520 | `			need_val = 1;` |
|        5 | 11521 | `			if( zIn < zEnd && zIn[0] == ':' ){` |
|      ! 0 | 11522 | `				zIn++;` |
|      ! 0 | 11523 | `			}` |
|        2 | 11524 | `		}` |
|        - | 11525 | `		/* Find option */` |
|        7 | 11526 | `		zArg = VmFindShortOpt(c,zArgIn,zArgEnd);` |
|        7 | 11527 | `		if( zArg == 0 ){` |
|        - | 11528 | `			/* No such option */` |
|        7 | 11529 | `			continue;` |
|        - | 11530 | `		}` |
|        - | 11531 | `		/* Extract option argument value */` |
|      ! 0 | 11532 | `		VmExtractOptArgValue(pArray,pWorker,zArg,zArgEnd,need_val,pCtx,(const char *)&c);` |
|      ! 0 | 11533 | `	}` |
|        3 | 11534 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) && ph7_array_count(apArg[1]) > 0 ){` |
|        - | 11535 | `		/* Process long options */` |
|      ! 0 | 11536 | `		ph7_array_walk(apArg[1],VmProcessLongOpt,&sLong);` |
|      ! 0 | 11537 | `	}` |
|        - | 11538 | `	/* Return the option array */` |
|        3 | 11539 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11540 | `	/*` |
|        - | 11541 | `	 * Don't worry about freeing memory, everything will be released` |
|        - | 11542 | `	 * automatically as soon we return from this foreign function.` |
|        - | 11543 | `	 */` |
|        3 | 11544 | `	return PH7_OK;` |
|        2 | 11545 |  |
|        - | 11546 | `/*` |
|        - | 11547 | ` * Array walker callback used for processing long options values.` |
|        - | 11548 | ` */` |
|      ! 0 | 11549 | `static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11550 |  |
|      ! 0 | 11551 | `	struct getopt_long_opt *pOpt = (struct getopt_long_opt *)pUserData;` |
|        - | 11552 | `	const char *zArg,*zOpt,*zEnd;` |
|      ! 0 | 11553 | `	int need_value = 0;` |
|        - | 11554 | `	int nByte;` |
|        - | 11555 | `	/* Value must be of type string */` |
|      ! 0 | 11556 | `	if( !ph7_value_is_string(pValue) ){` |
|        - | 11557 | `		/* Simply ignore */` |
|      ! 0 | 11558 | `		return PH7_OK;` |
|        - | 11559 | `	}` |
|      ! 0 | 11560 | `	zOpt = ph7_value_to_string(pValue,&nByte);` |
|      ! 0 | 11561 | `	if( nByte < 1 ){` |
|        - | 11562 | `		/* Empty string,ignore */` |
|      ! 0 | 11563 | `		return PH7_OK;` |
|        - | 11564 | `	}` |
|      ! 0 | 11565 | `	zEnd = &zOpt[nByte - 1];` |
|      ! 0 | 11566 | `	if( zEnd[0] == ':' ){` |
|        - | 11567 | `		char *zTerm;` |
|        - | 11568 | `		/* Try to extract a value */` |
|      ! 0 | 11569 | `		need_value = 1;` |
|      ! 0 | 11570 | `		while( zEnd >= zOpt && zEnd[0] == ':' ){` |
|      ! 0 | 11571 | `			zEnd--;` |
|      ! 0 | 11572 | `		}` |
|      ! 0 | 11573 | `		if( zOpt >= zEnd ){` |
|        - | 11574 | `			/* Empty string,ignore */` |
|      ! 0 | 11575 | `			SXUNUSED(pKey);` |
|      ! 0 | 11576 | `			return PH7_OK;` |
|        - | 11577 | `		}` |
|      ! 0 | 11578 | `		zEnd++;` |
|      ! 0 | 11579 | `		zTerm = (char *)zEnd;` |
|      ! 0 | 11580 | `		zTerm[0] = 0;` |
|      ! 0 | 11581 | `	}else{` |
|      ! 0 | 11582 | `		zEnd = &zOpt[nByte];` |
|        - | 11583 | `	}` |
|        - | 11584 | `	/* Find the option */` |
|      ! 0 | 11585 | `	zArg = VmFindLongOpt(zOpt,(int)(zEnd-zOpt),pOpt->zArgIn,pOpt->zArgEnd);` |
|      ! 0 | 11586 | `	if( zArg == 0 ){` |
|        - | 11587 | `		/* No such option,return immediately */` |
|      ! 0 | 11588 | `		return PH7_OK;` |
|        - | 11589 | `	}` |
|        - | 11590 | `	/* Try to extract a value */` |
|      ! 0 | 11591 | `	VmExtractOptArgValue(pOpt->pArray,pOpt->pWorker,zArg,pOpt->zArgEnd,need_value,pOpt->pCtx,zOpt);` |
|      ! 0 | 11592 | `	return PH7_OK;` |
|      ! 0 | 11593 |  |
|        - | 11594 | `/*` |
|        - | 11595 | ` * Section:` |
|        - | 11596 | ` *  JSON encoding/decoding routines.` |
|        - | 11597 | ` * Status:` |
|        - | 11598 | ` *    Devel.` |
|        - | 11599 | ` */` |
|        - | 11600 | `/* Forward reference */` |
|        - | 11601 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11602 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData);` |
|        - | 11603 | `/*` |
|        - | 11604 | ` * JSON encoder state is stored in an instance` |
|        - | 11605 | ` * of the following structure.` |
|        - | 11606 | ` */` |
|        - | 11607 | `typedef struct json_private_data json_private_data;` |
|        - | 11608 | `struct json_private_data` |
|        - | 11609 |  |
|        - | 11610 | `	ph7_context *pCtx; /* Call context */` |
|        - | 11611 | `	int isFirst;       /* True if first encoded entry */` |
|        - | 11612 | `	int iFlags;        /* JSON encoding flags */` |
|        - | 11613 | `	int nRecCount;     /* Recursion count */` |
|        - | 11614 | `};` |
|        - | 11615 | `/*` |
|        - | 11616 | ` * Returns the JSON representation of a value.In other word perform a JSON encoding operation.` |
|        - | 11617 | ` * According to wikipedia` |
|        - | 11618 | ` * JSON's basic types are:` |
|        - | 11619 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|        - | 11620 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|        - | 11621 | ` *   Boolean (true or false)` |
|        - | 11622 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|        - | 11623 | ` *    do not need to be of the same type)` |
|        - | 11624 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|        - | 11625 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|        - | 11626 | ` *     be distinct from each other)` |
|        - | 11627 | ` *   null (empty)` |
|        - | 11628 | ` * Non-significant white space may be added freely around the "structural characters"` |
|        - | 11629 | ` * (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|        - | 11630 | ` */` |
|        8 | 11631 | `static sxi32 VmJsonEncode(` |
|        - | 11632 | `	ph7_value *pIn,          /* Encode this value */` |
|        - | 11633 | `	json_private_data *pData /* Context data */` |
|        1 | 11634 | `	){` |
|        9 | 11635 | `		ph7_context *pCtx = pData->pCtx;` |
|        9 | 11636 | `		int iFlags = pData->iFlags;` |
|        - | 11637 | `		int nByte;` |
|        9 | 11638 | `		if( ph7_value_is_null(pIn) \|\| ph7_value_is_resource(pIn)){` |
|        - | 11639 | `			/* null */` |
|      ! 0 | 11640 | `			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|        9 | 11641 | `		}else if( ph7_value_is_bool(pIn) ){` |
|      ! 0 | 11642 | `			int iBool = ph7_value_to_bool(pIn);` |
|        - | 11643 | `			int iLen;` |
|        - | 11644 | `			/* true/false */` |
|      ! 0 | 11645 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|      ! 0 | 11646 | `			ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1);` |
|       12 | 11647 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|        - | 11648 | `			const char *zNum;` |
|        - | 11649 | `			/* Get a string representation of the number */` |
|        7 | 11650 | `			zNum = ph7_value_to_string(pIn,&nByte);` |
|        7 | 11651 | `			ph7_result_string(pCtx,zNum,nByte);` |
|        6 | 11652 | `		}else if( ph7_value_is_string(pIn) ){` |
|      ! 0 | 11653 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|        - | 11654 | `				const char *zNum;` |
|        - | 11655 | `				/* Encodes numeric strings as numbers. */` |
|      ! 0 | 11656 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|        - | 11657 | `				/* Get a string representation of the number */` |
|      ! 0 | 11658 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|      ! 0 | 11659 | `				ph7_result_string(pCtx,zNum,nByte);` |
|      ! 0 | 11660 | `			}else{` |
|        - | 11661 | `				const char *zIn,*zEnd;` |
|        - | 11662 | `				int c;` |
|        - | 11663 | `				/* Encode the string */` |
|      ! 0 | 11664 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|      ! 0 | 11665 | `				zEnd = &zIn[nByte];` |
|        - | 11666 | `				/* Append the double quote */` |
|      ! 0 | 11667 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      ! 0 | 11668 | `				for(;;){` |
|      ! 0 | 11669 | `					if( zIn >= zEnd ){` |
|        - | 11670 | `						/* No more input to process */` |
|      ! 0 | 11671 | `						break;` |
|        - | 11672 | `					}` |
|      ! 0 | 11673 | `					c = zIn[0];` |
|        - | 11674 | `					/* Advance the stream cursor */` |
|      ! 0 | 11675 | `					zIn++;` |
|      ! 0 | 11676 | `					if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|        - | 11677 | `						/* All < and > are converted to \u003C and \u003E */` |
|      ! 0 | 11678 | `						if( c == '<' ){` |
|      ! 0 | 11679 | `							ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1);` |
|      ! 0 | 11680 | `						}else{` |
|      ! 0 | 11681 | `							ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1);` |
|        - | 11682 | `						}` |
|      ! 0 | 11683 | `						continue;` |
|      ! 0 | 11684 | `					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|        - | 11685 | `						/* All &s are converted to \u0026.  */` |
|      ! 0 | 11686 | `						ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1);` |
|      ! 0 | 11687 | `						continue;` |
|      ! 0 | 11688 | `					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|        - | 11689 | `						/* All ' are converted to \u0027.   */` |
|      ! 0 | 11690 | `						ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1);` |
|      ! 0 | 11691 | `						continue;` |
|      ! 0 | 11692 | `					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|        - | 11693 | `						/* All " are converted to \u0022. */` |
|      ! 0 | 11694 | `						ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1);` |
|      ! 0 | 11695 | `						continue;` |
|        - | 11696 | `					}` |
|      ! 0 | 11697 | `					if( c == '"' \|\| (c == '\\' && ((iFlags & JSON_UNESCAPED_SLASHES)==0)) ){` |
|        - | 11698 | `						/* Unescape the character */` |
|      ! 0 | 11699 | `						ph7_result_string(pCtx,"\\",(int)sizeof(char));` |
|      ! 0 | 11700 | `					}` |
|        - | 11701 | `					/* Append character verbatim */` |
|      ! 0 | 11702 | `					ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      ! 0 | 11703 | `				}` |
|        - | 11704 | `				/* Append the double quote */` |
|      ! 0 | 11705 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      ! 0 | 11706 | `			}` |
|        3 | 11707 | `		}else if( ph7_value_is_array(pIn) ){` |
|        3 | 11708 | `			int c = '[',d = ']';` |
|        - | 11709 | `			/* Encode the array */` |
|        3 | 11710 | `			pData->isFirst = 1;` |
|        3 | 11711 | `			if( iFlags & JSON_FORCE_OBJECT ){` |
|        - | 11712 | `				/* Outputs an object rather than an array */` |
|      ! 0 | 11713 | `				c = '{';` |
|      ! 0 | 11714 | `				d = '}';` |
|      ! 0 | 11715 | `			}` |
|        - | 11716 | `			/* Append the square bracket or curly braces */` |
|        3 | 11717 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|        - | 11718 | `			/* Iterate throw array entries */` |
|        3 | 11719 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|        - | 11720 | `			/* Append the closing square bracket or curly braces */` |
|        3 | 11721 | `			ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char));` |
|        1 | 11722 | `		}else if( ph7_value_is_object(pIn) ){` |
|        - | 11723 | `			/* Encode the class instance */` |
|      ! 0 | 11724 | `			pData->isFirst = 1;` |
|        - | 11725 | `			/* Append the curly braces */` |
|      ! 0 | 11726 | `			ph7_result_string(pCtx,"{",(int)sizeof(char));` |
|        - | 11727 | `			/* Iterate throw class attribute */` |
|      ! 0 | 11728 | `			ph7_object_walk(pIn,VmJsonObjectEncode,pData);` |
|        - | 11729 | `			/* Append the closing curly braces  */` |
|      ! 0 | 11730 | `			ph7_result_string(pCtx,"}",(int)sizeof(char));` |
|      ! 0 | 11731 | `		}else{` |
|        - | 11732 | `			/* Can't happen */` |
|      ! 0 | 11733 | `			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|        - | 11734 | `		}` |
|        - | 11735 | `		/* All done */` |
|        9 | 11736 | `		return PH7_OK;` |
|        1 | 11737 |  |
|        - | 11738 | `/*` |
|        - | 11739 | ` * The following walker callback is invoked each time we need` |
|        - | 11740 | ` * to encode an array to JSON.` |
|        - | 11741 | ` */` |
|        6 | 11742 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11743 |  |
|        7 | 11744 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|        7 | 11745 | `	if( pJson->nRecCount > 31 ){` |
|        - | 11746 | `		/* Recursion limit reached,return immediately */` |
|      ! 0 | 11747 | `		return PH7_OK;` |
|        - | 11748 | `	}` |
|        7 | 11749 | `	if( !pJson->isFirst ){` |
|        - | 11750 | `		/* Append the colon first */` |
|        5 | 11751 | `		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));` |
|        2 | 11752 | `	}` |
|        7 | 11753 | `	if( pJson->iFlags & JSON_FORCE_OBJECT ){` |
|        - | 11754 | `		/* Outputs an object rather than an array */` |
|        - | 11755 | `		const char *zKey;` |
|        - | 11756 | `		int nByte;` |
|        - | 11757 | `		/* Extract a string representation of the key */` |
|      ! 0 | 11758 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|        - | 11759 | `		/* Append the key and the double colon */` |
|      ! 0 | 11760 | `		ph7_result_string_format(pJson->pCtx,"\"%.*s\":",nByte,zKey);` |
|      ! 0 | 11761 | `	}` |
|        - | 11762 | `	/* Encode the value */` |
|        7 | 11763 | `	pJson->nRecCount++;` |
|        7 | 11764 | `	VmJsonEncode(pValue,pJson);` |
|        7 | 11765 | `	pJson->nRecCount--;` |
|        7 | 11766 | `	pJson->isFirst = 0;` |
|        7 | 11767 | `	return PH7_OK;` |
|        4 | 11768 |  |
|        - | 11769 | `/*` |
|        - | 11770 | ` * The following walker callback is invoked each time we need to encode` |
|        - | 11771 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|        - | 11772 | ` */` |
|      ! 0 | 11773 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11774 |  |
|      ! 0 | 11775 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|      ! 0 | 11776 | `	if( pJson->nRecCount > 31 ){` |
|        - | 11777 | `		/* Recursion limit reached,return immediately */` |
|      ! 0 | 11778 | `		return PH7_OK;` |
|        - | 11779 | `	}` |
|      ! 0 | 11780 | `	if( !pJson->isFirst ){` |
|        - | 11781 | `		/* Append the colon first */` |
|      ! 0 | 11782 | `		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));` |
|      ! 0 | 11783 | `	}` |
|        - | 11784 | `	/* Append the attribute name and the double colon first */` |
|      ! 0 | 11785 | `	ph7_result_string_format(pJson->pCtx,"\"%s\":",zAttr);` |
|        - | 11786 | `	/* Encode the value */` |
|      ! 0 | 11787 | `	pJson->nRecCount++;` |
|      ! 0 | 11788 | `	VmJsonEncode(pValue,pJson);` |
|      ! 0 | 11789 | `	pJson->nRecCount--;` |
|      ! 0 | 11790 | `	pJson->isFirst = 0;` |
|      ! 0 | 11791 | `	return PH7_OK;` |
|      ! 0 | 11792 |  |
|        - | 11793 | `/*` |
|        - | 11794 | ` * string json_encode(mixed $value [, int $options = 0 ])` |
|        - | 11795 | ` *  Returns a string containing the JSON representation of value.` |
|        - | 11796 | ` * Parameters` |
|        - | 11797 | ` *  $value` |
|        - | 11798 | ` *  The value being encoded. Can be any type except a resource.` |
|        - | 11799 | ` * $options` |
|        - | 11800 | ` *  Bitmask consisting of:` |
|        - | 11801 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|        - | 11802 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|        - | 11803 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|        - | 11804 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|        - | 11805 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|        - | 11806 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|        - | 11807 | ` *  JSON_BIGINT_AS_STRING   Not used` |
|        - | 11808 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|        - | 11809 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|        - | 11810 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|        - | 11811 | ` * Return` |
|        - | 11812 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|        - | 11813 | ` */` |
|        2 | 11814 | `static int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11815 |  |
|        - | 11816 | `	json_private_data sJson;` |
|        3 | 11817 | `	if( nArg < 1 ){` |
|        - | 11818 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 11819 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11820 | `		return PH7_OK;` |
|        - | 11821 | `	}` |
|        - | 11822 | `	/* Prepare the JSON data */` |
|        3 | 11823 | `	sJson.nRecCount = 0;` |
|        3 | 11824 | `	sJson.pCtx = pCtx;` |
|        3 | 11825 | `	sJson.isFirst = 1;` |
|        3 | 11826 | `	sJson.iFlags = 0;` |
|        3 | 11827 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|        - | 11828 | `		/* Extract option flags */` |
|      ! 0 | 11829 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 11830 | `	}` |
|        - | 11831 | `	/* Perform the encoding operation */` |
|        3 | 11832 | `	VmJsonEncode(apArg[0],&sJson);` |
|        - | 11833 | `	/* All done */` |
|        3 | 11834 | `	return PH7_OK;` |
|        2 | 11835 |  |
|        - | 11836 | `/*` |
|        - | 11837 | ` * int json_last_error(void)` |
|        - | 11838 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|        - | 11839 | ` * Parameters` |
|        - | 11840 | ` *  None` |
|        - | 11841 | ` * Return` |
|        - | 11842 | ` *  Returns an integer, the value can be one of the following constants:` |
|        - | 11843 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|        - | 11844 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|        - | 11845 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|        - | 11846 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|        - | 11847 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|        - | 11848 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|        - | 11849 | ` */` |
|        8 | 11850 | `static int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11851 |  |
|       10 | 11852 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11853 | `	/* Return the error code */` |
|       10 | 11854 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|        4 | 11855 | `	SXUNUSED(nArg); /* cc warning */` |
|        4 | 11856 | `	SXUNUSED(apArg);` |
|       10 | 11857 | `	return PH7_OK;` |
|        2 | 11858 |  |
|        - | 11859 | `/* Possible tokens from the JSON tokenization process */` |
|        - | 11860 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|        - | 11861 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|        - | 11862 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|        - | 11863 | `#define JSON_TK_NULL    0x008 /* null */` |
|        - | 11864 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|        - | 11865 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|        - | 11866 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|        - | 11867 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|        - | 11868 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|        - | 11869 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|        - | 11870 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|        - | 11871 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|        - | 11872 | `/*` |
|        - | 11873 | ` * Tokenize an entire JSON input.` |
|        - | 11874 | ` * Get a single low-level token from the input file.` |
|        - | 11875 | ` * Update the stream pointer so that it points to the first` |
|        - | 11876 | ` * character beyond the extracted token.` |
|        - | 11877 | ` */` |
|       60 | 11878 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        2 | 11879 |  |
|       62 | 11880 | `	int *pJsonErr = (int *)pUserData;` |
|        - | 11881 | `	SyString *pStr;` |
|        - | 11882 | `	int c;` |
|        - | 11883 | `	/* Ignore leading white spaces */` |
|       66 | 11884 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - | 11885 | `		/* Advance the stream cursor */` |
|        6 | 11886 | `		if( pStream->zText[0] == '\n' ){` |
|        - | 11887 | `			/* Update line counter */` |
|      ! 0 | 11888 | `			pStream->nLine++;` |
|      ! 0 | 11889 | `		}` |
|        6 | 11890 | `		pStream->zText++;` |
|        2 | 11891 | `	}` |
|       62 | 11892 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - | 11893 | `		/* End of input reached */` |
|      ! 0 | 11894 | `		SXUNUSED(pCtxData); /* cc warning */` |
|      ! 0 | 11895 | `		return SXERR_EOF;` |
|        - | 11896 | `	}` |
|        - | 11897 | `	/* Record token starting position and line */` |
|       62 | 11898 | `	pToken->nLine = pStream->nLine;` |
|       62 | 11899 | `	pToken->pUserData = 0;` |
|       62 | 11900 | `	pStr = &pToken->sData;` |
|       62 | 11901 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|       77 | 11902 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|       44 | 11903 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|        - | 11904 | `			/* Single character */` |
|       36 | 11905 | `			c = pStream->zText[0];` |
|        - | 11906 | `			/* Set token type */` |
|       36 | 11907 | `			switch(c){` |
|        5 | 11908 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|       10 | 11909 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|        6 | 11910 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|        5 | 11911 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|        8 | 11912 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|        9 | 11913 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|      ! 0 | 11914 | `			default:` |
|      ! 0 | 11915 | `				break;` |
|        - | 11916 | `			}` |
|        - | 11917 | `			/* Advance the stream cursor */` |
|       36 | 11918 | `			pStream->zText++;` |
|       45 | 11919 | `	}else if( pStream->zText[0] == '"') {` |
|        - | 11920 | `		/* JSON string */` |
|       10 | 11921 | `		pStream->zText++;` |
|       10 | 11922 | `		pStr->zString++;` |
|        - | 11923 | `		/* Delimit the string */` |
|       32 | 11924 | `		while( pStream->zText < pStream->zEnd ){` |
|       32 | 11925 | `			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){` |
|       10 | 11926 | `				break;` |
|        - | 11927 | `			}` |
|       24 | 11928 | `			if( pStream->zText[0] == '\n' ){` |
|        - | 11929 | `				/* Update line counter */` |
|      ! 0 | 11930 | `				pStream->nLine++;` |
|      ! 0 | 11931 | `			}` |
|       24 | 11932 | `			pStream->zText++;` |
|        2 | 11933 | `		}` |
|       10 | 11934 | `		if( pStream->zText >= pStream->zEnd ){` |
|        - | 11935 | `			/* Missing closing '"' */` |
|      ! 0 | 11936 | `			pToken->nType = JSON_TK_INVALID;` |
|      ! 0 | 11937 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 11938 | `		}else{` |
|       10 | 11939 | `			pToken->nType = JSON_TK_STR;` |
|       10 | 11940 | `			pStream->zText++; /* Jump the closing double quotes */` |
|        2 | 11941 | `		}` |
|       24 | 11942 | `	}else if( pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|        - | 11943 | `		/* Number */` |
|       13 | 11944 | `		pStream->zText++;` |
|       13 | 11945 | `		pToken->nType = JSON_TK_NUM;` |
|       13 | 11946 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11947 | `			pStream->zText++;` |
|      ! 0 | 11948 | `		}` |
|       13 | 11949 | `		if( pStream->zText < pStream->zEnd ){` |
|       13 | 11950 | `			c = pStream->zText[0];` |
|       13 | 11951 | `			if( c == '.' ){` |
|        - | 11952 | `					/* Real number */` |
|      ! 0 | 11953 | `					pStream->zText++;` |
|      ! 0 | 11954 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11955 | `						pStream->zText++;` |
|      ! 0 | 11956 | `					}` |
|      ! 0 | 11957 | `					if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11958 | `						c = pStream->zText[0];` |
|      ! 0 | 11959 | `						if( c=='e' \|\| c=='E' ){` |
|      ! 0 | 11960 | `							pStream->zText++;` |
|      ! 0 | 11961 | `							if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11962 | `								c = pStream->zText[0];` |
|      ! 0 | 11963 | `								if( c =='+' \|\| c=='-' ){` |
|      ! 0 | 11964 | `									pStream->zText++;` |
|      ! 0 | 11965 | `								}` |
|      ! 0 | 11966 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11967 | `									pStream->zText++;` |
|      ! 0 | 11968 | `								}` |
|      ! 0 | 11969 | `							}` |
|      ! 0 | 11970 | `						}` |
|      ! 0 | 11971 | `					}` |
|       13 | 11972 | `				}else if( c=='e' \|\| c=='E' ){` |
|        - | 11973 | `					/* Real number */` |
|      ! 0 | 11974 | `					pStream->zText++;` |
|      ! 0 | 11975 | `					if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11976 | `						c = pStream->zText[0];` |
|      ! 0 | 11977 | `						if( c =='+' \|\| c=='-' ){` |
|      ! 0 | 11978 | `							pStream->zText++;` |
|      ! 0 | 11979 | `						}` |
|      ! 0 | 11980 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11981 | `							pStream->zText++;` |
|      ! 0 | 11982 | `						}` |
|      ! 0 | 11983 | `					}` |
|      ! 0 | 11984 | `				}` |
|        7 | 11985 | `			}` |
|       17 | 11986 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|        6 | 11987 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|        - | 11988 | `			/* boolean true */` |
|      ! 0 | 11989 | `			pToken->nType = JSON_TK_TRUE;` |
|        - | 11990 | `			/* Advance the stream cursor */` |
|      ! 0 | 11991 | `			pStream->zText += sizeof("true")-1;` |
|       11 | 11992 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|        6 | 11993 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|        - | 11994 | `			/* boolean false */` |
|      ! 0 | 11995 | `			pToken->nType = JSON_TK_FALSE;` |
|        - | 11996 | `			/* Advance the stream cursor */` |
|      ! 0 | 11997 | `			pStream->zText += sizeof("false")-1;` |
|       11 | 11998 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|        6 | 11999 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|        - | 12000 | `			/* NULL */` |
|      ! 0 | 12001 | `			pToken->nType = JSON_TK_NULL;` |
|        - | 12002 | `			/* Advance the stream cursor */` |
|      ! 0 | 12003 | `			pStream->zText += sizeof("null")-1;` |
|      ! 0 | 12004 | `	}else{` |
|        - | 12005 | `		/* Unexpected token */` |
|        8 | 12006 | `		pToken->nType = JSON_TK_INVALID;` |
|        - | 12007 | `		/* Advance the stream cursor */` |
|        8 | 12008 | `		pStream->zText++;` |
|        8 | 12009 | `		*pJsonErr = JSON_ERROR_SYNTAX;` |
|        - | 12010 | `		/* Abort processing immediatley */` |
|        8 | 12011 | `		return SXERR_ABORT;` |
|        - | 12012 | `	}` |
|        - | 12013 | `	/* record token length */` |
|       56 | 12014 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|       56 | 12015 | `	if( pToken->nType == JSON_TK_STR ){` |
|       10 | 12016 | `		pStr->nByte--;` |
|        4 | 12017 | `	}` |
|        - | 12018 | `	/* Return to the lexer */` |
|       56 | 12019 | `	return SXRET_OK;` |
|       32 | 12020 |  |
|        - | 12021 | `/*` |
|        - | 12022 | ` * JSON decoded input consumer callback signature.` |
|        - | 12023 | ` */` |
|        - | 12024 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|        - | 12025 | `/*` |
|        - | 12026 | ` * JSON decoder state is kept in the following structure.` |
|        - | 12027 | ` */` |
|        - | 12028 | `typedef struct json_decoder json_decoder;` |
|        - | 12029 | `struct json_decoder` |
|        - | 12030 |  |
|        - | 12031 | `	ph7_context *pCtx; /* Call context */` |
|        - | 12032 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|        - | 12033 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|        - | 12034 | `	int iFlags;        /* Configuration flags */` |
|        - | 12035 | `	SyToken *pIn;      /* Token stream */` |
|        - | 12036 | `	SyToken *pEnd;     /* End of the token stream */` |
|        - | 12037 | `	int rec_depth;     /* Recursion limit */` |
|        - | 12038 | `	int rec_count;     /* Current nesting level */` |
|        - | 12039 | `	int *pErr;         /* JSON decoding error if any */` |
|        - | 12040 | `};` |
|        - | 12041 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|        - | 12042 | `/* Forward declaration */` |
|        - | 12043 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|        - | 12044 | `/*` |
|        - | 12045 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|        - | 12046 | ` * the result in the given ph7_value.` |
|        - | 12047 | ` */` |
|        8 | 12048 | `static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)` |
|        2 | 12049 |  |
|       10 | 12050 | `	const char *zIn = pStr->zString;` |
|       10 | 12051 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|        - | 12052 | `	const char *zCur;` |
|        - | 12053 | `	int c;` |
|        - | 12054 | `	/* Mark the value as a string */` |
|       10 | 12055 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|        4 | 12056 | `	for(;;){` |
|       10 | 12057 | `		zCur = zIn;` |
|       32 | 12058 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|       24 | 12059 | `			zIn++;` |
|        2 | 12060 | `		}` |
|       10 | 12061 | `		if( zIn > zCur ){` |
|        - | 12062 | `			/* Append chunk verbatim */` |
|       10 | 12063 | `			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));` |
|        4 | 12064 | `		}` |
|       10 | 12065 | `		zIn++;` |
|       10 | 12066 | `		if( zIn >= zEnd ){` |
|        - | 12067 | `			/* End of the input reached */` |
|       10 | 12068 | `			break;` |
|        - | 12069 | `		}` |
|      ! 0 | 12070 | `		c = zIn[0];` |
|        - | 12071 | `		/* Unescape the character */` |
|      ! 0 | 12072 | `		switch(c){` |
|      ! 0 | 12073 | `		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|      ! 0 | 12074 | `		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|      ! 0 | 12075 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|      ! 0 | 12076 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|      ! 0 | 12077 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|      ! 0 | 12078 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|      ! 0 | 12079 | `		default:` |
|      ! 0 | 12080 | `			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));` |
|      ! 0 | 12081 | `			break;` |
|        - | 12082 | `		}` |
|        - | 12083 | `		/* Advance the stream cursor */` |
|      ! 0 | 12084 | `		zIn++;` |
|      ! 0 | 12085 | `	}` |
|       10 | 12086 |  |
|        - | 12087 | `/*` |
|        - | 12088 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|        - | 12089 | ` * According to wikipedia` |
|        - | 12090 | ` * JSON's basic types are:` |
|        - | 12091 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|        - | 12092 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|        - | 12093 | ` *   Boolean (true or false)` |
|        - | 12094 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|        - | 12095 | ` *    do not need to be of the same type)` |
|        - | 12096 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|        - | 12097 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|        - | 12098 | ` *     be distinct from each other)` |
|        - | 12099 | ` *   null (empty)` |
|        - | 12100 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|        - | 12101 | ` */` |
|       24 | 12102 | `static sxi32 VmJsonDecode(` |
|        - | 12103 | `	json_decoder *pDecoder, /* JSON decoder */` |
|        - | 12104 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|        2 | 12105 | `	){` |
|        - | 12106 | `	ph7_value *pWorker; /* Worker variable */` |
|        - | 12107 | `	sxi32 rc;` |
|        - | 12108 | `	/* Check if we do not nest to much */` |
|       26 | 12109 | `	if( pDecoder->rec_count >= pDecoder->rec_depth ){` |
|        - | 12110 | `		/* Nesting limit reached,abort decoding immediately */` |
|      ! 0 | 12111 | `		*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|      ! 0 | 12112 | `		return SXERR_ABORT;` |
|        - | 12113 | `	}` |
|       26 | 12114 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|        - | 12115 | `		/* Scalar value */` |
|       16 | 12116 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|       16 | 12117 | `		if( pWorker == 0 ){` |
|      ! 0 | 12118 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12119 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 12120 | `			return SXERR_ABORT;` |
|        - | 12121 | `		}` |
|        - | 12122 | `		/* Reflect the JSON image */` |
|       16 | 12123 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|        - | 12124 | `			/* Nullify the value.*/` |
|      ! 0 | 12125 | `			ph7_value_null(pWorker);` |
|       16 | 12126 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|        - | 12127 | `			/* Boolean value */` |
|      ! 0 | 12128 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|       16 | 12129 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|       13 | 12130 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|        - | 12131 | `			/*` |
|        - | 12132 | `			 * Numeric value.` |
|        - | 12133 | `			 * Get a string representation first then try to get a numeric` |
|        - | 12134 | `			 * value.` |
|        - | 12135 | `			 */` |
|       13 | 12136 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|        - | 12137 | `			/* Obtain a numeric representation */` |
|       13 | 12138 | `			PH7_MemObjToNumeric(pWorker);` |
|        7 | 12139 | `		}else{` |
|        - | 12140 | `			/* Dequote the string */` |
|        3 | 12141 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);` |
|        - | 12142 | `		}` |
|        - | 12143 | `		/* Invoke the consumer callback */` |
|       16 | 12144 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|       16 | 12145 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12146 | `			return SXERR_ABORT;` |
|        - | 12147 | `		}` |
|        - | 12148 | `		/* All done,advance the stream cursor */` |
|       16 | 12149 | `		pDecoder->pIn++;` |
|       19 | 12150 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|        - | 12151 | `		ProcJsonConsumer xOld;` |
|        - | 12152 | `		void *pOld;` |
|        - | 12153 | `		/* Array representation*/` |
|        5 | 12154 | `		pDecoder->pIn++;` |
|        - | 12155 | `		/* Create a working array */` |
|        5 | 12156 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|        5 | 12157 | `		if( pWorker == 0 ){` |
|      ! 0 | 12158 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12159 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 12160 | `			return SXERR_ABORT;` |
|        - | 12161 | `		}` |
|        - | 12162 | `		/* Save the old consumer */` |
|        5 | 12163 | `		xOld = pDecoder->xConsumer;` |
|        5 | 12164 | `		pOld = pDecoder->pUserData;` |
|        - | 12165 | `		/* Set the new consumer */` |
|        5 | 12166 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|        5 | 12167 | `		pDecoder->pUserData = pWorker;` |
|        - | 12168 | `		/* Decode the array */` |
|        7 | 12169 | `		for(;;){` |
|        - | 12170 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|        - | 12171 | `			 * do this.` |
|        - | 12172 | `			 */` |
|       21 | 12173 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|        7 | 12174 | `				pDecoder->pIn++;` |
|        1 | 12175 | `			}` |
|       15 | 12176 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CSB) /*']'*/ ){` |
|        5 | 12177 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|        5 | 12178 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|        2 | 12179 | `				}` |
|        5 | 12180 | `				break;` |
|        - | 12181 | `			}` |
|        - | 12182 | `			/* Recurse and decode the entry */` |
|       11 | 12183 | `			pDecoder->rec_count++;` |
|       11 | 12184 | `			rc = VmJsonDecode(pDecoder,0);` |
|       11 | 12185 | `			pDecoder->rec_count--;` |
|       11 | 12186 | `			if( rc == SXERR_ABORT ){` |
|        - | 12187 | `				/* Abort processing immediately */` |
|      ! 0 | 12188 | `				return SXERR_ABORT;` |
|        - | 12189 | `			}` |
|        - | 12190 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|       11 | 12191 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|       10 | 12192 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|        - | 12193 | `					/* Unexpected token,abort immediatley */` |
|      ! 0 | 12194 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 12195 | `					return SXERR_ABORT;` |
|        - | 12196 | `			}` |
|        1 | 12197 | `		}` |
|        - | 12198 | `		/* Restore the old consumer */` |
|        5 | 12199 | `		pDecoder->xConsumer = xOld;` |
|        5 | 12200 | `		pDecoder->pUserData = pOld;` |
|        - | 12201 | `		/* Invoke the old consumer on the decoded array */` |
|        5 | 12202 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|       10 | 12203 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|        - | 12204 | `		ProcJsonConsumer xOld;` |
|        - | 12205 | `		ph7_value *pKey;` |
|        - | 12206 | `		void *pOld;` |
|        - | 12207 | `		/* Object representation*/` |
|        8 | 12208 | `		pDecoder->pIn++;` |
|        - | 12209 | `		/* Return the object as an associative array */` |
|        8 | 12210 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|        3 | 12211 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_WARNING,` |
|        - | 12212 | `				"JSON Objects are always returned as an associative array"` |
|        - | 12213 | `				);` |
|        1 | 12214 | `		}` |
|        - | 12215 | `		/* Create a working array */` |
|        8 | 12216 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|        8 | 12217 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|        8 | 12218 | `		if( pWorker == 0 \|\| pKey == 0){` |
|      ! 0 | 12219 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12220 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 12221 | `			return SXERR_ABORT;` |
|        - | 12222 | `		}` |
|        - | 12223 | `		/* Save the old consumer */` |
|        8 | 12224 | `		xOld = pDecoder->xConsumer;` |
|        8 | 12225 | `		pOld = pDecoder->pUserData;` |
|        - | 12226 | `		/* Set the new consumer */` |
|        8 | 12227 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|        8 | 12228 | `		pDecoder->pUserData = pWorker;` |
|        - | 12229 | `		/* Decode the object */` |
|        6 | 12230 | `		for(;;){` |
|        - | 12231 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|        - | 12232 | `			 * do this.` |
|        - | 12233 | `			 */` |
|       16 | 12234 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|        3 | 12235 | `				pDecoder->pIn++;` |
|        1 | 12236 | `			}` |
|       14 | 12237 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CCB) /*'}'*/ ){` |
|        8 | 12238 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|        6 | 12239 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|        2 | 12240 | `				}` |
|        8 | 12241 | `				break;` |
|        - | 12242 | `			}` |
|        6 | 12243 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|        8 | 12244 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|        - | 12245 | `					/* Syntax error,return immediately */` |
|      ! 0 | 12246 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 12247 | `					return SXERR_ABORT;` |
|        - | 12248 | `			}` |
|        - | 12249 | `			/* Dequote the key */` |
|        8 | 12250 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);` |
|        - | 12251 | `			/* Jump the key and the colon */` |
|        8 | 12252 | `			pDecoder->pIn += 2;` |
|        - | 12253 | `			/* Recurse and decode the value */` |
|        8 | 12254 | `			pDecoder->rec_count++;` |
|        8 | 12255 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|        8 | 12256 | `			pDecoder->rec_count--;` |
|        8 | 12257 | `			if( rc == SXERR_ABORT ){` |
|        - | 12258 | `				/* Abort processing immediately */` |
|      ! 0 | 12259 | `				return SXERR_ABORT;` |
|        - | 12260 | `			}` |
|        - | 12261 | `			/* Reset the internal buffer of the key */` |
|        8 | 12262 | `			ph7_value_reset_string_cursor(pKey);` |
|        - | 12263 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|        2 | 12264 | `		}` |
|        - | 12265 | `		/* Restore the old consumer */` |
|        8 | 12266 | `		pDecoder->xConsumer = xOld;` |
|        8 | 12267 | `		pDecoder->pUserData = pOld;` |
|        - | 12268 | `		/* Invoke the old consumer on the decoded object*/` |
|        8 | 12269 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|        - | 12270 | `		/* Release the key */` |
|        8 | 12271 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|        5 | 12272 | `	}else{` |
|        - | 12273 | `		/* Unexpected token */` |
|      ! 0 | 12274 | `		return SXERR_ABORT; /* Abort immediately */` |
|        - | 12275 | `	}` |
|        - | 12276 | `	/* Release the worker variable */` |
|       26 | 12277 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|       26 | 12278 | `	return SXRET_OK;` |
|       14 | 12279 |  |
|        - | 12280 | `/*` |
|        - | 12281 | ` * The following JSON decoder callback is invoked each time` |
|        - | 12282 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|        - | 12283 | ` * is being decoded.` |
|        - | 12284 | ` */` |
|       16 | 12285 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|        2 | 12286 |  |
|       18 | 12287 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 12288 | `	/* Insert the entry */` |
|       18 | 12289 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|        8 | 12290 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 12291 | `	/* All done */` |
|       18 | 12292 | `	return SXRET_OK;` |
|        2 | 12293 |  |
|        - | 12294 | `/*` |
|        - | 12295 | ` * Standard JSON decoder callback.` |
|        - | 12296 | ` */` |
|        8 | 12297 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|        2 | 12298 |  |
|        - | 12299 | `	/* Return the value directly */` |
|       10 | 12300 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|        4 | 12301 | `	SXUNUSED(pKey); /* cc warning */` |
|        4 | 12302 | `	SXUNUSED(pUserData);` |
|        - | 12303 | `	/* All done */` |
|       10 | 12304 | `	return SXRET_OK;` |
|        2 | 12305 |  |
|        - | 12306 | `/*` |
|        - | 12307 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])` |
|        - | 12308 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|        - | 12309 | ` * Parameters` |
|        - | 12310 | ` *  $json` |
|        - | 12311 | ` *    The json string being decoded.` |
|        - | 12312 | ` * $assoc` |
|        - | 12313 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|        - | 12314 | ` * $depth` |
|        - | 12315 | ` *   User specified recursion depth.` |
|        - | 12316 | ` * $options` |
|        - | 12317 | ` *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported` |
|        - | 12318 | ` * (default is to cast large integers as floats)` |
|        - | 12319 | ` * Return` |
|        - | 12320 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|        - | 12321 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|        - | 12322 | ` *  or if the encoded data is deeper than the recursion limit.` |
|        - | 12323 | ` */` |
|       16 | 12324 | `static int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12325 |  |
|       18 | 12326 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12327 | `	json_decoder sDecoder;` |
|        - | 12328 | `	const char *zIn;` |
|        - | 12329 | `	SySet sToken;` |
|        - | 12330 | `	SyLex sLex;` |
|        - | 12331 | `	int nByte;` |
|        - | 12332 | `	sxi32 rc;` |
|       18 | 12333 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12334 | `		/* Missing/Invalid arguments, return NULL */` |
|      ! 0 | 12335 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12336 | `		return PH7_OK;` |
|        - | 12337 | `	}` |
|        - | 12338 | `	/* Extract the JSON string */` |
|       18 | 12339 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|       18 | 12340 | `	if( nByte < 1 ){` |
|        - | 12341 | `		/* Empty string,return NULL */` |
|        3 | 12342 | `		ph7_result_null(pCtx);` |
|        3 | 12343 | `		return PH7_OK;` |
|        - | 12344 | `	}` |
|        - | 12345 | `	/* Clear JSON error code */` |
|       16 | 12346 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - | 12347 | `	/* Tokenize the input */` |
|       16 | 12348 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|       16 | 12349 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|       16 | 12350 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|       16 | 12351 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|        - | 12352 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|        8 | 12353 | `		SyLexRelease(&sLex);` |
|        8 | 12354 | `		SySetRelease(&sToken);` |
|        - | 12355 | `		/* return NULL */` |
|        8 | 12356 | `		ph7_result_null(pCtx);` |
|        8 | 12357 | `		return PH7_OK;` |
|        - | 12358 | `	}` |
|        - | 12359 | `	/* Fill the decoder */` |
|       10 | 12360 | `	sDecoder.pCtx = pCtx;` |
|       10 | 12361 | `	sDecoder.pErr = &pVm->json_rc;` |
|       10 | 12362 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       10 | 12363 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|       10 | 12364 | `	sDecoder.iFlags = 0;` |
|       10 | 12365 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){` |
|        - | 12366 | `		/* Returned objects will be converted into associative arrays */` |
|        8 | 12367 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|        3 | 12368 | `	}` |
|       10 | 12369 | `	sDecoder.rec_depth = 32;` |
|       10 | 12370 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      ! 0 | 12371 | `		int nDepth = ph7_value_to_int(apArg[2]);` |
|      ! 0 | 12372 | `		if( nDepth > 1 && nDepth < 32 ){` |
|      ! 0 | 12373 | `			sDecoder.rec_depth = nDepth;` |
|      ! 0 | 12374 | `		}` |
|      ! 0 | 12375 | `	}` |
|       10 | 12376 | `	sDecoder.rec_count = 0;` |
|        - | 12377 | `	/* Set a default consumer */` |
|       10 | 12378 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|       10 | 12379 | `	sDecoder.pUserData = 0;` |
|        - | 12380 | `	/* Decode the raw JSON input */` |
|       10 | 12381 | `	rc = VmJsonDecode(&sDecoder,0);` |
|       10 | 12382 | `	if( rc == SXERR_ABORT \|\|  pVm->json_rc != JSON_ERROR_NONE ){` |
|        - | 12383 | `		/*` |
|        - | 12384 | `		 * Something goes wrong while decoding JSON input.Return NULL.` |
|        - | 12385 | `		 */` |
|      ! 0 | 12386 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12387 | `	}` |
|        - | 12388 | `	/* Clean-up the mess left behind */` |
|       10 | 12389 | `	SyLexRelease(&sLex);` |
|       10 | 12390 | `	SySetRelease(&sToken);` |
|        - | 12391 | `	/* All done */` |
|       10 | 12392 | `	return PH7_OK;` |
|       10 | 12393 |  |
|        - | 12394 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12395 | `/*` |
|        - | 12396 | ` * XML processing Functions.` |
|        - | 12397 | ` * Status:` |
|        - | 12398 | ` *    Devel.` |
|        - | 12399 | ` */` |
|        - | 12400 | `enum ph7_xml_handler_id{` |
|        - | 12401 | `	PH7_XML_START_TAG = 0, /* Start element handlers ID */` |
|        - | 12402 | `	PH7_XML_END_TAG,       /* End element handler ID*/` |
|        - | 12403 | `	PH7_XML_CDATA,         /* Character data handler ID*/` |
|        - | 12404 | `	PH7_XML_PI,            /* Processing instruction (PI) handler ID*/` |
|        - | 12405 | `	PH7_XML_DEF,           /* Default handler ID */` |
|        - | 12406 | `	PH7_XML_UNPED,         /* Unparsed entity declaration handler */` |
|        - | 12407 | `	PH7_XML_ND,            /* Notation declaration handler ID*/` |
|        - | 12408 | `	PH7_XML_EER,           /* External entity reference handler */` |
|        - | 12409 | `	PH7_XML_NS_START,      /* Start namespace declaration handler */` |
|        - | 12410 | `	PH7_XML_NS_END         /* End namespace declaration handler */` |
|        - | 12411 | `};` |
|        - | 12412 | `#define XML_TOTAL_HANDLER (PH7_XML_NS_END + 1)` |
|        - | 12413 | `/* An instance of the following structure describe a working` |
|        - | 12414 | ` * XML engine instance.` |
|        - | 12415 | ` */` |
|        - | 12416 | `typedef struct ph7_xml_engine ph7_xml_engine;` |
|        - | 12417 | `struct ph7_xml_engine` |
|        - | 12418 |  |
|        - | 12419 | `	ph7_vm *pVm;         /* VM that own this instance */` |
|        - | 12420 | `	ph7_context *pCtx;   /* Call context */` |
|        - | 12421 | `	SyXMLParser sParser; /* Underlying XML parser */` |
|        - | 12422 | `	ph7_value aCB[XML_TOTAL_HANDLER]; /* User-defined callbacks */` |
|        - | 12423 | `	ph7_value sParserValue; /* ph7_value holding this instance which is forwarded` |
|        - | 12424 | `							  * as the first argument to the user callbacks.` |
|        - | 12425 | `							  */` |
|        - | 12426 | `	int ns_sep;      /* Namespace separator */` |
|        - | 12427 | `	SyBlob sErr;     /* Error message consumer */` |
|        - | 12428 | `	sxi32 iErrCode;  /* Last error code */` |
|        - | 12429 | `	sxi32 iNest;     /* Nesting level */` |
|        - | 12430 | `	sxu32 nLine;     /* Last processed line */` |
|        - | 12431 | `	sxu32 nMagic;    /* Magic number so that we avoid misuse  */` |
|        - | 12432 | `};` |
|        - | 12433 | `#define XML_ENGINE_MAGIC 0x851EFC52` |
|        - | 12434 | `#define IS_INVALID_XML_ENGINE(XML) (XML == 0 \|\| (XML)->nMagic != XML_ENGINE_MAGIC)` |
|        - | 12435 | `/*` |
|        - | 12436 | ` * Allocate and initialize an XML engine.` |
|        - | 12437 | ` */` |
|       84 | 12438 | `static ph7_xml_engine * VmCreateXMLEngine(ph7_context *pCtx,int process_ns,int ns_sep)` |
|        1 | 12439 |  |
|        - | 12440 | `	ph7_xml_engine *pEngine;` |
|       85 | 12441 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12442 | `	ph7_value *pValue;` |
|        - | 12443 | `	sxu32 n;` |
|        - | 12444 | `	/* Allocate a new instance */` |
|       85 | 12445 | `	pEngine = (ph7_xml_engine *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_xml_engine));` |
|       85 | 12446 | `	if( pEngine == 0 ){` |
|        - | 12447 | `		/* Out of memory */` |
|      ! 0 | 12448 | `		return 0;` |
|        - | 12449 | `	}` |
|        - | 12450 | `	/* Zero the structure */` |
|       85 | 12451 | `	SyZero(pEngine,sizeof(ph7_xml_engine));` |
|        - | 12452 | `	/* Initialize fields */` |
|       85 | 12453 | `	pEngine->pVm = pVm;` |
|       85 | 12454 | `	pEngine->pCtx = 0;` |
|       85 | 12455 | `	pEngine->ns_sep = ns_sep;` |
|       85 | 12456 | `	SyXMLParserInit(&pEngine->sParser,&pVm->sAllocator,process_ns ? SXML_ENABLE_NAMESPACE : 0);` |
|       85 | 12457 | `	SyBlobInit(&pEngine->sErr,&pVm->sAllocator);` |
|       85 | 12458 | `	PH7_MemObjInit(pVm,&pEngine->sParserValue);` |
|      925 | 12459 | `	for( n = 0 ; n < SX_ARRAYSIZE(pEngine->aCB) ; ++n ){` |
|      841 | 12460 | `		pValue = &pEngine->aCB[n];` |
|        - | 12461 | `		/* NULLIFY the array entries,until someone register an event handler */` |
|      841 | 12462 | `		PH7_MemObjInit(&(*pVm),pValue);` |
|      421 | 12463 | `	}` |
|       85 | 12464 | `	ph7_value_resource(&pEngine->sParserValue,pEngine);` |
|       85 | 12465 | `	pEngine->iErrCode = SXML_ERROR_NONE;` |
|        - | 12466 | `	/* Finally set the magic number */` |
|       85 | 12467 | `	pEngine->nMagic = XML_ENGINE_MAGIC;` |
|       85 | 12468 | `	return pEngine;` |
|       43 | 12469 |  |
|        - | 12470 | `/*` |
|        - | 12471 | ` * Release an XML engine.` |
|        - | 12472 | ` */` |
|       84 | 12473 | `static void VmReleaseXMLEngine(ph7_xml_engine *pEngine)` |
|        1 | 12474 |  |
|       85 | 12475 | `	ph7_vm *pVm = pEngine->pVm;` |
|        - | 12476 | `	ph7_value *pValue;` |
|        - | 12477 | `	sxu32 n;` |
|        - | 12478 | `	/* Release fields */` |
|       85 | 12479 | `	SyBlobRelease(&pEngine->sErr);` |
|       85 | 12480 | `	SyXMLParserRelease(&pEngine->sParser);` |
|       85 | 12481 | `	PH7_MemObjRelease(&pEngine->sParserValue);` |
|      925 | 12482 | `	for( n = 0 ; n < SX_ARRAYSIZE(pEngine->aCB) ; ++n ){` |
|      841 | 12483 | `		pValue = &pEngine->aCB[n];` |
|      841 | 12484 | `		PH7_MemObjRelease(pValue);` |
|      421 | 12485 | `	}` |
|       85 | 12486 | `	pEngine->nMagic = 0x2621;` |
|        - | 12487 | `	/* Finally,release the whole instance */` |
|       85 | 12488 | `	SyMemBackendFree(&pVm->sAllocator,pEngine);` |
|       85 | 12489 |  |
|        - | 12490 | `/*` |
|        - | 12491 | ` * resource xml_parser_create([ string $encoding ])` |
|        - | 12492 | ` *  Create an UTF-8 XML parser.` |
|        - | 12493 | ` * Parameter` |
|        - | 12494 | ` *  $encoding` |
|        - | 12495 | ` *   (Only UTF-8 encoding is used)` |
|        - | 12496 | ` * Return` |
|        - | 12497 | ` *  Returns a resource handle for the new XML parser.` |
|        - | 12498 | ` */` |
|       80 | 12499 | `static int vm_builtin_xml_parser_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12500 |  |
|        - | 12501 | `	ph7_xml_engine *pEngine;` |
|        - | 12502 | `	/* Allocate a new instance */` |
|       81 | 12503 | `	pEngine = VmCreateXMLEngine(&(*pCtx),0,':');` |
|       81 | 12504 | `	if( pEngine == 0 ){` |
|      ! 0 | 12505 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12506 | `		/* Return null */` |
|      ! 0 | 12507 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12508 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12509 | `		SXUNUSED(apArg);` |
|      ! 0 | 12510 | `		return PH7_OK;` |
|        - | 12511 | `	}` |
|        - | 12512 | `	/* Return the engine as a resource */` |
|       81 | 12513 | `	ph7_result_resource(pCtx,pEngine);` |
|       81 | 12514 | `	return PH7_OK;` |
|       41 | 12515 |  |
|        - | 12516 | `/*` |
|        - | 12517 | ` * resource xml_parser_create_ns([ string $encoding[,string $separator = ':']])` |
|        - | 12518 | ` *  Create an UTF-8 XML parser with namespace support.` |
|        - | 12519 | ` * Parameter` |
|        - | 12520 | ` *  $encoding` |
|        - | 12521 | ` *   (Only UTF-8 encoding is supported)` |
|        - | 12522 | ` *  $separtor` |
|        - | 12523 | ` *   Namespace separator (a single character)` |
|        - | 12524 | ` * Return` |
|        - | 12525 | ` *  Returns a resource handle for the new XML parser.` |
|        - | 12526 | ` */` |
|        4 | 12527 | `static int vm_builtin_xml_parser_create_ns(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12528 |  |
|        - | 12529 | `	ph7_xml_engine *pEngine;` |
|        5 | 12530 | `	int ns_sep = ':';` |
|        5 | 12531 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      ! 0 | 12532 | `		const char *zSep = ph7_value_to_string(apArg[1],0);` |
|      ! 0 | 12533 | `		if( zSep[0] != 0 ){` |
|      ! 0 | 12534 | `			ns_sep = zSep[0];` |
|      ! 0 | 12535 | `		}` |
|      ! 0 | 12536 | `	}` |
|        - | 12537 | `	/* Allocate a new instance */` |
|        5 | 12538 | `	pEngine = VmCreateXMLEngine(&(*pCtx),TRUE,ns_sep);` |
|        5 | 12539 | `	if( pEngine == 0 ){` |
|      ! 0 | 12540 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12541 | `		/* Return null */` |
|      ! 0 | 12542 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12543 | `		return PH7_OK;` |
|        - | 12544 | `	}` |
|        - | 12545 | `	/* Return the engine as a resource */` |
|        5 | 12546 | `	ph7_result_resource(pCtx,pEngine);` |
|        5 | 12547 | `	return PH7_OK;` |
|        3 | 12548 |  |
|        - | 12549 | `/*` |
|        - | 12550 | ` * bool xml_parser_free(resource $parser)` |
|        - | 12551 | ` *  Release an XML engine.` |
|        - | 12552 | ` * Parameter` |
|        - | 12553 | ` *  $parser` |
|        - | 12554 | ` *   A reference to the XML parser to free.` |
|        - | 12555 | ` * Return` |
|        - | 12556 | ` *  This function returns FALSE if parser does not refer` |
|        - | 12557 | ` *  to a valid parser, or else it frees the parser and returns TRUE.` |
|        - | 12558 | ` */` |
|       84 | 12559 | `static int vm_builtin_xml_parser_free(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12560 |  |
|        - | 12561 | `	ph7_xml_engine *pEngine;` |
|       85 | 12562 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12563 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12564 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12565 | `		return PH7_OK;` |
|        - | 12566 | `	}` |
|        - | 12567 | `	/* Point to the XML engine */` |
|       85 | 12568 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       85 | 12569 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12570 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12571 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12572 | `		return PH7_OK;` |
|        - | 12573 | `	}` |
|        - | 12574 | `	/* Safely release the engine */` |
|       85 | 12575 | `	VmReleaseXMLEngine(pEngine);` |
|        - | 12576 | `	/* Return TRUE */` |
|       85 | 12577 | `	ph7_result_bool(pCtx,1);` |
|       85 | 12578 | `	return PH7_OK;` |
|       43 | 12579 |  |
|        - | 12580 | `/*` |
|        - | 12581 | ` * bool xml_set_element_handler(resource $parser,callback $start_element_handler,[callback $end_element_handler])` |
|        - | 12582 | ` * Sets the element handler functions for the XML parser. start_element_handler and end_element_handler` |
|        - | 12583 | ` * are strings containing the names of functions.` |
|        - | 12584 | ` * Parameters` |
|        - | 12585 | ` *  $parser` |
|        - | 12586 | ` *   A reference to the XML parser to set up start and end element handler functions.` |
|        - | 12587 | ` *  $start_element_handler` |
|        - | 12588 | ` *    The function named by start_element_handler must accept three parameters:` |
|        - | 12589 | ` *    start_element_handler(resource $parser,string $name,array $attribs)` |
|        - | 12590 | ` *    $parser` |
|        - | 12591 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12592 | ` *   $name` |
|        - | 12593 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 12594 | ` *		is called.If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 12595 | ` *  $attribs` |
|        - | 12596 | ` *      The third parameter, attribs, contains an associative array with the element's attributes (if any).` |
|        - | 12597 | ` *		The keys of this array are the attribute names, the values are the attribute values.` |
|        - | 12598 | ` *      Attribute names are case-folded on the same criteria as element names.Attribute values are not case-folded.` |
|        - | 12599 | ` *      The original order of the attributes can be retrieved by walking through attribs the normal way, using each().` |
|        - | 12600 | ` *      The first key in the array was the first attribute, and so on.` |
|        - | 12601 | ` *      Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 12602 | ` * $end_element_handler` |
|        - | 12603 | ` *     The function named by end_element_handler must accept two parameters:` |
|        - | 12604 | ` *     end_element_handler(resource $parser,string $name)` |
|        - | 12605 | ` *    $parser` |
|        - | 12606 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12607 | ` *   $name` |
|        - | 12608 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 12609 | ` *      is called.If case-folding is in effect for this parser, the element name will be in uppercase` |
|        - | 12610 | ` *      letters.` |
|        - | 12611 | ` *      If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 12612 | ` * Return` |
|        - | 12613 | ` * TRUE on success or FALSE on failure.` |
|        - | 12614 | ` */` |
|       66 | 12615 | `static int vm_builtin_xml_set_element_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12616 |  |
|        - | 12617 | `	ph7_xml_engine *pEngine;` |
|       67 | 12618 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12619 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12620 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12621 | `		return PH7_OK;` |
|        - | 12622 | `	}` |
|        - | 12623 | `	/* Point to the XML engine */` |
|       67 | 12624 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       67 | 12625 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12626 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12627 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12628 | `		return PH7_OK;` |
|        - | 12629 | `	}` |
|       67 | 12630 | `	if( nArg > 1 ){` |
|        - | 12631 | `		/* Save the start_element_handler callback for later invocation */` |
|       67 | 12632 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_START_TAG]);` |
|       67 | 12633 | `		if( nArg > 2 ){` |
|        - | 12634 | `			/* Save the end_element_handler callback for later invocation */` |
|       67 | 12635 | `			PH7_MemObjStore(apArg[2]/* User callback*/,&pEngine->aCB[PH7_XML_END_TAG]);` |
|       33 | 12636 | `		}` |
|       33 | 12637 | `	}` |
|        - | 12638 | `	/* All done,return TRUE */` |
|       67 | 12639 | `	ph7_result_bool(pCtx,1);` |
|       67 | 12640 | `	return PH7_OK;` |
|       34 | 12641 |  |
|        - | 12642 | `/*` |
|        - | 12643 | ` * bool xml_set_character_data_handler(resource $parser,callback $handler)` |
|        - | 12644 | ` *  Sets the character data handler function for the XML parser parser.` |
|        - | 12645 | ` * Parameters` |
|        - | 12646 | ` * $parser` |
|        - | 12647 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12648 | ` * $handler` |
|        - | 12649 | ` *  handler is a string containing the name of the callback.` |
|        - | 12650 | ` *  The function named by handler must accept two parameters:` |
|        - | 12651 | ` *   handler(resource $parser,string $data)` |
|        - | 12652 | ` *  $parser` |
|        - | 12653 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12654 | ` *  $data` |
|        - | 12655 | ` *   The second parameter, data, contains the character data as a string.` |
|        - | 12656 | ` *   Character data handler is called for every piece of a text in the XML document.` |
|        - | 12657 | ` *   It can be called multiple times inside each fragment (e.g. for non-ASCII strings).` |
|        - | 12658 | ` *   If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 12659 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12660 | ` *   can also be supplied.` |
|        - | 12661 | ` * Return` |
|        - | 12662 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12663 | ` */` |
|       40 | 12664 | `static int vm_builtin_xml_set_character_data_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12665 |  |
|        - | 12666 | `	ph7_xml_engine *pEngine;` |
|       41 | 12667 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12668 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12669 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12670 | `		return PH7_OK;` |
|        - | 12671 | `	}` |
|        - | 12672 | `	/* Point to the XML engine */` |
|       41 | 12673 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       41 | 12674 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12675 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12676 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12677 | `		return PH7_OK;` |
|        - | 12678 | `	}` |
|       41 | 12679 | `	if( nArg > 1 ){` |
|        - | 12680 | `		/* Save the user callback for later invocation */` |
|       41 | 12681 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_CDATA]);` |
|       20 | 12682 | `	}` |
|        - | 12683 | `	/* All done,return TRUE */` |
|       41 | 12684 | `	ph7_result_bool(pCtx,1);` |
|       41 | 12685 | `	return PH7_OK;` |
|       21 | 12686 |  |
|        - | 12687 | `/*` |
|        - | 12688 | ` * bool xml_set_default_handler(resource $parser,callback $handler)` |
|        - | 12689 | ` *  Set up default handler.` |
|        - | 12690 | ` * Parameters` |
|        - | 12691 | ` * $parser` |
|        - | 12692 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12693 | ` * $handler` |
|        - | 12694 | ` *  handler is a string containing the name of the callback.` |
|        - | 12695 | ` *  The function named by handler must accept two parameters:` |
|        - | 12696 | ` *   handler(resource $parser,string $data)` |
|        - | 12697 | ` *  $parser` |
|        - | 12698 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12699 | ` *  $data` |
|        - | 12700 | ` *   The second parameter, data, contains the character data.This may be the XML declaration` |
|        - | 12701 | ` *   document type declaration, entities or other data for which no other handler exists.` |
|        - | 12702 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12703 | ` *   can also be supplied.` |
|        - | 12704 | ` * Return` |
|        - | 12705 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12706 | ` */` |
|        2 | 12707 | `static int vm_builtin_xml_set_default_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12708 |  |
|        - | 12709 | `	ph7_xml_engine *pEngine;` |
|        3 | 12710 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12711 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12712 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12713 | `		return PH7_OK;` |
|        - | 12714 | `	}` |
|        - | 12715 | `	/* Point to the XML engine */` |
|        3 | 12716 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12717 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12718 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12719 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12720 | `		return PH7_OK;` |
|        - | 12721 | `	}` |
|        3 | 12722 | `	if( nArg > 1 ){` |
|        - | 12723 | `		/* Save the user callback for later invocation */` |
|        3 | 12724 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_DEF]);` |
|        1 | 12725 | `	}` |
|        - | 12726 | `	/* All done,return TRUE */` |
|        3 | 12727 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12728 | `	return PH7_OK;` |
|        2 | 12729 |  |
|        - | 12730 | `/*` |
|        - | 12731 | ` * bool xml_set_end_namespace_decl_handler(resource $parser,callback $handler)` |
|        - | 12732 | ` *  Set up end namespace declaration handler.` |
|        - | 12733 | ` * Parameters` |
|        - | 12734 | ` * $parser` |
|        - | 12735 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12736 | ` * $handler` |
|        - | 12737 | ` *  handler is a string containing the name of the callback.` |
|        - | 12738 | ` *  The function named by handler must accept two parameters:` |
|        - | 12739 | ` *   handler(resource $parser,string $prefix)` |
|        - | 12740 | ` *  $parser` |
|        - | 12741 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12742 | ` *  $prefix` |
|        - | 12743 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 12744 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12745 | ` *   can also be supplied.` |
|        - | 12746 | ` * Return` |
|        - | 12747 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12748 | ` */` |
|        2 | 12749 | `static int vm_builtin_xml_set_end_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12750 |  |
|        - | 12751 | `	ph7_xml_engine *pEngine;` |
|        3 | 12752 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12753 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12754 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12755 | `		return PH7_OK;` |
|        - | 12756 | `	}` |
|        - | 12757 | `	/* Point to the XML engine */` |
|        3 | 12758 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12759 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12760 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12761 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12762 | `		return PH7_OK;` |
|        - | 12763 | `	}` |
|        3 | 12764 | `	if( nArg > 1 ){` |
|        - | 12765 | `		/* Save the user callback for later invocation */` |
|        3 | 12766 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_NS_END]);` |
|        1 | 12767 | `	}` |
|        - | 12768 | `	/* All done,return TRUE */` |
|        3 | 12769 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12770 | `	return PH7_OK;` |
|        2 | 12771 |  |
|        - | 12772 | `/*` |
|        - | 12773 | ` * bool xml_set_start_namespace_decl_handler(resource $parser,callback $handler)` |
|        - | 12774 | ` *  Set up start namespace declaration handler.` |
|        - | 12775 | ` * Parameters` |
|        - | 12776 | ` * $parser` |
|        - | 12777 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12778 | ` * $handler` |
|        - | 12779 | ` *  handler is a string containing the name of the callback.` |
|        - | 12780 | ` *  The function named by handler must accept two parameters:` |
|        - | 12781 | ` *   handler(resource $parser,string $prefix,string $uri)` |
|        - | 12782 | ` *  $parser` |
|        - | 12783 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12784 | ` *  $prefix` |
|        - | 12785 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 12786 | ` *  $uri` |
|        - | 12787 | ` *    Uniform Resource Identifier (URI) of namespace.` |
|        - | 12788 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12789 | ` *   can also be supplied.` |
|        - | 12790 | ` * Return` |
|        - | 12791 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12792 | ` */` |
|        2 | 12793 | `static int vm_builtin_xml_set_start_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12794 |  |
|        - | 12795 | `	ph7_xml_engine *pEngine;` |
|        3 | 12796 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12797 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12798 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12799 | `		return PH7_OK;` |
|        - | 12800 | `	}` |
|        - | 12801 | `	/* Point to the XML engine */` |
|        3 | 12802 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12803 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12804 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12805 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12806 | `		return PH7_OK;` |
|        - | 12807 | `	}` |
|        3 | 12808 | `	if( nArg > 1 ){` |
|        - | 12809 | `		/* Save the user callback for later invocation */` |
|        3 | 12810 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_NS_START]);` |
|        1 | 12811 | `	}` |
|        - | 12812 | `	/* All done,return TRUE */` |
|        3 | 12813 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12814 | `	return PH7_OK;` |
|        2 | 12815 |  |
|        - | 12816 | `/*` |
|        - | 12817 | ` * bool xml_set_processing_instruction_handler(resource $parser,callback $handler)` |
|        - | 12818 | ` *  Set up processing instruction (PI) handler.` |
|        - | 12819 | ` * Parameters` |
|        - | 12820 | ` * $parser` |
|        - | 12821 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12822 | ` * $handler` |
|        - | 12823 | ` *  handler is a string containing the name of the callback.` |
|        - | 12824 | ` *  The function named by handler must accept three parameters:` |
|        - | 12825 | ` *   handler(resource $parser,string $target,string $data)` |
|        - | 12826 | ` *  $parser` |
|        - | 12827 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12828 | ` *  $target` |
|        - | 12829 | ` *   The second parameter, target, contains the PI target.` |
|        - | 12830 | ` *  $data` |
|        - | 12831 | `     The third parameter, data, contains the PI data.` |
|        - | 12832 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12833 | ` *   can also be supplied.` |
|        - | 12834 | ` * Return` |
|        - | 12835 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12836 | ` */` |
|        8 | 12837 | `static int vm_builtin_xml_set_processing_instruction_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12838 |  |
|        - | 12839 | `	ph7_xml_engine *pEngine;` |
|        9 | 12840 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12841 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12842 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12843 | `		return PH7_OK;` |
|        - | 12844 | `	}` |
|        - | 12845 | `	/* Point to the XML engine */` |
|        9 | 12846 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        9 | 12847 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12848 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12849 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12850 | `		return PH7_OK;` |
|        - | 12851 | `	}` |
|        9 | 12852 | `	if( nArg > 1 ){` |
|        - | 12853 | `		/* Save the user callback for later invocation */` |
|        9 | 12854 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_PI]);` |
|        4 | 12855 | `	}` |
|        - | 12856 | `	/* All done,return TRUE */` |
|        9 | 12857 | `	ph7_result_bool(pCtx,1);` |
|        9 | 12858 | `	return PH7_OK;` |
|        5 | 12859 |  |
|        - | 12860 | `/*` |
|        - | 12861 | ` * bool xml_set_unparsed_entity_decl_handler(resource $parser,callback $handler)` |
|        - | 12862 | ` *  Set up unparsed entity declaration handler.` |
|        - | 12863 | ` * Parameters` |
|        - | 12864 | ` * $parser` |
|        - | 12865 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12866 | ` * $handler` |
|        - | 12867 | ` *  handler is a string containing the name of the callback.` |
|        - | 12868 | ` *  The function named by handler must accept six parameters:` |
|        - | 12869 | ` *  handler(resource $parser,string $entity_name,string $base,string $system_id,string $public_id,string $notation_name)` |
|        - | 12870 | ` *  $parser` |
|        - | 12871 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12872 | ` *  $entity_name` |
|        - | 12873 | ` *   The name of the entity that is about to be defined.` |
|        - | 12874 | ` *  $base` |
|        - | 12875 | ` *   This is the base for resolving the system identifier (systemId) of the external entity.` |
|        - | 12876 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12877 | ` *  $system_id` |
|        - | 12878 | ` *   System identifier for the external entity.` |
|        - | 12879 | ` *  $public_id` |
|        - | 12880 | ` *    Public identifier for the external entity.` |
|        - | 12881 | ` *  $notation_name` |
|        - | 12882 | ` *    Name of the notation of this entity (see xml_set_notation_decl_handler()).` |
|        - | 12883 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12884 | ` *   can also be supplied.` |
|        - | 12885 | ` * Return` |
|        - | 12886 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12887 | ` */` |
|        2 | 12888 | `static int vm_builtin_xml_set_unparsed_entity_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12889 |  |
|        - | 12890 | `	ph7_xml_engine *pEngine;` |
|        3 | 12891 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12892 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12893 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12894 | `		return PH7_OK;` |
|        - | 12895 | `	}` |
|        - | 12896 | `	/* Point to the XML engine */` |
|        3 | 12897 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12898 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12899 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12900 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12901 | `		return PH7_OK;` |
|        - | 12902 | `	}` |
|        3 | 12903 | `	if( nArg > 1 ){` |
|        - | 12904 | `		/* Save the user callback for later invocation */` |
|        3 | 12905 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_UNPED]);` |
|        1 | 12906 | `	}` |
|        - | 12907 | `	/* All done,return TRUE */` |
|        3 | 12908 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12909 | `	return PH7_OK;` |
|        2 | 12910 |  |
|        - | 12911 | `/*` |
|        - | 12912 | ` * bool xml_set_notation_decl_handler(resource $parser,callback $handler)` |
|        - | 12913 | ` *  Set up notation declaration handler.` |
|        - | 12914 | ` * Parameters` |
|        - | 12915 | ` * $parser` |
|        - | 12916 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12917 | ` * $handler` |
|        - | 12918 | ` *  handler is a string containing the name of the callback.` |
|        - | 12919 | ` *  The function named by handler must accept five parameters:` |
|        - | 12920 | ` *  handler(resource $parser,string $entity_name,string $base,string $system_id,string $public_id)` |
|        - | 12921 | ` *  $parser` |
|        - | 12922 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12923 | ` *  $entity_name` |
|        - | 12924 | ` *   The name of the entity that is about to be defined.` |
|        - | 12925 | ` *  $base` |
|        - | 12926 | ` *   This is the base for resolving the system identifier (systemId) of the external entity.` |
|        - | 12927 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12928 | ` *  $system_id` |
|        - | 12929 | ` *   System identifier for the external entity.` |
|        - | 12930 | ` *  $public_id` |
|        - | 12931 | ` *    Public identifier for the external entity.` |
|        - | 12932 | ` *  Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12933 | ` *  can also be supplied.` |
|        - | 12934 | ` * Return` |
|        - | 12935 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12936 | ` */` |
|        2 | 12937 | `static int vm_builtin_xml_set_notation_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12938 |  |
|        - | 12939 | `	ph7_xml_engine *pEngine;` |
|        3 | 12940 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12941 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12942 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12943 | `		return PH7_OK;` |
|        - | 12944 | `	}` |
|        - | 12945 | `	/* Point to the XML engine */` |
|        3 | 12946 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12947 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12948 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12949 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12950 | `		return PH7_OK;` |
|        - | 12951 | `	}` |
|        3 | 12952 | `	if( nArg > 1 ){` |
|        - | 12953 | `		/* Save the user callback for later invocation */` |
|        3 | 12954 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_ND]);` |
|        1 | 12955 | `	}` |
|        - | 12956 | `	/* All done,return TRUE */` |
|        3 | 12957 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12958 | `	return PH7_OK;` |
|        2 | 12959 |  |
|        - | 12960 | `/*` |
|        - | 12961 | ` * bool xml_set_external_entity_ref_handler(resource $parser,callback $handler)` |
|        - | 12962 | ` *  Set up external entity reference handler.` |
|        - | 12963 | ` * Parameters` |
|        - | 12964 | ` * $parser` |
|        - | 12965 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12966 | ` * $handler` |
|        - | 12967 | ` *  handler is a string containing the name of the callback.` |
|        - | 12968 | ` *  The function named by handler must accept five parameters:` |
|        - | 12969 | ` *   handler(resource $parser,string $open_entity_names,string $base,string $system_id,string $public_id)` |
|        - | 12970 | ` *  $parser` |
|        - | 12971 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12972 | ` *  $open_entity_names` |
|        - | 12973 | ` *   The second parameter, open_entity_names, is a space-separated list of the names` |
|        - | 12974 | ` *   of the entities that are open for the parse of this entity (including the name of the referenced entity).` |
|        - | 12975 | ` *  $base` |
|        - | 12976 | ` *   This is the base for resolving the system identifier (system_id) of the external entity.` |
|        - | 12977 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12978 | ` *  $system_id` |
|        - | 12979 | ` *   The fourth parameter, system_id, is the system identifier as specified in the entity declaration.` |
|        - | 12980 | ` *  $public_id` |
|        - | 12981 | ` *   The fifth parameter, public_id, is the public identifier as specified in the entity declaration` |
|        - | 12982 | ` *   or an empty string if none was specified; the whitespace in the public identifier will have been` |
|        - | 12983 | ` *   normalized as required by the XML spec.` |
|        - | 12984 | ` * Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12985 | ` * can also be supplied.` |
|        - | 12986 | ` * Return` |
|        - | 12987 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12988 | ` */` |
|        2 | 12989 | `static int vm_builtin_xml_set_external_entity_ref_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12990 |  |
|        - | 12991 | `	ph7_xml_engine *pEngine;` |
|        3 | 12992 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12993 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12994 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12995 | `		return PH7_OK;` |
|        - | 12996 | `	}` |
|        - | 12997 | `	/* Point to the XML engine */` |
|        3 | 12998 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12999 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13000 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13001 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13002 | `		return PH7_OK;` |
|        - | 13003 | `	}` |
|        3 | 13004 | `	if( nArg > 1 ){` |
|        - | 13005 | `		/* Save the user callback for later invocation */` |
|        3 | 13006 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_EER]);` |
|        1 | 13007 | `	}` |
|        - | 13008 | `	/* All done,return TRUE */` |
|        3 | 13009 | `	ph7_result_bool(pCtx,1);` |
|        3 | 13010 | `	return PH7_OK;` |
|        2 | 13011 |  |
|        - | 13012 | `/*` |
|        - | 13013 | ` * int xml_get_current_line_number(resource $parser)` |
|        - | 13014 | ` *  Gets the current line number for the given XML parser.` |
|        - | 13015 | ` * Parameters` |
|        - | 13016 | ` * $parser` |
|        - | 13017 | ` *   A reference to the XML parser.` |
|        - | 13018 | ` * Return` |
|        - | 13019 | ` *  This function returns FALSE if parser does not refer` |
|        - | 13020 | ` *  to a valid parser, or else it returns which line the parser` |
|        - | 13021 | ` *  is currently at in its data buffer.` |
|        - | 13022 | ` */` |
|        8 | 13023 | `static int vm_builtin_xml_get_current_line_number(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13024 |  |
|        - | 13025 | `	ph7_xml_engine *pEngine;` |
|        9 | 13026 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13027 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13028 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13029 | `		return PH7_OK;` |
|        - | 13030 | `	}` |
|        - | 13031 | `	/* Point to the XML engine */` |
|        9 | 13032 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        9 | 13033 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13034 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13035 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13036 | `		return PH7_OK;` |
|        - | 13037 | `	}` |
|        - | 13038 | `	/* Return the line number */` |
|        9 | 13039 | `	ph7_result_int(pCtx,(int)pEngine->nLine);` |
|        9 | 13040 | `	return PH7_OK;` |
|        5 | 13041 |  |
|        - | 13042 | `/*` |
|        - | 13043 | ` * int xml_get_current_byte_index(resource $parser)` |
|        - | 13044 | ` *  Gets the current byte index of the given XML parser.` |
|        - | 13045 | ` * Parameters` |
|        - | 13046 | ` * $parser` |
|        - | 13047 | ` *   A reference to the XML parser.` |
|        - | 13048 | ` * Return` |
|        - | 13049 | ` *  This function returns FALSE if parser does not refer to a valid` |
|        - | 13050 | ` *  parser, or else it returns which byte index the parser is currently` |
|        - | 13051 | ` *  at in its data buffer (starting at 0).` |
|        - | 13052 | ` */` |
|        4 | 13053 | `static int vm_builtin_xml_get_current_byte_index(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13054 |  |
|        - | 13055 | `	ph7_xml_engine *pEngine;` |
|        - | 13056 | `	SyStream *pStream;` |
|        - | 13057 | `	SyToken *pToken;` |
|        5 | 13058 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13059 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13060 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13061 | `		return PH7_OK;` |
|        - | 13062 | `	}` |
|        - | 13063 | `	/* Point to the XML engine */` |
|        5 | 13064 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        5 | 13065 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13066 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13067 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13068 | `		return PH7_OK;` |
|        - | 13069 | `	}` |
|        - | 13070 | `	/* Point to the current processed token */` |
|        5 | 13071 | `	pToken = (SyToken *)SySetPeekCurrentEntry(&pEngine->sParser.sToken);` |
|        5 | 13072 | `	if( pToken == 0 ){` |
|        - | 13073 | `		/* Stream not yet processed */` |
|        3 | 13074 | `		ph7_result_int(pCtx,0);` |
|        3 | 13075 | `		return 0;` |
|        - | 13076 | `	}` |
|        - | 13077 | `	/* Point to the input stream */` |
|        3 | 13078 | `	pStream = &pEngine->sParser.sLex.sStream;` |
|        - | 13079 | `	/* Return the byte index */` |
|        3 | 13080 | `	ph7_result_int64(pCtx,(ph7_int64)(pToken->sData.zString-(const char *)pStream->zInput));` |
|        3 | 13081 | `	return PH7_OK;` |
|        3 | 13082 |  |
|        - | 13083 | `/*` |
|        - | 13084 | ` * bool xml_set_object(resource $parser,object &$object)` |
|        - | 13085 | ` *  Use XML Parser within an object.` |
|        - | 13086 | ` * NOTE` |
|        - | 13087 | ` *  This function is depreceated and is a no-op.` |
|        - | 13088 | ` * Parameters` |
|        - | 13089 | ` * $parser` |
|        - | 13090 | ` *   A reference to the XML parser.` |
|        - | 13091 | ` * $object` |
|        - | 13092 | ` *  The object where to use the XML parser.` |
|        - | 13093 | ` * Return` |
|        - | 13094 | ` * Always FALSE.` |
|        - | 13095 | ` */` |
|        2 | 13096 | `static int vm_builtin_xml_set_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13097 |  |
|        - | 13098 | `	ph7_xml_engine *pEngine;` |
|        3 | 13099 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_object(apArg[1]) ){` |
|        - | 13100 | `		/* Missing/Ivalid argument,return FALSE */` |
|        3 | 13101 | `		ph7_result_bool(pCtx,0);` |
|        3 | 13102 | `		return PH7_OK;` |
|        - | 13103 | `	}` |
|        - | 13104 | `	/* Point to the XML engine */` |
|      ! 0 | 13105 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|      ! 0 | 13106 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13107 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13108 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13109 | `		return PH7_OK;` |
|        - | 13110 | `	}` |
|        - | 13111 | `	/*  Throw a notice and return */` |
|      ! 0 | 13112 | `	ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"This function is depreceated and is a no-op."` |
|        - | 13113 | `		"In order to mimic this behaviour,you can supply instead of a function name an array "` |
|        - | 13114 | `		"containing an object reference and a method name."` |
|        - | 13115 | `		);` |
|        - | 13116 | `	/* Return FALSE */` |
|      ! 0 | 13117 | `	ph7_result_bool(pCtx,0);` |
|      ! 0 | 13118 | `	return PH7_OK;` |
|        2 | 13119 |  |
|        - | 13120 | `/*` |
|        - | 13121 | ` * int xml_get_current_column_number(resource $parser)` |
|        - | 13122 | ` *  Gets the current column number of the given XML parser.` |
|        - | 13123 | ` * Parameters` |
|        - | 13124 | ` * $parser` |
|        - | 13125 | ` *   A reference to the XML parser.` |
|        - | 13126 | ` * Return` |
|        - | 13127 | ` *  This function returns FALSE if parser does not refer to a valid parser, or else it returns` |
|        - | 13128 | ` *  which column on the current line (as given by xml_get_current_line_number()) the parser` |
|        - | 13129 | ` *  is currently at.` |
|        - | 13130 | ` */` |
|        4 | 13131 | `static int vm_builtin_xml_get_current_column_number(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13132 |  |
|        - | 13133 | `	ph7_xml_engine *pEngine;` |
|        - | 13134 | `	SyStream *pStream;` |
|        - | 13135 | `	SyToken *pToken;` |
|        5 | 13136 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13137 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13138 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13139 | `		return PH7_OK;` |
|        - | 13140 | `	}` |
|        - | 13141 | `	/* Point to the XML engine */` |
|        5 | 13142 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        5 | 13143 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13144 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13145 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13146 | `		return PH7_OK;` |
|        - | 13147 | `	}` |
|        - | 13148 | `	/* Point to the current processed token */` |
|        5 | 13149 | `	pToken = (SyToken *)SySetPeekCurrentEntry(&pEngine->sParser.sToken);` |
|        5 | 13150 | `	if( pToken == 0 ){` |
|        - | 13151 | `		/* Stream not yet processed */` |
|      ! 0 | 13152 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 13153 | `		return 0;` |
|        - | 13154 | `	}` |
|        - | 13155 | `	/* Point to the input stream */` |
|        5 | 13156 | `	pStream = &pEngine->sParser.sLex.sStream;` |
|        - | 13157 | `	/* Return the byte index */` |
|        5 | 13158 | `	ph7_result_int64(pCtx,(ph7_int64)(pToken->sData.zString-(const char *)pStream->zInput)/80);` |
|        5 | 13159 | `	return PH7_OK;` |
|        3 | 13160 |  |
|        - | 13161 | `/*` |
|        - | 13162 | ` * int xml_get_error_code(resource $parser)` |
|        - | 13163 | ` *  Get XML parser error code.` |
|        - | 13164 | ` * Parameters` |
|        - | 13165 | ` * $parser` |
|        - | 13166 | ` *   A reference to the XML parser.` |
|        - | 13167 | ` * Return` |
|        - | 13168 | ` *  This function returns FALSE if parser does not refer to a valid` |
|        - | 13169 | ` *  parser, or else it returns one of the error codes listed in the error` |
|        - | 13170 | ` *  codes section.` |
|        - | 13171 | ` */` |
|       32 | 13172 | `static int vm_builtin_xml_get_error_code(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13173 |  |
|        - | 13174 | `	ph7_xml_engine *pEngine;` |
|       33 | 13175 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13176 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13177 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13178 | `		return PH7_OK;` |
|        - | 13179 | `	}` |
|        - | 13180 | `	/* Point to the XML engine */` |
|       33 | 13181 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       33 | 13182 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13183 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13184 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13185 | `		return PH7_OK;` |
|        - | 13186 | `	}` |
|        - | 13187 | `	/* Return the error code if any */` |
|       33 | 13188 | `	ph7_result_int(pCtx,pEngine->iErrCode);` |
|       33 | 13189 | `	return PH7_OK;` |
|       17 | 13190 |  |
|        - | 13191 | `/*` |
|        - | 13192 | ` * XML parser event callbacks` |
|        - | 13193 | ` * Each time the unserlying XML parser extract a single token` |
|        - | 13194 | ` * from the input,one of the following callbacks are invoked.` |
|        - | 13195 | ` * IMP-XML-ENGINE-07-07-2012 22:02 FreeBSD [chm@symisc.net]` |
|        - | 13196 | ` */` |
|        - | 13197 | `/*` |
|        - | 13198 | ` * Create a scalar ph7_value holding the value` |
|        - | 13199 | ` * of an XML tag/attribute/CDATA and so on.` |
|        - | 13200 | ` */` |
|      148 | 13201 | `static ph7_value * VmXMLValue(ph7_xml_engine *pEngine,SyXMLRawStr *pXML,SyXMLRawStr *pNsUri)` |
|        1 | 13202 |  |
|        - | 13203 | `	ph7_value *pValue;` |
|        - | 13204 | `	/* Allocate a new scalar variable */` |
|      149 | 13205 | `	pValue = ph7_context_new_scalar(pEngine->pCtx);` |
|      149 | 13206 | `	if( pValue == 0 ){` |
|      ! 0 | 13207 | `		ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13208 | `		return 0;` |
|        - | 13209 | `	}` |
|      149 | 13210 | `	if( pNsUri && pNsUri->nByte > 0 ){` |
|        - | 13211 | `		/* Append namespace URI and the separator */` |
|        9 | 13212 | `		ph7_value_string_format(pValue,"%.*s%c",pNsUri->nByte,pNsUri->zString,pEngine->ns_sep);` |
|        4 | 13213 | `	}` |
|        - | 13214 | `	/* Copy the tag value */` |
|      149 | 13215 | `	ph7_value_string(pValue,pXML->zString,(int)pXML->nByte);` |
|      149 | 13216 | `	return pValue;` |
|       75 | 13217 |  |
|        - | 13218 | `/*` |
|        - | 13219 | ` * Create a 'ph7_value' of type array holding the values` |
|        - | 13220 | ` * of an XML tag attributes.` |
|        - | 13221 | ` */` |
|       62 | 13222 | `static ph7_value * VmXMLAttrValue(ph7_xml_engine *pEngine,SyXMLRawStr *aAttr,sxu32 nAttr)` |
|        1 | 13223 |  |
|        - | 13224 | `	ph7_value *pArray;` |
|        - | 13225 | `	/* Create an empty array */` |
|       63 | 13226 | `	pArray = ph7_context_new_array(pEngine->pCtx);` |
|       63 | 13227 | `	if( pArray == 0 ){` |
|      ! 0 | 13228 | `		ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13229 | `		return 0;` |
|        - | 13230 | `	}` |
|       63 | 13231 | `	if( nAttr > 0 ){` |
|        - | 13232 | `		ph7_value *pKey,*pValue;` |
|        - | 13233 | `		sxu32 n;` |
|        - | 13234 | `		/* Create worker variables */` |
|        5 | 13235 | `		pKey = ph7_context_new_scalar(pEngine->pCtx);` |
|        5 | 13236 | `		pValue = ph7_context_new_scalar(pEngine->pCtx);` |
|        5 | 13237 | `		if( pKey == 0 \|\| pValue == 0 ){` |
|      ! 0 | 13238 | `			ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13239 | `			return 0;` |
|        - | 13240 | `		}` |
|        - | 13241 | `		/* Copy attributes */` |
|        9 | 13242 | `		for( n = 0 ; n < nAttr ; n += 2 ){` |
|        - | 13243 | `			/* Reset string cursors */` |
|        5 | 13244 | `			ph7_value_reset_string_cursor(pKey);` |
|        5 | 13245 | `			ph7_value_reset_string_cursor(pValue);` |
|        - | 13246 | `			/* Copy attribute name and it's associated value */` |
|        5 | 13247 | `			ph7_value_string(pKey,aAttr[n].zString,(int)aAttr[n].nByte); /* Attribute name */` |
|        5 | 13248 | `			ph7_value_string(pValue,aAttr[n+1].zString,(int)aAttr[n+1].nByte); /* Attribute value */` |
|        - | 13249 | `			/* Insert in the array */` |
|        5 | 13250 | `			ph7_array_add_elem(pArray,pKey,pValue); /* Will make it's own copy */` |
|        3 | 13251 | `		}` |
|        - | 13252 | `		/* Release the worker variables */` |
|        5 | 13253 | `		ph7_context_release_value(pEngine->pCtx,pKey);` |
|        5 | 13254 | `		ph7_context_release_value(pEngine->pCtx,pValue);` |
|        2 | 13255 | `	}` |
|        - | 13256 | `	/* Return the freshly created array */` |
|       63 | 13257 | `	return pArray;` |
|       32 | 13258 |  |
|        - | 13259 | `/*` |
|        - | 13260 | ` * Start element handler.` |
|        - | 13261 | ` * The user defined callback must accept three parameters:` |
|        - | 13262 | ` *    start_element_handler(resource $parser,string $name,array $attribs )` |
|        - | 13263 | ` *    $parser` |
|        - | 13264 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13265 | ` *    $name` |
|        - | 13266 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 13267 | ` *		is called.If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 13268 | ` *    $attribs` |
|        - | 13269 | ` *      The third parameter, attribs, contains an associative array with the element's attributes (if any).` |
|        - | 13270 | ` *		The keys of this array are the attribute names, the values are the attribute values.` |
|        - | 13271 | ` *      Attribute names are case-folded on the same criteria as element names.Attribute values are not case-folded.` |
|        - | 13272 | ` *      The original order of the attributes can be retrieved by walking through attribs the normal way, using each().` |
|        - | 13273 | ` *      The first key in the array was the first attribute, and so on.` |
|        - | 13274 | ` *      Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 13275 | ` */` |
|       78 | 13276 | `static sxi32 VmXMLStartElementHandler(SyXMLRawStr *pStart,SyXMLRawStr *pNS,sxu32 nAttr,SyXMLRawStr *aAttr,void *pUserData)` |
|        1 | 13277 |  |
|       79 | 13278 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13279 | `	ph7_value *pCallback,*pTag,*pAttr;` |
|        - | 13280 | `	/* Point to the target user defined callback */` |
|       79 | 13281 | `	pCallback = &pEngine->aCB[PH7_XML_START_TAG];` |
|        - | 13282 | `	/* Make sure the given callback is callable */` |
|       79 | 13283 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13284 | `		/* Not callable,return immediately*/` |
|       17 | 13285 | `		return SXRET_OK;` |
|        - | 13286 | `	}` |
|        - | 13287 | `	/* Create a ph7_value holding the tag name */` |
|       63 | 13288 | `	pTag = VmXMLValue(pEngine,pStart,pNS);` |
|        - | 13289 | `	/* Create a ph7_value holding the tag attributes */` |
|       63 | 13290 | `	pAttr = VmXMLAttrValue(pEngine,aAttr,nAttr);` |
|       63 | 13291 | `	if( pTag == 0  \|\| pAttr == 0 ){` |
|      ! 0 | 13292 | `		SXUNUSED(pNS); /* cc warning */` |
|        - | 13293 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13294 | `		return SXRET_OK;` |
|        - | 13295 | `	}` |
|        - | 13296 | `	/* Invoke the user callback */` |
|       63 | 13297 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTag,pAttr,(ph7_value*)0);` |
|        - | 13298 | `	/* Clean-up the mess left behind */` |
|       63 | 13299 | `	ph7_context_release_value(pEngine->pCtx,pTag);` |
|       63 | 13300 | `	ph7_context_release_value(pEngine->pCtx,pAttr);` |
|       63 | 13301 | `	return SXRET_OK;` |
|       40 | 13302 |  |
|        - | 13303 | `/*` |
|        - | 13304 | ` * End element handler.` |
|        - | 13305 | ` * The user defined callback must accept two parameters:` |
|        - | 13306 | ` *  end_element_handler(resource $parser,string $name)` |
|        - | 13307 | ` *  $parser` |
|        - | 13308 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13309 | ` *  $name` |
|        - | 13310 | ` *   The second parameter, name, contains the name of the element for which this handler is called.` |
|        - | 13311 | ` *   If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 13312 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 13313 | ` *   can also be supplied.` |
|        - | 13314 | ` */` |
|       62 | 13315 | `static sxi32 VmXMLEndElementHandler(SyXMLRawStr *pEnd,SyXMLRawStr *pNS,void *pUserData)` |
|        1 | 13316 |  |
|       63 | 13317 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13318 | `	ph7_value *pCallback,*pTag;` |
|        - | 13319 | `	/* Point to the target user defined callback */` |
|       63 | 13320 | `	pCallback = &pEngine->aCB[PH7_XML_END_TAG];` |
|        - | 13321 | `	/* Make sure the given callback is callable */` |
|       63 | 13322 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13323 | `		/* Not callable,return immediately*/` |
|        9 | 13324 | `		return SXRET_OK;` |
|        - | 13325 | `	}` |
|        - | 13326 | `	/* Create a ph7_value holding the tag name */` |
|       55 | 13327 | `	pTag = VmXMLValue(pEngine,pEnd,pNS);` |
|       55 | 13328 | `	if( pTag == 0  ){` |
|      ! 0 | 13329 | `		SXUNUSED(pNS); /* cc warning */` |
|        - | 13330 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13331 | `		return SXRET_OK;` |
|        - | 13332 | `	}` |
|        - | 13333 | `	/* Invoke the user callback */` |
|       55 | 13334 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTag,(ph7_value*)0);` |
|        - | 13335 | `	/* Clean-up the mess left behind */` |
|       55 | 13336 | `	ph7_context_release_value(pEngine->pCtx,pTag);` |
|       55 | 13337 | `	return SXRET_OK;` |
|       32 | 13338 |  |
|        - | 13339 | `/*` |
|        - | 13340 | ` * Character data handler.` |
|        - | 13341 | ` *  The user defined callback must accept two parameters:` |
|        - | 13342 | ` *  handler(resource $parser,string $data)` |
|        - | 13343 | ` *  $parser` |
|        - | 13344 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13345 | ` *  $data` |
|        - | 13346 | ` *   The second parameter, data, contains the character data as a string.` |
|        - | 13347 | ` *   Character data handler is called for every piece of a text in the XML document.` |
|        - | 13348 | ` *   It can be called multiple times inside each fragment (e.g. for non-ASCII strings).` |
|        - | 13349 | ` *   If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 13350 | ` *   Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 13351 | ` */` |
|       28 | 13352 | `static sxi32 VmXMLTextHandler(SyXMLRawStr *pText,void *pUserData)` |
|        1 | 13353 |  |
|       29 | 13354 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13355 | `	ph7_value *pCallback,*pData;` |
|        - | 13356 | `	/* Point to the target user defined callback */` |
|       29 | 13357 | `	pCallback = &pEngine->aCB[PH7_XML_CDATA];` |
|        - | 13358 | `	/* Make sure the given callback is callable */` |
|       29 | 13359 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13360 | `		/* Not callable,return immediately*/` |
|       11 | 13361 | `		return SXRET_OK;` |
|        - | 13362 | `	}` |
|        - | 13363 | `	/* Create a ph7_value holding the data */` |
|       19 | 13364 | `	pData = VmXMLValue(pEngine,&(*pText),0);` |
|       19 | 13365 | `	if( pData == 0  ){` |
|        - | 13366 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13367 | `		return SXRET_OK;` |
|        - | 13368 | `	}` |
|        - | 13369 | `	/* Invoke the user callback */` |
|       19 | 13370 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pData,(ph7_value*)0);` |
|        - | 13371 | `	/* Clean-up the mess left behind */` |
|       19 | 13372 | `	ph7_context_release_value(pEngine->pCtx,pData);` |
|       19 | 13373 | `	return SXRET_OK;` |
|       15 | 13374 |  |
|        - | 13375 | `/*` |
|        - | 13376 | ` * Processing instruction (PI) handler.` |
|        - | 13377 | ` * The user defined callback must accept two parameters:` |
|        - | 13378 | ` *   handler(resource $parser,string $target,string $data)` |
|        - | 13379 | ` *  $parser` |
|        - | 13380 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13381 | ` *  $target` |
|        - | 13382 | ` *   The second parameter, target, contains the PI target.` |
|        - | 13383 | ` *  $data` |
|        - | 13384 | ` *    The third parameter, data, contains the PI data.` |
|        - | 13385 | ` *    Note: Instead of a function name, an array containing an object reference` |
|        - | 13386 | ` *    and a method name can also be supplied.` |
|        - | 13387 | ` */` |
|        8 | 13388 | `static sxi32 VmXMLPIHandler(SyXMLRawStr *pTargetStr,SyXMLRawStr *pDataStr,void *pUserData)` |
|        1 | 13389 |  |
|        9 | 13390 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13391 | `	ph7_value *pCallback,*pTarget,*pData;` |
|        - | 13392 | `	/* Point to the target user defined callback */` |
|        9 | 13393 | `	pCallback = &pEngine->aCB[PH7_XML_PI];` |
|        - | 13394 | `	/* Make sure the given callback is callable */` |
|        9 | 13395 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13396 | `		/* Not callable,return immediately*/` |
|        5 | 13397 | `		return SXRET_OK;` |
|        - | 13398 | `	}` |
|        - | 13399 | `	/* Get a ph7_value holding the data */` |
|        5 | 13400 | `	pTarget = VmXMLValue(pEngine,&(*pTargetStr),0);` |
|        5 | 13401 | `	pData = VmXMLValue(pEngine,&(*pDataStr),0);` |
|        5 | 13402 | `	if( pTarget == 0 \|\| pData == 0  ){` |
|        - | 13403 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13404 | `		return SXRET_OK;` |
|        - | 13405 | `	}` |
|        - | 13406 | `	/* Invoke the user callback */` |
|        5 | 13407 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTarget,pData,(ph7_value*)0);` |
|        - | 13408 | `	/* Clean-up the mess left behind */` |
|        5 | 13409 | `	ph7_context_release_value(pEngine->pCtx,pTarget);` |
|        5 | 13410 | `	ph7_context_release_value(pEngine->pCtx,pData);` |
|        5 | 13411 | `	return SXRET_OK;` |
|        5 | 13412 |  |
|        - | 13413 | `/*` |
|        - | 13414 | ` * Namespace declaration handler.` |
|        - | 13415 | ` * The user defined callback must accept two parameters:` |
|        - | 13416 | ` *    handler(resource $parser,string $prefix,string $uri)` |
|        - | 13417 | ` * $parser` |
|        - | 13418 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13419 | ` * $prefix` |
|        - | 13420 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 13421 | ` * $uri` |
|        - | 13422 | ` *   Uniform Resource Identifier (URI) of namespace.` |
|        - | 13423 | ` *   Note: Instead of a function name, an array containing an object reference` |
|        - | 13424 | ` *   and a method name can also be supplied.` |
|        - | 13425 | ` */` |
|        4 | 13426 | `static sxi32 VmXMLNSStartHandler(SyXMLRawStr *pUriStr,SyXMLRawStr *pPrefixStr,void *pUserData)` |
|        1 | 13427 |  |
|        5 | 13428 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13429 | `	ph7_value *pCallback,*pUri,*pPrefix;` |
|        - | 13430 | `	/* Point to the target user defined callback */` |
|        5 | 13431 | `	pCallback = &pEngine->aCB[PH7_XML_NS_START];` |
|        - | 13432 | `	/* Make sure the given callback is callable */` |
|        5 | 13433 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13434 | `		/* Not callable,return immediately*/` |
|        3 | 13435 | `		return SXRET_OK;` |
|        - | 13436 | `	}` |
|        - | 13437 | `	/* Get a ph7_value holding the PREFIX/URI */` |
|        3 | 13438 | `	pUri = VmXMLValue(pEngine,pUriStr,0);` |
|        3 | 13439 | `	pPrefix = VmXMLValue(pEngine,pPrefixStr,0);` |
|        3 | 13440 | `	if( pUri == 0 \|\| pPrefix == 0  ){` |
|        - | 13441 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13442 | `		return SXRET_OK;` |
|        - | 13443 | `	}` |
|        - | 13444 | `	/* Invoke the user callback */` |
|        3 | 13445 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pUri,pPrefix,(ph7_value*)0);` |
|        - | 13446 | `	/* Clean-up the mess left behind */` |
|        3 | 13447 | `	ph7_context_release_value(pEngine->pCtx,pUri);` |
|        3 | 13448 | `	ph7_context_release_value(pEngine->pCtx,pPrefix);` |
|        3 | 13449 | `	return SXRET_OK;` |
|        3 | 13450 |  |
|        - | 13451 | `/*` |
|        - | 13452 | ` * Namespace end declaration handler.` |
|        - | 13453 | ` * The user defined callback must accept two parameters:` |
|        - | 13454 | ` *    handler(resource $parser,string $prefix)` |
|        - | 13455 | ` * $parser` |
|        - | 13456 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13457 | ` * $prefix` |
|        - | 13458 | ` *  The prefix is a string used to reference the namespace within an XML object.` |
|        - | 13459 | ` *   Note: Instead of a function name, an array containing an object reference` |
|        - | 13460 | ` *   and a method name can also be supplied.` |
|        - | 13461 | ` */` |
|        4 | 13462 | `static sxi32 VmXMLNSEndHandler(SyXMLRawStr *pPrefixStr,void *pUserData)` |
|        1 | 13463 |  |
|        5 | 13464 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13465 | `	ph7_value *pCallback,*pPrefix;` |
|        - | 13466 | `	/* Point to the target user defined callback */` |
|        5 | 13467 | `	pCallback = &pEngine->aCB[PH7_XML_NS_END];` |
|        - | 13468 | `	/* Make sure the given callback is callable */` |
|        5 | 13469 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13470 | `		/* Not callable,return immediately*/` |
|        3 | 13471 | `		return SXRET_OK;` |
|        - | 13472 | `	}` |
|        - | 13473 | `	/* Get a ph7_value holding the prefix */` |
|        3 | 13474 | `	pPrefix = VmXMLValue(pEngine,pPrefixStr,0);` |
|        3 | 13475 | `	if( pPrefix == 0 ){` |
|        - | 13476 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13477 | `		return SXRET_OK;` |
|        - | 13478 | `	}` |
|        - | 13479 | `	/* Invoke the user callback */` |
|        3 | 13480 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pPrefix,(ph7_value*)0);` |
|        - | 13481 | `	/* Clean-up the mess left behind */` |
|        3 | 13482 | `	ph7_context_release_value(pEngine->pCtx,pPrefix);` |
|        3 | 13483 | `	return SXRET_OK;` |
|        3 | 13484 |  |
|        - | 13485 | `/*` |
|        - | 13486 | ` * Error Message consumer handler.` |
|        - | 13487 | ` * Each time the XML parser encounter a syntaxt error or any other error` |
|        - | 13488 | ` * related to XML processing,the following callback is invoked by the` |
|        - | 13489 | ` * underlying XML parser.` |
|        - | 13490 | ` */` |
|       34 | 13491 | `static sxi32 VmXMLErrorHandler(const char *zMessage,sxi32 iErrCode,SyToken *pToken,void *pUserData)` |
|        1 | 13492 |  |
|       35 | 13493 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13494 | `	/* Save the error code */` |
|       35 | 13495 | `	pEngine->iErrCode = iErrCode;` |
|       17 | 13496 | `	SXUNUSED(zMessage); /* cc warning */` |
|       35 | 13497 | `	if( pToken ){` |
|       35 | 13498 | `		pEngine->nLine = pToken->nLine;` |
|       17 | 13499 | `	}` |
|        - | 13500 | `	/* Abort XML processing immediately */` |
|       35 | 13501 | `	return SXERR_ABORT;` |
|        1 | 13502 |  |
|        - | 13503 | `/*` |
|        - | 13504 | ` * int xml_parse(resource $parser,string $data[,bool $is_final = false ])` |
|        - | 13505 | ` *  Parses an XML document. The handlers for the configured events are called` |
|        - | 13506 | ` *  as many times as necessary.` |
|        - | 13507 | ` * Parameters` |
|        - | 13508 | ` *  $parser` |
|        - | 13509 | ` *   A reference to the XML parser.` |
|        - | 13510 | ` *  $data` |
|        - | 13511 | ` *   Chunk of data to parse. A document may be parsed piece-wise by calling` |
|        - | 13512 | ` *   xml_parse() several times with new data, as long as the is_final parameter` |
|        - | 13513 | ` *   is set and TRUE when the last data is parsed.` |
|        - | 13514 | ` * $is_final` |
|        - | 13515 | ` *   NOT USED. This implementation require that all the processed input be` |
|        - | 13516 | ` *   entirely loaded in memory.` |
|        - | 13517 | ` * Return` |
|        - | 13518 | ` *  Returns 1 on success or 0 on failure.` |
|        - | 13519 | ` */` |
|       74 | 13520 | `static int vm_builtin_xml_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13521 |  |
|        - | 13522 | `	ph7_xml_engine *pEngine;` |
|        - | 13523 | `	SyXMLParser *pParser;` |
|        - | 13524 | `	const char *zData;` |
|        - | 13525 | `	int nByte;` |
|       75 | 13526 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|        - | 13527 | `		/* Missing/Ivalid arguments,return FALSE */` |
|      ! 0 | 13528 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13529 | `		return PH7_OK;` |
|        - | 13530 | `	}` |
|        - | 13531 | `	/* Point to the XML engine */` |
|       75 | 13532 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       75 | 13533 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13534 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13535 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13536 | `		return PH7_OK;` |
|        - | 13537 | `	}` |
|       75 | 13538 | `	if( pEngine->iNest > 0 ){` |
|        - | 13539 | `		/* This can happen when the user callback call xml_parse() again` |
|        - | 13540 | `		 * in it's body which is forbidden.` |
|        - | 13541 | `		 */` |
|      ! 0 | 13542 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|        - | 13543 | `			"Recursive call to %s,PH7 is returning false",` |
|      ! 0 | 13544 | `			ph7_function_name(pCtx)` |
|        - | 13545 | `			);` |
|        - | 13546 | `		/* Return FALSE */` |
|      ! 0 | 13547 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13548 | `		return PH7_OK;` |
|        - | 13549 | `	}` |
|       75 | 13550 | `	pEngine->pCtx = pCtx;` |
|        - | 13551 | `	/* Point to the underlying XML parser */` |
|       75 | 13552 | `	pParser = &pEngine->sParser;` |
|        - | 13553 | `	/* Register elements handler */` |
|       75 | 13554 | `	SyXMLParserSetEventHandler(pParser,pEngine,` |
|        - | 13555 | `		VmXMLStartElementHandler,` |
|        - | 13556 | `		VmXMLTextHandler,` |
|        - | 13557 | `		VmXMLErrorHandler,` |
|        - | 13558 |  |
|        - | 13559 | `		VmXMLEndElementHandler,` |
|        - | 13560 | `		VmXMLPIHandler,` |
|        - | 13561 |  |
|        - | 13562 |  |
|        - | 13563 | `		VmXMLNSStartHandler,` |
|        - | 13564 | `		VmXMLNSEndHandler` |
|        - | 13565 | `		);` |
|       75 | 13566 | `	pEngine->iErrCode = SXML_ERROR_NONE;` |
|        - | 13567 | `	/* Extract the raw XML input */` |
|       75 | 13568 | `	zData = ph7_value_to_string(apArg[1],&nByte);` |
|        - | 13569 | `	/* Start the parse process */` |
|       75 | 13570 | `	pEngine->iNest++;` |
|       75 | 13571 | `	SyXMLProcess(pParser,zData,(sxu32)nByte);` |
|       75 | 13572 | `	pEngine->iNest--;` |
|        - | 13573 | `	/* Return the parse result */` |
|       75 | 13574 | `	ph7_result_int(pCtx,pEngine->iErrCode == SXML_ERROR_NONE ? 1 : 0);` |
|       75 | 13575 | `	return PH7_OK;` |
|       38 | 13576 |  |
|        - | 13577 | `/*` |
|        - | 13578 | ` * bool xml_parser_set_option(resource $parser,int $option,mixed $value)` |
|        - | 13579 | ` *  Sets an option in an XML parser.` |
|        - | 13580 | ` * Parameters` |
|        - | 13581 | ` *  $parser` |
|        - | 13582 | ` *   A reference to the XML parser to set an option in.` |
|        - | 13583 | ` *  $option` |
|        - | 13584 | ` *    Which option to set. See below.` |
|        - | 13585 | ` *   The following options are available:` |
|        - | 13586 | ` *   XML_OPTION_CASE_FOLDING 	integer  Controls whether case-folding is enabled for this XML parser.` |
|        - | 13587 | ` *   XML_OPTION_SKIP_TAGSTART 	integer  Specify how many characters should be skipped in the beginning of a tag name.` |
|        - | 13588 | ` *   XML_OPTION_SKIP_WHITE 	    integer  Whether to skip values consisting of whitespace characters.` |
|        - | 13589 | ` *   XML_OPTION_TARGET_ENCODING string 	 Sets which target encoding to use in this XML parser.` |
|        - | 13590 | ` * $value` |
|        - | 13591 | ` *   The option's new value.` |
|        - | 13592 | ` * Return` |
|        - | 13593 | ` *  Returns 1 on success or 0 on failure.` |
|        - | 13594 | ` * Note:` |
|        - | 13595 | ` *  Well,none of these options have meaning under the built-in XML parser so a call to this` |
|        - | 13596 | ` *  function is a no-op.` |
|        - | 13597 | ` */` |
|        6 | 13598 | `static int vm_builtin_xml_parser_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13599 |  |
|        - | 13600 | `	ph7_xml_engine *pEngine;` |
|        7 | 13601 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13602 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13603 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13604 | `		return PH7_OK;` |
|        - | 13605 | `	}` |
|        - | 13606 | `	/* Point to the XML engine */` |
|        7 | 13607 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        7 | 13608 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13609 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13610 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13611 | `		return PH7_OK;` |
|        - | 13612 | `	}` |
|        - | 13613 | `	/* Always return FALSE */` |
|        7 | 13614 | `	ph7_result_bool(pCtx,0);` |
|        7 | 13615 | `	return PH7_OK;` |
|        4 | 13616 |  |
|        - | 13617 | `/*` |
|        - | 13618 | ` * mixed xml_parser_get_option(resource $parser,int $option)` |
|        - | 13619 | ` *  Get options from an XML parser.` |
|        - | 13620 | ` * Parameters` |
|        - | 13621 | ` *  $parser` |
|        - | 13622 | ` *   A reference to the XML parser to set an option in.` |
|        - | 13623 | ` * $option` |
|        - | 13624 | ` *   Which option to fetch.` |
|        - | 13625 | ` * Return` |
|        - | 13626 | ` *  This function returns FALSE if parser does not refer to a valid parser` |
|        - | 13627 | ` *  or if option isn't valid.Else the option's value is returned.` |
|        - | 13628 | ` */` |
|        2 | 13629 | `static int vm_builtin_xml_parser_get_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13630 |  |
|        - | 13631 | `	ph7_xml_engine *pEngine;` |
|        - | 13632 | `	int nOp;` |
|        3 | 13633 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13634 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13635 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13636 | `		return PH7_OK;` |
|        - | 13637 | `	}` |
|        - | 13638 | `	/* Point to the XML engine */` |
|        3 | 13639 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 13640 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13641 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13642 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13643 | `		return PH7_OK;` |
|        - | 13644 | `	}` |
|        - | 13645 | `	/* Extract the option */` |
|        3 | 13646 | `	nOp = ph7_value_to_int(apArg[1]);` |
|        3 | 13647 | `	switch(nOp){` |
|      ! 0 | 13648 | `	case SXML_OPTION_SKIP_TAGSTART:` |
|        - | 13649 | `	case SXML_OPTION_SKIP_WHITE:` |
|        - | 13650 | `	case SXML_OPTION_CASE_FOLDING:` |
|      ! 0 | 13651 | `		ph7_result_int(pCtx,0); break;` |
|      ! 0 | 13652 | `	case SXML_OPTION_TARGET_ENCODING:` |
|      ! 0 | 13653 | `		ph7_result_string(pCtx,"UTF-8",(int)sizeof("UTF-8")-1);` |
|      ! 0 | 13654 | `		break;` |
|        1 | 13655 | `	default:` |
|        - | 13656 | `		/* Unknown option,return FALSE*/` |
|        3 | 13657 | `		ph7_result_bool(pCtx,0);` |
|        2 | 13658 | `		break;` |
|        - | 13659 | `	}` |
|        3 | 13660 | `	return PH7_OK;` |
|        2 | 13661 |  |
|        - | 13662 | `/*` |
|        - | 13663 | ` * string xml_error_string(int $code)` |
|        - | 13664 | ` *  Gets the XML parser error string associated with the given code.` |
|        - | 13665 | ` * Parameters` |
|        - | 13666 | ` *  $code` |
|        - | 13667 | ` *   An error code from xml_get_error_code().` |
|        - | 13668 | ` * Return` |
|        - | 13669 | ` *  Returns a string with a textual description of the error` |
|        - | 13670 | ` *  code, or FALSE if no description was found.` |
|        - | 13671 | ` */` |
|       30 | 13672 | `static int vm_builtin_xml_error_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13673 |  |
|       31 | 13674 | `	int nErr = -1;` |
|       31 | 13675 | `	if( nArg > 0 ){` |
|       31 | 13676 | `		nErr = ph7_value_to_int(apArg[0]);` |
|       15 | 13677 | `	}` |
|       31 | 13678 | `	switch(nErr){` |
|        1 | 13679 | `	case SXML_ERROR_DUPLICATE_ATTRIBUTE:` |
|        3 | 13680 | `		ph7_result_string(pCtx,"Duplicate attribute",-1/*Compute length automatically*/);` |
|        3 | 13681 | `		break;` |
|      ! 0 | 13682 | `	case SXML_ERROR_INCORRECT_ENCODING:` |
|      ! 0 | 13683 | `		ph7_result_string(pCtx,"Incorrect encoding",-1);` |
|      ! 0 | 13684 | `		break;` |
|      ! 0 | 13685 | `	case SXML_ERROR_INVALID_TOKEN:` |
|      ! 0 | 13686 | `		ph7_result_string(pCtx,"Unexpected token",-1);` |
|      ! 0 | 13687 | `		break;` |
|        3 | 13688 | `	case SXML_ERROR_MISPLACED_XML_PI:` |
|        7 | 13689 | `		ph7_result_string(pCtx,"Misplaced processing instruction",-1);` |
|        7 | 13690 | `		break;` |
|      ! 0 | 13691 | `	case SXML_ERROR_NO_MEMORY:` |
|      ! 0 | 13692 | `		ph7_result_string(pCtx,"Out of memory",-1);` |
|      ! 0 | 13693 | `		break;` |
|        1 | 13694 | `	case SXML_ERROR_NONE:` |
|        3 | 13695 | `		ph7_result_string(pCtx,"Not an error",-1);` |
|        3 | 13696 | `		break;` |
|        1 | 13697 | `	case SXML_ERROR_TAG_MISMATCH:` |
|        3 | 13698 | `		ph7_result_string(pCtx,"Tag mismatch",-1);` |
|        3 | 13699 | `		break;` |
|      ! 0 | 13700 | `	case -1:` |
|      ! 0 | 13701 | `		ph7_result_string(pCtx,"Unknown error code",-1);` |
|      ! 0 | 13702 | `		break;` |
|        9 | 13703 | `	default:` |
|       19 | 13704 | `		ph7_result_string(pCtx,"Syntax error",-1);` |
|       18 | 13705 | `		break;` |
|        - | 13706 | `	}` |
|       31 | 13707 | `	return PH7_OK;` |
|        1 | 13708 |  |
|        - | 13709 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13710 | `/*` |
|        - | 13711 | ` * int utf8_encode(string $input)` |
|        - | 13712 | ` *  UTF-8 encoding.` |
|        - | 13713 | ` *  This function encodes the string data to UTF-8, and returns the encoded version.` |
|        - | 13714 | ` *  UTF-8 is a standard mechanism used by Unicode for encoding wide character values` |
|        - | 13715 | ` * into a byte stream. UTF-8 is transparent to plain ASCII characters, is self-synchronized` |
|        - | 13716 | ` * (meaning it is possible for a program to figure out where in the bytestream characters start)` |
|        - | 13717 | ` * and can be used with normal string comparison functions for sorting and such.` |
|        - | 13718 | ` *  Notes on UTF-8 (According to SQLite3 authors):` |
|        - | 13719 | ` *  Byte-0    Byte-1    Byte-2    Byte-3    Value` |
|        - | 13720 | ` *  0xxxxxxx                                 00000000 00000000 0xxxxxxx` |
|        - | 13721 | ` *  110yyyyy  10xxxxxx                       00000000 00000yyy yyxxxxxx` |
|        - | 13722 | ` *  1110zzzz  10yyyyyy  10xxxxxx             00000000 zzzzyyyy yyxxxxxx` |
|        - | 13723 | ` *  11110uuu  10uuzzzz  10yyyyyy  10xxxxxx   000uuuuu zzzzyyyy yyxxxxxx` |
|        - | 13724 | ` * Parameters` |
|        - | 13725 | ` * $input` |
|        - | 13726 | ` *   String to encode or NULL on failure.` |
|        - | 13727 | ` * Return` |
|        - | 13728 | ` *  An UTF-8 encoded string.` |
|        - | 13729 | ` */` |
|        2 | 13730 | `static int vm_builtin_utf8_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13731 |  |
|        - | 13732 | `	const unsigned char *zIn,*zEnd;` |
|        - | 13733 | `	int nByte,c,e;` |
|        3 | 13734 | `	if( nArg < 1 ){` |
|        - | 13735 | `		/* Missing arguments,return null */` |
|      ! 0 | 13736 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13737 | `		return PH7_OK;` |
|        - | 13738 | `	}` |
|        - | 13739 | `	/* Extract the target string */` |
|        3 | 13740 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 13741 | `	if( nByte < 1 ){` |
|        - | 13742 | `		/* Empty string,return null */` |
|      ! 0 | 13743 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13744 | `		return PH7_OK;` |
|        - | 13745 | `	}` |
|        3 | 13746 | `	zEnd = &zIn[nByte];` |
|        - | 13747 | `	/* Start the encoding process */` |
|        2 | 13748 | `	for(;;){` |
|        5 | 13749 | `		if( zIn >= zEnd ){` |
|        - | 13750 | `			/* End of input */` |
|        3 | 13751 | `			break;` |
|        - | 13752 | `		}` |
|        3 | 13753 | `		c = zIn[0];` |
|        - | 13754 | `		/* Advance the stream cursor */` |
|        3 | 13755 | `		zIn++;` |
|        - | 13756 | `		/* Encode */` |
|        3 | 13757 | `		if( c<0x00080 ){` |
|      ! 0 | 13758 | `			e = (c&0xFF);` |
|      ! 0 | 13759 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        3 | 13760 | `		}else if( c<0x00800 ){` |
|        3 | 13761 | `			e = 0xC0 + ((c>>6)&0x1F);` |
|        3 | 13762 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        3 | 13763 | `			e = 0x80 + (c & 0x3F);` |
|        3 | 13764 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        1 | 13765 | `		}else if( c<0x10000 ){` |
|      ! 0 | 13766 | `			e = 0xE0 + ((c>>12)&0x0F);` |
|      ! 0 | 13767 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13768 | `			e = 0x80 + ((c>>6) & 0x3F);` |
|      ! 0 | 13769 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13770 | `			e = 0x80 + (c & 0x3F);` |
|      ! 0 | 13771 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13772 | `		}else{` |
|      ! 0 | 13773 | `			e = 0xF0 + ((c>>18) & 0x07);` |
|      ! 0 | 13774 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13775 | `			e = 0x80 + ((c>>12) & 0x3F);` |
|      ! 0 | 13776 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13777 | `			e = 0x80 + ((c>>6) & 0x3F);` |
|      ! 0 | 13778 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13779 | `			e = 0x80 + (c & 0x3F);` |
|      ! 0 | 13780 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        - | 13781 | `		}` |
|        1 | 13782 | `	}` |
|        - | 13783 | `	/* All done */` |
|        3 | 13784 | `	return PH7_OK;` |
|        2 | 13785 |  |
|        - | 13786 | `/*` |
|        - | 13787 | ` * UTF-8 decoding routine extracted from the sqlite3 source tree.` |
|        - | 13788 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|        - | 13789 | ` * Status: Public Domain` |
|        - | 13790 | ` */` |
|        - | 13791 | `/*` |
|        - | 13792 | `** This lookup table is used to help decode the first byte of` |
|        - | 13793 | `** a multi-byte UTF8 character.` |
|        - | 13794 | `*/` |
|        - | 13795 | `static const unsigned char UtfTrans1[] = {` |
|        - | 13796 |  |
|        - | 13797 |  |
|        - | 13798 |  |
|        - | 13799 |  |
|        - | 13800 |  |
|        - | 13801 |  |
|        - | 13802 |  |
|        - | 13803 |  |
|        - | 13804 | `};` |
|        - | 13805 | `/*` |
|        - | 13806 | `** Translate a single UTF-8 character.  Return the unicode value.` |
|        - | 13807 | `**` |
|        - | 13808 | `** During translation, assume that the byte that zTerm points` |
|        - | 13809 | `** is a 0x00.` |
|        - | 13810 | `**` |
|        - | 13811 | `** Write a pointer to the next unread byte back into *pzNext.` |
|        - | 13812 | `**` |
|        - | 13813 | `** Notes On Invalid UTF-8:` |
|        - | 13814 | `**` |
|        - | 13815 | `**  *  This routine never allows a 7-bit character (0x00 through 0x7f) to` |
|        - | 13816 | `**     be encoded as a multi-byte character.  Any multi-byte character that` |
|        - | 13817 | `**     attempts to encode a value between 0x00 and 0x7f is rendered as 0xfffd.` |
|        - | 13818 | `**` |
|        - | 13819 | `**  *  This routine never allows a UTF16 surrogate value to be encoded.` |
|        - | 13820 | `**     If a multi-byte character attempts to encode a value between` |
|        - | 13821 | `**     0xd800 and 0xe000 then it is rendered as 0xfffd.` |
|        - | 13822 | `**` |
|        - | 13823 | `**  *  Bytes in the range of 0x80 through 0xbf which occur as the first` |
|        - | 13824 | `**     byte of a character are interpreted as single-byte characters` |
|        - | 13825 | `**     and rendered as themselves even though they are technically` |
|        - | 13826 | `**     invalid characters.` |
|        - | 13827 | `**` |
|        - | 13828 | `**  *  This routine accepts an infinite number of different UTF8 encodings` |
|        - | 13829 | `**     for unicode values 0x80 and greater.  It do not change over-length` |
|        - | 13830 | `**     encodings to 0xfffd as some systems recommend.` |
|        - | 13831 | `*/` |
|        - | 13832 | `#define READ_UTF8(zIn, zTerm, c)                           \` |
|        - | 13833 | `  c = *(zIn++);                                            \` |
|        - | 13834 | `  if( c>=0xc0 ){                                           \` |
|        - | 13835 | `    c = UtfTrans1[c-0xc0];                                 \` |
|        - | 13836 | `    while( zIn!=zTerm && (*zIn & 0xc0)==0x80 ){            \` |
|        - | 13837 | `      c = (c<<6) + (0x3f & *(zIn++));                      \` |
|        - | 13838 | `    }                                                      \` |
|        - | 13839 | `    if( c<0x80                                             \` |
|        - | 13840 | `        \|\| (c&0xFFFFF800)==0xD800                          \` |
|        - | 13841 | `        \|\| (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }        \` |
|        - | 13842 | `  }` |
|      150 | 13843 | `PH7_PRIVATE int PH7_Utf8Read(` |
|        - | 13844 | `  const unsigned char *z,         /* First byte of UTF-8 character */` |
|        - | 13845 | `  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */` |
|        - | 13846 | `  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */` |
|        1 | 13847 | `){` |
|        - | 13848 | `  int c;` |
|      153 | 13849 | `  READ_UTF8(z, zTerm, c);` |
|      151 | 13850 | `  *pzNext = z;` |
|      151 | 13851 | `  return c;` |
|        1 | 13852 |  |
|        - | 13853 | `/*` |
|        - | 13854 | ` * string utf8_decode(string $data)` |
|        - | 13855 | ` *  This function decodes data, assumed to be UTF-8 encoded, to unicode.` |
|        - | 13856 | ` * Parameters` |
|        - | 13857 | ` * data` |
|        - | 13858 | ` *  An UTF-8 encoded string.` |
|        - | 13859 | ` * Return` |
|        - | 13860 | ` *  Unicode decoded string or NULL on failure.` |
|        - | 13861 | ` */` |
|        2 | 13862 | `static int vm_builtin_utf8_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13863 |  |
|        - | 13864 | `	const unsigned char *zIn,*zEnd;` |
|        - | 13865 | `	int nByte,c;` |
|        3 | 13866 | `	if( nArg < 1 ){` |
|        - | 13867 | `		/* Missing arguments,return null */` |
|      ! 0 | 13868 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13869 | `		return PH7_OK;` |
|        - | 13870 | `	}` |
|        - | 13871 | `	/* Extract the target string */` |
|        3 | 13872 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 13873 | `	if( nByte < 1 ){` |
|        - | 13874 | `		/* Empty string,return null */` |
|      ! 0 | 13875 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13876 | `		return PH7_OK;` |
|        - | 13877 | `	}` |
|        3 | 13878 | `	zEnd = &zIn[nByte];` |
|        - | 13879 | `	/* Start the decoding process */` |
|        5 | 13880 | `	while( zIn < zEnd ){` |
|        3 | 13881 | `		c = PH7_Utf8Read(zIn,zEnd,&zIn);` |
|        3 | 13882 | `		if( c == 0x0 ){` |
|      ! 0 | 13883 | `			break;` |
|        - | 13884 | `		}` |
|        3 | 13885 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|        1 | 13886 | `	}` |
|        3 | 13887 | `	return PH7_OK;` |
|        2 | 13888 |  |
|        - | 13889 | `/* Table of built-in VM functions. */` |
|        - | 13890 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 13891 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 13892 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 13893 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 13894 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 13895 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 13896 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 13897 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 13898 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 13899 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 13900 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 13901 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 13902 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 13903 | `	    /* Constants management */` |
|        - | 13904 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 13905 | `	{ "define",   vm_builtin_define               },` |
|        - | 13906 | `	{ "constant", vm_builtin_constant             },` |
|        - | 13907 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 13908 | `	   /* Class/Object functions */` |
|        - | 13909 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 13910 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 13911 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 13912 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 13913 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 13914 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 13915 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 13916 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 13917 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 13918 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 13919 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 13920 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 13921 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 13922 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 13923 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 13924 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 13925 | `	   /* Random numbers/strings generators */` |
|        - | 13926 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 13927 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 13928 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 13929 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 13930 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 13931 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13932 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 13933 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 13934 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 13935 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13936 | `	   /* Language constructs functions */` |
|        - | 13937 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 13938 | `	{ "print", vm_builtin_print                   },` |
|        - | 13939 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 13940 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 13941 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 13942 | `	  /* Variable handling functions */` |
|        - | 13943 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 13944 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 13945 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 13946 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 13947 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 13948 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 13949 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 13950 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 13951 | `	  /* Ouput control functions */` |
|        - | 13952 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 13953 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 13954 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 13955 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 13956 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 13957 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 13958 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 13959 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 13960 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 13961 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 13962 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 13963 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 13964 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 13965 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 13966 | `	  /* Assertion functions */` |
|        - | 13967 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 13968 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 13969 | `	  /* Error reporting functions */` |
|        - | 13970 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 13971 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 13972 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 13973 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 13974 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 13975 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 13976 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 13977 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 13978 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 13979 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 13980 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 13981 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 13982 | `	  /* Release info */` |
|        - | 13983 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 13984 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 13985 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 13986 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 13987 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 13988 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 13989 | `	  /* hashmap */` |
|        - | 13990 | `	{"compact",          vm_builtin_compact       },` |
|        - | 13991 | `	{"extract",          vm_builtin_extract       },` |
|        - | 13992 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 13993 | `	  /* URL related function */` |
|        - | 13994 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 13995 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 13996 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13997 | `	   /* XML processing functions */` |
|        - | 13998 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 13999 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 14000 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 14001 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 14002 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 14003 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 14004 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 14005 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 14006 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 14007 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 14008 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 14009 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 14010 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 14011 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 14012 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 14013 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 14014 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 14015 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 14016 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 14017 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 14018 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 14019 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 14020 | `	   /* UTF-8 encoding/decoding */` |
|        - | 14021 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 14022 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 14023 | `	   /* Command line processing */` |
|        - | 14024 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 14025 | `	   /* JSON encoding/decoding */` |
|        - | 14026 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 14027 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 14028 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 14029 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 14030 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 14031 | `	   /* Files/URI inclusion facility */` |
|        - | 14032 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 14033 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 14034 | `	{ "include",      vm_builtin_include          },` |
|        - | 14035 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 14036 | `	{ "require",      vm_builtin_require          },` |
|        - | 14037 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 14038 | `};` |
|        - | 14039 | `/*` |
|        - | 14040 | ` * Register the built-in VM functions defined above.` |
|        - | 14041 | ` */` |
|     1240 | 14042 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 14043 |  |
|        - | 14044 | `	sxi32 rc;` |
|        - | 14045 | `	sxu32 n;` |
|   155002 | 14046 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 14047 | `		/* Note that these special functions have access` |
|        - | 14048 | `		 * to the underlying virtual machine as their` |
|        - | 14049 | `		 * private data.` |
|        - | 14050 | `		 */` |
|   153762 | 14051 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   153762 | 14052 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 14053 | `			return rc;` |
|        - | 14054 | `		}` |
|    76882 | 14055 | `	}` |
|     1242 | 14056 | `	return SXRET_OK;` |
|      622 | 14057 |  |
|        - | 14058 | `/*` |
|        - | 14059 | ` * Check if the given name refer to an installed class.` |
|        - | 14060 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 14061 | ` */` |
|     8324 | 14062 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 14063 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 14064 | `	const char *zName,  /* Name of the target class */` |
|        - | 14065 | `	sxu32 nByte,        /* zName length */` |
|        - | 14066 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 14067 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 14068 | `						 */` |
|        - | 14069 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 14070 | `	)` |
|        2 | 14071 |  |
|        - | 14072 | `	SyHashEntry *pEntry;` |
|        - | 14073 | `	ph7_class *pClass;` |
|     4162 | 14074 | `		SXUNUSED(iNest);` |
|        - | 14075 | `	/* Perform a hash lookup */` |
|     8326 | 14076 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|        - | 14077 |  |
|     8326 | 14078 | `	if( pEntry == 0 ){` |
|        - | 14079 | `		/* No such entry,return NULL */` |
|      ! 0 | 14080 | `		return 0;` |
|        - | 14081 | `	}` |
|     8326 | 14082 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|     8326 | 14083 | `	if( !iLoadable ){` |
|        - | 14084 | `		/* Return the first class seen */` |
|     7654 | 14085 | `		return pClass;` |
|      ! 0 | 14086 | `	}else{` |
|        - | 14087 | `		/* Check the collision list */` |
|      674 | 14088 | `		while(pClass){` |
|      674 | 14089 | `			if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) == 0 ){` |
|        - | 14090 | `				/* Class is loadable */` |
|      674 | 14091 | `				return pClass;` |
|        - | 14092 | `			}` |
|        - | 14093 | `			/* Point to the next entry */` |
|      ! 0 | 14094 | `			pClass = pClass->pNextName;` |
|      ! 0 | 14095 | `		}` |
|        - | 14096 | `	}` |
|        - | 14097 | `	/* No such loadable class */` |
|      ! 0 | 14098 | `	return 0;` |
|     4164 | 14099 |  |
|        - | 14100 | `/*` |
|        - | 14101 | ` * Reference Table Implementation` |
|        - | 14102 | ` * Status: stable <chm@symisc.net>` |
|        - | 14103 | ` * Intro` |
|        - | 14104 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 14105 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 14106 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 14107 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 14108 | ` *  Refer to the official for more information on this powerful` |
|        - | 14109 | ` *  extension.` |
|        - | 14110 | ` */` |
|        - | 14111 | `/*` |
|        - | 14112 | ` * Allocate a new reference entry.` |
|        - | 14113 | ` */` |
|  2763230 | 14114 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 14115 |  |
|        - | 14116 | `	VmRefObj *pRef;` |
|        - | 14117 | `	/* Allocate a new instance */` |
|  2763232 | 14118 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  2763232 | 14119 | `	if( pRef == 0 ){` |
|      ! 0 | 14120 | `		return 0;` |
|        - | 14121 | `	}` |
|        - | 14122 | `	/* Zero the structure */` |
|  2763232 | 14123 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 14124 | `	/* Initialize fields */` |
|  2763232 | 14125 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  2763232 | 14126 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  2763232 | 14127 | `	pRef->nIdx = nIdx;` |
|  2763232 | 14128 | `	return pRef;` |
|  1381617 | 14129 |  |
|        - | 14130 | `/*` |
|        - | 14131 | ` * Default hash function used by the reference table` |
|        - | 14132 | ` * for lookup/insertion operations.` |
|        - | 14133 | ` */` |
| 15634887 | 14134 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 14135 |  |
|        - | 14136 | `	/* Calculate the hash based on the memory object index */` |
| 15634889 | 14137 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 14138 |  |
|        - | 14139 | `/*` |
|        - | 14140 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 14141 | ` * in the reference table.` |
|        - | 14142 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 14143 | ` * otherwise.` |
|        - | 14144 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14145 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14146 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14147 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14148 | ` * Refer to the official for more information on this powerful` |
|        - | 14149 | ` * extension.` |
|        - | 14150 | ` */` |
|  8262654 | 14151 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 14152 |  |
|        - | 14153 | `	VmRefObj *pRef;` |
|        - | 14154 | `	sxu32 nBucket;` |
|        - | 14155 | `	/* Point to the appropriate bucket */` |
|  8262656 | 14156 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 14157 | `	/* Perform the lookup */` |
|  8262656 | 14158 | `	pRef = pVm->apRefObj[nBucket];` |
| 16993848 | 14159 | `	for(;;){` |
| 33994239 | 14160 | `		if( pRef == 0 ){` |
|  2818790 | 14161 | `			break;` |
|        - | 14162 | `		}` |
| 31175451 | 14163 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 14164 | `			/* Entry found */` |
|  5443868 | 14165 | `			return pRef;` |
|        - | 14166 | `		}` |
|        - | 14167 | `		/* Point to the next entry */` |
| 25731585 | 14168 | `		pRef = pRef->pNextCollide;` |
|        2 | 14169 | `	}` |
|        - | 14170 | `	/* No such entry,return NULL */` |
|  2818790 | 14171 | `	return 0;` |
|  4131329 | 14172 |  |
|        - | 14173 | `/*` |
|        - | 14174 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14175 | ` *` |
|        - | 14176 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14177 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14178 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14179 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14180 | ` * Refer to the official for more information on this powerful` |
|        - | 14181 | ` * extension.` |
|        - | 14182 | ` */` |
|  2763230 | 14183 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14184 |  |
|        - | 14185 | `	sxu32 nBucket;` |
|  2763232 | 14186 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 14187 | `		VmRefObj **apNew;` |
|        - | 14188 | `		sxu32 nNew;` |
|        - | 14189 | `		/* Allocate a larger table */` |
|     1712 | 14190 | `		nNew = pVm->nRefSize << 1;` |
|     1712 | 14191 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     1712 | 14192 | `		if( apNew ){` |
|     1712 | 14193 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 14194 | `			sxu32 n;` |
|        - | 14195 | `			/* Zero the structure */` |
|     1712 | 14196 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 14197 | `			/* Rehash all referenced entries */` |
|  2813486 | 14198 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 14199 | `				/* Remove old collision links */` |
|  2811776 | 14200 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 14201 | `				/* Point to the appropriate bucket */` |
|  2811776 | 14202 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 14203 | `				/* Insert the entry  */` |
|  2811776 | 14204 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2811776 | 14205 | `				if( apNew[nBucket] ){` |
|  2298042 | 14206 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149020 | 14207 | `				}` |
|  2811776 | 14208 | `				apNew[nBucket] = pEntry;` |
|        - | 14209 | `				/* Point to the next entry */` |
|  2811776 | 14210 | `				pEntry = pEntry->pNext;` |
|  1405889 | 14211 | `			}` |
|        - | 14212 | `			/* Release the old table */` |
|     1712 | 14213 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 14214 | `			/* Install the new one */` |
|     1712 | 14215 | `			pVm->apRefObj = apNew;` |
|     1712 | 14216 | `			pVm->nRefSize = nNew;` |
|      855 | 14217 | `		}` |
|      855 | 14218 | `	}` |
|        - | 14219 | `	/* Point to the appropriate bucket */` |
|  2763232 | 14220 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 14221 | `	/* Insert the entry */` |
|  2763232 | 14222 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  2763232 | 14223 | `	if( pVm->apRefObj[nBucket] ){` |
|  2287222 | 14224 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1143565 | 14225 | `	}` |
|  2763232 | 14226 | `	pVm->apRefObj[nBucket] = pRef;` |
|  2763232 | 14227 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  2763232 | 14228 | `	pVm->nRefUsed++;` |
|  2763232 | 14229 | `	return SXRET_OK;` |
|        2 | 14230 |  |
|        - | 14231 | `/*` |
|        - | 14232 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 14233 | ` * the reference table.` |
|        - | 14234 | ` * This function is invoked when the user perform an unset` |
|        - | 14235 | ` * call [i.e: unset($var); ].` |
|        - | 14236 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14237 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14238 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14239 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14240 | ` * Refer to the official for more information on this powerful` |
|        - | 14241 | ` * extension.` |
|        - | 14242 | ` */` |
|  2744080 | 14243 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14244 |  |
|        - | 14245 | `	ph7_hashmap_node **apNode;` |
|        - | 14246 | `	SyHashEntry **apEntry;` |
|        - | 14247 | `	sxu32 n;` |
|        - | 14248 | `	/* Point to the reference table */` |
|  2744082 | 14249 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2744082 | 14250 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 14251 | `	/* Unlink the entry from the reference table */` |
|  2803990 | 14252 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    59910 | 14253 | `		if( apEntry[n] ){` |
|    59878 | 14254 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    29938 | 14255 | `		}` |
|    29956 | 14256 | `	}` |
|  5431508 | 14257 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2687428 | 14258 | `		if( apNode[n] ){` |
|     5039 | 14259 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2519 | 14260 | `		}` |
|  1343715 | 14261 | `	}` |
|  2744082 | 14262 | `	if( pRef->pPrevCollide ){` |
|   946853 | 14263 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   473375 | 14264 | `	}else{` |
|  1797231 | 14265 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 14266 | `	}` |
|  2744082 | 14267 | `	if( pRef->pNextCollide ){` |
|  1487189 | 14268 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   743536 | 14269 | `	}` |
|  2744082 | 14270 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 14271 | `	/* Release the node */` |
|  2744082 | 14272 | `	SySetRelease(&pRef->aReference);` |
|  2744082 | 14273 | `	SySetRelease(&pRef->aArrEntries);` |
|  2744082 | 14274 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2744082 | 14275 | `	pVm->nRefUsed--;` |
|  2744082 | 14276 | `	return SXRET_OK;` |
|        2 | 14277 |  |
|        - | 14278 | `/*` |
|        - | 14279 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14280 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14281 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14282 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14283 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14284 | ` * Refer to the official for more information on this powerful` |
|        - | 14285 | ` * extension.` |
|        - | 14286 | ` */` |
|  2780590 | 14287 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 14288 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14289 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14290 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14291 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 14292 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 14293 | `	)` |
|        2 | 14294 |  |
|  2780592 | 14295 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 14296 | `	VmRefObj *pRef;` |
|        - | 14297 | `	/* Check if the referenced object already exists */` |
|  2780592 | 14298 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2780592 | 14299 | `	if( pRef == 0 ){` |
|        - | 14300 | `		/* Create a new entry */` |
|  2763232 | 14301 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  2763232 | 14302 | `		if( pRef == 0 ){` |
|      ! 0 | 14303 | `			return SXERR_MEM;` |
|        - | 14304 | `		}` |
|  2763232 | 14305 | `		pRef->iFlags = iFlags;` |
|        - | 14306 | `		/* Install the entry */` |
|  2763232 | 14307 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1381615 | 14308 | `	}` |
|  2785504 | 14309 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 14310 | `		/* Safely ignore the exception frame */` |
|     4914 | 14311 | `		pFrame = pFrame->pParent;` |
|        2 | 14312 | `	}` |
|  2780592 | 14313 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 14314 | `		VmSlot sRef;` |
|        - | 14315 | `		/* Local frame,record referenced entry so that it can` |
|        - | 14316 | `		 * be deleted when we leave this frame.` |
|        - | 14317 | `		 */` |
|    55572 | 14318 | `		sRef.nIdx = nIdx;` |
|    55572 | 14319 | `		sRef.pUserData = pEntry;` |
|    55572 | 14320 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 14321 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 14322 | `		}` |
|    27785 | 14323 | `	}` |
|  2780592 | 14324 | `	if( pEntry ){` |
|        - | 14325 | `		/* Address of the hash-entry */` |
|    72766 | 14326 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    36382 | 14327 | `	}` |
|  2780592 | 14328 | `	if( pMapEntry ){` |
|        - | 14329 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2705032 | 14330 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1352515 | 14331 | `	}` |
|  2780592 | 14332 | `	return SXRET_OK;` |
|  1390297 | 14333 |  |
|        - | 14334 | `/*` |
|        - | 14335 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 14336 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14337 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14338 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14339 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14340 | ` * Refer to the official for more information on this powerful` |
|        - | 14341 | ` * extension.` |
|        - | 14342 | ` */` |
|  2737964 | 14343 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 14344 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14345 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14346 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14347 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 14348 | `	)` |
|        2 | 14349 |  |
|        - | 14350 | `	VmRefObj *pRef;` |
|        - | 14351 | `	sxu32 n;` |
|        - | 14352 | `	/* Check if the referenced object already exists */` |
|  2737966 | 14353 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2737966 | 14354 | `	if( pRef == 0 ){` |
|        - | 14355 | `		/* Not such entry */` |
|    55540 | 14356 | `		return SXERR_NOTFOUND;` |
|        - | 14357 | `	}` |
|        - | 14358 | `	/* Remove the desired entry */` |
|  2682428 | 14359 | `	if( pEntry ){` |
|        - | 14360 | `		SyHashEntry **apEntry;` |
|       33 | 14361 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      129 | 14362 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|       97 | 14363 | `			if( apEntry[n] == pEntry ){` |
|        - | 14364 | `				/* Nullify the entry */` |
|       33 | 14365 | `				apEntry[n] = 0;` |
|        - | 14366 | `				/*` |
|        - | 14367 | `				 * NOTE:` |
|        - | 14368 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 14369 | `				 * we avoid wasting spaces.` |
|        - | 14370 | `				 */` |
|       16 | 14371 | `			}` |
|       49 | 14372 | `		}` |
|       16 | 14373 | `	}` |
|  2682428 | 14374 | `	if( pMapEntry ){` |
|        - | 14375 | `		ph7_hashmap_node **apNode;` |
|  2682396 | 14376 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5364878 | 14377 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2682484 | 14378 | `			if( apNode[n] == pMapEntry ){` |
|        - | 14379 | `				/* nullify the entry */` |
|  2682396 | 14380 | `				apNode[n] = 0;` |
|  1341197 | 14381 | `			}` |
|  1341243 | 14382 | `		}` |
|  1341197 | 14383 | `	}` |
|  2682428 | 14384 | `	return SXRET_OK;` |
|  1368984 | 14385 |  |
|        - | 14386 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 14387 | `/*` |
|        - | 14388 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 14389 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 14390 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 14391 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 14392 | ` * For more information on how to register IO stream devices,please` |
|        - | 14393 | ` * refer to the official documentation.` |
|        - | 14394 | ` */` |
|    19228 | 14395 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 14396 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 14397 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 14398 | `	int nByte              /* *pzDevice length*/` |
|        - | 14399 | `	)` |
|        2 | 14400 |  |
|        - | 14401 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 14402 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 14403 | `	SyString sDev,sCur;` |
|        - | 14404 | `	sxu32 n,nEntry;` |
|        - | 14405 | `	int rc;` |
|        - | 14406 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    19230 | 14407 | `	zNext = zCur = zIn = *pzDevice;` |
|    19230 | 14408 | `	zEnd = &zIn[nByte];` |
|  1182692 | 14409 | `	while( zIn < zEnd ){` |
|  1163466 | 14410 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 14411 | `			/* Got one */` |
|        3 | 14412 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 14413 | `			break;` |
|        - | 14414 | `		}` |
|        - | 14415 | `		/* Advance the cursor */` |
|  1163464 | 14416 | `		zIn++;` |
|        2 | 14417 | `	}` |
|    19230 | 14418 | `	if( zIn >= zEnd ){` |
|        - | 14419 | `		/* No such scheme,return the default stream */` |
|    19228 | 14420 | `		return pVm->pDefStream;` |
|        - | 14421 | `	}` |
|        3 | 14422 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 14423 | `	/* Remove leading and trailing white spaces */` |
|        3 | 14424 | `	SyStringFullTrim(&sDev);` |
|        - | 14425 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 14426 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 14427 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 14428 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 14429 | `		pStream = apStream[n];` |
|        3 | 14430 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 14431 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 14432 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 14433 | `		if( rc == 0 ){` |
|        - | 14434 | `			/* Stream device found */` |
|        3 | 14435 | `			*pzDevice = zNext;` |
|        3 | 14436 | `			return pStream;` |
|        - | 14437 | `		}` |
|      ! 0 | 14438 | `	}` |
|        - | 14439 | `	/* No such stream,return NULL */` |
|      ! 0 | 14440 | `	return 0;` |
|     9616 | 14441 |  |
|        - | 14442 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 14443 | `/*` |
|        - | 14444 | ` * Section:` |
|        - | 14445 | ` *    HTTP/URI related routines.` |
|        - | 14446 | ` * Status:` |
|        - | 14447 | ` *    Stable.` |
|        - | 14448 | ` */` |
|        - | 14449 | ` /*` |
|        - | 14450 | `  * URI Parser: Split an URI into components [i.e: Host,Path,Query,...].` |
|        - | 14451 | `  * URI syntax: [method:/][/[user[:pwd]@]host[:port]/][document]` |
|        - | 14452 | `  * This almost, but not quite, RFC1738 URI syntax.` |
|        - | 14453 | `  * This routine is not a validator,it does not check for validity` |
|        - | 14454 | `  * nor decode URI parts,the only thing this routine does is splitting` |
|        - | 14455 | `  * the input to its fields.` |
|        - | 14456 | `  * Upper layer are responsible of decoding and validating URI parts.` |
|        - | 14457 | `  * On success,this function populate the "SyhttpUri" structure passed` |
|        - | 14458 | `  * as the first argument. Otherwise SXERR_* is returned when a malformed` |
|        - | 14459 | `  * input is encountered.` |
|        - | 14460 | `  */` |
|       26 | 14461 | ` static sxi32 VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen)` |
|        1 | 14462 | ` {` |
|       27 | 14463 | `	 const char *zEnd = &zUri[nLen];` |
|       27 | 14464 | `	 sxu8 bHostOnly = FALSE;` |
|       27 | 14465 | `	 sxu8 bIPv6 = FALSE	;` |
|        - | 14466 | `	 const char *zCur;` |
|        - | 14467 | `	 SyString *pComp;` |
|       27 | 14468 | `	 sxu32 nPos = 0;` |
|        - | 14469 | `	 sxi32 rc;` |
|        - | 14470 | `	 /* Zero the structure first */` |
|       27 | 14471 | `	 SyZero(pOut,sizeof(SyhttpUri));` |
|        - | 14472 | `	 /* Remove leading and trailing white spaces  */` |
|       27 | 14473 | `	 SyStringInitFromBuf(&pOut->sRaw,zUri,nLen);` |
|       27 | 14474 | `	 SyStringFullTrim(&pOut->sRaw);` |
|        - | 14475 | `	 /* Find the first '/' separator */` |
|       27 | 14476 | `	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);` |
|       27 | 14477 | `	 if( rc != SXRET_OK ){` |
|        - | 14478 | `		 /* Assume a host name only */` |
|        7 | 14479 | `		 zCur = zEnd;` |
|        7 | 14480 | `		 bHostOnly = TRUE;` |
|        7 | 14481 | `		 goto ProcessHost;` |
|        - | 14482 | `	 }` |
|       21 | 14483 | `	 zCur = &zUri[nPos];` |
|       21 | 14484 | `	 if( zUri != zCur && zCur[-1] == ':' ){` |
|        - | 14485 | `		 /* Extract a scheme:` |
|        - | 14486 | `		  * Not that we can get an invalid scheme here.` |
|        - | 14487 | `		  * Fortunately the caller can discard any URI by comparing this scheme with its` |
|        - | 14488 | `		  * registered schemes and will report the error as soon as his comparison function` |
|        - | 14489 | `		  * fail.` |
|        - | 14490 | `		  */` |
|       19 | 14491 | `	 	pComp = &pOut->sScheme;` |
|       19 | 14492 | `		SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri - 1));` |
|       19 | 14493 | `		SyStringLeftTrim(pComp);` |
|        9 | 14494 | `	 }` |
|       21 | 14495 | `	 if( zCur[1] != '/' ){` |
|      ! 0 | 14496 | `		 if( zCur == zUri \|\| zCur[-1] == ':' ){` |
|        - | 14497 | `		  /* No authority */` |
|      ! 0 | 14498 | `		  goto PathSplit;` |
|        - | 14499 | `		}` |
|        - | 14500 | `		 /* There is something here , we will assume its an authority` |
|        - | 14501 | `		  * and someone has forgot the two prefix slashes "//",` |
|        - | 14502 | `		  * sooner or later we will detect if we are dealing with a malicious` |
|        - | 14503 | `		  * user or not,but now assume we are dealing with an authority` |
|        - | 14504 | `		  * and let the caller handle all the validation process.` |
|        - | 14505 | `		  */` |
|      ! 0 | 14506 | `		 goto ProcessHost;` |
|        - | 14507 | `	 }` |
|       21 | 14508 | `	 zUri = &zCur[2];` |
|       21 | 14509 | `	 zCur = zEnd;` |
|       21 | 14510 | `	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);` |
|       29 | 14511 | `	 if( rc == SXRET_OK ){` |
|       17 | 14512 | `		 zCur = &zUri[nPos];` |
|        8 | 14513 | `	 }` |
|        2 | 14514 | ` ProcessHost:` |
|        - | 14515 | `	 /* Extract user information if present */` |
|       27 | 14516 | `	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),'@',&nPos);` |
|       27 | 14517 | `	 if( rc == SXRET_OK ){` |
|        7 | 14518 | `		 if( nPos > 0 ){` |
|        - | 14519 | `			 sxu32 nPassOfft; /* Password offset */` |
|        7 | 14520 | `			 pComp = &pOut->sUser;` |
|        7 | 14521 | `			 SyStringInitFromBuf(pComp,zUri,nPos);` |
|        - | 14522 | `			 /* Extract the password if available */` |
|        7 | 14523 | `			 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPassOfft);` |
|        7 | 14524 | `			 if( rc == SXRET_OK && nPassOfft < nPos){` |
|        7 | 14525 | `				 pComp->nByte = nPassOfft;` |
|        7 | 14526 | `				 pComp = &pOut->sPass;` |
|        7 | 14527 | `				 pComp->zString = &zUri[nPassOfft+sizeof(char)];` |
|        7 | 14528 | `				 pComp->nByte = nPos - nPassOfft - 1;` |
|        3 | 14529 | `			 }` |
|        - | 14530 | `			 /* Update the cursor */` |
|        7 | 14531 | `			 zUri = &zUri[nPos+1];` |
|        4 | 14532 | `		 }else{` |
|      ! 0 | 14533 | `			 zUri++;` |
|        - | 14534 | `		 }` |
|        3 | 14535 | `	 }` |
|       27 | 14536 | `	 pComp = &pOut->sHost;` |
|       27 | 14537 | `	 while( zUri < zCur && SyisSpace(zUri[0])){` |
|      ! 0 | 14538 | `		 zUri++;` |
|      ! 0 | 14539 | `	 }` |
|       27 | 14540 | `	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri));` |
|       27 | 14541 | `	 if( pComp->zString[0] == '[' ){` |
|        - | 14542 | `		 /* An IPv6 Address: Make a simple naive test` |
|        - | 14543 | `		  */` |
|        3 | 14544 | `		 zUri++; pComp->zString++; pComp->nByte = 0;` |
|        9 | 14545 | `		 while( ((unsigned char)zUri[0] < 0xc0 && SyisHex(zUri[0])) \|\| zUri[0] == ':' ){` |
|        7 | 14546 | `			 zUri++; pComp->nByte++;` |
|        1 | 14547 | `		 }` |
|        3 | 14548 | `		 if( zUri[0] != ']' ){` |
|      ! 0 | 14549 | `			 return SXERR_CORRUPT; /* Malformed IPv6 address */` |
|        - | 14550 | `		 }` |
|        3 | 14551 | `		 zUri++;` |
|        3 | 14552 | `		 bIPv6 = TRUE;` |
|        1 | 14553 | `	 }` |
|        - | 14554 | `	 /* Extract a port number if available */` |
|       27 | 14555 | `	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPos);` |
|       27 | 14556 | `	 if( rc == SXRET_OK ){` |
|       11 | 14557 | `		 if( bIPv6 == FALSE ){` |
|       11 | 14558 | `			 pComp->nByte = (sxu32)(&zUri[nPos] - zUri);` |
|        5 | 14559 | `		 }` |
|       11 | 14560 | `		 pComp = &pOut->sPort;` |
|       11 | 14561 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zCur - &zUri[nPos+1]));` |
|        5 | 14562 | `	 }` |
|       27 | 14563 | `	 if( bHostOnly == TRUE ){` |
|        7 | 14564 | `		 return SXRET_OK;` |
|        - | 14565 | `	 }` |
|       10 | 14566 | `PathSplit:` |
|       21 | 14567 | `	 zUri = zCur;` |
|       21 | 14568 | `	 pComp = &pOut->sPath;` |
|       21 | 14569 | `	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zEnd-zUri));` |
|       21 | 14570 | `	 if( pComp->nByte == 0 ){` |
|        5 | 14571 | `		 return SXRET_OK; /* Empty path */` |
|        - | 14572 | `	 }` |
|       17 | 14573 | `	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'?',&nPos) ){` |
|        5 | 14574 | `		 pComp->nByte = nPos; /* Update path length */` |
|        5 | 14575 | `		 pComp = &pOut->sQuery;` |
|        5 | 14576 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]));` |
|        2 | 14577 | `	 }` |
|       17 | 14578 | `	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'#',&nPos) ){` |
|        - | 14579 | `		 /* Update path or query length */` |
|        5 | 14580 | `		 if( pComp == &pOut->sPath ){` |
|      ! 0 | 14581 | `			 pComp->nByte = nPos;` |
|      ! 0 | 14582 | `		 }else{` |
|        5 | 14583 | `			 if( &zUri[nPos] < (char *)SyStringData(pComp) ){` |
|        - | 14584 | `				 /* Malformed syntax : Query must be present before fragment */` |
|      ! 0 | 14585 | `				 return SXERR_SYNTAX;` |
|        - | 14586 | `			 }` |
|        5 | 14587 | `			 pComp->nByte -= (sxu32)(zEnd - &zUri[nPos]);` |
|        - | 14588 | `		 }` |
|        5 | 14589 | `		 pComp = &pOut->sFragment;` |
|        5 | 14590 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]))` |
|        2 | 14591 | `	 }` |
|       17 | 14592 | `	 return SXRET_OK;` |
|       14 | 14593 | ` }` |
|        - | 14594 | ` /*` |
|        - | 14595 | ` * Extract a single line from a raw HTTP request.` |
|        - | 14596 | ` * Return SXRET_OK on success,SXERR_EOF when end of input` |
|        - | 14597 | ` * and SXERR_MORE when more input is needed.` |
|        - | 14598 | ` */` |
|      ! 0 | 14599 | `static sxi32 VmGetNextLine(SyString *pCursor,SyString *pCurrent)` |
|      ! 0 | 14600 |  |
|        - | 14601 | `  	const char *zIn;` |
|        - | 14602 | `  	sxu32 nPos;` |
|        - | 14603 | `	/* Jump leading white spaces */` |
|      ! 0 | 14604 | `	SyStringLeftTrim(pCursor);` |
|      ! 0 | 14605 | `	if( pCursor->nByte < 1 ){` |
|      ! 0 | 14606 | `		SyStringInitFromBuf(pCurrent,0,0);` |
|      ! 0 | 14607 | `		return SXERR_EOF; /* End of input */` |
|        - | 14608 | `	}` |
|      ! 0 | 14609 | `	zIn = SyStringData(pCursor);` |
|      ! 0 | 14610 | `	if( SXRET_OK != SyByteListFind(pCursor->zString,pCursor->nByte,"\r\n",&nPos) ){` |
|        - | 14611 | `		/* Line not found,tell the caller to read more input from source */` |
|      ! 0 | 14612 | `		SyStringDupPtr(pCurrent,pCursor);` |
|      ! 0 | 14613 | `		return SXERR_MORE;` |
|        - | 14614 | `	}` |
|      ! 0 | 14615 | `  	pCurrent->zString = zIn;` |
|      ! 0 | 14616 | `  	pCurrent->nByte	= nPos;` |
|        - | 14617 | `  	/* advance the cursor so we can call this routine again */` |
|      ! 0 | 14618 | `  	pCursor->zString = &zIn[nPos];` |
|      ! 0 | 14619 | `  	pCursor->nByte -= nPos;` |
|      ! 0 | 14620 | `  	return SXRET_OK;` |
|      ! 0 | 14621 | ` }` |
|        - | 14622 | ` /*` |
|        - | 14623 | `  * Split a single MIME header into a name value pair.` |
|        - | 14624 | `  * This function return SXRET_OK,SXERR_CONTINUE on success.` |
|        - | 14625 | `  * Otherwise SXERR_NEXT is returned when a malformed header` |
|        - | 14626 | `  * is encountered.` |
|        - | 14627 | `  * Note: This function handle also mult-line headers.` |
|        - | 14628 | `  */` |
|      ! 0 | 14629 | ` static sxi32 VmHttpProcessOneHeader(SyhttpHeader *pHdr,SyhttpHeader *pLast,const char *zLine,sxu32 nLen)` |
|      ! 0 | 14630 | ` {` |
|        - | 14631 | `	 SyString *pName;` |
|        - | 14632 | `	 sxu32 nPos;` |
|        - | 14633 | `	 sxi32 rc;` |
|      ! 0 | 14634 | `	 if( nLen < 1 ){` |
|      ! 0 | 14635 | `		 return SXERR_NEXT;` |
|        - | 14636 | `	 }` |
|        - | 14637 | `	 /* Check for multi-line header */` |
|      ! 0 | 14638 | `	if( pLast && (zLine[-1] == ' ' \|\| zLine[-1] == '\t') ){` |
|      ! 0 | 14639 | `		SyString *pTmp = &pLast->sValue;` |
|      ! 0 | 14640 | `		SyStringFullTrim(pTmp);` |
|      ! 0 | 14641 | `		if( pTmp->nByte == 0 ){` |
|      ! 0 | 14642 | `			SyStringInitFromBuf(pTmp,zLine,nLen);` |
|      ! 0 | 14643 | `		}else{` |
|        - | 14644 | `			/* Update header value length */` |
|      ! 0 | 14645 | `			pTmp->nByte = (sxu32)(&zLine[nLen] - pTmp->zString);` |
|        - | 14646 | `		}` |
|        - | 14647 | `		 /* Simply tell the caller to reset its states and get another line */` |
|      ! 0 | 14648 | `		 return SXERR_CONTINUE;` |
|        - | 14649 | `	 }` |
|        - | 14650 | `	/* Split the header */` |
|      ! 0 | 14651 | `	pName = &pHdr->sName;` |
|      ! 0 | 14652 | `	rc = SyByteFind(zLine,nLen,':',&nPos);` |
|      ! 0 | 14653 | `	if(rc != SXRET_OK ){` |
|      ! 0 | 14654 | `		return SXERR_NEXT; /* Malformed header;Check the next entry */` |
|        - | 14655 | `	}` |
|      ! 0 | 14656 | `	SyStringInitFromBuf(pName,zLine,nPos);` |
|      ! 0 | 14657 | `	SyStringFullTrim(pName);` |
|        - | 14658 | `	/* Extract a header value */` |
|      ! 0 | 14659 | `	SyStringInitFromBuf(&pHdr->sValue,&zLine[nPos + 1],nLen - nPos - 1);` |
|        - | 14660 | `	/* Remove leading and trailing whitespaces */` |
|      ! 0 | 14661 | `	SyStringFullTrim(&pHdr->sValue);` |
|      ! 0 | 14662 | `	return SXRET_OK;` |
|      ! 0 | 14663 | ` }` |
|        - | 14664 | ` /*` |
|        - | 14665 | `  * Extract all MIME headers associated with a HTTP request.` |
|        - | 14666 | `  * After processing the first line of a HTTP request,the following` |
|        - | 14667 | `  * routine is called in order to extract MIME headers.` |
|        - | 14668 | `  * This function return SXRET_OK on success,SXERR_MORE when it needs` |
|        - | 14669 | `  * more inputs.` |
|        - | 14670 | `  * Note: Any malformed header is simply discarded.` |
|        - | 14671 | `  */` |
|      ! 0 | 14672 | ` static sxi32 VmHttpExtractHeaders(SyString *pRequest,SySet *pOut)` |
|      ! 0 | 14673 | ` {` |
|      ! 0 | 14674 | `	 SyhttpHeader *pLast = 0;` |
|        - | 14675 | `	 SyString sCurrent;` |
|        - | 14676 | `	 SyhttpHeader sHdr;` |
|        - | 14677 | `	 sxu8 bEol;` |
|        - | 14678 | `	 sxi32 rc;` |
|      ! 0 | 14679 | `	 if( SySetUsed(pOut) > 0 ){` |
|      ! 0 | 14680 | `		 pLast = (SyhttpHeader *)SySetAt(pOut,SySetUsed(pOut)-1);` |
|      ! 0 | 14681 | `	 }` |
|      ! 0 | 14682 | `	 bEol = FALSE;` |
|      ! 0 | 14683 | `	 for(;;){` |
|      ! 0 | 14684 | `		 SyZero(&sHdr,sizeof(SyhttpHeader));` |
|        - | 14685 | `		 /* Extract a single line from the raw HTTP request */` |
|      ! 0 | 14686 | `		 rc = VmGetNextLine(pRequest,&sCurrent);` |
|      ! 0 | 14687 | `		 if(rc != SXRET_OK ){` |
|      ! 0 | 14688 | `			 if( sCurrent.nByte < 1 ){` |
|      ! 0 | 14689 | `				 break;` |
|        - | 14690 | `			 }` |
|      ! 0 | 14691 | `			 bEol = TRUE;` |
|      ! 0 | 14692 | `		 }` |
|        - | 14693 | `		 /* Process the header */` |
|      ! 0 | 14694 | `		 if( SXRET_OK == VmHttpProcessOneHeader(&sHdr,pLast,sCurrent.zString,sCurrent.nByte)){` |
|      ! 0 | 14695 | `			 if( SXRET_OK != SySetPut(pOut,(const void *)&sHdr) ){` |
|      ! 0 | 14696 | `				 break;` |
|        - | 14697 | `			 }` |
|        - | 14698 | `			 /* Retrieve the last parsed header so we can handle multi-line header` |
|        - | 14699 | `			  * in case we face one of them.` |
|        - | 14700 | `			  */` |
|      ! 0 | 14701 | `			 pLast = (SyhttpHeader *)SySetPeek(pOut);` |
|      ! 0 | 14702 | `		 }` |
|      ! 0 | 14703 | `		 if( bEol ){` |
|      ! 0 | 14704 | `			 break;` |
|        - | 14705 | `		 }` |
|      ! 0 | 14706 | `	 } /* for(;;) */` |
|      ! 0 | 14707 | `	 return SXRET_OK;` |
|      ! 0 | 14708 | ` }` |
|        - | 14709 | ` /*` |
|        - | 14710 | `  * Process the first line of a HTTP request.` |
|        - | 14711 | `  * This routine perform the following operations` |
|        - | 14712 | `  *  1) Extract the HTTP method.` |
|        - | 14713 | `  *  2) Split the request URI to it's fields [ie: host,path,query,...].` |
|        - | 14714 | `  *  3) Extract the HTTP protocol version.` |
|        - | 14715 | `  */` |
|      ! 0 | 14716 | ` static sxi32 VmHttpProcessFirstLine(` |
|        - | 14717 | `	 SyString *pRequest, /* Raw HTTP request */` |
|        - | 14718 | `	 sxi32 *pMethod,     /* OUT: HTTP method */` |
|        - | 14719 | `	 SyhttpUri *pUri,    /* OUT: Parse of the URI */` |
|        - | 14720 | `	 sxi32 *pProto       /* OUT: HTTP protocol */` |
|        - | 14721 | `	 )` |
|      ! 0 | 14722 | ` {` |
|        - | 14723 | `	 static const char *azMethods[] = { "get","post","head","put"};` |
|        - | 14724 | `	 static const sxi32 aMethods[]  = { HTTP_METHOD_GET,HTTP_METHOD_POST,HTTP_METHOD_HEAD,HTTP_METHOD_PUT};` |
|        - | 14725 | `	 const char *zIn,*zEnd,*zPtr;` |
|        - | 14726 | `	 SyString sLine;` |
|        - | 14727 | `	 sxu32 nLen;` |
|        - | 14728 | `	 sxi32 rc;` |
|        - | 14729 | `	 /* Extract the first line and update the pointer */` |
|      ! 0 | 14730 | `	 rc = VmGetNextLine(pRequest,&sLine);` |
|      ! 0 | 14731 | `	 if( rc != SXRET_OK ){` |
|      ! 0 | 14732 | `		 return rc;` |
|        - | 14733 | `	 }` |
|      ! 0 | 14734 | `	 if ( sLine.nByte < 1 ){` |
|        - | 14735 | `		 /* Empty HTTP request */` |
|      ! 0 | 14736 | `		 return SXERR_EMPTY;` |
|        - | 14737 | `	 }` |
|        - | 14738 | `	 /* Delimit the line and ignore trailing and leading white spaces */` |
|      ! 0 | 14739 | `	 zIn = sLine.zString;` |
|      ! 0 | 14740 | `	 zEnd = &zIn[sLine.nByte];` |
|      ! 0 | 14741 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14742 | `		 zIn++;` |
|      ! 0 | 14743 | `	 }` |
|        - | 14744 | `	 /* Extract the HTTP method */` |
|      ! 0 | 14745 | `	 zPtr = zIn;` |
|      ! 0 | 14746 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14747 | `		 zIn++;` |
|      ! 0 | 14748 | `	 }` |
|      ! 0 | 14749 | `	 *pMethod = HTTP_METHOD_OTHR;` |
|      ! 0 | 14750 | `	 if( zIn > zPtr ){` |
|        - | 14751 | `		 sxu32 i;` |
|      ! 0 | 14752 | `		 nLen = (sxu32)(zIn-zPtr);` |
|      ! 0 | 14753 | `		 for( i = 0 ; i < SX_ARRAYSIZE(azMethods) ; ++i ){` |
|      ! 0 | 14754 | `			 if( SyStrnicmp(azMethods[i],zPtr,nLen) == 0 ){` |
|      ! 0 | 14755 | `				 *pMethod = aMethods[i];` |
|      ! 0 | 14756 | `				 break;` |
|        - | 14757 | `			 }` |
|      ! 0 | 14758 | `		 }` |
|      ! 0 | 14759 | `	 }` |
|        - | 14760 | `	 /* Jump trailing white spaces */` |
|      ! 0 | 14761 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14762 | `		 zIn++;` |
|      ! 0 | 14763 | `	 }` |
|        - | 14764 | `	  /* Extract the request URI */` |
|      ! 0 | 14765 | `	 zPtr = zIn;` |
|      ! 0 | 14766 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14767 | `		 zIn++;` |
|      ! 0 | 14768 | `	 }` |
|      ! 0 | 14769 | `	 if( zIn > zPtr ){` |
|      ! 0 | 14770 | `		 nLen = (sxu32)(zIn-zPtr);` |
|        - | 14771 | `		 /* Split raw URI to it's fields */` |
|      ! 0 | 14772 | `		 VmHttpSplitURI(pUri,zPtr,nLen);` |
|      ! 0 | 14773 | `	 }` |
|        - | 14774 | `	 /* Jump trailing white spaces */` |
|      ! 0 | 14775 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14776 | `		 zIn++;` |
|      ! 0 | 14777 | `	 }` |
|        - | 14778 | `	 /* Extract the HTTP version */` |
|      ! 0 | 14779 | `	 zPtr = zIn;` |
|      ! 0 | 14780 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14781 | `		 zIn++;` |
|      ! 0 | 14782 | `	 }` |
|      ! 0 | 14783 | `	 *pProto = HTTP_PROTO_11; /* HTTP/1.1 */` |
|      ! 0 | 14784 | `	 rc = 1;` |
|      ! 0 | 14785 | `	 if( zIn > zPtr ){` |
|      ! 0 | 14786 | `		 rc = SyStrnicmp(zPtr,"http/1.0",(sxu32)(zIn-zPtr));` |
|      ! 0 | 14787 | `	 }` |
|      ! 0 | 14788 | `	 if( !rc ){` |
|      ! 0 | 14789 | `		 *pProto = HTTP_PROTO_10; /* HTTP/1.0 */` |
|      ! 0 | 14790 | `	 }` |
|      ! 0 | 14791 | `	 return SXRET_OK;` |
|      ! 0 | 14792 | ` }` |
|        - | 14793 | ` /*` |
|        - | 14794 | `  * Tokenize,decode and split a raw query encoded as: "x-www-form-urlencoded"` |
|        - | 14795 | `  * into a name value pair.` |
|        - | 14796 | `  * Note that this encoding is implicit in GET based requests.` |
|        - | 14797 | `  * After the tokenization process,register the decoded queries` |
|        - | 14798 | `  * in the $_GET/$_POST/$_REQUEST superglobals arrays.` |
|        - | 14799 | `  */` |
|      ! 0 | 14800 | ` static sxi32 VmHttpSplitEncodedQuery(` |
|        - | 14801 | `	 ph7_vm *pVm,       /* Target VM */` |
|        - | 14802 | `	 SyString *pQuery,  /* Raw query to decode */` |
|        - | 14803 | `	 SyBlob *pWorker,   /* Working buffer */` |
|        - | 14804 | `	 int is_post        /* TRUE if we are dealing with a POST request */` |
|        - | 14805 | `	 )` |
|      ! 0 | 14806 | ` {` |
|      ! 0 | 14807 | `	 const char *zEnd = &pQuery->zString[pQuery->nByte];` |
|      ! 0 | 14808 | `	 const char *zIn = pQuery->zString;` |
|        - | 14809 | `	 ph7_value *pGet,*pRequest;` |
|        - | 14810 | `	 SyString sName,sValue;` |
|        - | 14811 | `	 const char *zPtr;` |
|        - | 14812 | `	 sxu32 nBlobOfft;` |
|        - | 14813 | `	 /* Extract superglobals */` |
|      ! 0 | 14814 | `	 if( is_post ){` |
|        - | 14815 | `		 /* $_POST superglobal */` |
|      ! 0 | 14816 | `		 pGet = VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|      ! 0 | 14817 | `	 }else{` |
|        - | 14818 | `		 /* $_GET superglobal */` |
|      ! 0 | 14819 | `		 pGet = VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|        - | 14820 | `	 }` |
|      ! 0 | 14821 | `	 pRequest = VmExtractSuper(&(*pVm),"_REQUEST",sizeof("_REQUEST")-1);` |
|        - | 14822 | `	 /* Split up the raw query */` |
|      ! 0 | 14823 | `	 for(;;){` |
|        - | 14824 | `		 /* Jump leading white spaces */` |
|      ! 0 | 14825 | `		 while(zIn < zEnd  && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14826 | `			 zIn++;` |
|      ! 0 | 14827 | `		 }` |
|      ! 0 | 14828 | `		 if( zIn >= zEnd ){` |
|      ! 0 | 14829 | `			 break;` |
|        - | 14830 | `		 }` |
|      ! 0 | 14831 | `		 zPtr = zIn;` |
|      ! 0 | 14832 | `		 while( zPtr < zEnd && zPtr[0] != '=' && zPtr[0] != '&' && zPtr[0] != ';' ){` |
|      ! 0 | 14833 | `			 zPtr++;` |
|      ! 0 | 14834 | `		 }` |
|        - | 14835 | `		 /* Reset the working buffer */` |
|      ! 0 | 14836 | `		 SyBlobReset(pWorker);` |
|        - | 14837 | `		 /* Decode the entry */` |
|      ! 0 | 14838 | `		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|        - | 14839 | `		 /* Save the entry */` |
|      ! 0 | 14840 | `		 sName.nByte = SyBlobLength(pWorker);` |
|      ! 0 | 14841 | `		 sValue.zString = 0;` |
|      ! 0 | 14842 | `		 sValue.nByte = 0;` |
|      ! 0 | 14843 | `		 if( zPtr < zEnd && zPtr[0] == '=' ){` |
|      ! 0 | 14844 | `			 zPtr++;` |
|      ! 0 | 14845 | `			 zIn = zPtr;` |
|        - | 14846 | `			 /* Store field value */` |
|      ! 0 | 14847 | `			 while( zPtr < zEnd && zPtr[0] != '&' && zPtr[0] != ';' ){` |
|      ! 0 | 14848 | `				 zPtr++;` |
|      ! 0 | 14849 | `			 }` |
|      ! 0 | 14850 | `			 if( zPtr > zIn ){` |
|        - | 14851 | `				 /* Decode the value */` |
|      ! 0 | 14852 | `				  nBlobOfft = SyBlobLength(pWorker);` |
|      ! 0 | 14853 | `				  SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14854 | `				  sValue.zString = (const char *)SyBlobDataAt(pWorker,nBlobOfft);` |
|      ! 0 | 14855 | `				  sValue.nByte = SyBlobLength(pWorker) - nBlobOfft;` |
|        - | 14856 |  |
|      ! 0 | 14857 | `			 }` |
|        - | 14858 | `			 /* Synchronize pointers */` |
|      ! 0 | 14859 | `			 zIn = zPtr;` |
|      ! 0 | 14860 | `		 }` |
|      ! 0 | 14861 | `		 sName.zString = (const char *)SyBlobData(pWorker);` |
|        - | 14862 | `		 /* Install the decoded query in the $_GET/$_REQUEST array */` |
|      ! 0 | 14863 | `		 if( pGet && (pGet->iFlags & MEMOBJ_HASHMAP) ){` |
|      ! 0 | 14864 | `			 VmHashmapInsert((ph7_hashmap *)pGet->x.pOther,` |
|      ! 0 | 14865 | `				 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14866 | `				 sValue.zString,(int)sValue.nByte` |
|        - | 14867 | `				 );` |
|      ! 0 | 14868 | `		 }` |
|      ! 0 | 14869 | `		 if( pRequest && (pRequest->iFlags & MEMOBJ_HASHMAP) ){` |
|      ! 0 | 14870 | `			 VmHashmapInsert((ph7_hashmap *)pRequest->x.pOther,` |
|      ! 0 | 14871 | `				 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14872 | `				 sValue.zString,(int)sValue.nByte` |
|        - | 14873 | `					 );` |
|      ! 0 | 14874 | `		 }` |
|        - | 14875 | `		 /* Advance the pointer */` |
|      ! 0 | 14876 | `		 zIn = &zPtr[1];` |
|      ! 0 | 14877 | `	 }` |
|        - | 14878 | `	/* All done*/` |
|      ! 0 | 14879 | `	return SXRET_OK;` |
|      ! 0 | 14880 | ` }` |
|        - | 14881 | ` /*` |
|        - | 14882 | `  * Extract MIME header value from the given set.` |
|        - | 14883 | `  * Return header value on success. NULL otherwise.` |
|        - | 14884 | `  */` |
|      ! 0 | 14885 | ` static SyString * VmHttpExtractHeaderValue(SySet *pSet,const char *zMime,sxu32 nByte)` |
|      ! 0 | 14886 | ` {` |
|        - | 14887 | `	 SyhttpHeader *aMime,*pMime;` |
|        - | 14888 | `	 SyString sMime;` |
|        - | 14889 | `	 sxu32 n;` |
|      ! 0 | 14890 | `	 SyStringInitFromBuf(&sMime,zMime,nByte);` |
|        - | 14891 | `	 /* Point to the MIME entries */` |
|      ! 0 | 14892 | `	 aMime = (SyhttpHeader *)SySetBasePtr(pSet);` |
|        - | 14893 | `	 /* Perform the lookup */` |
|      ! 0 | 14894 | `	 for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|      ! 0 | 14895 | `		 pMime = &aMime[n];` |
|      ! 0 | 14896 | `		 if( SyStringCmp(&sMime,&pMime->sName,SyStrnicmp) == 0 ){` |
|        - | 14897 | `			 /* Header found,return it's associated value */` |
|      ! 0 | 14898 | `			 return &pMime->sValue;` |
|        - | 14899 | `		 }` |
|      ! 0 | 14900 | `	 }` |
|        - | 14901 | `	 /* No such MIME header */` |
|      ! 0 | 14902 | `	 return 0;` |
|      ! 0 | 14903 | ` }` |
|        - | 14904 | ` /*` |
|        - | 14905 | `  * Tokenize and decode a raw "Cookie:" MIME header into a name value pair` |
|        - | 14906 | `  * and insert it's fields [i.e name,value] in the $_COOKIE superglobal.` |
|        - | 14907 | `  */` |
|      ! 0 | 14908 | ` static sxi32 VmHttpPorcessCookie(ph7_vm *pVm,SyBlob *pWorker,const char *zIn,sxu32 nByte)` |
|      ! 0 | 14909 | ` {` |
|      ! 0 | 14910 | `	 const char *zPtr,*zDelimiter,*zEnd = &zIn[nByte];` |
|        - | 14911 | `	 SyString sName,sValue;` |
|        - | 14912 | `	 ph7_value *pCookie;` |
|        - | 14913 | `	 sxu32 nOfft;` |
|        - | 14914 | `	 /* Make sure the $_COOKIE superglobal is available */` |
|      ! 0 | 14915 | `	 pCookie = VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 14916 | `	 if( pCookie == 0 \|\| (pCookie->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 14917 | `		 /* $_COOKIE superglobal not available */` |
|      ! 0 | 14918 | `		 return SXERR_NOTFOUND;` |
|        - | 14919 | `	 }` |
|      ! 0 | 14920 | `	 for(;;){` |
|        - | 14921 | `		  /* Jump leading white spaces */` |
|      ! 0 | 14922 | `		 while( zIn < zEnd && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14923 | `			 zIn++;` |
|      ! 0 | 14924 | `		 }` |
|      ! 0 | 14925 | `		 if( zIn >= zEnd ){` |
|      ! 0 | 14926 | `			 break;` |
|        - | 14927 | `		 }` |
|        - | 14928 | `		  /* Reset the working buffer */` |
|      ! 0 | 14929 | `		 SyBlobReset(pWorker);` |
|      ! 0 | 14930 | `		 zDelimiter = zIn;` |
|        - | 14931 | `		 /* Delimit the name[=value]; pair */` |
|      ! 0 | 14932 | `		 while( zDelimiter < zEnd && zDelimiter[0] != ';' ){` |
|      ! 0 | 14933 | `			 zDelimiter++;` |
|      ! 0 | 14934 | `		 }` |
|      ! 0 | 14935 | `		 zPtr = zIn;` |
|      ! 0 | 14936 | `		 while( zPtr < zDelimiter && zPtr[0] != '=' ){` |
|      ! 0 | 14937 | `			 zPtr++;` |
|      ! 0 | 14938 | `		 }` |
|        - | 14939 | `		 /* Decode the cookie */` |
|      ! 0 | 14940 | `		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14941 | `		 sName.nByte = SyBlobLength(pWorker);` |
|      ! 0 | 14942 | `		 zPtr++;` |
|      ! 0 | 14943 | `		 sValue.zString = 0;` |
|      ! 0 | 14944 | `		 sValue.nByte = 0;` |
|      ! 0 | 14945 | `		 if( zPtr < zDelimiter ){` |
|        - | 14946 | `			 /* Got a Cookie value */` |
|      ! 0 | 14947 | `			 nOfft = SyBlobLength(pWorker);` |
|      ! 0 | 14948 | `			 SyUriDecode(zPtr,(sxu32)(zDelimiter-zPtr),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14949 | `			 SyStringInitFromBuf(&sValue,SyBlobDataAt(pWorker,nOfft),SyBlobLength(pWorker)-nOfft);` |
|      ! 0 | 14950 | `		 }` |
|        - | 14951 | `		 /* Synchronize pointers */` |
|      ! 0 | 14952 | `		 zIn = &zDelimiter[1];` |
|        - | 14953 | `		 /* Perform the insertion */` |
|      ! 0 | 14954 | `		 sName.zString = (const char *)SyBlobData(pWorker);` |
|      ! 0 | 14955 | `		 VmHashmapInsert((ph7_hashmap *)pCookie->x.pOther,` |
|      ! 0 | 14956 | `			 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14957 | `			 sValue.zString,(int)sValue.nByte` |
|        - | 14958 | `			 );` |
|      ! 0 | 14959 | `	 }` |
|      ! 0 | 14960 | `	 return SXRET_OK;` |
|      ! 0 | 14961 | ` }` |
|        - | 14962 | ` /*` |
|        - | 14963 | `  * Process a full HTTP request and populate the appropriate arrays` |
|        - | 14964 | `  * such as $_SERVER,$_GET,$_POST,$_COOKIE,$_REQUEST,... with the information` |
|        - | 14965 | `  * extracted from the raw HTTP request. As an extension Symisc introduced` |
|        - | 14966 | `  * the $_HEADER array which hold a copy of the processed HTTP MIME headers` |
|        - | 14967 | `  * and their associated values. [i.e: $_HEADER['Server'],$_HEADER['User-Agent'],...].` |
|        - | 14968 | `  * This function return SXRET_OK on success. Any other return value indicates` |
|        - | 14969 | `  * a malformed HTTP request.` |
|        - | 14970 | `  */` |
|      ! 0 | 14971 | ` static sxi32 VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte)` |
|      ! 0 | 14972 | ` {` |
|        - | 14973 | `	 SyString *pName,*pValue,sRequest; /* Raw HTTP request */` |
|        - | 14974 | `	 ph7_value *pHeaderArray;          /* $_HEADER superglobal (Symisc eXtension to the PHP specification)*/` |
|        - | 14975 | `	 SyhttpHeader *pHeader;            /* MIME header */` |
|        - | 14976 | `	 SyhttpUri sUri;     /* Parse of the raw URI*/` |
|        - | 14977 | `	 SyBlob sWorker;     /* General purpose working buffer */` |
|        - | 14978 | `	 SySet sHeader;      /* MIME headers set */` |
|        - | 14979 | `	 sxi32 iMethod;      /* HTTP method [i.e: GET,POST,HEAD...]*/` |
|        - | 14980 | `	 sxi32 iVer;         /* HTTP protocol version */` |
|        - | 14981 | `	 sxi32 rc;` |
|      ! 0 | 14982 | `	 SyStringInitFromBuf(&sRequest,zRequest,nByte);` |
|      ! 0 | 14983 | `	 SySetInit(&sHeader,&pVm->sAllocator,sizeof(SyhttpHeader));` |
|      ! 0 | 14984 | `	 SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        - | 14985 | `	 /* Ignore leading and trailing white spaces*/` |
|      ! 0 | 14986 | `	 SyStringFullTrim(&sRequest);` |
|        - | 14987 | `	 /* Process the first line */` |
|      ! 0 | 14988 | `	 rc = VmHttpProcessFirstLine(&sRequest,&iMethod,&sUri,&iVer);` |
|      ! 0 | 14989 | `	 if( rc != SXRET_OK ){` |
|      ! 0 | 14990 | `		 return rc;` |
|        - | 14991 | `	 }` |
|        - | 14992 | `	 /* Process MIME headers */` |
|      ! 0 | 14993 | `	 VmHttpExtractHeaders(&sRequest,&sHeader);` |
|        - | 14994 | `	 /*` |
|        - | 14995 | `	  * Setup $_SERVER environments` |
|        - | 14996 | `	  */` |
|        - | 14997 | `	 /* 'SERVER_PROTOCOL': Name and revision of the information protocol via which the page was requested */` |
|      ! 0 | 14998 | `	 ph7_vm_config(pVm,` |
|        - | 14999 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15000 | `		 "SERVER_PROTOCOL",` |
|      ! 0 | 15001 | `		 iVer == HTTP_PROTO_10 ? "HTTP/1.0" : "HTTP/1.1",` |
|        - | 15002 | `		 sizeof("HTTP/1.1")-1` |
|        - | 15003 | `		 );` |
|        - | 15004 | `	 /* 'REQUEST_METHOD':  Which request method was used to access the page */` |
|      ! 0 | 15005 | `	 ph7_vm_config(pVm,` |
|        - | 15006 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15007 | `		 "REQUEST_METHOD",` |
|      ! 0 | 15008 | `		 iMethod == HTTP_METHOD_GET ?   "GET" :` |
|      ! 0 | 15009 | `		 (iMethod == HTTP_METHOD_POST ? "POST":` |
|      ! 0 | 15010 | `		 (iMethod == HTTP_METHOD_PUT  ? "PUT" :` |
|      ! 0 | 15011 | `		 (iMethod == HTTP_METHOD_HEAD ?  "HEAD" : "OTHER"))),` |
|        - | 15012 | `		 -1 /* Compute attribute length automatically */` |
|        - | 15013 | `		 );` |
|      ! 0 | 15014 | `	 if( SyStringLength(&sUri.sQuery) > 0 && iMethod == HTTP_METHOD_GET ){` |
|      ! 0 | 15015 | `		 pValue = &sUri.sQuery;` |
|        - | 15016 | `		 /* 'QUERY_STRING': The query string, if any, via which the page was accessed */` |
|      ! 0 | 15017 | `		 ph7_vm_config(pVm,` |
|        - | 15018 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15019 | `			 "QUERY_STRING",` |
|      ! 0 | 15020 | `			 pValue->zString,` |
|      ! 0 | 15021 | `			 pValue->nByte` |
|        - | 15022 | `			 );` |
|        - | 15023 | `		 /* Decoded the raw query */` |
|      ! 0 | 15024 | `		 VmHttpSplitEncodedQuery(&(*pVm),pValue,&sWorker,FALSE);` |
|      ! 0 | 15025 | `	 }` |
|        - | 15026 | `	 /* REQUEST_URI: The URI which was given in order to access this page; for instance, '/index.html' */` |
|      ! 0 | 15027 | `	 pValue = &sUri.sRaw;` |
|      ! 0 | 15028 | `	 ph7_vm_config(pVm,` |
|        - | 15029 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15030 | `		 "REQUEST_URI",` |
|      ! 0 | 15031 | `		 pValue->zString,` |
|      ! 0 | 15032 | `		 pValue->nByte` |
|        - | 15033 | `		 );` |
|        - | 15034 | `	 /*` |
|        - | 15035 | `	  * 'PATH_INFO'` |
|        - | 15036 | `	  * 'ORIG_PATH_INFO'` |
|        - | 15037 | `      * Contains any client-provided pathname information trailing the actual script filename but preceding` |
|        - | 15038 | `	  * the query string, if available. For instance, if the current script was accessed via the URL` |
|        - | 15039 | `	  * http://www.example.com/php/path_info.php/some/stuff?foo=bar, then $_SERVER['PATH_INFO'] would contain` |
|        - | 15040 | `	  * /some/stuff.` |
|        - | 15041 | `	  */` |
|      ! 0 | 15042 | `	 pValue = &sUri.sPath;` |
|      ! 0 | 15043 | `	 ph7_vm_config(pVm,` |
|        - | 15044 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15045 | `		 "PATH_INFO",` |
|      ! 0 | 15046 | `		 pValue->zString,` |
|      ! 0 | 15047 | `		 pValue->nByte` |
|        - | 15048 | `		 );` |
|      ! 0 | 15049 | `	 ph7_vm_config(pVm,` |
|        - | 15050 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15051 | `		 "ORIG_PATH_INFO",` |
|      ! 0 | 15052 | `		 pValue->zString,` |
|      ! 0 | 15053 | `		 pValue->nByte` |
|        - | 15054 | `		 );` |
|        - | 15055 | `	 /* 'HTTP_ACCEPT': Contents of the Accept: header from the current request, if there is one */` |
|      ! 0 | 15056 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept",sizeof("Accept")-1);` |
|      ! 0 | 15057 | `	 if( pValue ){` |
|      ! 0 | 15058 | `		 ph7_vm_config(pVm,` |
|        - | 15059 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15060 | `			 "HTTP_ACCEPT",` |
|      ! 0 | 15061 | `			 pValue->zString,` |
|      ! 0 | 15062 | `			 pValue->nByte` |
|        - | 15063 | `		 );` |
|      ! 0 | 15064 | `	 }` |
|        - | 15065 | `	 /* 'HTTP_ACCEPT_CHARSET': Contents of the Accept-Charset: header from the current request, if there is one. */` |
|      ! 0 | 15066 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Charset",sizeof("Accept-Charset")-1);` |
|      ! 0 | 15067 | `	 if( pValue ){` |
|      ! 0 | 15068 | `		 ph7_vm_config(pVm,` |
|        - | 15069 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15070 | `			 "HTTP_ACCEPT_CHARSET",` |
|      ! 0 | 15071 | `			 pValue->zString,` |
|      ! 0 | 15072 | `			 pValue->nByte` |
|        - | 15073 | `		 );` |
|      ! 0 | 15074 | `	 }` |
|        - | 15075 | `	 /* 'HTTP_ACCEPT_ENCODING': Contents of the Accept-Encoding: header from the current request, if there is one. */` |
|      ! 0 | 15076 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Encoding",sizeof("Accept-Encoding")-1);` |
|      ! 0 | 15077 | `	 if( pValue ){` |
|      ! 0 | 15078 | `		 ph7_vm_config(pVm,` |
|        - | 15079 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15080 | `			 "HTTP_ACCEPT_ENCODING",` |
|      ! 0 | 15081 | `			 pValue->zString,` |
|      ! 0 | 15082 | `			 pValue->nByte` |
|        - | 15083 | `		 );` |
|      ! 0 | 15084 | `	 }` |
|        - | 15085 | `	  /* 'HTTP_ACCEPT_LANGUAGE': Contents of the Accept-Language: header from the current request, if there is one */` |
|      ! 0 | 15086 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Language",sizeof("Accept-Language")-1);` |
|      ! 0 | 15087 | `	 if( pValue ){` |
|      ! 0 | 15088 | `		 ph7_vm_config(pVm,` |
|        - | 15089 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15090 | `			 "HTTP_ACCEPT_LANGUAGE",` |
|      ! 0 | 15091 | `			 pValue->zString,` |
|      ! 0 | 15092 | `			 pValue->nByte` |
|        - | 15093 | `		 );` |
|      ! 0 | 15094 | `	 }` |
|        - | 15095 | `	 /* 'HTTP_CONNECTION': Contents of the Connection: header from the current request, if there is one. */` |
|      ! 0 | 15096 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Connection",sizeof("Connection")-1);` |
|      ! 0 | 15097 | `	 if( pValue ){` |
|      ! 0 | 15098 | `		 ph7_vm_config(pVm,` |
|        - | 15099 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15100 | `			 "HTTP_CONNECTION",` |
|      ! 0 | 15101 | `			 pValue->zString,` |
|      ! 0 | 15102 | `			 pValue->nByte` |
|        - | 15103 | `		 );` |
|      ! 0 | 15104 | `	 }` |
|        - | 15105 | `	 /* 'HTTP_HOST': Contents of the Host: header from the current request, if there is one. */` |
|      ! 0 | 15106 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Host",sizeof("Host")-1);` |
|      ! 0 | 15107 | `	 if( pValue ){` |
|      ! 0 | 15108 | `		 ph7_vm_config(pVm,` |
|        - | 15109 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15110 | `			 "HTTP_HOST",` |
|      ! 0 | 15111 | `			 pValue->zString,` |
|      ! 0 | 15112 | `			 pValue->nByte` |
|        - | 15113 | `		 );` |
|      ! 0 | 15114 | `	 }` |
|        - | 15115 | `	 /* 'HTTP_REFERER': Contents of the Referer: header from the current request, if there is one. */` |
|      ! 0 | 15116 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Referer",sizeof("Referer")-1);` |
|      ! 0 | 15117 | `	 if( pValue ){` |
|      ! 0 | 15118 | `		 ph7_vm_config(pVm,` |
|        - | 15119 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15120 | `			 "HTTP_REFERER",` |
|      ! 0 | 15121 | `			 pValue->zString,` |
|      ! 0 | 15122 | `			 pValue->nByte` |
|        - | 15123 | `		 );` |
|      ! 0 | 15124 | `	 }` |
|        - | 15125 | `	 /* 'HTTP_USER_AGENT': Contents of the Referer: header from the current request, if there is one. */` |
|      ! 0 | 15126 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"User-Agent",sizeof("User-Agent")-1);` |
|      ! 0 | 15127 | `	 if( pValue ){` |
|      ! 0 | 15128 | `		 ph7_vm_config(pVm,` |
|        - | 15129 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15130 | `			 "HTTP_USER_AGENT",` |
|      ! 0 | 15131 | `			 pValue->zString,` |
|      ! 0 | 15132 | `			 pValue->nByte` |
|        - | 15133 | `		 );` |
|      ! 0 | 15134 | `	 }` |
|        - | 15135 | `	  /* 'PHP_AUTH_DIGEST': When doing Digest HTTP authentication this variable is set to the 'Authorization'` |
|        - | 15136 | `	   * header sent by the client (which you should then use to make the appropriate validation).` |
|        - | 15137 | `	   */` |
|      ! 0 | 15138 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Authorization",sizeof("Authorization")-1);` |
|      ! 0 | 15139 | `	 if( pValue ){` |
|      ! 0 | 15140 | `		 ph7_vm_config(pVm,` |
|        - | 15141 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15142 | `			 "PHP_AUTH_DIGEST",` |
|      ! 0 | 15143 | `			 pValue->zString,` |
|      ! 0 | 15144 | `			 pValue->nByte` |
|        - | 15145 | `		 );` |
|      ! 0 | 15146 | `		 ph7_vm_config(pVm,` |
|        - | 15147 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15148 | `			 "PHP_AUTH",` |
|      ! 0 | 15149 | `			 pValue->zString,` |
|      ! 0 | 15150 | `			 pValue->nByte` |
|        - | 15151 | `		 );` |
|      ! 0 | 15152 | `	 }` |
|        - | 15153 | `	 /* Install all clients HTTP headers in the $_HEADER superglobal */` |
|      ! 0 | 15154 | `	 pHeaderArray = VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|        - | 15155 | `	 /* Iterate throw the available MIME headers*/` |
|      ! 0 | 15156 | `	 SySetResetCursor(&sHeader);` |
|      ! 0 | 15157 | `	 pHeader = 0; /* stupid cc warning */` |
|      ! 0 | 15158 | `	 while( SXRET_OK == SySetGetNextEntry(&sHeader,(void **)&pHeader) ){` |
|      ! 0 | 15159 | `		 pName  = &pHeader->sName;` |
|      ! 0 | 15160 | `		 pValue = &pHeader->sValue;` |
|      ! 0 | 15161 | `		 if( pHeaderArray && (pHeaderArray->iFlags & MEMOBJ_HASHMAP)){` |
|        - | 15162 | `			 /* Insert the MIME header and it's associated value */` |
|      ! 0 | 15163 | `			 VmHashmapInsert((ph7_hashmap *)pHeaderArray->x.pOther,` |
|      ! 0 | 15164 | `				 pName->zString,(int)pName->nByte,` |
|      ! 0 | 15165 | `				 pValue->zString,(int)pValue->nByte` |
|        - | 15166 | `				 );` |
|      ! 0 | 15167 | `		 }` |
|      ! 0 | 15168 | `		 if( pName->nByte == sizeof("Cookie")-1 && SyStrnicmp(pName->zString,"Cookie",sizeof("Cookie")-1) == 0` |
|      ! 0 | 15169 | `			 && pValue->nByte > 0){` |
|        - | 15170 | `				 /* Process the name=value pair and insert them in the $_COOKIE superglobal array */` |
|      ! 0 | 15171 | `				 VmHttpPorcessCookie(&(*pVm),&sWorker,pValue->zString,pValue->nByte);` |
|      ! 0 | 15172 | `		 }` |
|      ! 0 | 15173 | `	 }` |
|      ! 0 | 15174 | `	 if( iMethod == HTTP_METHOD_POST ){` |
|        - | 15175 | `		 /* Extract raw POST data */` |
|      ! 0 | 15176 | `		 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Type",sizeof("Content-Type") - 1);` |
|      ! 0 | 15177 | `		 if( pValue && pValue->nByte >= sizeof("application/x-www-form-urlencoded") - 1 &&` |
|      ! 0 | 15178 | `			 SyMemcmp("application/x-www-form-urlencoded",pValue->zString,pValue->nByte) == 0 ){` |
|        - | 15179 | `				 /* Extract POST data length */` |
|      ! 0 | 15180 | `				 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Length",sizeof("Content-Length") - 1);` |
|      ! 0 | 15181 | `				 if( pValue ){` |
|      ! 0 | 15182 | `					 sxi32 iLen = 0; /* POST data length */` |
|      ! 0 | 15183 | `					 SyStrToInt32(pValue->zString,pValue->nByte,(void *)&iLen,0);` |
|      ! 0 | 15184 | `					 if( iLen > 0 ){` |
|        - | 15185 | `						 /* Remove leading and trailing white spaces */` |
|      ! 0 | 15186 | `						 SyStringFullTrim(&sRequest);` |
|      ! 0 | 15187 | `						 if( (int)sRequest.nByte > iLen ){` |
|      ! 0 | 15188 | `							 sRequest.nByte = (sxu32)iLen;` |
|      ! 0 | 15189 | `						 }` |
|        - | 15190 | `						 /* Decode POST data now */` |
|      ! 0 | 15191 | `						 VmHttpSplitEncodedQuery(&(*pVm),&sRequest,&sWorker,TRUE);` |
|      ! 0 | 15192 | `					 }` |
|      ! 0 | 15193 | `				 }` |
|      ! 0 | 15194 | `		 }` |
|      ! 0 | 15195 | `	 }` |
|        - | 15196 | `	 /* All done,clean-up the mess left behind */` |
|      ! 0 | 15197 | `	 SySetRelease(&sHeader);` |
|      ! 0 | 15198 | `	 SyBlobRelease(&sWorker);` |
|      ! 0 | 15199 | `	 return SXRET_OK;` |
|      ! 0 | 15200 | ` }` |
|        - | 15201 |  |
