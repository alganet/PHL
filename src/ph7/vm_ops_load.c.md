# src/ph7/vm_ops_load.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 766/872 lines (87.84%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/*` |
|      - |    8 | ` * Section:` |
|      - |    9 | ` *    Opcode handlers extracted from vm.c's dispatch loop. Each handler runs` |
|      - |   10 | ` *    one opcode arm against the caller's VmExecState: the loop syncs pTos/pc` |
|      - |   11 | ` *    in, calls the handler, reloads them and routes the returned VmOpRc onto` |
|      - |   12 | ` *    its labels (same idiom as VmCallFinish).` |
|      - |   13 | ` * Status:` |
|      - |   14 | ` *    Stable.` |
|      - |   15 | ` */` |
|      - |   16 | `/* Bind the dispatch-routing exits to handler semantics (see vm_dispatch.h),` |
|      - |   17 | ` * and map the macros' sState references onto our state parameter. */` |
|      - |   18 | `#define VM_EXIT_BREAK      { pState->pTos = pTos; pState->pc = pc; return VM_OP_NEXT; }` |
|      - |   19 | `#define VM_EXIT_ABORT      { pState->pTos = pTos; pState->pc = pc; return VM_OP_ABORT; }` |
|      - |   20 | `#define VM_EXIT_EXCEPTION  { pState->pTos = pTos; pState->pc = pc; return VM_OP_EXCEPTION; }` |
|      - |   21 | `#include "vm_dispatch.h"` |
|      - |   22 | `#define sState (*pState)` |
|      - |   23 |  |
|      - |   24 | `/*` |
|      - |   25 | ` * OP_STORE_REF: body moved verbatim from the OP_STORE_REF arm of` |
|      - |   26 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |   27 | ` */` |
|     52 |   28 | `PH7_PRIVATE VmOpRc VmExecOpStoreRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      4 |   29 | `{` |
|     56 |   30 | `	ph7_value *pTos = pState->pTos;` |
|     56 |   31 | `	ph7_value *pStack = pState->pStack;` |
|     56 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|     56 |   33 | `	sxi32 pc = pState->pc;` |
|      - |   34 | `	sxi32 rc;` |
|     26 |   35 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     56 |   36 | `	 SyString sName = { 0 , 0 };` |
|      - |   37 | `	 VmFrame *pFrameLocal;` |
|      - |   38 | `	SyHashEntry *pEntry;` |
|      - |   39 | `	sxu32 nIdx;` |
|      - |   40 | `#ifdef UNTRUST` |
|      - |   41 | `	if( pTos < pStack ){` |
|      - |   42 | `		VM_EXIT_ABORT;` |
|      - |   43 | `	}` |
|      - |   44 | `#endif` |
|     56 |   45 | `	if( pInstr->iP2 == 1 ){` |
|      - |   46 | ``		/* Member reference target: `$o->p =& $x` / `self::$s =& $x`. The`` |
|      - |   47 | `		 * preceding OP_MEMBER (PH7_MEMBER_REF_TARGET) resolved the property slot` |
|      - |   48 | `		 * and stashed it. Stack: [ ... , source, member-result(top) ]. Rebind the` |
|      - |   49 | `		 * property's nIdx to alias the source variable's slot and pin that slot` |
|      - |   50 | `		 * past its owning frame (like a use(&$x) capture) so neither frame` |
|      - |   51 | `		 * teardown nor a later unset recycles it while the property aliases it. */` |
|     11 |   52 | `		ph7_value *pSrc = &pTos[-1];` |
|     11 |   53 | `		sxu32 nSrcIdx = pSrc->nIdx;` |
|     11 |   54 | `		VmClassAttr *pVmAttr = pVm->pRefTargetAttr;` |
|     11 |   55 | `		ph7_class_attr *pStAttr = pVm->pRefTargetStaticAttr;` |
|     11 |   56 | `		if( nSrcIdx == SXU32_HIGH ){` |
|      - |   57 | ``			/* php: the RHS of `=&` must be a variable, not a constant expression.`` |
|      - |   58 | `			 * (The compiler already rejects the obvious literal forms.) */` |
|    ! 0 |   59 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |   60 | `				"Reference operator require a variable not a constant as it's right operand");` |
|     11 |   61 | `		}else if( pVmAttr ){` |
|      9 |   62 | `			sxu32 nOldIdx = pVmAttr->nIdx;` |
|      9 |   63 | `			if( nOldIdx != nSrcIdx ){` |
|      9 |   64 | `				if( (pVmAttr->iState & VM_CLASS_ATTR_REFBOUND) == 0 ){` |
|      - |   65 | `					/* Release this property's own (unshared) slot before repointing.` |
|      - |   66 | `					 * A reference-bound property bypasses typed coercion in php, so` |
|      - |   67 | `					 * drop any typed-slot enforcement entry too. */` |
|      9 |   68 | `					if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      5 |   69 | `						SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&nOldIdx,sizeof(sxu32),0);` |
|      2 |   70 | `					}` |
|      9 |   71 | `					PH7_VmUnsetMemObj(&(*pVm),nOldIdx,TRUE);` |
|      4 |   72 | `				}` |
|      9 |   73 | `				pVmAttr->nIdx = nSrcIdx;` |
|      9 |   74 | `				pVmAttr->iState \|= VM_CLASS_ATTR_REFBOUND;` |
|      9 |   75 | `				pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      9 |   76 | `				VmPinMemObjSlot(&(*pVm),nSrcIdx);` |
|      5 |   77 | `			}` |
|      7 |   78 | `		}else if( pStAttr ){` |
|      3 |   79 | `			if( pStAttr->nIdx != nSrcIdx ){` |
|      3 |   80 | `				pStAttr->nIdx = nSrcIdx;` |
|      3 |   81 | `				VmPinMemObjSlot(&(*pVm),nSrcIdx);` |
|      1 |   82 | `			}` |
|      1 |   83 | `		}` |
|     11 |   84 | `		if( pVm->pRefTargetThis ){` |
|      9 |   85 | `			PH7_ClassInstanceUnref(pVm->pRefTargetThis);` |
|      4 |   86 | `		}` |
|     11 |   87 | `		pVm->pRefTargetAttr = 0;` |
|     11 |   88 | `		pVm->pRefTargetStaticAttr = 0;` |
|     11 |   89 | `		pVm->pRefTargetThis = 0;` |
|      - |   90 | `		/* Pop the member-result; leave the source as the expression value. */` |
|     11 |   91 | `		VmPopOperand(&pTos,1);` |
|     11 |   92 | `		VM_EXIT_BREAK;` |
|      - |   93 | `	}` |
|     46 |   94 | `	if( pInstr->p3 == 0 ){` |
|      - |   95 | `		char *zName;` |
|      - |   96 | `		/* Take the variable name from the Next on the stack */` |
|    ! 0 |   97 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |   98 | `			/* Force a string cast */` |
|    ! 0 |   99 | `			PH7_MemObjToString(pTos);` |
|    ! 0 |  100 | `		}` |
|    ! 0 |  101 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    ! 0 |  102 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|    ! 0 |  103 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  104 | `			if( zName ){` |
|    ! 0 |  105 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  106 | `			}` |
|    ! 0 |  107 | `		}` |
|    ! 0 |  108 | `		PH7_MemObjRelease(pTos);` |
|    ! 0 |  109 | `		pTos--;` |
|    ! 0 |  110 | `	}else{` |
|     46 |  111 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|      - |  112 | `	}` |
|     46 |  113 | `	nIdx = pTos->nIdx;` |
|     46 |  114 | `	if(nIdx == SXU32_HIGH ){` |
|    ! 0 |  115 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|    ! 0 |  116 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |  117 | `				"Reference operator require a variable not a constant as it's right operand");` |
|    ! 0 |  118 | `		}else{` |
|      - |  119 | `			ph7_value *pObj;` |
|      - |  120 | `			/* Extract the desired variable and if not available dynamically create it */` |
|    ! 0 |  121 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|    ! 0 |  122 | `			if( pObj == 0 ){` |
|    ! 0 |  123 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      - |  124 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|    ! 0 |  125 | `				VM_EXIT_ABORT;` |
|      - |  126 | `			}` |
|      - |  127 | `			/* Perform the store operation */` |
|    ! 0 |  128 | `			PH7_MemObjStore(pTos,pObj);` |
|    ! 0 |  129 | `			pTos->nIdx = pObj->nIdx;` |
|    ! 0 |  130 | `		}` |
|     46 |  131 | `	}else if( sName.nByte > 0){` |
|     46 |  132 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      - |  133 | `			/* php 8.1's non-catchable fatal (compile-time there) */` |
|      3 |  134 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|      3 |  135 | `			pVm->iExitStatus = 255;` |
|      3 |  136 | `			pVm->bHaltRequested = 1;` |
|      3 |  137 | `			VM_EXIT_ABORT;` |
|    ! 0 |  138 | `		}else{` |
|     43 |  139 | `			pFrameLocal = pVm->pFrame;` |
|     43 |  140 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      - |  141 | `			/* Query the local frame */` |
|     43 |  142 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|     43 |  143 | `			if( pEntry ){` |
|    ! 0 |  144 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|    ! 0 |  145 | `			}else{` |
|     43 |  146 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|     43 |  147 | `				if( pFrameLocal->pParent == 0 ){` |
|      - |  148 | `					/* Insert in the $GLOBALS array */` |
|     39 |  149 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|     18 |  150 | `				}` |
|     43 |  151 | `				if( rc == SXRET_OK ){` |
|     43 |  152 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|     20 |  153 | `				}` |
|      - |  154 | `			}` |
|      - |  155 | `		}` |
|     20 |  156 | `	}` |
|     43 |  157 | `	VM_EXIT_BREAK;` |
|    ! 0 |  158 | `	VM_EXIT_BREAK;` |
|     30 |  159 | `}` |
|      - |  160 |  |
|      - |  161 | `/* LOAD_IDX's own iP2 context codes (they do NOT line up with PH7_MEMBER_*). */` |
|      - |  162 | `#define VM_IDX_CTX_ISSET 4` |
|      - |  163 | `#define VM_IDX_CTX_UNSET 5` |
|      - |  164 | `#define VM_IDX_CTX_EMPTY 6` |
|      - |  165 | `/*` |
|      - |  166 | ` * php rejects an OBJECT or an ARRAY used as an array offset, naming the offending` |
|      - |  167 | ` * type the way get_debug_type() does — the CLASS name for an object, "array" for` |
|      - |  168 | ` * an array — and wording the failure by context:` |
|      - |  169 | ` *` |
|      - |  170 | ` *   read/write   Cannot access offset of type Foo on array` |
|      - |  171 | ` *   isset/empty  Cannot access offset of type Foo in isset or empty` |
|      - |  172 | ` *   unset        Cannot unset offset of type Foo on array` |
|      - |  173 | ` *` |
|      - |  174 | ` * A RESOURCE is deliberately absent: php does not reject it, it warns` |
|      - |  175 | ` * ("Resource ID#N used as offset, casting to integer (N)") and uses the id as an` |
|      - |  176 | ` * integer key. VmOffsetResourceWarn() below handles that half.` |
|      - |  177 | ` *` |
|      - |  178 | ` * Returns TRUE and fills pMsg when the key must be rejected. iCtx is the` |
|      - |  179 | ` * instruction's iP2 (any value other than the isset/unset/empty codes reads as` |
|      - |  180 | ` * an access).` |
|      - |  181 | ` */` |
| 227122 |  182 | `static int VmOffsetTypeRejected(ph7_vm *pVm,ph7_value *pKey,int iCtx,SyBlob *pMsg)` |
|      5 |  183 | `{` |
|      - |  184 | `	const char *zType;` |
| 227127 |  185 | `	SyString *pClass = 0;` |
| 227127 |  186 | `	if( pKey == 0 ){` |
|    ! 0 |  187 | `		return FALSE;` |
|      - |  188 | `	}` |
| 227127 |  189 | `	if( pKey->iFlags & MEMOBJ_OBJ ){` |
|     15 |  190 | `		ph7_class_instance *pInst = (ph7_class_instance *)pKey->x.pOther;` |
|     15 |  191 | `		if( pInst && pInst->pClass ){` |
|     15 |  192 | `			pClass = &pInst->pClass->sName;` |
|      7 |  193 | `		}` |
|     15 |  194 | `		zType = "object";` |
| 227120 |  195 | `	}else if( pKey->iFlags & MEMOBJ_HASHMAP ){` |
|      5 |  196 | `		zType = "array";` |
|      3 |  197 | `	}else{` |
| 227109 |  198 | `		return FALSE;` |
|      - |  199 | `	}` |
|     19 |  200 | `	SyBlobInit(pMsg,&pVm->sAllocator);` |
|     19 |  201 | `	if( iCtx == VM_IDX_CTX_UNSET ){` |
|      3 |  202 | `		SyBlobAppend(pMsg,"Cannot unset offset of type ",sizeof("Cannot unset offset of type ")-1);` |
|      2 |  203 | `	}else{` |
|     17 |  204 | `		SyBlobAppend(pMsg,"Cannot access offset of type ",sizeof("Cannot access offset of type ")-1);` |
|      - |  205 | `	}` |
|     19 |  206 | `	if( pClass ){` |
|     15 |  207 | `		SyBlobAppend(pMsg,pClass->zString,pClass->nByte);` |
|      8 |  208 | `	}else{` |
|      5 |  209 | `		SyBlobAppend(pMsg,zType,(sxu32)SyStrlen(zType));` |
|      - |  210 | `	}` |
|     19 |  211 | `	if( iCtx == VM_IDX_CTX_ISSET \|\| iCtx == VM_IDX_CTX_EMPTY ){` |
|      7 |  212 | `		SyBlobAppend(pMsg," in isset or empty",sizeof(" in isset or empty")-1);` |
|      4 |  213 | `	}else{` |
|     13 |  214 | `		SyBlobAppend(pMsg," on array",sizeof(" on array")-1);` |
|      - |  215 | `	}` |
|     19 |  216 | `	return TRUE;` |
| 113566 |  217 | `}` |
|      - |  218 | `/*` |
|      - |  219 | ` * php's other half of the offset-type rules: a RESOURCE offset is accepted, with` |
|      - |  220 | `` * `Warning: Resource ID#N used as offset, casting to integer (N)`, and the key`` |
|      - |  221 | ` * becomes that integer. Rewrites pKey in place so the normal integer-key path` |
|      - |  222 | ` * takes over.` |
|      - |  223 | ` */` |
| 227104 |  224 | `static void VmOffsetResourceWarn(ph7_vm *pVm,ph7_value *pKey)` |
|      5 |  225 | `{` |
|      - |  226 | `	sxu32 nId;` |
| 227109 |  227 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_RES) == 0 ){` |
| 227105 |  228 | `		return;` |
|      - |  229 | `	}` |
|      5 |  230 | `	nId = PH7_VmResourceId(pVm,pKey->x.pOther);` |
|      7 |  231 | `	VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      2 |  232 | `		"Resource ID#%u used as offset, casting to integer (%u)",nId,nId);` |
|      5 |  233 | `	PH7_MemObjRelease(pKey);` |
|      5 |  234 | `	pKey->x.iVal = (sxi64)nId;` |
|      5 |  235 | `	MemObjSetType(pKey,MEMOBJ_INT);` |
| 113557 |  236 | `}` |
|      - |  237 | `/*` |
|      - |  238 | ` * php DEPRECATES (it does not reject) a NULL array offset, then normalizes it to` |
|      - |  239 | `` * the empty-string key "": `$a[null]`, `$a[$undef]` and `[null => v]` all warn`` |
|      - |  240 | `` * `Using null as an array offset is deprecated, use an empty string instead` and`` |
|      - |  241 | ` * then read/write the "" slot. Emit that E_DEPRECATED notice and return TRUE so` |
|      - |  242 | ` * the caller falls through to the ordinary lookup/insert, which already casts` |
|      - |  243 | ` * NULL->"" (HashmapLookup / HashmapInsert). A LOSSY-FLOAT offset stays a rejected` |
|      - |  244 | ` * TypeError: PHL deliberately targets php's NON-deprecated surface for the` |
|      - |  245 | ` * float->int truncation (VmRejectFloatOperand policy, §2), so only the null case` |
|      - |  246 | ` * is coerced here. Returns FALSE (no notice) for a non-null key.` |
|      - |  247 | ` */` |
| 243352 |  248 | `static int VmNullOffsetDeprecate(ph7_vm *pVm,ph7_value *pKey)` |
|      5 |  249 | `{` |
| 243357 |  250 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_NULL) == 0 ){` |
| 243339 |  251 | `		return FALSE;` |
|      - |  252 | `	}` |
|     19 |  253 | `	PH7_VmThrowError(&(*pVm),0,E_DEPRECATED,` |
|      - |  254 | `		"Using null as an array offset is deprecated, use an empty string instead");` |
|     19 |  255 | `	return TRUE;` |
| 121681 |  256 | `}` |
|      - |  257 | `/*` |
|      - |  258 | ` * OP_STORE_IDX_REF: body moved verbatim from the OP_STORE_IDX_REF arm of` |
|      - |  259 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  260 | ` */` |
| 243558 |  261 | `PH7_PRIVATE VmOpRc VmExecOpStoreIdxRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  262 | `{` |
| 243563 |  263 | `	ph7_value *pTos = pState->pTos;` |
| 243563 |  264 | `	ph7_value *pStack = pState->pStack;` |
| 243563 |  265 | `	VmInstr *aInstr = pState->aInstr;` |
| 243563 |  266 | `	sxi32 pc = pState->pc;` |
|      - |  267 | `	sxi32 rc;` |
| 121779 |  268 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 243563 |  269 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|      - |  270 | `	ph7_value *pKey;` |
|      - |  271 | `	sxu32 nIdx;` |
| 243563 |  272 | `	if( pInstr->iP1 ){` |
|      - |  273 | `		/* Key is next on stack */` |
|  69525 |  274 | `		pKey = pTos;` |
|  69525 |  275 | `		pTos--;` |
|  34765 |  276 | `	}else{` |
| 174043 |  277 | `		pKey = 0;` |
|      - |  278 | `	}` |
|      - |  279 | `		/* php DEPRECATES a null / lossy-float write subscript (then normalizes "" /` |
|      - |  280 | ``		 * truncates). A by-VALUE write (`$a[$k]=v`, PH7_OP_STORE_IDX) matches php on`` |
|      - |  281 | `		 * the null case: deprecate + coerce, since PH7_HashmapInsert casts NULL->"".` |
|      - |  282 | `		 * That deprecation is emitted DOWN AT THE INSERT (below), not here, so a null` |
|      - |  283 | ``		 * container that auto-vivifies (`$x=null; $x[null]=v`) gets it too — the base`` |
|      - |  284 | ``		 * is not yet a hashmap at this point. A by-REF write (`$a[$k]=&$x`,`` |
|      - |  285 | `		 * PH7_OP_STORE_IDX_REF) keeps the loud null TypeError: HashmapInsertByRef still` |
|      - |  286 | `		 * treats the "" key as auto-index (a separate, pre-existing byref bug —` |
|      - |  287 | ``		 * `$a[""] =& $x` drops the value), so deprecate-and-coerce there would turn a`` |
|      - |  288 | `		 * loud error into a SILENT WRONG answer (§7 residual — the two ship together).` |
|      - |  289 | `		 * A lossy-FLOAT subscript stays a loud TypeError in BOTH forms (the recorded` |
|      - |  290 | `		 * non-deprecated-surface policy, §2). */` |
| 243563 |  291 | `		if( pKey && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|  69327 |  292 | `			int bNull = (pKey->iFlags & MEMOBJ_NULL) != 0;` |
| 103991 |  293 | `			int bLossyFloat = (pKey->iFlags & MEMOBJ_REAL) != 0` |
|  69322 |  294 | `				&& pKey->rVal != (ph7_real)(sxi64)pKey->rVal;` |
|  69327 |  295 | `			int bByRef = (pInstr->iOp == PH7_OP_STORE_IDX_REF);` |
|      - |  296 | `			SyBlob sTypeMsg;` |
|      - |  297 | `			/* An object/array key is php's TypeError; a resource key warns and` |
|      - |  298 | `			 * becomes its integer id. Both used to be stringified silently. */` |
|  69327 |  299 | `			if( VmOffsetTypeRejected(&(*pVm),pKey,0,&sTypeMsg) ){` |
|      - |  300 | `				sxi32 rcSc;` |
|      5 |  301 | `				PH7_MemObjRelease(pKey);` |
|      5 |  302 | `				VmPopOperand(&pTos,1);` |
|      5 |  303 | `				rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      8 |  304 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      5 |  305 | `				rc = rcSc;` |
|      5 |  306 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  307 | `			}` |
|  69323 |  308 | `			VmOffsetResourceWarn(&(*pVm),pKey);` |
|  69323 |  309 | `			if( (bNull && bByRef) \|\| bLossyFloat ){` |
|      - |  310 | `				sxi32 rcSc;` |
|      3 |  311 | `				const char *zErr = bNull ? "Cannot access offset of type null on array"` |
|      1 |  312 | `				                         : "Cannot access offset of type float on array";` |
|      3 |  313 | `				PH7_MemObjRelease(pKey);` |
|      3 |  314 | `				VmPopOperand(&pTos,1);` |
|      3 |  315 | `				rcSc = VmThrowFromVm(&(*pVm),"TypeError",zErr,(sxu32)SyStrlen(zErr));` |
|      3 |  316 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  317 | `				rc = rcSc;` |
|      3 |  318 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  319 | `			}` |
|  34658 |  320 | `		}` |
| 243557 |  321 | `	nIdx = pTos->nIdx;` |
|      - |  322 | `	{` |
|      - |  323 | `		/* ArrayAccess::offsetSet dispatch.` |
|      - |  324 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|      - |  325 | `		 * the backing variable slot at nIdx. */` |
| 243557 |  326 | `		ph7_class_instance *pInst = 0;` |
| 243557 |  327 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|     89 |  328 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
| 243514 |  329 | `		}else if( nIdx != SXU32_HIGH ){` |
| 243471 |  330 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
| 243471 |  331 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|    ! 0 |  332 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|    ! 0 |  333 | `			}` |
| 121733 |  334 | `		}` |
| 243557 |  335 | `		if( pInst ){` |
|     89 |  336 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|     89 |  337 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|      - |  338 | `				ph7_class_method *pMeth;` |
|      - |  339 | `				ph7_value sNullKey;` |
|      - |  340 | `				ph7_value *apArg[2];` |
|     87 |  341 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|    ! 0 |  342 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |  343 | `						"Cannot assign by reference to overloaded object");` |
|    ! 0 |  344 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|    ! 0 |  345 | `					VmPopOperand(&pTos,2); /* container + value */` |
|    ! 0 |  346 | `					VM_EXIT_BREAK;` |
|      - |  347 | `				}` |
|     87 |  348 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  349 | `					"offsetSet",sizeof("offsetSet")-1);` |
|      - |  350 | `				/* Pop container; pTos now points to the value */` |
|     87 |  351 | `				VmPopOperand(&pTos,1);` |
|     87 |  352 | `				if( pKey == 0 ){` |
|     10 |  353 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|     10 |  354 | `					apArg[0] = &sNullKey;` |
|      6 |  355 | `				}else{` |
|     79 |  356 | `					apArg[0] = pKey;` |
|      - |  357 | `				}` |
|     87 |  358 | `				apArg[1] = pTos;` |
|     87 |  359 | `				if( pMeth ){` |
|     87 |  360 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|     42 |  361 | `				}` |
|     87 |  362 | `				if( pKey ){` |
|     79 |  363 | `					PH7_MemObjRelease(pKey);` |
|     41 |  364 | `				}else{` |
|     10 |  365 | `					PH7_MemObjRelease(&sNullKey);` |
|      - |  366 | `				}` |
|      - |  367 | `				/* Pop the value */` |
|     87 |  368 | `				VmPopOperand(&pTos,1);` |
|     87 |  369 | `				VM_EXIT_BREAK;` |
|      - |  370 | `			}` |
|      - |  371 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|      - |  372 | `			 * than silently coercing the object into a hashmap (which is` |
|      - |  373 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|      - |  374 | `			 * a few lines below). Match PHP. */` |
|      - |  375 | `			{` |
|      - |  376 | `				char zMsg[256];` |
|      3 |  377 | `				SyString *pName = &pInst->pClass->sName;` |
|      4 |  378 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  379 | `					"Cannot use object of type %.*s as array",` |
|      2 |  380 | `					(int)pName->nByte,pName->zString);` |
|      3 |  381 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      3 |  382 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      3 |  383 | `				VmPopOperand(&pTos,2); /* container + value */` |
|      3 |  384 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  385 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  386 | `			}` |
|      - |  387 | `		}` |
|      - |  388 | `	}` |
| 243471 |  389 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      - |  390 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|      - |  391 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|      - |  392 | `		 * checking true sharing count, then re-add after separation. */` |
| 243335 |  393 | `		if( nIdx != SXU32_HIGH ){` |
| 243335 |  394 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
| 365000 |  395 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
| 243335 |  396 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  397 | `				/* Only adjust refcount / perform COW if the backing variable` |
|      - |  398 | `				 * is still sharing the same hashmap instance. This mirrors` |
|      - |  399 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|      - |  400 | `				 * refcounts if the backing array was already separated. */` |
| 243335 |  401 | `				if( pBacking->x.pOther == (void *)pCur ){` |
| 243335 |  402 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
| 243335 |  403 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
| 243335 |  404 | `					pMap->iRef++;  /* Re-add stack ref */` |
| 243335 |  405 | `					pTos->x.pOther = pMap;` |
| 121670 |  406 | `				}else{` |
|      - |  407 | `					/* Backing variable no longer points at pCur: skip COW here` |
|      - |  408 | `					 * and operate on the hashmap currently on the stack. */` |
|    ! 0 |  409 | `					pMap = pCur;` |
|      - |  410 | `				}` |
| 121670 |  411 | `			}else{` |
|    ! 0 |  412 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  413 | `			}` |
| 121670 |  414 | `		}else{` |
|    ! 0 |  415 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  416 | `		}` |
| 243335 |  417 | `		if( pMap->iRef < 2 ){` |
|      - |  418 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|      - |  419 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|      - |  420 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|      - |  421 | `			 * no code checks iRef for COW decisions. */` |
|    ! 0 |  422 | `			pMap->iRef = 2;` |
|    ! 0 |  423 | `		}` |
| 121670 |  424 | `	}else{` |
|      - |  425 | `		ph7_value *pObj;` |
|    138 |  426 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|    138 |  427 | `		if( pObj == 0 ){` |
|    ! 0 |  428 | `			if( pKey ){` |
|    ! 0 |  429 | `			  PH7_MemObjRelease(pKey);` |
|    ! 0 |  430 | `			}` |
|    ! 0 |  431 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  432 | `			VM_EXIT_BREAK;` |
|      - |  433 | `		}` |
|      - |  434 | `		/* Phase#1: Load the array */` |
|    138 |  435 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|    107 |  436 | `			VmPopOperand(&pTos,1);` |
|    107 |  437 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|      - |  438 | `				/* Force a string cast */` |
|    ! 0 |  439 | `				PH7_MemObjToString(pTos);` |
|    ! 0 |  440 | `			}` |
|    107 |  441 | `			if( pKey == 0 ){` |
|      - |  442 | ``				/* `$s[] = 'x'` on a STRING: php raises the catchable Error`` |
|      - |  443 | `				 * "[] operator not supported for strings" and leaves the string` |
|      - |  444 | `				 * untouched. PHL silently APPENDED, so code that meant to build` |
|      - |  445 | `				 * an array from a variable holding a string quietly produced a` |
|      - |  446 | `				 * longer string instead of failing — a wrong answer, not a` |
|      - |  447 | `				 * missing diagnostic. */` |
|      - |  448 | `				SyBlob sErrMsg;` |
|      3 |  449 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      3 |  450 | `				SyBlobAppend(&sErrMsg,"[] operator not supported for strings",` |
|      - |  451 | `					sizeof("[] operator not supported for strings")-1);` |
|      3 |  452 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      3 |  453 | `				VM_EXIT_BREAK;` |
|    ! 0 |  454 | `			}else{` |
|      - |  455 | `				sxi64 iOfft;` |
|      - |  456 | `				sxi64 nLen;` |
|    105 |  457 | `				if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  458 | `					/* Force an int cast */` |
|    ! 0 |  459 | `					PH7_MemObjToInteger(pKey);` |
|    ! 0 |  460 | `				}` |
|    105 |  461 | `				iOfft = pKey->x.iVal;` |
|    105 |  462 | `				nLen = (sxi64)SyBlobLength(&pObj->sBlob);` |
|    105 |  463 | `				if( iOfft < 0 ){` |
|      - |  464 | `					/* php 7.1: a negative offset writes back from the end. */` |
|      5 |  465 | `					iOfft += nLen;` |
|      5 |  466 | `					if( iOfft < 0 ){` |
|      4 |  467 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset %qd",` |
|      1 |  468 | `							pKey->x.iVal);` |
|      3 |  469 | `						PH7_MemObjRelease(pKey);` |
|      3 |  470 | `						VM_EXIT_BREAK;` |
|      - |  471 | `					}` |
|      1 |  472 | `				}` |
|    103 |  473 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    103 |  474 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|    103 |  475 | `					if( SyBlobLength(&pTos->sBlob) > 1 ){` |
|      3 |  476 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      - |  477 | `							"Only the first byte will be assigned to the string offset");` |
|      1 |  478 | `					}` |
|    103 |  479 | `					if( iOfft >= nLen ){` |
|      - |  480 | `						/* php PADS WITH SPACES up to the offset. PH7 simply appended the` |
|      - |  481 | `						 * byte, so "abc" with [6]="Z" became "abcZ" rather than "abc   Z"` |
|      - |  482 | `						 * -- a silently wrong string. */` |
|      - |  483 | `						sxi64 nPad;` |
|      9 |  484 | `						for( nPad = nLen ; nPad < iOfft ; ++nPad ){` |
|      7 |  485 | `							SyBlobAppend(&pObj->sBlob," ",sizeof(char));` |
|      4 |  486 | `						}` |
|      3 |  487 | `						SyBlobAppend(&pObj->sBlob,(const void *)zBlob,sizeof(char));` |
|      2 |  488 | `					}else{` |
|    101 |  489 | `						char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|    101 |  490 | `						zData[iOfft] = zBlob[0];` |
|      - |  491 | `					}` |
|     51 |  492 | `				}` |
|      - |  493 | `			}` |
|    103 |  494 | `			if( pKey ){` |
|    103 |  495 | `			  PH7_MemObjRelease(pKey);` |
|     51 |  496 | `			}` |
|    103 |  497 | `			VM_EXIT_BREAK;` |
|     32 |  498 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - |  499 | `			/* php: only NULL and FALSE auto-vivify into an array. Writing an index into an` |
|      - |  500 | `			 * int/float/resource/TRUE is a catchable Error -- PH7 quietly REPLACED the value` |
|      - |  501 | `			 * with an array, destroying it ($x = 5; $x[0] = 1; left $x === [1]). */` |
|      - |  502 | `			/* php auto-vivifies false into an array with an 8.1 DEPRECATION; PHL` |
|      - |  503 | `			 * rejects any scalar base, false included (null still auto-vivifies). */` |
|     32 |  504 | `			int bScalar = (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0;` |
|     32 |  505 | `			if( bScalar ){` |
|      - |  506 | `				sxi32 rcSc;` |
|      8 |  507 | `				if( pKey ){` |
|      5 |  508 | `					PH7_MemObjRelease(pKey);` |
|      2 |  509 | `				}` |
|      8 |  510 | `				VmPopOperand(&pTos,1);` |
|      8 |  511 | `				rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot use a scalar value as an array",` |
|      - |  512 | `					sizeof("Cannot use a scalar value as an array")-1);` |
|      8 |  513 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      8 |  514 | `				rc = rcSc;` |
|      8 |  515 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  516 | `			}` |
|      - |  517 | `			/* Force a hashmap cast  */` |
|     26 |  518 | `			rc = PH7_MemObjToHashmap(pObj);` |
|     26 |  519 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  520 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|    ! 0 |  521 | `				VM_EXIT_ABORT;` |
|      - |  522 | `			}` |
|     12 |  523 | `		}` |
|      - |  524 | `		/* COW separate the backing variable before mutation */` |
|     26 |  525 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|      - |  526 | `	}` |
| 243359 |  527 | `	VmPopOperand(&pTos,1);` |
|      - |  528 | `	/* Phase#2: Perform the insertion */` |
| 243359 |  529 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|     19 |  530 | `		if( pMap == pVm->pGlobal ){` |
|      - |  531 | `			/* php 8.1: $GLOBALS['y'] =& $x binds the global $y to $x's` |
|      - |  532 | `			 * slot; an append has no name to bind (catchable Error). */` |
|      3 |  533 | `			if( pKey == 0 ){` |
|    ! 0 |  534 | `				rc = PH7_VmThrowGlobalsAppendError(&(*pVm));` |
|    ! 0 |  535 | `			}else{` |
|      3 |  536 | `				if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 |  537 | `					PH7_MemObjToString(pKey);` |
|    ! 0 |  538 | `				}` |
|      3 |  539 | `				if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|      - |  540 | `					/* Pathological empty name: keep the legacy diagnostic */` |
|    ! 0 |  541 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|      - |  542 | `						"$GLOBALS is a read-only array,insertion is forbidden");` |
|    ! 0 |  543 | `					rc = SXRET_OK;` |
|    ! 0 |  544 | `				}else{` |
|      4 |  545 | `					rc = PH7_VmInstallGlobalVar(&(*pVm),` |
|      2 |  546 | `						(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|      2 |  547 | `						0,pTos->nIdx);` |
|      - |  548 | `				}` |
|      - |  549 | `			}` |
|      2 |  550 | `		}else{` |
|      - |  551 | `			/* Insertion by reference */` |
|     17 |  552 | `			rc = PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|      - |  553 | `		}` |
|     10 |  554 | `	}else{` |
|      - |  555 | `		/* By-value store: a null key deprecates + normalizes to "" here — AFTER a` |
|      - |  556 | ``		 * null/false container has auto-vivified to an array, so `$x[null]=v` on an`` |
|      - |  557 | `		 * undefined $x gets the notice too (the top-of-handler check runs before` |
|      - |  558 | `		 * vivification, when the base is not yet a hashmap). Gated off the by-ref` |
|      - |  559 | ``		 * op so the degenerate `=&`-with-no-source-slot path stays loud (§7). */`` |
| 243341 |  560 | `		if( pInstr->iOp != PH7_OP_STORE_IDX_REF ){` |
| 243341 |  561 | `			VmNullOffsetDeprecate(&(*pVm),pKey);` |
| 121668 |  562 | `		}` |
| 243341 |  563 | `		rc = PH7_HashmapInsert(pMap,pKey,pTos);` |
|      - |  564 | `	}` |
| 243359 |  565 | `	if( pKey ){` |
|  69333 |  566 | `		PH7_MemObjRelease(pKey);` |
|  34664 |  567 | `	}` |
|      - |  568 | `	/* An append onto the occupied saturated auto-index threw php's catchable` |
|      - |  569 | `	 * Error (PH7_VmThrowArrayNextIndexError) — dispatch it like any other` |
|      - |  570 | `	 * store-path throw. Plain failures (OOM) keep their existing routes. */` |
| 243359 |  571 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
| 243355 |  572 | `	VM_EXIT_BREAK;` |
|    ! 0 |  573 | `	VM_EXIT_BREAK;` |
| 121784 |  574 | `}` |
|      - |  575 |  |
|      - |  576 | `/*` |
|      - |  577 | ` * OP_LOAD_CLOSURE: body moved verbatim from the OP_LOAD_CLOSURE arm of` |
|      - |  578 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  579 | ` */` |
|   1214 |  580 | `PH7_PRIVATE VmOpRc VmExecOpLoadClosure(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  581 | `{` |
|   1219 |  582 | `	ph7_value *pTos = pState->pTos;` |
|   1219 |  583 | `	ph7_value *pStack = pState->pStack;` |
|   1219 |  584 | `	VmInstr *aInstr = pState->aInstr;` |
|   1219 |  585 | `	sxi32 pc = pState->pc;` |
|      - |  586 | `	sxi32 rc;` |
|    607 |  587 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   1219 |  588 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|      - |  589 | `	/* The function whose name the Closure object will wrap: a fresh per-instantiation` |
|      - |  590 | `	 * copy for a real closure (built below), or the shared lambda function itself for a` |
|      - |  591 | `	 * plain anonymous function with no captured environment. */` |
|   1219 |  592 | `	ph7_vm_func *pTarget = pFunc;` |
|      - |  593 | `	/* A no-capture lambda declared inside a class method is not VM_FUNC_CLOSURE` |
|      - |  594 | `	 * (its env is empty), yet php still binds the creation-site class as its scope` |
|      - |  595 | ``	 * so `self::`/private access inside the body works. Detect the enclosing class`` |
|      - |  596 | `	 * here and route such a lambda through the per-instance closure path (a global` |
|      - |  597 | `	 * no-capture lambda peeks NULL and stays a shared function). */` |
|   1219 |  598 | `	ph7_class *pLoadScope = (pFunc->iFlags & VM_FUNC_CLOSURE) ? 0 : PH7_VmPeekDeclaringClass(pVm);` |
|   1219 |  599 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) \|\| pLoadScope ){` |
|      - |  600 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|      - |  601 | `		ph7_vm_func *pClosure;` |
|      - |  602 | `		char *zName;` |
|      - |  603 | `		sxu32 mLen;` |
|      - |  604 | `		sxu32 n;` |
|      - |  605 | `		/* Create a new VM function */` |
|   1209 |  606 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|      - |  607 | `		/* Generate an unique closure name */` |
|   1209 |  608 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|   1209 |  609 | `		if( pClosure == 0 \|\| zName == 0){` |
|    ! 0 |  610 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|    ! 0 |  611 | `			VM_EXIT_ABORT;` |
|      - |  612 | `		}` |
|   1209 |  613 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|   1209 |  614 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|    ! 0 |  615 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|    ! 0 |  616 | `		}` |
|      - |  617 | `		/* Zero the stucture */` |
|   1209 |  618 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|      - |  619 | `		/* Perform a structure assignment on read-only items */` |
|   1209 |  620 | `		pClosure->aArgs = pFunc->aArgs;` |
|   1209 |  621 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|   1209 |  622 | `		pClosure->aStatic = pFunc->aStatic;` |
|   1209 |  623 | `		pClosure->iFlags = pFunc->iFlags;` |
|      - |  624 | `		/* An in-class no-capture lambda routed here (pLoadScope set) must be a` |
|      - |  625 | `		 * real closure so PH7_VmClassMemberAccess honors its pUserData scope. */` |
|   1209 |  626 | `		pClosure->iFlags \|= VM_FUNC_CLOSURE;` |
|   1209 |  627 | `		pClosure->pUserData = pFunc->pUserData;` |
|   1209 |  628 | `		pClosure->sSignature = pFunc->sSignature;` |
|   1209 |  629 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|   1209 |  630 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|   1209 |  631 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|   1209 |  632 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|   1209 |  633 | `		pClosure->bStrictTypes = pFunc->bStrictTypes;` |
|   1209 |  634 | `		pClosure->nMaxStack = pFunc->nMaxStack;` |
|   1209 |  635 | `		if( pClosure->pUserData == 0 ){` |
|      - |  636 | `			/* Stamp the creation-site class scope (see PH7_VmPeekDeclaringClass):` |
|      - |  637 | `			 * a closure made in a method — or in another closure, whose own stamp` |
|      - |  638 | `			 * the peek reads — resolves self::/parent:: against it like php. */` |
|   1209 |  639 | `			pClosure->pUserData = (void *)PH7_VmPeekDeclaringClass(pVm);` |
|    602 |  640 | `		}` |
|      - |  641 | ``		/* Capture the creation-site late-static-binding class so `static::` inside`` |
|      - |  642 | `		 * the closure body resolves like php (the "called class", which may differ` |
|      - |  643 | `		 * from the declaring scope stamped above — e.g. a closure made in an` |
|      - |  644 | `		 * inherited method). A closure made outside any class captures NULL. */` |
|   1209 |  645 | `		pClosure->pLsbClass = (void *)PH7_VmPeekTopClass(pVm);` |
|      - |  646 | `		/* Reflection descriptor fields (getDocComment/getAttributes/getStartLine/` |
|      - |  647 | `		 * getEndLine/getFileName read the INSTANTIATED copy via $__fn) */` |
|   1209 |  648 | `		pClosure->aAttrs = pFunc->aAttrs;` |
|   1209 |  649 | `		pClosure->sDoc = pFunc->sDoc;` |
|   1209 |  650 | `		pClosure->sFile = pFunc->sFile;` |
|   1209 |  651 | `		pClosure->nLine = pFunc->nLine;` |
|   1209 |  652 | `		pClosure->nEndLine = pFunc->nEndLine;` |
|   1209 |  653 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|      - |  654 | `		/* Register the closure */` |
|   1209 |  655 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|      - |  656 | `		/* Set up closure environment */` |
|   1209 |  657 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   1209 |  658 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|   2657 |  659 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|      - |  660 | `			ph7_value *pValue;` |
|   1453 |  661 | `			pEnv = &aEnv[n];` |
|   1453 |  662 | `			sEnv.sName  = pEnv->sName;` |
|   1453 |  663 | `			sEnv.iFlags = pEnv->iFlags;` |
|   1453 |  664 | `			sEnv.nLine = pEnv->nLine;` |
|   1453 |  665 | `			sEnv.nIdx = SXU32_HIGH;` |
|   1453 |  666 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|   1448 |  667 | `			if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == VM_FUNC_ARG_BY_REF` |
|    793 |  668 | `			 && !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|     64 |  669 | `				&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|      - |  670 | `				/* Capture by reference: bind the env entry to the variable's` |
|      - |  671 | `				 * memory slot — creating a fresh null variable when missing,` |
|      - |  672 | ``				 * as php does (`use (&$f)` before $f is assigned) — and pin`` |
|      - |  673 | `				 * the slot past the creating frame's teardown so the closure` |
|      - |  674 | `				 * can outlive its birth scope. The call-time env install` |
|      - |  675 | `				 * aliases the name to this slot instead of copying a value. */` |
|     97 |  676 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,TRUE);` |
|     97 |  677 | `				if( pValue ){` |
|     97 |  678 | `					sEnv.nIdx = pValue->nIdx;` |
|     97 |  679 | `					VmPinMemObjSlot(pVm,pValue->nIdx);` |
|     47 |  680 | `				}` |
|     50 |  681 | `			}else{` |
|      - |  682 | `				/* Standard pass by value */` |
|   1359 |  683 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|   1359 |  684 | `				if( pValue ){` |
|      - |  685 | `					/* Copy imported value */` |
|    187 |  686 | `					PH7_MemObjStore(pValue,&sEnv.sValue);` |
|   1268 |  687 | `				}else if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == 0` |
|    607 |  688 | `					&& !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|     16 |  689 | `						&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|     33 |  690 | `					if( pFunc->iFlags & VM_FUNC_ARROW ){` |
|      - |  691 | `						/* An arrow function auto-captures free variables by value, but` |
|      - |  692 | `						 * php does NOT capture one that is UNDEFINED at creation: the` |
|      - |  693 | `						 * isolated body scope then simply has no such variable, so a` |
|      - |  694 | `						 * read of it there raises the normal "Undefined variable"` |
|      - |  695 | `						 * warning (and reflection's getClosureUsedVariables omits it).` |
|      - |  696 | `						 * Skip installing the capture so the body READ — not the` |
|      - |  697 | `						 * creation — warns, matching php. (A later assignment to the` |
|      - |  698 | `						 * outer variable does not retro-capture: arrow scope is` |
|      - |  699 | `						 * isolated.) An explicit by-value use() instead warns here and` |
|      - |  700 | `						 * binds NULL, handled just below. */` |
|     23 |  701 | `						continue;` |
|      - |  702 | `					}` |
|      - |  703 | ``					/* php reads a by-value `use ($q)` capture AT CLOSURE CREATION and`` |
|      - |  704 | `					 * warns when the variable is undefined there (the by-ref form` |
|      - |  705 | ``					 * `use (&$q)` above stays silent — it creates the binding). The`` |
|      - |  706 | `					 * auto-injected $this (VM_FUNC_ARG_IGNORE) never warns. The` |
|      - |  707 | `					 * capture still proceeds as NULL, as php does. php attributes the` |
|      - |  708 | `					 * warning to the capture's own line (which can differ from the` |
|      - |  709 | `					 * OP_LOAD_CLOSURE instruction line when the use-clause wraps), so` |
|      - |  710 | `					 * borrow the recorded line for the emission and restore it. */` |
|     11 |  711 | `					sxu32 nSavedLine = pVm->nCurLine;` |
|     11 |  712 | `					if( sEnv.nLine ){` |
|     11 |  713 | `						pVm->nCurLine = sEnv.nLine;` |
|      5 |  714 | `					}` |
|     11 |  715 | `					VmErrorFormat(pVm,PH7_CTX_WARNING,"Undefined variable $%z",&sEnv.sName);` |
|     11 |  716 | `					pVm->nCurLine = nSavedLine;` |
|      5 |  717 | `				}` |
|      - |  718 | `			}` |
|      - |  719 | `			/* Insert the imported variable */` |
|   1433 |  720 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|    719 |  721 | `		}` |
|   1209 |  722 | `		pTarget = pClosure;` |
|    602 |  723 | `	}` |
|      - |  724 | `	/* Wrap the target function in a Closure object and push it. Its captured environment` |
|      - |  725 | ``	 * (incl. any `$this`) stays in pTarget->aClosureEnv and is delivered by the normal call`` |
|      - |  726 | `	 * path when the closure is dispatched by name. */` |
|   1219 |  727 | `	pTos++;` |
|      - |  728 | `	{` |
|   1219 |  729 | `		ph7_class_instance *pCloObj = VmCreateClosure(pVm, &pTarget->sName, 0, 0);` |
|   1219 |  730 | `		if( pCloObj ){` |
|   1219 |  731 | `			pCloObj->iRef++;` |
|   1219 |  732 | `			pTos->x.pOther = pCloObj;` |
|   1219 |  733 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|    612 |  734 | `		}else{` |
|      - |  735 | `			/* OOM fallback: the name string is still a usable callable. */` |
|    ! 0 |  736 | `			PH7_MemObjStringAppend(pTos, pTarget->sName.zString, pTarget->sName.nByte);` |
|      - |  737 | `		}` |
|      - |  738 | `	}` |
|   1219 |  739 | `	VM_EXIT_BREAK;` |
|    ! 0 |  740 | `	VM_EXIT_BREAK;` |
|    612 |  741 | `}` |
|      - |  742 |  |
|      - |  743 |  |
|      - |  744 | `/*` |
|      - |  745 | `` * TRUE when this LOAD_IDX feeds a `??`: the coalesce test is the very next`` |
|      - |  746 | `` * instruction. php evaluates the whole left operand of `??` in isset-context,`` |
|      - |  747 | ` * so the access must stay SILENT and, when it misses, must yield NULL — an` |
|      - |  748 | ``  * out-of-range string offset that yielded "" instead made `$s[99] ?? $d` `` |
|      - |  749 | ` * evaluate to "" rather than $d, a wrong answer rather than a stray notice.` |
|      - |  750 | ` */` |
|     16 |  751 | `static int VmIdxFeedsCoalesce(const VmInstr *pInstr)` |
|      3 |  752 | `{` |
|     19 |  753 | `	return (pInstr+1)->iOp == PH7_OP_NULLC \|\| (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|      3 |  754 | `}` |
|      - |  755 | `/*` |
|      - |  756 | ` * OP_LOAD_IDX: body moved verbatim from the OP_LOAD_IDX arm of` |
|      - |  757 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  758 | ` */` |
| 679575 |  759 | `PH7_PRIVATE VmOpRc VmExecOpLoadIdx(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  760 | `{` |
| 679580 |  761 | `	ph7_value *pTos = pState->pTos;` |
| 679580 |  762 | `	ph7_value *pStack = pState->pStack;` |
| 679580 |  763 | `	VmInstr *aInstr = pState->aInstr;` |
| 679580 |  764 | `	sxi32 pc = pState->pc;` |
|      - |  765 | `	sxi32 rc;` |
| 340157 |  766 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 679580 |  767 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
| 679580 |  768 | `	ph7_hashmap *pMap = 0;` |
|      - |  769 | `	ph7_value *pIdx;` |
| 679580 |  770 | `	pIdx = 0;` |
| 679580 |  771 | `	if( pInstr->iP1 == 0 ){` |
|      3 |  772 | `		if( !pInstr->iP2){` |
|      - |  773 | `			/* No available index,load NULL */` |
|    ! 0 |  774 | `			if( pTos >= pStack ){` |
|    ! 0 |  775 | `				PH7_MemObjRelease(pTos);` |
|    ! 0 |  776 | `			}else{` |
|      - |  777 | `				/* TICKET 1433-020: Empty stack */` |
|    ! 0 |  778 | `				pTos++;` |
|    ! 0 |  779 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  780 | `				pTos->nIdx = SXU32_HIGH;` |
|      - |  781 | `			}` |
|      - |  782 | `			/* Emit a notice */` |
|    ! 0 |  783 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|      - |  784 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|    ! 0 |  785 | `			VM_EXIT_BREAK;` |
|      - |  786 | `		}` |
|      2 |  787 | `	}else{` |
| 679578 |  788 | `		pIdx = pTos;` |
| 679578 |  789 | `		pTos--;` |
|      - |  790 | `	}` |
| 679580 |  791 | `	if( pInstr->iP2 == 7 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - |  792 | ``		/* Keyed list destructuring `["k"=>$v] = $src` from a NON-array source: yield NULL`` |
|      - |  793 | `		 * (never char-index a string), warning once per key — matching PHP, which warns per` |
|      - |  794 | `		 * key here. A NULL source is silent; unlike the positional OP_LOAD_LIST path, a bool` |
|      - |  795 | `		 * source DOES warn (PHP warns for bool in keyed destructuring). */` |
|      7 |  796 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      5 |  797 | `			VmWarnCannotUseAsArray(&(*pVm),pTos->iFlags);` |
|      2 |  798 | `		}` |
|      7 |  799 | `		if( pIdx ){` |
|      - |  800 | `			/* Release the key (a string literal for keyed destructuring), like the` |
|      - |  801 | `			 * normal hashmap-read exit below — otherwise its blob is orphaned. */` |
|      7 |  802 | `			PH7_MemObjRelease(pIdx);` |
|      3 |  803 | `		}` |
|      7 |  804 | `		PH7_MemObjRelease(pTos);` |
|      7 |  805 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|      7 |  806 | `		VM_EXIT_BREAK;` |
|      - |  807 | `	}` |
| 679574 |  808 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|      - |  809 | `		/* String access */` |
| 532810 |  810 | `		if( pIdx ){` |
|      - |  811 | `			sxi64 iOfft;` |
| 532810 |  812 | `			sxi64 nLen = (sxi64)SyBlobLength(&pTos->sBlob);` |
| 532810 |  813 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  814 | `				/* Force an int cast */` |
|    ! 0 |  815 | `				PH7_MemObjToInteger(pIdx);` |
|    ! 0 |  816 | `			}` |
| 532810 |  817 | `			iOfft = pIdx->x.iVal;` |
|      - |  818 | `			/* php 7.1: a NEGATIVE offset counts back from the end ($s[-1] is the last` |
|      - |  819 | `			 * character). The offset used to be cast to UNSIGNED, so -1 became a huge` |
|      - |  820 | `			 * number, ran past the end and quietly produced NULL. */` |
| 532810 |  821 | `			if( iOfft < 0 ){` |
|      7 |  822 | `				iOfft += nLen;` |
|      3 |  823 | `			}` |
| 532814 |  824 | `			if( iOfft < 0 \|\| iOfft >= nLen ){` |
|      - |  825 | `				/* Out of range. In an isset()/empty() lookup php answers FALSE, so load` |
|      - |  826 | `				 * NULL there; everywhere else it WARNS and yields the empty string (PH7` |
|      - |  827 | `				 * silently produced NULL in both cases). */` |
|      - |  828 | `				/* LOAD_IDX carries its OWN iP2 codes, which do NOT line up with the` |
|      - |  829 | `				 * PH7_MEMBER_* ones: 4 = isset, 5 = unset, 6 = empty. All three are` |
|      - |  830 | `				 * lookups and must stay silent. */` |
|     11 |  831 | `				int bQuiet = pInstr->iP2 == 4 \|\| pInstr->iP2 == 5 \|\| pInstr->iP2 == 6` |
|     11 |  832 | `					\|\| pInstr->iP2 == 8 \|\| VmIdxFeedsCoalesce(pInstr);` |
|     10 |  833 | `				PH7_MemObjRelease(pTos);` |
|     10 |  834 | `				if( bQuiet ){` |
|      8 |  835 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  836 | `				}else{` |
|      3 |  837 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|      4 |  838 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Uninitialized string offset %qd",` |
|      1 |  839 | `						pIdx->x.iVal);` |
|      - |  840 | `				}` |
|      6 |  841 | `			}else{` |
| 532802 |  842 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
| 532802 |  843 | `				int c = zData[iOfft];` |
| 532802 |  844 | `				PH7_MemObjRelease(pTos);` |
| 532802 |  845 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
| 532802 |  846 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|      - |  847 | `			}` |
| 266781 |  848 | `		}else{` |
|      - |  849 | `			/* No available index,load NULL */` |
|    ! 0 |  850 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      - |  851 | `		}` |
| 532810 |  852 | `		VM_EXIT_BREAK;` |
|      - |  853 | `	}` |
| 146769 |  854 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      - |  855 | `		/* Object subscript: ArrayAccess dispatch.` |
|      - |  856 | `		 * iP2 codes:` |
|      - |  857 | `		 *   0 = read       → offsetGet` |
|      - |  858 | `		 *   3 = ??= peek   → offsetExists; offsetGet on hit; arm coalesce` |
|      - |  859 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|      - |  860 | `		 *   4 = isset()    → offsetExists` |
|      - |  861 | `		 *   5 = unset()    → offsetUnset` |
|      - |  862 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit` |
|      - |  863 | ``		 *   8 = `??` read  → same probe as 6 (offsetExists, then offsetGet on a`` |
|      - |  864 | `		 *                    hit) and no diagnostics anywhere in this op: php` |
|      - |  865 | ``		 *                    evaluates the whole left operand of `??` in`` |
|      - |  866 | `		 *                    isset-context, and calling offsetGet blindly also` |
|      - |  867 | `		 *                    surfaced warnings raised INSIDE a userland` |
|      - |  868 | `		 *                    offsetGet (e.g. ArrayObject's own array read). */` |
|    183 |  869 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|    183 |  870 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|    183 |  871 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|      - |  872 | `			ph7_class_method *pMeth;` |
|      - |  873 | `			ph7_value sResult;` |
|      - |  874 | `			ph7_value *apArg[1];` |
|    181 |  875 | `			if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 8) && pIdx == 0 ){` |
|      - |  876 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|    ! 0 |  877 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|      - |  878 | `					"Cannot use [] for reading");` |
|    ! 0 |  879 | `				PH7_MemObjRelease(pTos);` |
|    ! 0 |  880 | `				pTos->nIdx = SXU32_HIGH;` |
|    ! 0 |  881 | `				VM_EXIT_BREAK;` |
|      - |  882 | `			}` |
|    181 |  883 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|    181 |  884 | `			if( pInstr->iP2 == 4 \|\| pInstr->iP2 == 6 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 8 ){` |
|      - |  885 | ``				/* isset, empty, ??= and `??` all start with offsetExists. */`` |
|     81 |  886 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  887 | `					"offsetExists",sizeof("offsetExists")-1);` |
|     81 |  888 | `				apArg[0] = pIdx;` |
|     81 |  889 | `				if( pMeth ){` |
|     81 |  890 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     43 |  891 | `				}` |
|    143 |  892 | `			}else if( pInstr->iP2 == 5 ){` |
|     20 |  893 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  894 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|     20 |  895 | `				apArg[0] = pIdx;` |
|     20 |  896 | `				if( pMeth ){` |
|     20 |  897 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      8 |  898 | `				}` |
|     12 |  899 | `			}else{` |
|     89 |  900 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  901 | `					"offsetGet",sizeof("offsetGet")-1);` |
|     89 |  902 | `				apArg[0] = pIdx;` |
|     89 |  903 | `				if( pMeth ){` |
|     89 |  904 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     42 |  905 | `				}` |
|      - |  906 | `			}` |
|    181 |  907 | `			if( pInstr->iP2 == 4 ){` |
|      - |  908 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|      - |  909 | `				 * right truth value AND skips its "Expecting a variable not` |
|      - |  910 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|     53 |  911 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     53 |  912 | `				PH7_MemObjRelease(pTos);` |
|     53 |  913 | `				pTos->nIdx = SXU32_HIGH;` |
|     53 |  914 | `				if( bExists ){` |
|     28 |  915 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     28 |  916 | `					pTos->x.iVal = 1;` |
|     16 |  917 | `				}else{` |
|     29 |  918 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  919 | `				}` |
|    157 |  920 | `			}else if( pInstr->iP2 == 5 ){` |
|      - |  921 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|      - |  922 | `				 * vm_builtin_unset is a harmless no-op. */` |
|     20 |  923 | `				PH7_MemObjRelease(pTos);` |
|     20 |  924 | `				pTos->nIdx = SXU32_HIGH;` |
|     20 |  925 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    125 |  926 | `			}else if( pInstr->iP2 == 6 \|\| pInstr->iP2 == 8 ){` |
|      - |  927 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|      - |  928 | `				 * without calling offsetGet. If true, call offsetGet and` |
|      - |  929 | `				 * push the value so PH7_builtin_empty evaluates emptiness.` |
|      - |  930 | ``				 * `??` (8) needs the identical shape: NULL on a miss so the`` |
|      - |  931 | `				 * coalesce takes the default, the real value on a hit. */` |
|     22 |  932 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     22 |  933 | `				PH7_MemObjRelease(&sResult);` |
|     22 |  934 | `				PH7_MemObjRelease(pTos);` |
|     22 |  935 | `				pTos->nIdx = SXU32_HIGH;` |
|     22 |  936 | `				if( !bExists ){` |
|      8 |  937 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  938 | `				}else{` |
|     16 |  939 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  940 | `						"offsetGet",sizeof("offsetGet")-1);` |
|      - |  941 | `					ph7_value sValue;` |
|     16 |  942 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|     16 |  943 | `					apArg[0] = pIdx;` |
|     16 |  944 | `					if( pGet ){` |
|     16 |  945 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      7 |  946 | `					}` |
|     16 |  947 | `					PH7_MemObjStore(&sValue,pTos);` |
|     16 |  948 | `					PH7_MemObjRelease(&sValue);` |
|      - |  949 | `				}` |
|     22 |  950 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|     22 |  951 | `				VM_EXIT_BREAK; /* skip the duplicate sResult release below */` |
|     97 |  952 | `			}else if( pInstr->iP2 == 3 ){` |
|      - |  953 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|      - |  954 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|      - |  955 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|      - |  956 | `				 *     and push NULL.` |
|      - |  957 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|     10 |  958 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     10 |  959 | `				int bShouldArm = !bExists;` |
|      - |  960 | `				ph7_value sValue;` |
|     10 |  961 | `				PH7_MemObjRelease(&sResult);` |
|      - |  962 | `				/* Reset any prior arming defensively */` |
|     10 |  963 | `				VmCoalesceDisarm(pVm);` |
|     10 |  964 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|     10 |  965 | `				if( bExists ){` |
|      5 |  966 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  967 | `						"offsetGet",sizeof("offsetGet")-1);` |
|      5 |  968 | `					apArg[0] = pIdx;` |
|      5 |  969 | `					if( pGet ){` |
|      5 |  970 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      2 |  971 | `					}` |
|      5 |  972 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|      3 |  973 | `						bShouldArm = 1;` |
|      1 |  974 | `					}` |
|      2 |  975 | `				}` |
|     10 |  976 | `				PH7_MemObjRelease(pTos);` |
|     10 |  977 | `				pTos->nIdx = SXU32_HIGH;` |
|     10 |  978 | `				if( bShouldArm ){` |
|      - |  979 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|      - |  980 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|      - |  981 | `					 * intervening expression evaluation. */` |
|      8 |  982 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      8 |  983 | `					if( pIdx ){` |
|      8 |  984 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|      3 |  985 | `					}` |
|      8 |  986 | `					pVm->pCoalesceObj = pInst;` |
|      8 |  987 | `					pInst->iRef++;` |
|      8 |  988 | `					pVm->bCoalesceArmed = 1;` |
|      5 |  989 | `				}else{` |
|      3 |  990 | `					PH7_MemObjStore(&sValue,pTos);` |
|      - |  991 | `				}` |
|     10 |  992 | `				PH7_MemObjRelease(&sValue);` |
|     10 |  993 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|     10 |  994 | `				VM_EXIT_BREAK;` |
|    ! 0 |  995 | `			}else{` |
|      - |  996 | `				/* offsetGet: replace pTos with the returned value. */` |
|     89 |  997 | `				PH7_MemObjRelease(pTos);` |
|     89 |  998 | `				PH7_MemObjStore(&sResult,pTos);` |
|     89 |  999 | `				pTos->nIdx = SXU32_HIGH;` |
|      - | 1000 | `			}` |
|    153 | 1001 | `			PH7_MemObjRelease(&sResult);` |
|    153 | 1002 | `			if( pIdx ){` |
|    153 | 1003 | `				PH7_MemObjRelease(pIdx);` |
|     74 | 1004 | `			}` |
|    153 | 1005 | `			VM_EXIT_BREAK;` |
|      - | 1006 | `		}` |
|      - | 1007 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|      - | 1008 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|      3 | 1009 | `		if( pInst ){` |
|      - | 1010 | `			char zMsg[256];` |
|      3 | 1011 | `			SyString *pName = &pInst->pClass->sName;` |
|      4 | 1012 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 1013 | `				"Cannot use object of type %.*s as array",` |
|      2 | 1014 | `				(int)pName->nByte,pName->zString);` |
|      3 | 1015 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      3 | 1016 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      3 | 1017 | `			PH7_MemObjRelease(pTos);` |
|      3 | 1018 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 | 1019 | `			pTos->nIdx = SXU32_HIGH;` |
|      3 | 1020 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      - | 1021 | ``			/* `break` used to resume at the NEXT instruction: the catch ran and then`` |
|      - | 1022 | `			 * execution carried on inside the try block. */` |
|      3 | 1023 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - | 1024 | `		}` |
|    ! 0 | 1025 | `	}` |
| 146591 | 1026 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     23 | 1027 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|      - | 1028 | `			ph7_value *pObj;` |
|     23 | 1029 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      - | 1030 | `				/* php 8 write-context auto-vivify rules: NULL converts to array` |
|      - | 1031 | `				 * silently; FALSE converts with the 8.1 deprecation; any other` |
|      - | 1032 | `				 * scalar base — int/float/true/resource — is php's catchable` |
|      - | 1033 | `				 * "Cannot use a scalar value as an array" Error and the variable` |
|      - | 1034 | `				 * stays untouched (pre-fix the base was silently CONVERTED,` |
|      - | 1035 | ``				 * corrupting e.g. `$i = 5; $i[0]++` into array(0 => 6); string`` |
|      - | 1036 | `				 * bases were intercepted by the string-offset paths above). */` |
|      - | 1037 | `				/* php auto-converts false to an array with an 8.1 DEPRECATION; PHL` |
|      - | 1038 | `				 * rejects it like any other scalar base (null still auto-vivifies —` |
|      - | 1039 | `				 * it is not a bool). */` |
|     23 | 1040 | `				if( (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0 ){` |
|      - | 1041 | `					SyBlob sErrMsg;` |
|      7 | 1042 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      7 | 1043 | `					SyBlobAppend(&sErrMsg,"Cannot use a scalar value as an array",` |
|      - | 1044 | `						sizeof("Cannot use a scalar value as an array")-1);` |
|      7 | 1045 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      7 | 1046 | `					if( pIdx ){` |
|      7 | 1047 | `						PH7_MemObjRelease(pIdx);` |
|      3 | 1048 | `					}` |
|      7 | 1049 | `					PH7_MemObjRelease(pTos);` |
|      7 | 1050 | `					pTos->nIdx = SXU32_HIGH;` |
|      7 | 1051 | `					VM_EXIT_BREAK;` |
|      - | 1052 | `				}` |
|     17 | 1053 | `				PH7_MemObjToHashmap(pObj);` |
|     17 | 1054 | `				PH7_MemObjLoad(pObj,pTos);` |
|      8 | 1055 | `			}` |
|      8 | 1056 | `		}` |
|      8 | 1057 | `	}` |
| 146585 | 1058 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|      - | 1059 | `	/* php DEPRECATES both a null offset and a lossy-float subscript (then normalizes` |
|      - | 1060 | `	 * "" / truncates). PHL now matches php on the NULL offset (deprecate + coerce to` |
|      - | 1061 | `	 * the "" key, §2) but still rejects the lossy-FLOAT subscript with a TypeError on` |
|      - | 1062 | `	 * a READ or WRITE (iP2 0/1) — the recorded non-deprecated-surface policy. Both` |
|      - | 1063 | ``	 * stay lenient in isset()/empty()/`??`/unset(), where a throw would be wrong. */`` |
|      - | 1064 | `	/* An object/array key is rejected in EVERY context, including isset()/empty()/` |
|      - | 1065 | `	 * unset() where php still throws (only the wording changes) — unlike the` |
|      - | 1066 | `	 * null/float deprecations below, which stay lenient there. A resource key is` |
|      - | 1067 | `	 * accepted with a warning and becomes its integer id. */` |
| 146585 | 1068 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|      - | 1069 | `		SyBlob sTypeMsg;` |
| 146575 | 1070 | `		if( VmOffsetTypeRejected(&(*pVm),pIdx,pInstr->iP2,&sTypeMsg) ){` |
|     13 | 1071 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg));` |
|     13 | 1072 | `			PH7_MemObjRelease(pIdx);` |
|     13 | 1073 | `			PH7_MemObjRelease(pTos);` |
|     13 | 1074 | `			pTos->nIdx = SXU32_HIGH;` |
|     13 | 1075 | `			VM_EXIT_BREAK;` |
|      - | 1076 | `		}` |
| 146563 | 1077 | `		VmOffsetResourceWarn(&(*pVm),pIdx);` |
|  73279 | 1078 | `	}` |
| 146573 | 1079 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|      - | 1080 | `		/* php DEPRECATES a null offset in EVERY subscript context except unset()` |
|      - | 1081 | `		 * (iP2==5), then normalizes it to the "" key — read, write, isset (4),` |
|      - | 1082 | ``		 * empty (6), `??`/`??=` (3), the coalesce-chain probe (8), destructure`` |
|      - | 1083 | `		 * (2/7). Emit it here so all of them get it, not just read/write; the` |
|      - | 1084 | `		 * lookup/insert below casts NULL->"", and a plain read miss then warns` |
|      - | 1085 | ``		 * `Undefined array key ""` on the now-string pIdx (php's exact pair). */`` |
| 146563 | 1086 | `		if( (pIdx->iFlags & MEMOBJ_NULL) && pInstr->iP2 != 5 ){` |
|     15 | 1087 | `			VmNullOffsetDeprecate(&(*pVm),pIdx);` |
|      7 | 1088 | `		}` |
|      - | 1089 | `		/* A lossy-FLOAT subscript stays a rejected TypeError on a READ or WRITE` |
|      - | 1090 | `		 * (iP2 0/1) — the recorded non-deprecated-surface policy (§2); the lenient` |
|      - | 1091 | `		 * contexts (isset/empty/??/unset) truncate quietly as php's value does. */` |
| 146558 | 1092 | `		if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 1)` |
|  90247 | 1093 | `		 && (pIdx->iFlags & MEMOBJ_REAL)` |
|  73287 | 1094 | `		 && pIdx->rVal != (ph7_real)(sxi64)pIdx->rVal ){` |
|      - | 1095 | `			SyBlob sErrMsg;` |
|      3 | 1096 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      3 | 1097 | `			SyBlobAppend(&sErrMsg,"Cannot access offset of type float on array",` |
|      - | 1098 | `				sizeof("Cannot access offset of type float on array")-1);` |
|      3 | 1099 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|      3 | 1100 | `			PH7_MemObjRelease(pIdx);` |
|      3 | 1101 | `			PH7_MemObjRelease(pTos);` |
|      3 | 1102 | `			pTos->nIdx = SXU32_HIGH;` |
|      3 | 1103 | `			VM_EXIT_BREAK;` |
|      - | 1104 | `		}` |
|  73278 | 1105 | `	}` |
| 146571 | 1106 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
| 146563 | 1107 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|      - | 1108 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|      - | 1109 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|      - | 1110 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|      - | 1111 | `			 * NOT separate — that would defeat COW on every element read.` |
|      - | 1112 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|      - | 1113 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|   1267 | 1114 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|    631 | 1115 | `		}` |
|      - | 1116 | `		/* Point to the hashmap */` |
| 146563 | 1117 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
| 146563 | 1118 | `		if( pIdx ){` |
|      - | 1119 | `			/* Load the desired entry */` |
| 146561 | 1120 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|  73278 | 1121 | `		}` |
| 146563 | 1122 | `		if( pInstr->iP2 == 3 ){` |
|      - | 1123 | `			/* Null coalescing assign peek mode: separate only when we will` |
|      - | 1124 | `			 * actually write back. If the looked-up value is non-null, the` |
|      - | 1125 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|      - | 1126 | `			 * the parent can stay shared. If the value is null or the key is` |
|      - | 1127 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|      - | 1128 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|      - | 1129 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|      - | 1130 | `			 * correct for the outermost write. */` |
|     21 | 1131 | `			int needWrite = (rc != SXRET_OK);` |
|     21 | 1132 | `			if( !needWrite && pNode ){` |
|     13 | 1133 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|     13 | 1134 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|      7 | 1135 | `					needWrite = 1;` |
|      3 | 1136 | `				}` |
|      6 | 1137 | `			}` |
|     21 | 1138 | `			if( needWrite ){` |
|     15 | 1139 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|     15 | 1140 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|      - | 1141 | `					/* The map was actually copied — re-lookup so pNode points` |
|      - | 1142 | `					 * into the new map's storage. */` |
|      7 | 1143 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      7 | 1144 | `					if( pIdx ){` |
|      7 | 1145 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|      3 | 1146 | `					}` |
|      3 | 1147 | `				}` |
|      7 | 1148 | `			}` |
|     10 | 1149 | `		}` |
| 146563 | 1150 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|      - | 1151 | `			/* Create a new empty entry */` |
|    324 | 1152 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|    324 | 1153 | `			if( rc == SXRET_OK ){` |
|      - | 1154 | `				/* Point to the last inserted entry */` |
|    321 | 1155 | `				pNode = pMap->pLast;` |
|    161 | 1156 | `			}else{` |
|      - | 1157 | ``				/* An append lvalue (`$a[][...] = v`) whose saturated auto-index`` |
|      - | 1158 | `				 * is occupied threw php's catchable Error. Dispatch it here —` |
|      - | 1159 | `				 * falling through with a stale pMap->pLast is what silently` |
|      - | 1160 | `				 * overwrote $a[PHP_INT_MAX]. */` |
|      3 | 1161 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|      - | 1162 | `			}` |
|    160 | 1163 | `		}` |
|  73278 | 1164 | `	}` |
| 146564 | 1165 | `	if( rc != SXRET_OK && pIdx && (pInstr->iP2 == 2 \|\| pInstr->iP2 == 0)` |
|  40442 | 1166 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP)` |
|      9 | 1167 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|      - | 1168 | ``		/* `$a['k'] ?? $d` compiles its LHS as a plain read (iP2 == 0) followed`` |
|      - | 1169 | `		 * by NULLC/NULLC_JMP — php does NOT warn there, so peek ahead and stay` |
|      - | 1170 | `		 * silent (same guard the magic-accessor read path uses). */` |
|      - | 1171 | `		/* php warns when a missing key is READ (iP2 == 0) or destructured` |
|      - | 1172 | `		 * (iP2 == 2). isset/empty/??/unset (iP2 3-6) and write-context` |
|      - | 1173 | `		 * vivification (iP2 == 1) stay silent, as does a read on a non-array` |
|      - | 1174 | `		 * base (already diagnosed above). php prints an INT key bare and a` |
|      - | 1175 | `		 * STRING key quoted. */` |
|      - | 1176 | `		SyBlob sMsg;` |
|     10 | 1177 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     10 | 1178 | `		if( pIdx->iFlags & (MEMOBJ_STRING\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|      - | 1179 | `			SyString sKey;` |
|      8 | 1180 | `			if( (pIdx->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 1181 | `				PH7_MemObjToString(pIdx);` |
|    ! 0 | 1182 | `			}` |
|      8 | 1183 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|      8 | 1184 | `			SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);` |
|      5 | 1185 | `		}else{` |
|      3 | 1186 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 | 1187 | `				PH7_MemObjToInteger(pIdx);` |
|    ! 0 | 1188 | `			}` |
|      3 | 1189 | `			SyBlobFormat(&sMsg,"Undefined array key %qd",pIdx->x.iVal);` |
|      - | 1190 | `		}` |
|     10 | 1191 | `		SyBlobNullAppend(&sMsg);` |
|     10 | 1192 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|     10 | 1193 | `		SyBlobRelease(&sMsg);` |
|      4 | 1194 | `	}` |
| 146572 | 1195 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0` |
|  73294 | 1196 | `	 && (pInstr->iP2 == 0 \|\| pInstr->iP2 == 2)` |
|     16 | 1197 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|      - | 1198 | `		/* Subscripting a scalar base is a WARNING in php ("Trying to access array offset` |
|      - | 1199 | `		 * on int") that yields NULL. PH7 yielded NULL in silence. */` |
|     11 | 1200 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Trying to access array offset on %s",` |
|      3 | 1201 | `			VmArithTypeName(pTos));` |
|      3 | 1202 | `	}` |
| 146577 | 1203 | `	if( pIdx ){` |
| 146577 | 1204 | `		PH7_MemObjRelease(pIdx);` |
|  73286 | 1205 | `	}` |
| 146577 | 1206 | `	if( rc == SXRET_OK ){` |
|      - | 1207 | `		/* Load entry contents */` |
|  65707 | 1208 | `		if( pMap->iRef < 2 ){` |
|      - | 1209 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|      - | 1210 | `			 * of the entry value,rather than pointing to it.` |
|      - | 1211 | `			 */` |
|    117 | 1212 | `			pTos->nIdx = SXU32_HIGH;` |
|    117 | 1213 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|     60 | 1214 | `		}else{` |
|  65593 | 1215 | `			pTos->nIdx = pNode->nValIdx;` |
|  65593 | 1216 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|  65593 | 1217 | `			PH7_HashmapUnref(pMap);` |
|      - | 1218 | `		}` |
|  32856 | 1219 | `	}else{` |
|      - | 1220 | `		/* No such entry,load NULL */` |
|  80875 | 1221 | `		PH7_MemObjRelease(pTos);` |
|  80875 | 1222 | `		pTos->nIdx = SXU32_HIGH;` |
|      - | 1223 | `	}` |
| 146577 | 1224 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1225 | `	VM_EXIT_BREAK;` |
| 340170 | 1226 | `}` |
|      - | 1227 |  |
|      - | 1228 | `/*` |
|      - | 1229 | ` * OP_LOAD_MAP: body moved verbatim from the OP_LOAD_MAP arm of` |
|      - | 1230 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1231 | ` */` |
|  74044 | 1232 | `PH7_PRIVATE VmOpRc VmExecOpLoadMap(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1233 | `{` |
|  74049 | 1234 | `	ph7_value *pTos = pState->pTos;` |
|  74049 | 1235 | `	ph7_value *pStack = pState->pStack;` |
|  74049 | 1236 | `	VmInstr *aInstr = pState->aInstr;` |
|  74049 | 1237 | `	sxi32 pc = pState->pc;` |
|      - | 1238 | `	sxi32 rc;` |
|  37022 | 1239 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1240 | `	ph7_hashmap *pMap;` |
|      - | 1241 | `	/* Allocate a new hashmap instance */` |
|  74049 | 1242 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|  74049 | 1243 | `	if( pMap == 0 ){` |
|    ! 0 | 1244 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|    ! 0 | 1245 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|    ! 0 | 1246 | `		VM_EXIT_ABORT;` |
|      - | 1247 | `	}` |
|  74049 | 1248 | `	if( pInstr->iP1 > 0 ){` |
|  11043 | 1249 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|  11043 | 1250 | `		sxi32 rcSpread = SXRET_OK;` |
|      - | 1251 | `		/* Perform the insertion */` |
|  41877 | 1252 | `		while( pEntry < pTos ){` |
|  30857 | 1253 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|      - | 1254 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|      - | 1255 | `				 * semantics — string keys preserved (later wins), int keys` |
|      - | 1256 | `				 * renumbered. Same routine that backs array_merge. */` |
|    683 | 1257 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|    659 | 1258 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|    659 | 1259 | `					if( rcMerge != SXRET_OK ){` |
|      - | 1260 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|      - | 1261 | `						 * path — emit fatal and abort, leaving no partial` |
|      - | 1262 | `						 * map dangling. */` |
|    ! 0 | 1263 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|    ! 0 | 1264 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|    ! 0 | 1265 | `						rcSpread = PH7_ABORT;` |
|    ! 0 | 1266 | `						break;` |
|      1 | 1267 | `					}` |
|    354 | 1268 | `				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){` |
|      - | 1269 | `					/* Traversable unpacking (PHP 8.1): walk it into the map using the` |
|      - | 1270 | `					 * same key rules as array spread (string keys kept, int renumbered). */` |
|      5 | 1271 | `					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);` |
|      5 | 1272 | `					if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|    ! 0 | 1273 | `						rcSpread = rcW;` |
|    ! 0 | 1274 | `						break;` |
|      - | 1275 | `					}` |
|      3 | 1276 | `				}else{` |
|      - | 1277 | `					/* Throw a catchable Error matching PHP semantics. */` |
|     21 | 1278 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|     21 | 1279 | `					break;` |
|      1 | 1280 | `				}` |
|  30508 | 1281 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|      - | 1282 | `				/* Insertion by reference */` |
|    181 | 1283 | `				PH7_HashmapInsertByRef(pMap,` |
|    120 | 1284 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|    120 | 1285 | `					(sxu32)pEntry[1].x.iVal` |
|      - | 1286 | `					);` |
|     61 | 1287 | `			}else{` |
|      - | 1288 | `				/* An explicit key in an array LITERAL gets the same php diagnostics a` |
|      - | 1289 | `				 * subscript does — a float key that truncates deprecates, and so does an` |
|      - | 1290 | `				 * explicit null key. Only the subscript sites (LOAD_IDX/STORE_IDX) used to` |
|      - | 1291 | ``				 * emit these, so `[1.5 => "v"]` and `[null => "v"]` were silent. A key that`` |
|      - | 1292 | `				 * is ABSENT (auto-index) is a NULL slot here, not a null key, so the` |
|      - | 1293 | `				 * MEMOBJ_NULL check below is what tells the two apart. */` |
|  30057 | 1294 | `					if( (pEntry->iFlags & MEMOBJ_AUX_NOKEY) == 0 ){` |
|      - | 1295 | `						/* An object/array literal key is php's TypeError, a resource one` |
|      - | 1296 | `						 * warns and becomes its id — same rules as a subscript. */` |
|      - | 1297 | `						SyBlob sTypeMsg;` |
|  11235 | 1298 | `						if( VmOffsetTypeRejected(&(*pVm),pEntry,0,&sTypeMsg) ){` |
|      3 | 1299 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg));` |
|      2 | 1300 | `						}else{` |
|  11233 | 1301 | `							VmOffsetResourceWarn(&(*pVm),pEntry);` |
|      - | 1302 | `						}` |
|      - | 1303 | `						/* php DEPRECATES a null literal key (then normalizes to "") and` |
|      - | 1304 | `						 * rejects nothing there; PHL matches that (deprecate + fall` |
|      - | 1305 | `						 * through, PH7_HashmapInsert casts NULL->""). A lossy-FLOAT` |
|      - | 1306 | `						 * literal key still rejects with a TypeError — the recorded` |
|      - | 1307 | `						 * non-deprecated-surface policy, same as the subscript site. */` |
|  11235 | 1308 | `						int bNull = (pEntry->iFlags & MEMOBJ_NULL) != 0;` |
|  16851 | 1309 | `						int bLossyFloat = (pEntry->iFlags & MEMOBJ_REAL) != 0` |
|  11230 | 1310 | `							&& pEntry->rVal != (ph7_real)(sxi64)pEntry->rVal;` |
|  11235 | 1311 | `						if( bNull ){` |
|      3 | 1312 | `							VmNullOffsetDeprecate(&(*pVm),pEntry);` |
|  11234 | 1313 | `						}else if( bLossyFloat ){` |
|      3 | 1314 | `							const char *zErr = "Cannot access offset of type float on array";` |
|      - | 1315 | `							SyBlob sErrMsg;` |
|      3 | 1316 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      3 | 1317 | `							SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|      3 | 1318 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|      1 | 1319 | `						}` |
|   5615 | 1320 | `					}` |
|      - | 1321 | `				/* Standard insertion */` |
|  45083 | 1322 | `				PH7_HashmapInsert(pMap,` |
|  30052 | 1323 | `					(pEntry->iFlags & MEMOBJ_AUX_NOKEY) ? 0 /* Automatic index assign */ : pEntry,` |
|  15026 | 1324 | `					&pEntry[1]` |
|      - | 1325 | `				);` |
|      - | 1326 | `			}` |
|      - | 1327 | `			/* Next pair on the stack */` |
|  30839 | 1328 | `			pEntry += 2;` |
|      5 | 1329 | `		}` |
|      - | 1330 | `		/* Pop P1 elements */` |
|  11043 | 1331 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|  11043 | 1332 | `		if( rcSpread != SXRET_OK ){` |
|      - | 1333 | `			/* Discard the partially-built map and propagate the exception. */` |
|     21 | 1334 | `			PH7_HashmapRelease(pMap,TRUE);` |
|     21 | 1335 | `			if( rcSpread == PH7_ABORT ){` |
|    ! 0 | 1336 | `				VM_EXIT_ABORT;` |
|      - | 1337 | `			}` |
|      - | 1338 | `			{` |
|      - | 1339 | `				sxi32 iRp;` |
|     21 | 1340 | `				if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      6 | 1341 | `					pc = iRp;` |
|      6 | 1342 | `					VM_EXIT_BREAK;` |
|      - | 1343 | `				}` |
|      - | 1344 | `			}` |
|     15 | 1345 | `			VM_EXIT_EXCEPTION;` |
|      - | 1346 | `		}` |
|   5510 | 1347 | `	}` |
|      - | 1348 | `	/* Push the hashmap */` |
|  74031 | 1349 | `	pTos++;` |
|  74031 | 1350 | `	pTos->nIdx = SXU32_HIGH;` |
|  74031 | 1351 | `	pTos->x.pOther = pMap;` |
|  74031 | 1352 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|  74031 | 1353 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1354 | `	VM_EXIT_BREAK;` |
|  37027 | 1355 | `}` |
|      - | 1356 |  |
|      - | 1357 | `/*` |
|      - | 1358 | ` * OP_LOAD_LIST: body moved verbatim from the OP_LOAD_LIST arm of` |
|      - | 1359 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1360 | ` */` |
|    266 | 1361 | `PH7_PRIVATE VmOpRc VmExecOpLoadList(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1362 | `{` |
|    271 | 1363 | `	ph7_value *pTos = pState->pTos;` |
|    271 | 1364 | `	ph7_value *pStack = pState->pStack;` |
|    271 | 1365 | `	VmInstr *aInstr = pState->aInstr;` |
|    271 | 1366 | `	sxi32 pc = pState->pc;` |
|      - | 1367 | `	sxi32 rc;` |
|    133 | 1368 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1369 | `	ph7_value *pEntry;` |
|    271 | 1370 | `	if( pInstr->iP1 <= 0 ){` |
|      - | 1371 | `		/* Empty list,break immediately */` |
|    ! 0 | 1372 | `		VM_EXIT_BREAK;` |
|      - | 1373 | `	}` |
|    271 | 1374 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|      - | 1375 | `#ifdef UNTRUST` |
|      - | 1376 | `	if( &pEntry[-1] < pStack ){` |
|      - | 1377 | `		VM_EXIT_ABORT;` |
|      - | 1378 | `	}` |
|      - | 1379 | `#endif` |
|    271 | 1380 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|    265 | 1381 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|      - | 1382 | `		ph7_hashmap_node *pNode;` |
|      - | 1383 | `		ph7_value sKey,*pObj;` |
|      - | 1384 | `		/* Start Copying */` |
|    265 | 1385 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|    811 | 1386 | `		while( pEntry <= pTos ){` |
|    551 | 1387 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|    523 | 1388 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|    523 | 1389 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|    523 | 1390 | `					if( rc == SXRET_OK ){` |
|      - | 1391 | `						/* Store node value */` |
|    523 | 1392 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|    264 | 1393 | `					}else{` |
|      - | 1394 | `						/* Undefined array key */` |
|      - | 1395 | `						char zMsg[128];` |
|    ! 0 | 1396 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|    ! 0 | 1397 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|    ! 0 | 1398 | `						PH7_MemObjRelease(pObj);` |
|      - | 1399 | `					}` |
|    259 | 1400 | `				}` |
|    259 | 1401 | `			}` |
|    551 | 1402 | `			sKey.x.iVal++; /* Next numeric index */` |
|    551 | 1403 | `			pEntry++;` |
|      5 | 1404 | `		}` |
|    135 | 1405 | `	}else{` |
|      - | 1406 | `		/* Source is not an array */` |
|      - | 1407 | `		ph7_value *pObj;` |
|     18 | 1408 | `		while( pEntry <= pTos ){` |
|     12 | 1409 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|     12 | 1410 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|     12 | 1411 | `					PH7_MemObjRelease(pObj);` |
|      5 | 1412 | `				}` |
|      5 | 1413 | `			}` |
|     12 | 1414 | `			pEntry++;` |
|      2 | 1415 | `		}` |
|      8 | 1416 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|      - | 1417 | `			/* Positional list destructuring silences null+bool; warn for the rest. */` |
|      3 | 1418 | `			VmWarnCannotUseAsArray(&(*pVm),pTos[-pInstr->iP1].iFlags);` |
|      1 | 1419 | `		}` |
|      - | 1420 | `	}` |
|    271 | 1421 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|    271 | 1422 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1423 | `	VM_EXIT_BREAK;` |
|    138 | 1424 | `}` |
|      - | 1425 |  |
|      - | 1426 | `/*` |
|      - | 1427 | ` * OP_UNSET_VAR: body moved verbatim from the OP_UNSET_VAR arm of` |
|      - | 1428 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1429 | ` */` |
|   6766 | 1430 | `PH7_PRIVATE VmOpRc VmExecOpUnsetVar(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1431 | `{` |
|   6771 | 1432 | `	ph7_value *pTos = pState->pTos;` |
|   6771 | 1433 | `	ph7_value *pStack = pState->pStack;` |
|   6771 | 1434 | `	VmInstr *aInstr = pState->aInstr;` |
|   6771 | 1435 | `	sxi32 pc = pState->pc;` |
|      - | 1436 | `	sxi32 rc;` |
|   3383 | 1437 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1438 | `	/* unset($name): p3 is the variable name. Drops the NAME only — see VmUnsetVarByName */` |
|   6771 | 1439 | `	SyString *pName = (SyString *)pInstr->p3;` |
|   6771 | 1440 | `	if( pName && pVm->pFrame ){` |
|      - | 1441 | `		/* Inside a try{} the VM pushes an EXCEPTION frame; variables live in the body` |
|      - | 1442 | `		 * frame below it, so skip past it exactly as every other variable path does.` |
|      - | 1443 | `		 * Without this, unset($x) inside a try silently found nothing and did nothing. */` |
|   6771 | 1444 | `		VmFrame *pVarFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|   6771 | 1445 | `		sxi32 rcU = VmUnsetVarByName(&(*pVm),pVarFrame,pName->zString,pName->nByte);` |
|   6771 | 1446 | `		if( rcU == PH7_ABORT ){` |
|      3 | 1447 | `			VM_EXIT_ABORT;` |
|      - | 1448 | `		}` |
|      - | 1449 | `		/* Releasing the last holder can run a __destruct(), and that destructor may` |
|      - | 1450 | `		 * throw. Such a throw is PARKED in nBoundaryRc by the boundary rail; consume it` |
|      - | 1451 | `		 * here and route it, or the catch runs and execution resumes inside the try` |
|      - | 1452 | `		 * ("resumed-dtor" instead of php's "caught-dtor"). */` |
|   6769 | 1453 | `		if( pVm->nBoundaryRc != 0 ){` |
|      3 | 1454 | `			rc = pVm->nBoundaryRc;` |
|      3 | 1455 | `			pVm->nBoundaryRc = 0;` |
|      3 | 1456 | `			if( rc == PH7_ABORT ){` |
|    ! 0 | 1457 | `				VM_EXIT_ABORT;` |
|      - | 1458 | `			}` |
|      3 | 1459 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - | 1460 | `		}` |
|   3381 | 1461 | `	}` |
|   6767 | 1462 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1463 | `	VM_EXIT_BREAK;` |
|   3388 | 1464 | `}` |
|      - | 1465 |  |
