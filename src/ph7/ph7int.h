/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __PH7INT_H__
#define __PH7INT_H__
/* Internal interface definitions for PH7. */
#define PH7_PRIVATE
#include "ph7.h"

/* Return a human-readable PHP type name for a memory object value. */
PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal);

/* Granular SX library includes */
#include "sxtypes.h"      /* Base types: sxi32, sxu32, sxptr, sxreal, etc. */
#include "sxmacros.h"     /* SyString macros, linked list macros, byte operations */
#include "sxset.h"        /* SySet and SyBlob structures */
#include "sxmem.h"        /* SyMemBackend, SyMemBlock, SyMemHeader */
#include "sxmutex.h"      /* Mutex types and macros */
#include "sxhash.h"       /* Hash functions: SyBinHash, SyStrHash */
#include "sxhashtable.h"  /* SyHash, SyHashEntry structures */
#include "sxrand.h"       /* SyPRNGCtx structure */
#include "sxlex.h"        /* SyLex, SyToken, SyStream structures */
#include "sxfmt.h"        /* Formatting functions */
#include "sxstr.h"        /* String functions */
#include "sxutils.h"      /* Numeric parsing functions */
#include "sxbase64.h"     /* Base64 encode/decode */
#include "sxuri.h"        /* URI encode/decode */
#include "sxtime.h"       /* Time utilities */
#include "sxdigest.h"     /* MD5Context, SHA1Context, digest functions */
#include "sxblowfish.h"   /* bcrypt (Blowfish) password hashing */
#include "sxxml.h"        /* SyXMLParser, XML callbacks */
#include "sxzip.h"        /* SyArchive, SyArchiveEntry */

#ifndef PH7_PI
/* Value of PI */
#define PH7_PI 3.1415926535898
#endif
/* Uncaught/in-flight exception code value. A foreign function (built-in) that
 * propagates a callback-raised exception returns this so the OP_CALL dispatcher
 * unwinds through the nearest try/catch. */
#define PH7_EXCEPTION -255
/*
 * Constants for the largest and smallest possible 64-bit signed integers.
 * These macros are designed to work correctly on both 32-bit and 64-bit
 * compilers.
 */
#ifndef LARGEST_INT64
#define LARGEST_INT64  (0xffffffff|(((sxi64)0x7fffffff)<<32))
#endif
#ifndef SMALLEST_INT64
#define SMALLEST_INT64 (((sxi64)-1) - LARGEST_INT64)
#endif
/* Maximum input size for ph7_compile() in bytes. Override at build time with
 * -DPH7_MAX_INPUT_SIZE=N (e.g. a smaller value for embedded/tiny targets).
 * Runtime override: ph7_config(engine, PH7_CONFIG_MAX_INPUT, n). */
#ifndef PH7_MAX_INPUT_SIZE
#define PH7_MAX_INPUT_SIZE (64u*1024u*1024u)
#endif
/* Forward declaration of private structures */
typedef struct ph7_class_instance ph7_class_instance;
typedef struct ph7_foreach_info   ph7_foreach_info;
typedef struct ph7_foreach_step   ph7_foreach_step;
typedef struct ph7_hashmap_node   ph7_hashmap_node;
typedef struct ph7_hashmap        ph7_hashmap;
typedef struct ph7_class          ph7_class;


/* PH7 private declaration */
/*
 * Memory Objects.
 * Internally, the PH7 virtual machine manipulates nearly all PHP values
 * [i.e: string, int, float, resource, object, bool, null] as ph7_values structures.
 * Each ph7_values struct may cache multiple representations (string, integer etc.)
 * of the same value.
 */
struct ph7_value
{
	ph7_real rVal;      /* Real value */
	union{
		sxi64 iVal;     /* Integer value */
		void *pOther;   /* Other values (Object, Array, Resource, Namespace, etc.) */
	}x;
	sxi32 iFlags;       /* Control flags (see below) */
	ph7_vm *pVm;        /* Virtual machine that own this instance */
	SyBlob sBlob;       /* String values */
	sxu32 nIdx;         /* Index number of this entry in the global object allocator */
};
/* Allowed value types.
 */
#define MEMOBJ_STRING    0x001  /* Memory value is a UTF-8 string */
#define MEMOBJ_INT       0x002  /* Memory value is an integer */
#define MEMOBJ_REAL      0x004  /* Memory value is a real number */
#define MEMOBJ_BOOL      0x008  /* Memory value is a boolean */
#define MEMOBJ_NULL      0x020  /* Memory value is NULL */
#define MEMOBJ_HASHMAP   0x040  /* Memory value is a hashmap aka 'array' in the PHP jargon */
#define MEMOBJ_OBJ       0x080  /* Memory value is an object [i.e: class instance] */
#define MEMOBJ_RES       0x100  /* Memory value is a resource [User private data] */
#define MEMOBJ_VOID      0x200  /* Pseudo-type: function must not return a value */
#define MEMOBJ_REFERENCE 0x400  /* Memory value hold a reference (64-bit index) of another ph7_value */
#define MEMOBJ_AUX_SPREAD 0x800 /* Stack-only marker: this value is a spread source for the next LOAD_MAP */
#define MEMOBJ_NEVER     0x1000 /* Pseudo-type (return-only): never-returning function must not return at all */
/* Mask of all known types */
#define MEMOBJ_ALL (MEMOBJ_STRING|MEMOBJ_INT|MEMOBJ_REAL|MEMOBJ_BOOL|MEMOBJ_NULL|MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES)
/* Scalar variables
 * According to the PHP language reference manual
 *  Scalar variables are those containing an integer, float, string or boolean.
 *  Types array, object and resource are not scalar.
 */
#define MEMOBJ_SCALAR (MEMOBJ_STRING|MEMOBJ_INT|MEMOBJ_REAL|MEMOBJ_BOOL|MEMOBJ_NULL)
#define MEMOBJ_AUX (MEMOBJ_REFERENCE|MEMOBJ_AUX_SPREAD)
/*
 * The following macro clear the current ph7_value type and replace
 * it with the given one.
 */
#define MemObjSetType(OBJ,TYPE) ((OBJ)->iFlags = ((OBJ)->iFlags&~MEMOBJ_ALL)|TYPE)
/*
 * Signed 64-bit arithmetic with overflow detection. PHP promotes an integer
 * operation that overflows sxi64 to a floating-point result, so the executor
 * checks for overflow on every +,-,* (and ++/--) and re-runs the operation in
 * double precision when it trips. GCC/Clang expose the __builtin_*_overflow
 * intrinsics (zero cost, no UB); MSVC lacks them, so we fall back to portable
 * implementations defined in memobj.c. Each macro sets *pR to the wrapped
 * result and evaluates to non-zero on overflow.
 */
#if defined(__GNUC__) || defined(__clang__)
#define PH7_ADD_OVERFLOW64(a,b,pR) __builtin_add_overflow((a),(b),(pR))
#define PH7_SUB_OVERFLOW64(a,b,pR) __builtin_sub_overflow((a),(b),(pR))
#define PH7_MUL_OVERFLOW64(a,b,pR) __builtin_mul_overflow((a),(b),(pR))
#else
PH7_PRIVATE int PH7_AddOverflow64(sxi64 a,sxi64 b,sxi64 *pR);
PH7_PRIVATE int PH7_SubOverflow64(sxi64 a,sxi64 b,sxi64 *pR);
PH7_PRIVATE int PH7_MulOverflow64(sxi64 a,sxi64 b,sxi64 *pR);
#define PH7_ADD_OVERFLOW64(a,b,pR) PH7_AddOverflow64((a),(b),(pR))
#define PH7_SUB_OVERFLOW64(a,b,pR) PH7_SubOverflow64((a),(b),(pR))
#define PH7_MUL_OVERFLOW64(a,b,pR) PH7_MulOverflow64((a),(b),(pR))
#endif
/* ph7_value cast method signature */
typedef sxi32 (*ProcMemObjCast)(ph7_value *);
/* Forward reference */
typedef struct ph7_output_consumer ph7_output_consumer;
typedef struct ph7_user_func ph7_user_func;
typedef struct ph7_conf ph7_conf;
/*
 * An instance of the following structure store the default VM output
 * consumer and it's private data.
 * Client-programs can register their own output consumer callback
 * via the [PH7_VM_CONFIG_OUTPUT] configuration directive.
 * Please refer to the official documentation for more information
 * on how to register an output consumer callback.
 */
struct ph7_output_consumer
{
	ProcConsumer xConsumer; /* VM output consumer routine */
	void *pUserData;        /* Third argument to xConsumer() */
	ProcConsumer xDef;      /* Default output consumer routine */
	void *pDefData;         /* Third argument to xDef() */
};
/*
 * PH7 engine [i.e: ph7 instance] configuration is stored in
 * an instance of the following structure.
 * Please refer to the official documentation for more information
 * on how to configure your ph7 engine instance.
 */
struct ph7_conf
{
	ProcConsumer xErr;   /* Compile-time error consumer callback */
	void *pErrData;      /* Third argument to xErr() */
	SyBlob sErrConsumer; /* Default error consumer */
	ph7_clock xClock;    /* Optional embedder clock [PH7_CONFIG_CLOCK]; NULL => platform default */
	void *pClockData;    /* Third argument to xClock() */
	sxu32 nMaxInput;     /* Per-compile input byte cap [PH7_CONFIG_MAX_INPUT]; 0 = PH7_MAX_INPUT_SIZE */
};
/*
 * Signature of the C function responsible of expanding constant values.
 */
typedef void (*ProcConstant)(ph7_value *,void *);
/*
 * Each registered constant [i.e: __TIME__, __DATE__, PHP_OS, INT_MAX, etc.] is stored
 * in an instance of the following structure.
 * Please refer to the official documentation for more information
 * on how to create/install foreign constants.
 */
typedef struct ph7_constant ph7_constant;
struct ph7_constant
{
	SyString sName;        /* Constant name */
	ProcConstant xExpand;  /* Function responsible of expanding constant value */
	void *pUserData;       /* Last argument to xExpand() */
	SyString sFile;        /* Defining file (aliases the VM-lifetime dup in pVm->aFiles);
	                        * nByte == 0 = unknown/engine constant */
	sxu32 nLine;           /* Declaration line for `const`; 0 for define()/engine */
	sxu8 bUserDefined;     /* 1 when created by user code (const / define()):
	                        * Reflection isInternal()/getFileName() input */
	SySet aAttrs;          /* Declared #[...] attributes (ph7_attribute records) —
	                        * php 8.5 attributes on `const` statements */
};
typedef struct ph7_aux_data ph7_aux_data;
/*
 * Auxiliary data associated with each foreign function is stored
 * in a stack of the following structure.
 * Note that automatic tracked chunks are also stored in an instance
 * of this structure.
 */
struct ph7_aux_data
{
	void *pAuxData; /* Aux data */
};
/* Foreign functions signature */
typedef int (*ProchHostFunction)(ph7_context *,int,ph7_value **);
/*
 * Each installed foreign function is recored in an instance of the following
 * structure.
 * Please refer to the official documentation for more information on how
 * to create/install foreign functions.
 */
struct ph7_user_func
{
	ph7_vm *pVm;              /* VM that own this instance */
	SyString sName;           /* Foreign function name */
	ProchHostFunction xFunc;  /* Implementation of the foreign function */
	void *pUserData;          /* User private data [Refer to the official documentation for more information]*/
	SySet aAux;               /* Stack of auxiliary data [Refer to the official documentation for more information]*/
	sxi16 nMinArg;            /* Minimum required arguments for the PHP-8 ArgumentCountError
	                           * check at the OP_CALL choke point; 0 = no central enforcement
	                           * (the builtin self-validates, or genuinely accepts zero args). */
	sxu8 bAtLeast;            /* 0 -> "expects exactly N", 1 -> "expects at least N" (the
	                           * wording depends on whether the builtin has optional params). */
	const char *zSig;         /* PHP-style parameter list ("string $s, int $o = 0") from the
	                           * static signature table, or NULL: ReflectionFunction input for
	                           * internal functions. Points at static storage — never freed. */
	const char *zRet;         /* Return-type text from the same table, or NULL */
};
/*
 * The 'context' argument for an installable function. A pointer to an
 * instance of this structure is the first argument to the routines used
 * implement the foreign functions.
 */
typedef struct VmCallArgMap VmCallArgMap; /* Forward decl; full definition below. */
struct ph7_context
{
	ph7_user_func *pFunc;   /* Function information. */
	ph7_value *pRet;        /* Return value is stored here. */
	SySet sVar;             /* Container of dynamically allocated ph7_values
							 * [i.e: Garbage collection purposes.]
							 */
	SySet sChunk;           /* Track dynamically allocated chunks [ph7_aux_data instance].
							 * [i.e: Garbage collection purposes.]
							 */
	ph7_vm *pVm;            /* Virtual machine that own this context */
	sxi32 iFlags;           /* Call flags */
	VmCallArgMap *pArgMap;  /* Call-site named-argument map (or 0). Lets a builtin
	                         * such as call_user_func forward its callers' name:
	                         * arguments to the inner callback. */
};
/*
 * Each hashmap entry [i.e: array(4,5,6)] is recorded in an instance
 * of the following structure.
 */
/* Allowed hashmap node key types (iType below) */
#define HASHMAP_INT_NODE   1  /* Node with an int [i.e: 64-bit integer] key */
#define HASHMAP_BLOB_NODE  2  /* Node with a string/BLOB key */
struct ph7_hashmap_node
{
	ph7_hashmap *pMap;     /* Hashmap that own this instance */
	sxi32 iType;           /* Node type */
	union{
		sxi64 iKey;        /* Int key */
		SyBlob sKey;       /* Blob key */
	}xKey;
	sxi32 iFlags;          /* Control flags */
	sxu32 nHash;           /* Key hash value */
	sxu32 nValIdx;         /* Value stored in this node */
	ph7_hashmap_node *pNext,*pPrev;               /* Link to other entries [i.e: linear traversal] */
	ph7_hashmap_node *pNextCollide,*pPrevCollide; /* Collision chain */
};
/*
 * Each active hashmap aka array in the PHP jargon is represented
 * by an instance of the following structure.
 */
struct ph7_hashmap
{
	ph7_vm *pVm;                  /* VM that own this instance */
	ph7_hashmap_node **apBucket;  /* Hash bucket */
	ph7_hashmap_node *pFirst;     /* First inserted entry */
	ph7_hashmap_node *pLast;      /* Last inserted entry */
	ph7_hashmap_node *pCur;       /* Current entry */
	sxu32 nSize;                  /* Bucket size */
	sxu32 nEntry;                 /* Total number of inserted entries */
	sxu32 (*xIntHash)(sxi64);     /* Hash function for int_keys */
	sxu32 (*xBlobHash)(const void *,sxu32); /* Hash function for blob_keys */
	sxi64 iNextIdx;               /* Next available automatically assigned index */
	sxi32 iRef;                   /* Reference count. INVARIANT: the number of
								   * SHARERS for copy-on-write purposes is
								   * iRef minus the by-REFERENCE foreach steps
								   * on pActiveSteps (a by-ref loop iterates
								   * the LIVE map, php semantics) — any future
								   * separate/dup gate must use the discounted
								   * count like PH7_HashmapCowSeparate, never
								   * raw iRef. */
	sxi32 iFlags;                 /* Control flags (see HASHMAP_* below) */
	ph7_foreach_step *pActiveSteps; /* foreach steps currently iterating this map
									 * (per-step cursors — PH7_HashmapUnlinkNode
									 * advances any cursor parked on a dying node,
									 * node link re-arms parked cursors) */
};
/*
 * Hashmap control flags.
 */
#define HASHMAP_COUNTING 0x01 /* Set during recursive count to detect cycles */
#define HASHMAP_DUMPING  0x02 /* Set during var_export recursion to detect cycles */
/* An instance of the following structure is the context
 * for the FOREACH_STEP/FOREACH_INIT VM instructions.
 * Those instructions are used to implement the 'foreach'
 * statement.
 * This structure is made available to these instructions
 * as the P3 operand.
 */
struct ph7_foreach_info
{
	SyString sKey;      /* Key name. Empty otherwise*/
	SyString sValue;    /* Value name */
	sxi32 iFlags;       /* Control flags */
	SySet aStep;        /* Stack of steps [i.e: ph7_foreach_step instance] */
};
struct ph7_foreach_step
{
	sxi32 iFlags;                   /* Control flags (see below) */
	/* Iterate on those values */
	union {
		ph7_hashmap *pMap;          /* Hashmap [i.e: array in the PHP jargon] iteration
									 * Ex: foreach(array(1,2,3) as $key=>$value){}
									 */
		ph7_class_instance *pThis;  /* Class instance [i.e: object] iteration */
	}xIter;
	ph7_class_instance *pOwner;     /* IteratorAggregate: keeps aggregate alive during foreach */
	ph7_hashmap_node *pCursor;      /* Hashmap iteration: this loop's PRIVATE cursor.
									 * php iterates each foreach independently — the map's
									 * shared pCur would make nested loops over one array
									 * rewind each other (infinite loop). */
	struct VmFrame *pFrame;         /* Owning activation's frame (normalized past exception
									 * frames). aStep is per-STATEMENT and shared by every
									 * activation; OP_FOREACH_STEP selects the step whose
									 * pFrame matches the running activation so two suspended
									 * instances of one generator/fiber (or a recursive call)
									 * paused in the same textual foreach cannot clash on
									 * each other's cursor. */
	ph7_foreach_step *pNextActive;  /* Next step on the map's pActiveSteps list */
};
/* Foreach step control flags */
#define PH7_4EACH_STEP_HASHMAP 0x001 /* Hashmap iteration */
#define PH7_4EACH_STEP_OBJECT  0x002 /* Object  iteration */
#define PH7_4EACH_STEP_KEY     0x004 /* Make Key available */
#define PH7_4EACH_STEP_REF     0x008 /* Pass value by reference not copy */
#define PH7_4EACH_STEP_LIST    0x010 /* Value target is list() — destructure */
#define PH7_4EACH_STEP_ITERATOR 0x020 /* Object implements Iterator */
#define PH7_4EACH_STEP_FIRST    0x040 /* First iteration (skip next() call) */
/*
 * Each PH7 engine is identified by an instance of the following structure.
 * Please refer to the official documentation for more information
 * on how to configure your PH7 engine instance.
 */
