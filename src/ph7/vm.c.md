# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5203/7368 lines (70.62%)

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
|        - |   111 | `/*` |
|        - |   112 | ` * Each parsed URI is recorded and stored in an instance of the following structure.` |
|        - |   113 | ` * This structure and it's related routines are taken verbatim from the xHT project` |
|        - |   114 | ` * [A modern embeddable HTTP engine implementing all the RFC2616 methods]` |
|        - |   115 | ` * the xHT project is developed internally by Symisc Systems.` |
|        - |   116 | ` */` |
|        - |   117 | `typedef struct SyhttpUri SyhttpUri;` |
|        - |   118 | `struct SyhttpUri` |
|        - |   119 |  |
|        - |   120 | `	SyString sHost;     /* Hostname or IP address */` |
|        - |   121 | `	SyString sPort;     /* Port number */` |
|        - |   122 | `	SyString sPath;     /* Mandatory resource path passed verbatim (Not decoded) */` |
|        - |   123 | `	SyString sQuery;    /* Query part */` |
|        - |   124 | `	SyString sFragment; /* Fragment part */` |
|        - |   125 | `	SyString sScheme;   /* Scheme */` |
|        - |   126 | `	SyString sUser;     /* Username */` |
|        - |   127 | `	SyString sPass;     /* Password */` |
|        - |   128 | `	SyString sRaw;      /* Raw URI */` |
|        - |   129 | `};` |
|        - |   130 | `/*` |
|        - |   131 | ` * An instance of the following structure is used to record all MIME headers seen` |
|        - |   132 | ` * during a HTTP interaction.` |
|        - |   133 | ` * This structure and it's related routines are taken verbatim from the xHT project` |
|        - |   134 | ` * [A modern embeddable HTTP engine implementing all the RFC2616 methods]` |
|        - |   135 | ` * the xHT project is developed internally by Symisc Systems.` |
|        - |   136 | ` */` |
|        - |   137 | `typedef struct SyhttpHeader SyhttpHeader;` |
|        - |   138 | `struct SyhttpHeader` |
|        - |   139 |  |
|        - |   140 | `	SyString sName;    /* Header name [i.e:"Content-Type","Host","User-Agent"]. NOT NUL TERMINATED */` |
|        - |   141 | `	SyString sValue;   /* Header values [i.e: "text/html"]. NOT NUL TERMINATED */` |
|        - |   142 | `};` |
|        - |   143 | `/*` |
|        - |   144 | ` * Supported HTTP methods.` |
|        - |   145 | ` */` |
|        - |   146 | `#define HTTP_METHOD_GET  1 /* GET */` |
|        - |   147 | `#define HTTP_METHOD_HEAD 2 /* HEAD */` |
|        - |   148 | `#define HTTP_METHOD_POST 3 /* POST */` |
|        - |   149 | `#define HTTP_METHOD_PUT  4 /* PUT */` |
|        - |   150 | `#define HTTP_METHOD_OTHR 5 /* Other HTTP methods [i.e: DELETE,TRACE,OPTIONS...]*/` |
|        - |   151 | `/*` |
|        - |   152 | ` * Supported HTTP protocol version.` |
|        - |   153 | ` */` |
|        - |   154 | `#define HTTP_PROTO_10 1 /* HTTP/1.0 */` |
|        - |   155 | `#define HTTP_PROTO_11 2 /* HTTP/1.1 */` |
|        - |   156 | `/*` |
|        - |   157 | ` * Register a constant and it's associated expansion callback so that` |
|        - |   158 | ` * it can be expanded from the target PHP program.` |
|        - |   159 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|        - |   160 | ` * simple and work as follows:` |
|        - |   161 | ` * Each registered constant have a C procedure associated with it.` |
|        - |   162 | ` * This procedure known as the constant expansion callback is responsible` |
|        - |   163 | ` * of expanding the invoked constant to the desired value,for example:` |
|        - |   164 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|        - |   165 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|        - |   166 | ` * (Windows,Linux,...) and so on.` |
|        - |   167 | ` * Please refer to the official documentation for additional information.` |
|        - |   168 | ` */` |
|   188818 |   169 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |   170 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |   171 | `	const SyString *pName,  /* Constant name */` |
|        - |   172 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |   173 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |   174 | `	)` |
|        2 |   175 |  |
|        - |   176 | `	ph7_constant *pCons;` |
|        - |   177 | `	SyHashEntry *pEntry;` |
|        - |   178 | `	char *zDupName;` |
|        - |   179 | `	sxi32 rc;` |
|   188820 |   180 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   188820 |   181 | `	if( pEntry ){` |
|        - |   182 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   183 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   184 | `		pCons->xExpand = xExpand;` |
|        6 |   185 | `		pCons->pUserData = pUserData;` |
|        6 |   186 | `		return SXRET_OK;` |
|        - |   187 | `	}` |
|        - |   188 | `	/* Allocate a new constant instance */` |
|   188816 |   189 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   188816 |   190 | `	if( pCons == 0 ){` |
|      ! 0 |   191 | `		return 0;` |
|        - |   192 | `	}` |
|        - |   193 | `	/* Duplicate constant name */` |
|   188816 |   194 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   188816 |   195 | `	if( zDupName == 0 ){` |
|      ! 0 |   196 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   197 | `		return 0;` |
|        - |   198 | `	}` |
|        - |   199 | `	/* Install the constant */` |
|   188816 |   200 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   188816 |   201 | `	pCons->xExpand = xExpand;` |
|   188816 |   202 | `	pCons->pUserData = pUserData;` |
|   188816 |   203 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   188816 |   204 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   205 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   206 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   207 | `		return rc;` |
|        - |   208 | `	}` |
|        - |   209 | `	/* All done,constant can be invoked from PHP code */` |
|   188816 |   210 | `	return SXRET_OK;` |
|    94411 |   211 |  |
|        - |   212 | `/*` |
|        - |   213 | ` * Allocate a new foreign function instance.` |
|        - |   214 | ` * This function return SXRET_OK on success. Any other` |
|        - |   215 | ` * return value indicates failure.` |
|        - |   216 | ` * Please refer to the official documentation for an introduction to` |
|        - |   217 | ` * the foreign function mechanism.` |
|        - |   218 | ` */` |
|   410640 |   219 | `static sxi32 PH7_NewForeignFunction(` |
|        - |   220 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   221 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   222 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   223 | `	void *pUserData,          /* Foreign function private data */` |
|        - |   224 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|        - |   225 | `	)` |
|        2 |   226 |  |
|        - |   227 | `	ph7_user_func *pFunc;` |
|        - |   228 | `	char *zDup;` |
|        - |   229 | `	/* Allocate a new user function */` |
|   410642 |   230 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   410642 |   231 | `	if( pFunc == 0 ){` |
|      ! 0 |   232 | `		return SXERR_MEM;` |
|        - |   233 | `	}` |
|        - |   234 | `	/* Duplicate function name */` |
|   410642 |   235 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   410642 |   236 | `	if( zDup == 0 ){` |
|      ! 0 |   237 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   238 | `		return SXERR_MEM;` |
|        - |   239 | `	}` |
|        - |   240 | `	/* Zero the structure */` |
|   410642 |   241 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   242 | `	/* Initialize structure fields */` |
|   410642 |   243 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   410642 |   244 | `	pFunc->pVm   = pVm;` |
|   410642 |   245 | `	pFunc->xFunc = xFunc;` |
|   410642 |   246 | `	pFunc->pUserData = pUserData;` |
|   410642 |   247 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   248 | `	/* Write a pointer to the new function */` |
|   410642 |   249 | `	*ppOut = pFunc;` |
|   410642 |   250 | `	return SXRET_OK;` |
|   205322 |   251 |  |
|        - |   252 | `/*` |
|        - |   253 | ` * Install a foreign function and it's associated callback so that` |
|        - |   254 | ` * it can be invoked from the target PHP code.` |
|        - |   255 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   256 | ` * return value indicates failure.` |
|        - |   257 | ` * Please refer to the official documentation for an introduction to` |
|        - |   258 | ` * the foreign function mechanism.` |
|        - |   259 | ` */` |
|   411584 |   260 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|        - |   261 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   262 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   263 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   264 | `	void *pUserData           /* Foreign function private data */` |
|        - |   265 | `	)` |
|        2 |   266 |  |
|        - |   267 | `	ph7_user_func *pFunc;` |
|        - |   268 | `	SyHashEntry *pEntry;` |
|        - |   269 | `	sxi32 rc;` |
|        - |   270 | `	/* Overwrite any previously registered function with the same name */` |
|   411586 |   271 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   411586 |   272 | `	if( pEntry ){` |
|      946 |   273 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|      946 |   274 | `		pFunc->pUserData = pUserData;` |
|      946 |   275 | `		pFunc->xFunc = xFunc;` |
|      946 |   276 | `		SySetReset(&pFunc->aAux);` |
|      946 |   277 | `		return SXRET_OK;` |
|        - |   278 | `	}` |
|        - |   279 | `	/* Create a new user function */` |
|   410642 |   280 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   410642 |   281 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   282 | `		return rc;` |
|        - |   283 | `	}` |
|        - |   284 | `	/* Install the function in the corresponding hashtable */` |
|   410642 |   285 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   410642 |   286 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   287 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   288 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   289 | `		return rc;` |
|        - |   290 | `	}` |
|        - |   291 | `	/* User function successfully installed */` |
|   410642 |   292 | `	return SXRET_OK;` |
|   205794 |   293 |  |
|        - |   294 | `/*` |
|        - |   295 | ` * Initialize a VM function.` |
|        - |   296 | ` */` |
|    51176 |   297 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   298 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   299 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   300 | `	const char *zName,  /* Function name */` |
|        - |   301 | `	sxu32 nByte,        /* zName length */` |
|        - |   302 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   303 | `	void *pUserData     /* Function private data */` |
|        - |   304 | `	)` |
|        2 |   305 |  |
|        - |   306 | `	/* Zero the structure */` |
|    51178 |   307 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   308 | `	/* Initialize structure fields */` |
|        - |   309 | `	/* Arguments container */` |
|    51178 |   310 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   311 | `	/* Static variable container */` |
|    51178 |   312 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   313 | `	/* Bytecode container */` |
|    51178 |   314 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   315 | `    /* Preallocate some instruction slots */` |
|    51178 |   316 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   317 | `	/* Closure environment */` |
|    51178 |   318 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|    51178 |   319 | `	pFunc->iFlags = iFlags;` |
|    51178 |   320 | `	pFunc->pUserData = pUserData;` |
|    51178 |   321 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|    51178 |   322 | `	return SXRET_OK;` |
|        2 |   323 |  |
|        - |   324 | `/*` |
|        - |   325 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   326 | ` */` |
|   136570 |   327 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   328 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   329 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   330 | `	SyString *pName     /* Function name */` |
|        - |   331 | `	)` |
|        2 |   332 |  |
|        - |   333 | `	SyHashEntry *pEntry;` |
|        - |   334 | `	sxi32 rc;` |
|   136572 |   335 | `	if( pName == 0 ){` |
|        - |   336 | `		/* Use the built-in name */` |
|    16054 |   337 | `		pName = &pFunc->sName;` |
|     8026 |   338 | `	}` |
|        - |   339 | `	/* Check for duplicates (functions with the same name) first */` |
|   136572 |   340 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   136572 |   341 | `	if( pEntry ){` |
|    95236 |   342 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|    95236 |   343 | `		if( pLink != pFunc ){` |
|        - |   344 | `			/* Link */` |
|      185 |   345 | `			pFunc->pNextName = pLink;` |
|      185 |   346 | `			pEntry->pUserData = pFunc;` |
|       92 |   347 | `		}` |
|    95236 |   348 | `		return SXRET_OK;` |
|        - |   349 | `	}` |
|        - |   350 | `	/* First time seen */` |
|    41338 |   351 | `	pFunc->pNextName = 0;` |
|    41338 |   352 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    41338 |   353 | `	return rc;` |
|    68287 |   354 |  |
|        - |   355 | `/*` |
|        - |   356 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   357 | ` */` |
|    12292 |   358 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   359 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   360 | `	ph7_class *pClass /* Target Class */` |
|        - |   361 | `	)` |
|        2 |   362 |  |
|    12294 |   363 | `	SyString *pName = &pClass->sName;` |
|        - |   364 | `	SyHashEntry *pEntry;` |
|        - |   365 | `	sxi32 rc;` |
|        - |   366 | `	/* Check for duplicates */` |
|    12294 |   367 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    12294 |   368 | `	if( pEntry ){` |
|       63 |   369 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   370 | `		/* Link entry with the same name */` |
|       63 |   371 | `		pClass->pNextName = pLink;` |
|       63 |   372 | `		pEntry->pUserData = pClass;` |
|       63 |   373 | `		return SXRET_OK;` |
|        - |   374 | `	}` |
|    12232 |   375 | `	pClass->pNextName = 0;` |
|        - |   376 | `	/* Perform a simple hashtable insertion */` |
|    12232 |   377 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    12232 |   378 | `	return rc;` |
|     6148 |   379 |  |
|        - |   380 | `/*` |
|        - |   381 | ` * Instruction builder interface.` |
|        - |   382 | ` */` |
|  1294608 |   383 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   384 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   385 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   386 | `	sxi32 iP1,    /* First operand */` |
|        - |   387 | `	sxu32 iP2,    /* Second operand */` |
|        - |   388 | `	void *p3,     /* Third operand */` |
|        - |   389 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   390 | `	)` |
|        2 |   391 |  |
|        - |   392 | `	VmInstr sInstr;` |
|        - |   393 | `	sxi32 rc;` |
|        - |   394 | `	/* Fill the VM instruction */` |
|  1294610 |   395 | `	sInstr.iOp = (sxu8)iOp;` |
|  1294610 |   396 | `	sInstr.iP1 = iP1;` |
|  1294610 |   397 | `	sInstr.iP2 = iP2;` |
|  1294610 |   398 | `	sInstr.p3  = p3;` |
|  1294610 |   399 | `	if( pIndex ){` |
|        - |   400 | `		/* Instruction index in the bytecode array */` |
|    77858 |   401 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    38928 |   402 | `	}` |
|        - |   403 | `	/* Finally,record the instruction */` |
|  1294610 |   404 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  1294610 |   405 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   406 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   407 | `		/* Fall throw */` |
|      ! 0 |   408 | `	}` |
|  1294610 |   409 | `	return rc;` |
|        2 |   410 |  |
|        - |   411 | `/*` |
|        - |   412 | ` * Swap the current bytecode container with the given one.` |
|        - |   413 | ` */` |
|   124524 |   414 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   415 |  |
|   124526 |   416 | `	if( pContainer == 0 ){` |
|        - |   417 | `		/* Point to the default container */` |
|      ! 0 |   418 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   419 | `	}else{` |
|        - |   420 | `		/* Change container */` |
|   124526 |   421 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   422 | `	}` |
|   124526 |   423 | `	return SXRET_OK;` |
|        2 |   424 |  |
|        - |   425 | `/*` |
|        - |   426 | ` * Return the current bytecode container.` |
|        - |   427 | ` */` |
|    62262 |   428 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   429 |  |
|    62264 |   430 | `	return pVm->pByteContainer;` |
|        2 |   431 |  |
|        - |   432 | `/*` |
|        - |   433 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   434 | ` */` |
|    76580 |   435 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   436 |  |
|        - |   437 | `	VmInstr *pInstr;` |
|    76582 |   438 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|    76582 |   439 | `	return pInstr;` |
|        2 |   440 |  |
|        - |   441 | `/*` |
|        - |   442 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   443 | ` */` |
|   374218 |   444 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   445 |  |
|   374220 |   446 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   447 |  |
|        - |   448 | `/*` |
|        - |   449 | ` * Pop the last VM instruction.` |
|        - |   450 | ` */` |
|    73288 |   451 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   452 |  |
|    73290 |   453 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   454 |  |
|        - |   455 | `/*` |
|        - |   456 | ` * Peek the last VM instruction.` |
|        - |   457 | ` */` |
|   196052 |   458 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   459 |  |
|   196054 |   460 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   461 |  |
|     2628 |   462 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   463 |  |
|        - |   464 | `	VmInstr *aInstr;` |
|        - |   465 | `	sxu32 n;` |
|     2630 |   466 | `	n = SySetUsed(pVm->pByteContainer);` |
|     2630 |   467 | `	if( n < 2 ){` |
|      ! 0 |   468 | `		return 0;` |
|        - |   469 | `	}` |
|     2630 |   470 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|     2630 |   471 | `	return &aInstr[n - 2];` |
|     1316 |   472 |  |
|        - |   473 | `/*` |
|        - |   474 | ` * Allocate a new virtual machine frame.` |
|        - |   475 | ` */` |
|     8906 |   476 | `static VmFrame * VmNewFrame(` |
|        - |   477 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   478 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   479 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   480 | `	)` |
|        2 |   481 |  |
|        - |   482 | `	VmFrame *pFrame;` |
|        - |   483 | `	/* Allocate a new vm frame */` |
|     8908 |   484 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|     8908 |   485 | `	if( pFrame == 0 ){` |
|      ! 0 |   486 | `		return 0;` |
|        - |   487 | `	}` |
|        - |   488 | `	/* Zero the structure */` |
|     8908 |   489 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   490 | `	/* Initialize frame fields */` |
|     8908 |   491 | `	pFrame->pUserData = pUserData;` |
|     8908 |   492 | `	pFrame->pThis = pThis;` |
|     8908 |   493 | `	pFrame->pVm = pVm;` |
|     8908 |   494 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|     8908 |   495 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|     8908 |   496 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|     8908 |   497 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|     8908 |   498 | `	return pFrame;` |
|     4455 |   499 |  |
|        - |   500 | `/*` |
|        - |   501 | ` * Enter a VM frame.` |
|        - |   502 | ` */` |
|     8906 |   503 | `static sxi32 VmEnterFrame(` |
|        - |   504 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   505 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   506 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   507 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   508 | `	)` |
|        2 |   509 |  |
|        - |   510 | `	VmFrame *pFrame;` |
|        - |   511 | `	/* Allocate a new frame */` |
|     8908 |   512 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|     8908 |   513 | `	if( pFrame == 0 ){` |
|      ! 0 |   514 | `		return SXERR_MEM;` |
|        - |   515 | `	}` |
|        - |   516 | `	/* Link to the list of active VM frame */` |
|     8908 |   517 | `	pFrame->pParent = pVm->pFrame;` |
|     8908 |   518 | `	pVm->pFrame = pFrame;` |
|     8908 |   519 | `	if( ppFrame ){` |
|        - |   520 | `		/* Write a pointer to the new VM frame */` |
|     7704 |   521 | `		*ppFrame = pFrame;` |
|     3851 |   522 | `	}` |
|     8908 |   523 | `	return SXRET_OK;` |
|     4455 |   524 |  |
|        - |   525 | `/*` |
|        - |   526 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   527 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   528 | ` * information.` |
|        - |   529 | ` */` |
|       30 |   530 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        1 |   531 |  |
|        - |   532 | `	VmFrame *pTarget,*pFrame;` |
|       31 |   533 | `	SyHashEntry *pEntry = 0;` |
|        - |   534 | `	sxi32 rc;` |
|        - |   535 | `	/* Point to the upper frame */` |
|       31 |   536 | `	pFrame = pVm->pFrame;` |
|       31 |   537 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |   538 | `		/* Safely ignore the exception frame */` |
|      ! 0 |   539 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   540 | `	}` |
|       31 |   541 | `	pTarget = pFrame;` |
|       31 |   542 | `	pFrame = pTarget->pParent;` |
|       45 |   543 | `	while( pFrame ){` |
|       45 |   544 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   545 | `			/* Query the current frame */` |
|       31 |   546 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       31 |   547 | `			if( pEntry ){` |
|        - |   548 | `				/* Variable found */` |
|       31 |   549 | `				break;` |
|        - |   550 | `			}` |
|      ! 0 |   551 | `		}` |
|        - |   552 | `		/* Point to the upper frame */` |
|       15 |   553 | `		pFrame = pFrame->pParent;` |
|        1 |   554 | `	}` |
|       31 |   555 | `	if( pEntry == 0 ){` |
|        - |   556 | `		/* Inexistant variable */` |
|      ! 0 |   557 | `		return SXERR_NOTFOUND;` |
|        - |   558 | `	}` |
|        - |   559 | `	/* Link to the current frame */` |
|       31 |   560 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       31 |   561 | `	if( rc == SXRET_OK ){` |
|        - |   562 | `		sxu32 nIdx;` |
|       31 |   563 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       31 |   564 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       15 |   565 | `	}` |
|       31 |   566 | `	return rc;` |
|       16 |   567 |  |
|        - |   568 | `/*` |
|        - |   569 | ` * Leave the top-most active frame.` |
|        - |   570 | ` */` |
|     7700 |   571 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   572 |  |
|     7702 |   573 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|     7702 |   574 | `	if( pCurFrame ){` |
|        - |   575 | `		/* Unlink from the list of active VM frame */` |
|     7702 |   576 | `		pVm->pFrame = pCurFrame->pParent;` |
|     7702 |   577 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   578 | `			VmSlot  *aSlot;` |
|        - |   579 | `			sxu32 n;` |
|        - |   580 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|     7684 |   581 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    57258 |   582 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   583 | `				/* Unset the local variable */` |
|    49576 |   584 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    24789 |   585 | `			}` |
|        - |   586 | `			/* Remove local reference */` |
|     7684 |   587 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    57292 |   588 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    49610 |   589 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    24806 |   590 | `			}` |
|     3841 |   591 | `		}` |
|        - |   592 | `		/* Release internal containers */` |
|     7702 |   593 | `		SyHashRelease(&pCurFrame->hVar);` |
|     7702 |   594 | `		SySetRelease(&pCurFrame->sArg);` |
|     7702 |   595 | `		SySetRelease(&pCurFrame->sLocal);` |
|     7702 |   596 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   597 | `		/* Release the whole structure */` |
|     7702 |   598 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     3850 |   599 | `	}` |
|     7702 |   600 |  |
|        - |   601 | `/*` |
|        - |   602 | ` * Compare two functions signature and return the comparison result.` |
|        - |   603 | ` */` |
|      818 |   604 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   605 |  |
|      819 |   606 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      819 |   607 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      819 |   608 | `	const char *zSin = pSecond->zString;` |
|      819 |   609 | `	const char *zFin = pFirst->zString;` |
|      819 |   610 | `	const char *zPtr = zFin;` |
|      409 |   611 | `	for(;;){` |
|      819 |   612 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      410 |   613 | `			break;` |
|        - |   614 | `		}` |
|      ! 0 |   615 | `		if( zFin[0] != zSin[0] ){` |
|        - |   616 | `			/* mismatch */` |
|      ! 0 |   617 | `			break;` |
|        - |   618 | `		}` |
|      ! 0 |   619 | `		zFin++;` |
|      ! 0 |   620 | `		zSin++;` |
|      ! 0 |   621 | `	}` |
|      819 |   622 | `	return (int)(zFin-zPtr);` |
|        1 |   623 |  |
|        - |   624 | `/*` |
|        - |   625 | ` * Select the appropriate VM function for the current call context.` |
|        - |   626 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   627 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   628 | ` * Refer to the official documentation for more information.` |
|        - |   629 | ` */` |
|      128 |   630 | `static ph7_vm_func * VmOverload(` |
|        - |   631 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   632 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   633 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   634 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   635 | `	)` |
|        1 |   636 |  |
|        - |   637 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   638 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   639 | `	ph7_vm_func *pLink;` |
|        - |   640 | `	SyString sArgSig;` |
|        - |   641 | `	SyBlob sSig;` |
|        - |   642 |  |
|      129 |   643 | `	pLink = pList;` |
|      129 |   644 | `	i = 0;` |
|        - |   645 | `	/* Put functions expecting the same number of passed arguments */` |
|     1073 |   646 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1011 |   647 | `		if( pLink == 0 ){` |
|       67 |   648 | `			break;` |
|        - |   649 | `		}` |
|      945 |   650 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   651 | `			/* Candidate for overloading */` |
|      883 |   652 | `			apSet[i++] = pLink;` |
|      441 |   653 | `		}` |
|        - |   654 | `		/* Point to the next entry */` |
|      945 |   655 | `		pLink = pLink->pNextName;` |
|        1 |   656 | `	}` |
|      129 |   657 | `	if( i < 1 ){` |
|        - |   658 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   659 | `		return pList;` |
|        - |   660 | `	}` |
|      129 |   661 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   662 | `		/* Return the only candidate */` |
|       27 |   663 | `		return apSet[0];` |
|        - |   664 | `	}` |
|        - |   665 | `	/* Calculate function signature */` |
|      103 |   666 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      355 |   667 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      253 |   668 | `		int c = 'n'; /* null */` |
|      253 |   669 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   670 | `			/* Hashmap */` |
|       45 |   671 | `			c = 'h';` |
|      231 |   672 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   673 | `			/* bool */` |
|      ! 0 |   674 | `			c = 'b';` |
|      209 |   675 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   676 | `			/* int */` |
|        5 |   677 | `			c = 'i';` |
|      207 |   678 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   679 | `			/* String */` |
|      105 |   680 | `			c = 's';` |
|      153 |   681 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   682 | `			/* Float */` |
|      ! 0 |   683 | `			c = 'f';` |
|      101 |   684 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   685 | `			/* Class instance */` |
|      ! 0 |   686 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|      ! 0 |   687 | `			SyString *pName = &pClass->sName;` |
|      ! 0 |   688 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|      ! 0 |   689 | `			c = -1;` |
|      ! 0 |   690 | `		}` |
|      253 |   691 | `		if( c > 0 ){` |
|      253 |   692 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      126 |   693 | `		}` |
|      127 |   694 | `	}` |
|      103 |   695 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      103 |   696 | `	iTarget = 0;` |
|      103 |   697 | `	iMax = -1;` |
|        - |   698 | `	/* Select the appropriate function */` |
|      921 |   699 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   700 | `		/* Compare the two signatures */` |
|      819 |   701 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      819 |   702 | `		if( iCur > iMax ){` |
|      103 |   703 | `			iMax = iCur;` |
|      103 |   704 | `			iTarget = j;` |
|       51 |   705 | `		}` |
|      410 |   706 | `	}` |
|      103 |   707 | `	SyBlobRelease(&sSig);` |
|        - |   708 | `	/* Appropriate function for the current call context */` |
|      103 |   709 | `	return apSet[iTarget];` |
|       65 |   710 |  |
|        - |   711 | `/* Forward declaration */` |
|        - |   712 | `static sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult);` |
|        - |   713 | `static sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...);` |
|        - |   714 | `/*` |
|        - |   715 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   716 | ` * it can be instanciated from the executed PHP script.` |
|        - |   717 | ` */` |
|    50372 |   718 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   719 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   720 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   721 | `	)` |
|        2 |   722 |  |
|        - |   723 | `	ph7_class_method *pMeth;` |
|        - |   724 | `	ph7_class_attr *pAttr;` |
|        - |   725 | `	SyHashEntry *pEntry;` |
|        - |   726 | `	sxi32 rc;` |
|        - |   727 | `	/* Reset the loop cursor */` |
|    50374 |   728 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   729 | `	/* Process only static and constant attribute */` |
|   150070 |   730 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   731 | `		/* Extract the current attribute */` |
|    74512 |   732 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    74512 |   733 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   734 | `			ph7_value *pMemObj;` |
|        - |   735 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1290 |   736 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1290 |   737 | `			if( pMemObj == 0 ){` |
|      ! 0 |   738 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   739 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   740 | `					&pClass->sName,&pAttr->sName` |
|        - |   741 | `					);` |
|      ! 0 |   742 | `				return SXERR_MEM;` |
|        - |   743 | `			}` |
|     1290 |   744 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   745 | `				/* Initialize attribute default value (any complex expression) */` |
|     1290 |   746 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      644 |   747 | `			}` |
|        - |   748 | `			/* Record attribute index */` |
|     1290 |   749 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   750 | `			/* Install static attribute in the reference table */` |
|     1290 |   751 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      644 |   752 | `		}` |
|        2 |   753 | `	}` |
|        - |   754 | `	/* Install class methods */` |
|    50374 |   755 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |   756 | `		/* Do not mount interface methods since they are signatures only.` |
|        - |   757 | `		 */` |
|    35110 |   758 | `		return SXRET_OK;` |
|        - |   759 | `	}` |
|        - |   760 | `	/* Create constructor alias if not yet done */` |
|    15266 |   761 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   762 | `		/* User constructor with the same base class name */` |
|      200 |   763 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      200 |   764 | `		if( pEntry ){` |
|      ! 0 |   765 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   766 | `			/* Create the alias */` |
|      ! 0 |   767 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   768 | `		}` |
|       99 |   769 | `	}` |
|        - |   770 | `	/* Install the methods now */` |
|    15266 |   771 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   143422 |   772 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   120526 |   773 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   120526 |   774 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   120520 |   775 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   120520 |   776 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   777 | `				return rc;` |
|        - |   778 | `			}` |
|    60259 |   779 | `		}` |
|        2 |   780 | `	}` |
|        - |   781 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    15266 |   782 | `	pClass->bMounted = TRUE;` |
|    15266 |   783 | `	return SXRET_OK;` |
|    25188 |   784 |  |
|        - |   785 | `/*` |
|        - |   786 | ` * Allocate a private frame for attributes of the given` |
|        - |   787 | ` * class instance (Object in the PHP jargon).` |
|        - |   788 | ` */` |
|      560 |   789 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   790 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   791 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   792 | `	)` |
|        2 |   793 |  |
|      562 |   794 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   795 | `	ph7_class_attr *pAttr;` |
|        - |   796 | `	SyHashEntry *pEntry;` |
|        - |   797 | `	sxi32 rc;` |
|        - |   798 | `	/* Install class attribute in the private frame associated with this instance */` |
|      562 |   799 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     1250 |   800 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   801 | `		VmClassAttr *pVmAttr;` |
|        - |   802 | `		/* Extract the current attribute */` |
|      690 |   803 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      690 |   804 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|      690 |   805 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   806 | `			return SXERR_MEM;` |
|        - |   807 | `		}` |
|      690 |   808 | `		pVmAttr->pAttr = pAttr;` |
|      690 |   809 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   810 | `			ph7_value *pMemObj;` |
|        - |   811 | `			/* Reserve a memory object for this attribute */` |
|      684 |   812 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|      684 |   813 | `			if( pMemObj == 0 ){` |
|      ! 0 |   814 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   815 | `				return SXERR_MEM;` |
|        - |   816 | `			}` |
|      684 |   817 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|      684 |   818 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   819 | `				/* Initialize attribute default value (any complex expression) */` |
|      206 |   820 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      102 |   821 | `			}` |
|      684 |   822 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|      684 |   823 | `			if( rc != SXRET_OK ){` |
|        - |   824 | `				VmSlot sSlot;` |
|        - |   825 | `				/* Restore memory object */` |
|      ! 0 |   826 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   827 | `				sSlot.pUserData = 0;` |
|      ! 0 |   828 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   829 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   830 | `				return SXERR_MEM;` |
|        - |   831 | `			}` |
|        - |   832 | `			/* Install attribute in the reference table */` |
|      684 |   833 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      343 |   834 | `		}else{` |
|        - |   835 | `			/* Install static/constant attribute */` |
|        8 |   836 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   837 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   838 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   839 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   840 | `				return SXERR_MEM;` |
|        - |   841 | `			}` |
|        - |   842 | `		}` |
|        2 |   843 | `	}` |
|      562 |   844 | `	return SXRET_OK;` |
|      282 |   845 |  |
|        - |   846 | `/* Forward declaration */` |
|        - |   847 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   848 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   849 | `/*` |
|        - |   850 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   851 | ` */` |
|        - |   852 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   853 | `/*` |
|        - |   854 | ` * Reserve a constant memory object.` |
|        - |   855 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   856 | ` */` |
|   148198 |   857 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   858 |  |
|        - |   859 | `	ph7_value *pObj;` |
|        - |   860 | `	sxi32 rc;` |
|   148200 |   861 | `	if( pIndex ){` |
|        - |   862 | `		/* Object index in the object table */` |
|   144588 |   863 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|    72293 |   864 | `	}` |
|        - |   865 | `	/* Reserve a slot for the new object */` |
|   148200 |   866 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   148200 |   867 | `	if( rc != SXRET_OK ){` |
|        - |   868 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   869 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   870 | `		 */` |
|      ! 0 |   871 | `		return 0;` |
|        - |   872 | `	}` |
|   148200 |   873 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   148200 |   874 | `	return pObj;` |
|    74101 |   875 |  |
|        - |   876 | `/*` |
|        - |   877 | ` * Reserve a memory object.` |
|        - |   878 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   879 | ` */` |
|    75908 |   880 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   881 |  |
|        - |   882 | `	ph7_value *pObj;` |
|        - |   883 | `	sxi32 rc;` |
|    75910 |   884 | `	if( pIndex ){` |
|        - |   885 | `		/* Object index in the object table */` |
|    75910 |   886 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|    37954 |   887 | `	}` |
|        - |   888 | `	/* Reserve a slot for the new object */` |
|    75910 |   889 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|    75910 |   890 | `	if( rc != SXRET_OK ){` |
|        - |   891 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   892 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   893 | `		 */` |
|      ! 0 |   894 | `		return 0;` |
|        - |   895 | `	}` |
|    75910 |   896 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|    75910 |   897 | `	return pObj;` |
|    37956 |   898 |  |
|        - |   899 | `/* Forward declaration */` |
|        - |   900 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   901 | `/*` |
|        - |   902 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   903 | ` * directly as foreign functions.` |
|        - |   904 | ` */` |
|        - |   905 | `#define PH7_BUILTIN_LIB \` |
|        - |   906 | `	"class Exception { "\` |
|        - |   907 | `    "protected $message = 'Unknown exception';"\` |
|        - |   908 | `    "protected $code = 0;"\` |
|        - |   909 | `    "protected $file;"\` |
|        - |   910 | `    "protected $line;"\` |
|        - |   911 | `    "protected $trace;"\` |
|        - |   912 | `    "protected $previous;"\` |
|        - |   913 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   914 | `	"   if( isset($message) ){"\` |
|        - |   915 | `	"	  $this->message = $message;"\` |
|        - |   916 | `	"   }"\` |
|        - |   917 | `	"   $this->code = $code;"\` |
|        - |   918 | `	"   $this->file = __FILE__;"\` |
|        - |   919 | `	"   $this->line = __LINE__;"\` |
|        - |   920 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   921 | `	"   if( isset($previous) ){"\` |
|        - |   922 | `	"     $this->previous = $previous;"\` |
|        - |   923 | `	"   }"\` |
|        - |   924 | `	"}"\` |
|        - |   925 | `	"public function getMessage(){"\` |
|        - |   926 | `	"   return $this->message;"\` |
|        - |   927 | `	"}"\` |
|        - |   928 | `	" public function getCode(){"\` |
|        - |   929 | `	"  return $this->code;"\` |
|        - |   930 | `	"}"\` |
|        - |   931 | `	"public function getFile(){"\` |
|        - |   932 | `	"  return $this->file;"\` |
|        - |   933 | `	"}"\` |
|        - |   934 | `	"public function getLine(){"\` |
|        - |   935 | `	"  return $this->line;"\` |
|        - |   936 | `	"}"\` |
|        - |   937 | `	"public function getTrace(){"\` |
|        - |   938 | `	"   return $this->trace;"\` |
|        - |   939 | `	"}"\` |
|        - |   940 | `	"public function getTraceAsString(){"\` |
|        - |   941 | `	"  return debug_string_backtrace();"\` |
|        - |   942 | `	"}"\` |
|        - |   943 | `	"public function getPrevious(){"\` |
|        - |   944 | `	"    return $this->previous;"\` |
|        - |   945 | `	"}"\` |
|        - |   946 | `	"public function __toString(){"\` |
|        - |   947 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   948 | `    "}"\` |
|        - |   949 | `	"}"\` |
|        - |   950 | `	"class Error extends Exception { }"\` |
|        - |   951 | `	"class TypeError extends Error { }"\` |
|        - |   952 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   953 | `	"class ErrorException extends Exception { "\` |
|        - |   954 | `	"protected $severity;"\` |
|        - |   955 | `	"public function __construct(string $message = null,"\` |
|        - |   956 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   957 | `	"   if( isset($message) ){"\` |
|        - |   958 | `	"	  $this->message = $message;"\` |
|        - |   959 | `	"   }"\` |
|        - |   960 | `	"   $this->severity = $severity;"\` |
|        - |   961 | `	"   $this->code = $code;"\` |
|        - |   962 | `	"   $this->file = $filename;"\` |
|        - |   963 | `	"   $this->line = $lineno;"\` |
|        - |   964 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   965 | `	"   if( isset($previous) ){"\` |
|        - |   966 | `	"     $this->previous = $previous;"\` |
|        - |   967 | `	"   }"\` |
|        - |   968 | `	"}"\` |
|        - |   969 | `	"public function getSeverity(){"\` |
|        - |   970 | `	"   return $this->severity;"\` |
|        - |   971 | `    "}"\` |
|        - |   972 | `	"}"\` |
|        - |   973 | `	"interface Iterator {"\` |
|        - |   974 | `	"public function current();"\` |
|        - |   975 | `	"public function key();"\` |
|        - |   976 | `	"public function next();"\` |
|        - |   977 | `	"public function rewind();"\` |
|        - |   978 | `	"public function valid();"\` |
|        - |   979 | `	"}"\` |
|        - |   980 | `	"interface IteratorAggregate {"\` |
|        - |   981 | `	"public function getIterator();"\` |
|        - |   982 | `	"}"\` |
|        - |   983 | `	"interface Serializable {"\` |
|        - |   984 | `	"public function serialize();"\` |
|        - |   985 | `	"public function unserialize(string $serialized);"\` |
|        - |   986 | `	"}"\` |
|        - |   987 | `	"/* Directory releated IO */"\` |
|        - |   988 | `	"class Directory {"\` |
|        - |   989 | `	"public $handle = null;"\` |
|        - |   990 | `	"public $path  = null;"\` |
|        - |   991 | `	"public function __construct(string $path)"\` |
|        - |   992 | `	"{"\` |
|        - |   993 | `	"   $this->handle = opendir($path);"\` |
|        - |   994 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |   995 | `	"      $this->path = $path;"\` |
|        - |   996 | `	"   }"\` |
|        - |   997 | `	"}"\` |
|        - |   998 | `	"public function __destruct()"\` |
|        - |   999 | `	"{"\` |
|        - |  1000 | `	"  if( $this->handle != null ){"\` |
|        - |  1001 | `	"       closedir($this->handle);"\` |
|        - |  1002 | `	"  }"\` |
|        - |  1003 | `	"}"\` |
|        - |  1004 | `	"public function read()"\` |
|        - |  1005 | `	"{"\` |
|        - |  1006 | `	"    return readdir($this->handle);"\` |
|        - |  1007 | `	"}"\` |
|        - |  1008 | `	"public function rewind()"\` |
|        - |  1009 | `	"{"\` |
|        - |  1010 | `	"    rewinddir($this->handle);"\` |
|        - |  1011 | `	"}"\` |
|        - |  1012 | `	"public function close()"\` |
|        - |  1013 | `	"{"\` |
|        - |  1014 | `	"    closedir($this->handle);"\` |
|        - |  1015 | `	"    $this->handle = null;"\` |
|        - |  1016 | `	"}"\` |
|        - |  1017 | `	"}"\` |
|        - |  1018 | `	"class stdClass{"\` |
|        - |  1019 | `	"  public $value;"\` |
|        - |  1020 | `	" /* Magic methods */"\` |
|        - |  1021 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1022 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1023 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1024 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1025 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1026 | `	"}"\` |
|        - |  1027 | `	"function dir(string $path){"\` |
|        - |  1028 | `	"   return new Directory($path);"\` |
|        - |  1029 | `	"}"\` |
|        - |  1030 | `	"function Dir(string $path){"\` |
|        - |  1031 | `	"   return new Directory($path);"\` |
|        - |  1032 | `	"}"\` |
|        - |  1033 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1034 | `    "{"\` |
|        - |  1035 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1036 | `	"  $aDir = array();"\` |
|        - |  1037 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1038 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1039 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1040 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1041 | `	"   }"\` |
|        - |  1042 | `	"  closedir($pHandle);"\` |
|        - |  1043 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1044 | `	"      rsort($aDir);"\` |
|        - |  1045 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1046 | `	"      sort($aDir);"\` |
|        - |  1047 | `	"  }"\` |
|        - |  1048 | `	"  return $aDir;"\` |
|        - |  1049 | `	"}"\` |
|        - |  1050 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1051 | `	"/* Open the target directory */"\` |
|        - |  1052 | `	"$zDir = dirname($pattern);"\` |
|        - |  1053 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1054 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1055 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1056 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1057 | `	"	return FALSE;"\` |
|        - |  1058 | `	"}"\` |
|        - |  1059 | `	"$pattern = basename($pattern);"\` |
|        - |  1060 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1061 | `	"/* Loop throw available entries */"\` |
|        - |  1062 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1063 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1064 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1065 | `	"	if( $rc ){"\` |
|        - |  1066 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1067 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1068 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1069 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1070 | `	"		  }"\` |
|        - |  1071 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1072 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1073 | `	"		 continue;"\` |
|        - |  1074 | `	"	   }"\` |
|        - |  1075 | `	"	   /* Add the entry */"\` |
|        - |  1076 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1077 | `	"	}"\` |
|        - |  1078 | `	" }"\` |
|        - |  1079 | `	"/* Close the handle */"\` |
|        - |  1080 | `	"closedir($pHandle);"\` |
|        - |  1081 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1082 | `	"  /* Sort the array */"\` |
|        - |  1083 | `	"  sort($pArray);"\` |
|        - |  1084 | `	"}"\` |
|        - |  1085 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1086 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1087 | `	"  $pArray[] = $pattern;"\` |
|        - |  1088 | `	"}"\` |
|        - |  1089 | `	"/* Return the created array */"\` |
|        - |  1090 | `	"return $pArray;"\` |
|        - |  1091 | `   "}"\` |
|        - |  1092 | `   "/* Creates a temporary file */"\` |
|        - |  1093 | `   "function tmpfile(){"\` |
|        - |  1094 | `   "  /* Extract the temp directory */"\` |
|        - |  1095 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1096 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1097 | `   "    /* Use the current dir */"\` |
|        - |  1098 | `   "    $zTempDir = '.';"\` |
|        - |  1099 | `   "  }"\` |
|        - |  1100 | `   "  /* Create the file */"\` |
|        - |  1101 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1102 | `   "  return $pHandle;"\` |
|        - |  1103 | `   "}"\` |
|        - |  1104 | `   "/* Creates a temporary filename */"\` |
|        - |  1105 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1106 | `   "{"\` |
|        - |  1107 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1108 | `   "}"\` |
|        - |  1109 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1110 | `   " if( func_num_args() < 1 \|\| !is_array($pArray) ){  return 0; }"\` |
|        - |  1111 | `   "/* Copy arguments */"\` |
|        - |  1112 | `   "$nArgs = func_num_args();"\` |
|        - |  1113 | `   "$pNew = array();"\` |
|        - |  1114 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1115 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1116 | `    "}"\` |
|        - |  1117 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1118 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1119 | `	"/* Erase */"\` |
|        - |  1120 | `	"array_erase($pArray);"\` |
|        - |  1121 | `	"/* Unshift */"\` |
|        - |  1122 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1123 | `	"return sizeof($pArray);"\` |
|        - |  1124 | `    "}"\` |
|        - |  1125 | `	"function array_merge_recursive($array1, $array2){"\` |
|        - |  1126 | `	"if( func_num_args() < 1 ){ return NULL; }"\` |
|        - |  1127 | `    "$arrays = func_get_args();"\` |
|        - |  1128 | `    "$narrays = count($arrays);"\` |
|        - |  1129 | `    "$ret = $arrays[0];"\` |
|        - |  1130 | `    "for ($i = 1; $i < $narrays; $i++) {"\` |
|        - |  1131 | `	 " if( array_same($ret,$arrays[$i]) ){ /* Same instance */continue;}"\` |
|        - |  1132 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1133 | `     "  if (((string) $key) === ((string) intval($key))) {"\` |
|        - |  1134 | `     "   $ret[] = $value;"\` |
|        - |  1135 | `     "  }else{"\` |
|        - |  1136 | `     "  if (is_array($value) && isset($ret[$key]) ) {"\` |
|        - |  1137 | `     "   $ret[$key] = array_merge_recursive($ret[$key], $value);"\` |
|        - |  1138 | `     " }else {"\` |
|        - |  1139 | `     "   $ret[$key] = $value;"\` |
|        - |  1140 | `     "  }"\` |
|        - |  1141 | `     " }"\` |
|        - |  1142 | `     " }"\` |
|        - |  1143 | `	 "}"\` |
|        - |  1144 | `	 " return $ret;"\` |
|        - |  1145 | `    "}"\` |
|        - |  1146 | `	"function max(){"\` |
|        - |  1147 | `    "  $pArgs = func_get_args();"\` |
|        - |  1148 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1149 | `	"  return null;"\` |
|        - |  1150 | `    " }"\` |
|        - |  1151 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1152 | `    " $pArg = $pArgs[0];"\` |
|        - |  1153 | `	" if( !is_array($pArg) ){"\` |
|        - |  1154 | `	"   return $pArg; "\` |
|        - |  1155 | `	" }"\` |
|        - |  1156 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1157 | `	"   return null;"\` |
|        - |  1158 | `	" }"\` |
|        - |  1159 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1160 | `	" reset($pArg);"\` |
|        - |  1161 | `	" $max = current($pArg);"\` |
|        - |  1162 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1163 | `	"   if( $val > $max ){"\` |
|        - |  1164 | `	"     $max = $val;"\` |
|        - |  1165 | `    " }"\` |
|        - |  1166 | `	" }"\` |
|        - |  1167 | `	" return $max;"\` |
|        - |  1168 | `    " }"\` |
|        - |  1169 | `    " $max = $pArgs[0];"\` |
|        - |  1170 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1171 | `    " $val = $pArgs[$i];"\` |
|        - |  1172 | `	"if( $val > $max ){"\` |
|        - |  1173 | `	" $max = $val;"\` |
|        - |  1174 | `	"}"\` |
|        - |  1175 | `    " }"\` |
|        - |  1176 | `	" return $max;"\` |
|        - |  1177 | `    "}"\` |
|        - |  1178 | `	"function min(){"\` |
|        - |  1179 | `    "  $pArgs = func_get_args();"\` |
|        - |  1180 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1181 | `	"  return null;"\` |
|        - |  1182 | `    " }"\` |
|        - |  1183 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1184 | `    " $pArg = $pArgs[0];"\` |
|        - |  1185 | `	" if( !is_array($pArg) ){"\` |
|        - |  1186 | `	"   return $pArg; "\` |
|        - |  1187 | `	" }"\` |
|        - |  1188 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1189 | `	"   return null;"\` |
|        - |  1190 | `	" }"\` |
|        - |  1191 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1192 | `	" reset($pArg);"\` |
|        - |  1193 | `	" $min = current($pArg);"\` |
|        - |  1194 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1195 | `	"   if( $val < $min ){"\` |
|        - |  1196 | `	"     $min = $val;"\` |
|        - |  1197 | `    " }"\` |
|        - |  1198 | `	" }"\` |
|        - |  1199 | `	" return $min;"\` |
|        - |  1200 | `    " }"\` |
|        - |  1201 | `    " $min = $pArgs[0];"\` |
|        - |  1202 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1203 | `    " $val = $pArgs[$i];"\` |
|        - |  1204 | `	"if( $val < $min ){"\` |
|        - |  1205 | `	" $min = $val;"\` |
|        - |  1206 | `	" }"\` |
|        - |  1207 | `    " }"\` |
|        - |  1208 | `	" return $min;"\` |
|        - |  1209 | `	"}"\` |
|        - |  1210 | `	"function fileowner(string $file){"\` |
|        - |  1211 | `    " $a = stat($file);"\` |
|        - |  1212 | `	" if( !is_array($a) ){"\` |
|        - |  1213 | `	"	return false;"\` |
|        - |  1214 | `	" }"\` |
|        - |  1215 | `	" return $a['uid'];"\` |
|        - |  1216 | `    "}"\` |
|        - |  1217 | `    "function filegroup(string $file){"\` |
|        - |  1218 | `	" $a = stat($file);"\` |
|        - |  1219 | `	" if( !is_array($a) ){"\` |
|        - |  1220 | `	"	return false;"\` |
|        - |  1221 | `	" }"\` |
|        - |  1222 | `	" return $a['gid'];"\` |
|        - |  1223 | `    "}"\` |
|        - |  1224 | `	 "function fileinode(string $file){"\` |
|        - |  1225 | `	" $a = stat($file);"\` |
|        - |  1226 | `	" if( !is_array($a) ){"\` |
|        - |  1227 | `	"	return false;"\` |
|        - |  1228 | `	" }"\` |
|        - |  1229 | `	" return $a['ino'];"\` |
|        - |  1230 | `    "}"` |
|        - |  1231 |  |
|        - |  1232 | `/*` |
|        - |  1233 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1234 | ` * start compiling the target PHP program.` |
|        - |  1235 | ` */` |
|     1204 |  1236 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1237 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1238 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1239 | `	 )` |
|        2 |  1240 |  |
|        - |  1241 | `	SyString sBuiltin;` |
|        - |  1242 | `	ph7_value *pObj;` |
|        - |  1243 | `	sxi32 rc;` |
|        - |  1244 | `	/* Zero the structure */` |
|     1206 |  1245 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1246 | `	/* Initialize VM fields */` |
|     1206 |  1247 | `	pVm->pEngine = &(*pEngine);` |
|     1206 |  1248 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1249 | `	/* Instructions containers */` |
|     1206 |  1250 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     1206 |  1251 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     1206 |  1252 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1253 | `	/* Object containers */` |
|     1206 |  1254 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1206 |  1255 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1256 | `	/* Virtual machine internal containers */` |
|     1206 |  1257 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     1206 |  1258 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     1206 |  1259 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     1206 |  1260 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1206 |  1261 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     1206 |  1262 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     1206 |  1263 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     1206 |  1264 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     1206 |  1265 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     1206 |  1266 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     1206 |  1267 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     1206 |  1268 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     1206 |  1269 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     1206 |  1270 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     1206 |  1271 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|        - |  1272 | `	/* Configuration containers */` |
|     1206 |  1273 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     1206 |  1274 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     1206 |  1275 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     1206 |  1276 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     1206 |  1277 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1278 | `	/* Error callbacks containers */` |
|     1206 |  1279 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     1206 |  1280 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     1206 |  1281 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     1206 |  1282 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     1206 |  1283 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1284 | `	/* Set a default recursion limit */` |
|        - |  1285 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     1206 |  1286 | `	pVm->nMaxDepth = 32;` |
|        - |  1287 | `#else` |
|        - |  1288 | `	pVm->nMaxDepth = 16;` |
|        - |  1289 | `#endif` |
|        - |  1290 | `	/* Default assertion flags */` |
|     1206 |  1291 | `	pVm->iAssertFlags = PH7_ASSERT_WARNING; /* Issue a warning for each failed assertion */` |
|        - |  1292 | `	/* JSON return status */` |
|     1206 |  1293 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1294 | `	/* PRNG context */` |
|     1206 |  1295 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1296 | `	/* Install the null constant */` |
|     1206 |  1297 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1206 |  1298 | `	if( pObj == 0 ){` |
|      ! 0 |  1299 | `		rc = SXERR_MEM;` |
|      ! 0 |  1300 | `		goto Err;` |
|        - |  1301 | `	}` |
|     1206 |  1302 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1303 | `	/* Install the boolean TRUE constant */` |
|     1206 |  1304 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1206 |  1305 | `	if( pObj == 0 ){` |
|      ! 0 |  1306 | `		rc = SXERR_MEM;` |
|      ! 0 |  1307 | `		goto Err;` |
|        - |  1308 | `	}` |
|     1206 |  1309 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1310 | `	/* Install the boolean FALSE constant */` |
|     1206 |  1311 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1206 |  1312 | `	if( pObj == 0 ){` |
|      ! 0 |  1313 | `		rc = SXERR_MEM;` |
|      ! 0 |  1314 | `		goto Err;` |
|        - |  1315 | `	}` |
|     1206 |  1316 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1317 | `	/* Create the global frame */` |
|     1206 |  1318 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     1206 |  1319 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1320 | `		goto Err;` |
|        - |  1321 | `	}` |
|        - |  1322 | `	/* Initialize the code generator */` |
|     1206 |  1323 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1206 |  1324 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1325 | `		goto Err;` |
|        - |  1326 | `	}` |
|        - |  1327 | `	/* VM correctly initialized,set the magic number */` |
|     1206 |  1328 | `	pVm->nMagic = PH7_VM_INIT;` |
|     1206 |  1329 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1330 | `	/* Compile the built-in library */` |
|     1206 |  1331 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1332 | `	/* Reset the code generator */` |
|     1206 |  1333 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1206 |  1334 | `	return SXRET_OK;` |
|      ! 0 |  1335 | `Err:` |
|      ! 0 |  1336 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1337 | `	return rc;` |
|      604 |  1338 |  |
|        - |  1339 | `/*` |
|        - |  1340 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1341 | ` * routine which store the output in an internal blob.` |
|        - |  1342 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1343 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1344 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1345 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1346 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1347 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1348 | ` * to finish executing and extracting the output.` |
|        - |  1349 | ` */` |
|      ! 0 |  1350 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1351 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1352 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1353 | `	void *pUserData     /* User private data */` |
|        - |  1354 | `	)` |
|      ! 0 |  1355 |  |
|        - |  1356 | `	 sxi32 rc;` |
|        - |  1357 | `	 /* Store the output in an internal BLOB */` |
|      ! 0 |  1358 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|      ! 0 |  1359 | `	 return rc;` |
|      ! 0 |  1360 |  |
|        - |  1361 | `#define VM_STACK_GUARD 16` |
|        - |  1362 | `/*` |
|        - |  1363 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1364 | ` * our compiled PHP program.` |
|        - |  1365 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1366 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1367 | ` */` |
|    19950 |  1368 | `static ph7_value * VmNewOperandStack(` |
|        - |  1369 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1370 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1371 | `	)` |
|        2 |  1372 |  |
|        - |  1373 | `	ph7_value *pStack;` |
|        - |  1374 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1375 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1376 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1377 | `  ** on the maximum stack depth required.` |
|        - |  1378 | `  **` |
|        - |  1379 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1380 | `  */` |
|    19952 |  1381 | `	nInstr += VM_STACK_GUARD;` |
|    19952 |  1382 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    19952 |  1383 | `	if( pStack == 0 ){` |
|      ! 0 |  1384 | `		return 0;` |
|        - |  1385 | `	}` |
|        - |  1386 | `	/* Initialize the operand stack */` |
|  1246930 |  1387 | `	while( nInstr > 0 ){` |
|  1226980 |  1388 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1226980 |  1389 | `		--nInstr;` |
|        2 |  1390 | `	}` |
|        - |  1391 | `	/* Ready for bytecode execution */` |
|    19952 |  1392 | `	return pStack;` |
|     9977 |  1393 |  |
|        - |  1394 | `/* Forward declaration */` |
|        - |  1395 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1396 | `static int VmInstanceOf(ph7_class *pThis,ph7_class *pClass);` |
|        - |  1397 | `static int VmClassMemberAccess(ph7_vm *pVm,ph7_class *pClass,const SyString *pAttrName,sxi32 iProtection,int bLog);` |
|        - |  1398 | `/*` |
|        - |  1399 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1400 | ` * This routine gets called by the PH7 engine after` |
|        - |  1401 | ` * successful compilation of the target PHP program.` |
|        - |  1402 | ` */` |
|      944 |  1403 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1404 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1405 | `	)` |
|        2 |  1406 |  |
|        - |  1407 | `	SyHashEntry *pEntry;` |
|        - |  1408 | `	sxi32 rc;` |
|      946 |  1409 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1410 | `		/* Initialize your VM first */` |
|      ! 0 |  1411 | `		return SXERR_CORRUPT;` |
|        - |  1412 | `	}` |
|        - |  1413 | `	/* Mark the VM ready for byte-code execution */` |
|      946 |  1414 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1415 | `	/* Release the code generator now we have compiled our program */` |
|      946 |  1416 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1417 | `	/* Emit the DONE instruction */` |
|      946 |  1418 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|      946 |  1419 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1420 | `		return SXERR_MEM;` |
|        - |  1421 | `	}` |
|        - |  1422 | `	/* Script return value */` |
|      946 |  1423 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1424 | `	/* Allocate a new operand stack */` |
|      946 |  1425 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|      946 |  1426 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1427 | `		return SXERR_MEM;` |
|        - |  1428 | `	}` |
|        - |  1429 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1430 | `	 * private data. */` |
|      946 |  1431 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|      946 |  1432 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1433 | `	/* Allocate the reference table */` |
|      946 |  1434 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|      946 |  1435 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|      946 |  1436 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1437 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1438 | `		return SXERR_MEM;` |
|        - |  1439 | `	}` |
|        - |  1440 | `	/* Zero the reference table */` |
|      946 |  1441 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1442 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|      946 |  1443 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|      946 |  1444 | `	if( rc != SXRET_OK ){` |
|        - |  1445 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1446 | `		return rc;` |
|        - |  1447 | `	}` |
|        - |  1448 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|      946 |  1449 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|      946 |  1450 | `	if( rc != SXRET_OK ){` |
|        - |  1451 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1452 | `		return rc;` |
|        - |  1453 | `	}` |
|        - |  1454 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|      946 |  1455 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1456 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|      946 |  1457 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1458 | `	/* Initialize and install static and constants class attributes */` |
|      946 |  1459 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    10408 |  1460 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|     9464 |  1461 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|     9464 |  1462 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1463 | `			return rc;` |
|        - |  1464 | `		}` |
|        2 |  1465 | `	}` |
|        - |  1466 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|      946 |  1467 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1468 | `	/* VM is ready for bytecode execution */` |
|      946 |  1469 | `	return SXRET_OK;` |
|      474 |  1470 |  |
|        - |  1471 | `/*` |
|        - |  1472 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1473 | ` */` |
|      ! 0 |  1474 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1475 |  |
|      ! 0 |  1476 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1477 | `		return SXERR_CORRUPT;` |
|        - |  1478 | `	}` |
|        - |  1479 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1480 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1481 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1482 | `	/* Set the ready flag */` |
|      ! 0 |  1483 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1484 | `	return SXRET_OK;` |
|      ! 0 |  1485 |  |
|        - |  1486 | `/*` |
|        - |  1487 | ` * Release a Virtual Machine.` |
|        - |  1488 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1489 | ` */` |
|      936 |  1490 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1491 |  |
|        - |  1492 | `	/* Set the stale magic number */` |
|      938 |  1493 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1494 | `	/* Release the private memory subsystem */` |
|      938 |  1495 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      938 |  1496 | `	return SXRET_OK;` |
|        2 |  1497 |  |
|        - |  1498 | `/*` |
|        - |  1499 | ` * Initialize a foreign function call context.` |
|        - |  1500 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1501 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1502 | ` * functions.` |
|        - |  1503 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1504 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1505 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1506 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1507 | ` */` |
|   403770 |  1508 | `static sxi32 VmInitCallContext(` |
|        - |  1509 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1510 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1511 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1512 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1513 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1514 | `	)` |
|        2 |  1515 |  |
|   403772 |  1516 | `	pOut->pFunc = pFunc;` |
|   403772 |  1517 | `	pOut->pVm   = pVm;` |
|   403772 |  1518 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   403772 |  1519 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1520 | `	/* Assume a null return value */` |
|   403772 |  1521 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   403772 |  1522 | `	pOut->pRet = pRet;` |
|   403772 |  1523 | `	pOut->iFlags = iFlags;` |
|   403772 |  1524 | `	return SXRET_OK;` |
|        2 |  1525 |  |
|        - |  1526 | `/*` |
|        - |  1527 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1528 | ` * left behind.` |
|        - |  1529 | ` */` |
|   403770 |  1530 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1531 |  |
|        - |  1532 | `	sxu32 n;` |
|   403772 |  1533 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     4420 |  1534 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    12270 |  1535 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     7852 |  1536 | `			if( apObj[n] == 0 ){` |
|        - |  1537 | `				/* Already released */` |
|      250 |  1538 | `				continue;` |
|        - |  1539 | `			}` |
|     7604 |  1540 | `			PH7_MemObjRelease(apObj[n]);` |
|     7604 |  1541 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     3803 |  1542 | `		}` |
|     4420 |  1543 | `		SySetRelease(&pCtx->sVar);` |
|     2209 |  1544 | `	}` |
|   403772 |  1545 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1546 | `		ph7_aux_data *aAux;` |
|        - |  1547 | `		void *pChunk;` |
|        - |  1548 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1549 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1550 | `		 */` |
|        9 |  1551 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1552 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1553 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1554 | `			/* Release the chunk */` |
|       25 |  1555 | `			if( pChunk ){` |
|       25 |  1556 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1557 | `			}` |
|       13 |  1558 | `		}` |
|        9 |  1559 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1560 | `	}` |
|   403772 |  1561 |  |
|        - |  1562 | `/*` |
|        - |  1563 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1564 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1565 | ` */` |
|      248 |  1566 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1567 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1568 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1569 | `	)` |
|        2 |  1570 |  |
|      250 |  1571 | `	if( pValue == 0 ){` |
|        - |  1572 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1573 | `		return;` |
|        - |  1574 | `	}` |
|      250 |  1575 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      250 |  1576 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1577 | `		sxu32 n;` |
|      936 |  1578 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      936 |  1579 | `			if( apObj[n] == pValue ){` |
|      250 |  1580 | `				PH7_MemObjRelease(pValue);` |
|      250 |  1581 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1582 | `				/* Mark as released */` |
|      250 |  1583 | `				apObj[n] = 0;` |
|      250 |  1584 | `				break;` |
|        - |  1585 | `			}` |
|      345 |  1586 | `		}` |
|      124 |  1587 | `	}` |
|      126 |  1588 |  |
|        - |  1589 | `/*` |
|        - |  1590 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1591 | ` */` |
|  2203086 |  1592 | `static void VmPopOperand(` |
|        - |  1593 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1594 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1595 | `	)` |
|        2 |  1596 |  |
|  2203088 |  1597 | `	ph7_value *pTos = *ppTos;` |
|  4713558 |  1598 | `	while( nPop > 0 ){` |
|  2510472 |  1599 | `		PH7_MemObjRelease(pTos);` |
|  2510472 |  1600 | `		pTos--;` |
|  2510472 |  1601 | `		nPop--;` |
|        2 |  1602 | `	}` |
|        - |  1603 | `	/* Top of the stack */` |
|  2203088 |  1604 | `	*ppTos = pTos;` |
|  2203088 |  1605 |  |
|        - |  1606 | `/*` |
|        - |  1607 | ` * Reserve a memory object.` |
|        - |  1608 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1609 | ` */` |
|   607762 |  1610 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1611 |  |
|   607764 |  1612 | `	ph7_value *pObj = 0;` |
|        - |  1613 | `	VmSlot *pSlot;` |
|        - |  1614 | `	sxu32 nIdx;` |
|        - |  1615 | `	/* Check for a free slot */` |
|   607764 |  1616 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|   607764 |  1617 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|   607764 |  1618 | `	if( pSlot ){` |
|   531856 |  1619 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   531856 |  1620 | `		nIdx = pSlot->nIdx;` |
|   265927 |  1621 | `	}` |
|   607764 |  1622 | `	if( pObj == 0 ){` |
|        - |  1623 | `		/* Reserve a new memory object */` |
|    75910 |  1624 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|    75910 |  1625 | `		if( pObj == 0 ){` |
|      ! 0 |  1626 | `			return 0;` |
|        - |  1627 | `		}` |
|    37954 |  1628 | `	}` |
|        - |  1629 | `	/* Set a null default value */` |
|   607764 |  1630 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|   607764 |  1631 | `	pObj->nIdx = nIdx;` |
|   607764 |  1632 | `	return pObj;` |
|   303883 |  1633 |  |
|        - |  1634 | `/*` |
|        - |  1635 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1636 | ` */` |
|    14052 |  1637 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1638 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1639 | `	const char *zKey,  /* Entry key */` |
|        - |  1640 | `	sxu32 nByte,       /* Key length */` |
|        - |  1641 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1642 | `	)` |
|        2 |  1643 |  |
|        - |  1644 | `	ph7_value sKey;` |
|        - |  1645 | `	sxi32 rc;` |
|    14054 |  1646 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    14054 |  1647 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1648 | `	/* Perform the insertion */` |
|    14054 |  1649 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    14054 |  1650 | `	PH7_MemObjRelease(&sKey);` |
|    14054 |  1651 | `	return rc;` |
|        2 |  1652 |  |
|        - |  1653 | `/*` |
|        - |  1654 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1655 | ` * Return a pointer to the variable value on success.` |
|        - |  1656 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1657 | ` */` |
|  1994774 |  1658 | `static ph7_value * VmExtractMemObj(` |
|        - |  1659 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1660 | `	const SyString *pName, /* Variable name */` |
|        - |  1661 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1662 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1663 | `	)` |
|        2 |  1664 |  |
|  1994776 |  1665 | `	int bNullify = FALSE;` |
|        - |  1666 | `	SyHashEntry *pEntry;` |
|        - |  1667 | `	VmFrame *pFrame;` |
|        - |  1668 | `	ph7_value *pObj;` |
|        - |  1669 | `	sxu32 nIdx;` |
|        - |  1670 | `	sxi32 rc;` |
|        - |  1671 | `	/* Point to the top active frame */` |
|  1994776 |  1672 | `	pFrame = pVm->pFrame;` |
|  2044908 |  1673 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1674 | `		/* Safely ignore the exception frame */` |
|    50133 |  1675 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        1 |  1676 | `	}` |
|        - |  1677 | `	/* Perform the lookup */` |
|  1994776 |  1678 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1679 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1680 | `		pName = &sAnnon;` |
|        - |  1681 | `		/* Always nullify the object */` |
|      ! 0 |  1682 | `		bNullify = TRUE;` |
|      ! 0 |  1683 | `		bDup = FALSE;` |
|      ! 0 |  1684 | `	}` |
|        - |  1685 | `	/* Check the superglobals table first */` |
|  1994776 |  1686 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  1994776 |  1687 | `	if( pEntry == 0 ){` |
|        - |  1688 | `		/* Query the top active frame */` |
|  1994740 |  1689 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  1994740 |  1690 | `		if( pEntry == 0 ){` |
|    54628 |  1691 | `			char *zName = (char *)pName->zString;` |
|        - |  1692 | `			VmSlot sLocal;` |
|    54628 |  1693 | `			if( !bCreate ){` |
|        - |  1694 | `				/* Do not create the variable,return NULL instead */` |
|      466 |  1695 | `				return 0;` |
|        - |  1696 | `			}` |
|        - |  1697 | `			/* No such variable,automatically create a new one and install` |
|        - |  1698 | `			 * it in the current frame.` |
|        - |  1699 | `			 */` |
|    54164 |  1700 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    54164 |  1701 | `			if( pObj == 0 ){` |
|      ! 0 |  1702 | `				return 0;` |
|        - |  1703 | `			}` |
|    54164 |  1704 | `			nIdx = pObj->nIdx;` |
|    54164 |  1705 | `			if( bDup ){` |
|        - |  1706 | `				/* Duplicate name */` |
|      115 |  1707 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      115 |  1708 | `				if( zName == 0 ){` |
|      ! 0 |  1709 | `					return 0;` |
|        - |  1710 | `				}` |
|       57 |  1711 | `			}` |
|        - |  1712 | `			/* Link to the top active VM frame */` |
|    54164 |  1713 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    54164 |  1714 | `			if( rc != SXRET_OK ){` |
|        - |  1715 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1716 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1717 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1718 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1719 | `				return 0;` |
|        - |  1720 | `			}` |
|    54164 |  1721 | `			if( pFrame->pParent != 0 ){` |
|        - |  1722 | `				/* Local variable */` |
|    49576 |  1723 | `				sLocal.nIdx = nIdx;` |
|    49576 |  1724 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    24789 |  1725 | `			}else{` |
|        - |  1726 | `				/* Register in the $GLOBALS array */` |
|     4590 |  1727 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1728 | `			}` |
|        - |  1729 | `			/* Install in the reference table */` |
|    54164 |  1730 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1731 | `			/* Save object index */` |
|    54164 |  1732 | `			pObj->nIdx = nIdx;` |
|    27083 |  1733 | `		}else{` |
|        - |  1734 | `			/* Extract variable contents */` |
|  1940114 |  1735 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  1940114 |  1736 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  1940114 |  1737 | `			if( bNullify && pObj ){` |
|      ! 0 |  1738 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1739 | `			}` |
|        - |  1740 | `		}` |
|   997249 |  1741 | `	}else{` |
|        - |  1742 | `		/* Superglobal */` |
|       38 |  1743 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1744 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1745 | `	}` |
|  1994312 |  1746 | `	return pObj;` |
|   997499 |  1747 |  |
|        - |  1748 | `/*` |
|        - |  1749 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1750 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1751 | ` */` |
|      962 |  1752 | `static ph7_value * VmExtractSuper(` |
|        - |  1753 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1754 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1755 | `	sxu32 nByte        /* zName length */` |
|        - |  1756 | `	)` |
|        2 |  1757 |  |
|        - |  1758 | `	SyHashEntry *pEntry;` |
|        - |  1759 | `	ph7_value *pValue;` |
|        - |  1760 | `	sxu32 nIdx;` |
|        - |  1761 | `	/* Query the superglobal table */` |
|      964 |  1762 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|      964 |  1763 | `	if( pEntry == 0 ){` |
|        - |  1764 | `		/* No such entry */` |
|      ! 0 |  1765 | `		return 0;` |
|        - |  1766 | `	}` |
|        - |  1767 | `	/* Extract the superglobal index in the global object pool */` |
|      964 |  1768 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1769 | `	/* Extract the variable value  */` |
|      964 |  1770 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      964 |  1771 | `	return pValue;` |
|      483 |  1772 |  |
|        - |  1773 | `/*` |
|        - |  1774 | ` * Perform a raw hashmap insertion.` |
|        - |  1775 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1776 | ` */` |
|      960 |  1777 | `static sxi32 VmHashmapInsert(` |
|        - |  1778 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1779 | `	const char *zKey,   /* Entry key */` |
|        - |  1780 | `	int nKeylen,        /* zKey length*/` |
|        - |  1781 | `	const char *zData,  /* Entry data */` |
|        - |  1782 | `	int nLen            /* zData length */` |
|        - |  1783 | `	)` |
|        2 |  1784 |  |
|        - |  1785 | `	ph7_value sKey,sValue;` |
|        - |  1786 | `	sxi32 rc;` |
|      962 |  1787 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|      962 |  1788 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|      962 |  1789 | `	if( zKey ){` |
|      948 |  1790 | `		if( nKeylen < 0 ){` |
|      948 |  1791 | `			nKeylen = (int)SyStrlen(zKey);` |
|      473 |  1792 | `		}` |
|      948 |  1793 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|      473 |  1794 | `	}` |
|      962 |  1795 | `	if( zData ){` |
|      962 |  1796 | `		if( nLen < 0 ){` |
|        - |  1797 | `			/* Compute length automatically */` |
|      ! 0 |  1798 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1799 | `		}` |
|      962 |  1800 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|      480 |  1801 | `	}` |
|        - |  1802 | `	/* Perform the insertion */` |
|      962 |  1803 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|      962 |  1804 | `	PH7_MemObjRelease(&sKey);` |
|      962 |  1805 | `	PH7_MemObjRelease(&sValue);` |
|      962 |  1806 | `	return rc;` |
|        2 |  1807 |  |
|        - |  1808 | `/* Forward declaration */` |
|        - |  1809 | `static sxi32 VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte);` |
|        - |  1810 | `/*` |
|        - |  1811 | ` * Configure a working virtual machine instance.` |
|        - |  1812 | ` *` |
|        - |  1813 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1814 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1815 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1816 | ` * The second argument to this function is an integer configuration option` |
|        - |  1817 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1818 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1819 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1820 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1821 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1822 | ` */` |
|    15120 |  1823 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1824 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1825 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1826 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1827 | `	)` |
|        2 |  1828 |  |
|    15122 |  1829 | `	sxi32 rc = SXRET_OK;` |
|    15122 |  1830 | `	switch(nOp){` |
|      472 |  1831 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|      946 |  1832 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|      946 |  1833 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1834 | `		/* VM output consumer callback */` |
|        - |  1835 | `#ifdef UNTRUST` |
|        - |  1836 | `		if( xConsumer == 0 ){` |
|        - |  1837 | `			rc = SXERR_CORRUPT;` |
|        - |  1838 | `			break;` |
|        - |  1839 | `		}` |
|        - |  1840 | `#endif` |
|        - |  1841 | `		/* Install the output consumer */` |
|      946 |  1842 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|      946 |  1843 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|      946 |  1844 | `		break;` |
|        - |  1845 | `							   }` |
|      472 |  1846 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1847 | `		/* Import path */` |
|        - |  1848 | `		  const char *zPath;` |
|        - |  1849 | `		  SyString sPath;` |
|      946 |  1850 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1851 | `#if defined(UNTRUST)` |
|        - |  1852 | `		  if( zPath == 0 ){` |
|        - |  1853 | `			  rc = SXERR_EMPTY;` |
|        - |  1854 | `			  break;` |
|        - |  1855 | `		  }` |
|        - |  1856 | `#endif` |
|      946 |  1857 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1858 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1859 | `#ifdef __WINNT__` |
|        2 |  1860 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1861 | `#endif` |
|     1890 |  1862 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1863 | `		  /* Remove leading and trailing white spaces */` |
|      946 |  1864 | `		  SyStringFullTrim(&sPath);` |
|      946 |  1865 | `		  if( sPath.nByte > 0 ){` |
|        - |  1866 | `			  /* Store the path in the corresponding conatiner */` |
|      946 |  1867 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      472 |  1868 | `		  }` |
|      946 |  1869 | `		  break;` |
|        - |  1870 | `									 }` |
|      472 |  1871 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1872 | `		/* Run-Time Error report */` |
|      946 |  1873 | `		pVm->bErrReport = 1;` |
|      946 |  1874 | `		break;` |
|      ! 0 |  1875 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1876 | `		/* Recursion depth */` |
|      ! 0 |  1877 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1878 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1879 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1880 | `		}` |
|      ! 0 |  1881 | `		break;` |
|        - |  1882 | `									   }` |
|      ! 0 |  1883 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1884 | `		/* VM output length in bytes */` |
|      ! 0 |  1885 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1886 | `#ifdef UNTRUST` |
|        - |  1887 | `		if( pOut == 0 ){` |
|        - |  1888 | `			rc = SXERR_CORRUPT;` |
|        - |  1889 | `			break;` |
|        - |  1890 | `		}` |
|        - |  1891 | `#endif` |
|      ! 0 |  1892 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1893 | `		break;` |
|        - |  1894 | `							   }` |
|        - |  1895 |  |
|     4720 |  1896 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1897 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1898 | `		/* Create a new superglobal/global variable */` |
|     9442 |  1899 | `		const char *zName = va_arg(ap,const char *);` |
|     9442 |  1900 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1901 | `		SyHashEntry *pEntry;` |
|        - |  1902 | `		ph7_value *pObj;` |
|        - |  1903 | `		sxu32 nByte;` |
|        - |  1904 | `		sxu32 nIdx;` |
|        - |  1905 | `#ifdef UNTRUST` |
|        - |  1906 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  1907 | `			rc = SXERR_CORRUPT;` |
|        - |  1908 | `			break;` |
|        - |  1909 | `		}` |
|        - |  1910 | `#endif` |
|     9442 |  1911 | `		nByte = SyStrlen(zName);` |
|     9442 |  1912 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1913 | `			/* Check if the superglobal is already installed */` |
|     9442 |  1914 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     4722 |  1915 | `		}else{` |
|        - |  1916 | `			/* Query the top active VM frame */` |
|      ! 0 |  1917 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1918 | `		}` |
|     9442 |  1919 | `		if( pEntry ){` |
|        - |  1920 | `			/* Variable already installed */` |
|      ! 0 |  1921 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1922 | `			/* Extract contents */` |
|      ! 0 |  1923 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  1924 | `			if( pObj ){` |
|        - |  1925 | `				/* Overwrite old contents */` |
|      ! 0 |  1926 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  1927 | `			}` |
|      ! 0 |  1928 | `		}else{` |
|        - |  1929 | `			/* Install a new variable */` |
|     9442 |  1930 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|     9442 |  1931 | `			if( pObj == 0 ){` |
|      ! 0 |  1932 | `				rc = SXERR_MEM;` |
|      ! 0 |  1933 | `				break;` |
|        - |  1934 | `			}` |
|     9442 |  1935 | `			nIdx = pObj->nIdx;` |
|        - |  1936 | `			/* Copy value */` |
|     9442 |  1937 | `			PH7_MemObjStore(pValue,pObj);` |
|     9442 |  1938 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1939 | `				/* Install the superglobal */` |
|     9442 |  1940 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     4722 |  1941 | `			}else{` |
|        - |  1942 | `				/* Install in the current frame */` |
|      ! 0 |  1943 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1944 | `			}` |
|     9442 |  1945 | `			if( rc == SXRET_OK ){` |
|        - |  1946 | `				SyHashEntry *pRef;` |
|     9442 |  1947 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|     9442 |  1948 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     4722 |  1949 | `				}else{` |
|      ! 0 |  1950 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1951 | `				}` |
|        - |  1952 | `				/* Install in the reference table */` |
|     9442 |  1953 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|     9442 |  1954 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1955 | `					/* Register in the $GLOBALS array */` |
|     9442 |  1956 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     4720 |  1957 | `				}` |
|     4720 |  1958 | `			}` |
|        - |  1959 | `		}` |
|     9442 |  1960 | `		break;` |
|        - |  1961 | `									}` |
|      473 |  1962 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1963 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1964 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1965 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1966 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1967 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1968 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|      948 |  1969 | `		const char *zKey   = va_arg(ap,const char *);` |
|      948 |  1970 | `		const char *zValue = va_arg(ap,const char *);` |
|      948 |  1971 | `		int nLen = va_arg(ap,int);` |
|        - |  1972 | `		ph7_hashmap *pMap;` |
|        - |  1973 | `		ph7_value *pValue;` |
|      948 |  1974 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1975 | `			/* Extract the $_ENV superglobal */` |
|        3 |  1976 | `			pValue = VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|      947 |  1977 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  1978 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  1979 | `			pValue = VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|      946 |  1980 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  1981 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  1982 | `			pValue = VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|      946 |  1983 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  1984 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  1985 | `			pValue = VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|      946 |  1986 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  1987 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  1988 | `			pValue = VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|      946 |  1989 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  1990 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  1991 | `			pValue = VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  1992 | `		}else{` |
|        - |  1993 | `			/* Extract the $_SERVER superglobal */` |
|      946 |  1994 | `			pValue = VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  1995 | `		}` |
|      948 |  1996 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1997 | `			/* No such entry */` |
|      ! 0 |  1998 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1999 | `			break;` |
|        - |  2000 | `		}` |
|        - |  2001 | `		/* Point to the hashmap */` |
|      948 |  2002 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2003 | `		/* Perform the insertion */` |
|      948 |  2004 | `		rc = VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|      948 |  2005 | `		break;` |
|        - |  2006 | `								   }` |
|        7 |  2007 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2008 | `		/* Script arguments */` |
|       16 |  2009 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2010 | `		ph7_hashmap *pMap;` |
|        - |  2011 | `		ph7_value *pValue;` |
|        - |  2012 | `		sxu32 n;` |
|       16 |  2013 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2014 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2015 | `			break;` |
|        - |  2016 | `		}` |
|        - |  2017 | `		/* Extract the $argv array */` |
|       16 |  2018 | `		pValue = VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       16 |  2019 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2020 | `			/* No such entry */` |
|      ! 0 |  2021 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2022 | `			break;` |
|        - |  2023 | `		}` |
|        - |  2024 | `		/* Point to the hashmap */` |
|       16 |  2025 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2026 | `		/* Perform the insertion */` |
|       16 |  2027 | `		n = (sxu32)SyStrlen(zValue);` |
|       16 |  2028 | `		rc = VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       16 |  2029 | `		if( rc == SXRET_OK ){` |
|       16 |  2030 | `			if( pMap->nEntry > 1 ){` |
|        - |  2031 | `				/* Append space separator first */` |
|       10 |  2032 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        4 |  2033 | `			}` |
|       16 |  2034 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|        7 |  2035 | `		}` |
|       16 |  2036 | `		break;` |
|        - |  2037 | `								  }` |
|      ! 0 |  2038 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2039 | `		/* error_log() consumer */` |
|      ! 0 |  2040 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2041 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2042 | `		break;` |
|        - |  2043 | `										}` |
|      ! 0 |  2044 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2045 | `		/* Script return value */` |
|      ! 0 |  2046 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2047 | `#ifdef UNTRUST` |
|        - |  2048 | `		if( ppValue == 0 ){` |
|        - |  2049 | `			rc = SXERR_CORRUPT;` |
|        - |  2050 | `			break;` |
|        - |  2051 | `		}` |
|        - |  2052 | `#endif` |
|      ! 0 |  2053 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2054 | `		break;` |
|        - |  2055 | `								   }` |
|      944 |  2056 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2057 | `		/* Register an IO stream device */` |
|     1890 |  2058 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2059 | `		/* Make sure we are dealing with a valid IO stream */` |
|     2832 |  2060 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     1890 |  2061 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2062 | `				/* Invalid stream */` |
|      ! 0 |  2063 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2064 | `				break;` |
|        - |  2065 | `		}` |
|     1890 |  2066 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2067 | `			/* Make the 'file://' stream the defaut stream device */` |
|      946 |  2068 | `			pVm->pDefStream = pStream;` |
|      472 |  2069 | `		}` |
|        - |  2070 | `		/* Insert in the appropriate container */` |
|     1890 |  2071 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     1890 |  2072 | `		break;` |
|        - |  2073 | `								  }` |
|      ! 0 |  2074 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2075 | `		/* Point to the VM internal output consumer buffer */` |
|      ! 0 |  2076 | `		const void **ppOut = va_arg(ap,const void **);` |
|      ! 0 |  2077 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2078 | `#ifdef UNTRUST` |
|        - |  2079 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2080 | `			rc = SXERR_CORRUPT;` |
|        - |  2081 | `			break;` |
|        - |  2082 | `		}` |
|        - |  2083 | `#endif` |
|      ! 0 |  2084 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|      ! 0 |  2085 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|      ! 0 |  2086 | `		break;` |
|        - |  2087 | `									   }` |
|      ! 0 |  2088 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2089 | `		/* Raw HTTP request*/` |
|      ! 0 |  2090 | `		const char *zRequest = va_arg(ap,const char *);` |
|      ! 0 |  2091 | `		int nByte = va_arg(ap,int);` |
|      ! 0 |  2092 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2093 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2094 | `			break;` |
|        - |  2095 | `		}` |
|      ! 0 |  2096 | `		if( nByte < 0 ){` |
|        - |  2097 | `			/* Compute length automatically */` |
|      ! 0 |  2098 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2099 | `		}` |
|        - |  2100 | `		/* Process the request */` |
|      ! 0 |  2101 | `		rc = VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|      ! 0 |  2102 | `		break;` |
|        - |  2103 | `									}` |
|      ! 0 |  2104 | `	default:` |
|        - |  2105 | `		/* Unknown configuration option */` |
|      ! 0 |  2106 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2107 | `		break;` |
|        - |  2108 | `	}` |
|    15122 |  2109 | `	return rc;` |
|        2 |  2110 |  |
|        - |  2111 | `/* Forward declaration */` |
|        - |  2112 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2113 | `/*` |
|        - |  2114 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2115 | ` * format.` |
|        - |  2116 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2117 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2118 | ` * (STDOUT).` |
|        - |  2119 | ` */` |
|        2 |  2120 | `static sxi32 VmByteCodeDump(` |
|        - |  2121 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2122 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2123 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2124 | `	)` |
|        1 |  2125 |  |
|        - |  2126 | `	static const char zDump[] = {` |
|        - |  2127 | `		"====================================================\n"` |
|        - |  2128 | `		"PH7 VM Dump\n"` |
|        - |  2129 | `		"====================================================\n"` |
|        - |  2130 | `	};` |
|        - |  2131 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2132 | `	sxi32 rc = SXRET_OK;` |
|        - |  2133 | `	sxu32 n;` |
|        - |  2134 | `	/* Point to the PH7 instructions */` |
|        3 |  2135 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2136 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2137 | `	n = 0;` |
|        3 |  2138 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2139 | `	/* Dump instructions */` |
|        6 |  2140 | `	for(;;){` |
|       13 |  2141 | `		if( pInstr >= pEnd ){` |
|        - |  2142 | `			/* No more instructions */` |
|        3 |  2143 | `			break;` |
|        - |  2144 | `		}` |
|        - |  2145 | `		/* Format and call the consumer callback */` |
|       16 |  2146 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       10 |  2147 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       10 |  2148 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       11 |  2149 | `		if( rc != SXRET_OK ){` |
|        - |  2150 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2151 | `			return rc;` |
|        - |  2152 | `		}` |
|       11 |  2153 | `		++n;` |
|       11 |  2154 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2155 | `	}` |
|        3 |  2156 | `	return rc;` |
|        2 |  2157 |  |
|        - |  2158 | `/* Forward declaration */` |
|        - |  2159 | `static int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData);` |
|        - |  2160 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2161 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2162 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2163 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2164 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2165 | `/*` |
|        - |  2166 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2167 | ` * consumer callback.` |
|        - |  2168 | ` */` |
|       86 |  2169 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        2 |  2170 |  |
|       88 |  2171 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|       88 |  2172 | `	sxi32 rc = SXRET_OK;` |
|        - |  2173 | `	/* Append a new line */` |
|        - |  2174 | `#ifdef __WINNT__` |
|        2 |  2175 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2176 | `#else` |
|       86 |  2177 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2178 | `#endif` |
|        - |  2179 | `	/* Invoke the output consumer callback */` |
|       88 |  2180 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|       88 |  2181 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  2182 | `		/* Increment output length */` |
|       87 |  2183 | `		pVm->nOutputLen += SyBlobLength(pMsg);` |
|       43 |  2184 | `	}` |
|       88 |  2185 | `	return rc;` |
|        2 |  2186 |  |
|        - |  2187 | `/*` |
|        - |  2188 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2189 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2190 | ` * information.` |
|        - |  2191 | ` */` |
|       86 |  2192 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, SyString *pFile, sxi32 iLine)` |
|        2 |  2193 |  |
|       88 |  2194 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2195 | `		ph7_value apArg[4];` |
|        - |  2196 | `		ph7_value *apArgPtr[4];` |
|        - |  2197 | `		ph7_value sResult;` |
|        - |  2198 | `		SyString sErr;` |
|        - |  2199 | `		/* Prepare arguments */` |
|        9 |  2200 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        9 |  2201 | `		SyStringInitFromBuf(&sErr,zMessage,SyStrlen(zMessage));` |
|        9 |  2202 | `		PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|        9 |  2203 | `		if( pFile ){` |
|        9 |  2204 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|        9 |  2205 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|        5 |  2206 | `		}else{` |
|      ! 0 |  2207 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2208 | `		}` |
|        9 |  2209 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|        9 |  2210 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2211 | `		/* Set up pointer array */` |
|        9 |  2212 | `		apArgPtr[0] = &apArg[0];` |
|        9 |  2213 | `		apArgPtr[1] = &apArg[1];` |
|        9 |  2214 | `		apArgPtr[2] = &apArg[2];` |
|        9 |  2215 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2216 | `		/* Call the handler */` |
|        9 |  2217 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2218 | `		/* Check return value */` |
|        9 |  2219 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2220 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2221 | `		}` |
|        - |  2222 | `		/* Release */` |
|        9 |  2223 | `		PH7_MemObjRelease(&apArg[0]);` |
|        9 |  2224 | `		PH7_MemObjRelease(&apArg[1]);` |
|        9 |  2225 | `		PH7_MemObjRelease(&apArg[2]);` |
|        9 |  2226 | `		PH7_MemObjRelease(&apArg[3]);` |
|        9 |  2227 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2228 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2229 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|        9 |  2230 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2231 | `	}` |
|        - |  2232 | `	/* No handler, always call error handler */` |
|       80 |  2233 | `	return TRUE;` |
|       45 |  2234 |  |
|       62 |  2235 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2236 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2237 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2238 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2239 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2240 | `	)` |
|        2 |  2241 |  |
|       64 |  2242 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2243 | `	SyString *pFile;` |
|        - |  2244 | `	char *zErr;` |
|       64 |  2245 | `	sxi32 rc = SXRET_OK;` |
|       64 |  2246 | `	if( !pVm->bErrReport ){` |
|        - |  2247 | `		/* Don't bother reporting errors */` |
|        3 |  2248 | `		return SXRET_OK;` |
|        - |  2249 | `	}` |
|        - |  2250 | `	/* Reset the working buffer */` |
|       62 |  2251 | `	SyBlobReset(pWorker);` |
|        - |  2252 | `	/* Peek the processed file if available */` |
|       62 |  2253 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       62 |  2254 | `	if( pFile ){` |
|        - |  2255 | `		/* Append file name */` |
|       62 |  2256 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       62 |  2257 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       30 |  2258 | `	}` |
|       62 |  2259 | `	zErr = "Error: ";` |
|       62 |  2260 | `	switch(iErr){` |
|       27 |  2261 | `	case PH7_CTX_WARNING: zErr = "Warning: "; break;` |
|       14 |  2262 | `	case PH7_CTX_NOTICE:  zErr = "Notice: ";  break;` |
|       11 |  2263 | `	default:` |
|       23 |  2264 | `		iErr = PH7_CTX_ERR;` |
|       22 |  2265 | `		break;` |
|        - |  2266 | `	}` |
|       62 |  2267 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       62 |  2268 | `	if( pFuncName ){` |
|        - |  2269 | `		/* Append function name first */` |
|       29 |  2270 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       29 |  2271 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       14 |  2272 | `	}` |
|       62 |  2273 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2274 | `	/* Check for user error handler */` |
|       62 |  2275 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, pFile, 0) ){` |
|       53 |  2276 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       26 |  2277 | `	}` |
|       62 |  2278 | `	return rc;` |
|       33 |  2279 |  |
|        - |  2280 | `/*` |
|        - |  2281 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2282 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2283 | ` * information.` |
|        - |  2284 | ` */` |
|       26 |  2285 | `static sxi32 VmThrowErrorAp(` |
|        - |  2286 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2287 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2288 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2289 | `	const char *zFormat, /* Format message */` |
|        - |  2290 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2291 | `	)` |
|        2 |  2292 |  |
|       28 |  2293 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2294 | `	SyBlob sMsg;` |
|        - |  2295 | `	SyString *pFile;` |
|        - |  2296 | `	char *zErr;` |
|       28 |  2297 | `	sxi32 rc = SXRET_OK;` |
|       28 |  2298 | `	if( !pVm->bErrReport ){` |
|        - |  2299 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2300 | `		return SXRET_OK;` |
|        - |  2301 | `	}` |
|        - |  2302 | `	/* Reset the working buffer */` |
|       28 |  2303 | `	SyBlobReset(pWorker);` |
|        - |  2304 | `	/* Peek the processed file if available */` |
|       28 |  2305 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       28 |  2306 | `	if( pFile ){` |
|        - |  2307 | `		/* Append file name */` |
|       28 |  2308 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       28 |  2309 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       13 |  2310 | `	}` |
|       28 |  2311 | `	zErr = "Error: ";` |
|       28 |  2312 | `	switch(iErr){` |
|       10 |  2313 | `	case PH7_CTX_WARNING: zErr = "Warning: "; break;` |
|        7 |  2314 | `	case PH7_CTX_NOTICE:  zErr = "Notice: ";  break;` |
|        6 |  2315 | `	default:` |
|       13 |  2316 | `		iErr = PH7_CTX_ERR;` |
|       12 |  2317 | `		break;` |
|        - |  2318 | `	}` |
|       28 |  2319 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       28 |  2320 | `	if( pFuncName ){` |
|        - |  2321 | `		/* Append function name first */` |
|       14 |  2322 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       14 |  2323 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|        6 |  2324 | `	}` |
|        - |  2325 | `	/* Format the raw message */` |
|       28 |  2326 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       28 |  2327 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2328 | `	/* Check if a user error handler is installed */` |
|       28 |  2329 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), pFile, 0) ){` |
|        - |  2330 | `		/* No handler or handler returned TRUE, normal processing */` |
|       28 |  2331 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       28 |  2332 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2333 | `	}` |
|       28 |  2334 | `	SyBlobRelease(&sMsg);` |
|       28 |  2335 | `	return rc;` |
|       15 |  2336 |  |
|        - |  2337 | `/*` |
|        - |  2338 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2339 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2340 | ` * information.` |
|        - |  2341 | ` * ------------------------------------` |
|        - |  2342 | ` * Simple boring wrapper function.` |
|        - |  2343 | ` * ------------------------------------` |
|        - |  2344 | ` */` |
|       14 |  2345 | `static sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2346 |  |
|        - |  2347 | `	va_list ap;` |
|        - |  2348 | `	sxi32 rc;` |
|       15 |  2349 | `	va_start(ap,zFormat);` |
|       15 |  2350 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2351 | `	va_end(ap);` |
|       15 |  2352 | `	return rc;` |
|        1 |  2353 |  |
|        - |  2354 | `/*` |
|        - |  2355 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2356 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2357 | ` * information.` |
|        - |  2358 | ` * ------------------------------------` |
|        - |  2359 | ` * Simple boring wrapper function.` |
|        - |  2360 | ` * ------------------------------------` |
|        - |  2361 | ` */` |
|       12 |  2362 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2363 |  |
|        - |  2364 | `	sxi32 rc;` |
|       14 |  2365 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       14 |  2366 | `	return rc;` |
|        2 |  2367 |  |
|        - |  2368 | `/*` |
|        - |  2369 | ` * Resolve function context from the current frame.` |
|        - |  2370 | ` */` |
|       16 |  2371 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2372 |  |
|        - |  2373 | `	VmFrame *pFrame;` |
|        - |  2374 | `	ph7_vm_func *pFunc;` |
|       17 |  2375 | `	*pzFuncName = 0;` |
|       17 |  2376 | `	*pnFuncLen = 0;` |
|       17 |  2377 | `	pFrame = pVm->pFrame;` |
|       17 |  2378 | `	if( pFrame == 0 ){` |
|      ! 0 |  2379 | `		return;` |
|        - |  2380 | `	}` |
|       17 |  2381 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  2382 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  2383 | `	}` |
|       17 |  2384 | `	if( pFrame->pParent == 0 ){` |
|       17 |  2385 | `		return;` |
|        - |  2386 | `	}` |
|      ! 0 |  2387 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      ! 0 |  2388 | `	if( pFunc == 0 ){` |
|      ! 0 |  2389 | `		return;` |
|        - |  2390 | `	}` |
|      ! 0 |  2391 | `	*pzFuncName = pFunc->sName.zString;` |
|      ! 0 |  2392 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|        9 |  2393 |  |
|        - |  2394 | `/*` |
|        - |  2395 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2396 | ` */` |
|        8 |  2397 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2398 |  |
|        - |  2399 | `	SyBlob sOut;` |
|        - |  2400 | `	SyString *pFile;` |
|        9 |  2401 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2402 | `		return PH7_OK;` |
|        - |  2403 | `	}` |
|        9 |  2404 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2405 | `		zClass = "Exception";` |
|      ! 0 |  2406 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2407 | `	}` |
|        9 |  2408 | `	if( zMsg == 0 ){` |
|      ! 0 |  2409 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2410 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2411 | `	}` |
|        9 |  2412 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|        9 |  2413 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|        4 |  2414 | `	}` |
|        9 |  2415 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        9 |  2416 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|        9 |  2417 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|        9 |  2418 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|        9 |  2419 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|        9 |  2420 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|        9 |  2421 | `	if( pFile ){` |
|        9 |  2422 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|        9 |  2423 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|        9 |  2424 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|        4 |  2425 | `	}` |
|        9 |  2426 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|        9 |  2427 | `	if( pFile ){` |
|        9 |  2428 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|        9 |  2429 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|        9 |  2430 | `		if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2431 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2432 | `		}else{` |
|        9 |  2433 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2434 | `		}` |
|        4 |  2435 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2436 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2437 | `	}else{` |
|      ! 0 |  2438 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2439 | `	}` |
|        9 |  2440 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|        9 |  2441 | `	if( pFile ){` |
|        9 |  2442 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|        9 |  2443 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|        9 |  2444 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|        9 |  2445 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|        4 |  2446 | `	}` |
|        9 |  2447 | `	VmCallErrorHandler(pVm,&sOut);` |
|        9 |  2448 | `	SyBlobRelease(&sOut);` |
|        9 |  2449 | `	return PH7_ABORT;` |
|        5 |  2450 |  |
|        - |  2451 | `/*` |
|        - |  2452 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2453 | ` */` |
|        8 |  2454 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2455 |  |
|        - |  2456 | `	ph7_vm *pVm;` |
|        - |  2457 | `	ph7_class *pClass;` |
|        - |  2458 | `	ph7_class_instance *pThis;` |
|        - |  2459 | `	ph7_class_method *pCons;` |
|        - |  2460 | `	ph7_value sArg;` |
|        - |  2461 | `	ph7_value *apArg[1];` |
|        - |  2462 | `	SyBlob sMsg;` |
|        - |  2463 | `	SyString sMsgStr;` |
|        - |  2464 | `	VmFrame *pFrame;` |
|        - |  2465 | `	va_list ap;` |
|        - |  2466 | `	sxi32 rc;` |
|        - |  2467 |  |
|       10 |  2468 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2469 | `		return PH7_ABORT;` |
|        - |  2470 | `	}` |
|       10 |  2471 | `	pVm = pCtx->pVm;` |
|       10 |  2472 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2473 | `		zClass = "Error";` |
|      ! 0 |  2474 | `	}` |
|       10 |  2475 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|       10 |  2476 | `	if( pClass == 0 ){` |
|      ! 0 |  2477 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2478 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2479 | `			zClass` |
|        - |  2480 | `			);` |
|        - |  2481 | `	}` |
|       10 |  2482 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       10 |  2483 | `	if( pThis == 0 ){` |
|      ! 0 |  2484 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2485 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2486 | `			);` |
|        - |  2487 | `	}` |
|        - |  2488 |  |
|       10 |  2489 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       10 |  2490 | `	va_start(ap,zFormat);` |
|       10 |  2491 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|       10 |  2492 | `	va_end(ap);` |
|        - |  2493 |  |
|       10 |  2494 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       10 |  2495 | `	if( pCons ){` |
|       10 |  2496 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       10 |  2497 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       10 |  2498 | `		apArg[0] = &sArg;` |
|       10 |  2499 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       10 |  2500 | `		PH7_MemObjRelease(&sArg);` |
|        4 |  2501 | `	}` |
|       10 |  2502 | `	SyBlobRelease(&sMsg);` |
|        - |  2503 |  |
|       10 |  2504 | `	pFrame = pVm->pFrame;` |
|       10 |  2505 | `	if( pFrame ){` |
|       12 |  2506 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        3 |  2507 | `			pFrame = pFrame->pParent;` |
|        1 |  2508 | `		}` |
|       10 |  2509 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        4 |  2510 | `	}` |
|       10 |  2511 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       10 |  2512 | `	PH7_ClassInstanceUnref(pThis);` |
|       10 |  2513 | `	if( rc == SXERR_ABORT ){` |
|        7 |  2514 | `		return PH7_ABORT;` |
|        - |  2515 | `	}` |
|        3 |  2516 | `	return PH7_EXCEPTION;` |
|        6 |  2517 |  |
|        - |  2518 | `/*` |
|        - |  2519 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2520 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2521 | ` */` |
|      ! 0 |  2522 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2523 |  |
|        - |  2524 | `	ph7_vm *pVm;` |
|        - |  2525 | `	SyBlob sMsg;` |
|      ! 0 |  2526 | `	const char *zFuncName = 0;` |
|      ! 0 |  2527 | `	int nFuncLen = 0;` |
|        - |  2528 | `	va_list ap;` |
|        - |  2529 | `	sxi32 rc;` |
|        - |  2530 |  |
|      ! 0 |  2531 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2532 | `		return PH7_OK;` |
|        - |  2533 | `	}` |
|      ! 0 |  2534 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2535 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2536 | `		zClass = "Error";` |
|      ! 0 |  2537 | `	}` |
|        - |  2538 |  |
|      ! 0 |  2539 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2540 |  |
|      ! 0 |  2541 | `	va_start(ap,zFormat);` |
|      ! 0 |  2542 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2543 | `	va_end(ap);` |
|        - |  2544 |  |
|      ! 0 |  2545 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2546 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2547 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2548 | `	}` |
|      ! 0 |  2549 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2550 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2551 | `	}` |
|      ! 0 |  2552 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2553 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2554 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2555 | `	return rc;` |
|      ! 0 |  2556 |  |
|        - |  2557 | `/*` |
|        - |  2558 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2559 | ` *` |
|        - |  2560 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2561 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2562 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2563 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2564 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2565 | ` * then the program execution is halted.` |
|        - |  2566 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2567 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2568 | ` * or to reset the VM to it's initial state.` |
|        - |  2569 | ` */` |
|    19950 |  2570 | `static sxi32 VmByteCodeExec(` |
|        - |  2571 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2572 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2573 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2574 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2575 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2576 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2577 | `	int is_callback      /* TRUE if we are executing a callback */` |
|        - |  2578 | `	)` |
|        2 |  2579 |  |
|        - |  2580 | `	VmInstr *pInstr;` |
|        - |  2581 | `	ph7_value *pTos;` |
|        - |  2582 | `	SySet aArg;` |
|        - |  2583 | `	sxi32 pc;` |
|        - |  2584 | `	sxi32 rc;` |
|        - |  2585 | `	/* Argument container */` |
|    19952 |  2586 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    19952 |  2587 | `	if( nTos < 0 ){` |
|    19436 |  2588 | `		pTos = &pStack[-1];` |
|     9719 |  2589 | `	}else{` |
|      518 |  2590 | `		pTos = &pStack[nTos];` |
|        - |  2591 | `	}` |
|    19952 |  2592 | `	pc = 0;` |
|        - |  2593 | `	/* Execute as much as we can */` |
|  3299054 |  2594 | `	for(;;){` |
|        - |  2595 | `		/* Fetch the instruction to execute */` |
|  6597406 |  2596 | `		pInstr = &aInstr[pc];` |
|  6597406 |  2597 | `		rc = SXRET_OK;` |
|        - |  2598 | `/*` |
|        - |  2599 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2600 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2601 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2602 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2603 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2604 | ` */` |
|  6597406 |  2605 | `		switch(pInstr->iOp){` |
|        - |  2606 | `/*` |
|        - |  2607 | ` * DONE: P1 * *` |
|        - |  2608 | ` *` |
|        - |  2609 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2610 | ` * and return immediately.` |
|        - |  2611 | ` */` |
|     9964 |  2612 | `case PH7_OP_DONE:` |
|    19930 |  2613 | `	if( pInstr->iP1 ){` |
|        - |  2614 | `#ifdef UNTRUST` |
|        - |  2615 | `		if( pTos < pStack ){` |
|        - |  2616 | `			goto Abort;` |
|        - |  2617 | `		}` |
|        - |  2618 | `#endif` |
|    10920 |  2619 | `		if( pLastRef ){` |
|     7292 |  2620 | `			*pLastRef = pTos->nIdx;` |
|     3645 |  2621 | `		}` |
|    10920 |  2622 | `		if( pResult ){` |
|        - |  2623 | `			/* Execution result */` |
|    10626 |  2624 | `			PH7_MemObjStore(pTos,pResult);` |
|     5312 |  2625 | `		}` |
|    10920 |  2626 | `		VmPopOperand(&pTos,1);` |
|    14471 |  2627 | `	}else if( pLastRef ){` |
|        - |  2628 | `		/* Nothing referenced */` |
|      380 |  2629 | `		*pLastRef = SXU32_HIGH;` |
|      189 |  2630 | `	}` |
|    19930 |  2631 | `	goto Done;` |
|        - |  2632 | `/*` |
|        - |  2633 | ` * HALT: P1 * *` |
|        - |  2634 | ` *` |
|        - |  2635 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2636 | ` * and abort immediately.` |
|        - |  2637 | ` */` |
|        4 |  2638 | `case PH7_OP_HALT:` |
|        9 |  2639 | `	if( pInstr->iP1 ){` |
|        - |  2640 | `#ifdef UNTRUST` |
|        - |  2641 | `		if( pTos < pStack ){` |
|        - |  2642 | `			goto Abort;` |
|        - |  2643 | `		}` |
|        - |  2644 | `#endif` |
|        9 |  2645 | `		if( pLastRef ){` |
|      ! 0 |  2646 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2647 | `		}` |
|        9 |  2648 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2649 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2650 | `				/* Output the exit message */` |
|        7 |  2651 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2652 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2653 | `				if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  2654 | `					/* Increment output length */` |
|        5 |  2655 | `					pVm->nOutputLen += SyBlobLength(&pTos->sBlob);` |
|        2 |  2656 | `				}` |
|        3 |  2657 | `			}` |
|        7 |  2658 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2659 | `			/* Record exit status */` |
|        5 |  2660 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2661 | `		}` |
|        9 |  2662 | `		VmPopOperand(&pTos,1);` |
|        4 |  2663 | `	}else if( pLastRef ){` |
|        - |  2664 | `		/* Nothing referenced */` |
|      ! 0 |  2665 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2666 | `	}` |
|        - |  2667 | `	/* Check if we're in an included file context */` |
|        9 |  2668 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2669 | `		/* Terminate the entire process */` |
|        9 |  2670 | `		exit(pVm->iExitStatus);` |
|        - |  2671 | `	}` |
|      ! 0 |  2672 | `	goto Abort;` |
|        - |  2673 | `/*` |
|        - |  2674 | ` * JMP: * P2 *` |
|        - |  2675 | ` *` |
|        - |  2676 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2677 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2678 | ` */` |
|   149178 |  2679 | `case PH7_OP_JMP:` |
|   298402 |  2680 | `	pc = pInstr->iP2 - 1;` |
|   298402 |  2681 | `	break;` |
|        - |  2682 | `/*` |
|        - |  2683 | ` * JZ: P1 P2 *` |
|        - |  2684 | ` *` |
|        - |  2685 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2686 | ` * entry in the stack if P1 is zero.` |
|        - |  2687 | ` */` |
|   329744 |  2688 | `case PH7_OP_JZ:` |
|        - |  2689 | `#ifdef UNTRUST` |
|        - |  2690 | `	if( pTos < pStack ){` |
|        - |  2691 | `		goto Abort;` |
|        - |  2692 | `	}` |
|        - |  2693 | `#endif` |
|        - |  2694 | `	/* Get a boolean value */` |
|   659578 |  2695 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       77 |  2696 | `		PH7_MemObjToBool(pTos);` |
|       38 |  2697 | `	}` |
|   659578 |  2698 | `	if( !pTos->x.iVal ){` |
|        - |  2699 | `		/* Take the jump */` |
|   314232 |  2700 | `		pc = pInstr->iP2 - 1;` |
|   157115 |  2701 | `	}` |
|   659578 |  2702 | `	if( !pInstr->iP1 ){` |
|   514608 |  2703 | `		VmPopOperand(&pTos,1);` |
|   257325 |  2704 | `	}` |
|   659578 |  2705 | `	break;` |
|        - |  2706 | `/*` |
|        - |  2707 | ` * JNZ: P1 P2 *` |
|        - |  2708 | ` *` |
|        - |  2709 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2710 | ` * entry in the stack if P1 is zero.` |
|        - |  2711 | ` */` |
|    27715 |  2712 | `case PH7_OP_JNZ:` |
|        - |  2713 | `#ifdef UNTRUST` |
|        - |  2714 | `	if( pTos < pStack ){` |
|        - |  2715 | `		goto Abort;` |
|        - |  2716 | `	}` |
|        - |  2717 | `#endif` |
|        - |  2718 | `	/* Get a boolean value */` |
|    55432 |  2719 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2720 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2721 | `	}` |
|    55432 |  2722 | `	if( pTos->x.iVal ){` |
|        - |  2723 | `		/* Take the jump */` |
|     3068 |  2724 | `		pc = pInstr->iP2 - 1;` |
|     1533 |  2725 | `	}` |
|    55432 |  2726 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2727 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2728 | `	}` |
|    55432 |  2729 | `	break;` |
|        - |  2730 | `/*` |
|        - |  2731 | ` * NOOP: * * *` |
|        - |  2732 | ` *` |
|        - |  2733 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2734 | ` * destination.` |
|        - |  2735 | ` */` |
|      ! 0 |  2736 | `case PH7_OP_NOOP:` |
|      ! 0 |  2737 | `	break;` |
|        - |  2738 | `/*` |
|        - |  2739 | ` * POP: P1 * *` |
|        - |  2740 | ` *` |
|        - |  2741 | ` * Pop P1 elements from the operand stack.` |
|        - |  2742 | ` */` |
|   267526 |  2743 | `case PH7_OP_POP: {` |
|   535098 |  2744 | `	sxi32 n = pInstr->iP1;` |
|   535098 |  2745 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2746 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2747 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2748 | `	}` |
|   535098 |  2749 | `	VmPopOperand(&pTos,n);` |
|   535098 |  2750 | `	break;` |
|        - |  2751 | `				 }` |
|        - |  2752 | `/*` |
|        - |  2753 | ` * CVT_INT: * * *` |
|        - |  2754 | ` *` |
|        - |  2755 | ` * Force the top of the stack to be an integer.` |
|        - |  2756 | ` */` |
|       29 |  2757 | `case PH7_OP_CVT_INT:` |
|        - |  2758 | `#ifdef UNTRUST` |
|        - |  2759 | `	if( pTos < pStack ){` |
|        - |  2760 | `		goto Abort;` |
|        - |  2761 | `	}` |
|        - |  2762 | `#endif` |
|       60 |  2763 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       27 |  2764 | `		PH7_MemObjToInteger(pTos);` |
|       13 |  2765 | `	}` |
|        - |  2766 | `	/* Invalidate any prior representation */` |
|       60 |  2767 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       60 |  2768 | `	break;` |
|        - |  2769 | `/*` |
|        - |  2770 | ` * CVT_REAL: * * *` |
|        - |  2771 | ` *` |
|        - |  2772 | ` * Force the top of the stack to be a real.` |
|        - |  2773 | ` */` |
|        4 |  2774 | `case PH7_OP_CVT_REAL:` |
|        - |  2775 | `#ifdef UNTRUST` |
|        - |  2776 | `	if( pTos < pStack ){` |
|        - |  2777 | `		goto Abort;` |
|        - |  2778 | `	}` |
|        - |  2779 | `#endif` |
|        9 |  2780 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2781 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2782 | `	}` |
|        - |  2783 | `	/* Invalidate any prior representation */` |
|        9 |  2784 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2785 | `	break;` |
|        - |  2786 | `/*` |
|        - |  2787 | ` * CVT_STR: * * *` |
|        - |  2788 | ` *` |
|        - |  2789 | ` * Force the top of the stack to be a string.` |
|        - |  2790 | ` */` |
|      136 |  2791 | `case PH7_OP_CVT_STR:` |
|        - |  2792 | `#ifdef UNTRUST` |
|        - |  2793 | `	if( pTos < pStack ){` |
|        - |  2794 | `		goto Abort;` |
|        - |  2795 | `	}` |
|        - |  2796 | `#endif` |
|      274 |  2797 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      274 |  2798 | `		PH7_MemObjToString(pTos);` |
|      136 |  2799 | `	}` |
|      274 |  2800 | `	break;` |
|        - |  2801 | `/*` |
|        - |  2802 | ` * CVT_BOOL: * * *` |
|        - |  2803 | ` *` |
|        - |  2804 | ` * Force the top of the stack to be a boolean.` |
|        - |  2805 | ` */` |
|        5 |  2806 | `case PH7_OP_CVT_BOOL:` |
|        - |  2807 | `#ifdef UNTRUST` |
|        - |  2808 | `	if( pTos < pStack ){` |
|        - |  2809 | `		goto Abort;` |
|        - |  2810 | `	}` |
|        - |  2811 | `#endif` |
|       11 |  2812 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  2813 | `		PH7_MemObjToBool(pTos);` |
|        3 |  2814 | `	}` |
|       11 |  2815 | `	break;` |
|        - |  2816 | `/*` |
|        - |  2817 | ` * CVT_NULL: * * *` |
|        - |  2818 | ` *` |
|        - |  2819 | ` * Nullify the top of the stack.` |
|        - |  2820 | ` */` |
|        3 |  2821 | `case PH7_OP_CVT_NULL:` |
|        - |  2822 | `#ifdef UNTRUST` |
|        - |  2823 | `	if( pTos < pStack ){` |
|        - |  2824 | `		goto Abort;` |
|        - |  2825 | `	}` |
|        - |  2826 | `#endif` |
|        7 |  2827 | `	PH7_MemObjRelease(pTos);` |
|        7 |  2828 | `	break;` |
|        - |  2829 | `/*` |
|        - |  2830 | ` * CVT_NUMC: * * *` |
|        - |  2831 | ` *` |
|        - |  2832 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  2833 | ` */` |
|      ! 0 |  2834 | `case PH7_OP_CVT_NUMC:` |
|        - |  2835 | `#ifdef UNTRUST` |
|        - |  2836 | `	if( pTos < pStack ){` |
|        - |  2837 | `		goto Abort;` |
|        - |  2838 | `	}` |
|        - |  2839 | `#endif` |
|        - |  2840 | `	/* Force a numeric cast */` |
|      ! 0 |  2841 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  2842 | `	break;` |
|        - |  2843 | `/*` |
|        - |  2844 | ` * CVT_ARRAY: * * *` |
|        - |  2845 | ` *` |
|        - |  2846 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  2847 | ` */` |
|       10 |  2848 | `case PH7_OP_CVT_ARRAY:` |
|        - |  2849 | `#ifdef UNTRUST` |
|        - |  2850 | `	if( pTos < pStack ){` |
|        - |  2851 | `		goto Abort;` |
|        - |  2852 | `	}` |
|        - |  2853 | `#endif` |
|        - |  2854 | `	/* Force a hashmap cast */` |
|       21 |  2855 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  2856 | `	if( rc != SXRET_OK ){` |
|        - |  2857 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  2858 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  2859 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  2860 | `	}` |
|       21 |  2861 | `	break;` |
|        - |  2862 | `/*` |
|        - |  2863 | ` * CVT_OBJ: * * *` |
|        - |  2864 | ` *` |
|        - |  2865 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  2866 | ` */` |
|        8 |  2867 | `case PH7_OP_CVT_OBJ:` |
|        - |  2868 | `#ifdef UNTRUST` |
|        - |  2869 | `	if( pTos < pStack ){` |
|        - |  2870 | `		goto Abort;` |
|        - |  2871 | `	}` |
|        - |  2872 | `#endif` |
|       17 |  2873 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  2874 | `		/* Force a 'stdClass()' cast */` |
|       17 |  2875 | `		PH7_MemObjToObject(pTos);` |
|        8 |  2876 | `	}` |
|       17 |  2877 | `	break;` |
|        - |  2878 | `/*` |
|        - |  2879 | ` * ERR_CTRL * * *` |
|        - |  2880 | ` *` |
|        - |  2881 | ` * Error control operator.` |
|        - |  2882 | ` */` |
|     7673 |  2883 | `case PH7_OP_ERR_CTRL:` |
|        - |  2884 | `	/*` |
|        - |  2885 | `	 * TICKET 1433-038:` |
|        - |  2886 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2887 | `	 * use the public API,to control error output.` |
|        - |  2888 | `	 */` |
|    15346 |  2889 | `	break;` |
|        - |  2890 | `/*` |
|        - |  2891 | ` * IS_A * * *` |
|        - |  2892 | ` *` |
|        - |  2893 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  2894 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  2895 | ` * holding a class name or an object).` |
|        - |  2896 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  2897 | ` */` |
|       11 |  2898 | `case PH7_OP_IS_A:{` |
|       23 |  2899 | `	ph7_value *pNos = &pTos[-1];` |
|       23 |  2900 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  2901 | `#ifdef UNTRUST` |
|        - |  2902 | `	if( pNos < pStack ){` |
|        - |  2903 | `		goto Abort;` |
|        - |  2904 | `	}` |
|        - |  2905 | `#endif` |
|       23 |  2906 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       21 |  2907 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       21 |  2908 | `		ph7_class *pClass = 0;` |
|        - |  2909 | `		/* Extract the target class */` |
|       21 |  2910 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  2911 | `			/* Instance already loaded */` |
|      ! 0 |  2912 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       21 |  2913 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2914 | `			/* Perform the query */` |
|       31 |  2915 | `			pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|       20 |  2916 | `				SyBlobLength(&pTos->sBlob),FALSE,0);` |
|       10 |  2917 | `		}` |
|       21 |  2918 | `		if( pClass ){` |
|        - |  2919 | `			/* Perform the query */` |
|       21 |  2920 | `			iRes = VmInstanceOf(pThis->pClass,pClass);` |
|       10 |  2921 | `		}` |
|       10 |  2922 | `	}` |
|        - |  2923 | `	/* Push result */` |
|       23 |  2924 | `	VmPopOperand(&pTos,1);` |
|       23 |  2925 | `	PH7_MemObjRelease(pTos);` |
|       23 |  2926 | `	pTos->x.iVal = iRes;` |
|       23 |  2927 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       23 |  2928 | `	break;` |
|        - |  2929 | `				 }` |
|        - |  2930 |  |
|        - |  2931 | `/*` |
|        - |  2932 | ` * LOADC P1 P2 *` |
|        - |  2933 | ` *` |
|        - |  2934 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  2935 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  2936 | ` */` |
|   601963 |  2937 | `case PH7_OP_LOADC: {` |
|        - |  2938 | `	ph7_value *pObj;` |
|        - |  2939 | `	/* Reserve a room */` |
|  1203972 |  2940 | `	pTos++;` |
|  1203972 |  2941 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1203972 |  2942 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2943 | `			SyHashEntry *pEntry;` |
|        - |  2944 | `			/* Candidate for expansion via user defined callbacks */` |
|    11742 |  2945 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    11742 |  2946 | `			if( pEntry ){` |
|    10778 |  2947 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2948 | `				/* Set a NULL default value */` |
|    10778 |  2949 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    10778 |  2950 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  2951 | `				/* Invoke the callback and deal with the expanded value */` |
|    10778 |  2952 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  2953 | `				/* Mark as constant */` |
|    10778 |  2954 | `				pTos->nIdx = SXU32_HIGH;` |
|    10778 |  2955 | `				break;` |
|        - |  2956 | `			}` |
|      482 |  2957 | `		}` |
|  1193196 |  2958 | `		PH7_MemObjLoad(pObj,pTos);` |
|   596621 |  2959 | `	}else{` |
|        - |  2960 | `		/* Set a NULL value */` |
|      ! 0 |  2961 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2962 | `	}` |
|        - |  2963 | `	/* Mark as constant */` |
|  1193196 |  2964 | `	pTos->nIdx = SXU32_HIGH;` |
|  1193196 |  2965 | `	break;` |
|        - |  2966 | `				  }` |
|        - |  2967 | `/*` |
|        - |  2968 | ` * LOAD: P1 * P3` |
|        - |  2969 | ` *` |
|        - |  2970 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  2971 | ` * from the P3 operand.` |
|        - |  2972 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  2973 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  2974 | ` */` |
|   857058 |  2975 | `case PH7_OP_LOAD:{` |
|        - |  2976 | `	ph7_value *pObj;` |
|        - |  2977 | `	SyString sName;` |
|  1714338 |  2978 | `	if( pInstr->p3 == 0 ){` |
|        - |  2979 | `		/* Take the variable name from the top of the stack */` |
|        - |  2980 | `#ifdef UNTRUST` |
|        - |  2981 | `		if( pTos < pStack ){` |
|        - |  2982 | `			goto Abort;` |
|        - |  2983 | `		}` |
|        - |  2984 | `#endif` |
|        - |  2985 | `		/* Force a string cast */` |
|       25 |  2986 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2987 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  2988 | `		}` |
|       25 |  2989 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       13 |  2990 | `	}else{` |
|  1714314 |  2991 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  2992 | `		/* Reserve a room for the target object */` |
|  1714314 |  2993 | `		pTos++;` |
|        - |  2994 | `	}` |
|        - |  2995 | `	/* Extract the requested memory object */` |
|  1714338 |  2996 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  1714338 |  2997 | `	if( pObj == 0 ){` |
|      456 |  2998 | `		if( pInstr->iP1 ){` |
|        - |  2999 | `			/* Variable not found,load NULL */` |
|      456 |  3000 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3001 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3002 | `			}else{` |
|      456 |  3003 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3004 | `			}` |
|      456 |  3005 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|   857287 |  3006 | `			break;` |
|      ! 0 |  3007 | `		}else{` |
|        - |  3008 | `			/* Fatal error */` |
|      ! 0 |  3009 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3010 | `			goto Abort;` |
|        - |  3011 | `		}` |
|        - |  3012 | `	}` |
|        - |  3013 | `	/* Load variable contents */` |
|  1713884 |  3014 | `	PH7_MemObjLoad(pObj,pTos);` |
|  1713884 |  3015 | `	pTos->nIdx = pObj->nIdx;` |
|  1713884 |  3016 | `	break;` |
|        - |  3017 | `				   }` |
|        - |  3018 | `/*` |
|        - |  3019 | ` * LOAD_MAP P1 * *` |
|        - |  3020 | ` *` |
|        - |  3021 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3022 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3023 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3024 | ` */` |
|    12988 |  3025 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3026 | `	ph7_hashmap *pMap;` |
|        - |  3027 | `	/* Allocate a new hashmap instance */` |
|    25978 |  3028 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    25978 |  3029 | `	if( pMap == 0 ){` |
|      ! 0 |  3030 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3031 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3032 | `		goto Abort;` |
|        - |  3033 | `	}` |
|    25978 |  3034 | `	if( pInstr->iP1 > 0 ){` |
|     1414 |  3035 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3036 | `		/* Perform the insertion */` |
|     3994 |  3037 | `		while( pEntry < pTos ){` |
|     2582 |  3038 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3039 | `				/* Insertion by reference */` |
|      142 |  3040 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3041 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3042 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3043 | `					);` |
|       48 |  3044 | `			}else{` |
|        - |  3045 | `				/* Standard insertion */` |
|     3731 |  3046 | `				PH7_HashmapInsert(pMap,` |
|     2486 |  3047 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     1243 |  3048 | `					&pEntry[1]` |
|        - |  3049 | `				);` |
|        - |  3050 | `			}` |
|        - |  3051 | `			/* Next pair on the stack */` |
|     2582 |  3052 | `			pEntry += 2;` |
|        2 |  3053 | `		}` |
|        - |  3054 | `		/* Pop P1 elements */` |
|     1414 |  3055 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|      706 |  3056 | `	}` |
|        - |  3057 | `	/* Push the hashmap */` |
|    25978 |  3058 | `	pTos++;` |
|    25978 |  3059 | `	pTos->nIdx = SXU32_HIGH;` |
|    25978 |  3060 | `	pTos->x.pOther = pMap;` |
|    25978 |  3061 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    25978 |  3062 | `	break;` |
|        - |  3063 | `					  }` |
|        - |  3064 | `/*` |
|        - |  3065 | ` * LOAD_LIST: P1 * *` |
|        - |  3066 | ` *` |
|        - |  3067 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3068 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3069 | ` * Caveats:` |
|        - |  3070 | ` *  This implementation support only a single nesting level.` |
|        - |  3071 | ` */` |
|       17 |  3072 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3073 | `	ph7_value *pEntry;` |
|       35 |  3074 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3075 | `		/* Empty list,break immediately */` |
|      ! 0 |  3076 | `		break;` |
|        - |  3077 | `	}` |
|       35 |  3078 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3079 | `#ifdef UNTRUST` |
|        - |  3080 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3081 | `		goto Abort;` |
|        - |  3082 | `	}` |
|        - |  3083 | `#endif` |
|       35 |  3084 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       31 |  3085 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3086 | `		ph7_hashmap_node *pNode;` |
|        - |  3087 | `		ph7_value sKey,*pObj;` |
|        - |  3088 | `		/* Start Copying */` |
|       31 |  3089 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|       99 |  3090 | `		while( pEntry <= pTos ){` |
|       69 |  3091 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       65 |  3092 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       65 |  3093 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       65 |  3094 | `					if( rc == SXRET_OK ){` |
|        - |  3095 | `						/* Store node value */` |
|       65 |  3096 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       33 |  3097 | `					}else{` |
|        - |  3098 | `						/* Nullify the variable */` |
|      ! 0 |  3099 | `						PH7_MemObjRelease(pObj);` |
|        - |  3100 | `					}` |
|       32 |  3101 | `				}` |
|       32 |  3102 | `			}` |
|       69 |  3103 | `			sKey.x.iVal++; /* Next numeric index */` |
|       69 |  3104 | `			pEntry++;` |
|        1 |  3105 | `		}` |
|       15 |  3106 | `	}` |
|       35 |  3107 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       35 |  3108 | `	break;` |
|        - |  3109 | `					   }` |
|        - |  3110 | `/*` |
|        - |  3111 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3112 | ` *` |
|        - |  3113 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3114 | ` * from the stack.` |
|        - |  3115 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3116 | ` * instead.` |
|        - |  3117 | ` */` |
|   120251 |  3118 | `case PH7_OP_LOAD_IDX: {` |
|   240548 |  3119 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   240548 |  3120 | `	ph7_hashmap *pMap = 0;` |
|        - |  3121 | `	ph7_value *pIdx;` |
|   240548 |  3122 | `	pIdx = 0;` |
|   240548 |  3123 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3124 | `		if( !pInstr->iP2){` |
|        - |  3125 | `			/* No available index,load NULL */` |
|      ! 0 |  3126 | `			if( pTos >= pStack ){` |
|      ! 0 |  3127 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3128 | `			}else{` |
|        - |  3129 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3130 | `				pTos++;` |
|      ! 0 |  3131 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3132 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3133 | `			}` |
|        - |  3134 | `			/* Emit a notice */` |
|      ! 0 |  3135 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3136 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3137 | `			break;` |
|        - |  3138 | `		}` |
|      ! 0 |  3139 | `	}else{` |
|   240548 |  3140 | `		pIdx = pTos;` |
|   240548 |  3141 | `		pTos--;` |
|        - |  3142 | `	}` |
|   240548 |  3143 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3144 | `		/* String access */` |
|   176002 |  3145 | `		if( pIdx ){` |
|        - |  3146 | `			sxu32 nOfft;` |
|   176002 |  3147 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3148 | `				/* Force an int cast */` |
|      ! 0 |  3149 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3150 | `			}` |
|   176002 |  3151 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   176002 |  3152 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3153 | `				/* Invalid offset,load null */` |
|      ! 0 |  3154 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3155 | `			}else{` |
|   176002 |  3156 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   176002 |  3157 | `				int c = zData[nOfft];` |
|   176002 |  3158 | `				PH7_MemObjRelease(pTos);` |
|   176002 |  3159 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   176002 |  3160 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3161 | `			}` |
|    88024 |  3162 | `		}else{` |
|        - |  3163 | `			/* No available index,load NULL */` |
|      ! 0 |  3164 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3165 | `		}` |
|   176002 |  3166 | `		break;` |
|        - |  3167 | `	}` |
|    64548 |  3168 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3169 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3170 | `			ph7_value *pObj;` |
|      ! 0 |  3171 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3172 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3173 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3174 | `			}` |
|      ! 0 |  3175 | `		}` |
|      ! 0 |  3176 | `	}` |
|    64548 |  3177 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    64548 |  3178 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3179 | `		/* Point to the hashmap */` |
|    64548 |  3180 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    64548 |  3181 | `		if( pIdx ){` |
|        - |  3182 | `			/* Load the desired entry */` |
|    64548 |  3183 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    32273 |  3184 | `		}` |
|    64548 |  3185 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3186 | `			/* Create a new empty entry */` |
|      ! 0 |  3187 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3188 | `			if( rc == SXRET_OK ){` |
|        - |  3189 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3190 | `				pNode = pMap->pLast;` |
|      ! 0 |  3191 | `			}` |
|      ! 0 |  3192 | `		}` |
|    32273 |  3193 | `	}` |
|    64548 |  3194 | `	if( pIdx ){` |
|    64548 |  3195 | `		PH7_MemObjRelease(pIdx);` |
|    32273 |  3196 | `	}` |
|    64548 |  3197 | `	if( rc == SXRET_OK ){` |
|        - |  3198 | `		/* Load entry contents */` |
|    31382 |  3199 | `		if( pMap->iRef < 2 ){` |
|        - |  3200 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3201 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3202 | `			 */` |
|      ! 0 |  3203 | `			pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  3204 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|      ! 0 |  3205 | `		}else{` |
|    31382 |  3206 | `			pTos->nIdx = pNode->nValIdx;` |
|    31382 |  3207 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    31382 |  3208 | `			PH7_HashmapUnref(pMap);` |
|        - |  3209 | `		}` |
|    15692 |  3210 | `	}else{` |
|        - |  3211 | `		/* No such entry,load NULL */` |
|    33168 |  3212 | `		PH7_MemObjRelease(pTos);` |
|    33168 |  3213 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3214 | `	}` |
|    64548 |  3215 | `	break;` |
|        - |  3216 | `					  }` |
|        - |  3217 | `/*` |
|        - |  3218 | ` * LOAD_CLOSURE * * P3` |
|        - |  3219 | ` *` |
|        - |  3220 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3221 | ` * name in the stack.` |
|        - |  3222 | ` */` |
|        2 |  3223 | `case PH7_OP_LOAD_CLOSURE:{` |
|        5 |  3224 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        5 |  3225 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3226 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3227 | `		ph7_vm_func *pClosure;` |
|        - |  3228 | `		char *zName;` |
|        - |  3229 | `		sxu32 mLen;` |
|        - |  3230 | `		sxu32 n;` |
|        - |  3231 | `		/* Create a new VM function */` |
|        5 |  3232 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3233 | `		/* Generate an unique closure name */` |
|        5 |  3234 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        5 |  3235 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3236 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3237 | `			goto Abort;` |
|        - |  3238 | `		}` |
|        5 |  3239 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        5 |  3240 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3241 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3242 | `		}` |
|        - |  3243 | `		/* Zero the stucture */` |
|        5 |  3244 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3245 | `		/* Perform a structure assignment on read-only items */` |
|        5 |  3246 | `		pClosure->aArgs = pFunc->aArgs;` |
|        5 |  3247 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        5 |  3248 | `		pClosure->aStatic = pFunc->aStatic;` |
|        5 |  3249 | `		pClosure->iFlags = pFunc->iFlags;` |
|        5 |  3250 | `		pClosure->pUserData = pFunc->pUserData;` |
|        5 |  3251 | `		pClosure->sSignature = pFunc->sSignature;` |
|        5 |  3252 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3253 | `		/* Register the closure */` |
|        5 |  3254 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3255 | `		/* Set up closure environment */` |
|        5 |  3256 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        5 |  3257 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       13 |  3258 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3259 | `			ph7_value *pValue;` |
|        9 |  3260 | `			pEnv = &aEnv[n];` |
|        9 |  3261 | `			sEnv.sName  = pEnv->sName;` |
|        9 |  3262 | `			sEnv.iFlags = pEnv->iFlags;` |
|        9 |  3263 | `			sEnv.nIdx = SXU32_HIGH;` |
|        9 |  3264 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|        9 |  3265 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3266 | `				/* Pass by reference */` |
|      ! 0 |  3267 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3268 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3269 | `					);` |
|      ! 0 |  3270 | `			}` |
|        - |  3271 | `			/* Standard pass by value */` |
|        9 |  3272 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|        9 |  3273 | `			if( pValue ){` |
|        - |  3274 | `				/* Copy imported value */` |
|        5 |  3275 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        2 |  3276 | `			}` |
|        - |  3277 | `			/* Insert the imported variable */` |
|        9 |  3278 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|        5 |  3279 | `		}` |
|        - |  3280 | `		/* Finally,load the closure name on the stack */` |
|        5 |  3281 | `		pTos++;` |
|        5 |  3282 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        2 |  3283 | `	}` |
|        5 |  3284 | `	break;` |
|        - |  3285 | `						 }` |
|        - |  3286 | `/*` |
|        - |  3287 | ` * STORE * P2 P3` |
|        - |  3288 | ` *` |
|        - |  3289 | ` * Perform a store (Assignment) operation.` |
|        - |  3290 | ` */` |
|    77974 |  3291 | `case PH7_OP_STORE: {` |
|        - |  3292 | `	ph7_value *pObj;` |
|        - |  3293 | `	SyString sName;` |
|        - |  3294 | `#ifdef UNTRUST` |
|        - |  3295 | `	if( pTos < pStack ){` |
|        - |  3296 | `		goto Abort;` |
|        - |  3297 | `	}` |
|        - |  3298 | `#endif` |
|   155950 |  3299 | `	if( pInstr->iP2 ){` |
|        - |  3300 | `		sxu32 nIdx;` |
|        - |  3301 | `		/* Member store operation */` |
|      508 |  3302 | `		nIdx = pTos->nIdx;` |
|      508 |  3303 | `		VmPopOperand(&pTos,1);` |
|      508 |  3304 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3305 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3306 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3307 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3308 | `		}else{` |
|        - |  3309 | `			/* Point to the desired memory object */` |
|      504 |  3310 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      504 |  3311 | `			if( pObj ){` |
|        - |  3312 | `				/* Perform the store operation */` |
|      504 |  3313 | `				PH7_MemObjStore(pTos,pObj);` |
|      251 |  3314 | `			}` |
|        - |  3315 | `		}` |
|    78229 |  3316 | `		break;` |
|   155444 |  3317 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3318 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3319 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3320 | `			/* Force a string cast */` |
|      ! 0 |  3321 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3322 | `		}` |
|        7 |  3323 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3324 | `		pTos--;` |
|        - |  3325 | `#ifdef UNTRUST` |
|        - |  3326 | `		if( pTos < pStack  ){` |
|        - |  3327 | `			goto Abort;` |
|        - |  3328 | `		}` |
|        - |  3329 | `#endif` |
|        4 |  3330 | `	}else{` |
|   155438 |  3331 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3332 | `	}` |
|        - |  3333 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   155444 |  3334 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   155444 |  3335 | `	if( pObj == 0 ){` |
|      ! 0 |  3336 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3337 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3338 | `		goto Abort;` |
|        - |  3339 | `	}` |
|   155444 |  3340 | `	if( !pInstr->p3 ){` |
|        7 |  3341 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3342 | `	}` |
|        - |  3343 | `	/* Perform the store operation */` |
|   155444 |  3344 | `	PH7_MemObjStore(pTos,pObj);` |
|   155444 |  3345 | `	break;` |
|        - |  3346 | `				   }` |
|        - |  3347 | `/*` |
|        - |  3348 | ` * STORE_IDX:   P1 * P3` |
|        - |  3349 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3350 | ` *` |
|        - |  3351 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3352 | ` */` |
|    68070 |  3353 | `case PH7_OP_STORE_IDX:` |
|        - |  3354 | `case PH7_OP_STORE_IDX_REF: {` |
|   136142 |  3355 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3356 | `	ph7_value *pKey;` |
|        - |  3357 | `	sxu32 nIdx;` |
|   136142 |  3358 | `	if( pInstr->iP1 ){` |
|        - |  3359 | `		/* Key is next on stack */` |
|    50634 |  3360 | `		pKey = pTos;` |
|    50634 |  3361 | `		pTos--;` |
|    25318 |  3362 | `	}else{` |
|    85510 |  3363 | `		pKey = 0;` |
|        - |  3364 | `	}` |
|   136142 |  3365 | `	nIdx = pTos->nIdx;` |
|   136142 |  3366 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3367 | `		/* Hashmap already loaded */` |
|   136090 |  3368 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   136090 |  3369 | `		if( pMap->iRef < 2 ){` |
|        - |  3370 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3371 | `			pMap->iRef = 2;` |
|      ! 0 |  3372 | `		}` |
|    68046 |  3373 | `	}else{` |
|        - |  3374 | `		ph7_value *pObj;` |
|       53 |  3375 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3376 | `		if( pObj == 0 ){` |
|      ! 0 |  3377 | `			if( pKey ){` |
|      ! 0 |  3378 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3379 | `			}` |
|      ! 0 |  3380 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3381 | `			break;` |
|        - |  3382 | `		}` |
|        - |  3383 | `		/* Phase#1: Load the array */` |
|       53 |  3384 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3385 | `			VmPopOperand(&pTos,1);` |
|       53 |  3386 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3387 | `				/* Force a string cast */` |
|      ! 0 |  3388 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3389 | `			}` |
|       53 |  3390 | `			if( pKey == 0 ){` |
|        - |  3391 | `				/* Append string */` |
|        3 |  3392 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3393 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3394 | `				}` |
|        2 |  3395 | `			}else{` |
|        - |  3396 | `				sxu32 nOfft;` |
|       51 |  3397 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3398 | `					/* Force an int cast */` |
|       51 |  3399 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3400 | `				}` |
|       51 |  3401 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3402 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3403 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3404 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3405 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3406 | `				}else{` |
|      ! 0 |  3407 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3408 | `						/* Perform an append operation */` |
|      ! 0 |  3409 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3410 | `					}` |
|        - |  3411 | `				}` |
|        - |  3412 | `			}` |
|       53 |  3413 | `			if( pKey ){` |
|       51 |  3414 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3415 | `			}` |
|       53 |  3416 | `			break;` |
|      ! 0 |  3417 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3418 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3419 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3420 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3421 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3422 | `				goto Abort;` |
|        - |  3423 | `			}` |
|      ! 0 |  3424 | `		}` |
|      ! 0 |  3425 | `		pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - |  3426 | `	}` |
|   136090 |  3427 | `	VmPopOperand(&pTos,1);` |
|        - |  3428 | `	/* Phase#2: Perform the insertion */` |
|   136090 |  3429 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3430 | `		/* Insertion by reference */` |
|       13 |  3431 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        7 |  3432 | `	}else{` |
|   136078 |  3433 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3434 | `	}` |
|   136090 |  3435 | `	if( pKey ){` |
|    50584 |  3436 | `		PH7_MemObjRelease(pKey);` |
|    25291 |  3437 | `	}` |
|   136090 |  3438 | `	break;` |
|        - |  3439 | `					   }` |
|        - |  3440 | `/*` |
|        - |  3441 | ` * INCR: P1 * *` |
|        - |  3442 | ` *` |
|        - |  3443 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3444 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3445 | ` * the stack and increment after that.` |
|        - |  3446 | ` */` |
|    93064 |  3447 | `case PH7_OP_INCR:` |
|        - |  3448 | `#ifdef UNTRUST` |
|        - |  3449 | `	if( pTos < pStack ){` |
|        - |  3450 | `		goto Abort;` |
|        - |  3451 | `	}` |
|        - |  3452 | `#endif` |
|   186174 |  3453 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   186174 |  3454 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3455 | `			ph7_value *pObj;` |
|   186174 |  3456 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3457 | `				/* Force a numeric cast */` |
|   186174 |  3458 | `				PH7_MemObjToNumeric(pObj);` |
|   186174 |  3459 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3460 | `					pObj->rVal++;` |
|        - |  3461 | `					/* Try to get an integer representation */` |
|      ! 0 |  3462 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3463 | `				}else{` |
|   186174 |  3464 | `					pObj->x.iVal++;` |
|   186174 |  3465 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3466 | `				}` |
|   186174 |  3467 | `				if( pInstr->iP1 ){` |
|        - |  3468 | `					/* Pre-icrement */` |
|       55 |  3469 | `					PH7_MemObjStore(pObj,pTos);` |
|       27 |  3470 | `				}` |
|    93108 |  3471 | `			}` |
|    93110 |  3472 | `		}else{` |
|      ! 0 |  3473 | `			if( pInstr->iP1 ){` |
|        - |  3474 | `				/* Force a numeric cast */` |
|      ! 0 |  3475 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3476 | `				/* Pre-increment */` |
|      ! 0 |  3477 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3478 | `					pTos->rVal++;` |
|        - |  3479 | `					/* Try to get an integer representation */` |
|      ! 0 |  3480 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3481 | `				}else{` |
|      ! 0 |  3482 | `					pTos->x.iVal++;` |
|      ! 0 |  3483 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3484 | `				}` |
|      ! 0 |  3485 | `			}` |
|        - |  3486 | `		}` |
|    93108 |  3487 | `	}` |
|   186174 |  3488 | `	break;` |
|        - |  3489 | `/*` |
|        - |  3490 | ` * DECR: P1 * *` |
|        - |  3491 | ` *` |
|        - |  3492 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3493 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3494 | ` * and decrement after that.` |
|        - |  3495 | ` */` |
|        2 |  3496 | `case PH7_OP_DECR:` |
|        - |  3497 | `#ifdef UNTRUST` |
|        - |  3498 | `	if( pTos < pStack ){` |
|        - |  3499 | `		goto Abort;` |
|        - |  3500 | `	}` |
|        - |  3501 | `#endif` |
|        5 |  3502 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3503 | `		/* Force a numeric cast */` |
|        5 |  3504 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3505 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3506 | `			ph7_value *pObj;` |
|        5 |  3507 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3508 | `				/* Force a numeric cast */` |
|        5 |  3509 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3510 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3511 | `					pObj->rVal--;` |
|        - |  3512 | `					/* Try to get an integer representation */` |
|      ! 0 |  3513 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3514 | `				}else{` |
|        5 |  3515 | `					pObj->x.iVal--;` |
|        5 |  3516 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3517 | `				}` |
|        5 |  3518 | `				if( pInstr->iP1 ){` |
|        - |  3519 | `					/* Pre-icrement */` |
|      ! 0 |  3520 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3521 | `				}` |
|        2 |  3522 | `			}` |
|        3 |  3523 | `		}else{` |
|      ! 0 |  3524 | `			if( pInstr->iP1 ){` |
|        - |  3525 | `				/* Pre-increment */` |
|      ! 0 |  3526 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3527 | `					pTos->rVal--;` |
|        - |  3528 | `					/* Try to get an integer representation */` |
|      ! 0 |  3529 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3530 | `				}else{` |
|      ! 0 |  3531 | `					pTos->x.iVal--;` |
|      ! 0 |  3532 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3533 | `				}` |
|      ! 0 |  3534 | `			}` |
|        - |  3535 | `		}` |
|        2 |  3536 | `	}` |
|        5 |  3537 | `	break;` |
|        - |  3538 | `/*` |
|        - |  3539 | ` * UMINUS: * * *` |
|        - |  3540 | ` *` |
|        - |  3541 | ` * Perform a unary minus operation.` |
|        - |  3542 | ` */` |
|    16952 |  3543 | `case PH7_OP_UMINUS:` |
|        - |  3544 | `#ifdef UNTRUST` |
|        - |  3545 | `	if( pTos < pStack ){` |
|        - |  3546 | `		goto Abort;` |
|        - |  3547 | `	}` |
|        - |  3548 | `#endif` |
|        - |  3549 | `	/* Force a numeric (integer,real or both) cast */` |
|    33906 |  3550 | `	PH7_MemObjToNumeric(pTos);` |
|    33906 |  3551 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       19 |  3552 | `		pTos->rVal = -pTos->rVal;` |
|        9 |  3553 | `	}` |
|    33906 |  3554 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    33888 |  3555 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    16943 |  3556 | `	}` |
|    33906 |  3557 | `	break;` |
|        - |  3558 | `/*` |
|        - |  3559 | ` * UPLUS: * * *` |
|        - |  3560 | ` *` |
|        - |  3561 | ` * Perform a unary plus operation.` |
|        - |  3562 | ` */` |
|       16 |  3563 | `case PH7_OP_UPLUS:` |
|        - |  3564 | `#ifdef UNTRUST` |
|        - |  3565 | `	if( pTos < pStack ){` |
|        - |  3566 | `		goto Abort;` |
|        - |  3567 | `	}` |
|        - |  3568 | `#endif` |
|        - |  3569 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3570 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3571 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3572 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3573 | `	}` |
|       33 |  3574 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3575 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3576 | `	}` |
|       33 |  3577 | `	break;` |
|        - |  3578 | `/*` |
|        - |  3579 | ` * OP_LNOT: * * *` |
|        - |  3580 | ` *` |
|        - |  3581 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3582 | ` * with its complement.` |
|        - |  3583 | ` */` |
|    26597 |  3584 | `case PH7_OP_LNOT:` |
|        - |  3585 | `#ifdef UNTRUST` |
|        - |  3586 | `	if( pTos < pStack ){` |
|        - |  3587 | `		goto Abort;` |
|        - |  3588 | `	}` |
|        - |  3589 | `#endif` |
|        - |  3590 | `	/* Force a boolean cast */` |
|    53240 |  3591 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3592 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3593 | `	}` |
|    53240 |  3594 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    53240 |  3595 | `	break;` |
|        - |  3596 | `/*` |
|        - |  3597 | ` * OP_BITNOT: * * *` |
|        - |  3598 | ` *` |
|        - |  3599 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3600 | ` * with its ones-complement.` |
|        - |  3601 | ` */` |
|        3 |  3602 | `case PH7_OP_BITNOT:` |
|        - |  3603 | `#ifdef UNTRUST` |
|        - |  3604 | `	if( pTos < pStack ){` |
|        - |  3605 | `		goto Abort;` |
|        - |  3606 | `	}` |
|        - |  3607 | `#endif` |
|        - |  3608 | `	/* Force an integer cast */` |
|        7 |  3609 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3610 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3611 | `	}` |
|        7 |  3612 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|        7 |  3613 | `	break;` |
|        - |  3614 | `/* OP_MUL * * *` |
|        - |  3615 | ` * OP_MUL_STORE * * *` |
|        - |  3616 | ` *` |
|        - |  3617 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3618 | ` * and push the result back onto the stack.` |
|        - |  3619 | ` */` |
|     1231 |  3620 | `case PH7_OP_MUL:` |
|        - |  3621 | `case PH7_OP_MUL_STORE: {` |
|     2464 |  3622 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3623 | `	/* Force the operand to be numeric */` |
|        - |  3624 | `#ifdef UNTRUST` |
|        - |  3625 | `	if( pNos < pStack ){` |
|        - |  3626 | `		goto Abort;` |
|        - |  3627 | `	}` |
|        - |  3628 | `#endif` |
|     2464 |  3629 | `	PH7_MemObjToNumeric(pTos);` |
|     2464 |  3630 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3631 | `	/* Perform the requested operation */` |
|     2464 |  3632 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3633 | `		/* Floating point arithemic */` |
|        - |  3634 | `		ph7_real a,b,r;` |
|       17 |  3635 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3636 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3637 | `		}` |
|       17 |  3638 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3639 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3640 | `		}` |
|       17 |  3641 | `		a = pNos->rVal;` |
|       17 |  3642 | `		b = pTos->rVal;` |
|       17 |  3643 | `		r = a * b;` |
|        - |  3644 | `		/* Push the result */` |
|       17 |  3645 | `		pNos->rVal = r;` |
|       17 |  3646 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3647 | `		/* Try to get an integer representation */` |
|       17 |  3648 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3649 | `	}else{` |
|        - |  3650 | `		/* Integer arithmetic */` |
|        - |  3651 | `		sxi64 a,b,r;` |
|     2448 |  3652 | `		a = pNos->x.iVal;` |
|     2448 |  3653 | `		b = pTos->x.iVal;` |
|     2448 |  3654 | `		r = a * b;` |
|        - |  3655 | `		/* Push the result */` |
|     2448 |  3656 | `		pNos->x.iVal = r;` |
|     2448 |  3657 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3658 | `	}` |
|     2464 |  3659 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3660 | `		ph7_value *pObj;` |
|       19 |  3661 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3662 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  3663 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  3664 | `			PH7_MemObjStore(pNos,pObj);` |
|        9 |  3665 | `		}` |
|        9 |  3666 | `	}` |
|     2464 |  3667 | `	VmPopOperand(&pTos,1);` |
|     2464 |  3668 | `	break;` |
|        - |  3669 | `				 }` |
|        - |  3670 | `/* OP_ADD * * *` |
|        - |  3671 | ` *` |
|        - |  3672 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3673 | ` * and push the result back onto the stack.` |
|        - |  3674 | ` */` |
|      424 |  3675 | `case PH7_OP_ADD:{` |
|      850 |  3676 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3677 | `#ifdef UNTRUST` |
|        - |  3678 | `	if( pNos < pStack ){` |
|        - |  3679 | `		goto Abort;` |
|        - |  3680 | `	}` |
|        - |  3681 | `#endif` |
|        - |  3682 | `	/* Perform the addition */` |
|      850 |  3683 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      850 |  3684 | `	VmPopOperand(&pTos,1);` |
|      850 |  3685 | `	break;` |
|        - |  3686 | `				}` |
|        - |  3687 | `/*` |
|        - |  3688 | ` * OP_ADD_STORE * * *` |
|        - |  3689 | ` *` |
|        - |  3690 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3691 | ` * and push the result back onto the stack.` |
|        - |  3692 | ` */` |
|      481 |  3693 | `case PH7_OP_ADD_STORE:{` |
|      963 |  3694 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3695 | `	ph7_value *pObj;` |
|        - |  3696 | `	sxu32 nIdx;` |
|        - |  3697 | `#ifdef UNTRUST` |
|        - |  3698 | `	if( pNos < pStack ){` |
|        - |  3699 | `		goto Abort;` |
|        - |  3700 | `	}` |
|        - |  3701 | `#endif` |
|        - |  3702 | `	/* Perform the addition */` |
|      963 |  3703 | `	nIdx = pTos->nIdx;` |
|      963 |  3704 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3705 | `	/* Peform the store operation */` |
|      963 |  3706 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3707 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      963 |  3708 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      963 |  3709 | `		PH7_MemObjStore(pTos,pObj);` |
|      481 |  3710 | `	}` |
|        - |  3711 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      963 |  3712 | `	PH7_MemObjStore(pTos,pNos);` |
|      963 |  3713 | `	VmPopOperand(&pTos,1);` |
|      963 |  3714 | `	break;` |
|        - |  3715 | `				}` |
|        - |  3716 | `/* OP_SUB * * *` |
|        - |  3717 | ` *` |
|        - |  3718 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3719 | ` * first (what was next on the stack) from the second (the` |
|        - |  3720 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3721 | ` */` |
|      280 |  3722 | `case PH7_OP_SUB: {` |
|      561 |  3723 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3724 | `#ifdef UNTRUST` |
|        - |  3725 | `	if( pNos < pStack ){` |
|        - |  3726 | `		goto Abort;` |
|        - |  3727 | `	}` |
|        - |  3728 | `#endif` |
|      561 |  3729 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3730 | `		/* Floating point arithemic */` |
|        - |  3731 | `		ph7_real a,b,r;` |
|       73 |  3732 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3733 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3734 | `		}` |
|       73 |  3735 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3736 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3737 | `		}` |
|       73 |  3738 | `		a = pNos->rVal;` |
|       73 |  3739 | `		b = pTos->rVal;` |
|       73 |  3740 | `		r = a - b;` |
|        - |  3741 | `		/* Push the result */` |
|       73 |  3742 | `		pNos->rVal = r;` |
|       73 |  3743 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3744 | `		/* Try to get an integer representation */` |
|       73 |  3745 | `		PH7_MemObjTryInteger(pNos);` |
|       37 |  3746 | `	}else{` |
|        - |  3747 | `		/* Integer arithmetic */` |
|        - |  3748 | `		sxi64 a,b,r;` |
|      489 |  3749 | `		a = pNos->x.iVal;` |
|      489 |  3750 | `		b = pTos->x.iVal;` |
|      489 |  3751 | `		r = a - b;` |
|        - |  3752 | `		/* Push the result */` |
|      489 |  3753 | `		pNos->x.iVal = r;` |
|      489 |  3754 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3755 | `	}` |
|      561 |  3756 | `	VmPopOperand(&pTos,1);` |
|      561 |  3757 | `	break;` |
|        - |  3758 | `				 }` |
|        - |  3759 | `/* OP_SUB_STORE * * *` |
|        - |  3760 | ` *` |
|        - |  3761 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3762 | ` * first (what was next on the stack) from the second (the` |
|        - |  3763 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3764 | ` */` |
|        1 |  3765 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3766 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3767 | `	ph7_value *pObj;` |
|        - |  3768 | `#ifdef UNTRUST` |
|        - |  3769 | `	if( pNos < pStack ){` |
|        - |  3770 | `		goto Abort;` |
|        - |  3771 | `	}` |
|        - |  3772 | `#endif` |
|        3 |  3773 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3774 | `		/* Floating point arithemic */` |
|        - |  3775 | `		ph7_real a,b,r;` |
|      ! 0 |  3776 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3777 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3778 | `		}` |
|      ! 0 |  3779 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3780 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  3781 | `		}` |
|      ! 0 |  3782 | `		a = pTos->rVal;` |
|      ! 0 |  3783 | `		b = pNos->rVal;` |
|      ! 0 |  3784 | `		r = a - b;` |
|        - |  3785 | `		/* Push the result */` |
|      ! 0 |  3786 | `		pNos->rVal = r;` |
|      ! 0 |  3787 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3788 | `		/* Try to get an integer representation */` |
|      ! 0 |  3789 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  3790 | `	}else{` |
|        - |  3791 | `		/* Integer arithmetic */` |
|        - |  3792 | `		sxi64 a,b,r;` |
|        3 |  3793 | `		a = pTos->x.iVal;` |
|        3 |  3794 | `		b = pNos->x.iVal;` |
|        3 |  3795 | `		r = a - b;` |
|        - |  3796 | `		/* Push the result */` |
|        3 |  3797 | `		pNos->x.iVal = r;` |
|        3 |  3798 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3799 | `	}` |
|        3 |  3800 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3801 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3802 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3803 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3804 | `	}` |
|        3 |  3805 | `	VmPopOperand(&pTos,1);` |
|        3 |  3806 | `	break;` |
|        - |  3807 | `				 }` |
|        - |  3808 |  |
|        - |  3809 | `/*` |
|        - |  3810 | ` * OP_MOD * * *` |
|        - |  3811 | ` *` |
|        - |  3812 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3813 | ` * first (what was next on the stack) from the second (the` |
|        - |  3814 | ` * top of the stack) and push the remainder after division` |
|        - |  3815 | ` * onto the stack.` |
|        - |  3816 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3817 | ` */` |
|      296 |  3818 | `case PH7_OP_MOD:{` |
|      594 |  3819 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3820 | `	sxi64 a,b,r;` |
|        - |  3821 | `#ifdef UNTRUST` |
|        - |  3822 | `	if( pNos < pStack ){` |
|        - |  3823 | `		goto Abort;` |
|        - |  3824 | `	}` |
|        - |  3825 | `#endif` |
|        - |  3826 | `	/* Force the operands to be integer */` |
|      594 |  3827 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3828 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3829 | `	}` |
|      594 |  3830 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  3831 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  3832 | `	}` |
|        - |  3833 | `	/* Perform the requested operation */` |
|      594 |  3834 | `	a = pNos->x.iVal;` |
|      594 |  3835 | `	b = pTos->x.iVal;` |
|      594 |  3836 | `	if( b == 0 ){` |
|        3 |  3837 | `		r = 0;` |
|        3 |  3838 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3839 | `		/* goto Abort; */` |
|        2 |  3840 | `	}else{` |
|      591 |  3841 | `		r = a%b;` |
|        - |  3842 | `	}` |
|        - |  3843 | `	/* Push the result */` |
|      594 |  3844 | `	pNos->x.iVal = r;` |
|      594 |  3845 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      594 |  3846 | `	VmPopOperand(&pTos,1);` |
|      594 |  3847 | `	break;` |
|        - |  3848 | `				}` |
|        - |  3849 | `/*` |
|        - |  3850 | ` * OP_MOD_STORE * * *` |
|        - |  3851 | ` *` |
|        - |  3852 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3853 | ` * first (what was next on the stack) from the second (the` |
|        - |  3854 | ` * top of the stack) and push the remainder after division` |
|        - |  3855 | ` * onto the stack.` |
|        - |  3856 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3857 | ` */` |
|        1 |  3858 | `case PH7_OP_MOD_STORE: {` |
|        3 |  3859 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3860 | `	ph7_value *pObj;` |
|        - |  3861 | `	sxi64 a,b,r;` |
|        - |  3862 | `#ifdef UNTRUST` |
|        - |  3863 | `	if( pNos < pStack ){` |
|        - |  3864 | `		goto Abort;` |
|        - |  3865 | `	}` |
|        - |  3866 | `#endif` |
|        - |  3867 | `	/* Force the operands to be integer */` |
|        3 |  3868 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3869 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3870 | `	}` |
|        3 |  3871 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3872 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3873 | `	}` |
|        - |  3874 | `	/* Perform the requested operation */` |
|        3 |  3875 | `	a = pTos->x.iVal;` |
|        3 |  3876 | `	b = pNos->x.iVal;` |
|        3 |  3877 | `	if( b == 0 ){` |
|      ! 0 |  3878 | `		r = 0;` |
|      ! 0 |  3879 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3880 | `		/* goto Abort; */` |
|      ! 0 |  3881 | `	}else{` |
|        3 |  3882 | `		r = a%b;` |
|        - |  3883 | `	}` |
|        - |  3884 | `	/* Push the result */` |
|        3 |  3885 | `	pNos->x.iVal = r;` |
|        3 |  3886 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  3887 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3888 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3889 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3890 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3891 | `	}` |
|        3 |  3892 | `	VmPopOperand(&pTos,1);` |
|        3 |  3893 | `	break;` |
|        - |  3894 | `				}` |
|        - |  3895 | `/*` |
|        - |  3896 | ` * OP_DIV * * *` |
|        - |  3897 | ` *` |
|        - |  3898 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3899 | ` * first (what was next on the stack) from the second (the` |
|        - |  3900 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3901 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3902 | ` */` |
|       28 |  3903 | `case PH7_OP_DIV:{` |
|       58 |  3904 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3905 | `	ph7_real a,b,r;` |
|        - |  3906 | `#ifdef UNTRUST` |
|        - |  3907 | `	if( pNos < pStack ){` |
|        - |  3908 | `		goto Abort;` |
|        - |  3909 | `	}` |
|        - |  3910 | `#endif` |
|        - |  3911 | `	/* Force the operands to be real */` |
|       58 |  3912 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  3913 | `		PH7_MemObjToReal(pTos);` |
|       26 |  3914 | `	}` |
|       58 |  3915 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  3916 | `		PH7_MemObjToReal(pNos);` |
|        9 |  3917 | `	}` |
|        - |  3918 | `	/* Perform the requested operation */` |
|       58 |  3919 | `	a = pNos->rVal;` |
|       58 |  3920 | `	b = pTos->rVal;` |
|       58 |  3921 | `	if( b == 0 ){` |
|        - |  3922 | `		/* Division by zero */` |
|        3 |  3923 | `		pNos->rVal = 0;` |
|        3 |  3924 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  3925 | `		/* goto Abort; */` |
|        2 |  3926 | `	}else{` |
|       55 |  3927 | `		r = a/b;` |
|        - |  3928 | `		/* Push the result */` |
|       55 |  3929 | `		pNos->rVal = r;` |
|       55 |  3930 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3931 | `		/* Try to get an integer representation */` |
|       55 |  3932 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3933 | `	}` |
|       58 |  3934 | `	VmPopOperand(&pTos,1);` |
|       58 |  3935 | `	break;` |
|        - |  3936 | `				}` |
|        - |  3937 | `/*` |
|        - |  3938 | ` * OP_DIV_STORE * * *` |
|        - |  3939 | ` *` |
|        - |  3940 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3941 | ` * first (what was next on the stack) from the second (the` |
|        - |  3942 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3943 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3944 | ` */` |
|        1 |  3945 | `case PH7_OP_DIV_STORE:{` |
|        3 |  3946 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3947 | `	ph7_value *pObj;` |
|        - |  3948 | `	ph7_real a,b,r;` |
|        - |  3949 | `#ifdef UNTRUST` |
|        - |  3950 | `	if( pNos < pStack ){` |
|        - |  3951 | `		goto Abort;` |
|        - |  3952 | `	}` |
|        - |  3953 | `#endif` |
|        - |  3954 | `	/* Force the operands to be real */` |
|        3 |  3955 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3956 | `		PH7_MemObjToReal(pTos);` |
|        1 |  3957 | `	}` |
|        3 |  3958 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3959 | `		PH7_MemObjToReal(pNos);` |
|        1 |  3960 | `	}` |
|        - |  3961 | `	/* Perform the requested operation */` |
|        3 |  3962 | `	a = pTos->rVal;` |
|        3 |  3963 | `	b = pNos->rVal;` |
|        3 |  3964 | `	if( b == 0 ){` |
|        - |  3965 | `		/* Division by zero */` |
|      ! 0 |  3966 | `		r = 0;` |
|      ! 0 |  3967 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  3968 | `		/* goto Abort; */` |
|      ! 0 |  3969 | `	}else{` |
|        3 |  3970 | `		r = a/b;` |
|        - |  3971 | `		/* Push the result */` |
|        3 |  3972 | `		pNos->rVal = r;` |
|        3 |  3973 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3974 | `		/* Try to get an integer representation */` |
|        3 |  3975 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3976 | `	}` |
|        3 |  3977 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3978 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3979 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3980 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3981 | `	}` |
|        3 |  3982 | `	VmPopOperand(&pTos,1);` |
|        3 |  3983 | `	break;` |
|        - |  3984 | `				}` |
|        - |  3985 | `/* OP_BAND * * *` |
|        - |  3986 | ` *` |
|        - |  3987 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3988 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  3989 | ` * two elements.` |
|        - |  3990 | `*/` |
|        - |  3991 | `/* OP_BOR * * *` |
|        - |  3992 | ` *` |
|        - |  3993 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3994 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  3995 | ` * two elements.` |
|        - |  3996 | ` */` |
|        - |  3997 | `/* OP_BXOR * * *` |
|        - |  3998 | ` *` |
|        - |  3999 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4000 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4001 | ` * two elements.` |
|        - |  4002 | ` */` |
|       19 |  4003 | `case PH7_OP_BAND:` |
|        - |  4004 | `case PH7_OP_BOR:` |
|        - |  4005 | `case PH7_OP_BXOR:{` |
|       39 |  4006 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4007 | `	sxi64 a,b,r;` |
|        - |  4008 | `#ifdef UNTRUST` |
|        - |  4009 | `	if( pNos < pStack ){` |
|        - |  4010 | `		goto Abort;` |
|        - |  4011 | `	}` |
|        - |  4012 | `#endif` |
|        - |  4013 | `	/* Force the operands to be integer */` |
|       39 |  4014 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4015 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4016 | `	}` |
|       39 |  4017 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4018 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4019 | `	}` |
|        - |  4020 | `	/* Perform the requested operation */` |
|       39 |  4021 | `	a = pNos->x.iVal;` |
|       39 |  4022 | `	b = pTos->x.iVal;` |
|       39 |  4023 | `	switch(pInstr->iOp){` |
|        6 |  4024 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4025 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4026 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4027 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        7 |  4028 | `	case PH7_OP_BAND_STORE:` |
|        7 |  4029 | `	case PH7_OP_BAND:` |
|       15 |  4030 | `	default:          r = a&b; break;` |
|        - |  4031 | `	}` |
|        - |  4032 | `	/* Push the result */` |
|       39 |  4033 | `	pNos->x.iVal = r;` |
|       39 |  4034 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       39 |  4035 | `	VmPopOperand(&pTos,1);` |
|       39 |  4036 | `	break;` |
|        - |  4037 | `				 }` |
|        - |  4038 | `/* OP_BAND_STORE * * *` |
|        - |  4039 | ` *` |
|        - |  4040 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4041 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4042 | ` * two elements.` |
|        - |  4043 | `*/` |
|        - |  4044 | `/* OP_BOR_STORE * * *` |
|        - |  4045 | ` *` |
|        - |  4046 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4047 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4048 | ` * two elements.` |
|        - |  4049 | ` */` |
|        - |  4050 | `/* OP_BXOR_STORE * * *` |
|        - |  4051 | ` *` |
|        - |  4052 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4053 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4054 | ` * two elements.` |
|        - |  4055 | ` */` |
|        7 |  4056 | `case PH7_OP_BAND_STORE:` |
|        - |  4057 | `case PH7_OP_BOR_STORE:` |
|        - |  4058 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4059 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4060 | `	ph7_value *pObj;` |
|        - |  4061 | `	sxi64 a,b,r;` |
|        - |  4062 | `#ifdef UNTRUST` |
|        - |  4063 | `	if( pNos < pStack ){` |
|        - |  4064 | `		goto Abort;` |
|        - |  4065 | `	}` |
|        - |  4066 | `#endif` |
|        - |  4067 | `	/* Force the operands to be integer */` |
|       15 |  4068 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4069 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4070 | `	}` |
|       15 |  4071 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4072 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4073 | `	}` |
|        - |  4074 | `	/* Perform the requested operation */` |
|       15 |  4075 | `	a = pTos->x.iVal;` |
|       15 |  4076 | `	b = pNos->x.iVal;` |
|       15 |  4077 | `	switch(pInstr->iOp){` |
|        2 |  4078 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4079 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4080 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4081 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4082 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4083 | `	case PH7_OP_BAND:` |
|        5 |  4084 | `	default:          r = a&b; break;` |
|        - |  4085 | `	}` |
|        - |  4086 | `	/* Push the result */` |
|       15 |  4087 | `	pNos->x.iVal = r;` |
|       15 |  4088 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4089 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4090 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4091 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4092 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4093 | `	}` |
|       15 |  4094 | `	VmPopOperand(&pTos,1);` |
|       15 |  4095 | `	break;` |
|        - |  4096 | `				 }` |
|        - |  4097 | `/* OP_SHL * * *` |
|        - |  4098 | ` *` |
|        - |  4099 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4100 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4101 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4102 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4103 | ` */` |
|        - |  4104 | `/* OP_SHR * * *` |
|        - |  4105 | ` *` |
|        - |  4106 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4107 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4108 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4109 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4110 | ` */` |
|        9 |  4111 | `case PH7_OP_SHL:` |
|        - |  4112 | `case PH7_OP_SHR: {` |
|       19 |  4113 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4114 | `	sxi64 a,r;` |
|        - |  4115 | `	sxi32 b;` |
|        - |  4116 | `#ifdef UNTRUST` |
|        - |  4117 | `	if( pNos < pStack ){` |
|        - |  4118 | `		goto Abort;` |
|        - |  4119 | `	}` |
|        - |  4120 | `#endif` |
|        - |  4121 | `	/* Force the operands to be integer */` |
|       19 |  4122 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4123 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4124 | `	}` |
|       19 |  4125 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4126 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4127 | `	}` |
|        - |  4128 | `	/* Perform the requested operation */` |
|       19 |  4129 | `	a = pNos->x.iVal;` |
|       19 |  4130 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4131 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4132 | `		r = a << b;` |
|        6 |  4133 | `	}else{` |
|        9 |  4134 | `		r = a >> b;` |
|        - |  4135 | `	}` |
|        - |  4136 | `	/* Push the result */` |
|       19 |  4137 | `	pNos->x.iVal = r;` |
|       19 |  4138 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4139 | `	VmPopOperand(&pTos,1);` |
|       19 |  4140 | `	break;` |
|        - |  4141 | `				 }` |
|        - |  4142 | `/*  OP_SHL_STORE * * *` |
|        - |  4143 | ` *` |
|        - |  4144 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4145 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4146 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4147 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4148 | ` */` |
|        - |  4149 | `/* OP_SHR_STORE * * *` |
|        - |  4150 | ` *` |
|        - |  4151 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4152 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4153 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4154 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4155 | ` */` |
|        7 |  4156 | `case PH7_OP_SHL_STORE:` |
|        - |  4157 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4158 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4159 | `	ph7_value *pObj;` |
|        - |  4160 | `	sxi64 a,r;` |
|        - |  4161 | `	sxi32 b;` |
|        - |  4162 | `#ifdef UNTRUST` |
|        - |  4163 | `	if( pNos < pStack ){` |
|        - |  4164 | `		goto Abort;` |
|        - |  4165 | `	}` |
|        - |  4166 | `#endif` |
|        - |  4167 | `	/* Force the operands to be integer */` |
|       15 |  4168 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4169 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4170 | `	}` |
|       15 |  4171 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4172 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4173 | `	}` |
|        - |  4174 | `	/* Perform the requested operation */` |
|       15 |  4175 | `	a = pTos->x.iVal;` |
|       15 |  4176 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4177 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4178 | `		r = a << b;` |
|        4 |  4179 | `	}else{` |
|        9 |  4180 | `		r = a >> b;` |
|        - |  4181 | `	}` |
|        - |  4182 | `	/* Push the result */` |
|       15 |  4183 | `	pNos->x.iVal = r;` |
|       15 |  4184 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4185 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4186 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4187 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4188 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4189 | `	}` |
|       15 |  4190 | `	VmPopOperand(&pTos,1);` |
|       15 |  4191 | `	break;` |
|        - |  4192 | `				 }` |
|        - |  4193 | `/* CAT:  P1 * *` |
|        - |  4194 | ` *` |
|        - |  4195 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4196 | ` * back.` |
|        - |  4197 | ` */` |
|    51654 |  4198 | `case PH7_OP_CAT:{` |
|        - |  4199 | `	ph7_value *pNos,*pCur;` |
|   103310 |  4200 | `	if( pInstr->iP1 < 1 ){` |
|    73632 |  4201 | `		pNos = &pTos[-1];` |
|    36817 |  4202 | `	}else{` |
|    29680 |  4203 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4204 | `	}` |
|        - |  4205 | `#ifdef UNTRUST` |
|        - |  4206 | `	if( pNos < pStack ){` |
|        - |  4207 | `		goto Abort;` |
|        - |  4208 | `	}` |
|        - |  4209 | `#endif` |
|        - |  4210 | `	/* Force a string cast */` |
|   103310 |  4211 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      540 |  4212 | `		PH7_MemObjToString(pNos);` |
|      269 |  4213 | `	}` |
|   103310 |  4214 | `	pCur = &pNos[1];` |
|   216824 |  4215 | `	while( pCur <= pTos ){` |
|   113516 |  4216 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    53198 |  4217 | `			PH7_MemObjToString(pCur);` |
|    26598 |  4218 | `		}` |
|        - |  4219 | `		/* Perform the concatenation */` |
|   113516 |  4220 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   113476 |  4221 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    56737 |  4222 | `		}` |
|   113516 |  4223 | `		SyBlobRelease(&pCur->sBlob);` |
|   113516 |  4224 | `		pCur++;` |
|        2 |  4225 | `	}` |
|   103310 |  4226 | `	pTos = pNos;` |
|   103310 |  4227 | `	break;` |
|        - |  4228 | `				}` |
|        - |  4229 | `/*  CAT_STORE: * * *` |
|        - |  4230 | ` *` |
|        - |  4231 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4232 | ` * back.` |
|        - |  4233 | ` */` |
|     1194 |  4234 | `case PH7_OP_CAT_STORE:{` |
|     2389 |  4235 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4236 | `	ph7_value *pObj;` |
|        - |  4237 | `#ifdef UNTRUST` |
|        - |  4238 | `	if( pNos < pStack ){` |
|        - |  4239 | `		goto Abort;` |
|        - |  4240 | `	}` |
|        - |  4241 | `#endif` |
|     2389 |  4242 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4243 | `		/* Force a string cast */` |
|      461 |  4244 | `		PH7_MemObjToString(pTos);` |
|      230 |  4245 | `	}` |
|     2389 |  4246 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4247 | `		/* Force a string cast */` |
|      ! 0 |  4248 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4249 | `	}` |
|        - |  4250 | `	/* Perform the concatenation (Reverse order) */` |
|     2389 |  4251 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     2389 |  4252 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     1194 |  4253 | `	}` |
|        - |  4254 | `	/* Perform the store operation */` |
|     2389 |  4255 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4256 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     2389 |  4257 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     2389 |  4258 | `		PH7_MemObjStore(pTos,pObj);` |
|     1194 |  4259 | `	}` |
|     2389 |  4260 | `	PH7_MemObjStore(pTos,pNos);` |
|     2389 |  4261 | `	VmPopOperand(&pTos,1);` |
|     2389 |  4262 | `	break;` |
|        - |  4263 | `				}` |
|        - |  4264 | `/* OP_AND: * * *` |
|        - |  4265 | ` *` |
|        - |  4266 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4267 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4268 | ` * stack.` |
|        - |  4269 | ` */` |
|        - |  4270 | `/* OP_OR: * * *` |
|        - |  4271 | ` *` |
|        - |  4272 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4273 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4274 | ` * stack.` |
|        - |  4275 | ` */` |
|    55719 |  4276 | `case PH7_OP_LAND:` |
|        - |  4277 | `case PH7_OP_LOR: {` |
|   111484 |  4278 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4279 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4280 | `#ifdef UNTRUST` |
|        - |  4281 | `	if( pNos < pStack ){` |
|        - |  4282 | `		goto Abort;` |
|        - |  4283 | `	}` |
|        - |  4284 | `#endif` |
|        - |  4285 | `	/* Force a boolean cast */` |
|   111484 |  4286 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4287 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4288 | `	}` |
|   111484 |  4289 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4290 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4291 | `	}` |
|   111484 |  4292 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   111484 |  4293 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   111484 |  4294 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4295 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    59120 |  4296 | `		v1 = and_logic[v1*3+v2];` |
|    29583 |  4297 | `	}else{` |
|        - |  4298 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|    52366 |  4299 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4300 | `	}` |
|   111484 |  4301 | `	if( v1 == 2 ){` |
|      ! 0 |  4302 | `		v1 = 1;` |
|      ! 0 |  4303 | `	}` |
|   111484 |  4304 | `	VmPopOperand(&pTos,1);` |
|   111484 |  4305 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   111484 |  4306 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   111484 |  4307 | `	break;` |
|        - |  4308 | `				 }` |
|        - |  4309 | `/* OP_LXOR: * * *` |
|        - |  4310 | ` *` |
|        - |  4311 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4312 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4313 | ` * stack.` |
|        - |  4314 | ` * According to the PHP language reference manual:` |
|        - |  4315 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4316 | ` *  TRUE,but not both.` |
|        - |  4317 | ` */` |
|        5 |  4318 | `case PH7_OP_LXOR:{` |
|       11 |  4319 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4320 | `	sxi32 v = 0;` |
|        - |  4321 | `#ifdef UNTRUST` |
|        - |  4322 | `	if( pNos < pStack ){` |
|        - |  4323 | `		goto Abort;` |
|        - |  4324 | `	}` |
|        - |  4325 | `#endif` |
|        - |  4326 | `	/* Force a boolean cast */` |
|       11 |  4327 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4328 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4329 | `	}` |
|       11 |  4330 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4331 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4332 | `	}` |
|       11 |  4333 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4334 | `		v = 1;` |
|        3 |  4335 | `	}` |
|       11 |  4336 | `	VmPopOperand(&pTos,1);` |
|       11 |  4337 | `	pTos->x.iVal = v;` |
|       11 |  4338 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4339 | `	break;` |
|        - |  4340 | `				 }` |
|        - |  4341 | `/* OP_EQ P1 P2 P3` |
|        - |  4342 | ` *` |
|        - |  4343 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4344 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4345 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4346 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4347 | ` */` |
|        - |  4348 | `/* OP_NEQ P1 P2 P3` |
|        - |  4349 | ` *` |
|        - |  4350 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4351 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4352 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4353 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4354 | ` */` |
|     3023 |  4355 | `case PH7_OP_EQ:` |
|        - |  4356 | `case PH7_OP_NEQ: {` |
|     6048 |  4357 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4358 | `	/* Perform the comparison and act accordingly */` |
|        - |  4359 | `#ifdef UNTRUST` |
|        - |  4360 | `	if( pNos < pStack ){` |
|        - |  4361 | `		goto Abort;` |
|        - |  4362 | `	}` |
|        - |  4363 | `#endif` |
|     6048 |  4364 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     6048 |  4365 | `	if( pInstr->iOp == PH7_OP_EQ ){` |
|     6026 |  4366 | `		rc = rc == 0;` |
|     3014 |  4367 | `	}else{` |
|       24 |  4368 | `		rc = rc != 0;` |
|        - |  4369 | `	}` |
|     6048 |  4370 | `	VmPopOperand(&pTos,1);` |
|     6048 |  4371 | `	if( !pInstr->iP2 ){` |
|        - |  4372 | `		/* Push comparison result without taking the jump */` |
|     6048 |  4373 | `		PH7_MemObjRelease(pTos);` |
|     6048 |  4374 | `		pTos->x.iVal = rc;` |
|        - |  4375 | `		/* Invalidate any prior representation */` |
|     6048 |  4376 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3025 |  4377 | `	}else{` |
|      ! 0 |  4378 | `		if( rc ){` |
|        - |  4379 | `			/* Jump to the desired location */` |
|      ! 0 |  4380 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4381 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4382 | `		}` |
|        - |  4383 | `	}` |
|     6048 |  4384 | `	break;` |
|        - |  4385 | `				 }` |
|        - |  4386 | `/* OP_TEQ P1 P2 *` |
|        - |  4387 | ` *` |
|        - |  4388 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4389 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4390 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4391 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4392 | ` */` |
|    82830 |  4393 | `case PH7_OP_TEQ: {` |
|   165662 |  4394 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4395 | `	/* Perform the comparison and act accordingly */` |
|        - |  4396 | `#ifdef UNTRUST` |
|        - |  4397 | `	if( pNos < pStack ){` |
|        - |  4398 | `		goto Abort;` |
|        - |  4399 | `	}` |
|        - |  4400 | `#endif` |
|   165662 |  4401 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0) == 0;` |
|   165662 |  4402 | `	VmPopOperand(&pTos,1);` |
|   165662 |  4403 | `	if( !pInstr->iP2 ){` |
|        - |  4404 | `		/* Push comparison result without taking the jump */` |
|   165662 |  4405 | `		PH7_MemObjRelease(pTos);` |
|   165662 |  4406 | `		pTos->x.iVal = rc;` |
|        - |  4407 | `		/* Invalidate any prior representation */` |
|   165662 |  4408 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    82832 |  4409 | `	}else{` |
|      ! 0 |  4410 | `		if( rc ){` |
|        - |  4411 | `			/* Jump to the desired location */` |
|      ! 0 |  4412 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4413 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4414 | `		}` |
|        - |  4415 | `	}` |
|   165662 |  4416 | `	break;` |
|        - |  4417 | `				 }` |
|        - |  4418 | `/* OP_TNE P1 P2 *` |
|        - |  4419 | ` *` |
|        - |  4420 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4421 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4422 | ` * instruction.` |
|        - |  4423 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4424 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4425 | ` *` |
|        - |  4426 | ` */` |
|    65212 |  4427 | `case PH7_OP_TNE: {` |
|   130426 |  4428 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4429 | `	/* Perform the comparison and act accordingly */` |
|        - |  4430 | `#ifdef UNTRUST` |
|        - |  4431 | `	if( pNos < pStack ){` |
|        - |  4432 | `		goto Abort;` |
|        - |  4433 | `	}` |
|        - |  4434 | `#endif` |
|   130426 |  4435 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0) != 0;` |
|   130426 |  4436 | `	VmPopOperand(&pTos,1);` |
|   130426 |  4437 | `	if( !pInstr->iP2 ){` |
|        - |  4438 | `		/* Push comparison result without taking the jump */` |
|   130426 |  4439 | `		PH7_MemObjRelease(pTos);` |
|   130426 |  4440 | `		pTos->x.iVal = rc;` |
|        - |  4441 | `		/* Invalidate any prior representation */` |
|   130426 |  4442 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    65214 |  4443 | `	}else{` |
|      ! 0 |  4444 | `		if( rc ){` |
|        - |  4445 | `			/* Jump to the desired location */` |
|      ! 0 |  4446 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4447 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4448 | `		}` |
|        - |  4449 | `	}` |
|   130426 |  4450 | `	break;` |
|        - |  4451 | `				 }` |
|        - |  4452 | `/* OP_LT P1 P2 P3` |
|        - |  4453 | ` *` |
|        - |  4454 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4455 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4456 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4457 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4458 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4459 | ` *` |
|        - |  4460 | ` */` |
|        - |  4461 | `/* OP_LE P1 P2 P3` |
|        - |  4462 | ` *` |
|        - |  4463 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4464 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4465 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4466 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4467 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4468 | ` *` |
|        - |  4469 | ` */` |
|    68830 |  4470 | `case PH7_OP_LT:` |
|        - |  4471 | `case PH7_OP_LE: {` |
|   137706 |  4472 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4473 | `	/* Perform the comparison and act accordingly */` |
|        - |  4474 | `#ifdef UNTRUST` |
|        - |  4475 | `	if( pNos < pStack ){` |
|        - |  4476 | `		goto Abort;` |
|        - |  4477 | `	}` |
|        - |  4478 | `#endif` |
|   137706 |  4479 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   137706 |  4480 | `	if( pInstr->iOp == PH7_OP_LE ){` |
|      395 |  4481 | `		rc = rc < 1;` |
|      198 |  4482 | `	}else{` |
|   137312 |  4483 | `		rc = rc < 0;` |
|        - |  4484 | `	}` |
|   137706 |  4485 | `	VmPopOperand(&pTos,1);` |
|   137706 |  4486 | `	if( !pInstr->iP2 ){` |
|        - |  4487 | `		/* Push comparison result without taking the jump */` |
|   137706 |  4488 | `		PH7_MemObjRelease(pTos);` |
|   137706 |  4489 | `		pTos->x.iVal = rc;` |
|        - |  4490 | `		/* Invalidate any prior representation */` |
|   137706 |  4491 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    68876 |  4492 | `	}else{` |
|      ! 0 |  4493 | `		if( rc ){` |
|        - |  4494 | `			/* Jump to the desired location */` |
|      ! 0 |  4495 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4496 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4497 | `		}` |
|        - |  4498 | `	}` |
|   137706 |  4499 | `	break;` |
|        - |  4500 | `				}` |
|        - |  4501 | `/* OP_GT P1 P2 P3` |
|        - |  4502 | ` *` |
|        - |  4503 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4504 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4505 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4506 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4507 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4508 | ` *` |
|        - |  4509 | ` */` |
|        - |  4510 | `/* OP_GE P1 P2 P3` |
|        - |  4511 | ` *` |
|        - |  4512 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4513 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4514 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4515 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4516 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4517 | ` *` |
|        - |  4518 | ` */` |
|    23893 |  4519 | `case PH7_OP_GT:` |
|        - |  4520 | `case PH7_OP_GE: {` |
|    47788 |  4521 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4522 | `	/* Perform the comparison and act accordingly */` |
|        - |  4523 | `#ifdef UNTRUST` |
|        - |  4524 | `	if( pNos < pStack ){` |
|        - |  4525 | `		goto Abort;` |
|        - |  4526 | `	}` |
|        - |  4527 | `#endif` |
|    47788 |  4528 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    47788 |  4529 | `	if( pInstr->iOp == PH7_OP_GE ){` |
|    47642 |  4530 | `		rc = rc >= 0;` |
|    23822 |  4531 | `	}else{` |
|      148 |  4532 | `		rc = rc > 0;` |
|        - |  4533 | `	}` |
|    47788 |  4534 | `	VmPopOperand(&pTos,1);` |
|    47788 |  4535 | `	if( !pInstr->iP2 ){` |
|        - |  4536 | `		/* Push comparison result without taking the jump */` |
|    47788 |  4537 | `		PH7_MemObjRelease(pTos);` |
|    47788 |  4538 | `		pTos->x.iVal = rc;` |
|        - |  4539 | `		/* Invalidate any prior representation */` |
|    47788 |  4540 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    23895 |  4541 | `	}else{` |
|      ! 0 |  4542 | `		if( rc ){` |
|        - |  4543 | `			/* Jump to the desired location */` |
|      ! 0 |  4544 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4545 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4546 | `		}` |
|        - |  4547 | `	}` |
|    47788 |  4548 | `	break;` |
|        - |  4549 | `				}` |
|        - |  4550 | `/* OP_SEQ P1 P2 *` |
|        - |  4551 | ` * Strict string comparison.` |
|        - |  4552 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4553 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4554 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4555 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4556 | ` * use PH7_OP_EQ.` |
|        - |  4557 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4558 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4559 | ` */` |
|        - |  4560 | `/* OP_SNE P1 P2 *` |
|        - |  4561 | ` * Strict string comparison.` |
|        - |  4562 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4563 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4564 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4565 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4566 | ` * use PH7_OP_EQ.` |
|        - |  4567 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4568 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4569 | ` */` |
|       18 |  4570 | `case PH7_OP_SEQ:` |
|        - |  4571 | `case PH7_OP_SNE: {` |
|       38 |  4572 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4573 | `	SyString s1,s2;` |
|        - |  4574 | `	/* Perform the comparison and act accordingly */` |
|        - |  4575 | `#ifdef UNTRUST` |
|        - |  4576 | `	if( pNos < pStack ){` |
|        - |  4577 | `		goto Abort;` |
|        - |  4578 | `	}` |
|        - |  4579 | `#endif` |
|        - |  4580 | `	/* Force a string cast */` |
|       38 |  4581 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        9 |  4582 | `		PH7_MemObjToString(pTos);` |
|        4 |  4583 | `	}` |
|       38 |  4584 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4585 | `		PH7_MemObjToString(pNos);` |
|        2 |  4586 | `	}` |
|       38 |  4587 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4588 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4589 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4590 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4591 | `		rc = rc != 0;` |
|      ! 0 |  4592 | `	}else{` |
|       38 |  4593 | `		rc = rc == 0;` |
|        - |  4594 | `	}` |
|       38 |  4595 | `	VmPopOperand(&pTos,1);` |
|       38 |  4596 | `	if( !pInstr->iP2 ){` |
|        - |  4597 | `		/* Push comparison result without taking the jump */` |
|       38 |  4598 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4599 | `		pTos->x.iVal = rc;` |
|        - |  4600 | `		/* Invalidate any prior representation */` |
|       38 |  4601 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4602 | `	}else{` |
|      ! 0 |  4603 | `		if( rc ){` |
|        - |  4604 | `			/* Jump to the desired location */` |
|      ! 0 |  4605 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4606 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4607 | `		}` |
|        - |  4608 | `	}` |
|       38 |  4609 | `	break;` |
|        - |  4610 | `				 }` |
|        - |  4611 | `/*` |
|        - |  4612 | ` * OP_LOAD_REF * * *` |
|        - |  4613 | ` * Push the index of a referenced object on the stack.` |
|        - |  4614 | ` */` |
|       57 |  4615 | `case PH7_OP_LOAD_REF: {` |
|        - |  4616 | `	sxu32 nIdx;` |
|        - |  4617 | `#ifdef UNTRUST` |
|        - |  4618 | `	if( pTos < pStack ){` |
|        - |  4619 | `		goto Abort;` |
|        - |  4620 | `	}` |
|        - |  4621 | `#endif` |
|        - |  4622 | `	/* Extract memory object index */` |
|      115 |  4623 | `	nIdx = pTos->nIdx;` |
|      115 |  4624 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4625 | `		/* Nullify the object */` |
|       95 |  4626 | `		PH7_MemObjRelease(pTos);` |
|        - |  4627 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4628 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4629 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4630 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4631 | `	}` |
|      115 |  4632 | `	break;` |
|        - |  4633 | `					  }` |
|        - |  4634 | `/*` |
|        - |  4635 | ` * OP_STORE_REF * * P3` |
|        - |  4636 | ` * Perform an assignment operation by reference.` |
|        - |  4637 | ` */` |
|       14 |  4638 | ` case PH7_OP_STORE_REF: {` |
|       30 |  4639 | `	 SyString sName = { 0 , 0 };` |
|        - |  4640 | `	 VmFrame *pFrameLocal;` |
|        - |  4641 | `	SyHashEntry *pEntry;` |
|        - |  4642 | `	sxu32 nIdx;` |
|        - |  4643 | `#ifdef UNTRUST` |
|        - |  4644 | `	if( pTos < pStack ){` |
|        - |  4645 | `		goto Abort;` |
|        - |  4646 | `	}` |
|        - |  4647 | `#endif` |
|       30 |  4648 | `	if( pInstr->p3 == 0 ){` |
|        - |  4649 | `		char *zName;` |
|        - |  4650 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4651 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4652 | `			/* Force a string cast */` |
|      ! 0 |  4653 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4654 | `		}` |
|      ! 0 |  4655 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4656 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4657 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4658 | `			if( zName ){` |
|      ! 0 |  4659 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4660 | `			}` |
|      ! 0 |  4661 | `		}` |
|      ! 0 |  4662 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4663 | `		pTos--;` |
|      ! 0 |  4664 | `	}else{` |
|       30 |  4665 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4666 | `	}` |
|       30 |  4667 | `	nIdx = pTos->nIdx;` |
|       30 |  4668 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4669 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4670 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4671 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4672 | `		}else{` |
|        - |  4673 | `			ph7_value *pObj;` |
|        - |  4674 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4675 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4676 | `			if( pObj == 0 ){` |
|      ! 0 |  4677 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4678 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4679 | `				goto Abort;` |
|        - |  4680 | `			}` |
|        - |  4681 | `			/* Perform the store operation */` |
|      ! 0 |  4682 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4683 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4684 | `		}` |
|       30 |  4685 | `	}else if( sName.nByte > 0){` |
|       30 |  4686 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4687 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4688 | `		}else{` |
|       30 |  4689 | `			pFrameLocal = pVm->pFrame;` |
|       50 |  4690 | `			while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4691 | `				/* Safely ignore the exception frame */` |
|       21 |  4692 | `				pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4693 | `			}` |
|        - |  4694 | `			/* Query the local frame */` |
|       30 |  4695 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       30 |  4696 | `			if( pEntry ){` |
|      ! 0 |  4697 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4698 | `			}else{` |
|       30 |  4699 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       30 |  4700 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4701 | `					/* Insert in the $GLOBALS array */` |
|       26 |  4702 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       12 |  4703 | `				}` |
|       30 |  4704 | `				if( rc == SXRET_OK ){` |
|       30 |  4705 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       14 |  4706 | `				}` |
|        - |  4707 | `			}` |
|        - |  4708 | `		}` |
|       14 |  4709 | `	}` |
|       30 |  4710 | `	break;` |
|        - |  4711 | `				 }` |
|        - |  4712 | `/*` |
|        - |  4713 | ` * OP_UPLINK P1 * *` |
|        - |  4714 | ` * Link a variable to the top active VM frame.` |
|        - |  4715 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4716 | ` */` |
|       14 |  4717 | `case PH7_OP_UPLINK: {` |
|       29 |  4718 | `	if( pVm->pFrame->pParent ){` |
|       29 |  4719 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4720 | `		SyString sName;` |
|        - |  4721 | `		/* Perform the link */` |
|       59 |  4722 | `		while( pLink <= pTos ){` |
|       31 |  4723 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4724 | `				/* Force a string cast */` |
|      ! 0 |  4725 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4726 | `			}` |
|       31 |  4727 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       31 |  4728 | `			if( sName.nByte > 0 ){` |
|       31 |  4729 | `				VmFrameLink(&(*pVm),&sName);` |
|       15 |  4730 | `			}` |
|       31 |  4731 | `			pLink++;` |
|        1 |  4732 | `		}` |
|       14 |  4733 | `	}` |
|       29 |  4734 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       29 |  4735 | `	break;` |
|        - |  4736 | `					}` |
|        - |  4737 | `/*` |
|        - |  4738 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4739 | ` * Push an exception in the corresponding container so that` |
|        - |  4740 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4741 | ` */` |
|       10 |  4742 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       22 |  4743 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4744 | `	VmFrame *pFrameLocal;` |
|       22 |  4745 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4746 | `	/* Create the exception frame */` |
|       22 |  4747 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       22 |  4748 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4749 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4750 | `		goto Abort;` |
|        - |  4751 | `	}` |
|        - |  4752 | `	/* Mark the special frame */` |
|       22 |  4753 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       22 |  4754 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4755 | `	/* Point to the frame that trigger the exception */` |
|       22 |  4756 | `	pFrameLocal = pFrameLocal->pParent;` |
|       34 |  4757 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|       13 |  4758 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4759 | `	}` |
|       22 |  4760 | `	pException->pFrame = pFrameLocal;` |
|       22 |  4761 | `	break;` |
|        - |  4762 | `							}` |
|        - |  4763 | `/*` |
|        - |  4764 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4765 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4766 | ` */` |
|        9 |  4767 | `case PH7_OP_POP_EXCEPTION: {` |
|       20 |  4768 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       20 |  4769 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4770 | `		ph7_exception **apException;` |
|        - |  4771 | `		/* Pop the loaded exception */` |
|        7 |  4772 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        7 |  4773 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|        7 |  4774 | `			(void)SySetPop(&pVm->aException);` |
|        3 |  4775 | `		}` |
|        3 |  4776 | `	}` |
|       20 |  4777 | `	pException->pFrame = 0;` |
|        - |  4778 | `	/* Leave the exception frame */` |
|       20 |  4779 | `	VmLeaveFrame(&(*pVm));` |
|       20 |  4780 | `	break;` |
|        - |  4781 | `							}` |
|        - |  4782 |  |
|        - |  4783 | `/*` |
|        - |  4784 | ` * OP_THROW * P2 *` |
|        - |  4785 | ` * Throw an user exception.` |
|        - |  4786 | ` */` |
|        8 |  4787 | `case PH7_OP_THROW: {` |
|       18 |  4788 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       18 |  4789 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  4790 | `#ifdef UNTRUST` |
|        - |  4791 | `	if( pTos < pStack ){` |
|        - |  4792 | `		goto Abort;` |
|        - |  4793 | `	}` |
|        - |  4794 | `#endif` |
|       24 |  4795 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4796 | `		/* Safely ignore the exception frame */` |
|        8 |  4797 | `		pFrameLocal = pFrameLocal->pParent;` |
|        2 |  4798 | `	}` |
|        - |  4799 | `	/* Tell the upper layer that an exception was thrown */` |
|       18 |  4800 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       18 |  4801 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       18 |  4802 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4803 | `		ph7_class *pException;` |
|        - |  4804 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  4805 | `		 */` |
|       18 |  4806 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       18 |  4807 | `		if( pException == 0 \|\| !VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  4808 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  4809 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  4810 | `			if( rc == SXERR_ABORT ){` |
|        - |  4811 | `				/* Abort processing immediately */` |
|      ! 0 |  4812 | `				goto Abort;` |
|        - |  4813 | `			}` |
|      ! 0 |  4814 | `		}else{` |
|        - |  4815 | `			/* Throw the exception */` |
|       18 |  4816 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       18 |  4817 | `			if( rc == SXERR_ABORT ){` |
|        - |  4818 | `				/* Abort processing immediately */` |
|        3 |  4819 | `				goto Abort;` |
|        - |  4820 | `			}` |
|        - |  4821 | `		}` |
|        9 |  4822 | `	}else{` |
|        - |  4823 | `		/* Expecting a class instance */` |
|      ! 0 |  4824 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  4825 | `		if( rc == SXERR_ABORT ){` |
|        - |  4826 | `			/* Abort processing immediately */` |
|      ! 0 |  4827 | `			goto Abort;` |
|        - |  4828 | `		}` |
|        - |  4829 | `	}` |
|        - |  4830 | `	/* Pop the top entry */` |
|       16 |  4831 | `	VmPopOperand(&pTos,1);` |
|        - |  4832 | `	/* Perform an unconditional jump */` |
|       16 |  4833 | `	pc = nJump - 1;` |
|       16 |  4834 | `	break;` |
|        - |  4835 | `				   }` |
|        - |  4836 | `/*` |
|        - |  4837 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  4838 | ` * Prepare a foreach step.` |
|        - |  4839 | ` */` |
|     3435 |  4840 | `case PH7_OP_FOREACH_INIT: {` |
|     6872 |  4841 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4842 | `	void *pName;` |
|        - |  4843 | `#ifdef UNTRUST` |
|        - |  4844 | `	if( pTos < pStack ){` |
|        - |  4845 | `		goto Abort;` |
|        - |  4846 | `	}` |
|        - |  4847 | `#endif` |
|     6872 |  4848 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4849 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  4850 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4851 | `			/* Force a string cast */` |
|      ! 0 |  4852 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4853 | `		}` |
|        - |  4854 | `		/* Duplicate name */` |
|      ! 0 |  4855 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4856 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4857 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4858 | `		}` |
|      ! 0 |  4859 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4860 | `	}` |
|     6872 |  4861 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  4862 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4863 | `			/* Force a string cast */` |
|      ! 0 |  4864 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4865 | `		}` |
|        - |  4866 | `		/* Duplicate name */` |
|      ! 0 |  4867 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4868 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4869 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4870 | `		}` |
|      ! 0 |  4871 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4872 | `	}` |
|        - |  4873 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     6872 |  4874 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4875 | `		/* Jump out of the loop */` |
|      ! 0 |  4876 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4877 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4878 | `		}` |
|      ! 0 |  4879 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4880 | `	}else{` |
|        - |  4881 | `		ph7_foreach_step *pStep;` |
|     6872 |  4882 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     6872 |  4883 | `		if( pStep == 0 ){` |
|      ! 0 |  4884 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4885 | `			/* Jump out of the loop */` |
|      ! 0 |  4886 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4887 | `		}else{` |
|        - |  4888 | `			/* Zero the structure */` |
|     6872 |  4889 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4890 | `			/* Prepare the step */` |
|     6872 |  4891 | `			pStep->iFlags = pInfo->iFlags;` |
|     6872 |  4892 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     6864 |  4893 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4894 | `				/* Reset the internal loop cursor */` |
|     6864 |  4895 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4896 | `				/* Mark the step */` |
|     6864 |  4897 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     6864 |  4898 | `				pStep->xIter.pMap = pMap;` |
|     6864 |  4899 | `				pMap->iRef++;` |
|     3433 |  4900 | `			}else{` |
|        9 |  4901 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4902 | `				/* Reset the loop cursor */` |
|        9 |  4903 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|        - |  4904 | `				/* Mark the step */` |
|        9 |  4905 | `				pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  4906 | `				pStep->xIter.pThis = pThis;` |
|        9 |  4907 | `				pThis->iRef++;` |
|        - |  4908 | `			}` |
|        - |  4909 | `		}` |
|     6872 |  4910 | `		if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  4911 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  4912 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  4913 | `			/* Jump out of the loop */` |
|      ! 0 |  4914 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4915 | `		}` |
|        - |  4916 | `	}` |
|     6872 |  4917 | `	VmPopOperand(&pTos,1);` |
|     6872 |  4918 | `	break;` |
|        - |  4919 | `						  }` |
|        - |  4920 | `/*` |
|        - |  4921 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  4922 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  4923 | ` */` |
|    58145 |  4924 | `case PH7_OP_FOREACH_STEP: {` |
|   116292 |  4925 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4926 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  4927 | `	ph7_value *pValue;` |
|        - |  4928 | `	VmFrame *pFrameLocal;` |
|        - |  4929 | `	/* Peek the last step */` |
|   116292 |  4930 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   116292 |  4931 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   116292 |  4932 | `	pFrameLocal = pVm->pFrame;` |
|   121302 |  4933 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4934 | `		/* Safely ignore the exception frame */` |
|     5011 |  4935 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4936 | `	}` |
|   116292 |  4937 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   116268 |  4938 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  4939 | `		ph7_hashmap_node *pNode;` |
|        - |  4940 | `		/* Extract the current node value */` |
|   116268 |  4941 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   116268 |  4942 | `		if( pNode == 0 ){` |
|        - |  4943 | `			/* No more entry to process */` |
|     6864 |  4944 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     6864 |  4945 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4946 | `				/* Break the reference with the last element */` |
|        5 |  4947 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  4948 | `			}` |
|        - |  4949 | `			/* Automatically reset the loop cursor */` |
|     6864 |  4950 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4951 | `			/* Cleanup the mess left behind */` |
|     6864 |  4952 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     6864 |  4953 | `			SySetPop(&pInfo->aStep);` |
|     6864 |  4954 | `			PH7_HashmapUnref(pMap);` |
|     3433 |  4955 | `		}else{` |
|   109406 |  4956 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      135 |  4957 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      135 |  4958 | `				if( pKey ){` |
|      135 |  4959 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|       67 |  4960 | `				}` |
|       67 |  4961 | `			}` |
|   109406 |  4962 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4963 | `				SyHashEntry *pEntry;` |
|        - |  4964 | `				/* Pass by reference */` |
|       13 |  4965 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       13 |  4966 | `				if( pEntry ){` |
|       13 |  4967 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|        7 |  4968 | `				}else{` |
|      ! 0 |  4969 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  4970 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  4971 | `				}` |
|        7 |  4972 | `			}else{` |
|        - |  4973 | `				/* Make a copy of the entry value */` |
|   109394 |  4974 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   109394 |  4975 | `				if( pValue ){` |
|   109394 |  4976 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    54696 |  4977 | `				}` |
|        - |  4978 | `			}` |
|        - |  4979 | `		}` |
|    58135 |  4980 | `	}else{` |
|       25 |  4981 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  4982 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  4983 | `		SyHashEntry *pEntry;` |
|        - |  4984 | `		/* Point to the next attribute */` |
|       29 |  4985 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  4986 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  4987 | `			/* Check access permission */` |
|       31 |  4988 | `			if( VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  4989 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  4990 | `					break; /* Access is granted */` |
|        - |  4991 | `			}` |
|        1 |  4992 | `		}` |
|       25 |  4993 | `		if( pEntry == 0 ){` |
|        - |  4994 | `			/* Clean up the mess left behind */` |
|        9 |  4995 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  4996 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4997 | `				/* Break the reference with the last element */` |
|        3 |  4998 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  4999 | `			}` |
|        9 |  5000 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5001 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5002 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5003 | `		}else{` |
|       17 |  5004 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5005 | `			ph7_value *pAttrValue;` |
|       17 |  5006 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5007 | `				/* Fill with the current attribute name */` |
|       17 |  5008 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5009 | `				if( pKey ){` |
|       17 |  5010 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5011 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5012 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5013 | `				}` |
|        8 |  5014 | `			}` |
|        - |  5015 | `			/* Extract attribute value */` |
|       17 |  5016 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5017 | `			if( pAttrValue ){` |
|       17 |  5018 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5019 | `					/* Pass by reference */` |
|        3 |  5020 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5021 | `					if( pEntry ){` |
|        3 |  5022 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5023 | `					}else{` |
|      ! 0 |  5024 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5025 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5026 | `					}` |
|        2 |  5027 | `				}else{` |
|        - |  5028 | `					/* Make a copy of the attribute value */` |
|       15 |  5029 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5030 | `					if( pValue ){` |
|       15 |  5031 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5032 | `					}` |
|        - |  5033 | `				}` |
|        8 |  5034 | `			}` |
|        - |  5035 | `		}` |
|        - |  5036 | `	}` |
|   116292 |  5037 | `	break;` |
|        - |  5038 | `						  }` |
|        - |  5039 | `/*` |
|        - |  5040 | ` * OP_MEMBER P1 P2` |
|        - |  5041 | ` * Load class attribute/method on the stack.` |
|        - |  5042 | ` */` |
|      438 |  5043 | `case PH7_OP_MEMBER: {` |
|        - |  5044 | `	ph7_class_instance *pThis;` |
|        - |  5045 | `	ph7_value *pNos;` |
|        - |  5046 | `	SyString sName;` |
|      878 |  5047 | `	if( !pInstr->iP1 ){` |
|      820 |  5048 | `		pNos = &pTos[-1];` |
|        - |  5049 | `#ifdef UNTRUST` |
|        - |  5050 | `		if( pNos < pStack ){` |
|        - |  5051 | `			goto Abort;` |
|        - |  5052 | `		}` |
|        - |  5053 | `#endif` |
|      820 |  5054 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5055 | `			ph7_class *pClass;` |
|        - |  5056 | `			/* Class already instantiated */` |
|      820 |  5057 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5058 | `			/* Point to the instantiated class */` |
|      820 |  5059 | `			pClass = pThis->pClass;` |
|        - |  5060 | `			/* Extract attribute name first */` |
|      820 |  5061 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      820 |  5062 | `			if( pInstr->iP2 ){` |
|        - |  5063 | `				/* Method call */` |
|      120 |  5064 | `				ph7_class_method *pMeth = 0;` |
|      120 |  5065 | `				if( sName.nByte > 0 ){` |
|        - |  5066 | `					/* Extract the target method */` |
|      120 |  5067 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       59 |  5068 | `				}` |
|      120 |  5069 | `				if( pMeth == 0 ){` |
|      ! 0 |  5070 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5071 | `						&pClass->sName,&sName` |
|        - |  5072 | `						);` |
|        - |  5073 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5074 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5075 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5076 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5077 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5078 | `				}else{` |
|        - |  5079 | `					/* Push method name on the stack */` |
|      120 |  5080 | `					PH7_MemObjRelease(pTos);` |
|      120 |  5081 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      120 |  5082 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5083 | `				}` |
|      120 |  5084 | `				pTos->nIdx = SXU32_HIGH;` |
|       61 |  5085 | `			}else{` |
|        - |  5086 | `				/* Attribute access */` |
|      702 |  5087 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5088 | `				SyHashEntry *pEntry;` |
|        - |  5089 | `				/* Extract the target attribute */` |
|      702 |  5090 | `				if( sName.nByte > 0 ){` |
|      702 |  5091 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|      702 |  5092 | `					if( pEntry ){` |
|        - |  5093 | `						/* Point to the attribute value */` |
|      700 |  5094 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|      349 |  5095 | `					}` |
|      350 |  5096 | `				}` |
|      702 |  5097 | `				if( pObjAttr == 0 ){` |
|        - |  5098 | `					/* No such attribute,load null */` |
|        4 |  5099 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5100 | `						&pClass->sName,&sName);` |
|        - |  5101 | `					/* Call the __get magic method if available */` |
|        3 |  5102 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5103 | `				}` |
|      702 |  5104 | `				VmPopOperand(&pTos,1);` |
|        - |  5105 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5106 | `				 * This is due to the following case:` |
|        - |  5107 | `				 *     (new TestClass())->foo;` |
|        - |  5108 | `				 */` |
|      702 |  5109 | `				pThis->iRef++;` |
|      702 |  5110 | `				PH7_MemObjRelease(pTos);` |
|      702 |  5111 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|      702 |  5112 | `				if( pObjAttr ){` |
|      700 |  5113 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5114 | `					/* Check attribute access */` |
|      700 |  5115 | `					if( VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5116 | `						/* Load attribute */` |
|      700 |  5117 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|      700 |  5118 | `						if( pValue ){` |
|      700 |  5119 | `							if( pThis->iRef < 2 ){` |
|        - |  5120 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5121 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5122 | `								 */` |
|        3 |  5123 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5124 | `							}else{` |
|        - |  5125 | `								/* Simple load */` |
|      698 |  5126 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5127 | `							}` |
|      700 |  5128 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|      698 |  5129 | `								if( pThis->iRef > 1 ){` |
|        - |  5130 | `									/* Load attribute index */` |
|      696 |  5131 | `									pTos->nIdx = pObjAttr->nIdx;` |
|      347 |  5132 | `								}` |
|      348 |  5133 | `							}` |
|      349 |  5134 | `						}` |
|      349 |  5135 | `					}` |
|      349 |  5136 | `				}` |
|        - |  5137 | `				/* Safely unreference the object */` |
|      702 |  5138 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5139 | `			}` |
|      411 |  5140 | `		}else{` |
|      ! 0 |  5141 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5142 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5143 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5144 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5145 | `		}` |
|      411 |  5146 | `	}else{` |
|        - |  5147 | `		/* Static member access using class name */` |
|       59 |  5148 | `		pNos = pTos;` |
|       59 |  5149 | `		pThis = 0;` |
|       59 |  5150 | `		if( !pInstr->p3 ){` |
|       57 |  5151 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       57 |  5152 | `			pNos--;` |
|        - |  5153 | `#ifdef UNTRUST` |
|        - |  5154 | `			if( pNos < pStack ){` |
|        - |  5155 | `				goto Abort;` |
|        - |  5156 | `			}` |
|        - |  5157 | `#endif` |
|       29 |  5158 | `		}else{` |
|        - |  5159 | `			/* Attribute name already computed */` |
|        3 |  5160 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5161 | `		}` |
|       59 |  5162 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|       59 |  5163 | `			ph7_class *pClass = 0;` |
|       59 |  5164 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5165 | `				/* Class already instantiated */` |
|      ! 0 |  5166 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5167 | `				pClass = pThis->pClass;` |
|      ! 0 |  5168 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5169 | `			}else{` |
|        - |  5170 | `				/* Try to extract the target class */` |
|       59 |  5171 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       88 |  5172 | `					pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pNos->sBlob),` |
|       29 |  5173 | `						SyBlobLength(&pNos->sBlob),FALSE,0);` |
|       29 |  5174 | `				}` |
|        - |  5175 | `			}` |
|       59 |  5176 | `			if( pClass == 0 ){` |
|        - |  5177 | `				/* Undefined class */` |
|      ! 0 |  5178 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5179 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5180 | `					);` |
|      ! 0 |  5181 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5182 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5183 | `				}` |
|      ! 0 |  5184 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5185 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5186 | `			}else{` |
|       59 |  5187 | `				if( pInstr->iP2 ){` |
|        - |  5188 | `					/* Method call */` |
|       25 |  5189 | `					ph7_class_method *pMeth = 0;` |
|       25 |  5190 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5191 | `						/* Extract the target method */` |
|       25 |  5192 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       12 |  5193 | `					}` |
|       25 |  5194 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5195 | `						if( pMeth ){` |
|      ! 0 |  5196 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5197 | `								&pClass->sName,&sName` |
|        - |  5198 | `								);` |
|      ! 0 |  5199 | `						}else{` |
|      ! 0 |  5200 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5201 | `								&pClass->sName,&sName` |
|        - |  5202 | `								);` |
|        - |  5203 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5204 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5205 | `						}` |
|        - |  5206 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5207 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5208 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5209 | `						}` |
|      ! 0 |  5210 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5211 | `					}else{` |
|        - |  5212 | `						/* Push method name on the stack */` |
|       25 |  5213 | `						PH7_MemObjRelease(pTos);` |
|       25 |  5214 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       25 |  5215 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5216 | `					}` |
|       25 |  5217 | `					pTos->nIdx = SXU32_HIGH;` |
|       13 |  5218 | `				}else{` |
|        - |  5219 | `					/* Attribute access */` |
|       35 |  5220 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5221 | `					/* Check for special ::class pseudo-constant */` |
|       49 |  5222 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       28 |  5223 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5224 | `						/* ::class returns the fully qualified class name */` |
|        - |  5225 | `						/* Pop the attribute name from the stack */` |
|       27 |  5226 | `						if( !pInstr->p3 ){` |
|       27 |  5227 | `							VmPopOperand(&pTos,1);` |
|       13 |  5228 | `						}` |
|       27 |  5229 | `						PH7_MemObjRelease(pTos);` |
|        - |  5230 | `						/* Load the class name */` |
|       27 |  5231 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       27 |  5232 | `						pTos->nIdx = SXU32_HIGH;` |
|       14 |  5233 | `					}else{` |
|        - |  5234 | `						/* Extract the target attribute */` |
|        9 |  5235 | `						if( sName.nByte > 0 ){` |
|        9 |  5236 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        4 |  5237 | `						}` |
|        9 |  5238 | `						if( pAttr == 0 ){` |
|        - |  5239 | `							/* No such attribute,load null */` |
|      ! 0 |  5240 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5241 | `								&pClass->sName,&sName);` |
|        - |  5242 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5243 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5244 | `						}` |
|        - |  5245 | `						/* Pop the attribute name from the stack */` |
|        9 |  5246 | `						if( !pInstr->p3 ){` |
|        7 |  5247 | `							VmPopOperand(&pTos,1);` |
|        3 |  5248 | `						}` |
|        9 |  5249 | `						PH7_MemObjRelease(pTos);` |
|        9 |  5250 | `						pTos->nIdx = SXU32_HIGH;` |
|        9 |  5251 | `						if( pAttr ){` |
|        9 |  5252 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5253 | `								/* Access to a non static attribute */` |
|      ! 0 |  5254 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5255 | `									&pClass->sName,&pAttr->sName` |
|        - |  5256 | `									);` |
|      ! 0 |  5257 | `							}else{` |
|        - |  5258 | `								ph7_value *pValue;` |
|        - |  5259 | `								/* Check if the access to the attribute is allowed */` |
|        9 |  5260 | `								if( VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5261 | `									/* Load the desired attribute */` |
|        9 |  5262 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|        9 |  5263 | `									if( pValue ){` |
|        9 |  5264 | `										PH7_MemObjLoad(pValue,pTos);` |
|        9 |  5265 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5266 | `											/* Load index number */` |
|        3 |  5267 | `											pTos->nIdx = pAttr->nIdx;` |
|        1 |  5268 | `										}` |
|        4 |  5269 | `									}` |
|        4 |  5270 | `								}` |
|        - |  5271 | `							}` |
|        4 |  5272 | `						}` |
|        - |  5273 | `					}` |
|        - |  5274 | `				}` |
|       59 |  5275 | `				if( pThis ){` |
|        - |  5276 | `					/* Safely unreference the object */` |
|      ! 0 |  5277 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5278 | `				}` |
|        - |  5279 | `			}` |
|       30 |  5280 | `		}else{` |
|        - |  5281 | `			/* Pop operands */` |
|      ! 0 |  5282 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5283 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5284 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5285 | `			}` |
|      ! 0 |  5286 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5287 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5288 | `		}` |
|        - |  5289 | `	}` |
|      878 |  5290 | `	break;` |
|        - |  5291 | `					}` |
|        - |  5292 | `/*` |
|        - |  5293 | ` * OP_NEW P1 * * *` |
|        - |  5294 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5295 | ` */` |
|      247 |  5296 | `case PH7_OP_NEW: {` |
|      496 |  5297 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      496 |  5298 | `	ph7_class *pClass = 0;` |
|        - |  5299 | `	ph7_class_instance *pNew;` |
|      496 |  5300 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5301 | `		/* Try to extract the desired class */` |
|      743 |  5302 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      494 |  5303 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      247 |  5304 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5305 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5306 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5307 | `	}` |
|      496 |  5308 | `	if( pClass == 0 ){` |
|        - |  5309 | `		/* No such class */` |
|      ! 0 |  5310 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined,PH7 is loading NULL",` |
|      ! 0 |  5311 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5312 | `			);` |
|      ! 0 |  5313 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5314 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5315 | `			/* Pop given arguments */` |
|      ! 0 |  5316 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5317 | `		}` |
|      ! 0 |  5318 | `	}else{` |
|        - |  5319 | `		ph7_class_method *pCons;` |
|        - |  5320 | `		/* Create a new class instance */` |
|      496 |  5321 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      496 |  5322 | `		if( pNew == 0 ){` |
|      ! 0 |  5323 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5324 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5325 | `				&pClass->sName` |
|        - |  5326 | `			);` |
|      ! 0 |  5327 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5328 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5329 | `				/* Pop given arguments */` |
|      ! 0 |  5330 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5331 | `			}` |
|      ! 0 |  5332 | `			break;` |
|        - |  5333 | `		}` |
|        - |  5334 | `		/* Check if a constructor is available */` |
|      496 |  5335 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      496 |  5336 | `		if( pCons == 0 ){` |
|      444 |  5337 | `			SyString *pName = &pClass->sName;` |
|        - |  5338 | `			/* Check for a constructor with the same base class name */` |
|      444 |  5339 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      221 |  5340 | `		}` |
|      496 |  5341 | `		if( pCons ){` |
|        - |  5342 | `			/* Call the class constructor */` |
|       54 |  5343 | `			SySetReset(&aArg);` |
|       96 |  5344 | `			while( pArg < pTos ){` |
|       44 |  5345 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       44 |  5346 | `				pArg++;` |
|        2 |  5347 | `			}` |
|       54 |  5348 | `			if( pVm->bErrReport ){` |
|        - |  5349 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5350 | `				sxu32 n;` |
|       12 |  5351 | `				n = SySetUsed(&aArg);` |
|        - |  5352 | `				/* Emit a notice for missing arguments */` |
|       28 |  5353 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       18 |  5354 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       18 |  5355 | `					if( pFuncArg ){` |
|       18 |  5356 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5357 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5358 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5359 | `						}` |
|        8 |  5360 | `					}` |
|       18 |  5361 | `					n++;` |
|        2 |  5362 | `				}` |
|        5 |  5363 | `			}` |
|       54 |  5364 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5365 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       54 |  5366 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5367 | `				pNew->iRef = 1;` |
|      ! 0 |  5368 | `			}` |
|       26 |  5369 | `		}` |
|      496 |  5370 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5371 | `			/* Pop given arguments */` |
|       38 |  5372 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       18 |  5373 | `		}` |
|      496 |  5374 | `		PH7_MemObjRelease(pTos);` |
|      496 |  5375 | `		pTos->x.pOther = pNew;` |
|      496 |  5376 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5377 | `	}` |
|      496 |  5378 | `	break;` |
|        - |  5379 | `				 }` |
|        - |  5380 | `/*` |
|        - |  5381 | ` * OP_CLONE * * *` |
|        - |  5382 | ` * Perfome a clone operation.` |
|        - |  5383 | ` */` |
|       23 |  5384 | `case PH7_OP_CLONE: {` |
|        - |  5385 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5386 | `#ifdef UNTRUST` |
|        - |  5387 | `	if( pTos < pStack ){` |
|        - |  5388 | `		goto Abort;` |
|        - |  5389 | `	}` |
|        - |  5390 | `#endif` |
|        - |  5391 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5392 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5393 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5394 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5395 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5396 | `		break;` |
|        - |  5397 | `	}` |
|        - |  5398 | `	/* Point to the source */` |
|       44 |  5399 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5400 | `	/* Perform the clone operation */` |
|       44 |  5401 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5402 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5403 | `	if( pClone == 0 ){` |
|      ! 0 |  5404 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5405 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5406 | `	}else{` |
|        - |  5407 | `		/* Load the cloned object */` |
|       44 |  5408 | `		pTos->x.pOther = pClone;` |
|       44 |  5409 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5410 | `	}` |
|       44 |  5411 | `	break;` |
|        - |  5412 | `				   }` |
|        - |  5413 | `/*` |
|        - |  5414 | ` * OP_SWITCH * * P3` |
|        - |  5415 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5416 | ` */` |
|       16 |  5417 | `case PH7_OP_SWITCH: {` |
|       34 |  5418 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5419 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5420 | `	ph7_value sValue,sCaseValue;` |
|        - |  5421 | `	sxu32 n,nEntry;` |
|        - |  5422 | `#ifdef UNTRUST` |
|        - |  5423 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5424 | `		goto Abort;` |
|        - |  5425 | `	}` |
|        - |  5426 | `#endif` |
|        - |  5427 | `	/* Point to the case table  */` |
|       34 |  5428 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       34 |  5429 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5430 | `	/* Select the appropriate case block to execute */` |
|       34 |  5431 | `	PH7_MemObjInit(pVm,&sValue);` |
|       34 |  5432 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       68 |  5433 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       68 |  5434 | `		pCase = &aCase[n];` |
|       68 |  5435 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5436 | `		/* Execute the case expression first */` |
|       68 |  5437 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5438 | `		/* Compare the two expression */` |
|       68 |  5439 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       68 |  5440 | `		PH7_MemObjRelease(&sValue);` |
|       68 |  5441 | `		PH7_MemObjRelease(&sCaseValue);` |
|       68 |  5442 | `		if( rc == 0 ){` |
|        - |  5443 | `			/* Value match,jump to this block */` |
|       34 |  5444 | `			pc = pCase->nStart - 1;` |
|       34 |  5445 | `			break;` |
|        - |  5446 | `		}` |
|       19 |  5447 | `	}` |
|       34 |  5448 | `	VmPopOperand(&pTos,1);` |
|       34 |  5449 | `	if( n >= nEntry ){` |
|        - |  5450 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5451 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5452 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5453 | `		}else{` |
|        - |  5454 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5455 | `			pc = pSwitch->nOut - 1;` |
|        - |  5456 | `		}` |
|      ! 0 |  5457 | `	}` |
|       34 |  5458 | `	break;` |
|        - |  5459 | `					}` |
|        - |  5460 | `/*` |
|        - |  5461 | ` * OP_CALL P1 * *` |
|        - |  5462 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5463 | ` *  function on the stack.` |
|        - |  5464 | ` */` |
|   205701 |  5465 | `case PH7_OP_CALL: {` |
|   411448 |  5466 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5467 | `	SyHashEntry *pEntry;` |
|        - |  5468 | `	SyString sName;` |
|        - |  5469 | `	/* Extract function name */` |
|   411448 |  5470 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5471 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5472 | `			ph7_value sResult;` |
|      ! 0 |  5473 | `			SySetReset(&aArg);` |
|      ! 0 |  5474 | `			while( pArg < pTos ){` |
|      ! 0 |  5475 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5476 | `				pArg++;` |
|      ! 0 |  5477 | `			}` |
|      ! 0 |  5478 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5479 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5480 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5481 | `			SySetReset(&aArg);` |
|        - |  5482 | `			/* Pop given arguments */` |
|      ! 0 |  5483 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5484 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5485 | `			}` |
|        - |  5486 | `			/* Copy result */` |
|      ! 0 |  5487 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5488 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5489 | `		}else{` |
|        3 |  5490 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5491 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5492 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5493 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5494 | `			}else{` |
|        - |  5495 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5496 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5497 | `			}` |
|        - |  5498 | `			/* Pop given arguments */` |
|        3 |  5499 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5500 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5501 | `			}` |
|        - |  5502 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5503 | `			PH7_MemObjRelease(pTos);` |
|        - |  5504 | `		}` |
|   205697 |  5505 | `		break;` |
|        - |  5506 | `	}` |
|   411446 |  5507 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5508 | `	/* Check for a compiled function first */` |
|   411446 |  5509 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|   411446 |  5510 | `	if( pEntry ){` |
|        - |  5511 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5512 | `		ph7_class_instance *pThis;` |
|        - |  5513 | `		ph7_value *pFrameStack;` |
|        - |  5514 | `		ph7_vm_func *pVmFunc;` |
|        - |  5515 | `		ph7_class *pSelf;` |
|        - |  5516 | `		VmFrame *pFrame;` |
|        - |  5517 | `		ph7_value *pObj;` |
|        - |  5518 | `		VmSlot sArg;` |
|        - |  5519 | `		sxu32 n;` |
|        - |  5520 | `		/* initialize fields */` |
|     7672 |  5521 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     7672 |  5522 | `		pThis = 0;` |
|     7672 |  5523 | `		pSelf = 0;` |
|     7672 |  5524 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5525 | `			ph7_class_method *pMeth;` |
|        - |  5526 | `			/* Class method call */` |
|      362 |  5527 | `			ph7_value *pTarget = &pTos[-1];` |
|      362 |  5528 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5529 | `				/* Extract the 'this' pointer */` |
|      362 |  5530 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5531 | `					/* Instance already loaded */` |
|      332 |  5532 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|      332 |  5533 | `					pThis->iRef++;` |
|      332 |  5534 | `					pSelf = pThis->pClass;` |
|      165 |  5535 | `				}` |
|      362 |  5536 | `				if( pSelf == 0 ){` |
|       31 |  5537 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5538 | `						/* "Late Static Binding" class name */` |
|       37 |  5539 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       12 |  5540 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       12 |  5541 | `					}` |
|       31 |  5542 | `					if( pSelf == 0 ){` |
|        7 |  5543 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        3 |  5544 | `					}` |
|       15 |  5545 | `				}` |
|      362 |  5546 | `				if( pThis == 0  ){` |
|       31 |  5547 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       33 |  5548 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5549 | `						/* Safely ignore the exception frame */` |
|        3 |  5550 | `						pFrameLocal = pFrameLocal->pParent;` |
|        1 |  5551 | `					}` |
|       31 |  5552 | `					if( pFrameLocal->pParent ){` |
|        - |  5553 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5554 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5555 | `						if( pThis ){` |
|       13 |  5556 | `							pThis->iRef++;` |
|        6 |  5557 | `						}` |
|        9 |  5558 | `					}` |
|       15 |  5559 | `				}` |
|      362 |  5560 | `				VmPopOperand(&pTos,1);` |
|      362 |  5561 | `				PH7_MemObjRelease(pTos);` |
|        - |  5562 | `				/* Synchronize pointers */` |
|      362 |  5563 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5564 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5565 | `				 * user have already computed the random generated unique class method name` |
|        - |  5566 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5567 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5568 | `				 */` |
|      362 |  5569 | `				while( pArg < pStack ){` |
|      ! 0 |  5570 | `					pArg++;` |
|      ! 0 |  5571 | `				}` |
|      362 |  5572 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5573 | `					/* Check if the call is allowed */` |
|      362 |  5574 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|      362 |  5575 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        5 |  5576 | `						if( !VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5577 | `							/* Pop given arguments */` |
|      ! 0 |  5578 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5579 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5580 | `							}` |
|        - |  5581 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5582 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5583 | `							break;` |
|        - |  5584 | `						}` |
|        2 |  5585 | `					}` |
|      180 |  5586 | `				}` |
|      180 |  5587 | `			}` |
|      180 |  5588 | `		}` |
|        - |  5589 | `		/* Check The recursion limit */` |
|     7672 |  5590 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5591 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5592 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5593 | `				&pVmFunc->sName);` |
|        - |  5594 | `			/* Pop given arguments */` |
|        3 |  5595 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5596 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5597 | `			}` |
|        - |  5598 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5599 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5600 | `			break;` |
|        - |  5601 | `		}` |
|     7670 |  5602 | `		if( pVmFunc->pNextName ){` |
|        - |  5603 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      129 |  5604 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       64 |  5605 | `		}` |
|        - |  5606 | `		/* Extract the formal argument set */` |
|     7670 |  5607 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5608 | `		/* Create a new VM frame  */` |
|     7670 |  5609 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|     7670 |  5610 | `		if( rc != SXRET_OK ){` |
|        - |  5611 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5612 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5613 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5614 | `				&pVmFunc->sName);` |
|        - |  5615 | `			/* Pop given arguments */` |
|      ! 0 |  5616 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5617 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5618 | `			}` |
|        - |  5619 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5620 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5621 | `			break;` |
|        - |  5622 | `		}` |
|     7670 |  5623 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5624 | `			/* Install the '$this' variable */` |
|        - |  5625 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|      342 |  5626 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|      342 |  5627 | `			if( pObj ){` |
|        - |  5628 | `				/* Reflect the change */` |
|      342 |  5629 | `				pObj->x.pOther = pThis;` |
|      342 |  5630 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      170 |  5631 | `			}` |
|      170 |  5632 | `		}` |
|     7670 |  5633 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5634 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5635 | `			/* Install static variables */` |
|      ! 0 |  5636 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5637 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5638 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5639 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5640 | `					/* Initialize the static variables */` |
|      ! 0 |  5641 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5642 | `					if( pObj ){` |
|        - |  5643 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5644 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5645 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5646 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5647 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5648 | `						}` |
|      ! 0 |  5649 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5650 | `					}else{` |
|      ! 0 |  5651 | `						continue;` |
|        - |  5652 | `					}` |
|      ! 0 |  5653 | `				}` |
|        - |  5654 | `				/* Install in the current frame */` |
|      ! 0 |  5655 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5656 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5657 | `			}` |
|      ! 0 |  5658 | `		}` |
|        - |  5659 | `		/* Push arguments in the local frame */` |
|     7670 |  5660 | `		n = 0;` |
|    21954 |  5661 | `		while( pArg < pTos ){` |
|    14286 |  5662 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    14186 |  5663 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5664 | `					/* NULL values are redirected to default arguments */` |
|      746 |  5665 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      746 |  5666 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5667 | `						goto Abort;` |
|        - |  5668 | `					}` |
|      372 |  5669 | `				}` |
|        - |  5670 | `				/* Make sure the given arguments are of the correct type */` |
|    14186 |  5671 | `				if( aFormalArg[n].nType > 0 ){` |
|      982 |  5672 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5673 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5674 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5675 | `						ph7_class *pClass;` |
|        - |  5676 | `						/* Try to extract the desired class */` |
|      ! 0 |  5677 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5678 | `						if( pClass ){` |
|      ! 0 |  5679 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5680 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5681 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5682 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5683 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5684 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5685 | `								}` |
|      ! 0 |  5686 | `							}else{` |
|        - |  5687 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  5688 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  5689 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  5690 | `								if( ! VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  5691 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5692 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5693 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5694 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5695 | `								}` |
|        - |  5696 | `							}` |
|      ! 0 |  5697 | `						}` |
|      982 |  5698 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5699 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5700 | `						/* Cast to the desired type */` |
|      ! 0 |  5701 | `						xCast(pArg);` |
|      ! 0 |  5702 | `					}` |
|      490 |  5703 | `				}` |
|    14186 |  5704 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5705 | `					/* Pass by reference */` |
|       25 |  5706 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  5707 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  5708 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  5709 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5710 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  5711 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  5712 | `						}` |
|        - |  5713 | `						/* Switch to pass by value */` |
|      ! 0 |  5714 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  5715 | `					}else{` |
|        - |  5716 | `						SyHashEntry *pRefEntry;` |
|        - |  5717 | `						/* Install the referenced variable in the private function frame */` |
|       25 |  5718 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       25 |  5719 | `						if( pRefEntry == 0 ){` |
|       37 |  5720 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       24 |  5721 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       25 |  5722 | `							sArg.nIdx = pArg->nIdx;` |
|       25 |  5723 | `							sArg.pUserData = 0;` |
|       25 |  5724 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       12 |  5725 | `						}` |
|       25 |  5726 | `						pObj = 0;` |
|        - |  5727 | `					}` |
|       13 |  5728 | `				}else{` |
|        - |  5729 | `					/* Pass by value,make a copy of the given argument */` |
|    14162 |  5730 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5731 | `				}` |
|     7094 |  5732 | `			}else{` |
|        - |  5733 | `				char zName[32];` |
|        - |  5734 | `				SyString sArgName;` |
|        - |  5735 | `				/* Set a dummy name */` |
|      101 |  5736 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      101 |  5737 | `				sArgName.zString = zName;` |
|        - |  5738 | `				/* Annonymous argument */` |
|      101 |  5739 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  5740 | `			}` |
|    14286 |  5741 | `			if( pObj ){` |
|    14262 |  5742 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  5743 | `				/* Insert argument index  */` |
|    14262 |  5744 | `				sArg.nIdx = pObj->nIdx;` |
|    14262 |  5745 | `				sArg.pUserData = 0;` |
|    14262 |  5746 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|     7130 |  5747 | `			}` |
|    14286 |  5748 | `			PH7_MemObjRelease(pArg);` |
|    14286 |  5749 | `			pArg++;` |
|    14286 |  5750 | `			++n;` |
|        2 |  5751 | `		}` |
|        - |  5752 | `		/* Set up closure environment */` |
|     7670 |  5753 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5754 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  5755 | `			ph7_value *pValue;` |
|        - |  5756 | `			sxu32 iEnv;` |
|        9 |  5757 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       25 |  5758 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       17 |  5759 | `				pEnv = &aEnv[iEnv];` |
|       17 |  5760 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  5761 | `					/* Do not install null value */` |
|        9 |  5762 | `					continue;` |
|        - |  5763 | `				}` |
|        9 |  5764 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|        9 |  5765 | `				if( pValue == 0 ){` |
|      ! 0 |  5766 | `					continue;` |
|        - |  5767 | `				}` |
|        - |  5768 | `				/* Invalidate any prior representation */` |
|        9 |  5769 | `				PH7_MemObjRelease(pValue);` |
|        - |  5770 | `				/* Duplicate bound variable value */` |
|        9 |  5771 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        5 |  5772 | `			}` |
|        4 |  5773 | `		}` |
|        - |  5774 | `		/* Process default values */` |
|     8470 |  5775 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|      802 |  5776 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|      792 |  5777 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      792 |  5778 | `				if( pObj ){` |
|        - |  5779 | `					/* Evaluate the default value and extract it's result */` |
|      792 |  5780 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|      792 |  5781 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5782 | `						goto Abort;` |
|        - |  5783 | `					}` |
|        - |  5784 | `					/* Insert argument index */` |
|      792 |  5785 | `					sArg.nIdx = pObj->nIdx;` |
|      792 |  5786 | `					sArg.pUserData = 0;` |
|      792 |  5787 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  5788 | `					/* Make sure the default argument is of the correct type */` |
|      792 |  5789 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5790 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5791 | `						/* Cast to the desired type */` |
|      ! 0 |  5792 | `						xCast(pObj);` |
|      ! 0 |  5793 | `					}` |
|      395 |  5794 | `				}` |
|      395 |  5795 | `			}` |
|      802 |  5796 | `			++n;` |
|        2 |  5797 | `		}` |
|        - |  5798 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  5799 | `		 * does not return anything.` |
|        - |  5800 | `		 */` |
|     7670 |  5801 | `		PH7_MemObjRelease(pTos);` |
|     7670 |  5802 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  5803 | `		/* Allocate a new operand stack and evaluate the function body */` |
|     7670 |  5804 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|     7670 |  5805 | `		if( pFrameStack == 0 ){` |
|        - |  5806 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5807 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5808 | `				&pVmFunc->sName);` |
|      ! 0 |  5809 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5810 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5811 | `			}` |
|      ! 0 |  5812 | `			break;` |
|        - |  5813 | `		}` |
|     7670 |  5814 | `		if( pSelf ){` |
|        - |  5815 | `			/* Push class name */` |
|      360 |  5816 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      179 |  5817 | `		}` |
|        - |  5818 | `		/* Increment nesting level */` |
|     7670 |  5819 | `		pVm->nRecursionDepth++;` |
|        - |  5820 | `		/* Execute function body */` |
|     7670 |  5821 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  5822 | `		/* Decrement nesting level */` |
|     7670 |  5823 | `		pVm->nRecursionDepth--;` |
|     7670 |  5824 | `		if( pSelf ){` |
|        - |  5825 | `			/* Pop class name */` |
|      360 |  5826 | `			(void)SySetPop(&pVm->aSelf);` |
|      179 |  5827 | `		}` |
|        - |  5828 | `		/* Cleanup the mess left behind */` |
|     7670 |  5829 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  5830 | `			/* Return by reference,reflect that */` |
|        9 |  5831 | `			if( n != SXU32_HIGH ){` |
|        9 |  5832 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  5833 | `				sxu32 i;` |
|        - |  5834 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  5835 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  5836 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  5837 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  5838 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5839 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5840 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  5841 | `								&pVmFunc->sName);` |
|      ! 0 |  5842 | `						}` |
|      ! 0 |  5843 | `						n = SXU32_HIGH;` |
|      ! 0 |  5844 | `						break;` |
|        - |  5845 | `					}` |
|        3 |  5846 | `				}` |
|        5 |  5847 | `			}else{` |
|      ! 0 |  5848 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5849 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5850 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  5851 | `						&pVmFunc->sName);` |
|      ! 0 |  5852 | `				}` |
|        - |  5853 | `			}` |
|        9 |  5854 | `			pTos->nIdx = n;` |
|        4 |  5855 | `		}` |
|        - |  5856 | `		/* Cleanup the mess left behind */` |
|     7670 |  5857 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  5858 | `			/* An exception was throw in this frame */` |
|        7 |  5859 | `			pFrame = pFrame->pParent;` |
|        7 |  5860 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  5861 | `				/* Pop the resutlt */` |
|        5 |  5862 | `				VmPopOperand(&pTos,1);` |
|        - |  5863 | `				/* Jump to this destination */` |
|        5 |  5864 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  5865 | `				rc = PH7_OK;` |
|        3 |  5866 | `			}else{` |
|        3 |  5867 | `				if( pFrame->pParent ){` |
|        3 |  5868 | `					rc = PH7_EXCEPTION;` |
|        2 |  5869 | `				}else{` |
|        - |  5870 | `					/* Continue normal execution */` |
|      ! 0 |  5871 | `					rc = PH7_OK;` |
|        - |  5872 | `				}` |
|        - |  5873 | `			}` |
|        3 |  5874 | `		}` |
|        - |  5875 | `		/* Free the operand stack */` |
|     7670 |  5876 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  5877 | `		/* Leave the frame */` |
|     7670 |  5878 | `		VmLeaveFrame(&(*pVm));` |
|     7670 |  5879 | `		if( rc == PH7_ABORT ){` |
|        - |  5880 | `			/* Abort processing immeditaley */` |
|      ! 0 |  5881 | `			goto Abort;` |
|     7670 |  5882 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5883 | `			goto Exception;` |
|        - |  5884 | `		}` |
|     3835 |  5885 | `	}else{` |
|        - |  5886 | `		ph7_user_func *pFunc;` |
|        - |  5887 | `		ph7_context sCtx;` |
|        - |  5888 | `		ph7_value sRet;` |
|        - |  5889 | `		/* Look for an installed foreign function */` |
|   403776 |  5890 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   403776 |  5891 | `		if( pEntry == 0 ){` |
|        - |  5892 | `			/* Call to undefined function */` |
|        5 |  5893 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  5894 | `			/* Pop given arguments */` |
|        5 |  5895 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5896 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5897 | `			}` |
|        - |  5898 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  5899 | `			PH7_MemObjRelease(pTos);` |
|        5 |  5900 | `			break;` |
|        - |  5901 | `		}` |
|   403772 |  5902 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  5903 | `		/* Start collecting function arguments */` |
|   403772 |  5904 | `		SySetReset(&aArg);` |
|  1096528 |  5905 | `		while( pArg < pTos ){` |
|   692758 |  5906 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   692758 |  5907 | `			pArg++;` |
|        2 |  5908 | `		}` |
|        - |  5909 | `		/* Assume a null return value */` |
|   403772 |  5910 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  5911 | `		/* Init the call context */` |
|   403772 |  5912 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  5913 | `		/* Call the foreign function */` |
|   403772 |  5914 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5915 | `		/* Release the call context */` |
|   403772 |  5916 | `		VmReleaseCallContext(&sCtx);` |
|   403772 |  5917 | `		if( rc == PH7_ABORT ){` |
|        9 |  5918 | `			goto Abort;` |
|   403764 |  5919 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5920 | `			goto Exception;` |
|        - |  5921 | `		}` |
|   403762 |  5922 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5923 | `			/* Pop function name and arguments */` |
|   389164 |  5924 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   194603 |  5925 | `		}` |
|        - |  5926 | `		/* Save foreign function return value */` |
|   403762 |  5927 | `		PH7_MemObjStore(&sRet,pTos);` |
|   403762 |  5928 | `		PH7_MemObjRelease(&sRet);` |
|        - |  5929 | `	}` |
|   411428 |  5930 | `	break;` |
|        - |  5931 | `				  }` |
|        - |  5932 | `/*` |
|        - |  5933 | ` * OP_CONSUME: P1 * *` |
|        - |  5934 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  5935 | ` */` |
|     8103 |  5936 | `case PH7_OP_CONSUME: {` |
|    16208 |  5937 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    16208 |  5938 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  5939 |  |
|    16208 |  5940 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    16208 |  5941 | `	pCur = pOut;` |
|        - |  5942 | `	/* Start the consume process  */` |
|    32438 |  5943 | `	while( pOut <= pTos ){` |
|        - |  5944 | `		/* Force a string cast */` |
|    16232 |  5945 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|       56 |  5946 | `			PH7_MemObjToString(pOut);` |
|       27 |  5947 | `		}` |
|    16232 |  5948 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  5949 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  5950 | `			/* Invoke the output consumer callback */` |
|     8794 |  5951 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|     8794 |  5952 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  5953 | `				/* Increment output length */` |
|     3330 |  5954 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     1664 |  5955 | `			}` |
|     8794 |  5956 | `			SyBlobRelease(&pOut->sBlob);` |
|     8794 |  5957 | `			if( rc == SXERR_ABORT ){` |
|        - |  5958 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  5959 | `				goto Abort;` |
|        - |  5960 | `			}` |
|     4396 |  5961 | `		}` |
|    16232 |  5962 | `		pOut++;` |
|        2 |  5963 | `	}` |
|    16208 |  5964 | `	pTos = &pCur[-1];` |
|    16206 |  5965 | `	break;` |
|        - |  5966 | `					 }` |
|        - |  5967 |  |
|        - |  5968 | `		} /* Switch() */` |
|  6577456 |  5969 | `		pc++; /* Next instruction in the stream */` |
|        2 |  5970 | `	} /* For(;;) */` |
|     9964 |  5971 | `Done:` |
|    19930 |  5972 | `	SySetRelease(&aArg);` |
|    19930 |  5973 | `	return SXRET_OK;` |
|        5 |  5974 | `Abort:` |
|       11 |  5975 | `	SySetRelease(&aArg);` |
|       29 |  5976 | `	while( pTos >= pStack ){` |
|       19 |  5977 | `		PH7_MemObjRelease(pTos);` |
|       19 |  5978 | `		pTos--;` |
|        1 |  5979 | `	}` |
|       11 |  5980 | `	return PH7_ABORT;` |
|        2 |  5981 | `Exception:` |
|        5 |  5982 | `	SySetRelease(&aArg);` |
|        9 |  5983 | `	while( pTos >= pStack ){` |
|        5 |  5984 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5985 | `		pTos--;` |
|        1 |  5986 | `	}` |
|        5 |  5987 | `	return PH7_EXCEPTION;` |
|     9973 |  5988 |  |
|        - |  5989 | `/*` |
|        - |  5990 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  5991 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  5992 | ` * See block-comment on that function for additional information.` |
|        - |  5993 | ` */` |
|    10822 |  5994 | `static sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  5995 |  |
|        - |  5996 | `	ph7_value *pStack;` |
|        - |  5997 | `	sxi32 rc;` |
|        - |  5998 | `	/* Allocate a new operand stack */` |
|    10824 |  5999 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    10824 |  6000 | `	if( pStack == 0 ){` |
|      ! 0 |  6001 | `		return SXERR_MEM;` |
|        - |  6002 | `	}` |
|        - |  6003 | `	/* Execute the program */` |
|    10824 |  6004 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  6005 | `	/* Free the operand stack */` |
|    10824 |  6006 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6007 | `	/* Execution result */` |
|    10824 |  6008 | `	return rc;` |
|     5413 |  6009 |  |
|        - |  6010 | `/*` |
|        - |  6011 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6012 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6013 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6014 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6015 | ` * execution ends.` |
|        - |  6016 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6017 | ` * additional information.` |
|        - |  6018 | ` */` |
|      936 |  6019 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6020 |  |
|        - |  6021 | `	VmShutdownCB *pEntry;` |
|        - |  6022 | `	ph7_value *apArg[10];` |
|        - |  6023 | `	sxu32 n,nEntry;` |
|        - |  6024 | `	int i;` |
|        - |  6025 | `	/* Point to the stack of registered callbacks */` |
|      938 |  6026 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    10298 |  6027 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|     9362 |  6028 | `		apArg[i] = 0;` |
|     4682 |  6029 | `	}` |
|      940 |  6030 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6031 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6032 | `		if( pEntry ){` |
|        - |  6033 | `			/* Prepare callback arguments if any */` |
|        3 |  6034 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6035 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6036 | `					break;` |
|        - |  6037 | `				}` |
|      ! 0 |  6038 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6039 | `			}` |
|        - |  6040 | `			/* Invoke the callback */` |
|        3 |  6041 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6042 | `			/*` |
|        - |  6043 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6044 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6045 | `			 */` |
|        3 |  6046 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6047 | `			if( pEntry ){` |
|        3 |  6048 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6049 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6050 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6051 | `				}` |
|        1 |  6052 | `			}` |
|        1 |  6053 | `		}` |
|        2 |  6054 | `	}` |
|      938 |  6055 | `	SySetReset(&pVm->aShutdown);` |
|      938 |  6056 |  |
|        - |  6057 | `/*` |
|        - |  6058 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6059 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6060 | ` * See block-comment on that function for additional information.` |
|        - |  6061 | ` */` |
|      944 |  6062 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6063 |  |
|        - |  6064 | `	/* Make sure we are ready to execute this program */` |
|      946 |  6065 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6066 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6067 | `	}` |
|        - |  6068 | `	/* Set the execution magic number  */` |
|      946 |  6069 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6070 | `	/* Execute the program */` |
|      946 |  6071 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6072 | `	/* Invoke any shutdown callbacks */` |
|      942 |  6073 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6074 | `	/*` |
|        - |  6075 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6076 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6077 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6078 | `	 */` |
|      942 |  6079 | `	return SXRET_OK;` |
|      474 |  6080 |  |
|        - |  6081 | `/*` |
|        - |  6082 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6083 | ` * the desired message.` |
|        - |  6084 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6085 | ` * in 'api.c' for additional information.` |
|        - |  6086 | ` */` |
|      380 |  6087 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6088 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6089 | `	SyString *pString /* Message to output */` |
|        - |  6090 | `	)` |
|        2 |  6091 |  |
|      382 |  6092 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      382 |  6093 | `	sxi32 rc = SXRET_OK;` |
|        - |  6094 | `	/* Call the output consumer */` |
|      382 |  6095 | `	if( pString->nByte > 0 ){` |
|      382 |  6096 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      382 |  6097 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6098 | `			/* Increment output length */` |
|       17 |  6099 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6100 | `		}` |
|      190 |  6101 | `	}` |
|      382 |  6102 | `	return rc;` |
|        2 |  6103 |  |
|        - |  6104 | `/*` |
|        - |  6105 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6106 | ` * callback to consume the formatted message.` |
|        - |  6107 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6108 | ` * in 'api.c' for additional information.` |
|        - |  6109 | ` */` |
|        2 |  6110 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6111 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6112 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6113 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6114 | `	)` |
|        1 |  6115 |  |
|        3 |  6116 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6117 | `	sxi32 rc = SXRET_OK;` |
|        - |  6118 | `	SyBlob sWorker;` |
|        - |  6119 | `	/* Format the message and call the output consumer */` |
|        3 |  6120 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6121 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6122 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6123 | `		/* Consume the formatted message */` |
|        3 |  6124 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6125 | `	}` |
|        3 |  6126 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6127 | `		/* Increment output length */` |
|      ! 0 |  6128 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6129 | `	}` |
|        - |  6130 | `	/* Release the working buffer */` |
|        3 |  6131 | `	SyBlobRelease(&sWorker);` |
|        3 |  6132 | `	return rc;` |
|        1 |  6133 |  |
|        - |  6134 | `/*` |
|        - |  6135 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6136 | ` * This function never fail and always return a pointer` |
|        - |  6137 | ` * to a null terminated string.` |
|        - |  6138 | ` */` |
|       10 |  6139 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6140 |  |
|       11 |  6141 | `	const char *zOp = "Unknown     ";` |
|       11 |  6142 | `	switch(nOp){` |
|        3 |  6143 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6144 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6145 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6146 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6147 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6148 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6149 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6150 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6151 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6152 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6153 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6154 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6155 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6156 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6157 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6158 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6159 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6160 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6161 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6162 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6163 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6164 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6165 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6166 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6167 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6168 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6169 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6170 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6171 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6172 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6173 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6174 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6175 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6176 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6177 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6178 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6179 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6180 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6181 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6182 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6183 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6184 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6185 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6186 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6187 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6188 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6189 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6190 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6191 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6192 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6193 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6194 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6195 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6196 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6197 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6198 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6199 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6200 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6201 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6202 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6203 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6204 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6205 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6206 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6207 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6208 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6209 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6210 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6211 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6212 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6213 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6214 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6215 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6216 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6217 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6218 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6219 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6220 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6221 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6222 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6223 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6224 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6225 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6226 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6227 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6228 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6229 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6230 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6231 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6232 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6233 | `	default:` |
|      ! 0 |  6234 | `		break;` |
|        - |  6235 | `	}` |
|       11 |  6236 | `	return zOp;` |
|        1 |  6237 |  |
|        - |  6238 | `/*` |
|        - |  6239 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6240 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6241 | ` * is responsible of consuming the generated dump.` |
|        - |  6242 | ` */` |
|        2 |  6243 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6244 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6245 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6246 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6247 | `	)` |
|        1 |  6248 |  |
|        - |  6249 | `	sxi32 rc;` |
|        3 |  6250 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6251 | `	return rc;` |
|        1 |  6252 |  |
|        - |  6253 | `/*` |
|        - |  6254 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6255 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6256 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6257 | ` * in 'compile.c' for additional information.` |
|        - |  6258 | ` */` |
|        8 |  6259 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6260 |  |
|        9 |  6261 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6262 | `	/* Evaluate and expand constant value */` |
|        9 |  6263 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6264 |  |
|        - |  6265 | `/*` |
|        - |  6266 | ` * Section:` |
|        - |  6267 | ` *  Function handling functions.` |
|        - |  6268 | ` * Status:` |
|        - |  6269 | ` *    Stable.` |
|        - |  6270 | ` */` |
|        - |  6271 | `/*` |
|        - |  6272 | ` * int func_num_args(void)` |
|        - |  6273 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6274 | ` * Parameters` |
|        - |  6275 | ` *   None.` |
|        - |  6276 | ` * Return` |
|        - |  6277 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6278 | ` *  or -1 if called from the globe scope.` |
|        - |  6279 | ` */` |
|      750 |  6280 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6281 |  |
|        - |  6282 | `	VmFrame *pFrame;` |
|        - |  6283 | `	ph7_vm *pVm;` |
|        - |  6284 | `	/* Point to the target VM */` |
|      752 |  6285 | `	pVm = pCtx->pVm;` |
|        - |  6286 | `	/* Current frame */` |
|      752 |  6287 | `	pFrame = pVm->pFrame;` |
|      752 |  6288 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6289 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6290 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6291 | `	}` |
|      752 |  6292 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6293 | `		SXUNUSED(nArg);` |
|      ! 0 |  6294 | `		SXUNUSED(apArg);` |
|        - |  6295 | `		/* Global frame,return -1 */` |
|      ! 0 |  6296 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6297 | `		return SXRET_OK;` |
|        - |  6298 | `	}` |
|        - |  6299 | `	/* Total number of arguments passed to the enclosing function */` |
|      752 |  6300 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      752 |  6301 | `	ph7_result_int(pCtx,nArg);` |
|      752 |  6302 | `	return SXRET_OK;` |
|      377 |  6303 |  |
|        - |  6304 | `/*` |
|        - |  6305 | ` * value func_get_arg(int $arg_num)` |
|        - |  6306 | ` *   Return an item from the argument list.` |
|        - |  6307 | ` * Parameters` |
|        - |  6308 | ` *  Argument number(index start from zero).` |
|        - |  6309 | ` * Return` |
|        - |  6310 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6311 | ` */` |
|        6 |  6312 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6313 |  |
|        8 |  6314 | `	ph7_value *pObj = 0;` |
|        8 |  6315 | `	VmSlot *pSlot = 0;` |
|        - |  6316 | `	VmFrame *pFrame;` |
|        - |  6317 | `	ph7_vm *pVm;` |
|        - |  6318 | `	/* Point to the target VM */` |
|        8 |  6319 | `	pVm = pCtx->pVm;` |
|        - |  6320 | `	/* Current frame */` |
|        8 |  6321 | `	pFrame = pVm->pFrame;` |
|        8 |  6322 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6323 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6324 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6325 | `	}` |
|        8 |  6326 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6327 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6328 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6329 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6330 | `		return SXRET_OK;` |
|        - |  6331 | `	}` |
|        - |  6332 | `	/* Extract the desired index */` |
|        5 |  6333 | `	nArg = ph7_value_to_int(apArg[0]);` |
|        5 |  6334 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6335 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6336 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6337 | `		return SXRET_OK;` |
|        - |  6338 | `	}` |
|        - |  6339 | `	/* Extract the desired argument */` |
|        5 |  6340 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|        5 |  6341 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6342 | `			/* Return the desired argument */` |
|        5 |  6343 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|        3 |  6344 | `		}else{` |
|        - |  6345 | `			/* No such argument,return false */` |
|      ! 0 |  6346 | `			ph7_result_bool(pCtx,0);` |
|        - |  6347 | `		}` |
|        3 |  6348 | `	}else{` |
|        - |  6349 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6350 | `		ph7_result_bool(pCtx,0);` |
|        - |  6351 | `	}` |
|        5 |  6352 | `	return SXRET_OK;` |
|        5 |  6353 |  |
|        - |  6354 | `/*` |
|        - |  6355 | ` * array func_get_args_byref(void)` |
|        - |  6356 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6357 | ` * Parameters` |
|        - |  6358 | ` *  None.` |
|        - |  6359 | ` * Return` |
|        - |  6360 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6361 | ` *  member of the current user-defined function's argument list.` |
|        - |  6362 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6363 | ` * NOTE:` |
|        - |  6364 | ` *  Arguments are returned to the array by reference.` |
|        - |  6365 | ` */` |
|        2 |  6366 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6367 |  |
|        - |  6368 | `	ph7_value *pArray;` |
|        - |  6369 | `	VmFrame *pFrame;` |
|        - |  6370 | `	VmSlot *aSlot;` |
|        - |  6371 | `	sxu32 n;` |
|        - |  6372 | `	/* Point to the current frame */` |
|        3 |  6373 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6374 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6375 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6376 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6377 | `	}` |
|        3 |  6378 | `	if( pFrame->pParent == 0 ){` |
|        - |  6379 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6380 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6381 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6382 | `		return SXRET_OK;` |
|        - |  6383 | `	}` |
|        - |  6384 | `	/* Create a new array */` |
|        3 |  6385 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6386 | `	if( pArray == 0 ){` |
|      ! 0 |  6387 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6388 | `		SXUNUSED(apArg);` |
|      ! 0 |  6389 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6390 | `		return SXRET_OK;` |
|        - |  6391 | `	}` |
|        - |  6392 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6393 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6394 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6395 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6396 | `	}` |
|        - |  6397 | `	/* Return the freshly created array */` |
|        3 |  6398 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6399 | `	return SXRET_OK;` |
|        2 |  6400 |  |
|        - |  6401 | `/*` |
|        - |  6402 | ` * array func_get_args(void)` |
|        - |  6403 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6404 | ` * Parameters` |
|        - |  6405 | ` *  None.` |
|        - |  6406 | ` * Return` |
|        - |  6407 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6408 | ` *  member of the current user-defined function's argument list.` |
|        - |  6409 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6410 | ` */` |
|       46 |  6411 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6412 |  |
|       47 |  6413 | `	ph7_value *pObj = 0;` |
|        - |  6414 | `	ph7_value *pArray;` |
|        - |  6415 | `	VmFrame *pFrame;` |
|        - |  6416 | `	VmSlot *aSlot;` |
|        - |  6417 | `	sxu32 n;` |
|        - |  6418 | `	/* Point to the current frame */` |
|       47 |  6419 | `	pFrame = pCtx->pVm->pFrame;` |
|       47 |  6420 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6421 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6422 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6423 | `	}` |
|       47 |  6424 | `	if( pFrame->pParent == 0 ){` |
|        - |  6425 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6426 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6427 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6428 | `		return SXRET_OK;` |
|        - |  6429 | `	}` |
|        - |  6430 | `	/* Create a new array */` |
|       47 |  6431 | `	pArray = ph7_context_new_array(pCtx);` |
|       47 |  6432 | `	if( pArray == 0 ){` |
|      ! 0 |  6433 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6434 | `		SXUNUSED(apArg);` |
|      ! 0 |  6435 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6436 | `		return SXRET_OK;` |
|        - |  6437 | `	}` |
|        - |  6438 | `	/* Start filling the array with the given arguments */` |
|       47 |  6439 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      143 |  6440 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|       97 |  6441 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|       97 |  6442 | `		if( pObj ){` |
|       97 |  6443 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       48 |  6444 | `		}` |
|       49 |  6445 | `	}` |
|        - |  6446 | `	/* Return the freshly created array */` |
|       47 |  6447 | `	ph7_result_value(pCtx,pArray);` |
|       47 |  6448 | `	return SXRET_OK;` |
|       24 |  6449 |  |
|        - |  6450 | `/*` |
|        - |  6451 | ` * bool function_exists(string $name)` |
|        - |  6452 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6453 | ` * Parameters` |
|        - |  6454 | ` *  The name of the desired function.` |
|        - |  6455 | ` * Return` |
|        - |  6456 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6457 | ` */` |
|     1728 |  6458 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6459 |  |
|        - |  6460 | `	const char *zName;` |
|        - |  6461 | `	ph7_vm *pVm;` |
|        - |  6462 | `	int nLen;` |
|        - |  6463 | `	int res;` |
|     1730 |  6464 | `	if( nArg < 1 ){` |
|        - |  6465 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6466 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6467 | `		return SXRET_OK;` |
|        - |  6468 | `	}` |
|        - |  6469 | `	/* Point to the target VM */` |
|     1730 |  6470 | `	pVm = pCtx->pVm;` |
|        - |  6471 | `	/* Extract the function name */` |
|     1730 |  6472 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6473 | `	/* Assume the function is not defined */` |
|     1730 |  6474 | `	res = 0;` |
|        - |  6475 | `	/* Perform the lookup */` |
|     2592 |  6476 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1724 |  6477 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6478 | `			/* Function is defined */` |
|      212 |  6479 | `			res = 1;` |
|      105 |  6480 | `	}` |
|     1730 |  6481 | `	ph7_result_bool(pCtx,res);` |
|     1730 |  6482 | `	return SXRET_OK;` |
|      866 |  6483 |  |
|        - |  6484 | `/* Forward declaration */` |
|        - |  6485 | `static ph7_class * VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg);` |
|        - |  6486 | `/*` |
|        - |  6487 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6488 | ` * [i.e: Whether it is callable or not].` |
|        - |  6489 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6490 | ` */` |
|    11352 |  6491 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6492 |  |
|    11354 |  6493 | `	int res = 0;` |
|    11354 |  6494 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6495 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6496 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6497 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6498 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6499 | `		if( pMethod && CallInvoke ){` |
|        - |  6500 | `			ph7_value sResult;` |
|        - |  6501 | `			sxi32 rc;` |
|        - |  6502 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6503 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6504 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6505 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6506 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6507 | `			}` |
|      ! 0 |  6508 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6509 | `		}` |
|    11354 |  6510 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  6511 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        7 |  6512 | `		if( pMap->nEntry > 1 ){` |
|        - |  6513 | `			ph7_class *pClass;` |
|        - |  6514 | `			ph7_value *pV;` |
|        - |  6515 | `			/* Extract the target class */` |
|        7 |  6516 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|        7 |  6517 | `			if( pV ){` |
|        7 |  6518 | `				pClass = VmExtractClassFromValue(pVm,pV);` |
|        7 |  6519 | `				if( pClass ){` |
|        - |  6520 | `					ph7_class_method *pMethod;` |
|        - |  6521 | `					/* Extract the target method */` |
|        7 |  6522 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|        7 |  6523 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6524 | `						/* Perform the lookup */` |
|        7 |  6525 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|        7 |  6526 | `						if( pMethod ){` |
|        - |  6527 | `							/* Method is callable */` |
|        5 |  6528 | `							res = 1;` |
|        2 |  6529 | `						}` |
|        3 |  6530 | `					}` |
|        3 |  6531 | `				}` |
|        3 |  6532 | `			}` |
|        4 |  6533 | `		}` |
|    11351 |  6534 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6535 | `		const char *zName;` |
|        - |  6536 | `		int nLen;` |
|        - |  6537 | `		/* Extract the name */` |
|     2784 |  6538 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6539 | `		/* Perform the lookup */` |
|     2787 |  6540 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|        6 |  6541 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6542 | `				/* Function is callable */` |
|     2780 |  6543 | `				res = 1;` |
|     1389 |  6544 | `		}` |
|     1391 |  6545 | `	}` |
|    11354 |  6546 | `	return res;` |
|        2 |  6547 |  |
|        - |  6548 | `/*` |
|        - |  6549 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6550 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6551 | ` * Parameters` |
|        - |  6552 | ` * $name` |
|        - |  6553 | ` *    The callback function to check` |
|        - |  6554 | ` * $syntax_only` |
|        - |  6555 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6556 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6557 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6558 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6559 | ` *    a string.` |
|        - |  6560 | ` * Return` |
|        - |  6561 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6562 | ` */` |
|       14 |  6563 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6564 |  |
|        - |  6565 | `	ph7_vm *pVm;` |
|        - |  6566 | `	int res;` |
|       15 |  6567 | `	if( nArg < 1 ){` |
|        - |  6568 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6569 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6570 | `		return SXRET_OK;` |
|        - |  6571 | `	}` |
|        - |  6572 | `	/* Point to the target VM */` |
|       15 |  6573 | `	pVm = pCtx->pVm;` |
|        - |  6574 | `	/* Perform the requested operation */` |
|       15 |  6575 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6576 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6577 | `	return SXRET_OK;` |
|        8 |  6578 |  |
|        - |  6579 | `/*` |
|        - |  6580 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6581 | ` * defined below.` |
|        - |  6582 | ` */` |
|     1040 |  6583 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6584 |  |
|     1041 |  6585 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6586 | `	ph7_value sName;` |
|        - |  6587 | `	sxi32 rc;` |
|        - |  6588 | `	/* Prepare the function name for insertion */` |
|     1041 |  6589 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1041 |  6590 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6591 | `	/* Perform the insertion */` |
|     1041 |  6592 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1041 |  6593 | `	PH7_MemObjRelease(&sName);` |
|     1041 |  6594 | `	return rc;` |
|        1 |  6595 |  |
|        - |  6596 | `/*` |
|        - |  6597 | ` * array get_defined_functions(void)` |
|        - |  6598 | ` *  Returns an array of all defined functions.` |
|        - |  6599 | ` * Parameter` |
|        - |  6600 | ` *  None.` |
|        - |  6601 | ` * Return` |
|        - |  6602 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6603 | ` *  both built-in (internal) and user-defined.` |
|        - |  6604 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6605 | ` *  defined ones using $arr["user"].` |
|        - |  6606 | ` * Note:` |
|        - |  6607 | ` *  NULL is returned on failure.` |
|        - |  6608 | ` */` |
|        2 |  6609 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6610 |  |
|        - |  6611 | `	ph7_value *pArray,*pEntry;` |
|        - |  6612 | `	/* NOTE:` |
|        - |  6613 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6614 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6615 | `	 */` |
|        3 |  6616 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6617 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6618 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6619 | `		SXUNUSED(apArg);` |
|        - |  6620 | `		/* Return NULL */` |
|      ! 0 |  6621 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6622 | `		return SXRET_OK;` |
|        - |  6623 | `	}` |
|        3 |  6624 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6625 | `	if( pEntry == 0 ){` |
|        - |  6626 | `		/* Return NULL */` |
|      ! 0 |  6627 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6628 | `		return SXRET_OK;` |
|        - |  6629 | `	}` |
|        - |  6630 | `	/* Fill with the appropriate information */` |
|        3 |  6631 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6632 | `	/* Create the 'internal' index */` |
|        3 |  6633 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6634 | `	/* Create the user-func array */` |
|        3 |  6635 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6636 | `	if( pEntry == 0 ){` |
|        - |  6637 | `		/* Return NULL */` |
|      ! 0 |  6638 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6639 | `		return SXRET_OK;` |
|        - |  6640 | `	}` |
|        - |  6641 | `	/* Fill with the appropriate information */` |
|        3 |  6642 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6643 | `	/* Create the 'user' index */` |
|        3 |  6644 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6645 | `	/* Return the multi-dimensional array */` |
|        3 |  6646 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6647 | `	return SXRET_OK;` |
|        2 |  6648 |  |
|        - |  6649 | `/*` |
|        - |  6650 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6651 | ` *  Register a function for execution on shutdown.` |
|        - |  6652 | ` * Note` |
|        - |  6653 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6654 | ` *  be called in the same order as they were registered.` |
|        - |  6655 | ` * Parameters` |
|        - |  6656 | ` *  $callback` |
|        - |  6657 | ` *   The shutdown callback to register.` |
|        - |  6658 | ` * $param` |
|        - |  6659 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6660 | ` * Return` |
|        - |  6661 | ` *  Nothing.` |
|        - |  6662 | ` */` |
|        2 |  6663 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6664 |  |
|        - |  6665 | `	VmShutdownCB sEntry;` |
|        - |  6666 | `	int i,j;` |
|        3 |  6667 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6668 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6669 | `		return PH7_OK;` |
|        - |  6670 | `	}` |
|        - |  6671 | `	/* Zero the Entry */` |
|        3 |  6672 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6673 | `	/* Initialize fields */` |
|        3 |  6674 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6675 | `	/* Save the callback name for later invocation name */` |
|        3 |  6676 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6677 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6678 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6679 | `	}` |
|        - |  6680 | `	/* Copy arguments */` |
|        3 |  6681 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6682 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6683 | `			/* Limit reached */` |
|      ! 0 |  6684 | `			break;` |
|        - |  6685 | `		}` |
|      ! 0 |  6686 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  6687 | `	}` |
|        3 |  6688 | `	sEntry.nArg = j;` |
|        - |  6689 | `	/* Install the callback */` |
|        3 |  6690 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  6691 | `	return PH7_OK;` |
|        2 |  6692 |  |
|        - |  6693 | `/*` |
|        - |  6694 | ` * Section:` |
|        - |  6695 | ` *  Class handling functions.` |
|        - |  6696 | ` * Status:` |
|        - |  6697 | ` *    Stable.` |
|        - |  6698 | ` */` |
|        - |  6699 | `/*` |
|        - |  6700 | ` * Extract the top active class. NULL is returned` |
|        - |  6701 | ` * if the class stack is empty.` |
|        - |  6702 | ` */` |
|       50 |  6703 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6704 |  |
|       52 |  6705 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6706 | `	ph7_class **apClass;` |
|       52 |  6707 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6708 | `		/* Empty stack,return NULL */` |
|       15 |  6709 | `		return 0;` |
|        - |  6710 | `	}` |
|        - |  6711 | `	/* Peek the last entry */` |
|       38 |  6712 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|       38 |  6713 | `	return apClass[pSet->nUsed - 1];` |
|       27 |  6714 |  |
|        - |  6715 | `/*` |
|        - |  6716 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  6717 | ` *   Get the class that declared the currently executing method.` |
|        - |  6718 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  6719 | ` *` |
|        - |  6720 | ` * Parameters` |
|        - |  6721 | ` *   pVm: Target VM` |
|        - |  6722 | ` *` |
|        - |  6723 | ` * Return` |
|        - |  6724 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  6725 | ` *   - Not executing within a class method` |
|        - |  6726 | ` *` |
|        - |  6727 | ` * Note` |
|        - |  6728 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  6729 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  6730 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  6731 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  6732 | ` *   declaring class.` |
|        - |  6733 | ` */` |
|       18 |  6734 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        1 |  6735 |  |
|       19 |  6736 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  6737 | `	ph7_vm_func *pVmFunc;` |
|        - |  6738 |  |
|        - |  6739 | `	/* Skip exception frames to find the actual method frame */` |
|       19 |  6740 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  6741 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6742 | `	}` |
|        - |  6743 |  |
|        - |  6744 | `	/* Check if we're in a method context */` |
|       19 |  6745 | `	if( pFrame->pParent ){` |
|       15 |  6746 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       15 |  6747 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  6748 | `			/* Return the declaring class */` |
|       15 |  6749 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  6750 | `		}` |
|      ! 0 |  6751 | `	}` |
|        - |  6752 |  |
|        5 |  6753 | `	return 0;` |
|       10 |  6754 |  |
|        - |  6755 |  |
|        - |  6756 | `/*` |
|        - |  6757 | ` * string get_class ([ object $object = NULL ] )` |
|        - |  6758 | ` *   Returns the name of the class of an object` |
|        - |  6759 | ` * Parameters` |
|        - |  6760 | ` *  object` |
|        - |  6761 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|        - |  6762 | ` * Return` |
|        - |  6763 | ` *  The name of the class of which object is an instance.` |
|        - |  6764 | ` *  Returns FALSE if object is not an object.` |
|        - |  6765 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|        - |  6766 | ` */` |
|       18 |  6767 | `static int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6768 |  |
|        - |  6769 | `	ph7_class *pClass;` |
|        - |  6770 | `	SyString *pName;` |
|       20 |  6771 | `	if( nArg < 1 ){` |
|        - |  6772 | `		/* Check if we are inside a class */` |
|      ! 0 |  6773 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|      ! 0 |  6774 | `		if( pClass ){` |
|        - |  6775 | `			/* Point to the class name */` |
|      ! 0 |  6776 | `			pName = &pClass->sName;` |
|      ! 0 |  6777 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|      ! 0 |  6778 | `		}else{` |
|        - |  6779 | `			/* Not inside class,return FALSE */` |
|      ! 0 |  6780 | `			ph7_result_bool(pCtx,0);` |
|        - |  6781 | `		}` |
|      ! 0 |  6782 | `	}else{` |
|        - |  6783 | `		/* Extract the target class */` |
|       20 |  6784 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       20 |  6785 | `		if( pClass ){` |
|       18 |  6786 | `			pName = &pClass->sName;` |
|        - |  6787 | `			/* Return the class name */` |
|       18 |  6788 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|       10 |  6789 | `		}else{` |
|        - |  6790 | `			/* Not a class instance,return FALSE */` |
|        3 |  6791 | `			ph7_result_bool(pCtx,0);` |
|        - |  6792 | `		}` |
|        - |  6793 | `	}` |
|       20 |  6794 | `	return PH7_OK;` |
|        2 |  6795 |  |
|        - |  6796 | `/*` |
|        - |  6797 | ` * string get_parent_class([object $object = NULL ] )` |
|        - |  6798 | ` *   Returns the name of the parent class of an object` |
|        - |  6799 | ` * Parameters` |
|        - |  6800 | ` *  object` |
|        - |  6801 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|        - |  6802 | ` * Return` |
|        - |  6803 | ` *  The name of the parent class of which object is an instance.` |
|        - |  6804 | ` *  Returns FALSE if object is not an object or if the object does` |
|        - |  6805 | ` *  not have a parent.` |
|        - |  6806 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|        - |  6807 | ` */` |
|        8 |  6808 | `static int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6809 |  |
|        - |  6810 | `	ph7_class *pClass;` |
|        - |  6811 | `	SyString *pName;` |
|        9 |  6812 | `	if( nArg < 1 ){` |
|        - |  6813 | `		/* Check if we are inside a class [i.e: a method call]*/` |
|        3 |  6814 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|        3 |  6815 | `		if( pClass && pClass->pBase ){` |
|        - |  6816 | `			/* Point to the class name */` |
|        3 |  6817 | `			pName = &pClass->pBase->sName;` |
|        3 |  6818 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        2 |  6819 | `		}else{` |
|        - |  6820 | `			/* Not inside class,return FALSE */` |
|      ! 0 |  6821 | `			ph7_result_bool(pCtx,0);` |
|        - |  6822 | `		}` |
|        2 |  6823 | `	}else{` |
|        - |  6824 | `		/* Extract the target class */` |
|        7 |  6825 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        7 |  6826 | `		if( pClass ){` |
|        7 |  6827 | `			if( pClass->pBase ){` |
|        5 |  6828 | `				pName = &pClass->pBase->sName;` |
|        - |  6829 | `				/* Return the parent class name */` |
|        5 |  6830 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        3 |  6831 | `			}else{` |
|        - |  6832 | `				/* Object does not have a parent class */` |
|        3 |  6833 | `				ph7_result_bool(pCtx,0);` |
|        - |  6834 | `			}` |
|        4 |  6835 | `		}else{` |
|        - |  6836 | `			/* Not a class instance,return FALSE */` |
|      ! 0 |  6837 | `			ph7_result_bool(pCtx,0);` |
|        - |  6838 | `		}` |
|        - |  6839 | `	}` |
|        9 |  6840 | `	return PH7_OK;` |
|        1 |  6841 |  |
|        - |  6842 | `/*` |
|        - |  6843 | ` * string get_called_class(void)` |
|        - |  6844 | ` *   Gets the name of the class the static method is called in.` |
|        - |  6845 | ` * Parameters` |
|        - |  6846 | ` *  None.` |
|        - |  6847 | ` * Return` |
|        - |  6848 | ` *  Returns the class name. Returns FALSE if called from outside a class.` |
|        - |  6849 | ` */` |
|        4 |  6850 | `static int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6851 |  |
|        - |  6852 | `	ph7_class *pClass;` |
|        - |  6853 | `	/* Check if we are inside a class [i.e: a method call] */` |
|        5 |  6854 | `	pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|        5 |  6855 | `	if( pClass ){` |
|        - |  6856 | `		SyString *pName;` |
|        - |  6857 | `		/* Point to the class name */` |
|        5 |  6858 | `		pName = &pClass->sName;` |
|        5 |  6859 | `		ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|        3 |  6860 | `	}else{` |
|      ! 0 |  6861 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6862 | `		SXUNUSED(apArg);` |
|        - |  6863 | `		/* Not inside class,return FALSE */` |
|      ! 0 |  6864 | `		ph7_result_bool(pCtx,0);` |
|        - |  6865 | `	}` |
|        5 |  6866 | `	return PH7_OK;` |
|        1 |  6867 |  |
|        - |  6868 | `/*` |
|        - |  6869 | ` * Extract a ph7_class from the given ph7_value.` |
|        - |  6870 | ` * The given value must be of type object [i.e: class instance] or` |
|        - |  6871 | ` * string which hold the class name.` |
|        - |  6872 | ` */` |
|       78 |  6873 | `static ph7_class * VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|        2 |  6874 |  |
|       80 |  6875 | `	ph7_class *pClass = 0;` |
|       80 |  6876 | `	if( ph7_value_is_object(pArg) ){` |
|        - |  6877 | `		/* Class instance already loaded,no need to perform a lookup */` |
|       44 |  6878 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|       59 |  6879 | `	}else if( ph7_value_is_string(pArg) ){` |
|        - |  6880 | `		const char *zClass;` |
|        - |  6881 | `		int nLen;` |
|        - |  6882 | `		/* Extract class name */` |
|       35 |  6883 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|       35 |  6884 | `		if( nLen > 0 ){` |
|        - |  6885 | `			SyHashEntry *pEntry;` |
|        - |  6886 | `			/* Perform a lookup */` |
|       35 |  6887 | `			pEntry = SyHashGet(&pVm->hClass,(const void *)zClass,(sxu32)nLen);` |
|       35 |  6888 | `			if( pEntry ){` |
|        - |  6889 | `				/* Point to the desired class */` |
|       31 |  6890 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|       15 |  6891 | `			}` |
|       17 |  6892 | `		}` |
|       17 |  6893 | `	}` |
|       80 |  6894 | `	return pClass;` |
|        2 |  6895 |  |
|        - |  6896 | `/*` |
|        - |  6897 | ` * bool property_exists(mixed $class,string $property)` |
|        - |  6898 | ` *   Checks if the object or class has a property.` |
|        - |  6899 | ` * Parameters` |
|        - |  6900 | ` *  class` |
|        - |  6901 | ` *   The class name or an object of the class to test for` |
|        - |  6902 | ` * property` |
|        - |  6903 | ` *  The name of the property` |
|        - |  6904 | ` * Return` |
|        - |  6905 | ` *   Returns TRUE if the property exists,FALSE otherwise.` |
|        - |  6906 | ` */` |
|       12 |  6907 | `static int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6908 |  |
|       13 |  6909 | `	int res = 0; /* Assume attribute does not exists */` |
|       13 |  6910 | `	if( nArg > 1 ){` |
|        - |  6911 | `		ph7_class *pClass;` |
|       13 |  6912 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       13 |  6913 | `		if( pClass ){` |
|        - |  6914 | `			const char *zName;` |
|        - |  6915 | `			int nLen;` |
|        - |  6916 | `			/* Extract attribute name */` |
|       13 |  6917 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|       13 |  6918 | `			if( nLen > 0 ){` |
|        - |  6919 | `				/* Perform the lookup in the attribute and method table */` |
|       12 |  6920 | `				if( SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen) != 0` |
|        8 |  6921 | `					\|\| SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6922 | `						/* property exists,flag that */` |
|       11 |  6923 | `						res = 1;` |
|        5 |  6924 | `				}` |
|        6 |  6925 | `			}` |
|        6 |  6926 | `		}` |
|        6 |  6927 | `	}` |
|       13 |  6928 | `	ph7_result_bool(pCtx,res);` |
|       13 |  6929 | `	return PH7_OK;` |
|        1 |  6930 |  |
|        - |  6931 | `/*` |
|        - |  6932 | ` * bool method_exists(mixed $class,string $method)` |
|        - |  6933 | ` *   Checks if the given method is a class member.` |
|        - |  6934 | ` * Parameters` |
|        - |  6935 | ` *  class` |
|        - |  6936 | ` *   The class name or an object of the class to test for` |
|        - |  6937 | ` * property` |
|        - |  6938 | ` *  The name of the method` |
|        - |  6939 | ` * Return` |
|        - |  6940 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|        - |  6941 | ` */` |
|        4 |  6942 | `static int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6943 |  |
|        5 |  6944 | `	int res = 0; /* Assume method does not exists */` |
|        5 |  6945 | `	if( nArg > 1 ){` |
|        - |  6946 | `		ph7_class *pClass;` |
|        5 |  6947 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        5 |  6948 | `		if( pClass ){` |
|        - |  6949 | `			const char *zName;` |
|        - |  6950 | `			int nLen;` |
|        - |  6951 | `			/* Extract method name */` |
|        5 |  6952 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|        5 |  6953 | `			if( nLen > 0 ){` |
|        - |  6954 | `				/* Perform the lookup in the method table */` |
|        5 |  6955 | `				if( SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6956 | `					/* method exists,flag that */` |
|        3 |  6957 | `					res = 1;` |
|        1 |  6958 | `				}` |
|        2 |  6959 | `			}` |
|        2 |  6960 | `		}` |
|        2 |  6961 | `	}` |
|        5 |  6962 | `	ph7_result_bool(pCtx,res);` |
|        5 |  6963 | `	return PH7_OK;` |
|        1 |  6964 |  |
|        - |  6965 | `/*` |
|        - |  6966 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|        - |  6967 | ` *   Checks if the class has been defined.` |
|        - |  6968 | ` * Parameters` |
|        - |  6969 | ` *  class_name` |
|        - |  6970 | ` *   The class name. The name is matched in a case-sensitive manner` |
|        - |  6971 | ` *   unlinke the standard PHP engine.` |
|        - |  6972 | ` *  autoload` |
|        - |  6973 | ` *   Whether or not to call __autoload by default.` |
|        - |  6974 | ` * Return` |
|        - |  6975 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|        - |  6976 | ` */` |
|       12 |  6977 | `static int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6978 |  |
|       14 |  6979 | `	int res = 0; /* Assume class does not exists */` |
|       14 |  6980 | `	if( nArg > 0 ){` |
|        - |  6981 | `		const char *zName;` |
|        - |  6982 | `		int nLen;` |
|        - |  6983 | `		/* Extract given name */` |
|       14 |  6984 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6985 | `		/* Perform a hashlookup */` |
|       14 |  6986 | `		if( nLen > 0 && SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6987 | `			/* class is available */` |
|       10 |  6988 | `			res = 1;` |
|        4 |  6989 | `		}` |
|        6 |  6990 | `	}` |
|       14 |  6991 | `	ph7_result_bool(pCtx,res);` |
|       14 |  6992 | `	return PH7_OK;` |
|        2 |  6993 |  |
|        - |  6994 | `/*` |
|        - |  6995 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|        - |  6996 | ` *   Checks if the interface has been defined.` |
|        - |  6997 | ` * Parameters` |
|        - |  6998 | ` *  class_name` |
|        - |  6999 | ` *   The class name. The name is matched in a case-sensitive manner` |
|        - |  7000 | ` *   unlinke the standard PHP engine.` |
|        - |  7001 | ` *  autoload` |
|        - |  7002 | ` *   Whether or not to call __autoload by default.` |
|        - |  7003 | ` * Return` |
|        - |  7004 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|        - |  7005 | ` */` |
|        6 |  7006 | `static int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7007 |  |
|        7 |  7008 | `	int res = 0; /* Assume class does not exists */` |
|        7 |  7009 | `	if( nArg > 0 ){` |
|        7 |  7010 | `		SyHashEntry *pEntry = 0;` |
|        - |  7011 | `		const char *zName;` |
|        - |  7012 | `		int nLen;` |
|        - |  7013 | `		/* Extract given name */` |
|        7 |  7014 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7015 | `		/* Perform a hashlookup */` |
|        7 |  7016 | `		if( nLen > 0 ){` |
|        7 |  7017 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|        3 |  7018 | `		}` |
|        7 |  7019 | `		if( pEntry ){` |
|        5 |  7020 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        5 |  7021 | `			while( pClass ){` |
|        5 |  7022 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |  7023 | `					/* interface is available */` |
|        5 |  7024 | `					res = 1;` |
|        5 |  7025 | `					break;` |
|        - |  7026 | `				}` |
|        - |  7027 | `				/* Next with the same name */` |
|      ! 0 |  7028 | `				pClass = pClass->pNextName;` |
|      ! 0 |  7029 | `			}` |
|        2 |  7030 | `		}` |
|        3 |  7031 | `	}` |
|        7 |  7032 | `	ph7_result_bool(pCtx,res);` |
|        7 |  7033 | `	return PH7_OK;` |
|        1 |  7034 |  |
|        - |  7035 | `/*` |
|        - |  7036 | ` * bool class_alias([string $original[,string $alias ]])` |
|        - |  7037 | ` *   Creates an alias for a class.` |
|        - |  7038 | ` * Parameters` |
|        - |  7039 | ` *  original` |
|        - |  7040 | ` *    The original class.` |
|        - |  7041 | ` *  alias` |
|        - |  7042 | ` *   The alias name for the class.` |
|        - |  7043 | ` * Return` |
|        - |  7044 | ` *   Returns TRUE on success or FALSE on failure.` |
|        - |  7045 | ` */` |
|        2 |  7046 | `static int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7047 |  |
|        - |  7048 | `	const char *zOld,*zNew;` |
|        - |  7049 | `	int nOldLen,nNewLen;` |
|        - |  7050 | `	SyHashEntry *pEntry;` |
|        - |  7051 | `	ph7_class *pClass;` |
|        - |  7052 | `	char *zDup;` |
|        - |  7053 | `	sxi32 rc;` |
|        3 |  7054 | `	if( nArg < 2 ){` |
|        - |  7055 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  7056 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7057 | `		return PH7_OK;` |
|        - |  7058 | `	}` |
|        - |  7059 | `	/* Extract old class name */` |
|        3 |  7060 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|        - |  7061 | `	/* Extract alias name */` |
|        3 |  7062 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|        3 |  7063 | `	if( nNewLen < 1 ){` |
|        - |  7064 | `		/* Invalid alias name,return FALSE */` |
|      ! 0 |  7065 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7066 | `		return PH7_OK;` |
|        - |  7067 | `	}` |
|        - |  7068 | `	/* Perform a hash lookup */` |
|        3 |  7069 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,(sxu32)nOldLen);` |
|        3 |  7070 | `	if( pEntry ==  0 ){` |
|        - |  7071 | `		/* No such class,return FALSE */` |
|      ! 0 |  7072 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7073 | `		return PH7_OK;` |
|        - |  7074 | `	}` |
|        - |  7075 | `	/* Point to the class */` |
|        3 |  7076 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7077 | `	/* Duplicate alias name */` |
|        3 |  7078 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,(sxu32)nNewLen);` |
|        3 |  7079 | `	if( zDup == 0 ){` |
|        - |  7080 | `		/* Out of memory,return FALSE */` |
|      ! 0 |  7081 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7082 | `		return PH7_OK;` |
|        - |  7083 | `	}` |
|        - |  7084 | `	/* Create the alias */` |
|        3 |  7085 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,(sxu32)nNewLen,pClass);` |
|        3 |  7086 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7087 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|      ! 0 |  7088 | `	}` |
|        3 |  7089 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|        3 |  7090 | `	return PH7_OK;` |
|        2 |  7091 |  |
|        - |  7092 | `/*` |
|        - |  7093 | ` * array get_declared_classes(void)` |
|        - |  7094 | ` *   Returns an array with the name of the defined classes` |
|        - |  7095 | ` * Parameters` |
|        - |  7096 | ` *  None` |
|        - |  7097 | ` * Return` |
|        - |  7098 | ` *   Returns an array of the names of the declared classes` |
|        - |  7099 | ` *   in the current script.` |
|        - |  7100 | ` * Note:` |
|        - |  7101 | ` *   NULL is returned on failure.` |
|        - |  7102 | ` */` |
|        2 |  7103 | `static int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7104 |  |
|        - |  7105 | `	ph7_value *pName,*pArray;` |
|        - |  7106 | `	SyHashEntry *pEntry;` |
|        - |  7107 | `	/* Create a new array first */` |
|        3 |  7108 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7109 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7110 | `	if( pArray == 0 \|\| pName == 0){` |
|      ! 0 |  7111 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7112 | `		SXUNUSED(apArg);` |
|        - |  7113 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7114 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7115 | `		return PH7_OK;` |
|        - |  7116 | `	}` |
|        - |  7117 | `	/* Fill the array with the defined classes */` |
|        3 |  7118 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|       50 |  7119 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|       47 |  7120 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7121 | `		/* Do not register classes defined as interfaces */` |
|       47 |  7122 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       41 |  7123 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|        - |  7124 | `			/* insert class name */` |
|       41 |  7125 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7126 | `			/* Reset the cursor */` |
|       41 |  7127 | `			ph7_value_reset_string_cursor(pName);` |
|       20 |  7128 | `		}` |
|        1 |  7129 | `	}` |
|        - |  7130 | `	/* Return the created array */` |
|        3 |  7131 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7132 | `	return PH7_OK;` |
|        2 |  7133 |  |
|        - |  7134 | `/*` |
|        - |  7135 | ` * array get_declared_interfaces(void)` |
|        - |  7136 | ` *   Returns an array with the name of the defined interfaces` |
|        - |  7137 | ` * Parameters` |
|        - |  7138 | ` *  None` |
|        - |  7139 | ` * Return` |
|        - |  7140 | ` *   Returns an array of the names of the declared interfaces` |
|        - |  7141 | ` *   in the current script.` |
|        - |  7142 | ` * Note:` |
|        - |  7143 | ` *   NULL is returned on failure.` |
|        - |  7144 | ` */` |
|        2 |  7145 | `static int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7146 |  |
|        - |  7147 | `	ph7_value *pName,*pArray;` |
|        - |  7148 | `	SyHashEntry *pEntry;` |
|        - |  7149 | `	/* Create a new array first */` |
|        3 |  7150 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7151 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7152 | `	if( pArray == 0 \|\| pName == 0 ){` |
|      ! 0 |  7153 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7154 | `		SXUNUSED(apArg);` |
|        - |  7155 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7156 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7157 | `		return PH7_OK;` |
|        - |  7158 | `	}` |
|        - |  7159 | `	/* Fill the array with the defined classes */` |
|        3 |  7160 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|       52 |  7161 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|       49 |  7162 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  7163 | `		/* Register classes defined as interfaces only */` |
|       49 |  7164 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        9 |  7165 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|        - |  7166 | `			/* insert interface name */` |
|        9 |  7167 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7168 | `			/* Reset the cursor */` |
|        9 |  7169 | `			ph7_value_reset_string_cursor(pName);` |
|        4 |  7170 | `		}` |
|        1 |  7171 | `	}` |
|        - |  7172 | `	/* Return the created array */` |
|        3 |  7173 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7174 | `	return PH7_OK;` |
|        2 |  7175 |  |
|        - |  7176 | `/*` |
|        - |  7177 | ` * array get_class_methods(string/object $class_name)` |
|        - |  7178 | ` *   Returns an array with the name of the class methods` |
|        - |  7179 | ` * Parameters` |
|        - |  7180 | ` *  class_name` |
|        - |  7181 | ` *  The class name or class instance` |
|        - |  7182 | ` * Return` |
|        - |  7183 | ` *  Returns an array of method names defined for the class specified by class_name.` |
|        - |  7184 | ` *  In case of an error, it returns NULL.` |
|        - |  7185 | ` * Note:` |
|        - |  7186 | ` *   NULL is returned on failure.` |
|        - |  7187 | ` */` |
|        6 |  7188 | `static int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7189 |  |
|        - |  7190 | `	ph7_value *pName,*pArray;` |
|        - |  7191 | `	SyHashEntry *pEntry;` |
|        - |  7192 | `	ph7_class *pClass;` |
|        - |  7193 | `	/* Extract the target class first */` |
|        7 |  7194 | `	pClass = 0;` |
|        7 |  7195 | `	if( nArg > 0 ){` |
|        7 |  7196 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        3 |  7197 | `	}` |
|        7 |  7198 | `	if( pClass == 0 ){` |
|        - |  7199 | `		/* No such class,return NULL */` |
|        3 |  7200 | `		ph7_result_null(pCtx);` |
|        3 |  7201 | `		return PH7_OK;` |
|        - |  7202 | `	}` |
|        - |  7203 | `	/* Create a new array  */` |
|        5 |  7204 | `	pArray = ph7_context_new_array(pCtx);` |
|        5 |  7205 | `	pName = ph7_context_new_scalar(pCtx);` |
|        5 |  7206 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7207 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7208 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7209 | `		return PH7_OK;` |
|        - |  7210 | `	}` |
|        - |  7211 | `	/* Fill the array with the defined methods */` |
|        5 |  7212 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|       17 |  7213 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       13 |  7214 | `		ph7_class_method *pMethod = (ph7_class_method *)pEntry->pUserData;` |
|        - |  7215 | `		/* Insert method name */` |
|       13 |  7216 | `		ph7_value_string(pName,SyStringData(&pMethod->sFunc.sName),(int)SyStringLength(&pMethod->sFunc.sName));` |
|       13 |  7217 | `		ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|        - |  7218 | `		/* Reset the cursor */` |
|       13 |  7219 | `		ph7_value_reset_string_cursor(pName);` |
|        1 |  7220 | `	}` |
|        - |  7221 | `	/* Return the created array */` |
|        5 |  7222 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7223 | `	/*` |
|        - |  7224 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7225 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7226 | `	 */` |
|        5 |  7227 | `	return PH7_OK;` |
|        4 |  7228 |  |
|        - |  7229 | `/*` |
|        - |  7230 | ` * This function return TRUE(1) if the given class attribute stored` |
|        - |  7231 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|        - |  7232 | ` * from the current scope.Otherwise FALSE is returned.` |
|        - |  7233 | ` */` |
|      740 |  7234 | `static int VmClassMemberAccess(` |
|        - |  7235 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7236 | `	ph7_class *pClass,         /* Target Class */` |
|        - |  7237 | `	const SyString *pAttrName, /* Attribute name */` |
|        - |  7238 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|        - |  7239 | `	int bLog                   /* TRUE to log forbidden access. */` |
|        - |  7240 | `	)` |
|        2 |  7241 |  |
|      742 |  7242 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|      178 |  7243 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  7244 | `		ph7_vm_func *pVmFunc;` |
|      182 |  7245 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|        - |  7246 | `			/* Safely ignore the exception frame */` |
|        5 |  7247 | `			pFrame = pFrame->pParent;` |
|        1 |  7248 | `		}` |
|      178 |  7249 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      178 |  7250 | `		if( pVmFunc == 0 \|\| (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|        9 |  7251 | `			goto dis; /* Access is forbidden */` |
|        - |  7252 | `		}` |
|      170 |  7253 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - |  7254 | `			/* Must be the same instance */` |
|        7 |  7255 | `			if( (ph7_class *)pVmFunc->pUserData != pClass ){` |
|      ! 0 |  7256 | `				goto dis; /* Access is forbidden */` |
|        - |  7257 | `			}` |
|        4 |  7258 | `		}else{` |
|        - |  7259 | `			/* Protected */` |
|      164 |  7260 | `			ph7_class *pBase = (ph7_class *)pVmFunc->pUserData;` |
|        - |  7261 | `			/* Must be a derived class */` |
|      164 |  7262 | `			if( !VmInstanceOf(pClass,pBase) ){` |
|      ! 0 |  7263 | `				goto dis; /* Access is forbidden */` |
|        - |  7264 | `			}` |
|        - |  7265 | `		}` |
|       84 |  7266 | `	}` |
|      734 |  7267 | `	return 1; /* Access is granted */` |
|        4 |  7268 | `dis:` |
|        9 |  7269 | `	if( bLog ){` |
|      ! 0 |  7270 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7271 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|      ! 0 |  7272 | `			&pClass->sName,pAttrName);` |
|      ! 0 |  7273 | `	}` |
|        9 |  7274 | `	return 0; /* Access is forbidden */` |
|      372 |  7275 |  |
|        - |  7276 | `/*` |
|        - |  7277 | ` * array get_class_vars(string/object $class_name)` |
|        - |  7278 | ` *   Get the default properties of the class` |
|        - |  7279 | ` * Parameters` |
|        - |  7280 | ` *  class_name` |
|        - |  7281 | ` *   The class name or class instance` |
|        - |  7282 | ` * Return` |
|        - |  7283 | ` *  Returns an associative array of declared properties visible from the current scope` |
|        - |  7284 | ` *  with their default value. The resulting array elements are in the form` |
|        - |  7285 | ` *  of varname => value.` |
|        - |  7286 | ` * Note:` |
|        - |  7287 | ` *   NULL is returned on failure.` |
|        - |  7288 | ` */` |
|        2 |  7289 | `static int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7290 |  |
|        - |  7291 | `	ph7_value *pName,*pArray,sValue;` |
|        - |  7292 | `	SyHashEntry *pEntry;` |
|        - |  7293 | `	ph7_class *pClass;` |
|        - |  7294 | `	/* Extract the target class first */` |
|        3 |  7295 | `	pClass = 0;` |
|        3 |  7296 | `	if( nArg > 0 ){` |
|        3 |  7297 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        1 |  7298 | `	}` |
|        3 |  7299 | `	if( pClass == 0 ){` |
|        - |  7300 | `		/* No such class,return NULL */` |
|      ! 0 |  7301 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7302 | `		return PH7_OK;` |
|        - |  7303 | `	}` |
|        - |  7304 | `	/* Create a new array  */` |
|        3 |  7305 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7306 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7307 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|        3 |  7308 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7309 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7310 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7311 | `		return PH7_OK;` |
|        - |  7312 | `	}` |
|        - |  7313 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|        3 |  7314 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        8 |  7315 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        5 |  7316 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|        - |  7317 | `		/* Check if the access is allowed */` |
|        5 |  7318 | `		if( VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        5 |  7319 | `			SyString *pAttrName = &pAttr->sName;` |
|        5 |  7320 | `			ph7_value *pValue = 0;` |
|        5 |  7321 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |  7322 | `				/* Extract static attribute value which is always computed */` |
|        5 |  7323 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|        3 |  7324 | `			}else{` |
|      ! 0 |  7325 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      ! 0 |  7326 | `					PH7_MemObjRelease(&sValue);` |
|        - |  7327 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|      ! 0 |  7328 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue);` |
|      ! 0 |  7329 | `					pValue = &sValue;` |
|      ! 0 |  7330 | `				}` |
|        - |  7331 | `			}` |
|        - |  7332 | `			/* Fill in the array */` |
|        5 |  7333 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|        5 |  7334 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|        - |  7335 | `			/* Reset the cursor */` |
|        5 |  7336 | `			ph7_value_reset_string_cursor(pName);` |
|        2 |  7337 | `		}` |
|        1 |  7338 | `	}` |
|        3 |  7339 | `	PH7_MemObjRelease(&sValue);` |
|        - |  7340 | `	/* Return the created array */` |
|        3 |  7341 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7342 | `	/*` |
|        - |  7343 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7344 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7345 | `	 */` |
|        3 |  7346 | `	return PH7_OK;` |
|        2 |  7347 |  |
|        - |  7348 | `/*` |
|        - |  7349 | ` * array get_object_vars(object $this)` |
|        - |  7350 | ` *   Gets the properties of the given object` |
|        - |  7351 | ` * Parameters` |
|        - |  7352 | ` *  this` |
|        - |  7353 | ` *   A class instance` |
|        - |  7354 | ` * Return` |
|        - |  7355 | ` *  Returns an associative array of defined object accessible non-static properties` |
|        - |  7356 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|        - |  7357 | ` *  it will be returned with a NULL value.` |
|        - |  7358 | ` * Note:` |
|        - |  7359 | ` *   NULL is returned on failure.` |
|        - |  7360 | ` */` |
|        2 |  7361 | `static int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7362 |  |
|        3 |  7363 | `	ph7_class_instance *pThis = 0;` |
|        - |  7364 | `	ph7_value *pName,*pArray;` |
|        - |  7365 | `	SyHashEntry *pEntry;` |
|        3 |  7366 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|        - |  7367 | `		/* Extract the target instance */` |
|        3 |  7368 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        1 |  7369 | `	}` |
|        3 |  7370 | `	if( pThis == 0 ){` |
|        - |  7371 | `		/* No such instance,return NULL */` |
|      ! 0 |  7372 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7373 | `		return PH7_OK;` |
|        - |  7374 | `	}` |
|        - |  7375 | `	/* Create a new array  */` |
|        3 |  7376 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7377 | `	pName = ph7_context_new_scalar(pCtx);` |
|        3 |  7378 | `	if( pArray == 0 \|\| pName == 0){` |
|        - |  7379 | `		/* Out of memory,return NULL */` |
|      ! 0 |  7380 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7381 | `		return PH7_OK;` |
|        - |  7382 | `	}` |
|        - |  7383 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|        3 |  7384 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  7385 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|        7 |  7386 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  7387 | `		SyString *pAttrName;` |
|        7 |  7388 | `		if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        - |  7389 | `			/* Only non-static/constant attributes are extracted */` |
|      ! 0 |  7390 | `			continue;` |
|        - |  7391 | `		}` |
|        7 |  7392 | `		pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  7393 | `		/* Check if the access is allowed */` |
|        7 |  7394 | `		if( VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|        3 |  7395 | `			ph7_value *pValue = 0;` |
|        - |  7396 | `			/* Extract attribute */` |
|        3 |  7397 | `			pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|        3 |  7398 | `			if( pValue ){` |
|        - |  7399 | `				/* Insert attribute name in the array */` |
|        3 |  7400 | `				ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|        3 |  7401 | `				ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|        1 |  7402 | `			}` |
|        - |  7403 | `			/* Reset the cursor */` |
|        3 |  7404 | `			ph7_value_reset_string_cursor(pName);` |
|        1 |  7405 | `		}` |
|        1 |  7406 | `	}` |
|        - |  7407 | `	/* Return the created array */` |
|        3 |  7408 | `	ph7_result_value(pCtx,pArray);` |
|        - |  7409 | `	/*` |
|        - |  7410 | `	 * Don't worry about freeing memory here,everything will be relased` |
|        - |  7411 | `	 * automatically as soon we return from this foreign function.` |
|        - |  7412 | `	 */` |
|        3 |  7413 | `	return PH7_OK;` |
|        2 |  7414 |  |
|        - |  7415 | `/*` |
|        - |  7416 | ` * This function returns TRUE if the given class is an implemented` |
|        - |  7417 | ` * interface.Otherwise FALSE is returned.` |
|        - |  7418 | ` */` |
|      158 |  7419 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|        2 |  7420 |  |
|        - |  7421 | `	ph7_class **apInterface;` |
|        - |  7422 | `	sxu32 n;` |
|      160 |  7423 | `	if( SySetUsed(pSet) < 1 ){` |
|        - |  7424 | `		/* Empty interface container */` |
|      158 |  7425 | `		return FALSE;` |
|        - |  7426 | `	}` |
|        - |  7427 | `	/* Point to the set of implemented interfaces */` |
|        3 |  7428 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|        - |  7429 | `	/* Perform the lookup */` |
|        3 |  7430 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
|        3 |  7431 | `		if( apInterface[n] == pClass ){` |
|        3 |  7432 | `			return TRUE;` |
|        - |  7433 | `		}` |
|      ! 0 |  7434 | `	}` |
|      ! 0 |  7435 | `	return FALSE;` |
|       81 |  7436 |  |
|        - |  7437 | `/*` |
|        - |  7438 | ` * This function returns TRUE if the given class (first argument)` |
|        - |  7439 | ` * is an instance of the main class (second argument).` |
|        - |  7440 | ` * Otherwise FALSE is returned.` |
|        - |  7441 | ` */` |
|      214 |  7442 | `static int VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|        2 |  7443 |  |
|        - |  7444 | `	ph7_class *pParent;` |
|        - |  7445 | `	sxi32 rc;` |
|      216 |  7446 | `	if( pThis == pClass ){` |
|        - |  7447 | `		/* Instance of the same class */` |
|      140 |  7448 | `		return TRUE;` |
|        - |  7449 | `	}` |
|        - |  7450 | `	/* Check implemented interfaces */` |
|       78 |  7451 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
|       78 |  7452 | `	if( rc ){` |
|        3 |  7453 | `		return TRUE;` |
|        - |  7454 | `	}` |
|        - |  7455 | `	/* Check parent classes */` |
|       76 |  7456 | `	pParent = pThis->pBase;` |
|      158 |  7457 | `	while( pParent ){` |
|      156 |  7458 | `		if( pParent == pClass ){` |
|        - |  7459 | `			/* Same instance */` |
|       74 |  7460 | `			return TRUE;` |
|        - |  7461 | `		}` |
|        - |  7462 | `		/* Check the implemented interfaces */` |
|       84 |  7463 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|       84 |  7464 | `		if( rc ){` |
|      ! 0 |  7465 | `			return TRUE;` |
|        - |  7466 | `		}` |
|        - |  7467 | `		/* Point to the parent class */` |
|       84 |  7468 | `		pParent = pParent->pBase;` |
|        2 |  7469 | `	}` |
|        - |  7470 | `	/* Not an instance of the the given class */` |
|        3 |  7471 | `	return FALSE;` |
|      109 |  7472 |  |
|        - |  7473 | `/*` |
|        - |  7474 | ` * This function returns TRUE if the given class (first argument)` |
|        - |  7475 | ` * is a subclass of the main class (second argument).` |
|        - |  7476 | ` * Otherwise FALSE is returned.` |
|        - |  7477 | ` */` |
|        4 |  7478 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|        1 |  7479 |  |
|        5 |  7480 | `	SySet *pInterface = &pClass->aInterface;` |
|        - |  7481 | `	SyHashEntry *pEntry;` |
|        - |  7482 | `	SyString *pName;` |
|        - |  7483 | `	sxi32 rc;` |
|        5 |  7484 | `	while( pClass ){` |
|        5 |  7485 | `		pName = &pClass->sName;` |
|        - |  7486 | `		/* Query the derived hashtable */` |
|        5 |  7487 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|        5 |  7488 | `		if( pEntry ){` |
|        5 |  7489 | `			return TRUE;` |
|        - |  7490 | `		}` |
|      ! 0 |  7491 | `		pClass = pClass->pBase;` |
|      ! 0 |  7492 | `	}` |
|      ! 0 |  7493 | `	rc = VmQueryInterfaceSet(pBase,pInterface);` |
|      ! 0 |  7494 | `	if( rc ){` |
|      ! 0 |  7495 | `		return TRUE;` |
|        - |  7496 | `	}` |
|        - |  7497 | `	/* Not a subclass */` |
|      ! 0 |  7498 | `	return FALSE;` |
|        3 |  7499 |  |
|        - |  7500 | `/*` |
|        - |  7501 | ` * bool is_a(object $object,string $class_name)` |
|        - |  7502 | ` *   Checks if the object is of this class or has this class as one of its parents.` |
|        - |  7503 | ` * Parameters` |
|        - |  7504 | ` *  object` |
|        - |  7505 | ` *   The tested object` |
|        - |  7506 | ` * class_name` |
|        - |  7507 | ` *  The class name` |
|        - |  7508 | ` * Return` |
|        - |  7509 | ` *   Returns TRUE if the object is of this class or has this class as one of its` |
|        - |  7510 | ` *   parents, FALSE otherwise.` |
|        - |  7511 | ` */` |
|        2 |  7512 | `static int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7513 |  |
|        3 |  7514 | `	int res = 0; /* Assume FALSE by default */` |
|        3 |  7515 | `	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){` |
|        3 |  7516 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7517 | `		ph7_class *pClass;` |
|        - |  7518 | `		/* Extract the given class */` |
|        3 |  7519 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|        3 |  7520 | `		if( pClass ){` |
|        - |  7521 | `			/* Perform the query */` |
|        3 |  7522 | `			res = VmInstanceOf(pThis->pClass,pClass);` |
|        1 |  7523 | `		}` |
|        1 |  7524 | `	}` |
|        - |  7525 | `	/* Query result */` |
|        3 |  7526 | `	ph7_result_bool(pCtx,res);` |
|        3 |  7527 | `	return PH7_OK;` |
|        1 |  7528 |  |
|        - |  7529 | `/*` |
|        - |  7530 | ` * bool is_subclass_of(object/string $object,object/string $class_name)` |
|        - |  7531 | ` *   Checks if the object has this class as one of its parents.` |
|        - |  7532 | ` * Parameters` |
|        - |  7533 | ` *  object` |
|        - |  7534 | ` *   The tested object` |
|        - |  7535 | ` * class_name` |
|        - |  7536 | ` *  The class name` |
|        - |  7537 | ` * Return` |
|        - |  7538 | ` *  This function returns TRUE if the object , belongs to a class` |
|        - |  7539 | ` *  which is a subclass of class_name, FALSE otherwise.` |
|        - |  7540 | ` */` |
|        6 |  7541 | `static int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7542 |  |
|        7 |  7543 | `	int res = 0; /* Assume FALSE by default */` |
|        7 |  7544 | `	if( nArg > 1 ){` |
|        - |  7545 | `		ph7_class *pClass,*pMain;` |
|        - |  7546 | `		/* Extract the given classes */` |
|        7 |  7547 | `		pClass = VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|        7 |  7548 | `		pMain = VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|        7 |  7549 | `		if( pClass && pMain ){` |
|        - |  7550 | `			/* Perform the query */` |
|        5 |  7551 | `			res = VmSubclassOf(pClass,pMain);` |
|        2 |  7552 | `		}` |
|        3 |  7553 | `	}` |
|        - |  7554 | `	/* Query result */` |
|        7 |  7555 | `	ph7_result_bool(pCtx,res);` |
|        7 |  7556 | `	return PH7_OK;` |
|        1 |  7557 |  |
|        - |  7558 | `/*` |
|        - |  7559 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  7560 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  7561 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  7562 | ` * return value indicates failure.` |
|        - |  7563 | ` */` |
|      218 |  7564 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  7565 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7566 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  7567 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  7568 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  7569 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  7570 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  7571 | `	)` |
|        2 |  7572 |  |
|        - |  7573 | `	ph7_value *aStack;` |
|        - |  7574 | `	VmInstr aInstr[2];` |
|        - |  7575 | `	int iCursor;` |
|        - |  7576 | `	int i;` |
|        - |  7577 | `	/* Create a new operand stack */` |
|      220 |  7578 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|      220 |  7579 | `	if( aStack == 0 ){` |
|      ! 0 |  7580 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7581 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  7582 | `		return SXERR_MEM;` |
|        - |  7583 | `	}` |
|        - |  7584 | `	/* Fill the operand stack with the given arguments */` |
|      300 |  7585 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       82 |  7586 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7587 | `		/*` |
|        - |  7588 | `		 * Symisc eXtension:` |
|        - |  7589 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7590 | `		 */` |
|       82 |  7591 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|       42 |  7592 | `	}` |
|      220 |  7593 | `	iCursor = nArg + 1;` |
|      220 |  7594 | `	if( pThis ){` |
|        - |  7595 | `		/*` |
|        - |  7596 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  7597 | `		 */` |
|      214 |  7598 | `		pThis->iRef++; /* Increment reference count */` |
|      214 |  7599 | `		aStack[i].x.pOther = pThis;` |
|      214 |  7600 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      106 |  7601 | `	}` |
|      220 |  7602 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|      220 |  7603 | `	i++;` |
|        - |  7604 | `	/* Push method name */` |
|      220 |  7605 | `	SyBlobReset(&aStack[i].sBlob);` |
|      220 |  7606 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|      220 |  7607 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|      220 |  7608 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  7609 | `	/* Emit the CALL istruction */` |
|      220 |  7610 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      220 |  7611 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      220 |  7612 | `	aInstr[0].iP2 = 0;` |
|      220 |  7613 | `	aInstr[0].p3  = 0;` |
|        - |  7614 | `	/* Emit the DONE instruction */` |
|      220 |  7615 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      220 |  7616 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|      220 |  7617 | `	aInstr[1].iP2 = 0;` |
|      220 |  7618 | `	aInstr[1].p3  = 0;` |
|        - |  7619 | `	/* Execute the method body (if available) */` |
|      220 |  7620 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  7621 | `	/* Clean up the mess left behind */` |
|      220 |  7622 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      220 |  7623 | `	return PH7_OK;` |
|      111 |  7624 |  |
|        - |  7625 | `/*` |
|        - |  7626 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  7627 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  7628 | ` * in the apArg[] array.` |
|        - |  7629 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7630 | ` * return value indicates failure.` |
|        - |  7631 | ` */` |
|      316 |  7632 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  7633 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7634 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7635 | `	int nArg,          /* Total number of given arguments */` |
|        - |  7636 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  7637 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  7638 | `	)` |
|        2 |  7639 |  |
|        - |  7640 | `	ph7_value *aStack;` |
|        - |  7641 | `	VmInstr aInstr[2];` |
|        - |  7642 | `	int i;` |
|      318 |  7643 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  7644 | `		/* Don't bother processing,it's invalid anyway */` |
|        9 |  7645 | `		if( pResult ){` |
|        - |  7646 | `			/* Assume a null return value */` |
|      ! 0 |  7647 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7648 | `		}` |
|        9 |  7649 | `		return SXERR_INVALID;` |
|        - |  7650 | `	}` |
|      310 |  7651 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7652 | `		/* Class method */` |
|       11 |  7653 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  7654 | `		ph7_class_method *pMethod = 0;` |
|       11 |  7655 | `		ph7_class_instance *pThis = 0;` |
|       11 |  7656 | `		ph7_class *pClass = 0;` |
|        - |  7657 | `		ph7_value *pValue;` |
|        - |  7658 | `		sxi32 rc;` |
|       11 |  7659 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  7660 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  7661 | `			if( pResult ){` |
|        - |  7662 | `				/* Assume a null return value */` |
|      ! 0 |  7663 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7664 | `			}` |
|      ! 0 |  7665 | `			return SXRET_OK;` |
|        - |  7666 | `		}` |
|        - |  7667 | `		/* Extract the class name or an instance of it */` |
|       11 |  7668 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  7669 | `		if( pValue ){` |
|       11 |  7670 | `			pClass = VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  7671 | `		}` |
|       11 |  7672 | `		if( pClass == 0 ){` |
|        - |  7673 | `			/* No such class,return NULL */` |
|      ! 0 |  7674 | `			if( pResult ){` |
|      ! 0 |  7675 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7676 | `			}` |
|      ! 0 |  7677 | `			return SXRET_OK;` |
|        - |  7678 | `		}` |
|       11 |  7679 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  7680 | `			/* Point to the class instance */` |
|        5 |  7681 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  7682 | `		}` |
|        - |  7683 | `		/* Try to extract the method */` |
|       11 |  7684 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  7685 | `		if( pValue ){` |
|       11 |  7686 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  7687 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  7688 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  7689 | `			}` |
|        5 |  7690 | `		}` |
|       11 |  7691 | `		if( pMethod == 0 ){` |
|        - |  7692 | `			/* No such method,return NULL */` |
|      ! 0 |  7693 | `			if( pResult ){` |
|      ! 0 |  7694 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7695 | `			}` |
|      ! 0 |  7696 | `			return SXRET_OK;` |
|        - |  7697 | `		}` |
|        - |  7698 | `		/* Call the class method */` |
|       11 |  7699 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  7700 | `		return rc;` |
|        - |  7701 | `	}` |
|        - |  7702 | `	/* Create a new operand stack */` |
|      300 |  7703 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      300 |  7704 | `	if( aStack == 0 ){` |
|      ! 0 |  7705 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7706 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  7707 | `		if( pResult ){` |
|        - |  7708 | `			/* Assume a null return value */` |
|      ! 0 |  7709 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7710 | `		}` |
|      ! 0 |  7711 | `		return SXERR_MEM;` |
|        - |  7712 | `	}` |
|        - |  7713 | `	/* Fill the operand stack with the given arguments */` |
|      928 |  7714 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      629 |  7715 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7716 | `		/*` |
|        - |  7717 | `		 * Symisc eXtension:` |
|        - |  7718 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7719 | `		 */` |
|      629 |  7720 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      315 |  7721 | `	}` |
|        - |  7722 | `	/* Push the function name */` |
|      300 |  7723 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      300 |  7724 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7725 | `	/* Emit the CALL istruction */` |
|      300 |  7726 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      300 |  7727 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      300 |  7728 | `	aInstr[0].iP2 = 0;` |
|      300 |  7729 | `	aInstr[0].p3  = 0;` |
|        - |  7730 | `	/* Emit the DONE instruction */` |
|      300 |  7731 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      300 |  7732 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      300 |  7733 | `	aInstr[1].iP2 = 0;` |
|      300 |  7734 | `	aInstr[1].p3  = 0;` |
|        - |  7735 | `	/* Execute the function body (if available) */` |
|      300 |  7736 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  7737 | `	/* Clean up the mess left behind */` |
|      300 |  7738 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      300 |  7739 | `	return PH7_OK;` |
|      160 |  7740 |  |
|        - |  7741 | `/*` |
|        - |  7742 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  7743 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  7744 | ` * parameter.` |
|        - |  7745 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7746 | ` * return value indicates failure.` |
|        - |  7747 | ` */` |
|      190 |  7748 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  7749 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7750 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7751 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  7752 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  7753 | `	)` |
|        1 |  7754 |  |
|        - |  7755 | `	ph7_value *pArg;` |
|        - |  7756 | `	SySet aArg;` |
|        - |  7757 | `	va_list ap;` |
|        - |  7758 | `	sxi32 rc;` |
|      191 |  7759 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7760 | `	/* Copy arguments one after one */` |
|      191 |  7761 | `	va_start(ap,pResult);` |
|      319 |  7762 | `	for(;;){` |
|      639 |  7763 | `		pArg = va_arg(ap,ph7_value *);` |
|      639 |  7764 | `		if( pArg == 0 ){` |
|      191 |  7765 | `			break;` |
|        - |  7766 | `		}` |
|      449 |  7767 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  7768 | `	}` |
|        - |  7769 | `	/* Call the core routine */` |
|      191 |  7770 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  7771 | `	/* Cleanup */` |
|      191 |  7772 | `	SySetRelease(&aArg);` |
|      191 |  7773 | `	return rc;` |
|        1 |  7774 |  |
|        - |  7775 | `/*` |
|        - |  7776 | ` * value call_user_func(callable $callback[,value $parameter[, value $... ]])` |
|        - |  7777 | ` *  Call the callback given by the first parameter.` |
|        - |  7778 | ` * Parameter` |
|        - |  7779 | ` *  $callback` |
|        - |  7780 | ` *   The callable to be called.` |
|        - |  7781 | ` *  ...` |
|        - |  7782 | ` *    Zero or more parameters to be passed to the callback.` |
|        - |  7783 | ` * Return` |
|        - |  7784 | ` *  Th return value of the callback, or FALSE on error.` |
|        - |  7785 | ` */` |
|       14 |  7786 | `static int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7787 |  |
|        - |  7788 | `	ph7_value sResult; /* Store callback return value here */` |
|        - |  7789 | `	sxi32 rc;` |
|       15 |  7790 | `	if( nArg < 1 ){` |
|        - |  7791 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  7792 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7793 | `		return PH7_OK;` |
|        - |  7794 | `	}` |
|       15 |  7795 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|       15 |  7796 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7797 | `	/* Try to invoke the callback */` |
|       15 |  7798 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|       15 |  7799 | `	if( rc != SXRET_OK ){` |
|        - |  7800 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|      ! 0 |  7801 | `		ph7_result_bool(pCtx,0); /* return false */` |
|      ! 0 |  7802 | `	}else{` |
|        - |  7803 | `		/* Callback result */` |
|       15 |  7804 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        - |  7805 | `	}` |
|       15 |  7806 | `	PH7_MemObjRelease(&sResult);` |
|       15 |  7807 | `	return PH7_OK;` |
|        8 |  7808 |  |
|        - |  7809 | `/*` |
|        - |  7810 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|        - |  7811 | ` *  Call a callback with an array of parameters.` |
|        - |  7812 | ` * Parameter` |
|        - |  7813 | ` *  $callback` |
|        - |  7814 | ` *   The callable to be called.` |
|        - |  7815 | ` * $param_arr` |
|        - |  7816 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|        - |  7817 | ` * Return` |
|        - |  7818 | ` *  Returns the return value of the callback, or FALSE on error.` |
|        - |  7819 | ` */` |
|       10 |  7820 | `static int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7821 |  |
|        - |  7822 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|        - |  7823 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|        - |  7824 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|        - |  7825 | `	SySet aArg;               /* Arguments containers */` |
|        - |  7826 | `	sxi32 rc;` |
|        - |  7827 | `	sxu32 n;` |
|       11 |  7828 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|        - |  7829 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  7830 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7831 | `		return PH7_OK;` |
|        - |  7832 | `	}` |
|       11 |  7833 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|       11 |  7834 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7835 | `	/* Initialize the arguments container */` |
|       11 |  7836 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7837 | `	/* Turn hashmap entries into callback arguments */` |
|       11 |  7838 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       11 |  7839 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|       23 |  7840 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        - |  7841 | `		/* Extract node value */` |
|       13 |  7842 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|       13 |  7843 | `			SySetPut(&aArg,(const void *)&pValue);` |
|        6 |  7844 | `		}` |
|        - |  7845 | `		/* Point to the next entry */` |
|       13 |  7846 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        7 |  7847 | `	}` |
|        - |  7848 | `	/* Try to invoke the callback */` |
|       11 |  7849 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|       11 |  7850 | `	if( rc != SXRET_OK ){` |
|        - |  7851 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|      ! 0 |  7852 | `		ph7_result_bool(pCtx,0); /* return false */` |
|      ! 0 |  7853 | `	}else{` |
|        - |  7854 | `		/* Callback result */` |
|       11 |  7855 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        - |  7856 | `	}` |
|        - |  7857 | `	/* Cleanup the mess left behind */` |
|       11 |  7858 | `	PH7_MemObjRelease(&sResult);` |
|       11 |  7859 | `	SySetRelease(&aArg);` |
|       11 |  7860 | `	return PH7_OK;` |
|        6 |  7861 |  |
|        - |  7862 | `/*` |
|        - |  7863 | ` * bool defined(string $name)` |
|        - |  7864 | ` *  Checks whether a given named constant exists.` |
|        - |  7865 | ` * Parameter:` |
|        - |  7866 | ` *  Name of the desired constant.` |
|        - |  7867 | ` * Return` |
|        - |  7868 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  7869 | ` */` |
|       12 |  7870 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7871 |  |
|        - |  7872 | `	const char *zName;` |
|       13 |  7873 | `	int nLen = 0;` |
|       13 |  7874 | `	int res = 0;` |
|       13 |  7875 | `	if( nArg < 1 ){` |
|        - |  7876 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  7877 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  7878 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7879 | `		return SXRET_OK;` |
|        - |  7880 | `	}` |
|        - |  7881 | `	/* Extract constant name */` |
|       13 |  7882 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7883 | `	/* Perform the lookup */` |
|       13 |  7884 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7885 | `		/* Already defined */` |
|        7 |  7886 | `		res = 1;` |
|        3 |  7887 | `	}` |
|       13 |  7888 | `	ph7_result_bool(pCtx,res);` |
|       13 |  7889 | `	return SXRET_OK;` |
|        7 |  7890 |  |
|        - |  7891 | `/*` |
|        - |  7892 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  7893 | ` * below.` |
|        - |  7894 | ` */` |
|        8 |  7895 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  7896 |  |
|       10 |  7897 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  7898 | `	/* Expand constant value */` |
|       10 |  7899 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7900 |  |
|        - |  7901 | `/*` |
|        - |  7902 | ` * bool define(string $constant_name,expression value)` |
|        - |  7903 | ` *  Defines a named constant at runtime.` |
|        - |  7904 | ` * Parameter:` |
|        - |  7905 | ` *  $constant_name` |
|        - |  7906 | ` *   The name of the constant` |
|        - |  7907 | ` *  $value` |
|        - |  7908 | ` *   Constant value` |
|        - |  7909 | ` * Return:` |
|        - |  7910 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7911 | ` */` |
|       10 |  7912 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7913 |  |
|        - |  7914 | `	const char *zName;  /* Constant name */` |
|        - |  7915 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7916 | `	int nLen = 0;       /* Name length */` |
|        - |  7917 | `	sxi32 rc;` |
|       12 |  7918 | `	if( nArg < 2 ){` |
|        - |  7919 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7920 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7921 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7922 | `		return SXRET_OK;` |
|        - |  7923 | `	}` |
|       12 |  7924 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7925 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7926 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7927 | `		return SXRET_OK;` |
|        - |  7928 | `	}` |
|        - |  7929 | `	/* Extract constant name */` |
|       12 |  7930 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7931 | `	if( nLen < 1 ){` |
|      ! 0 |  7932 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7933 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7934 | `		return SXRET_OK;` |
|        - |  7935 | `	}` |
|        - |  7936 | `	/* Duplicate constant value */` |
|       12 |  7937 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7938 | `	if( pValue == 0 ){` |
|      ! 0 |  7939 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7940 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7941 | `		return SXRET_OK;` |
|        - |  7942 | `	}` |
|        - |  7943 | `	/* Initialize the memory object */` |
|       12 |  7944 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7945 | `	/* Register the constant */` |
|       12 |  7946 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7947 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7948 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7949 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7950 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7951 | `		return SXRET_OK;` |
|        - |  7952 | `	}` |
|        - |  7953 | `	/* Duplicate constant value */` |
|       12 |  7954 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7955 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7956 | `		/* Lower case the constant name */` |
|      ! 0 |  7957 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7958 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7959 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  7960 | `				/* UTF-8 stream */` |
|      ! 0 |  7961 | `				zCur++;` |
|      ! 0 |  7962 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  7963 | `					zCur++;` |
|      ! 0 |  7964 | `				}` |
|      ! 0 |  7965 | `				continue;` |
|        - |  7966 | `			}` |
|      ! 0 |  7967 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  7968 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  7969 | `				zCur[0] = (char)c;` |
|      ! 0 |  7970 | `			}` |
|      ! 0 |  7971 | `			zCur++;` |
|      ! 0 |  7972 | `		}` |
|        - |  7973 | `		/* Finally,register the constant */` |
|      ! 0 |  7974 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  7975 | `	}` |
|        - |  7976 | `	/* All done,return TRUE */` |
|       12 |  7977 | `	ph7_result_bool(pCtx,1);` |
|       12 |  7978 | `	return SXRET_OK;` |
|        7 |  7979 |  |
|        - |  7980 | `/*` |
|        - |  7981 | ` * value constant(string $name)` |
|        - |  7982 | ` *  Returns the value of a constant` |
|        - |  7983 | ` * Parameter` |
|        - |  7984 | ` *  $name` |
|        - |  7985 | ` *    Name of the constant.` |
|        - |  7986 | ` * Return` |
|        - |  7987 | ` *  Constant value or NULL if not defined.` |
|        - |  7988 | ` */` |
|        8 |  7989 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7990 |  |
|        - |  7991 | `	SyHashEntry *pEntry;` |
|        - |  7992 | `	ph7_constant *pCons;` |
|        - |  7993 | `	const char *zName; /* Constant name */` |
|        - |  7994 | `	ph7_value sVal;    /* Constant value */` |
|        - |  7995 | `	int nLen;` |
|       10 |  7996 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  7997 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  7998 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  7999 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8000 | `		return SXRET_OK;` |
|        - |  8001 | `	}` |
|        - |  8002 | `	/* Extract the constant name */` |
|       10 |  8003 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8004 | `	/* Perform the query */` |
|       10 |  8005 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  8006 | `	if( pEntry == 0 ){` |
|        3 |  8007 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  8008 | `		ph7_result_null(pCtx);` |
|        3 |  8009 | `		return SXRET_OK;` |
|        - |  8010 | `	}` |
|        8 |  8011 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  8012 | `	/* Point to the structure that describe the constant */` |
|        8 |  8013 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  8014 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  8015 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  8016 | `	/* Return that value */` |
|        8 |  8017 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  8018 | `	/* Cleanup */` |
|        8 |  8019 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  8020 | `	return SXRET_OK;` |
|        6 |  8021 |  |
|        - |  8022 | `/*` |
|        - |  8023 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  8024 | ` * defined below.` |
|        - |  8025 | ` */` |
|      410 |  8026 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8027 |  |
|      411 |  8028 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8029 | `	ph7_value sName;` |
|        - |  8030 | `	sxi32 rc;` |
|        - |  8031 | `	/* Prepare the constant name for insertion */` |
|      411 |  8032 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      411 |  8033 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8034 | `	/* Perform the insertion */` |
|      411 |  8035 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      411 |  8036 | `	PH7_MemObjRelease(&sName);` |
|      411 |  8037 | `	return rc;` |
|        1 |  8038 |  |
|        - |  8039 | `/*` |
|        - |  8040 | ` * array get_defined_constants(void)` |
|        - |  8041 | ` *  Returns an associative array with the names of all defined` |
|        - |  8042 | ` *  constants.` |
|        - |  8043 | ` * Parameters` |
|        - |  8044 | ` *  NONE.` |
|        - |  8045 | ` * Returns` |
|        - |  8046 | ` *  Returns the names of all the constants currently defined.` |
|        - |  8047 | ` */` |
|        2 |  8048 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8049 |  |
|        - |  8050 | `	ph7_value *pArray;` |
|        - |  8051 | `	/* Create the array first*/` |
|        3 |  8052 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8053 | `	if( pArray == 0 ){` |
|      ! 0 |  8054 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8055 | `		SXUNUSED(apArg);` |
|        - |  8056 | `		/* Return NULL */` |
|      ! 0 |  8057 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8058 | `		return SXRET_OK;` |
|        - |  8059 | `	}` |
|        - |  8060 | `	/* Fill the array with the defined constants */` |
|        3 |  8061 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  8062 | `	/* Return the created array */` |
|        3 |  8063 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8064 | `	return SXRET_OK;` |
|        2 |  8065 |  |
|        - |  8066 | `/*` |
|        - |  8067 | ` * Section:` |
|        - |  8068 | ` *  Output Control (OB) functions.` |
|        - |  8069 | ` * Status:` |
|        - |  8070 | ` *    Stable.` |
|        - |  8071 | ` */` |
|        - |  8072 | `/* Forward declaration */` |
|        - |  8073 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry);` |
|        - |  8074 | `/*` |
|        - |  8075 | ` * void ob_clean(void)` |
|        - |  8076 | ` *  This function discards the contents of the output buffer.` |
|        - |  8077 | ` *  This function does not destroy the output buffer like ob_end_clean() does.` |
|        - |  8078 | ` * Parameter` |
|        - |  8079 | ` *  None` |
|        - |  8080 | ` * Return` |
|        - |  8081 | ` *  No value is returned.` |
|        - |  8082 | ` */` |
|        2 |  8083 | `static int vm_builtin_ob_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8084 |  |
|        3 |  8085 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8086 | `	VmObEntry *pOb;` |
|        1 |  8087 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8088 | `	SXUNUSED(apArg);` |
|        - |  8089 | `	/* Peek the top most OB */` |
|        3 |  8090 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8091 | `	if( pOb ){` |
|        3 |  8092 | `		SyBlobRelease(&pOb->sOB);` |
|        1 |  8093 | `	}` |
|        3 |  8094 | `	return PH7_OK;` |
|        1 |  8095 |  |
|        - |  8096 | `/*` |
|        - |  8097 | ` * bool ob_end_clean(void)` |
|        - |  8098 | ` *  Clean (erase) the output buffer and turn off output buffering` |
|        - |  8099 | ` *  This function discards the contents of the topmost output buffer and turns` |
|        - |  8100 | ` *  off this output buffering. If you want to further process the buffer's contents` |
|        - |  8101 | ` *  you have to call ob_get_contents() before ob_end_clean() as the buffer contents` |
|        - |  8102 | ` *  are discarded when ob_end_clean() is called.` |
|        - |  8103 | ` * Parameter` |
|        - |  8104 | ` *  None` |
|        - |  8105 | ` * Return` |
|        - |  8106 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first that you called` |
|        - |  8107 | ` *  the function without an active buffer or that for some reason a buffer could not be deleted` |
|        - |  8108 | ` * (possible for special buffer)` |
|        - |  8109 | ` */` |
|     2614 |  8110 | `static int vm_builtin_ob_end_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8111 |  |
|     2616 |  8112 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8113 | `	VmObEntry *pOb;` |
|        - |  8114 | `	/* Pop the top most OB */` |
|     2616 |  8115 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|     2616 |  8116 | `	if( pOb == 0){` |
|        - |  8117 | `		/* No such OB,return FALSE */` |
|      ! 0 |  8118 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8119 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8120 | `		SXUNUSED(apArg);` |
|      ! 0 |  8121 | `	}else{` |
|        - |  8122 | `		/* Release */` |
|     2616 |  8123 | `		VmObRestore(pVm,pOb);` |
|        - |  8124 | `		/* Return true */` |
|     2616 |  8125 | `		ph7_result_bool(pCtx,1);` |
|        - |  8126 | `	}` |
|     2616 |  8127 | `	return PH7_OK;` |
|        2 |  8128 |  |
|        - |  8129 | `/*` |
|        - |  8130 | ` * string ob_get_contents(void)` |
|        - |  8131 | ` *  Gets the contents of the output buffer without clearing it.` |
|        - |  8132 | ` * Parameter` |
|        - |  8133 | ` *  None` |
|        - |  8134 | ` * Return` |
|        - |  8135 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|        - |  8136 | ` */` |
|        6 |  8137 | `static int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8138 |  |
|        7 |  8139 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8140 | `	VmObEntry *pOb;` |
|        - |  8141 | `	/* Peek the top most OB */` |
|        7 |  8142 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        7 |  8143 | `	if( pOb == 0 ){` |
|        - |  8144 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8145 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8146 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8147 | `		SXUNUSED(apArg);` |
|      ! 0 |  8148 | `	}else{` |
|        - |  8149 | `		/* Return contents */` |
|        7 |  8150 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB));` |
|        - |  8151 | `	}` |
|        7 |  8152 | `	return PH7_OK;` |
|        1 |  8153 |  |
|        - |  8154 | `/*` |
|        - |  8155 | ` * string ob_get_clean(void)` |
|        - |  8156 | ` * string ob_get_flush(void)` |
|        - |  8157 | ` *  Get current buffer contents and delete current output buffer.` |
|        - |  8158 | ` * Parameter` |
|        - |  8159 | ` *  None` |
|        - |  8160 | ` * Return` |
|        - |  8161 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|        - |  8162 | ` */` |
|     3892 |  8163 | `static int vm_builtin_ob_get_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8164 |  |
|     3894 |  8165 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8166 | `	VmObEntry *pOb;` |
|        - |  8167 | `	/* Pop the top most OB */` |
|     3894 |  8168 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|     3894 |  8169 | `	if( pOb == 0 ){` |
|        - |  8170 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8171 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8172 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8173 | `		SXUNUSED(apArg);` |
|      ! 0 |  8174 | `	}else{` |
|        - |  8175 | `		/* Return contents */` |
|     3894 |  8176 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB)); /* Will make it's own copy */` |
|        - |  8177 | `		/* Release */` |
|     3894 |  8178 | `		VmObRestore(pVm,pOb);` |
|        - |  8179 | `	}` |
|     3894 |  8180 | `	return PH7_OK;` |
|        2 |  8181 |  |
|        - |  8182 | `/*` |
|        - |  8183 | ` * int ob_get_length(void)` |
|        - |  8184 | ` *  Return the length of the output buffer.` |
|        - |  8185 | ` * Parameter` |
|        - |  8186 | ` *  None` |
|        - |  8187 | ` * Return` |
|        - |  8188 | ` *  Returns the length of the output buffer contents or FALSE if no buffering is active.` |
|        - |  8189 | ` */` |
|        2 |  8190 | `static int vm_builtin_ob_get_length(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8191 |  |
|        3 |  8192 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8193 | `	VmObEntry *pOb;` |
|        - |  8194 | `	/* Peek the top most OB */` |
|        3 |  8195 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8196 | `	if( pOb == 0 ){` |
|        - |  8197 | `		/* No active OB,return FALSE */` |
|      ! 0 |  8198 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8199 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8200 | `		SXUNUSED(apArg);` |
|      ! 0 |  8201 | `	}else{` |
|        - |  8202 | `		/* Return OB length */` |
|        3 |  8203 | `		ph7_result_int64(pCtx,(ph7_int64)SyBlobLength(&pOb->sOB));` |
|        - |  8204 | `	}` |
|        3 |  8205 | `	return PH7_OK;` |
|        1 |  8206 |  |
|        - |  8207 | `/*` |
|        - |  8208 | ` * int ob_get_level(void)` |
|        - |  8209 | ` *  Returns the nesting level of the output buffering mechanism.` |
|        - |  8210 | ` * Parameter` |
|        - |  8211 | ` *  None` |
|        - |  8212 | ` * Return` |
|        - |  8213 | ` *  Returns the level of nested output buffering handlers or zero if output buffering is not active.` |
|        - |  8214 | ` */` |
|        6 |  8215 | `static int vm_builtin_ob_get_level(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8216 |  |
|        7 |  8217 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8218 | `	int iNest;` |
|        3 |  8219 | `	SXUNUSED(nArg); /* cc warning */` |
|        3 |  8220 | `	SXUNUSED(apArg);` |
|        - |  8221 | `	/* Nesting level */` |
|        7 |  8222 | `	iNest = (int)SySetUsed(&pVm->aOB);` |
|        - |  8223 | `	/* Return the nesting value */` |
|        7 |  8224 | `	ph7_result_int(pCtx,iNest);` |
|        7 |  8225 | `	return PH7_OK;` |
|        1 |  8226 |  |
|        - |  8227 | `/*` |
|        - |  8228 | ` * Output Buffer(OB) default VM consumer routine.All VM output is now redirected` |
|        - |  8229 | ` * to a stackable internal buffer,until the user call [ob_get_clean(),ob_end_clean(),...].` |
|        - |  8230 | ` * Refer to the implementation of [ob_start()] for more information.` |
|        - |  8231 | ` */` |
|     5832 |  8232 | `static int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData)` |
|        2 |  8233 |  |
|     5834 |  8234 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|        - |  8235 | `	VmObEntry *pEntry;` |
|        - |  8236 | `	ph7_value sResult;` |
|        - |  8237 | `	/* Peek the top most entry */` |
|     5834 |  8238 | `	pEntry = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|     5834 |  8239 | `	if( pEntry == 0 ){` |
|        - |  8240 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8241 | `		return PH7_OK;` |
|        - |  8242 | `	}` |
|     5834 |  8243 | `	PH7_MemObjInit(pVm,&sResult);` |
|     5834 |  8244 | `	if( ph7_value_is_callable(&pEntry->sCallback) && pVm->nObDepth < 15 ){` |
|        - |  8245 | `		ph7_value sArg,*apArg[2];` |
|        - |  8246 | `		/* Fill the first argument */` |
|      ! 0 |  8247 | `		PH7_MemObjInitFromString(pVm,&sArg,0);` |
|      ! 0 |  8248 | `		PH7_MemObjStringAppend(&sArg,(const char *)pData,nDataLen);` |
|      ! 0 |  8249 | `		apArg[0] = &sArg;` |
|        - |  8250 | `		/* Call the 'filter' callback */` |
|      ! 0 |  8251 | `		pVm->nObDepth++;` |
|      ! 0 |  8252 | `		PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult);` |
|      ! 0 |  8253 | `		pVm->nObDepth--;` |
|      ! 0 |  8254 | `		if( sResult.iFlags & MEMOBJ_STRING ){` |
|        - |  8255 | `			/* Extract the function result */` |
|      ! 0 |  8256 | `			pData = SyBlobData(&sResult.sBlob);` |
|      ! 0 |  8257 | `			nDataLen = SyBlobLength(&sResult.sBlob);` |
|      ! 0 |  8258 | `		}` |
|      ! 0 |  8259 | `		PH7_MemObjRelease(&sArg);` |
|      ! 0 |  8260 | `	}` |
|     5834 |  8261 | `	if( nDataLen > 0 ){` |
|        - |  8262 | `		/* Redirect the VM output to the internal buffer */` |
|     5834 |  8263 | `		SyBlobAppend(&pEntry->sOB,pData,nDataLen);` |
|     2916 |  8264 | `	}` |
|        - |  8265 | `	/* Release */` |
|     5834 |  8266 | `	PH7_MemObjRelease(&sResult);` |
|     5834 |  8267 | `	return PH7_OK;` |
|     2918 |  8268 |  |
|        - |  8269 | `/*` |
|        - |  8270 | ` * Restore the default consumer.` |
|        - |  8271 | ` * Refer to the implementation of [ob_end_clean()] for more` |
|        - |  8272 | ` * information.` |
|        - |  8273 | ` */` |
|     6508 |  8274 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry)` |
|        2 |  8275 |  |
|     6510 |  8276 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|     6510 |  8277 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|        - |  8278 | `		/* No more stackable OB */` |
|     6492 |  8279 | `		pCons->xConsumer = pCons->xDef;` |
|     6492 |  8280 | `		pCons->pUserData = pCons->pDefData;` |
|     3245 |  8281 | `	}` |
|        - |  8282 | `	/* Release OB data */` |
|     6510 |  8283 | `	PH7_MemObjRelease(&pEntry->sCallback);` |
|     6510 |  8284 | `	SyBlobRelease(&pEntry->sOB);` |
|     6510 |  8285 |  |
|        - |  8286 | `/*` |
|        - |  8287 | ` * bool ob_start([ callback $output_callback] )` |
|        - |  8288 | ` * This function will turn output buffering on. While output buffering is active no output` |
|        - |  8289 | ` *  is sent from the script (other than headers), instead the output is stored in an internal` |
|        - |  8290 | ` *  buffer.` |
|        - |  8291 | ` * Parameter` |
|        - |  8292 | ` *  $output_callback` |
|        - |  8293 | ` *   An optional output_callback function may be specified. This function takes a string` |
|        - |  8294 | ` *   as a parameter and should return a string. The function will be called when the output` |
|        - |  8295 | ` *   buffer is flushed (sent) or cleaned (with ob_flush(), ob_clean() or similar function)` |
|        - |  8296 | ` *   or when the output buffer is flushed to the browser at the end of the request.` |
|        - |  8297 | ` *   When output_callback is called, it will receive the contents of the output buffer` |
|        - |  8298 | ` *   as its parameter and is expected to return a new output buffer as a result, which will` |
|        - |  8299 | ` *   be sent to the browser. If the output_callback is not a callable function, this function` |
|        - |  8300 | ` *   will return FALSE.` |
|        - |  8301 | ` *   If the callback function has two parameters, the second parameter is filled with` |
|        - |  8302 | ` *   a bit-field consisting of PHP_OUTPUT_HANDLER_START, PHP_OUTPUT_HANDLER_CONT` |
|        - |  8303 | ` *   and PHP_OUTPUT_HANDLER_END.` |
|        - |  8304 | ` *   If output_callback returns FALSE original input is sent to the browser.` |
|        - |  8305 | ` *   The output_callback parameter may be bypassed by passing a NULL value.` |
|        - |  8306 | ` * Return` |
|        - |  8307 | ` *   Returns TRUE on success or FALSE on failure.` |
|        - |  8308 | ` */` |
|     6508 |  8309 | `static int vm_builtin_ob_start(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8310 |  |
|     6510 |  8311 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8312 | `	VmObEntry sOb;` |
|        - |  8313 | `	sxi32 rc;` |
|        - |  8314 | `	/* Initialize the OB entry */` |
|     6510 |  8315 | `	PH7_MemObjInit(pCtx->pVm,&sOb.sCallback);` |
|     6510 |  8316 | `	SyBlobInit(&sOb.sOB,&pVm->sAllocator);` |
|     6510 |  8317 | `	if( nArg > 0 && (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) ){` |
|        - |  8318 | `		/* Save the callback name for later invocation */` |
|      ! 0 |  8319 | `		PH7_MemObjStore(apArg[0],&sOb.sCallback);` |
|      ! 0 |  8320 | `	}` |
|        - |  8321 | `	/* Push in the stack */` |
|     6510 |  8322 | `	rc = SySetPut(&pVm->aOB,(const void *)&sOb);` |
|     6510 |  8323 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8324 | `		PH7_MemObjRelease(&sOb.sCallback);` |
|      ! 0 |  8325 | `	}else{` |
|     6510 |  8326 | `		ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        - |  8327 | `		/* Substitute the default VM consumer */` |
|     6510 |  8328 | `		if( pCons->xConsumer != VmObConsumer ){` |
|     6492 |  8329 | `			pCons->xDef = pCons->xConsumer;` |
|     6492 |  8330 | `			pCons->pDefData = pCons->pUserData;` |
|        - |  8331 | `			/* Install the new consumer */` |
|     6492 |  8332 | `			pCons->xConsumer = VmObConsumer;` |
|     6492 |  8333 | `			pCons->pUserData = pVm;` |
|     3245 |  8334 | `		}` |
|        - |  8335 | `	}` |
|     6510 |  8336 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     6510 |  8337 | `	return PH7_OK;` |
|        2 |  8338 |  |
|        - |  8339 | `/*` |
|        - |  8340 | ` * Flush Output buffer to the default VM output consumer.` |
|        - |  8341 | ` * Refer to the implementation of [ob_flush()] for more` |
|        - |  8342 | ` * information.` |
|        - |  8343 | ` */` |
|        4 |  8344 | `static sxi32 VmObFlush(ph7_vm *pVm,VmObEntry *pEntry,int bRelease)` |
|        1 |  8345 |  |
|        5 |  8346 | `	SyBlob *pBlob = &pEntry->sOB;` |
|        - |  8347 | `	sxi32 rc;` |
|        - |  8348 | `	/* Flush contents */` |
|        5 |  8349 | `	rc = PH7_OK;` |
|        5 |  8350 | `	if( SyBlobLength(pBlob) > 0 ){` |
|        - |  8351 | `		/* Call the VM output consumer */` |
|        5 |  8352 | `		rc = pVm->sVmConsumer.xDef(SyBlobData(pBlob),SyBlobLength(pBlob),pVm->sVmConsumer.pDefData);` |
|        - |  8353 | `		/* Increment VM output counter */` |
|        5 |  8354 | `		pVm->nOutputLen += SyBlobLength(pBlob);` |
|        5 |  8355 | `		if( rc != PH7_ABORT ){` |
|        5 |  8356 | `			rc = PH7_OK;` |
|        2 |  8357 | `		}` |
|        2 |  8358 | `	}` |
|        5 |  8359 | `	if( bRelease ){` |
|        3 |  8360 | `		VmObRestore(&(*pVm),pEntry);` |
|        2 |  8361 | `	}else{` |
|        - |  8362 | `		/* Reset the blob */` |
|        3 |  8363 | `		SyBlobReset(pBlob);` |
|        - |  8364 | `	}` |
|        5 |  8365 | `	return rc;` |
|        1 |  8366 |  |
|        - |  8367 | `/*` |
|        - |  8368 | ` * void ob_flush(void)` |
|        - |  8369 | ` * void flush(void)` |
|        - |  8370 | ` *  Flush (send) the output buffer.` |
|        - |  8371 | ` * Parameter` |
|        - |  8372 | ` *  None` |
|        - |  8373 | ` * Return` |
|        - |  8374 | ` *  No return value.` |
|        - |  8375 | ` */` |
|        2 |  8376 | `static int vm_builtin_ob_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8377 |  |
|        3 |  8378 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8379 | `	VmObEntry *pOb;` |
|        - |  8380 | `	sxi32 rc;` |
|        - |  8381 | `	/* Peek the top most OB entry */` |
|        3 |  8382 | `	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);` |
|        3 |  8383 | `	if( pOb == 0 ){` |
|        - |  8384 | `		/* Empty stack,return immediately */` |
|      ! 0 |  8385 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8386 | `		SXUNUSED(apArg);` |
|      ! 0 |  8387 | `		return PH7_OK;` |
|        - |  8388 | `	}` |
|        - |  8389 | `	/* Flush contents */` |
|        3 |  8390 | `	rc = VmObFlush(pVm,pOb,FALSE);` |
|        3 |  8391 | `	return rc;` |
|        2 |  8392 |  |
|        - |  8393 | `/*` |
|        - |  8394 | ` * bool ob_end_flush(void)` |
|        - |  8395 | ` *  Flush (send) the output buffer and turn off output buffering.` |
|        - |  8396 | ` * Parameter` |
|        - |  8397 | ` *  None` |
|        - |  8398 | ` * Return` |
|        - |  8399 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first` |
|        - |  8400 | ` *  that you called the function without an active buffer or that for some reason` |
|        - |  8401 | ` *  a buffer could not be deleted (possible for special buffer).` |
|        - |  8402 | ` */` |
|        2 |  8403 | `static int vm_builtin_ob_end_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8404 |  |
|        3 |  8405 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8406 | `	VmObEntry *pOb;` |
|        - |  8407 | `	sxi32 rc;` |
|        - |  8408 | `	/* Pop the top most OB entry */` |
|        3 |  8409 | `	pOb = (VmObEntry *)SySetPop(&pVm->aOB);` |
|        3 |  8410 | `	if( pOb == 0 ){` |
|        - |  8411 | `		/* Empty stack,return FALSE */` |
|      ! 0 |  8412 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8413 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8414 | `		SXUNUSED(apArg);` |
|      ! 0 |  8415 | `		return PH7_OK;` |
|        - |  8416 | `	}` |
|        - |  8417 | `	/* Flush contents */` |
|        3 |  8418 | `	rc = VmObFlush(pVm,pOb,TRUE);` |
|        - |  8419 | `	/* Return true */` |
|        3 |  8420 | `	ph7_result_bool(pCtx,1);` |
|        3 |  8421 | `	return rc;` |
|        2 |  8422 |  |
|        - |  8423 | `/*` |
|        - |  8424 | ` * void ob_implicit_flush([int $flag = true ])` |
|        - |  8425 | ` *  ob_implicit_flush() will turn implicit flushing on or off.` |
|        - |  8426 | ` *  Implicit flushing will result in a flush operation after every` |
|        - |  8427 | ` *  output call, so that explicit calls to flush() will no longer be needed.` |
|        - |  8428 | ` * Parameter` |
|        - |  8429 | ` *  $flag` |
|        - |  8430 | ` *   TRUE to turn implicit flushing on, FALSE otherwise.` |
|        - |  8431 | ` * Return` |
|        - |  8432 | ` *   Nothing` |
|        - |  8433 | ` */` |
|        4 |  8434 | `static int vm_builtin_ob_implicit_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8435 |  |
|        - |  8436 | `	/* NOTE: As of this version,this function is a no-op.` |
|        - |  8437 | `	 * PH7 is smart enough to flush it's internal buffer when appropriate.` |
|        - |  8438 | `	 */` |
|        2 |  8439 | `	SXUNUSED(pCtx);` |
|        2 |  8440 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  8441 | `	SXUNUSED(apArg);` |
|        5 |  8442 | `	return PH7_OK;` |
|        1 |  8443 |  |
|        - |  8444 | `/*` |
|        - |  8445 | ` * array ob_list_handlers(void)` |
|        - |  8446 | ` *  Lists all output handlers in use.` |
|        - |  8447 | ` * Parameter` |
|        - |  8448 | ` *  None` |
|        - |  8449 | ` * Return` |
|        - |  8450 | ` *  This will return an array with the output handlers in use (if any).` |
|        - |  8451 | ` */` |
|        2 |  8452 | `static int vm_builtin_ob_list_handlers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8453 |  |
|        3 |  8454 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8455 | `	ph7_value *pArray;` |
|        - |  8456 | `	VmObEntry *aEntry;` |
|        - |  8457 | `	ph7_value sVal;` |
|        - |  8458 | `	sxu32 n;` |
|        3 |  8459 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|        - |  8460 | `		/* Empty stack,return null */` |
|      ! 0 |  8461 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8462 | `		return PH7_OK;` |
|        - |  8463 | `	}` |
|        - |  8464 | `	/* Create a new array */` |
|        3 |  8465 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8466 | `	if( pArray == 0 ){` |
|        - |  8467 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8468 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8469 | `		SXUNUSED(apArg);` |
|      ! 0 |  8470 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8471 | `		return PH7_OK;` |
|        - |  8472 | `	}` |
|        3 |  8473 | `	PH7_MemObjInit(pVm,&sVal);` |
|        - |  8474 | `	/* Point to the installed OB entries */` |
|        3 |  8475 | `	aEntry = (VmObEntry *)SySetBasePtr(&pVm->aOB);` |
|        - |  8476 | `	/* Perform the requested operation */` |
|        5 |  8477 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; n++ ){` |
|        3 |  8478 | `		VmObEntry *pEntry = &aEntry[n];` |
|        - |  8479 | `		/* Extract handler name */` |
|        3 |  8480 | `		SyBlobReset(&sVal.sBlob);` |
|        3 |  8481 | `		if( pEntry->sCallback.iFlags & MEMOBJ_STRING ){` |
|        - |  8482 | `			/* Callback,dup it's name */` |
|      ! 0 |  8483 | `			SyBlobDup(&pEntry->sCallback.sBlob,&sVal.sBlob);` |
|        3 |  8484 | `		}else if( pEntry->sCallback.iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  8485 | `			SyBlobAppend(&sVal.sBlob,"Class Method",sizeof("Class Method")-1);` |
|      ! 0 |  8486 | `		}else{` |
|        3 |  8487 | `			SyBlobAppend(&sVal.sBlob,"default output handler",sizeof("default output handler")-1);` |
|        - |  8488 | `		}` |
|        3 |  8489 | `		sVal.iFlags = MEMOBJ_STRING;` |
|        - |  8490 | `		/* Perform the insertion */` |
|        3 |  8491 | `		ph7_array_add_elem(pArray,0/* Automatic index assign */,&sVal /* Will make it's own copy */);` |
|        2 |  8492 | `	}` |
|        3 |  8493 | `	PH7_MemObjRelease(&sVal);` |
|        - |  8494 | `	/* Return the freshly created array */` |
|        3 |  8495 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8496 | `	return PH7_OK;` |
|        2 |  8497 |  |
|        - |  8498 | `/*` |
|        - |  8499 | ` * Section:` |
|        - |  8500 | ` *  Random numbers/string generators.` |
|        - |  8501 | ` * Status:` |
|        - |  8502 | ` *    Stable.` |
|        - |  8503 | ` */` |
|        - |  8504 | `/*` |
|        - |  8505 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  8506 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  8507 | ` * used by te SQLite3 library.` |
|        - |  8508 | ` */` |
|     1014 |  8509 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  8510 |  |
|        - |  8511 | `	sxu32 iNum;` |
|     1016 |  8512 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     1016 |  8513 | `	return iNum;` |
|        2 |  8514 |  |
|        - |  8515 | `/*` |
|        - |  8516 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  8517 | ` * Note that the generated string is NOT null terminated.` |
|        - |  8518 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  8519 | ` * by te SQLite3 library.` |
|        - |  8520 | ` */` |
|    35268 |  8521 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  8522 |  |
|        - |  8523 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  8524 | `	int i;` |
|        - |  8525 | `	/* Generate a binary string first */` |
|    35270 |  8526 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  8527 | `	/* Turn the binary string into english based alphabet */` |
|   388122 |  8528 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   352854 |  8529 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   176428 |  8530 | `	 }` |
|    35270 |  8531 |  |
|        - |  8532 | `/*` |
|        - |  8533 | ` * int rand()` |
|        - |  8534 | ` * int mt_rand()` |
|        - |  8535 | ` * int rand(int $min,int $max)` |
|        - |  8536 | ` * int mt_rand(int $min,int $max)` |
|        - |  8537 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  8538 | ` * Parameter` |
|        - |  8539 | ` *  $min` |
|        - |  8540 | ` *    The lowest value to return (default: 0)` |
|        - |  8541 | ` *  $max` |
|        - |  8542 | ` *   The highest value to return (default: getrandmax())` |
|        - |  8543 | ` * Return` |
|        - |  8544 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  8545 | ` * Note:` |
|        - |  8546 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8547 | ` *  by te SQLite3 library.` |
|        - |  8548 | ` */` |
|       20 |  8549 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8550 |  |
|        - |  8551 | `	sxu32 iNum;` |
|        - |  8552 | `	/* Generate the random number */` |
|       21 |  8553 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  8554 | `	if( nArg > 1 ){` |
|        - |  8555 | `		sxu32 iMin,iMax;` |
|        3 |  8556 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  8557 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  8558 | `		if( iMin < iMax ){` |
|        3 |  8559 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  8560 | `			if( iDiv > 0 ){` |
|        3 |  8561 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  8562 | `			}` |
|        1 |  8563 | `		}else if(iMax > 0 ){` |
|      ! 0 |  8564 | `			iNum %= iMax;` |
|      ! 0 |  8565 | `		}` |
|        1 |  8566 | `	}` |
|        - |  8567 | `	/* Return the number */` |
|       21 |  8568 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  8569 | `	return SXRET_OK;` |
|        1 |  8570 |  |
|        - |  8571 | `/*` |
|        - |  8572 | ` * int getrandmax(void)` |
|        - |  8573 | ` * int mt_getrandmax(void)` |
|        - |  8574 | ` * int rc4_getrandmax(void)` |
|        - |  8575 | ` *   Show largest possible random value` |
|        - |  8576 | ` * Return` |
|        - |  8577 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  8578 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  8579 | ` * Note:` |
|        - |  8580 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8581 | ` *  by te SQLite3 library.` |
|        - |  8582 | ` */` |
|        4 |  8583 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8584 |  |
|        2 |  8585 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  8586 | `	SXUNUSED(apArg);` |
|        5 |  8587 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  8588 | `	return SXRET_OK;` |
|        1 |  8589 |  |
|        - |  8590 | `/*` |
|        - |  8591 | ` * string rand_str()` |
|        - |  8592 | ` * string rand_str(int $len)` |
|        - |  8593 | ` *  Generate a random string (English alphabet).` |
|        - |  8594 | ` * Parameter` |
|        - |  8595 | ` *  $len` |
|        - |  8596 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  8597 | ` * Return` |
|        - |  8598 | ` *   A pseudo random string.` |
|        - |  8599 | ` * Note:` |
|        - |  8600 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8601 | ` *  by te SQLite3 library.` |
|        - |  8602 | ` *  This function is a symisc extension.` |
|        - |  8603 | ` */` |
|      122 |  8604 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8605 |  |
|        - |  8606 | `	char zString[1024];` |
|      124 |  8607 | `	int iLen = 0x10;` |
|      124 |  8608 | `	if( nArg > 0 ){` |
|        - |  8609 | `		/* Get the desired length */` |
|      124 |  8610 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      124 |  8611 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  8612 | `			/* Default length */` |
|        3 |  8613 | `			iLen = 0x10;` |
|        1 |  8614 | `		}` |
|       61 |  8615 | `	}` |
|        - |  8616 | `	/* Generate the random string */` |
|      124 |  8617 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  8618 | `	/* Return the generated string */` |
|      124 |  8619 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      124 |  8620 | `	return SXRET_OK;` |
|        2 |  8621 |  |
|        - |  8622 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  8623 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  8624 | `/* Unique ID private data */` |
|        - |  8625 | `struct unique_id_data` |
|        - |  8626 |  |
|        - |  8627 | `	ph7_context *pCtx; /* Call context */` |
|        - |  8628 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  8629 | `};` |
|        - |  8630 | `/*` |
|        - |  8631 | ` * Binary to hex consumer callback.` |
|        - |  8632 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  8633 | ` * defined below.` |
|        - |  8634 | ` */` |
|      192 |  8635 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  8636 |  |
|      193 |  8637 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  8638 | `	sxu32 nBuflen;` |
|        - |  8639 | `	/* Extract result buffer length */` |
|      193 |  8640 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  8641 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  8642 | `			/*` |
|        - |  8643 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  8644 | `			 * string will be 13 characters long` |
|        - |  8645 | `			 */` |
|       25 |  8646 | `		return SXERR_ABORT;` |
|        - |  8647 | `	}` |
|      169 |  8648 | `	if( nBuflen > 22 ){` |
|      ! 0 |  8649 | `		return SXERR_ABORT;` |
|        - |  8650 | `	}` |
|        - |  8651 | `	/* Safely Consume the hex stream */` |
|      169 |  8652 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  8653 | `	return SXRET_OK;` |
|       97 |  8654 |  |
|        - |  8655 | `/*` |
|        - |  8656 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  8657 | ` *  Generate a unique ID` |
|        - |  8658 | ` * Parameter` |
|        - |  8659 | ` * $prefix` |
|        - |  8660 | ` *  Append this prefix to the generated unique ID.` |
|        - |  8661 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  8662 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  8663 | ` * $more_entropy` |
|        - |  8664 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  8665 | ` *  that the result will be unique.` |
|        - |  8666 | ` * Return` |
|        - |  8667 | ` *  Returns the unique identifier, as a string.` |
|        - |  8668 | ` */` |
|       24 |  8669 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8670 |  |
|        - |  8671 | `	struct unique_id_data sUniq;` |
|        - |  8672 | `	unsigned char zDigest[20];` |
|       25 |  8673 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8674 | `	const char *zPrefix;` |
|        - |  8675 | `	SHA1Context sCtx;` |
|        - |  8676 | `	char zRandom[7];` |
|        - |  8677 | `	int nPrefix;` |
|        - |  8678 | `	int entropy;` |
|        - |  8679 | `	/* Generate a random string first */` |
|       25 |  8680 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  8681 | `	/* Initialize fields */` |
|       25 |  8682 | `	zPrefix = 0;` |
|       25 |  8683 | `	nPrefix = 0;` |
|       25 |  8684 | `	entropy = 0;` |
|       25 |  8685 | `	if( nArg > 0 ){` |
|        - |  8686 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  8687 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  8688 | `		if( nArg > 1 ){` |
|      ! 0 |  8689 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  8690 | `		}` |
|      ! 0 |  8691 | `	}` |
|       25 |  8692 | `	SHA1Init(&sCtx);` |
|        - |  8693 | `	/* Generate the random ID */` |
|       25 |  8694 | `	if( nPrefix > 0 ){` |
|      ! 0 |  8695 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  8696 | `	}` |
|        - |  8697 | `	/* Append the random ID */` |
|       25 |  8698 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  8699 | `	/* Append the random string */` |
|       25 |  8700 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  8701 | `	/* Increment the number */` |
|       25 |  8702 | `	pVm->unique_id++;` |
|       25 |  8703 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  8704 | `	/* Hexify the digest */` |
|       25 |  8705 | `	sUniq.pCtx = pCtx;` |
|       25 |  8706 | `	sUniq.entropy = entropy;` |
|       25 |  8707 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  8708 | `	/* All done */` |
|       25 |  8709 | `	return PH7_OK;` |
|        1 |  8710 |  |
|        - |  8711 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  8712 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  8713 | `/*` |
|        - |  8714 | ` * Section:` |
|        - |  8715 | ` *  Language construct implementation as foreign functions.` |
|        - |  8716 | ` * Status:` |
|        - |  8717 | ` *    Stable.` |
|        - |  8718 | ` */` |
|        - |  8719 | `/*` |
|        - |  8720 | ` * void echo($string...)` |
|        - |  8721 | ` *  Output one or more messages.` |
|        - |  8722 | ` * Parameters` |
|        - |  8723 | ` *  $string` |
|        - |  8724 | ` *   Message to output.` |
|        - |  8725 | ` * Return` |
|        - |  8726 | ` *  NULL.` |
|        - |  8727 | ` */` |
|      ! 0 |  8728 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  8729 |  |
|        - |  8730 | `	const char *zData;` |
|      ! 0 |  8731 | `	int nDataLen = 0;` |
|        - |  8732 | `	ph7_vm *pVm;` |
|        - |  8733 | `	int i,rc;` |
|        - |  8734 | `	/* Point to the target VM */` |
|      ! 0 |  8735 | `	pVm = pCtx->pVm;` |
|        - |  8736 | `	/* Output */` |
|      ! 0 |  8737 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  8738 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  8739 | `		if( nDataLen > 0 ){` |
|      ! 0 |  8740 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  8741 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  8742 | `				/* Increment output length */` |
|      ! 0 |  8743 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  8744 | `			}` |
|      ! 0 |  8745 | `			if( rc == SXERR_ABORT ){` |
|        - |  8746 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  8747 | `				return PH7_ABORT;` |
|        - |  8748 | `			}` |
|      ! 0 |  8749 | `		}` |
|      ! 0 |  8750 | `	}` |
|      ! 0 |  8751 | `	return SXRET_OK;` |
|      ! 0 |  8752 |  |
|        - |  8753 | `/*` |
|        - |  8754 | ` * int print($string...)` |
|        - |  8755 | ` *  Output one or more messages.` |
|        - |  8756 | ` * Parameters` |
|        - |  8757 | ` *  $string` |
|        - |  8758 | ` *   Message to output.` |
|        - |  8759 | ` * Return` |
|        - |  8760 | ` *  1 always.` |
|        - |  8761 | ` */` |
|        2 |  8762 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8763 |  |
|        - |  8764 | `	const char *zData;` |
|        3 |  8765 | `	int nDataLen = 0;` |
|        - |  8766 | `	ph7_vm *pVm;` |
|        - |  8767 | `	int i,rc;` |
|        - |  8768 | `	/* Point to the target VM */` |
|        3 |  8769 | `	pVm = pCtx->pVm;` |
|        - |  8770 | `	/* Output */` |
|        5 |  8771 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  8772 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  8773 | `		if( nDataLen > 0 ){` |
|        3 |  8774 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  8775 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  8776 | `				/* Increment output length */` |
|        3 |  8777 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  8778 | `			}` |
|        3 |  8779 | `			if( rc == SXERR_ABORT ){` |
|        - |  8780 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  8781 | `				return PH7_ABORT;` |
|        - |  8782 | `			}` |
|        1 |  8783 | `		}` |
|        2 |  8784 | `	}` |
|        - |  8785 | `	/* Return 1 */` |
|        3 |  8786 | `	ph7_result_int(pCtx,1);` |
|        3 |  8787 | `	return SXRET_OK;` |
|        2 |  8788 |  |
|        - |  8789 | `/*` |
|        - |  8790 | ` * void exit(string $msg)` |
|        - |  8791 | ` * void exit(int $status)` |
|        - |  8792 | ` * void die(string $ms)` |
|        - |  8793 | ` * void die(int $status)` |
|        - |  8794 | ` *   Output a message and terminate program execution.` |
|        - |  8795 | ` * Parameter` |
|        - |  8796 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  8797 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  8798 | ` *  and not printed` |
|        - |  8799 | ` * Return` |
|        - |  8800 | ` *  NULL` |
|        - |  8801 | ` */` |
|      ! 0 |  8802 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  8803 |  |
|      ! 0 |  8804 | `	if( nArg > 0 ){` |
|      ! 0 |  8805 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  8806 | `			const char *zData;` |
|      ! 0 |  8807 | `			int iLen = 0;` |
|        - |  8808 | `			/* Print exit message */` |
|      ! 0 |  8809 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  8810 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  8811 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  8812 | `			sxi32 iExitStatus;` |
|        - |  8813 | `			/* Record exit status code */` |
|      ! 0 |  8814 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  8815 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  8816 | `		}` |
|      ! 0 |  8817 | `	}` |
|        - |  8818 | `	/* Check if we are in an included file */` |
|      ! 0 |  8819 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  8820 | `		/* Exit the entire process */` |
|      ! 0 |  8821 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  8822 | `	}` |
|        - |  8823 | `	/* Abort processing immediately */` |
|      ! 0 |  8824 | `	return PH7_ABORT;` |
|      ! 0 |  8825 |  |
|        - |  8826 | `/*` |
|        - |  8827 | ` * bool isset($var,...)` |
|        - |  8828 | ` *  Finds out whether a variable is set.` |
|        - |  8829 | ` * Parameters` |
|        - |  8830 | ` *  One or more variable to check.` |
|        - |  8831 | ` * Return` |
|        - |  8832 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  8833 | ` */` |
|    50500 |  8834 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8835 |  |
|        - |  8836 | `	ph7_value *pObj;` |
|    50502 |  8837 | `	int res = 0;` |
|        - |  8838 | `	int i;` |
|    50502 |  8839 | `	if( nArg < 1 ){` |
|        - |  8840 | `		/* Missing arguments,return false */` |
|      ! 0 |  8841 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  8842 | `		return SXRET_OK;` |
|        - |  8843 | `	}` |
|        - |  8844 | `	/* Iterate over available arguments */` |
|    68080 |  8845 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    50502 |  8846 | `		pObj = apArg[i];` |
|    50502 |  8847 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    32894 |  8848 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  8849 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  8850 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  8851 | `			}` |
|    16446 |  8852 | `		}` |
|    50502 |  8853 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    50502 |  8854 | `		if( !res ){` |
|        - |  8855 | `			/* Variable not set,return FALSE */` |
|    32924 |  8856 | `			ph7_result_bool(pCtx,0);` |
|    32924 |  8857 | `			return SXRET_OK;` |
|        - |  8858 | `		}` |
|     8791 |  8859 | `	}` |
|        - |  8860 | `	/* All given variable are set,return TRUE */` |
|    17580 |  8861 | `	ph7_result_bool(pCtx,1);` |
|    17580 |  8862 | `	return SXRET_OK;` |
|    25252 |  8863 |  |
|        - |  8864 | `/*` |
|        - |  8865 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  8866 | ` * frame,the reference table and discard it's contents.` |
|        - |  8867 | ` * This function never fail and always return SXRET_OK.` |
|        - |  8868 | ` */` |
|   591246 |  8869 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  8870 |  |
|        - |  8871 | `	ph7_value *pObj;` |
|        - |  8872 | `	VmRefObj *pRef;` |
|   591248 |  8873 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|   591248 |  8874 | `	if( pObj ){` |
|        - |  8875 | `		/* Release the object */` |
|   591248 |  8876 | `		PH7_MemObjRelease(pObj);` |
|   295623 |  8877 | `	}` |
|        - |  8878 | `	/* Remove old reference links */` |
|   591248 |  8879 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|   591248 |  8880 | `	if( pRef ){` |
|   591228 |  8881 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  8882 | `		/* Unlink from the reference table */` |
|   591228 |  8883 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|   591228 |  8884 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  8885 | `			VmSlot sFree;` |
|        - |  8886 | `			/* Restore to the free list */` |
|   591222 |  8887 | `			sFree.nIdx = nObjIdx;` |
|   591222 |  8888 | `			sFree.pUserData = 0;` |
|   591222 |  8889 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|   295610 |  8890 | `		}` |
|   295613 |  8891 | `	}` |
|   591248 |  8892 | `	return SXRET_OK;` |
|        2 |  8893 |  |
|        - |  8894 | `/*` |
|        - |  8895 | ` * void unset($var,...)` |
|        - |  8896 | ` *   Unset one or more given variable.` |
|        - |  8897 | ` * Parameters` |
|        - |  8898 | ` *  One or more variable to unset.` |
|        - |  8899 | ` * Return` |
|        - |  8900 | ` *  Nothing.` |
|        - |  8901 | ` */` |
|     2626 |  8902 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8903 |  |
|        - |  8904 | `	ph7_value *pObj;` |
|        - |  8905 | `	ph7_vm *pVm;` |
|        - |  8906 | `	int i;` |
|        - |  8907 | `	/* Point to the target VM */` |
|     2628 |  8908 | `	pVm = pCtx->pVm;` |
|        - |  8909 | `	/* Iterate and unset */` |
|     8108 |  8910 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     5482 |  8911 | `		pObj = apArg[i];` |
|     5482 |  8912 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      700 |  8913 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  8914 | `				/* Throw an error */` |
|      ! 0 |  8915 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  8916 | `			}` |
|      351 |  8917 | `		}else{` |
|     4783 |  8918 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  8919 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     4783 |  8920 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     4777 |  8921 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2388 |  8922 | `			}` |
|        - |  8923 | `		}` |
|     2742 |  8924 | `	}` |
|     2628 |  8925 | `	return SXRET_OK;` |
|        2 |  8926 |  |
|        - |  8927 | `/*` |
|        - |  8928 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  8929 | ` */` |
|      108 |  8930 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8931 |  |
|      109 |  8932 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      109 |  8933 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  8934 | `	ph7_value *pObj;` |
|        - |  8935 | `	sxu32 nIdx;` |
|        - |  8936 | `	/* Extract the memory object */` |
|      109 |  8937 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      109 |  8938 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      109 |  8939 | `	if( pObj ){` |
|      109 |  8940 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      107 |  8941 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  8942 | `				SyString sName;` |
|        - |  8943 | `				ph7_value sKey;` |
|        - |  8944 | `				/* Perform the insertion */` |
|      107 |  8945 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      107 |  8946 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      107 |  8947 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      107 |  8948 | `				PH7_MemObjRelease(&sKey);` |
|       53 |  8949 | `			}` |
|       53 |  8950 | `		}` |
|       54 |  8951 | `	}` |
|      109 |  8952 | `	return SXRET_OK;` |
|        1 |  8953 |  |
|        - |  8954 | `/*` |
|        - |  8955 | ` * array get_defined_vars(void)` |
|        - |  8956 | ` *  Returns an array of all defined variables.` |
|        - |  8957 | ` * Parameter` |
|        - |  8958 | ` *  None` |
|        - |  8959 | ` * Return` |
|        - |  8960 | ` *  An array with all the variables defined in the current scope.` |
|        - |  8961 | ` */` |
|        2 |  8962 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8963 |  |
|        3 |  8964 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8965 | `	ph7_value *pArray;` |
|        - |  8966 | `	/* Create a new array */` |
|        3 |  8967 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8968 | ` 	if( pArray == 0 ){` |
|      ! 0 |  8969 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8970 | `		SXUNUSED(apArg);` |
|        - |  8971 | `		/* Return NULL */` |
|      ! 0 |  8972 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8973 | `		return SXRET_OK;` |
|        - |  8974 | `	}` |
|        - |  8975 | `	/* Superglobals first */` |
|        3 |  8976 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  8977 | `	/* Then variable defined in the current frame */` |
|        3 |  8978 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  8979 | `	/* Finally,return the created array */` |
|        3 |  8980 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8981 | `	return SXRET_OK;` |
|        2 |  8982 |  |
|        - |  8983 | `/*` |
|        - |  8984 | ` * bool gettype($var)` |
|        - |  8985 | ` *  Get the type of a variable` |
|        - |  8986 | ` * Parameters` |
|        - |  8987 | ` *   $var` |
|        - |  8988 | ` *    The variable being type checked.` |
|        - |  8989 | ` * Return` |
|        - |  8990 | ` *   String representation of the given variable type.` |
|        - |  8991 | ` */` |
|       26 |  8992 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8993 |  |
|       27 |  8994 | `	const char *zType = "Empty";` |
|       27 |  8995 | `	if( nArg > 0 ){` |
|       27 |  8996 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       13 |  8997 | `	}` |
|        - |  8998 | `	/* Return the variable type */` |
|       27 |  8999 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       27 |  9000 | `	return SXRET_OK;` |
|        1 |  9001 |  |
|        - |  9002 | `/*` |
|        - |  9003 | ` * string get_resource_type(resource $handle)` |
|        - |  9004 | ` *  This function gets the type of the given resource.` |
|        - |  9005 | ` * Parameters` |
|        - |  9006 | ` *  $handle` |
|        - |  9007 | ` *  The evaluated resource handle.` |
|        - |  9008 | ` * Return` |
|        - |  9009 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  9010 | ` *  representing its type. If the type is not identified by this function` |
|        - |  9011 | ` *  the return value will be the string Unknown.` |
|        - |  9012 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  9013 | ` *  is not a resource.` |
|        - |  9014 | ` */` |
|        2 |  9015 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9016 |  |
|        3 |  9017 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  9018 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  9019 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9020 | `		return PH7_OK;` |
|        - |  9021 | `	}` |
|        3 |  9022 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  9023 | `	return SXRET_OK;` |
|        2 |  9024 |  |
|        - |  9025 | `/*` |
|        - |  9026 | ` * void var_dump(expression,....)` |
|        - |  9027 | ` *   var_dump � Dumps information about a variable` |
|        - |  9028 | ` * Parameters` |
|        - |  9029 | ` *   One or more expression to dump.` |
|        - |  9030 | ` * Returns` |
|        - |  9031 | ` *  Nothing.` |
|        - |  9032 | ` */` |
|      248 |  9033 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9034 |  |
|        - |  9035 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  9036 | `	int i;` |
|      250 |  9037 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  9038 | `	/* Dump one or more expressions */` |
|      504 |  9039 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      256 |  9040 | `		ph7_value *pObj = apArg[i];` |
|        - |  9041 | `		/* Reset the working buffer */` |
|      256 |  9042 | `		SyBlobReset(&sDump);` |
|        - |  9043 | `		/* Dump the given expression */` |
|      256 |  9044 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  9045 | `		/* Output */` |
|      256 |  9046 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      256 |  9047 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      127 |  9048 | `		}` |
|      129 |  9049 | `	}` |
|        - |  9050 | `	/* Release the working buffer */` |
|      250 |  9051 | `	SyBlobRelease(&sDump);` |
|      250 |  9052 | `	return SXRET_OK;` |
|        2 |  9053 |  |
|        - |  9054 | `/*` |
|        - |  9055 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  9056 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  9057 | ` * Parameters` |
|        - |  9058 | ` *   expression: Expression to dump` |
|        - |  9059 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  9060 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  9061 | ` *            print_r() will return the information rather than print it.` |
|        - |  9062 | ` * Return` |
|        - |  9063 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  9064 | ` *  Otherwise, the return value is TRUE.` |
|        - |  9065 | ` */` |
|       16 |  9066 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9067 |  |
|       17 |  9068 | `	int ret_string = 0;` |
|        - |  9069 | `	SyBlob sDump;` |
|       17 |  9070 | `	if( nArg < 1 ){` |
|        - |  9071 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9072 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9073 | `		return SXRET_OK;` |
|        - |  9074 | `	}` |
|       17 |  9075 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  9076 | `	if ( nArg > 1 ){` |
|        - |  9077 | `		/* Where to redirect output */` |
|       11 |  9078 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  9079 | `	}` |
|        - |  9080 | `	/* Generate dump */` |
|       17 |  9081 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  9082 | `	if( !ret_string ){` |
|        - |  9083 | `		/* Output dump */` |
|        7 |  9084 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9085 | `		/* Return true */` |
|        7 |  9086 | `		ph7_result_bool(pCtx,1);` |
|        4 |  9087 | `	}else{` |
|        - |  9088 | `		/* Generated dump as return value */` |
|       11 |  9089 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9090 | `	}` |
|        - |  9091 | `	/* Release the working buffer */` |
|       17 |  9092 | `	SyBlobRelease(&sDump);` |
|       17 |  9093 | `	return SXRET_OK;` |
|        9 |  9094 |  |
|        - |  9095 | `/*` |
|        - |  9096 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  9097 | ` * Same job as print_r. (see coment above)` |
|        - |  9098 | ` */` |
|        2 |  9099 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9100 |  |
|        3 |  9101 | `	int ret_string = 0;` |
|        - |  9102 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  9103 | `	if( nArg < 1 ){` |
|        - |  9104 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9105 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9106 | `		return SXRET_OK;` |
|        - |  9107 | `	}` |
|        3 |  9108 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  9109 | `	if ( nArg > 1 ){` |
|        - |  9110 | `		/* Where to redirect output */` |
|        3 |  9111 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  9112 | `	}` |
|        - |  9113 | `	/* Generate dump */` |
|        3 |  9114 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  9115 | `	if( !ret_string ){` |
|        - |  9116 | `		/* Output dump */` |
|      ! 0 |  9117 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9118 | `		/* Return NULL */` |
|      ! 0 |  9119 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9120 | `	}else{` |
|        - |  9121 | `		/* Generated dump as return value */` |
|        3 |  9122 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9123 | `	}` |
|        - |  9124 | `	/* Release the working buffer */` |
|        3 |  9125 | `	SyBlobRelease(&sDump);` |
|        3 |  9126 | `	return SXRET_OK;` |
|        2 |  9127 |  |
|        - |  9128 | `/*` |
|        - |  9129 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  9130 | ` *  Set/get the various assert flags.` |
|        - |  9131 | ` * Parameter` |
|        - |  9132 | ` * $what` |
|        - |  9133 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  9134 | ` *   ASSERT_WARNING         Issue a warning for each failed assertion` |
|        - |  9135 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  9136 | ` *   ASSERT_QUIET_EVAL      Not used` |
|        - |  9137 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  9138 | ` * $value` |
|        - |  9139 | ` *   An optional new value for the option.` |
|        - |  9140 | ` * Return` |
|        - |  9141 | ` *  Old setting on success or FALSE on failure.` |
|        - |  9142 | ` */` |
|        8 |  9143 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9144 |  |
|        9 |  9145 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9146 | `	int iOld,iNew,iValue;` |
|        9 |  9147 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|        - |  9148 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  9149 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9150 | `		return PH7_OK;` |
|        - |  9151 | `	}` |
|        - |  9152 | `	/* Save old assertion flags */` |
|        9 |  9153 | `	iOld = pVm->iAssertFlags;` |
|        - |  9154 | `	/* Extract the new flags */` |
|        9 |  9155 | `	iNew = ph7_value_to_int(apArg[0]);` |
|        9 |  9156 | `	if( iNew == PH7_ASSERT_DISABLE ){` |
|        7 |  9157 | `		pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        7 |  9158 | `		if( nArg > 1 ){` |
|        5 |  9159 | `			iValue = !ph7_value_to_bool(apArg[1]);` |
|        5 |  9160 | `			if( iValue ){` |
|        - |  9161 | `				/* Disable assertion */` |
|        3 |  9162 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        1 |  9163 | `			}` |
|        3 |  9164 | `		}` |
|        6 |  9165 | `	}else if( iNew == PH7_ASSERT_WARNING ){` |
|      ! 0 |  9166 | `		pVm->iAssertFlags &= ~PH7_ASSERT_WARNING;` |
|      ! 0 |  9167 | `		if( nArg > 1 ){` |
|      ! 0 |  9168 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  9169 | `			if( iValue ){` |
|        - |  9170 | `				/* Issue a warning for each failed assertion */` |
|      ! 0 |  9171 | `				pVm->iAssertFlags \|= PH7_ASSERT_WARNING;` |
|      ! 0 |  9172 | `			}` |
|      ! 0 |  9173 | `		}` |
|        3 |  9174 | `	}else if( iNew == PH7_ASSERT_BAIL ){` |
|        3 |  9175 | `		pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        3 |  9176 | `		if( nArg > 1 ){` |
|        3 |  9177 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|        3 |  9178 | `			if( iValue ){` |
|        - |  9179 | `				/* Terminate execution on failed assertions */` |
|        3 |  9180 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        1 |  9181 | `			}` |
|        2 |  9182 | `		}` |
|        1 |  9183 | `	}else if( iNew == PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  9184 | `		pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9185 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|        - |  9186 | `			/* Callback to call on failed assertions */` |
|      ! 0 |  9187 | `			PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  9188 | `			pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9189 | `		}` |
|      ! 0 |  9190 | `	}` |
|        - |  9191 | `	/* Return the old flags */` |
|        9 |  9192 | `	ph7_result_int(pCtx,iOld);` |
|        9 |  9193 | `	return PH7_OK;` |
|        5 |  9194 |  |
|        - |  9195 | `/*` |
|        - |  9196 | ` * bool assert(mixed $assertion)` |
|        - |  9197 | ` *  Checks if assertion is FALSE.` |
|        - |  9198 | ` * Parameter` |
|        - |  9199 | ` *  $assertion` |
|        - |  9200 | ` *    The assertion to test.` |
|        - |  9201 | ` * Return` |
|        - |  9202 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  9203 | ` */` |
|       14 |  9204 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9205 |  |
|       15 |  9206 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9207 | `	ph7_value *pAssert;` |
|        - |  9208 | `	int iFlags,iResult;` |
|       15 |  9209 | `	if( nArg < 1 ){` |
|        - |  9210 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  9211 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9212 | `		return PH7_OK;` |
|        - |  9213 | `	}` |
|       15 |  9214 | `	iFlags = pVm->iAssertFlags;` |
|       15 |  9215 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  9216 | `		/* Assertion is disabled,return FALSE */` |
|      ! 0 |  9217 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9218 | `		return PH7_OK;` |
|        - |  9219 | `	}` |
|       15 |  9220 | `	pAssert = apArg[0];` |
|       15 |  9221 | `	iResult = 1; /* cc warning */` |
|       15 |  9222 | `	if( pAssert->iFlags & MEMOBJ_STRING ){` |
|        - |  9223 | `		SyString sChunk;` |
|        5 |  9224 | `		SyStringInitFromBuf(&sChunk,SyBlobData(&pAssert->sBlob),SyBlobLength(&pAssert->sBlob));` |
|        5 |  9225 | `		if( sChunk.nByte > 0 ){` |
|        5 |  9226 | `			VmEvalChunk(pVm,pCtx,&sChunk,PH7_PHP_ONLY\|PH7_PHP_EXPR,FALSE);` |
|        - |  9227 | `			/* Extract evaluation result */` |
|        5 |  9228 | `			iResult = ph7_value_to_bool(pCtx->pRet);` |
|        3 |  9229 | `		}else{` |
|      ! 0 |  9230 | `			iResult = 0;` |
|        - |  9231 | `		}` |
|        3 |  9232 | `	}else{` |
|        - |  9233 | `		/* Perform a boolean cast */` |
|       11 |  9234 | `		iResult = ph7_value_to_bool(apArg[0]);` |
|        - |  9235 | `	}` |
|       15 |  9236 | `	if( !iResult ){` |
|        - |  9237 | `		/* Assertion failed */` |
|        9 |  9238 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  9239 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  9240 | `			ph7_value sFile,sLine;` |
|        - |  9241 | `			ph7_value *apCbArg[3];` |
|        - |  9242 | `			SyString *pFile;` |
|        - |  9243 | `			/* Extract the processed script */` |
|      ! 0 |  9244 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  9245 | `			if( pFile == 0 ){` |
|      ! 0 |  9246 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  9247 | `			}` |
|        - |  9248 | `			/* Invoke the callback */` |
|      ! 0 |  9249 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  9250 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  9251 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  9252 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  9253 | `			apCbArg[2] = pAssert;` |
|      ! 0 |  9254 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  9255 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  9256 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  9257 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  9258 | `		}` |
|        9 |  9259 | `		if( iFlags & PH7_ASSERT_WARNING ){` |
|        - |  9260 | `			/* Emit a warning */` |
|        9 |  9261 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Assertion failed");` |
|        4 |  9262 | `		}` |
|        9 |  9263 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  9264 | `			/* Abort VM execution immediately */` |
|        3 |  9265 | `			return PH7_ABORT;` |
|        - |  9266 | `		}` |
|        3 |  9267 | `	}` |
|        - |  9268 | `	/* Assertion result */` |
|       13 |  9269 | `	ph7_result_bool(pCtx,iResult);` |
|       13 |  9270 | `	return PH7_OK;` |
|        8 |  9271 |  |
|        - |  9272 | `/*` |
|        - |  9273 | ` * Section:` |
|        - |  9274 | ` *  Error reporting functions.` |
|        - |  9275 | ` * Status:` |
|        - |  9276 | ` *    Stable.` |
|        - |  9277 | ` */` |
|        - |  9278 | `/*` |
|        - |  9279 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  9280 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  9281 | ` * Parameters` |
|        - |  9282 | ` *  $error_msg` |
|        - |  9283 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  9284 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  9285 | ` * $error_type` |
|        - |  9286 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  9287 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  9288 | ` * Return` |
|        - |  9289 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  9290 | ` */` |
|       12 |  9291 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9292 |  |
|       14 |  9293 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  9294 | `	int rc = PH7_OK;` |
|       14 |  9295 | `	if( nArg > 0 ){` |
|        - |  9296 | `		const char *zErr;` |
|        - |  9297 | `		int nLen;` |
|        - |  9298 | `		/* Extract the error message */` |
|       12 |  9299 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  9300 | `		if( nArg > 1 ){` |
|        - |  9301 | `			/* Extract the error type */` |
|       12 |  9302 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  9303 | `			switch( nErr ){` |
|        1 |  9304 | `			case 1:   /* E_ERROR */` |
|        - |  9305 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  9306 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  9307 | `			case 256: /* E_USER_ERROR */` |
|        3 |  9308 | `				nErr = PH7_CTX_ERR;` |
|        3 |  9309 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  9310 | `				break;` |
|        1 |  9311 | `			case 2:   /* E_WARNING */` |
|        - |  9312 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  9313 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  9314 | `			case 512: /* E_USER_WARNING */` |
|        3 |  9315 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  9316 | `				break;` |
|        3 |  9317 | `			default:` |
|        8 |  9318 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  9319 | `				break;` |
|        - |  9320 | `			}` |
|        5 |  9321 | `		}` |
|        - |  9322 | `		/* Report error */` |
|       12 |  9323 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  9324 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  9325 | `			return rc;` |
|        - |  9326 | `		}` |
|        - |  9327 | `		/* Return true */` |
|       12 |  9328 | `		ph7_result_bool(pCtx,1);` |
|        7 |  9329 | `	}else{` |
|        - |  9330 | `		/* Missing arguments,return FALSE */` |
|        3 |  9331 | `		ph7_result_bool(pCtx,0);` |
|        - |  9332 | `	}` |
|       14 |  9333 | `	return rc;` |
|        8 |  9334 |  |
|        - |  9335 | `/*` |
|        - |  9336 | ` * int error_reporting([int $level])` |
|        - |  9337 | ` *  Sets which PHP errors are reported.` |
|        - |  9338 | ` * Parameters` |
|        - |  9339 | ` *  $level` |
|        - |  9340 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  9341 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  9342 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  9343 | ` *   levels will not always behave as expected.` |
|        - |  9344 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  9345 | ` *   in the predefined constants.` |
|        - |  9346 | ` * Return` |
|        - |  9347 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  9348 | ` *   parameter is given.` |
|        - |  9349 | ` */` |
|       18 |  9350 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9351 |  |
|       19 |  9352 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9353 | `	int nOld;` |
|        - |  9354 | `	/* Extract the old reporting level */` |
|       19 |  9355 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       19 |  9356 | `	if( nArg > 0 ){` |
|        - |  9357 | `		int nNew;` |
|        - |  9358 | `		/* Extract the desired error reporting level */` |
|       11 |  9359 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       11 |  9360 | `		if( !nNew ){` |
|        - |  9361 | `			/* Do not report errors at all */` |
|        5 |  9362 | `			pVm->bErrReport = 0;` |
|        3 |  9363 | `		}else{` |
|        - |  9364 | `			/* Report all errors */` |
|        7 |  9365 | `			pVm->bErrReport = 1;` |
|        - |  9366 | `		}` |
|        5 |  9367 | `	}` |
|        - |  9368 | `	/* Return the old level */` |
|       19 |  9369 | `	ph7_result_int(pCtx,nOld);` |
|       19 |  9370 | `	return PH7_OK;` |
|        1 |  9371 |  |
|        - |  9372 | `/*` |
|        - |  9373 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  9374 | ` *  Send an error message somewhere.` |
|        - |  9375 | ` * Parameter` |
|        - |  9376 | ` *  $message` |
|        - |  9377 | ` *   The error message that should be logged.` |
|        - |  9378 | ` *  $message_type` |
|        - |  9379 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  9380 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  9381 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  9382 | ` *       This is the default option.` |
|        - |  9383 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  9384 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  9385 | ` *    2  No longer an option.` |
|        - |  9386 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  9387 | ` *       to the end of the message string.` |
|        - |  9388 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  9389 | ` *  $destination` |
|        - |  9390 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  9391 | ` *  $extra_headers` |
|        - |  9392 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  9393 | ` * Return` |
|        - |  9394 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9395 | ` * NOTE:` |
|        - |  9396 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  9397 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  9398 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  9399 | ` *  Otherwise this function is no-op.` |
|        - |  9400 | ` */` |
|        4 |  9401 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9402 |  |
|        - |  9403 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  9404 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  9405 | `	int iType = 0;` |
|        5 |  9406 | `	if( nArg < 1 ){` |
|        - |  9407 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  9408 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9409 | `		return PH7_OK;` |
|        - |  9410 | `	}` |
|        5 |  9411 | `	if( pVm->xErrLog  ){` |
|        - |  9412 | `		/* Invoke the user callback */` |
|      ! 0 |  9413 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  9414 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  9415 | `		if( nArg > 1 ){` |
|      ! 0 |  9416 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  9417 | `			if( nArg > 2 ){` |
|      ! 0 |  9418 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  9419 | `				if( nArg > 3 ){` |
|      ! 0 |  9420 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  9421 | `				}` |
|      ! 0 |  9422 | `			}` |
|      ! 0 |  9423 | `		}` |
|      ! 0 |  9424 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  9425 | `	}` |
|        - |  9426 | `	/* Retun TRUE */` |
|        5 |  9427 | `	ph7_result_bool(pCtx,1);` |
|        5 |  9428 | `	return PH7_OK;` |
|        3 |  9429 |  |
|        - |  9430 | `/*` |
|        - |  9431 | ` * bool restore_exception_handler(void)` |
|        - |  9432 | ` *  Restores the previously defined exception handler function.` |
|        - |  9433 | ` * Parameter` |
|        - |  9434 | ` *  None` |
|        - |  9435 | ` * Return` |
|        - |  9436 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  9437 | ` */` |
|        4 |  9438 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9439 |  |
|        5 |  9440 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9441 | `	ph7_value *pOld,*pNew;` |
|        - |  9442 | `	/* Point to the old and the new handler */` |
|        5 |  9443 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  9444 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  9445 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9446 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9447 | `		SXUNUSED(apArg);` |
|        - |  9448 | `		/* No installed handler,return FALSE */` |
|        5 |  9449 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9450 | `		return PH7_OK;` |
|        - |  9451 | `	}` |
|        - |  9452 | `	/* Copy the old handler */` |
|      ! 0 |  9453 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9454 | `	PH7_MemObjRelease(pOld);` |
|        - |  9455 | `	/* Return TRUE */` |
|      ! 0 |  9456 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9457 | `	return PH7_OK;` |
|        3 |  9458 |  |
|        - |  9459 | `/*` |
|        - |  9460 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  9461 | ` *  Sets a user-defined exception handler function.` |
|        - |  9462 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  9463 | ` * NOTE` |
|        - |  9464 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  9465 | ` *  the satndard PHP engine.` |
|        - |  9466 | ` * Parameters` |
|        - |  9467 | ` *  $exception_handler` |
|        - |  9468 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  9469 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  9470 | ` *   that was thrown.` |
|        - |  9471 | ` *  Note:` |
|        - |  9472 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9473 | ` * Return` |
|        - |  9474 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  9475 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9476 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9477 | ` */` |
|        4 |  9478 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9479 |  |
|        5 |  9480 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9481 | `	ph7_value *pOld,*pNew;` |
|        - |  9482 | `	/* Point to the old and the new handler */` |
|        5 |  9483 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  9484 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  9485 | `	/* Return the old handler */` |
|        5 |  9486 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        5 |  9487 | `	if( nArg > 0 ){` |
|        5 |  9488 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9489 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  9490 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  9491 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  9492 | `		}else{` |
|        5 |  9493 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9494 | `			/* Install the new handler */` |
|        5 |  9495 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9496 | `		}` |
|        2 |  9497 | `	}` |
|        5 |  9498 | `	return PH7_OK;` |
|        1 |  9499 |  |
|        - |  9500 | `/*` |
|        - |  9501 | ` * bool restore_error_handler(void)` |
|        - |  9502 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9503 | ` * Parameters:` |
|        - |  9504 | ` *  None.` |
|        - |  9505 | ` * Return` |
|        - |  9506 | ` *  Always TRUE.` |
|        - |  9507 | ` */` |
|        4 |  9508 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9509 |  |
|        5 |  9510 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9511 | `	ph7_value *pOld,*pNew;` |
|        - |  9512 | `	/* Point to the old and the new handler */` |
|        5 |  9513 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  9514 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  9515 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9516 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9517 | `		SXUNUSED(apArg);` |
|        - |  9518 | `		/* No installed callback,return FALSE */` |
|        5 |  9519 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9520 | `		return PH7_OK;` |
|        - |  9521 | `	}` |
|        - |  9522 | `	/* Copy the old callback */` |
|      ! 0 |  9523 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9524 | `	PH7_MemObjRelease(pOld);` |
|        - |  9525 | `	/* Return TRUE */` |
|      ! 0 |  9526 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9527 | `	return PH7_OK;` |
|        3 |  9528 |  |
|        - |  9529 | `/*` |
|        - |  9530 | ` * value set_error_handler(callable $error_handler)` |
|        - |  9531 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9532 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9533 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9534 | ` *  Sets a user-defined error handler function.` |
|        - |  9535 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  9536 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  9537 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  9538 | ` *  conditions (using trigger_error()).` |
|        - |  9539 | ` * Parameters` |
|        - |  9540 | ` *  $error_handler` |
|        - |  9541 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  9542 | ` *   describing the error.` |
|        - |  9543 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  9544 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  9545 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  9546 | ` *   The function can be shown as:` |
|        - |  9547 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  9548 | ` *     errno` |
|        - |  9549 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  9550 | ` *   errstr` |
|        - |  9551 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  9552 | ` *   errfile` |
|        - |  9553 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  9554 | ` *     was raised in, as a string.` |
|        - |  9555 | ` *  Note:` |
|        - |  9556 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9557 | ` * Return` |
|        - |  9558 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  9559 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9560 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9561 | ` */` |
|     5226 |  9562 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9563 |  |
|     5228 |  9564 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9565 | `	ph7_value *pOld,*pNew;` |
|        - |  9566 | `	/* Point to the old and the new handler */` |
|     5228 |  9567 | `	pOld = &pVm->aErrCB[0];` |
|     5228 |  9568 | `	pNew = &pVm->aErrCB[1];` |
|        - |  9569 | `	/* Return the old handler */` |
|     5228 |  9570 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     5228 |  9571 | `	if( nArg > 0 ){` |
|     5228 |  9572 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9573 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     2613 |  9574 | `			PH7_MemObjRelease(pNew);` |
|     2613 |  9575 | `			ph7_result_bool(pCtx,1);` |
|     1307 |  9576 | `		}else{` |
|     2616 |  9577 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9578 | `			/* Install the new handler */` |
|     2616 |  9579 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9580 | `		}` |
|     2613 |  9581 | `	}` |
|     5228 |  9582 | `	return PH7_OK;` |
|        2 |  9583 |  |
|        - |  9584 | `/*` |
|        - |  9585 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  9586 | ` *  Generates a backtrace.` |
|        - |  9587 | ` * Paramaeter` |
|        - |  9588 | ` *  $options` |
|        - |  9589 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  9590 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  9591 | ` *   all the function/method arguments, to save memory.` |
|        - |  9592 | ` * $limit` |
|        - |  9593 | ` *   (Not Used)` |
|        - |  9594 | ` * Return` |
|        - |  9595 | ` *  An array.The possible returned elements are as follows:` |
|        - |  9596 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  9597 | ` *          Name        Type      Description` |
|        - |  9598 | ` *          ------      ------     -----------` |
|        - |  9599 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  9600 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  9601 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  9602 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  9603 | ` *          object      object    The current object.` |
|        - |  9604 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  9605 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  9606 | ` */` |
|       26 |  9607 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9608 |  |
|       28 |  9609 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9610 | `	ph7_value *pArray;` |
|        - |  9611 | `	ph7_class *pClass;` |
|        - |  9612 | `	ph7_value *pValue;` |
|        - |  9613 | `	SyString *pFile;` |
|        - |  9614 | `	/* Create a new array */` |
|       28 |  9615 | `	pArray = ph7_context_new_array(pCtx);` |
|       28 |  9616 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       28 |  9617 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9618 | `		/* Out of memory,return NULL */` |
|      ! 0 |  9619 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  9620 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9621 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9622 | `		SXUNUSED(apArg);` |
|      ! 0 |  9623 | `		return PH7_OK;` |
|        - |  9624 | `	}` |
|        - |  9625 | `	/* Dump running function name and it's arguments  */` |
|       28 |  9626 | `	if( pVm->pFrame->pParent ){` |
|       28 |  9627 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  9628 | `		ph7_vm_func *pFunc;` |
|        - |  9629 | `		ph7_value *pArg;` |
|       28 |  9630 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9631 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  9632 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  9633 | `		}` |
|       28 |  9634 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       28 |  9635 | `		if( pFrame->pParent && pFunc ){` |
|       28 |  9636 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|       28 |  9637 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|       28 |  9638 | `			ph7_value_reset_string_cursor(pValue);` |
|       13 |  9639 | `		}` |
|        - |  9640 | `		/* Function arguments */` |
|       28 |  9641 | `		pArg = ph7_context_new_array(pCtx);` |
|       28 |  9642 | `		if( pArg  ){` |
|        - |  9643 | `			ph7_value *pObj;` |
|        - |  9644 | `			VmSlot *aSlot;` |
|        - |  9645 | `			sxu32 n;` |
|        - |  9646 | `			/* Start filling the array with the given arguments */` |
|       28 |  9647 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|       98 |  9648 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|       72 |  9649 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|       72 |  9650 | `				if( pObj ){` |
|       72 |  9651 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|       35 |  9652 | `				}` |
|       37 |  9653 | `			}` |
|        - |  9654 | `			/* Save the array */` |
|       28 |  9655 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|       13 |  9656 | `		}` |
|       13 |  9657 | `	}` |
|       28 |  9658 | `	ph7_value_int(pValue,1);` |
|        - |  9659 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  9660 | `	 * line numbers at run-time. )` |
|        - |  9661 | `	 */` |
|       28 |  9662 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  9663 | `	/* Current processed script */` |
|       28 |  9664 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       28 |  9665 | `	if( pFile ){` |
|       28 |  9666 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|       28 |  9667 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|       28 |  9668 | `		ph7_value_reset_string_cursor(pValue);` |
|       13 |  9669 | `	}` |
|        - |  9670 | `	/* Top class */` |
|       28 |  9671 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|       28 |  9672 | `	if( pClass ){` |
|       24 |  9673 | `		ph7_value_reset_string_cursor(pValue);` |
|       24 |  9674 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       24 |  9675 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|       11 |  9676 | `	}` |
|        - |  9677 | `	/* Return the freshly created array */` |
|       28 |  9678 | `	ph7_result_value(pCtx,pArray);` |
|        - |  9679 | `	/*` |
|        - |  9680 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  9681 | `	 * as soon we return from this function.` |
|        - |  9682 | `	 */` |
|       28 |  9683 | `	return PH7_OK;` |
|       15 |  9684 |  |
|        - |  9685 | `/*` |
|        - |  9686 | ` * Generate a small backtrace.` |
|        - |  9687 | ` * Store the generated dump in the given BLOB` |
|        - |  9688 | ` */` |
|        4 |  9689 | `static int VmMiniBacktrace(` |
|        - |  9690 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  9691 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  9692 | `	)` |
|        1 |  9693 |  |
|        5 |  9694 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  9695 | `	ph7_vm_func *pFunc;` |
|        - |  9696 | `	ph7_class *pClass;` |
|        - |  9697 | `	SyString *pFile;` |
|        - |  9698 | `	/* Called function */` |
|        5 |  9699 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9700 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  9701 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  9702 | `	}` |
|        5 |  9703 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  9704 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  9705 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  9706 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  9707 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  9708 | `	}else{` |
|      ! 0 |  9709 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  9710 | `	}` |
|        5 |  9711 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  9712 | `	/* Current processed script */` |
|        5 |  9713 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  9714 | `	if( pFile ){` |
|        5 |  9715 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  9716 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  9717 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  9718 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  9719 | `	}` |
|        - |  9720 | `	/* Top class */` |
|        5 |  9721 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  9722 | `	if( pClass ){` |
|      ! 0 |  9723 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  9724 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  9725 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  9726 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  9727 | `	}` |
|        5 |  9728 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  9729 | `	/* All done */` |
|        5 |  9730 | `	return SXRET_OK;` |
|        1 |  9731 |  |
|        - |  9732 | `/*` |
|        - |  9733 | ` * void debug_print_backtrace()` |
|        - |  9734 | ` *  Prints a backtrace` |
|        - |  9735 | ` * Parameters` |
|        - |  9736 | ` * None` |
|        - |  9737 | ` * Return` |
|        - |  9738 | ` * NULL` |
|        - |  9739 | ` */` |
|        2 |  9740 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9741 |  |
|        3 |  9742 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9743 | `	SyBlob sDump;` |
|        3 |  9744 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  9745 | `	/* Generate the backtrace */` |
|        3 |  9746 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  9747 | `	/* Output backtrace */` |
|        3 |  9748 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9749 | `	/* All done,cleanup */` |
|        3 |  9750 | `	SyBlobRelease(&sDump);` |
|        1 |  9751 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9752 | `	SXUNUSED(apArg);` |
|        3 |  9753 | `	return PH7_OK;` |
|        1 |  9754 |  |
|        - |  9755 | `/*` |
|        - |  9756 | ` * string debug_string_backtrace()` |
|        - |  9757 | ` *  Generate a backtrace` |
|        - |  9758 | ` * Parameters` |
|        - |  9759 | ` * None` |
|        - |  9760 | ` * Return` |
|        - |  9761 | ` *  A mini backtrace().` |
|        - |  9762 | ` * Note that this is a symisc extension.` |
|        - |  9763 | ` */` |
|        2 |  9764 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9765 |  |
|        3 |  9766 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9767 | `	SyBlob sDump;` |
|        3 |  9768 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  9769 | `	/* Generate the backtrace */` |
|        3 |  9770 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  9771 | `	/* Return the backtrace */` |
|        3 |  9772 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  9773 | `	/* All done,cleanup */` |
|        3 |  9774 | `	SyBlobRelease(&sDump);` |
|        1 |  9775 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9776 | `	SXUNUSED(apArg);` |
|        3 |  9777 | `	return PH7_OK;` |
|        1 |  9778 |  |
|        - |  9779 | `/*` |
|        - |  9780 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  9781 | ` * exception is triggered.` |
|        - |  9782 | ` */` |
|       10 |  9783 | `static sxi32 VmUncaughtException(` |
|        - |  9784 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  9785 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  9786 | `	)` |
|        2 |  9787 |  |
|        - |  9788 | `	ph7_value *apArg[2],sArg;` |
|       12 |  9789 | `	int nArg = 1;` |
|        - |  9790 | `	sxi32 rc;` |
|       12 |  9791 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  9792 | `		/* Nesting limit reached */` |
|      ! 0 |  9793 | `		return SXRET_OK;` |
|        - |  9794 | `	}` |
|        - |  9795 | `	/* Call any exception handler if available */` |
|       12 |  9796 | `	PH7_MemObjInit(pVm,&sArg);` |
|       12 |  9797 | `	if( pThis ){` |
|        - |  9798 | `		/* Load the exception instance */` |
|       12 |  9799 | `		sArg.x.pOther = pThis;` |
|       12 |  9800 | `		pThis->iRef++;` |
|       12 |  9801 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|        7 |  9802 | `	}else{` |
|      ! 0 |  9803 | `		nArg = 0;` |
|        - |  9804 | `	}` |
|       12 |  9805 | `	apArg[0] = &sArg;` |
|        - |  9806 | `	/* Call the exception handler if available */` |
|       12 |  9807 | `	pVm->nExceptDepth++;` |
|       12 |  9808 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|       12 |  9809 | `	pVm->nExceptDepth--;` |
|       12 |  9810 | `	if( rc != SXRET_OK ){` |
|        - |  9811 | `		SyBlob sMsgBuf;` |
|        9 |  9812 | `		const char *zClass = "Exception";` |
|        9 |  9813 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  9814 | `		const char *zMsg;` |
|        - |  9815 | `		sxu32 nMsg;` |
|        - |  9816 | `		const char *zFuncName;` |
|        - |  9817 | `		int nFuncLen;` |
|        9 |  9818 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|        9 |  9819 | `		if( pThis ){` |
|        - |  9820 | `			ph7_class_method *pGetMessage;` |
|        - |  9821 | `			ph7_value sMsg;` |
|        - |  9822 | `			const char *zTmp;` |
|        - |  9823 | `			int nTmp;` |
|        9 |  9824 | `			zClass = pThis->pClass->sName.zString;` |
|        9 |  9825 | `			nClass = pThis->pClass->sName.nByte;` |
|        9 |  9826 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|        9 |  9827 | `			if( pGetMessage ){` |
|        9 |  9828 | `				PH7_MemObjInit(pVm,&sMsg);` |
|        9 |  9829 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|        9 |  9830 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|        9 |  9831 | `					if( zTmp && nTmp > 0 ){` |
|        9 |  9832 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|        4 |  9833 | `					}` |
|        4 |  9834 | `				}` |
|        9 |  9835 | `				PH7_MemObjRelease(&sMsg);` |
|        4 |  9836 | `			}` |
|        4 |  9837 | `		}` |
|        9 |  9838 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  9839 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  9840 | `		}` |
|        9 |  9841 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|        9 |  9842 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|        9 |  9843 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|        9 |  9844 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|        9 |  9845 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  9846 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|        9 |  9847 | `		rc = SXERR_ABORT;` |
|        4 |  9848 | `	}` |
|       12 |  9849 | `	PH7_MemObjRelease(&sArg);` |
|       12 |  9850 | `	return rc;` |
|        7 |  9851 |  |
|        - |  9852 | `/*` |
|        - |  9853 | ` * Throw an user exception.` |
|        - |  9854 | ` */` |
|       24 |  9855 | `static sxi32 VmThrowException(` |
|        - |  9856 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  9857 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  9858 | `	)` |
|        2 |  9859 |  |
|        - |  9860 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  9861 | `	ph7_exception **apException;` |
|        - |  9862 | `	ph7_exception *pException;` |
|        - |  9863 | `	/* Point to the stack of loaded exceptions */` |
|       26 |  9864 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       26 |  9865 | `	pException = 0;` |
|       26 |  9866 | `	pCatch = 0;` |
|       26 |  9867 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  9868 | `		ph7_exception_block *aCatch;` |
|        - |  9869 | `		ph7_class *pClass;` |
|        - |  9870 | `		sxu32 j;` |
|        - |  9871 | `		/* Locate the appropriate block to execute */` |
|       16 |  9872 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       16 |  9873 | `		(void)SySetPop(&pVm->aException);` |
|       16 |  9874 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       16 |  9875 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       16 |  9876 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  9877 | `			/* Extract the target class */` |
|       16 |  9878 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       16 |  9879 | `			if( pClass == 0 ){` |
|        - |  9880 | `				/* No such class */` |
|      ! 0 |  9881 | `				continue;` |
|        - |  9882 | `			}` |
|       16 |  9883 | `			if( VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  9884 | `				/* Catch block found,break immeditaley */` |
|       16 |  9885 | `				pCatch = &aCatch[j];` |
|       16 |  9886 | `				break;` |
|        - |  9887 | `			}` |
|      ! 0 |  9888 | `		}` |
|        7 |  9889 | `	}` |
|        - |  9890 | `	/* Execute the cached block if available */` |
|       26 |  9891 | `	if( pCatch == 0 ){` |
|        - |  9892 | `		sxi32 rc;` |
|       12 |  9893 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|       12 |  9894 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  9895 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  9896 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9897 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  9898 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  9899 | `			}` |
|      ! 0 |  9900 | `			if( pException->pFrame == pFrame ){` |
|        - |  9901 | `				/* Tell the upper layer that the exception was caught */` |
|      ! 0 |  9902 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  9903 | `			}` |
|      ! 0 |  9904 | `		}` |
|       12 |  9905 | `		return rc;` |
|      ! 0 |  9906 | `	}else{` |
|       16 |  9907 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  9908 | `		sxi32 rc;` |
|       24 |  9909 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  9910 | `			/* Safely ignore the exception frame */` |
|       10 |  9911 | `			pFrame = pFrame->pParent;` |
|        2 |  9912 | `		}` |
|       16 |  9913 | `		if( pException->pFrame == pFrame ){` |
|        - |  9914 | `			/* Tell the upper layer that the exception was caught */` |
|        8 |  9915 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        3 |  9916 | `		}` |
|        - |  9917 | `		/* Create a private frame first */` |
|       16 |  9918 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       16 |  9919 | `		if( rc == SXRET_OK ){` |
|        - |  9920 | `			/* Mark as catch frame */` |
|       16 |  9921 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       16 |  9922 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       16 |  9923 | `			if( pObj ){` |
|        - |  9924 | `				/* Install the exception instance */` |
|       16 |  9925 | `				pThis->iRef++; /* Increment reference count */` |
|       16 |  9926 | `				pObj->x.pOther = pThis;` |
|       16 |  9927 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        7 |  9928 | `			}` |
|        - |  9929 | `			/* Exceute the block */` |
|       16 |  9930 | `			VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  9931 | `			/* Leave the frame */` |
|       16 |  9932 | `			VmLeaveFrame(&(*pVm));` |
|        7 |  9933 | `		}` |
|        - |  9934 | `	}` |
|        - |  9935 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  9936 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  9937 | `	 */` |
|       16 |  9938 | `	return SXRET_OK;` |
|       14 |  9939 |  |
|        - |  9940 | `/*` |
|        - |  9941 | ` * Section:` |
|        - |  9942 | ` *  Version,Credits and Copyright related functions.` |
|        - |  9943 | ` * Status:` |
|        - |  9944 | ` *    Stable.` |
|        - |  9945 | ` */` |
|        - |  9946 | `/*` |
|        - |  9947 | ` * string ph7version(void)` |
|        - |  9948 | ` *  Returns the running version of the PH7 version.` |
|        - |  9949 | ` * Parameters` |
|        - |  9950 | ` *  None` |
|        - |  9951 | ` * Return` |
|        - |  9952 | ` * Current PH7 version.` |
|        - |  9953 | ` */` |
|        2 |  9954 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9955 |  |
|        1 |  9956 | `	SXUNUSED(nArg);` |
|        1 |  9957 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  9958 | `	/* Current engine version */` |
|        3 |  9959 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  9960 | `	return PH7_OK;` |
|        1 |  9961 |  |
|        - |  9962 | `/*` |
|        - |  9963 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  9964 | ` */` |
|        - |  9965 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  9966 | ` "<html><head>"\` |
|        - |  9967 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  9968 | ` "<style type=\"text/css\">"\` |
|        - |  9969 | ` "div {"\` |
|        - |  9970 | `     "border: 1px solid #cccccc;"\` |
|        - |  9971 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - |  9972 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - |  9973 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - |  9974 | `     "-moz-border-radius-topright: 10px;"\` |
|        - |  9975 | `     "-webkit-border-radius: 10px;"\` |
|        - |  9976 | `     "-o-border-radius: 10px;"\` |
|        - |  9977 | `     "border-radius: 10px;"\` |
|        - |  9978 | `     "padding-left: 2em;"\` |
|        - |  9979 | `     "background-color: white;"\` |
|        - |  9980 | `     "margin-left: auto;"\` |
|        - |  9981 | `     "font-family: verdana;"\` |
|        - |  9982 | `     "padding-right: 2em;"\` |
|        - |  9983 | `     "margin-right: auto;"\` |
|        - |  9984 | `     "}"\` |
|        - |  9985 | `     "body {"\` |
|        - |  9986 | `     "padding: 0.2em;"\` |
|        - |  9987 | `     "font-style: normal;"\` |
|        - |  9988 | `     "font-size: medium;"\` |
|        - |  9989 | `     "background-color: #f2f2f2;"\` |
|        - |  9990 | `     "}"\` |
|        - |  9991 | `     "hr {"\` |
|        - |  9992 | `     "border-style: solid none none;"\` |
|        - |  9993 | `     "border-width: 1px medium medium;"\` |
|        - |  9994 | `     "border-top: 1px solid #cccccc;"\` |
|        - |  9995 | `     "height: 1px;"\` |
|        - |  9996 | `     "}"\` |
|        - |  9997 | `     "a {"\` |
|        - |  9998 | `     "color: #3366cc;"\` |
|        - |  9999 | `     "text-decoration: none;"\` |
|        - | 10000 | `     "}"\` |
|        - | 10001 | `     "a:hover {"\` |
|        - | 10002 | `     "color: #999999;"\` |
|        - | 10003 | `     "}"\` |
|        - | 10004 | `     "a:active {"\` |
|        - | 10005 | `     "color: #663399;"\` |
|        - | 10006 | `     "}"\` |
|        - | 10007 | `     "h1 {"\` |
|        - | 10008 | `     "margin: 0;"\` |
|        - | 10009 | `     "padding: 0;"\` |
|        - | 10010 | `     "font-family: Verdana;"\` |
|        - | 10011 | `     "font-weight: bold;"\` |
|        - | 10012 | `     "font-style: normal;"\` |
|        - | 10013 | `     "font-size: medium;"\` |
|        - | 10014 | `     "text-transform: capitalize;"\` |
|        - | 10015 | `     "color: #0a328c;"\` |
|        - | 10016 | `     "}"\` |
|        - | 10017 | `     "p {"\` |
|        - | 10018 | `     "margin: 0 auto;"\` |
|        - | 10019 | `     "font-size: medium;"\` |
|        - | 10020 | `     "font-style: normal;"\` |
|        - | 10021 | `     "font-family: verdana;"\` |
|        - | 10022 | `     "}"\` |
|        - | 10023 | `"</style></head><body>"\` |
|        - | 10024 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 10025 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 10026 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 10027 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 10028 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 10029 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 10030 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 10031 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 10032 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 10033 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 10034 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 10035 |  |
|        - | 10036 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10037 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 10038 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 10039 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 10040 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10041 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 10042 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10043 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 10044 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10045 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 10046 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10047 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 10048 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 10049 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 10050 |  |
|        - | 10051 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 10052 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 10053 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 10054 | `"&nbsp;*<br>"\` |
|        - | 10055 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 10056 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 10057 | `"&nbsp;* are met:<br>"\` |
|        - | 10058 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 10059 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 10060 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 10061 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 10062 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 10063 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 10064 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 10065 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 10066 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 10067 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 10068 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 10069 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 10070 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 10071 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 10072 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 10073 | `"&nbsp;*<br>"\` |
|        - | 10074 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 10075 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 10076 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 10077 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 10078 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 10079 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 10080 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 10081 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 10082 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 10083 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 10084 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 10085 | `"&nbsp;*/<br>"\` |
|        - | 10086 | `"</span></small></small></p>"\` |
|        - | 10087 | `"</div></body></html>"` |
|        - | 10088 | `/*` |
|        - | 10089 | ` * bool ph7credits(void)` |
|        - | 10090 | ` * bool ph7info(void)` |
|        - | 10091 | ` * bool ph7copyright(void)` |
|        - | 10092 | ` *  Prints out the credits for PH7 engine` |
|        - | 10093 | ` * Parameters` |
|        - | 10094 | ` *  None` |
|        - | 10095 | ` * Return` |
|        - | 10096 | ` *  Always TRUE` |
|        - | 10097 | ` */` |
|        2 | 10098 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10099 |  |
|        3 | 10100 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 10101 | `	/* Expand the HTML page above*/` |
|        3 | 10102 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 10103 | `	ph7_context_output_format(` |
|        1 | 10104 | `		pCtx,` |
|        - | 10105 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 10106 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 10107 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 10108 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 10109 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 10110 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 10111 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 10112 | `#ifdef __WINNT__` |
|        - | 10113 | `		"Windows NT"` |
|        - | 10114 | `#elif defined(__UNIXES__)` |
|        - | 10115 | `		"UNIX-Like"` |
|        - | 10116 | `#else` |
|        - | 10117 | `		"Other OS"` |
|        - | 10118 | `#endif` |
|        - | 10119 | `		);` |
|        3 | 10120 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 10121 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10122 | `	SXUNUSED(apArg);` |
|        - | 10123 | `	/* Return TRUE */` |
|        - | 10124 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 10125 | `	return PH7_OK;` |
|        1 | 10126 |  |
|        - | 10127 | `/*` |
|        - | 10128 | ` * Section:` |
|        - | 10129 | ` *    URL related routines.` |
|        - | 10130 | ` * Status:` |
|        - | 10131 | ` *    Stable.` |
|        - | 10132 | ` */` |
|        - | 10133 | `/* Forward declaration */` |
|        - | 10134 | `static sxi32 VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen);` |
|        - | 10135 | `/*` |
|        - | 10136 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 10137 | ` *  Parse a URL and return its fields.` |
|        - | 10138 | ` * Parameters` |
|        - | 10139 | ` *  $url` |
|        - | 10140 | ` *   The URL to parse.` |
|        - | 10141 | ` * $component` |
|        - | 10142 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 10143 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 10144 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 10145 | ` *  in which case the return value will be an integer).` |
|        - | 10146 | ` * Return` |
|        - | 10147 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 10148 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 10149 | ` *  this array are:` |
|        - | 10150 | ` *   scheme - e.g. http` |
|        - | 10151 | ` *   host` |
|        - | 10152 | ` *   port` |
|        - | 10153 | ` *   user` |
|        - | 10154 | ` *   pass` |
|        - | 10155 | ` *   path` |
|        - | 10156 | ` *   query - after the question mark ?` |
|        - | 10157 | ` *   fragment - after the hashmark #` |
|        - | 10158 | ` * Note:` |
|        - | 10159 | ` *  FALSE is returned on failure.` |
|        - | 10160 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 10161 | ` *  with the standard PHP engine.` |
|        - | 10162 | ` */` |
|       28 | 10163 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10164 |  |
|        - | 10165 | `	const char *zStr; /* Input string */` |
|        - | 10166 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 10167 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 10168 | `	int nLen;` |
|        - | 10169 | `	sxi32 rc;` |
|       29 | 10170 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10171 | `		/* Missing/Invalid arguments,return FALSE */` |
|        3 | 10172 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10173 | `		return PH7_OK;` |
|        - | 10174 | `	}` |
|        - | 10175 | `	/* Extract the given URI */` |
|       27 | 10176 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       27 | 10177 | `	if( nLen < 1 ){` |
|        - | 10178 | `		/* Nothing to process,return FALSE */` |
|      ! 0 | 10179 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10180 | `		return PH7_OK;` |
|        - | 10181 | `	}` |
|        - | 10182 | `	/* Get a parse */` |
|       27 | 10183 | `	rc = VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 10184 | `	if( rc != SXRET_OK ){` |
|        - | 10185 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 10186 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10187 | `		return PH7_OK;` |
|        - | 10188 | `	}` |
|       27 | 10189 | `	if( nArg > 1 ){` |
|      ! 0 | 10190 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 10191 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 10192 | `		switch(nComponent){` |
|      ! 0 | 10193 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 10194 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 10195 | `			if( pComp->nByte < 1 ){` |
|        - | 10196 | `				/* No available value,return NULL */` |
|      ! 0 | 10197 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10198 | `			}else{` |
|      ! 0 | 10199 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10200 | `			}` |
|      ! 0 | 10201 | `			break;` |
|      ! 0 | 10202 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 10203 | `			pComp = &sURI.sHost;` |
|      ! 0 | 10204 | `			if( pComp->nByte < 1 ){` |
|        - | 10205 | `				/* No available value,return NULL */` |
|      ! 0 | 10206 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10207 | `			}else{` |
|      ! 0 | 10208 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10209 | `			}` |
|      ! 0 | 10210 | `			break;` |
|      ! 0 | 10211 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 10212 | `			pComp = &sURI.sPort;` |
|      ! 0 | 10213 | `			if( pComp->nByte < 1 ){` |
|        - | 10214 | `				/* No available value,return NULL */` |
|      ! 0 | 10215 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10216 | `			}else{` |
|      ! 0 | 10217 | `				int iPort = 0;` |
|        - | 10218 | `				/* Cast the value to integer */` |
|      ! 0 | 10219 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 10220 | `				ph7_result_int(pCtx,iPort);` |
|        - | 10221 | `			}` |
|      ! 0 | 10222 | `			break;` |
|      ! 0 | 10223 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 10224 | `			pComp = &sURI.sUser;` |
|      ! 0 | 10225 | `			if( pComp->nByte < 1 ){` |
|        - | 10226 | `				/* No available value,return NULL */` |
|      ! 0 | 10227 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10228 | `			}else{` |
|      ! 0 | 10229 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10230 | `			}` |
|      ! 0 | 10231 | `			break;` |
|      ! 0 | 10232 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 10233 | `			pComp = &sURI.sPass;` |
|      ! 0 | 10234 | `			if( pComp->nByte < 1 ){` |
|        - | 10235 | `				/* No available value,return NULL */` |
|      ! 0 | 10236 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10237 | `			}else{` |
|      ! 0 | 10238 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10239 | `			}` |
|      ! 0 | 10240 | `			break;` |
|      ! 0 | 10241 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 10242 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 10243 | `			if( pComp->nByte < 1 ){` |
|        - | 10244 | `				/* No available value,return NULL */` |
|      ! 0 | 10245 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10246 | `			}else{` |
|      ! 0 | 10247 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10248 | `			}` |
|      ! 0 | 10249 | `			break;` |
|      ! 0 | 10250 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 10251 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 10252 | `			if( pComp->nByte < 1 ){` |
|        - | 10253 | `				/* No available value,return NULL */` |
|      ! 0 | 10254 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10255 | `			}else{` |
|      ! 0 | 10256 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10257 | `			}` |
|      ! 0 | 10258 | `			break;` |
|      ! 0 | 10259 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 10260 | `			pComp = &sURI.sPath;` |
|      ! 0 | 10261 | `			if( pComp->nByte < 1 ){` |
|        - | 10262 | `				/* No available value,return NULL */` |
|      ! 0 | 10263 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10264 | `			}else{` |
|      ! 0 | 10265 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10266 | `			}` |
|      ! 0 | 10267 | `			break;` |
|      ! 0 | 10268 | `		default:` |
|        - | 10269 | `			/* No such entry,return NULL */` |
|      ! 0 | 10270 | `			ph7_result_null(pCtx);` |
|      ! 0 | 10271 | `			break;` |
|        - | 10272 | `		}` |
|      ! 0 | 10273 | `	}else{` |
|        - | 10274 | `		ph7_value *pArray,*pValue;` |
|        - | 10275 | `		/* Return an associative array */` |
|       27 | 10276 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 10277 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 10278 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10279 | `			/* Out of memory */` |
|      ! 0 | 10280 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10281 | `			/* Return false */` |
|      ! 0 | 10282 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 10283 | `			return PH7_OK;` |
|        - | 10284 | `		}` |
|        - | 10285 | `		/* Fill the array */` |
|       27 | 10286 | `		pComp = &sURI.sScheme;` |
|       27 | 10287 | `		if( pComp->nByte > 0 ){` |
|       19 | 10288 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 10289 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 10290 | `		}` |
|        - | 10291 | `		/* Reset the string cursor */` |
|       27 | 10292 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10293 | `		pComp = &sURI.sHost;` |
|       27 | 10294 | `		if( pComp->nByte > 0 ){` |
|       25 | 10295 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 10296 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 10297 | `		}` |
|        - | 10298 | `		/* Reset the string cursor */` |
|       27 | 10299 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10300 | `		pComp = &sURI.sPort;` |
|       27 | 10301 | `		if( pComp->nByte > 0 ){` |
|       11 | 10302 | `			int iPort = 0;/* cc warning */` |
|        - | 10303 | `			/* Convert to integer */` |
|       11 | 10304 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 10305 | `			ph7_value_int(pValue,iPort);` |
|       11 | 10306 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 10307 | `		}` |
|        - | 10308 | `		/* Reset the string cursor */` |
|       27 | 10309 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10310 | `		pComp = &sURI.sUser;` |
|       27 | 10311 | `		if( pComp->nByte > 0 ){` |
|        7 | 10312 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10313 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 10314 | `		}` |
|        - | 10315 | `		/* Reset the string cursor */` |
|       27 | 10316 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10317 | `		pComp = &sURI.sPass;` |
|       27 | 10318 | `		if( pComp->nByte > 0 ){` |
|        7 | 10319 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10320 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 10321 | `		}` |
|        - | 10322 | `		/* Reset the string cursor */` |
|       27 | 10323 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10324 | `		pComp = &sURI.sPath;` |
|       27 | 10325 | `		if( pComp->nByte > 0 ){` |
|       17 | 10326 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 10327 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 10328 | `		}` |
|        - | 10329 | `		/* Reset the string cursor */` |
|       27 | 10330 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10331 | `		pComp = &sURI.sQuery;` |
|       27 | 10332 | `		if( pComp->nByte > 0 ){` |
|        5 | 10333 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10334 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 10335 | `		}` |
|        - | 10336 | `		/* Reset the string cursor */` |
|       27 | 10337 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10338 | `		pComp = &sURI.sFragment;` |
|       27 | 10339 | `		if( pComp->nByte > 0 ){` |
|        5 | 10340 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10341 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 10342 | `		}` |
|        - | 10343 | `		/* Return the created array */` |
|       27 | 10344 | `		ph7_result_value(pCtx,pArray);` |
|        - | 10345 | `		/* NOTE:` |
|        - | 10346 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 10347 | `		 * automatically as soon we return from this function.` |
|        - | 10348 | `		 */` |
|        - | 10349 | `	}` |
|        - | 10350 | `	/* All done */` |
|       27 | 10351 | `	return PH7_OK;` |
|       15 | 10352 |  |
|        - | 10353 | `/*` |
|        - | 10354 | ` * Section:` |
|        - | 10355 | ` *   Array related routines.` |
|        - | 10356 | ` * Status:` |
|        - | 10357 | ` *    Stable.` |
|        - | 10358 | ` * Note 2012-5-21 01:04:15:` |
|        - | 10359 | ` *  Array related functions that need access to the underlying` |
|        - | 10360 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 10361 | ` */` |
|        - | 10362 | `/*` |
|        - | 10363 | ` * The [compact()] function store it's state information in an instance` |
|        - | 10364 | ` * of the following structure.` |
|        - | 10365 | ` */` |
|        - | 10366 | `struct compact_data` |
|        - | 10367 |  |
|        - | 10368 | `	ph7_value *pArray;  /* Target array */` |
|        - | 10369 | `	int nRecCount;      /* Recursion count */` |
|        - | 10370 | `};` |
|        - | 10371 | `/*` |
|        - | 10372 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 10373 | ` */` |
|      ! 0 | 10374 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 10375 |  |
|      ! 0 | 10376 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 10377 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 10378 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 10379 | `	/* Act according to the hashmap value */` |
|      ! 0 | 10380 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 10381 | `		SyString sVar;` |
|      ! 0 | 10382 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 10383 | `		if( sVar.nByte > 0 ){` |
|        - | 10384 | `			/* Query the current frame */` |
|      ! 0 | 10385 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 10386 | `			/* ^` |
|        - | 10387 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 10388 | `			 */` |
|      ! 0 | 10389 | `			if( pKey ){` |
|        - | 10390 | `				/* Perform the insertion */` |
|      ! 0 | 10391 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 10392 | `			}` |
|      ! 0 | 10393 | `		}` |
|      ! 0 | 10394 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 10395 | `		int rc;` |
|        - | 10396 | `		/* Recursively traverse this array */` |
|      ! 0 | 10397 | `		pData->nRecCount++;` |
|      ! 0 | 10398 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 10399 | `		pData->nRecCount--;` |
|      ! 0 | 10400 | `		return rc;` |
|        - | 10401 | `	}` |
|      ! 0 | 10402 | `	return SXRET_OK;` |
|      ! 0 | 10403 |  |
|        - | 10404 | `/*` |
|        - | 10405 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 10406 | ` *  Create array containing variables and their values.` |
|        - | 10407 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 10408 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 10409 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 10410 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 10411 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 10412 | ` * Parameters` |
|        - | 10413 | ` *  $varname` |
|        - | 10414 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 10415 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 10416 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 10417 | ` *   it recursively.` |
|        - | 10418 | ` * Return` |
|        - | 10419 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 10420 | ` */` |
|        2 | 10421 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10422 |  |
|        - | 10423 | `	ph7_value *pArray,*pObj;` |
|        3 | 10424 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10425 | `	const char *zName;` |
|        - | 10426 | `	SyString sVar;` |
|        - | 10427 | `	int i,nLen;` |
|        3 | 10428 | `	if( nArg < 1 ){` |
|        - | 10429 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 10430 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10431 | `		return PH7_OK;` |
|        - | 10432 | `	}` |
|        - | 10433 | `	/* Create the array */` |
|        3 | 10434 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10435 | `	if( pArray == 0 ){` |
|        - | 10436 | `		/* Out of memory */` |
|      ! 0 | 10437 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10438 | `		/* Return NULL */` |
|      ! 0 | 10439 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10440 | `		return PH7_OK;` |
|        - | 10441 | `	}` |
|        - | 10442 | `	/* Perform the requested operation */` |
|        7 | 10443 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 10444 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 10445 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 10446 | `				struct compact_data sData;` |
|      ! 0 | 10447 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 10448 | `				/* Recursively walk the array */` |
|      ! 0 | 10449 | `				sData.nRecCount = 0;` |
|      ! 0 | 10450 | `				sData.pArray = pArray;` |
|      ! 0 | 10451 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 10452 | `			}` |
|      ! 0 | 10453 | `		}else{` |
|        - | 10454 | `			/* Extract variable name */` |
|        5 | 10455 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 10456 | `			if( nLen > 0 ){` |
|        5 | 10457 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 10458 | `				/* Check if the variable is available in the current frame */` |
|        5 | 10459 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 10460 | `				if( pObj ){` |
|        5 | 10461 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 10462 | `				}` |
|        2 | 10463 | `			}` |
|        - | 10464 | `		}` |
|        3 | 10465 | `	}` |
|        - | 10466 | `	/* Return the array */` |
|        3 | 10467 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10468 | `	return PH7_OK;` |
|        2 | 10469 |  |
|        - | 10470 | `/*` |
|        - | 10471 | ` * The [extract()] function store it's state information in an instance` |
|        - | 10472 | ` * of the following structure.` |
|        - | 10473 | ` */` |
|        - | 10474 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 10475 | `struct extract_aux_data` |
|        - | 10476 |  |
|        - | 10477 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 10478 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 10479 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 10480 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 10481 | `	int iFlags;           /* Control flags */` |
|        - | 10482 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 10483 | `};` |
|        - | 10484 | `/* Forward declaration */` |
|        - | 10485 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 10486 | `/*` |
|        - | 10487 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 10488 | ` *   Import variables into the current symbol table from an array.` |
|        - | 10489 | ` * Parameters` |
|        - | 10490 | ` * $var_array` |
|        - | 10491 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 10492 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 10493 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 10494 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 10495 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 10496 | ` * $extract_type` |
|        - | 10497 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 10498 | ` *  It can be one of the following values:` |
|        - | 10499 | ` *   EXTR_OVERWRITE` |
|        - | 10500 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 10501 | ` *   EXTR_SKIP` |
|        - | 10502 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 10503 | ` *   EXTR_PREFIX_SAME` |
|        - | 10504 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 10505 | ` *   EXTR_PREFIX_ALL` |
|        - | 10506 | ` *       Prefix all variable names with prefix.` |
|        - | 10507 | ` *   EXTR_PREFIX_INVALID` |
|        - | 10508 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 10509 | ` *   EXTR_IF_EXISTS` |
|        - | 10510 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 10511 | ` *       otherwise do nothing.` |
|        - | 10512 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 10513 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 10514 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 10515 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 10516 | ` *      the current symbol table.` |
|        - | 10517 | ` * $prefix` |
|        - | 10518 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 10519 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 10520 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 10521 | ` *  underscore character.` |
|        - | 10522 | ` * Return` |
|        - | 10523 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 10524 | ` */` |
|        4 | 10525 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10526 |  |
|        - | 10527 | `	extract_aux_data sAux;` |
|        - | 10528 | `	ph7_hashmap *pMap;` |
|        5 | 10529 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 10530 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 10531 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 10532 | `		return PH7_OK;` |
|        - | 10533 | `	}` |
|        - | 10534 | `	/* Point to the target hashmap */` |
|        5 | 10535 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 10536 | `	if( pMap->nEntry < 1 ){` |
|        - | 10537 | `		/* Empty map,return  0 */` |
|      ! 0 | 10538 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 10539 | `		return PH7_OK;` |
|        - | 10540 | `	}` |
|        - | 10541 | `	/* Prepare the aux data */` |
|        5 | 10542 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 10543 | `	if( nArg > 1 ){` |
|        3 | 10544 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 10545 | `		if( nArg > 2 ){` |
|      ! 0 | 10546 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 10547 | `		}` |
|        1 | 10548 | `	}` |
|        5 | 10549 | `	sAux.pVm = pCtx->pVm;` |
|        - | 10550 | `	/* Invoke the worker callback */` |
|        5 | 10551 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 10552 | `	/* Number of variables successfully imported */` |
|        5 | 10553 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 10554 | `	return PH7_OK;` |
|        3 | 10555 |  |
|        - | 10556 | `/*` |
|        - | 10557 | ` * Worker callback for the [extract()] function defined` |
|        - | 10558 | ` * below.` |
|        - | 10559 | ` */` |
|        8 | 10560 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 10561 |  |
|        9 | 10562 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 10563 | `	int iFlags = pAux->iFlags;` |
|        9 | 10564 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 10565 | `	ph7_value *pObj;` |
|        - | 10566 | `	SyString sVar;` |
|        9 | 10567 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 10568 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 10569 | `	}` |
|        - | 10570 | `	/* Perform a string cast */` |
|        9 | 10571 | `	PH7_MemObjToString(pKey);` |
|        9 | 10572 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 10573 | `		/* Unavailable variable name */` |
|      ! 0 | 10574 | `		return SXRET_OK;` |
|        - | 10575 | `	}` |
|        9 | 10576 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 10577 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 10578 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 10579 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 10580 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10581 | `			);` |
|      ! 0 | 10582 | `	}else{` |
|       13 | 10583 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 10584 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 10585 | `	}` |
|        9 | 10586 | `	sVar.zString = pAux->zWorker;` |
|        - | 10587 | `	/* Try to extract the variable */` |
|        9 | 10588 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 10589 | `	if( pObj ){` |
|        - | 10590 | `		/* Collision */` |
|        3 | 10591 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 10592 | `			return SXRET_OK;` |
|        - | 10593 | `		}` |
|        3 | 10594 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 10595 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 10596 | `				/* Already prefixed */` |
|      ! 0 | 10597 | `				return SXRET_OK;` |
|        - | 10598 | `			}` |
|      ! 0 | 10599 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 10600 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 10601 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10602 | `				);` |
|      ! 0 | 10603 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 10604 | `		}` |
|        2 | 10605 | `	}else{` |
|        - | 10606 | `		/* Create the variable */` |
|        7 | 10607 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 10608 | `	}` |
|        9 | 10609 | `	if( pObj ){` |
|        - | 10610 | `		/* Overwrite the old value */` |
|        9 | 10611 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 10612 | `		/* Increment counter */` |
|        9 | 10613 | `		pAux->iCount++;` |
|        4 | 10614 | `	}` |
|        9 | 10615 | `	return SXRET_OK;` |
|        5 | 10616 |  |
|        - | 10617 | `/*` |
|        - | 10618 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 10619 | ` * defined below.` |
|        - | 10620 | ` */` |
|        2 | 10621 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 10622 |  |
|        3 | 10623 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 10624 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 10625 | `	ph7_value *pObj;` |
|        - | 10626 | `	SyString sVar;` |
|        - | 10627 | `	/* Perform a string cast */` |
|        3 | 10628 | `	PH7_MemObjToString(pKey);` |
|        3 | 10629 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 10630 | `		/* Unavailable variable name */` |
|      ! 0 | 10631 | `		return SXRET_OK;` |
|        - | 10632 | `	}` |
|        3 | 10633 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 10634 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 10635 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 10636 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 10637 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10638 | `			);` |
|        2 | 10639 | `	}else{` |
|      ! 0 | 10640 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 10641 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 10642 | `	}` |
|        3 | 10643 | `	sVar.zString = pAux->zWorker;` |
|        - | 10644 | `	/* Extract the variable */` |
|        3 | 10645 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 10646 | `	if( pObj ){` |
|        3 | 10647 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 10648 | `	}` |
|        3 | 10649 | `	return SXRET_OK;` |
|        2 | 10650 |  |
|        - | 10651 | `/*` |
|        - | 10652 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 10653 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 10654 | ` * Parameters` |
|        - | 10655 | ` * $types` |
|        - | 10656 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 10657 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 10658 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 10659 | ` *  POST includes the POST uploaded file information.` |
|        - | 10660 | ` *  Note:` |
|        - | 10661 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 10662 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 10663 | ` * $prefix` |
|        - | 10664 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 10665 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 10666 | ` *  variable named $pref_userid.` |
|        - | 10667 | ` * Return` |
|        - | 10668 | ` *  TRUE on success or FALSE on failure.` |
|        - | 10669 | ` */` |
|        2 | 10670 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10671 |  |
|        - | 10672 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 10673 | `	extract_aux_data sAux;` |
|        - | 10674 | `	int nLen,nPrefixLen;` |
|        - | 10675 | `	ph7_value *pSuper;` |
|        - | 10676 | `	ph7_vm *pVm;` |
|        - | 10677 | `	/* By default import only $_GET variables  */` |
|        3 | 10678 | `	zImport = "G";` |
|        3 | 10679 | `	nLen = (int)sizeof(char);` |
|        3 | 10680 | `	zPrefix = 0;` |
|        3 | 10681 | `	nPrefixLen = 0;` |
|        3 | 10682 | `	if( nArg > 0 ){` |
|        3 | 10683 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 10684 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 10685 | `		}` |
|        3 | 10686 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 10687 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 10688 | `		}` |
|        1 | 10689 | `	}` |
|        - | 10690 | `	/* Point to the underlying VM */` |
|        3 | 10691 | `	pVm = pCtx->pVm;` |
|        - | 10692 | `	/* Initialize the aux data */` |
|        3 | 10693 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 10694 | `	sAux.zPrefix = zPrefix;` |
|        3 | 10695 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 10696 | `	sAux.pVm = pVm;` |
|        - | 10697 | `	/* Extract */` |
|        3 | 10698 | `	zEnd = &zImport[nLen];` |
|        5 | 10699 | `	while( zImport < zEnd ){` |
|        3 | 10700 | `		int c = zImport[0];` |
|        3 | 10701 | `		pSuper = 0;` |
|        3 | 10702 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 10703 | `			/* Import $_GET variables */` |
|        3 | 10704 | `			pSuper = VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 10705 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 10706 | `			/* Import $_POST variables */` |
|      ! 0 | 10707 | `			pSuper = VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 10708 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 10709 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 10710 | `			pSuper = VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 10711 | `		}` |
|        3 | 10712 | `		if( pSuper ){` |
|        - | 10713 | `			/* Iterate throw array entries */` |
|        3 | 10714 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 10715 | `		}` |
|        - | 10716 | `		/* Advance the cursor */` |
|        3 | 10717 | `		zImport++;` |
|        1 | 10718 | `	}` |
|        - | 10719 | `	/* All done,return TRUE*/` |
|        3 | 10720 | `	ph7_result_bool(pCtx,0);` |
|        3 | 10721 | `	return PH7_OK;` |
|        1 | 10722 |  |
|        - | 10723 | `/*` |
|        - | 10724 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 10725 | ` * Refer to the eval() language construct implementation for more` |
|        - | 10726 | ` * information.` |
|        - | 10727 | ` */` |
|     7710 | 10728 | `static sxi32 VmEvalChunk(` |
|        - | 10729 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 10730 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 10731 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 10732 | `	int iFlags,         /* Compile flag */` |
|        - | 10733 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 10734 | `	)` |
|        2 | 10735 |  |
|        - | 10736 | `	SySet *pByteCode,aByteCode;` |
|     7712 | 10737 | `	ProcConsumer xErr = 0;` |
|     7712 | 10738 | `	void *pErrData = 0;` |
|        - | 10739 | `	/* Initialize bytecode container */` |
|     7712 | 10740 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     7712 | 10741 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 10742 | `	/* Reset the code generator */` |
|     7712 | 10743 | `	if( bTrueReturn ){` |
|        - | 10744 | `		/* Included file,log compile-time errors */` |
|     6489 | 10745 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     6489 | 10746 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3244 | 10747 | `	}` |
|     7712 | 10748 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 10749 | `	/* Swap bytecode container */` |
|     7712 | 10750 | `	pByteCode = pVm->pByteContainer;` |
|     7712 | 10751 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 10752 | `	/* Compile the chunk */` |
|     7712 | 10753 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    11567 | 10754 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 10755 | `		/* Compilation error,return false */` |
|        3 | 10756 | `		if( pCtx ){` |
|        3 | 10757 | `			ph7_result_bool(pCtx,0);` |
|        1 | 10758 | `		}` |
|        2 | 10759 | `	}else{` |
|        - | 10760 | `		/* Mount any newly defined classes */` |
|        - | 10761 | `		SyHashEntry *pEntry;` |
|        - | 10762 | `		ph7_class *pClass;` |
|        - | 10763 | `		ph7_value sResult; /* Return value */` |
|        - | 10764 | `		sxi32 rc;` |
|     7710 | 10765 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   214482 | 10766 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   202920 | 10767 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 10768 | `			/* Only mount classes that haven't been mounted yet */` |
|   202920 | 10769 | `			if( !pClass->bMounted ){` |
|    40912 | 10770 | `				rc = VmMountUserClass(pVm,pClass);` |
|    40912 | 10771 | `				if( rc != SXRET_OK ){` |
|        - | 10772 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 10773 | `					if( pCtx ){` |
|      ! 0 | 10774 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 10775 | `					}` |
|      ! 0 | 10776 | `					goto Cleanup;` |
|        - | 10777 | `				}` |
|    20455 | 10778 | `			}` |
|        2 | 10779 | `		}` |
|     7710 | 10780 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 10781 | `			/* Out of memory */` |
|      ! 0 | 10782 | `			if( pCtx ){` |
|      ! 0 | 10783 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 10784 | `			}` |
|      ! 0 | 10785 | `			goto Cleanup;` |
|        - | 10786 | `		}` |
|     7710 | 10787 | `		if( bTrueReturn ){` |
|        - | 10788 | `			/* Assume a boolean true return value */` |
|     6489 | 10789 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3245 | 10790 | `		}else{` |
|        - | 10791 | `			/* Assume a null return value */` |
|     1222 | 10792 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 10793 | `		}` |
|        - | 10794 | `		/* Execute the compiled chunk */` |
|     7710 | 10795 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|     7710 | 10796 | `		if( pCtx ){` |
|        - | 10797 | `			/* Set the execution result */` |
|     6506 | 10798 | `			ph7_result_value(pCtx,&sResult);` |
|     3252 | 10799 | `		}` |
|     7710 | 10800 | `		PH7_MemObjRelease(&sResult);` |
|        - | 10801 | `	}` |
|     3855 | 10802 | `Cleanup:` |
|        - | 10803 | `	/* Cleanup the mess left behind */` |
|     7712 | 10804 | `	pVm->pByteContainer = pByteCode;` |
|     7712 | 10805 | `	SySetRelease(&aByteCode);` |
|     7712 | 10806 | `	return SXRET_OK;` |
|        2 | 10807 |  |
|        - | 10808 | `/*` |
|        - | 10809 | ` * value eval(string $code)` |
|        - | 10810 | ` *   Evaluate a string as PHP code.` |
|        - | 10811 | ` * Parameter` |
|        - | 10812 | ` *  code: PHP code to evaluate.` |
|        - | 10813 | ` * Return` |
|        - | 10814 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 10815 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 10816 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 10817 | ` */` |
|       16 | 10818 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10819 |  |
|        - | 10820 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 | 10821 | `	if( nArg < 1 ){` |
|        - | 10822 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10823 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10824 | `		return SXRET_OK;` |
|        - | 10825 | `	}` |
|        - | 10826 | `	/* Chunk to evaluate */` |
|       18 | 10827 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 | 10828 | `	if( sChunk.nByte < 1 ){` |
|        - | 10829 | `		/* Empty string,return NULL */` |
|        3 | 10830 | `		ph7_result_null(pCtx);` |
|        3 | 10831 | `		return SXRET_OK;` |
|        - | 10832 | `	}` |
|        - | 10833 | `	/* Eval the chunk */` |
|       16 | 10834 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 | 10835 | `	return SXRET_OK;` |
|       10 | 10836 |  |
|        - | 10837 | `/*` |
|        - | 10838 | ` * Check if a file path is already included.` |
|        - | 10839 | ` */` |
|    12972 | 10840 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 | 10841 |  |
|        - | 10842 | `	SyString *aEntries;` |
|        - | 10843 | `	sxu32 n;` |
|    12973 | 10844 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 10845 | `	/* Perform a linear search */` |
| 42056941 | 10846 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 42043975 | 10847 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 10848 | `			/* Already included */` |
|        7 | 10849 | `			return TRUE;` |
|        - | 10850 | `		}` |
| 21021985 | 10851 | `	}` |
|    12967 | 10852 | `	return FALSE;` |
|     6487 | 10853 |  |
|        - | 10854 | `/*` |
|        - | 10855 | ` * Push a file path in the appropriate VM container.` |
|        - | 10856 | ` */` |
|    14168 | 10857 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 10858 |  |
|        - | 10859 | `	SyString sPath;` |
|        - | 10860 | `	char *zDup;` |
|        - | 10861 | `#ifdef __WINNT__` |
|        - | 10862 | `	char *zCur;` |
|        - | 10863 | `#endif` |
|        - | 10864 | `	sxi32 rc;` |
|    14170 | 10865 | `	if( nLen < 0 ){` |
|     1198 | 10866 | `		nLen = SyStrlen(zPath);` |
|      598 | 10867 | `	}` |
|        - | 10868 | `	/* Duplicate the file path first */` |
|    14170 | 10869 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    14170 | 10870 | `	if( zDup == 0 ){` |
|      ! 0 | 10871 | `		return SXERR_MEM;` |
|        - | 10872 | `	}` |
|        - | 10873 | `#ifdef __WINNT__` |
|        - | 10874 | `	/* Normalize path on windows` |
|        - | 10875 | `	 * Example:` |
|        - | 10876 | `	 *    Path/To/File.php` |
|        - | 10877 | `	 * becomes` |
|        - | 10878 | `	 *   path\to\file.php` |
|        - | 10879 | `	 */` |
|        2 | 10880 | `	zCur = zDup;` |
|        2 | 10881 | `	while( zCur[0] != 0 ){` |
|        2 | 10882 | `		if( zCur[0] == '/' ){` |
|        2 | 10883 | `			zCur[0] = '\\';` |
|        2 | 10884 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 10885 | `			int c = SyToLower(zCur[0]);` |
|        1 | 10886 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 10887 | `		}` |
|        2 | 10888 | `		zCur++;` |
|        2 | 10889 | `	}` |
|        - | 10890 | `#endif` |
|        - | 10891 | `	/* Install the file path */` |
|    14170 | 10892 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    14170 | 10893 | `	if( !bMain ){` |
|    12973 | 10894 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 10895 | `			/* Already included */` |
|        7 | 10896 | `			*pNew = 0;` |
|        4 | 10897 | `		}else{` |
|        - | 10898 | `			/* Insert in the corresponding container */` |
|    12967 | 10899 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    12967 | 10900 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10901 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 10902 | `				return rc;` |
|        - | 10903 | `			}` |
|    12967 | 10904 | `			*pNew = 1;` |
|        - | 10905 | `		}` |
|     6486 | 10906 | `	}` |
|    14170 | 10907 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    14170 | 10908 | `	return SXRET_OK;` |
|     7086 | 10909 |  |
|        - | 10910 | `/*` |
|        - | 10911 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 10912 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 10913 | ` * indicates failure.` |
|        - | 10914 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 10915 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 10916 | ` * operations.` |
|        - | 10917 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 10918 | ` * this function is a no-op.` |
|        - | 10919 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 10920 | ` * constructs for more information.` |
|        - | 10921 | ` */` |
|     6494 | 10922 | `static sxi32 VmExecIncludedFile(` |
|        - | 10923 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 10924 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 10925 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 10926 | `	 )` |
|        2 | 10927 |  |
|        - | 10928 | `	sxi32 rc;` |
|        - | 10929 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10930 | `	const ph7_io_stream *pStream;` |
|        - | 10931 | `	SyBlob sContents;` |
|        - | 10932 | `	void *pHandle;` |
|        - | 10933 | `	ph7_vm *pVm;` |
|        - | 10934 | `	int isNew;` |
|        - | 10935 | `	/* Initialize fields */` |
|     6496 | 10936 | `	pVm = pCtx->pVm;` |
|     6496 | 10937 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     6496 | 10938 | `	isNew = 0;` |
|        - | 10939 | `	/* Extract the associated stream */` |
|     6496 | 10940 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 10941 | `	/*` |
|        - | 10942 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 10943 | `	 * in a read-only mode.` |
|        - | 10944 | `	 */` |
|     6496 | 10945 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     6496 | 10946 | `	if( pHandle == 0 ){` |
|        3 | 10947 | `		return SXERR_IO;` |
|        - | 10948 | `	}` |
|     6493 | 10949 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     6493 | 10950 | `	if( IncludeOnce && !isNew ){` |
|        - | 10951 | `		/* Already included */` |
|        5 | 10952 | `		rc = SXERR_EXISTS;` |
|        3 | 10953 | `	}else{` |
|        - | 10954 | `		/* Read the whole file contents */` |
|     6489 | 10955 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     6489 | 10956 | `		if( rc == SXRET_OK ){` |
|        - | 10957 | `			SyString sScript;` |
|        - | 10958 | `			/* Compile and execute the script */` |
|     6489 | 10959 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     6489 | 10960 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3244 | 10961 | `		}` |
|        - | 10962 | `	}` |
|        - | 10963 | `	/* Pop from the set of included file */` |
|     6493 | 10964 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 10965 | `	/* Close the handle */` |
|     6493 | 10966 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 10967 | `	/* Release the working buffer */` |
|     6493 | 10968 | `	SyBlobRelease(&sContents);` |
|        - | 10969 | `#else` |
|        - | 10970 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 10971 | `	SXUNUSED(pPath);` |
|        - | 10972 | `	SXUNUSED(IncludeOnce);` |
|        - | 10973 | `	rc = SXERR_IO;` |
|        - | 10974 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     6493 | 10975 | `	return rc;` |
|     3249 | 10976 |  |
|        - | 10977 | `/*` |
|        - | 10978 | ` * string get_include_path(void)` |
|        - | 10979 | ` *  Gets the current include_path configuration option.` |
|        - | 10980 | ` * Parameter` |
|        - | 10981 | ` *  None` |
|        - | 10982 | ` * Return` |
|        - | 10983 | ` *  Included paths as a string` |
|        - | 10984 | ` */` |
|        2 | 10985 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10986 |  |
|        3 | 10987 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10988 | `	SyString *aEntry;` |
|        - | 10989 | `	int dir_sep;` |
|        - | 10990 | `	sxu32 n;` |
|        - | 10991 | `#ifdef __WINNT__` |
|        1 | 10992 | `	dir_sep = ';';` |
|        - | 10993 | `#else` |
|        - | 10994 | `	/* Assume UNIX path separator */` |
|        2 | 10995 | `	dir_sep = ':';` |
|        - | 10996 | `#endif` |
|        1 | 10997 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10998 | `	SXUNUSED(apArg);` |
|        - | 10999 | `	/* Point to the list of import paths */` |
|        3 | 11000 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 11001 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 11002 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 11003 | `		if( n > 0 ){` |
|        - | 11004 | `			/* Append dir seprator */` |
|      ! 0 | 11005 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 11006 | `		}` |
|        - | 11007 | `		/* Append path */` |
|        3 | 11008 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 11009 | `	}` |
|        3 | 11010 | `	return PH7_OK;` |
|        1 | 11011 |  |
|        - | 11012 | `/*` |
|        - | 11013 | ` * string get_get_included_files(void)` |
|        - | 11014 | ` *  Gets the current include_path configuration option.` |
|        - | 11015 | ` * Parameter` |
|        - | 11016 | ` *  None` |
|        - | 11017 | ` * Return` |
|        - | 11018 | ` *  Included paths as a string` |
|        - | 11019 | ` */` |
|        2 | 11020 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11021 |  |
|        3 | 11022 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 11023 | `	ph7_value *pArray,*pWorker;` |
|        - | 11024 | `	SyString *pEntry;` |
|        - | 11025 | `	int c,d;` |
|        - | 11026 | `	/* Create an array and a working value */` |
|        3 | 11027 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 11028 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11029 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 11030 | `		/* Out of memory,return null */` |
|      ! 0 | 11031 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11032 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11033 | `		SXUNUSED(apArg);` |
|      ! 0 | 11034 | `		return PH7_OK;` |
|        - | 11035 | `	}` |
|        3 | 11036 | `	c = d = '/';` |
|        - | 11037 | `#ifdef __WINNT__` |
|        1 | 11038 | `	d = '\\';` |
|        - | 11039 | `#endif` |
|        - | 11040 | `	/* Iterate throw entries */` |
|        3 | 11041 | `	SySetResetCursor(pFiles);` |
|     2691 | 11042 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 11043 | `		const char *zBase,*zEnd;` |
|        - | 11044 | `		int iLen;` |
|        - | 11045 | `		/* reset the string cursor */` |
|     2689 | 11046 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 11047 | `		/* Extract base name */` |
|     2689 | 11048 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 11049 | `		/* Ignore trailing '/' */` |
|     4033 | 11050 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 11051 | `			zEnd--;` |
|      ! 0 | 11052 | `		}` |
|     2689 | 11053 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|    75459 | 11054 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    71427 | 11055 | `			zEnd--;` |
|        1 | 11056 | `		}` |
|     2689 | 11057 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     2689 | 11058 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 11059 | `		/* Copy entry name */` |
|     2689 | 11060 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 11061 | `		/* Perform the insertion */` |
|     2689 | 11062 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 11063 | `	}` |
|        - | 11064 | `	/* All done,return the created array */` |
|        3 | 11065 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11066 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 11067 | `	 * by the engine as soon we return from this foreign` |
|        - | 11068 | `	 * function.` |
|        - | 11069 | `	 */` |
|        3 | 11070 | `	return PH7_OK;` |
|        2 | 11071 |  |
|        - | 11072 | `/*` |
|        - | 11073 | ` * include:` |
|        - | 11074 | ` * According to the PHP reference manual.` |
|        - | 11075 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 11076 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 11077 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 11078 | ` *  include() will finally check in the calling script's own directory` |
|        - | 11079 | ` *  and the current working directory before failing. The include()` |
|        - | 11080 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 11081 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 11082 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 11083 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 11084 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 11085 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 11086 | ` *  directory to find the requested file.` |
|        - | 11087 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 11088 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 11089 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 11090 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 11091 | ` */` |
|     6482 | 11092 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11093 |  |
|        - | 11094 | `	SyString sFile;` |
|        - | 11095 | `	sxi32 rc;` |
|     6484 | 11096 | `	if( nArg < 1 ){` |
|        - | 11097 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11098 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11099 | `		return SXRET_OK;` |
|        - | 11100 | `	}` |
|        - | 11101 | `	/* File to include */` |
|     6484 | 11102 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     6484 | 11103 | `	if( sFile.nByte < 1 ){` |
|        - | 11104 | `		/* Empty string,return NULL */` |
|      ! 0 | 11105 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11106 | `		return SXRET_OK;` |
|        - | 11107 | `	}` |
|        - | 11108 | `	/* Open,compile and execute the desired script */` |
|     6484 | 11109 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     6484 | 11110 | `	if( rc != SXRET_OK ){` |
|        - | 11111 | `		/* Emit a warning and return false */` |
|        3 | 11112 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 11113 | `		ph7_result_bool(pCtx,0);` |
|        1 | 11114 | `	}` |
|     6484 | 11115 | `	return SXRET_OK;` |
|     3243 | 11116 |  |
|        - | 11117 | `/*` |
|        - | 11118 | ` * include_once:` |
|        - | 11119 | ` *  According to the PHP reference manual.` |
|        - | 11120 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 11121 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 11122 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 11123 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 11124 | ` *   just once.` |
|        - | 11125 | ` */` |
|        4 | 11126 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11127 |  |
|        - | 11128 | `	SyString sFile;` |
|        - | 11129 | `	sxi32 rc;` |
|        5 | 11130 | `	if( nArg < 1 ){` |
|        - | 11131 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11132 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11133 | `		return SXRET_OK;` |
|        - | 11134 | `	}` |
|        - | 11135 | `	/* File to include */` |
|        5 | 11136 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11137 | `	if( sFile.nByte < 1 ){` |
|        - | 11138 | `		/* Empty string,return NULL */` |
|      ! 0 | 11139 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11140 | `		return SXRET_OK;` |
|        - | 11141 | `	}` |
|        - | 11142 | `	/* Open,compile and execute the desired script */` |
|        5 | 11143 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11144 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11145 | `		/* File already included,return TRUE */` |
|        3 | 11146 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11147 | `		return SXRET_OK;` |
|        - | 11148 | `	}` |
|        3 | 11149 | `	if( rc != SXRET_OK ){` |
|        - | 11150 | `		/* Emit a warning and return false */` |
|      ! 0 | 11151 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11152 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11153 | ` 	}` |
|        3 | 11154 | `	return SXRET_OK;` |
|        3 | 11155 |  |
|        - | 11156 | `/*` |
|        - | 11157 | ` * require.` |
|        - | 11158 | ` *  According to the PHP reference manual.` |
|        - | 11159 | ` *   require() is identical to include() except upon failure it will` |
|        - | 11160 | ` *   also produce a fatal level error.` |
|        - | 11161 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 11162 | ` *   emits a warning  which allows the script to continue.` |
|        - | 11163 | ` */` |
|        4 | 11164 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11165 |  |
|        - | 11166 | `	SyString sFile;` |
|        - | 11167 | `	sxi32 rc;` |
|        5 | 11168 | `	if( nArg < 1 ){` |
|        - | 11169 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11170 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11171 | `		return SXRET_OK;` |
|        - | 11172 | `	}` |
|        - | 11173 | `	/* File to include */` |
|        5 | 11174 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11175 | `	if( sFile.nByte < 1 ){` |
|        - | 11176 | `		/* Empty string,return NULL */` |
|      ! 0 | 11177 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11178 | `		return SXRET_OK;` |
|        - | 11179 | `	}` |
|        - | 11180 | `	/* Open,compile and execute the desired script */` |
|        5 | 11181 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 11182 | `	if( rc != SXRET_OK ){` |
|        - | 11183 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11184 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11185 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11186 | `		return PH7_ABORT;` |
|        - | 11187 | `	}` |
|        5 | 11188 | `	return SXRET_OK;` |
|        3 | 11189 |  |
|        - | 11190 | `/*` |
|        - | 11191 | ` * require_once:` |
|        - | 11192 | ` *  According to the PHP reference manual.` |
|        - | 11193 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 11194 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 11195 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 11196 | ` *   and how it differs from its non _once siblings.` |
|        - | 11197 | ` */` |
|        4 | 11198 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11199 |  |
|        - | 11200 | `	SyString sFile;` |
|        - | 11201 | `	sxi32 rc;` |
|        5 | 11202 | `	if( nArg < 1 ){` |
|        - | 11203 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11204 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11205 | `		return SXRET_OK;` |
|        - | 11206 | `	}` |
|        - | 11207 | `	/* File to include */` |
|        5 | 11208 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11209 | `	if( sFile.nByte < 1 ){` |
|        - | 11210 | `		/* Empty string,return NULL */` |
|      ! 0 | 11211 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11212 | `		return SXRET_OK;` |
|        - | 11213 | `	}` |
|        - | 11214 | `	/* Open,compile and execute the desired script */` |
|        5 | 11215 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11216 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11217 | `		/* File already included,return TRUE */` |
|        3 | 11218 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11219 | `		return SXRET_OK;` |
|        - | 11220 | `	}` |
|        3 | 11221 | `	if( rc != SXRET_OK ){` |
|        - | 11222 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11223 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11224 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11225 | `		return PH7_ABORT;` |
|        - | 11226 | `	}` |
|        3 | 11227 | `	return SXRET_OK;` |
|        3 | 11228 |  |
|        - | 11229 | `/*` |
|        - | 11230 | ` * Section:` |
|        - | 11231 | ` *  Command line arguments processing.` |
|        - | 11232 | ` * Status:` |
|        - | 11233 | ` *    Stable.` |
|        - | 11234 | ` */` |
|        - | 11235 | `/*` |
|        - | 11236 | ` * Check if a short option argument [i.e: -c] is available in the command` |
|        - | 11237 | ` * line string. Return a pointer to the start of the stream on success.` |
|        - | 11238 | ` * NULL otherwise.` |
|        - | 11239 | ` */` |
|        6 | 11240 | `static const char * VmFindShortOpt(int c,const char *zIn,const char *zEnd)` |
|        1 | 11241 |  |
|      199 | 11242 | `	while( zIn < zEnd ){` |
|      193 | 11243 | `		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == c ){` |
|        - | 11244 | `			/* Got one */` |
|      ! 0 | 11245 | `			return &zIn[1];` |
|        - | 11246 | `		}` |
|        - | 11247 | `		/* Advance the cursor */` |
|      193 | 11248 | `		zIn++;` |
|        1 | 11249 | `	}` |
|        - | 11250 | `	/* No such option */` |
|        7 | 11251 | `	return 0;` |
|        4 | 11252 |  |
|        - | 11253 | `/*` |
|        - | 11254 | ` * Check if a long option argument [i.e: --opt] is available in the command` |
|        - | 11255 | ` * line string. Return a pointer to the start of the stream on success.` |
|        - | 11256 | ` * NULL otherwise.` |
|        - | 11257 | ` */` |
|      ! 0 | 11258 | `static const char * VmFindLongOpt(const char *zLong,int nByte,const char *zIn,const char *zEnd)` |
|      ! 0 | 11259 |  |
|        - | 11260 | `	const char *zOpt;` |
|      ! 0 | 11261 | `	while( zIn < zEnd ){` |
|      ! 0 | 11262 | `		if( zIn[0] == '-' && &zIn[1] < zEnd && (int)zIn[1] == '-' ){` |
|      ! 0 | 11263 | `			zIn += 2;` |
|      ! 0 | 11264 | `			zOpt = zIn;` |
|      ! 0 | 11265 | `			while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 11266 | `				if( zIn[0] == '=' /* --opt=val */){` |
|      ! 0 | 11267 | `					break;` |
|        - | 11268 | `				}` |
|      ! 0 | 11269 | `				zIn++;` |
|      ! 0 | 11270 | `			}` |
|        - | 11271 | `			/* Test */` |
|      ! 0 | 11272 | `			if( (int)(zIn-zOpt) == nByte && SyMemcmp(zOpt,zLong,nByte) == 0 ){` |
|        - | 11273 | `				/* Got one,return it's value */` |
|      ! 0 | 11274 | `				return zIn;` |
|        - | 11275 | `			}` |
|        - | 11276 |  |
|      ! 0 | 11277 | `		}else{` |
|      ! 0 | 11278 | `			zIn++;` |
|        - | 11279 | `		}` |
|      ! 0 | 11280 | `	}` |
|        - | 11281 | `	/* No such option */` |
|      ! 0 | 11282 | `	return 0;` |
|      ! 0 | 11283 |  |
|        - | 11284 | `/*` |
|        - | 11285 | ` * Long option [i.e: --opt] arguments private data structure.` |
|        - | 11286 | ` */` |
|        - | 11287 | `struct getopt_long_opt` |
|        - | 11288 |  |
|        - | 11289 | `	const char *zArgIn,*zArgEnd; /* Command line arguments */` |
|        - | 11290 | `	ph7_value *pWorker;  /* Worker variable*/` |
|        - | 11291 | `	ph7_value *pArray;   /* getopt() return value */` |
|        - | 11292 | `	ph7_context *pCtx;   /* Call Context */` |
|        - | 11293 | `};` |
|        - | 11294 | `/* Forward declaration */` |
|        - | 11295 | `static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11296 | `/*` |
|        - | 11297 | ` * Extract short or long argument option values.` |
|        - | 11298 | ` */` |
|      ! 0 | 11299 | `static void VmExtractOptArgValue(` |
|        - | 11300 | `	ph7_value *pArray,  /* getopt() return value */` |
|        - | 11301 | `	ph7_value *pWorker, /* Worker variable */` |
|        - | 11302 | `	const char *zArg,   /* Argument stream */` |
|        - | 11303 | `	const char *zArgEnd,/* End of the argument stream  */` |
|        - | 11304 | `	int need_val,       /* TRUE to fetch option argument */` |
|        - | 11305 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 11306 | `	const char *zName   /* Option name */)` |
|      ! 0 | 11307 |  |
|      ! 0 | 11308 | `	ph7_value_bool(pWorker,0);` |
|      ! 0 | 11309 | `	if( !need_val ){` |
|        - | 11310 | `		/*` |
|        - | 11311 | `		 * Option does not need arguments.` |
|        - | 11312 | `		 * Insert the option name and a boolean FALSE.` |
|        - | 11313 | `		 */` |
|      ! 0 | 11314 | `		ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11315 | `	}else{` |
|        - | 11316 | `		const char *zCur;` |
|        - | 11317 | `		/* Extract option argument */` |
|      ! 0 | 11318 | `		zArg++;` |
|      ! 0 | 11319 | `		if( zArg < zArgEnd && zArg[0] == '=' ){` |
|      ! 0 | 11320 | `			zArg++;` |
|      ! 0 | 11321 | `		}` |
|      ! 0 | 11322 | `		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11323 | `			zArg++;` |
|      ! 0 | 11324 | `		}` |
|      ! 0 | 11325 | `		if( zArg >= zArgEnd \|\| zArg[0] == '-' ){` |
|        - | 11326 | `			/*` |
|        - | 11327 | `			 * Argument not found.` |
|        - | 11328 | `			 * Insert the option name and a boolean FALSE.` |
|        - | 11329 | `			 */` |
|      ! 0 | 11330 | `			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11331 | `			return;` |
|        - | 11332 | `		}` |
|        - | 11333 | `		/* Delimit the value */` |
|      ! 0 | 11334 | `		zCur = zArg;` |
|      ! 0 | 11335 | `		if( zArg[0] == '\'' \|\| zArg[0] == '"' ){` |
|      ! 0 | 11336 | `			int d = zArg[0];` |
|        - | 11337 | `			/* Delimt the argument */` |
|      ! 0 | 11338 | `			zArg++;` |
|      ! 0 | 11339 | `			zCur = zArg;` |
|      ! 0 | 11340 | `			while( zArg < zArgEnd ){` |
|      ! 0 | 11341 | `				if( zArg[0] == d && zArg[-1] != '\\' ){` |
|        - | 11342 | `					/* Delimiter found,exit the loop  */` |
|      ! 0 | 11343 | `					break;` |
|        - | 11344 | `				}` |
|      ! 0 | 11345 | `				zArg++;` |
|      ! 0 | 11346 | `			}` |
|        - | 11347 | `			/* Save the value */` |
|      ! 0 | 11348 | `			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|      ! 0 | 11349 | `			if( zArg < zArgEnd ){ zArg++; }` |
|      ! 0 | 11350 | `		}else{` |
|      ! 0 | 11351 | `			while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){` |
|      ! 0 | 11352 | `				zArg++;` |
|      ! 0 | 11353 | `			}` |
|        - | 11354 | `			/* Save the value */` |
|      ! 0 | 11355 | `			ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|        - | 11356 | `		}` |
|        - | 11357 | `		/*` |
|        - | 11358 | `		 * Check if we are dealing with multiple values.` |
|        - | 11359 | `		 * If so,create an array to hold them,rather than a scalar variable.` |
|        - | 11360 | `		 */` |
|      ! 0 | 11361 | `		while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11362 | `			zArg++;` |
|      ! 0 | 11363 | `		}` |
|      ! 0 | 11364 | `		if( zArg < zArgEnd && zArg[0] != '-' ){` |
|        - | 11365 | `			ph7_value *pOptArg; /* Array of option arguments */` |
|      ! 0 | 11366 | `			pOptArg = ph7_context_new_array(pCtx);` |
|      ! 0 | 11367 | `			if( pOptArg == 0 ){` |
|      ! 0 | 11368 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11369 | `			}else{` |
|        - | 11370 | `				/* Insert the first value */` |
|      ! 0 | 11371 | `				ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */` |
|      ! 0 | 11372 | `				for(;;){` |
|      ! 0 | 11373 | `					if( zArg >= zArgEnd \|\| zArg[0] == '-' ){` |
|        - | 11374 | `						/* No more value */` |
|      ! 0 | 11375 | `						break;` |
|        - | 11376 | `					}` |
|        - | 11377 | `					/* Delimit the value */` |
|      ! 0 | 11378 | `					zCur = zArg;` |
|      ! 0 | 11379 | `					if( zArg < zArgEnd && zArg[0] == '\\' ){` |
|      ! 0 | 11380 | `						zArg++;` |
|      ! 0 | 11381 | `						zCur = zArg;` |
|      ! 0 | 11382 | `					}` |
|      ! 0 | 11383 | `					while( zArg < zArgEnd && !SyisSpace(zArg[0]) ){` |
|      ! 0 | 11384 | `						zArg++;` |
|      ! 0 | 11385 | `					}` |
|        - | 11386 | `					/* Reset the string cursor */` |
|      ! 0 | 11387 | `					ph7_value_reset_string_cursor(pWorker);` |
|        - | 11388 | `					/* Save the value */` |
|      ! 0 | 11389 | `					ph7_value_string(pWorker,zCur,(int)(zArg-zCur));` |
|        - | 11390 | `					/* Insert */` |
|      ! 0 | 11391 | `					ph7_array_add_elem(pOptArg,0,pWorker); /* Will make it's own copy */` |
|        - | 11392 | `					/* Jump trailing white spaces */` |
|      ! 0 | 11393 | `					while( zArg < zArgEnd && (unsigned char)zArg[0] < 0xc0 && SyisSpace(zArg[0]) ){` |
|      ! 0 | 11394 | `						zArg++;` |
|      ! 0 | 11395 | `					}` |
|      ! 0 | 11396 | `				}` |
|        - | 11397 | `				/* Insert the option arg array */` |
|      ! 0 | 11398 | `				ph7_array_add_strkey_elem(pArray,(const char *)zName,pOptArg); /* Will make it's own copy */` |
|        - | 11399 | `				/* Safely release */` |
|      ! 0 | 11400 | `				ph7_context_release_value(pCtx,pOptArg);` |
|        - | 11401 | `			}` |
|      ! 0 | 11402 | `		}else{` |
|        - | 11403 | `			/* Single value */` |
|      ! 0 | 11404 | `			ph7_array_add_strkey_elem(pArray,(const char *)zName,pWorker); /* Will make it's own copy */` |
|        - | 11405 | `		}` |
|        - | 11406 | `	}` |
|      ! 0 | 11407 |  |
|        - | 11408 | `/*` |
|        - | 11409 | ` * array getopt(string $options[,array $longopts ])` |
|        - | 11410 | ` *   Gets options from the command line argument list.` |
|        - | 11411 | ` * Parameters` |
|        - | 11412 | ` *  $options` |
|        - | 11413 | ` *   Each character in this string will be used as option characters` |
|        - | 11414 | ` *   and matched against options passed to the script starting with` |
|        - | 11415 | ` *   a single hyphen (-). For example, an option string "x" recognizes` |
|        - | 11416 | ` *   an option -x. Only a-z, A-Z and 0-9 are allowed.` |
|        - | 11417 | ` *  $longopts` |
|        - | 11418 | ` *   An array of options. Each element in this array will be used as option` |
|        - | 11419 | ` *   strings and matched against options passed to the script starting with` |
|        - | 11420 | ` *   two hyphens (--). For example, an longopts element "opt" recognizes an` |
|        - | 11421 | ` *   option --opt.` |
|        - | 11422 | ` * Return` |
|        - | 11423 | ` *  This function will return an array of option / argument pairs or FALSE` |
|        - | 11424 | ` *  on failure.` |
|        - | 11425 | ` */` |
|        2 | 11426 | `static int vm_builtin_getopt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11427 |  |
|        - | 11428 | `	const char *zIn,*zEnd,*zArg,*zArgIn,*zArgEnd;` |
|        - | 11429 | `	struct getopt_long_opt sLong;` |
|        - | 11430 | `	ph7_value *pArray,*pWorker;` |
|        - | 11431 | `	SyBlob *pArg;` |
|        - | 11432 | `	int nByte;` |
|        3 | 11433 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11434 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 11435 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Missing/Invalid option arguments");` |
|      ! 0 | 11436 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11437 | `		return PH7_OK;` |
|        - | 11438 | `	}` |
|        - | 11439 | `	/* Extract option arguments */` |
|        3 | 11440 | `	zIn  = ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 11441 | `	zEnd = &zIn[nByte];` |
|        - | 11442 | `	/* Point to the string representation of the $argv[] array */` |
|        3 | 11443 | `	pArg = &pCtx->pVm->sArgv;` |
|        - | 11444 | `	/* Create a new empty array and a worker variable */` |
|        3 | 11445 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11446 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11447 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|      ! 0 | 11448 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11449 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11450 | `		return PH7_OK;` |
|        - | 11451 | `	}` |
|        3 | 11452 | `	if( SyBlobLength(pArg) < 1 ){` |
|        - | 11453 | `		/* Empty command line,return the empty array*/` |
|      ! 0 | 11454 | `		ph7_result_value(pCtx,pArray);` |
|        - | 11455 | `		/* Everything will be released automatically when we return` |
|        - | 11456 | `		 * from this function.` |
|        - | 11457 | `		 */` |
|      ! 0 | 11458 | `		return PH7_OK;` |
|        - | 11459 | `	}` |
|        3 | 11460 | `	zArgIn = (const char *)SyBlobData(pArg);` |
|        3 | 11461 | `	zArgEnd = &zArgIn[SyBlobLength(pArg)];` |
|        - | 11462 | `	/* Fill the long option structure */` |
|        3 | 11463 | `	sLong.pArray = pArray;` |
|        3 | 11464 | `	sLong.pWorker = pWorker;` |
|        3 | 11465 | `	sLong.zArgIn =  zArgIn;` |
|        3 | 11466 | `	sLong.zArgEnd = zArgEnd;` |
|        3 | 11467 | `	sLong.pCtx = pCtx;` |
|        - | 11468 | `	/* Start processing */` |
|        9 | 11469 | `	while( zIn < zEnd ){` |
|        7 | 11470 | `		int c = zIn[0];` |
|        7 | 11471 | `		int need_val = 0;` |
|        - | 11472 | `		/* Advance the stream cursor */` |
|        7 | 11473 | `		zIn++;` |
|        - | 11474 | `		/* Ignore non-alphanum characters */` |
|        7 | 11475 | `		if( !SyisAlphaNum(c) ){` |
|      ! 0 | 11476 | `			continue;` |
|        - | 11477 | `		}` |
|        7 | 11478 | `		if( zIn < zEnd && zIn[0] == ':' ){` |
|        5 | 11479 | `			zIn++;` |
|        5 | 11480 | `			need_val = 1;` |
|        5 | 11481 | `			if( zIn < zEnd && zIn[0] == ':' ){` |
|      ! 0 | 11482 | `				zIn++;` |
|      ! 0 | 11483 | `			}` |
|        2 | 11484 | `		}` |
|        - | 11485 | `		/* Find option */` |
|        7 | 11486 | `		zArg = VmFindShortOpt(c,zArgIn,zArgEnd);` |
|        7 | 11487 | `		if( zArg == 0 ){` |
|        - | 11488 | `			/* No such option */` |
|        7 | 11489 | `			continue;` |
|        - | 11490 | `		}` |
|        - | 11491 | `		/* Extract option argument value */` |
|      ! 0 | 11492 | `		VmExtractOptArgValue(pArray,pWorker,zArg,zArgEnd,need_val,pCtx,(const char *)&c);` |
|      ! 0 | 11493 | `	}` |
|        3 | 11494 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) && ph7_array_count(apArg[1]) > 0 ){` |
|        - | 11495 | `		/* Process long options */` |
|      ! 0 | 11496 | `		ph7_array_walk(apArg[1],VmProcessLongOpt,&sLong);` |
|      ! 0 | 11497 | `	}` |
|        - | 11498 | `	/* Return the option array */` |
|        3 | 11499 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11500 | `	/*` |
|        - | 11501 | `	 * Don't worry about freeing memory, everything will be released` |
|        - | 11502 | `	 * automatically as soon we return from this foreign function.` |
|        - | 11503 | `	 */` |
|        3 | 11504 | `	return PH7_OK;` |
|        2 | 11505 |  |
|        - | 11506 | `/*` |
|        - | 11507 | ` * Array walker callback used for processing long options values.` |
|        - | 11508 | ` */` |
|      ! 0 | 11509 | `static int VmProcessLongOpt(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11510 |  |
|      ! 0 | 11511 | `	struct getopt_long_opt *pOpt = (struct getopt_long_opt *)pUserData;` |
|        - | 11512 | `	const char *zArg,*zOpt,*zEnd;` |
|      ! 0 | 11513 | `	int need_value = 0;` |
|        - | 11514 | `	int nByte;` |
|        - | 11515 | `	/* Value must be of type string */` |
|      ! 0 | 11516 | `	if( !ph7_value_is_string(pValue) ){` |
|        - | 11517 | `		/* Simply ignore */` |
|      ! 0 | 11518 | `		return PH7_OK;` |
|        - | 11519 | `	}` |
|      ! 0 | 11520 | `	zOpt = ph7_value_to_string(pValue,&nByte);` |
|      ! 0 | 11521 | `	if( nByte < 1 ){` |
|        - | 11522 | `		/* Empty string,ignore */` |
|      ! 0 | 11523 | `		return PH7_OK;` |
|        - | 11524 | `	}` |
|      ! 0 | 11525 | `	zEnd = &zOpt[nByte - 1];` |
|      ! 0 | 11526 | `	if( zEnd[0] == ':' ){` |
|        - | 11527 | `		char *zTerm;` |
|        - | 11528 | `		/* Try to extract a value */` |
|      ! 0 | 11529 | `		need_value = 1;` |
|      ! 0 | 11530 | `		while( zEnd >= zOpt && zEnd[0] == ':' ){` |
|      ! 0 | 11531 | `			zEnd--;` |
|      ! 0 | 11532 | `		}` |
|      ! 0 | 11533 | `		if( zOpt >= zEnd ){` |
|        - | 11534 | `			/* Empty string,ignore */` |
|      ! 0 | 11535 | `			SXUNUSED(pKey);` |
|      ! 0 | 11536 | `			return PH7_OK;` |
|        - | 11537 | `		}` |
|      ! 0 | 11538 | `		zEnd++;` |
|      ! 0 | 11539 | `		zTerm = (char *)zEnd;` |
|      ! 0 | 11540 | `		zTerm[0] = 0;` |
|      ! 0 | 11541 | `	}else{` |
|      ! 0 | 11542 | `		zEnd = &zOpt[nByte];` |
|        - | 11543 | `	}` |
|        - | 11544 | `	/* Find the option */` |
|      ! 0 | 11545 | `	zArg = VmFindLongOpt(zOpt,(int)(zEnd-zOpt),pOpt->zArgIn,pOpt->zArgEnd);` |
|      ! 0 | 11546 | `	if( zArg == 0 ){` |
|        - | 11547 | `		/* No such option,return immediately */` |
|      ! 0 | 11548 | `		return PH7_OK;` |
|        - | 11549 | `	}` |
|        - | 11550 | `	/* Try to extract a value */` |
|      ! 0 | 11551 | `	VmExtractOptArgValue(pOpt->pArray,pOpt->pWorker,zArg,pOpt->zArgEnd,need_value,pOpt->pCtx,zOpt);` |
|      ! 0 | 11552 | `	return PH7_OK;` |
|      ! 0 | 11553 |  |
|        - | 11554 | `/*` |
|        - | 11555 | ` * Section:` |
|        - | 11556 | ` *  JSON encoding/decoding routines.` |
|        - | 11557 | ` * Status:` |
|        - | 11558 | ` *    Devel.` |
|        - | 11559 | ` */` |
|        - | 11560 | `/* Forward reference */` |
|        - | 11561 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11562 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData);` |
|        - | 11563 | `/*` |
|        - | 11564 | ` * JSON encoder state is stored in an instance` |
|        - | 11565 | ` * of the following structure.` |
|        - | 11566 | ` */` |
|        - | 11567 | `typedef struct json_private_data json_private_data;` |
|        - | 11568 | `struct json_private_data` |
|        - | 11569 |  |
|        - | 11570 | `	ph7_context *pCtx; /* Call context */` |
|        - | 11571 | `	int isFirst;       /* True if first encoded entry */` |
|        - | 11572 | `	int iFlags;        /* JSON encoding flags */` |
|        - | 11573 | `	int nRecCount;     /* Recursion count */` |
|        - | 11574 | `};` |
|        - | 11575 | `/*` |
|        - | 11576 | ` * Returns the JSON representation of a value.In other word perform a JSON encoding operation.` |
|        - | 11577 | ` * According to wikipedia` |
|        - | 11578 | ` * JSON's basic types are:` |
|        - | 11579 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|        - | 11580 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|        - | 11581 | ` *   Boolean (true or false)` |
|        - | 11582 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|        - | 11583 | ` *    do not need to be of the same type)` |
|        - | 11584 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|        - | 11585 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|        - | 11586 | ` *     be distinct from each other)` |
|        - | 11587 | ` *   null (empty)` |
|        - | 11588 | ` * Non-significant white space may be added freely around the "structural characters"` |
|        - | 11589 | ` * (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|        - | 11590 | ` */` |
|        8 | 11591 | `static sxi32 VmJsonEncode(` |
|        - | 11592 | `	ph7_value *pIn,          /* Encode this value */` |
|        - | 11593 | `	json_private_data *pData /* Context data */` |
|        1 | 11594 | `	){` |
|        9 | 11595 | `		ph7_context *pCtx = pData->pCtx;` |
|        9 | 11596 | `		int iFlags = pData->iFlags;` |
|        - | 11597 | `		int nByte;` |
|        9 | 11598 | `		if( ph7_value_is_null(pIn) \|\| ph7_value_is_resource(pIn)){` |
|        - | 11599 | `			/* null */` |
|      ! 0 | 11600 | `			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|        9 | 11601 | `		}else if( ph7_value_is_bool(pIn) ){` |
|      ! 0 | 11602 | `			int iBool = ph7_value_to_bool(pIn);` |
|        - | 11603 | `			int iLen;` |
|        - | 11604 | `			/* true/false */` |
|      ! 0 | 11605 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|      ! 0 | 11606 | `			ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1);` |
|       12 | 11607 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|        - | 11608 | `			const char *zNum;` |
|        - | 11609 | `			/* Get a string representation of the number */` |
|        7 | 11610 | `			zNum = ph7_value_to_string(pIn,&nByte);` |
|        7 | 11611 | `			ph7_result_string(pCtx,zNum,nByte);` |
|        6 | 11612 | `		}else if( ph7_value_is_string(pIn) ){` |
|      ! 0 | 11613 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|        - | 11614 | `				const char *zNum;` |
|        - | 11615 | `				/* Encodes numeric strings as numbers. */` |
|      ! 0 | 11616 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|        - | 11617 | `				/* Get a string representation of the number */` |
|      ! 0 | 11618 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|      ! 0 | 11619 | `				ph7_result_string(pCtx,zNum,nByte);` |
|      ! 0 | 11620 | `			}else{` |
|        - | 11621 | `				const char *zIn,*zEnd;` |
|        - | 11622 | `				int c;` |
|        - | 11623 | `				/* Encode the string */` |
|      ! 0 | 11624 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|      ! 0 | 11625 | `				zEnd = &zIn[nByte];` |
|        - | 11626 | `				/* Append the double quote */` |
|      ! 0 | 11627 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      ! 0 | 11628 | `				for(;;){` |
|      ! 0 | 11629 | `					if( zIn >= zEnd ){` |
|        - | 11630 | `						/* No more input to process */` |
|      ! 0 | 11631 | `						break;` |
|        - | 11632 | `					}` |
|      ! 0 | 11633 | `					c = zIn[0];` |
|        - | 11634 | `					/* Advance the stream cursor */` |
|      ! 0 | 11635 | `					zIn++;` |
|      ! 0 | 11636 | `					if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|        - | 11637 | `						/* All < and > are converted to \u003C and \u003E */` |
|      ! 0 | 11638 | `						if( c == '<' ){` |
|      ! 0 | 11639 | `							ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1);` |
|      ! 0 | 11640 | `						}else{` |
|      ! 0 | 11641 | `							ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1);` |
|        - | 11642 | `						}` |
|      ! 0 | 11643 | `						continue;` |
|      ! 0 | 11644 | `					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|        - | 11645 | `						/* All &s are converted to \u0026.  */` |
|      ! 0 | 11646 | `						ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1);` |
|      ! 0 | 11647 | `						continue;` |
|      ! 0 | 11648 | `					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|        - | 11649 | `						/* All ' are converted to \u0027.   */` |
|      ! 0 | 11650 | `						ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1);` |
|      ! 0 | 11651 | `						continue;` |
|      ! 0 | 11652 | `					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|        - | 11653 | `						/* All " are converted to \u0022. */` |
|      ! 0 | 11654 | `						ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1);` |
|      ! 0 | 11655 | `						continue;` |
|        - | 11656 | `					}` |
|      ! 0 | 11657 | `					if( c == '"' \|\| (c == '\\' && ((iFlags & JSON_UNESCAPED_SLASHES)==0)) ){` |
|        - | 11658 | `						/* Unescape the character */` |
|      ! 0 | 11659 | `						ph7_result_string(pCtx,"\\",(int)sizeof(char));` |
|      ! 0 | 11660 | `					}` |
|        - | 11661 | `					/* Append character verbatim */` |
|      ! 0 | 11662 | `					ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      ! 0 | 11663 | `				}` |
|        - | 11664 | `				/* Append the double quote */` |
|      ! 0 | 11665 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      ! 0 | 11666 | `			}` |
|        3 | 11667 | `		}else if( ph7_value_is_array(pIn) ){` |
|        3 | 11668 | `			int c = '[',d = ']';` |
|        - | 11669 | `			/* Encode the array */` |
|        3 | 11670 | `			pData->isFirst = 1;` |
|        3 | 11671 | `			if( iFlags & JSON_FORCE_OBJECT ){` |
|        - | 11672 | `				/* Outputs an object rather than an array */` |
|      ! 0 | 11673 | `				c = '{';` |
|      ! 0 | 11674 | `				d = '}';` |
|      ! 0 | 11675 | `			}` |
|        - | 11676 | `			/* Append the square bracket or curly braces */` |
|        3 | 11677 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|        - | 11678 | `			/* Iterate throw array entries */` |
|        3 | 11679 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|        - | 11680 | `			/* Append the closing square bracket or curly braces */` |
|        3 | 11681 | `			ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char));` |
|        1 | 11682 | `		}else if( ph7_value_is_object(pIn) ){` |
|        - | 11683 | `			/* Encode the class instance */` |
|      ! 0 | 11684 | `			pData->isFirst = 1;` |
|        - | 11685 | `			/* Append the curly braces */` |
|      ! 0 | 11686 | `			ph7_result_string(pCtx,"{",(int)sizeof(char));` |
|        - | 11687 | `			/* Iterate throw class attribute */` |
|      ! 0 | 11688 | `			ph7_object_walk(pIn,VmJsonObjectEncode,pData);` |
|        - | 11689 | `			/* Append the closing curly braces  */` |
|      ! 0 | 11690 | `			ph7_result_string(pCtx,"}",(int)sizeof(char));` |
|      ! 0 | 11691 | `		}else{` |
|        - | 11692 | `			/* Can't happen */` |
|      ! 0 | 11693 | `			ph7_result_string(pCtx,"null",(int)sizeof("null")-1);` |
|        - | 11694 | `		}` |
|        - | 11695 | `		/* All done */` |
|        9 | 11696 | `		return PH7_OK;` |
|        1 | 11697 |  |
|        - | 11698 | `/*` |
|        - | 11699 | ` * The following walker callback is invoked each time we need` |
|        - | 11700 | ` * to encode an array to JSON.` |
|        - | 11701 | ` */` |
|        6 | 11702 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11703 |  |
|        7 | 11704 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|        7 | 11705 | `	if( pJson->nRecCount > 31 ){` |
|        - | 11706 | `		/* Recursion limit reached,return immediately */` |
|      ! 0 | 11707 | `		return PH7_OK;` |
|        - | 11708 | `	}` |
|        7 | 11709 | `	if( !pJson->isFirst ){` |
|        - | 11710 | `		/* Append the colon first */` |
|        5 | 11711 | `		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));` |
|        2 | 11712 | `	}` |
|        7 | 11713 | `	if( pJson->iFlags & JSON_FORCE_OBJECT ){` |
|        - | 11714 | `		/* Outputs an object rather than an array */` |
|        - | 11715 | `		const char *zKey;` |
|        - | 11716 | `		int nByte;` |
|        - | 11717 | `		/* Extract a string representation of the key */` |
|      ! 0 | 11718 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|        - | 11719 | `		/* Append the key and the double colon */` |
|      ! 0 | 11720 | `		ph7_result_string_format(pJson->pCtx,"\"%.*s\":",nByte,zKey);` |
|      ! 0 | 11721 | `	}` |
|        - | 11722 | `	/* Encode the value */` |
|        7 | 11723 | `	pJson->nRecCount++;` |
|        7 | 11724 | `	VmJsonEncode(pValue,pJson);` |
|        7 | 11725 | `	pJson->nRecCount--;` |
|        7 | 11726 | `	pJson->isFirst = 0;` |
|        7 | 11727 | `	return PH7_OK;` |
|        4 | 11728 |  |
|        - | 11729 | `/*` |
|        - | 11730 | ` * The following walker callback is invoked each time we need to encode` |
|        - | 11731 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|        - | 11732 | ` */` |
|      ! 0 | 11733 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11734 |  |
|      ! 0 | 11735 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|      ! 0 | 11736 | `	if( pJson->nRecCount > 31 ){` |
|        - | 11737 | `		/* Recursion limit reached,return immediately */` |
|      ! 0 | 11738 | `		return PH7_OK;` |
|        - | 11739 | `	}` |
|      ! 0 | 11740 | `	if( !pJson->isFirst ){` |
|        - | 11741 | `		/* Append the colon first */` |
|      ! 0 | 11742 | `		ph7_result_string(pJson->pCtx,",",(int)sizeof(char));` |
|      ! 0 | 11743 | `	}` |
|        - | 11744 | `	/* Append the attribute name and the double colon first */` |
|      ! 0 | 11745 | `	ph7_result_string_format(pJson->pCtx,"\"%s\":",zAttr);` |
|        - | 11746 | `	/* Encode the value */` |
|      ! 0 | 11747 | `	pJson->nRecCount++;` |
|      ! 0 | 11748 | `	VmJsonEncode(pValue,pJson);` |
|      ! 0 | 11749 | `	pJson->nRecCount--;` |
|      ! 0 | 11750 | `	pJson->isFirst = 0;` |
|      ! 0 | 11751 | `	return PH7_OK;` |
|      ! 0 | 11752 |  |
|        - | 11753 | `/*` |
|        - | 11754 | ` * string json_encode(mixed $value [, int $options = 0 ])` |
|        - | 11755 | ` *  Returns a string containing the JSON representation of value.` |
|        - | 11756 | ` * Parameters` |
|        - | 11757 | ` *  $value` |
|        - | 11758 | ` *  The value being encoded. Can be any type except a resource.` |
|        - | 11759 | ` * $options` |
|        - | 11760 | ` *  Bitmask consisting of:` |
|        - | 11761 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|        - | 11762 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|        - | 11763 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|        - | 11764 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|        - | 11765 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|        - | 11766 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|        - | 11767 | ` *  JSON_BIGINT_AS_STRING   Not used` |
|        - | 11768 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|        - | 11769 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|        - | 11770 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|        - | 11771 | ` * Return` |
|        - | 11772 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|        - | 11773 | ` */` |
|        2 | 11774 | `static int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11775 |  |
|        - | 11776 | `	json_private_data sJson;` |
|        3 | 11777 | `	if( nArg < 1 ){` |
|        - | 11778 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 11779 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11780 | `		return PH7_OK;` |
|        - | 11781 | `	}` |
|        - | 11782 | `	/* Prepare the JSON data */` |
|        3 | 11783 | `	sJson.nRecCount = 0;` |
|        3 | 11784 | `	sJson.pCtx = pCtx;` |
|        3 | 11785 | `	sJson.isFirst = 1;` |
|        3 | 11786 | `	sJson.iFlags = 0;` |
|        3 | 11787 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|        - | 11788 | `		/* Extract option flags */` |
|      ! 0 | 11789 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 11790 | `	}` |
|        - | 11791 | `	/* Perform the encoding operation */` |
|        3 | 11792 | `	VmJsonEncode(apArg[0],&sJson);` |
|        - | 11793 | `	/* All done */` |
|        3 | 11794 | `	return PH7_OK;` |
|        2 | 11795 |  |
|        - | 11796 | `/*` |
|        - | 11797 | ` * int json_last_error(void)` |
|        - | 11798 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|        - | 11799 | ` * Parameters` |
|        - | 11800 | ` *  None` |
|        - | 11801 | ` * Return` |
|        - | 11802 | ` *  Returns an integer, the value can be one of the following constants:` |
|        - | 11803 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|        - | 11804 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|        - | 11805 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|        - | 11806 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|        - | 11807 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|        - | 11808 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|        - | 11809 | ` */` |
|        8 | 11810 | `static int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11811 |  |
|       10 | 11812 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11813 | `	/* Return the error code */` |
|       10 | 11814 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|        4 | 11815 | `	SXUNUSED(nArg); /* cc warning */` |
|        4 | 11816 | `	SXUNUSED(apArg);` |
|       10 | 11817 | `	return PH7_OK;` |
|        2 | 11818 |  |
|        - | 11819 | `/* Possible tokens from the JSON tokenization process */` |
|        - | 11820 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|        - | 11821 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|        - | 11822 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|        - | 11823 | `#define JSON_TK_NULL    0x008 /* null */` |
|        - | 11824 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|        - | 11825 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|        - | 11826 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|        - | 11827 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|        - | 11828 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|        - | 11829 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|        - | 11830 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|        - | 11831 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|        - | 11832 | `/*` |
|        - | 11833 | ` * Tokenize an entire JSON input.` |
|        - | 11834 | ` * Get a single low-level token from the input file.` |
|        - | 11835 | ` * Update the stream pointer so that it points to the first` |
|        - | 11836 | ` * character beyond the extracted token.` |
|        - | 11837 | ` */` |
|       60 | 11838 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|        2 | 11839 |  |
|       62 | 11840 | `	int *pJsonErr = (int *)pUserData;` |
|        - | 11841 | `	SyString *pStr;` |
|        - | 11842 | `	int c;` |
|        - | 11843 | `	/* Ignore leading white spaces */` |
|       66 | 11844 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|        - | 11845 | `		/* Advance the stream cursor */` |
|        6 | 11846 | `		if( pStream->zText[0] == '\n' ){` |
|        - | 11847 | `			/* Update line counter */` |
|      ! 0 | 11848 | `			pStream->nLine++;` |
|      ! 0 | 11849 | `		}` |
|        6 | 11850 | `		pStream->zText++;` |
|        2 | 11851 | `	}` |
|       62 | 11852 | `	if( pStream->zText >= pStream->zEnd ){` |
|        - | 11853 | `		/* End of input reached */` |
|      ! 0 | 11854 | `		SXUNUSED(pCtxData); /* cc warning */` |
|      ! 0 | 11855 | `		return SXERR_EOF;` |
|        - | 11856 | `	}` |
|        - | 11857 | `	/* Record token starting position and line */` |
|       62 | 11858 | `	pToken->nLine = pStream->nLine;` |
|       62 | 11859 | `	pToken->pUserData = 0;` |
|       62 | 11860 | `	pStr = &pToken->sData;` |
|       62 | 11861 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|       77 | 11862 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|       44 | 11863 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|        - | 11864 | `			/* Single character */` |
|       36 | 11865 | `			c = pStream->zText[0];` |
|        - | 11866 | `			/* Set token type */` |
|       36 | 11867 | `			switch(c){` |
|        5 | 11868 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|       10 | 11869 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|        6 | 11870 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|        5 | 11871 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|        8 | 11872 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|        9 | 11873 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|      ! 0 | 11874 | `			default:` |
|      ! 0 | 11875 | `				break;` |
|        - | 11876 | `			}` |
|        - | 11877 | `			/* Advance the stream cursor */` |
|       36 | 11878 | `			pStream->zText++;` |
|       45 | 11879 | `	}else if( pStream->zText[0] == '"') {` |
|        - | 11880 | `		/* JSON string */` |
|       10 | 11881 | `		pStream->zText++;` |
|       10 | 11882 | `		pStr->zString++;` |
|        - | 11883 | `		/* Delimit the string */` |
|       32 | 11884 | `		while( pStream->zText < pStream->zEnd ){` |
|       32 | 11885 | `			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){` |
|       10 | 11886 | `				break;` |
|        - | 11887 | `			}` |
|       24 | 11888 | `			if( pStream->zText[0] == '\n' ){` |
|        - | 11889 | `				/* Update line counter */` |
|      ! 0 | 11890 | `				pStream->nLine++;` |
|      ! 0 | 11891 | `			}` |
|       24 | 11892 | `			pStream->zText++;` |
|        2 | 11893 | `		}` |
|       10 | 11894 | `		if( pStream->zText >= pStream->zEnd ){` |
|        - | 11895 | `			/* Missing closing '"' */` |
|      ! 0 | 11896 | `			pToken->nType = JSON_TK_INVALID;` |
|      ! 0 | 11897 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 11898 | `		}else{` |
|       10 | 11899 | `			pToken->nType = JSON_TK_STR;` |
|       10 | 11900 | `			pStream->zText++; /* Jump the closing double quotes */` |
|        2 | 11901 | `		}` |
|       24 | 11902 | `	}else if( pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|        - | 11903 | `		/* Number */` |
|       13 | 11904 | `		pStream->zText++;` |
|       13 | 11905 | `		pToken->nType = JSON_TK_NUM;` |
|       13 | 11906 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11907 | `			pStream->zText++;` |
|      ! 0 | 11908 | `		}` |
|       13 | 11909 | `		if( pStream->zText < pStream->zEnd ){` |
|       13 | 11910 | `			c = pStream->zText[0];` |
|       13 | 11911 | `			if( c == '.' ){` |
|        - | 11912 | `					/* Real number */` |
|      ! 0 | 11913 | `					pStream->zText++;` |
|      ! 0 | 11914 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11915 | `						pStream->zText++;` |
|      ! 0 | 11916 | `					}` |
|      ! 0 | 11917 | `					if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11918 | `						c = pStream->zText[0];` |
|      ! 0 | 11919 | `						if( c=='e' \|\| c=='E' ){` |
|      ! 0 | 11920 | `							pStream->zText++;` |
|      ! 0 | 11921 | `							if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11922 | `								c = pStream->zText[0];` |
|      ! 0 | 11923 | `								if( c =='+' \|\| c=='-' ){` |
|      ! 0 | 11924 | `									pStream->zText++;` |
|      ! 0 | 11925 | `								}` |
|      ! 0 | 11926 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11927 | `									pStream->zText++;` |
|      ! 0 | 11928 | `								}` |
|      ! 0 | 11929 | `							}` |
|      ! 0 | 11930 | `						}` |
|      ! 0 | 11931 | `					}` |
|       13 | 11932 | `				}else if( c=='e' \|\| c=='E' ){` |
|        - | 11933 | `					/* Real number */` |
|      ! 0 | 11934 | `					pStream->zText++;` |
|      ! 0 | 11935 | `					if( pStream->zText < pStream->zEnd ){` |
|      ! 0 | 11936 | `						c = pStream->zText[0];` |
|      ! 0 | 11937 | `						if( c =='+' \|\| c=='-' ){` |
|      ! 0 | 11938 | `							pStream->zText++;` |
|      ! 0 | 11939 | `						}` |
|      ! 0 | 11940 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|      ! 0 | 11941 | `							pStream->zText++;` |
|      ! 0 | 11942 | `						}` |
|      ! 0 | 11943 | `					}` |
|      ! 0 | 11944 | `				}` |
|        7 | 11945 | `			}` |
|       17 | 11946 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|        6 | 11947 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|        - | 11948 | `			/* boolean true */` |
|      ! 0 | 11949 | `			pToken->nType = JSON_TK_TRUE;` |
|        - | 11950 | `			/* Advance the stream cursor */` |
|      ! 0 | 11951 | `			pStream->zText += sizeof("true")-1;` |
|       11 | 11952 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|        6 | 11953 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|        - | 11954 | `			/* boolean false */` |
|      ! 0 | 11955 | `			pToken->nType = JSON_TK_FALSE;` |
|        - | 11956 | `			/* Advance the stream cursor */` |
|      ! 0 | 11957 | `			pStream->zText += sizeof("false")-1;` |
|       11 | 11958 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|        6 | 11959 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|        - | 11960 | `			/* NULL */` |
|      ! 0 | 11961 | `			pToken->nType = JSON_TK_NULL;` |
|        - | 11962 | `			/* Advance the stream cursor */` |
|      ! 0 | 11963 | `			pStream->zText += sizeof("null")-1;` |
|      ! 0 | 11964 | `	}else{` |
|        - | 11965 | `		/* Unexpected token */` |
|        8 | 11966 | `		pToken->nType = JSON_TK_INVALID;` |
|        - | 11967 | `		/* Advance the stream cursor */` |
|        8 | 11968 | `		pStream->zText++;` |
|        8 | 11969 | `		*pJsonErr = JSON_ERROR_SYNTAX;` |
|        - | 11970 | `		/* Abort processing immediatley */` |
|        8 | 11971 | `		return SXERR_ABORT;` |
|        - | 11972 | `	}` |
|        - | 11973 | `	/* record token length */` |
|       56 | 11974 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|       56 | 11975 | `	if( pToken->nType == JSON_TK_STR ){` |
|       10 | 11976 | `		pStr->nByte--;` |
|        4 | 11977 | `	}` |
|        - | 11978 | `	/* Return to the lexer */` |
|       56 | 11979 | `	return SXRET_OK;` |
|       32 | 11980 |  |
|        - | 11981 | `/*` |
|        - | 11982 | ` * JSON decoded input consumer callback signature.` |
|        - | 11983 | ` */` |
|        - | 11984 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|        - | 11985 | `/*` |
|        - | 11986 | ` * JSON decoder state is kept in the following structure.` |
|        - | 11987 | ` */` |
|        - | 11988 | `typedef struct json_decoder json_decoder;` |
|        - | 11989 | `struct json_decoder` |
|        - | 11990 |  |
|        - | 11991 | `	ph7_context *pCtx; /* Call context */` |
|        - | 11992 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|        - | 11993 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|        - | 11994 | `	int iFlags;        /* Configuration flags */` |
|        - | 11995 | `	SyToken *pIn;      /* Token stream */` |
|        - | 11996 | `	SyToken *pEnd;     /* End of the token stream */` |
|        - | 11997 | `	int rec_depth;     /* Recursion limit */` |
|        - | 11998 | `	int rec_count;     /* Current nesting level */` |
|        - | 11999 | `	int *pErr;         /* JSON decoding error if any */` |
|        - | 12000 | `};` |
|        - | 12001 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|        - | 12002 | `/* Forward declaration */` |
|        - | 12003 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|        - | 12004 | `/*` |
|        - | 12005 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|        - | 12006 | ` * the result in the given ph7_value.` |
|        - | 12007 | ` */` |
|        8 | 12008 | `static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)` |
|        2 | 12009 |  |
|       10 | 12010 | `	const char *zIn = pStr->zString;` |
|       10 | 12011 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|        - | 12012 | `	const char *zCur;` |
|        - | 12013 | `	int c;` |
|        - | 12014 | `	/* Mark the value as a string */` |
|       10 | 12015 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|        4 | 12016 | `	for(;;){` |
|       10 | 12017 | `		zCur = zIn;` |
|       32 | 12018 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|       24 | 12019 | `			zIn++;` |
|        2 | 12020 | `		}` |
|       10 | 12021 | `		if( zIn > zCur ){` |
|        - | 12022 | `			/* Append chunk verbatim */` |
|       10 | 12023 | `			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));` |
|        4 | 12024 | `		}` |
|       10 | 12025 | `		zIn++;` |
|       10 | 12026 | `		if( zIn >= zEnd ){` |
|        - | 12027 | `			/* End of the input reached */` |
|       10 | 12028 | `			break;` |
|        - | 12029 | `		}` |
|      ! 0 | 12030 | `		c = zIn[0];` |
|        - | 12031 | `		/* Unescape the character */` |
|      ! 0 | 12032 | `		switch(c){` |
|      ! 0 | 12033 | `		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|      ! 0 | 12034 | `		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|      ! 0 | 12035 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|      ! 0 | 12036 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|      ! 0 | 12037 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|      ! 0 | 12038 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|      ! 0 | 12039 | `		default:` |
|      ! 0 | 12040 | `			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));` |
|      ! 0 | 12041 | `			break;` |
|        - | 12042 | `		}` |
|        - | 12043 | `		/* Advance the stream cursor */` |
|      ! 0 | 12044 | `		zIn++;` |
|      ! 0 | 12045 | `	}` |
|       10 | 12046 |  |
|        - | 12047 | `/*` |
|        - | 12048 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|        - | 12049 | ` * According to wikipedia` |
|        - | 12050 | ` * JSON's basic types are:` |
|        - | 12051 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|        - | 12052 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|        - | 12053 | ` *   Boolean (true or false)` |
|        - | 12054 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|        - | 12055 | ` *    do not need to be of the same type)` |
|        - | 12056 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|        - | 12057 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|        - | 12058 | ` *     be distinct from each other)` |
|        - | 12059 | ` *   null (empty)` |
|        - | 12060 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|        - | 12061 | ` */` |
|       24 | 12062 | `static sxi32 VmJsonDecode(` |
|        - | 12063 | `	json_decoder *pDecoder, /* JSON decoder */` |
|        - | 12064 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|        2 | 12065 | `	){` |
|        - | 12066 | `	ph7_value *pWorker; /* Worker variable */` |
|        - | 12067 | `	sxi32 rc;` |
|        - | 12068 | `	/* Check if we do not nest to much */` |
|       26 | 12069 | `	if( pDecoder->rec_count >= pDecoder->rec_depth ){` |
|        - | 12070 | `		/* Nesting limit reached,abort decoding immediately */` |
|      ! 0 | 12071 | `		*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|      ! 0 | 12072 | `		return SXERR_ABORT;` |
|        - | 12073 | `	}` |
|       26 | 12074 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|        - | 12075 | `		/* Scalar value */` |
|       16 | 12076 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|       16 | 12077 | `		if( pWorker == 0 ){` |
|      ! 0 | 12078 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12079 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 12080 | `			return SXERR_ABORT;` |
|        - | 12081 | `		}` |
|        - | 12082 | `		/* Reflect the JSON image */` |
|       16 | 12083 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|        - | 12084 | `			/* Nullify the value.*/` |
|      ! 0 | 12085 | `			ph7_value_null(pWorker);` |
|       16 | 12086 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|        - | 12087 | `			/* Boolean value */` |
|      ! 0 | 12088 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|       16 | 12089 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|       13 | 12090 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|        - | 12091 | `			/*` |
|        - | 12092 | `			 * Numeric value.` |
|        - | 12093 | `			 * Get a string representation first then try to get a numeric` |
|        - | 12094 | `			 * value.` |
|        - | 12095 | `			 */` |
|       13 | 12096 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|        - | 12097 | `			/* Obtain a numeric representation */` |
|       13 | 12098 | `			PH7_MemObjToNumeric(pWorker);` |
|        7 | 12099 | `		}else{` |
|        - | 12100 | `			/* Dequote the string */` |
|        3 | 12101 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);` |
|        - | 12102 | `		}` |
|        - | 12103 | `		/* Invoke the consumer callback */` |
|       16 | 12104 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|       16 | 12105 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12106 | `			return SXERR_ABORT;` |
|        - | 12107 | `		}` |
|        - | 12108 | `		/* All done,advance the stream cursor */` |
|       16 | 12109 | `		pDecoder->pIn++;` |
|       19 | 12110 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|        - | 12111 | `		ProcJsonConsumer xOld;` |
|        - | 12112 | `		void *pOld;` |
|        - | 12113 | `		/* Array representation*/` |
|        5 | 12114 | `		pDecoder->pIn++;` |
|        - | 12115 | `		/* Create a working array */` |
|        5 | 12116 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|        5 | 12117 | `		if( pWorker == 0 ){` |
|      ! 0 | 12118 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12119 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 12120 | `			return SXERR_ABORT;` |
|        - | 12121 | `		}` |
|        - | 12122 | `		/* Save the old consumer */` |
|        5 | 12123 | `		xOld = pDecoder->xConsumer;` |
|        5 | 12124 | `		pOld = pDecoder->pUserData;` |
|        - | 12125 | `		/* Set the new consumer */` |
|        5 | 12126 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|        5 | 12127 | `		pDecoder->pUserData = pWorker;` |
|        - | 12128 | `		/* Decode the array */` |
|        7 | 12129 | `		for(;;){` |
|        - | 12130 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|        - | 12131 | `			 * do this.` |
|        - | 12132 | `			 */` |
|       21 | 12133 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|        7 | 12134 | `				pDecoder->pIn++;` |
|        1 | 12135 | `			}` |
|       15 | 12136 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CSB) /*']'*/ ){` |
|        5 | 12137 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|        5 | 12138 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|        2 | 12139 | `				}` |
|        5 | 12140 | `				break;` |
|        - | 12141 | `			}` |
|        - | 12142 | `			/* Recurse and decode the entry */` |
|       11 | 12143 | `			pDecoder->rec_count++;` |
|       11 | 12144 | `			rc = VmJsonDecode(pDecoder,0);` |
|       11 | 12145 | `			pDecoder->rec_count--;` |
|       11 | 12146 | `			if( rc == SXERR_ABORT ){` |
|        - | 12147 | `				/* Abort processing immediately */` |
|      ! 0 | 12148 | `				return SXERR_ABORT;` |
|        - | 12149 | `			}` |
|        - | 12150 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|       11 | 12151 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|       10 | 12152 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|        - | 12153 | `					/* Unexpected token,abort immediatley */` |
|      ! 0 | 12154 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 12155 | `					return SXERR_ABORT;` |
|        - | 12156 | `			}` |
|        1 | 12157 | `		}` |
|        - | 12158 | `		/* Restore the old consumer */` |
|        5 | 12159 | `		pDecoder->xConsumer = xOld;` |
|        5 | 12160 | `		pDecoder->pUserData = pOld;` |
|        - | 12161 | `		/* Invoke the old consumer on the decoded array */` |
|        5 | 12162 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|       10 | 12163 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|        - | 12164 | `		ProcJsonConsumer xOld;` |
|        - | 12165 | `		ph7_value *pKey;` |
|        - | 12166 | `		void *pOld;` |
|        - | 12167 | `		/* Object representation*/` |
|        8 | 12168 | `		pDecoder->pIn++;` |
|        - | 12169 | `		/* Return the object as an associative array */` |
|        8 | 12170 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|        3 | 12171 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_WARNING,` |
|        - | 12172 | `				"JSON Objects are always returned as an associative array"` |
|        - | 12173 | `				);` |
|        1 | 12174 | `		}` |
|        - | 12175 | `		/* Create a working array */` |
|        8 | 12176 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|        8 | 12177 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|        8 | 12178 | `		if( pWorker == 0 \|\| pKey == 0){` |
|      ! 0 | 12179 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12180 | `			/* Abort the decoding operation immediately */` |
|      ! 0 | 12181 | `			return SXERR_ABORT;` |
|        - | 12182 | `		}` |
|        - | 12183 | `		/* Save the old consumer */` |
|        8 | 12184 | `		xOld = pDecoder->xConsumer;` |
|        8 | 12185 | `		pOld = pDecoder->pUserData;` |
|        - | 12186 | `		/* Set the new consumer */` |
|        8 | 12187 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|        8 | 12188 | `		pDecoder->pUserData = pWorker;` |
|        - | 12189 | `		/* Decode the object */` |
|        6 | 12190 | `		for(;;){` |
|        - | 12191 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|        - | 12192 | `			 * do this.` |
|        - | 12193 | `			 */` |
|       16 | 12194 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|        3 | 12195 | `				pDecoder->pIn++;` |
|        1 | 12196 | `			}` |
|       14 | 12197 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CCB) /*'}'*/ ){` |
|        8 | 12198 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|        6 | 12199 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|        2 | 12200 | `				}` |
|        8 | 12201 | `				break;` |
|        - | 12202 | `			}` |
|        6 | 12203 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|        8 | 12204 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|        - | 12205 | `					/* Syntax error,return immediately */` |
|      ! 0 | 12206 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|      ! 0 | 12207 | `					return SXERR_ABORT;` |
|        - | 12208 | `			}` |
|        - | 12209 | `			/* Dequote the key */` |
|        8 | 12210 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);` |
|        - | 12211 | `			/* Jump the key and the colon */` |
|        8 | 12212 | `			pDecoder->pIn += 2;` |
|        - | 12213 | `			/* Recurse and decode the value */` |
|        8 | 12214 | `			pDecoder->rec_count++;` |
|        8 | 12215 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|        8 | 12216 | `			pDecoder->rec_count--;` |
|        8 | 12217 | `			if( rc == SXERR_ABORT ){` |
|        - | 12218 | `				/* Abort processing immediately */` |
|      ! 0 | 12219 | `				return SXERR_ABORT;` |
|        - | 12220 | `			}` |
|        - | 12221 | `			/* Reset the internal buffer of the key */` |
|        8 | 12222 | `			ph7_value_reset_string_cursor(pKey);` |
|        - | 12223 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|        2 | 12224 | `		}` |
|        - | 12225 | `		/* Restore the old consumer */` |
|        8 | 12226 | `		pDecoder->xConsumer = xOld;` |
|        8 | 12227 | `		pDecoder->pUserData = pOld;` |
|        - | 12228 | `		/* Invoke the old consumer on the decoded object*/` |
|        8 | 12229 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|        - | 12230 | `		/* Release the key */` |
|        8 | 12231 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|        5 | 12232 | `	}else{` |
|        - | 12233 | `		/* Unexpected token */` |
|      ! 0 | 12234 | `		return SXERR_ABORT; /* Abort immediately */` |
|        - | 12235 | `	}` |
|        - | 12236 | `	/* Release the worker variable */` |
|       26 | 12237 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|       26 | 12238 | `	return SXRET_OK;` |
|       14 | 12239 |  |
|        - | 12240 | `/*` |
|        - | 12241 | ` * The following JSON decoder callback is invoked each time` |
|        - | 12242 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|        - | 12243 | ` * is being decoded.` |
|        - | 12244 | ` */` |
|       16 | 12245 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|        2 | 12246 |  |
|       18 | 12247 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 12248 | `	/* Insert the entry */` |
|       18 | 12249 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|        8 | 12250 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 12251 | `	/* All done */` |
|       18 | 12252 | `	return SXRET_OK;` |
|        2 | 12253 |  |
|        - | 12254 | `/*` |
|        - | 12255 | ` * Standard JSON decoder callback.` |
|        - | 12256 | ` */` |
|        8 | 12257 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|        2 | 12258 |  |
|        - | 12259 | `	/* Return the value directly */` |
|       10 | 12260 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|        4 | 12261 | `	SXUNUSED(pKey); /* cc warning */` |
|        4 | 12262 | `	SXUNUSED(pUserData);` |
|        - | 12263 | `	/* All done */` |
|       10 | 12264 | `	return SXRET_OK;` |
|        2 | 12265 |  |
|        - | 12266 | `/*` |
|        - | 12267 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])` |
|        - | 12268 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|        - | 12269 | ` * Parameters` |
|        - | 12270 | ` *  $json` |
|        - | 12271 | ` *    The json string being decoded.` |
|        - | 12272 | ` * $assoc` |
|        - | 12273 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|        - | 12274 | ` * $depth` |
|        - | 12275 | ` *   User specified recursion depth.` |
|        - | 12276 | ` * $options` |
|        - | 12277 | ` *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported` |
|        - | 12278 | ` * (default is to cast large integers as floats)` |
|        - | 12279 | ` * Return` |
|        - | 12280 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|        - | 12281 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|        - | 12282 | ` *  or if the encoded data is deeper than the recursion limit.` |
|        - | 12283 | ` */` |
|       16 | 12284 | `static int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12285 |  |
|       18 | 12286 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12287 | `	json_decoder sDecoder;` |
|        - | 12288 | `	const char *zIn;` |
|        - | 12289 | `	SySet sToken;` |
|        - | 12290 | `	SyLex sLex;` |
|        - | 12291 | `	int nByte;` |
|        - | 12292 | `	sxi32 rc;` |
|       18 | 12293 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12294 | `		/* Missing/Invalid arguments, return NULL */` |
|        3 | 12295 | `		ph7_result_null(pCtx);` |
|        3 | 12296 | `		return PH7_OK;` |
|        - | 12297 | `	}` |
|        - | 12298 | `	/* Extract the JSON string */` |
|       16 | 12299 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|       16 | 12300 | `	if( nByte < 1 ){` |
|        - | 12301 | `		/* Empty string,return NULL */` |
|      ! 0 | 12302 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12303 | `		return PH7_OK;` |
|        - | 12304 | `	}` |
|        - | 12305 | `	/* Clear JSON error code */` |
|       16 | 12306 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - | 12307 | `	/* Tokenize the input */` |
|       16 | 12308 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|       16 | 12309 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|       16 | 12310 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|       16 | 12311 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|        - | 12312 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|        8 | 12313 | `		SyLexRelease(&sLex);` |
|        8 | 12314 | `		SySetRelease(&sToken);` |
|        - | 12315 | `		/* return NULL */` |
|        8 | 12316 | `		ph7_result_null(pCtx);` |
|        8 | 12317 | `		return PH7_OK;` |
|        - | 12318 | `	}` |
|        - | 12319 | `	/* Fill the decoder */` |
|       10 | 12320 | `	sDecoder.pCtx = pCtx;` |
|       10 | 12321 | `	sDecoder.pErr = &pVm->json_rc;` |
|       10 | 12322 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       10 | 12323 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|       10 | 12324 | `	sDecoder.iFlags = 0;` |
|       10 | 12325 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){` |
|        - | 12326 | `		/* Returned objects will be converted into associative arrays */` |
|        8 | 12327 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|        3 | 12328 | `	}` |
|       10 | 12329 | `	sDecoder.rec_depth = 32;` |
|       10 | 12330 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      ! 0 | 12331 | `		int nDepth = ph7_value_to_int(apArg[2]);` |
|      ! 0 | 12332 | `		if( nDepth > 1 && nDepth < 32 ){` |
|      ! 0 | 12333 | `			sDecoder.rec_depth = nDepth;` |
|      ! 0 | 12334 | `		}` |
|      ! 0 | 12335 | `	}` |
|       10 | 12336 | `	sDecoder.rec_count = 0;` |
|        - | 12337 | `	/* Set a default consumer */` |
|       10 | 12338 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|       10 | 12339 | `	sDecoder.pUserData = 0;` |
|        - | 12340 | `	/* Decode the raw JSON input */` |
|       10 | 12341 | `	rc = VmJsonDecode(&sDecoder,0);` |
|       10 | 12342 | `	if( rc == SXERR_ABORT \|\|  pVm->json_rc != JSON_ERROR_NONE ){` |
|        - | 12343 | `		/*` |
|        - | 12344 | `		 * Something goes wrong while decoding JSON input.Return NULL.` |
|        - | 12345 | `		 */` |
|      ! 0 | 12346 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12347 | `	}` |
|        - | 12348 | `	/* Clean-up the mess left behind */` |
|       10 | 12349 | `	SyLexRelease(&sLex);` |
|       10 | 12350 | `	SySetRelease(&sToken);` |
|        - | 12351 | `	/* All done */` |
|       10 | 12352 | `	return PH7_OK;` |
|       10 | 12353 |  |
|        - | 12354 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12355 | `/*` |
|        - | 12356 | ` * XML processing Functions.` |
|        - | 12357 | ` * Status:` |
|        - | 12358 | ` *    Devel.` |
|        - | 12359 | ` */` |
|        - | 12360 | `enum ph7_xml_handler_id{` |
|        - | 12361 | `	PH7_XML_START_TAG = 0, /* Start element handlers ID */` |
|        - | 12362 | `	PH7_XML_END_TAG,       /* End element handler ID*/` |
|        - | 12363 | `	PH7_XML_CDATA,         /* Character data handler ID*/` |
|        - | 12364 | `	PH7_XML_PI,            /* Processing instruction (PI) handler ID*/` |
|        - | 12365 | `	PH7_XML_DEF,           /* Default handler ID */` |
|        - | 12366 | `	PH7_XML_UNPED,         /* Unparsed entity declaration handler */` |
|        - | 12367 | `	PH7_XML_ND,            /* Notation declaration handler ID*/` |
|        - | 12368 | `	PH7_XML_EER,           /* External entity reference handler */` |
|        - | 12369 | `	PH7_XML_NS_START,      /* Start namespace declaration handler */` |
|        - | 12370 | `	PH7_XML_NS_END         /* End namespace declaration handler */` |
|        - | 12371 | `};` |
|        - | 12372 | `#define XML_TOTAL_HANDLER (PH7_XML_NS_END + 1)` |
|        - | 12373 | `/* An instance of the following structure describe a working` |
|        - | 12374 | ` * XML engine instance.` |
|        - | 12375 | ` */` |
|        - | 12376 | `typedef struct ph7_xml_engine ph7_xml_engine;` |
|        - | 12377 | `struct ph7_xml_engine` |
|        - | 12378 |  |
|        - | 12379 | `	ph7_vm *pVm;         /* VM that own this instance */` |
|        - | 12380 | `	ph7_context *pCtx;   /* Call context */` |
|        - | 12381 | `	SyXMLParser sParser; /* Underlying XML parser */` |
|        - | 12382 | `	ph7_value aCB[XML_TOTAL_HANDLER]; /* User-defined callbacks */` |
|        - | 12383 | `	ph7_value sParserValue; /* ph7_value holding this instance which is forwarded` |
|        - | 12384 | `							  * as the first argument to the user callbacks.` |
|        - | 12385 | `							  */` |
|        - | 12386 | `	int ns_sep;      /* Namespace separator */` |
|        - | 12387 | `	SyBlob sErr;     /* Error message consumer */` |
|        - | 12388 | `	sxi32 iErrCode;  /* Last error code */` |
|        - | 12389 | `	sxi32 iNest;     /* Nesting level */` |
|        - | 12390 | `	sxu32 nLine;     /* Last processed line */` |
|        - | 12391 | `	sxu32 nMagic;    /* Magic number so that we avoid misuse  */` |
|        - | 12392 | `};` |
|        - | 12393 | `#define XML_ENGINE_MAGIC 0x851EFC52` |
|        - | 12394 | `#define IS_INVALID_XML_ENGINE(XML) (XML == 0 \|\| (XML)->nMagic != XML_ENGINE_MAGIC)` |
|        - | 12395 | `/*` |
|        - | 12396 | ` * Allocate and initialize an XML engine.` |
|        - | 12397 | ` */` |
|       84 | 12398 | `static ph7_xml_engine * VmCreateXMLEngine(ph7_context *pCtx,int process_ns,int ns_sep)` |
|        1 | 12399 |  |
|        - | 12400 | `	ph7_xml_engine *pEngine;` |
|       85 | 12401 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12402 | `	ph7_value *pValue;` |
|        - | 12403 | `	sxu32 n;` |
|        - | 12404 | `	/* Allocate a new instance */` |
|       85 | 12405 | `	pEngine = (ph7_xml_engine *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_xml_engine));` |
|       85 | 12406 | `	if( pEngine == 0 ){` |
|        - | 12407 | `		/* Out of memory */` |
|      ! 0 | 12408 | `		return 0;` |
|        - | 12409 | `	}` |
|        - | 12410 | `	/* Zero the structure */` |
|       85 | 12411 | `	SyZero(pEngine,sizeof(ph7_xml_engine));` |
|        - | 12412 | `	/* Initialize fields */` |
|       85 | 12413 | `	pEngine->pVm = pVm;` |
|       85 | 12414 | `	pEngine->pCtx = 0;` |
|       85 | 12415 | `	pEngine->ns_sep = ns_sep;` |
|       85 | 12416 | `	SyXMLParserInit(&pEngine->sParser,&pVm->sAllocator,process_ns ? SXML_ENABLE_NAMESPACE : 0);` |
|       85 | 12417 | `	SyBlobInit(&pEngine->sErr,&pVm->sAllocator);` |
|       85 | 12418 | `	PH7_MemObjInit(pVm,&pEngine->sParserValue);` |
|      925 | 12419 | `	for( n = 0 ; n < SX_ARRAYSIZE(pEngine->aCB) ; ++n ){` |
|      841 | 12420 | `		pValue = &pEngine->aCB[n];` |
|        - | 12421 | `		/* NULLIFY the array entries,until someone register an event handler */` |
|      841 | 12422 | `		PH7_MemObjInit(&(*pVm),pValue);` |
|      421 | 12423 | `	}` |
|       85 | 12424 | `	ph7_value_resource(&pEngine->sParserValue,pEngine);` |
|       85 | 12425 | `	pEngine->iErrCode = SXML_ERROR_NONE;` |
|        - | 12426 | `	/* Finally set the magic number */` |
|       85 | 12427 | `	pEngine->nMagic = XML_ENGINE_MAGIC;` |
|       85 | 12428 | `	return pEngine;` |
|       43 | 12429 |  |
|        - | 12430 | `/*` |
|        - | 12431 | ` * Release an XML engine.` |
|        - | 12432 | ` */` |
|       84 | 12433 | `static void VmReleaseXMLEngine(ph7_xml_engine *pEngine)` |
|        1 | 12434 |  |
|       85 | 12435 | `	ph7_vm *pVm = pEngine->pVm;` |
|        - | 12436 | `	ph7_value *pValue;` |
|        - | 12437 | `	sxu32 n;` |
|        - | 12438 | `	/* Release fields */` |
|       85 | 12439 | `	SyBlobRelease(&pEngine->sErr);` |
|       85 | 12440 | `	SyXMLParserRelease(&pEngine->sParser);` |
|       85 | 12441 | `	PH7_MemObjRelease(&pEngine->sParserValue);` |
|      925 | 12442 | `	for( n = 0 ; n < SX_ARRAYSIZE(pEngine->aCB) ; ++n ){` |
|      841 | 12443 | `		pValue = &pEngine->aCB[n];` |
|      841 | 12444 | `		PH7_MemObjRelease(pValue);` |
|      421 | 12445 | `	}` |
|       85 | 12446 | `	pEngine->nMagic = 0x2621;` |
|        - | 12447 | `	/* Finally,release the whole instance */` |
|       85 | 12448 | `	SyMemBackendFree(&pVm->sAllocator,pEngine);` |
|       85 | 12449 |  |
|        - | 12450 | `/*` |
|        - | 12451 | ` * resource xml_parser_create([ string $encoding ])` |
|        - | 12452 | ` *  Create an UTF-8 XML parser.` |
|        - | 12453 | ` * Parameter` |
|        - | 12454 | ` *  $encoding` |
|        - | 12455 | ` *   (Only UTF-8 encoding is used)` |
|        - | 12456 | ` * Return` |
|        - | 12457 | ` *  Returns a resource handle for the new XML parser.` |
|        - | 12458 | ` */` |
|       80 | 12459 | `static int vm_builtin_xml_parser_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12460 |  |
|        - | 12461 | `	ph7_xml_engine *pEngine;` |
|        - | 12462 | `	/* Allocate a new instance */` |
|       81 | 12463 | `	pEngine = VmCreateXMLEngine(&(*pCtx),0,':');` |
|       81 | 12464 | `	if( pEngine == 0 ){` |
|      ! 0 | 12465 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12466 | `		/* Return null */` |
|      ! 0 | 12467 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12468 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12469 | `		SXUNUSED(apArg);` |
|      ! 0 | 12470 | `		return PH7_OK;` |
|        - | 12471 | `	}` |
|        - | 12472 | `	/* Return the engine as a resource */` |
|       81 | 12473 | `	ph7_result_resource(pCtx,pEngine);` |
|       81 | 12474 | `	return PH7_OK;` |
|       41 | 12475 |  |
|        - | 12476 | `/*` |
|        - | 12477 | ` * resource xml_parser_create_ns([ string $encoding[,string $separator = ':']])` |
|        - | 12478 | ` *  Create an UTF-8 XML parser with namespace support.` |
|        - | 12479 | ` * Parameter` |
|        - | 12480 | ` *  $encoding` |
|        - | 12481 | ` *   (Only UTF-8 encoding is supported)` |
|        - | 12482 | ` *  $separtor` |
|        - | 12483 | ` *   Namespace separator (a single character)` |
|        - | 12484 | ` * Return` |
|        - | 12485 | ` *  Returns a resource handle for the new XML parser.` |
|        - | 12486 | ` */` |
|        4 | 12487 | `static int vm_builtin_xml_parser_create_ns(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12488 |  |
|        - | 12489 | `	ph7_xml_engine *pEngine;` |
|        5 | 12490 | `	int ns_sep = ':';` |
|        5 | 12491 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      ! 0 | 12492 | `		const char *zSep = ph7_value_to_string(apArg[1],0);` |
|      ! 0 | 12493 | `		if( zSep[0] != 0 ){` |
|      ! 0 | 12494 | `			ns_sep = zSep[0];` |
|      ! 0 | 12495 | `		}` |
|      ! 0 | 12496 | `	}` |
|        - | 12497 | `	/* Allocate a new instance */` |
|        5 | 12498 | `	pEngine = VmCreateXMLEngine(&(*pCtx),TRUE,ns_sep);` |
|        5 | 12499 | `	if( pEngine == 0 ){` |
|      ! 0 | 12500 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - | 12501 | `		/* Return null */` |
|      ! 0 | 12502 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12503 | `		return PH7_OK;` |
|        - | 12504 | `	}` |
|        - | 12505 | `	/* Return the engine as a resource */` |
|        5 | 12506 | `	ph7_result_resource(pCtx,pEngine);` |
|        5 | 12507 | `	return PH7_OK;` |
|        3 | 12508 |  |
|        - | 12509 | `/*` |
|        - | 12510 | ` * bool xml_parser_free(resource $parser)` |
|        - | 12511 | ` *  Release an XML engine.` |
|        - | 12512 | ` * Parameter` |
|        - | 12513 | ` *  $parser` |
|        - | 12514 | ` *   A reference to the XML parser to free.` |
|        - | 12515 | ` * Return` |
|        - | 12516 | ` *  This function returns FALSE if parser does not refer` |
|        - | 12517 | ` *  to a valid parser, or else it frees the parser and returns TRUE.` |
|        - | 12518 | ` */` |
|       84 | 12519 | `static int vm_builtin_xml_parser_free(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12520 |  |
|        - | 12521 | `	ph7_xml_engine *pEngine;` |
|       85 | 12522 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12523 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12524 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12525 | `		return PH7_OK;` |
|        - | 12526 | `	}` |
|        - | 12527 | `	/* Point to the XML engine */` |
|       85 | 12528 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       85 | 12529 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12530 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12531 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12532 | `		return PH7_OK;` |
|        - | 12533 | `	}` |
|        - | 12534 | `	/* Safely release the engine */` |
|       85 | 12535 | `	VmReleaseXMLEngine(pEngine);` |
|        - | 12536 | `	/* Return TRUE */` |
|       85 | 12537 | `	ph7_result_bool(pCtx,1);` |
|       85 | 12538 | `	return PH7_OK;` |
|       43 | 12539 |  |
|        - | 12540 | `/*` |
|        - | 12541 | ` * bool xml_set_element_handler(resource $parser,callback $start_element_handler,[callback $end_element_handler])` |
|        - | 12542 | ` * Sets the element handler functions for the XML parser. start_element_handler and end_element_handler` |
|        - | 12543 | ` * are strings containing the names of functions.` |
|        - | 12544 | ` * Parameters` |
|        - | 12545 | ` *  $parser` |
|        - | 12546 | ` *   A reference to the XML parser to set up start and end element handler functions.` |
|        - | 12547 | ` *  $start_element_handler` |
|        - | 12548 | ` *    The function named by start_element_handler must accept three parameters:` |
|        - | 12549 | ` *    start_element_handler(resource $parser,string $name,array $attribs)` |
|        - | 12550 | ` *    $parser` |
|        - | 12551 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12552 | ` *   $name` |
|        - | 12553 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 12554 | ` *		is called.If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 12555 | ` *  $attribs` |
|        - | 12556 | ` *      The third parameter, attribs, contains an associative array with the element's attributes (if any).` |
|        - | 12557 | ` *		The keys of this array are the attribute names, the values are the attribute values.` |
|        - | 12558 | ` *      Attribute names are case-folded on the same criteria as element names.Attribute values are not case-folded.` |
|        - | 12559 | ` *      The original order of the attributes can be retrieved by walking through attribs the normal way, using each().` |
|        - | 12560 | ` *      The first key in the array was the first attribute, and so on.` |
|        - | 12561 | ` *      Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 12562 | ` * $end_element_handler` |
|        - | 12563 | ` *     The function named by end_element_handler must accept two parameters:` |
|        - | 12564 | ` *     end_element_handler(resource $parser,string $name)` |
|        - | 12565 | ` *    $parser` |
|        - | 12566 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12567 | ` *   $name` |
|        - | 12568 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 12569 | ` *      is called.If case-folding is in effect for this parser, the element name will be in uppercase` |
|        - | 12570 | ` *      letters.` |
|        - | 12571 | ` *      If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 12572 | ` * Return` |
|        - | 12573 | ` * TRUE on success or FALSE on failure.` |
|        - | 12574 | ` */` |
|       66 | 12575 | `static int vm_builtin_xml_set_element_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12576 |  |
|        - | 12577 | `	ph7_xml_engine *pEngine;` |
|       67 | 12578 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12579 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12580 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12581 | `		return PH7_OK;` |
|        - | 12582 | `	}` |
|        - | 12583 | `	/* Point to the XML engine */` |
|       67 | 12584 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       67 | 12585 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12586 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12587 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12588 | `		return PH7_OK;` |
|        - | 12589 | `	}` |
|       67 | 12590 | `	if( nArg > 1 ){` |
|        - | 12591 | `		/* Save the start_element_handler callback for later invocation */` |
|       67 | 12592 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_START_TAG]);` |
|       67 | 12593 | `		if( nArg > 2 ){` |
|        - | 12594 | `			/* Save the end_element_handler callback for later invocation */` |
|       67 | 12595 | `			PH7_MemObjStore(apArg[2]/* User callback*/,&pEngine->aCB[PH7_XML_END_TAG]);` |
|       33 | 12596 | `		}` |
|       33 | 12597 | `	}` |
|        - | 12598 | `	/* All done,return TRUE */` |
|       67 | 12599 | `	ph7_result_bool(pCtx,1);` |
|       67 | 12600 | `	return PH7_OK;` |
|       34 | 12601 |  |
|        - | 12602 | `/*` |
|        - | 12603 | ` * bool xml_set_character_data_handler(resource $parser,callback $handler)` |
|        - | 12604 | ` *  Sets the character data handler function for the XML parser parser.` |
|        - | 12605 | ` * Parameters` |
|        - | 12606 | ` * $parser` |
|        - | 12607 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12608 | ` * $handler` |
|        - | 12609 | ` *  handler is a string containing the name of the callback.` |
|        - | 12610 | ` *  The function named by handler must accept two parameters:` |
|        - | 12611 | ` *   handler(resource $parser,string $data)` |
|        - | 12612 | ` *  $parser` |
|        - | 12613 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12614 | ` *  $data` |
|        - | 12615 | ` *   The second parameter, data, contains the character data as a string.` |
|        - | 12616 | ` *   Character data handler is called for every piece of a text in the XML document.` |
|        - | 12617 | ` *   It can be called multiple times inside each fragment (e.g. for non-ASCII strings).` |
|        - | 12618 | ` *   If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 12619 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12620 | ` *   can also be supplied.` |
|        - | 12621 | ` * Return` |
|        - | 12622 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12623 | ` */` |
|       40 | 12624 | `static int vm_builtin_xml_set_character_data_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12625 |  |
|        - | 12626 | `	ph7_xml_engine *pEngine;` |
|       41 | 12627 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12628 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12629 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12630 | `		return PH7_OK;` |
|        - | 12631 | `	}` |
|        - | 12632 | `	/* Point to the XML engine */` |
|       41 | 12633 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       41 | 12634 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12635 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12636 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12637 | `		return PH7_OK;` |
|        - | 12638 | `	}` |
|       41 | 12639 | `	if( nArg > 1 ){` |
|        - | 12640 | `		/* Save the user callback for later invocation */` |
|       41 | 12641 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_CDATA]);` |
|       20 | 12642 | `	}` |
|        - | 12643 | `	/* All done,return TRUE */` |
|       41 | 12644 | `	ph7_result_bool(pCtx,1);` |
|       41 | 12645 | `	return PH7_OK;` |
|       21 | 12646 |  |
|        - | 12647 | `/*` |
|        - | 12648 | ` * bool xml_set_default_handler(resource $parser,callback $handler)` |
|        - | 12649 | ` *  Set up default handler.` |
|        - | 12650 | ` * Parameters` |
|        - | 12651 | ` * $parser` |
|        - | 12652 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12653 | ` * $handler` |
|        - | 12654 | ` *  handler is a string containing the name of the callback.` |
|        - | 12655 | ` *  The function named by handler must accept two parameters:` |
|        - | 12656 | ` *   handler(resource $parser,string $data)` |
|        - | 12657 | ` *  $parser` |
|        - | 12658 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12659 | ` *  $data` |
|        - | 12660 | ` *   The second parameter, data, contains the character data.This may be the XML declaration` |
|        - | 12661 | ` *   document type declaration, entities or other data for which no other handler exists.` |
|        - | 12662 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12663 | ` *   can also be supplied.` |
|        - | 12664 | ` * Return` |
|        - | 12665 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12666 | ` */` |
|        2 | 12667 | `static int vm_builtin_xml_set_default_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12668 |  |
|        - | 12669 | `	ph7_xml_engine *pEngine;` |
|        3 | 12670 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12671 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12672 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12673 | `		return PH7_OK;` |
|        - | 12674 | `	}` |
|        - | 12675 | `	/* Point to the XML engine */` |
|        3 | 12676 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12677 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12678 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12679 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12680 | `		return PH7_OK;` |
|        - | 12681 | `	}` |
|        3 | 12682 | `	if( nArg > 1 ){` |
|        - | 12683 | `		/* Save the user callback for later invocation */` |
|        3 | 12684 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_DEF]);` |
|        1 | 12685 | `	}` |
|        - | 12686 | `	/* All done,return TRUE */` |
|        3 | 12687 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12688 | `	return PH7_OK;` |
|        2 | 12689 |  |
|        - | 12690 | `/*` |
|        - | 12691 | ` * bool xml_set_end_namespace_decl_handler(resource $parser,callback $handler)` |
|        - | 12692 | ` *  Set up end namespace declaration handler.` |
|        - | 12693 | ` * Parameters` |
|        - | 12694 | ` * $parser` |
|        - | 12695 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12696 | ` * $handler` |
|        - | 12697 | ` *  handler is a string containing the name of the callback.` |
|        - | 12698 | ` *  The function named by handler must accept two parameters:` |
|        - | 12699 | ` *   handler(resource $parser,string $prefix)` |
|        - | 12700 | ` *  $parser` |
|        - | 12701 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12702 | ` *  $prefix` |
|        - | 12703 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 12704 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12705 | ` *   can also be supplied.` |
|        - | 12706 | ` * Return` |
|        - | 12707 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12708 | ` */` |
|        2 | 12709 | `static int vm_builtin_xml_set_end_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12710 |  |
|        - | 12711 | `	ph7_xml_engine *pEngine;` |
|        3 | 12712 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12713 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12714 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12715 | `		return PH7_OK;` |
|        - | 12716 | `	}` |
|        - | 12717 | `	/* Point to the XML engine */` |
|        3 | 12718 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12719 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12720 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12721 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12722 | `		return PH7_OK;` |
|        - | 12723 | `	}` |
|        3 | 12724 | `	if( nArg > 1 ){` |
|        - | 12725 | `		/* Save the user callback for later invocation */` |
|        3 | 12726 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_NS_END]);` |
|        1 | 12727 | `	}` |
|        - | 12728 | `	/* All done,return TRUE */` |
|        3 | 12729 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12730 | `	return PH7_OK;` |
|        2 | 12731 |  |
|        - | 12732 | `/*` |
|        - | 12733 | ` * bool xml_set_start_namespace_decl_handler(resource $parser,callback $handler)` |
|        - | 12734 | ` *  Set up start namespace declaration handler.` |
|        - | 12735 | ` * Parameters` |
|        - | 12736 | ` * $parser` |
|        - | 12737 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12738 | ` * $handler` |
|        - | 12739 | ` *  handler is a string containing the name of the callback.` |
|        - | 12740 | ` *  The function named by handler must accept two parameters:` |
|        - | 12741 | ` *   handler(resource $parser,string $prefix,string $uri)` |
|        - | 12742 | ` *  $parser` |
|        - | 12743 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12744 | ` *  $prefix` |
|        - | 12745 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 12746 | ` *  $uri` |
|        - | 12747 | ` *    Uniform Resource Identifier (URI) of namespace.` |
|        - | 12748 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12749 | ` *   can also be supplied.` |
|        - | 12750 | ` * Return` |
|        - | 12751 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12752 | ` */` |
|        2 | 12753 | `static int vm_builtin_xml_set_start_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12754 |  |
|        - | 12755 | `	ph7_xml_engine *pEngine;` |
|        3 | 12756 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12757 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12758 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12759 | `		return PH7_OK;` |
|        - | 12760 | `	}` |
|        - | 12761 | `	/* Point to the XML engine */` |
|        3 | 12762 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12763 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12764 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12765 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12766 | `		return PH7_OK;` |
|        - | 12767 | `	}` |
|        3 | 12768 | `	if( nArg > 1 ){` |
|        - | 12769 | `		/* Save the user callback for later invocation */` |
|        3 | 12770 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_NS_START]);` |
|        1 | 12771 | `	}` |
|        - | 12772 | `	/* All done,return TRUE */` |
|        3 | 12773 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12774 | `	return PH7_OK;` |
|        2 | 12775 |  |
|        - | 12776 | `/*` |
|        - | 12777 | ` * bool xml_set_processing_instruction_handler(resource $parser,callback $handler)` |
|        - | 12778 | ` *  Set up processing instruction (PI) handler.` |
|        - | 12779 | ` * Parameters` |
|        - | 12780 | ` * $parser` |
|        - | 12781 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12782 | ` * $handler` |
|        - | 12783 | ` *  handler is a string containing the name of the callback.` |
|        - | 12784 | ` *  The function named by handler must accept three parameters:` |
|        - | 12785 | ` *   handler(resource $parser,string $target,string $data)` |
|        - | 12786 | ` *  $parser` |
|        - | 12787 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12788 | ` *  $target` |
|        - | 12789 | ` *   The second parameter, target, contains the PI target.` |
|        - | 12790 | ` *  $data` |
|        - | 12791 | `     The third parameter, data, contains the PI data.` |
|        - | 12792 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12793 | ` *   can also be supplied.` |
|        - | 12794 | ` * Return` |
|        - | 12795 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12796 | ` */` |
|        8 | 12797 | `static int vm_builtin_xml_set_processing_instruction_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12798 |  |
|        - | 12799 | `	ph7_xml_engine *pEngine;` |
|        9 | 12800 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12801 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12802 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12803 | `		return PH7_OK;` |
|        - | 12804 | `	}` |
|        - | 12805 | `	/* Point to the XML engine */` |
|        9 | 12806 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        9 | 12807 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12808 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12809 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12810 | `		return PH7_OK;` |
|        - | 12811 | `	}` |
|        9 | 12812 | `	if( nArg > 1 ){` |
|        - | 12813 | `		/* Save the user callback for later invocation */` |
|        9 | 12814 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_PI]);` |
|        4 | 12815 | `	}` |
|        - | 12816 | `	/* All done,return TRUE */` |
|        9 | 12817 | `	ph7_result_bool(pCtx,1);` |
|        9 | 12818 | `	return PH7_OK;` |
|        5 | 12819 |  |
|        - | 12820 | `/*` |
|        - | 12821 | ` * bool xml_set_unparsed_entity_decl_handler(resource $parser,callback $handler)` |
|        - | 12822 | ` *  Set up unparsed entity declaration handler.` |
|        - | 12823 | ` * Parameters` |
|        - | 12824 | ` * $parser` |
|        - | 12825 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12826 | ` * $handler` |
|        - | 12827 | ` *  handler is a string containing the name of the callback.` |
|        - | 12828 | ` *  The function named by handler must accept six parameters:` |
|        - | 12829 | ` *  handler(resource $parser,string $entity_name,string $base,string $system_id,string $public_id,string $notation_name)` |
|        - | 12830 | ` *  $parser` |
|        - | 12831 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12832 | ` *  $entity_name` |
|        - | 12833 | ` *   The name of the entity that is about to be defined.` |
|        - | 12834 | ` *  $base` |
|        - | 12835 | ` *   This is the base for resolving the system identifier (systemId) of the external entity.` |
|        - | 12836 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12837 | ` *  $system_id` |
|        - | 12838 | ` *   System identifier for the external entity.` |
|        - | 12839 | ` *  $public_id` |
|        - | 12840 | ` *    Public identifier for the external entity.` |
|        - | 12841 | ` *  $notation_name` |
|        - | 12842 | ` *    Name of the notation of this entity (see xml_set_notation_decl_handler()).` |
|        - | 12843 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12844 | ` *   can also be supplied.` |
|        - | 12845 | ` * Return` |
|        - | 12846 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12847 | ` */` |
|        2 | 12848 | `static int vm_builtin_xml_set_unparsed_entity_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12849 |  |
|        - | 12850 | `	ph7_xml_engine *pEngine;` |
|        3 | 12851 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12852 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12853 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12854 | `		return PH7_OK;` |
|        - | 12855 | `	}` |
|        - | 12856 | `	/* Point to the XML engine */` |
|        3 | 12857 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12858 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12859 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12860 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12861 | `		return PH7_OK;` |
|        - | 12862 | `	}` |
|        3 | 12863 | `	if( nArg > 1 ){` |
|        - | 12864 | `		/* Save the user callback for later invocation */` |
|        3 | 12865 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_UNPED]);` |
|        1 | 12866 | `	}` |
|        - | 12867 | `	/* All done,return TRUE */` |
|        3 | 12868 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12869 | `	return PH7_OK;` |
|        2 | 12870 |  |
|        - | 12871 | `/*` |
|        - | 12872 | ` * bool xml_set_notation_decl_handler(resource $parser,callback $handler)` |
|        - | 12873 | ` *  Set up notation declaration handler.` |
|        - | 12874 | ` * Parameters` |
|        - | 12875 | ` * $parser` |
|        - | 12876 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12877 | ` * $handler` |
|        - | 12878 | ` *  handler is a string containing the name of the callback.` |
|        - | 12879 | ` *  The function named by handler must accept five parameters:` |
|        - | 12880 | ` *  handler(resource $parser,string $entity_name,string $base,string $system_id,string $public_id)` |
|        - | 12881 | ` *  $parser` |
|        - | 12882 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12883 | ` *  $entity_name` |
|        - | 12884 | ` *   The name of the entity that is about to be defined.` |
|        - | 12885 | ` *  $base` |
|        - | 12886 | ` *   This is the base for resolving the system identifier (systemId) of the external entity.` |
|        - | 12887 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12888 | ` *  $system_id` |
|        - | 12889 | ` *   System identifier for the external entity.` |
|        - | 12890 | ` *  $public_id` |
|        - | 12891 | ` *    Public identifier for the external entity.` |
|        - | 12892 | ` *  Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12893 | ` *  can also be supplied.` |
|        - | 12894 | ` * Return` |
|        - | 12895 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12896 | ` */` |
|        2 | 12897 | `static int vm_builtin_xml_set_notation_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12898 |  |
|        - | 12899 | `	ph7_xml_engine *pEngine;` |
|        3 | 12900 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12901 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12902 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12903 | `		return PH7_OK;` |
|        - | 12904 | `	}` |
|        - | 12905 | `	/* Point to the XML engine */` |
|        3 | 12906 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12907 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12908 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12909 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12910 | `		return PH7_OK;` |
|        - | 12911 | `	}` |
|        3 | 12912 | `	if( nArg > 1 ){` |
|        - | 12913 | `		/* Save the user callback for later invocation */` |
|        3 | 12914 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_ND]);` |
|        1 | 12915 | `	}` |
|        - | 12916 | `	/* All done,return TRUE */` |
|        3 | 12917 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12918 | `	return PH7_OK;` |
|        2 | 12919 |  |
|        - | 12920 | `/*` |
|        - | 12921 | ` * bool xml_set_external_entity_ref_handler(resource $parser,callback $handler)` |
|        - | 12922 | ` *  Set up external entity reference handler.` |
|        - | 12923 | ` * Parameters` |
|        - | 12924 | ` * $parser` |
|        - | 12925 | ` *   A reference to the XML parser to set up character data handler function.` |
|        - | 12926 | ` * $handler` |
|        - | 12927 | ` *  handler is a string containing the name of the callback.` |
|        - | 12928 | ` *  The function named by handler must accept five parameters:` |
|        - | 12929 | ` *   handler(resource $parser,string $open_entity_names,string $base,string $system_id,string $public_id)` |
|        - | 12930 | ` *  $parser` |
|        - | 12931 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 12932 | ` *  $open_entity_names` |
|        - | 12933 | ` *   The second parameter, open_entity_names, is a space-separated list of the names` |
|        - | 12934 | ` *   of the entities that are open for the parse of this entity (including the name of the referenced entity).` |
|        - | 12935 | ` *  $base` |
|        - | 12936 | ` *   This is the base for resolving the system identifier (system_id) of the external entity.` |
|        - | 12937 | ` *   Currently this parameter will always be set to an empty string.` |
|        - | 12938 | ` *  $system_id` |
|        - | 12939 | ` *   The fourth parameter, system_id, is the system identifier as specified in the entity declaration.` |
|        - | 12940 | ` *  $public_id` |
|        - | 12941 | ` *   The fifth parameter, public_id, is the public identifier as specified in the entity declaration` |
|        - | 12942 | ` *   or an empty string if none was specified; the whitespace in the public identifier will have been` |
|        - | 12943 | ` *   normalized as required by the XML spec.` |
|        - | 12944 | ` * Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 12945 | ` * can also be supplied.` |
|        - | 12946 | ` * Return` |
|        - | 12947 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12948 | ` */` |
|        2 | 12949 | `static int vm_builtin_xml_set_external_entity_ref_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12950 |  |
|        - | 12951 | `	ph7_xml_engine *pEngine;` |
|        3 | 12952 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12953 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12954 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12955 | `		return PH7_OK;` |
|        - | 12956 | `	}` |
|        - | 12957 | `	/* Point to the XML engine */` |
|        3 | 12958 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 12959 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12960 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12961 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12962 | `		return PH7_OK;` |
|        - | 12963 | `	}` |
|        3 | 12964 | `	if( nArg > 1 ){` |
|        - | 12965 | `		/* Save the user callback for later invocation */` |
|        3 | 12966 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_EER]);` |
|        1 | 12967 | `	}` |
|        - | 12968 | `	/* All done,return TRUE */` |
|        3 | 12969 | `	ph7_result_bool(pCtx,1);` |
|        3 | 12970 | `	return PH7_OK;` |
|        2 | 12971 |  |
|        - | 12972 | `/*` |
|        - | 12973 | ` * int xml_get_current_line_number(resource $parser)` |
|        - | 12974 | ` *  Gets the current line number for the given XML parser.` |
|        - | 12975 | ` * Parameters` |
|        - | 12976 | ` * $parser` |
|        - | 12977 | ` *   A reference to the XML parser.` |
|        - | 12978 | ` * Return` |
|        - | 12979 | ` *  This function returns FALSE if parser does not refer` |
|        - | 12980 | ` *  to a valid parser, or else it returns which line the parser` |
|        - | 12981 | ` *  is currently at in its data buffer.` |
|        - | 12982 | ` */` |
|        8 | 12983 | `static int vm_builtin_xml_get_current_line_number(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12984 |  |
|        - | 12985 | `	ph7_xml_engine *pEngine;` |
|        9 | 12986 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12987 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 12988 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12989 | `		return PH7_OK;` |
|        - | 12990 | `	}` |
|        - | 12991 | `	/* Point to the XML engine */` |
|        9 | 12992 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        9 | 12993 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 12994 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 12995 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12996 | `		return PH7_OK;` |
|        - | 12997 | `	}` |
|        - | 12998 | `	/* Return the line number */` |
|        9 | 12999 | `	ph7_result_int(pCtx,(int)pEngine->nLine);` |
|        9 | 13000 | `	return PH7_OK;` |
|        5 | 13001 |  |
|        - | 13002 | `/*` |
|        - | 13003 | ` * int xml_get_current_byte_index(resource $parser)` |
|        - | 13004 | ` *  Gets the current byte index of the given XML parser.` |
|        - | 13005 | ` * Parameters` |
|        - | 13006 | ` * $parser` |
|        - | 13007 | ` *   A reference to the XML parser.` |
|        - | 13008 | ` * Return` |
|        - | 13009 | ` *  This function returns FALSE if parser does not refer to a valid` |
|        - | 13010 | ` *  parser, or else it returns which byte index the parser is currently` |
|        - | 13011 | ` *  at in its data buffer (starting at 0).` |
|        - | 13012 | ` */` |
|        4 | 13013 | `static int vm_builtin_xml_get_current_byte_index(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13014 |  |
|        - | 13015 | `	ph7_xml_engine *pEngine;` |
|        - | 13016 | `	SyStream *pStream;` |
|        - | 13017 | `	SyToken *pToken;` |
|        5 | 13018 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13019 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13020 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13021 | `		return PH7_OK;` |
|        - | 13022 | `	}` |
|        - | 13023 | `	/* Point to the XML engine */` |
|        5 | 13024 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        5 | 13025 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13026 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13027 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13028 | `		return PH7_OK;` |
|        - | 13029 | `	}` |
|        - | 13030 | `	/* Point to the current processed token */` |
|        5 | 13031 | `	pToken = (SyToken *)SySetPeekCurrentEntry(&pEngine->sParser.sToken);` |
|        5 | 13032 | `	if( pToken == 0 ){` |
|        - | 13033 | `		/* Stream not yet processed */` |
|        3 | 13034 | `		ph7_result_int(pCtx,0);` |
|        3 | 13035 | `		return 0;` |
|        - | 13036 | `	}` |
|        - | 13037 | `	/* Point to the input stream */` |
|        3 | 13038 | `	pStream = &pEngine->sParser.sLex.sStream;` |
|        - | 13039 | `	/* Return the byte index */` |
|        3 | 13040 | `	ph7_result_int64(pCtx,(ph7_int64)(pToken->sData.zString-(const char *)pStream->zInput));` |
|        3 | 13041 | `	return PH7_OK;` |
|        3 | 13042 |  |
|        - | 13043 | `/*` |
|        - | 13044 | ` * bool xml_set_object(resource $parser,object &$object)` |
|        - | 13045 | ` *  Use XML Parser within an object.` |
|        - | 13046 | ` * NOTE` |
|        - | 13047 | ` *  This function is depreceated and is a no-op.` |
|        - | 13048 | ` * Parameters` |
|        - | 13049 | ` * $parser` |
|        - | 13050 | ` *   A reference to the XML parser.` |
|        - | 13051 | ` * $object` |
|        - | 13052 | ` *  The object where to use the XML parser.` |
|        - | 13053 | ` * Return` |
|        - | 13054 | ` * Always FALSE.` |
|        - | 13055 | ` */` |
|        2 | 13056 | `static int vm_builtin_xml_set_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13057 |  |
|        - | 13058 | `	ph7_xml_engine *pEngine;` |
|        3 | 13059 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_object(apArg[1]) ){` |
|        - | 13060 | `		/* Missing/Ivalid argument,return FALSE */` |
|        3 | 13061 | `		ph7_result_bool(pCtx,0);` |
|        3 | 13062 | `		return PH7_OK;` |
|        - | 13063 | `	}` |
|        - | 13064 | `	/* Point to the XML engine */` |
|      ! 0 | 13065 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|      ! 0 | 13066 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13067 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13068 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13069 | `		return PH7_OK;` |
|        - | 13070 | `	}` |
|        - | 13071 | `	/*  Throw a notice and return */` |
|      ! 0 | 13072 | `	ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"This function is depreceated and is a no-op."` |
|        - | 13073 | `		"In order to mimic this behaviour,you can supply instead of a function name an array "` |
|        - | 13074 | `		"containing an object reference and a method name."` |
|        - | 13075 | `		);` |
|        - | 13076 | `	/* Return FALSE */` |
|      ! 0 | 13077 | `	ph7_result_bool(pCtx,0);` |
|      ! 0 | 13078 | `	return PH7_OK;` |
|        2 | 13079 |  |
|        - | 13080 | `/*` |
|        - | 13081 | ` * int xml_get_current_column_number(resource $parser)` |
|        - | 13082 | ` *  Gets the current column number of the given XML parser.` |
|        - | 13083 | ` * Parameters` |
|        - | 13084 | ` * $parser` |
|        - | 13085 | ` *   A reference to the XML parser.` |
|        - | 13086 | ` * Return` |
|        - | 13087 | ` *  This function returns FALSE if parser does not refer to a valid parser, or else it returns` |
|        - | 13088 | ` *  which column on the current line (as given by xml_get_current_line_number()) the parser` |
|        - | 13089 | ` *  is currently at.` |
|        - | 13090 | ` */` |
|        4 | 13091 | `static int vm_builtin_xml_get_current_column_number(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13092 |  |
|        - | 13093 | `	ph7_xml_engine *pEngine;` |
|        - | 13094 | `	SyStream *pStream;` |
|        - | 13095 | `	SyToken *pToken;` |
|        5 | 13096 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13097 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13098 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13099 | `		return PH7_OK;` |
|        - | 13100 | `	}` |
|        - | 13101 | `	/* Point to the XML engine */` |
|        5 | 13102 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        5 | 13103 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13104 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13105 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13106 | `		return PH7_OK;` |
|        - | 13107 | `	}` |
|        - | 13108 | `	/* Point to the current processed token */` |
|        5 | 13109 | `	pToken = (SyToken *)SySetPeekCurrentEntry(&pEngine->sParser.sToken);` |
|        5 | 13110 | `	if( pToken == 0 ){` |
|        - | 13111 | `		/* Stream not yet processed */` |
|      ! 0 | 13112 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 13113 | `		return 0;` |
|        - | 13114 | `	}` |
|        - | 13115 | `	/* Point to the input stream */` |
|        5 | 13116 | `	pStream = &pEngine->sParser.sLex.sStream;` |
|        - | 13117 | `	/* Return the byte index */` |
|        5 | 13118 | `	ph7_result_int64(pCtx,(ph7_int64)(pToken->sData.zString-(const char *)pStream->zInput)/80);` |
|        5 | 13119 | `	return PH7_OK;` |
|        3 | 13120 |  |
|        - | 13121 | `/*` |
|        - | 13122 | ` * int xml_get_error_code(resource $parser)` |
|        - | 13123 | ` *  Get XML parser error code.` |
|        - | 13124 | ` * Parameters` |
|        - | 13125 | ` * $parser` |
|        - | 13126 | ` *   A reference to the XML parser.` |
|        - | 13127 | ` * Return` |
|        - | 13128 | ` *  This function returns FALSE if parser does not refer to a valid` |
|        - | 13129 | ` *  parser, or else it returns one of the error codes listed in the error` |
|        - | 13130 | ` *  codes section.` |
|        - | 13131 | ` */` |
|       32 | 13132 | `static int vm_builtin_xml_get_error_code(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13133 |  |
|        - | 13134 | `	ph7_xml_engine *pEngine;` |
|       33 | 13135 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13136 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13137 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13138 | `		return PH7_OK;` |
|        - | 13139 | `	}` |
|        - | 13140 | `	/* Point to the XML engine */` |
|       33 | 13141 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       33 | 13142 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13143 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13144 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13145 | `		return PH7_OK;` |
|        - | 13146 | `	}` |
|        - | 13147 | `	/* Return the error code if any */` |
|       33 | 13148 | `	ph7_result_int(pCtx,pEngine->iErrCode);` |
|       33 | 13149 | `	return PH7_OK;` |
|       17 | 13150 |  |
|        - | 13151 | `/*` |
|        - | 13152 | ` * XML parser event callbacks` |
|        - | 13153 | ` * Each time the unserlying XML parser extract a single token` |
|        - | 13154 | ` * from the input,one of the following callbacks are invoked.` |
|        - | 13155 | ` * IMP-XML-ENGINE-07-07-2012 22:02 FreeBSD [chm@symisc.net]` |
|        - | 13156 | ` */` |
|        - | 13157 | `/*` |
|        - | 13158 | ` * Create a scalar ph7_value holding the value` |
|        - | 13159 | ` * of an XML tag/attribute/CDATA and so on.` |
|        - | 13160 | ` */` |
|      148 | 13161 | `static ph7_value * VmXMLValue(ph7_xml_engine *pEngine,SyXMLRawStr *pXML,SyXMLRawStr *pNsUri)` |
|        1 | 13162 |  |
|        - | 13163 | `	ph7_value *pValue;` |
|        - | 13164 | `	/* Allocate a new scalar variable */` |
|      149 | 13165 | `	pValue = ph7_context_new_scalar(pEngine->pCtx);` |
|      149 | 13166 | `	if( pValue == 0 ){` |
|      ! 0 | 13167 | `		ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13168 | `		return 0;` |
|        - | 13169 | `	}` |
|      149 | 13170 | `	if( pNsUri && pNsUri->nByte > 0 ){` |
|        - | 13171 | `		/* Append namespace URI and the separator */` |
|        9 | 13172 | `		ph7_value_string_format(pValue,"%.*s%c",pNsUri->nByte,pNsUri->zString,pEngine->ns_sep);` |
|        4 | 13173 | `	}` |
|        - | 13174 | `	/* Copy the tag value */` |
|      149 | 13175 | `	ph7_value_string(pValue,pXML->zString,(int)pXML->nByte);` |
|      149 | 13176 | `	return pValue;` |
|       75 | 13177 |  |
|        - | 13178 | `/*` |
|        - | 13179 | ` * Create a 'ph7_value' of type array holding the values` |
|        - | 13180 | ` * of an XML tag attributes.` |
|        - | 13181 | ` */` |
|       62 | 13182 | `static ph7_value * VmXMLAttrValue(ph7_xml_engine *pEngine,SyXMLRawStr *aAttr,sxu32 nAttr)` |
|        1 | 13183 |  |
|        - | 13184 | `	ph7_value *pArray;` |
|        - | 13185 | `	/* Create an empty array */` |
|       63 | 13186 | `	pArray = ph7_context_new_array(pEngine->pCtx);` |
|       63 | 13187 | `	if( pArray == 0 ){` |
|      ! 0 | 13188 | `		ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13189 | `		return 0;` |
|        - | 13190 | `	}` |
|       63 | 13191 | `	if( nAttr > 0 ){` |
|        - | 13192 | `		ph7_value *pKey,*pValue;` |
|        - | 13193 | `		sxu32 n;` |
|        - | 13194 | `		/* Create worker variables */` |
|        5 | 13195 | `		pKey = ph7_context_new_scalar(pEngine->pCtx);` |
|        5 | 13196 | `		pValue = ph7_context_new_scalar(pEngine->pCtx);` |
|        5 | 13197 | `		if( pKey == 0 \|\| pValue == 0 ){` |
|      ! 0 | 13198 | `			ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13199 | `			return 0;` |
|        - | 13200 | `		}` |
|        - | 13201 | `		/* Copy attributes */` |
|        9 | 13202 | `		for( n = 0 ; n < nAttr ; n += 2 ){` |
|        - | 13203 | `			/* Reset string cursors */` |
|        5 | 13204 | `			ph7_value_reset_string_cursor(pKey);` |
|        5 | 13205 | `			ph7_value_reset_string_cursor(pValue);` |
|        - | 13206 | `			/* Copy attribute name and it's associated value */` |
|        5 | 13207 | `			ph7_value_string(pKey,aAttr[n].zString,(int)aAttr[n].nByte); /* Attribute name */` |
|        5 | 13208 | `			ph7_value_string(pValue,aAttr[n+1].zString,(int)aAttr[n+1].nByte); /* Attribute value */` |
|        - | 13209 | `			/* Insert in the array */` |
|        5 | 13210 | `			ph7_array_add_elem(pArray,pKey,pValue); /* Will make it's own copy */` |
|        3 | 13211 | `		}` |
|        - | 13212 | `		/* Release the worker variables */` |
|        5 | 13213 | `		ph7_context_release_value(pEngine->pCtx,pKey);` |
|        5 | 13214 | `		ph7_context_release_value(pEngine->pCtx,pValue);` |
|        2 | 13215 | `	}` |
|        - | 13216 | `	/* Return the freshly created array */` |
|       63 | 13217 | `	return pArray;` |
|       32 | 13218 |  |
|        - | 13219 | `/*` |
|        - | 13220 | ` * Start element handler.` |
|        - | 13221 | ` * The user defined callback must accept three parameters:` |
|        - | 13222 | ` *    start_element_handler(resource $parser,string $name,array $attribs )` |
|        - | 13223 | ` *    $parser` |
|        - | 13224 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13225 | ` *    $name` |
|        - | 13226 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|        - | 13227 | ` *		is called.If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 13228 | ` *    $attribs` |
|        - | 13229 | ` *      The third parameter, attribs, contains an associative array with the element's attributes (if any).` |
|        - | 13230 | ` *		The keys of this array are the attribute names, the values are the attribute values.` |
|        - | 13231 | ` *      Attribute names are case-folded on the same criteria as element names.Attribute values are not case-folded.` |
|        - | 13232 | ` *      The original order of the attributes can be retrieved by walking through attribs the normal way, using each().` |
|        - | 13233 | ` *      The first key in the array was the first attribute, and so on.` |
|        - | 13234 | ` *      Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 13235 | ` */` |
|       78 | 13236 | `static sxi32 VmXMLStartElementHandler(SyXMLRawStr *pStart,SyXMLRawStr *pNS,sxu32 nAttr,SyXMLRawStr *aAttr,void *pUserData)` |
|        1 | 13237 |  |
|       79 | 13238 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13239 | `	ph7_value *pCallback,*pTag,*pAttr;` |
|        - | 13240 | `	/* Point to the target user defined callback */` |
|       79 | 13241 | `	pCallback = &pEngine->aCB[PH7_XML_START_TAG];` |
|        - | 13242 | `	/* Make sure the given callback is callable */` |
|       79 | 13243 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13244 | `		/* Not callable,return immediately*/` |
|       17 | 13245 | `		return SXRET_OK;` |
|        - | 13246 | `	}` |
|        - | 13247 | `	/* Create a ph7_value holding the tag name */` |
|       63 | 13248 | `	pTag = VmXMLValue(pEngine,pStart,pNS);` |
|        - | 13249 | `	/* Create a ph7_value holding the tag attributes */` |
|       63 | 13250 | `	pAttr = VmXMLAttrValue(pEngine,aAttr,nAttr);` |
|       63 | 13251 | `	if( pTag == 0  \|\| pAttr == 0 ){` |
|      ! 0 | 13252 | `		SXUNUSED(pNS); /* cc warning */` |
|        - | 13253 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13254 | `		return SXRET_OK;` |
|        - | 13255 | `	}` |
|        - | 13256 | `	/* Invoke the user callback */` |
|       63 | 13257 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTag,pAttr,(ph7_value*)0);` |
|        - | 13258 | `	/* Clean-up the mess left behind */` |
|       63 | 13259 | `	ph7_context_release_value(pEngine->pCtx,pTag);` |
|       63 | 13260 | `	ph7_context_release_value(pEngine->pCtx,pAttr);` |
|       63 | 13261 | `	return SXRET_OK;` |
|       40 | 13262 |  |
|        - | 13263 | `/*` |
|        - | 13264 | ` * End element handler.` |
|        - | 13265 | ` * The user defined callback must accept two parameters:` |
|        - | 13266 | ` *  end_element_handler(resource $parser,string $name)` |
|        - | 13267 | ` *  $parser` |
|        - | 13268 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13269 | ` *  $name` |
|        - | 13270 | ` *   The second parameter, name, contains the name of the element for which this handler is called.` |
|        - | 13271 | ` *   If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|        - | 13272 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|        - | 13273 | ` *   can also be supplied.` |
|        - | 13274 | ` */` |
|       62 | 13275 | `static sxi32 VmXMLEndElementHandler(SyXMLRawStr *pEnd,SyXMLRawStr *pNS,void *pUserData)` |
|        1 | 13276 |  |
|       63 | 13277 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13278 | `	ph7_value *pCallback,*pTag;` |
|        - | 13279 | `	/* Point to the target user defined callback */` |
|       63 | 13280 | `	pCallback = &pEngine->aCB[PH7_XML_END_TAG];` |
|        - | 13281 | `	/* Make sure the given callback is callable */` |
|       63 | 13282 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13283 | `		/* Not callable,return immediately*/` |
|        9 | 13284 | `		return SXRET_OK;` |
|        - | 13285 | `	}` |
|        - | 13286 | `	/* Create a ph7_value holding the tag name */` |
|       55 | 13287 | `	pTag = VmXMLValue(pEngine,pEnd,pNS);` |
|       55 | 13288 | `	if( pTag == 0  ){` |
|      ! 0 | 13289 | `		SXUNUSED(pNS); /* cc warning */` |
|        - | 13290 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13291 | `		return SXRET_OK;` |
|        - | 13292 | `	}` |
|        - | 13293 | `	/* Invoke the user callback */` |
|       55 | 13294 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTag,(ph7_value*)0);` |
|        - | 13295 | `	/* Clean-up the mess left behind */` |
|       55 | 13296 | `	ph7_context_release_value(pEngine->pCtx,pTag);` |
|       55 | 13297 | `	return SXRET_OK;` |
|       32 | 13298 |  |
|        - | 13299 | `/*` |
|        - | 13300 | ` * Character data handler.` |
|        - | 13301 | ` *  The user defined callback must accept two parameters:` |
|        - | 13302 | ` *  handler(resource $parser,string $data)` |
|        - | 13303 | ` *  $parser` |
|        - | 13304 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13305 | ` *  $data` |
|        - | 13306 | ` *   The second parameter, data, contains the character data as a string.` |
|        - | 13307 | ` *   Character data handler is called for every piece of a text in the XML document.` |
|        - | 13308 | ` *   It can be called multiple times inside each fragment (e.g. for non-ASCII strings).` |
|        - | 13309 | ` *   If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|        - | 13310 | ` *   Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|        - | 13311 | ` */` |
|       28 | 13312 | `static sxi32 VmXMLTextHandler(SyXMLRawStr *pText,void *pUserData)` |
|        1 | 13313 |  |
|       29 | 13314 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13315 | `	ph7_value *pCallback,*pData;` |
|        - | 13316 | `	/* Point to the target user defined callback */` |
|       29 | 13317 | `	pCallback = &pEngine->aCB[PH7_XML_CDATA];` |
|        - | 13318 | `	/* Make sure the given callback is callable */` |
|       29 | 13319 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13320 | `		/* Not callable,return immediately*/` |
|       11 | 13321 | `		return SXRET_OK;` |
|        - | 13322 | `	}` |
|        - | 13323 | `	/* Create a ph7_value holding the data */` |
|       19 | 13324 | `	pData = VmXMLValue(pEngine,&(*pText),0);` |
|       19 | 13325 | `	if( pData == 0  ){` |
|        - | 13326 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13327 | `		return SXRET_OK;` |
|        - | 13328 | `	}` |
|        - | 13329 | `	/* Invoke the user callback */` |
|       19 | 13330 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pData,(ph7_value*)0);` |
|        - | 13331 | `	/* Clean-up the mess left behind */` |
|       19 | 13332 | `	ph7_context_release_value(pEngine->pCtx,pData);` |
|       19 | 13333 | `	return SXRET_OK;` |
|       15 | 13334 |  |
|        - | 13335 | `/*` |
|        - | 13336 | ` * Processing instruction (PI) handler.` |
|        - | 13337 | ` * The user defined callback must accept two parameters:` |
|        - | 13338 | ` *   handler(resource $parser,string $target,string $data)` |
|        - | 13339 | ` *  $parser` |
|        - | 13340 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13341 | ` *  $target` |
|        - | 13342 | ` *   The second parameter, target, contains the PI target.` |
|        - | 13343 | ` *  $data` |
|        - | 13344 | ` *    The third parameter, data, contains the PI data.` |
|        - | 13345 | ` *    Note: Instead of a function name, an array containing an object reference` |
|        - | 13346 | ` *    and a method name can also be supplied.` |
|        - | 13347 | ` */` |
|        8 | 13348 | `static sxi32 VmXMLPIHandler(SyXMLRawStr *pTargetStr,SyXMLRawStr *pDataStr,void *pUserData)` |
|        1 | 13349 |  |
|        9 | 13350 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13351 | `	ph7_value *pCallback,*pTarget,*pData;` |
|        - | 13352 | `	/* Point to the target user defined callback */` |
|        9 | 13353 | `	pCallback = &pEngine->aCB[PH7_XML_PI];` |
|        - | 13354 | `	/* Make sure the given callback is callable */` |
|        9 | 13355 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13356 | `		/* Not callable,return immediately*/` |
|        5 | 13357 | `		return SXRET_OK;` |
|        - | 13358 | `	}` |
|        - | 13359 | `	/* Get a ph7_value holding the data */` |
|        5 | 13360 | `	pTarget = VmXMLValue(pEngine,&(*pTargetStr),0);` |
|        5 | 13361 | `	pData = VmXMLValue(pEngine,&(*pDataStr),0);` |
|        5 | 13362 | `	if( pTarget == 0 \|\| pData == 0  ){` |
|        - | 13363 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13364 | `		return SXRET_OK;` |
|        - | 13365 | `	}` |
|        - | 13366 | `	/* Invoke the user callback */` |
|        5 | 13367 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTarget,pData,(ph7_value*)0);` |
|        - | 13368 | `	/* Clean-up the mess left behind */` |
|        5 | 13369 | `	ph7_context_release_value(pEngine->pCtx,pTarget);` |
|        5 | 13370 | `	ph7_context_release_value(pEngine->pCtx,pData);` |
|        5 | 13371 | `	return SXRET_OK;` |
|        5 | 13372 |  |
|        - | 13373 | `/*` |
|        - | 13374 | ` * Namespace declaration handler.` |
|        - | 13375 | ` * The user defined callback must accept two parameters:` |
|        - | 13376 | ` *    handler(resource $parser,string $prefix,string $uri)` |
|        - | 13377 | ` * $parser` |
|        - | 13378 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13379 | ` * $prefix` |
|        - | 13380 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|        - | 13381 | ` * $uri` |
|        - | 13382 | ` *   Uniform Resource Identifier (URI) of namespace.` |
|        - | 13383 | ` *   Note: Instead of a function name, an array containing an object reference` |
|        - | 13384 | ` *   and a method name can also be supplied.` |
|        - | 13385 | ` */` |
|        4 | 13386 | `static sxi32 VmXMLNSStartHandler(SyXMLRawStr *pUriStr,SyXMLRawStr *pPrefixStr,void *pUserData)` |
|        1 | 13387 |  |
|        5 | 13388 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13389 | `	ph7_value *pCallback,*pUri,*pPrefix;` |
|        - | 13390 | `	/* Point to the target user defined callback */` |
|        5 | 13391 | `	pCallback = &pEngine->aCB[PH7_XML_NS_START];` |
|        - | 13392 | `	/* Make sure the given callback is callable */` |
|        5 | 13393 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13394 | `		/* Not callable,return immediately*/` |
|        3 | 13395 | `		return SXRET_OK;` |
|        - | 13396 | `	}` |
|        - | 13397 | `	/* Get a ph7_value holding the PREFIX/URI */` |
|        3 | 13398 | `	pUri = VmXMLValue(pEngine,pUriStr,0);` |
|        3 | 13399 | `	pPrefix = VmXMLValue(pEngine,pPrefixStr,0);` |
|        3 | 13400 | `	if( pUri == 0 \|\| pPrefix == 0  ){` |
|        - | 13401 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13402 | `		return SXRET_OK;` |
|        - | 13403 | `	}` |
|        - | 13404 | `	/* Invoke the user callback */` |
|        3 | 13405 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pUri,pPrefix,(ph7_value*)0);` |
|        - | 13406 | `	/* Clean-up the mess left behind */` |
|        3 | 13407 | `	ph7_context_release_value(pEngine->pCtx,pUri);` |
|        3 | 13408 | `	ph7_context_release_value(pEngine->pCtx,pPrefix);` |
|        3 | 13409 | `	return SXRET_OK;` |
|        3 | 13410 |  |
|        - | 13411 | `/*` |
|        - | 13412 | ` * Namespace end declaration handler.` |
|        - | 13413 | ` * The user defined callback must accept two parameters:` |
|        - | 13414 | ` *    handler(resource $parser,string $prefix)` |
|        - | 13415 | ` * $parser` |
|        - | 13416 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|        - | 13417 | ` * $prefix` |
|        - | 13418 | ` *  The prefix is a string used to reference the namespace within an XML object.` |
|        - | 13419 | ` *   Note: Instead of a function name, an array containing an object reference` |
|        - | 13420 | ` *   and a method name can also be supplied.` |
|        - | 13421 | ` */` |
|        4 | 13422 | `static sxi32 VmXMLNSEndHandler(SyXMLRawStr *pPrefixStr,void *pUserData)` |
|        1 | 13423 |  |
|        5 | 13424 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13425 | `	ph7_value *pCallback,*pPrefix;` |
|        - | 13426 | `	/* Point to the target user defined callback */` |
|        5 | 13427 | `	pCallback = &pEngine->aCB[PH7_XML_NS_END];` |
|        - | 13428 | `	/* Make sure the given callback is callable */` |
|        5 | 13429 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|        - | 13430 | `		/* Not callable,return immediately*/` |
|        3 | 13431 | `		return SXRET_OK;` |
|        - | 13432 | `	}` |
|        - | 13433 | `	/* Get a ph7_value holding the prefix */` |
|        3 | 13434 | `	pPrefix = VmXMLValue(pEngine,pPrefixStr,0);` |
|        3 | 13435 | `	if( pPrefix == 0 ){` |
|        - | 13436 | `		/* Out of mem,return immediately */` |
|      ! 0 | 13437 | `		return SXRET_OK;` |
|        - | 13438 | `	}` |
|        - | 13439 | `	/* Invoke the user callback */` |
|        3 | 13440 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pPrefix,(ph7_value*)0);` |
|        - | 13441 | `	/* Clean-up the mess left behind */` |
|        3 | 13442 | `	ph7_context_release_value(pEngine->pCtx,pPrefix);` |
|        3 | 13443 | `	return SXRET_OK;` |
|        3 | 13444 |  |
|        - | 13445 | `/*` |
|        - | 13446 | ` * Error Message consumer handler.` |
|        - | 13447 | ` * Each time the XML parser encounter a syntaxt error or any other error` |
|        - | 13448 | ` * related to XML processing,the following callback is invoked by the` |
|        - | 13449 | ` * underlying XML parser.` |
|        - | 13450 | ` */` |
|       34 | 13451 | `static sxi32 VmXMLErrorHandler(const char *zMessage,sxi32 iErrCode,SyToken *pToken,void *pUserData)` |
|        1 | 13452 |  |
|       35 | 13453 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|        - | 13454 | `	/* Save the error code */` |
|       35 | 13455 | `	pEngine->iErrCode = iErrCode;` |
|       17 | 13456 | `	SXUNUSED(zMessage); /* cc warning */` |
|       35 | 13457 | `	if( pToken ){` |
|       35 | 13458 | `		pEngine->nLine = pToken->nLine;` |
|       17 | 13459 | `	}` |
|        - | 13460 | `	/* Abort XML processing immediately */` |
|       35 | 13461 | `	return SXERR_ABORT;` |
|        1 | 13462 |  |
|        - | 13463 | `/*` |
|        - | 13464 | ` * int xml_parse(resource $parser,string $data[,bool $is_final = false ])` |
|        - | 13465 | ` *  Parses an XML document. The handlers for the configured events are called` |
|        - | 13466 | ` *  as many times as necessary.` |
|        - | 13467 | ` * Parameters` |
|        - | 13468 | ` *  $parser` |
|        - | 13469 | ` *   A reference to the XML parser.` |
|        - | 13470 | ` *  $data` |
|        - | 13471 | ` *   Chunk of data to parse. A document may be parsed piece-wise by calling` |
|        - | 13472 | ` *   xml_parse() several times with new data, as long as the is_final parameter` |
|        - | 13473 | ` *   is set and TRUE when the last data is parsed.` |
|        - | 13474 | ` * $is_final` |
|        - | 13475 | ` *   NOT USED. This implementation require that all the processed input be` |
|        - | 13476 | ` *   entirely loaded in memory.` |
|        - | 13477 | ` * Return` |
|        - | 13478 | ` *  Returns 1 on success or 0 on failure.` |
|        - | 13479 | ` */` |
|       74 | 13480 | `static int vm_builtin_xml_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13481 |  |
|        - | 13482 | `	ph7_xml_engine *pEngine;` |
|        - | 13483 | `	SyXMLParser *pParser;` |
|        - | 13484 | `	const char *zData;` |
|        - | 13485 | `	int nByte;` |
|       75 | 13486 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|        - | 13487 | `		/* Missing/Ivalid arguments,return FALSE */` |
|        3 | 13488 | `		ph7_result_bool(pCtx,0);` |
|        3 | 13489 | `		return PH7_OK;` |
|        - | 13490 | `	}` |
|        - | 13491 | `	/* Point to the XML engine */` |
|       73 | 13492 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|       73 | 13493 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13494 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13495 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13496 | `		return PH7_OK;` |
|        - | 13497 | `	}` |
|       73 | 13498 | `	if( pEngine->iNest > 0 ){` |
|        - | 13499 | `		/* This can happen when the user callback call xml_parse() again` |
|        - | 13500 | `		 * in it's body which is forbidden.` |
|        - | 13501 | `		 */` |
|      ! 0 | 13502 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|        - | 13503 | `			"Recursive call to %s,PH7 is returning false",` |
|      ! 0 | 13504 | `			ph7_function_name(pCtx)` |
|        - | 13505 | `			);` |
|        - | 13506 | `		/* Return FALSE */` |
|      ! 0 | 13507 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13508 | `		return PH7_OK;` |
|        - | 13509 | `	}` |
|       73 | 13510 | `	pEngine->pCtx = pCtx;` |
|        - | 13511 | `	/* Point to the underlying XML parser */` |
|       73 | 13512 | `	pParser = &pEngine->sParser;` |
|        - | 13513 | `	/* Register elements handler */` |
|       73 | 13514 | `	SyXMLParserSetEventHandler(pParser,pEngine,` |
|        - | 13515 | `		VmXMLStartElementHandler,` |
|        - | 13516 | `		VmXMLTextHandler,` |
|        - | 13517 | `		VmXMLErrorHandler,` |
|        - | 13518 | `		0,` |
|        - | 13519 | `		VmXMLEndElementHandler,` |
|        - | 13520 | `		VmXMLPIHandler,` |
|        - | 13521 | `		0,` |
|        - | 13522 | `		0,` |
|        - | 13523 | `		VmXMLNSStartHandler,` |
|        - | 13524 | `		VmXMLNSEndHandler` |
|        - | 13525 | `		);` |
|       73 | 13526 | `	pEngine->iErrCode = SXML_ERROR_NONE;` |
|        - | 13527 | `	/* Extract the raw XML input */` |
|       73 | 13528 | `	zData = ph7_value_to_string(apArg[1],&nByte);` |
|        - | 13529 | `	/* Start the parse process */` |
|       73 | 13530 | `	pEngine->iNest++;` |
|       73 | 13531 | `	SyXMLProcess(pParser,zData,(sxu32)nByte);` |
|       73 | 13532 | `	pEngine->iNest--;` |
|        - | 13533 | `	/* Return the parse result */` |
|       73 | 13534 | `	ph7_result_int(pCtx,pEngine->iErrCode == SXML_ERROR_NONE ? 1 : 0);` |
|       73 | 13535 | `	return PH7_OK;` |
|       38 | 13536 |  |
|        - | 13537 | `/*` |
|        - | 13538 | ` * bool xml_parser_set_option(resource $parser,int $option,mixed $value)` |
|        - | 13539 | ` *  Sets an option in an XML parser.` |
|        - | 13540 | ` * Parameters` |
|        - | 13541 | ` *  $parser` |
|        - | 13542 | ` *   A reference to the XML parser to set an option in.` |
|        - | 13543 | ` *  $option` |
|        - | 13544 | ` *    Which option to set. See below.` |
|        - | 13545 | ` *   The following options are available:` |
|        - | 13546 | ` *   XML_OPTION_CASE_FOLDING 	integer  Controls whether case-folding is enabled for this XML parser.` |
|        - | 13547 | ` *   XML_OPTION_SKIP_TAGSTART 	integer  Specify how many characters should be skipped in the beginning of a tag name.` |
|        - | 13548 | ` *   XML_OPTION_SKIP_WHITE 	    integer  Whether to skip values consisting of whitespace characters.` |
|        - | 13549 | ` *   XML_OPTION_TARGET_ENCODING string 	 Sets which target encoding to use in this XML parser.` |
|        - | 13550 | ` * $value` |
|        - | 13551 | ` *   The option's new value.` |
|        - | 13552 | ` * Return` |
|        - | 13553 | ` *  Returns 1 on success or 0 on failure.` |
|        - | 13554 | ` * Note:` |
|        - | 13555 | ` *  Well,none of these options have meaning under the built-in XML parser so a call to this` |
|        - | 13556 | ` *  function is a no-op.` |
|        - | 13557 | ` */` |
|        6 | 13558 | `static int vm_builtin_xml_parser_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13559 |  |
|        - | 13560 | `	ph7_xml_engine *pEngine;` |
|        7 | 13561 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13562 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13563 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13564 | `		return PH7_OK;` |
|        - | 13565 | `	}` |
|        - | 13566 | `	/* Point to the XML engine */` |
|        7 | 13567 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        7 | 13568 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13569 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13570 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13571 | `		return PH7_OK;` |
|        - | 13572 | `	}` |
|        - | 13573 | `	/* Always return FALSE */` |
|        7 | 13574 | `	ph7_result_bool(pCtx,0);` |
|        7 | 13575 | `	return PH7_OK;` |
|        4 | 13576 |  |
|        - | 13577 | `/*` |
|        - | 13578 | ` * mixed xml_parser_get_option(resource $parser,int $option)` |
|        - | 13579 | ` *  Get options from an XML parser.` |
|        - | 13580 | ` * Parameters` |
|        - | 13581 | ` *  $parser` |
|        - | 13582 | ` *   A reference to the XML parser to set an option in.` |
|        - | 13583 | ` * $option` |
|        - | 13584 | ` *   Which option to fetch.` |
|        - | 13585 | ` * Return` |
|        - | 13586 | ` *  This function returns FALSE if parser does not refer to a valid parser` |
|        - | 13587 | ` *  or if option isn't valid.Else the option's value is returned.` |
|        - | 13588 | ` */` |
|        2 | 13589 | `static int vm_builtin_xml_parser_get_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13590 |  |
|        - | 13591 | `	ph7_xml_engine *pEngine;` |
|        - | 13592 | `	int nOp;` |
|        3 | 13593 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 13594 | `		/* Missing/Ivalid argument,return FALSE */` |
|      ! 0 | 13595 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13596 | `		return PH7_OK;` |
|        - | 13597 | `	}` |
|        - | 13598 | `	/* Point to the XML engine */` |
|        3 | 13599 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|        3 | 13600 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|        - | 13601 | `		/* Corrupt engine,return FALSE */` |
|      ! 0 | 13602 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13603 | `		return PH7_OK;` |
|        - | 13604 | `	}` |
|        - | 13605 | `	/* Extract the option */` |
|        3 | 13606 | `	nOp = ph7_value_to_int(apArg[1]);` |
|        3 | 13607 | `	switch(nOp){` |
|      ! 0 | 13608 | `	case SXML_OPTION_SKIP_TAGSTART:` |
|        - | 13609 | `	case SXML_OPTION_SKIP_WHITE:` |
|        - | 13610 | `	case SXML_OPTION_CASE_FOLDING:` |
|      ! 0 | 13611 | `		ph7_result_int(pCtx,0); break;` |
|      ! 0 | 13612 | `	case SXML_OPTION_TARGET_ENCODING:` |
|      ! 0 | 13613 | `		ph7_result_string(pCtx,"UTF-8",(int)sizeof("UTF-8")-1);` |
|      ! 0 | 13614 | `		break;` |
|        1 | 13615 | `	default:` |
|        - | 13616 | `		/* Unknown option,return FALSE*/` |
|        3 | 13617 | `		ph7_result_bool(pCtx,0);` |
|        2 | 13618 | `		break;` |
|        - | 13619 | `	}` |
|        3 | 13620 | `	return PH7_OK;` |
|        2 | 13621 |  |
|        - | 13622 | `/*` |
|        - | 13623 | ` * string xml_error_string(int $code)` |
|        - | 13624 | ` *  Gets the XML parser error string associated with the given code.` |
|        - | 13625 | ` * Parameters` |
|        - | 13626 | ` *  $code` |
|        - | 13627 | ` *   An error code from xml_get_error_code().` |
|        - | 13628 | ` * Return` |
|        - | 13629 | ` *  Returns a string with a textual description of the error` |
|        - | 13630 | ` *  code, or FALSE if no description was found.` |
|        - | 13631 | ` */` |
|       30 | 13632 | `static int vm_builtin_xml_error_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13633 |  |
|       31 | 13634 | `	int nErr = -1;` |
|       31 | 13635 | `	if( nArg > 0 ){` |
|       31 | 13636 | `		nErr = ph7_value_to_int(apArg[0]);` |
|       15 | 13637 | `	}` |
|       31 | 13638 | `	switch(nErr){` |
|        1 | 13639 | `	case SXML_ERROR_DUPLICATE_ATTRIBUTE:` |
|        3 | 13640 | `		ph7_result_string(pCtx,"Duplicate attribute",-1/*Compute length automatically*/);` |
|        3 | 13641 | `		break;` |
|      ! 0 | 13642 | `	case SXML_ERROR_INCORRECT_ENCODING:` |
|      ! 0 | 13643 | `		ph7_result_string(pCtx,"Incorrect encoding",-1);` |
|      ! 0 | 13644 | `		break;` |
|      ! 0 | 13645 | `	case SXML_ERROR_INVALID_TOKEN:` |
|      ! 0 | 13646 | `		ph7_result_string(pCtx,"Unexpected token",-1);` |
|      ! 0 | 13647 | `		break;` |
|        3 | 13648 | `	case SXML_ERROR_MISPLACED_XML_PI:` |
|        7 | 13649 | `		ph7_result_string(pCtx,"Misplaced processing instruction",-1);` |
|        7 | 13650 | `		break;` |
|      ! 0 | 13651 | `	case SXML_ERROR_NO_MEMORY:` |
|      ! 0 | 13652 | `		ph7_result_string(pCtx,"Out of memory",-1);` |
|      ! 0 | 13653 | `		break;` |
|        1 | 13654 | `	case SXML_ERROR_NONE:` |
|        3 | 13655 | `		ph7_result_string(pCtx,"Not an error",-1);` |
|        3 | 13656 | `		break;` |
|        1 | 13657 | `	case SXML_ERROR_TAG_MISMATCH:` |
|        3 | 13658 | `		ph7_result_string(pCtx,"Tag mismatch",-1);` |
|        3 | 13659 | `		break;` |
|      ! 0 | 13660 | `	case -1:` |
|      ! 0 | 13661 | `		ph7_result_string(pCtx,"Unknown error code",-1);` |
|      ! 0 | 13662 | `		break;` |
|        9 | 13663 | `	default:` |
|       19 | 13664 | `		ph7_result_string(pCtx,"Syntax error",-1);` |
|       18 | 13665 | `		break;` |
|        - | 13666 | `	}` |
|       31 | 13667 | `	return PH7_OK;` |
|        1 | 13668 |  |
|        - | 13669 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13670 | `/*` |
|        - | 13671 | ` * int utf8_encode(string $input)` |
|        - | 13672 | ` *  UTF-8 encoding.` |
|        - | 13673 | ` *  This function encodes the string data to UTF-8, and returns the encoded version.` |
|        - | 13674 | ` *  UTF-8 is a standard mechanism used by Unicode for encoding wide character values` |
|        - | 13675 | ` * into a byte stream. UTF-8 is transparent to plain ASCII characters, is self-synchronized` |
|        - | 13676 | ` * (meaning it is possible for a program to figure out where in the bytestream characters start)` |
|        - | 13677 | ` * and can be used with normal string comparison functions for sorting and such.` |
|        - | 13678 | ` *  Notes on UTF-8 (According to SQLite3 authors):` |
|        - | 13679 | ` *  Byte-0    Byte-1    Byte-2    Byte-3    Value` |
|        - | 13680 | ` *  0xxxxxxx                                 00000000 00000000 0xxxxxxx` |
|        - | 13681 | ` *  110yyyyy  10xxxxxx                       00000000 00000yyy yyxxxxxx` |
|        - | 13682 | ` *  1110zzzz  10yyyyyy  10xxxxxx             00000000 zzzzyyyy yyxxxxxx` |
|        - | 13683 | ` *  11110uuu  10uuzzzz  10yyyyyy  10xxxxxx   000uuuuu zzzzyyyy yyxxxxxx` |
|        - | 13684 | ` * Parameters` |
|        - | 13685 | ` * $input` |
|        - | 13686 | ` *   String to encode or NULL on failure.` |
|        - | 13687 | ` * Return` |
|        - | 13688 | ` *  An UTF-8 encoded string.` |
|        - | 13689 | ` */` |
|        2 | 13690 | `static int vm_builtin_utf8_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13691 |  |
|        - | 13692 | `	const unsigned char *zIn,*zEnd;` |
|        - | 13693 | `	int nByte,c,e;` |
|        3 | 13694 | `	if( nArg < 1 ){` |
|        - | 13695 | `		/* Missing arguments,return null */` |
|      ! 0 | 13696 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13697 | `		return PH7_OK;` |
|        - | 13698 | `	}` |
|        - | 13699 | `	/* Extract the target string */` |
|        3 | 13700 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 13701 | `	if( nByte < 1 ){` |
|        - | 13702 | `		/* Empty string,return null */` |
|      ! 0 | 13703 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13704 | `		return PH7_OK;` |
|        - | 13705 | `	}` |
|        3 | 13706 | `	zEnd = &zIn[nByte];` |
|        - | 13707 | `	/* Start the encoding process */` |
|        2 | 13708 | `	for(;;){` |
|        5 | 13709 | `		if( zIn >= zEnd ){` |
|        - | 13710 | `			/* End of input */` |
|        3 | 13711 | `			break;` |
|        - | 13712 | `		}` |
|        3 | 13713 | `		c = zIn[0];` |
|        - | 13714 | `		/* Advance the stream cursor */` |
|        3 | 13715 | `		zIn++;` |
|        - | 13716 | `		/* Encode */` |
|        3 | 13717 | `		if( c<0x00080 ){` |
|      ! 0 | 13718 | `			e = (c&0xFF);` |
|      ! 0 | 13719 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        3 | 13720 | `		}else if( c<0x00800 ){` |
|        3 | 13721 | `			e = 0xC0 + ((c>>6)&0x1F);` |
|        3 | 13722 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        3 | 13723 | `			e = 0x80 + (c & 0x3F);` |
|        3 | 13724 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        1 | 13725 | `		}else if( c<0x10000 ){` |
|      ! 0 | 13726 | `			e = 0xE0 + ((c>>12)&0x0F);` |
|      ! 0 | 13727 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13728 | `			e = 0x80 + ((c>>6) & 0x3F);` |
|      ! 0 | 13729 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13730 | `			e = 0x80 + (c & 0x3F);` |
|      ! 0 | 13731 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13732 | `		}else{` |
|      ! 0 | 13733 | `			e = 0xF0 + ((c>>18) & 0x07);` |
|      ! 0 | 13734 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13735 | `			e = 0x80 + ((c>>12) & 0x3F);` |
|      ! 0 | 13736 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13737 | `			e = 0x80 + ((c>>6) & 0x3F);` |
|      ! 0 | 13738 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|      ! 0 | 13739 | `			e = 0x80 + (c & 0x3F);` |
|      ! 0 | 13740 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|        - | 13741 | `		}` |
|        1 | 13742 | `	}` |
|        - | 13743 | `	/* All done */` |
|        3 | 13744 | `	return PH7_OK;` |
|        2 | 13745 |  |
|        - | 13746 | `/*` |
|        - | 13747 | ` * UTF-8 decoding routine extracted from the sqlite3 source tree.` |
|        - | 13748 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|        - | 13749 | ` * Status: Public Domain` |
|        - | 13750 | ` */` |
|        - | 13751 | `/*` |
|        - | 13752 | `** This lookup table is used to help decode the first byte of` |
|        - | 13753 | `** a multi-byte UTF8 character.` |
|        - | 13754 | `*/` |
|        - | 13755 | `static const unsigned char UtfTrans1[] = {` |
|        - | 13756 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|        - | 13757 | `  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,` |
|        - | 13758 | `  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,` |
|        - | 13759 | `  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,` |
|        - | 13760 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|        - | 13761 | `  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,` |
|        - | 13762 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|        - | 13763 | `  0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,` |
|        - | 13764 | `};` |
|        - | 13765 | `/*` |
|        - | 13766 | `** Translate a single UTF-8 character.  Return the unicode value.` |
|        - | 13767 | `**` |
|        - | 13768 | `** During translation, assume that the byte that zTerm points` |
|        - | 13769 | `** is a 0x00.` |
|        - | 13770 | `**` |
|        - | 13771 | `** Write a pointer to the next unread byte back into *pzNext.` |
|        - | 13772 | `**` |
|        - | 13773 | `** Notes On Invalid UTF-8:` |
|        - | 13774 | `**` |
|        - | 13775 | `**  *  This routine never allows a 7-bit character (0x00 through 0x7f) to` |
|        - | 13776 | `**     be encoded as a multi-byte character.  Any multi-byte character that` |
|        - | 13777 | `**     attempts to encode a value between 0x00 and 0x7f is rendered as 0xfffd.` |
|        - | 13778 | `**` |
|        - | 13779 | `**  *  This routine never allows a UTF16 surrogate value to be encoded.` |
|        - | 13780 | `**     If a multi-byte character attempts to encode a value between` |
|        - | 13781 | `**     0xd800 and 0xe000 then it is rendered as 0xfffd.` |
|        - | 13782 | `**` |
|        - | 13783 | `**  *  Bytes in the range of 0x80 through 0xbf which occur as the first` |
|        - | 13784 | `**     byte of a character are interpreted as single-byte characters` |
|        - | 13785 | `**     and rendered as themselves even though they are technically` |
|        - | 13786 | `**     invalid characters.` |
|        - | 13787 | `**` |
|        - | 13788 | `**  *  This routine accepts an infinite number of different UTF8 encodings` |
|        - | 13789 | `**     for unicode values 0x80 and greater.  It do not change over-length` |
|        - | 13790 | `**     encodings to 0xfffd as some systems recommend.` |
|        - | 13791 | `*/` |
|        - | 13792 | `#define READ_UTF8(zIn, zTerm, c)                           \` |
|        - | 13793 | `  c = *(zIn++);                                            \` |
|        - | 13794 | `  if( c>=0xc0 ){                                           \` |
|        - | 13795 | `    c = UtfTrans1[c-0xc0];                                 \` |
|        - | 13796 | `    while( zIn!=zTerm && (*zIn & 0xc0)==0x80 ){            \` |
|        - | 13797 | `      c = (c<<6) + (0x3f & *(zIn++));                      \` |
|        - | 13798 | `    }                                                      \` |
|        - | 13799 | `    if( c<0x80                                             \` |
|        - | 13800 | `        \|\| (c&0xFFFFF800)==0xD800                          \` |
|        - | 13801 | `        \|\| (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }        \` |
|        - | 13802 | `  }` |
|      150 | 13803 | `PH7_PRIVATE int PH7_Utf8Read(` |
|        - | 13804 | `  const unsigned char *z,         /* First byte of UTF-8 character */` |
|        - | 13805 | `  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */` |
|        - | 13806 | `  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */` |
|        1 | 13807 | `){` |
|        - | 13808 | `  int c;` |
|      153 | 13809 | `  READ_UTF8(z, zTerm, c);` |
|      151 | 13810 | `  *pzNext = z;` |
|      151 | 13811 | `  return c;` |
|        1 | 13812 |  |
|        - | 13813 | `/*` |
|        - | 13814 | ` * string utf8_decode(string $data)` |
|        - | 13815 | ` *  This function decodes data, assumed to be UTF-8 encoded, to unicode.` |
|        - | 13816 | ` * Parameters` |
|        - | 13817 | ` * data` |
|        - | 13818 | ` *  An UTF-8 encoded string.` |
|        - | 13819 | ` * Return` |
|        - | 13820 | ` *  Unicode decoded string or NULL on failure.` |
|        - | 13821 | ` */` |
|        2 | 13822 | `static int vm_builtin_utf8_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13823 |  |
|        - | 13824 | `	const unsigned char *zIn,*zEnd;` |
|        - | 13825 | `	int nByte,c;` |
|        3 | 13826 | `	if( nArg < 1 ){` |
|        - | 13827 | `		/* Missing arguments,return null */` |
|      ! 0 | 13828 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13829 | `		return PH7_OK;` |
|        - | 13830 | `	}` |
|        - | 13831 | `	/* Extract the target string */` |
|        3 | 13832 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|        3 | 13833 | `	if( nByte < 1 ){` |
|        - | 13834 | `		/* Empty string,return null */` |
|      ! 0 | 13835 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13836 | `		return PH7_OK;` |
|        - | 13837 | `	}` |
|        3 | 13838 | `	zEnd = &zIn[nByte];` |
|        - | 13839 | `	/* Start the decoding process */` |
|        5 | 13840 | `	while( zIn < zEnd ){` |
|        3 | 13841 | `		c = PH7_Utf8Read(zIn,zEnd,&zIn);` |
|        3 | 13842 | `		if( c == 0x0 ){` |
|      ! 0 | 13843 | `			break;` |
|        - | 13844 | `		}` |
|        3 | 13845 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|        1 | 13846 | `	}` |
|        3 | 13847 | `	return PH7_OK;` |
|        2 | 13848 |  |
|        - | 13849 | `/* Table of built-in VM functions. */` |
|        - | 13850 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 13851 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 13852 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 13853 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 13854 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 13855 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 13856 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 13857 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 13858 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 13859 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 13860 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 13861 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 13862 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 13863 | `	    /* Constants management */` |
|        - | 13864 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 13865 | `	{ "define",   vm_builtin_define               },` |
|        - | 13866 | `	{ "constant", vm_builtin_constant             },` |
|        - | 13867 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 13868 | `	   /* Class/Object functions */` |
|        - | 13869 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 13870 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 13871 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 13872 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 13873 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 13874 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 13875 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 13876 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 13877 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 13878 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 13879 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 13880 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 13881 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 13882 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 13883 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 13884 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 13885 | `	   /* Random numbers/strings generators */` |
|        - | 13886 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 13887 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 13888 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 13889 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 13890 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 13891 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13892 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 13893 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 13894 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 13895 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13896 | `	   /* Language constructs functions */` |
|        - | 13897 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 13898 | `	{ "print", vm_builtin_print                   },` |
|        - | 13899 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 13900 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 13901 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 13902 | `	  /* Variable handling functions */` |
|        - | 13903 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 13904 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 13905 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 13906 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 13907 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 13908 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 13909 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 13910 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 13911 | `	  /* Ouput control functions */` |
|        - | 13912 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 13913 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 13914 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 13915 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 13916 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 13917 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 13918 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 13919 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 13920 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 13921 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 13922 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 13923 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 13924 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 13925 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 13926 | `	  /* Assertion functions */` |
|        - | 13927 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 13928 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 13929 | `	  /* Error reporting functions */` |
|        - | 13930 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 13931 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 13932 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 13933 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 13934 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 13935 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 13936 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 13937 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 13938 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 13939 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 13940 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 13941 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 13942 | `	  /* Release info */` |
|        - | 13943 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 13944 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 13945 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 13946 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 13947 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 13948 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 13949 | `	  /* hashmap */` |
|        - | 13950 | `	{"compact",          vm_builtin_compact       },` |
|        - | 13951 | `	{"extract",          vm_builtin_extract       },` |
|        - | 13952 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 13953 | `	  /* URL related function */` |
|        - | 13954 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 13955 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 13956 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13957 | `	   /* XML processing functions */` |
|        - | 13958 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 13959 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 13960 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 13961 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 13962 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 13963 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 13964 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 13965 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 13966 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 13967 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 13968 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 13969 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 13970 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 13971 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 13972 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 13973 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 13974 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 13975 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 13976 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 13977 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 13978 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 13979 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13980 | `	   /* UTF-8 encoding/decoding */` |
|        - | 13981 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 13982 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 13983 | `	   /* Command line processing */` |
|        - | 13984 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 13985 | `	   /* JSON encoding/decoding */` |
|        - | 13986 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 13987 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 13988 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 13989 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 13990 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 13991 | `	   /* Files/URI inclusion facility */` |
|        - | 13992 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 13993 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 13994 | `	{ "include",      vm_builtin_include          },` |
|        - | 13995 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 13996 | `	{ "require",      vm_builtin_require          },` |
|        - | 13997 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 13998 | `};` |
|        - | 13999 | `/*` |
|        - | 14000 | ` * Register the built-in VM functions defined above.` |
|        - | 14001 | ` */` |
|      944 | 14002 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 14003 |  |
|        - | 14004 | `	sxi32 rc;` |
|        - | 14005 | `	sxu32 n;` |
|   118002 | 14006 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 14007 | `		/* Note that these special functions have access` |
|        - | 14008 | `		 * to the underlying virtual machine as their` |
|        - | 14009 | `		 * private data.` |
|        - | 14010 | `		 */` |
|   117058 | 14011 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   117058 | 14012 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 14013 | `			return rc;` |
|        - | 14014 | `		}` |
|    58530 | 14015 | `	}` |
|      946 | 14016 | `	return SXRET_OK;` |
|      474 | 14017 |  |
|        - | 14018 | `/*` |
|        - | 14019 | ` * Check if the given name refer to an installed class.` |
|        - | 14020 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 14021 | ` */` |
|     5510 | 14022 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 14023 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 14024 | `	const char *zName,  /* Name of the target class */` |
|        - | 14025 | `	sxu32 nByte,        /* zName length */` |
|        - | 14026 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 14027 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 14028 | `						 */` |
|        - | 14029 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 14030 | `	)` |
|        2 | 14031 |  |
|        - | 14032 | `	SyHashEntry *pEntry;` |
|        - | 14033 | `	ph7_class *pClass;` |
|     2755 | 14034 | `		SXUNUSED(iNest);` |
|        - | 14035 | `	/* Perform a hash lookup */` |
|     5512 | 14036 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|        - | 14037 |  |
|     5512 | 14038 | `	if( pEntry == 0 ){` |
|        - | 14039 | `		/* No such entry,return NULL */` |
|      ! 0 | 14040 | `		return 0;` |
|        - | 14041 | `	}` |
|     5512 | 14042 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|     5512 | 14043 | `	if( !iLoadable ){` |
|        - | 14044 | `		/* Return the first class seen */` |
|     4980 | 14045 | `		return pClass;` |
|      ! 0 | 14046 | `	}else{` |
|        - | 14047 | `		/* Check the collision list */` |
|      534 | 14048 | `		while(pClass){` |
|      534 | 14049 | `			if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) == 0 ){` |
|        - | 14050 | `				/* Class is loadable */` |
|      534 | 14051 | `				return pClass;` |
|        - | 14052 | `			}` |
|        - | 14053 | `			/* Point to the next entry */` |
|      ! 0 | 14054 | `			pClass = pClass->pNextName;` |
|      ! 0 | 14055 | `		}` |
|        - | 14056 | `	}` |
|        - | 14057 | `	/* No such loadable class */` |
|      ! 0 | 14058 | `	return 0;` |
|     2757 | 14059 |  |
|        - | 14060 | `/*` |
|        - | 14061 | ` * Reference Table Implementation` |
|        - | 14062 | ` * Status: stable <chm@symisc.net>` |
|        - | 14063 | ` * Intro` |
|        - | 14064 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 14065 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 14066 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 14067 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 14068 | ` *  Refer to the official for more information on this powerful` |
|        - | 14069 | ` *  extension.` |
|        - | 14070 | ` */` |
|        - | 14071 | `/*` |
|        - | 14072 | ` * Allocate a new reference entry.` |
|        - | 14073 | ` */` |
|   606818 | 14074 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 14075 |  |
|        - | 14076 | `	VmRefObj *pRef;` |
|        - | 14077 | `	/* Allocate a new instance */` |
|   606820 | 14078 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|   606820 | 14079 | `	if( pRef == 0 ){` |
|      ! 0 | 14080 | `		return 0;` |
|        - | 14081 | `	}` |
|        - | 14082 | `	/* Zero the structure */` |
|   606820 | 14083 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 14084 | `	/* Initialize fields */` |
|   606820 | 14085 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|   606820 | 14086 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|   606820 | 14087 | `	pRef->nIdx = nIdx;` |
|   606820 | 14088 | `	return pRef;` |
|   303411 | 14089 |  |
|        - | 14090 | `/*` |
|        - | 14091 | ` * Default hash function used by the reference table` |
|        - | 14092 | ` * for lookup/insertion operations.` |
|        - | 14093 | ` */` |
|  2756112 | 14094 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 14095 |  |
|        - | 14096 | `	/* Calculate the hash based on the memory object index */` |
|  2756114 | 14097 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 14098 |  |
|        - | 14099 | `/*` |
|        - | 14100 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 14101 | ` * in the reference table.` |
|        - | 14102 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 14103 | ` * otherwise.` |
|        - | 14104 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14105 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14106 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14107 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14108 | ` * Refer to the official for more information on this powerful` |
|        - | 14109 | ` * extension.` |
|        - | 14110 | ` */` |
|  1798378 | 14111 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 14112 |  |
|        - | 14113 | `	VmRefObj *pRef;` |
|        - | 14114 | `	sxu32 nBucket;` |
|        - | 14115 | `	/* Point to the appropriate bucket */` |
|  1798380 | 14116 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 14117 | `	/* Perform the lookup */` |
|  1798380 | 14118 | `	pRef = pVm->apRefObj[nBucket];` |
|  5263388 | 14119 | `	for(;;){` |
| 10515373 | 14120 | `		if( pRef == 0 ){` |
|   656416 | 14121 | `			break;` |
|        - | 14122 | `		}` |
|  9858959 | 14123 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 14124 | `			/* Entry found */` |
|  1141966 | 14125 | `			return pRef;` |
|        - | 14126 | `		}` |
|        - | 14127 | `		/* Point to the next entry */` |
|  8716995 | 14128 | `		pRef = pRef->pNextCollide;` |
|        2 | 14129 | `	}` |
|        - | 14130 | `	/* No such entry,return NULL */` |
|   656416 | 14131 | `	return 0;` |
|   899191 | 14132 |  |
|        - | 14133 | `/*` |
|        - | 14134 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14135 | ` *` |
|        - | 14136 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14137 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14138 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14139 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14140 | ` * Refer to the official for more information on this powerful` |
|        - | 14141 | ` * extension.` |
|        - | 14142 | ` */` |
|   606818 | 14143 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14144 |  |
|        - | 14145 | `	sxu32 nBucket;` |
|   606820 | 14146 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 14147 | `		VmRefObj **apNew;` |
|        - | 14148 | `		sxu32 nNew;` |
|        - | 14149 | `		/* Allocate a larger table */` |
|     1128 | 14150 | `		nNew = pVm->nRefSize << 1;` |
|     1128 | 14151 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     1128 | 14152 | `		if( apNew ){` |
|     1128 | 14153 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 14154 | `			sxu32 n;` |
|        - | 14155 | `			/* Zero the structure */` |
|     1128 | 14156 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 14157 | `			/* Rehash all referenced entries */` |
|    97724 | 14158 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 14159 | `				/* Remove old collision links */` |
|    96598 | 14160 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 14161 | `				/* Point to the appropriate bucket */` |
|    96598 | 14162 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 14163 | `				/* Insert the entry  */` |
|    96598 | 14164 | `				pEntry->pNextCollide = apNew[nBucket];` |
|    96598 | 14165 | `				if( apNew[nBucket] ){` |
|    82784 | 14166 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|    41391 | 14167 | `				}` |
|    96598 | 14168 | `				apNew[nBucket] = pEntry;` |
|        - | 14169 | `				/* Point to the next entry */` |
|    96598 | 14170 | `				pEntry = pEntry->pNext;` |
|    48300 | 14171 | `			}` |
|        - | 14172 | `			/* Release the old table */` |
|     1128 | 14173 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 14174 | `			/* Install the new one */` |
|     1128 | 14175 | `			pVm->apRefObj = apNew;` |
|     1128 | 14176 | `			pVm->nRefSize = nNew;` |
|      563 | 14177 | `		}` |
|      563 | 14178 | `	}` |
|        - | 14179 | `	/* Point to the appropriate bucket */` |
|   606820 | 14180 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 14181 | `	/* Insert the entry */` |
|   606820 | 14182 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|   606820 | 14183 | `	if( pVm->apRefObj[nBucket] ){` |
|   567018 | 14184 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|   283507 | 14185 | `	}` |
|   606820 | 14186 | `	pVm->apRefObj[nBucket] = pRef;` |
|   606820 | 14187 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|   606820 | 14188 | `	pVm->nRefUsed++;` |
|   606820 | 14189 | `	return SXRET_OK;` |
|        2 | 14190 |  |
|        - | 14191 | `/*` |
|        - | 14192 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 14193 | ` * the reference table.` |
|        - | 14194 | ` * This function is invoked when the user perform an unset` |
|        - | 14195 | ` * call [i.e: unset($var); ].` |
|        - | 14196 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14197 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14198 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14199 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14200 | ` * Refer to the official for more information on this powerful` |
|        - | 14201 | ` * extension.` |
|        - | 14202 | ` */` |
|   591226 | 14203 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14204 |  |
|        - | 14205 | `	ph7_hashmap_node **apNode;` |
|        - | 14206 | `	SyHashEntry **apEntry;` |
|        - | 14207 | `	sxu32 n;` |
|        - | 14208 | `	/* Point to the reference table */` |
|   591228 | 14209 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|   591228 | 14210 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 14211 | `	/* Unlink the entry from the reference table */` |
|   645014 | 14212 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    53788 | 14213 | `		if( apEntry[n] ){` |
|    53756 | 14214 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    26877 | 14215 | `		}` |
|    26895 | 14216 | `	}` |
|  1132588 | 14217 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|   541362 | 14218 | `		if( apNode[n] ){` |
|     4879 | 14219 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2439 | 14220 | `		}` |
|   270682 | 14221 | `	}` |
|   591228 | 14222 | `	if( pRef->pPrevCollide ){` |
|   336908 | 14223 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   168823 | 14224 | `	}else{` |
|   254322 | 14225 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 14226 | `	}` |
|   591228 | 14227 | `	if( pRef->pNextCollide ){` |
|   545513 | 14228 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   272729 | 14229 | `	}` |
|   591228 | 14230 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 14231 | `	/* Release the node */` |
|   591228 | 14232 | `	SySetRelease(&pRef->aReference);` |
|   591228 | 14233 | `	SySetRelease(&pRef->aArrEntries);` |
|   591228 | 14234 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|   591228 | 14235 | `	pVm->nRefUsed--;` |
|   591228 | 14236 | `	return SXRET_OK;` |
|        2 | 14237 |  |
|        - | 14238 | `/*` |
|        - | 14239 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14240 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14241 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14242 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14243 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14244 | ` * Refer to the official for more information on this powerful` |
|        - | 14245 | ` * extension.` |
|        - | 14246 | ` */` |
|   621036 | 14247 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 14248 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14249 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14250 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14251 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 14252 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 14253 | `	)` |
|        2 | 14254 |  |
|   621038 | 14255 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 14256 | `	VmRefObj *pRef;` |
|        - | 14257 | `	/* Check if the referenced object already exists */` |
|   621038 | 14258 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|   621038 | 14259 | `	if( pRef == 0 ){` |
|        - | 14260 | `		/* Create a new entry */` |
|   606820 | 14261 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|   606820 | 14262 | `		if( pRef == 0 ){` |
|      ! 0 | 14263 | `			return SXERR_MEM;` |
|        - | 14264 | `		}` |
|   606820 | 14265 | `		pRef->iFlags = iFlags;` |
|        - | 14266 | `		/* Install the entry */` |
|   606820 | 14267 | `		VmRefObjInsert(&(*pVm),pRef);` |
|   303409 | 14268 | `	}` |
|   625950 | 14269 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 14270 | `		/* Safely ignore the exception frame */` |
|     4914 | 14271 | `		pFrame = pFrame->pParent;` |
|        2 | 14272 | `	}` |
|   621038 | 14273 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 14274 | `		VmSlot sRef;` |
|        - | 14275 | `		/* Local frame,record referenced entry so that it can` |
|        - | 14276 | `		 * be deleted when we leave this frame.` |
|        - | 14277 | `		 */` |
|    49610 | 14278 | `		sRef.nIdx = nIdx;` |
|    49610 | 14279 | `		sRef.pUserData = pEntry;` |
|    49610 | 14280 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 14281 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 14282 | `		}` |
|    24804 | 14283 | `	}` |
|   621038 | 14284 | `	if( pEntry ){` |
|        - | 14285 | `		/* Address of the hash-entry */` |
|    63662 | 14286 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    31830 | 14287 | `	}` |
|   621038 | 14288 | `	if( pMapEntry ){` |
|        - | 14289 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|   555408 | 14290 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|   277703 | 14291 | `	}` |
|   621038 | 14292 | `	return SXRET_OK;` |
|   310520 | 14293 |  |
|        - | 14294 | `/*` |
|        - | 14295 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 14296 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14297 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14298 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14299 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14300 | ` * Refer to the official for more information on this powerful` |
|        - | 14301 | ` * extension.` |
|        - | 14302 | ` */` |
|   586096 | 14303 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 14304 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14305 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14306 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14307 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 14308 | `	)` |
|        2 | 14309 |  |
|        - | 14310 | `	VmRefObj *pRef;` |
|        - | 14311 | `	sxu32 n;` |
|        - | 14312 | `	/* Check if the referenced object already exists */` |
|   586098 | 14313 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|   586098 | 14314 | `	if( pRef == 0 ){` |
|        - | 14315 | `		/* Not such entry */` |
|    49578 | 14316 | `		return SXERR_NOTFOUND;` |
|        - | 14317 | `	}` |
|        - | 14318 | `	/* Remove the desired entry */` |
|   536522 | 14319 | `	if( pEntry ){` |
|        - | 14320 | `		SyHashEntry **apEntry;` |
|       33 | 14321 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      129 | 14322 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|       97 | 14323 | `			if( apEntry[n] == pEntry ){` |
|        - | 14324 | `				/* Nullify the entry */` |
|       33 | 14325 | `				apEntry[n] = 0;` |
|        - | 14326 | `				/*` |
|        - | 14327 | `				 * NOTE:` |
|        - | 14328 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 14329 | `				 * we avoid wasting spaces.` |
|        - | 14330 | `				 */` |
|       16 | 14331 | `			}` |
|       49 | 14332 | `		}` |
|       16 | 14333 | `	}` |
|   536522 | 14334 | `	if( pMapEntry ){` |
|        - | 14335 | `		ph7_hashmap_node **apNode;` |
|   536490 | 14336 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  1073066 | 14337 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|   536578 | 14338 | `			if( apNode[n] == pMapEntry ){` |
|        - | 14339 | `				/* nullify the entry */` |
|   536490 | 14340 | `				apNode[n] = 0;` |
|   268244 | 14341 | `			}` |
|   268290 | 14342 | `		}` |
|   268244 | 14343 | `	}` |
|   536522 | 14344 | `	return SXRET_OK;` |
|   293050 | 14345 |  |
|        - | 14346 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 14347 | `/*` |
|        - | 14348 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 14349 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 14350 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 14351 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 14352 | ` * For more information on how to register IO stream devices,please` |
|        - | 14353 | ` * refer to the official documentation.` |
|        - | 14354 | ` */` |
|    18298 | 14355 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 14356 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 14357 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 14358 | `	int nByte              /* *pzDevice length*/` |
|        - | 14359 | `	)` |
|        2 | 14360 |  |
|        - | 14361 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 14362 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 14363 | `	SyString sDev,sCur;` |
|        - | 14364 | `	sxu32 n,nEntry;` |
|        - | 14365 | `	int rc;` |
|        - | 14366 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    18300 | 14367 | `	zNext = zCur = zIn = *pzDevice;` |
|    18300 | 14368 | `	zEnd = &zIn[nByte];` |
|  1110206 | 14369 | `	while( zIn < zEnd ){` |
|  1091910 | 14370 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 14371 | `			/* Got one */` |
|        3 | 14372 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 14373 | `			break;` |
|        - | 14374 | `		}` |
|        - | 14375 | `		/* Advance the cursor */` |
|  1091908 | 14376 | `		zIn++;` |
|        2 | 14377 | `	}` |
|    18300 | 14378 | `	if( zIn >= zEnd ){` |
|        - | 14379 | `		/* No such scheme,return the default stream */` |
|    18298 | 14380 | `		return pVm->pDefStream;` |
|        - | 14381 | `	}` |
|        3 | 14382 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 14383 | `	/* Remove leading and trailing white spaces */` |
|        3 | 14384 | `	SyStringFullTrim(&sDev);` |
|        - | 14385 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 14386 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 14387 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 14388 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 14389 | `		pStream = apStream[n];` |
|        3 | 14390 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 14391 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 14392 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 14393 | `		if( rc == 0 ){` |
|        - | 14394 | `			/* Stream device found */` |
|        3 | 14395 | `			*pzDevice = zNext;` |
|        3 | 14396 | `			return pStream;` |
|        - | 14397 | `		}` |
|      ! 0 | 14398 | `	}` |
|        - | 14399 | `	/* No such stream,return NULL */` |
|      ! 0 | 14400 | `	return 0;` |
|     9151 | 14401 |  |
|        - | 14402 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 14403 | `/*` |
|        - | 14404 | ` * Section:` |
|        - | 14405 | ` *    HTTP/URI related routines.` |
|        - | 14406 | ` * Status:` |
|        - | 14407 | ` *    Stable.` |
|        - | 14408 | ` */` |
|        - | 14409 | ` /*` |
|        - | 14410 | `  * URI Parser: Split an URI into components [i.e: Host,Path,Query,...].` |
|        - | 14411 | `  * URI syntax: [method:/][/[user[:pwd]@]host[:port]/][document]` |
|        - | 14412 | `  * This almost, but not quite, RFC1738 URI syntax.` |
|        - | 14413 | `  * This routine is not a validator,it does not check for validity` |
|        - | 14414 | `  * nor decode URI parts,the only thing this routine does is splitting` |
|        - | 14415 | `  * the input to its fields.` |
|        - | 14416 | `  * Upper layer are responsible of decoding and validating URI parts.` |
|        - | 14417 | `  * On success,this function populate the "SyhttpUri" structure passed` |
|        - | 14418 | `  * as the first argument. Otherwise SXERR_* is returned when a malformed` |
|        - | 14419 | `  * input is encountered.` |
|        - | 14420 | `  */` |
|       26 | 14421 | ` static sxi32 VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen)` |
|        1 | 14422 | ` {` |
|       27 | 14423 | `	 const char *zEnd = &zUri[nLen];` |
|       27 | 14424 | `	 sxu8 bHostOnly = FALSE;` |
|       27 | 14425 | `	 sxu8 bIPv6 = FALSE	;` |
|        - | 14426 | `	 const char *zCur;` |
|        - | 14427 | `	 SyString *pComp;` |
|       27 | 14428 | `	 sxu32 nPos = 0;` |
|        - | 14429 | `	 sxi32 rc;` |
|        - | 14430 | `	 /* Zero the structure first */` |
|       27 | 14431 | `	 SyZero(pOut,sizeof(SyhttpUri));` |
|        - | 14432 | `	 /* Remove leading and trailing white spaces  */` |
|       27 | 14433 | `	 SyStringInitFromBuf(&pOut->sRaw,zUri,nLen);` |
|       27 | 14434 | `	 SyStringFullTrim(&pOut->sRaw);` |
|        - | 14435 | `	 /* Find the first '/' separator */` |
|       27 | 14436 | `	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);` |
|       27 | 14437 | `	 if( rc != SXRET_OK ){` |
|        - | 14438 | `		 /* Assume a host name only */` |
|        7 | 14439 | `		 zCur = zEnd;` |
|        7 | 14440 | `		 bHostOnly = TRUE;` |
|        7 | 14441 | `		 goto ProcessHost;` |
|        - | 14442 | `	 }` |
|       21 | 14443 | `	 zCur = &zUri[nPos];` |
|       21 | 14444 | `	 if( zUri != zCur && zCur[-1] == ':' ){` |
|        - | 14445 | `		 /* Extract a scheme:` |
|        - | 14446 | `		  * Not that we can get an invalid scheme here.` |
|        - | 14447 | `		  * Fortunately the caller can discard any URI by comparing this scheme with its` |
|        - | 14448 | `		  * registered schemes and will report the error as soon as his comparison function` |
|        - | 14449 | `		  * fail.` |
|        - | 14450 | `		  */` |
|       19 | 14451 | `	 	pComp = &pOut->sScheme;` |
|       19 | 14452 | `		SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri - 1));` |
|       19 | 14453 | `		SyStringLeftTrim(pComp);` |
|        9 | 14454 | `	 }` |
|       21 | 14455 | `	 if( zCur[1] != '/' ){` |
|      ! 0 | 14456 | `		 if( zCur == zUri \|\| zCur[-1] == ':' ){` |
|        - | 14457 | `		  /* No authority */` |
|      ! 0 | 14458 | `		  goto PathSplit;` |
|        - | 14459 | `		}` |
|        - | 14460 | `		 /* There is something here , we will assume its an authority` |
|        - | 14461 | `		  * and someone has forgot the two prefix slashes "//",` |
|        - | 14462 | `		  * sooner or later we will detect if we are dealing with a malicious` |
|        - | 14463 | `		  * user or not,but now assume we are dealing with an authority` |
|        - | 14464 | `		  * and let the caller handle all the validation process.` |
|        - | 14465 | `		  */` |
|      ! 0 | 14466 | `		 goto ProcessHost;` |
|        - | 14467 | `	 }` |
|       21 | 14468 | `	 zUri = &zCur[2];` |
|       21 | 14469 | `	 zCur = zEnd;` |
|       21 | 14470 | `	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);` |
|       29 | 14471 | `	 if( rc == SXRET_OK ){` |
|       17 | 14472 | `		 zCur = &zUri[nPos];` |
|        8 | 14473 | `	 }` |
|        2 | 14474 | ` ProcessHost:` |
|        - | 14475 | `	 /* Extract user information if present */` |
|       27 | 14476 | `	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),'@',&nPos);` |
|       27 | 14477 | `	 if( rc == SXRET_OK ){` |
|        7 | 14478 | `		 if( nPos > 0 ){` |
|        - | 14479 | `			 sxu32 nPassOfft; /* Password offset */` |
|        7 | 14480 | `			 pComp = &pOut->sUser;` |
|        7 | 14481 | `			 SyStringInitFromBuf(pComp,zUri,nPos);` |
|        - | 14482 | `			 /* Extract the password if available */` |
|        7 | 14483 | `			 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPassOfft);` |
|        7 | 14484 | `			 if( rc == SXRET_OK && nPassOfft < nPos){` |
|        7 | 14485 | `				 pComp->nByte = nPassOfft;` |
|        7 | 14486 | `				 pComp = &pOut->sPass;` |
|        7 | 14487 | `				 pComp->zString = &zUri[nPassOfft+sizeof(char)];` |
|        7 | 14488 | `				 pComp->nByte = nPos - nPassOfft - 1;` |
|        3 | 14489 | `			 }` |
|        - | 14490 | `			 /* Update the cursor */` |
|        7 | 14491 | `			 zUri = &zUri[nPos+1];` |
|        4 | 14492 | `		 }else{` |
|      ! 0 | 14493 | `			 zUri++;` |
|        - | 14494 | `		 }` |
|        3 | 14495 | `	 }` |
|       27 | 14496 | `	 pComp = &pOut->sHost;` |
|       27 | 14497 | `	 while( zUri < zCur && SyisSpace(zUri[0])){` |
|      ! 0 | 14498 | `		 zUri++;` |
|      ! 0 | 14499 | `	 }` |
|       27 | 14500 | `	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri));` |
|       27 | 14501 | `	 if( pComp->zString[0] == '[' ){` |
|        - | 14502 | `		 /* An IPv6 Address: Make a simple naive test` |
|        - | 14503 | `		  */` |
|        3 | 14504 | `		 zUri++; pComp->zString++; pComp->nByte = 0;` |
|        9 | 14505 | `		 while( ((unsigned char)zUri[0] < 0xc0 && SyisHex(zUri[0])) \|\| zUri[0] == ':' ){` |
|        7 | 14506 | `			 zUri++; pComp->nByte++;` |
|        1 | 14507 | `		 }` |
|        3 | 14508 | `		 if( zUri[0] != ']' ){` |
|      ! 0 | 14509 | `			 return SXERR_CORRUPT; /* Malformed IPv6 address */` |
|        - | 14510 | `		 }` |
|        3 | 14511 | `		 zUri++;` |
|        3 | 14512 | `		 bIPv6 = TRUE;` |
|        1 | 14513 | `	 }` |
|        - | 14514 | `	 /* Extract a port number if available */` |
|       27 | 14515 | `	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPos);` |
|       27 | 14516 | `	 if( rc == SXRET_OK ){` |
|       11 | 14517 | `		 if( bIPv6 == FALSE ){` |
|       11 | 14518 | `			 pComp->nByte = (sxu32)(&zUri[nPos] - zUri);` |
|        5 | 14519 | `		 }` |
|       11 | 14520 | `		 pComp = &pOut->sPort;` |
|       11 | 14521 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zCur - &zUri[nPos+1]));` |
|        5 | 14522 | `	 }` |
|       27 | 14523 | `	 if( bHostOnly == TRUE ){` |
|        7 | 14524 | `		 return SXRET_OK;` |
|        - | 14525 | `	 }` |
|       10 | 14526 | `PathSplit:` |
|       21 | 14527 | `	 zUri = zCur;` |
|       21 | 14528 | `	 pComp = &pOut->sPath;` |
|       21 | 14529 | `	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zEnd-zUri));` |
|       21 | 14530 | `	 if( pComp->nByte == 0 ){` |
|        5 | 14531 | `		 return SXRET_OK; /* Empty path */` |
|        - | 14532 | `	 }` |
|       17 | 14533 | `	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'?',&nPos) ){` |
|        5 | 14534 | `		 pComp->nByte = nPos; /* Update path length */` |
|        5 | 14535 | `		 pComp = &pOut->sQuery;` |
|        5 | 14536 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]));` |
|        2 | 14537 | `	 }` |
|       17 | 14538 | `	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'#',&nPos) ){` |
|        - | 14539 | `		 /* Update path or query length */` |
|        5 | 14540 | `		 if( pComp == &pOut->sPath ){` |
|      ! 0 | 14541 | `			 pComp->nByte = nPos;` |
|      ! 0 | 14542 | `		 }else{` |
|        5 | 14543 | `			 if( &zUri[nPos] < (char *)SyStringData(pComp) ){` |
|        - | 14544 | `				 /* Malformed syntax : Query must be present before fragment */` |
|      ! 0 | 14545 | `				 return SXERR_SYNTAX;` |
|        - | 14546 | `			 }` |
|        5 | 14547 | `			 pComp->nByte -= (sxu32)(zEnd - &zUri[nPos]);` |
|        - | 14548 | `		 }` |
|        5 | 14549 | `		 pComp = &pOut->sFragment;` |
|        5 | 14550 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]))` |
|        2 | 14551 | `	 }` |
|       17 | 14552 | `	 return SXRET_OK;` |
|       14 | 14553 | ` }` |
|        - | 14554 | ` /*` |
|        - | 14555 | ` * Extract a single line from a raw HTTP request.` |
|        - | 14556 | ` * Return SXRET_OK on success,SXERR_EOF when end of input` |
|        - | 14557 | ` * and SXERR_MORE when more input is needed.` |
|        - | 14558 | ` */` |
|      ! 0 | 14559 | `static sxi32 VmGetNextLine(SyString *pCursor,SyString *pCurrent)` |
|      ! 0 | 14560 |  |
|        - | 14561 | `  	const char *zIn;` |
|        - | 14562 | `  	sxu32 nPos;` |
|        - | 14563 | `	/* Jump leading white spaces */` |
|      ! 0 | 14564 | `	SyStringLeftTrim(pCursor);` |
|      ! 0 | 14565 | `	if( pCursor->nByte < 1 ){` |
|      ! 0 | 14566 | `		SyStringInitFromBuf(pCurrent,0,0);` |
|      ! 0 | 14567 | `		return SXERR_EOF; /* End of input */` |
|        - | 14568 | `	}` |
|      ! 0 | 14569 | `	zIn = SyStringData(pCursor);` |
|      ! 0 | 14570 | `	if( SXRET_OK != SyByteListFind(pCursor->zString,pCursor->nByte,"\r\n",&nPos) ){` |
|        - | 14571 | `		/* Line not found,tell the caller to read more input from source */` |
|      ! 0 | 14572 | `		SyStringDupPtr(pCurrent,pCursor);` |
|      ! 0 | 14573 | `		return SXERR_MORE;` |
|        - | 14574 | `	}` |
|      ! 0 | 14575 | `  	pCurrent->zString = zIn;` |
|      ! 0 | 14576 | `  	pCurrent->nByte	= nPos;` |
|        - | 14577 | `  	/* advance the cursor so we can call this routine again */` |
|      ! 0 | 14578 | `  	pCursor->zString = &zIn[nPos];` |
|      ! 0 | 14579 | `  	pCursor->nByte -= nPos;` |
|      ! 0 | 14580 | `  	return SXRET_OK;` |
|      ! 0 | 14581 | ` }` |
|        - | 14582 | ` /*` |
|        - | 14583 | `  * Split a single MIME header into a name value pair.` |
|        - | 14584 | `  * This function return SXRET_OK,SXERR_CONTINUE on success.` |
|        - | 14585 | `  * Otherwise SXERR_NEXT is returned when a malformed header` |
|        - | 14586 | `  * is encountered.` |
|        - | 14587 | `  * Note: This function handle also mult-line headers.` |
|        - | 14588 | `  */` |
|      ! 0 | 14589 | ` static sxi32 VmHttpProcessOneHeader(SyhttpHeader *pHdr,SyhttpHeader *pLast,const char *zLine,sxu32 nLen)` |
|      ! 0 | 14590 | ` {` |
|        - | 14591 | `	 SyString *pName;` |
|        - | 14592 | `	 sxu32 nPos;` |
|        - | 14593 | `	 sxi32 rc;` |
|      ! 0 | 14594 | `	 if( nLen < 1 ){` |
|      ! 0 | 14595 | `		 return SXERR_NEXT;` |
|        - | 14596 | `	 }` |
|        - | 14597 | `	 /* Check for multi-line header */` |
|      ! 0 | 14598 | `	if( pLast && (zLine[-1] == ' ' \|\| zLine[-1] == '\t') ){` |
|      ! 0 | 14599 | `		SyString *pTmp = &pLast->sValue;` |
|      ! 0 | 14600 | `		SyStringFullTrim(pTmp);` |
|      ! 0 | 14601 | `		if( pTmp->nByte == 0 ){` |
|      ! 0 | 14602 | `			SyStringInitFromBuf(pTmp,zLine,nLen);` |
|      ! 0 | 14603 | `		}else{` |
|        - | 14604 | `			/* Update header value length */` |
|      ! 0 | 14605 | `			pTmp->nByte = (sxu32)(&zLine[nLen] - pTmp->zString);` |
|        - | 14606 | `		}` |
|        - | 14607 | `		 /* Simply tell the caller to reset its states and get another line */` |
|      ! 0 | 14608 | `		 return SXERR_CONTINUE;` |
|        - | 14609 | `	 }` |
|        - | 14610 | `	/* Split the header */` |
|      ! 0 | 14611 | `	pName = &pHdr->sName;` |
|      ! 0 | 14612 | `	rc = SyByteFind(zLine,nLen,':',&nPos);` |
|      ! 0 | 14613 | `	if(rc != SXRET_OK ){` |
|      ! 0 | 14614 | `		return SXERR_NEXT; /* Malformed header;Check the next entry */` |
|        - | 14615 | `	}` |
|      ! 0 | 14616 | `	SyStringInitFromBuf(pName,zLine,nPos);` |
|      ! 0 | 14617 | `	SyStringFullTrim(pName);` |
|        - | 14618 | `	/* Extract a header value */` |
|      ! 0 | 14619 | `	SyStringInitFromBuf(&pHdr->sValue,&zLine[nPos + 1],nLen - nPos - 1);` |
|        - | 14620 | `	/* Remove leading and trailing whitespaces */` |
|      ! 0 | 14621 | `	SyStringFullTrim(&pHdr->sValue);` |
|      ! 0 | 14622 | `	return SXRET_OK;` |
|      ! 0 | 14623 | ` }` |
|        - | 14624 | ` /*` |
|        - | 14625 | `  * Extract all MIME headers associated with a HTTP request.` |
|        - | 14626 | `  * After processing the first line of a HTTP request,the following` |
|        - | 14627 | `  * routine is called in order to extract MIME headers.` |
|        - | 14628 | `  * This function return SXRET_OK on success,SXERR_MORE when it needs` |
|        - | 14629 | `  * more inputs.` |
|        - | 14630 | `  * Note: Any malformed header is simply discarded.` |
|        - | 14631 | `  */` |
|      ! 0 | 14632 | ` static sxi32 VmHttpExtractHeaders(SyString *pRequest,SySet *pOut)` |
|      ! 0 | 14633 | ` {` |
|      ! 0 | 14634 | `	 SyhttpHeader *pLast = 0;` |
|        - | 14635 | `	 SyString sCurrent;` |
|        - | 14636 | `	 SyhttpHeader sHdr;` |
|        - | 14637 | `	 sxu8 bEol;` |
|        - | 14638 | `	 sxi32 rc;` |
|      ! 0 | 14639 | `	 if( SySetUsed(pOut) > 0 ){` |
|      ! 0 | 14640 | `		 pLast = (SyhttpHeader *)SySetAt(pOut,SySetUsed(pOut)-1);` |
|      ! 0 | 14641 | `	 }` |
|      ! 0 | 14642 | `	 bEol = FALSE;` |
|      ! 0 | 14643 | `	 for(;;){` |
|      ! 0 | 14644 | `		 SyZero(&sHdr,sizeof(SyhttpHeader));` |
|        - | 14645 | `		 /* Extract a single line from the raw HTTP request */` |
|      ! 0 | 14646 | `		 rc = VmGetNextLine(pRequest,&sCurrent);` |
|      ! 0 | 14647 | `		 if(rc != SXRET_OK ){` |
|      ! 0 | 14648 | `			 if( sCurrent.nByte < 1 ){` |
|      ! 0 | 14649 | `				 break;` |
|        - | 14650 | `			 }` |
|      ! 0 | 14651 | `			 bEol = TRUE;` |
|      ! 0 | 14652 | `		 }` |
|        - | 14653 | `		 /* Process the header */` |
|      ! 0 | 14654 | `		 if( SXRET_OK == VmHttpProcessOneHeader(&sHdr,pLast,sCurrent.zString,sCurrent.nByte)){` |
|      ! 0 | 14655 | `			 if( SXRET_OK != SySetPut(pOut,(const void *)&sHdr) ){` |
|      ! 0 | 14656 | `				 break;` |
|        - | 14657 | `			 }` |
|        - | 14658 | `			 /* Retrieve the last parsed header so we can handle multi-line header` |
|        - | 14659 | `			  * in case we face one of them.` |
|        - | 14660 | `			  */` |
|      ! 0 | 14661 | `			 pLast = (SyhttpHeader *)SySetPeek(pOut);` |
|      ! 0 | 14662 | `		 }` |
|      ! 0 | 14663 | `		 if( bEol ){` |
|      ! 0 | 14664 | `			 break;` |
|        - | 14665 | `		 }` |
|      ! 0 | 14666 | `	 } /* for(;;) */` |
|      ! 0 | 14667 | `	 return SXRET_OK;` |
|      ! 0 | 14668 | ` }` |
|        - | 14669 | ` /*` |
|        - | 14670 | `  * Process the first line of a HTTP request.` |
|        - | 14671 | `  * This routine perform the following operations` |
|        - | 14672 | `  *  1) Extract the HTTP method.` |
|        - | 14673 | `  *  2) Split the request URI to it's fields [ie: host,path,query,...].` |
|        - | 14674 | `  *  3) Extract the HTTP protocol version.` |
|        - | 14675 | `  */` |
|      ! 0 | 14676 | ` static sxi32 VmHttpProcessFirstLine(` |
|        - | 14677 | `	 SyString *pRequest, /* Raw HTTP request */` |
|        - | 14678 | `	 sxi32 *pMethod,     /* OUT: HTTP method */` |
|        - | 14679 | `	 SyhttpUri *pUri,    /* OUT: Parse of the URI */` |
|        - | 14680 | `	 sxi32 *pProto       /* OUT: HTTP protocol */` |
|        - | 14681 | `	 )` |
|      ! 0 | 14682 | ` {` |
|        - | 14683 | `	 static const char *azMethods[] = { "get","post","head","put"};` |
|        - | 14684 | `	 static const sxi32 aMethods[]  = { HTTP_METHOD_GET,HTTP_METHOD_POST,HTTP_METHOD_HEAD,HTTP_METHOD_PUT};` |
|        - | 14685 | `	 const char *zIn,*zEnd,*zPtr;` |
|        - | 14686 | `	 SyString sLine;` |
|        - | 14687 | `	 sxu32 nLen;` |
|        - | 14688 | `	 sxi32 rc;` |
|        - | 14689 | `	 /* Extract the first line and update the pointer */` |
|      ! 0 | 14690 | `	 rc = VmGetNextLine(pRequest,&sLine);` |
|      ! 0 | 14691 | `	 if( rc != SXRET_OK ){` |
|      ! 0 | 14692 | `		 return rc;` |
|        - | 14693 | `	 }` |
|      ! 0 | 14694 | `	 if ( sLine.nByte < 1 ){` |
|        - | 14695 | `		 /* Empty HTTP request */` |
|      ! 0 | 14696 | `		 return SXERR_EMPTY;` |
|        - | 14697 | `	 }` |
|        - | 14698 | `	 /* Delimit the line and ignore trailing and leading white spaces */` |
|      ! 0 | 14699 | `	 zIn = sLine.zString;` |
|      ! 0 | 14700 | `	 zEnd = &zIn[sLine.nByte];` |
|      ! 0 | 14701 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14702 | `		 zIn++;` |
|      ! 0 | 14703 | `	 }` |
|        - | 14704 | `	 /* Extract the HTTP method */` |
|      ! 0 | 14705 | `	 zPtr = zIn;` |
|      ! 0 | 14706 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14707 | `		 zIn++;` |
|      ! 0 | 14708 | `	 }` |
|      ! 0 | 14709 | `	 *pMethod = HTTP_METHOD_OTHR;` |
|      ! 0 | 14710 | `	 if( zIn > zPtr ){` |
|        - | 14711 | `		 sxu32 i;` |
|      ! 0 | 14712 | `		 nLen = (sxu32)(zIn-zPtr);` |
|      ! 0 | 14713 | `		 for( i = 0 ; i < SX_ARRAYSIZE(azMethods) ; ++i ){` |
|      ! 0 | 14714 | `			 if( SyStrnicmp(azMethods[i],zPtr,nLen) == 0 ){` |
|      ! 0 | 14715 | `				 *pMethod = aMethods[i];` |
|      ! 0 | 14716 | `				 break;` |
|        - | 14717 | `			 }` |
|      ! 0 | 14718 | `		 }` |
|      ! 0 | 14719 | `	 }` |
|        - | 14720 | `	 /* Jump trailing white spaces */` |
|      ! 0 | 14721 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14722 | `		 zIn++;` |
|      ! 0 | 14723 | `	 }` |
|        - | 14724 | `	  /* Extract the request URI */` |
|      ! 0 | 14725 | `	 zPtr = zIn;` |
|      ! 0 | 14726 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14727 | `		 zIn++;` |
|      ! 0 | 14728 | `	 }` |
|      ! 0 | 14729 | `	 if( zIn > zPtr ){` |
|      ! 0 | 14730 | `		 nLen = (sxu32)(zIn-zPtr);` |
|        - | 14731 | `		 /* Split raw URI to it's fields */` |
|      ! 0 | 14732 | `		 VmHttpSplitURI(pUri,zPtr,nLen);` |
|      ! 0 | 14733 | `	 }` |
|        - | 14734 | `	 /* Jump trailing white spaces */` |
|      ! 0 | 14735 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14736 | `		 zIn++;` |
|      ! 0 | 14737 | `	 }` |
|        - | 14738 | `	 /* Extract the HTTP version */` |
|      ! 0 | 14739 | `	 zPtr = zIn;` |
|      ! 0 | 14740 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|      ! 0 | 14741 | `		 zIn++;` |
|      ! 0 | 14742 | `	 }` |
|      ! 0 | 14743 | `	 *pProto = HTTP_PROTO_11; /* HTTP/1.1 */` |
|      ! 0 | 14744 | `	 rc = 1;` |
|      ! 0 | 14745 | `	 if( zIn > zPtr ){` |
|      ! 0 | 14746 | `		 rc = SyStrnicmp(zPtr,"http/1.0",(sxu32)(zIn-zPtr));` |
|      ! 0 | 14747 | `	 }` |
|      ! 0 | 14748 | `	 if( !rc ){` |
|      ! 0 | 14749 | `		 *pProto = HTTP_PROTO_10; /* HTTP/1.0 */` |
|      ! 0 | 14750 | `	 }` |
|      ! 0 | 14751 | `	 return SXRET_OK;` |
|      ! 0 | 14752 | ` }` |
|        - | 14753 | ` /*` |
|        - | 14754 | `  * Tokenize,decode and split a raw query encoded as: "x-www-form-urlencoded"` |
|        - | 14755 | `  * into a name value pair.` |
|        - | 14756 | `  * Note that this encoding is implicit in GET based requests.` |
|        - | 14757 | `  * After the tokenization process,register the decoded queries` |
|        - | 14758 | `  * in the $_GET/$_POST/$_REQUEST superglobals arrays.` |
|        - | 14759 | `  */` |
|      ! 0 | 14760 | ` static sxi32 VmHttpSplitEncodedQuery(` |
|        - | 14761 | `	 ph7_vm *pVm,       /* Target VM */` |
|        - | 14762 | `	 SyString *pQuery,  /* Raw query to decode */` |
|        - | 14763 | `	 SyBlob *pWorker,   /* Working buffer */` |
|        - | 14764 | `	 int is_post        /* TRUE if we are dealing with a POST request */` |
|        - | 14765 | `	 )` |
|      ! 0 | 14766 | ` {` |
|      ! 0 | 14767 | `	 const char *zEnd = &pQuery->zString[pQuery->nByte];` |
|      ! 0 | 14768 | `	 const char *zIn = pQuery->zString;` |
|        - | 14769 | `	 ph7_value *pGet,*pRequest;` |
|        - | 14770 | `	 SyString sName,sValue;` |
|        - | 14771 | `	 const char *zPtr;` |
|        - | 14772 | `	 sxu32 nBlobOfft;` |
|        - | 14773 | `	 /* Extract superglobals */` |
|      ! 0 | 14774 | `	 if( is_post ){` |
|        - | 14775 | `		 /* $_POST superglobal */` |
|      ! 0 | 14776 | `		 pGet = VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|      ! 0 | 14777 | `	 }else{` |
|        - | 14778 | `		 /* $_GET superglobal */` |
|      ! 0 | 14779 | `		 pGet = VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|        - | 14780 | `	 }` |
|      ! 0 | 14781 | `	 pRequest = VmExtractSuper(&(*pVm),"_REQUEST",sizeof("_REQUEST")-1);` |
|        - | 14782 | `	 /* Split up the raw query */` |
|      ! 0 | 14783 | `	 for(;;){` |
|        - | 14784 | `		 /* Jump leading white spaces */` |
|      ! 0 | 14785 | `		 while(zIn < zEnd  && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14786 | `			 zIn++;` |
|      ! 0 | 14787 | `		 }` |
|      ! 0 | 14788 | `		 if( zIn >= zEnd ){` |
|      ! 0 | 14789 | `			 break;` |
|        - | 14790 | `		 }` |
|      ! 0 | 14791 | `		 zPtr = zIn;` |
|      ! 0 | 14792 | `		 while( zPtr < zEnd && zPtr[0] != '=' && zPtr[0] != '&' && zPtr[0] != ';' ){` |
|      ! 0 | 14793 | `			 zPtr++;` |
|      ! 0 | 14794 | `		 }` |
|        - | 14795 | `		 /* Reset the working buffer */` |
|      ! 0 | 14796 | `		 SyBlobReset(pWorker);` |
|        - | 14797 | `		 /* Decode the entry */` |
|      ! 0 | 14798 | `		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|        - | 14799 | `		 /* Save the entry */` |
|      ! 0 | 14800 | `		 sName.nByte = SyBlobLength(pWorker);` |
|      ! 0 | 14801 | `		 sValue.zString = 0;` |
|      ! 0 | 14802 | `		 sValue.nByte = 0;` |
|      ! 0 | 14803 | `		 if( zPtr < zEnd && zPtr[0] == '=' ){` |
|      ! 0 | 14804 | `			 zPtr++;` |
|      ! 0 | 14805 | `			 zIn = zPtr;` |
|        - | 14806 | `			 /* Store field value */` |
|      ! 0 | 14807 | `			 while( zPtr < zEnd && zPtr[0] != '&' && zPtr[0] != ';' ){` |
|      ! 0 | 14808 | `				 zPtr++;` |
|      ! 0 | 14809 | `			 }` |
|      ! 0 | 14810 | `			 if( zPtr > zIn ){` |
|        - | 14811 | `				 /* Decode the value */` |
|      ! 0 | 14812 | `				  nBlobOfft = SyBlobLength(pWorker);` |
|      ! 0 | 14813 | `				  SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14814 | `				  sValue.zString = (const char *)SyBlobDataAt(pWorker,nBlobOfft);` |
|      ! 0 | 14815 | `				  sValue.nByte = SyBlobLength(pWorker) - nBlobOfft;` |
|        - | 14816 |  |
|      ! 0 | 14817 | `			 }` |
|        - | 14818 | `			 /* Synchronize pointers */` |
|      ! 0 | 14819 | `			 zIn = zPtr;` |
|      ! 0 | 14820 | `		 }` |
|      ! 0 | 14821 | `		 sName.zString = (const char *)SyBlobData(pWorker);` |
|        - | 14822 | `		 /* Install the decoded query in the $_GET/$_REQUEST array */` |
|      ! 0 | 14823 | `		 if( pGet && (pGet->iFlags & MEMOBJ_HASHMAP) ){` |
|      ! 0 | 14824 | `			 VmHashmapInsert((ph7_hashmap *)pGet->x.pOther,` |
|      ! 0 | 14825 | `				 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14826 | `				 sValue.zString,(int)sValue.nByte` |
|        - | 14827 | `				 );` |
|      ! 0 | 14828 | `		 }` |
|      ! 0 | 14829 | `		 if( pRequest && (pRequest->iFlags & MEMOBJ_HASHMAP) ){` |
|      ! 0 | 14830 | `			 VmHashmapInsert((ph7_hashmap *)pRequest->x.pOther,` |
|      ! 0 | 14831 | `				 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14832 | `				 sValue.zString,(int)sValue.nByte` |
|        - | 14833 | `					 );` |
|      ! 0 | 14834 | `		 }` |
|        - | 14835 | `		 /* Advance the pointer */` |
|      ! 0 | 14836 | `		 zIn = &zPtr[1];` |
|      ! 0 | 14837 | `	 }` |
|        - | 14838 | `	/* All done*/` |
|      ! 0 | 14839 | `	return SXRET_OK;` |
|      ! 0 | 14840 | ` }` |
|        - | 14841 | ` /*` |
|        - | 14842 | `  * Extract MIME header value from the given set.` |
|        - | 14843 | `  * Return header value on success. NULL otherwise.` |
|        - | 14844 | `  */` |
|      ! 0 | 14845 | ` static SyString * VmHttpExtractHeaderValue(SySet *pSet,const char *zMime,sxu32 nByte)` |
|      ! 0 | 14846 | ` {` |
|        - | 14847 | `	 SyhttpHeader *aMime,*pMime;` |
|        - | 14848 | `	 SyString sMime;` |
|        - | 14849 | `	 sxu32 n;` |
|      ! 0 | 14850 | `	 SyStringInitFromBuf(&sMime,zMime,nByte);` |
|        - | 14851 | `	 /* Point to the MIME entries */` |
|      ! 0 | 14852 | `	 aMime = (SyhttpHeader *)SySetBasePtr(pSet);` |
|        - | 14853 | `	 /* Perform the lookup */` |
|      ! 0 | 14854 | `	 for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|      ! 0 | 14855 | `		 pMime = &aMime[n];` |
|      ! 0 | 14856 | `		 if( SyStringCmp(&sMime,&pMime->sName,SyStrnicmp) == 0 ){` |
|        - | 14857 | `			 /* Header found,return it's associated value */` |
|      ! 0 | 14858 | `			 return &pMime->sValue;` |
|        - | 14859 | `		 }` |
|      ! 0 | 14860 | `	 }` |
|        - | 14861 | `	 /* No such MIME header */` |
|      ! 0 | 14862 | `	 return 0;` |
|      ! 0 | 14863 | ` }` |
|        - | 14864 | ` /*` |
|        - | 14865 | `  * Tokenize and decode a raw "Cookie:" MIME header into a name value pair` |
|        - | 14866 | `  * and insert it's fields [i.e name,value] in the $_COOKIE superglobal.` |
|        - | 14867 | `  */` |
|      ! 0 | 14868 | ` static sxi32 VmHttpPorcessCookie(ph7_vm *pVm,SyBlob *pWorker,const char *zIn,sxu32 nByte)` |
|      ! 0 | 14869 | ` {` |
|      ! 0 | 14870 | `	 const char *zPtr,*zDelimiter,*zEnd = &zIn[nByte];` |
|        - | 14871 | `	 SyString sName,sValue;` |
|        - | 14872 | `	 ph7_value *pCookie;` |
|        - | 14873 | `	 sxu32 nOfft;` |
|        - | 14874 | `	 /* Make sure the $_COOKIE superglobal is available */` |
|      ! 0 | 14875 | `	 pCookie = VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 14876 | `	 if( pCookie == 0 \|\| (pCookie->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 14877 | `		 /* $_COOKIE superglobal not available */` |
|      ! 0 | 14878 | `		 return SXERR_NOTFOUND;` |
|        - | 14879 | `	 }` |
|      ! 0 | 14880 | `	 for(;;){` |
|        - | 14881 | `		  /* Jump leading white spaces */` |
|      ! 0 | 14882 | `		 while( zIn < zEnd && SyisSpace(zIn[0]) ){` |
|      ! 0 | 14883 | `			 zIn++;` |
|      ! 0 | 14884 | `		 }` |
|      ! 0 | 14885 | `		 if( zIn >= zEnd ){` |
|      ! 0 | 14886 | `			 break;` |
|        - | 14887 | `		 }` |
|        - | 14888 | `		  /* Reset the working buffer */` |
|      ! 0 | 14889 | `		 SyBlobReset(pWorker);` |
|      ! 0 | 14890 | `		 zDelimiter = zIn;` |
|        - | 14891 | `		 /* Delimit the name[=value]; pair */` |
|      ! 0 | 14892 | `		 while( zDelimiter < zEnd && zDelimiter[0] != ';' ){` |
|      ! 0 | 14893 | `			 zDelimiter++;` |
|      ! 0 | 14894 | `		 }` |
|      ! 0 | 14895 | `		 zPtr = zIn;` |
|      ! 0 | 14896 | `		 while( zPtr < zDelimiter && zPtr[0] != '=' ){` |
|      ! 0 | 14897 | `			 zPtr++;` |
|      ! 0 | 14898 | `		 }` |
|        - | 14899 | `		 /* Decode the cookie */` |
|      ! 0 | 14900 | `		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14901 | `		 sName.nByte = SyBlobLength(pWorker);` |
|      ! 0 | 14902 | `		 zPtr++;` |
|      ! 0 | 14903 | `		 sValue.zString = 0;` |
|      ! 0 | 14904 | `		 sValue.nByte = 0;` |
|      ! 0 | 14905 | `		 if( zPtr < zDelimiter ){` |
|        - | 14906 | `			 /* Got a Cookie value */` |
|      ! 0 | 14907 | `			 nOfft = SyBlobLength(pWorker);` |
|      ! 0 | 14908 | `			 SyUriDecode(zPtr,(sxu32)(zDelimiter-zPtr),PH7_VmBlobConsumer,pWorker,TRUE);` |
|      ! 0 | 14909 | `			 SyStringInitFromBuf(&sValue,SyBlobDataAt(pWorker,nOfft),SyBlobLength(pWorker)-nOfft);` |
|      ! 0 | 14910 | `		 }` |
|        - | 14911 | `		 /* Synchronize pointers */` |
|      ! 0 | 14912 | `		 zIn = &zDelimiter[1];` |
|        - | 14913 | `		 /* Perform the insertion */` |
|      ! 0 | 14914 | `		 sName.zString = (const char *)SyBlobData(pWorker);` |
|      ! 0 | 14915 | `		 VmHashmapInsert((ph7_hashmap *)pCookie->x.pOther,` |
|      ! 0 | 14916 | `			 sName.zString,(int)sName.nByte,` |
|      ! 0 | 14917 | `			 sValue.zString,(int)sValue.nByte` |
|        - | 14918 | `			 );` |
|      ! 0 | 14919 | `	 }` |
|      ! 0 | 14920 | `	 return SXRET_OK;` |
|      ! 0 | 14921 | ` }` |
|        - | 14922 | ` /*` |
|        - | 14923 | `  * Process a full HTTP request and populate the appropriate arrays` |
|        - | 14924 | `  * such as $_SERVER,$_GET,$_POST,$_COOKIE,$_REQUEST,... with the information` |
|        - | 14925 | `  * extracted from the raw HTTP request. As an extension Symisc introduced` |
|        - | 14926 | `  * the $_HEADER array which hold a copy of the processed HTTP MIME headers` |
|        - | 14927 | `  * and their associated values. [i.e: $_HEADER['Server'],$_HEADER['User-Agent'],...].` |
|        - | 14928 | `  * This function return SXRET_OK on success. Any other return value indicates` |
|        - | 14929 | `  * a malformed HTTP request.` |
|        - | 14930 | `  */` |
|      ! 0 | 14931 | ` static sxi32 VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte)` |
|      ! 0 | 14932 | ` {` |
|        - | 14933 | `	 SyString *pName,*pValue,sRequest; /* Raw HTTP request */` |
|        - | 14934 | `	 ph7_value *pHeaderArray;          /* $_HEADER superglobal (Symisc eXtension to the PHP specification)*/` |
|        - | 14935 | `	 SyhttpHeader *pHeader;            /* MIME header */` |
|        - | 14936 | `	 SyhttpUri sUri;     /* Parse of the raw URI*/` |
|        - | 14937 | `	 SyBlob sWorker;     /* General purpose working buffer */` |
|        - | 14938 | `	 SySet sHeader;      /* MIME headers set */` |
|        - | 14939 | `	 sxi32 iMethod;      /* HTTP method [i.e: GET,POST,HEAD...]*/` |
|        - | 14940 | `	 sxi32 iVer;         /* HTTP protocol version */` |
|        - | 14941 | `	 sxi32 rc;` |
|      ! 0 | 14942 | `	 SyStringInitFromBuf(&sRequest,zRequest,nByte);` |
|      ! 0 | 14943 | `	 SySetInit(&sHeader,&pVm->sAllocator,sizeof(SyhttpHeader));` |
|      ! 0 | 14944 | `	 SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        - | 14945 | `	 /* Ignore leading and trailing white spaces*/` |
|      ! 0 | 14946 | `	 SyStringFullTrim(&sRequest);` |
|        - | 14947 | `	 /* Process the first line */` |
|      ! 0 | 14948 | `	 rc = VmHttpProcessFirstLine(&sRequest,&iMethod,&sUri,&iVer);` |
|      ! 0 | 14949 | `	 if( rc != SXRET_OK ){` |
|      ! 0 | 14950 | `		 return rc;` |
|        - | 14951 | `	 }` |
|        - | 14952 | `	 /* Process MIME headers */` |
|      ! 0 | 14953 | `	 VmHttpExtractHeaders(&sRequest,&sHeader);` |
|        - | 14954 | `	 /*` |
|        - | 14955 | `	  * Setup $_SERVER environments` |
|        - | 14956 | `	  */` |
|        - | 14957 | `	 /* 'SERVER_PROTOCOL': Name and revision of the information protocol via which the page was requested */` |
|      ! 0 | 14958 | `	 ph7_vm_config(pVm,` |
|        - | 14959 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14960 | `		 "SERVER_PROTOCOL",` |
|      ! 0 | 14961 | `		 iVer == HTTP_PROTO_10 ? "HTTP/1.0" : "HTTP/1.1",` |
|        - | 14962 | `		 sizeof("HTTP/1.1")-1` |
|        - | 14963 | `		 );` |
|        - | 14964 | `	 /* 'REQUEST_METHOD':  Which request method was used to access the page */` |
|      ! 0 | 14965 | `	 ph7_vm_config(pVm,` |
|        - | 14966 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14967 | `		 "REQUEST_METHOD",` |
|      ! 0 | 14968 | `		 iMethod == HTTP_METHOD_GET ?   "GET" :` |
|      ! 0 | 14969 | `		 (iMethod == HTTP_METHOD_POST ? "POST":` |
|      ! 0 | 14970 | `		 (iMethod == HTTP_METHOD_PUT  ? "PUT" :` |
|      ! 0 | 14971 | `		 (iMethod == HTTP_METHOD_HEAD ?  "HEAD" : "OTHER"))),` |
|        - | 14972 | `		 -1 /* Compute attribute length automatically */` |
|        - | 14973 | `		 );` |
|      ! 0 | 14974 | `	 if( SyStringLength(&sUri.sQuery) > 0 && iMethod == HTTP_METHOD_GET ){` |
|      ! 0 | 14975 | `		 pValue = &sUri.sQuery;` |
|        - | 14976 | `		 /* 'QUERY_STRING': The query string, if any, via which the page was accessed */` |
|      ! 0 | 14977 | `		 ph7_vm_config(pVm,` |
|        - | 14978 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14979 | `			 "QUERY_STRING",` |
|      ! 0 | 14980 | `			 pValue->zString,` |
|      ! 0 | 14981 | `			 pValue->nByte` |
|        - | 14982 | `			 );` |
|        - | 14983 | `		 /* Decoded the raw query */` |
|      ! 0 | 14984 | `		 VmHttpSplitEncodedQuery(&(*pVm),pValue,&sWorker,FALSE);` |
|      ! 0 | 14985 | `	 }` |
|        - | 14986 | `	 /* REQUEST_URI: The URI which was given in order to access this page; for instance, '/index.html' */` |
|      ! 0 | 14987 | `	 pValue = &sUri.sRaw;` |
|      ! 0 | 14988 | `	 ph7_vm_config(pVm,` |
|        - | 14989 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 14990 | `		 "REQUEST_URI",` |
|      ! 0 | 14991 | `		 pValue->zString,` |
|      ! 0 | 14992 | `		 pValue->nByte` |
|        - | 14993 | `		 );` |
|        - | 14994 | `	 /*` |
|        - | 14995 | `	  * 'PATH_INFO'` |
|        - | 14996 | `	  * 'ORIG_PATH_INFO'` |
|        - | 14997 | `      * Contains any client-provided pathname information trailing the actual script filename but preceding` |
|        - | 14998 | `	  * the query string, if available. For instance, if the current script was accessed via the URL` |
|        - | 14999 | `	  * http://www.example.com/php/path_info.php/some/stuff?foo=bar, then $_SERVER['PATH_INFO'] would contain` |
|        - | 15000 | `	  * /some/stuff.` |
|        - | 15001 | `	  */` |
|      ! 0 | 15002 | `	 pValue = &sUri.sPath;` |
|      ! 0 | 15003 | `	 ph7_vm_config(pVm,` |
|        - | 15004 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15005 | `		 "PATH_INFO",` |
|      ! 0 | 15006 | `		 pValue->zString,` |
|      ! 0 | 15007 | `		 pValue->nByte` |
|        - | 15008 | `		 );` |
|      ! 0 | 15009 | `	 ph7_vm_config(pVm,` |
|        - | 15010 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15011 | `		 "ORIG_PATH_INFO",` |
|      ! 0 | 15012 | `		 pValue->zString,` |
|      ! 0 | 15013 | `		 pValue->nByte` |
|        - | 15014 | `		 );` |
|        - | 15015 | `	 /* 'HTTP_ACCEPT': Contents of the Accept: header from the current request, if there is one */` |
|      ! 0 | 15016 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept",sizeof("Accept")-1);` |
|      ! 0 | 15017 | `	 if( pValue ){` |
|      ! 0 | 15018 | `		 ph7_vm_config(pVm,` |
|        - | 15019 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15020 | `			 "HTTP_ACCEPT",` |
|      ! 0 | 15021 | `			 pValue->zString,` |
|      ! 0 | 15022 | `			 pValue->nByte` |
|        - | 15023 | `		 );` |
|      ! 0 | 15024 | `	 }` |
|        - | 15025 | `	 /* 'HTTP_ACCEPT_CHARSET': Contents of the Accept-Charset: header from the current request, if there is one. */` |
|      ! 0 | 15026 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Charset",sizeof("Accept-Charset")-1);` |
|      ! 0 | 15027 | `	 if( pValue ){` |
|      ! 0 | 15028 | `		 ph7_vm_config(pVm,` |
|        - | 15029 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15030 | `			 "HTTP_ACCEPT_CHARSET",` |
|      ! 0 | 15031 | `			 pValue->zString,` |
|      ! 0 | 15032 | `			 pValue->nByte` |
|        - | 15033 | `		 );` |
|      ! 0 | 15034 | `	 }` |
|        - | 15035 | `	 /* 'HTTP_ACCEPT_ENCODING': Contents of the Accept-Encoding: header from the current request, if there is one. */` |
|      ! 0 | 15036 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Encoding",sizeof("Accept-Encoding")-1);` |
|      ! 0 | 15037 | `	 if( pValue ){` |
|      ! 0 | 15038 | `		 ph7_vm_config(pVm,` |
|        - | 15039 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15040 | `			 "HTTP_ACCEPT_ENCODING",` |
|      ! 0 | 15041 | `			 pValue->zString,` |
|      ! 0 | 15042 | `			 pValue->nByte` |
|        - | 15043 | `		 );` |
|      ! 0 | 15044 | `	 }` |
|        - | 15045 | `	  /* 'HTTP_ACCEPT_LANGUAGE': Contents of the Accept-Language: header from the current request, if there is one */` |
|      ! 0 | 15046 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Language",sizeof("Accept-Language")-1);` |
|      ! 0 | 15047 | `	 if( pValue ){` |
|      ! 0 | 15048 | `		 ph7_vm_config(pVm,` |
|        - | 15049 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15050 | `			 "HTTP_ACCEPT_LANGUAGE",` |
|      ! 0 | 15051 | `			 pValue->zString,` |
|      ! 0 | 15052 | `			 pValue->nByte` |
|        - | 15053 | `		 );` |
|      ! 0 | 15054 | `	 }` |
|        - | 15055 | `	 /* 'HTTP_CONNECTION': Contents of the Connection: header from the current request, if there is one. */` |
|      ! 0 | 15056 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Connection",sizeof("Connection")-1);` |
|      ! 0 | 15057 | `	 if( pValue ){` |
|      ! 0 | 15058 | `		 ph7_vm_config(pVm,` |
|        - | 15059 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15060 | `			 "HTTP_CONNECTION",` |
|      ! 0 | 15061 | `			 pValue->zString,` |
|      ! 0 | 15062 | `			 pValue->nByte` |
|        - | 15063 | `		 );` |
|      ! 0 | 15064 | `	 }` |
|        - | 15065 | `	 /* 'HTTP_HOST': Contents of the Host: header from the current request, if there is one. */` |
|      ! 0 | 15066 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Host",sizeof("Host")-1);` |
|      ! 0 | 15067 | `	 if( pValue ){` |
|      ! 0 | 15068 | `		 ph7_vm_config(pVm,` |
|        - | 15069 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15070 | `			 "HTTP_HOST",` |
|      ! 0 | 15071 | `			 pValue->zString,` |
|      ! 0 | 15072 | `			 pValue->nByte` |
|        - | 15073 | `		 );` |
|      ! 0 | 15074 | `	 }` |
|        - | 15075 | `	 /* 'HTTP_REFERER': Contents of the Referer: header from the current request, if there is one. */` |
|      ! 0 | 15076 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Referer",sizeof("Referer")-1);` |
|      ! 0 | 15077 | `	 if( pValue ){` |
|      ! 0 | 15078 | `		 ph7_vm_config(pVm,` |
|        - | 15079 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15080 | `			 "HTTP_REFERER",` |
|      ! 0 | 15081 | `			 pValue->zString,` |
|      ! 0 | 15082 | `			 pValue->nByte` |
|        - | 15083 | `		 );` |
|      ! 0 | 15084 | `	 }` |
|        - | 15085 | `	 /* 'HTTP_USER_AGENT': Contents of the Referer: header from the current request, if there is one. */` |
|      ! 0 | 15086 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"User-Agent",sizeof("User-Agent")-1);` |
|      ! 0 | 15087 | `	 if( pValue ){` |
|      ! 0 | 15088 | `		 ph7_vm_config(pVm,` |
|        - | 15089 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15090 | `			 "HTTP_USER_AGENT",` |
|      ! 0 | 15091 | `			 pValue->zString,` |
|      ! 0 | 15092 | `			 pValue->nByte` |
|        - | 15093 | `		 );` |
|      ! 0 | 15094 | `	 }` |
|        - | 15095 | `	  /* 'PHP_AUTH_DIGEST': When doing Digest HTTP authentication this variable is set to the 'Authorization'` |
|        - | 15096 | `	   * header sent by the client (which you should then use to make the appropriate validation).` |
|        - | 15097 | `	   */` |
|      ! 0 | 15098 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Authorization",sizeof("Authorization")-1);` |
|      ! 0 | 15099 | `	 if( pValue ){` |
|      ! 0 | 15100 | `		 ph7_vm_config(pVm,` |
|        - | 15101 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15102 | `			 "PHP_AUTH_DIGEST",` |
|      ! 0 | 15103 | `			 pValue->zString,` |
|      ! 0 | 15104 | `			 pValue->nByte` |
|        - | 15105 | `		 );` |
|      ! 0 | 15106 | `		 ph7_vm_config(pVm,` |
|        - | 15107 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|        - | 15108 | `			 "PHP_AUTH",` |
|      ! 0 | 15109 | `			 pValue->zString,` |
|      ! 0 | 15110 | `			 pValue->nByte` |
|        - | 15111 | `		 );` |
|      ! 0 | 15112 | `	 }` |
|        - | 15113 | `	 /* Install all clients HTTP headers in the $_HEADER superglobal */` |
|      ! 0 | 15114 | `	 pHeaderArray = VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|        - | 15115 | `	 /* Iterate throw the available MIME headers*/` |
|      ! 0 | 15116 | `	 SySetResetCursor(&sHeader);` |
|      ! 0 | 15117 | `	 pHeader = 0; /* stupid cc warning */` |
|      ! 0 | 15118 | `	 while( SXRET_OK == SySetGetNextEntry(&sHeader,(void **)&pHeader) ){` |
|      ! 0 | 15119 | `		 pName  = &pHeader->sName;` |
|      ! 0 | 15120 | `		 pValue = &pHeader->sValue;` |
|      ! 0 | 15121 | `		 if( pHeaderArray && (pHeaderArray->iFlags & MEMOBJ_HASHMAP)){` |
|        - | 15122 | `			 /* Insert the MIME header and it's associated value */` |
|      ! 0 | 15123 | `			 VmHashmapInsert((ph7_hashmap *)pHeaderArray->x.pOther,` |
|      ! 0 | 15124 | `				 pName->zString,(int)pName->nByte,` |
|      ! 0 | 15125 | `				 pValue->zString,(int)pValue->nByte` |
|        - | 15126 | `				 );` |
|      ! 0 | 15127 | `		 }` |
|      ! 0 | 15128 | `		 if( pName->nByte == sizeof("Cookie")-1 && SyStrnicmp(pName->zString,"Cookie",sizeof("Cookie")-1) == 0` |
|      ! 0 | 15129 | `			 && pValue->nByte > 0){` |
|        - | 15130 | `				 /* Process the name=value pair and insert them in the $_COOKIE superglobal array */` |
|      ! 0 | 15131 | `				 VmHttpPorcessCookie(&(*pVm),&sWorker,pValue->zString,pValue->nByte);` |
|      ! 0 | 15132 | `		 }` |
|      ! 0 | 15133 | `	 }` |
|      ! 0 | 15134 | `	 if( iMethod == HTTP_METHOD_POST ){` |
|        - | 15135 | `		 /* Extract raw POST data */` |
|      ! 0 | 15136 | `		 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Type",sizeof("Content-Type") - 1);` |
|      ! 0 | 15137 | `		 if( pValue && pValue->nByte >= sizeof("application/x-www-form-urlencoded") - 1 &&` |
|      ! 0 | 15138 | `			 SyMemcmp("application/x-www-form-urlencoded",pValue->zString,pValue->nByte) == 0 ){` |
|        - | 15139 | `				 /* Extract POST data length */` |
|      ! 0 | 15140 | `				 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Length",sizeof("Content-Length") - 1);` |
|      ! 0 | 15141 | `				 if( pValue ){` |
|      ! 0 | 15142 | `					 sxi32 iLen = 0; /* POST data length */` |
|      ! 0 | 15143 | `					 SyStrToInt32(pValue->zString,pValue->nByte,(void *)&iLen,0);` |
|      ! 0 | 15144 | `					 if( iLen > 0 ){` |
|        - | 15145 | `						 /* Remove leading and trailing white spaces */` |
|      ! 0 | 15146 | `						 SyStringFullTrim(&sRequest);` |
|      ! 0 | 15147 | `						 if( (int)sRequest.nByte > iLen ){` |
|      ! 0 | 15148 | `							 sRequest.nByte = (sxu32)iLen;` |
|      ! 0 | 15149 | `						 }` |
|        - | 15150 | `						 /* Decode POST data now */` |
|      ! 0 | 15151 | `						 VmHttpSplitEncodedQuery(&(*pVm),&sRequest,&sWorker,TRUE);` |
|      ! 0 | 15152 | `					 }` |
|      ! 0 | 15153 | `				 }` |
|      ! 0 | 15154 | `		 }` |
|      ! 0 | 15155 | `	 }` |
|        - | 15156 | `	 /* All done,clean-up the mess left behind */` |
|      ! 0 | 15157 | `	 SySetRelease(&sHeader);` |
|      ! 0 | 15158 | `	 SyBlobRelease(&sWorker);` |
|      ! 0 | 15159 | `	 return SXRET_OK;` |
|      ! 0 | 15160 | ` }` |
|        - | 15161 |  |
