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

#ifndef PH7_PI
/* Value of PI */
/* pi to DOUBLE precision. It used to be 3.1415926535898 -- only 14 significant
 * digits -- so M_PI differed from php's in the 13th place and every trig result
 * built on it was quietly off (rad2deg(M_PI) gave 180.0000000000004). */
#define PH7_PI 3.14159265358979323846
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
typedef struct PH7_NativeIterVtab PH7_NativeIterVtab;


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
/* The pending offset of a `$s[k] ??= v`, owned by its MEMOBJ_AUX_COALSTROFF peek result.
 * Carries its own allocator so PH7_MemObjRelease can free it without a VM pointer, exactly
 * like VmDeferredPath. */
typedef struct VmCoalStrOff VmCoalStrOff;
struct VmCoalStrOff
{
	SyMemBackend *pAlloc;
	ph7_value sKey;      /* the RAW offset, unresolved: the store re-resolves it LOUDLY */
};
/* The pending __call / __callStatic routing, owned by its MEMOBJ_AUX_MAGICCALL carrier
 * slot. Carries its own allocator so PH7_MemObjRelease can free it without a VM pointer,
 * exactly like VmCoalStrOff.
 *
 * It used to be three fields on the VM, set by OP_MEMBER and read by the OP_CALL that
 * followed. That only held while the arguments were evaluated BEFORE the member op; once
 * the callee is resolved first — php's order — an argument that is itself a routed call
 * (`$o->outer($o->inner(1))`) runs in between and would overwrite the outer routing. The
 * record rides the slot instead, so it nests, and an abandoned call cannot leak the
 * receiver reference. */
typedef struct VmMagicCall VmMagicCall;
struct VmMagicCall
{
	SyMemBackend *pAlloc;
	ph7_class_instance *pRecv; /* OWNED receiver reference; 0 for a static routing */
	ph7_class *pClass;         /* the class whose handler answers */
	SyBlob sName;              /* the method name as the CALL SITE spelled it */
};
/*
 * D1 commit 2: a captured lvalue path for a deferred by-ref/by-value call argument
 * ($a["k"], $o->p, and nested/undefined-base forms). Built on a lookup MISS by the
 * LOAD_IDX/MEMBER record modes and re-walked by VmResolveDeferredArgs at OP_CALL once the
 * callee's by-ref shape is known. Owned by a MEMOBJ_AUX_DEFPATH stack slot's x.pOther.
 */
typedef struct VmDeferStep VmDeferStep;
typedef struct VmDeferredPath VmDeferredPath;
struct VmDeferStep {
	int       isProp;    /* 0 = subscript element, 1 = object property */
	int       bAppend;   /* element step with NO key: `f($a[])`, php's append. Only a by-REF
	                      * parameter may take one — a by-value binding is php's runtime
	                      * `Cannot use [] for reading` Error. */
	ph7_value sKey;      /* element: deep-copied index value (copied before pIdx is released) */
	SyString  sProp;     /* property: name, into pName below (owned by the path allocation) */
	char     *zProp;     /* property: owned copy of the name bytes (freed with the path) */
};
struct VmDeferredPath {
	SyMemBackend *pAlloc;    /* allocator, so PH7_MemObjRelease can self-free without a pVm */
	int           eRoot;     /* 0 = real container nIdx, 1 = undefined-var name, 2 = string base,
	                          * 3 = a value ALREADY FETCHED (VM_DEFER_ROOT_PREFETCH below) */
	sxu32         nRootIdx;  /* eRoot==0: aMemObj slot of the root container ($a/$o) */
	SyString      sRootName; /* eRoot==1: variable name (VM-lifetime bytecode string, borrowed) */
	sxu8          nOverKind; /* eRoot==3: which verdict a by-REFERENCE binding gets (VM_OVER_*) */
	ph7_class    *pOverClass;/* eRoot==3: the class that answered — php's message names it */
	SyString      sOverName; /* eRoot==3: the PROPERTY name (empty for an element) */
	char         *zOverName; /* eRoot==3: owned bytes behind sOverName */
	ph7_value     sPrefetch; /* eRoot==3: what the accessor answered */
	sxu32         nStep;     /* number of captured steps (outer-to-inner) */
	sxu32         nAlloc;    /* capacity of aStep */
	VmDeferStep  *aStep;     /* captured steps */
};
/*
 * eRoot == 3. Some fetches cannot be DEFERRED at all: a userland `offsetGet` (or a
 * subclass override of a native one) is a method call, and php runs it where the
 * subscript is WRITTEN, whatever the parameter turns out to be. So the accessor runs
 * at the fetch and the carrier holds its RESULT — the by-ref decision still arrives at
 * OP_CALL, and all it decides is php's `Indirect modification of overloaded element`
 * notice, since a value the container copied has no slot to alias either way.
 */
#define VM_DEFER_ROOT_PREFETCH 3
/* What a by-REFERENCE binding of a prefetched value is, in php's words. All three are
 * decided at the CALL because none of them is about the fetch: the same fetch feeding a
 * by-VALUE parameter is silent. */
#define VM_OVER_ELEM 0 /* Notice: Indirect modification of overloaded element of C has no effect */
#define VM_OVER_PROP 1 /* Notice: Indirect modification of overloaded property C::$p has no effect */
#define VM_OVER_HOOK 2 /* Error:  Indirect modification of C::$p is not allowed */
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
#define MEMOBJ_AUX_NOKEY 0x2000 /* Stack-only marker: absent array-literal key (see PH7_LOADC_NOKEY) */
#define MEMOBJ_AUX_CUFVAL 0x4000 /* Stack-only marker: the ENGINE deliberately handed this by-ref
                                  * argument a by-value copy, so the by-ref binder must NOT raise its
                                  * "could not be passed by reference" Error for it. Two producers:
                                  * call_user_func(), which php warns about and copies; and an
                                  * argument UNPACKED out of a temporary array (`f(...[1])`), whose
                                  * element php binds into a temporary nothing can observe. */
#define MEMOBJ_AUX_DEFERRED 0x8000 /* Stack-only marker (D1): a deferred call argument whose target did
                                    * not exist at load time. The value is NULL; x.pOther carries the
                                    * lazy-lvalue descriptor (a plain-variable name pointer, or an
                                    * element/property descriptor). OP_CALL's VmResolveDeferredArgs
                                    * materializes it for a by-ref parameter or warns+passes NULL for a
                                    * by-value one, clearing this flag. Never survives into a stored
                                    * value: it is part of MEMOBJ_AUX, so MemObjStore strips it. */
#define MEMOBJ_AUX_DEFPATH 0x10000 /* Stack-only marker (D1 commit 2): a deferred call argument that is an
                                    * array-element ($a["k"]) or property ($o->p) lvalue whose target was
                                    * ABSENT at load time. The value is NULL; x.pOther owns a heap
                                    * VmDeferredPath (captured lvalue chain). VmResolveDeferredArgs re-walks
                                    * it in vivify-mode (by-ref) or read+warn-mode (by-value). Unlike
                                    * MEMOBJ_AUX_DEFERRED (a borrowed name pointer), this OWNS heap memory:
                                    * PH7_MemObjRelease frees it at the TOP, before its MEMOBJ_NULL
                                    * short-circuit, so every pop/abort/exception path releases it. Part of
                                    * MEMOBJ_AUX, so MemObjStore strips the flag on copy. */
#define MEMOBJ_AUX_COALSTROFF 0x40000 /* Stack-only marker: this NULL is the peek result of a
                                       * `$s[k] ??= v` over a STRING, and it OWNS a heap
                                       * VmCoalStrOff holding the RAW offset (x.pOther) for the
                                       * OP_NULLC_STORE that follows — which has to write into
                                       * the string OFFSET, and by then the offset value is gone.
                                       * Same ownership contract as MEMOBJ_AUX_DEFPATH:
                                       * PH7_MemObjRelease frees it, so an abandoned statement
                                       * cannot leak it, and it nests (one carrier per pending
                                       * ??= on the operand stack) where a single VM-wide slot
                                       * could not. */
#define MEMOBJ_AUX_MAGICCALL 0x80000 /* Stack-only marker: this callee slot is the engine's own
                                      * __call/__callStatic dispatch, not a callable at all. OP_MEMBER
                                      * sets it (with the receiver/class/original name latched on the
                                      * VM) where a missing or inaccessible method must route through
                                      * the magic handler; OP_CALL sees the mark BEFORE any callable
                                      * decode and runs the packing body directly. It is what replaced
                                      * the "__phl_magic_call" NAME the four OP_MEMBER sites used to
                                      * write into this slot -- a hidden global function that
                                      * function_exists() and get_defined_functions() both reported.
                                      * The slot itself stays NULL-typed. Part of MEMOBJ_AUX, so a
                                      * copy can never carry it. */
#define MEMOBJ_AUX_MEMBERCALL 0x100000 /* Stack-only marker: this callee slot came out of an OP_MEMBER
                                       * method resolution, which already DECIDED the call's
                                       * visibility against the entry it actually chose. OP_CALL's
                                       * own screen must then stand down: it re-derives the method
                                       * from the FUNCTION's name against its declaring class, and
                                       * a trait adaptation splits those apart — `pub as private
                                       * pHi` and `prot as public opened` share one struct name and
                                       * one sVmName with the method they were made from, so the
                                       * re-derivation answered for the ORIGINAL and got the rule
                                       * backwards in both directions. The mark rides the exact
                                       * stack slot the call consumes (like MEMOBJ_AUX_MAGICCALL),
                                       * so it cannot leak to another call the way a VM-wide latch
                                       * could. Part of MEMOBJ_AUX, so a copy can never carry it. */
#define MEMOBJ_AUX_STROFFSET 0x20000 /* Stack-only marker: this value was READ OUT of a string by a
                                      * subscript ($s[1]). It carries the BASE's slot index like any
                                      * other element read, but a string offset is not a slot: php
                                      * refuses to make a reference to one
                                      * ("Cannot create references to/from string offsets"), and
                                      * binding the index anyway aliased the WHOLE STRING — writing
                                      * through the reference replaced it. The reference-binding
                                      * sites test this. Part of MEMOBJ_AUX, so MemObjStore strips
                                      * it: a plain `$c = $s[1]` copy carries nothing. */
/* Mask of all known types */
#define MEMOBJ_ALL (MEMOBJ_STRING|MEMOBJ_INT|MEMOBJ_REAL|MEMOBJ_BOOL|MEMOBJ_NULL|MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES)
/* Scalar variables
 * According to the PHP language reference manual
 *  Scalar variables are those containing an integer, float, string or boolean.
 *  Types array, object and resource are not scalar.
 */
#define MEMOBJ_SCALAR (MEMOBJ_STRING|MEMOBJ_INT|MEMOBJ_REAL|MEMOBJ_BOOL|MEMOBJ_NULL)
#define MEMOBJ_AUX (MEMOBJ_REFERENCE|MEMOBJ_AUX_SPREAD|MEMOBJ_AUX_NOKEY|MEMOBJ_AUX_CUFVAL|MEMOBJ_AUX_DEFERRED|MEMOBJ_AUX_DEFPATH|MEMOBJ_AUX_STROFFSET|MEMOBJ_AUX_COALSTROFF|MEMOBJ_AUX_MAGICCALL|MEMOBJ_AUX_MEMBERCALL)
/* Closure-instance flags (ph7_class_instance.iFlags), shared by vm_exec.c's OP_LOAD_FCC
 * and vm_exec_ctx.c's closure machinery. Distinct from CLASS_INSTANCE_DESTROYED 0x001
 * (oo.c) and VM_INSTANCE_DUMPING 0x002 (vm_builtin_var.c), which share the same word. */
/* ph7_class_instance.iFlags bit: this Closure is a bound/static first-class callable and
 * carries $__this/$__scope. Lets the hot plain-closure unwrap skip those attribute lookups.
 * (Distinct from CLASS_INSTANCE_DESTROYED 0x001 and VM_INSTANCE_DUMPING 0x002.) */
#define VM_INSTANCE_FCC_BOUND 0x004
/* ph7_class_instance.iFlags bit: this Closure wraps an __invoke OBJECT, and the engine —
 * not the source — is what named `__invoke` (Closure::fromCallable($obj)). php resolves it
 * the way it resolves `$obj()`, so a non-public __invoke is dispatched rather than denied;
 * `$obj->__invoke(...)` and `[$obj,'__invoke']`, which the SOURCE names, stay denied and
 * never carry this bit. Read by VmClosureUnwrap, which arms the engine's magic latch. */
#define VM_INSTANCE_FCC_INVOKE_OBJ 0x010
/* ph7_class_instance.iFlags bit: this Closure's $__fn names a METHOD of $__this's class (or
 * of $__scope) — `$o->m(...)`, `C::m(...)`, `Closure::fromCallable([$o,'m'])`. Without it the
 * unwrap had to GUESS, by asking whether the class declares a method of that name, and it
 * guessed wrong in both directions: a name the class answers only through __call fell through
 * to the plain-function route and failed with "Call to undefined function m()", and a name
 * that happened to match a global function ran the FUNCTION. A bound plain closure
 * (`function(){…}->bindTo($o)`) never carries this bit, which is what the guess was really
 * trying to detect. */
#define VM_INSTANCE_FCC_METHOD 0x020
/* ph7_class_instance.iFlags bit: this Closure's callee was RESOLVED to a real, directly
 * callable method when the closure was BUILT — the way php resolves one, keeping the
 * function itself rather than a name. No dispatch site may re-decide its visibility against
 * the CALLER: that is what killed an escaped `$this->priv(...)` php runs anywhere. A closure
 * whose creation resolved to the class's __call/__callStatic TRAMPOLINE instead (a missing or
 * inaccessible name on a class that declares one) deliberately does NOT carry the bit — its
 * dispatch has to reach the catch-all, as php's does. */
#define VM_INSTANCE_FCC_SCREENED 0x040
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
	sxu8 bHasMaxArg;          /* 0 -> no central too-many-arguments check (the SAFE default: this
	                           * struct is SyZero'd on creation, so an unstamped builtin must mean
	                           * "unenforced", never "accepts at most zero"). 1 -> nMaxArg applies. */
	sxi16 nMaxArg;            /* Maximum accepted arguments when bHasMaxArg; derived from the
	                           * signature. A variadic parameter leaves bHasMaxArg at 0. */
	const char *zSig;         /* PHP-style parameter list ("string $s, int $o = 0") from the
	                           * static signature table, or NULL: ReflectionFunction input for
	                           * internal functions. Points at static storage — never freed. */
	const char *zRet;         /* Return-type text from the same table, or NULL */
	sxu32 nByRefMask;         /* D1: bit N set => positional parameter N is by-reference (`&$p` in zSig),
	                           * derived once in VmSetBuiltinSignatures. Lets OP_CALL materialize a
	                           * deferred by-ref out-param regardless of how the builtin was reached
	                           * (bare name, dynamic `$f=...`, or callable) — the compile-time
	                           * GenStateByRefBuiltinMask only sees the bare-name case. 0 when unstamped. */
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
	sxi32 nThrowRc;         /* Status of a throw this host function raised through
	                         * PH7_VmThrowException (0 when it never threw). The
	                         * OP_CALL boundary re-reads it: a builtin that threw and
	                         * still returned PH7_OK would otherwise let the VM carry
	                         * on inside the try the throw abandoned. See
	                         * VmHostFuncThrowRc(). */
	VmCallArgMap *pArgMap;  /* Call-site named-argument map (or 0). Lets a builtin
	                         * such as call_user_func forward its callers' name:
	                         * arguments to the inner callback. */
	ph7_class_instance *pThis; /* VM_FUNC_NATIVE method only: the receiver, or 0 for a static
	                         * call and for every plain host function. Read through
	                         * PH7_ContextThis(); the reference is owned by the CALLER for the
	                         * duration of the call, so a native body must not unref it. */
	ph7_class *pCalledClass;/* VM_FUNC_NATIVE method only: the class the call was made
	                         * THROUGH (php's late-static-binding target), which for an
	                         * inherited method is the subclass, not the declaring class. 0 for
	                         * a plain host function. */
	ph7_value sThis;        /* Scratch MEMOBJ_OBJ view of pThis, materialized on the first
	                         * PH7_ContextThisValue() call so a native body can reach the
	                         * receiver through the ordinary ph7_value object helpers
	                         * (ph7_object_fetch_attr & co). bThisInit gates the lazy init;
	                         * VmReleaseCallContext tears it down. */
	sxu8 bThisInit;         /* 1 once sThis has been initialized */
};
/*
 * Each hashmap entry [i.e: array(4,5,6)] is recorded in an instance
 * of the following structure.
 */
/* Allowed hashmap node key types (iType below) */
#define HASHMAP_INT_NODE   1  /* Node with an int [i.e: 64-bit integer] key */
#define HASHMAP_BLOB_NODE  2  /* Node with a string/BLOB key */
/* Node control flags (iFlags below) */
#define HASHMAP_NODE_FOREIGN_OBJ 0x001 /* Node holds a reference to a foreign ph7_value
                                        * [i.e: array(&var) / $a[] =& $var ] */
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
	sxu8 bIntKeySeen;             /* An integer key has been inserted at least once. php 8.3
	                               * carries the auto-index through NEGATIVE keys: the first
	                               * int key sets the next index to key+1 even when negative
	                               * ($a[-4]=x; $a[]=y stores y at -3), where it used to
	                               * restart at 0. Only the FIRST key may move the index
	                               * downwards, hence the flag. */
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
	const void *pNativeValue; /* A NATIVE attribute's literal (PH7_NativeConstDef *), or 0.
	                   * php declares `#[Attribute(Attribute::TARGET_CLASS)]` on two of its
	                   * own classes, and a class declared from C has no compiler to emit
	                   * byte-code for the argument — so the value rides as a literal and
	                   * every reader takes this branch when aByteCode is empty. */
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
	sxu32 nLoopId;        /* This block's loop/switch id (0 when it is neither) */
	sxu32 nOuterLoopId;   /* Loop/switch that was innermost when this one was entered */
	sxu32 nScopeId;       /* Try/catch scope in effect INSIDE this block (0 = none). An
	                       * exception block mints its own; every other block inherits. */
	sxu32 nOuterScopeId;  /* Scope that was innermost when this block was entered */
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
	ph7_class *pCurClass; /* Class/interface/trait/enum whose BODY is currently being compiled
	                       * (0 at top level). Saved/restored around each class-body compiler so
	                       * a nested anonymous class overrides it. Lets a const-expression that
	                       * compiles OUTSIDE any function block — a property default or a
	                       * parameter default — resolve __TRAIT__ to the enclosing trait, which
	                       * the block-chain walk alone cannot see (no func block on the chain). */
	int iInMemberDefault; /* > 0 while compiling a property/parameter DEFAULT value. Such a
	                       * const-expression belongs to pCurClass, never to a lexically-
	                       * enclosing method, so __TRAIT__ reads pCurClass directly rather than
	                       * walking the block chain (which would leak into the enclosing
	                       * function — e.g. an anonymous class's default inside a trait method). */
	GenBlock sGlobal;    /* Global block */
	ProcConsumer xErr;   /* Error consumer callback */
	void *pErrData;      /* Third argument to xErr() */
	SySet aLabel;        /* Label table */
	SySet aGoto;         /* Gotos table */
	SySet aNullsafeJmp;  /* Pending NULLSAFE_JMP instruction indices (sxu32) */
	int nCommaExprOk;    /* > 0 while compiling a for() clause, the ONLY place php's grammar
	                      * allows a comma-separated expression list (PH7's comma OPERATOR
	                      * is otherwise a PH7-ism php rejects — §10) */
	const char *zClauseCloser; /* When an expression has a trailing token the grammar can't
	                      * absorb, the tree builder names it and, if this is set, says what
	                      * the enclosing construct expected: `;` after `return`, `,`/`;`
	                      * after `echo`, `)` for a for() post clause, `]` inside an array
	                      * literal, and so on. Each construct saves/sets/restores it around
	                      * its expression compile. NULL means "no expecting clause" — php
	                      * prints none for a plain expression statement. */
	int nExprEchoOk;     /* > 0 only while compiling the synthesized `echo` of a `<?= ... ?>`
	                      * short tag, which is the one place an echo legitimately compiles
	                      * as an EXPRESSION. Everywhere else `echo` in expression position
	                      * is a php parse error (it was a Symisc extension — §10) */
	sxu32 nLoopId;       /* Monotonic id handed to each loop/switch block as it is entered */
	sxu32 nCurLoopId;    /* Innermost loop/switch currently open (0 = none) */
	SySet aLoopParent;   /* aLoopParent[id-1] = enclosing loop id, so the ancestry of any loop
	                      * can be walked after compilation. php's only goto restriction is
	                      * "'goto' into loop or switch statement is disallowed": a jump is
	                      * illegal exactly when the LABEL sits in a loop that does not also
	                      * enclose the GOTO. Both ends record their loop id; the fixup pass
	                      * walks up from the goto's to look for the label's. */
	sxu32 nScopeId;      /* Monotonic id handed to each try/catch/finally block entered */
	sxu32 nCurScopeId;   /* Innermost such block currently open (0 = none) */
	SySet aScope;        /* aScope[id-1] = that block's GenScope: its enclosing scope id and
	                      * its kind. Same shape and purpose as aLoopParent above, for the
	                      * other after-the-fact goto question: a jump out of a try/catch is
	                      * legal exactly when the LABEL's scope also ENCLOSES the goto, and
	                      * what it must unwind on the way is read off the chain between them
	                      * (GenStateJumpScope). Comparing NESTING DEPTHS instead cannot tell
	                      * two sibling trys apart, which let a goto jump into one — skipping
	                      * its OP_LOAD_EXCEPTION, or landing in another bytecode array. */
	SyBlob sWorker;      /* General purpose working buffer */
	SyBlob sErrBuf;      /* Error buffer */
	SyBlob sNamespace;   /* Current namespace path (e.g. "App\\Models") */
	SyHash hUseImports;      /* use imports: short alias -> FQN (classes) */
	SyHash hUseFuncImports;  /* use function imports: short alias -> FQN */
	SyHash hUseConstImports; /* use const imports: short alias -> FQN */
	SyHash hSeenClass;       /* FQNs of the classes DECLARED so far in this compile unit */
	SyHash hSeenFunc;        /* FQNs of the functions DECLARED so far in this compile unit
	                          * (both: php refuses an import a declaration already took —
	                          * these outlive a namespace switch, unlike the import tables) */
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
	SyString sPendingClosureName; /* php's `{closure:SCOPE:LINE}` name built for the closure
	                      * whose body GenStateCompileFunc is about to compile. The caller
	                      * knows the 'function' keyword's LINE and the ENCLOSING scope; the
	                      * name must be on the ph7_vm_func before the body compiles, because
	                      * __FUNCTION__ inside it resolves at compile time. Consumed (and
	                      * cleared) the moment the function state is initialized. */
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
	sxu32 nCatchJmpPc;/* Pending loop jump parked by a break/continue that left a DETACHED catch
	                   * mini-program (OP_CATCH_JMP): its target pc in this body frame's
	                   * bytecode, 0 when none is armed. The sibling of bHasRet/sRet — the same
	                   * park-here, act-at-the-landing-pad contract, cleared by the same
	                   * VmClearFramePending. Consumed by the owning try's OP_POP_EXCEPTION. */
	sxu16 nCatchJmpLevels;/* Detached-container boundaries still to leave before it is taken */
	sxu16 nCatchJmpCross; /* Enclosing try activations to drain (run their finally) before it */
	sxu32 nCallLine;  /* Line of the OP_CALL that pushed this frame (0 for the global frame).
	                   * debug_backtrace() reports a frame's line as the line of the call
	                   * SITE, not of the code running inside it. */
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
 * One entry of a userland handler STACK (set_error_handler /
 * set_exception_handler). php's stack has no depth limit and every entry is a
 * real one -- the `null` a reset pushes included -- so each restore brings back
 * exactly what the matching set replaced, and nothing under it is lost.
 */