struct ph7
{
	SyMemBackend sAllocator;     /* Low level memory allocation subsystem */
	const ph7_vfs *pVfs;         /* Underlying Virtual File System */
	ph7_conf xConf;              /* Configuration */
#if defined(PH7_ENABLE_THREADS)
	const SyMutexMethods *pMethods;  /* Mutex methods */
	SyMutex *pMutex;                 /* Per-engine mutex */
#endif
	ph7_vm *pVms;      /* List of active VM */
	sxi32 iVm;         /* Total number of active VM */
	ph7 *pNext,*pPrev; /* List of active engines */
	sxu32 nMagic;      /* Sanity check against misuse */
};
/* Code generation data structures */
typedef sxi32 (*ProcErrorGen)(void *,sxi32,sxu32,const char *,...);
typedef struct ph7_expr_node   ph7_expr_node;
typedef struct ph7_expr_op     ph7_expr_op;
typedef struct ph7_gen_state   ph7_gen_state;
/*
 * Lexer trivia sidecar record: a doc-comment (or, later, an attribute
 * group) captured OUT of the token stream, keyed by the index the NEXT
 * real token receives in the chunk's token set. sText points into the
 * raw script buffer — consumers must duplicate before the buffer dies.
 */
typedef struct ph7_trivia ph7_trivia;
struct ph7_trivia
{
	sxu32 nTokIdx;   /* Index of the next real token in the chunk token set */
	sxu8  iKind;     /* PH7_TRIVIA_* */
	SyString sText;  /* Raw span (docblock includes its delimiters) */
	sxu32 nLine;     /* Line the trivia starts on */
};
#define PH7_TRIVIA_DOC  1 /* A doc-comment: slash-star-star ... star-slash */
#define PH7_TRIVIA_ATTR 2 /* An attribute group: the span between #[ and its ] */
/*
 * One compiled attribute argument: an optional name (named argument) and
 * the constant expression's bytecode, evaluated lazily at
 * ReflectionAttribute::getArguments()/newInstance() time (PHP's
 * lazy-instantiation semantics).
 */
typedef struct ph7_attr_arg ph7_attr_arg;
struct ph7_attr_arg
{
	SyString sName;   /* Named-argument name (duplicated); nByte == 0 = positional */
	SySet aByteCode;  /* Compiled expression, OP_DONE(p1=1) terminated (VmInstr) */
};
/*
 * One #[...] attribute as declared: the compile-time-resolved FQN and its
 * argument list.
 */
typedef struct ph7_attribute ph7_attribute;
struct ph7_attribute
{
	SyString sName;   /* Fully-qualified class name (resolved via use imports /
	                   * current namespace at compile time; duplicated) */
	SySet aArgs;      /* ph7_attr_arg records */
	sxu32 nLine;      /* Line the attribute appears on */
};
typedef struct GenBlock        GenBlock;
typedef sxi32 (*ProcLangConstruct)(ph7_gen_state *);
typedef sxi32 (*ProcNodeConstruct)(ph7_gen_state *,sxi32);
/*
 * Each supported operator [i.e: +, -, ==, *, %, >>, >=, new, etc.] is represented
 * by an instance of the following structure.
 * The PH7 parser does not use any external tools and is 100% handcoded.
 * That is, the PH7 parser is thread-safe ,full reentrant, produce consistant
 * compile-time errrors and at least 7 times faster than the standard PHP parser.
 */
struct ph7_expr_op
{
	SyString sOp;   /* String representation of the operator [i.e: "+","*","=="...] */
	sxi32 iOp;      /* Operator ID */
	sxi32 iPrec;    /* Operator precedence: 1 == Highest */
	sxi32 iAssoc;   /* Operator associativity (either left,right or non-associative) */
	sxi32 iVmOp;    /* VM OP code for this operator [i.e: PH7_OP_EQ,PH7_OP_LT,PH7_OP_MUL...]*/
};
/*
 * Each expression node is parsed out and recorded
 * in an instance of the following structure.
 */
struct ph7_expr_node
{
	const ph7_expr_op *pOp;  /* Operator ID or NULL if literal, constant, variable, function or class method call */
	ph7_expr_node *pLeft;    /* Left expression tree */
	ph7_expr_node *pRight;   /* Right expression tree */
	SyToken *pStart;         /* Stream of tokens that belong to this node */
	SyToken *pEnd;           /* End of token stream */
	sxi32 iFlags;            /* Node construct flags */
	ProcNodeConstruct xCode; /* C routine responsible of compiling this node */
	SySet aNodeArgs;         /* Node arguments. Only used by postfix operators [i.e: function call]*/
	SyString sArgName;       /* Named argument label (empty if positional) */
	ph7_expr_node *pCond;    /* Condition: Only used by the ternary operator '?:' */
};
/* Node Construct flags */
#define EXPR_NODE_PRE_INCR    0x01 /* Pre-icrement/decrement [i.e: ++$i,--$j] node */
#define EXPR_NODE_SPREAD      0x02 /* Argument unpacking: ...$expr */
#define EXPR_NODE_NAMED_ARG   0x04 /* Named argument: name: $expr */
#define EXPR_NODE_PARENS      0x08 /* Root of a parenthesized sub-expression */
#define EXPR_NODE_FCC         0x10 /* First-class callable marker: a lone `...` as the
                                    * whole argument list, e.g. f(...) — wrap the callee
                                    * in a Closure instead of calling it. */
/*
 * A block of instructions is recorded in an instance of the following structure.
 * This structure is used only during compile-time and have no meaning
 * during bytecode execution.
 */
struct GenBlock
{
	ph7_gen_state *pGen;  /* State of the code generator */
	GenBlock *pParent;    /* Upper block or NULL if global */
	sxu32 nFirstInstr;    /* First instruction to execute  */
	sxi32 iFlags;         /* Block control flags (see below) */
	SySet aJumpFix;       /* Jump fixup (JumpFixup instance) */
	void *pUserData;      /* Upper layer private data */
	/* The following two fields are used only when compiling
	 * the 'do..while()' language construct.
	 */
	sxu8 bPostContinue;    /* TRUE when compiling the do..while() statement */
	SySet aPostContFix;    /* Post-continue jump fix */
};
/*
 * Code generator state is remembered in an instance of the following
 * structure. We put the information in this structure and pass around
 * a pointer to this structure, rather than pass around  all of the
 * information separately. This helps reduce the number of  arguments
 * to generator functions.
 * This structure is used only during compile-time and have no meaning
 * during bytecode execution.
 */
struct ph7_gen_state
{
	ph7_vm *pVm;         /* VM that own this instance */
	SyHash hLiteral;     /* Constant string Literals table */
	SyHash hNumLiteral;  /* Numeric literals table */
	SyHash hVar;         /* Collected variable hashtable */
	GenBlock *pCurrent;  /* Current processed block */
	GenBlock sGlobal;    /* Global block */
	ProcConsumer xErr;   /* Error consumer callback */
	void *pErrData;      /* Third argument to xErr() */
	SySet aLabel;        /* Label table */
	SySet aGoto;         /* Gotos table */
	SySet aNullsafeJmp;  /* Pending NULLSAFE_JMP instruction indices (sxu32) */
	SyBlob sWorker;      /* General purpose working buffer */
	SyBlob sErrBuf;      /* Error buffer */
	SyBlob sNamespace;   /* Current namespace path (e.g. "App\\Models") */
	SyHash hUseImports;      /* use imports: short alias -> FQN (classes) */
	SyHash hUseFuncImports;  /* use function imports: short alias -> FQN */
	SyHash hUseConstImports; /* use const imports: short alias -> FQN */
	SyToken *pIn;        /* Current processed token */
	SyToken *pEnd;       /* Last token in the stream */
	sxu32 nErr;          /* Total number of compilation error */
	SyToken *pRawIn;     /* Current processed raw token */
	SyToken *pRawEnd;    /* Last raw token in the stream */
	SySet   *pTokenSet;  /* Token containers */
	sxi8 bStrictTypes;       /* Current file's strict_types mode (0 = weak/unset, 1 = strict) */
	sxi8 bStrictTypesLocked; /* 1 once the current file has emitted any non-declare top-level statement */
	sxi8 bInGenerator;       /* ROOT C: 1 while compiling a generator function body (a yield appears at
	                          * this function's own level). Gates inline try/catch/finally so `yield`
	                          * inside a catch/finally suspends correctly; non-generators keep the
	                          * legacy detached-mini-program path. Saved/restored across nested funcs. */
	SySet aTrivia;       /* Trivia sidecar for the current chunk (ph7_trivia records from the
	                      * main-chunk tokenize calls; reset with the token set) */
	SyString sPendingDoc;/* Docblock immediately preceding the statement being dispatched;
	                      * consumed by the declaration compilers, discarded at the next
	                      * statement boundary (points into the raw script buffer) */
	SySet aPendingAttrs; /* Attribute-group trivia (ph7_trivia) bound to the statement being
	                      * dispatched; unlike docs, PHP requires attributes to be adjacent,
	                      * so this resets at every boundary */
};
/* Forward references */
typedef struct ph7_vm_func_closure_env ph7_vm_func_closure_env;
typedef struct ph7_vm_func_static_var  ph7_vm_func_static_var;
typedef struct ph7_vm_func_arg ph7_vm_func_arg;
typedef struct ph7_vm_func ph7_vm_func;
typedef struct VmFrame VmFrame;
struct VmFrame
{
	VmFrame *pParent; /* Parent frame or NULL if global scope */
	void *pUserData;  /* Upper layer private data associated with this frame */
	ph7_class_instance *pThis; /* Current class instance [i.e: the '$this' variable].NULL otherwise */
	ph7_class *pBoundScope; /* Closure::bindTo/call scope override for private/protected access (Increment 2) */
	SySet sLocal;     /* Local variables container (VmSlot instance) */
	ph7_vm *pVm;      /* VM that own this frame */
	SyHash hVar;      /* Variable hashtable for fast lookup */
	SySet sArg;       /* Function arguments container */
	SySet sRef;       /* Local reference table (VmSlot instance) */
	sxi32 iFlags;     /* Frame configuration flags (See below)*/
	sxu32 iExceptionJump; /* Exception jump destination */
	ph7_value sRet;   /* Deferred catch/finally `return` value targeting THIS body frame */
	int bHasRet;      /* TRUE when sRet holds a live pending return */
	sxu32 nRetGen;    /* Bumped on every sRet write (see VmThrowException finally path) */
	int nActualArgs;  /* Actual call arity (band A #4): how many arguments the CALLER passed,
	                   * stamped by the OP_CALL / generator-fiber install sites; -1 when
	                   * unknown (non-call frames) - func_num_args()/func_get_args() then fall
	                   * back to the installed-formals count. Unlike sArg this excludes
	                   * defaulted params and counts variadic-packed args individually. */
};
#define VM_FRAME_EXCEPTION  0x01 /* Special Exception frame */
#define VM_FRAME_THROW      0x02 /* An exception was thrown */
#define VM_FRAME_CATCH      0x04 /* Catch frame */
/*
 * Suspendable execution context.
 * Used by Fiber and Generator to save/restore execution state.
 */
typedef struct ph7_exec_ctx ph7_exec_ctx;
/* Execution context states */
#define PH7_CTX_STATE_CREATED    0  /* Allocated but never started */
#define PH7_CTX_STATE_RUNNING    1  /* Currently executing */
#define PH7_CTX_STATE_SUSPENDED  2  /* Paused at suspend point */
#define PH7_CTX_STATE_COMPLETED  3  /* Returned normally */
#define PH7_CTX_STATE_CLOSED     4  /* Destroyed */
struct ph7_exec_ctx
{
	ph7_vm *pVm;              /* Owning VM */
	ph7_vm_func *pFunc;       /* The function being executed */
	VmFrame *pFrame;          /* Detached execution frame */
	ph7_value *pStack;        /* Private operand stack */
	sxu32 nStackCap;          /* Its allocated slot count (VmNewOperandStack size); grows
	                           * with pStack when an OP_SPREAD in this body reallocs it */
	sxu32 nStackOrig;         /* The ORIGINAL (ungrown) capacity — fixed at creation and used
	                           * to seed each resume's headroom reference, so a spread inside a
	                           * yield loop can't ratchet the stack up across resumes */
	sxi32 nTos;               /* Saved top-of-stack index */
	sxi32 pc;                 /* Saved program counter (resume point) */
	sxi32 iState;             /* One of PH7_CTX_STATE_* */
	ph7_value sSuspendValue;  /* Value passed out via Fiber::suspend() / yield */
	ph7_value sRetValue;      /* Final return value */
	sxu32 nExceptionBase;     /* Exception-stack depth below this body's own handlers
	                           * (caller depth); refreshed at each resume */
	SySet aSavedException;    /* This body's own exception handlers (ph7_exception*),
	                           * parked here while suspended so a generator/fiber that
	                           * suspends inside a try does not corrupt the caller's
	                           * exception stack */
	SySet aSavedFinally;      /* ROOT C: this body's own pending finally actions
	                           * (VmFinallyAction), parked while suspended so a generator
	                           * that yields inside a finally reached by return/break/rethrow
	                           * does not leave its record on the shared VM stack (where an
	                           * out-of-order-resumed sibling generator would mis-pop it) */
	sxu32 nFinallyBase;       /* aFinallyAction depth below this body's own records */
	SySet aSavedSelf;         /* Stage 4: this coroutine's own aSelf (self::/static::)
	                           * entries, parked while suspended (ph7_class* pointers) */
	sxu32 nSelfBase;          /* aSelf depth below this coroutine's own pushes */
	void *pPrivate;           /* Generator wrapper (ph7_generator*) or NULL for fibers */
	ph7_class_instance *pInjected; /* Generator::throw() inject-at-yield: exception to raise at
	                                * the suspended yield on the next resume, or NULL. One-shot:
	                                * consumed (cleared) by the loop-top inject check. Holds a
	                                * reference for the duration of the resume. */
	sxu8 bClosing;                 /* Set while VmCloseCtx force-drives this suspended generator's
	                                * pending `finally` blocks at destruction (unset / out-of-scope
	                                * / GC before completion). The body-resume entry redirects into
	                                * the innermost open try's finally chain instead of resuming at
	                                * the yield, and OP_YIELD raises PHP's "Cannot yield from finally
	                                * in a force-closed generator". Stays set for the whole close run. */
	/* `yield from` delegation state — per generator instance, so independent
	 * instances never clash (unlike the shared foreach aStep). */
	ph7_value sDelegate;             /* The iterable being delegated (kept alive) */
	ph7_hashmap_node *pDelegateNode; /* Array cursor: next node to read, else 0 */
	sxi32 iDelegateState;            /* 0=inactive, 1=array, 2=iterator, 3=generator */
	/* BYTECODE stage 4: deep Fiber::suspend() record-segment parking. */
	void *pParkedSegment;            /* VmParkedSegment* (opaque here): the trampoline
	                                  * record chain + innermost activation parked when a
	                                  * suspend fires inside a nested PHP call; NULL when
	                                  * suspended at the body level (pc/nTos above suffice) */
	int nBodyExecDepth;              /* pVm->nVmExecDepth of this ctx's body invocation. A
	                                  * suspend at a DEEPER native depth is inside a C->PHP
	                                  * callback (usort comparator, etc.) and cannot park
	                                  * across the native frame — it raises a catchable
	                                  * FiberError instead (the one scoped divergence). */
};
/* Special return code from VmByteCodeExec signaling fiber suspension */
#define PH7_SUSPEND  0x100
/*
 * Generator wrapper around ph7_exec_ctx.
 * Adds yield key tracking on top of the suspendable execution context.
 */
typedef struct ph7_generator ph7_generator;
struct ph7_generator
{
	ph7_exec_ctx *pCtx;       /* Execution context (allocated separately) */
	ph7_value sYieldValue;    /* Last yielded value (for current()) */
	ph7_value sYieldKey;      /* Last yielded key (for key()) */
	sxi64 iImplicitKey;       /* Auto-increment key counter */
};
/*
 * Output control buffer entry.
 */
typedef struct VmObEntry VmObEntry;
struct VmObEntry
{
	ph7_value sCallback; /* User defined callback */
	SyBlob sOB;          /* Output buffer consumer */
};
/*
 * HTTP response header entry.
 * Stored in ph7_vm.aResponseHeaders (a SySet of VmResponseHeader).
 */
