# src/ph7/vm_ops_load.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1037/1146 lines (90.49%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `/*` |
|       - |    8 | ` * Section:` |
|       - |    9 | ` *    Opcode handlers extracted from vm.c's dispatch loop. Each handler runs` |
|       - |   10 | ` *    one opcode arm against the caller's VmExecState: the loop syncs pTos/pc` |
|       - |   11 | ` *    in, calls the handler, reloads them and routes the returned VmOpRc onto` |
|       - |   12 | ` *    its labels (same idiom as VmCallFinish).` |
|       - |   13 | ` * Status:` |
|       - |   14 | ` *    Stable.` |
|       - |   15 | ` */` |
|       - |   16 | `/* Bind the dispatch-routing exits to handler semantics (see vm_dispatch.h),` |
|       - |   17 | ` * and map the macros' sState references onto our state parameter. */` |
|       - |   18 | `#define VM_EXIT_BREAK      { pState->pTos = pTos; pState->pc = pc; return VM_OP_NEXT; }` |
|       - |   19 | `#define VM_EXIT_ABORT      { pState->pTos = pTos; pState->pc = pc; return VM_OP_ABORT; }` |
|       - |   20 | `#define VM_EXIT_EXCEPTION  { pState->pTos = pTos; pState->pc = pc; return VM_OP_EXCEPTION; }` |
|       - |   21 | `#include "vm_dispatch.h"` |
|       - |   22 | `#define sState (*pState)` |
|       - |   23 |  |
|       - |   24 | `/*` |
|       - |   25 | ` * OP_STORE_REF: body moved verbatim from the OP_STORE_REF arm of` |
|       - |   26 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |   27 | ` */` |
|      66 |   28 | `PH7_PRIVATE VmOpRc VmExecOpStoreRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |   29 | `{` |
|      71 |   30 | `	ph7_value *pTos = pState->pTos;` |
|      71 |   31 | `	ph7_value *pStack = pState->pStack;` |
|      71 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|      71 |   33 | `	sxi32 pc = pState->pc;` |
|       - |   34 | `	sxi32 rc;` |
|      33 |   35 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      71 |   36 | `	 SyString sName = { 0 , 0 };` |
|       - |   37 | `	 VmFrame *pFrameLocal;` |
|       - |   38 | `	SyHashEntry *pEntry;` |
|       - |   39 | `	sxu32 nIdx;` |
|       - |   40 | `#ifdef UNTRUST` |
|       - |   41 | `	if( pTos < pStack ){` |
|       - |   42 | `		VM_EXIT_ABORT;` |
|       - |   43 | `	}` |
|       - |   44 | `#endif` |
|      71 |   45 | `	if( pInstr->iP2 == 1 ){` |
|       - |   46 | ``		/* Member reference target: `$o->p =& $x` / `self::$s =& $x`. The`` |
|       - |   47 | `		 * preceding OP_MEMBER (PH7_MEMBER_REF_TARGET) resolved the property slot` |
|       - |   48 | `		 * and stashed it. Stack: [ ... , source, member-result(top) ]. Rebind the` |
|       - |   49 | `		 * property's nIdx to alias the source variable's slot and pin that slot` |
|       - |   50 | `		 * past its owning frame (like a use(&$x) capture) so neither frame` |
|       - |   51 | `		 * teardown nor a later unset recycles it while the property aliases it. */` |
|      16 |   52 | `		ph7_value *pSrc = &pTos[-1];` |
|      16 |   53 | `		sxu32 nSrcIdx = pSrc->nIdx;` |
|      16 |   54 | `		VmClassAttr *pVmAttr = pVm->pRefTargetAttr;` |
|      16 |   55 | `		ph7_class_attr *pStAttr = pVm->pRefTargetStaticAttr;` |
|      16 |   56 | `		if( pSrc->iFlags & MEMOBJ_AUX_STROFFSET ){` |
|       - |   57 | ``			/* `$o->p =& $s[1]`: a string offset is not a slot (its index is the`` |
|       - |   58 | `			 * BASE STRING's), and php refuses the reference outright. Settle the` |
|       - |   59 | `			 * stashed target state exactly as the success path does, then throw. */` |
|       5 |   60 | `			if( pVm->pRefTargetThis ){` |
|       3 |   61 | `				PH7_ClassInstanceUnref(pVm->pRefTargetThis);` |
|       1 |   62 | `			}` |
|       5 |   63 | `			pVm->pRefTargetAttr = 0;` |
|       5 |   64 | `			pVm->pRefTargetStaticAttr = 0;` |
|       5 |   65 | `			pVm->pRefTargetThis = 0;` |
|       5 |   66 | `			VmPopOperand(&pTos,1); /* the member result */` |
|       5 |   67 | `			rc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|       - |   68 | `				sizeof("Cannot create references to/from string offsets")-1);` |
|       5 |   69 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 |   70 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |   71 | `		}` |
|      11 |   72 | `		if( nSrcIdx == SXU32_HIGH ){` |
|       - |   73 | ``			/* php: the RHS of `=&` must be a variable, not a constant expression.`` |
|       - |   74 | `			 * (The compiler already rejects the obvious literal forms.) */` |
|     ! 0 |   75 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - |   76 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      11 |   77 | `		}else if( pVmAttr ){` |
|       9 |   78 | `			sxu32 nOldIdx = pVmAttr->nIdx;` |
|       9 |   79 | `			if( nOldIdx != nSrcIdx ){` |
|       9 |   80 | `				if( (pVmAttr->iState & VM_CLASS_ATTR_REFBOUND) == 0 ){` |
|       - |   81 | `					/* Release this property's own (unshared) slot before repointing.` |
|       - |   82 | `					 * A reference-bound property bypasses typed coercion in php, so` |
|       - |   83 | `					 * drop any typed-slot enforcement entry too. */` |
|       9 |   84 | `					if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       5 |   85 | `						SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&nOldIdx,sizeof(sxu32),0);` |
|       2 |   86 | `					}` |
|       9 |   87 | `					PH7_VmUnsetMemObj(&(*pVm),nOldIdx,TRUE);` |
|       4 |   88 | `				}` |
|       9 |   89 | `				pVmAttr->nIdx = nSrcIdx;` |
|       9 |   90 | `				pVmAttr->iState \|= VM_CLASS_ATTR_REFBOUND;` |
|       9 |   91 | `				pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       9 |   92 | `				VmPinMemObjSlot(&(*pVm),nSrcIdx);` |
|       5 |   93 | `			}` |
|       7 |   94 | `		}else if( pStAttr ){` |
|       3 |   95 | `			if( pStAttr->nIdx != nSrcIdx ){` |
|       3 |   96 | `				pStAttr->nIdx = nSrcIdx;` |
|       3 |   97 | `				VmPinMemObjSlot(&(*pVm),nSrcIdx);` |
|       1 |   98 | `			}` |
|       1 |   99 | `		}` |
|      11 |  100 | `		if( pVm->pRefTargetThis ){` |
|       9 |  101 | `			PH7_ClassInstanceUnref(pVm->pRefTargetThis);` |
|       4 |  102 | `		}` |
|      11 |  103 | `		pVm->pRefTargetAttr = 0;` |
|      11 |  104 | `		pVm->pRefTargetStaticAttr = 0;` |
|      11 |  105 | `		pVm->pRefTargetThis = 0;` |
|       - |  106 | `		/* Pop the member-result; leave the source as the expression value. */` |
|      11 |  107 | `		VmPopOperand(&pTos,1);` |
|      11 |  108 | `		VM_EXIT_BREAK;` |
|       - |  109 | `	}` |
|      57 |  110 | `	if( pInstr->p3 == 0 ){` |
|       - |  111 | `		char *zName;` |
|       - |  112 | `		/* Take the variable name from the Next on the stack */` |
|     ! 0 |  113 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  114 | `			/* Force a string cast */` |
|     ! 0 |  115 | `			PH7_MemObjToString(pTos);` |
|     ! 0 |  116 | `		}` |
|     ! 0 |  117 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|     ! 0 |  118 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|     ! 0 |  119 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     ! 0 |  120 | `			if( zName ){` |
|     ! 0 |  121 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|     ! 0 |  122 | `			}` |
|     ! 0 |  123 | `		}` |
|     ! 0 |  124 | `		PH7_MemObjRelease(pTos);` |
|     ! 0 |  125 | `		pTos--;` |
|     ! 0 |  126 | `	}else{` |
|      57 |  127 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|       - |  128 | `	}` |
|      57 |  129 | `	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){` |
|       - |  130 | `		/* php: a string OFFSET cannot be either end of a reference. The value read` |
|       - |  131 | `		 * out of a string still carries the BASE VARIABLE's slot, so binding it` |
|       - |  132 | `		 * aliased the whole string and a later write through the reference REPLACED` |
|       - |  133 | `		 * it ($s = "abc"; $r = &$s[1]; $r = "Z"; left $s === "Z"). Same Error the` |
|       - |  134 | `		 * by-ref ARGUMENT path raises for f($s[1]). */` |
|       7 |  135 | `		rc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|       - |  136 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|       7 |  137 | `		PH7_MemObjRelease(pTos);` |
|       7 |  138 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       7 |  139 | `		pTos->nIdx = SXU32_HIGH;` |
|       7 |  140 | `		if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 |  141 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  142 | `	}` |
|      51 |  143 | `	nIdx = pTos->nIdx;` |
|      51 |  144 | `	if(nIdx == SXU32_HIGH ){` |
|     ! 0 |  145 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|     ! 0 |  146 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - |  147 | `				"Reference operator require a variable not a constant as it's right operand");` |
|     ! 0 |  148 | `		}else{` |
|       - |  149 | `			ph7_value *pObj;` |
|       - |  150 | `			/* Extract the desired variable and if not available dynamically create it */` |
|     ! 0 |  151 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|     ! 0 |  152 | `			if( pObj == 0 ){` |
|     ! 0 |  153 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  154 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|     ! 0 |  155 | `				VM_EXIT_ABORT;` |
|       - |  156 | `			}` |
|       - |  157 | `			/* Perform the store operation */` |
|     ! 0 |  158 | `			PH7_MemObjStore(pTos,pObj);` |
|     ! 0 |  159 | `			pTos->nIdx = pObj->nIdx;` |
|     ! 0 |  160 | `		}` |
|      51 |  161 | `	}else if( sName.nByte > 0){` |
|      51 |  162 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|       - |  163 | `			/* php 8.1's non-catchable fatal (compile-time there) */` |
|       3 |  164 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       3 |  165 | `			pVm->iExitStatus = 255;` |
|       3 |  166 | `			pVm->bHaltRequested = 1;` |
|       3 |  167 | `			VM_EXIT_ABORT;` |
|     ! 0 |  168 | `		}else{` |
|      48 |  169 | `			pFrameLocal = pVm->pFrame;` |
|      48 |  170 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       - |  171 | `			/* Query the local frame */` |
|      48 |  172 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|      48 |  173 | `			if( pEntry ){` |
|     ! 0 |  174 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|     ! 0 |  175 | `			}else{` |
|      48 |  176 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|      48 |  177 | `				if( pFrameLocal->pParent == 0 ){` |
|       - |  178 | `					/* Insert in the $GLOBALS array */` |
|      39 |  179 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|      18 |  180 | `				}` |
|      48 |  181 | `				if( rc == SXRET_OK ){` |
|      48 |  182 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|      22 |  183 | `				}` |
|       - |  184 | `			}` |
|       - |  185 | `		}` |
|      22 |  186 | `	}` |
|      48 |  187 | `	VM_EXIT_BREAK;` |
|     ! 0 |  188 | `	VM_EXIT_BREAK;` |
|      38 |  189 | `}` |
|       - |  190 |  |
|       - |  191 | `/* LOAD_IDX's own iP2 context codes (they do NOT line up with PH7_MEMBER_*). */` |
|       - |  192 | `#define VM_IDX_CTX_ISSET 4` |
|       - |  193 | `#define VM_IDX_CTX_UNSET 5` |
|       - |  194 | `#define VM_IDX_CTX_EMPTY 6` |
|       - |  195 | `/*` |
|       - |  196 | ` * php rejects an OBJECT or an ARRAY used as an array offset, naming the offending` |
|       - |  197 | ` * type the way get_debug_type() does — the CLASS name for an object, "array" for` |
|       - |  198 | ` * an array — and wording the failure by context:` |
|       - |  199 | ` *` |
|       - |  200 | ` *   read/write   Cannot access offset of type Foo on array` |
|       - |  201 | ` *   isset/empty  Cannot access offset of type Foo in isset or empty` |
|       - |  202 | ` *   unset        Cannot unset offset of type Foo on array` |
|       - |  203 | ` *` |
|       - |  204 | ` * A RESOURCE is deliberately absent: php does not reject it, it warns` |
|       - |  205 | ` * ("Resource ID#N used as offset, casting to integer (N)") and uses the id as an` |
|       - |  206 | ` * integer key. VmOffsetResourceWarn() below handles that half.` |
|       - |  207 | ` *` |
|       - |  208 | ` * Returns TRUE and fills pMsg when the key must be rejected. iCtx is the` |
|       - |  209 | ` * instruction's iP2 (any value other than the isset/unset/empty codes reads as` |
|       - |  210 | ` * an access).` |
|       - |  211 | ` */` |
|  266504 |  212 | `static int VmOffsetTypeRejected(ph7_vm *pVm,ph7_value *pKey,int iCtx,SyBlob *pMsg)` |
|       5 |  213 | `{` |
|       - |  214 | `	const char *zType;` |
|  266509 |  215 | `	SyString *pClass = 0;` |
|  266509 |  216 | `	if( pKey == 0 ){` |
|     ! 0 |  217 | `		return FALSE;` |
|       - |  218 | `	}` |
|  266509 |  219 | `	if( pKey->iFlags & MEMOBJ_OBJ ){` |
|      27 |  220 | `		ph7_class_instance *pInst = (ph7_class_instance *)pKey->x.pOther;` |
|      27 |  221 | `		if( pInst && pInst->pClass ){` |
|      27 |  222 | `			pClass = &pInst->pClass->sName;` |
|      12 |  223 | `		}` |
|      27 |  224 | `		zType = "object";` |
|  266497 |  225 | `	}else if( pKey->iFlags & MEMOBJ_HASHMAP ){` |
|      16 |  226 | `		zType = "array";` |
|       9 |  227 | `	}else{` |
|  266471 |  228 | `		return FALSE;` |
|       - |  229 | `	}` |
|      41 |  230 | `	SyBlobInit(pMsg,&pVm->sAllocator);` |
|      41 |  231 | `	if( iCtx == VM_IDX_CTX_UNSET ){` |
|       3 |  232 | `		SyBlobAppend(pMsg,"Cannot unset offset of type ",sizeof("Cannot unset offset of type ")-1);` |
|       2 |  233 | `	}else{` |
|      39 |  234 | `		SyBlobAppend(pMsg,"Cannot access offset of type ",sizeof("Cannot access offset of type ")-1);` |
|       - |  235 | `	}` |
|      41 |  236 | `	if( pClass ){` |
|      27 |  237 | `		SyBlobAppend(pMsg,pClass->zString,pClass->nByte);` |
|      15 |  238 | `	}else{` |
|      16 |  239 | `		SyBlobAppend(pMsg,zType,(sxu32)SyStrlen(zType));` |
|       - |  240 | `	}` |
|      41 |  241 | `	if( iCtx == VM_IDX_CTX_ISSET \|\| iCtx == VM_IDX_CTX_EMPTY ){` |
|       7 |  242 | `		SyBlobAppend(pMsg," in isset or empty",sizeof(" in isset or empty")-1);` |
|       4 |  243 | `	}else{` |
|      35 |  244 | `		SyBlobAppend(pMsg," on array",sizeof(" on array")-1);` |
|       - |  245 | `	}` |
|      41 |  246 | `	return TRUE;` |
|  133257 |  247 | `}` |
|       - |  248 | `/*` |
|       - |  249 | ` * php's other half of the offset-type rules: a RESOURCE offset is accepted, with` |
|       - |  250 | `` * `Warning: Resource ID#N used as offset, casting to integer (N)`, and the key`` |
|       - |  251 | ` * becomes that integer. Rewrites pKey in place so the normal integer-key path` |
|       - |  252 | ` * takes over.` |
|       - |  253 | ` */` |
|  266466 |  254 | `static void VmOffsetResourceWarn(ph7_vm *pVm,ph7_value *pKey)` |
|       5 |  255 | `{` |
|       - |  256 | `	sxu32 nId;` |
|  266471 |  257 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_RES) == 0 ){` |
|  266461 |  258 | `		return;` |
|       - |  259 | `	}` |
|      11 |  260 | `	nId = PH7_VmResourceId(pVm,pKey->x.pOther);` |
|      16 |  261 | `	VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       5 |  262 | `		"Resource ID#%u used as offset, casting to integer (%u)",nId,nId);` |
|      11 |  263 | `	PH7_MemObjRelease(pKey);` |
|      11 |  264 | `	pKey->x.iVal = (sxi64)nId;` |
|      11 |  265 | `	MemObjSetType(pKey,MEMOBJ_INT);` |
|  133238 |  266 | `}` |
|       - |  267 | `/*` |
|       - |  268 | ` * php DEPRECATES (it does not reject) a NULL array offset, then normalizes it to` |
|       - |  269 | `` * the empty-string key "": `$a[null]`, `$a[$undef]` and `[null => v]` all warn`` |
|       - |  270 | `` * `Using null as an array offset is deprecated, use an empty string instead` and`` |
|       - |  271 | ` * then read/write the "" slot. Emit that E_DEPRECATED notice and return TRUE so` |
|       - |  272 | ` * the caller falls through to the ordinary lookup/insert, which already casts` |
|       - |  273 | ` * NULL->"" (HashmapLookup / HashmapInsert). A LOSSY-FLOAT offset stays a rejected` |
|       - |  274 | ` * TypeError: PHL deliberately targets php's NON-deprecated surface for the` |
|       - |  275 | ` * float->int truncation (VmRejectFloatOperand policy, §2), so only the null case` |
|       - |  276 | ` * is coerced here. Returns FALSE (no notice) for a non-null key.` |
|       - |  277 | ` */` |
|  292962 |  278 | `static int VmNullOffsetDeprecate(ph7_vm *pVm,ph7_value *pKey)` |
|       5 |  279 | `{` |
|  292967 |  280 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_NULL) == 0 ){` |
|  292945 |  281 | `		return FALSE;` |
|       - |  282 | `	}` |
|      24 |  283 | `	PH7_VmThrowError(&(*pVm),0,E_DEPRECATED,` |
|       - |  284 | `		"Using null as an array offset is deprecated, use an empty string instead");` |
|      24 |  285 | `	return TRUE;` |
|  146486 |  286 | `}` |
|       - |  287 | `/*` |
|       - |  288 | ` * The three rules above, applied to a BUILTIN's key argument.` |
|       - |  289 | ` *` |
|       - |  290 | `` * php's array_key_exists() does not run a `string\|int` ZPP row on its $key — it`` |
|       - |  291 | `` * hands the value to the same offset machinery `$a[$key]` uses, so the two agree`` |
|       - |  292 | ` * on every type: an object or an array is the catchable` |
|       - |  293 | `` * `Cannot access offset of type X on array`, a RESOURCE warns and becomes its`` |
|       - |  294 | `` * integer id, `null` deprecates and reads the "" key, and a bool/float/numeric`` |
|       - |  295 | ` * string folds the way any subscript folds. PHL's builtin had its own narrower` |
|       - |  296 | `` * check and therefore its own answers — a `string\|int` TypeError for the two`` |
|       - |  297 | `` * cases php ACCEPTS (null, lossy float) and a silent `false` for the three php`` |
|       - |  298 | ` * REJECTS or coerces (object, array, resource). The object case was the worst of` |
|       - |  299 | ` * them: a __toString() object was stringified and could answer TRUE for a key` |
|       - |  300 | ` * php refuses to look up at all.` |
|       - |  301 | ` *` |
|       - |  302 | ` * bZppWording picks which of php's two messages the caller reports. php words the` |
|       - |  303 | ` * illegal-type rejection differently in the alias than in the function itself —` |
|       - |  304 | `` * `key_exists(): Argument #1 ($key) must be a valid array offset type` vs the`` |
|       - |  305 | ` * engine's offset Error — verified against 8.5.8; the null-key DEPRECATION is the` |
|       - |  306 | ` * array_key_exists() wording in both.` |
|       - |  307 | ` *` |
|       - |  308 | ` * pKey is rewritten in place (resource -> int), so callers pass a private copy,` |
|       - |  309 | ` * not the caller's own argument slot. Returns SXRET_OK when the key is usable, or` |
|       - |  310 | ` * the status of the TypeError thrown.` |
|       - |  311 | ` */` |
|     136 |  312 | `PH7_PRIVATE sxi32 PH7_VmArrayKeyArg(ph7_context *pCtx,ph7_value *pKey,int bZppWording)` |
|       5 |  313 | `{` |
|     141 |  314 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - |  315 | `	SyBlob sMsg;` |
|     141 |  316 | `	if( VmOffsetTypeRejected(pVm,pKey,0,&sMsg) ){` |
|       - |  317 | `		sxi32 rc;` |
|      11 |  318 | `		if( bZppWording ){` |
|       5 |  319 | `			SyBlobRelease(&sMsg);` |
|       7 |  320 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  321 | `				"%s(): Argument #1 ($key) must be a valid array offset type",` |
|       2 |  322 | `				ph7_function_name(pCtx));` |
|       - |  323 | `		}` |
|      10 |  324 | `		rc = PH7_VmThrowException(pCtx,"TypeError","%.*s",` |
|       6 |  325 | `			(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       7 |  326 | `		SyBlobRelease(&sMsg);` |
|       7 |  327 | `		return rc;` |
|       - |  328 | `	}` |
|     131 |  329 | `	VmOffsetResourceWarn(pVm,pKey);` |
|     126 |  330 | `	if( (pKey->iFlags & MEMOBJ_REAL)` |
|      73 |  331 | `	 && pKey->rVal != (ph7_real)(sxi64)pKey->rVal ){` |
|       - |  332 | `		/* php DEPRECATES the lossy float and truncates; PHL rejects it, here as` |
|       - |  333 | ``		 * everywhere else (§10) — and with the SAME message `$a[5.7]` gives, so`` |
|       - |  334 | `		 * the builtin and the subscript stay one rule. */` |
|       8 |  335 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  336 | `			"Cannot access offset of type float on array");` |
|       - |  337 | `	}` |
|     125 |  338 | `	if( pKey->iFlags & MEMOBJ_NULL ){` |
|       - |  339 | `		/* php names the parameter here rather than "an array offset"; the effect` |
|       - |  340 | `		 * is the engine's — the lookup below reads the "" key. */` |
|       3 |  341 | `		PH7_VmThrowError(pVm,0,E_DEPRECATED,` |
|       - |  342 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - |  343 | `			"use an empty string instead");` |
|       1 |  344 | `	}` |
|     125 |  345 | `	return SXRET_OK;` |
|      73 |  346 | `}` |
|       - |  347 | `/*` |
|       - |  348 | ` * Does this string START with an integer php's is_numeric_string would answer` |
|       - |  349 | ` * IS_LONG for? Returns 0 when it does not — no digits at all ("", "-", "p"), a` |
|       - |  350 | ` * FLOAT-shaped prefix ("1.5", ".5", "1.", "1e2", "1E2x") or a run that overflows` |
|       - |  351 | ` * a signed 64-bit int ("9223372036854775808") — 1 when it does and only optional` |
|       - |  352 | ` * trailing WHITESPACE follows (" 12 ", "+12", "007"), and 2 when it does but` |
|       - |  353 | ` * trailing DATA follows ("12abc", "0x1", "1e", "1 x"), which is php's` |
|       - |  354 | `` * `Illegal string offset` warning. *piVal receives the integer for 1 and 2.`` |
|       - |  355 | ` *` |
|       - |  356 | ` * php reaches the same three answers through is_numeric_string_ex(allow_errors=1)` |
|       - |  357 | ` * plus its IS_LONG/IS_DOUBLE verdict: a fraction or a well-formed exponent makes` |
|       - |  358 | ` * the verdict IS_DOUBLE, which a string offset refuses, while a bare 'e' is just` |
|       - |  359 | ` * trailing data.` |
|       - |  360 | ` */` |
|     100 |  361 | `static int VmStringOffsetInt(const char *zIn,sxu32 nByte,sxi64 *piVal)` |
|       4 |  362 | `{` |
|     104 |  363 | `	const char *z = zIn, *zEnd = &zIn[nByte], *zDigit;` |
|     104 |  364 | `	sxu64 uVal = 0, uLimit;` |
|     104 |  365 | `	int isNeg = 0, nDigit, i;` |
|     116 |  366 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|      14 |  367 | `		z++;` |
|       2 |  368 | `	}` |
|     104 |  369 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       7 |  370 | `		isNeg = z[0] == '-';` |
|       7 |  371 | `		z++;` |
|       3 |  372 | `	}` |
|     104 |  373 | `	zDigit = z;` |
|     242 |  374 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|     140 |  375 | `		z++;` |
|       2 |  376 | `	}` |
|     104 |  377 | `	nDigit = (int)(z - zDigit);` |
|     104 |  378 | `	if( nDigit < 1 ){` |
|      42 |  379 | `		return 0;` |
|       - |  380 | `	}` |
|      64 |  381 | `	if( z < zEnd && z[0] == '.' ){` |
|       - |  382 | `		/* "1." and "1.5" alike: php reads a double from here. */` |
|       5 |  383 | `		return 0;` |
|       - |  384 | `	}` |
|      60 |  385 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       3 |  386 | `		const char *zExp = &z[1];` |
|       3 |  387 | `		if( zExp < zEnd && (zExp[0] == '+' \|\| zExp[0] == '-') ){` |
|     ! 0 |  388 | `			zExp++;` |
|     ! 0 |  389 | `		}` |
|       3 |  390 | `		if( zExp < zEnd && (unsigned char)zExp[0] < 0xc0 && SyisDigit(zExp[0]) ){` |
|       3 |  391 | `			return 0;` |
|       - |  392 | `		}` |
|     ! 0 |  393 | `	}` |
|       - |  394 | `	/* Accumulate unsigned so PHP_INT_MIN's magnitude (2^63) is representable —` |
|       - |  395 | `	 * "-9223372036854775808" is a legal offset, "9223372036854775808" is not. */` |
|      62 |  396 | `	while( nDigit > 1 && zDigit[0] == '0' ){` |
|       5 |  397 | `		zDigit++; nDigit--;` |
|       1 |  398 | `	}` |
|      58 |  399 | `	uLimit = isNeg ? (sxu64)SXI64_HIGH + 1 : (sxu64)SXI64_HIGH;` |
|      58 |  400 | `	if( nDigit > 19 ){` |
|     ! 0 |  401 | `		return 0;` |
|       - |  402 | `	}` |
|     184 |  403 | `	for( i = 0 ; i < nDigit ; ++i ){` |
|     130 |  404 | `		sxu64 d = (sxu64)(zDigit[i] - '0');` |
|     130 |  405 | `		if( uVal > (uLimit - d)/10 ){` |
|       3 |  406 | `			return 0;` |
|       - |  407 | `		}` |
|     128 |  408 | `		uVal = uVal*10 + d;` |
|      65 |  409 | `	}` |
|      56 |  410 | `	*piVal = isNeg ? (sxi64)(~uVal + 1) : (sxi64)uVal;` |
|      66 |  411 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|      12 |  412 | `		z++;` |
|       2 |  413 | `	}` |
|      56 |  414 | `	return z == zEnd ? 1 : 2;` |
|      54 |  415 | `}` |
|       - |  416 | `/*` |
|       - |  417 | ` * php's offset rules for a STRING container (zend_check_string_offset, and the` |
|       - |  418 | ` * isset/empty arm of ZEND_ISSET_ISEMPTY_DIM_OBJ). They are NOT the array rules,` |
|       - |  419 | ` * and PHL applied none of them: every offset went through an int cast, so` |
|       - |  420 | `` * `$s[""]`, `$s["-"]` and `$s["p"]` all answered `$s[0]` — a silent wrong answer`` |
|       - |  421 | ` * on code php refuses to run. php's table:` |
|       - |  422 | ` *` |
|       - |  423 | ` *   int                     the offset` |
|       - |  424 | ` *   null / bool / float     Warning: String offset cast occurred, then cast` |
|       - |  425 | ` *   integer-shaped string   the offset (leading/trailing space and '+' allowed)` |
|       - |  426 | ` *   int-then-garbage string Warning: Illegal string offset "12abc", then 12` |
|       - |  427 | ` *   any other string        TypeError: Cannot access offset of type string on string` |
|       - |  428 | ` *   array/object/resource   TypeError, naming the type (an object's CLASS)` |
|       - |  429 | ` *` |
|       - |  430 | ` * isset()/empty() raise NOTHING and answer "not set" for every shape the read` |
|       - |  431 | `` * path would reject OR warn about: `isset($s["0x1"])` is false even though`` |
|       - |  432 | `` * reading it warns and yields `$s[0]`. A `??` fetch sits BETWEEN that and a real`` |
|       - |  433 | ` * read — it suppresses the not-set diagnostics but still warns about the offset` |
|       - |  434 | ` * SHAPE and still reads it. iLevel selects which of the three (VM_STROFF_LOUD /` |
|       - |  435 | ` * _COALESCE / _ISSET).` |
|       - |  436 | ` */` |
|  744081 |  437 | `PH7_PRIVATE int VmStringOffsetResolve(ph7_vm *pVm,ph7_value *pIdx,int iLevel,sxi64 *piOfft,SyBlob *pMsg)` |
|       5 |  438 | `{` |
|  744086 |  439 | `	if( (pIdx->iFlags & MEMOBJ_INT) != 0 && (pIdx->iFlags & MEMOBJ_REAL) == 0 ){` |
|  743926 |  440 | `		*piOfft = pIdx->x.iVal;` |
|  743926 |  441 | `		return VM_STROFF_OK;` |
|       - |  442 | `	}` |
|     164 |  443 | `	if( pIdx->iFlags & MEMOBJ_STRING ){` |
|     154 |  444 | `		int eInt = VmStringOffsetInt((const char *)SyBlobData(&pIdx->sBlob),` |
|      50 |  445 | `			SyBlobLength(&pIdx->sBlob),piOfft);` |
|     104 |  446 | `		if( eInt == 1 ){` |
|      24 |  447 | `			return VM_STROFF_OK;` |
|       - |  448 | `		}` |
|      82 |  449 | `		if( eInt == 2 && iLevel != VM_STROFF_ISSET ){` |
|       - |  450 | ``			/* int-then-garbage. This is the one diagnostic a `??` fetch keeps:`` |
|       - |  451 | ``			 * `$s["1x"] ?? "d"` warns and answers $s[1] (PHL called it "not set"`` |
|       - |  452 | `			 * and answered the default — a wrong VALUE, not just a missing` |
|       - |  453 | `			 * warning). Only isset()/empty() stay silent about it. */` |
|       - |  454 | `			SyString sKey;` |
|      26 |  455 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|      26 |  456 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset \"%z\"",&sKey);` |
|      26 |  457 | `			return VM_STROFF_OK;` |
|       - |  458 | `		}` |
|      58 |  459 | `		if( iLevel != VM_STROFF_LOUD ){` |
|      24 |  460 | `			return VM_STROFF_MISS;` |
|       4 |  461 | `		}` |
|      78 |  462 | `	}else if( (pIdx->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|       - |  463 | `		/* null / bool / float: php casts, but says so in a real read or write. */` |
|      40 |  464 | `		if( iLevel == VM_STROFF_LOUD ){` |
|      26 |  465 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"String offset cast occurred");` |
|      12 |  466 | `		}` |
|      40 |  467 | `		PH7_MemObjToInteger(pIdx);` |
|      40 |  468 | `		*piOfft = pIdx->x.iVal;` |
|      40 |  469 | `		return VM_STROFF_OK;` |
|      24 |  470 | `	}else if( iLevel == VM_STROFF_ISSET ){` |
|       - |  471 | `		/* An array/object/resource offset is "not set" for isset()/empty() — but a` |
|       - |  472 | ``		 * `??` fetch RAISES for it (probed: `$s[[]] ?? "d"` is the TypeError while`` |
|       - |  473 | ``		 * `isset($s[[]])` is false), so only the fully-quiet level answers MISS. */`` |
|      10 |  474 | `		return VM_STROFF_MISS;` |
|       - |  475 | `	}` |
|       - |  476 | `	{` |
|       - |  477 | `		char zBuf[128];` |
|      50 |  478 | `		SyBlobInit(pMsg,&pVm->sAllocator);` |
|      73 |  479 | `		SyBlobFormat(pMsg,"Cannot access offset of type %s on string",` |
|      23 |  480 | `			VmValueGivenName(pIdx,zBuf,sizeof(zBuf)));` |
|       - |  481 | `	}` |
|      50 |  482 | `	return VM_STROFF_REJECT;` |
|  372802 |  483 | `}` |
|       - |  484 | `/*` |
|       - |  485 | ` * OP_STORE_IDX_REF: body moved verbatim from the OP_STORE_IDX_REF arm of` |
|       - |  486 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  487 | ` */` |
|  293194 |  488 | `PH7_PRIVATE VmOpRc VmExecOpStoreIdxRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  489 | `{` |
|  293199 |  490 | `	ph7_value *pTos = pState->pTos;` |
|  293199 |  491 | `	ph7_value *pStack = pState->pStack;` |
|  293199 |  492 | `	VmInstr *aInstr = pState->aInstr;` |
|  293199 |  493 | `	sxi32 pc = pState->pc;` |
|       - |  494 | `	sxi32 rc;` |
|  146597 |  495 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  293199 |  496 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|       - |  497 | `	ph7_value *pKey;` |
|       - |  498 | `	sxu32 nIdx;` |
|  293199 |  499 | `	if( pInstr->iP1 ){` |
|       - |  500 | `		/* Key is next on stack */` |
|   84073 |  501 | `		pKey = pTos;` |
|   84073 |  502 | `		pTos--;` |
|   42039 |  503 | `	}else{` |
|  209131 |  504 | `		pKey = 0;` |
|       - |  505 | `	}` |
|       - |  506 | `		/* php DEPRECATES a null / lossy-float write subscript (then normalizes "" /` |
|       - |  507 | ``		 * truncates); it does not reject either. A null key — by-VALUE (`$a[$k]=v`)`` |
|       - |  508 | ``		 * OR by-REF (`$a[$k]=&$x`) — deprecates + coerces to the "" key: the notice is`` |
|       - |  509 | `		 * emitted DOWN AT THE INSERT (below), not here, so a null/false container that` |
|       - |  510 | ``		 * auto-vivifies (`$x=null; $x[null]=v`) gets it too (the base is not yet a`` |
|       - |  511 | `		 * hashmap at this point), and PH7_HashmapInsert / HashmapInsertByRef both cast` |
|       - |  512 | `		 * NULL->"". A lossy-FLOAT subscript stays a loud TypeError in BOTH forms (the` |
|       - |  513 | `		 * recorded non-deprecated-surface policy, §2). */` |
|  293199 |  514 | `		if( pKey && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|       - |  515 | `			SyBlob sTypeMsg;` |
|       - |  516 | `			/* An object/array key is php's TypeError; a resource key warns and` |
|       - |  517 | `			 * becomes its integer id. Both used to be stringified silently. */` |
|   83823 |  518 | `			if( VmOffsetTypeRejected(&(*pVm),pKey,0,&sTypeMsg) ){` |
|       - |  519 | `				sxi32 rcSc;` |
|       8 |  520 | `				PH7_MemObjRelease(pKey);` |
|       8 |  521 | `				VmPopOperand(&pTos,1);` |
|       8 |  522 | `				rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      12 |  523 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       8 |  524 | `				rc = rcSc;` |
|       8 |  525 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  526 | `			}` |
|   83817 |  527 | `			VmOffsetResourceWarn(&(*pVm),pKey);` |
|   83812 |  528 | `			if( (pKey->iFlags & MEMOBJ_REAL)` |
|   41914 |  529 | `			 && pKey->rVal != (ph7_real)(sxi64)pKey->rVal ){` |
|       - |  530 | `				sxi32 rcSc;` |
|       3 |  531 | `				const char *zErr = "Cannot access offset of type float on array";` |
|       3 |  532 | `				PH7_MemObjRelease(pKey);` |
|       3 |  533 | `				VmPopOperand(&pTos,1);` |
|       3 |  534 | `				rcSc = VmThrowFromVm(&(*pVm),"TypeError",zErr,(sxu32)SyStrlen(zErr));` |
|       3 |  535 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  536 | `				rc = rcSc;` |
|       3 |  537 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  538 | `			}` |
|   41905 |  539 | `		}` |
|  293191 |  540 | `	nIdx = pTos->nIdx;` |
|       - |  541 | `	{` |
|       - |  542 | `		/* ArrayAccess::offsetSet dispatch.` |
|       - |  543 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|       - |  544 | `		 * the backing variable slot at nIdx. */` |
|  293191 |  545 | `		ph7_class_instance *pInst = 0;` |
|  293191 |  546 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      89 |  547 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|  293148 |  548 | `		}else if( nIdx != SXU32_HIGH ){` |
|  293105 |  549 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  293105 |  550 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|     ! 0 |  551 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|     ! 0 |  552 | `			}` |
|  146550 |  553 | `		}` |
|  293191 |  554 | `		if( pInst ){` |
|      89 |  555 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|      89 |  556 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|       - |  557 | `				ph7_class_method *pMeth;` |
|       - |  558 | `				ph7_value sNullKey;` |
|       - |  559 | `				ph7_value *apArg[2];` |
|      87 |  560 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|     ! 0 |  561 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - |  562 | `						"Cannot assign by reference to overloaded object");` |
|     ! 0 |  563 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|     ! 0 |  564 | `					VmPopOperand(&pTos,2); /* container + value */` |
|     ! 0 |  565 | `					VM_EXIT_BREAK;` |
|       - |  566 | `				}` |
|      87 |  567 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - |  568 | `					"offsetSet",sizeof("offsetSet")-1);` |
|       - |  569 | `				/* Pop container; pTos now points to the value */` |
|      87 |  570 | `				VmPopOperand(&pTos,1);` |
|      87 |  571 | `				if( pKey == 0 ){` |
|      10 |  572 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|      10 |  573 | `					apArg[0] = &sNullKey;` |
|       6 |  574 | `				}else{` |
|      79 |  575 | `					apArg[0] = pKey;` |
|       - |  576 | `				}` |
|      87 |  577 | `				apArg[1] = pTos;` |
|      87 |  578 | `				if( pMeth ){` |
|      87 |  579 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|      42 |  580 | `				}` |
|      87 |  581 | `				if( pKey ){` |
|      79 |  582 | `					PH7_MemObjRelease(pKey);` |
|      41 |  583 | `				}else{` |
|      10 |  584 | `					PH7_MemObjRelease(&sNullKey);` |
|       - |  585 | `				}` |
|       - |  586 | `				/* Pop the value */` |
|      87 |  587 | `				VmPopOperand(&pTos,1);` |
|      87 |  588 | `				VM_EXIT_BREAK;` |
|       - |  589 | `			}` |
|       - |  590 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|       - |  591 | `			 * than silently coercing the object into a hashmap (which is` |
|       - |  592 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|       - |  593 | `			 * a few lines below). Match PHP. */` |
|       - |  594 | `			{` |
|       - |  595 | `				char zMsg[256];` |
|       3 |  596 | `				SyString *pName = &pInst->pClass->sName;` |
|       4 |  597 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - |  598 | `					"Cannot use object of type %.*s as array",` |
|       2 |  599 | `					(int)pName->nByte,pName->zString);` |
|       3 |  600 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|       3 |  601 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|       3 |  602 | `				VmPopOperand(&pTos,2); /* container + value */` |
|       3 |  603 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  604 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  605 | `			}` |
|       - |  606 | `		}` |
|       - |  607 | `	}` |
|  293105 |  608 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       - |  609 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|       - |  610 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|       - |  611 | `		 * checking true sharing count, then re-add after separation. */` |
|  292913 |  612 | `		if( nIdx != SXU32_HIGH ){` |
|  292913 |  613 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  439367 |  614 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|  292913 |  615 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  616 | `				/* Only adjust refcount / perform COW if the backing variable` |
|       - |  617 | `				 * is still sharing the same hashmap instance. This mirrors` |
|       - |  618 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|       - |  619 | `				 * refcounts if the backing array was already separated. */` |
|  292913 |  620 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|  292913 |  621 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|  292913 |  622 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|  292913 |  623 | `					pMap->iRef++;  /* Re-add stack ref */` |
|  292913 |  624 | `					pTos->x.pOther = pMap;` |
|  146459 |  625 | `				}else{` |
|       - |  626 | `					/* Backing variable no longer points at pCur: skip COW here` |
|       - |  627 | `					 * and operate on the hashmap currently on the stack. */` |
|     ! 0 |  628 | `					pMap = pCur;` |
|       - |  629 | `				}` |
|  146459 |  630 | `			}else{` |
|     ! 0 |  631 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  632 | `			}` |
|  146459 |  633 | `		}else{` |
|     ! 0 |  634 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  635 | `		}` |
|  292913 |  636 | `		if( pMap->iRef < 2 ){` |
|       - |  637 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|       - |  638 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|       - |  639 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|       - |  640 | `			 * no code checks iRef for COW decisions. */` |
|     ! 0 |  641 | `			pMap->iRef = 2;` |
|     ! 0 |  642 | `		}` |
|  146459 |  643 | `	}else{` |
|       - |  644 | `		ph7_value *pObj;` |
|     197 |  645 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     197 |  646 | `		if( pObj == 0 ){` |
|     ! 0 |  647 | `			if( pKey ){` |
|     ! 0 |  648 | `			  PH7_MemObjRelease(pKey);` |
|     ! 0 |  649 | `			}` |
|     ! 0 |  650 | `			VmPopOperand(&pTos,1);` |
|     ! 0 |  651 | `			VM_EXIT_BREAK;` |
|       - |  652 | `		}` |
|       - |  653 | `		/* Phase#1: Load the array */` |
|     197 |  654 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|     154 |  655 | `			VmPopOperand(&pTos,1);` |
|     154 |  656 | `			if( pKey == 0 ){` |
|       - |  657 | ``				/* `$s[] = 'x'` on a STRING: php raises the catchable Error`` |
|       - |  658 | `				 * "[] operator not supported for strings" and leaves the string` |
|       - |  659 | `				 * untouched. PHL silently APPENDED, so code that meant to build` |
|       - |  660 | `				 * an array from a variable holding a string quietly produced a` |
|       - |  661 | `				 * longer string instead of failing — a wrong answer, not a` |
|       - |  662 | `				 * missing diagnostic. */` |
|       - |  663 | `				SyBlob sErrMsg;` |
|       6 |  664 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       6 |  665 | `				SyBlobAppend(&sErrMsg,"[] operator not supported for strings",` |
|       - |  666 | `					sizeof("[] operator not supported for strings")-1);` |
|       6 |  667 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       6 |  668 | `				VM_EXIT_BREAK;` |
|     ! 0 |  669 | `			}else{` |
|     150 |  670 | `				sxi64 iOfft = 0;` |
|       - |  671 | `				SyBlob sTypeMsg;` |
|       - |  672 | `				/* php's offset rules run BEFORE the RHS is looked at: an offset it` |
|       - |  673 | `				 * refuses is the TypeError alone. The RHS cast below used to happen` |
|       - |  674 | ``				 * first, so every rejected shape — and `$s[] = [1,2]` above — came`` |
|       - |  675 | ``				 * with a spurious `Array to string conversion` in front of it. */`` |
|     150 |  676 | `				if( VmStringOffsetResolve(&(*pVm),pKey,VM_STROFF_LOUD,&iOfft,&sTypeMsg) == VM_STROFF_REJECT ){` |
|       - |  677 | `					sxi32 rcSc;` |
|       7 |  678 | `					PH7_MemObjRelease(pKey);` |
|       7 |  679 | `					rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      16 |  680 | `					if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       7 |  681 | `					rc = rcSc;` |
|       7 |  682 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  683 | `				}` |
|       - |  684 | `				/* Force a string cast on the RHS (user-visible: an array warns` |
|       - |  685 | `				 * "Array to string conversion" before the offset write, §2, and a` |
|       - |  686 | `				 * not-stringable object throws — the target string is untouched) */` |
|       - |  687 | `				{` |
|     144 |  688 | `					sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|     144 |  689 | `					if( rcSv != SXRET_OK ){` |
|       5 |  690 | `						PH7_MemObjRelease(pKey);` |
|       7 |  691 | `						PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|     ! 0 |  692 | `					}` |
|       - |  693 | `				}` |
|     139 |  694 | `				if( VmStringOffsetWrite(&(*pVm),pObj,iOfft,pTos) != SXRET_OK ){` |
|       - |  695 | `					sxi32 rcEm;` |
|       9 |  696 | `					PH7_MemObjRelease(pKey);` |
|       9 |  697 | `					rcEm = VmThrowFromVm(&(*pVm),"Error",` |
|       - |  698 | `						"Cannot assign an empty string to a string offset",` |
|       - |  699 | `						sizeof("Cannot assign an empty string to a string offset")-1);` |
|       9 |  700 | `					if( rcEm == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 |  701 | `					rc = rcEm;` |
|      11 |  702 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  703 | `				}` |
|       - |  704 | `			}` |
|     131 |  705 | `			if( pKey ){` |
|     131 |  706 | `			  PH7_MemObjRelease(pKey);` |
|      64 |  707 | `			}` |
|     131 |  708 | `			VM_EXIT_BREAK;` |
|      45 |  709 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - |  710 | `			/* php: only NULL and FALSE auto-vivify into an array. Writing an index into an` |
|       - |  711 | `			 * int/float/resource/TRUE is a catchable Error -- PH7 quietly REPLACED the value` |
|       - |  712 | `			 * with an array, destroying it ($x = 5; $x[0] = 1; left $x === [1]). */` |
|       - |  713 | `			/* php auto-vivifies false into an array with an 8.1 DEPRECATION; PHL` |
|       - |  714 | `			 * rejects any scalar base, false included (null still auto-vivifies). */` |
|      45 |  715 | `			int bScalar = (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0;` |
|      45 |  716 | `			if( bScalar ){` |
|       - |  717 | `				sxi32 rcSc;` |
|       8 |  718 | `				if( pKey ){` |
|       5 |  719 | `					PH7_MemObjRelease(pKey);` |
|       2 |  720 | `				}` |
|       8 |  721 | `				VmPopOperand(&pTos,1);` |
|       8 |  722 | `				rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot use a scalar value as an array",` |
|       - |  723 | `					sizeof("Cannot use a scalar value as an array")-1);` |
|       8 |  724 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       8 |  725 | `				rc = rcSc;` |
|       8 |  726 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  727 | `			}` |
|       - |  728 | `			/* Force a hashmap cast  */` |
|      39 |  729 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      39 |  730 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  731 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|     ! 0 |  732 | `				VM_EXIT_ABORT;` |
|       - |  733 | `			}` |
|      18 |  734 | `		}` |
|       - |  735 | `		/* COW separate the backing variable before mutation */` |
|      39 |  736 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|       - |  737 | `	}` |
|  292949 |  738 | `	VmPopOperand(&pTos,1);` |
|       - |  739 | `	/* Phase#2: Perform the insertion. A null key deprecates + normalizes to "" for` |
|       - |  740 | `	 * BOTH the by-value and the by-ref store — emitted HERE, after a null/false` |
|       - |  741 | ``	 * container has auto-vivified to an array, so `$x[null]=v` and `$x[null]=&$y` on`` |
|       - |  742 | `	 * an undefined $x get the notice too (the top-of-handler check runs before` |
|       - |  743 | `	 * vivification, when the base is not yet a hashmap). HashmapInsert /` |
|       - |  744 | `	 * HashmapInsertByRef both then cast NULL->"". A null pKey==0 (an append, no key)` |
|       - |  745 | `	 * is not a null OFFSET and is left alone. */` |
|  292949 |  746 | `	VmNullOffsetDeprecate(&(*pVm),pKey);` |
|  292949 |  747 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && (pTos->iFlags & MEMOBJ_AUX_STROFFSET) ){` |
|       - |  748 | ``		/* `$a[] = &$s[1]`: the source is a string OFFSET, which php refuses to`` |
|       - |  749 | `		 * reference — and whose slot index is the BASE STRING's, so binding it` |
|       - |  750 | ``		 * aliased the whole string. Same Error the `=&` / by-ref-argument paths`` |
|       - |  751 | `		 * raise. The key is already popped; drop it and abandon the store. */` |
|       - |  752 | `		sxi32 rcSc;` |
|       5 |  753 | `		if( pKey ){` |
|       3 |  754 | `			PH7_MemObjRelease(pKey);` |
|       1 |  755 | `		}` |
|       5 |  756 | `		rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|       - |  757 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|       5 |  758 | `		if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 |  759 | `		rc = rcSc;` |
|       5 |  760 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  761 | `	}` |
|  292945 |  762 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|      36 |  763 | `		if( pMap == pVm->pGlobal ){` |
|       - |  764 | `			/* php 8.1: $GLOBALS['y'] =& $x binds the global $y to $x's` |
|       - |  765 | `			 * slot; an append has no name to bind (catchable Error). */` |
|       3 |  766 | `			if( pKey == 0 ){` |
|     ! 0 |  767 | `				rc = PH7_VmThrowGlobalsAppendError(&(*pVm));` |
|     ! 0 |  768 | `			}else{` |
|       3 |  769 | `				if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 |  770 | `					PH7_MemObjToString(pKey);` |
|     ! 0 |  771 | `				}` |
|       3 |  772 | `				if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|       - |  773 | `					/* Pathological empty name: keep the legacy diagnostic */` |
|     ! 0 |  774 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|       - |  775 | `						"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 |  776 | `					rc = SXRET_OK;` |
|     ! 0 |  777 | `				}else{` |
|       4 |  778 | `					rc = PH7_VmInstallGlobalVar(&(*pVm),` |
|       2 |  779 | `						(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|       2 |  780 | `						0,pTos->nIdx);` |
|       - |  781 | `				}` |
|       - |  782 | `			}` |
|       2 |  783 | `		}else{` |
|       - |  784 | `			/* Insertion by reference */` |
|      34 |  785 | `			rc = PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|       - |  786 | `		}` |
|      19 |  787 | `	}else{` |
|  292911 |  788 | `		rc = PH7_HashmapInsert(pMap,pKey,pTos);` |
|       - |  789 | `	}` |
|  292945 |  790 | `	if( pKey ){` |
|   83835 |  791 | `		PH7_MemObjRelease(pKey);` |
|   41915 |  792 | `	}` |
|       - |  793 | `	/* An append onto the occupied saturated auto-index threw php's catchable` |
|       - |  794 | `	 * Error (PH7_VmThrowArrayNextIndexError) — dispatch it like any other` |
|       - |  795 | `	 * store-path throw. Plain failures (OOM) keep their existing routes. */` |
|  292947 |  796 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|  292941 |  797 | `	VM_EXIT_BREAK;` |
|     ! 0 |  798 | `	VM_EXIT_BREAK;` |
|  146602 |  799 | `}` |
|       - |  800 |  |
|       - |  801 | `/*` |
|       - |  802 | ` * OP_LOAD_CLOSURE: body moved verbatim from the OP_LOAD_CLOSURE arm of` |
|       - |  803 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  804 | ` */` |
|    3510 |  805 | `PH7_PRIVATE VmOpRc VmExecOpLoadClosure(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  806 | `{` |
|    3515 |  807 | `	ph7_value *pTos = pState->pTos;` |
|    3515 |  808 | `	ph7_value *pStack = pState->pStack;` |
|    3515 |  809 | `	VmInstr *aInstr = pState->aInstr;` |
|    3515 |  810 | `	sxi32 pc = pState->pc;` |
|       - |  811 | `	sxi32 rc;` |
|    1755 |  812 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    3515 |  813 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       - |  814 | `	/* The function whose name the Closure object will wrap: a fresh per-instantiation` |
|       - |  815 | `	 * copy for a real closure (built below), or the shared lambda function itself for a` |
|       - |  816 | `	 * plain anonymous function with no captured environment. */` |
|    3515 |  817 | `	ph7_vm_func *pTarget = pFunc;` |
|       - |  818 | `	/* A no-capture lambda declared inside a class method is not VM_FUNC_CLOSURE` |
|       - |  819 | `	 * (its env is empty), yet php still binds the creation-site class as its scope` |
|       - |  820 | ``	 * so `self::`/private access inside the body works. Detect the enclosing class`` |
|       - |  821 | `	 * here and route such a lambda through the per-instance closure path (a global` |
|       - |  822 | `	 * no-capture lambda peeks NULL and stays a shared function). */` |
|    3515 |  823 | `	ph7_class *pLoadScope = (pFunc->iFlags & VM_FUNC_CLOSURE) ? 0 : PH7_VmPeekDeclaringClass(pVm);` |
|    3515 |  824 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) \|\| pLoadScope ){` |
|       - |  825 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|       - |  826 | `		ph7_vm_func *pClosure;` |
|       - |  827 | `		char *zName;` |
|       - |  828 | `		sxu32 mLen;` |
|       - |  829 | `		sxu32 n;` |
|       - |  830 | `		/* Create a new VM function */` |
|    3499 |  831 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|       - |  832 | `		/* Generate an unique closure name */` |
|    3499 |  833 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|    3499 |  834 | `		if( pClosure == 0 \|\| zName == 0){` |
|     ! 0 |  835 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|     ! 0 |  836 | `			VM_EXIT_ABORT;` |
|       - |  837 | `		}` |
|    3499 |  838 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|    3499 |  839 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|     ! 0 |  840 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|     ! 0 |  841 | `		}` |
|       - |  842 | `		/* Zero the stucture */` |
|    3499 |  843 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|       - |  844 | `		/* Perform a structure assignment on read-only items */` |
|    3499 |  845 | `		pClosure->aArgs = pFunc->aArgs;` |
|    3499 |  846 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|    3499 |  847 | `		pClosure->aStatic = pFunc->aStatic;` |
|    3499 |  848 | `		pClosure->iFlags = pFunc->iFlags;` |
|       - |  849 | `		/* An in-class no-capture lambda routed here (pLoadScope set) must be a` |
|       - |  850 | `		 * real closure so PH7_VmClassMemberAccess honors its pUserData scope. */` |
|    3499 |  851 | `		pClosure->iFlags \|= VM_FUNC_CLOSURE;` |
|    3499 |  852 | `		pClosure->pUserData = pFunc->pUserData;` |
|    3499 |  853 | `		pClosure->sSignature = pFunc->sSignature;` |
|    3499 |  854 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|    3499 |  855 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|    3499 |  856 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|    3499 |  857 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|    3499 |  858 | `		pClosure->bStrictTypes = pFunc->bStrictTypes;` |
|    3499 |  859 | `		pClosure->nMaxStack = pFunc->nMaxStack;` |
|    3499 |  860 | `		if( pClosure->pUserData == 0 ){` |
|       - |  861 | `			/* Stamp the creation-site class scope (see PH7_VmPeekDeclaringClass):` |
|       - |  862 | `			 * a closure made in a method — or in another closure, whose own stamp` |
|       - |  863 | `			 * the peek reads — resolves self::/parent:: against it like php. */` |
|    3499 |  864 | `			pClosure->pUserData = (void *)PH7_VmPeekDeclaringClass(pVm);` |
|    1747 |  865 | `		}` |
|       - |  866 | ``		/* Capture the creation-site late-static-binding class so `static::` inside`` |
|       - |  867 | `		 * the closure body resolves like php (the "called class", which may differ` |
|       - |  868 | `		 * from the declaring scope stamped above — e.g. a closure made in an` |
|       - |  869 | `		 * inherited method). A closure made outside any class captures NULL. */` |
|    3499 |  870 | `		pClosure->pLsbClass = (void *)PH7_VmPeekTopClass(pVm);` |
|       - |  871 | `		/* Reflection descriptor fields (getDocComment/getAttributes/getStartLine/` |
|       - |  872 | `		 * getEndLine/getFileName read the INSTANTIATED copy via $__fn) */` |
|    3499 |  873 | `		pClosure->aAttrs = pFunc->aAttrs;` |
|    3499 |  874 | `		pClosure->sDoc = pFunc->sDoc;` |
|    3499 |  875 | `		pClosure->sFile = pFunc->sFile;` |
|    3499 |  876 | `		pClosure->nLine = pFunc->nLine;` |
|    3499 |  877 | `		pClosure->nEndLine = pFunc->nEndLine;` |
|    3499 |  878 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|       - |  879 | `		/* Register the closure */` |
|    3499 |  880 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|       - |  881 | `		/* Set up closure environment */` |
|    3499 |  882 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|    3499 |  883 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|    7623 |  884 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|       - |  885 | `			ph7_value *pValue;` |
|    4129 |  886 | `			pEnv = &aEnv[n];` |
|    4129 |  887 | `			sEnv.sName  = pEnv->sName;` |
|    4129 |  888 | `			sEnv.iFlags = pEnv->iFlags;` |
|    4129 |  889 | `			sEnv.nLine = pEnv->nLine;` |
|    4129 |  890 | `			sEnv.nIdx = SXU32_HIGH;` |
|    4129 |  891 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|    4124 |  892 | `			if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == VM_FUNC_ARG_BY_REF` |
|    2137 |  893 | `			 && !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|      70 |  894 | `				&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|       - |  895 | `				/* Capture by reference: bind the env entry to the variable's` |
|       - |  896 | `				 * memory slot — creating a fresh null variable when missing,` |
|       - |  897 | ``				 * as php does (`use (&$f)` before $f is assigned) — and pin`` |
|       - |  898 | `				 * the slot past the creating frame's teardown so the closure` |
|       - |  899 | `				 * can outlive its birth scope. The call-time env install` |
|       - |  900 | `				 * aliases the name to this slot instead of copying a value. */` |
|     103 |  901 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,TRUE);` |
|     103 |  902 | `				if( pValue ){` |
|     103 |  903 | `					sEnv.nIdx = pValue->nIdx;` |
|     103 |  904 | `					VmPinMemObjSlot(pVm,pValue->nIdx);` |
|      50 |  905 | `				}` |
|      53 |  906 | `			}else{` |
|       - |  907 | `				/* Standard pass by value */` |
|    4029 |  908 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|    4029 |  909 | `				if( pValue ){` |
|       - |  910 | `					/* Copy imported value */` |
|     603 |  911 | `					PH7_MemObjStore(pValue,&sEnv.sValue);` |
|    3730 |  912 | `				}else if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == 0` |
|    1736 |  913 | `					&& !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|      18 |  914 | `						&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|      38 |  915 | `					if( pFunc->iFlags & VM_FUNC_ARROW ){` |
|       - |  916 | `						/* An arrow function auto-captures free variables by value, but` |
|       - |  917 | `						 * php does NOT capture one that is UNDEFINED at creation: the` |
|       - |  918 | `						 * isolated body scope then simply has no such variable, so a` |
|       - |  919 | `						 * read of it there raises the normal "Undefined variable"` |
|       - |  920 | `						 * warning (and reflection's getClosureUsedVariables omits it).` |
|       - |  921 | `						 * Skip installing the capture so the body READ — not the` |
|       - |  922 | `						 * creation — warns, matching php. (A later assignment to the` |
|       - |  923 | `						 * outer variable does not retro-capture: arrow scope is` |
|       - |  924 | `						 * isolated.) An explicit by-value use() instead warns here and` |
|       - |  925 | `						 * binds NULL, handled just below. */` |
|      28 |  926 | `						continue;` |
|       - |  927 | `					}` |
|       - |  928 | ``					/* php reads a by-value `use ($q)` capture AT CLOSURE CREATION and`` |
|       - |  929 | `					 * warns when the variable is undefined there (the by-ref form` |
|       - |  930 | ``					 * `use (&$q)` above stays silent — it creates the binding). The`` |
|       - |  931 | `					 * auto-injected $this (VM_FUNC_ARG_IGNORE) never warns. The` |
|       - |  932 | `					 * capture still proceeds as NULL, as php does. php attributes the` |
|       - |  933 | `					 * warning to the capture's own line (which can differ from the` |
|       - |  934 | `					 * OP_LOAD_CLOSURE instruction line when the use-clause wraps), so` |
|       - |  935 | `					 * borrow the recorded line for the emission and restore it. */` |
|      11 |  936 | `					sxu32 nSavedLine = pVm->nCurLine;` |
|      11 |  937 | `					if( sEnv.nLine ){` |
|      11 |  938 | `						pVm->nCurLine = sEnv.nLine;` |
|       5 |  939 | `					}` |
|      11 |  940 | `					VmErrorFormat(pVm,PH7_CTX_WARNING,"Undefined variable $%z",&sEnv.sName);` |
|      11 |  941 | `					pVm->nCurLine = nSavedLine;` |
|       5 |  942 | `				}` |
|       - |  943 | `			}` |
|       - |  944 | `			/* Insert the imported variable */` |
|    4105 |  945 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|    2055 |  946 | `		}` |
|    3499 |  947 | `		pTarget = pClosure;` |
|    1747 |  948 | `	}` |
|       - |  949 | `	/* Wrap the target function in a Closure object and push it. Its captured environment` |
|       - |  950 | ``	 * (incl. any `$this`) stays in pTarget->aClosureEnv and is delivered by the normal call`` |
|       - |  951 | `	 * path when the closure is dispatched by name. */` |
|    3515 |  952 | `	pTos++;` |
|       - |  953 | `	{` |
|    3515 |  954 | `		ph7_class_instance *pCloObj = VmCreateClosure(pVm, &pTarget->sName, 0, 0);` |
|    3515 |  955 | `		if( pCloObj ){` |
|    3515 |  956 | `			pCloObj->iRef++;` |
|    3515 |  957 | `			pTos->x.pOther = pCloObj;` |
|    3515 |  958 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|    1760 |  959 | `		}else{` |
|       - |  960 | `			/* OOM fallback: the name string is still a usable callable. */` |
|     ! 0 |  961 | `			PH7_MemObjStringAppend(pTos, pTarget->sName.zString, pTarget->sName.nByte);` |
|       - |  962 | `		}` |
|       - |  963 | `	}` |
|    3515 |  964 | `	VM_EXIT_BREAK;` |
|     ! 0 |  965 | `	VM_EXIT_BREAK;` |
|    1760 |  966 | `}` |
|       - |  967 |  |
|       - |  968 |  |
|       - |  969 | `/*` |
|       - |  970 | `` * TRUE when this LOAD_IDX feeds a `??`: the coalesce test is the very next`` |
|       - |  971 | `` * instruction. php evaluates the whole left operand of `??` in isset-context,`` |
|       - |  972 | ` * so the access must stay SILENT and, when it misses, must yield NULL — an` |
|       - |  973 | ``  * out-of-range string offset that yielded "" instead made `$s[99] ?? $d` `` |
|       - |  974 | ` * evaluate to "" rather than $d, a wrong answer rather than a stray notice.` |
|       - |  975 | ` */` |
|  743871 |  976 | `static int VmIdxFeedsCoalesce(const VmInstr *pInstr)` |
|       5 |  977 | `{` |
|  743876 |  978 | `	return (pInstr+1)->iOp == PH7_OP_NULLC \|\| (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|       5 |  979 | `}` |
|       - |  980 | `/*` |
|       - |  981 | `` * php's string-offset STORE, shared by `$s[i] = v` (OP_STORE_IDX) and`` |
|       - |  982 | `` * `$s[i] ??= v` (OP_NULLC_STORE — `??=` is not an assign-op, so php performs a`` |
|       - |  983 | ` * real offset write there): resolve iRawOfft against the string's CURRENT length` |
|       - |  984 | ` * (a negative offset counts back from the end and, when it still lands before the` |
|       - |  985 | `` * start, warns `Illegal string offset` and writes NOTHING), refuse an EMPTY`` |
|       - |  986 | ` * replacement, warn when more than one byte was handed over, PAD WITH SPACES up` |
|       - |  987 | ` * to the offset, then write the first byte.` |
|       - |  988 | ` *` |
|       - |  989 | ` * pVal must ALREADY be a string: that coercion is user-visible (it warns for an` |
|       - |  990 | ` * array, throws for a not-stringable object) and stays with the callers, which` |
|       - |  991 | ` * are the only places that can route a throw. Answers SXRET_OK when the store` |
|       - |  992 | `` * happened or was skipped, and SXERR_INVALID for php's `Cannot assign an empty`` |
|       - |  993 | `` * string to a string offset` Error — raised by the caller for the same reason.`` |
|       - |  994 | ` */` |
|     176 |  995 | `PH7_PRIVATE sxi32 VmStringOffsetWrite(ph7_vm *pVm,ph7_value *pStr,sxi64 iRawOfft,ph7_value *pVal)` |
|       3 |  996 | `{` |
|     179 |  997 | `	sxi64 nLen = (sxi64)SyBlobLength(&pStr->sBlob);` |
|     179 |  998 | `	sxi64 iOfft = iRawOfft;` |
|       - |  999 | `	const char *zVal;` |
|     179 | 1000 | `	if( iOfft < 0 ){` |
|       - | 1001 | `		/* php 7.1: a negative offset writes back from the end. */` |
|       9 | 1002 | `		iOfft += nLen;` |
|       9 | 1003 | `		if( iOfft < 0 ){` |
|       7 | 1004 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset %qd",iRawOfft);` |
|       7 | 1005 | `			return SXRET_OK;` |
|       - | 1006 | `		}` |
|       1 | 1007 | `	}` |
|     173 | 1008 | `	if( SyBlobLength(&pVal->sBlob) < 1 ){` |
|       - | 1009 | ``		/* php refuses to write NOTHING into an offset — `$s[2] = ""`, and the`` |
|       - | 1010 | ``		 * `= null` / `= false` that stringify to "" — where PHL silently ignored`` |
|       - | 1011 | `		 * the store. */` |
|      13 | 1012 | `		return SXERR_INVALID;` |
|       - | 1013 | `	}` |
|     161 | 1014 | `	zVal = (const char *)SyBlobData(&pVal->sBlob);` |
|     161 | 1015 | `	if( SyBlobLength(&pVal->sBlob) > 1 ){` |
|      10 | 1016 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1017 | `			"Only the first byte will be assigned to the string offset");` |
|       4 | 1018 | `	}` |
|     161 | 1019 | `	if( iOfft >= nLen ){` |
|       - | 1020 | `		/* php PADS WITH SPACES up to the offset. PH7 simply appended the byte, so` |
|       - | 1021 | `		 * "abc" with [6]="Z" became "abcZ" rather than "abc   Z" -- a silently` |
|       - | 1022 | `		 * wrong string. */` |
|       - | 1023 | `		sxi64 nPad;` |
|     209 | 1024 | `		for( nPad = nLen ; nPad < iOfft ; ++nPad ){` |
|     169 | 1025 | `			SyBlobAppend(&pStr->sBlob," ",sizeof(char));` |
|      85 | 1026 | `		}` |
|      41 | 1027 | `		SyBlobAppend(&pStr->sBlob,(const void *)zVal,sizeof(char));` |
|      21 | 1028 | `	}else{` |
|     121 | 1029 | `		char *zData = (char *)SyBlobData(&pStr->sBlob);` |
|     121 | 1030 | `		zData[iOfft] = zVal[0];` |
|       - | 1031 | `	}` |
|     161 | 1032 | `	return SXRET_OK;` |
|      91 | 1033 | `}` |
|       - | 1034 | `/*` |
|       - | 1035 | `` * Does this LOAD_IDX feed a `??=` (rather than a plain `??`)? The compiler emits`` |
|       - | 1036 | ` * the LHS peek, then OP_NULLC_JMP, then the RHS, then OP_NULLC_STORE — so the` |
|       - | 1037 | ` * NULLC_JMP right after is what distinguishes the assigning form, whose store` |
|       - | 1038 | ` * still has to happen when the peek answers null.` |
|       - | 1039 | ` */` |
|  743887 | 1040 | `static int VmIdxFeedsCoalesceAssign(const VmInstr *pInstr)` |
|       5 | 1041 | `{` |
|  743892 | 1042 | `	return (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|       5 | 1043 | `}` |
|       - | 1044 | `/*` |
|       - | 1045 | ` * OP_LOAD_IDX: body moved verbatim from the OP_LOAD_IDX arm of` |
|       - | 1046 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1047 | ` */` |
|  951762 | 1048 | `PH7_PRIVATE VmOpRc VmExecOpLoadIdx(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1049 | `{` |
|  951767 | 1050 | `	ph7_value *pTos = pState->pTos;` |
|  951767 | 1051 | `	ph7_value *pStack = pState->pStack;` |
|  951767 | 1052 | `	VmInstr *aInstr = pState->aInstr;` |
|  951767 | 1053 | `	sxi32 pc = pState->pc;` |
|       - | 1054 | `	sxi32 rc;` |
|  476807 | 1055 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  951767 | 1056 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|  951767 | 1057 | `	ph7_hashmap *pMap = 0;` |
|       - | 1058 | `	ph7_value *pIdx;` |
|       - | 1059 | `	/* D1 commit 2: iP2==9 is the deferred-record mode. It behaves exactly like a plain read` |
|       - | 1060 | `	 * (iP2==0) for base dispatch / the read tail, EXCEPT the dedicated block right after the` |
|       - | 1061 | `	 * index is popped, which — on a lookup MISS with a reachable base — captures the lvalue` |
|       - | 1062 | `	 * path (MEMOBJ_AUX_DEFPATH) instead of warning, and exits. Everything else sees iP2. */` |
|  951767 | 1063 | `	sxi32 iP2 = (pInstr->iP2 == 9) ? 0 : pInstr->iP2;` |
|  951767 | 1064 | `	pIdx = 0;` |
|  951767 | 1065 | `	if( pInstr->iP1 == 0 ){` |
|       3 | 1066 | `		if( !iP2){` |
|       - | 1067 | `			/* No available index,load NULL */` |
|     ! 0 | 1068 | `			if( pTos >= pStack ){` |
|     ! 0 | 1069 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 | 1070 | `			}else{` |
|       - | 1071 | `				/* TICKET 1433-020: Empty stack */` |
|     ! 0 | 1072 | `				pTos++;` |
|     ! 0 | 1073 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     ! 0 | 1074 | `				pTos->nIdx = SXU32_HIGH;` |
|       - | 1075 | `			}` |
|       - | 1076 | `			/* Emit a notice */` |
|     ! 0 | 1077 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|       - | 1078 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|     ! 0 | 1079 | `			VM_EXIT_BREAK;` |
|       - | 1080 | `		}` |
|       2 | 1081 | `	}else{` |
|  951765 | 1082 | `		pIdx = pTos;` |
|  951765 | 1083 | `		pTos--;` |
|       - | 1084 | `	}` |
|  951767 | 1085 | `	if( pInstr->iP2 == 9 && pIdx ){` |
|       - | 1086 | `		/* D1 commit 2 record mode. Decide whether to CAPTURE this subscript as a deferred` |
|       - | 1087 | `		 * lvalue step (the by-ref/by-value decision is not known until OP_CALL), or fall` |
|       - | 1088 | `		 * through to a plain read (iP2 was normalized to 0 above). We defer only when the` |
|       - | 1089 | `		 * base can be reached again at resolve time: an existing descriptor (nested), the` |
|       - | 1090 | `		 * commit-1 undefined-variable marker, or a real container/string/scalar slot` |
|       - | 1091 | `		 * (nIdx != SXU32_HIGH). On a present array key we DO NOT defer — a hit already` |
|       - | 1092 | `		 * yields the aliasable read slot the by-ref binder needs. */` |
|   61304 | 1093 | `		VmDeferredPath *pPath = 0;` |
|   61304 | 1094 | `		int bDefer = 0, eRoot = 0;` |
|   61304 | 1095 | `		if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|       - | 1096 | `			/* Nested: the base already carries a descriptor — extend it in place. */` |
|       3 | 1097 | `			pPath = (VmDeferredPath *)pTos->x.pOther;` |
|       3 | 1098 | `			bDefer = 1;` |
|   61303 | 1099 | `		}else if( pTos->iFlags & MEMOBJ_AUX_DEFERRED ){` |
|       - | 1100 | `			/* Undefined base variable (commit-1 marker): root the descriptor by name. */` |
|       - | 1101 | `			SyString sRootName;` |
|       3 | 1102 | `			SyStringInitFromBuf(&sRootName,(const char *)pTos->x.pOther,` |
|       - | 1103 | `				pTos->x.pOther ? SyStrlen((const char *)pTos->x.pOther) : 0);` |
|       3 | 1104 | `			pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|       3 | 1105 | `			pTos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name; don't free x.pOther */` |
|       3 | 1106 | `			pTos->x.pOther = 0;` |
|       3 | 1107 | `			bDefer = (pPath != 0);` |
|   61301 | 1108 | `		}else if( pTos->nIdx != SXU32_HIGH ){` |
|       - | 1109 | `			/* A real base slot: array/scalar -> eRoot 0 (by-ref may vivify), string -> 2. */` |
|   61294 | 1110 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 1111 | `				/* Probe hit/miss on a COPY of the key: PH7_HashmapLookup casts a NULL key to` |
|       - | 1112 | `				 * "" in place, which would suppress the fall-through read's null-offset` |
|       - | 1113 | `				 * deprecation (hit) and capture the wrong key in the step (miss). */` |
|       - | 1114 | `				ph7_value idxProbe;` |
|   22885 | 1115 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   22885 | 1116 | `				PH7_MemObjInit(&(*pVm),&idxProbe);` |
|   22885 | 1117 | `				PH7_MemObjStore(pIdx,&idxProbe);` |
|   22885 | 1118 | `				if( PH7_HashmapLookup(pMap,&idxProbe,&pNode) == SXRET_OK ){` |
|   22861 | 1119 | `					bDefer = 0; /* present key: fall through and read it as an aliasable slot */` |
|   11433 | 1120 | `				}else{` |
|      26 | 1121 | `					eRoot = 0; bDefer = 1;` |
|       - | 1122 | `				}` |
|   22885 | 1123 | `				PH7_MemObjRelease(&idxProbe);` |
|   49854 | 1124 | `			}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       5 | 1125 | `				bDefer = 0; /* ArrayAccess: not a deferrable lvalue — read normally */` |
|   38412 | 1126 | `			}else if( pTos->iFlags & MEMOBJ_STRING ){` |
|   38410 | 1127 | `				eRoot = 2; bDefer = 1;` |
|   19358 | 1128 | `			}else{` |
|     ! 0 | 1129 | `				eRoot = 0; bDefer = 1; /* NULL/other reachable scalar base */` |
|       - | 1130 | `			}` |
|   61294 | 1131 | `			if( bDefer && pPath == 0 ){` |
|   38434 | 1132 | `				pPath = VmDeferPathNew(&(*pVm),eRoot,pTos->nIdx,0);` |
|   38434 | 1133 | `				if( pPath == 0 ){` |
|     ! 0 | 1134 | `					bDefer = 0;` |
|     ! 0 | 1135 | `				}` |
|   19365 | 1136 | `			}` |
|   30795 | 1137 | `		}` |
|   61304 | 1138 | `		if( bDefer ){` |
|       - | 1139 | `			/* Append this element step (deep-copies pIdx) and leave the carrier on pTos. */` |
|   38438 | 1140 | `			if( VmDeferPathPushElem(pPath,pIdx) == SXRET_OK ){` |
|   38438 | 1141 | `				if( (pTos->iFlags & MEMOBJ_AUX_DEFPATH) == 0 ){` |
|       - | 1142 | `					/* Collapse the base value into the descriptor carrier. */` |
|   38436 | 1143 | `					PH7_MemObjRelease(pTos);` |
|   38436 | 1144 | `					pTos->x.pOther = pPath;` |
|   38436 | 1145 | `					pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|   38436 | 1146 | `					pTos->nIdx = SXU32_HIGH;` |
|   19366 | 1147 | `				}` |
|   38438 | 1148 | `				PH7_MemObjRelease(pIdx);` |
|   38438 | 1149 | `				VM_EXIT_BREAK;` |
|       - | 1150 | `			}` |
|       - | 1151 | `			/* Out of memory appending a step. */` |
|     ! 0 | 1152 | `			if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|       - | 1153 | `				/* Nested base: pPath IS pTos's carrier and stays owned by it — keep it (a` |
|       - | 1154 | `				 * step short) and exit, rather than fall through and misread a NULL-typed` |
|       - | 1155 | `				 * carrier as an array base. Resolves the shorter path; OOM-only degradation. */` |
|     ! 0 | 1156 | `				PH7_MemObjRelease(pIdx);` |
|     ! 0 | 1157 | `				VM_EXIT_BREAK;` |
|       - | 1158 | `			}` |
|       - | 1159 | `			/* A freshly-allocated path we still own: drop it and fall back to a plain read. */` |
|     ! 0 | 1160 | `			VmFreeDeferredPath(pPath);` |
|     ! 0 | 1161 | `		}` |
|   11433 | 1162 | `	}` |
|  913334 | 1163 | `	if( iP2 == 7 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - | 1164 | ``		/* Keyed list destructuring `["k"=>$v] = $src` from a NON-array source: yield NULL`` |
|       - | 1165 | `		 * (never char-index a string), warning once per key — matching PHP, which warns per` |
|       - | 1166 | `		 * key here. A NULL source is silent; unlike the positional OP_LOAD_LIST path, a bool` |
|       - | 1167 | `		 * source DOES warn (PHP warns for bool in keyed destructuring). */` |
|       7 | 1168 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       5 | 1169 | `			VmWarnCannotUseAsArray(&(*pVm),pTos->iFlags);` |
|       2 | 1170 | `		}` |
|       7 | 1171 | `		if( pIdx ){` |
|       - | 1172 | `			/* Release the key (a string literal for keyed destructuring), like the` |
|       - | 1173 | `			 * normal hashmap-read exit below — otherwise its blob is orphaned. */` |
|       7 | 1174 | `			PH7_MemObjRelease(pIdx);` |
|       3 | 1175 | `		}` |
|       7 | 1176 | `		PH7_MemObjRelease(pTos);` |
|       7 | 1177 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       7 | 1178 | `		VM_EXIT_BREAK;` |
|       - | 1179 | `	}` |
|  913328 | 1180 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|       - | 1181 | `		/* String access */` |
|  743902 | 1182 | `		if( iP2 == VM_IDX_CTX_UNSET ){` |
|       - | 1183 | ``			/* php: a string offset cannot be unset AT ALL — `unset($s[0])` is the`` |
|       - | 1184 | ``			 * catchable `Error: Cannot unset string offsets`, whatever the offset is`` |
|       - | 1185 | `			 * and whether or not it is in range. PHL read the character but left the` |
|       - | 1186 | `			 * BASE VARIABLE's slot index on the result, so the trailing unset()` |
|       - | 1187 | ``			 * builtin freed the base itself: `$s = "abc"; unset($s[1]);` left $s`` |
|       - | 1188 | ``			 * UNDEFINED, and `unset($a["k"][0])` deleted the whole element. */`` |
|      11 | 1189 | `			rc = VmThrowFromVm(&(*pVm),"Error","Cannot unset string offsets",` |
|       - | 1190 | `				sizeof("Cannot unset string offsets")-1);` |
|      11 | 1191 | `			if( pIdx ){` |
|      11 | 1192 | `				PH7_MemObjRelease(pIdx);` |
|       5 | 1193 | `			}` |
|      11 | 1194 | `			PH7_MemObjRelease(pTos);` |
|      11 | 1195 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      11 | 1196 | `			pTos->nIdx = SXU32_HIGH;` |
|      11 | 1197 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      11 | 1198 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1199 | `		}` |
|  743892 | 1200 | `		if( pIdx ){` |
|  743892 | 1201 | `			sxi64 iOfft = 0, iRaw;` |
|  743892 | 1202 | `			sxi64 nLen = (sxi64)SyBlobLength(&pTos->sBlob);` |
|       - | 1203 | `			/* LOAD_IDX carries its OWN iP2 codes, which do NOT line up with the` |
|       - | 1204 | ``			 * PH7_MEMBER_* ones: 4 = isset, 5 = unset, 6 = empty, 8 = `??` read,`` |
|       - | 1205 | ``			 * 3 = `??=` peek. All are LOOKUPS, but php splits them into TWO`` |
|       - | 1206 | `			 * levels: isset()/empty()/unset() say nothing at all, while a` |
|       - | 1207 | ``			 * `??`/`??=` fetch still warns about the offset SHAPE and reads it`` |
|       - | 1208 | `			 * (VM_STROFF_COALESCE). The ??= peek is recognised by the NULLC_JMP` |
|       - | 1209 | `			 * that follows it, since its iP2 does not distinguish the base type. */` |
| 1116560 | 1210 | `			int iOfftLevel = (iP2 == 4 \|\| iP2 == 5 \|\| iP2 == 6) ? VM_STROFF_ISSET` |
| 1486188 | 1211 | `				: ((iP2 == 8 \|\| VmIdxFeedsCoalesce(pInstr)) ? VM_STROFF_COALESCE` |
|  742315 | 1212 | `				: VM_STROFF_LOUD);` |
|  743892 | 1213 | `			int bQuiet = iOfftLevel != VM_STROFF_LOUD;` |
|       - | 1214 | `			SyBlob sTypeMsg;` |
|       - | 1215 | `			int eOfft;` |
|  743892 | 1216 | `			VmCoalStrOff *pCoalOff = 0;` |
|  743892 | 1217 | `			if( VmIdxFeedsCoalesceAssign(pInstr) ){` |
|       - | 1218 | ``				/* `$s[k] ??= v`: the OP_NULLC_STORE ahead has to write into the`` |
|       - | 1219 | `				 * string OFFSET, and by then the offset is gone — this op consumes` |
|       - | 1220 | `				 * it. Carry the RAW index to the store on the peek's own result` |
|       - | 1221 | `				 * (MEMOBJ_AUX_COALSTROFF), which nests and cannot leak; the copy` |
|       - | 1222 | `				 * must predate the resolution below, which casts a float/null/bool` |
|       - | 1223 | `				 * in place, because php re-resolves the offset LOUDLY at the store:` |
|       - | 1224 | `				 * the peek is the quiet half of its pair. */` |
|      53 | 1225 | `				pCoalOff = VmCoalStrOffNew(&(*pVm),pIdx);` |
|      26 | 1226 | `			}` |
|  743892 | 1227 | `			eOfft = VmStringOffsetResolve(&(*pVm),pIdx,iOfftLevel,&iOfft,&sTypeMsg);` |
|  743892 | 1228 | `			if( eOfft == VM_STROFF_MISS ){` |
|       - | 1229 | `				/* A lookup over an offset php refuses: not set, in silence. */` |
|      32 | 1230 | `				PH7_MemObjRelease(pIdx);` |
|      32 | 1231 | `				PH7_MemObjRelease(pTos);` |
|      32 | 1232 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      32 | 1233 | `				if( pCoalOff ){` |
|       - | 1234 | ``					/* A `??=` whose offset the READ refuses: php raises at the`` |
|       - | 1235 | `					 * STORE instead, so keep the base slot reachable and hand the` |
|       - | 1236 | `					 * offset to OP_NULLC_STORE. */` |
|       3 | 1237 | `					pTos->x.pOther = (void *)pCoalOff;` |
|       3 | 1238 | `					pTos->iFlags \|= MEMOBJ_AUX_STROFFSET\|MEMOBJ_AUX_COALSTROFF;` |
|       2 | 1239 | `				}else{` |
|      30 | 1240 | `					pTos->nIdx = SXU32_HIGH;` |
|       - | 1241 | `				}` |
|      50 | 1242 | `				VM_EXIT_BREAK;` |
|       - | 1243 | `			}` |
|  743862 | 1244 | `			if( eOfft == VM_STROFF_REJECT ){` |
|       - | 1245 | `				/* php's TypeError for an offset type a string refuses. PHL cast` |
|       - | 1246 | ``				 * every offset to int, so `$s[""]`, `$s["-"]` and `$s["p"]` all`` |
|       - | 1247 | ``				 * answered `$s[0]`. Routed as a mid-expression throw (this opcode`` |
|       - | 1248 | `				 * is not a call boundary), so the rest of the expression is` |
|       - | 1249 | `				 * abandoned the way php abandons it. */` |
|      40 | 1250 | `				VmFreeCoalStrOff(pCoalOff);` |
|      40 | 1251 | `				rc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      40 | 1252 | `				PH7_MemObjRelease(pIdx);` |
|      40 | 1253 | `				PH7_MemObjRelease(pTos);` |
|      40 | 1254 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      40 | 1255 | `				pTos->nIdx = SXU32_HIGH;` |
|      40 | 1256 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      40 | 1257 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1258 | `			}` |
|  743826 | 1259 | `			iRaw = iOfft;` |
|       - | 1260 | `			/* php 7.1: a NEGATIVE offset counts back from the end ($s[-1] is the last` |
|       - | 1261 | `			 * character). The offset used to be cast to UNSIGNED, so -1 became a huge` |
|       - | 1262 | `			 * number, ran past the end and quietly produced NULL. */` |
|  743826 | 1263 | `			if( iOfft < 0 ){` |
|      18 | 1264 | `				iOfft += nLen;` |
|       8 | 1265 | `			}` |
|  743826 | 1266 | `			if( iOfft < 0 \|\| iOfft >= nLen ){` |
|       - | 1267 | `				/* Out of range. In an isset()/empty() lookup php answers FALSE, so load` |
|       - | 1268 | `				 * NULL there; everywhere else it WARNS and yields the empty string (PH7` |
|       - | 1269 | `				 * silently produced NULL in both cases). */` |
|      65 | 1270 | `				PH7_MemObjRelease(pTos);` |
|      65 | 1271 | `				if( bQuiet ){` |
|      56 | 1272 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      29 | 1273 | `				}else{` |
|      10 | 1274 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|      14 | 1275 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Uninitialized string offset %qd",` |
|       4 | 1276 | `						iRaw);` |
|       - | 1277 | `				}` |
|      34 | 1278 | `			}else{` |
|  743764 | 1279 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|  743764 | 1280 | `				int c = zData[iOfft];` |
|  743764 | 1281 | `				PH7_MemObjRelease(pTos);` |
|  743764 | 1282 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|  743764 | 1283 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|       - | 1284 | `			}` |
|  743826 | 1285 | `			if( pCoalOff ){` |
|      49 | 1286 | `				if( pTos->iFlags & MEMOBJ_NULL ){` |
|       - | 1287 | ``					/* Out of range: the `??=` will store, so hand the offset over. */`` |
|      41 | 1288 | `					pTos->x.pOther = (void *)pCoalOff;` |
|      41 | 1289 | `					pTos->iFlags \|= MEMOBJ_AUX_COALSTROFF;` |
|      21 | 1290 | `				}else{` |
|       - | 1291 | ``					/* A real byte: the `??=` short-circuits over the store. */`` |
|       9 | 1292 | `					VmFreeCoalStrOff(pCoalOff);` |
|       - | 1293 | `				}` |
|      24 | 1294 | `			}` |
|       - | 1295 | `			/* The result still carries the BASE VARIABLE's slot index, which is` |
|       - | 1296 | `			 * harmless for a plain read and WRONG for anything that would ALIAS it:` |
|       - | 1297 | `			 * a string offset is not a slot. Mark it so the reference-binding sites` |
|       - | 1298 | `			 * raise php's Error instead of aliasing the whole string --` |
|       - | 1299 | ``			 * `$r = &$s[1]; $r = "Z";` REPLACED $s with "Z". */`` |
|  743826 | 1300 | `			pTos->iFlags \|= MEMOBJ_AUX_STROFFSET;` |
|  372672 | 1301 | `		}else{` |
|       - | 1302 | `			/* No available index,load NULL */` |
|     ! 0 | 1303 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       - | 1304 | `		}` |
|  743826 | 1305 | `		VM_EXIT_BREAK;` |
|       - | 1306 | `	}` |
|  169431 | 1307 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - | 1308 | `		/* Object subscript: ArrayAccess dispatch.` |
|       - | 1309 | `		 * iP2 codes:` |
|       - | 1310 | `		 *   0 = read       → offsetGet` |
|       - | 1311 | `		 *   3 = ??= peek   → offsetExists; offsetGet on hit; arm coalesce` |
|       - | 1312 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|       - | 1313 | `		 *   4 = isset()    → offsetExists` |
|       - | 1314 | `		 *   5 = unset()    → offsetUnset` |
|       - | 1315 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit` |
|       - | 1316 | ``		 *   8 = `??` read  → same probe as 6 (offsetExists, then offsetGet on a`` |
|       - | 1317 | `		 *                    hit) and no diagnostics anywhere in this op: php` |
|       - | 1318 | ``		 *                    evaluates the whole left operand of `??` in`` |
|       - | 1319 | `		 *                    isset-context, and calling offsetGet blindly also` |
|       - | 1320 | `		 *                    surfaced warnings raised INSIDE a userland` |
|       - | 1321 | `		 *                    offsetGet (e.g. ArrayObject's own array read). */` |
|     185 | 1322 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|     185 | 1323 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|     185 | 1324 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|       - | 1325 | `			ph7_class_method *pMeth;` |
|       - | 1326 | `			ph7_value sResult;` |
|       - | 1327 | `			ph7_value *apArg[1];` |
|     181 | 1328 | `			if( (iP2 == 0 \|\| iP2 == 3 \|\| iP2 == 8) && pIdx == 0 ){` |
|       - | 1329 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|     ! 0 | 1330 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|       - | 1331 | `					"Cannot use [] for reading");` |
|     ! 0 | 1332 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 | 1333 | `				pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 1334 | `				VM_EXIT_BREAK;` |
|       - | 1335 | `			}` |
|     181 | 1336 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|     181 | 1337 | `			if( iP2 == 4 \|\| iP2 == 6 \|\| iP2 == 3 \|\| iP2 == 8 ){` |
|       - | 1338 | ``				/* isset, empty, ??= and `??` all start with offsetExists. */`` |
|      81 | 1339 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 1340 | `					"offsetExists",sizeof("offsetExists")-1);` |
|      81 | 1341 | `				apArg[0] = pIdx;` |
|      81 | 1342 | `				if( pMeth ){` |
|      81 | 1343 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      43 | 1344 | `				}` |
|     143 | 1345 | `			}else if( iP2 == 5 ){` |
|      20 | 1346 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 1347 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|      20 | 1348 | `				apArg[0] = pIdx;` |
|      20 | 1349 | `				if( pMeth ){` |
|      20 | 1350 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       8 | 1351 | `				}` |
|      12 | 1352 | `			}else{` |
|      89 | 1353 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 1354 | `					"offsetGet",sizeof("offsetGet")-1);` |
|      89 | 1355 | `				apArg[0] = pIdx;` |
|      89 | 1356 | `				if( pMeth ){` |
|      89 | 1357 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      42 | 1358 | `				}` |
|       - | 1359 | `			}` |
|     181 | 1360 | `			if( iP2 == 4 ){` |
|       - | 1361 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|       - | 1362 | `				 * right truth value AND skips its "Expecting a variable not` |
|       - | 1363 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|      53 | 1364 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      53 | 1365 | `				PH7_MemObjRelease(pTos);` |
|      53 | 1366 | `				pTos->nIdx = SXU32_HIGH;` |
|      53 | 1367 | `				if( bExists ){` |
|      28 | 1368 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      28 | 1369 | `					pTos->x.iVal = 1;` |
|      16 | 1370 | `				}else{` |
|      29 | 1371 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 1372 | `				}` |
|     157 | 1373 | `			}else if( iP2 == 5 ){` |
|       - | 1374 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|       - | 1375 | `				 * vm_builtin_unset is a harmless no-op. */` |
|      20 | 1376 | `				PH7_MemObjRelease(pTos);` |
|      20 | 1377 | `				pTos->nIdx = SXU32_HIGH;` |
|      20 | 1378 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     125 | 1379 | `			}else if( iP2 == 6 \|\| iP2 == 8 ){` |
|       - | 1380 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|       - | 1381 | `				 * without calling offsetGet. If true, call offsetGet and` |
|       - | 1382 | `				 * push the value so PH7_builtin_empty evaluates emptiness.` |
|       - | 1383 | ``				 * `??` (8) needs the identical shape: NULL on a miss so the`` |
|       - | 1384 | `				 * coalesce takes the default, the real value on a hit. */` |
|      23 | 1385 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      23 | 1386 | `				PH7_MemObjRelease(&sResult);` |
|      23 | 1387 | `				PH7_MemObjRelease(pTos);` |
|      23 | 1388 | `				pTos->nIdx = SXU32_HIGH;` |
|      23 | 1389 | `				if( !bExists ){` |
|       9 | 1390 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       6 | 1391 | `				}else{` |
|      17 | 1392 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 1393 | `						"offsetGet",sizeof("offsetGet")-1);` |
|       - | 1394 | `					ph7_value sValue;` |
|      17 | 1395 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|      17 | 1396 | `					apArg[0] = pIdx;` |
|      17 | 1397 | `					if( pGet ){` |
|      17 | 1398 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|       7 | 1399 | `					}` |
|      17 | 1400 | `					PH7_MemObjStore(&sValue,pTos);` |
|      17 | 1401 | `					PH7_MemObjRelease(&sValue);` |
|       - | 1402 | `				}` |
|      23 | 1403 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      23 | 1404 | `				VM_EXIT_BREAK; /* skip the duplicate sResult release below */` |
|      97 | 1405 | `			}else if( iP2 == 3 ){` |
|       - | 1406 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|       - | 1407 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|       - | 1408 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|       - | 1409 | `				 *     and push NULL.` |
|       - | 1410 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|      10 | 1411 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      10 | 1412 | `				int bShouldArm = !bExists;` |
|       - | 1413 | `				ph7_value sValue;` |
|      10 | 1414 | `				PH7_MemObjRelease(&sResult);` |
|       - | 1415 | `				/* Reset any prior arming defensively */` |
|      10 | 1416 | `				VmCoalesceDisarm(pVm);` |
|      10 | 1417 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|      10 | 1418 | `				if( bExists ){` |
|       5 | 1419 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 1420 | `						"offsetGet",sizeof("offsetGet")-1);` |
|       5 | 1421 | `					apArg[0] = pIdx;` |
|       5 | 1422 | `					if( pGet ){` |
|       5 | 1423 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|       2 | 1424 | `					}` |
|       5 | 1425 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|       3 | 1426 | `						bShouldArm = 1;` |
|       1 | 1427 | `					}` |
|       2 | 1428 | `				}` |
|      10 | 1429 | `				PH7_MemObjRelease(pTos);` |
|      10 | 1430 | `				pTos->nIdx = SXU32_HIGH;` |
|      10 | 1431 | `				if( bShouldArm ){` |
|       - | 1432 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|       - | 1433 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|       - | 1434 | `					 * intervening expression evaluation. */` |
|       8 | 1435 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       8 | 1436 | `					if( pIdx ){` |
|       8 | 1437 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|       3 | 1438 | `					}` |
|       8 | 1439 | `					pVm->pCoalesceObj = pInst;` |
|       8 | 1440 | `					pInst->iRef++;` |
|       8 | 1441 | `					pVm->bCoalesceArmed = 1;` |
|       5 | 1442 | `				}else{` |
|       3 | 1443 | `					PH7_MemObjStore(&sValue,pTos);` |
|       - | 1444 | `				}` |
|      10 | 1445 | `				PH7_MemObjRelease(&sValue);` |
|      10 | 1446 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      10 | 1447 | `				VM_EXIT_BREAK;` |
|     ! 0 | 1448 | `			}else{` |
|       - | 1449 | `				/* offsetGet: replace pTos with the returned value. */` |
|      89 | 1450 | `				PH7_MemObjRelease(pTos);` |
|      89 | 1451 | `				PH7_MemObjStore(&sResult,pTos);` |
|      89 | 1452 | `				pTos->nIdx = SXU32_HIGH;` |
|       - | 1453 | `			}` |
|     153 | 1454 | `			PH7_MemObjRelease(&sResult);` |
|     153 | 1455 | `			if( pIdx ){` |
|     153 | 1456 | `				PH7_MemObjRelease(pIdx);` |
|      74 | 1457 | `			}` |
|     153 | 1458 | `			VM_EXIT_BREAK;` |
|       - | 1459 | `		}` |
|       - | 1460 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|       - | 1461 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|       6 | 1462 | `		if( pInst ){` |
|       - | 1463 | `			char zMsg[256];` |
|       6 | 1464 | `			SyString *pName = &pInst->pClass->sName;` |
|       8 | 1465 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 1466 | `				"Cannot use object of type %.*s as array",` |
|       4 | 1467 | `				(int)pName->nByte,pName->zString);` |
|       6 | 1468 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|       6 | 1469 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|       6 | 1470 | `			PH7_MemObjRelease(pTos);` |
|       6 | 1471 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       6 | 1472 | `			pTos->nIdx = SXU32_HIGH;` |
|       6 | 1473 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       - | 1474 | ``			/* `break` used to resume at the NEXT instruction: the catch ran and then`` |
|       - | 1475 | `			 * execution carried on inside the try block. */` |
|       8 | 1476 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1477 | `		}` |
|     ! 0 | 1478 | `	}` |
|  169251 | 1479 | `	if( (iP2 == 1 \|\| iP2 == 3 \|\| iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      45 | 1480 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|       - | 1481 | `			ph7_value *pObj;` |
|      41 | 1482 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       - | 1483 | `				/* php 8 write-context auto-vivify rules: NULL converts to array` |
|       - | 1484 | `				 * silently; FALSE converts with the 8.1 deprecation; any other` |
|       - | 1485 | `				 * scalar base — int/float/true/resource — is php's catchable` |
|       - | 1486 | `				 * "Cannot use a scalar value as an array" Error and the variable` |
|       - | 1487 | `				 * stays untouched (pre-fix the base was silently CONVERTED,` |
|       - | 1488 | ``				 * corrupting e.g. `$i = 5; $i[0]++` into array(0 => 6); string`` |
|       - | 1489 | `				 * bases were intercepted by the string-offset paths above). */` |
|       - | 1490 | `				/* php auto-converts false to an array with an 8.1 DEPRECATION; PHL` |
|       - | 1491 | `				 * rejects it like any other scalar base (null still auto-vivifies —` |
|       - | 1492 | `				 * it is not a bool). */` |
|      41 | 1493 | `				if( (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0 ){` |
|       - | 1494 | `					/* unset() has its own wording for the same base: php's` |
|       - | 1495 | `					 * "Cannot unset offset in a non-array variable". */` |
|      16 | 1496 | `					const char *zErr = (iP2 == VM_IDX_CTX_UNSET)` |
|       - | 1497 | `						? "Cannot unset offset in a non-array variable"` |
|       7 | 1498 | `						: "Cannot use a scalar value as an array";` |
|       - | 1499 | `					SyBlob sErrMsg;` |
|      16 | 1500 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      16 | 1501 | `					SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|      16 | 1502 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      16 | 1503 | `					if( pIdx ){` |
|      16 | 1504 | `						PH7_MemObjRelease(pIdx);` |
|       7 | 1505 | `					}` |
|      16 | 1506 | `					PH7_MemObjRelease(pTos);` |
|      16 | 1507 | `					pTos->nIdx = SXU32_HIGH;` |
|      16 | 1508 | `					VM_EXIT_BREAK;` |
|       - | 1509 | `				}` |
|       - | 1510 | `				/* unset() CREATES NOTHING: a null/undefined base stays null where PHL` |
|       - | 1511 | ``				 * converted it to an empty array (`$n = null; unset($n[0]);` left`` |
|       - | 1512 | `				 * $n === []), which is also what materialised the missing intermediate` |
|       - | 1513 | ``				 * in `unset($a["y"]["z"])`. Leaving the base alone, the lookup below`` |
|       - | 1514 | `				 * misses, the tail loads NULL with no slot index, and the trailing` |
|       - | 1515 | `				 * unset() builtin is the no-op php's is. */` |
|      27 | 1516 | `				if( iP2 != VM_IDX_CTX_UNSET ){` |
|      25 | 1517 | `					PH7_MemObjToHashmap(pObj);` |
|      25 | 1518 | `					PH7_MemObjLoad(pObj,pTos);` |
|      11 | 1519 | `				}` |
|      12 | 1520 | `			}` |
|      12 | 1521 | `		}` |
|      14 | 1522 | `	}` |
|  169237 | 1523 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|       - | 1524 | `	/* php DEPRECATES both a null offset and a lossy-float subscript (then normalizes` |
|       - | 1525 | `	 * "" / truncates). PHL now matches php on the NULL offset (deprecate + coerce to` |
|       - | 1526 | `	 * the "" key, §2) but still rejects the lossy-FLOAT subscript with a TypeError on` |
|       - | 1527 | `	 * a READ or WRITE (iP2 0/1) — the recorded non-deprecated-surface policy. Both` |
|       - | 1528 | ``	 * stay lenient in isset()/empty()/`??`/unset(), where a throw would be wrong. */`` |
|       - | 1529 | `	/* An object/array key is rejected in EVERY context, including isset()/empty()/` |
|       - | 1530 | `	 * unset() where php still throws (only the wording changes) — unlike the` |
|       - | 1531 | `	 * null/float deprecations below, which stay lenient there. A resource key is` |
|       - | 1532 | `	 * accepted with a warning and becomes its integer id. */` |
|  169237 | 1533 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|       - | 1534 | `		SyBlob sTypeMsg;` |
|  169175 | 1535 | `		if( VmOffsetTypeRejected(&(*pVm),pIdx,iP2,&sTypeMsg) ){` |
|       - | 1536 | `			/* Routed as a mid-expression throw, like the STRING arm above: this` |
|       - | 1537 | `			 * opcode is not a call boundary, so PARKING it let the rest of the` |
|       - | 1538 | ``			 * expression run first — `str_repeat($a[$obj], 2)` reached the builtin`` |
|       - | 1539 | `			 * with the NULL the abandoned read left and died on its ZPP TypeError` |
|       - | 1540 | `			 * instead, and a CAUGHT one still ran the enclosing call afterwards. */` |
|      22 | 1541 | `			rc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      22 | 1542 | `			PH7_MemObjRelease(pIdx);` |
|      22 | 1543 | `			PH7_MemObjRelease(pTos);` |
|      22 | 1544 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      22 | 1545 | `			pTos->nIdx = SXU32_HIGH;` |
|      31 | 1546 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      20 | 1547 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1548 | `		}` |
|  169155 | 1549 | `		VmOffsetResourceWarn(&(*pVm),pIdx);` |
|   84575 | 1550 | `	}` |
|  169217 | 1551 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|       - | 1552 | `		/* php DEPRECATES a null offset in EVERY subscript context except unset()` |
|       - | 1553 | `		 * (iP2==5), then normalizes it to the "" key — read, write, isset (4),` |
|       - | 1554 | ``		 * empty (6), `??`/`??=` (3), the coalesce-chain probe (8), destructure`` |
|       - | 1555 | `		 * (2/7). Emit it here so all of them get it, not just read/write; the` |
|       - | 1556 | `		 * lookup/insert below casts NULL->"", and a plain read miss then warns` |
|       - | 1557 | ``		 * `Undefined array key ""` on the now-string pIdx (php's exact pair). */`` |
|  169155 | 1558 | `		if( (pIdx->iFlags & MEMOBJ_NULL) && iP2 != 5 ){` |
|      17 | 1559 | `			VmNullOffsetDeprecate(&(*pVm),pIdx);` |
|       8 | 1560 | `		}` |
|       - | 1561 | `		/* A lossy-FLOAT subscript stays a rejected TypeError on a READ or WRITE` |
|       - | 1562 | `		 * (iP2 0/1) — the recorded non-deprecated-surface policy (§2); the lenient` |
|       - | 1563 | `		 * contexts (isset/empty/??/unset) truncate quietly as php's value does. */` |
|  169150 | 1564 | `		if( (iP2 == 0 \|\| iP2 == 1)` |
|  103799 | 1565 | `		 && (pIdx->iFlags & MEMOBJ_REAL)` |
|   84585 | 1566 | `		 && pIdx->rVal != (ph7_real)(sxi64)pIdx->rVal ){` |
|       - | 1567 | `			SyBlob sErrMsg;` |
|       6 | 1568 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       6 | 1569 | `			SyBlobAppend(&sErrMsg,"Cannot access offset of type float on array",` |
|       - | 1570 | `				sizeof("Cannot access offset of type float on array")-1);` |
|       6 | 1571 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|       6 | 1572 | `			PH7_MemObjRelease(pIdx);` |
|       6 | 1573 | `			PH7_MemObjRelease(pTos);` |
|       6 | 1574 | `			pTos->nIdx = SXU32_HIGH;` |
|       6 | 1575 | `			VM_EXIT_BREAK;` |
|       - | 1576 | `		}` |
|   84573 | 1577 | `	}` |
|  169213 | 1578 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|  169153 | 1579 | `		if( iP2 == 1 \|\| iP2 == 5 ){` |
|       - | 1580 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|       - | 1581 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|       - | 1582 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|       - | 1583 | `			 * NOT separate — that would defeat COW on every element read.` |
|       - | 1584 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|       - | 1585 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|    1497 | 1586 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|     746 | 1587 | `		}` |
|       - | 1588 | `		/* Point to the hashmap */` |
|  169153 | 1589 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|  169153 | 1590 | `		if( pIdx ){` |
|       - | 1591 | `			/* Load the desired entry */` |
|  169151 | 1592 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|   84573 | 1593 | `		}` |
|  169153 | 1594 | `		if( iP2 == 3 ){` |
|       - | 1595 | `			/* Null coalescing assign peek mode: separate only when we will` |
|       - | 1596 | `			 * actually write back. If the looked-up value is non-null, the` |
|       - | 1597 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|       - | 1598 | `			 * the parent can stay shared. If the value is null or the key is` |
|       - | 1599 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|       - | 1600 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|       - | 1601 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|       - | 1602 | `			 * correct for the outermost write. */` |
|      25 | 1603 | `			int needWrite = (rc != SXRET_OK);` |
|      25 | 1604 | `			if( !needWrite && pNode ){` |
|      13 | 1605 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|      13 | 1606 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|       7 | 1607 | `					needWrite = 1;` |
|       3 | 1608 | `				}` |
|       6 | 1609 | `			}` |
|      25 | 1610 | `			if( needWrite ){` |
|      19 | 1611 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      19 | 1612 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|       - | 1613 | `					/* The map was actually copied — re-lookup so pNode points` |
|       - | 1614 | `					 * into the new map's storage. */` |
|       7 | 1615 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       7 | 1616 | `					if( pIdx ){` |
|       7 | 1617 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|       3 | 1618 | `					}` |
|       3 | 1619 | `				}` |
|       9 | 1620 | `			}` |
|      12 | 1621 | `		}` |
|       - | 1622 | `		/* iP2 == 5 (unset) is deliberately NOT here: php's unset() never creates` |
|       - | 1623 | `		 * the key it is about to remove, so a MISS must stay a miss. The` |
|       - | 1624 | `		 * insert-then-unset round trip was invisible on the LAST step but left` |
|       - | 1625 | ``		 * every INTERMEDIATE behind -- `unset($a["y"]["z"])` grew an empty`` |
|       - | 1626 | `		 * $a["y"]. The COW separation the unset context needs happened above and` |
|       - | 1627 | `		 * does not depend on this insert. */` |
|  169153 | 1628 | `		if( rc != SXRET_OK && (iP2 == 1 \|\| iP2 == 3) ){` |
|       - | 1629 | `			/* Create a new empty entry */` |
|      90 | 1630 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      90 | 1631 | `			if( rc == SXRET_OK ){` |
|       - | 1632 | `				/* Point to the last inserted entry */` |
|      88 | 1633 | `				pNode = pMap->pLast;` |
|      46 | 1634 | `			}else{` |
|       - | 1635 | ``				/* An append lvalue (`$a[][...] = v`) whose saturated auto-index`` |
|       - | 1636 | `				 * is occupied threw php's catchable Error. Dispatch it here —` |
|       - | 1637 | `				 * falling through with a stale pMap->pLast is what silently` |
|       - | 1638 | `				 * overwrote $a[PHP_INT_MAX]. */` |
|       7 | 1639 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|       - | 1640 | `			}` |
|      42 | 1641 | `		}` |
|   84573 | 1642 | `	}` |
|  169206 | 1643 | `	if( rc != SXRET_OK && pIdx && (iP2 == 2 \|\| iP2 == 0)` |
|   48171 | 1644 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP)` |
|      35 | 1645 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|       - | 1646 | ``		/* `$a['k'] ?? $d` compiles its LHS as a plain read (iP2 == 0) followed`` |
|       - | 1647 | `		 * by NULLC/NULLC_JMP — php does NOT warn there, so peek ahead and stay` |
|       - | 1648 | `		 * silent (same guard the magic-accessor read path uses). */` |
|       - | 1649 | `		/* php warns when a missing key is READ (iP2 == 0) or destructured` |
|       - | 1650 | `		 * (iP2 == 2). isset/empty/??/unset (iP2 3-6) and write-context` |
|       - | 1651 | `		 * vivification (iP2 == 1) stay silent, as does a read on a non-array` |
|       - | 1652 | `		 * base (already diagnosed above). php prints an INT key bare and a` |
|       - | 1653 | `		 * STRING key quoted, and it decides which one the key IS by the same fold` |
|       - | 1654 | ``		 * the lookup just used: `$a["10"]` looked for the INTEGER key 10, so php`` |
|       - | 1655 | ``		 * says `Undefined array key 10`. PHL rendered the operand as WRITTEN --`` |
|       - | 1656 | ``		 * quoting every string and, worse, printing `$a[false]` as the "" key it`` |
|       - | 1657 | `		 * never looked in (false is the integer key 0). The canonical-numeric rule` |
|       - | 1658 | `		 * is the hashmap's own, so ask it rather than re-derive it. */` |
|       - | 1659 | `		SyBlob sMsg;` |
|      65 | 1660 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      65 | 1661 | `		if( PH7_HashmapKeyIsInt(pIdx) ){` |
|      24 | 1662 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      15 | 1663 | `				PH7_MemObjToInteger(pIdx);` |
|       7 | 1664 | `			}` |
|      24 | 1665 | `			SyBlobFormat(&sMsg,"Undefined array key %qd",pIdx->x.iVal);` |
|      13 | 1666 | `		}else{` |
|       - | 1667 | `			/* Not an integer key: PH7_HashmapKeyIsInt left it a printable string. */` |
|       - | 1668 | `			SyString sKey;` |
|      43 | 1669 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|      43 | 1670 | `			SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);` |
|       - | 1671 | `		}` |
|      65 | 1672 | `		SyBlobNullAppend(&sMsg);` |
|      65 | 1673 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|      65 | 1674 | `		SyBlobRelease(&sMsg);` |
|      30 | 1675 | `	}` |
|  169168 | 1676 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0` |
|   84595 | 1677 | `	 && (iP2 == 0 \|\| iP2 == 2)` |
|      19 | 1678 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|       - | 1679 | `		/* Subscripting a scalar base is a WARNING in php ("Trying to access array offset` |
|       - | 1680 | `		 * on int") that yields NULL. PH7 yielded NULL in silence. */` |
|      11 | 1681 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Trying to access array offset on %s",` |
|       3 | 1682 | `			VmArithTypeName(pTos));` |
|       3 | 1683 | `	}` |
|  169173 | 1684 | `	if( pIdx ){` |
|  169173 | 1685 | `		PH7_MemObjRelease(pIdx);` |
|   84584 | 1686 | `	}` |
|  169173 | 1687 | `	if( rc == SXRET_OK ){` |
|       - | 1688 | `		/* Load entry contents */` |
|   72897 | 1689 | `		if( pMap->iRef < 2 ){` |
|       - | 1690 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|       - | 1691 | `			 * of the entry value,rather than pointing to it.` |
|       - | 1692 | `			 */` |
|     227 | 1693 | `			pTos->nIdx = SXU32_HIGH;` |
|     227 | 1694 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|     116 | 1695 | `		}else{` |
|   72675 | 1696 | `			pTos->nIdx = pNode->nValIdx;` |
|   72675 | 1697 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|   72675 | 1698 | `			PH7_HashmapUnref(pMap);` |
|       - | 1699 | `		}` |
|   36451 | 1700 | `	}else{` |
|       - | 1701 | `		/* No such entry,load NULL */` |
|   96281 | 1702 | `		PH7_MemObjRelease(pTos);` |
|   96281 | 1703 | `		pTos->nIdx = SXU32_HIGH;` |
|       - | 1704 | `	}` |
|  169173 | 1705 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1706 | `	VM_EXIT_BREAK;` |
|  476774 | 1707 | `}` |
|       - | 1708 |  |
|       - | 1709 | `/*` |
|       - | 1710 | ` * OP_LOAD_MAP: body moved verbatim from the OP_LOAD_MAP arm of` |
|       - | 1711 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1712 | ` */` |
|   83686 | 1713 | `PH7_PRIVATE VmOpRc VmExecOpLoadMap(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1714 | `{` |
|   83691 | 1715 | `	ph7_value *pTos = pState->pTos;` |
|   83691 | 1716 | `	ph7_value *pStack = pState->pStack;` |
|   83691 | 1717 | `	VmInstr *aInstr = pState->aInstr;` |
|   83691 | 1718 | `	sxi32 pc = pState->pc;` |
|       - | 1719 | `	sxi32 rc;` |
|   41843 | 1720 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 1721 | `	ph7_hashmap *pMap;` |
|       - | 1722 | `	/* Allocate a new hashmap instance */` |
|   83691 | 1723 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|   83691 | 1724 | `	if( pMap == 0 ){` |
|     ! 0 | 1725 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     ! 0 | 1726 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|     ! 0 | 1727 | `		VM_EXIT_ABORT;` |
|       - | 1728 | `	}` |
|   83691 | 1729 | `	if( pInstr->iP1 > 0 ){` |
|   14093 | 1730 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|   14093 | 1731 | `		sxi32 rcSpread = SXRET_OK;` |
|       - | 1732 | `		/* Perform the insertion */` |
|   51215 | 1733 | `		while( pEntry < pTos ){` |
|   37145 | 1734 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|       - | 1735 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|       - | 1736 | `				 * semantics — string keys preserved (later wins), int keys` |
|       - | 1737 | `				 * renumbered. Same routine that backs array_merge. */` |
|     683 | 1738 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|     659 | 1739 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|     659 | 1740 | `					if( rcMerge != SXRET_OK ){` |
|       - | 1741 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|       - | 1742 | `						 * path — emit fatal and abort, leaving no partial` |
|       - | 1743 | `						 * map dangling. */` |
|     ! 0 | 1744 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     ! 0 | 1745 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|     ! 0 | 1746 | `						rcSpread = PH7_ABORT;` |
|     ! 0 | 1747 | `						break;` |
|       1 | 1748 | `					}` |
|     354 | 1749 | `				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){` |
|       - | 1750 | `					/* Traversable unpacking (PHP 8.1): walk it into the map using the` |
|       - | 1751 | `					 * same key rules as array spread (string keys kept, int renumbered). */` |
|       5 | 1752 | `					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);` |
|       5 | 1753 | `					if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|     ! 0 | 1754 | `						rcSpread = rcW;` |
|     ! 0 | 1755 | `						break;` |
|       - | 1756 | `					}` |
|       3 | 1757 | `				}else{` |
|       - | 1758 | `					/* Throw a catchable Error matching PHP semantics. */` |
|      21 | 1759 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|      21 | 1760 | `					break;` |
|       1 | 1761 | `				}` |
|   36796 | 1762 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|       - | 1763 | `				/* Insertion by reference */` |
|     181 | 1764 | `				PH7_HashmapInsertByRef(pMap,` |
|     120 | 1765 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     120 | 1766 | `					(sxu32)pEntry[1].x.iVal` |
|       - | 1767 | `					);` |
|      61 | 1768 | `			}else{` |
|       - | 1769 | `				/* An explicit key in an array LITERAL gets the same php diagnostics a` |
|       - | 1770 | `				 * subscript does — a float key that truncates deprecates, and so does an` |
|       - | 1771 | `				 * explicit null key. Only the subscript sites (LOAD_IDX/STORE_IDX) used to` |
|       - | 1772 | ``				 * emit these, so `[1.5 => "v"]` and `[null => "v"]` were silent. A key that`` |
|       - | 1773 | `				 * is ABSENT (auto-index) is a NULL slot here, not a null key, so the` |
|       - | 1774 | `				 * MEMOBJ_NULL check below is what tells the two apart. */` |
|   36345 | 1775 | `					if( (pEntry->iFlags & MEMOBJ_AUX_NOKEY) == 0 ){` |
|       - | 1776 | `						/* An object/array literal key is php's TypeError, a resource one` |
|       - | 1777 | `						 * warns and becomes its id — same rules as a subscript. */` |
|       - | 1778 | `						SyBlob sTypeMsg;` |
|   13385 | 1779 | `						if( VmOffsetTypeRejected(&(*pVm),pEntry,0,&sTypeMsg) ){` |
|       3 | 1780 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg));` |
|       2 | 1781 | `						}else{` |
|   13383 | 1782 | `							VmOffsetResourceWarn(&(*pVm),pEntry);` |
|       - | 1783 | `						}` |
|       - | 1784 | `						/* php DEPRECATES a null literal key (then normalizes to "") and` |
|       - | 1785 | `						 * rejects nothing there; PHL matches that (deprecate + fall` |
|       - | 1786 | `						 * through, PH7_HashmapInsert casts NULL->""). A lossy-FLOAT` |
|       - | 1787 | `						 * literal key still rejects with a TypeError — the recorded` |
|       - | 1788 | `						 * non-deprecated-surface policy, same as the subscript site. */` |
|   13385 | 1789 | `						int bNull = (pEntry->iFlags & MEMOBJ_NULL) != 0;` |
|   20076 | 1790 | `						int bLossyFloat = (pEntry->iFlags & MEMOBJ_REAL) != 0` |
|   13380 | 1791 | `							&& pEntry->rVal != (ph7_real)(sxi64)pEntry->rVal;` |
|   13385 | 1792 | `						if( bNull ){` |
|       3 | 1793 | `							VmNullOffsetDeprecate(&(*pVm),pEntry);` |
|   13384 | 1794 | `						}else if( bLossyFloat ){` |
|       3 | 1795 | `							const char *zErr = "Cannot access offset of type float on array";` |
|       - | 1796 | `							SyBlob sErrMsg;` |
|       3 | 1797 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 1798 | `							SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|       3 | 1799 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|       1 | 1800 | `						}` |
|    6690 | 1801 | `					}` |
|       - | 1802 | `				/* Standard insertion */` |
|   54515 | 1803 | `				PH7_HashmapInsert(pMap,` |
|   36340 | 1804 | `					(pEntry->iFlags & MEMOBJ_AUX_NOKEY) ? 0 /* Automatic index assign */ : pEntry,` |
|   18170 | 1805 | `					&pEntry[1]` |
|       - | 1806 | `				);` |
|       - | 1807 | `			}` |
|       - | 1808 | `			/* Next pair on the stack */` |
|   37127 | 1809 | `			pEntry += 2;` |
|       5 | 1810 | `		}` |
|       - | 1811 | `		/* Pop P1 elements */` |
|   14093 | 1812 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|   14093 | 1813 | `		if( rcSpread != SXRET_OK ){` |
|       - | 1814 | `			/* Discard the partially-built map and propagate the exception. */` |
|      21 | 1815 | `			PH7_HashmapRelease(pMap,TRUE);` |
|      21 | 1816 | `			if( rcSpread == PH7_ABORT ){` |
|     ! 0 | 1817 | `				VM_EXIT_ABORT;` |
|       - | 1818 | `			}` |
|       - | 1819 | `			{` |
|       - | 1820 | `				sxi32 iRp;` |
|      21 | 1821 | `				if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|       6 | 1822 | `					pc = iRp;` |
|       6 | 1823 | `					VM_EXIT_BREAK;` |
|       - | 1824 | `				}` |
|       - | 1825 | `			}` |
|      15 | 1826 | `			VM_EXIT_EXCEPTION;` |
|       - | 1827 | `		}` |
|    7035 | 1828 | `	}` |
|       - | 1829 | `	/* Push the hashmap */` |
|   83673 | 1830 | `	pTos++;` |
|   83673 | 1831 | `	pTos->nIdx = SXU32_HIGH;` |
|   83673 | 1832 | `	pTos->x.pOther = pMap;` |
|   83673 | 1833 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|   83673 | 1834 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1835 | `	VM_EXIT_BREAK;` |
|   41848 | 1836 | `}` |
|       - | 1837 |  |
|       - | 1838 | `/*` |
|       - | 1839 | ` * OP_LOAD_LIST: body moved verbatim from the OP_LOAD_LIST arm of` |
|       - | 1840 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1841 | ` */` |
|     484 | 1842 | `PH7_PRIVATE VmOpRc VmExecOpLoadList(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1843 | `{` |
|     489 | 1844 | `	ph7_value *pTos = pState->pTos;` |
|     489 | 1845 | `	ph7_value *pStack = pState->pStack;` |
|     489 | 1846 | `	VmInstr *aInstr = pState->aInstr;` |
|     489 | 1847 | `	sxi32 pc = pState->pc;` |
|       - | 1848 | `	sxi32 rc;` |
|     242 | 1849 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 1850 | `	ph7_value *pEntry;` |
|     489 | 1851 | `	sxi32 rcEnforce = SXRET_OK;` |
|     489 | 1852 | `	if( pInstr->iP1 <= 0 ){` |
|       - | 1853 | `		/* Empty list,break immediately */` |
|     ! 0 | 1854 | `		VM_EXIT_BREAK;` |
|       - | 1855 | `	}` |
|     489 | 1856 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|       - | 1857 | `#ifdef UNTRUST` |
|       - | 1858 | `	if( &pEntry[-1] < pStack ){` |
|       - | 1859 | `		VM_EXIT_ABORT;` |
|       - | 1860 | `	}` |
|       - | 1861 | `#endif` |
|     489 | 1862 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|     473 | 1863 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|       - | 1864 | `		ph7_hashmap_node *pNode;` |
|       - | 1865 | `		ph7_value sKey,*pObj;` |
|       - | 1866 | `		/* Start Copying */` |
|     473 | 1867 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|    1385 | 1868 | `		while( pEntry <= pTos ){` |
|     933 | 1869 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|     903 | 1870 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|     903 | 1871 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|    1719 | 1872 | `					int bTyped = SyHashTotalEntry(&pVm->hTypedSlot) > 0` |
|     898 | 1873 | `						&& SyHashGet(&pVm->hTypedSlot,(const void *)&pEntry->nIdx,sizeof(sxu32)) != 0;` |
|     903 | 1874 | `					if( rc != SXRET_OK ){` |
|       - | 1875 | `						/* Undefined array key */` |
|       - | 1876 | `						char zMsg[128];` |
|       5 | 1877 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|       5 | 1878 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|       2 | 1879 | `					}` |
|     903 | 1880 | `					if( !bTyped ){` |
|     873 | 1881 | `						if( rc == SXRET_OK ){` |
|       - | 1882 | `							/* Store node value */` |
|     873 | 1883 | `							PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|     439 | 1884 | `						}else{` |
|     ! 0 | 1885 | `							PH7_MemObjRelease(pObj);` |
|       - | 1886 | `						}` |
|     439 | 1887 | `					}else{` |
|       - | 1888 | ``						/* Typed/readonly property target (`[$o->p] = [...]`): a`` |
|       - | 1889 | `						 * direct slot write would bypass the typed-slot table, so` |
|       - | 1890 | `						 * enforce on a temp first — a TypeError leaves the property` |
|       - | 1891 | `						 * untouched, and a missing key assigns null, which a` |
|       - | 1892 | `						 * non-nullable type rejects exactly like php (warning, then` |
|       - | 1893 | `						 * "Cannot assign null to property ... of type ..."). */` |
|       - | 1894 | `						ph7_value sVal;` |
|      31 | 1895 | `						PH7_MemObjInit(&(*pVm),&sVal);` |
|      31 | 1896 | `						if( rc == SXRET_OK ){` |
|      27 | 1897 | `							PH7_HashmapExtractNodeValue(pNode,&sVal,TRUE);` |
|      13 | 1898 | `						}` |
|      31 | 1899 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),pEntry->nIdx,&sVal,0);` |
|      31 | 1900 | `						if( rcEnforce != SXRET_OK ){` |
|       - | 1901 | `							/* Thrown: stop assigning (php aborts the list at the` |
|       - | 1902 | `							 * first failing element), settle the stack, route. */` |
|      17 | 1903 | `							PH7_MemObjRelease(&sVal);` |
|      17 | 1904 | `							break;` |
|       - | 1905 | `						}` |
|      15 | 1906 | `						PH7_MemObjStore(&sVal,pObj);` |
|      15 | 1907 | `						PH7_MemObjRelease(&sVal);` |
|       - | 1908 | `					}` |
|     441 | 1909 | `				}` |
|     441 | 1910 | `			}` |
|     917 | 1911 | `			sKey.x.iVal++; /* Next numeric index */` |
|     917 | 1912 | `			pEntry++;` |
|       5 | 1913 | `		}` |
|     239 | 1914 | `	}else{` |
|       - | 1915 | `		/* Source is not an array: php warns first (silencing ONLY null — a bool` |
|       - | 1916 | `		 * source warns too, php 8), then assigns null to every target. A typed` |
|       - | 1917 | `		 * property target receives that null THROUGH enforcement, so a` |
|       - | 1918 | `		 * non-nullable type throws "Cannot assign null to property ..." exactly` |
|       - | 1919 | `		 * like php instead of silently nulling the slot. PHL DIVERGENCE: bool` |
|       - | 1920 | `		 * FALSE stays silent (php warns) — it is the end-of-array sentinel of` |
|       - | 1921 | `` 		 * the documented each() extension's `while (list(..) = each($a))` `` |
|       - | 1922 | `		 * idiom, which would otherwise warn on every normal loop exit. */` |
|       - | 1923 | `		ph7_value *pObj;` |
|      30 | 1924 | `		int bFalseSrc = (pTos[-pInstr->iP1].iFlags & MEMOBJ_BOOL) != 0` |
|      16 | 1925 | `			&& pTos[-pInstr->iP1].x.iVal == 0;` |
|      19 | 1926 | `		if( (pTos[-pInstr->iP1].iFlags & MEMOBJ_NULL) == 0 && !bFalseSrc ){` |
|      12 | 1927 | `			VmWarnCannotUseAsArray(&(*pVm),pTos[-pInstr->iP1].iFlags);` |
|       5 | 1928 | `		}` |
|      33 | 1929 | `		while( pEntry <= pTos ){` |
|      23 | 1930 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|      23 | 1931 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      42 | 1932 | `					int bTyped = SyHashTotalEntry(&pVm->hTypedSlot) > 0` |
|      20 | 1933 | `						&& SyHashGet(&pVm->hTypedSlot,(const void *)&pEntry->nIdx,sizeof(sxu32)) != 0;` |
|      23 | 1934 | `					if( !bTyped ){` |
|      15 | 1935 | `						PH7_MemObjRelease(pObj);` |
|       9 | 1936 | `					}else{` |
|       - | 1937 | `						ph7_value sVal;` |
|       9 | 1938 | `						PH7_MemObjInit(&(*pVm),&sVal);` |
|       9 | 1939 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),pEntry->nIdx,&sVal,0);` |
|       9 | 1940 | `						if( rcEnforce != SXRET_OK ){` |
|       7 | 1941 | `							PH7_MemObjRelease(&sVal);` |
|       7 | 1942 | `							break;` |
|       - | 1943 | `						}` |
|       3 | 1944 | `						PH7_MemObjStore(&sVal,pObj);` |
|       3 | 1945 | `						PH7_MemObjRelease(&sVal);` |
|       - | 1946 | `					}` |
|       7 | 1947 | `				}` |
|       7 | 1948 | `			}` |
|      17 | 1949 | `			pEntry++;` |
|       3 | 1950 | `		}` |
|       - | 1951 | `	}` |
|     489 | 1952 | `	if( rcEnforce != SXRET_OK ){` |
|       - | 1953 | `		/* Settle this op's own operands: the P1 entries AND the source value —` |
|       - | 1954 | `		 * its statement-level OP_POP is skipped when a catch resumes at the` |
|       - | 1955 | `		 * landing pad, so leaving it would leak one operand slot per caught` |
|       - | 1956 | `		 * throw. A NESTED destructure can still have the outer list's operands` |
|       - | 1957 | `		 * abandoned above the try's base, so on an in-place catch drain to the` |
|       - | 1958 | `		 * catching try's recorded depth (like the fetch-point router and the` |
|       - | 1959 | `		 * generator inject path), not just our own pops. */` |
|      23 | 1960 | `		VmPopOperand(&pTos,pInstr->iP1 + 1);` |
|      23 | 1961 | `		if( rcEnforce == PH7_ABORT ){` |
|     ! 0 | 1962 | `			VM_EXIT_ABORT;` |
|       - | 1963 | `		}` |
|       - | 1964 | `		{` |
|       - | 1965 | `			sxi32 _iRpL;` |
|      34 | 1966 | `			PH7_INLINE_RESUME_BREAK()` |
|      23 | 1967 | `			if( VmRecordedResume(pVm,&_iRpL,pState->pEntryFrame,aInstr) ){` |
|      25 | 1968 | `				PH7_RESUME_DRAIN()` |
|      23 | 1969 | `				pc = _iRpL;` |
|      23 | 1970 | `				VM_EXIT_BREAK;` |
|       - | 1971 | `			}` |
|       - | 1972 | `		}` |
|     ! 0 | 1973 | `		VM_EXIT_EXCEPTION;` |
|       - | 1974 | `	}` |
|     467 | 1975 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|     467 | 1976 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1977 | `	VM_EXIT_BREAK;` |
|     247 | 1978 | `}` |
|       - | 1979 |  |
|       - | 1980 | `/*` |
|       - | 1981 | ` * OP_UNSET_VAR: body moved verbatim from the OP_UNSET_VAR arm of` |
|       - | 1982 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1983 | ` */` |
|    6994 | 1984 | `PH7_PRIVATE VmOpRc VmExecOpUnsetVar(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1985 | `{` |
|    6999 | 1986 | `	ph7_value *pTos = pState->pTos;` |
|    6999 | 1987 | `	ph7_value *pStack = pState->pStack;` |
|    6999 | 1988 | `	VmInstr *aInstr = pState->aInstr;` |
|    6999 | 1989 | `	sxi32 pc = pState->pc;` |
|       - | 1990 | `	sxi32 rc;` |
|    3497 | 1991 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 1992 | `	/* unset($name): p3 is the variable name. Drops the NAME only — see VmUnsetVarByName */` |
|    6999 | 1993 | `	SyString *pName = (SyString *)pInstr->p3;` |
|    6999 | 1994 | `	if( pName && pVm->pFrame ){` |
|       - | 1995 | `		/* Inside a try{} the VM pushes an EXCEPTION frame; variables live in the body` |
|       - | 1996 | `		 * frame below it, so skip past it exactly as every other variable path does.` |
|       - | 1997 | `		 * Without this, unset($x) inside a try silently found nothing and did nothing. */` |
|    6999 | 1998 | `		VmFrame *pVarFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|    6999 | 1999 | `		sxi32 rcU = VmUnsetVarByName(&(*pVm),pVarFrame,pName->zString,pName->nByte);` |
|    6999 | 2000 | `		if( rcU == PH7_ABORT ){` |
|       3 | 2001 | `			VM_EXIT_ABORT;` |
|       - | 2002 | `		}` |
|       - | 2003 | `		/* Releasing the last holder can run a __destruct(), and that destructor may` |
|       - | 2004 | `		 * throw. Such a throw is PARKED in nBoundaryRc by the boundary rail; consume it` |
|       - | 2005 | `		 * here and route it, or the catch runs and execution resumes inside the try` |
|       - | 2006 | `		 * ("resumed-dtor" instead of php's "caught-dtor"). */` |
|    6997 | 2007 | `		if( pVm->nBoundaryRc != 0 ){` |
|       3 | 2008 | `			rc = pVm->nBoundaryRc;` |
|       3 | 2009 | `			pVm->nBoundaryRc = 0;` |
|       3 | 2010 | `			if( rc == PH7_ABORT ){` |
|     ! 0 | 2011 | `				VM_EXIT_ABORT;` |
|       - | 2012 | `			}` |
|       3 | 2013 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2014 | `		}` |
|    3495 | 2015 | `	}` |
|    6995 | 2016 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2017 | `	VM_EXIT_BREAK;` |
|    3502 | 2018 | `}` |
|       - | 2019 |  |