typedef struct VmHandlerSlot VmHandlerSlot;
struct VmHandlerSlot {
	ph7_value sCb;   /* the saved handler, MEMOBJ_NULL for a reset entry */
	sxi64 iLevels;   /* its set_error_handler() $error_levels (E_ALL elsewhere) */
};
/*
 * php 8's E_ALL. The default of error_reporting() AND of set_error_handler()'s
 * $error_levels, so both read it from here (E_STRICT/2048 left the set in php 8).
 */
#define PH7_E_ALL_MASK 30719
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
	SySet aByRefArg;          /* Caller slots (sxu32) this body's by-REFERENCE parameters
	                           * alias. The body outlives its caller's frame, so whichever
	                           * of the two dies last releases the slot: the caller's
	                           * teardown counts this frame's name as a holder and skips it,
	                           * and this ctx's teardown asks PH7_VmReleaseUnheldSlot once
	                           * its own names are gone. */
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
	SyBlob sOB;          /* Output buffer consumer (RAW bytes: php runs the
	                      * handler on the way OUT, not on the way in) */
	ph7_int64 iFlags;    /* PH7_OB_* below, php's own numeric values. 64 bits wide
	                      * because php stores whatever it was given (minus the two
	                      * nibbles it reserves) and reports it back verbatim. */
	ph7_int64 nChunk;    /* ob_start()'s $chunk_size (0 or negative: buffer
	                      * everything). 64 bits: php accepts a chunk larger than a
	                      * 32-bit count and reports it back. */
	ph7_int64 nSize;     /* php's ALLOCATION for this buffer, which ob_get_status()
	                      * reports: 16 KB, or the chunk size rounded up to 4 KB, and
	                      * grown by php's own rule on each write. Tracked rather than
	                      * derived because the answer depends on how the bytes
	                      * ARRIVED — 40 writes of 1000 give 49152 where one write of
	                      * 40000 gives 40960. */
};
/*
 * Output-handler flags and phases. These are php's own values: the first group is
 * what ob_get_status() reports in its `flags` entry, the second what the handler
 * receives as its `$phase` argument.
 */
#define PH7_OB_USER      0x0001 /* Handler is a userland callback */
#define PH7_OB_CLEANABLE 0x0010
#define PH7_OB_FLUSHABLE 0x0020
#define PH7_OB_REMOVABLE 0x0040
#define PH7_OB_STDFLAGS  0x0070
#define PH7_OB_STARTED   0x1000 /* Handler has been invoked at least once */
#define PH7_OB_DISABLED  0x2000 /* Handler answered FALSE: never called again */
#define PH7_OB_PROCESSED 0x4000 /* Handler has produced output */
/* What ob_start() keeps of the $flags it is given: everything except the phase
 * nibble and the state nibble, which are the engine's own to set. */
#define PH7_OB_FLAGMASK  (~(ph7_int64)0xF00F)
/* Phases (an op, plus PH7_OB_START until the handler has run once) */
#define PH7_OB_WRITE 0
#define PH7_OB_START 1
#define PH7_OB_CLEAN 2
#define PH7_OB_FLUSH 4
#define PH7_OB_FINAL 8
/*
 * mt_srand()/srand()'s $mode. php compares the argument against MT_RAND_PHP for
 * EQUALITY, so every other value — including an out-of-range one — selects the
 * standard generator.
 */
/* stream_wrapper_register()'s $flags: php defines this one bit. A wrapper that
 * declares itself a URL is the one allow_url_fopen and allow_url_include gate. */
#define PH7_STREAM_IS_URL 1
#define PH7_MT_RAND_MT19937 0
#define PH7_MT_RAND_PHP     1
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
	sxu32 nLine;      /* Source line of this `use ($x)` capture (0 = unknown/implicit):
					   * php reports an undefined by-value capture's E_WARNING at the
					   * variable's own line, which can differ from the closure keyword's
					   * line when the `use` clause wraps. Set only for explicit captures. */
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
#define VM_FUNC_BOUND        0x40000 /* Bound by an UNCONDITIONAL top-level declaration; a second such
                                      * binding of the same name fatals ("Cannot redeclare function ..."),
                                      * matching PHP. Conditional declarations are not marked. */
#define VM_FUNC_ARROW        0x80000 /* Arrow function (`fn()=>expr`): its aClosureEnv captures are ALL
                                      * implicit (auto-scanned from the body), never an explicit `use`
                                      * clause. php does not warn about an undefined auto-capture at
                                      * closure creation — the read fires the warning inside the body —
                                      * so the OP_LOAD_CLOSURE undefined-capture warning is suppressed. */
#define VM_FUNC_NATIVE       0x100000 /* The body is a C routine, not bytecode: ph7_vm_func::pNative
                                       * holds it and aByteCode stays EMPTY. OP_CALL branches to the
                                       * host-function path (no frame, no call record, no operand
                                       * stack) while every step BEFORE the branch — the sVmName
                                       * lookup, $this/self resolution, visibility — runs unchanged,
                                       * so a native method inherits, overrides and dispatches like
                                       * any other. This is what lets a builtin class own its C code
                                       * as a METHOD instead of a global `__prefix_verb` thunk. */
#define VM_FUNC_NATIVE_STATIC 0x200000 /* A VM_FUNC_NATIVE method declared static. Staticness is
                                       * otherwise recorded only on ph7_class_method::iFlags, which
                                       * the OP_CALL dispatcher does not hold — and it must know,
                                       * because the method path falls back to the CALLER's $this
                                       * when the target slot carries a class name rather than an
                                       * object. For a bytecode method that fallback is harmless;
                                       * for a native one it would hand the body a receiver on a
                                       * `C::m()` call and shift how it reads its arguments. Set by
                                       * the native builder only: the compiler's behaviour for
                                       * bytecode methods is deliberately left untouched. */
/* next free bit: 0x400000 */
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
	SyString sClosureName; /* A closure/arrow function's php-VISIBLE name, php 8.4's
	                      * `{closure:SCOPE:LINE}` (Zend/zend_compile.c, zend_begin_func_decl).
	                      * Built at COMPILE time — the scope part names the ENCLOSING
	                      * function, which only the compiler knows — and duplicated into the
	                      * VM allocator. sName stays the synthesized unique lookup key
	                      * ("[lambda_3]"); this is what __FUNCTION__, a backtrace, Reflection
	                      * and is_callable() report. nByte == 0 for anything but a closure. */
	void *pUserData;     /* Upper layer private data associated with this instance */
	void *pLsbClass;     /* For a closure: the late-static-binding class captured at its
	                      * creation site (ph7_class*), so `static::` inside the body
	                      * resolves like php. NULL for a plain function/method. */
	ph7_user_func *pNative; /* VM_FUNC_NATIVE only: the C body. A ph7_user_func rather than a
	                      * bespoke record because that struct ALREADY carries everything the
	                      * host-call path reads — xFunc, pUserData, sName, the min/max arity
	                      * bounds, zSig/zRet and nByRefMask — so the existing OP_CALL foreign
	                      * block, VmInitCallContext, ph7_context_user_data(), ph7_function_name()
	                      * and VmEnforceBuiltinArgTypes all work on it verbatim. It is NOT
	                      * registered in pVm->hHostFunction: it hangs off this method alone and
	                      * is reachable only through the method, never as a global name. */
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
	SyHash hAttr;         /* Class PROPERTIES [static + instance]. Constants live in hConst */
	SyHash hConst;        /* Class CONSTANTS [incl. enum cases] — php keeps constants and
	                       * properties in SEPARATE namespaces, so `const C` and `public $C`
	                       * coexist. Keyed by name, disjoint from hAttr. */
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
	void (*xRelease)(ph7_vm *,ph7_class_instance *); /* Native teardown for an instance of this class,
	                       * run by PH7_ClassInstanceRelease while the instance's slots are still
	                       * readable. This is NOT __destruct: php's WeakReference declares no
	                       * destructor, so a native class that must release a C-side resource
	                       * states it here instead of growing a method Reflection would report. */
	const PH7_NativeIterVtab *pIterVtab; /* How an InternalIterator walks an instance of this class
	                       * (php's get_iterator handler). Set on native IteratorAggregates whose
	                       * getIterator() answers PH7_NativeIteratorNew(); 0 everywhere else. */
	sxi32 (*xPresent)(ph7_vm *,ph7_class_instance *,ph7_value *,int); /* php's get_properties /
	                       * get_debug_info handlers, as one callback told which is asking:
	                       * the SHAPE a class SHOWS, which for several native classes is nothing
	                       * like the engine state it keeps. php presents a DateTime as
	                       * date/timezone_type/timezone and a WeakReference as ["object"], while
	                       * the slots underneath are a timestamp and a C-side cell — those slots
	                       * carry PH7_MOD_HIDDEN, and this fills an ARRAY with what php shows.
	                       * The last argument is 1 for the DEBUG surfaces (var_dump/print_r) and 0
	                       * for the property ones (var_export, the (array) cast), because php's
	                       * two handlers do not agree: a WeakReference shows ["object"] to
	                       * var_dump and NOTHING to (array), while a DateTime shows the same three
	                       * keys to both. Never consulted by get_object_vars()/foreach, which php
	                       * answers from the real (scoped) properties, nor yet by serialize(),
	                       * where php's answer is an __serialize/__unserialize pair. */
	const char *zNewRefusal; /* php's create_object refusal TEXT for a PH7_CLASS_NOINSTANTIATE
	                       * class, when it is not the usual "Instantiation of class %s is not
	                       * allowed". php words Directory's as "Cannot directly construct
	                       * Directory, use dir() instead"; 0 selects the standard sentence. */
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
#define PH7_CLASS_STATIC_DEFER 0x200 /* This class's static table is not fully materialized: at least
                                      * one static property's default THREW when it was evaluated at
                                      * mount (PH7_CLASS_ATTR_STATIC_DEFER on the attribute) or failed
                                      * its type check (VM_CLASS_ATTR_TYPE_DEFER on the slot). A hint
                                      * only: the access/instantiation sites call
                                      * PH7_VmMaterializeClassStatics, which re-scans the whole base
                                      * chain. Set on the class whose mount saw the failure; the gate
                                      * (VmClassStaticDeferPending) walks the bases, so mount ORDER
                                      * between a base and its subclass does not matter. */
#define PH7_CLASS_BOUND       0x100 /* Bound by an UNCONDITIONAL top-level declaration. PHP fatals on a
                                     * second such binding of the same name ("Cannot redeclare ..."); a
                                     * conditional (if/loop/func-nested) declaration is NOT marked, so the
                                     * `if(false){class C{}}` / `if(!class_exists){..}` guard idioms hoist. */
#define PH7_CLASS_NOCLONE     0x400 /* `clone $o` is a catchable Error for this class. A native class whose
                                     * instances own a C-side resource keyed by a private slot cannot be
                                     * copied slot-by-slot (WeakReference's shared cell would be dropped
                                     * twice), which is exactly why php makes those classes uncloneable.
                                     * Enum cases carry the same rule through PH7_CLASS_ENUM, and
                                     * Generator/Fiber are named directly at the OP_CLONE test. */
#define PH7_CLASS_NOSERIALIZE 0x800 /* serialize() of an instance is a catchable Exception naming the
                                     * class, php's answer for every class holding engine state.
                                     * Without it the default object path emits the private slots —
                                     * for these classes a raw POINTER, which unserialize() would
                                     * hand straight back to a method. php's ZEND_ACC_NOT_SERIALIZABLE:
                                     * tested FIRST and unconditionally, so a subclass declaring
                                     * __serialize() is refused too (DOMXPath is the case that shows
                                     * it). INHERITED — the serializer walks pBase, because php's flag
                                     * rides down to every user subclass. */