typedef struct VmResponseHeader VmResponseHeader;
struct VmResponseHeader
{
	SyString sName;   /* Header name (e.g. "Content-Type"), case-preserving */
	SyString sValue;  /* Header value (e.g. "text/html") */
};
/*
 * Each collected function argument is recorded in an instance
 * of the following structure.
 * Note that as an extension, PH7 implements full type hinting
 * which mean that any function can have it's own signature.
 * Example:
 *      function foo(int $a,string $b,float $c,ClassInstance $d){}
 * This is how the powerful function overloading mechanism is
 * implemented.
 * Note that as an extension, PH7 allow function arguments to have
 * any complex default value associated with them unlike the standard
 * PHP engine.
 * Example:
 *    function foo(int $a = rand() & 1023){}
 *    now, when foo is called without arguments [i.e: foo()] the
 *    $a variable (first parameter) will be set to a random number
 *    between 0 and 1023 inclusive.
 * Refer to the official documentation for more information on this
 * mechanism and other extension introduced by the PH7 engine.
 */
struct ph7_vm_func_arg
{
	SyString sName;      /* Argument name */
	SySet aByteCode;     /* Compiled default value associated with this argument */
	sxu32 nType;         /* Type of this argument [i.e: array, int, string, float, object, etc.] */
	SyString sClass;     /* Class name if the argument expect a class instance [i.e: function foo(BaseClass $bar){} ] */
	sxi32 iFlags;        /* Configuration flags */
	SySet aUnionAlts;    /* Union type alternatives (ph7_type_alt). Empty unless VM_FUNC_ARG_UNION is set. */
	SyString sTypeName;  /* Original type text for error messages, normalized in canonical PHP order */
	sxi32 iPromoteVis;   /* PH7_CLASS_PROT_* when VM_FUNC_ARG_PROMOTED is set */
	SySet aAttrs;        /* Declared #[...] attributes (ph7_attribute records) */
};
/*
 * One alternative within a union type declaration. Used by parameters,
 * return types, and properties when the declaration is `T1|T2|...`,
 * `A&B` (intersection), or `(A&B)|C` (DNF).
 */
typedef struct ph7_type_alt ph7_type_alt;
struct ph7_type_alt
{
	sxu32 nType;     /* MEMOBJ_* bitmask, or SXU32_HIGH for a class/interface alternative */
	SyString sClass; /* Class/interface name when nType == SXU32_HIGH */
	sxu32 nGroup;    /* Intersection-group id: atoms sharing a group are ANDed (A&B),
	                  * distinct groups are ORed. A pure union is one atom per group. */
};
/* Maximum alternatives in one type declaration; bounds the on-stack atom array
 * in the parser and the per-group tally in the enforcer. Larger than any real
 * union/DNF type. */
#define PHL_UNION_MAX_ALTS 32
/*
 * Each static variable is parsed out and remembered in an instance
 * of the following structure.
 * Note that as an extension, PH7 allow static variable have
 * any complex default value associated with them unlike the standard
 * PHP engine.
 * Example:
 *   static $rand_str = 'PH7'.rand_str(3); // Concatenate 'PH7' with
 *                                         // a random three characters(English alphabet)
 *   var_dump($rand_str);
 *   //You should see something like this
 *   string(6 'PH7awt');
 */
struct ph7_vm_func_static_var
{
	SyString sName;   /* Static variable name */
	SySet aByteCode;  /* Compiled initialization expression  */
	sxu32 nIdx;       /* Object index in the global memory object container */
};
/*
 * Each imported variable from the outside closure environnment is recoded
 * in an instance of the following structure.
 */
struct ph7_vm_func_closure_env
{
	SyString sName;   /* Imported variable name */
	int iFlags;       /* Control flags */
	ph7_value sValue; /* Imported variable value */
	sxu32 nIdx;       /* Reference to the bounded variable if passed by reference
					   *[Example:
					   *  $x = 1;
					   *  $closure = function() use (&$x) { ++$x; }
					   *  $closure();
					   *]
					   */
};
/* Function configuration flags */
#define VM_FUNC_ARG_BY_REF   0x001 /* Argument passed by reference */
#define VM_FUNC_ARG_HAS_DEF  0x002 /* Argument has default value associated with it */
#define VM_FUNC_REF_RETURN   0x004 /* Return by reference */
#define VM_FUNC_CLASS_METHOD 0x008 /* VM function is in fact a class method */
#define VM_FUNC_CLOSURE      0x010 /* VM function is a closure */
#define VM_FUNC_ARG_IGNORE   0x020 /* Do not install argument in the current frame */
#define VM_FUNC_GENERATOR    0x040 /* VM function is a generator (contains yield) */
#define VM_FUNC_ARG_VARIADIC 0x080 /* Argument is variadic (...$args) */
#define VM_FUNC_ARG_NULLABLE 0x100 /* Argument type is nullable (?type or T|null) */
#define VM_FUNC_ARG_UNION    0x200 /* Argument has a union type (use aUnionAlts) */
#define VM_FUNC_ARG_PROMOTED 0x400 /* Constructor promoted property (iPromoteVis holds visibility) */
#define VM_FUNC_ARG_READONLY 0x800 /* Promoted property is readonly (PHP 8.1) */
#define VM_FUNC_RETURN_NULLABLE 0x1000 /* Return type is nullable (?T, T|null, A|B|null) — func-level */
#define VM_FUNC_INTERNAL     0x2000 /* Function was defined while compiling a builtin chunk
                                     * (embedded PHP library). Reflection reports it as internal:
                                     * isInternal() true, getFileName() false. */
#define VM_FUNC_STATIC_CL    0x4000 /* Static closure/arrow fn (`static function () {}` /
                                     * `static fn () =>`): no $this auto-capture, bind refused. */
#define VM_FUNC_ARG_PRIV_SET 0x8000  /* Promoted property is private(set) (PHP 8.4) */
#define VM_FUNC_ARG_PROT_SET 0x10000 /* Promoted property is protected(set) (PHP 8.4) */
#define VM_FUNC_HOOK_SET_EXPR 0x20000 /* `set => expr` property hook (PHP 8.4): the dispatcher
                                       * stores the implicit return value into the backing slot */
/* next free bit: 0x40000 */
/*
 * Each user defined function is parsed out and stored in an instance
 * of the following structure.
 * PH7 introduced some powerfull extensions to the PHP 5 programming
 * language like function overloading, type hinting, complex default
 * arguments values and many more.
 * Please refer to the official documentation for more information.
 */
struct ph7_vm_func
{
	SySet aArgs;         /* Expected arguments (ph7_vm_func_arg instance) */
	SySet aStatic;       /* Static variable (ph7_vm_func_static_var instance) */
	SyString sName;      /* Function name */
	SySet aByteCode;     /* Compiled function body */
	SySet aClosureEnv;   /* Closure environment (ph7_vm_func_closure_env instace) */
	sxi32 iFlags;        /* VM function configuration */
	SyString sSignature; /* Function signature used to implement function overloading
						  * (Refer to the official docuemntation for more information
						  *  on this powerfull feature)
						  */
	sxu32 nReturnType;   /* Return type hint (MEMOBJ_* constant, MEMOBJ_VOID, or SXU32_HIGH for class) */
	SyString sReturnClass; /* Class name when nReturnType == SXU32_HIGH */
	SySet aReturnUnion;  /* Return-type union alternatives (ph7_type_alt). Empty unless union return. */
	SyString sReturnTypeName; /* Original return-type text for error messages, in canonical PHP order */
	sxu8 bStrictTypes;   /* 1 if defining file declared strict_types=1 (governs return-value coercion) */
	sxu32 nMaxStack;     /* Cached operand-stack depth for this body (BYTECODE stage 7):
						  * 0 = not yet computed; otherwise the number of slots to allocate
						  * per call (a tight bound from VmComputeMaxStack, or the whole
						  * instruction count when the body is not statically modelable). */
	SySet aAttrs;        /* Declared #[...] attributes (ph7_attribute records) */
	SyString sDoc;       /* Doc-comment immediately preceding the declaration, delimiters
						  * included (duplicated into the VM allocator); nByte == 0 = none,
						  * Reflection getDocComment() then reports false. */
	SyString sFile;      /* Path of the defining file (aliases the VM-lifetime dup in pVm->aFiles).
						  * nByte == 0 when unknown (builtin chunk, eval, direct API compile):
						  * Reflection getFileName() then reports false. */
	sxu32 nLine;         /* Line of the 'function'/'fn' keyword (Reflection getStartLine) */
	sxu32 nEndLine;      /* Line of the closing brace of the body (Reflection getEndLine) */
	void *pUserData;     /* Upper layer private data associated with this instance */
	ph7_vm_func *pNextName; /* Next VM function with the same name as this one */
};
/* Forward reference */
typedef struct ph7_builtin_constant ph7_builtin_constant;
typedef struct ph7_builtin_func ph7_builtin_func;
/*
 * Each built-in foreign function (C function) is stored in an
 * instance of the following structure.
 * Please refer to the official documentation for more information
 * on how to create/install foreign functions.
 */
struct ph7_builtin_func
{
	const char *zName;        /* Function name [i.e: strlen(), rand(), array_merge(), etc.]*/
	ProchHostFunction xFunc;  /* C routine performing the computation */
};
/*
 * Each built-in foreign constant is stored in an instance
 * of the following structure.
 * Please refer to the official documentation for more information
 * on how to create/install foreign constants.
 */
struct ph7_builtin_constant
{
	const char *zName;     /* Constant name */
	ProcConstant xExpand;  /* C routine responsible of expanding constant value*/
};
/* Forward reference */
typedef struct ph7_class_method ph7_class_method;
typedef struct ph7_class_attr   ph7_class_attr;
/*
 * Each class is parsed out and stored in an instance of the following structure.
 * PH7 introduced powerfull extensions to the PHP 5 OO subsystems.
 * Please refer to the official documentation for more information.
 */
struct ph7_class
{
	ph7_class *pBase;     /* Base class if any */
	SyHash hDerived;      /* Derived [child] classes */
	SyString sName;       /* Class full qualified name */
	sxi32 iFlags;         /* Class configuration flags [i.e: final, interface, abstract, etc.]  */
	SyHash hAttr;         /* Class attributes [i.e: variables and constants] */
	SyHash hMethod;       /* Class methods */
	sxu32 nLine;          /* Line number on which this class was declared */
	SySet aInterface;     /* Implemented interface container */
	SySet aTrait;         /* Used trait container */
	ph7_class *pNextName; /* Next class [interface, abstract, etc.] with the same name */
	int bMounted;         /* TRUE if class has been mounted (internal VM state) */
	SyString sFile;       /* Path of the defining file (aliases the VM-lifetime dup in pVm->aFiles).
	                       * nByte == 0 when unknown: Reflection getFileName() reports false. */
	sxu32 nEndLine;       /* Line of the class body's closing brace (Reflection getEndLine) */
	SyString sDoc;        /* Doc-comment preceding the declaration (duplicated; empty = none) */
	SySet aAttrs;         /* Declared #[...] attributes (ph7_attribute records) */
	sxu32 nEnumBacking;   /* Enum backing type: 0 = pure/not an enum, MEMOBJ_INT or MEMOBJ_STRING */
	SySet aEnumCases;     /* Enum cases (ph7_class_attr *) in declaration order. Case singletons
	                       * materialize lazily and INDIVIDUALLY on first access (php 8.1: a broken
	                       * sibling case does not poison a valid one); an unmaterialized case has
	                       * nIdx == SXU32_HIGH. */
};
/* Class configuration flags */
#define PH7_CLASS_FINAL       0x001 /* Class is final [cannot be extended] */
#define PH7_CLASS_INTERFACE   0x002 /* Class is interface */
#define PH7_CLASS_ABSTRACT    0x004 /* Class is abstract */
#define PH7_CLASS_TRAIT       0x008 /* Class is a trait */
#define PH7_CLASS_TRAIT_VISITING 0x010 /* Trait is currently being applied (cycle detection) */
#define PH7_CLASS_READONLY    0x020 /* Class is readonly (PHP 8.2): every declared property is readonly */
#define PH7_CLASS_INTERNAL    0x040 /* Class was defined while compiling a builtin chunk (embedded PHP
                                     * library). Reflection reports it as internal: isInternal() true,
                                     * getFileName() false. */
#define PH7_CLASS_ENUM        0x080 /* Class is an enum (PHP 8.1). Also carries PH7_CLASS_FINAL. */
/* Class attribute/methods/constants protection levels */
#define PH7_CLASS_PROT_PUBLIC     1 /* public */
#define PH7_CLASS_PROT_PROTECTED  2 /* protected */
#define PH7_CLASS_PROT_PRIVATE    3 /* private */
/*
 * each class attribute (variable, constants) is parsed out and stored
 * in an instance of the following structure.
 */
struct ph7_class_attr
{
	SyString sName;      /* Atrribute name */
	sxi32 iFlags;        /* Attribute configuration [i.e: static, variable, constant, etc.] */
	sxi32 iProtection;   /* Protection level [i.e: public, private, protected] */
	SySet aByteCode;     /* Compiled attribute body */
	sxu32 nIdx;          /* Attribute index */
	sxu32 nLine;         /* Line number on which this attribute was defined */
	sxu32 nType;         /* Declared type: MEMOBJ_* bitmask, SXU32_HIGH for class, 0 = untyped */
	SyString sClass;     /* Class/interface name when nType == SXU32_HIGH */
	SyString sTypeName;  /* Original type text for error messages (e.g. "?int", "Foo", "string|int") */
	SySet aUnionAlts;    /* Union alternatives (ph7_type_alt). Empty unless PH7_CLASS_ATTR_UNION is set. */
	ph7_class *pDeclClass; /* Class that originally declared this attribute */
	SyString sDoc;       /* Doc-comment preceding the declaration (duplicated; empty = none) */
	SySet aAttrs;        /* Declared #[...] attributes (ph7_attribute records) */
};
/* Attribute configuration */
#define PH7_CLASS_ATTR_STATIC       0x001  /* Static attribute */
#define PH7_CLASS_ATTR_CONSTANT     0x002  /* Constant attribute */
#define PH7_CLASS_ATTR_ABSTRACT     0x004  /* Abstract method */
#define PH7_CLASS_ATTR_FINAL        0x008  /* Final method */
#define PH7_CLASS_ATTR_TYPED        0x010  /* Property has an explicit declared type */
#define PH7_CLASS_ATTR_NULLABLE     0x020  /* Type allows null (?type prefix or T|null union) */
#define PH7_CLASS_ATTR_UNION        0x040  /* Property has a union type (use aUnionAlts) */
#define PH7_CLASS_ATTR_READONLY     0x080  /* readonly property (PHP 8.1) */
#define PH7_CLASS_ATTR_DYNAMIC      0x100  /* Runtime-added (dynamic) property: the ph7_class_attr is
                                            * instance-owned (synthesized, not class-declared) and must
                                            * be freed when the instance is released. */
#define PH7_CLASS_ATTR_ENUMCASE     0x200  /* Enum case: a class constant whose value is the lazily
                                            * materialized case singleton (aByteCode holds the BACKING
                                            * value expression for backed enums; empty when pure). */
#define PH7_CLASS_ATTR_EVALING      0x400  /* Transient: this constant's initializer is being evaluated
                                            * (on-demand, VmClassConstEvalOnDemand). Re-entry means a
                                            * self-referencing constant — php's catchable Error. */
#define PH7_CLASS_ATTR_PRIVATE_SET  0x800  /* private(set) asymmetric visibility (PHP 8.4): writes
                                            * only from the DECLARING class scope (subclasses excluded) */
#define PH7_CLASS_ATTR_PROTECTED_SET 0x1000 /* protected(set) asymmetric visibility (PHP 8.4): writes
                                            * from the declaring class or a subclass scope */
#define PH7_CLASS_ATTR_PUBLIC_SET   0x2000 /* explicit public(set): behaviorally the default, kept
                                            * for the weaker-than-set check and reflection output */
#define PH7_CLASS_ATTR_HOOK_GET     0x4000 /* property has a `get` hook (PHP 8.4): reads dispatch
                                            * __phl_hook_get_NAME (guard-bypassed inside hooks) */
#define PH7_CLASS_ATTR_HOOK_SET     0x8000 /* property has a `set` hook (PHP 8.4): plain writes
                                            * dispatch __phl_hook_set_NAME */
#define PH7_CLASS_ATTR_HOOK_VIRTUAL 0x10000 /* PHP 8.4 VIRTUAL hooked property: none of its own
                                            * hook bodies references `$this->NAME`, so php gives it
                                            * no backing store — excluded from the raw object
                                            * surfaces (var_dump/(array)/print_r/serialize/
                                            * get_class_vars and the get-dispatching walks when it
                                            * has no get hook), no default allowed, reads without a
                                            * get hook are php's "is write-only" Error. PHL still
                                            * allocates the (null) backing slot; this flag hides it. */
/* next free bit: 0x20000 */
/*
 * Each class method is parsed out and stored in an instance of the following
 * structure.
 * PH7 introduced some powerfull extensions to the PHP 5 programming
 * language like function overloading,type hinting,complex default
 * arguments and many more.
 * Please refer to the official documentation for more information.
 */
struct ph7_class_method
{
	ph7_vm_func sFunc;   /* Compiled method body */
	SyString sVmName;    /* Automatically generated name assigned to this method.
						  * Typically this is "[class_name__method_name@random_string]"
						  */
	sxi32 iProtection;   /* Protection level [i.e: public,private,protected] */
	sxi32 iFlags;        /* Methods configuration */
	sxi32 iCloneDepth;   /* Clone depth [Only used by the magic method __clone ] */
    sxu32 nLine;         /* Line on which this method was defined */
};
/*
 * Each active object (class instance) is represented by an instance of
 * the following structure.
 */
struct ph7_class_instance
{
	ph7_vm *pVm;        /* VM that own this instance */
	ph7_class *pClass;  /* Object is an instance of this class */
	SyHash hAttr;       /* Hashtable of active class members */
	sxi32 iRef;         /* Reference count */
	sxi32 iFlags;       /* Control flags */
	sxu32 nObjId;       /* Per-instance monotonic handle id (from pVm->nNextObjId,
	                     * never reused). Drives spl_object_id/hash + var_dump #N. */
};
/*
 * A single instruction of the virtual machine has an opcode
 * and as many as three operands.
 * Each VM instruction resulting from compiling a PHP script
 * is stored in an instance of the following structure.
 */
typedef struct VmInstr VmInstr;
struct VmInstr
{
	sxu8  iOp; /* Operation to preform */
	sxi32 iP1; /* First operand */
	sxu32 iP2; /* Second operand (Often the jump destination) */
	void *p3;  /* Third operand (Often Upper layer private data) */
};
/*
 * Named-argument metadata attached to PH7_OP_CALL instructions via p3.
 * Also carries the namespace-qualification flag formerly stored as p3=(void*)1.
 */
struct VmCallArgMap
{
	sxu8 bHasNamed;      /* 1 if any argument uses name: syntax */
	sxu8 bIsNamespaced;  /* 1 if compiler namespace-qualified the call */
	sxu8 bStrict;        /* 1 if the call site's file declared strict_types=1 */
	sxu32 nOrigNameLit;  /* Original (unqualified) name-literal index + 1, stored
						  * when the CALL handler namespace-qualified the name so
						  * a following NEW can re-qualify with CLASS imports.
						  * 0 = unset. (Formerly abused OP_CALL's iP2, colliding
						  * with the hasSpread flag: `new N\C(...$args)`.) */
	sxu32 nTotal;        /* Total number of compile-time arguments */
	SyString *aNames;    /* Array of nTotal names. nByte==0 means positional. */
};
/* Each active class instance attribute is represented by an instance
 * of the following structure.
 */
typedef struct VmClassAttr VmClassAttr;
struct VmClassAttr
{
	ph7_class_attr *pAttr; /* Class attribute */
	sxu32 nIdx;            /* Memory object index */
	sxi32 iState;          /* Per-instance state: VM_CLASS_ATTR_UNINIT */
	ph7_class *pOwner;     /* Class that declares this attribute (for error msgs) */
};
#define VM_CLASS_ATTR_UNINIT  0x01 /* Typed property never written (PHP 7.4+); also the
                                    * write-once latch for readonly properties (cleared on
                                    * the first successful write — see VmEnforcePropertyTypeOnStore) */
 /* Forward reference */
typedef struct VmRefObj VmRefObj;
/*
 * Each catch [i.e catch(Exception $e){ } ] block is parsed out and stored
 * in an instance of the following structure.
 */
typedef struct ph7_exception_block ph7_exception_block;
typedef struct ph7_exception ph7_exception;
struct ph7_exception_block
{
	SySet aClasses;  /* Exception class names (SyString instances) for multi-catch */
	SyString sThis;  /* Instance name [i.e: $e..] */
	SySet sByteCode; /* Block compiled instructions (legacy; unused once ROOT C inlining lands) */
	sxu32 iHandlerPc;/* ROOT C: inline PC where this catch body begins (0 = not inlined) */
};
/*
 * Context for the exception mechanism.
 */
struct ph7_exception
{
	ph7_vm *pVm;    /* VM that own this exception */
	SySet sEntry;   /* Compiled 'catch' blocks (ph7_exception_block instance)
				     * container.
					 */
	SySet sFinally; /* Compiled 'finally' block bytecode (legacy; unused once ROOT C inlining lands) */
	int iHasFinally;/* TRUE if a finally block was compiled */
	int iFinallyDone;/* TRUE if the finally block was already executed (legacy VmLocalExec path) */
	int iInlined;   /* ROOT C: TRUE when this try's catch/finally are inlined into the function
					 * bytecode (generator body). FALSE = legacy detached-mini-program path. */
	sxu32 iFinallyPc;/* ROOT C: inline PC where the finally body begins (0 = no finally) */
	sxu32 iEndCatchPc;/* ROOT C: inline PC just after the whole try/catch/finally (normal exit) */
	sxu32 iNextFinallyPc;/* ROOT C: iFinallyPc of the lexically-enclosing try-with-finally in the
					   * same function, or 0 — threads a return/break out through nested finallys */
	int iInCatch;   /* ROOT C: TRUE while a catch body of this try is running (finally still owed) */
	ph7_class_instance *pInflight;/* ROOT C: exception to bind at OP_CATCH / re-raise at END_FINALLY */
	VmFrame *pFrame; /* Frame that trigger the exception */
	sxu32 iLandingPc;/* Post-try landing pad (= OP_LOAD_EXCEPTION's iP2). Mirrors the
					  * exception frame's iExceptionJump but survives that frame's
					  * teardown, so an in-place catch can record where to resume. */
	void *pOwnerInstr;/* Bytecode array (VmInstr*) this try was compiled into. iLandingPc
					   * indexes THIS array; the resume only fires in the exec running it
					   * (distinguishes a mini-program from the body that shares its frame). */
	sxi32 iStackDepth;/* Operand-stack base (0-based TOS index = pTos-pStack, -1 when empty)
					   * captured when this try opened at OP_LOAD_EXCEPTION. Used only by
					   * Generator::throw() inject-at-yield to drain the abandoned
					   * (mid-expression) operand slots back to the try's base before
					   * landing at iLandingPc. */
	ph7_exception *pCompiled;/* BYTECODE stage 2b: NULL on the compiler-owned object; on a
					   * runtime ACTIVATION (clone pushed by OP_LOAD_EXCEPTION) this points
					   * at the compiled origin. Every activation of a lexical try carries
					   * its OWN mutable state (pFrame/iFinallyDone/iInCatch/pInflight/
					   * iStackDepth) — recursion levels no longer share one object, which
					   * ran every level's catch/finally against the deepest frame. */
};
/*
 * ROOT C: a pending non-local exit for an inline `finally` body. When control
 * enters a finally (normal fall-through, a caught/unmatched throw, or a return/
 * break/continue crossing the try), one of these is pushed onto pVm->aFinallyAction;
 * the finally's terminating OP_END_FINALLY pops it and dispatches accordingly. A
 * return/break/continue crossing several nested finallys keeps ONE record on the
 * stack and re-drives it through each finally via ph7_exception.iNextFinallyPc.
 */
#define PH7_FA_FALLTHROUGH 0  /* Resume at iNextPc (post-construct landing) */
#define PH7_FA_RETHROW     1  /* Re-raise pExc after the finally runs */
#define PH7_FA_RETURN      2  /* Return sRet from pTargetBody after the finally chain */
#define PH7_FA_JMP         3  /* Break/continue: resume at iNextPc after the finally chain */
typedef struct VmFinallyAction VmFinallyAction;
struct VmFinallyAction
{
	int eKind;                    /* One of PH7_FA_* */
	sxu32 iNextPc;                /* FALLTHROUGH/JMP: pc (0-based) to resume at in this array */
	ph7_class_instance *pExc;     /* RETHROW: exception to re-raise (holds a ref) */
	ph7_value sRet;               /* RETURN: the value to return (owned) */
	int bHasRetVal;               /* RETURN: TRUE if sRet holds a real value (vs bare `return;`) */
	void *pTargetBody;            /* RETURN: VmFrame* the return materializes on */
	int nCross;                   /* trys still to cross through their finallys (-1 = unbounded,
	                               * for RETURN; a positive count bounds a break/continue to the
	                               * trys between it and its target loop) */
};
/* Forward reference */
typedef struct ph7_case_expr ph7_case_expr;
typedef struct ph7_switch ph7_switch;
/*
 * Each compiled case block in a swicth statement is compiled
 * and stored in an instance of the following structure.
 */
struct ph7_case_expr
{
	SySet aByteCode;   /* Compiled body of the case block */
	sxu32 nStart;      /* First instruction to execute */
};
/*
 * Each compiled switch statement is parsed out and stored
 * in an instance of the following structure.
 */
struct ph7_switch
{
	SySet aCaseExpr;  /* Compile case block */
	sxu32 nOut;       /* First instruction to execute after this statement */
	sxu32 nDefault;   /* First instruction to execute in the default block */
};
/*
 * Each arm of a PHP 8.0 match expression is compiled into
 * an instance of the following structure.
 */
typedef struct ph7_match_arm ph7_match_arm;
typedef struct ph7_match     ph7_match;
struct ph7_match_arm
{
	SySet aConds;   /* SySet of SySet (VmInstr) — one compiled bytecode block per condition value */
	SySet aResult;  /* Compiled bytecode of the arm's result expression */
	int   bDefault; /* 1 if this is the 'default' arm */
};
struct ph7_match
{
	SySet aArms;    /* SySet of ph7_match_arm */
};
/* Assertion flags */
#define PH7_ASSERT_DISABLE    0x01  /* Disable assertion */
#define PH7_ASSERT_WARNING    0x02  /* Deprecated in PHP 8: kept for constant compatibility only */
#define PH7_ASSERT_BAIL       0x04  /* Terminate execution on failed assertions */
#define PH7_ASSERT_QUIET_EVAL 0x08  /* Not used */
#define PH7_ASSERT_CALLBACK   0x10  /* Callback to call on failed assertions */
/*
 * error_log() consumer function signature.
 * Refer to the [PH7_VM_CONFIG_ERR_LOG_HANDLER] configuration directive
 * for more information on how to register an error_log consumer().
 */
typedef void (*ProcErrLog)(const char *,int,const char *,const char *);
/*
 * An instance of the following structure hold the bytecode instructions
 * resulting from compiling a PHP script.
 * This structure contains the complete state of the virtual machine.
 */
/* In-flight magic-accessor guard entry (band A #3a; see vm.c helpers). */
typedef struct VmMagicGuard VmMagicGuard;
struct VmMagicGuard
{
	void *pThis;      /* instance identity */
	sxu32 nNameHash;  /* property-name hash (SyBinHash) */
	sxu8 cKind;       /* accessor kind: 'g' = __get */
};
/* Pending property write-back entry (PHP 8.4 hooks + magic ??=): a LIFO of
 * these (ph7_vm.aHookRmw) carries every write whose dispatch is deferred past
 * OP_MEMBER to a later opcode:
 *   VM_HOOK_PEND_RMW        — read-modify-write on a hooked property: OP_MEMBER
 *                             dispatched the get hook (or read the raw backing
 *                             store when set-only) into a fresh SCRATCH memobj
 *                             slot; the modify op (++/--/compound-assign)
 *                             mutates the scratch and its tail consumes the
 *                             entry (matched by kind + scratch index) to
 *                             dispatch the set hook with the computed value.
 *   VM_HOOK_PEND_COAL_HOOK  — `$o->p ??= v` on a hooked property: the entry is
 *                             consumed by the OP_NULLC_STORE at nPc (matched by
 *                             owner + pc) to dispatch the set hook.
 *   VM_HOOK_PEND_COAL_MAGIC — `$o->p ??= v` on a missing property whose class
 *                             declares __set: consumed the same way, dispatching
 *                             __set(sName, value).
 * The armed window is [nJmpPc, nPc]: an owner fetch outside it means the
 * statement was abandoned (a routed throw) or the ??= short-circuit jump was
 * taken — the entry is dropped, no set dispatch (php: the throw/skip discards
 * the write). LIFO order makes nested arms (a ??= RHS containing further
 * hooked stores or coalesce-assigns, a recursive re-entry through a cast
 * inside a modify op) nest correctly. Each entry owns one instance reference;
 * MAGIC entries own their name blob. */