#define PH7_CLASS_NOSERIALIZE_SUBOK 0x2000 /* The SOFT refusal: php's `ce->serialize` deny HANDLER,
                                     * which the serializer consults only AFTER looking for
                                     * __serialize()/__sleep() — so a SUBCLASS that declares either
                                     * one serializes normally, and php says so in the sentence
                                     * ("…is not allowed, unless serialization methods are
                                     * implemented in a subclass"). The DOM node classes are the
                                     * users; __wakeup() alone does NOT rescue them. Inherited the
                                     * same way as the hard flag. */
#define PH7_CLASS_DIM_WRITABLE 0x4000 /* A write through `$obj[k]` LANDS on this class's storage.
                                       * php's split is the read_dimension handler: an internal
                                       * class whose own handler hands back the real element
                                       * (ArrayObject, ArrayIterator, WeakMap) supports indirect
                                       * modification, while everything routed through
                                       * zend_std_read_dimension — every userland ArrayAccess, and
                                       * the SPL classes that keep the standard handler — gets a
                                       * TEMPORARY, so php notices and drops the write. Inherited
                                       * by subclasses (the flag is looked up along pBase), but
                                       * only while the native offsetGet is still the one that
                                       * answers: an override takes the class off the fast handler
                                       * in php too. See PH7_VmDimFetchWritable. */
#define PH7_CLASS_NOINSTANTIATE 0x1000 /* `new C` is refused by the OBJECT-CREATION step, before the
                                     * constructor's visibility is ever consulted — php's
                                     * "Instantiation of class %s is not allowed", which its
                                     * create_object handler raises. The distinction is visible:
                                     * Closure's __construct is PRIVATE (Reflection prints it that
                                     * way), so without this flag `new Closure` reports a visibility
                                     * refusal ("Call to private Closure::__construct() from global
                                     * scope") where php reports the instantiation one. A class that
                                     * merely wants a private ctor does NOT want this bit. */
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
	const void *pNativeValue; /* A native class's literal initializer (PH7_NativeConstDef*), or 0.
	                      * A compiled declaration expresses its default as aByteCode evaluated at
	                      * mount; the C builder has no compiler to emit that, so it hands the
	                      * literal here and the mount writes it straight into the reserved slot.
	                      * Mutually exclusive with a non-empty aByteCode. */
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
#define PH7_CLASS_ATTR_REFBOUND     0x80000 /* STATIC property currently bound to another slot by `=&`
                                             * (`C::$s =& $x`). The instance side records this per
                                             * INSTANCE (VM_CLASS_ATTR_REFBOUND); a static has one slot
                                             * per declaration, so the bit lives here. It says the slot
                                             * this attribute points at is held by a COUNTED pin, and a
                                             * rebind must give that pin back rather than free a slot the
                                             * attribute never owned. */
#define PH7_CLASS_ATTR_HIDDEN       0x40000 /* A NATIVE class's engine slot: real storage that php keeps
                                            * in its own C struct and therefore never shows. Excluded
                                            * from every PRESENTATION surface — var_dump/print_r/
                                            * var_export, (array), get_object_vars, foreach, json_encode,
                                            * serialize, http_build_query and Reflection's property
                                            * listing — while `new`, clone and the native bodies' own
                                            * PH7_NativeAttr() reads still see it. Set from
                                            * PH7_MOD_HIDDEN on a PH7_NativePropDef. Use it for a slot
                                            * php shows NOTHING for (a handle, a cursor cache); a slot
                                            * php shows under a DIFFERENT name (ArrayObject's `storage`,
                                            * DateTime's `date`) wants the presentation hook §7.4 (e)
                                            * still asks for, not this bit. */
#define PH7_CLASS_ATTR_STATIC_DEFER 0x20000 /* STATIC property whose default initializer THREW when it
                                            * was evaluated at class mount. php never evaluates a static
                                            * default at declaration time — it materializes the class's
                                            * static table at the FIRST static-property access — so the
                                            * mount-time throw is raised MUTED (VmEvalDefaultMuted: no
                                            * catch runs, nothing is reported) and rolled back whole,
                                            * and the initializer re-runs at that first access
                                            * (PH7_VmMaterializeClassStatics), where php raises it.
                                            * Cleared once the initializer completes without throwing:
                                            * a class whose bad default is never READ stays silent in
                                            * both engines, and a re-run that now succeeds (the constant
                                            * it names was define()d after the declaration) answers the
                                            * value, as php's does. */
/* next free bit: 0x40000 */
/*
 * Declaring a class from C (oo_native.c).
 *
 * A subsystem describes its classes as static tables and hands them to
 * PH7_InstallNativeClasses(), which drives the very builders the compiler drives
 * for `class Foo {}`. The point of the exercise is the METHOD table: a method's
 * body may be a C routine (VM_FUNC_NATIVE), so the engine-access helpers that had
 * to be global `__prefix_verb()` thunks — because only a global function could be
 * C — become methods of the class they always belonged to.
 */
/* Member modifiers. Visibility defaults to public when none is given. */
#define PH7_MOD_PUBLIC     0x00
#define PH7_MOD_PROTECTED  0x01
#define PH7_MOD_PRIVATE    0x02
#define PH7_MOD_STATIC     0x04
#define PH7_MOD_FINAL      0x08
#define PH7_MOD_ABSTRACT   0x10 /* No body: an interface's method, or an abstract declaration */
#define PH7_MOD_HIDDEN     0x20 /* PROPERTY only: an engine slot php keeps in its own struct and
                                 * never presents (PH7_CLASS_ATTR_HIDDEN). */
#define PH7_MOD_READONLY   0x40 /* PROPERTY only: php's `readonly` (PH7_CLASS_ATTR_READONLY) */
#define PH7_MOD_PROT_SET   0x80 /* PROPERTY only: php's `protected(set)` asymmetric visibility */
#define PH7_MOD_PRIV_SET   0x100 /* PROPERTY only: php's `private(set)` asymmetric visibility */
/* Literal kinds a native class constant may carry */
#define PH7_NATIVE_VAL_NULL   0
#define PH7_NATIVE_VAL_INT    1
#define PH7_NATIVE_VAL_STRING 2
#define PH7_NATIVE_VAL_BOOL   3
#define PH7_NATIVE_VAL_DOUBLE 4
#define PH7_NATIVE_VAL_ARRAY  6 /* The EMPTY array, php's `private array $trace = [];`. The
                                 * only array literal a stub default needs — anything with
                                 * elements would want the compiler's byte-code. */
#define PH7_NATIVE_VAL_NONE   5 /* On a PROPERTY row only: the slot has NO default at all,
                                 * php's `public string $name;`. It needs a declared zType to
                                 * mean anything (an untyped slot without a default is null),
                                 * and it makes the property UNINITIALIZED at `new` — reading
                                 * it before the class's own C body writes it is php's
                                 * "must not be accessed before initialization" Error, and
                                 * hasDefaultValue() answers false. PH7_NATIVE_VAL_NULL is the
                                 * different thing it reads like: an explicit `= null`. */
typedef struct PH7_NativeMethodDef PH7_NativeMethodDef;
typedef struct PH7_NativeConstDef  PH7_NativeConstDef;
typedef struct PH7_NativeClassSpec PH7_NativeClassSpec;
struct PH7_NativeMethodDef
{
	const char *zName;       /* php-visible method name */
	sxi32 iMods;             /* PH7_MOD_* */
	const char *zSig;        /* PHP-style parameter list ("string $name, int $flags = 0"),
	                          * or 0 for "unenforced". Static storage: never freed. Drives
	                          * arity enforcement, the by-ref mask AND Reflection, from the
	                          * one string — the same contract aBuiltinSig[] has. */
	const char *zRet;        /* Return-type text, or 0 */
	ProchHostFunction xFunc; /* The body */
};
struct PH7_NativeConstDef
{
	const char *zName;
	sxi32 iMods;
	sxi32 iType;             /* PH7_NATIVE_VAL_* */
	ph7_int64 iValue;        /* INT / BOOL */
	const char *zValue;      /* STRING */
	double rValue;           /* DOUBLE */
};
/*
 * A declared property. Its default is the same literal record a constant uses --
 * a compiled declaration would carry compiled byte-code here, which the builder
 * has no compiler to emit, so the value is stated directly and materialized at
 * `new` (instance) or at mount (static) by PH7_NativeLiteralValue.
 */
typedef struct PH7_NativePropDef PH7_NativePropDef;
struct PH7_NativePropDef
{
	const char *zName;
	sxi32 iMods;             /* PH7_MOD_* (STATIC supported; FINAL ignored) */
	PH7_NativeConstDef sDefault; /* iType PH7_NATIVE_VAL_NULL = plain null default,
	                              * PH7_NATIVE_VAL_NONE = no default at all (typed slots) */
	const char *zType;       /* Declared type as php writes it ("?string", "int", "DateInterval"),
	                          * or 0 for an untyped slot. Enforced on every store and printed by
	                          * Reflection exactly as a compiled `public ?string $p` would be —
	                          * php declares a type on every property it presents, so a slot the
	                          * class SHOWS wants one. Single atoms only (a leading `?` plus one
	                          * scalar keyword or class name); a union needs the compiler's
	                          * alternative set and is not expressible here. */
};
struct PH7_NativeClassSpec
{
	const char *zName;
	const char *zParent;     /* or 0 */
	const char *zImplements; /* comma-separated list, or 0 */
	sxi32 iFlags;            /* PH7_CLASS_FINAL / ABSTRACT / INTERFACE / READONLY */
	const PH7_NativeMethodDef *aMethod; sxu32 nMethod;
	const PH7_NativeConstDef  *aConst;  sxu32 nConst;
	const PH7_NativePropDef   *aProp;   sxu32 nProp;
	void (*xRelease)(ph7_vm *,ph7_class_instance *); /* or 0; see ph7_class::xRelease */
	const PH7_NativeIterVtab *pIterVtab; /* or 0; see ph7_class::pIterVtab */
	sxi32 (*xPresent)(ph7_vm *,ph7_class_instance *,ph7_value *,int); /* or 0; see ph7_class::xPresent */
};
/*
 * One `case Name = <literal>;` of a native ENUM. The backing value is the same
 * literal record a constant carries; iType PH7_NATIVE_VAL_NULL is a PURE enum's
 * case, which has no value at all.
 */
typedef struct PH7_NativeEnumCase PH7_NativeEnumCase;
struct PH7_NativeEnumCase
{
	const char *zName;
	PH7_NativeConstDef sValue;   /* the backing literal; zName/iMods unused */
};
PH7_PRIVATE sxi32 PH7_InstallNativeClasses(ph7_vm *pVm,const PH7_NativeClassSpec *aSpec,sxu32 nSpec);
PH7_PRIVATE int PH7_ClassInstancePresent(ph7_class_instance *pThis,ph7_value *pOut,int bDebug);
PH7_PRIVATE sxi32 PH7_InstallEnumInterfaceMethods(ph7_vm *pVm,ph7_class *pClass);
PH7_PRIVATE sxi32 PH7_InstallNativeEnum(ph7_vm *pVm,const char *zName,sxu32 nBacking,
	const PH7_NativeEnumCase *aCase,sxu32 nCase,
	const PH7_NativeMethodDef *aMethod,sxu32 nMethod);
PH7_PRIVATE sxi32 PH7_NativeClassInstallMethod(ph7_vm *pVm,ph7_class *pClass,
	const PH7_NativeMethodDef *pDef,void *pUserData);
/*
 * Attach one `#[Name(literal, ...)]` to a class declared from C. php puts an
 * attribute on two of its own attribute classes, and the whole record — the FQN
 * plus its arguments — is what the engine reads to VALIDATE a target and what
 * ReflectionAttribute answers.
 */
typedef struct PH7_NativeAttrArg PH7_NativeAttrArg;
struct PH7_NativeAttrArg
{
	const char *zName;           /* named argument, or 0 for a positional one */
	PH7_NativeConstDef sValue;   /* the literal; zName/iMods unused */
};
PH7_PRIVATE sxi32 PH7_NativeClassAddAttribute(ph7_vm *pVm,ph7_class *pClass,
	const char *zAttr,const PH7_NativeAttrArg *aArg,sxu32 nArg);
PH7_PRIVATE sxi32 PH7_NativeClassInstallProperty(ph7_vm *pVm,ph7_class *pClass,
	const PH7_NativePropDef *pDef);
PH7_PRIVATE void PH7_NativeLiteralValue(ph7_vm *pVm,const void *pLiteral,ph7_value *pOut);
PH7_PRIVATE void PH7_NativeSetProp(ph7_vm *pVm,ph7_class_instance *pObj,
	const char *zProp,sxu32 nProp,ph7_value *pSrcVal);
/*
 * Reading and writing a native instance's own declared slots. Every native class
 * does this constantly (the date family had a private copy of the whole set), so
 * the accessors live with the builder that declares the slots.
 */
PH7_PRIVATE ph7_value * PH7_NativeAttr(ph7_class_instance *pObj,const char *zName);
PH7_PRIVATE int PH7_NativeAttrIsUninit(ph7_class_instance *pObj,const char *zName);
PH7_PRIVATE sxi64 PH7_NativeAttrInt(ph7_class_instance *pObj,const char *zName);
PH7_PRIVATE ph7_class_instance * PH7_NativeAttrObj(ph7_class_instance *pObj,const char *zName);
PH7_PRIVATE int PH7_NativeAttrTruthy(ph7_class_instance *pObj,const char *zName);
PH7_PRIVATE void PH7_NativeAttrStr(ph7_class_instance *pObj,const char *zName,
	const char **pzOut,int *pnOut);
PH7_PRIVATE void PH7_NativeSetAttrInt(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,sxi64 iVal);
PH7_PRIVATE void PH7_NativeSetAttrStr(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,
	const char *zVal,int nVal);
PH7_PRIVATE void PH7_NativeSetAttrBool(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,int bVal);
PH7_PRIVATE void PH7_NativeSetAttrObj(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,
	ph7_class_instance *pVal);
PH7_PRIVATE void PH7_NativeResultObject(ph7_context *pCtx,ph7_class_instance *pObj);
/*
 * php's InternalIterator: the Iterator a native IteratorAggregate answers when the
 * PHP it replaced was a GENERATOR -- the one body a C method cannot be. It is one
 * class in php too, wrapping whatever internal iterator the aggregate handed over,
 * so PHL gives it the same shape: fixed state slots on the iterator, and a vtable
 * on the AGGREGATE'S CLASS (ph7_class::pIterVtab, php's get_iterator handler) that
 * knows how to position and advance that aggregate's cursor. current()/key()/valid()
 * need no vtable entry -- they read the slots the two below leave behind.
 */
struct PH7_NativeIterVtab
{
	void (*xRewind)(ph7_vm *pVm,ph7_class_instance *pIt); /* settle on the first element */
	void (*xNext)(ph7_vm *pVm,ph7_class_instance *pIt);   /* settle on the one after */
};
/* The state slots, private to InternalIterator and shared by every vtable:
 * the aggregate, the value and key at the cursor, an integer cursor and a spare
 * one for the vtable's own bookkeeping, and whether the walk is over. */
#define PH7_NATIVE_IT_SRC  "__src"
#define PH7_NATIVE_IT_CUR  "__cur"
#define PH7_NATIVE_IT_KEY  "__key"
#define PH7_NATIVE_IT_POS  "__pos"
#define PH7_NATIVE_IT_AUX  "__aux"
#define PH7_NATIVE_IT_DONE "__done"
PH7_PRIVATE sxi32 PH7_VmInstallNativeIterator(ph7_vm *pVm);
PH7_PRIVATE ph7_class_instance * PH7_NativeIteratorNew(ph7_vm *pVm,ph7_class_instance *pSrc);
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
 * ph7_class_instance::iFlags bit set while the object's __clone() magic method
 * runs. PHP 8.3 lets __clone() re-initialize the cloned object's readonly
 * properties, so the readonly store guard consults this flag on the executing
 * $this. (Other iFlags bits are declared privately in their owning .c file:
 * 0x001 destroyed, 0x002 dumping, 0x004 fcc-bound.)
 */
#define VM_INSTANCE_CLONING 0x008
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
	sxu8  bStrict; /* strict_types mode of the COMPILATION UNIT this instruction came from.
	                * Stamped by PH7_VmEmitInstr beside nLine, and published to
	                * pVm->bCurStrict under the same nLine != 0 gate, so an engine-dispatched
	                * call (a magic method, a property hook) can bind its arguments under the
	                * CALLING file's mode — php's rule — instead of always coercing. Sits in
	                * the padding after iOp: sizeof(VmInstr) is unchanged. */
	sxi32 iP1; /* First operand */
	sxu32 iP2; /* Second operand (Often the jump destination) */
	void *p3;  /* Third operand (Often Upper layer private data) */
	sxu32 nLine; /* Source line this instruction was compiled from (0 = unknown).
	              * Stamped by PH7_VmEmitInstr from the codegen's current token, so
	              * every one of its ~150 call sites keeps its signature. */
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
	sxu32 nNewClassInstr;/* Instruction index + 1 of the class-name push, for a call node
	                      * that is a `new`'s operand. The reorder puts that push before
	                      * the constructor arguments, so the NEW codegen can no longer
	                      * find it by peeking one instruction back. 0 = unset. */
	sxu8 bArgShapes;     /* 1 when the two masks below describe THIS call's argument
						  * positions. The compiler sets it for every call whose actual
						  * positions survive to the runtime stack unchanged — i.e. no
						  * spread and at most 31 arguments. 0 means "unknown shapes":
						  * the by-ref binders fall back to their runtime nIdx test. */
	sxu32 nNonLvalMask;  /* bit N: argument N is a HARD non-lvalue (a literal, an operator
						  * result, a cast, a class constant, `@$x`, `$o?->p`, an assignment
						  * — php's `zend_is_variable` says no and it is not a call either).
						  * Binding one to a by-ref parameter is php's catchable
						  * `Argument #N ($p) could not be passed by reference` Error, raised
						  * at the CALL before the callee's ZPP runs. */
	sxu32 nTempCallMask; /* bit N: argument N is the RESULT of a call or a `new` — php's
						  * SEND_VAR_NO_REF: an E_NOTICE ("Only variables should be passed
						  * by reference") and then it operates on the temporary. */
	sxu32 nTotal;        /* Total number of compile-time arguments */
	SyString *aNames;    /* Array of nTotal names. nByte==0 means positional. */
	SyString sAssertSrc; /* Direct assert() calls only: the first argument's rendered
						  * source text (php's zend_ast_export shape, e.g. `1 == 2`),
						  * captured at compile time so a failing assertion reports
						  * `assert(1 == 2)` like php instead of the evaluated value.
						  * {0,0} for every other call site; bytes live in the VM
						  * allocator. See PH7_GenRenderAssertSpan (compile_literal.c). */
};
/*
 * A class declaration whose parent/interface/trait could not be resolved at
 * compile time (the enclosing file's statements — spl_autoload_register — had
 * not executed yet). The compiler captures the declaration's SOURCE plus a
 * reconstructed namespace/use-import prefix and defers the whole compile to
 * OP_CLASS_DEFER at the declaration's execution point (VmExecDeferredClass,
 * vm_include.c), where the autoloader is live. aRequired lists the names that
 * were missing; each still-missing one throws php's catchable
 * `Class/Interface/Trait "X" not found` Error before the re-compile runs.
 */
typedef struct VmDeferredReq VmDeferredReq;
struct VmDeferredReq
{
	SyString sName;  /* Fully-qualified name (allocator-owned) */
	sxu8 cKind;      /* PH7_DEFER_KIND_* — picks the not-found noun */
};
#define PH7_DEFER_KIND_CLASS     0
#define PH7_DEFER_KIND_INTERFACE 1
#define PH7_DEFER_KIND_TRAIT     2
typedef struct VmDeferredClass VmDeferredClass;
struct VmDeferredClass
{
	SyString sText;     /* Re-compilable chunk: namespace + use-imports prefix +
						 * the declaration source (anon: wrapped in `if (false) { new ... }`) */
	SyString sSelfName; /* FQN the compile must install (anon: the synthesized name) —
						 * the post-eval existence check */
	SyString sAnonName; /* Synthesized anonymous-class name to inject via
						 * pVm->sDeferAnonName ({0,0} for a named declaration) */
	SySet aRequired;    /* VmDeferredReq — names unresolved at compile time */
	sxu32 nLine;        /* Declaration line (diagnostics) */
	sxu8 bDone;         /* 1 once the declaration executed successfully (idempotent site) */
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
#define VM_CLASS_ATTR_TYPE_DEFER 0x04 /* Typed STATIC property whose eagerly-evaluated DEFAULT failed
                                       * its type check at class mount. php evaluates static defaults
                                       * lazily, so the failure is deferred: any static-property access
                                       * on the class (read/write/isset, any property) and any
                                       * instantiation throws the catchable "Cannot assign <kind> to
                                       * property C::$s of type T" TypeError; a never-touched class
                                       * stays silent. Raised by PH7_VmMaterializeClassStatics, which
                                       * also sets the flag when a DEFERRED default (the sibling
                                       * PH7_CLASS_ATTR_STATIC_DEFER) evaluates at first access and
                                       * only then fails its type check. */
#define VM_CLASS_ATTR_REFBOUND 0x02 /* Property is bound to a reference (`$o->p =& $x`): its nIdx
                                    * slot is SHARED with (and pinned by) the source variable, so
                                    * PH7_VmReleaseInstanceAttr must NOT release/recycle it — the
                                    * surviving alias would dangle. Mirrors the use(&$x) pin. */
 /* Forward reference */
typedef struct VmSlot VmSlot;
struct VmSlot
{
	sxu32 nIdx;      /* Index in pVm->aMemObj[] */
	void *pUserData; /* Upper-layer private data */
};
typedef struct VmRefObj VmRefObj;
/* Reference-object body (vm.c reference machinery; shared with vm_builtin_var.c's unset) */
struct VmRefObj
{
	SySet aReference;  /* Table of references to this memory object */
	SySet aArrEntries; /* Foreign hashmap entries [i.e: array(&$a) ] */
	sxu32 nIdx;        /* Referenced object index */
	sxu32 nPin;        /* Holders the table cannot name, COUNTED so the last one to go
	                    * can release the slot: reference-bound properties (one per
	                    * binding, dropped when the property is released or re-bound).
	                    * A slot pinned by a site that never unpins (a use(&$x) capture,
	                    * a static, an enum case) leaves this 0 and relies on the
	                    * VM_REF_IDX_KEEP flag alone, which is a permanent pin. */
	sxi32 iFlags;      /* Configuration flags */
	VmRefObj *pNextCollide,*pPrevCollide; /* Collision link */
	VmRefObj *pNext,*pPrev;               /* List of all referenced objects */
};
#define VM_REF_IDX_KEEP  0x001 /* Do not restore the memory object to the free list */
/* VmObEntry struct moved to ph7int.h */

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
	SySet *pByteCode;/* Compiled instructions of a DETACHED catch body (the path every
	                  * non-generator try takes; NULL for a ROOT C inline catch, which compiles
	                  * into the function's own array). Heap-allocated so its ADDRESS is stable:
	                  * a `break`/`continue` inside the body records this container in its
	                  * JumpFixup, resolved long after PH7_CompileCatch returned and after
	                  * sEntry grew (both would move an embedded SySet). */
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
	sxi32 iErrSuppress;/* '@' suppression depth at try entry. A throw from inside `@expr`
	                    * unwinds past the ERR_CTRL that would have closed the window, so
	                    * the catch restores this snapshot instead of leaking the depth —
	                    * and a try/catch nested INSIDE an `@` still stays suppressed. */
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
#define PH7_ASSERT_ZEND_OFF   0x20  /* zend.assertions < 1: assert() compiled out (php CLI default -1) */
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
 *   VM_HOOK_PEND_RMW_MAGIC  — read-modify-write on an OVERLOADED property
 *                             (`$o->n++`, `$o->n .= 'x'`): the same scratch-slot
 *                             rail as VM_HOOK_PEND_RMW, with __get having
 *                             provided the current value and __set(sName, value)
 *                             taking the computed one.
 *   VM_HOOK_PEND_RMW_DIM    — `$o[$k] op= v` on an ArrayAccess element (php's
 *                             ASSIGN_DIM_OP): offsetGet($k) provided the current
 *                             value and offsetSet($k, value) takes the computed
 *                             one. The KEY lives in its own reserved memobj,
 *                             whose index this kind keeps in nBackIdx.
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
#define VM_HOOK_PEND_RMW_MAGIC   3
#define VM_HOOK_PEND_RMW_DIM     4
/* The SCRATCH-slot kinds: armed by the fetch (OP_MEMBER / OP_LOAD_IDX) and
 * consumed by the modify op's tail through VmHookRmwConsume, matched by the
 * scratch index the fetch left in pTos->nIdx. */
#define VM_HOOK_PEND_IS_RMW(iKind) \
	((iKind) == VM_HOOK_PEND_RMW || (iKind) == VM_HOOK_PEND_RMW_MAGIC \
	 || (iKind) == VM_HOOK_PEND_RMW_DIM)
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

/* A shared weak cell: one per weakly-referenced target instance. pObj nulls
 * when the target is released (the PH7_ClassInstanceRelease hook); nRef
 * counts the PHP-side handles (WeakReference objects, WeakMap entries). */
typedef struct VmWeakCell VmWeakCell;
struct VmWeakCell
{
	ph7_class_instance *pObj; /* target instance; 0 once dead */
	ph7_class_instance *pRef; /* the ONE WeakReference handed out for pObj, so
	                           * WeakReference::create($o) answers the same object
	                           * twice as php's does. NOT owned: the WeakReference's
	                           * own release nulls it. */
	sxu32 nRef;               /* PHP-side handle count */
};
/* The OPEN directory handle behind one DirectoryIterator (php's u.dir.dirp).
 * Registered per instance rather than in a property slot: a slot holding a C
 * pointer would be compared by `==` (php's two equal-positioned iterators are
 * equal) and COPIED by clone, and php's clone opens the directory again. The
 * class's xRelease closes it. */
typedef struct VmDirHandle VmDirHandle;
struct VmDirHandle
{
	const ph7_io_stream *pStream; /* device that opened it */
	void *pHandle;                /* its handle */
	ph7_class_instance *pThis;    /* the owning instance -- and the hash KEY's bytes,
	                               * which SyHashInsert borrows rather than copies */
};
/* One -d/-c php.ini directive queued for the INI chunk (name/value are
 * allocator-owned copies; see PH7_VM_CONFIG_INI_ENTRY) */
typedef struct VmIniEntry VmIniEntry;
struct VmIniEntry
{
	SyString sName;
	SyString sValue;
};
/*
 * One live php.ini directive. The table was an embedded-PHP array on a private
 * `__IniS` class; it is C now, seeded lazily on the first INI call from the static
 * defaults merged with the CLI's -d/-c queue (aIniCli).
 *
 * php exposes both a global_value and a local_value per directive: ini_set() moves
 * the local one, ini_restore() puts the global one back, and get_cfg_var() answers
 * the global one. Both blobs live on the VM allocator, so they are freed with it --
 * no release hook, exactly as aIniCli needs none.
 */
/*
 * The diagnostics of the last date parse — DateTime::getLastErrors()'s whole answer.
 *
 * php keeps one such record per request and both date classes read it, so this is a
 * VM field rather than the PHL-only `public static DateTime::$__dtLastErr` it used to
 * be. Every message is a static literal owned by the parser, so nothing here owns
 * memory and the record needs no release hook. The kept-vs-total split is php's:
 * `error_count` counts every error the scan raised, while the `errors` map holds one
 * entry per POSITION (a later error at a position php has already reported replaces
 * the message rather than adding a row).
 */
#define PH7_DT_MAX_WARN 3
#define PH7_DT_MAX_ERR  8
typedef struct phl_dt_lasterr phl_dt_lasterr;
struct phl_dt_lasterr
{
	sxu8 bSet;                        /* 0 -> getLastErrors() answers php's `false` */
	int nWarn;                        /* warning_count (total) */
	int nWarnKept;                    /* rows in the warnings map */
	int aWarnPos[PH7_DT_MAX_WARN];
	const char *azWarn[PH7_DT_MAX_WARN];
	int nErr;                         /* error_count (total) */
	int nErrKept;                     /* rows in the errors map */
	int aErrPos[PH7_DT_MAX_ERR];
	const char *azErr[PH7_DT_MAX_ERR];
};
typedef struct VmIniSlot VmIniSlot;
struct VmIniSlot
{
	SyString sName;   /* static default name, or a VM-lifetime dup of a CLI name */
	sxi32 iAccess;    /* INI_USER|INI_PERDIR|INI_SYSTEM bitmask php reports */
	SyBlob sGlobal;   /* php's global_value */
	SyBlob sLocal;    /* php's local_value (what ini_get answers, modulo live wiring) */
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
	SyPRNGCtx sPrng;            /* PRNG context (engine-internal, OS-seeded entropy) */
	SyMT19937Ctx sMt;           /* MT19937 backing rand()/mt_rand(); reset by srand()/mt_srand() */
	sxi32 mtSeeded;             /* TRUE once sMt holds a seed (lazy: first draw seeds from the OS CSPRNG, like PHP) */
	SySet aMemObj;              /* Object allocation table */
	SySet aLitObj;              /* Literals allocation table */
	ph7_value *aOps;            /* Operand stack */
	SySet aFreeObj;             /* Stack of free memory objects */
	SyHash hClass;              /* Compiled classes container */
	SyHash hConstant;           /* Host-application and user defined constants container */
	SyHash hHostFunction;       /* Host-application installable functions */
	SyHash hFunction;           /* Compiled functions */
	SyHash hSuper;              /* Superglobals hashtable */
	SyHash hPDO;                /* PDO installed drivers */
	SyBlob sConsumer;           /* Default VM consumer [i.e Redirect all VM output to this blob] */
	SyBlob sWorker;             /* General purpose working buffer */
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
	int bRenderingUncaught;     /* TRUE while the uncaught-exception report is being rendered. That
	                             * report asks the exception for its own trace (getTraceAsString,
	                             * userland code), so anything that throws in there would re-enter
	                             * the renderer and recurse until the process dies — which is exactly
	                             * what a bad max-arity stamp did (18 GB before the OOM killer, 19 Jul
	                             * 2026). The guard makes the second entry fall back to the
	                             * synthesized trace instead of looping. */
	int bCompilingBuiltin;      /* TRUE while the embedded builtin PHP library chunks compile at VM
	                             * init: classes/functions defined then are stamped INTERNAL so
	                             * Reflection reports isInternal() like Zend does for C-level code. */
	int bReflectBypass;         /* Consume-once: the next method OP_CALL skips the visibility
	                             * check (ReflectionMethod::invoke bypasses protection like PHP
	                             * 8.1+). Cleared by the check site; never survives past one call. */
	int bCallbackWeak;          /* Consume-once: the next OP_CALL is an INTERNAL function invoking a
	                             * userland callback (array_map, usort, an autoloader, a shutdown
	                             * function, Reflection's invoke, Closure::call), which php runs in
	                             * WEAK mode however strict the file that reached the builtin is —
	                             * there is no "calling file" at such a boundary. The two php
	                             * FORWARDS, call_user_func and call_user_func_array, do not set it:
	                             * they pass the caller's own mode on an argument map. Cleared at
	                             * the head of OP_CALL like the latches below. */
	int bMagicDispatch;         /* Consume-once: the next method OP_CALL is the ENGINE reaching for
	                             * a magic method (PH7_VmCallMagicMethod), so the visibility check
	                             * lets a non-public one through — php only WARNS at such a
	                             * declaration and dispatches anyway. Set at the engine's own
	                             * dispatch sites only, so a call the USER wrote (including a
	                             * first-class `$o->__get(...)`, which reaches the same C
	                             * dispatcher) is still denied. Cleared at the head of OP_CALL. */
	int bClosureScreened;       /* Consume-once: the method call now being dispatched comes out of a
	                             * Closure whose callee was RESOLVED and screened when the closure was
	                             * BUILT (`$this->p(...)`, `Closure::fromCallable([$this,'p'])`,
	                             * ReflectionMethod::getClosure), so no site may re-decide its
	                             * visibility against the CALLER. php stores a resolved function +
	                             * scope in the Closure and never looks the name up again; PHL keeps a
	                             * name, so without this latch an escaped closure over a private method
	                             * died at the invocation php runs. Armed by VmClosureUnwrap, read by
	                             * the array-callable dispatch sites, cleared at the head of OP_CALL. */
	char zDefTz[68];            /* date_default_timezone_set() identifier, stored verbatim like php
	                             * (default "UTC"; only UTC/GMT are accepted — no tz database) */
	sxu32 nDefTz;               /* zDefTz length in bytes */
	phl_dt_lasterr sDtLastErr;  /* DateTime::getLastErrors()'s answer. Was a PHL-only
	                             * `public static $__dtLastErr` on DateTime, a property php has
	                             * no equivalent of; both date classes read this field now. */
	SySet aShutdown;            /* Stack of shutdown user callbacks */
	SySet aIniCli;              /* php.ini directives from the CLI (-d/-c): VmIniEntry copies,
	                             * merged into aIniTab when the directive table is seeded */
	SySet aIniTab;              /* The live directive table (VmIniSlot), sorted by name so
	                             * ini_get_all() needs no sort of its own */
	sxu8 bIniSeeded;            /* aIniTab has been built (lazily, on the first INI call) */
	/* Session state. Was a private `__SessS` class with five static properties, which
	 * the INI subsystem had to reach into to live-wire session.name/session.save_path;
	 * both subsystems read these fields now, so neither depends on the other's shape. */
	sxi32 iSessStatus;          /* PHP_SESSION_NONE / _ACTIVE */
	SyBlob sSessId;             /* current session id ("" = none yet) */
	SyBlob sSessName;           /* cookie/session name (default "PHPSESSID") */
	SyBlob sSessPath;           /* save path ("" = not resolved yet -> sys_get_temp_dir()) */
	sxu8 bSessWired;            /* the shutdown writer + cookie have been installed once */
	SyHash hWeakCell;           /* instance pointer bytes -> VmWeakCell* (weak-reference registry;
	                             * PH7_ClassInstanceRelease kills matching cells on free) */
	SyHash hDirHandle;          /* instance pointer bytes -> VmDirHandle* (the open DIR* behind a
	                             * DirectoryIterator; the class's xRelease closes and unregisters) */
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
	VmClassAttr *pRefTargetAttr; /* Pending reference-store target (`$o->p =& $x`): OP_MEMBER tagged
	                             * PH7_MEMBER_REF_TARGET resolved the instance property slot and
	                             * stashed it here; the immediately-following member-marked
	                             * OP_STORE_REF rebinds it to alias the source variable's slot.
	                             * One-instruction lifetime by construction. */
	ph7_class_attr *pRefTargetStaticAttr; /* Same, for a static-property target (`self::$s =& $x`). */
	ph7_class_instance *pRefTargetThis;   /* Instance owning pRefTargetAttr; retained (iRef++) by
	                             * OP_MEMBER, released by the consuming OP_STORE_REF. */
	SySet aHookRmw;             /* Pending property-hook read-modify-write write-backs (LIFO;
	                             * VmHookRmw entries — see the struct above ph7_vm). */
	ph7_class_instance *pMagicCallThis; /* Pending __call receiver (band A #3b): OP_MEMBER hit a
	                             * missing (or inaccessible) method on a class declaring
	                             * __call/__callStatic and marked the callee slot
	                             * MEMOBJ_AUX_MAGICCALL; the packing body OP_CALL then runs
	                             * (VmMagicCallDispatch) consumes this + the class + the original
	                             * name. Holds a reference; NULL for __callStatic. */
	ph7_class *pMagicCallClass; /* Pending __call/__callStatic declaring class */
	ph7_class *pConstEvalClass; /* Transient: class whose constant/property initializer bytecode is
	                             * being evaluated (VmLocalExec has no method frame, so self::/parent::
	                             * inside an initializer resolve through this fallback — consulted by
	                             * PH7_VmPeekDeclaringClass/PH7_VmPeekTopClass when no frame matches). */
	void *pConstEvalFrame;      /* The VmFrame that was current when an ON-DEMAND const initializer
	                             * eval began (VmLocalExec pushes no frame). While the current frame
	                             * still equals it, self::/parent:: resolve to pConstEvalClass even
	                             * though an outer method frame exists (e.g. Base::CONST accessed from
	                             * Sub::method() must NOT resolve self to Sub). A method call inside the
	                             * initializer pushes a new frame, so the marker no longer matches and
	                             * that method's own declaring class wins. NULL outside on-demand eval. */
	sxi32 nConstEvalDepth;      /* Nesting depth of constant/enum-case initializer evaluations. A
	                             * cycle detected at an inner level (pConstCycleAttr) is thrown only
	                             * when depth returns to 0 — a throw INSIDE an initializer mini-exec
	                             * cannot be routed to a user catch (pre-existing engine restriction),
	                             * so the outermost, opcode-level evaluation raises it instead. */
	ph7_class_attr *pConstCycleAttr;  /* Self-referencing constant detected during evaluation */
	ph7_class *pConstCycleClass;      /* ...and the class it belongs to (for the Error message) */
	SyBlob sMagicCallName;      /* Pending original method name (stable copy) */
	ph7_user_func *pMagicCallFunc; /* The __call/__callStatic packing body's function record, built on
	                             * first use (PH7_VmMagicCallFunc) and NOT registered in
	                             * hHostFunction: OP_CALL points straight at it, so the dispatch has
	                             * no PHP-visible name to reach it by. */
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
	ph7_value sExceptionCB;    /* ACTIVE set_exception_handler() handler */
	ph7_value sErrCB;          /* ACTIVE set_error_handler() handler */
	sxi64 iErrCBLevels;        /* sErrCB's $error_levels mask: a handler is only called for the
	                            * levels it was REGISTERED for, and every other one falls
	                            * through to the engine's own reporting. Read at full width --
	                            * php ANDs a zend_long, so 2^32+1024 still selects
	                            * E_USER_NOTICE. */
	SySet aExceptionCBSaved;   /* VmHandlerSlot stack underneath sExceptionCB */
	SySet aErrCBSaved;         /* VmHandlerSlot stack underneath sErrCB */
	void *pStdin;              /* STDIN IO stream */
	void *pStdout;             /* STDOUT IO stream */
	void *pStderr;             /* STDERR IO stream */
	int bErrReport;            /* TRUE to report all runtime Error/Warning/Notice */
	int bDisplayErrors;        /* display_errors ini gate: TRUE emits the DISPLAY copy of a
	                            * runtime diagnostic (`\nWarning: msg in F on line N`) to the
	                            * program output stream (stdout). php CLI default: off. */
	int bLogErrors;            /* log_errors ini gate: TRUE emits the LOG copy of a runtime
	                            * diagnostic (`PHP Warning:  msg in F on line N`) to the error
	                            * stream (stderr via sVmErrConsumer). php CLI default: on. */
	int bGcEnabled;            /* gc_enable()/gc_disable() state reported by gc_enabled()/
	                            * gc_status(); PHL frees by refcount, so the cycle collector
	                            * is a no-op and this flag is purely observational. */
	sxi32 iErrMask;      /* error_reporting() level. PH7 collapsed it to the bErrReport
	                      * boolean, so E_ALL & ~E_DEPRECATED still printed every
	                      * deprecation — any non-zero level meant "report all". */
	int nRecursionDepth;       /* Current PHP call depth (OP_CALL frames only) */
	int nErrSuppress;          /* '@' error-control depth: >0 means the diagnostics raised
	                            * while evaluating the suppressed expression are not printed
	                            * (a user error handler is still invoked, as in php). Nests. */
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
	int nObDepth;              /* Output handlers currently running (0 outside one) */
	sxu32 nObActive;           /* 1-based index of the buffer whose handler is running
	                            * (0 outside one). php truncates the ob stack at that
	                            * buffer for the duration: ob_get_level()/contents()/
	                            * length()/list_handlers() answer for IT, not for
	                            * whatever is stacked above it. */
	int bConstEnum;            /* Expanding constants to DESCRIBE them
	                            * (get_defined_constants): php reports a deprecated
	                            * constant when it is READ, and listing the table is
	                            * not a read. */
	int bObRefused;            /* An ob call refused from inside a handler ended the
	                            * request: that operation delivers nothing more. */
	VmFrame *pObFrame;         /* Frame that CALLED the running output handler. The
	                            * handler's own body runs in a deeper frame, so
	                            * `nObDepth > 0 && pFrame != pObFrame` is "we are
	                            * inside the handler" — and it stays false for the
	                            * in-place catch PHL runs, in the caller's frame,
	                            * when the handler throws. */
	int nExceptDepth;          /* Exception depth */
	int nExcCtorDepth;         /* Engine-raised throws whose exception __construct is running
	                            * (VmExcCtorEnter): caps the self-feeding case where building
	                            * an exception throws again. */
	sxu32 nLazyInitLine;       /* While a LAZY class initializer runs (a static property's
	                            * deferred default, a class constant's on-demand evaluation):
	                            * the line of the ACCESS that triggered it. A Throwable born
	                            * in the initializer's OWN bytecode is stamped with THIS line
	                            * rather than the initializer's, because that is where php
	                            * evaluates the expression. 0 = not in one, or the access site
	                            * was internal (prelude) code whose line means nothing in the
	                            * file the stamp names. See PH7_VmStampThrowableSite. */
	sxi32 nLazyInitDepth;      /* nVmExecDepth of that initializer's own activation. The
	                            * override applies at THIS depth only: anything the
	                            * initializer manages to call — an autoloader, a nested
	                            * constant's evaluation — runs its own lines and keeps them. */
	int nMuteThrow;            /* > 0 while an initializer runs MUTED (VmEvalDefaultMuted): an
	                            * uncaught throw runs no exception handler, prints no report and
	                            * leaves iExitStatus alone, because php has not reached that code
	                            * yet. Depth-counted (an initializer can mount another class). */
	int closure_cnt;           /* Loaded closures counter */
	int json_rc;               /* JSON return status [refer to json_encode()/json_decode()]*/
	sxu32 unique_id;           /* Random number used to generate unique ID [refer to uniqid() for more info]*/
	sxu32 nNextObjId;          /* Next object handle id to hand out (monotonic; reset to 1 per exec
	                            * so a reused VM looks like a fresh process). See ph7_class_instance.nObjId */
	ProcErrLog xErrLog;        /* error_log() consumer [refer to PH7_VM_CONFIG_ERR_LOG_HANDLER] */
	sxu32 nOutputLen;          /* Total number of generated output */
	ph7_output_consumer sVmConsumer; /* Registered output consumer callback */
	ph7_output_consumer sVmErrConsumer; /* Diagnostics (stderr) consumer [PH7_VM_CONFIG_ERR_STREAM].
	                            * When xConsumer is 0 the log copy falls back to sVmConsumer so
	                            * embedders that never wire a stderr stream still see diagnostics. */
	int iAssertFlags;          /* Assertion flags */
	ph7_value sAssertCallback; /* Callback to call on failed assertions */
	VmRefObj **apRefObj;       /* Hashtable of referenced object */
	VmRefObj *pRefList;        /* List of referenced memory objects */
	sxu32 nRefSize;            /* apRefObj[] size */
	sxu32 nRefUsed;            /* Total entries in apRefObj[] */
	SySet aSelf;               /* 'self' stack used for static member access [i.e: self::MyConstant] */
	ph7_hashmap *pGlobal;      /* $GLOBALS hashmap */
	sxu32 nGlobalIdx;          /* $GLOBALS index */
	sxu32 nCurLine;            /* Line of the instruction currently executing (0 outside the
	                            * dispatch loop). Every runtime diagnostic, debug_backtrace()
	                            * and Throwable reads its line from here. */
	sxu8 bCurStrict;           /* strict_types mode of the unit that instruction came from,
	                            * published beside nCurLine. Read by the argument binder when
	                            * an OP_CALL carries no compiled call map — which is every
	                            * ENGINE-dispatched call (magic method, property hook), where
	                            * php still applies the calling file's mode. */
	sxi32 nLastErrType;        /* error_get_last(): severity of the last UNHANDLED diagnostic
	                            * (0 = none yet). php records one even when '@' or
	                            * error_reporting() hides it, but NOT when a user handler
	                            * claimed it by returning true. */
	sxu32 nLastErrLine;        /* ... its line */
	SyBlob sLastErrMsg;        /* ... its message */
	SyBlob sLastErrFile;       /* ... its file */
	char zDisplayName[256];    /* Scratch for PH7_VmFuncDisplayName: a closure's INTERNAL name is a
	                            * synthesized unique key ("[closure_3]"), but php shows
	                            * "{closure:file:line}". Valid until the next call. */
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
	sxi32 iCmpCallbackExc;     /* The dispatch STATUS a comparison callback did not return with
								* (PH7_EXCEPTION, or PH7_ABORT for an UNCAUGHT throw), so the
								* driver (usort/uasort/uksort and the array_udiff/
								* array_uintersect families) can abort and propagate exactly
								* it. Zero when no comparison raised; a comparator has no
								* status channel, so this latch is the only way out. */
	int iMbEncoding;           /* mbstring's internal encoding, an MB_ENC_* id from
								* builtin_mb.c; 0 is UTF-8, which is why zeroing the
								* VM leaves php's default in place. */
	sxi32 iMbSubstitute;       /* mbstring's substitute code point ('?' at VM init;
								* 0 is a code point a script may really ask for). */
	sxu8 iMbSubstMode;         /* how it is written: builtin_mb.c's MB_SUBST_* — the
								* code point itself, nothing at all, or the U+/entity
								* spelling of what could not be represented. php keeps
								* the two apart, so setting "long" does not forget the
								* code point an error character still takes. */
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
	sxu32 nLastEvalErr;        /* Compile-error count of the most recent VmEvalChunk unit. Unlike
								* sCodeGen.nErr it survives the nested-compile state save/restore,
								* so VmExecDeferredClass can tell whether ITS chunk failed even
								* when the deferred declaration executes inside an outer compile
								* (an autoload-during-compile require). */
	SyString sDeferAnonName;   /* One-shot synthesized-name override for the next anonymous-class
								* compile: set by VmExecDeferredClass before re-compiling a
								* deferred `new class ... {}` chunk so the runtime-installed
								* class carries the SAME name the site's OP_NEW loads; consumed
								* (cleared) by PH7_CompileAnnonClass. {0,0} otherwise. */
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
	ph7_class *pIncClass;      /* Cached __PHP_Incomplete_Class pointer: unserialize()'s carrier for a
	                            * disallowed or unknown class. Every script-level property access or
	                            * method call on an instance is php's incomplete-object diagnostic
	                            * (PH7_VmIncompleteMsg); the engine itself reads hAttr freely. */
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
#ifdef PH7_ENABLE_LIBXML
	SySet aLibxmlErr;          /* Queued phl_libxml_err entries (libxml_get_errors) */
	int bLibxmlInternalErr;    /* libxml_use_internal_errors(true) is active */
	void *pLibxmlLastErr;      /* phl_libxml_err* slot backing libxml_get_last_error */
	void *pXmlDocs;            /* phl_xmldoc registry chain; freed on reset/release */
	void *pXmlWriters;         /* XMLWriter registry chain; freed on reset/release */
#endif
	/* php numbers every resource with a small sequential id that (int) casts and
	 * "Resource id #N" render, and that distinguishes two live resources from one
	 * another. PHL's resource value is a bare void*, so the id lives in this
	 * per-VM registry: pointer -> phl_res_id, assigned on first observation.
	 * Freed with the VM (ids are never recycled, as php's may be). */
	SyHash hResourceId;        /* void* -> phl_res_id* */
	sxu32 nResourceIdNext;     /* Next id to hand out (php's start at 1) */
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
  PH7_OP_NULLC,         /* Null coalescing ?? */
  PH7_OP_NULLC_JMP,     /* Null coalescing assign short-circuit jump */
  PH7_OP_NULLC_STORE,   /* Null coalescing assign store */
  PH7_OP_NULLSAFE_JMP,  /* Nullsafe (?->) short-circuit jump */
  PH7_OP_SPREAD,        /* Mark TOS for argument unpacking (...$arr) */
  PH7_OP_FLAG_SPREAD,   /* Flag TOS as a spread source for the next LOAD_MAP */
  PH7_OP_CATCH,         /* Bind the in-flight exception into a catch variable (ROOT C inline catch) */
  PH7_OP_END_FINALLY,   /* Terminate an inline finally: dispatch the pending action (ROOT C) */
  PH7_OP_SET_FINALLY_RET,/* Seed a pending RETURN and enter the innermost enclosing finally (ROOT C) */
  PH7_OP_SET_FINALLY_JMP,/* Seed a pending BREAK/CONTINUE (jump target) and enter a finally (ROOT C) */
  PH7_OP_CATCH_JMP,     /* Jump to iP2, leaving the try/catch structures iP1 describes (see
                         * PH7_CATCH_JMP_P1): LEVELS detached catch/finally mini-programs and
                         * CROSS enclosing trys whose OP_POP_EXCEPTION the jump skips. Two
                         * regimes. LEVELS > 0: iP2 is a pc in the OWNING body's bytecode, which
                         * this mini-program cannot address — park it on that body's frame and
                         * end the mini-program; each try's OP_POP_EXCEPTION landing pad on the
                         * way out decrements, and the last one drains CROSS and takes the jump,
                         * exactly as it materializes a catch's parked `return`. LEVELS == 0:
                         * iP2 is in THIS array (a `goto` out of a try body) — just drain CROSS
                         * and jump. Emitted for break/continue/goto alike. */
  PH7_OP_UNSET_VAR,     /* unset($name): drop ONE name binding (p3 = name), never the shared slot */
  PH7_OP_CALL_INIT,     /* Screen a call's callee where it is WRITTEN, before its arguments run:
                         * php resolves one at INIT_FCALL / INIT_DYNAMIC_CALL and raises the
                         * direct dispatch's own Error there. Emitted only for a callee the
                         * following OP_CALL would be the first to look at — a member callee
                         * was already screened by its OP_MEMBER. iP2 = 1 when the compiler
                         * namespace-qualified the name, which the global fallback needs. */
  PH7_OP_ROT_CALLEE,    /* Rotate this call's CALLEE — which the codegen pushed BEFORE the
                         * arguments, because php resolves a callee where it is written — up
                         * to the top of the stack, so OP_CALL sees the [args…][callee] layout
                         * its whole dispatch is written against. iP1 = compile-time argument
                         * count, iP2 = PH7_ROT_* flags (SPREAD: re-derive the runtime count
                         * from this call's unpack runs; TWOSLOT: the callee is an OP_MEMBER
                         * method pair [receiver][name], not a single value). */
  PH7_OP_CLASS_DEFER    /* Deferred class declaration: p3 = VmDeferredClass. Compile-time
                         * resolution of a parent/interface/trait failed (autoloader not yet
                         * REGISTERED — the declaring file's own statements had not run), so the
                         * whole declaration re-compiles here, at its execution point, where
                         * spl_autoload_register has taken effect. php's own model: classes with
                         * unresolved parents are declared in execution order, not hoisted. */
};
/*
 * PH7_OP_CATCH_JMP.iP1 payload. Both halves are nesting depths of the source, never
 * large: LEVELS = detached catch/finally boundaries the jump leaves (0 = none, it
 * stays in this bytecode array), CROSS = enclosing try activations whose
 * OP_POP_EXCEPTION the jump skips, and whose finally it must therefore drain itself.
 */
#define PH7_CATCH_JMP_P1(LEVELS,CROSS) \
	((sxi32)((((sxu32)(CROSS)) << 16) | ((sxu32)(LEVELS) & 0xFFFFu)))
#define PH7_CATCH_JMP_LEVELS(P1) ((sxu16)((sxu32)(P1) & 0xFFFFu))
#define PH7_CATCH_JMP_CROSS(P1)  ((sxu16)(((sxu32)(P1) >> 16) & 0xFFFFu))
/* LOADC.iP1 bit flags */
#define PH7_LOADC_EXPAND   0x01 /* Candidate for constant/function/class expansion */
#define PH7_LOADC_NOKEY    0x04 /* The nil this pushes is an ABSENT array-literal key (auto-index),
                                 * not an explicit `null =>` one. The two are both MEMOBJ_NULL on the
                                 * stack, and LOAD_MAP must tell them apart: an absent key auto-indexes
                                 * silently, an explicit null key deprecates and stores under "". */
#define PH7_LOADC_ABSOLUTE 0x02 /* Fully-qualified — skip namespace prefixing */
#define PH7_LOADC_NOGLOBAL 0x08 /* The p3 candidate came from a `use const` import, which php
                                 * resolves WITHOUT a global fallback: if that exact name is
                                 * undefined the read is an Error, even when a global constant
                                 * of the imported alias's short name exists. */
/* ROT_CALLEE.iP2 — what the rotation has to know about the region it is turning over. */
#define PH7_ROT_SPREAD  0x1 /* This call unpacks: the runtime argument count is iP1 plus the net
                             * growth of its OWN captured spread runs (VmSpreadOwnExtra). */
#define PH7_ROT_TWOSLOT 0x2 /* The callee is an OP_MEMBER method pair — [receiver][method name] —
                             * which OP_CALL reads as two slots ($this / the late-static-binding
                             * class from the receiver, the name from the top). A __call routing
                             * collapses that pair to ONE marked carrier slot at run time, which
                             * the handler detects rather than guessing. */
/* MEMBER.iP2 — member-access context. 0=read is the default; the unset/isset/empty modes mirror the
 * array LOAD_IDX context modes so unset()/isset()/empty() on a property behave like on an array elem. */
#define PH7_MEMBER_READ   0 /* attribute read */
#define PH7_MEMBER_METHOD 1 /* method-call preparation */
#define PH7_MEMBER_UNSET  2 /* unset($o->p): remove the property */
#define PH7_MEMBER_ISSET  3 /* isset($o->p): silent on a read-miss */
#define PH7_MEMBER_EMPTY  4 /* empty($o->p): silent on a read-miss */
#define PH7_MEMBER_WRITE  5 /* write-lvalue base ($o->arr[..]=, $o->p??=): auto-create a missing prop */
#define PH7_MEMBER_REF_TARGET 6 /* reference-store target ($o->p =& $x, C::$s =& $x): resolve the
                                 * property slot and stash it for the following OP_STORE_REF; skip
                                 * the read/hook/magic machinery (a ref bind neither reads nor coerces) */
#define PH7_MEMBER_DEFPATH 7    /* D1 commit 2: deferred by-ref/by-value property call arg ($o->p). Reads a
                                 * present property (like READ); on a miss/magic, records the lvalue path
                                 * (MEMOBJ_AUX_DEFPATH) that OP_CALL re-walks in vivify or read+warn mode */
#define PH7_MEMBER_COALESCE 9 /* `$o->p ?? d`: php's THIRD accessor level, between a read and an
                               * isset(). Silent on a read-miss like isset()/empty(), but the
                               * expression takes the property's VALUE, not a truth: __isset()
                               * GATES the access and __get() (or a get HOOK) ANSWERS it, and
                               * with no __isset declared the accessor answers on its own.
                               * `??` used to compile as PH7_MEMBER_ISSET, so every accessor
                               * path handed the coalesce a BOOLEAN. */
#define PH7_MEMBER_LIST_TARGET 8 /* positional list-destructuring store target ([$o->p] = [...]): a pure
                                  * write whose value arrives only at the following OP_LOAD_LIST, which
                                  * writes the slot directly (with typed-slot enforcement). Skip the
                                  * uninitialized-typed read Error and the __get consult, and vivify a
                                  * missing property like a write base */
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
#define PH7_TK_MEMBER_NAME 0x4000000 /* Reserved word used as a member NAME right after -> / ?-> / ::
                                      * (Enum::Null, C::Array, $o->list()): a plain identifier, never
                                      * the literal value — GenStateLoadLiteral skips its value conversion. */
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
	JSON_ERROR_RECURSION, /* A container already being encoded shows up inside itself (php value 6) */
	JSON_ERROR_INF_OR_NAN = 7, /* Inf or NaN given to json_encode (php value) */
	JSON_ERROR_UNSUPPORTED_TYPE = 8, /* A resource given to json_encode (php value) */
	JSON_ERROR_INVALID_PROPERTY_NAME = 9, /* Object-mode decode of a property name with a
	                                       * LEADING NUL byte — php reserves that prefix for
	                                       * mangled private/protected names (php value) */
	JSON_ERROR_UTF16 = 10, /* Unpaired UTF-16 surrogate in a \uXXXX escape (php value) */
	JSON_ERROR_NON_BACKED_ENUM = 11 /* Non-backed enum given to json_encode (php 8.1 value) */
};
/* The following constants can be combined to form options for json_encode(). */
#define	JSON_HEX_TAG           0x01  /* All < and > are converted to \u003C and \u003E. */
#define JSON_HEX_AMP           0x02  /* All &s are converted to \u0026. */
#define JSON_HEX_APOS          0x04  /* All ' are converted to \u0027. */
#define JSON_HEX_QUOT          0x08  /* All " are converted to \u0022. */
#define JSON_FORCE_OBJECT      0x10  /* Outputs an object rather than an array */
#define JSON_NUMERIC_CHECK     0x20  /* Encodes numeric strings as numbers. */
#define JSON_PRETTY_PRINT      0x80  /* Use whitespace in returned data to format it.*/
#define JSON_UNESCAPED_SLASHES 0x40  /* Don't escape '/' */
#define JSON_UNESCAPED_UNICODE 0x100 /* Emit multibyte UTF-8 raw instead of \uXXXX */
#define JSON_PARTIAL_OUTPUT_ON_ERROR 0x200 /* Substitute (0 / null / "") for an unencodable
                                            * piece and record the error instead of failing */
#define JSON_PRESERVE_ZERO_FRACTION  0x400 /* A float with no fractional digits prints ".0"
                                            * (1.0 encodes as "1.0", not "1") */
#define JSON_UNESCAPED_LINE_TERMINATORS 0x800 /* ...U+2028/U+2029 included */
#define JSON_INVALID_UTF8_IGNORE     0x100000 /* Drop ill-formed UTF-8 instead of failing */
#define JSON_INVALID_UTF8_SUBSTITUTE 0x200000 /* ...replace it with U+FFFD */
#define JSON_THROW_ON_ERROR    0x400000 /* Throw JsonException on encode/decode error */
/* DECODE flags: php numbers the decode options in their own space, so each shares a
 * bit with an encode flag exactly as php's JSON_BIGINT_AS_STRING shares 2 with
 * JSON_HEX_AMP (and JSON_OBJECT_AS_ARRAY shares 1 with JSON_HEX_TAG). */
#define JSON_OBJECT_AS_ARRAY   0x01
#define JSON_BIGINT_AS_STRING  0x02
/*
 * extract() $flags — php's ENUM (ext/standard/php_array.h), not a bitmask.
 * PH7 exposed a legacy power-of-two bitmask here (1/2/4/8/16/32/64), which
 * changed the meaning of valid php source: extract($a,1) is EXTR_SKIP in php
 * but was EXTR_OVERWRITE in PHL, and EXTR_PREFIX_ALL printed 8 instead of 3.
 * The values below ARE php's, and vm_builtin_extract() dispatches on
 * (flags & 0xff) exactly like php does.
 */
#define PH7_EXTR_OVERWRITE        0
#define PH7_EXTR_SKIP             1
#define PH7_EXTR_PREFIX_SAME      2
#define PH7_EXTR_PREFIX_ALL       3
#define PH7_EXTR_PREFIX_INVALID   4
#define PH7_EXTR_PREFIX_IF_EXISTS 5
#define PH7_EXTR_IF_EXISTS        6
#define PH7_EXTR_REFS             0x100 /* php's by-reference extraction (rides above the mode) */
/*
 * pathinfo() $flags, glob() $flags and parse_ini_*() $scanner_mode — php's VALUES.
 *
 * Each of these was a PH7 invention (pathinfo counted 1/2/3/4 where php's are POWERS
 * OF TWO, glob used its own 1..64 ladder, and the ini scanner started at 1), which
 * changes the meaning of valid php source: `PATHINFO_DIRNAME|PATHINFO_BASENAME` is 3
 * in both engines but PHL read 3 as PATHINFO_EXTENSION, a script passing php's literal
 * 64 to json_encode() got JSON_BIGINT_AS_STRING instead of JSON_UNESCAPED_SLASHES, and
 * INI_SCANNER_RAW (php 1) selected nothing.
 *
 * The GLOB_* values are php 8.5's OWN portable set (main/php_glob.h, new in 8.5), which
 * a default build uses on every platform including MSVC — the bundled branch is taken
 * unless the POSIX build is configured with --enable-system-glob (off by default), and
 * the win32 build has no such option. Do NOT read them off the host <glob.h>: glibc's
 * ladder is different (MARK 2, NOSORT 4, BRACE 1024, ONLYDIR 8192), and so was php's
 * own pre-8.5 win32/glob.h (NOESCAPE 0x2000). PATHINFO_*, INI_SCANNER_* and JSON_* are
 * plain #defines in ext/standard and ext/json, never platform-conditional.
 */
#define PH7_PATHINFO_DIRNAME    1
#define PH7_PATHINFO_BASENAME   2
#define PH7_PATHINFO_EXTENSION  4
#define PH7_PATHINFO_FILENAME   8
#define PH7_PATHINFO_ALL        (PH7_PATHINFO_DIRNAME|PH7_PATHINFO_BASENAME|\
                                 PH7_PATHINFO_EXTENSION|PH7_PATHINFO_FILENAME)