#define VM_HOOK_PEND_RMW         0
#define VM_HOOK_PEND_COAL_HOOK   1
#define VM_HOOK_PEND_COAL_MAGIC  2
typedef struct VmHookRmw VmHookRmw;
struct VmHookRmw
{
	sxu8 iKind;                 /* VM_HOOK_PEND_* */
	ph7_class_instance *pThis;  /* receiver (owns one reference while pending) */
	ph7_class_attr *pAttr;      /* hooked property (hook kinds; 0 for MAGIC) */
	sxu32 nBackIdx;             /* BACKING slot index (for `set => expr` stores) */
	sxu32 nScratchIdx;          /* RMW: scratch slot the modify op operates on;
	                             * SXU32_HIGH for the coalesce kinds */
	SyBlob sName;               /* COAL_MAGIC: property name copy (entry-owned) */
	void *pOwnerStack;          /* arming activation's operand-stack base (identity;
	                             * a nested exec — even a recursive one over the same
	                             * bytecode — has a different base, so it never drops
	                             * an enclosing activation's pending entry) */
	void *pInstrs;              /* arming activation's bytecode array */
	sxu32 nJmpPc;               /* first pc of the armed window (RMW: == nPc;
	                             * coalesce: the OP_NULLC_JMP right after the arm) */
	sxu32 nPc;                  /* pc of the consuming op (RMW: the modify op;
	                             * coalesce: the OP_NULLC_STORE) */
};
struct ph7_vm
{
	SyMemBackend sAllocator;	/* Memory backend */
#if defined(PH7_ENABLE_THREADS)
	SyMutex *pMutex;           /* Recursive mutex associated with VM. */
#endif
	ph7 *pEngine;               /* Interpreter that own this VM */
	SySet aByteCode;            /* Default bytecode container */
	SySet *pByteContainer;      /* Current bytecode container */
	VmFrame *pFrame;            /* Stack of active frames */
	SyPRNGCtx sPrng;            /* PRNG context */
	SySet aMemObj;              /* Object allocation table */
	SySet aLitObj;              /* Literals allocation table */
	ph7_value *aOps;            /* Operand stack */
	SySet aFreeObj;             /* Stack of free memory objects */
	SyHash hClass;              /* Compiled classes container */
	SyHash hConstant;           /* Host-application and user defined constants container */
	SyHash hHostFunction;       /* Host-application installable functions */
	SyHash hFunction;           /* Compiled functions */
	SyBlob sNamespace;          /* Current namespace (e.g. "App\\Models") */
	SyHash hUseImports;         /* Current use imports: short alias -> FQN (classes) */
	SyHash hUseConstImports;    /* Current use const imports: short alias -> FQN */
	SyHash hSuper;              /* Superglobals hashtable */
	SyHash hPDO;                /* PDO installed drivers */
	SyBlob sConsumer;           /* Default VM consumer [i.e Redirect all VM output to this blob] */
	SyBlob sWorker;             /* General purpose working buffer */
	SyBlob sArgv;               /* $argv[] collector [refer to the [getopt()] implementation for more information] */
	SySet aFiles;               /* Stack of processed files */
	SySet aPaths;               /* Set of import paths */
	SySet aIncluded;            /* Set of included files */
	SySet aOB;                  /* Stackable output buffers */
	SySet aResponseHeaders;     /* HTTP response headers (VmResponseHeader entries) */
	int iResponseStatus;        /* HTTP response status code (default 200) */
	int bHeadersSent;           /* TRUE once non-OB output has been emitted */
	int bHttpContext;           /* TRUE when an HTTP request has been fed (server/CGI mode) */
	int bInlineTryCatch;        /* ROOT C: TRUE once the inline try/catch/finally VM handlers exist,
	                             * enabling the compiler to inline generator-body try/catch (so a
	                             * `yield` in a catch/finally suspends). Default 0 = legacy path. */
	int bCompilingBuiltin;      /* TRUE while the embedded builtin PHP library chunks compile at VM
	                             * init: classes/functions defined then are stamped INTERNAL so
	                             * Reflection reports isInternal() like Zend does for C-level code. */
	int bReflectBypass;         /* Consume-once: the next method OP_CALL skips the visibility
	                             * check (ReflectionMethod::invoke bypasses protection like PHP
	                             * 8.1+). Cleared by the check site; never survives past one call. */
	char zDefTz[68];            /* date_default_timezone_set() identifier, stored verbatim like php
	                             * (default "UTC"; only UTC/GMT are accepted — no tz database) */
	sxu32 nDefTz;               /* zDefTz length in bytes */
	SySet aShutdown;            /* Stack of shutdown user callbacks */
	SySet aAutoload;            /* Stack of spl_autoload callbacks */
	SyHash hAutoloadActive;     /* Classes currently being autoloaded (reentrancy guard) */
	SyHash hTypedSlot;          /* memobj nIdx -> VmClassAttr* for typed property enforcement */
	SySet aException;           /* Stack of loaded exception */
	SySet aFinallyAction;       /* ROOT C: stack of VmFinallyAction — pending action (fallthrough /
	                             * rethrow / return / break-continue) for each inline finally in flight */
	ph7_class_instance *pPendingException; /* Exception deferred past a finally block */
	ph7_class_instance *pInflightException; /* Exception being unwound while a finally runs; a throw from
	                                         * that finally that escapes the finally chains it as $previous
	                                         * (PHP finally-supersede) */
	sxu32 nInflightExcBase;                 /* Exception-stack depth when the in-flight finally started; a throw
	                                         * is "leaving the finally" once the stack unwinds to/below this */
	VmFrame *pResumeFrame;      /* Body frame whose in-place catch consumed the live throw (ROOT B) */
	sxu32 iResumePc;            /* Its post-try landing pad (1-based, as iExceptionJump) */
	void *pResumeInstr;         /* Bytecode array the catching try lives in; resume only in that exec */
	sxi32 iResumeStackDepth;    /* Operand-stack base (0-based TOS index) of the catching try, recorded
	                             * with the resume target. Used only by Generator::throw() inject-at-yield
	                             * to drain abandoned mid-expression operands before landing at iResumePc. */
	/* ROOT C inline redirect: set by VmThrowException when a throw is caught by an INLINE
	 * try (generator body). The throw site checks pInlineInstr==aInstr, drains the operand
	 * stack to iInlineDrain, and jumps to iInlinePc; a mismatch means an outer exec owns it,
	 * so the throw propagates. Separate from the ROOT B fields above (legacy path). */
	void *pInlineInstr;         /* Owner bytecode array of the catching inline try (0 = none) */
	sxu32 iInlinePc;            /* 0-based target pc (iHandlerPc or iFinallyPc) */
	sxi32 iInlineDrain;         /* Operand-stack base to drain to before landing (0-based TOS idx) */
	SySet aMagicGuard;          /* In-flight magic-accessor guard (php's property guard):
	                             * {instance, property-name hash, kind} entries pushed around a
	                             * __get dispatch so a self-recursive read of the same property
	                             * falls back to the undefined-property path instead of looping. */
	ph7_class_instance *pMagicSetThis; /* Pending __set receiver (band A #3b): OP_MEMBER detected a
	                             * plain store to a missing/inaccessible property whose class
	                             * declares __set; the VALUE only exists at the immediately-
	                             * following OP_STORE, which consumes this (with sMagicSetName)
	                             * and dispatches __set($name,$value). Holds a reference;
	                             * one-instruction lifetime by construction. */
	SyBlob sMagicSetName;       /* Pending __set property name (stable copy) */
	ph7_class_instance *pHookSetThis; /* Pending property-hook set receiver (PHP 8.4): OP_MEMBER
	                             * detected a plain store to a hooked property; the following
	                             * OP_STORE consumes this (with pHookSetAttr/nHookSetIdx) and
	                             * dispatches __phl_hook_set_NAME — or throws the read-only
	                             * Error when the property has no set hook. Owns one instance
	                             * reference while armed. */
	ph7_class_attr *pHookSetAttr; /* Pending hook-set property (declared attr; name + flags) */
	sxu32 nHookSetIdx;          /* Pending hook-set BACKING slot index (for `set => expr`) */
	SySet aHookRmw;             /* Pending property-hook read-modify-write write-backs (LIFO;
	                             * VmHookRmw entries — see the struct above ph7_vm). */
	ph7_class_instance *pMagicCallThis; /* Pending __call receiver (band A #3b): OP_MEMBER hit a
	                             * missing method on a class declaring __call/__callStatic and
	                             * redirected the callee to the hidden packing trampoline
	                             * (vm_builtin_magic_call), which consumes this + the class +
	                             * the original name. Holds a reference; NULL for __callStatic. */
	ph7_class *pMagicCallClass; /* Pending __call/__callStatic declaring class */
	ph7_class *pConstEvalClass; /* Transient: class whose constant/property initializer bytecode is
	                             * being evaluated (VmLocalExec has no method frame, so self::/parent::
	                             * inside an initializer resolve through this fallback — consulted by
	                             * PH7_VmPeekDeclaringClass/PH7_VmPeekTopClass when no frame matches). */
	sxi32 nConstEvalDepth;      /* Nesting depth of constant/enum-case initializer evaluations. A
	                             * cycle detected at an inner level (pConstCycleAttr) is thrown only
	                             * when depth returns to 0 — a throw INSIDE an initializer mini-exec
	                             * cannot be routed to a user catch (pre-existing engine restriction),
	                             * so the outermost, opcode-level evaluation raises it instead. */
	ph7_class_attr *pConstCycleAttr;  /* Self-referencing constant detected during evaluation */
	ph7_class *pConstCycleClass;      /* ...and the class it belongs to (for the Error message) */
	SyBlob sMagicCallName;      /* Pending original method name (stable copy) */
	sxi32 nBoundaryRc;          /* C-boundary parked throw status (0 / PH7_EXCEPTION / PH7_ABORT).
	                             * Set by VmBoundaryPark when a PHP callee invoked from a C site
	                             * (magic method, cast hook, __destruct, user callback) raised and
	                             * that C site has no status channel to route it. Consumed once per
	                             * dispatch at the executor's fetch point (and cleared wherever the
	                             * same in-flight throw is landed via VmRecordedResume or the inline
	                             * redirect), so a swallowed throw outlives at most the C remainder
	                             * of one opcode instead of silently resuming execution. */
	SySet aIOstream;            /* Installed IO stream container */
	const ph7_io_stream *pDefStream; /* Default IO stream [i.e: typically this is the 'file://' stream] */
	ph7_value sExec;           /* Compiled script return value [Can be extracted via the PH7_VM_CONFIG_EXEC_VALUE directive]*/
	ph7_value aExceptionCB[2]; /* Installed exception handler callbacks via [set_exception_handler()] */
	ph7_value aErrCB[2];       /* Installed error handler callback via [set_error_handler()] */
	void *pStdin;              /* STDIN IO stream */
	void *pStdout;             /* STDOUT IO stream */
	void *pStderr;             /* STDERR IO stream */
	int bErrReport;            /* TRUE to report all runtime Error/Warning/Notice */
	int nRecursionDepth;       /* Current PHP call depth (OP_CALL frames only) */
	int nMaxDepth;             /* Maximum PHP call depth; 0 == unbounded (the host
	                            * default: PHP frames are heap-bound since the
	                            * iterative executor, so recursion is limited by
	                            * memory like the main PHP engine). Embedders opt in
	                            * via PH7_VM_CONFIG_RECURSION_DEPTH. */
	int nVmExecDepth;          /* Live native VmByteCodeExec activations (C-stack guard;
	                            * see the VmByteCodeExec wrapper in vm.c) */
	int nMaxNativeDepth;       /* Maximum native VmByteCodeExec nesting (mini-programs,
	                            * C->PHP callbacks, ctx start/resume, eval/include) —
	                            * what actually protects the C stack now that PHP
	                            * recursion is iterative. Platform-sized default,
	                            * PH7_VM_CONFIG_NATIVE_DEPTH overrides. */
	void *pIdleCallFrames;     /* Freelist of VmCallFrame nodes (BYTECODE stage 2):
	                            * fixed-size, strictly LIFO per invocation — reusing
	                            * them skips a pool alloc/free round-trip per PHP
	                            * call (the measured trampoline overhead). Backing
	                            * memory is allocator-owned; freed wholesale. */
	void *pIdleOperandStacks;  /* Freelist of recycled operand-stack buffers (BYTECODE
	                            * stage 7): a returning PHP call recycles its (tight-sized)
	                            * operand stack here instead of freeing it, so a same-size
	                            * call (recursion / a hot call loop) reuses it — skipping
	                            * the buffer alloc AND the per-slot init. LIFO, exact-size
	                            * head match, capped length; buffers are plain allocator
	                            * blocks so cold/suspend/abort paths can still raw-free them. */
	int nIdleOperandStacks;    /* Length of pIdleOperandStacks (cap: VM_STACK_POOL_MAX) */
	void *pIdleStackNodes;     /* Freelist of spare VmIdleStack nodes (BYTECODE stage 7b):
	                            * reused across recycle/reuse cycles so a parked buffer's
	                            * wrapper node isn't pool-alloc/freed per call (mirrors
	                            * pIdleCallFrames). Allocator-owned; freed wholesale. */
	int nObDepth;              /* OB depth */
	int nExceptDepth;          /* Exception depth */
	int closure_cnt;           /* Loaded closures counter */
	int json_rc;               /* JSON return status [refer to json_encode()/json_decode()]*/
	sxu32 unique_id;           /* Random number used to generate unique ID [refer to uniqid() for more info]*/
	sxu32 nNextObjId;          /* Next object handle id to hand out (monotonic; reset to 1 per exec
	                            * so a reused VM looks like a fresh process). See ph7_class_instance.nObjId */
	ProcErrLog xErrLog;        /* error_log() consumer [refer to PH7_VM_CONFIG_ERR_LOG_HANDLER] */
	sxu32 nOutputLen;          /* Total number of generated output */
	ph7_output_consumer sVmConsumer; /* Registered output consumer callback */
	int iAssertFlags;          /* Assertion flags */
	ph7_value sAssertCallback; /* Callback to call on failed assertions */
	VmRefObj **apRefObj;       /* Hashtable of referenced object */
	VmRefObj *pRefList;        /* List of referenced memory objects */
	sxu32 nRefSize;            /* apRefObj[] size */
	sxu32 nRefUsed;            /* Total entries in apRefObj[] */
	SySet aSelf;               /* 'self' stack used for static member access [i.e: self::MyConstant] */
	ph7_hashmap *pGlobal;      /* $GLOBALS hashmap */
	sxu32 nGlobalIdx;          /* $GLOBALS index */
	sxu32 nSuperBaseline;      /* SySetUsed(aMemObj) snapshot taken in PH7_VmMakeReady
								* right before the superglobals are created. ph7_vm_reset()
								* releases and truncates aMemObj back to this watermark then
								* rebuilds the per-exec object graph, so a compiled VM can be
								* re-executed (compile-once / execute-many) without state
								* bleed or unbounded heap growth. */
	/* Index of the shared empty-string literal reserved at VM init */
	sxu32 nEmptyStringIdx;
	/* Argument-unpacking capture (PHP 8.1 named-parameter semantics for spreads).
	 * Populated by OP_SPREAD; CALL/NEW derive each call's own arg-count growth from
	 * these runs (VmSpreadOwnExtra) and replay the keys (VmBuildEffectiveArgMap),
	 * then consume this call's runs. See the VmSpreadRun/VmSpreadKey machinery in vm.c. */
	SySet aSpreadRun;          /* VmSpreadRun: one entry per expansion in the current arg list */
	sxu32 nSpreadCallBase;     /* Index into aSpreadRun of the first run owned by the CALL/NEW
	                            * currently dispatching (VmSpreadOwnExtra records it; the replay
	                            * and consume use it instead of an ambiguous pStart scan, which a
	                            * zero-width `...[]` run sharing a nested call's base slot fooled) */
	SySet aSpreadKey;          /* VmSpreadKey: one (off,len) per expanded element, in order */
	SyBlob sSpreadKeyBlob;     /* Backing bytes for the string keys referenced by aSpreadKey */
	SySet aEffArgName;         /* SyString: effective per-actual-slot arg names built at CALL */
	sxi32 iCmpCallbackExc;     /* Set when a comparison callback raised an exception so the
								* driver (usort/uasort/uksort and the array_udiff/
								* array_uintersect families) can abort and propagate
								* PH7_EXCEPTION. */
	sxi32 iExitStatus;         /* Script exit status */
	sxu8 bHaltRequested;       /* Set by exit/die (OP_HALT or the builtin) so the halt
								* cascades out of nested execution units (include/require/
								* eval chunks) instead of hard-exiting the process; the
								* top-level executor then runs shutdown callbacks normally. */
	sxu8 bInReset;             /* Set while ph7_vm_reset() bulk-releases the per-exec
								* object pool. Suppresses user __destruct invocation during
								* that teardown: destructors would run arbitrary PHP against a
								* half-reset VM (reference table already gone, $GLOBALS nulled)
								* and could realloc aMemObj mid-release. PH7 never ran
								* global-scope destructors before (release nuked the arena),
								* so this preserves prior semantics while staying crash-safe.
								* Engine-level instance memory is still reclaimed. */
	ph7_gen_state sCodeGen;    /* Code generator module */
	ph7_exec_ctx *pActiveCtx;  /* Currently executing fiber/generator context (NULL in normal code) */
	ph7_class *pFiberClass;    /* Cached Fiber class pointer for fast dispatch */
	ph7_class *pGeneratorClass; /* Cached Generator class pointer */
	ph7_class *pClosureClass;  /* Cached Closure class pointer (closures are instances of it) */
	ph7_class_instance *pClosureThis; /* Transient: bound $this for a bound PLAIN closure about to be
	                                   * invoked, set by VmClosureUnwrap, consumed (ref transferred) at
	                                   * the OP_CALL user-function frame setup. Owns one reference. */
	ph7_class *pClosureScope; /* Transient: bound $__scope class for the same bound PLAIN closure
	                           * (private/protected visibility override); consumed alongside pClosureThis. */
	ph7_class *pStdClass;      /* Cached stdClass pointer (target of (object) cast + dynamic props) */
	ph7_class *pArrayAccessClass; /* Cached ArrayAccess interface pointer */
	ph7_class *pCountableClass;   /* Cached Countable interface pointer */
	ph7_class *pStringableClass;  /* Cached Stringable interface pointer */
	ph7_class *pJsonSerializableClass; /* Cached JsonSerializable interface pointer */
	ph7_class *pTraversableClass; /* Cached Traversable interface pointer (iterable type check) */
	/* Pending null-coalesce-assign target on an ArrayAccess subscript.
	 * Set by LOAD_IDX iP2=3 when the key is missing on an ArrayAccess
	 * object; consumed by NULLC_STORE so it can dispatch to offsetSet
	 * instead of writing through the (synthetic) pNos->nIdx. NULLC_STORE
	 * always clears it, matched or not. */
	ph7_class_instance *pCoalesceObj;
	ph7_value sCoalesceKey;
	int bCoalesceArmed;
#ifdef PH7_ENABLE_PCRE
	int iPcreLastError;        /* preg_last_error() return value */
#endif
	ph7_vm *pNext,*pPrev;      /* List of active VM's */
	sxu32 nMagic;              /* Sanity check against misuse */
};
/*
 * Allowed value for ph7_vm.nMagic
 */
#define PH7_VM_INIT   0xFADE9512  /* VM correctly initialized */
#define PH7_VM_RUN    0xEA271285  /* VM ready to execute PH7 bytecode */
#define PH7_VM_EXEC   0xCAFE2DAD  /* VM executing PH7 bytecode */
#define PH7_VM_STALE  0xBAD1DEAD  /* Stale VM */
/*
 * Error codes according to the PHP language reference manual.
 */
enum iErrCode
{
	E_ERROR             = 1,   /* Fatal run-time errors. These indicate errors that can not be recovered
							    * from, such as a memory allocation problem. Execution of the script is
							    * halted.
								* The only fatal error under PH7 is an out-of-memory. All others erros
								* even a call to undefined function will not halt script execution.
							    */
	E_WARNING           = 2,   /* Run-time warnings (non-fatal errors). Execution of the script is not halted.  */
	E_PARSE             = 4,   /* Compile-time parse errors. Parse errors should only be generated by the parser.*/
	E_NOTICE            = 8,   /* Run-time notices. Indicate that the script encountered something that could
							    * indicate an error, but could also happen in the normal course of running a script.
							    */
	E_CORE_WARNING      = 16,  /* Fatal errors that occur during PHP's initial startup. This is like an E_ERROR
							    * except it is generated by the core of PHP.
							    */
	E_USER_ERROR        = 256,  /* User-generated error message.*/
	E_USER_WARNING      = 512,  /* User-generated warning message.*/
	E_USER_NOTICE       = 1024, /* User-generated notice message.*/
	E_STRICT            = 2048, /* Enable to have PHP suggest changes to your code which will ensure the best interoperability
								 * and forward compatibility of your code.
								 */
	E_RECOVERABLE_ERROR = 4096, /* Catchable fatal error. It indicates that a probably dangerous error occured, but did not
								 * leave the Engine in an unstable state. If the error is not caught by a user defined handle
								 * the application aborts as it was an E_ERROR.
								 */
	E_DEPRECATED        = 8192, /* Run-time notices. Enable this to receive warnings about code that will not
								 * work in future versions.
								 */
	E_USER_DEPRECATED   = 16384, /* User-generated warning message. */
	E_ALL               = 32767  /* All errors and warnings */
};
/*
 * Each VM instruction resulting from compiling a PHP script is represented
 * by one of the following OP codes.
 * The program consists of a linear sequence of operations. Each operation
 * has an opcode and 3 operands.Operands P1 is an integer.
 * Operand P2 is an unsigned integer and operand P3 is a memory address.
 * Few opcodes use all 3 operands.
 */
enum ph7_vm_op {
  PH7_OP_DONE =   1,   /* Done */
  PH7_OP_HALT,         /* Halt */
  PH7_OP_LOAD,         /* Load memory object */
  PH7_OP_LOADC,        /* Load constant */
  PH7_OP_LOAD_IDX,     /* Load array entry */
  PH7_OP_LOAD_MAP,     /* Load hashmap('array') */
  PH7_OP_LOAD_LIST,    /* Load list */
  PH7_OP_LOAD_CLOSURE, /* Load closure */
  PH7_OP_LOAD_FCC,     /* Load first-class callable: wrap a function/method as a Closure */
  PH7_OP_NOOP,         /* NOOP */
  PH7_OP_JMP,          /* Unconditional jump */
  PH7_OP_JZ,           /* Jump on zero (FALSE jump) */
  PH7_OP_JNZ,          /* Jump on non-zero (TRUE jump) */
  PH7_OP_POP,          /* Stack POP */
  PH7_OP_CAT,          /* Concatenation */
  PH7_OP_CVT_INT,      /* Integer cast */
  PH7_OP_CVT_STR,      /* String cast */
  PH7_OP_CVT_REAL,     /* Float cast */
  PH7_OP_CALL,         /* Function call */
  PH7_OP_UMINUS,       /* Unary minus '-'*/
  PH7_OP_UPLUS,        /* Unary plus '+'*/
  PH7_OP_BITNOT,       /* Bitwise not '~' */
  PH7_OP_LNOT,         /* Logical not '!' */
  PH7_OP_MUL,          /* Multiplication '*' */
  PH7_OP_DIV,          /* Division '/' */
  PH7_OP_MOD,          /* Modulus '%' */
  PH7_OP_POW,          /* Exponentiation '**' */
  PH7_OP_ADD,          /* Add '+' */
  PH7_OP_SUB,          /* Sub '-' */
  PH7_OP_SHL,          /* Left shift '<<' */
  PH7_OP_SHR,          /* Right shift '>>' */
  PH7_OP_LT,           /* Less than '<' */
  PH7_OP_LE,           /* Less or equal '<=' */
  PH7_OP_GT,           /* Greater than '>' */
  PH7_OP_GE,           /* Greater or equal '>=' */
  PH7_OP_SPACESHIP,    /* Spaceship '<=>' */
  PH7_OP_EQ,           /* Equal '==' */
  PH7_OP_NEQ,          /* Not equal '!=' */
  PH7_OP_TEQ,          /* Type equal '===' */
  PH7_OP_TNE,          /* Type not equal '!==' */
  PH7_OP_BAND,         /* Bitwise and '&' */
  PH7_OP_BXOR,         /* Bitwise xor '^' */
  PH7_OP_BOR,          /* Bitwise or '|' */
  PH7_OP_LAND,         /* Logical and '&&','and' */
  PH7_OP_LOR,          /* Logical or  '||','or' */
  PH7_OP_LXOR,         /* Logical xor 'xor' */
  PH7_OP_STORE,        /* Store Object */
  PH7_OP_STORE_IDX,    /* Store indexed object */
  PH7_OP_STORE_IDX_REF,/* Store indexed object by reference */
  PH7_OP_PULL,         /* Stack pull */
  PH7_OP_SWAP,         /* Stack swap */
  PH7_OP_YIELD,        /* Stack yield */
  PH7_OP_YIELD_FROM,   /* Generator delegation (yield from <iterable>) */
  PH7_OP_CVT_BOOL,     /* Boolean cast */
  PH7_OP_CVT_NUMC,     /* Numeric (integer,real or both) type cast */
  PH7_OP_INCR,         /* Increment ++ */
  PH7_OP_DECR,         /* Decrement -- */
  PH7_OP_NEW,          /* new */
  PH7_OP_CLONE,        /* clone */
  PH7_OP_CLONE_APPLY,  /* apply clone() property updates (PHP 8.5) */
  PH7_OP_ADD_STORE,    /* Add and store '+=' */
  PH7_OP_SUB_STORE,    /* Sub and store '-=' */
  PH7_OP_MUL_STORE,    /* Mul and store '*=' */
  PH7_OP_DIV_STORE,    /* Div and store '/=' */
  PH7_OP_MOD_STORE,    /* Mod and store '%=' */
  PH7_OP_POW_STORE,    /* Pow and store '**=' */
  PH7_OP_CAT_STORE,    /* Cat and store '.=' */
  PH7_OP_SHL_STORE,    /* Shift left and store '>>=' */
  PH7_OP_SHR_STORE,    /* Shift right and store '<<=' */
  PH7_OP_BAND_STORE,   /* Bitand and store '&=' */
  PH7_OP_BOR_STORE,    /* Bitor and store '|=' */
  PH7_OP_BXOR_STORE,   /* Bitxor and store '^=' */
  PH7_OP_CONSUME,      /* Consume VM output */
  PH7_OP_LOAD_REF,     /* Load reference */
  PH7_OP_STORE_REF,    /* Store a reference to a variable*/
  PH7_OP_MEMBER,       /* Class member run-time access */
  PH7_OP_UPLINK,       /* Run-Time frame link */
  PH7_OP_CVT_NULL,     /* NULL cast */
  PH7_OP_CVT_ARRAY,    /* Array cast */
  PH7_OP_CVT_OBJ,      /* Object cast */
  PH7_OP_FOREACH_INIT, /* For each init */
  PH7_OP_FOREACH_STEP, /* For each step */
  PH7_OP_IS_A,         /* Instanceof */
  PH7_OP_LOAD_EXCEPTION,/* Load an exception */
  PH7_OP_POP_EXCEPTION, /* POP an exception */
  PH7_OP_THROW,         /* Throw exception */
  PH7_OP_SWITCH,        /* Switch operation */
  PH7_OP_MATCH,         /* Match expression (PHP 8.0) */
  PH7_OP_ERR_CTRL,     /* Error control */
  PH7_OP_DUP,          /* Duplicate top of stack */
  PH7_OP_NSSWITCH,     /* Switch active namespace at runtime */
  PH7_OP_USECONST,     /* Register a use-const import at runtime */
  PH7_OP_NULLC,         /* Null coalescing ?? */
  PH7_OP_NULLC_JMP,     /* Null coalescing assign short-circuit jump */
  PH7_OP_NULLC_STORE,   /* Null coalescing assign store */
  PH7_OP_NULLSAFE_JMP,  /* Nullsafe (?->) short-circuit jump */
  PH7_OP_SPREAD,        /* Mark TOS for argument unpacking (...$arr) */
  PH7_OP_FLAG_SPREAD,   /* Flag TOS as a spread source for the next LOAD_MAP */
  PH7_OP_CATCH,         /* Bind the in-flight exception into a catch variable (ROOT C inline catch) */
  PH7_OP_END_FINALLY,   /* Terminate an inline finally: dispatch the pending action (ROOT C) */
  PH7_OP_SET_FINALLY_RET,/* Seed a pending RETURN and enter the innermost enclosing finally (ROOT C) */
  PH7_OP_SET_FINALLY_JMP /* Seed a pending BREAK/CONTINUE (jump target) and enter a finally (ROOT C) */
};
/* LOADC.iP1 bit flags */
#define PH7_LOADC_EXPAND   0x01 /* Candidate for constant/function/class expansion */
#define PH7_LOADC_ABSOLUTE 0x02 /* Fully-qualified — skip namespace prefixing */
/* MEMBER.iP2 — member-access context. 0=read is the default; the unset/isset/empty modes mirror the
 * array LOAD_IDX context modes so unset()/isset()/empty() on a property behave like on an array elem. */
#define PH7_MEMBER_READ   0 /* attribute read */
#define PH7_MEMBER_METHOD 1 /* method-call preparation */
#define PH7_MEMBER_UNSET  2 /* unset($o->p): remove the property */
#define PH7_MEMBER_ISSET  3 /* isset($o->p): silent on a read-miss */
#define PH7_MEMBER_EMPTY  4 /* empty($o->p): silent on a read-miss */
#define PH7_MEMBER_WRITE  5 /* write-lvalue base ($o->arr[..]=, $o->p??=): auto-create a missing prop */
/* -- END-OF INSTRUCTIONS -- */
/*
 * Expression Operators ID.
 */
enum ph7_expr_id {
	EXPR_OP_NEW = 1,   /* new */
	EXPR_OP_CLONE,     /* clone */
	EXPR_OP_ARROW,     /* -> */
	EXPR_OP_NULLSAFE_ARROW, /* ?-> (PHP 8.0 nullsafe) */
	EXPR_OP_DC,        /* :: */
	EXPR_OP_SUBSCRIPT, /* []: Subscripting */
	EXPR_OP_FUNC_CALL, /* func_call() */
	EXPR_OP_INCR,      /* ++ */
	EXPR_OP_DECR,      /* -- */
	EXPR_OP_BITNOT,    /* ~ */
	EXPR_OP_UMINUS,    /* Unary minus  */
	EXPR_OP_UPLUS,     /* Unary plus */
	EXPR_OP_TYPECAST,  /* Type cast [i.e: (int),(float),(string)...] */
	EXPR_OP_ALT,       /* @ */
	EXPR_OP_INSTOF,    /* instanceof */
	EXPR_OP_LOGNOT,    /* logical not ! */
	EXPR_OP_MUL,       /* Multiplication */
	EXPR_OP_DIV,       /* division */
	EXPR_OP_MOD,       /* Modulus */
	EXPR_OP_POW,       /* Exponentiation ** */
	EXPR_OP_ADD,       /* Addition */
	EXPR_OP_SUB,       /* Substraction */
	EXPR_OP_DOT,       /* Concatenation */
	EXPR_OP_SHL,       /* Left shift */
	EXPR_OP_SHR,       /* Right shift */
	EXPR_OP_LT,        /* Less than */
	EXPR_OP_LE,        /* Less equal */
	EXPR_OP_GT,        /* Greater than */
	EXPR_OP_GE,        /* Greater equal */
	EXPR_OP_SPACESHIP, /* Spaceship <=> */
	EXPR_OP_EQ,        /* Equal == */
	EXPR_OP_NE,        /* Not equal != <> */
	EXPR_OP_TEQ,       /* Type equal === */
	EXPR_OP_TNE,       /* Type not equal !== */
	EXPR_OP_BAND,      /* Biwise and '&' */
	EXPR_OP_REF,       /* Reference operator '&' */
	EXPR_OP_XOR,       /* bitwise xor '^' */
	EXPR_OP_BOR,       /* bitwise or '|' */
	EXPR_OP_LAND,      /* Logical and '&&','and' */
	EXPR_OP_LOR,       /* Logical or  '||','or'*/
	EXPR_OP_LXOR,      /* Logical xor 'xor' */
	EXPR_OP_QUESTY,    /* Ternary operator '?' */
	EXPR_OP_NULLC,     /* Null coalescing '??' */
	EXPR_OP_ASSIGN,    /* Assignment '=' */
	EXPR_OP_ADD_ASSIGN, /* Combined operator: += */
	EXPR_OP_SUB_ASSIGN, /* Combined operator: -= */
	EXPR_OP_MUL_ASSIGN, /* Combined operator: *= */
	EXPR_OP_DIV_ASSIGN, /* Combined operator: /= */
	EXPR_OP_MOD_ASSIGN, /* Combined operator: %= */
	EXPR_OP_POW_ASSIGN, /* Combined operator: **= */
	EXPR_OP_DOT_ASSIGN, /* Combined operator: .= */
	EXPR_OP_AND_ASSIGN, /* Combined operator: &= */
	EXPR_OP_OR_ASSIGN,  /* Combined operator: |= */
	EXPR_OP_XOR_ASSIGN, /* Combined operator: ^= */
	EXPR_OP_SHL_ASSIGN, /* Combined operator: <<= */
	EXPR_OP_SHR_ASSIGN, /* Combined operator: >>= */
	EXPR_OP_NULLC_ASSIGN, /* Combined operator: null coalescing assign */
	EXPR_OP_PIPE,       /* PHP 8.5 pipe operator: |> */
	EXPR_OP_COMMA       /* Comma expression */
};
/*
 * Very high level tokens.
 */
#define PH7_TOKEN_RAW 0x001 /* Raw text [i.e: HTML,XML...] */
#define PH7_TOKEN_PHP 0x002 /* PHP chunk */
/*
 * Lexer token codes
 * The following set of constants are the tokens recognized
 * by the lexer when processing PHP input.
 * Important: Token values MUST BE A POWER OF TWO.
 */
#define PH7_TK_INTEGER   0x0000001  /* Integer */
#define PH7_TK_REAL      0x0000002  /* Real number */
#define PH7_TK_NUM       (PH7_TK_INTEGER|PH7_TK_REAL) /* Numeric token,either integer or real */
#define PH7_TK_KEYWORD   0x0000004 /* Keyword [i.e: while,for,if,foreach...] */
#define PH7_TK_ID        0x0000008 /* Alphanumeric or UTF-8 stream */
#define PH7_TK_DOLLAR    0x0000010 /* '$' Dollar sign */
#define PH7_TK_OP        0x0000020 /* Operator [i.e: +,*,/...] */
#define PH7_TK_OCB       0x0000040 /* Open curly brace'{' */
#define PH7_TK_CCB       0x0000080 /* Closing curly brace'}' */
#define PH7_TK_NSSEP     0x0000100 /* Namespace separator '\' */
#define PH7_TK_LPAREN    0x0000200 /* Left parenthesis '(' */
#define PH7_TK_RPAREN    0x0000400 /* Right parenthesis ')' */
#define PH7_TK_OSB       0x0000800 /* Open square bracket '[' */
#define PH7_TK_CSB       0x0001000 /* Closing square bracket ']' */
#define PH7_TK_DSTR      0x0002000 /* Double quoted string "$str" */
#define PH7_TK_SSTR      0x0004000 /* Single quoted string 'str' */
#define PH7_TK_HEREDOC   0x0008000 /* Heredoc <<< */
#define PH7_TK_NOWDOC    0x0010000 /* Nowdoc <<< */
#define PH7_TK_COMMA     0x0020000 /* Comma ',' */
#define PH7_TK_SEMI      0x0040000 /* Semi-colon ";" */
#define PH7_TK_BSTR      0x0080000 /* Backtick quoted string [i.e: Shell command `date`] */
#define PH7_TK_COLON     0x0100000 /* single Colon ':' */
#define PH7_TK_AMPER     0x0200000 /* Ampersand '&' */
#define PH7_TK_EQUAL     0x0400000 /* Equal '=' */
#define PH7_TK_ARRAY_OP  0x0800000 /* Array operator '=>' */
#define PH7_TK_ELLIPSIS  0x1000000 /* Ellipsis '...' */
#define PH7_TK_OTHER     0x2000000 /* Other symbols */
/*
 * PHP keyword.
 * These words have special meaning in PHP. Some of them represent things which look like
 * functions, some look like constants, and so on, but they're not, really: they are language constructs.
 * You cannot use any of the following words as constants, class names, function or method names.
 * Using them as variable names is generally OK, but could lead to confusion.
 */
#define PH7_TKWRD_EXTENDS      1 /* extends */
#define PH7_TKWRD_ENDSWITCH    2 /* endswitch */
#define PH7_TKWRD_SWITCH       3 /* switch */
#define PH7_TKWRD_PRINT        4 /* print */
#define PH7_TKWRD_INTERFACE    5 /* interface */
#define PH7_TKWRD_ENDDEC       6 /* enddeclare */
#define PH7_TKWRD_DECLARE      7 /* declare */
/* The number '8' is reserved for PH7_TK_ID */
#define PH7_TKWRD_REQONCE      9 /* require_once */
#define PH7_TKWRD_REQUIRE      10 /* require */
#define PH7_TKWRD_ELIF         0x4000000 /* elseif: MUST BE A POWER OF TWO */
#define PH7_TKWRD_ELSE         0x8000000 /* else:  MUST BE A POWER OF TWO */
#define PH7_TKWRD_IF           13 /* if */
#define PH7_TKWRD_FINAL        14 /* final */
#define PH7_TKWRD_LIST         15 /* list */
#define PH7_TKWRD_STATIC       16 /* static */
#define PH7_TKWRD_CASE         17 /* case */
#define PH7_TKWRD_SELF         18 /* self */
#define PH7_TKWRD_FUNCTION     19 /* function */
#define PH7_TKWRD_NAMESPACE    20 /* namespace */
#define PH7_TKWRD_ENDIF        0x400000 /* endif: MUST BE A POWER OF TWO */
#define PH7_TKWRD_CLONE        0x80 /* clone: MUST BE A POWER OF TWO  */
#define PH7_TKWRD_NEW          0x100 /* new: MUST BE A POWER OF TWO  */
#define PH7_TKWRD_CONST        22 /* const */
#define PH7_TKWRD_THROW        23 /* throw */
#define PH7_TKWRD_USE          24 /* use */
#define PH7_TKWRD_ENDWHILE     0x800000 /* endwhile: MUST BE A POWER OF TWO */
#define PH7_TKWRD_WHILE        26 /* while */
#define PH7_TKWRD_EVAL         27 /* eval */
#define PH7_TKWRD_VAR          28 /* var */
#define PH7_TKWRD_ARRAY        0x200 /* array: MUST BE A POWER OF TWO */
#define PH7_TKWRD_ABSTRACT     29 /* abstract */
#define PH7_TKWRD_TRY          30 /* try */
#define PH7_TKWRD_AND          0x400 /* and: MUST BE A POWER OF TWO  */
#define PH7_TKWRD_DEFAULT      31 /* default */
#define PH7_TKWRD_CLASS        32 /* class */
#define PH7_TKWRD_AS           33 /* as */
#define PH7_TKWRD_CONTINUE     34 /* continue */
#define PH7_TKWRD_EXIT         35 /* exit */
#define PH7_TKWRD_DIE          36 /* die */
#define PH7_TKWRD_ECHO         37 /* echo */
#define PH7_TKWRD_GLOBAL       38 /* global */
#define PH7_TKWRD_IMPLEMENTS   39 /* implements */
#define PH7_TKWRD_INCONCE      40 /* include_once */
#define PH7_TKWRD_INCLUDE      41 /* include */
#define PH7_TKWRD_EMPTY        42 /* empty */
#define PH7_TKWRD_INSTANCEOF   0x800 /* instanceof: MUST BE A POWER OF TWO  */
#define PH7_TKWRD_ISSET        43 /* isset */
#define PH7_TKWRD_PARENT       44 /* parent */
#define PH7_TKWRD_PRIVATE      45 /* private */
#define PH7_TKWRD_ENDFOR       0x1000000 /* endfor: MUST BE A POWER OF TWO */
#define PH7_TKWRD_END4EACH     0x2000000 /* endforeach: MUST BE A POWER OF TWO */
#define PH7_TKWRD_FOR          48 /* for */
#define PH7_TKWRD_FOREACH      49 /* foreach */
#define PH7_TKWRD_OR           0x1000 /* or: MUST BE A POWER OF TWO  */
#define PH7_TKWRD_PROTECTED    50 /* protected */
#define PH7_TKWRD_DO           51 /* do */
#define PH7_TKWRD_PUBLIC       52 /* public */
#define PH7_TKWRD_CATCH        53 /* catch */
#define PH7_TKWRD_RETURN       54 /* return */
#define PH7_TKWRD_UNSET        0x2000 /* unset: MUST BE A POWER OF TWO  */
#define PH7_TKWRD_XOR          0x4000 /* xor: MUST BE A POWER OF TWO  */
#define PH7_TKWRD_BREAK        55 /* break */
#define PH7_TKWRD_GOTO         56 /* goto */
#define PH7_TKWRD_TRAIT        57 /* trait */
#define PH7_TKWRD_INSTEADOF    58 /* insteadof */
#define PH7_TKWRD_FINALLY      59 /* finally */
#define PH7_TKWRD_YIELD        60 /* yield */
#define PH7_TKWRD_FN           61 /* fn (PHP 7.4 arrow function) */
#define PH7_TKWRD_MATCH        62 /* match (PHP 8.0 match expression) */
#define PH7_TKWRD_BOOL         0x8000  /* bool:  MUST BE A POWER OF TWO */
#define PH7_TKWRD_INT          0x10000  /* int:   MUST BE A POWER OF TWO */
#define PH7_TKWRD_FLOAT        0x20000  /* float:  MUST BE A POWER OF TWO */
#define PH7_TKWRD_STRING       0x40000  /* string: MUST BE A POWER OF TWO */
#define PH7_TKWRD_OBJECT       0x80000 /* object: MUST BE A POWER OF TWO */
/* 0x100000 and 0x200000 are free: they were the PH7-ism 'eq'/'ne' string
 * comparison operators, removed so both stay usable as plain identifiers. */