#define PH7_GLOB_ERR            0x0004
#define PH7_GLOB_MARK           0x0008
#define PH7_GLOB_NOCHECK        0x0010
#define PH7_GLOB_NOSORT         0x0020
#define PH7_GLOB_BRACE          0x0080
#define PH7_GLOB_NOESCAPE       0x1000
#define PH7_GLOB_ONLYDIR        0x40000000
#define PH7_INI_SCANNER_NORMAL  0
#define PH7_INI_SCANNER_RAW     1
#define PH7_INI_SCANNER_TYPED   2
/* php's INI_SCANNER_TYPED is 2 — not defined here because PHL does not register the
 * constant (nor honour any scanner mode yet); §5 tracks it with the missing JSON_*. */
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
PH7_PRIVATE int PH7_MemObjStringNumericPrefix(ph7_value *pValue,const char **pzTail);
PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToStringUV(ph7_value *pObj);
PH7_PRIVATE int PH7_MemObjIsNotStringable(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj);
PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pData);
/* lex.c function prototypes */
PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut,sxu32 nBaseLine);
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
PH7_PRIVATE void PH7_VmClassNameAnchor(const char **pzName,sxu32 *pnByte);
PH7_PRIVATE sxi32 PH7_VmMaterializeClassConst(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);
PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable);
PH7_PRIVATE sxi32 PH7_VmRegisterConstant(ph7_vm *pVm,const SyString *pName,ProcConstant xExpand,void *pUserData);
PH7_PRIVATE sxi32 PH7_VmRegisterConstantEx(ph7_vm *pVm,const SyString *pName,ProcConstant xExpand,
	void *pUserData,const SyString *pFile,sxu32 nLine,int bUser);
PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(ph7_vm *pVm,const SyString *pName,ProchHostFunction xFunc,void *pUserData);
/* Builds a ph7_user_func WITHOUT registering it as a global name. The native-class
 * builder uses it for method bodies, which are reachable only through their class. */
PH7_PRIVATE sxi32 PH7_NewForeignFunction(ph7_vm *pVm,const SyString *pName,ProchHostFunction xFunc,
	void *pUserData,ph7_user_func **ppOut);
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
PH7_PRIVATE sxu32 PH7_VmResourceId(ph7_vm *pVm,void *pRes);
PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...);
PH7_PRIVATE sxi32 PH7_VmThrowExceptionCode(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,...);
PH7_PRIVATE sxi32 VmHostFuncThrowRc(ph7_context *pCtx,sxi32 rc);
PH7_PRIVATE sxi32 PH7_VmThrowArrayNextIndexError(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_VmThrowGlobalsAppendError(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_VmInstallGlobalVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue,sxu32 nRefIdx);
PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...);
PH7_PRIVATE void  PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData);
PH7_PRIVATE sxi32 PH7_VmDump(ph7_vm *pVm,ProcConsumer xConsumer,void *pUserData);
PH7_PRIVATE sxi32 PH7_VmInstallDateTime(ph7_vm *pVm);
#ifndef PH7_DISABLE_BUILTIN_FUNC
/* Shared between builtin_date.c (procedural date functions) and
 * builtin_date_parse.c (the DateTime family) */
PH7_PRIVATE sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm,int uSec);
PH7_PRIVATE sxi64 DtDaysFromCivil(sxi64 y,int m,int d);
PH7_PRIVATE void DtCivilFromDays(sxi64 z,sxi64 *py,int *pm,int *pd);
PH7_PRIVATE sxi64 DtFloorDiv(sxi64 a,sxi64 b);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
PH7_PRIVATE const char * PH7_VmBuiltinSigLookup(const char *zName,sxu32 nLen,const char **pzRet);
PH7_PRIVATE void PH7_VmStoreArgByRef(ph7_vm *pVm,ph7_value *pArg,ph7_value *pNewVal);
PH7_PRIVATE sxi32 PH7_ValueToStringUV(ph7_context *pCtx,ph7_value *pValue,const char **pzData,int *pnLen);
PH7_PRIVATE void PH7_VmThrowWarningFmt(ph7_vm *pVm,const char *zFmt,...);
PH7_PRIVATE sxi32 PH7_CheckCallbackArg(ph7_context *pCtx,ph7_value *pCb,int iArg,const char *zParam,int bNullable);
PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_VmInstallReflectionTypes(ph7_vm *pVm); /* vm_builtin_reflection.c */
PH7_PRIVATE sxi32 PH7_VmInstallReflectionSmall(ph7_vm *pVm); /* vm_builtin_reflection.c */
PH7_PRIVATE sxi32 PH7_VmInstallReflectionAttribute(ph7_vm *pVm); /* vm_builtin_reflection.c */
PH7_PRIVATE sxi32 PH7_VmInstallReflectionEnum(ph7_vm *pVm); /* vm_builtin_reflection.c */
PH7_PRIVATE sxi32 PH7_VmInstallReflectionClass(ph7_vm *pVm); /* vm_builtin_reflection.c */
PH7_PRIVATE sxi32 PH7_VmInstallReflectionFunc(ph7_vm *pVm);  /* vm_builtin_reflection.c */
PH7_PRIVATE sxi32 PH7_VmInstallReflectionHookType(ph7_vm *pVm); /* vm_builtin_reflection.c */
PH7_PRIVATE sxi32 PH7_VmInstallReflectionMember(ph7_vm *pVm); /* vm_builtin_reflection.c */
PH7_PRIVATE sxi32 PH7_ClosurePresent(ph7_vm *pVm,ph7_class_instance *pThis,
	ph7_value *pOut,int bDebug); /* vm_builtin_reflection.c: Closure's ph7_class::xPresent */
PH7_PRIVATE sxi32 PH7_VmInstallBuiltinLib(ph7_vm *pVm); /* vm_builtin_lib.c */
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
PH7_PRIVATE void PH7_VmMtSrand(ph7_vm *pVm,sxu32 nSeed,int bLegacyTwist);
PH7_PRIVATE sxu32 PH7_VmMtRand(ph7_vm *pVm);
PH7_PRIVATE sxi64 PH7_VmMtRandRange(ph7_vm *pVm,sxi64 iMin,sxi64 iMax);
PH7_PRIVATE void PH7_VmReleaseInstanceAttr(ph7_vm *pVm,VmClassAttr *pVmAttr);
PH7_PRIVATE sxi32 PH7_VmCallClassMethod(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_method *pMethod,
	ph7_value *pResult,int nArg,ph7_value **apArg);
PH7_PRIVATE sxi32 PH7_VmCallClassMethodMap(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_method *pMethod,
	ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap);
PH7_PRIVATE sxi32 PH7_VmCallMethodSwallow(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_method *pMethod,
	ph7_value *pResult,int nArg,ph7_value **apArg,int *pbThrew);
PH7_PRIVATE sxi32 PH7_VmCallMethodUnchecked(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_method *pMethod,
	ph7_value *pResult,int nArg,ph7_value **apArg);
PH7_PRIVATE sxi32 PH7_VmCallUserFunction(ph7_vm *pVm,ph7_value *pFunc,int nArg,ph7_value **apArg,ph7_value *pResult);
PH7_PRIVATE sxi32 PH7_VmCallUserFunctionWithMap(ph7_vm *pVm,ph7_value *pFunc,int nArg,ph7_value **apArg,ph7_value *pResult,VmCallArgMap *pArgMap);
PH7_PRIVATE ph7_class_instance * PH7_VmCallerThisFor(ph7_vm *pVm,ph7_class *pClass);
PH7_PRIVATE ph7_class_instance * PH7_VmStaticFallbackThis(ph7_vm *pVm,ph7_class *pClass);
PH7_PRIVATE sxi32 PH7_VmDispatchMagicCall(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,
	const char *zName,sxu32 nName,ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pArgMap);
/* Per-element callback for PH7_VmIteratorWalk: return SXRET_OK to continue,
 * SXERR_EOF to stop early (not an error), or PH7_EXCEPTION/PH7_ABORT to propagate. */
typedef sxi32 (*ProcIterStep)(ph7_vm *pVm,ph7_value *pKey,ph7_value *pValue,void *pUserData);
PH7_PRIVATE sxi32 PH7_VmIteratorWalk(ph7_vm *pVm,ph7_value *pObj,ProcIterStep xStep,void *pUserData);
PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(ph7_vm *pVm,ph7_value *pFunc,ph7_value *pResult,...);
PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce);
PH7_PRIVATE void PH7_VmWarnByRefArgsGivenValue(ph7_vm *pVm,ph7_value *pCallable,int nArg,ph7_hashmap_node **apNode,SyString *aNames);
PH7_PRIVATE void PH7_VmCufDropByRefArgs(ph7_context *pCtx,ph7_value *pCallable,int nArg,ph7_value **apArg);
PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen);
PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm);
PH7_PRIVATE ph7_class * PH7_VmPeekSelfClass(ph7_vm *pVm);
PH7_PRIVATE ph7_class * PH7_VmTraitUsingClass(ph7_vm *pVm,ph7_class *pTrait,ph7_class *pFrom);
PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm);
PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke);
PH7_PRIVATE int PH7_VmArrayCallableParts(ph7_vm *pVm,ph7_hashmap *pMap,ph7_value **ppTarget,ph7_value **ppMethod);
PH7_PRIVATE int PH7_VmCallableStringParts(const char *zName,sxu32 nName,
	const char **pzCls,sxu32 *pnCls,const char **pzMeth,sxu32 *pnMeth);
PH7_PRIVATE int PH7_VmClassLookupRaised(ph7_vm *pVm,sxi32 nBrcBefore,const void *pResumeBefore);
PH7_PRIVATE const char * PH7_VmCallableReason(ph7_vm *pVm,ph7_value *pValue,char *zBuf,int nBuf);
PH7_PRIVATE void PH7_VmCallableName(ph7_vm *pVm,ph7_value *pValue,SyBlob *pOut);
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
PH7_PRIVATE int PH7_PcrePatternCheck(ph7_vm *pVm,const char *zPattern,int nLen,
	char *zErr,sxu32 nErr);
/* RegexIterator's five operation modes, php's REGIT_MODE_* values — they are the
 * class constants, so the numbers are php-visible and fixed. */
#define PH7_REGIT_MATCH        0
#define PH7_REGIT_GET_MATCH    1
#define PH7_REGIT_ALL_MATCHES  2
#define PH7_REGIT_SPLIT        3
#define PH7_REGIT_REPLACE      4
PH7_PRIVATE sxi32 PH7_PcreRegitApply(ph7_context *pCtx,int iMode,ph7_value *pPattern,
	ph7_value *pSubject,int iPregFlags,ph7_value *pRepl,ph7_value *pOut,int *pbOk);
#endif /* PH7_ENABLE_PCRE */
/* One resource pointer's php-visible id. Allocated per distinct resource and
 * owned by ph7_vm.hResourceId, whose key is the `pRes` field itself (SyHash
 * stores the key POINTER, so it must outlive the entry). Core (used by
 * PH7_VmResourceId) — must NOT sit under PH7_ENABLE_LIBXML or the tiny build,
 * which omits libxml, fails to compile it. */
typedef struct phl_res_id phl_res_id;
struct phl_res_id {
	void *pRes;   /* The resource pointer, and the hash key */
	sxu32 nId;    /* php-visible id */
};
#ifdef PH7_ENABLE_LIBXML
/* One entry in the per-VM libxml error queue (mirrors php's LibXMLError:
 * level/code/column/message/file/line).  Strings are SyMemBackend copies
 * owned by the queue and released by PH7_LibxmlClearErrors(). */
typedef struct phl_libxml_err phl_libxml_err;
struct phl_libxml_err {
	int iLevel;      /* LIBXML_ERR_WARNING/ERROR/FATAL */
	int iCode;       /* raw libxml2 error code */
	int iLine;
	int iColumn;
	SyString sMsg;   /* message text, trailing newline preserved (php parity) */
	SyString sFile;  /* source file/URI, empty for in-memory strings */
};
/* Per-VM owner of one libxml document tree.  See the lifetime notes at the
 * top of vm_libxml.c: docs are only freed at VM reset/release, never while
 * PHP code could still hold a wrapper into them. */
typedef struct phl_xmldoc phl_xmldoc;
struct phl_xmldoc {
	void *pDoc;         /* xmlDocPtr (void* keeps libxml headers out of ph7int.h) */
	SySet aOrphans;     /* xmlNodePtr's unlinked from the tree but still owned */
	ph7_vm *pVm;        /* Owning VM (error routing from libxml callbacks) */
	int bPreserveWS;    /* DOMDocument->preserveWhiteSpace */
	int bFormatOutput;  /* DOMDocument->formatOutput */
	phl_xmldoc *pNext;  /* Registry chain (pVm->pXmlDocs) */
};
/* One PHP-visible DOM node handle: the MEMOBJ_RES payload behind every DOM
 * wrapper object.  pNode points into pShell's tree (or IS the xmlDoc); the
 * shell outlives every handle (docs are only freed at VM reset/release). */