/*
 * PHP-exact ENT_* flag values for the html-entity family. Single source of
 * truth: constant.c declares the PHP-visible ENT_* constants from these and
 * builtin.c implements the semantics against them. The low two bits are the
 * quote bits (ENT_QUOTES = both, ENT_COMPAT = double only, ENT_NOQUOTES = 0)
 * and bits 16|32 select the doctype — composites, not independent flags.
 */
#define PH7_ENT_QUOTE_SINGLE 0x01 /* encode/decode ' */
#define PH7_ENT_QUOTE_DOUBLE 0x02 /* encode/decode " (== ENT_COMPAT) */
#define PH7_ENT_QUOTES       (PH7_ENT_QUOTE_DOUBLE|PH7_ENT_QUOTE_SINGLE)
#define PH7_ENT_IGNORE       0x04 /* drop invalid UTF-8 units */
#define PH7_ENT_SUBSTITUTE   0x08 /* invalid UTF-8 unit -> U+FFFD */
#define PH7_ENT_DOC_MASK     0x30 /* doctype selector */
#define PH7_ENT_DOC_HTML401  0x00
#define PH7_ENT_DOC_XML1     0x10
#define PH7_ENT_DOC_XHTML    0x20
#define PH7_ENT_DOC_HTML5    0x30
#define PH7_ENT_DISALLOWED   0x80 /* substitute doctype-disallowed codepoints */
/* The shared default for all five builtins (php 8.1+): ENT_QUOTES|ENT_SUBSTITUTE|ENT_HTML401. */
#define PH7_ENT_DEFAULT      (PH7_ENT_QUOTES|PH7_ENT_SUBSTITUTE|PH7_ENT_DOC_HTML401)
/* JSON encoding/decoding related definition */
enum json_err_code{
	JSON_ERROR_NONE = 0,  /* No error has occurred. */
	JSON_ERROR_DEPTH,     /* The maximum stack depth has been exceeded.  */
	JSON_ERROR_STATE_MISMATCH, /* Occurs with underflow or with the modes mismatch.  */
	JSON_ERROR_CTRL_CHAR, /* Control character error, possibly incorrectly encoded.  */
	JSON_ERROR_SYNTAX,    /* Syntax error. */
	JSON_ERROR_UTF8,      /* Malformed UTF-8 characters */
	JSON_ERROR_NON_BACKED_ENUM = 11 /* Non-backed enum given to json_encode (php 8.1 value) */
};
/* The following constants can be combined to form options for json_encode(). */
#define	JSON_HEX_TAG           0x01  /* All < and > are converted to \u003C and \u003E. */
#define JSON_HEX_AMP           0x02  /* All &s are converted to \u0026. */
#define JSON_HEX_APOS          0x04  /* All ' are converted to \u0027. */
#define JSON_HEX_QUOT          0x08  /* All " are converted to \u0022. */
#define JSON_FORCE_OBJECT      0x10  /* Outputs an object rather than an array */
#define JSON_NUMERIC_CHECK     0x20  /* Encodes numeric strings as numbers. */
#define JSON_BIGINT_AS_STRING  0x40  /* Not used */
#define JSON_PRETTY_PRINT      0x80  /* Use whitespace in returned data to format it.*/
#define JSON_UNESCAPED_SLASHES 0x100 /* Don't escape '/' */
#define JSON_UNESCAPED_UNICODE 0x200 /* Not used */
/*
 * Each parsed URI is recorded and stored in an instance of the following structure.
 */
typedef struct SyhttpUri SyhttpUri;
struct SyhttpUri
{
	SyString sHost;     /* Hostname or IP address */
	SyString sPort;     /* Port number */
	SyString sPath;     /* Mandatory resource path passed verbatim (Not decoded) */
	SyString sQuery;    /* Query part */
	SyString sFragment; /* Fragment part */
	SyString sScheme;   /* Scheme */
	SyString sUser;     /* Username */
	SyString sPass;     /* Password */
	SyString sRaw;      /* Raw URI */
};
/*
 * An instance of the following structure is used to record all MIME headers seen
 * during a HTTP interaction.
 */
typedef struct SyhttpHeader SyhttpHeader;
struct SyhttpHeader
{
	SyString sName;    /* Header name [i.e:"Content-Type","Host","User-Agent"]. NOT NUL TERMINATED */
	SyString sValue;   /* Header values [i.e: "text/html"]. NOT NUL TERMINATED */
};
/*
 * Supported HTTP methods.
 */
#define HTTP_METHOD_GET  1 /* GET */
#define HTTP_METHOD_HEAD 2 /* HEAD */
#define HTTP_METHOD_POST 3 /* POST */
#define HTTP_METHOD_PUT  4 /* PUT */
#define HTTP_METHOD_OTHR 5 /* Other HTTP methods [i.e: DELETE,TRACE,OPTIONS...]*/
/*
 * Supported HTTP protocol version.
 */
#define HTTP_PROTO_10 1 /* HTTP/1.0 */
#define HTTP_PROTO_11 2 /* HTTP/1.1 */
/*
 * XML engine handler IDs and structure.
 */
#ifndef PH7_DISABLE_BUILTIN_FUNC
enum ph7_xml_handler_id{
	PH7_XML_START_TAG = 0, /* Start element handlers ID */
	PH7_XML_END_TAG,       /* End element handler ID*/
	PH7_XML_CDATA,         /* Character data handler ID*/
	PH7_XML_PI,            /* Processing instruction (PI) handler ID*/
	PH7_XML_DEF,           /* Default handler ID */
	PH7_XML_UNPED,         /* Unparsed entity declaration handler */
	PH7_XML_ND,            /* Notation declaration handler ID*/
	PH7_XML_EER,           /* External entity reference handler */
	PH7_XML_NS_START,      /* Start namespace declaration handler */
	PH7_XML_NS_END         /* End namespace declaration handler */
};
#define XML_TOTAL_HANDLER (PH7_XML_NS_END + 1)
typedef struct ph7_xml_engine ph7_xml_engine;
struct ph7_xml_engine
{
	ph7_vm *pVm;         /* VM that own this instance */
	ph7_context *pCtx;   /* Call context */
	SyXMLParser sParser; /* Underlying XML parser */
	ph7_value aCB[XML_TOTAL_HANDLER]; /* User-defined callbacks */
	ph7_value sParserValue; /* ph7_value holding this instance which is forwarded
							  * as the first argument to the user callbacks.
							  */
	int ns_sep;      /* Namespace separator */
	SyBlob sErr;     /* Error message consumer */
	sxi32 iErrCode;  /* Last error code */
	sxi32 iNest;     /* Nesting level */
	sxu32 nLine;     /* Last processed line */
	sxu32 nMagic;    /* Magic number so that we avoid misuse  */
};
#define XML_ENGINE_MAGIC 0x851EFC52
#define IS_INVALID_XML_ENGINE(XML) (XML == 0 || (XML)->nMagic != XML_ENGINE_MAGIC)
#endif /* PH7_DISABLE_BUILTIN_FUNC */
/* memobj.c function prototypes */
PH7_PRIVATE sxi32 PH7_MemObjDump(SyBlob *pOut,ph7_value *pObj,int ShowType,int nTab,int nDepth,int isRef);
PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal);
PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore);
PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest);
PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal);
PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray);
PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal);
PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal);
PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal);
PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen);
#if 0
/* Not used in the current release of the PH7 engine */
PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap);
#endif
PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest);
PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest);
PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj);
PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags);
PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue);
PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj);
PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pData);
/* lex.c function prototypes */
PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut);
PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut,SySet *pTrivia);
/* vm.c function prototypes */
PH7_PRIVATE void PH7_VmReleaseContextValue(ph7_context *pCtx,ph7_value *pValue);
PH7_PRIVATE sxi32 PH7_VmInitFuncState(ph7_vm *pVm,ph7_vm_func *pFunc,const char *zName,sxu32 nByte,
	sxi32 iFlags,void *pUserData);
PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(ph7_vm *pVm,ph7_vm_func *pFunc,SyString *pName);
PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(ph7_vm *pVm,ph7_class_instance *pObj);
PH7_PRIVATE ph7_value * PH7_VmCreateDynamicAttr(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nName,VmClassAttr **ppAttr);
PH7_PRIVATE sxi32 PH7_VmRefObjRemove(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry,ph7_hashmap_node *pMapEntry);
PH7_PRIVATE sxi32 PH7_VmRefObjInstall(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry,ph7_hashmap_node *pMapEntry,sxi32 iFlags);
PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew);
PH7_PRIVATE ph7_class * PH7_VmExtractClass(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable,sxi32 iNest);
PH7_PRIVATE sxi32 PH7_VmMaterializeClassConst(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);
PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable);
PH7_PRIVATE sxi32 PH7_VmRegisterConstant(ph7_vm *pVm,const SyString *pName,ProcConstant xExpand,void *pUserData);
PH7_PRIVATE sxi32 PH7_VmRegisterConstantEx(ph7_vm *pVm,const SyString *pName,ProcConstant xExpand,
	void *pUserData,const SyString *pFile,sxu32 nLine,int bUser);
PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(ph7_vm *pVm,const SyString *pName,ProchHostFunction xFunc,void *pUserData);
PH7_PRIVATE sxi32 PH7_VmInstallClass(ph7_vm *pVm,ph7_class *pClass);
PH7_PRIVATE sxi32 PH7_VmBlobConsumer(const void *pSrc,unsigned int nLen,void *pUserData);
PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm);
PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex);
PH7_PRIVATE sxi32 PH7_VmOutputConsume(ph7_vm *pVm,SyString *pString);
PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(ph7_vm *pVm,const char *zFormat,va_list ap);
PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap);
PH7_PRIVATE sxi32 PH7_VmThrowError(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zMessage);
PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx);
PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...);
PH7_PRIVATE sxi32 PH7_VmThrowArrayNextIndexError(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_VmThrowGlobalsAppendError(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_VmInstallGlobalVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue,sxu32 nRefIdx);
PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...);
PH7_PRIVATE void  PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData);
PH7_PRIVATE sxi32 PH7_VmDump(ph7_vm *pVm,ProcConsumer xConsumer,void *pUserData);
PH7_PRIVATE sxi32 PH7_VmEvalBuiltinChunk(ph7_vm *pVm,const char *zSrc,sxu32 nLen);
PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm);
PH7_PRIVATE const char * PH7_VmBuiltinSigLookup(const char *zName,sxu32 nLen,const char **pzRet);
PH7_PRIVATE void PH7_VmStoreArgByRef(ph7_vm *pVm,ph7_value *pArg,ph7_value *pNewVal);
PH7_PRIVATE void PH7_VmThrowDeprecatedFmt(ph7_vm *pVm,const char *zFmt,...);
PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm); /* vm_builtin_reflection.c */
PH7_PRIVATE ph7_class_instance * PH7_VmNewClosure(ph7_vm *pVm,const SyString *pName,
	ph7_class_instance *pBoundThis,const SyString *pScope);
PH7_PRIVATE sxi32 PH7_VmEnforcePropStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue);
PH7_PRIVATE int PH7_VmSlotRefCount(ph7_vm *pVm,sxu32 nIdx);
PH7_PRIVATE sxi32 PH7_VmInit(ph7_vm *pVm,ph7 *pEngine);
PH7_PRIVATE sxi32 PH7_VmConfigure(ph7_vm *pVm,sxi32 nOp,va_list ap);
PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm);
/* Fiber API helpers (used by api.c) */
PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal);
PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult);
PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult);
PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber);
PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber);
PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber);
PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_VmMakeReady(ph7_vm *pVm);
PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm);
PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm);
PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm);
PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm);
PH7_PRIVATE VmInstr *PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex);
PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer);
PH7_PRIVATE sxi32 PH7_VmEmitInstr(ph7_vm *pVm,sxi32 iOp,sxi32 iP1,sxu32 iP2,void *p3,sxu32 *pIndex);
PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm);
PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm,VmClassAttr *pVmAttr);
PH7_PRIVATE sxi32 PH7_VmCallClassMethod(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_method *pMethod,
	ph7_value *pResult,int nArg,ph7_value **apArg);
PH7_PRIVATE sxi32 PH7_VmCallClassMethodMap(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_method *pMethod,
	ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap);
PH7_PRIVATE sxi32 PH7_VmCallUserFunction(ph7_vm *pVm,ph7_value *pFunc,int nArg,ph7_value **apArg,ph7_value *pResult);
PH7_PRIVATE sxi32 PH7_VmCallUserFunctionWithMap(ph7_vm *pVm,ph7_value *pFunc,int nArg,ph7_value **apArg,ph7_value *pResult,VmCallArgMap *pArgMap);
/* Per-element callback for PH7_VmIteratorWalk: return SXRET_OK to continue,
 * SXERR_EOF to stop early (not an error), or PH7_EXCEPTION/PH7_ABORT to propagate. */