typedef struct phl_domnode phl_domnode;
struct phl_domnode {
	phl_xmldoc *pShell; /* Owning document registry entry */
	void *pNode;        /* xmlNodePtr / xmlDocPtr / xmlAttrPtr */
};
/* vm_libxml.c */
PH7_PRIVATE void PH7_RegisterLibxmlConstants(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_VmInstallLibxml(ph7_vm *pVm);
PH7_PRIVATE void PH7_LibxmlVmReset(ph7_vm *pVm);
PH7_PRIVATE void PH7_LibxmlVmRelease(ph7_vm *pVm);
PH7_PRIVATE void PH7_LibxmlClearErrors(ph7_vm *pVm);
PH7_PRIVATE phl_xmldoc * PH7_LibxmlNewDoc(ph7_vm *pVm,void *pXmlDocPtr);
PH7_PRIVATE sxu32 PH7_LibxmlCaptureBegin(ph7_vm *pVm);
PH7_PRIVATE void PH7_LibxmlCaptureEnd(ph7_vm *pVm,sxu32 nMark,const char *zFnName);
/* Push one error onto the per-VM queue + last-error slot (strings copied).
 * The shared structured-error callback and the DOM schema error hooks both
 * funnel through this so ph7int.h needs no libxml types. */
PH7_PRIVATE void PH7_LibxmlQueueError(ph7_vm *pVm,int iLevel,int iCode,int iLine,int iColumn,
	const char *zMsg,const char *zFile);
/* vm_dom.c */
PH7_PRIVATE sxi32 PH7_VmInstallDom(ph7_vm *pVm);
/* vm_xmlwriter.c */
PH7_PRIVATE sxi32 PH7_VmInstallXmlWriter(ph7_vm *pVm);
PH7_PRIVATE void PH7_XmlWriterVmSweep(ph7_vm *pVm);
#endif /* PH7_ENABLE_LIBXML */
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
PH7_PRIVATE ph7_socket PH7_NetConnect(const char *zHost,int iPort,int iTimeoutMs,int *pErrno,const char **pzErr);
PH7_PRIVATE ph7_socket PH7_NetAccept(ph7_socket listenSock,struct sockaddr *pAddr,ph7_socklen *pAddrLen);
PH7_PRIVATE int PH7_NetRecv(ph7_socket sock,void *pBuf,int nLen,int flags);
PH7_PRIVATE int PH7_NetSend(ph7_socket sock,const void *pBuf,int nLen,int flags);
PH7_PRIVATE int PH7_NetSendAll(ph7_socket sock,const void *pBuf,int nLen);
PH7_PRIVATE void PH7_NetClose(ph7_socket sock);
PH7_PRIVATE void PH7_NetSetTimeout(ph7_socket sock,int iMilliseconds);
PH7_PRIVATE void PH7_NetSetRwTimeout(ph7_socket sock,ph7_int64 iSeconds,ph7_int64 iMicroseconds);
PH7_PRIVATE void PH7_NetSetBlocking(ph7_socket sock,int bBlocking);
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
PH7_PRIVATE sxi32 PH7_VmUnserializeOne(ph7_context *pCtx,const char *zIn,int nByte,int *pnRead,ph7_value *pOut);
PH7_PRIVATE void PH7_AppendShortestReal(SyBlob *pOut,double d);
/* memobj.c float-shape helper (php_gcvt/smart_str_append_double semantics);
 * shared by the float->string cast and builtin.c's printf float conversions */
#ifndef PH7_OMIT_FLOATING_POINT
PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric);
#endif
/* builtin.c utf8 function prototypes */
PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates);
PH7_PRIVATE int VmLocalExecThrew(ph7_vm *pVm,sxi32 rc,const void *pResumeBefore,const void *pInlineBefore);
PH7_PRIVATE int PH7_VmIsAutoGlobal(const char *zName,sxu32 nByte);
PH7_PRIVATE sxi32 PH7_VmArrayKeyArg(ph7_context *pCtx,ph7_value *pKey,int bZppWording);
PH7_PRIVATE sxi32 PH7_VmExecAttrArg(ph7_vm *pVm,SySet *pByteCode,ph7_class *pDeclCls,ph7_value *pResult);
PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...);
/* vm_builtin_class.c function prototypes */
PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass);
PH7_PRIVATE void PH7_VmStampThrowableSite(ph7_vm *pVm,ph7_class_instance *pThis);
PH7_PRIVATE int PH7_VmClassMemberAccess(ph7_vm *pVm,ph7_class *pClass,const SyString *pAttrName,sxi32 iProtection,int bLog);
PH7_PRIVATE ph7_class * PH7_VmCallerScope(ph7_vm *pVm);
PH7_PRIVATE ph7_class * PH7_VmCallerScopeName(ph7_vm *pVm);
PH7_PRIVATE ph7_class * PH7_VmMethodScopeName(ph7_vm *pVm,ph7_class *pClass,ph7_class_method *pMeth);
PH7_PRIVATE int PH7_VmFccMethodIsDirect(ph7_vm *pVm,ph7_class *pClass,const char *zName,sxu32 nName);
PH7_PRIVATE ph7_class * PH7_VmClosureScopeClass(ph7_vm *pVm,ph7_class_instance *pClosure);
PH7_PRIVATE ph7_class * PH7_VmComposingClass(ph7_class *pClass,ph7_class *pDecl);
PH7_PRIVATE void PH7_ClassMethodRegisteredName(ph7_class *pClass,const char *zName,sxu32 nByte,SyString *pOut);
PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg);
PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_trait_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_clone(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg);
/* vm_builtin_ob.c function prototypes */
PH7_PRIVATE int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData);
PH7_PRIVATE void PH7_VmObFlushAll(ph7_vm *pVm);
PH7_PRIVATE int vm_builtin_ob_clean(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_ob_end_clean(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_ob_get_clean(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_flush(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_ob_get_flush(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_ob_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg);
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
PH7_PRIVATE int PH7_builtin_acosh(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_asinh(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_atanh(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_expm1(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_log1p(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_deg2rad(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_rad2deg(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_fpow(ph7_context *pCtx,int nArg,ph7_value **apArg);
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
PH7_PRIVATE int PH7_builtin_number_format(ph7_context *pCtx,int nArg,ph7_value **apArg);
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
PH7_PRIVATE int PH7_builtin_hrtime(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_date_default_timezone_get(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_date_default_timezone_set(ph7_context *pCtx,int nArg,ph7_value **apArg);
/* builtin_mb.c (UTF-8-only mb_* family) */
/* Shared ZPP helper: resolve an `int`-typed parameter with php's full contract
 * (null deprecation, lossy float / float-string deprecations, TypeErrors for
 * NAN/INF/non-numeric). Builtins with int params should use it instead of a
 * bare ph7_value_to_int64(), which coerces silently. */
PH7_PRIVATE sxi32 PH7_IntArgResolve(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,
	int iArgNum,const char *zParamName,const char *zTypeStr,sxi64 *pOut);
PH7_PRIVATE int PH7_builtin_mb_strlen_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_substr_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_case_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_convert_case_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_strpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_str_split_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_trim_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_internal_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_substitute_character_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_scrub_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_check_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_strwidth_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_chr_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_ord_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_ucfirst_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_strstr_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_substr_count_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_str_pad_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_strcut_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_strimwidth_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_detect_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_mb_convert_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg);
/* vm_builtin_spl.c */
PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm);
/* vm_builtin_tokenizer.c */
PH7_PRIVATE sxi32 PH7_VmInstallTokenizer(ph7_vm *pVm);
PH7_PRIVATE void PH7_RegisterTokenizerConstants(ph7_vm *pVm);
/* vm_builtin_session.c */
PH7_PRIVATE sxi32 PH7_VmInstallSession(ph7_vm *pVm);
/* vm_builtin_ini.c */
PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm);
PH7_PRIVATE sxi64 PH7_VmIniGetInt(ph7_vm *pVm,const char *zName,sxi64 iDefault);
PH7_PRIVATE void PH7_VmIniGetStr(ph7_vm *pVm,const char *zName,SyBlob *pOut);
PH7_PRIVATE int PH7_VmIniGetBool(ph7_vm *pVm,const char *zName,int bDefault);
/* vfs_win.c / vfs_unix.c exported structs */
#ifdef __WINNT__
extern const ph7_vfs sWinVfs;
extern const ph7_io_stream sWinFileStream;
/* Would php's MapViewOfFile() copy of this plain file map a view of zero
 * requested bytes? stream_copy_to_stream() answers false then. */
PH7_PRIVATE int PH7_WinFileMapsEmptyView(void *pHandle,ph7_int64 nAhead);
#elif defined(__UNIXES__)
extern const ph7_vfs sUnixVfs;
extern const ph7_io_stream sUnixFileStream;
#endif
/* Built-in IO stream drivers: tcp:// lives in vfs_stream.c; php://, data://
 * and the pipe (popen) stream live in vfs_io_driver.c. Registration (vfs.c)
 * and the standard-stream exporters reference them across those files. */
extern const ph7_io_stream sTCP_Stream;
extern const ph7_io_stream sDATA_Stream;
extern const ph7_io_stream sPHP_Stream;
/* IO private state carried by every open stream handle (fopen/opendir/popen
 * resources and the exported std streams). Shared between vfs.c,
 * vfs_stream.c and vfs_io_driver.c. */
typedef struct io_private io_private;
struct io_private
{
	const ph7_io_stream *pStream; /* Underlying IO device */
	void *pHandle; /* IO handle */
	/* Unbuffered IO */
	SyBlob sBuffer; /* Working buffer */
	sxu32 nOfft;    /* Current read offset */
	/* What the opener was ASKED for. php reports both back from
	 * stream_get_meta_data() and neither was retained here, so the `uri` and
	 * `mode` keys of that array simply did not exist. sUri stays EMPTY for a
	 * stream php opens without a wrapper (a popen() pipe), which is exactly
	 * when php omits the key. */
	SyBlob sUri;     /* the path/URI as the opener received it */
	char zMode[16];  /* the mode string, php's own field width */
	/* Per-handle settings the stream_set_* family writes. */
	sxu32 nChunk;    /* stream_set_chunk_size(), php's 8192 by default */
	sxu8 bNonBlock;  /* stream_set_blocking(false) took effect at the descriptor */
	sxu8 bHasTimeout;/* stream_set_timeout() armed one, so an EAGAIN read EXPIRED */
	sxu8 bTimedOut;  /* the last read expired; php's meta `timed_out`, cleared by the next */
	sxu8 bEof;       /* a read on this handle has already come back empty */
	sxu8 bDir;       /* opendir()/dir() handle rather than a byte stream */
	sxu32 iMagic;   /* Sanity check to avoid misuse */
};
#define IO_PRIVATE_MAGIC 0xFEAC14
/* A user-facing handle (fopen/opendir/popen) that has been fclose()'d/closedir()'d/
 * pclose()'d keeps its io_private alive but stamped with this magic, so every
 * ph7_value that still references it observes a closed resource
 * (gettype()=='resource (closed)', is_resource()==false), matching php. */
#define IO_PRIVATE_CLOSED_MAGIC 0xC105ED
/* Make sure we are dealing with a valid io_private instance */
#define IO_PRIVATE_INVALID(IO) ( IO == 0 || IO->iMagic != IO_PRIVATE_MAGIC )
PH7_PRIVATE void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut);
PH7_PRIVATE void SetIOPrivateOpenedAs(io_private *pDev,const char *zUri,int nUriLen,const char *zMode,int nModeLen);
PH7_PRIVATE void MarkIOPrivateClosed(io_private *pDev);
/* "Failed to open stream" warning helper (vfs.c, errno-based); used by the
 * fopen/opendir/file_* family in vfs_stream.c. */
PH7_PRIVATE void VfsThrowOpenWarning(ph7_context *pCtx,const char *zFile);
PH7_PRIVATE int PH7_VfsStatDoubleUp(ph7_value *pIn,ph7_value *pOut);
PH7_PRIVATE const char * VfsStrerror(int iErr);
/* Stream-device predicates (vfs_io_driver.c) */
PH7_PRIVATE int is_php_stream(const ph7_io_stream *pStream);
PH7_PRIVATE int is_data_stream(const ph7_io_stream *pStream);
/* Which php:// sub-stream a handle opened (PH7_IO_STREAM_*, 0 when unknown).
 * stream_get_meta_data() names MEMORY, TEMP and STDIO apart, and the device
 * itself is the only place that knows. */
PH7_PRIVATE int PH7_PhpStreamKind(void *pHandle);
/* The POSIX descriptor behind an open handle, or -1 for a device that has none
 * (a memory buffer, a data:// payload, a userland wrapper) and on Windows,
 * where the file devices carry a HANDLE instead. Only the settings php applies
 * AT the descriptor need it. */
PH7_PRIVATE int PH7_StreamPosixFd(io_private *pDev);
/* Is this a handle php's plain-files device would own: a file, a pipe, a
 * standard stream? */
PH7_PRIVATE int PH7_StreamIsPlainDevice(io_private *pDev);
/* 1 / 0 / -1 ("ask the device instead"): can this handle report a position?
 * php's stream_get_meta_data() `seekable` is exactly this question. */
PH7_PRIVATE int PH7_StreamHandleCanSeek(io_private *pDev);
/* The php:// sub-streams, as PH7_PhpStreamKind() reports them. */
#define PH7_IO_STREAM_STDIN  1 /* php://stdin */
#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */
#define PH7_IO_STREAM_STDERR 3 /* php://stderr */
#define PH7_IO_STREAM_OUTPUT 4 /* php://output */
#define PH7_IO_STREAM_MEMORY 5 /* php://memory, php://temp, and data:// payloads */
PH7_PRIVATE int PH7_Utf8Read(
  const unsigned char *z,         /* First byte of UTF-8 character */
  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */
  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */
);
PH7_PRIVATE sxi32 PH7_Utf8ReadStrict(const unsigned char *z,sxu32 n,sxu32 *pLen);
/* parse.c function prototypes */
PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc);
PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot);
PH7_PRIVATE int PH7_ExprContainsNullsafe(ph7_expr_node *pNode);
PH7_PRIVATE int PH7_ExprNodeIsThis(ph7_expr_node *pNode);
PH7_PRIVATE sxi32 GenStateWriteTargetCheck(ph7_gen_state *pGen,ph7_expr_node *pTarget,int bUnset);
PH7_PRIVATE void PH7_ExprSubtreeSpan(ph7_expr_node *pNode,SyToken **ppMin,SyToken **ppMax);
PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext);
PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd);
/* TRUE when a KEYWORD token opens `[static] fn[&](…) =>` rather than naming a
 * variable/member/label ($fn, $o->fn, C::fn, \A\fn, f(fn: 1)). Every raw-token
 * lookahead that has to step over an arrow function must ask this first; the
 * test is positional, so a MALFORMED `fn` still reaches the arrow parser and
 * keeps php's `expecting "("`. */
PH7_PRIVATE int PH7_TokenOpensArrowFunc(SyToken *pStart,SyToken *pTok,SyToken *pEnd);
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
PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData);
PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved);
PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...);
PH7_PRIVATE sxi32 PH7_GenSyntaxError(ph7_gen_state *pGen,SyToken *pTok,const char *zExpecting);
PH7_PRIVATE sxi32 PH7_CompileScript(ph7_vm *pVm,SyString *pScript,sxi32 iFlags);
/* constant.c function prototypes */
PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm);
/* vm.c reference/frame internals shared with vm_builtin_var.c */
PH7_PRIVATE VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);
PH7_PRIVATE sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);
PH7_PRIVATE int VmDropFrameLocalSlot(ph7_vm *pVm,sxu32 nIdx);
PH7_PRIVATE void VmDropFrameRefEntry(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry);
PH7_PRIVATE sxu32 PH7_VmSlotHolderCount(ph7_vm *pVm,sxu32 nIdx);
PH7_PRIVATE void PH7_VmReleaseUnheldSlot(ph7_vm *pVm,sxu32 nIdx);
PH7_PRIVATE void PH7_VmRebindVarSlot(ph7_vm *pVm,VmFrame *pFrame,SyHashEntry *pEntry,
	const char *zName,sxu32 nByte,sxu32 nIdx);
PH7_PRIVATE void PH7_VmBindVarSlot(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte,
	sxu32 nIdx);
PH7_PRIVATE int PH7_VmVarNameIsInternal(const char *zName,sxu32 nByte);
/* vm_builtin_var.c function prototypes (rows stay in vm.c's aVmFunc[]) */
PH7_PRIVATE sxi32 VmUnsetVarByName(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte);
PH7_PRIVATE sxi32 VmUnsetVarByNameEx(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte,int bNameGuard);
PH7_PRIVATE int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_debug_type(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_settype(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_resource_id(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg);
/* vm_builtin_lang.c function prototypes (rows stay in vm.c's aVmFunc[]) */
PH7_PRIVATE sxi32 VmClassConstEvalOnDemand(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);
PH7_PRIVATE void VmDeprecatedConstNotice(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pMember);
PH7_PRIVATE ph7_value * VmEnumCaseBackingValue(ph7_vm *pVm,ph7_class_attr *pCase);
PH7_PRIVATE sxi32 VmEnumMaterialize(ph7_vm *pVm,ph7_class *pClass);
PH7_PRIVATE void VmExpandConstantWithNotice(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut);
PH7_PRIVATE sxi32 VmExpandConstantOnce(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut);
PH7_PRIVATE void VmTrackOutput(ph7_vm *pVm, sxu32 nLen);
PH7_PRIVATE ph7_value * VmExtractMemObj(ph7_vm *pVm,const SyString *pName,int bDup,int bCreate);
/* D1 commit 2: deferred-lvalue-path capture (built by the LOAD_IDX/MEMBER record modes) */
PH7_PRIVATE VmDeferredPath * VmDeferPathNew(ph7_vm *pVm,int eRoot,sxu32 nRootIdx,const SyString *pName);
PH7_PRIVATE VmDeferredPath * VmDeferPathNewPrefetch(ph7_vm *pVm,int nKind,ph7_class *pClass,
	const SyString *pName,ph7_value *pVal);
PH7_PRIVATE sxi32 VmDeferPathPushElem(VmDeferredPath *pPath,ph7_value *pKey);
PH7_PRIVATE sxi32 VmDeferPathPushAppend(VmDeferredPath *pPath);
PH7_PRIVATE sxi32 VmDeferPathPushProp(VmDeferredPath *pPath,const SyString *pName);
PH7_PRIVATE void VmFreeDeferredPath(VmDeferredPath *pPath);
PH7_PRIVATE VmCoalStrOff * VmCoalStrOffNew(ph7_vm *pVm,ph7_value *pKey);
PH7_PRIVATE void VmFreeCoalStrOff(VmCoalStrOff *pCoal);
PH7_PRIVATE VmMagicCall * VmMagicCallNew(ph7_vm *pVm,ph7_class_instance *pRecv,
	ph7_class *pClass,const SyString *pName);
PH7_PRIVATE void VmFreeMagicCall(VmMagicCall *pPend);
PH7_PRIVATE void VmExpandUserConstant(ph7_value *pVal,void *pUserData);
PH7_PRIVATE ph7_class * VmExtractEnumClass(ph7_vm *pVm,ph7_value *pName);
PH7_PRIVATE int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_enum_cases(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_enum_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_enum_from(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_enum_tryfrom(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_extension_loaded(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_loaded_extensions(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg);
/* vm.c frame/backtrace internals shared with vm_builtin_error.c */
PH7_PRIVATE VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);
PH7_PRIVATE void VmBuildBacktrace(ph7_vm *pVm,sxi32 iOptions,sxi32 iLimit,ph7_value *pList);
PH7_PRIVATE void PH7_VmTraceToString(ph7_vm *pVm,ph7_value *pTrace,int bMainMarker,SyBlob *pOut);
PH7_PRIVATE void PH7_VmFrameActualArgs(ph7_vm *pVm,VmFrame *pFrame,ph7_value *pArray);
/* vm_builtin_error.c function prototypes (rows stay in vm.c's aVmFunc[]) */
PH7_PRIVATE int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_error_clear_last(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_error_get_last(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg);
/* Autoload-callback record (spl_autoload_register in vm_include.c; walked by
 * VmTriggerAutoload in vm.c) */
typedef struct VmAutoloadCB VmAutoloadCB;
struct VmAutoloadCB
{
	ph7_value sCallback; /* Autoload callback (string or [obj,method] array) */
};
/* Shutdown-callback record (register_shutdown_function in vm_builtin_call.c;
 * invoked by VmInvokeShutdownCallbacks in vm.c) */
typedef struct VmShutdownCB VmShutdownCB;
struct VmShutdownCB
{
	ph7_value sCallback; /* Shutdown callback */
	ph7_value aArg[10];   /* Callback arguments (10 maximum arguments) */
	int nArg;             /* Total number of given arguments */
};
/* Operand-stack guard slack (vm.c allocator; checked by the call machinery) */
#define VM_STACK_GUARD 16
/* vm.c closure/exception internals shared with vm_builtin_call.c */
PH7_PRIVATE int VmValueIsClosure(ph7_vm *pVm,ph7_value *pVal);
PH7_PRIVATE int PH7_VmFuncDisplayName(ph7_vm *pVm,ph7_vm_func *pFunc,const char **pzOut);
PH7_PRIVATE sxi32 VmClosureUnwrap(ph7_vm *pVm,ph7_value *pVal,ph7_value *pOut);
PH7_PRIVATE sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);
PH7_PRIVATE sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);
PH7_PRIVATE ph7_value * VmNewOperandStack(ph7_vm *pVm,sxu32 nInstr);
/* Fiber/generator trampoline state (BYTECODE stages 2-4); shared between
 * vm.c's interpreter and vm_exec_ctx.c's park/resume machinery. */
/*
 * Boundary state of one VmByteCodeExec activation (BYTECODE.md stage 1):
 * everything the executor must restore to continue an activation after a
 * nested call returns. pc/pTos are authoritative here only at activation
 * boundaries — the dispatch loop keeps them in locals for the hot path and
 * syncs around the call epilogue and the terminal labels. Stage 2 stacks
 * these records to replace the native recursion.
 */
typedef struct VmExecState VmExecState;
struct VmExecState
{
	VmInstr *aInstr;        /* Bytecode of this activation */
	ph7_value *pStack;      /* Operand-stack base (owned by this activation) */
	ph7_value *pTos;        /* Top-of-stack (synced at boundaries) */
	sxu32 nStackCap;        /* pStack's allocated slot count; grows when an OP_SPREAD in
	                         * this activation reallocs the operand stack (see
	                         * VmGrowOperandStack). Saved/restored with the activation. */
	sxu32 nStackOrig;       /* The activation's ORIGINAL (ungrown) capacity — nMaxStack+guard,
	                         * fixed at entry. VmGrowOperandStack sizes headroom relative to
	                         * THIS (not the grown nStackCap) so capacity can't ratchet up
	                         * across statements that share one operand stack. */
	sxi32 pc;               /* Program counter (synced at boundaries) */
	sxu32 nExceptionBase;   /* Exception-stack depth at entry (finally-drain floor) */
	sxu32 nFinallyActBase;  /* aFinallyAction depth at entry: actions above it belong to
	                         * this activation and are DISCARDED (refs released) when the
	                         * activation ends — a `return` inside a redirect-entered
	                         * finally short-circuits OP_END_FINALLY, orphaning its
	                         * pending action (an FA_RETHROW holding the swallowed
	                         * exception), which would otherwise be mis-popped by an
	                         * enclosing function's next END_FINALLY. */
	VmFrame *pEntryFrame;   /* Active frame at entry (exec identity for VmRecordedResume) */
	ph7_value *pResult;     /* Where the terminal OP_DONE stores the result (or NULL) */
	sxu32 *pLastRef;        /* By-ref return out-param (or NULL) */
	ph7_vm_func *pEnforceRetFunc; /* Return-type enforcement target (user-fn bodies only) */
	sxu8 is_callback;       /* TRUE only for a C->PHP callback trampoline activation */
	sxu8 bReturnPropagates; /* TRUE only for a catch/finally mini-program */
};
/*
 * One in-flight user-function call: what the caller's OP_CALL set up and the
 * pop boundary (VmCallFinish) must tear down.
 */
typedef struct VmCallRecord VmCallRecord;
struct VmCallRecord
{
	ph7_vm_func *pVmFunc;   /* Callee */
	VmFrame *pFrame;        /* Callee's VM frame (entered by the OP_CALL setup) */
	ph7_value *pFrameStack; /* Callee's operand stack (owned; freed here). NULL when the body was skipped */
	sxu32 nStackCap;        /* pFrameStack's allocated slot count — nMaxStack+VM_STACK_GUARD
	                         * at setup, updated if an OP_SPREAD in the callee grew it; the
	                         * pop-time recycle releases exactly this many slots */
	sxu32 nLastRef;         /* Callee body's last-referenced slot (by-ref return) */
	sxu8 bSelfPushed;       /* TRUE when the setup pushed onto pVm->aSelf */
};
/*
 * One node of the in-loop call-record stack (BYTECODE stage 2): the caller's
 * activation to restore plus the in-flight call to finish, linked to the
 * next-outer record. Nodes are pool-allocated individually so pointers into
 * them (sState.pLastRef aims at sCall.nLastRef while the callee runs) stay
 * stable — a growable array would invalidate them on realloc. The stack is a
 * LOCAL of each native VmByteCodeExec invocation: an inner native entry
 * (mini-program, C->PHP callback, ctx resume) can never unwind records that
 * belong to an outer invocation, preserving the old nesting isolation by
 * construction.
 */
typedef struct VmCallFrame VmCallFrame;
struct VmCallFrame
{
	VmExecState sCaller;   /* Caller activation, restored on pop */
	VmCallRecord sCall;    /* The in-flight call, finished (VmCallFinish) on pop */
	VmCallFrame *pPrev;    /* Next-outer record, or NULL at this invocation's base */
};
typedef struct VmParkedSegment VmParkedSegment;
/*
 * BYTECODE stage 4: a Fiber::suspend() from inside a nested PHP call parks the
 * whole trampoline record segment here instead of unwinding it. The records,
 * their VmFrames and operand stacks all stay alive on the heap (that IS what a
 * suspended fiber is); only the dispatch loop's pointers move into the ctx.
 * Resume re-pushes the chain and continues INSIDE the innermost callee.
 */
struct VmParkedSegment
{
	VmExecState sState;    /* Innermost activation — resume re-enters here (pTos synced) */
	VmCallFrame *pCallTop; /* Parked record chain (caller activations toward the body) */
	VmFrame *pTopFrame;    /* pVm->pFrame at suspend (innermost callee / open-try frame) */
	sxu32 nOldExcBase;     /* pCtx->nExceptionBase at park — resume rebases the segment's
	                        * absolute nExceptionBase floors by (newBase - nOldExcBase) */
	sxu32 nOldFinBase;     /* pCtx->nFinallyBase at park — resume rebases the segment's
	                        * absolute nFinallyActBase floors by its OWN delta (the two
	                        * stacks move independently) */
	int nRecords;          /* Chain length: each record contributed one nRecursionDepth++
	                        * (and, if bSelfPushed, one aSelf push) that VmCallFinish never
	                        * ran. Deactivate that accounting while parked, reactivate on
	                        * resume; an abandoned segment stays deactivated. */
};

PH7_PRIVATE sxi32 VmByteCodeExec(ph7_vm *pVm,VmInstr *aInstr,ph7_value *pStack,int nTos,
	ph7_value *pResult,sxu32 *pLastRef,int is_callback,sxi32 nPc,
	ph7_vm_func *pEnforceRetFunc,int bReturnPropagates,
	VmParkedSegment *pAdoptSegment,ph7_value **ppBaseOwner,
	sxu32 *pnBaseCap,sxu32 nStackOrig);
/* vm.c frame/type-enforcement internals shared with vm_exec_ctx.c (and the
 * upcoming vm_error.c) */
PH7_PRIVATE int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass);
PH7_PRIVATE sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict,
	ph7_class *pSelf);
PH7_PRIVATE void VmMaterializeIntTyped(ph7_value *pVal, sxu32 nType);
PH7_PRIVATE void VmDropResumeTarget(ph7_vm *pVm, VmFrame *pFrame);
PH7_PRIVATE sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue,int bCloneInit);
PH7_PRIVATE sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict);
PH7_PRIVATE void VmExcReleaseAll(ph7_vm *pVm,SySet *pSet);
PH7_PRIVATE const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf);
PH7_PRIVATE int VmFuncHasReturnType(ph7_vm_func *pFunc);
PH7_PRIVATE int PH7_VmHookSplitName(SyString *pName,SyString *pProp,const char **pzKind);
PH7_PRIVATE int PH7_VmHookFuncName(ph7_class *pClass,ph7_vm_func *pFunc,SyBlob *pOut);
PH7_PRIVATE sxu32 VmFuncRequiredArgCount(ph7_vm_func *pFunc,sxu32 *pnNonVariadic);
PH7_PRIVATE void VmLeaveFrame(ph7_vm *pVm);
PH7_PRIVATE int VmNativeNestingExceeded(ph7_vm *pVm);
PH7_PRIVATE sxi32 VmNativeNestingFatal(ph7_vm *pVm);
PH7_PRIVATE VmFrame * VmNewFrame(ph7_vm *pVm, void *pUserData, ph7_class_instance *pThis);
PH7_PRIVATE ph7_class *VmHintScopeClass(ph7_vm *pVm, ph7_class *pDecl, ph7_class *pUsing);
PH7_PRIVATE int VmClassHintMatches(ph7_vm *pVm,const SyString *pName,ph7_class *pScope,
	ph7_value *pVal,ph7_class **ppResolved);
PH7_PRIVATE const char *VmHintTextResolved(ph7_vm *pVm,const SyString *pDeclared,ph7_class *pScope,
	char *zBuf,sxu32 nBuf);
PH7_PRIVATE ph7_class *VmResolveTypeClass(ph7_vm *pVm, const SyString *pCN, ph7_class *pSelf);
PH7_PRIVATE const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf);
PH7_PRIVATE const char *VmClassHintTypeName(const SyString *pAsWritten,ph7_class *pResolved,
	int bNullable,char *zBuf,sxu32 nBuf);
PH7_PRIVATE sxi32 VmThrowBuiltinTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName, sxu32 nPassed,sxu32 nRequired,sxu32 nMaxDeclared);
PH7_PRIVATE sxi32 VmThrowBuiltinTooManyArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName, sxu32 nPassed,sxu32 nMax,sxu32 nRequired);
PH7_PRIVATE sxi32 VmThrowTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName, sxu32 nPassed,sxu32 nRequired,sxu32 nNonVariadic,int bCallSite);
PH7_PRIVATE void PH7_VmWarnByRefValueGiven(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee,sxu32 nArgPos,SyString *pArgName);
PH7_PRIVATE sxi32 VmThrowByRefRefusal(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,sxu32 nArgPos,SyString *pArgName);
PH7_PRIVATE sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven);
PH7_PRIVATE sxi32 VmVariadicElementTypeCheck(ph7_vm *pVm,ph7_class *pSelfHint,ph7_vm_func *pCallee,ph7_vm_func_arg *pFormal,ph7_value *pVal,sxu32 nArgPos,int bCallIsStrict);
PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex);
/* Argument-unpacking key capture (PHP 8.1 named-parameter semantics for spreads).
 * `pMap->aNames` is COMPILE-TIME metadata indexed by compile-time argument
 * position, but a runtime spread expands its slot to a variable element count,
 * so any spread that expands to !=1 element shifts the following actual stack
 * positions out of alignment with aNames — and the element keys (which PHP 8.1
 * treats as named arguments) are otherwise discarded. OP_SPREAD records one
 * VmSpreadRun per expansion plus one VmSpreadKey per element (in order) on the
 * VM; CALL/NEW replay them (VmBuildEffectiveArgMap) into an effective map with
 * one name entry per ACTUAL slot, then let the existing named-argument resolver
 * run unchanged. The same runs give each call its own argument-count growth
 * (VmSpreadOwnExtra). This call's runs are consumed (truncated) at the CALL. */
typedef struct VmSpreadRun VmSpreadRun;
struct VmSpreadRun {
	ph7_value *pStart;   /* First stack slot the expansion wrote (the source slot) */
	sxu32 nCount;        /* Elements produced (0 for an empty array) */
	sxu32 nKeyStart;     /* aSpreadKey index of this run's first element key */
	sxu32 nBlobStart;    /* sSpreadKeyBlob length before this run's keys were appended */
};
typedef struct VmSpreadKey VmSpreadKey;
struct VmSpreadKey {
	sxu32 nOff;          /* Byte offset into pVm->sSpreadKeyBlob (valid iff nLen>0) */
	sxu32 nLen;          /* Key length; 0 == integer key == positional element */
};
#define VM_STACK_UNMODELED SXU32_HIGH /* shared by vm.c (stack modeling) and vm_exec.c */
/* vm_ops_*.c — opcode handlers extracted from VmByteCodeExecBody. The loop
 * syncs pTos/pc into its VmExecState, calls the handler, reloads them and
 * maps the returned code onto its labels. */
typedef enum VmOpRc {
	VM_OP_NEXT = 0,   /* arm done: fall to the loop's trailing pc++ */
	VM_OP_ABORT,      /* -> the loop's Abort label */
	VM_OP_EXCEPTION   /* -> the loop's Exception label */
} VmOpRc;
PH7_PRIVATE int VmRecordedResume(ph7_vm *pVm,sxi32 *pResumePc,VmFrame *pEntryFrame,VmInstr *aInstr);
PH7_PRIVATE void VmPopOperand(ph7_value **ppTos, sxi32 nPop);
PH7_PRIVATE void VmForeachStepUnlink(ph7_foreach_info *pInfo,ph7_foreach_step *pStep);
PH7_PRIVATE int VmHookGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName);
PH7_PRIVATE void VmForeachHashmapStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,int bPop);
PH7_PRIVATE void VmForeachStepAbandon(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,ph7_class_instance *pThis);
PH7_PRIVATE int VmClassAllowsDynamicProps(ph7_vm *pVm,ph7_class *pClass);
PH7_PRIVATE int VmClassHasAttributeNamed(ph7_class *pClass,const char *zName,sxu32 nName);
/* __PHP_Incomplete_Class: unserialize()'s carrier object (vm.c helpers).
 * PH7_INCOMPLETE_MAGIC_MEMBER is php's MAGIC_MEMBER — the dynamic property that
 * remembers the original class name; the serializer strips it back out. */