typedef sxi32 (*ProcIterStep)(ph7_vm *pVm,ph7_value *pKey,ph7_value *pValue,void *pUserData);
PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData);
PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(ph7_vm *pVm,ph7_value *pFunc,ph7_value *pResult,...);
PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce);
PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen);
PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm);
PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm);
PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke);
PH7_PRIVATE ph7_value * PH7_VmExtractSuper(ph7_vm *pVm,const char *zName,sxu32 nByte);
PH7_PRIVATE sxi32 PH7_VmHashmapInsert(ph7_hashmap *pMap,const char *zKey,int nKeylen,const char *zData,int nLen);
#ifndef PH7_DISABLE_DISK_IO
PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(ph7_vm *pVm,const char **pzDevice,int nByte);
#endif /* PH7_DISABLE_BUILTIN_FUNC || PH7_DISABLE_DISK_IO */
/* vm_http.c function prototypes */
PH7_PRIVATE sxi32 PH7_VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen);
PH7_PRIVATE sxi32 PH7_VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte);
/* vm_http_response.c function prototypes */
PH7_PRIVATE void PH7_RegisterHttpResponseFunctions(ph7_vm *pVm);
PH7_PRIVATE void PH7_VmReleaseResponseHeaders(ph7_vm *pVm);
/* vm_pcre.c function prototypes */
#ifdef PH7_ENABLE_PCRE
PH7_PRIVATE void PH7_RegisterPcreFunctions(ph7_vm *pVm);
PH7_PRIVATE void PH7_RegisterPcreConstants(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_PcreMatchQuiet(ph7_context *pCtx,const char *zPat,int nPat,
	const char *zSub,int nSub,int *pMatched);
#endif /* PH7_ENABLE_PCRE */
/* net.c types and function prototypes */
#ifdef PH7_ENABLE_NET
#ifdef __WINNT__
#include <winsock2.h>
typedef SOCKET ph7_socket;
typedef int ph7_socklen;
#define PH7_NET_INVALID_SOCKET INVALID_SOCKET
#else
typedef int ph7_socket;
typedef unsigned int ph7_socklen;
#define PH7_NET_INVALID_SOCKET (-1)
#endif
struct sockaddr; /* Forward declaration */
PH7_PRIVATE int PH7_NetInit(void);
PH7_PRIVATE void PH7_NetCleanup(void);
PH7_PRIVATE ph7_socket PH7_NetListen(const char *zHost,int iPort,int iBacklog);
PH7_PRIVATE ph7_socket PH7_NetAccept(ph7_socket listenSock,struct sockaddr *pAddr,ph7_socklen *pAddrLen);
PH7_PRIVATE int PH7_NetRecv(ph7_socket sock,void *pBuf,int nLen,int flags);
PH7_PRIVATE int PH7_NetSend(ph7_socket sock,const void *pBuf,int nLen,int flags);
PH7_PRIVATE int PH7_NetSendAll(ph7_socket sock,const void *pBuf,int nLen);
PH7_PRIVATE void PH7_NetClose(ph7_socket sock);
PH7_PRIVATE void PH7_NetSetTimeout(ph7_socket sock,int iMilliseconds);
PH7_PRIVATE void PH7_NetAddrToString(const struct sockaddr *pAddr,char *zBuf,int nBufLen);
PH7_PRIVATE int PH7_NetAddrPort(const struct sockaddr *pAddr);
#endif /* PH7_ENABLE_NET */
/* vm_json.c function prototypes */
PH7_PRIVATE int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_json_last_error_msg(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_json_validate(ph7_context *pCtx,int nArg,ph7_value **apArg);
/* vm_serialize.c function prototypes */
PH7_PRIVATE int vm_builtin_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE void PH7_AppendShortestReal(SyBlob *pOut,double d);
/* memobj.c float-shape helper (php_gcvt/smart_str_append_double semantics);
 * shared by the float->string cast and builtin.c's printf float conversions */
#ifndef PH7_OMIT_FLOATING_POINT
PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric);
#endif
/* vm_xml.c function prototypes */
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE int vm_builtin_xml_parser_create(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_xml_parser_create_ns(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_xml_parser_free(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_xml_set_element_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_xml_set_character_data_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_xml_set_default_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_xml_set_end_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_xml_set_start_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_xml_set_processing_instruction_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_xml_set_unparsed_entity_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_xml_set_notation_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_xml_set_external_entity_ref_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_xml_get_current_line_number(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_xml_get_current_byte_index(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_xml_set_object(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_xml_get_current_column_number(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_xml_get_error_code(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_xml_parse(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_xml_parser_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_xml_parser_get_option(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_xml_error_string(ph7_context *pCtx,int nArg,ph7_value **apArg);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
PH7_PRIVATE int vm_builtin_utf8_encode(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_utf8_decode(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates);
PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...);
/* vm_builtin_class.c function prototypes */
PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass);
PH7_PRIVATE int PH7_VmClassMemberAccess(ph7_vm *pVm,ph7_class *pClass,const SyString *pAttrName,sxi32 iProtection,int bLog);
PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg);
PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg);
/* vm_builtin_ob.c function prototypes */
PH7_PRIVATE int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData);
PH7_PRIVATE int vm_builtin_ob_clean(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_ob_end_clean(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_ob_get_clean(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_ob_get_length(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_ob_get_level(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_ob_start(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_ob_flush(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_ob_end_flush(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_ob_implicit_flush(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_ob_list_handlers(ph7_context *pCtx,int nArg,ph7_value **apArg);
/* vm_builtin_getopt.c function prototypes */
PH7_PRIVATE int vm_builtin_getopt(ph7_context *pCtx,int nArg,ph7_value **apArg);
/* builtin_math.c function prototypes */
#ifdef PH7_ENABLE_MATH_FUNC
PH7_PRIVATE int PH7_builtin_sqrt(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_exp(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_floor(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_cos(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_acos(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_cosh(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_sin(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_asin(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_sinh(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_tan(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_atan(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_tanh(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg);
#endif /* PH7_ENABLE_MATH_FUNC */
PH7_PRIVATE int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_intdiv(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg);
/* builtin_date.c function prototypes */
PH7_PRIVATE int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_date_default_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_date_default_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg);
/* vfs_zip.c function prototypes */
PH7_PRIVATE int PH7_builtin_zip_open(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_zip_close(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_zip_read(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_zip_entry_open(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_zip_entry_close(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_zip_entry_name(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_zip_entry_filesize(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_zip_entry_compressedsize(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_zip_entry_read(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_zip_entry_reset_read_cursor(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_zip_entry_compressionmethod(ph7_context *pCtx,int nArg,ph7_value **apArg);
/* vfs_win.c / vfs_unix.c exported structs */
#ifdef __WINNT__
extern const ph7_vfs sWinVfs;
extern const ph7_io_stream sWinFileStream;
#elif defined(__UNIXES__)
extern const ph7_vfs sUnixVfs;
extern const ph7_io_stream sUnixFileStream;
#endif
PH7_PRIVATE int PH7_Utf8Read(
  const unsigned char *z,         /* First byte of UTF-8 character */
  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */
  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */
);
/* parse.c function prototypes */
PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc);
PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot);
PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode);
PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext);
PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd);
PH7_PRIVATE const ph7_expr_op * PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast);
PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet);
/* compile.c function prototypes */
PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType);
PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_InitCodeGenerator(ph7_vm *pVm,ProcConsumer xErr,void *pErrData);
PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(ph7_vm *pVm,ProcConsumer xErr,void *pErrData);
PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...);
PH7_PRIVATE sxi32 PH7_CompileScript(ph7_vm *pVm,SyString *pScript,sxi32 iFlags);
/* constant.c function prototypes */
PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm);
/* builtin.c function prototypes */
PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm);
/* hashmap.c function prototypes */
PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(ph7_vm *pVm,sxu32 (*xIntHash)(sxi64),sxu32 (*xBlobHash)(const void *,sxu32));
PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS);
PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap);
PH7_PRIVATE sxi32 PH7_HashmapLookup(ph7_hashmap *pMap,ph7_value *pKey,ph7_hashmap_node **ppNode);
PH7_PRIVATE sxi32 PH7_HashmapInsert(ph7_hashmap *pMap,ph7_value *pKey,ph7_value *pVal);
PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(ph7_hashmap *pMap,ph7_value *pKey,sxu32 nRefIdx);
PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest);
PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight);
PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore);
PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest);
PH7_PRIVATE sxi32 PH7_HashmapDupMaterialized(ph7_hashmap *pSrc,ph7_hashmap *pDest);
PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue);
PH7_PRIVATE sxi32 PH7_HashmapCmp(ph7_hashmap *pLeft,ph7_hashmap *pRight,int bStrict);
PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep);
PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep);
PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap);
PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore);
PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey);
PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth);
PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth);
PH7_PRIVATE sxi32 PH7_HashmapWalk(ph7_hashmap *pMap,int (*xWalk)(ph7_value *,ph7_value *,void *),void *pUserData);
PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap);
/* php value-name helper (true/false/class-name/null); used by the range()/
 * array_rand() domain-error messages in hashmap.c, which are compiled in every
 * mode, so it must stay outside the PH7_DISABLE_DISK_IO guard. */
PH7_PRIVATE const char *VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf);
/* Numeric-string classifier — php's is_numeric_string() grammar — shared from
 * hashmap.c (range/array_rand) for the stage-2 ZPP domain-error sweep
 * (PLAN §3.9(a)). RangeStrToNumber only ever returns ERROR/LONG/DOUBLE; the
 * STRING/DIGIT codes are range()-internal endpoint tags. range() and array_rand()
 * are core builtins compiled in every mode, so these must stay outside the
 * PH7_DISABLE_DISK_IO guard. */
#define RANGE_IN_ERROR   0
#define RANGE_IN_LONG    1
#define RANGE_IN_DOUBLE  2
#define RANGE_IN_STRING  3
#define RANGE_IN_DIGIT   4
PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble);
#ifndef PH7_DISABLE_DISK_IO
PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut);
/* builtin.c function prototypes */
PH7_PRIVATE sxi32 PH7_InputFormat(int (*xConsumer)(ph7_context *,const char *,int,void *),
	ph7_context *pCtx,const char *zIn,int nByte,int nArg,ph7_value **apArg,void *pUserData,int vf);
PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte);
PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg);
PH7_PRIVATE sxi32 PH7_ProcessCsv(const char *zInput,int nByte,int delim,int encl,
	int escape,sxi32 (*xConsumer)(const char *,int,void *),void *pUserData);
PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData);
PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen);
PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection);
#endif /* PH7_DISABLE_BUILTIN_FUNC || PH7_DISABLE_DISK_IO */
/* oo.c function prototypes */
PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine);
PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags);
PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,
	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags);
PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte);
PH7_PRIVATE ph7_class_attr   * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte);
PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr);
PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth);
PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase);
PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait);
PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase);
PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface);
PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass);
PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc);
PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest);
PH7_PRIVATE void  PH7_ClassInstanceUnref(ph7_class_instance *pThis);
PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth);
PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,const char *zMethod,
	sxu32 nByte,const SyString *pAttrName,ph7_value *pResult);
PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr);
PH7_PRIVATE sxi32 PH7_VmHookGetAttrValue(ph7_class_instance *pThis,VmClassAttr *pVmAttr,ph7_value *pOut);
PH7_PRIVATE void PH7_MemObjPrintRInline(SyBlob *pOut,ph7_value *pObj);
PH7_PRIVATE ph7_value * PH7_EnumCaseNameValue(ph7_class_instance *pThis);
PH7_PRIVATE ph7_value * PH7_EnumCaseBackingValueOf(ph7_class_instance *pThis);
PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap);
PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(ph7_class_instance *pThis,
	int (*xWalk)(const char *,ph7_value *,void *),void *pUserData);
PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName);
/* vfs.c */
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,
	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew);
PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut);
PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen);
PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm);
PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void);
PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource);
PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm);
PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm);
PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm);
/* lib.c function prototypes */
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE sxi32 SyXMLParserInit(SyXMLParser *pParser,SyMemBackend *pAllocator,sxi32 iFlags);
PH7_PRIVATE sxi32 SyXMLParserSetEventHandler(SyXMLParser *pParser,
	void *pUserData,
	ProcXMLStartTagHandler xStartTag,
	ProcXMLTextHandler xRaw,
	ProcXMLSyntaxErrorHandler xErr,
	ProcXMLStartDocument xStartDoc,
	ProcXMLEndTagHandler xEndTag,
	ProcXMLPIHandler xPi,
	ProcXMLEndDocument xEndDoc,
	ProcXMLDoctypeHandler xDoctype,
	ProcXMLNameSpaceStart xNameSpace,
	ProcXMLNameSpaceEnd xNameSpaceEnd
	);
PH7_PRIVATE sxi32 SyXMLProcess(SyXMLParser *pParser,const char *zInput,sxu32 nByte);
PH7_PRIVATE sxi32 SyXMLParserRelease(SyXMLParser *pParser);
PH7_PRIVATE sxi32 SyArchiveInit(SyArchive *pArch,SyMemBackend *pAllocator,ProcHash xHash,ProcRawStrCmp xCmp);
PH7_PRIVATE sxi32 SyArchiveRelease(SyArchive *pArch);
PH7_PRIVATE sxi32 SyArchiveResetLoopCursor(SyArchive *pArch);
PH7_PRIVATE sxi32 SyArchiveGetNextEntry(SyArchive *pArch,SyArchiveEntry **ppEntry);
PH7_PRIVATE sxi32 SyZipExtractFromBuf(SyArchive *pArch,const char *zBuf,sxu32 nLen);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE sxi32 SyBinToHexConsumer(const void *pIn,sxu32 nLen,ProcConsumer xConsumer,void *pConsumerData);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
#ifndef PH7_DISABLE_BUILTIN_FUNC
#ifndef PH7_DISABLE_HASH_FUNC
PH7_PRIVATE sxu32 SyCrc32(const void *pSrc,sxu32 nLen);
PH7_PRIVATE void MD5Update(MD5Context *ctx, const unsigned char *buf, unsigned int len);
PH7_PRIVATE void MD5Final(unsigned char digest[16], MD5Context *ctx);
PH7_PRIVATE sxi32 MD5Init(MD5Context *pCtx);
PH7_PRIVATE sxi32 SyMD5Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[16]);
PH7_PRIVATE void SHA1Init(SHA1Context *context);
PH7_PRIVATE void SHA1Update(SHA1Context *context,const unsigned char *data,unsigned int len);
PH7_PRIVATE void SHA1Final(SHA1Context *context, unsigned char digest[20]);
PH7_PRIVATE sxi32 SySha1Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[20]);
#endif
#endif /* PH7_DISABLE_BUILTIN_FUNC */
PH7_PRIVATE sxi32 SyRandomness(SyPRNGCtx *pCtx,void *pBuf,sxu32 nLen);
PH7_PRIVATE sxi32 SyRandomnessInit(SyPRNGCtx *pCtx,ProcRandomSeed xSeed,void *pUserData);
PH7_PRIVATE sxu32 SyBufferFormat(char *zBuf,sxu32 nLen,const char *zFormat,...);
PH7_PRIVATE sxu32 SyBlobFormatAp(SyBlob *pBlob,const char *zFormat,va_list ap);
PH7_PRIVATE sxu32 SyBlobFormat(SyBlob *pBlob,const char *zFormat,...);
PH7_PRIVATE sxi32 SyProcFormat(ProcConsumer xConsumer,void *pData,const char *zFormat,...);
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE const char *SyTimeGetMonth(sxi32 iMonth);
PH7_PRIVATE const char *SyTimeGetDay(sxi32 iDay);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
PH7_PRIVATE sxi32 SyUriDecode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData,int bUTF8);
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE sxi32 SyUriEncode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);
#endif
PH7_PRIVATE sxi32 SyLexRelease(SyLex *pLex);
PH7_PRIVATE sxi32 SyLexTokenizeInput(SyLex *pLex,const char *zInput,sxu32 nLen,void *pCtxData,ProcSort xSort,ProcCmp xCmp);
PH7_PRIVATE sxi32 SyLexInit(SyLex *pLex,SySet *pSet,ProcTokenizer xTokenizer,void *pUserData);
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE sxi32 SyBase64Decode(const char *zB64,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);
PH7_PRIVATE sxi32 SyBase64Encode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
PH7_PRIVATE sxi32 SyStrToReal(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);
PH7_PRIVATE sxi32 SyBinaryStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);
PH7_PRIVATE sxi32 SyOctalStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);
PH7_PRIVATE sxi32 SyHexStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);
PH7_PRIVATE sxi32 SyHexToint(sxi32 c);
PH7_PRIVATE sxi32 SyStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);
PH7_PRIVATE sxi32 SyStrToInt32(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);
PH7_PRIVATE sxi32 SyStrIsNumeric(const char *zSrc,sxu32 nLen,sxu8 *pReal,const char **pzTail);
PH7_PRIVATE SyHashEntry *SyHashLastEntry(SyHash *pHash);
PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData);
PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData);
PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32(*xStep)(SyHashEntry *,void *),void *pUserData);
PH7_PRIVATE SyHashEntry *SyHashGetNextEntry(SyHash *pHash);
PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash);
PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry);
PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData);
PH7_PRIVATE SyHashEntry *SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen);
PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash);
PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp);
PH7_PRIVATE sxu32 SyStrHash(const void *pSrc,sxu32 nLen);
PH7_PRIVATE sxu32 SyBinHash(const void *pSrc,sxu32 nLen);
PH7_PRIVATE sxu32 Systrcpy(char *zDest,sxu32 nDestLen,const char *zSrc,sxu32 nLen);
PH7_PRIVATE void *SySetAt(SySet *pSet,sxu32 nIdx);
PH7_PRIVATE void *SySetPop(SySet *pSet);
PH7_PRIVATE void *SySetPeek(SySet *pSet);
PH7_PRIVATE sxi32 SySetRelease(SySet *pSet);
PH7_PRIVATE sxi32 SySetReset(SySet *pSet);
PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet);
PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry);
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize);
PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem);
PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem);
PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize);
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft);
#endif
PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob);
PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob);
PH7_PRIVATE sxi32 SyBlobCmp(SyBlob *pLeft,SyBlob *pRight);
PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest);
PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob);
PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize);
PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte);
PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator);
PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize);
PH7_PRIVATE char *SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize);
PH7_PRIVATE void *SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize);
PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend);
PH7_PRIVATE sxi32 SyMemBackendInitFromOthers(SyMemBackend *pBackend,const SyMemMethods *pMethods,ProcMemError xMemErr,void *pUserData);
PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void *pUserData);
PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent);
#if 0
/* Not used in the current release of the PH7 engine */
PH7_PRIVATE void *SyMemBackendPoolRealloc(SyMemBackend *pBackend,void *pOld,sxu32 nByte);
#endif
PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void *pChunk);
PH7_PRIVATE void *SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte);
PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void *pChunk);
PH7_PRIVATE void *SyMemBackendRealloc(SyMemBackend *pBackend,void *pOld,sxu32 nByte);
PH7_PRIVATE void *SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte);
#if defined(PH7_ENABLE_THREADS)
PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods);
PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend);
#endif
PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen);
PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize);
PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize);
PH7_PRIVATE sxi32 SyStrnicmp(const char *zLeft,const char *zRight,sxu32 SLen);
PH7_PRIVATE sxi32 SyStrnmicmp(const void *pLeft, const void *pRight,sxu32 SLen);
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE sxi32 SyStrncmp(const char *zLeft,const char *zRight,sxu32 nLen);
#endif
PH7_PRIVATE sxi32 SyByteListFind(const char *zSrc,sxu32 nLen,const char *zList,sxu32 *pFirstPos);
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE sxi32 SyByteFind2(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos);
#endif
PH7_PRIVATE sxi32 SyByteFind(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos);
PH7_PRIVATE sxu32 SyStrlen(const char *zSrc);
#if defined(PH7_ENABLE_THREADS)
PH7_PRIVATE const SyMutexMethods *SyMutexExportMethods(void);
PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods);
PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend);
#endif
#endif /* __PH7INT_H__ */