#define PH7_INCOMPLETE_MAGIC_MEMBER "__PHP_Incomplete_Class_Name"
PH7_PRIVATE int PH7_VmIsIncompleteClass(ph7_vm *pVm,ph7_class *pClass);
PH7_PRIVATE void PH7_VmIncompleteMsg(ph7_vm *pVm,ph7_class_instance *pThis,const char *zWhat,SyBlob *pOut);
PH7_PRIVATE void PH7_VmIncompleteAccessWarn(ph7_vm *pVm,ph7_class_instance *pThis,SyString *pFuncName);
PH7_PRIVATE void VmRecreateDeclaredAttr(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_attr *pAttr,VmClassAttr **ppAttr);
PH7_PRIVATE int VmMemberCtxIsLookup(sxi32 iP2);
PH7_PRIVATE int VmMemberCtxWantsValue(sxi32 iP2);
PH7_PRIVATE int VmMemberNextIsWrite(const VmInstr *pNext);
PH7_PRIVATE int VmMemberNextIsRmw(const VmInstr *pNext);
PH7_PRIVATE int VmNextIsCompoundAssign(const VmInstr *pNext);
PH7_PRIVATE sxi32 VmSpreadOwnExtra(ph7_vm *pVm, sxi32 iP1, ph7_value *pTos);
PH7_PRIVATE VmCallArgMap *VmEffCallArgMap(ph7_vm *pVm, VmInstr *pInstr, ph7_value *pArg, sxu32 nActual, VmCallArgMap *pStorage);
PH7_PRIVATE int VmMagicGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind);
PH7_PRIVATE void VmMagicGuardPush(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind);
PH7_PRIVATE void VmMagicGuardPop(ph7_vm *pVm);
PH7_PRIVATE void VmPinMemObjSlot(ph7_vm *pVm,sxu32 nIdx);
PH7_PRIVATE void VmPinMemObjSlotCounted(ph7_vm *pVm,sxu32 nIdx);
PH7_PRIVATE void VmUnpinMemObjSlot(ph7_vm *pVm,sxu32 nIdx);
PH7_PRIVATE sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData);
PH7_PRIVATE int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal);
PH7_PRIVATE sxi32 VmHashmapRefInsert(ph7_hashmap *pMap, const char *zKey, sxu32 nByte, sxu32 nRefIdx);
PH7_PRIVATE void VmWarnCannotUseAsArray(ph7_vm *pVm,sxi32 iFlags);
PH7_PRIVATE sxi32 VmHookRmwConsume(ph7_vm *pVm,sxu32 nIdx);
PH7_PRIVATE void VmHookRmwFreeScratch(ph7_vm *pVm,sxu32 nIdx);
PH7_PRIVATE sxi32 VmHookSetDispatch(ph7_vm *pVm,ph7_class_instance *pHThis,ph7_class_attr *pHAttr,sxu32 nBackIdx,ph7_value *pValue);
PH7_PRIVATE void VmMagicSetDispatch(ph7_vm *pVm,ph7_class_instance *pSetThis,const SyString *pName,ph7_value *pValue);
PH7_PRIVATE ph7_exception * VmExcActivate(ph7_vm *pVm,ph7_exception *pCompiled);
PH7_PRIVATE ph7_exception * VmExcLive(ph7_vm *pVm,ph7_exception *pCompiled);
PH7_PRIVATE sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight);
PH7_PRIVATE sxu32 VmComputeMaxStack(ph7_vm *pVm, VmInstr *aInstr, sxu32 nInstr);
PH7_PRIVATE sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase);
PH7_PRIVATE sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName);
PH7_PRIVATE void VmHookRmwDropTop(ph7_vm *pVm);
PH7_PRIVATE sxi32 VmInitCallContext(ph7_context *pOut, ph7_vm *pVm, ph7_user_func *pFunc, ph7_value *pRet, sxi32 iFlags);
PH7_PRIVATE void VmMaterializeCatchReturn(ph7_vm *pVm, ph7_value *pResult, VmFrame *pEntryFrame);
PH7_PRIVATE ph7_value * VmOperandStackAlloc(ph7_vm *pVm, sxu32 nSlots);
PH7_PRIVATE void VmOperandStackRecycle(ph7_vm *pVm, ph7_value *pStack, sxu32 nCap);
PH7_PRIVATE ph7_vm_func * VmOverload(ph7_vm *pVm, ph7_vm_func *pList, ph7_value *aArg, int nArg);
PH7_PRIVATE void VmReleaseCallContext(ph7_context *pCtx);
PH7_PRIVATE sxi32 VmResolveNamedArgs(ph7_vm *pVm, VmCallArgMap *pMap, ph7_vm_func_arg *aFormalArg, sxu32 nNonVariadic, sxi32 iVariadicIdx, sxu32 nActual, sxi32 *aSlot, sxu8 *aUsed);
PH7_PRIVATE void VmSpreadExpandMap(ph7_vm *pVm, ph7_value **ppTos, ph7_hashmap *pMap, int bVarSource);
PH7_PRIVATE sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData);
PH7_PRIVATE int VmStringWantsPerlIncr(ph7_value *pVal);
PH7_PRIVATE sxi32 VmSuspendCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, sxi32 pc, sxi32 nTos);
PH7_PRIVATE int VmExcMatches(ph7_exception *pExc,ph7_exception *pCompiled);
PH7_PRIVATE void VmSpreadConsume(ph7_vm *pVm);
PH7_PRIVATE VmOpRc VmExecOpSwitch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpCatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpLoadException(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpSpaceship(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpGe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpLe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpTne(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpTeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpNeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpLor(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpShrStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpShr(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpMulStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpLoadList(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpUnsetVar(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpConsume(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpMatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpClone(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpThrow(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpNullcStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpDiv(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpModStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpMod(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpSubStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpSub(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpPowStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE void PH7_MemObjPow(ph7_value *pBase,ph7_value *pExp,ph7_value *pOut);
PH7_PRIVATE VmOpRc VmExecOpLoadMap(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpLoadIdx(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpLoadClosure(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpStoreIdxRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpStoreRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpMember(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpNew(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpForeachInit(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
PH7_PRIVATE VmOpRc VmExecOpForeachStep(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr);
/* vm_error.c — error/diagnostics/type-enforcement machinery shared with vm.c */
PH7_PRIVATE sxi32 PH7_VmErrPhpBit(sxi32 iErr);
PH7_PRIVATE void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);
PH7_PRIVATE sxi32 VmEnterFrame(ph7_vm *pVm,void *pUserData,ph7_class_instance *pThis,VmFrame **ppFrame);
PH7_PRIVATE void VmExcRelease(ph7_vm *pVm,ph7_exception *pExc);
PH7_PRIVATE void VmClearFramePending(VmFrame *pFrame);
PH7_PRIVATE sxi32 VmDrainCrossedTrys(ph7_vm *pVm,sxu32 nCross,sxu32 nFloor);
PH7_PRIVATE sxi32 VmLocalExecIntoObj(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj,int bReturnPropagates);

PH7_PRIVATE sxi32 VmArithOperandCheck(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,const char *zOp,SyBlob *pMsgOut);
PH7_PRIVATE const char * VmArithTypeName(ph7_value *pVal);
PH7_PRIVATE const char * VmArithValueName(ph7_value *pVal);
PH7_PRIVATE void VmIncDecTypeErrorMsg(ph7_value *pVal,int bIncr,SyBlob *pMsgOut);
PH7_PRIVATE sxi32 VmUnaryArithOperandCheck(ph7_vm *pVm,ph7_value *pVal,SyBlob *pMsgOut);
PH7_PRIVATE void VmBitNotTypeErrorMsg(ph7_value *pVal,SyBlob *pMsgOut);
PH7_PRIVATE void VmStringBitwise(ph7_value *pLeft,ph7_value *pRight,int cOp,SyBlob *pOut);
PH7_PRIVATE sxi32 VmCheckReadonlyMutate(ph7_vm *pVm,sxu32 nIdx);
PH7_PRIVATE sxi32 VmCheckSetVisibility(ph7_vm *pVm,ph7_class *pOwner,ph7_class_attr *pAttr);
PH7_PRIVATE sxi32 VmCloneApplyUpdate(ph7_vm *pVm,ph7_class_instance *pClone, const char *zName,sxu32 nName,ph7_value *pValue);
PH7_PRIVATE void VmCoalesceDisarm(ph7_vm *pVm);
PH7_PRIVATE sxi32 VmConstCycleThrow(ph7_vm *pVm);
PH7_PRIVATE ph7_class * VmCurrentSelf(ph7_vm *pVm);
PH7_PRIVATE sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy);
PH7_PRIVATE sxi32 VmEnforceTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue);
PH7_PRIVATE sxi32 VmCheckTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue);
PH7_PRIVATE sxi32 VmEnforceArgType(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_vm_func_arg *pFormal,
	sxu32 nArgPos,ph7_value *pVal,int bStrict,ph7_class *pSelfHint);
PH7_PRIVATE int VmClassStaticDeferPending(ph7_class *pClass);
PH7_PRIVATE sxi32 PH7_VmMaterializeClassStatics(ph7_vm *pVm,ph7_class *pClass);
PH7_PRIVATE sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue);
PH7_PRIVATE sxi32 VmEnumMaterializeCase(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pCase);
PH7_PRIVATE int VmFinallyAdvance(ph7_vm *pVm, VmInstr *aInstr, int *pnCross, sxu32 *pPc);
PH7_PRIVATE int VmRecursionExceeded(ph7_vm *pVm);
PH7_PRIVATE sxi32 VmRecursionFatal(ph7_vm *pVm);
PH7_PRIVATE int VmValueIsLossyToInt(ph7_value *pVal);
PH7_PRIVATE sxi32 VmRejectFloatOperand(ph7_vm *pVm,ph7_value *pVal);
PH7_PRIVATE sxi32 VmThrowArgNotPassed(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName, sxu32 nArg,SyString *pArgName);
PH7_PRIVATE sxi32 VmThrowBuiltinError(ph7_vm *pVm,const char *zClass,sxu32 nClass,SyBlob *pMsg);
PH7_PRIVATE sxi32 VmThrowFixedError(ph7_vm *pVm, const char *zClass, const char *zMsg);
PH7_PRIVATE sxi32 VmThrowFromVm(ph7_vm *pVm, const char *zClass, const char *zMsg, sxu32 nMsg);
PH7_PRIVATE sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg);
PH7_PRIVATE sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad);
PH7_PRIVATE sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr);
PH7_PRIVATE sxi32 VmUncaughtException(ph7_vm *pVm, ph7_class_instance *pThis);
/* vm_arg_check.c — builtin arity/signature enforcement, called from vm.c */
PH7_PRIVATE void VmSetBuiltinArity(ph7_vm *pVm);
PH7_PRIVATE void VmSetBuiltinSignatures(ph7_vm *pVm);
/* Signature-string derivations, shared with the native-class builder: one
 * PHP-style parameter list ("string $s, int $o = 0") is the single source of a
 * callee's arity bounds and by-ref positions, for a builtin and a native method
 * alike — which is how a native method gets the too-few/too-many ArgumentCountError
 * that a prelude-declared method never had. */
PH7_PRIVATE void VmDeriveArityFromSig(const char *zSig,sxi16 *pnMin,sxu8 *pbAtLeast,sxi16 *pnMax,sxu8 *pbHasMax);
PH7_PRIVATE sxu32 VmDeriveByRefMaskFromSig(const char *zSig);
PH7_PRIVATE int VmBuiltinPrefersRef(SyString *pName);
PH7_PRIVATE sxi32 PH7_VmBindNamedArgsToSig(ph7_context *pCtx,ph7_user_func *pFunc,VmCallArgMap *pMap,int *pnArg,ph7_value **apArg);
PH7_PRIVATE sxi32 VmEnforceBuiltinArgTypes(ph7_context *pCtx,ph7_user_func *pFunc,int nGiven,ph7_value **apArg);
PH7_PRIVATE sxi32 PH7_VmScreenByRefArgShapes(ph7_context *pCtx,ph7_user_func *pFunc,VmCallArgMap *pMap,int nGiven,ph7_value **apArg);
PH7_PRIVATE int PH7_VmSigParamName(const char *zSig,int nPos,SyString *pOut);
PH7_PRIVATE int PH7_VmArgRefusedByRef(VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal);
PH7_PRIVATE sxi32 PH7_VmResolveDeferredArgs(ph7_vm *pVm,ph7_value *pArg,ph7_value *pTos,
	ph7_vm_func_arg *pFormal,sxu32 nFormal,sxu32 nByRefMask,int bAllByRef,int bAllByValue,
	VmCallArgMap *pCallMap);
PH7_PRIVATE void PH7_VmArgTempCallNotice(ph7_vm *pVm,VmCallArgMap *pMap,sxu32 nPos,ph7_value *pVal);
PH7_PRIVATE int PH7_ArgSatisfiesString(ph7_value *pArg);
PH7_PRIVATE void VmDeprecatedAttrNotice(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass);
/* vm_exec_ctx.c — Fiber/Generator/Closure engine shared with vm.c */
PH7_PRIVATE sxi32 PH7_VmInstallClosureNative(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_VmInstallFiberNative(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_VmInstallGeneratorNative(ph7_vm *pVm);
PH7_PRIVATE int vm_builtin_Closure_bindTo(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Closure_call(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Closure_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Closure_fromCallable(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);
PH7_PRIVATE ph7_class_instance * VmCreateClosure(ph7_vm *pVm, const SyString *pName, ph7_class_instance *pBoundThis, const SyString *pScope);
PH7_PRIVATE ph7_class * VmFccResolveScope(ph7_vm *pVm, ph7_value *pTarget);
PH7_PRIVATE ph7_class * PH7_VmResolveScopeName(ph7_vm *pVm, const char *zCls, sxu32 nCls);
PH7_PRIVATE int PH7_VmIsScopeKeyword(const char *zName,sxu32 nName);
PH7_PRIVATE ph7_class_instance * VmFccWrapValue(ph7_vm *pVm, ph7_value *pValue);
PH7_PRIVATE void PH7_VmByRefArgWriteBack(ph7_vm *pVm,ph7_value *pArg,sxi32 iPreFlags);
PH7_PRIVATE sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx, ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg, int bStrict, ph7_class *pSelfHint, int bCallSiteInMsg, int bAliasByRef);
PH7_PRIVATE ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj);
PH7_PRIVATE ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);
PH7_PRIVATE ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);
PH7_PRIVATE void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);
PH7_PRIVATE void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);
PH7_PRIVATE sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult);
/* vm_include.c function prototypes (rows stay in vm.c's aVmFunc[]) */
PH7_PRIVATE sxi32 VmMountUserClass(ph7_vm *pVm,ph7_class *pClass);
PH7_PRIVATE sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);
PH7_PRIVATE sxi32 VmExecDeferredClass(ph7_vm *pVm,VmDeferredClass *pDefer,VmDeferredReq **ppMissing);
PH7_PRIVATE ph7_user_func * PH7_VmMagicCallFunc(ph7_vm *pVm);
PH7_PRIVATE int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_set_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg);
/* vm_builtin_call.c — callable machinery shared with vm.c's interpreter */
PH7_PRIVATE ph7_class * PH7_VmResolveParentClass(ph7_vm *pVm);
PH7_PRIVATE void VmBoundaryPark(ph7_vm *pVm,sxi32 rc);
/*
 * The status a C->PHP dispatch answers when the callee did NOT return: the
 * builtin driving the loop must abandon it and hand the status straight out.
 * Both members matter and testing only the first is a silent wrong answer —
 * php stops an internal function the moment its callback throws, and an
 * UNCAUGHT throw comes back as PH7_ABORT (VmUncaughtException reports the
 * fatal and answers SXERR_ABORT), not as PH7_EXCEPTION. A loop that tested
 * only PH7_EXCEPTION therefore ran the callback again for every remaining
 * element — repeating its side effects and re-reporting the fatal once per
 * element. Same set VmBoundaryPark parks; see its comment.
 */
#define PH7_CALLBACK_UNWOUND(rc) ((rc) == PH7_EXCEPTION || (rc) == PH7_ABORT)
PH7_PRIVATE sxi32 PH7_VmCallCallbackByValue(ph7_vm *pVm,ph7_value *pFunc,int nArg,
	ph7_value **apArg,ph7_value *pResult,sxu32 nRefOkMask);
PH7_PRIVATE sxi32 VmIterCallMethod(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,ph7_value *pResult);
PH7_PRIVATE sxi32 VmCallClassMethodLsb(ph7_vm *pVm,ph7_class *pCalled,ph7_class_instance *pThis,
	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg,VmCallArgMap *pMap);
PH7_PRIVATE sxi32 VmCallClassMethodWithMap(ph7_vm *pVm,ph7_class_instance *pThis,
	ph7_class_method *pMethod,ph7_value *pResult,int nArg,
	ph7_value **apArg,VmCallArgMap *pMap);
PH7_PRIVATE sxi32 VmCallObjectInvoke(ph7_vm *pVm,ph7_class_instance *pThis,
	int nArg,ph7_value **apArg,ph7_value *pResult,VmCallArgMap *pMap);
PH7_PRIVATE sxi32 VmRaiseNotCallable(ph7_vm *pVm,ph7_class_instance *pThis);
PH7_PRIVATE int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg);
/* builtin.c function prototypes */
PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm);
/* builtin_hash.c function prototypes */
#ifndef PH7_DISABLE_BUILTIN_FUNC
/* Binary-to-hex consumer shared by bin2hex() (builtin.c) and the hash
 * builtins (builtin_hash.c). */
PH7_PRIVATE int HashConsumer(const void *pData,unsigned int nLen,void *pUserData);
#ifndef PH7_DISABLE_HASH_FUNC
PH7_PRIVATE int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_hash_hmac_algos(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_hash_init(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_hash_update(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_hash_final(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_hash_copy(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_hash_pbkdf2(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_hash_hkdf(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_hash_file(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_hash_hmac_file(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_hash_update_file(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_hash_update_stream(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE sxi32 PH7_VmInstallHashContext(ph7_vm *pVm);
/* php's one hash_init() flag: request an HMAC rather than a plain digest. */
#define PH7_HASH_HMAC 1
PH7_PRIVATE int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg);
#endif /* PH7_DISABLE_HASH_FUNC */
PH7_PRIVATE int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
/* builtin_fmt.c function prototypes (PH7_NEED_FMT_AND_INI: compiled whenever
 * disk I/O is enabled, independently of PH7_DISABLE_BUILTIN_FUNC) */
#ifndef PH7_DISABLE_DISK_IO
PH7_PRIVATE int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg);
#endif /* PH7_DISABLE_DISK_IO */
/* builtin_parse.c function prototypes */
#ifndef PH7_DISABLE_BUILTIN_FUNC
/* HTML entity escape engine: shared by the htmlspecialchars/htmlentities
 * family (builtin.c) and filter_var's SANITIZE filters (builtin_parse.c). */
/* The charsets the HTML entity family models: php's own UTF-8 and ISO-8859-1
 * (one byte per character, its VALUE the code point). Everything else keeps
 * php's unsupported-charset warning. */
#define PH7_HTML_CS_UTF8   0
#define PH7_HTML_CS_LATIN1 1
PH7_PRIVATE void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bAll,int bDoubleEncode,int iCs);
PH7_PRIVATE void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bFull,int iCs);
PH7_PRIVATE int HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx);
PH7_PRIVATE void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags,int iCs);
PH7_PRIVATE int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_convert_uuencode(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_convert_uudecode(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_rawurlencode(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_http_build_query(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_parse_str(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_rawurldecode(ph7_context *pCtx,int nArg,ph7_value **apArg);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
#ifndef PH7_DISABLE_DISK_IO
PH7_PRIVATE int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg);
#endif /* PH7_DISABLE_DISK_IO */
/* builtin_string.c function prototypes */
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_levenshtein(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_similar_text(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_str_rot13(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_metaphone(ph7_context *pCtx,int nArg,ph7_value **apArg);
/* builtin_pack.c — the binary-string pair */
PH7_PRIVATE int PH7_builtin_pack(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_unpack(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strnatcmp(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_version_compare(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_count_chars(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_substr_replace(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
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
PH7_PRIVATE int PH7_HashmapNodeIsRef(ph7_hashmap_node *pNode);
PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey);
PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm);
/* hashmap.c engine helpers shared with hashmap_sort.c / hashmap_builtin.c */
PH7_PRIVATE ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode);
PH7_PRIVATE sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict);
PH7_PRIVATE void HashmapRehashIntNode(ph7_hashmap_node *pEntry);
PH7_PRIVATE sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected);
PH7_PRIVATE sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve);
PH7_PRIVATE int HashmapFindValue(ph7_hashmap *pMap,ph7_value *pNeedle,ph7_hashmap_node **ppNode,int bStrict);
PH7_PRIVATE int HashmapValueStrEq(ph7_value *pA,ph7_value *pB,int bUserVisible,sxi32 *pRc);
PH7_PRIVATE sxi32 HashmapStringifyElems(ph7_hashmap *pMap);
PH7_PRIVATE int HashmapFindStringValue(ph7_hashmap *pMap,ph7_value *pNeedle,ph7_hashmap_node **ppNode,sxi32 *pRc);
PH7_PRIVATE sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest);
PH7_PRIVATE sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest);
PH7_PRIVATE sxi32 HashmapInsert(ph7_hashmap *pMap,ph7_value *pKey,ph7_value *pVal);
PH7_PRIVATE sxi32 HashmapLookupIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_hashmap_node **ppNode);
PH7_PRIVATE sxi32 HashmapLookupBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_hashmap_node **ppNode);
/* hashmap_sort.c: the SQLite-derived merge sort and the sort builtin family.
 * Shared with hashmap.c (shuffle/array_unique/array_rand) and referenced from
 * the aHashmapFunc[] registration table; compiled in every mode. */
typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);
PH7_PRIVATE sxi32 PH7_HashmapShuffle(ph7_hashmap *pMap);
PH7_PRIVATE sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData);
PH7_PRIVATE void HashmapSortRehash(ph7_hashmap *pMap);
PH7_PRIVATE int HashmapValueFlagEqual(ph7_vm *pVm,ph7_value *pA,ph7_value *pB,int base,int bFold);
PH7_PRIVATE int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_natsort(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg);
/* hashmap_builtin.c function prototypes (the array_* family; referenced from
 * the aHashmapFunc[] registration table in hashmap.c; compiled in every mode) */
PH7_PRIVATE int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_unshift(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_merge_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_diff_ukey(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_intersect_ukey(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_udiff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_uintersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_udiff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_uintersect_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_intersect_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_multisort(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_max(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_min(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_iterator_to_array(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_iterator_count(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int ph7_iterator_apply(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth);
PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth,int bProp);
PH7_PRIVATE sxi32 PH7_HashmapWalk(ph7_hashmap *pMap,int (*xWalk)(ph7_value *,ph7_value *,void *),void *pUserData);
PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap);
/* php's key fold, shared with the diagnostics that must print a key the way the
 * LOOKUP saw it. Leaves a non-integer key as a printable string value. */
PH7_PRIVATE int PH7_HashmapKeyIsInt(ph7_value *pKey);
/* php value-name helper (true/false/class-name/null); used by the range()/
 * array_rand() domain-error messages in hashmap.c, which are compiled in every
 * mode, so it must stay outside the PH7_DISABLE_DISK_IO guard. */
PH7_PRIVATE const char *VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf);
/* Outcomes of php's STRING-container offset rules (VmStringOffsetResolve). */
#define VM_STROFF_OK      0  /* *piOfft holds php's offset */
#define VM_STROFF_REJECT  1  /* php's TypeError; pMsg carries its message */
#define VM_STROFF_MISS    2  /* lenient context: answer "not set", say nothing */
/* ...and its DIAGNOSTIC LEVEL, of which php has three, not two:
 *   VM_STROFF_LOUD      a real read or write: every warning, and an offset TYPE
 *                       php refuses is the TypeError.
 *   VM_STROFF_COALESCE  a `??` / `??=` fetch: the NOT-SET diagnostics are
 *                       suppressed (no `Uninitialized string offset`, and a
 *                       refused offset TYPE answers "not set"), and so is the
 *                       null/bool/float CAST notice — but the offset SHAPE
 *                       warning still fires and the offset is still read:
 *                       `$s["1x"] ?? "d"` warns `Illegal string offset "1x"`
 *                       and answers `$s[1]`.
 *   VM_STROFF_ISSET     isset()/empty()/unset(): fully quiet, every shape.
 */
#define VM_STROFF_LOUD     0
#define VM_STROFF_COALESCE 1
#define VM_STROFF_ISSET    2
PH7_PRIVATE int VmStringOffsetResolve(ph7_vm *pVm,ph7_value *pIdx,int iLevel,sxi64 *piOfft,SyBlob *pMsg);
PH7_PRIVATE sxi32 VmStringOffsetWrite(ph7_vm *pVm,ph7_value *pStr,sxi64 iRawOfft,ph7_value *pVal);
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
PH7_PRIVATE sxi32 PH7_FormatCheckArgCount(ph7_context *pCtx,const char *zFormat,int nByte,int nValues,int nFixed,int bVararg);
PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg);
PH7_PRIVATE sxi32 PH7_CheckStreamArg(ph7_context *pCtx,ph7_value *pArg,int iArg,const char *zName);
/* $escape = "" disables escape processing: a sentinel outside 0..255 so no byte
 * of a field can ever compare equal to it. */
#define PH7_CSV_NO_ESCAPE 256
/* Cursor of the incremental "is this record still open?" scan (see
 * PH7_CsvScanOpen); fgetcsv() keeps one per record it is assembling. */
typedef struct PH7_CsvScan PH7_CsvScan;
struct PH7_CsvScan {
	int iState;   /* 0 field start, 1 unquoted, 2 inside the enclosure, 3 past it */
	sxu32 nPos;   /* how much of the record has been scanned */
};
PH7_PRIVATE void PH7_CsvScanInit(PH7_CsvScan *pScan);
PH7_PRIVATE int PH7_CsvScanOpen(PH7_CsvScan *pScan,const char *zIn,sxu32 nByte,
	int delim,int encl,int escape);
PH7_PRIVATE sxi32 PH7_ProcessCsv(ph7_value *pArray,const char *zInput,int nByte,
	int delim,int encl,int escape,int *pbOpen);
PH7_PRIVATE sxi32 PH7_CsvCharArg(ph7_context *pCtx,ph7_value *pArg,int iArg,
	const char *zName,int bAllowEmpty,int *pChar);
PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen);
PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection,int iScannerMode);
PH7_PRIVATE int PH7_VmQueryConstant(ph7_vm *pVm,const char *zName,sxu32 nName,ph7_value *pOut);
#endif /* PH7_DISABLE_BUILTIN_FUNC || PH7_DISABLE_DISK_IO */
/* Natural-order compare: unguarded because hashmap.c's SORT_NATURAL path (always
 * compiled) uses it, even in the tiny build. [[tiny-build-disk-io-guard-fragility]] */
PH7_PRIVATE int PH7_StrNatCmp(const char *zA,int nA,const char *zB,int nB,int bFold);
/* oo.c function prototypes */
PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine);
PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags);
PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,
	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags);
PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte);
PH7_PRIVATE ph7_class_attr   * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte);
PH7_PRIVATE ph7_class_attr   * PH7_ClassExtractConstant(ph7_class *pClass,const char *zName,sxu32 nByte);
PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr);
PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth);
PH7_PRIVATE int PH7_MagicMethodMustBePublic(const SyString *pName);
PH7_PRIVATE sxi32 PH7_VmCallMagicMethod(ph7_vm *pVm,ph7_class_instance *pThis,
	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg);
PH7_PRIVATE sxi32 PH7_VmCallMagicMethodLsb(ph7_vm *pVm,ph7_class *pCalled,ph7_class_instance *pThis,
	ph7_class_method *pMethod,ph7_value *pResult,int nArg,ph7_value **apArg);
PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase);
PH7_PRIVATE sxi32 PH7_ClassUseTrait(ph7_gen_state *pGen,ph7_class *pClass,ph7_class *pTrait);
PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase);
PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface);
PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass);
PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc);
PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest);
PH7_PRIVATE void  PH7_ClassInstanceUnref(ph7_class_instance *pThis);
PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth);
PH7_PRIVATE int PH7_UnmangleAttrName(const char *zKey,sxu32 nKey,SyString *pClass,SyString *pName);
PH7_PRIVATE SyHashEntry * PH7_ClassInstanceAttrEntry(ph7_class_instance *pThis,const char *zName,sxu32 nName);
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
PH7_PRIVATE int PH7_VmDimFetchWritable(ph7_class *pClass);
PH7_PRIVATE sxu32 PH7_SplDimElemSlot(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pKey,int bCreate);
PH7_PRIVATE void PH7_VmOverloadedElemNotice(ph7_vm *pVm,ph7_class *pClass,ph7_value *pVal);
PH7_PRIVATE void PH7_VmOverloadedPropNotice(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,ph7_value *pVal);
PH7_PRIVATE ph7_class_instance * PH7_ContextThis(ph7_context *pCtx);
PH7_PRIVATE ph7_class * PH7_ContextCalledClass(ph7_context *pCtx);
PH7_PRIVATE ph7_value * PH7_ContextThisValue(ph7_context *pCtx);
/* vfs.c */
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE int PH7_StreamIsUrlWrapper(const ph7_io_stream *pStream);
PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,
	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew,const char *zCaller);
PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut);
PH7_PRIVATE int PH7_VfsAppendFile(ph7_context *pCtx,const char *zFile,const void *pData,int nLen);
PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle);
PH7_PRIVATE ph7_int64 PH7_StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen);
PH7_PRIVATE const char * PH7_ExtractBaseName(const char *zPath,int nByte,int *pLen);
PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm);
/* vfs_stream.c / vfs_io_driver.c function prototypes (referenced from the
 * registration tables in vfs.c's PH7_RegisterIORoutine) */
#ifndef PH7_DISABLE_DISK_IO
PH7_PRIVATE int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_escapeshellarg(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_exec(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_escapeshellcmd(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_stream_get_line(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_dir(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_passthru(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_proc_nice(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_shell_exec(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_system(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_stream_set_blocking(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_stream_set_timeout(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_stream_set_chunk_size(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_stream_set_read_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_stream_set_write_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_stream_supports_lock(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_stream_is_local(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_stream_copy_to_stream(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_stream_get_transports(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_stream_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg);
PH7_PRIVATE int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg);
#endif /* PH7_DISABLE_DISK_IO */
PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void);
PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource);
PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource);
PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm);
PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm);
PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm);
/* lib.c function prototypes */
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
PH7_PRIVATE sxi32 SyUriDecode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData,int bPlus);
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE sxi32 SyUriEncode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);
PH7_PRIVATE sxi32 SyUriEncodeRaw(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);
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
/* used by hashmap.c's key sorting — must stay visible in the tiny build */
PH7_PRIVATE sxi32 SyStrncmp(const char *zLeft,const char *zRight,sxu32 nLen);
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
