# src/ph7/vm_ops_load.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 844/956 lines (88.28%)

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
| 252926 |  182 | `static int VmOffsetTypeRejected(ph7_vm *pVm,ph7_value *pKey,int iCtx,SyBlob *pMsg)` |
|      5 |  183 | `{` |
|      - |  184 | `	const char *zType;` |
| 252931 |  185 | `	SyString *pClass = 0;` |
| 252931 |  186 | `	if( pKey == 0 ){` |
|    ! 0 |  187 | `		return FALSE;` |
|      - |  188 | `	}` |
| 252931 |  189 | `	if( pKey->iFlags & MEMOBJ_OBJ ){` |
|     15 |  190 | `		ph7_class_instance *pInst = (ph7_class_instance *)pKey->x.pOther;` |
|     15 |  191 | `		if( pInst && pInst->pClass ){` |
|     15 |  192 | `			pClass = &pInst->pClass->sName;` |
|      7 |  193 | `		}` |
|     15 |  194 | `		zType = "object";` |
| 252924 |  195 | `	}else if( pKey->iFlags & MEMOBJ_HASHMAP ){` |
|      5 |  196 | `		zType = "array";` |
|      3 |  197 | `	}else{` |
| 252913 |  198 | `		return FALSE;` |
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
| 126468 |  217 | `}` |
|      - |  218 | `/*` |
|      - |  219 | ` * php's other half of the offset-type rules: a RESOURCE offset is accepted, with` |
|      - |  220 | `` * `Warning: Resource ID#N used as offset, casting to integer (N)`, and the key`` |
|      - |  221 | ` * becomes that integer. Rewrites pKey in place so the normal integer-key path` |
|      - |  222 | ` * takes over.` |
|      - |  223 | ` */` |
| 252908 |  224 | `static void VmOffsetResourceWarn(ph7_vm *pVm,ph7_value *pKey)` |
|      5 |  225 | `{` |
|      - |  226 | `	sxu32 nId;` |
| 252913 |  227 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_RES) == 0 ){` |
| 252909 |  228 | `		return;` |
|      - |  229 | `	}` |
|      5 |  230 | `	nId = PH7_VmResourceId(pVm,pKey->x.pOther);` |
|      7 |  231 | `	VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      2 |  232 | `		"Resource ID#%u used as offset, casting to integer (%u)",nId,nId);` |
|      5 |  233 | `	PH7_MemObjRelease(pKey);` |
|      5 |  234 | `	pKey->x.iVal = (sxi64)nId;` |
|      5 |  235 | `	MemObjSetType(pKey,MEMOBJ_INT);` |
| 126459 |  236 | `}` |
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
| 267460 |  248 | `static int VmNullOffsetDeprecate(ph7_vm *pVm,ph7_value *pKey)` |
|      5 |  249 | `{` |
| 267465 |  250 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_NULL) == 0 ){` |
| 267445 |  251 | `		return FALSE;` |
|      - |  252 | `	}` |
|     22 |  253 | `	PH7_VmThrowError(&(*pVm),0,E_DEPRECATED,` |
|      - |  254 | `		"Using null as an array offset is deprecated, use an empty string instead");` |
|     22 |  255 | `	return TRUE;` |
| 133735 |  256 | `}` |
|      - |  257 | `/*` |
|      - |  258 | ` * OP_STORE_IDX_REF: body moved verbatim from the OP_STORE_IDX_REF arm of` |
|      - |  259 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  260 | ` */` |
| 267650 |  261 | `PH7_PRIVATE VmOpRc VmExecOpStoreIdxRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  262 | `{` |
| 267655 |  263 | `	ph7_value *pTos = pState->pTos;` |
| 267655 |  264 | `	ph7_value *pStack = pState->pStack;` |
| 267655 |  265 | `	VmInstr *aInstr = pState->aInstr;` |
| 267655 |  266 | `	sxi32 pc = pState->pc;` |
|      - |  267 | `	sxi32 rc;` |
| 133825 |  268 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 267655 |  269 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|      - |  270 | `	ph7_value *pKey;` |
|      - |  271 | `	sxu32 nIdx;` |
| 267655 |  272 | `	if( pInstr->iP1 ){` |
|      - |  273 | `		/* Key is next on stack */` |
|  81075 |  274 | `		pKey = pTos;` |
|  81075 |  275 | `		pTos--;` |
|  40540 |  276 | `	}else{` |
| 186585 |  277 | `		pKey = 0;` |
|      - |  278 | `	}` |
|      - |  279 | `		/* php DEPRECATES a null / lossy-float write subscript (then normalizes "" /` |
|      - |  280 | ``		 * truncates); it does not reject either. A null key — by-VALUE (`$a[$k]=v`)`` |
|      - |  281 | ``		 * OR by-REF (`$a[$k]=&$x`) — deprecates + coerces to the "" key: the notice is`` |
|      - |  282 | `		 * emitted DOWN AT THE INSERT (below), not here, so a null/false container that` |
|      - |  283 | ``		 * auto-vivifies (`$x=null; $x[null]=v`) gets it too (the base is not yet a`` |
|      - |  284 | `		 * hashmap at this point), and PH7_HashmapInsert / HashmapInsertByRef both cast` |
|      - |  285 | `		 * NULL->"". A lossy-FLOAT subscript stays a loud TypeError in BOTH forms (the` |
|      - |  286 | `		 * recorded non-deprecated-surface policy, §2). */` |
| 267655 |  287 | `		if( pKey && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|      - |  288 | `			SyBlob sTypeMsg;` |
|      - |  289 | `			/* An object/array key is php's TypeError; a resource key warns and` |
|      - |  290 | `			 * becomes its integer id. Both used to be stringified silently. */` |
|  80865 |  291 | `			if( VmOffsetTypeRejected(&(*pVm),pKey,0,&sTypeMsg) ){` |
|      - |  292 | `				sxi32 rcSc;` |
|      5 |  293 | `				PH7_MemObjRelease(pKey);` |
|      5 |  294 | `				VmPopOperand(&pTos,1);` |
|      5 |  295 | `				rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      8 |  296 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      5 |  297 | `				rc = rcSc;` |
|      5 |  298 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  299 | `			}` |
|  80861 |  300 | `			VmOffsetResourceWarn(&(*pVm),pKey);` |
|  80856 |  301 | `			if( (pKey->iFlags & MEMOBJ_REAL)` |
|  40436 |  302 | `			 && pKey->rVal != (ph7_real)(sxi64)pKey->rVal ){` |
|      - |  303 | `				sxi32 rcSc;` |
|      3 |  304 | `				const char *zErr = "Cannot access offset of type float on array";` |
|      3 |  305 | `				PH7_MemObjRelease(pKey);` |
|      3 |  306 | `				VmPopOperand(&pTos,1);` |
|      3 |  307 | `				rcSc = VmThrowFromVm(&(*pVm),"TypeError",zErr,(sxu32)SyStrlen(zErr));` |
|      3 |  308 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  309 | `				rc = rcSc;` |
|      3 |  310 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  311 | `			}` |
|  40427 |  312 | `		}` |
| 267649 |  313 | `	nIdx = pTos->nIdx;` |
|      - |  314 | `	{` |
|      - |  315 | `		/* ArrayAccess::offsetSet dispatch.` |
|      - |  316 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|      - |  317 | `		 * the backing variable slot at nIdx. */` |
| 267649 |  318 | `		ph7_class_instance *pInst = 0;` |
| 267649 |  319 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|     89 |  320 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
| 267606 |  321 | `		}else if( nIdx != SXU32_HIGH ){` |
| 267563 |  322 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
| 267563 |  323 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|    ! 0 |  324 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|    ! 0 |  325 | `			}` |
| 133779 |  326 | `		}` |
| 267649 |  327 | `		if( pInst ){` |
|     89 |  328 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|     89 |  329 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|      - |  330 | `				ph7_class_method *pMeth;` |
|      - |  331 | `				ph7_value sNullKey;` |
|      - |  332 | `				ph7_value *apArg[2];` |
|     87 |  333 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|    ! 0 |  334 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |  335 | `						"Cannot assign by reference to overloaded object");` |
|    ! 0 |  336 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|    ! 0 |  337 | `					VmPopOperand(&pTos,2); /* container + value */` |
|    ! 0 |  338 | `					VM_EXIT_BREAK;` |
|      - |  339 | `				}` |
|     87 |  340 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  341 | `					"offsetSet",sizeof("offsetSet")-1);` |
|      - |  342 | `				/* Pop container; pTos now points to the value */` |
|     87 |  343 | `				VmPopOperand(&pTos,1);` |
|     87 |  344 | `				if( pKey == 0 ){` |
|     10 |  345 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|     10 |  346 | `					apArg[0] = &sNullKey;` |
|      6 |  347 | `				}else{` |
|     79 |  348 | `					apArg[0] = pKey;` |
|      - |  349 | `				}` |
|     87 |  350 | `				apArg[1] = pTos;` |
|     87 |  351 | `				if( pMeth ){` |
|     87 |  352 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|     42 |  353 | `				}` |
|     87 |  354 | `				if( pKey ){` |
|     79 |  355 | `					PH7_MemObjRelease(pKey);` |
|     41 |  356 | `				}else{` |
|     10 |  357 | `					PH7_MemObjRelease(&sNullKey);` |
|      - |  358 | `				}` |
|      - |  359 | `				/* Pop the value */` |
|     87 |  360 | `				VmPopOperand(&pTos,1);` |
|     87 |  361 | `				VM_EXIT_BREAK;` |
|      - |  362 | `			}` |
|      - |  363 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|      - |  364 | `			 * than silently coercing the object into a hashmap (which is` |
|      - |  365 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|      - |  366 | `			 * a few lines below). Match PHP. */` |
|      - |  367 | `			{` |
|      - |  368 | `				char zMsg[256];` |
|      3 |  369 | `				SyString *pName = &pInst->pClass->sName;` |
|      4 |  370 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  371 | `					"Cannot use object of type %.*s as array",` |
|      2 |  372 | `					(int)pName->nByte,pName->zString);` |
|      3 |  373 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      3 |  374 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      3 |  375 | `				VmPopOperand(&pTos,2); /* container + value */` |
|      3 |  376 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  377 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  378 | `			}` |
|      - |  379 | `		}` |
|      - |  380 | `	}` |
| 267563 |  381 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      - |  382 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|      - |  383 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|      - |  384 | `		 * checking true sharing count, then re-add after separation. */` |
| 267415 |  385 | `		if( nIdx != SXU32_HIGH ){` |
| 267415 |  386 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
| 401120 |  387 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
| 267415 |  388 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  389 | `				/* Only adjust refcount / perform COW if the backing variable` |
|      - |  390 | `				 * is still sharing the same hashmap instance. This mirrors` |
|      - |  391 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|      - |  392 | `				 * refcounts if the backing array was already separated. */` |
| 267415 |  393 | `				if( pBacking->x.pOther == (void *)pCur ){` |
| 267415 |  394 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
| 267415 |  395 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
| 267415 |  396 | `					pMap->iRef++;  /* Re-add stack ref */` |
| 267415 |  397 | `					pTos->x.pOther = pMap;` |
| 133710 |  398 | `				}else{` |
|      - |  399 | `					/* Backing variable no longer points at pCur: skip COW here` |
|      - |  400 | `					 * and operate on the hashmap currently on the stack. */` |
|    ! 0 |  401 | `					pMap = pCur;` |
|      - |  402 | `				}` |
| 133710 |  403 | `			}else{` |
|    ! 0 |  404 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  405 | `			}` |
| 133710 |  406 | `		}else{` |
|    ! 0 |  407 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  408 | `		}` |
| 267415 |  409 | `		if( pMap->iRef < 2 ){` |
|      - |  410 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|      - |  411 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|      - |  412 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|      - |  413 | `			 * no code checks iRef for COW decisions. */` |
|    ! 0 |  414 | `			pMap->iRef = 2;` |
|    ! 0 |  415 | `		}` |
| 133710 |  416 | `	}else{` |
|      - |  417 | `		ph7_value *pObj;` |
|    153 |  418 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|    153 |  419 | `		if( pObj == 0 ){` |
|    ! 0 |  420 | `			if( pKey ){` |
|    ! 0 |  421 | `			  PH7_MemObjRelease(pKey);` |
|    ! 0 |  422 | `			}` |
|    ! 0 |  423 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  424 | `			VM_EXIT_BREAK;` |
|      - |  425 | `		}` |
|      - |  426 | `		/* Phase#1: Load the array */` |
|    153 |  427 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|    110 |  428 | `			VmPopOperand(&pTos,1);` |
|      - |  429 | `			/* Force a string cast on the RHS (user-visible: an array warns` |
|      - |  430 | `			 * "Array to string conversion" before the offset write, §2) */` |
|    110 |  431 | `			PH7_MemObjToStringUV(pTos);` |
|    110 |  432 | `			if( pKey == 0 ){` |
|      - |  433 | ``				/* `$s[] = 'x'` on a STRING: php raises the catchable Error`` |
|      - |  434 | `				 * "[] operator not supported for strings" and leaves the string` |
|      - |  435 | `				 * untouched. PHL silently APPENDED, so code that meant to build` |
|      - |  436 | `				 * an array from a variable holding a string quietly produced a` |
|      - |  437 | `				 * longer string instead of failing — a wrong answer, not a` |
|      - |  438 | `				 * missing diagnostic. */` |
|      - |  439 | `				SyBlob sErrMsg;` |
|      3 |  440 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      3 |  441 | `				SyBlobAppend(&sErrMsg,"[] operator not supported for strings",` |
|      - |  442 | `					sizeof("[] operator not supported for strings")-1);` |
|      3 |  443 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      3 |  444 | `				VM_EXIT_BREAK;` |
|    ! 0 |  445 | `			}else{` |
|      - |  446 | `				sxi64 iOfft;` |
|      - |  447 | `				sxi64 nLen;` |
|    108 |  448 | `				if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  449 | `					/* Force an int cast */` |
|    ! 0 |  450 | `					PH7_MemObjToInteger(pKey);` |
|    ! 0 |  451 | `				}` |
|    108 |  452 | `				iOfft = pKey->x.iVal;` |
|    108 |  453 | `				nLen = (sxi64)SyBlobLength(&pObj->sBlob);` |
|    108 |  454 | `				if( iOfft < 0 ){` |
|      - |  455 | `					/* php 7.1: a negative offset writes back from the end. */` |
|      5 |  456 | `					iOfft += nLen;` |
|      5 |  457 | `					if( iOfft < 0 ){` |
|      4 |  458 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset %qd",` |
|      1 |  459 | `							pKey->x.iVal);` |
|      3 |  460 | `						PH7_MemObjRelease(pKey);` |
|      3 |  461 | `						VM_EXIT_BREAK;` |
|      - |  462 | `					}` |
|      1 |  463 | `				}` |
|    106 |  464 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    106 |  465 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|    106 |  466 | `					if( SyBlobLength(&pTos->sBlob) > 1 ){` |
|      6 |  467 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      - |  468 | `							"Only the first byte will be assigned to the string offset");` |
|      2 |  469 | `					}` |
|    106 |  470 | `					if( iOfft >= nLen ){` |
|      - |  471 | `						/* php PADS WITH SPACES up to the offset. PH7 simply appended the` |
|      - |  472 | `						 * byte, so "abc" with [6]="Z" became "abcZ" rather than "abc   Z"` |
|      - |  473 | `						 * -- a silently wrong string. */` |
|      - |  474 | `						sxi64 nPad;` |
|      9 |  475 | `						for( nPad = nLen ; nPad < iOfft ; ++nPad ){` |
|      7 |  476 | `							SyBlobAppend(&pObj->sBlob," ",sizeof(char));` |
|      4 |  477 | `						}` |
|      3 |  478 | `						SyBlobAppend(&pObj->sBlob,(const void *)zBlob,sizeof(char));` |
|      2 |  479 | `					}else{` |
|    104 |  480 | `						char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|    104 |  481 | `						zData[iOfft] = zBlob[0];` |
|      - |  482 | `					}` |
|     52 |  483 | `				}` |
|      - |  484 | `			}` |
|    106 |  485 | `			if( pKey ){` |
|    106 |  486 | `			  PH7_MemObjRelease(pKey);` |
|     52 |  487 | `			}` |
|    106 |  488 | `			VM_EXIT_BREAK;` |
|     44 |  489 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - |  490 | `			/* php: only NULL and FALSE auto-vivify into an array. Writing an index into an` |
|      - |  491 | `			 * int/float/resource/TRUE is a catchable Error -- PH7 quietly REPLACED the value` |
|      - |  492 | `			 * with an array, destroying it ($x = 5; $x[0] = 1; left $x === [1]). */` |
|      - |  493 | `			/* php auto-vivifies false into an array with an 8.1 DEPRECATION; PHL` |
|      - |  494 | `			 * rejects any scalar base, false included (null still auto-vivifies). */` |
|     44 |  495 | `			int bScalar = (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0;` |
|     44 |  496 | `			if( bScalar ){` |
|      - |  497 | `				sxi32 rcSc;` |
|      8 |  498 | `				if( pKey ){` |
|      5 |  499 | `					PH7_MemObjRelease(pKey);` |
|      2 |  500 | `				}` |
|      8 |  501 | `				VmPopOperand(&pTos,1);` |
|      8 |  502 | `				rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot use a scalar value as an array",` |
|      - |  503 | `					sizeof("Cannot use a scalar value as an array")-1);` |
|      8 |  504 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      8 |  505 | `				rc = rcSc;` |
|      8 |  506 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  507 | `			}` |
|      - |  508 | `			/* Force a hashmap cast  */` |
|     38 |  509 | `			rc = PH7_MemObjToHashmap(pObj);` |
|     38 |  510 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  511 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|    ! 0 |  512 | `				VM_EXIT_ABORT;` |
|      - |  513 | `			}` |
|     17 |  514 | `		}` |
|      - |  515 | `		/* COW separate the backing variable before mutation */` |
|     38 |  516 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|      - |  517 | `	}` |
| 267449 |  518 | `	VmPopOperand(&pTos,1);` |
|      - |  519 | `	/* Phase#2: Perform the insertion. A null key deprecates + normalizes to "" for` |
|      - |  520 | `	 * BOTH the by-value and the by-ref store — emitted HERE, after a null/false` |
|      - |  521 | ``	 * container has auto-vivified to an array, so `$x[null]=v` and `$x[null]=&$y` on`` |
|      - |  522 | `	 * an undefined $x get the notice too (the top-of-handler check runs before` |
|      - |  523 | `	 * vivification, when the base is not yet a hashmap). HashmapInsert /` |
|      - |  524 | `	 * HashmapInsertByRef both then cast NULL->"". A null pKey==0 (an append, no key)` |
|      - |  525 | `	 * is not a null OFFSET and is left alone. */` |
| 267449 |  526 | `	VmNullOffsetDeprecate(&(*pVm),pKey);` |
| 267449 |  527 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|     36 |  528 | `		if( pMap == pVm->pGlobal ){` |
|      - |  529 | `			/* php 8.1: $GLOBALS['y'] =& $x binds the global $y to $x's` |
|      - |  530 | `			 * slot; an append has no name to bind (catchable Error). */` |
|      3 |  531 | `			if( pKey == 0 ){` |
|    ! 0 |  532 | `				rc = PH7_VmThrowGlobalsAppendError(&(*pVm));` |
|    ! 0 |  533 | `			}else{` |
|      3 |  534 | `				if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 |  535 | `					PH7_MemObjToString(pKey);` |
|    ! 0 |  536 | `				}` |
|      3 |  537 | `				if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|      - |  538 | `					/* Pathological empty name: keep the legacy diagnostic */` |
|    ! 0 |  539 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|      - |  540 | `						"$GLOBALS is a read-only array,insertion is forbidden");` |
|    ! 0 |  541 | `					rc = SXRET_OK;` |
|    ! 0 |  542 | `				}else{` |
|      4 |  543 | `					rc = PH7_VmInstallGlobalVar(&(*pVm),` |
|      2 |  544 | `						(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|      2 |  545 | `						0,pTos->nIdx);` |
|      - |  546 | `				}` |
|      - |  547 | `			}` |
|      2 |  548 | `		}else{` |
|      - |  549 | `			/* Insertion by reference */` |
|     34 |  550 | `			rc = PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|      - |  551 | `		}` |
|     19 |  552 | `	}else{` |
| 267415 |  553 | `		rc = PH7_HashmapInsert(pMap,pKey,pTos);` |
|      - |  554 | `	}` |
| 267449 |  555 | `	if( pKey ){` |
|  80881 |  556 | `		PH7_MemObjRelease(pKey);` |
|  40438 |  557 | `	}` |
|      - |  558 | `	/* An append onto the occupied saturated auto-index threw php's catchable` |
|      - |  559 | `	 * Error (PH7_VmThrowArrayNextIndexError) — dispatch it like any other` |
|      - |  560 | `	 * store-path throw. Plain failures (OOM) keep their existing routes. */` |
| 267451 |  561 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
| 267445 |  562 | `	VM_EXIT_BREAK;` |
|    ! 0 |  563 | `	VM_EXIT_BREAK;` |
| 133830 |  564 | `}` |
|      - |  565 |  |
|      - |  566 | `/*` |
|      - |  567 | ` * OP_LOAD_CLOSURE: body moved verbatim from the OP_LOAD_CLOSURE arm of` |
|      - |  568 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  569 | ` */` |
|   1530 |  570 | `PH7_PRIVATE VmOpRc VmExecOpLoadClosure(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  571 | `{` |
|   1535 |  572 | `	ph7_value *pTos = pState->pTos;` |
|   1535 |  573 | `	ph7_value *pStack = pState->pStack;` |
|   1535 |  574 | `	VmInstr *aInstr = pState->aInstr;` |
|   1535 |  575 | `	sxi32 pc = pState->pc;` |
|      - |  576 | `	sxi32 rc;` |
|    765 |  577 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   1535 |  578 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|      - |  579 | `	/* The function whose name the Closure object will wrap: a fresh per-instantiation` |
|      - |  580 | `	 * copy for a real closure (built below), or the shared lambda function itself for a` |
|      - |  581 | `	 * plain anonymous function with no captured environment. */` |
|   1535 |  582 | `	ph7_vm_func *pTarget = pFunc;` |
|      - |  583 | `	/* A no-capture lambda declared inside a class method is not VM_FUNC_CLOSURE` |
|      - |  584 | `	 * (its env is empty), yet php still binds the creation-site class as its scope` |
|      - |  585 | ``	 * so `self::`/private access inside the body works. Detect the enclosing class`` |
|      - |  586 | `	 * here and route such a lambda through the per-instance closure path (a global` |
|      - |  587 | `	 * no-capture lambda peeks NULL and stays a shared function). */` |
|   1535 |  588 | `	ph7_class *pLoadScope = (pFunc->iFlags & VM_FUNC_CLOSURE) ? 0 : PH7_VmPeekDeclaringClass(pVm);` |
|   1535 |  589 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) \|\| pLoadScope ){` |
|      - |  590 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|      - |  591 | `		ph7_vm_func *pClosure;` |
|      - |  592 | `		char *zName;` |
|      - |  593 | `		sxu32 mLen;` |
|      - |  594 | `		sxu32 n;` |
|      - |  595 | `		/* Create a new VM function */` |
|   1519 |  596 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|      - |  597 | `		/* Generate an unique closure name */` |
|   1519 |  598 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|   1519 |  599 | `		if( pClosure == 0 \|\| zName == 0){` |
|    ! 0 |  600 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|    ! 0 |  601 | `			VM_EXIT_ABORT;` |
|      - |  602 | `		}` |
|   1519 |  603 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|   1519 |  604 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|    ! 0 |  605 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|    ! 0 |  606 | `		}` |
|      - |  607 | `		/* Zero the stucture */` |
|   1519 |  608 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|      - |  609 | `		/* Perform a structure assignment on read-only items */` |
|   1519 |  610 | `		pClosure->aArgs = pFunc->aArgs;` |
|   1519 |  611 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|   1519 |  612 | `		pClosure->aStatic = pFunc->aStatic;` |
|   1519 |  613 | `		pClosure->iFlags = pFunc->iFlags;` |
|      - |  614 | `		/* An in-class no-capture lambda routed here (pLoadScope set) must be a` |
|      - |  615 | `		 * real closure so PH7_VmClassMemberAccess honors its pUserData scope. */` |
|   1519 |  616 | `		pClosure->iFlags \|= VM_FUNC_CLOSURE;` |
|   1519 |  617 | `		pClosure->pUserData = pFunc->pUserData;` |
|   1519 |  618 | `		pClosure->sSignature = pFunc->sSignature;` |
|   1519 |  619 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|   1519 |  620 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|   1519 |  621 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|   1519 |  622 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|   1519 |  623 | `		pClosure->bStrictTypes = pFunc->bStrictTypes;` |
|   1519 |  624 | `		pClosure->nMaxStack = pFunc->nMaxStack;` |
|   1519 |  625 | `		if( pClosure->pUserData == 0 ){` |
|      - |  626 | `			/* Stamp the creation-site class scope (see PH7_VmPeekDeclaringClass):` |
|      - |  627 | `			 * a closure made in a method — or in another closure, whose own stamp` |
|      - |  628 | `			 * the peek reads — resolves self::/parent:: against it like php. */` |
|   1519 |  629 | `			pClosure->pUserData = (void *)PH7_VmPeekDeclaringClass(pVm);` |
|    757 |  630 | `		}` |
|      - |  631 | ``		/* Capture the creation-site late-static-binding class so `static::` inside`` |
|      - |  632 | `		 * the closure body resolves like php (the "called class", which may differ` |
|      - |  633 | `		 * from the declaring scope stamped above — e.g. a closure made in an` |
|      - |  634 | `		 * inherited method). A closure made outside any class captures NULL. */` |
|   1519 |  635 | `		pClosure->pLsbClass = (void *)PH7_VmPeekTopClass(pVm);` |
|      - |  636 | `		/* Reflection descriptor fields (getDocComment/getAttributes/getStartLine/` |
|      - |  637 | `		 * getEndLine/getFileName read the INSTANTIATED copy via $__fn) */` |
|   1519 |  638 | `		pClosure->aAttrs = pFunc->aAttrs;` |
|   1519 |  639 | `		pClosure->sDoc = pFunc->sDoc;` |
|   1519 |  640 | `		pClosure->sFile = pFunc->sFile;` |
|   1519 |  641 | `		pClosure->nLine = pFunc->nLine;` |
|   1519 |  642 | `		pClosure->nEndLine = pFunc->nEndLine;` |
|   1519 |  643 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|      - |  644 | `		/* Register the closure */` |
|   1519 |  645 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|      - |  646 | `		/* Set up closure environment */` |
|   1519 |  647 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   1519 |  648 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|   3361 |  649 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|      - |  650 | `			ph7_value *pValue;` |
|   1847 |  651 | `			pEnv = &aEnv[n];` |
|   1847 |  652 | `			sEnv.sName  = pEnv->sName;` |
|   1847 |  653 | `			sEnv.iFlags = pEnv->iFlags;` |
|   1847 |  654 | `			sEnv.nLine = pEnv->nLine;` |
|   1847 |  655 | `			sEnv.nIdx = SXU32_HIGH;` |
|   1847 |  656 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|   1842 |  657 | `			if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == VM_FUNC_ARG_BY_REF` |
|    990 |  658 | `			 && !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|     64 |  659 | `				&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|      - |  660 | `				/* Capture by reference: bind the env entry to the variable's` |
|      - |  661 | `				 * memory slot — creating a fresh null variable when missing,` |
|      - |  662 | ``				 * as php does (`use (&$f)` before $f is assigned) — and pin`` |
|      - |  663 | `				 * the slot past the creating frame's teardown so the closure` |
|      - |  664 | `				 * can outlive its birth scope. The call-time env install` |
|      - |  665 | `				 * aliases the name to this slot instead of copying a value. */` |
|     97 |  666 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,TRUE);` |
|     97 |  667 | `				if( pValue ){` |
|     97 |  668 | `					sEnv.nIdx = pValue->nIdx;` |
|     97 |  669 | `					VmPinMemObjSlot(pVm,pValue->nIdx);` |
|     47 |  670 | `				}` |
|     50 |  671 | `			}else{` |
|      - |  672 | `				/* Standard pass by value */` |
|   1753 |  673 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|   1753 |  674 | `				if( pValue ){` |
|      - |  675 | `					/* Copy imported value */` |
|    306 |  676 | `					PH7_MemObjStore(pValue,&sEnv.sValue);` |
|   1601 |  677 | `				}else if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == 0` |
|    745 |  678 | `					&& !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|     17 |  679 | `						&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|     35 |  680 | `					if( pFunc->iFlags & VM_FUNC_ARROW ){` |
|      - |  681 | `						/* An arrow function auto-captures free variables by value, but` |
|      - |  682 | `						 * php does NOT capture one that is UNDEFINED at creation: the` |
|      - |  683 | `						 * isolated body scope then simply has no such variable, so a` |
|      - |  684 | `						 * read of it there raises the normal "Undefined variable"` |
|      - |  685 | `						 * warning (and reflection's getClosureUsedVariables omits it).` |
|      - |  686 | `						 * Skip installing the capture so the body READ — not the` |
|      - |  687 | `						 * creation — warns, matching php. (A later assignment to the` |
|      - |  688 | `						 * outer variable does not retro-capture: arrow scope is` |
|      - |  689 | `						 * isolated.) An explicit by-value use() instead warns here and` |
|      - |  690 | `						 * binds NULL, handled just below. */` |
|     25 |  691 | `						continue;` |
|      - |  692 | `					}` |
|      - |  693 | ``					/* php reads a by-value `use ($q)` capture AT CLOSURE CREATION and`` |
|      - |  694 | `					 * warns when the variable is undefined there (the by-ref form` |
|      - |  695 | ``					 * `use (&$q)` above stays silent — it creates the binding). The`` |
|      - |  696 | `					 * auto-injected $this (VM_FUNC_ARG_IGNORE) never warns. The` |
|      - |  697 | `					 * capture still proceeds as NULL, as php does. php attributes the` |
|      - |  698 | `					 * warning to the capture's own line (which can differ from the` |
|      - |  699 | `					 * OP_LOAD_CLOSURE instruction line when the use-clause wraps), so` |
|      - |  700 | `					 * borrow the recorded line for the emission and restore it. */` |
|     11 |  701 | `					sxu32 nSavedLine = pVm->nCurLine;` |
|     11 |  702 | `					if( sEnv.nLine ){` |
|     11 |  703 | `						pVm->nCurLine = sEnv.nLine;` |
|      5 |  704 | `					}` |
|     11 |  705 | `					VmErrorFormat(pVm,PH7_CTX_WARNING,"Undefined variable $%z",&sEnv.sName);` |
|     11 |  706 | `					pVm->nCurLine = nSavedLine;` |
|      5 |  707 | `				}` |
|      - |  708 | `			}` |
|      - |  709 | `			/* Insert the imported variable */` |
|   1825 |  710 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|    915 |  711 | `		}` |
|   1519 |  712 | `		pTarget = pClosure;` |
|    757 |  713 | `	}` |
|      - |  714 | `	/* Wrap the target function in a Closure object and push it. Its captured environment` |
|      - |  715 | ``	 * (incl. any `$this`) stays in pTarget->aClosureEnv and is delivered by the normal call`` |
|      - |  716 | `	 * path when the closure is dispatched by name. */` |
|   1535 |  717 | `	pTos++;` |
|      - |  718 | `	{` |
|   1535 |  719 | `		ph7_class_instance *pCloObj = VmCreateClosure(pVm, &pTarget->sName, 0, 0);` |
|   1535 |  720 | `		if( pCloObj ){` |
|   1535 |  721 | `			pCloObj->iRef++;` |
|   1535 |  722 | `			pTos->x.pOther = pCloObj;` |
|   1535 |  723 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|    770 |  724 | `		}else{` |
|      - |  725 | `			/* OOM fallback: the name string is still a usable callable. */` |
|    ! 0 |  726 | `			PH7_MemObjStringAppend(pTos, pTarget->sName.zString, pTarget->sName.nByte);` |
|      - |  727 | `		}` |
|      - |  728 | `	}` |
|   1535 |  729 | `	VM_EXIT_BREAK;` |
|    ! 0 |  730 | `	VM_EXIT_BREAK;` |
|    770 |  731 | `}` |
|      - |  732 |  |
|      - |  733 |  |
|      - |  734 | `/*` |
|      - |  735 | `` * TRUE when this LOAD_IDX feeds a `??`: the coalesce test is the very next`` |
|      - |  736 | `` * instruction. php evaluates the whole left operand of `??` in isset-context,`` |
|      - |  737 | ` * so the access must stay SILENT and, when it misses, must yield NULL — an` |
|      - |  738 | ``  * out-of-range string offset that yielded "" instead made `$s[99] ?? $d` `` |
|      - |  739 | ` * evaluate to "" rather than $d, a wrong answer rather than a stray notice.` |
|      - |  740 | ` */` |
|     30 |  741 | `static int VmIdxFeedsCoalesce(const VmInstr *pInstr)` |
|      5 |  742 | `{` |
|     35 |  743 | `	return (pInstr+1)->iOp == PH7_OP_NULLC \|\| (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|      5 |  744 | `}` |
|      - |  745 | `/*` |
|      - |  746 | ` * OP_LOAD_IDX: body moved verbatim from the OP_LOAD_IDX arm of` |
|      - |  747 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  748 | ` */` |
| 752645 |  749 | `PH7_PRIVATE VmOpRc VmExecOpLoadIdx(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  750 | `{` |
| 752650 |  751 | `	ph7_value *pTos = pState->pTos;` |
| 752650 |  752 | `	ph7_value *pStack = pState->pStack;` |
| 752650 |  753 | `	VmInstr *aInstr = pState->aInstr;` |
| 752650 |  754 | `	sxi32 pc = pState->pc;` |
|      - |  755 | `	sxi32 rc;` |
| 377055 |  756 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 752650 |  757 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
| 752650 |  758 | `	ph7_hashmap *pMap = 0;` |
|      - |  759 | `	ph7_value *pIdx;` |
|      - |  760 | `	/* D1 commit 2: iP2==9 is the deferred-record mode. It behaves exactly like a plain read` |
|      - |  761 | `	 * (iP2==0) for base dispatch / the read tail, EXCEPT the dedicated block right after the` |
|      - |  762 | `	 * index is popped, which — on a lookup MISS with a reachable base — captures the lvalue` |
|      - |  763 | `	 * path (MEMOBJ_AUX_DEFPATH) instead of warning, and exits. Everything else sees iP2. */` |
| 752650 |  764 | `	sxi32 iP2 = (pInstr->iP2 == 9) ? 0 : pInstr->iP2;` |
| 752650 |  765 | `	pIdx = 0;` |
| 752650 |  766 | `	if( pInstr->iP1 == 0 ){` |
|      3 |  767 | `		if( !iP2){` |
|      - |  768 | `			/* No available index,load NULL */` |
|    ! 0 |  769 | `			if( pTos >= pStack ){` |
|    ! 0 |  770 | `				PH7_MemObjRelease(pTos);` |
|    ! 0 |  771 | `			}else{` |
|      - |  772 | `				/* TICKET 1433-020: Empty stack */` |
|    ! 0 |  773 | `				pTos++;` |
|    ! 0 |  774 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  775 | `				pTos->nIdx = SXU32_HIGH;` |
|      - |  776 | `			}` |
|      - |  777 | `			/* Emit a notice */` |
|    ! 0 |  778 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|      - |  779 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|    ! 0 |  780 | `			VM_EXIT_BREAK;` |
|      - |  781 | `		}` |
|      2 |  782 | `	}else{` |
| 752648 |  783 | `		pIdx = pTos;` |
| 752648 |  784 | `		pTos--;` |
|      - |  785 | `	}` |
| 752650 |  786 | `	if( pInstr->iP2 == 9 && pIdx ){` |
|      - |  787 | `		/* D1 commit 2 record mode. Decide whether to CAPTURE this subscript as a deferred` |
|      - |  788 | `		 * lvalue step (the by-ref/by-value decision is not known until OP_CALL), or fall` |
|      - |  789 | `		 * through to a plain read (iP2 was normalized to 0 above). We defer only when the` |
|      - |  790 | `		 * base can be reached again at resolve time: an existing descriptor (nested), the` |
|      - |  791 | `		 * commit-1 undefined-variable marker, or a real container/string/scalar slot` |
|      - |  792 | `		 * (nIdx != SXU32_HIGH). On a present array key we DO NOT defer — a hit already` |
|      - |  793 | `		 * yields the aliasable read slot the by-ref binder needs. */` |
|  45177 |  794 | `		VmDeferredPath *pPath = 0;` |
|  45177 |  795 | `		int bDefer = 0, eRoot = 0;` |
|  45177 |  796 | `		if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|      - |  797 | `			/* Nested: the base already carries a descriptor — extend it in place. */` |
|      3 |  798 | `			pPath = (VmDeferredPath *)pTos->x.pOther;` |
|      3 |  799 | `			bDefer = 1;` |
|  45176 |  800 | `		}else if( pTos->iFlags & MEMOBJ_AUX_DEFERRED ){` |
|      - |  801 | `			/* Undefined base variable (commit-1 marker): root the descriptor by name. */` |
|      - |  802 | `			SyString sRootName;` |
|      3 |  803 | `			SyStringInitFromBuf(&sRootName,(const char *)pTos->x.pOther,` |
|      - |  804 | `				pTos->x.pOther ? SyStrlen((const char *)pTos->x.pOther) : 0);` |
|      3 |  805 | `			pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|      3 |  806 | `			pTos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name; don't free x.pOther */` |
|      3 |  807 | `			pTos->x.pOther = 0;` |
|      3 |  808 | `			bDefer = (pPath != 0);` |
|  45174 |  809 | `		}else if( pTos->nIdx != SXU32_HIGH ){` |
|      - |  810 | `			/* A real base slot: array/scalar -> eRoot 0 (by-ref may vivify), string -> 2. */` |
|  45169 |  811 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      - |  812 | `				/* Probe hit/miss on a COPY of the key: PH7_HashmapLookup casts a NULL key to` |
|      - |  813 | `				 * "" in place, which would suppress the fall-through read's null-offset` |
|      - |  814 | `				 * deprecation (hit) and capture the wrong key in the step (miss). */` |
|      - |  815 | `				ph7_value idxProbe;` |
|  21735 |  816 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|  21735 |  817 | `				PH7_MemObjInit(&(*pVm),&idxProbe);` |
|  21735 |  818 | `				PH7_MemObjStore(pIdx,&idxProbe);` |
|  21735 |  819 | `				if( PH7_HashmapLookup(pMap,&idxProbe,&pNode) == SXRET_OK ){` |
|  21719 |  820 | `					bDefer = 0; /* present key: fall through and read it as an aliasable slot */` |
|  10862 |  821 | `				}else{` |
|     18 |  822 | `					eRoot = 0; bDefer = 1;` |
|      - |  823 | `				}` |
|  21735 |  824 | `				PH7_MemObjRelease(&idxProbe);` |
|  34304 |  825 | `			}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      5 |  826 | `				bDefer = 0; /* ArrayAccess: not a deferrable lvalue — read normally */` |
|  23437 |  827 | `			}else if( pTos->iFlags & MEMOBJ_STRING ){` |
|  23435 |  828 | `				eRoot = 2; bDefer = 1;` |
|  11835 |  829 | `			}else{` |
|    ! 0 |  830 | `				eRoot = 0; bDefer = 1; /* NULL/other reachable scalar base */` |
|      - |  831 | `			}` |
|  45169 |  832 | `			if( bDefer && pPath == 0 ){` |
|  23451 |  833 | `				pPath = VmDeferPathNew(&(*pVm),eRoot,pTos->nIdx,0);` |
|  23451 |  834 | `				if( pPath == 0 ){` |
|    ! 0 |  835 | `					bDefer = 0;` |
|    ! 0 |  836 | `				}` |
|  11838 |  837 | `			}` |
|  22697 |  838 | `		}` |
|  45177 |  839 | `		if( bDefer ){` |
|      - |  840 | `			/* Append this element step (deep-copies pIdx) and leave the carrier on pTos. */` |
|  23455 |  841 | `			if( VmDeferPathPushElem(pPath,pIdx) == SXRET_OK ){` |
|  23455 |  842 | `				if( (pTos->iFlags & MEMOBJ_AUX_DEFPATH) == 0 ){` |
|      - |  843 | `					/* Collapse the base value into the descriptor carrier. */` |
|  23453 |  844 | `					PH7_MemObjRelease(pTos);` |
|  23453 |  845 | `					pTos->x.pOther = pPath;` |
|  23453 |  846 | `					pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|  23453 |  847 | `					pTos->nIdx = SXU32_HIGH;` |
|  11839 |  848 | `				}` |
|  23455 |  849 | `				PH7_MemObjRelease(pIdx);` |
|  23455 |  850 | `				VM_EXIT_BREAK;` |
|      - |  851 | `			}` |
|      - |  852 | `			/* Out of memory appending a step. */` |
|    ! 0 |  853 | `			if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|      - |  854 | `				/* Nested base: pPath IS pTos's carrier and stays owned by it — keep it (a` |
|      - |  855 | `				 * step short) and exit, rather than fall through and misread a NULL-typed` |
|      - |  856 | `				 * carrier as an array base. Resolves the shorter path; OOM-only degradation. */` |
|    ! 0 |  857 | `				PH7_MemObjRelease(pIdx);` |
|    ! 0 |  858 | `				VM_EXIT_BREAK;` |
|      - |  859 | `			}` |
|      - |  860 | `			/* A freshly-allocated path we still own: drop it and fall back to a plain read. */` |
|    ! 0 |  861 | `			VmFreeDeferredPath(pPath);` |
|    ! 0 |  862 | `		}` |
|  10861 |  863 | `	}` |
| 729200 |  864 | `	if( iP2 == 7 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - |  865 | ``		/* Keyed list destructuring `["k"=>$v] = $src` from a NON-array source: yield NULL`` |
|      - |  866 | `		 * (never char-index a string), warning once per key — matching PHP, which warns per` |
|      - |  867 | `		 * key here. A NULL source is silent; unlike the positional OP_LOAD_LIST path, a bool` |
|      - |  868 | `		 * source DOES warn (PHP warns for bool in keyed destructuring). */` |
|      7 |  869 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      5 |  870 | `			VmWarnCannotUseAsArray(&(*pVm),pTos->iFlags);` |
|      2 |  871 | `		}` |
|      7 |  872 | `		if( pIdx ){` |
|      - |  873 | `			/* Release the key (a string literal for keyed destructuring), like the` |
|      - |  874 | `			 * normal hashmap-read exit below — otherwise its blob is orphaned. */` |
|      7 |  875 | `			PH7_MemObjRelease(pIdx);` |
|      3 |  876 | `		}` |
|      7 |  877 | `		PH7_MemObjRelease(pTos);` |
|      7 |  878 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|      7 |  879 | `		VM_EXIT_BREAK;` |
|      - |  880 | `	}` |
| 729194 |  881 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|      - |  882 | `		/* String access */` |
| 569262 |  883 | `		if( pIdx ){` |
|      - |  884 | `			sxi64 iOfft;` |
| 569262 |  885 | `			sxi64 nLen = (sxi64)SyBlobLength(&pTos->sBlob);` |
| 569262 |  886 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  887 | `				/* Force an int cast */` |
|    ! 0 |  888 | `				PH7_MemObjToInteger(pIdx);` |
|    ! 0 |  889 | `			}` |
| 569262 |  890 | `			iOfft = pIdx->x.iVal;` |
|      - |  891 | `			/* php 7.1: a NEGATIVE offset counts back from the end ($s[-1] is the last` |
|      - |  892 | `			 * character). The offset used to be cast to UNSIGNED, so -1 became a huge` |
|      - |  893 | `			 * number, ran past the end and quietly produced NULL. */` |
| 569262 |  894 | `			if( iOfft < 0 ){` |
|      7 |  895 | `				iOfft += nLen;` |
|      3 |  896 | `			}` |
| 569266 |  897 | `			if( iOfft < 0 \|\| iOfft >= nLen ){` |
|      - |  898 | `				/* Out of range. In an isset()/empty() lookup php answers FALSE, so load` |
|      - |  899 | `				 * NULL there; everywhere else it WARNS and yields the empty string (PH7` |
|      - |  900 | `				 * silently produced NULL in both cases). */` |
|      - |  901 | `				/* LOAD_IDX carries its OWN iP2 codes, which do NOT line up with the` |
|      - |  902 | `				 * PH7_MEMBER_* ones: 4 = isset, 5 = unset, 6 = empty. All three are` |
|      - |  903 | `				 * lookups and must stay silent. */` |
|     11 |  904 | `				int bQuiet = iP2 == 4 \|\| iP2 == 5 \|\| iP2 == 6` |
|     11 |  905 | `					\|\| iP2 == 8 \|\| VmIdxFeedsCoalesce(pInstr);` |
|     10 |  906 | `				PH7_MemObjRelease(pTos);` |
|     10 |  907 | `				if( bQuiet ){` |
|      8 |  908 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  909 | `				}else{` |
|      3 |  910 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|      4 |  911 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Uninitialized string offset %qd",` |
|      1 |  912 | `						pIdx->x.iVal);` |
|      - |  913 | `				}` |
|      6 |  914 | `			}else{` |
| 569254 |  915 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
| 569254 |  916 | `				int c = zData[iOfft];` |
| 569254 |  917 | `				PH7_MemObjRelease(pTos);` |
| 569254 |  918 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
| 569254 |  919 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|      - |  920 | `			}` |
| 285248 |  921 | `		}else{` |
|      - |  922 | `			/* No available index,load NULL */` |
|    ! 0 |  923 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      - |  924 | `		}` |
| 569262 |  925 | `		VM_EXIT_BREAK;` |
|      - |  926 | `	}` |
| 159937 |  927 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      - |  928 | `		/* Object subscript: ArrayAccess dispatch.` |
|      - |  929 | `		 * iP2 codes:` |
|      - |  930 | `		 *   0 = read       → offsetGet` |
|      - |  931 | `		 *   3 = ??= peek   → offsetExists; offsetGet on hit; arm coalesce` |
|      - |  932 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|      - |  933 | `		 *   4 = isset()    → offsetExists` |
|      - |  934 | `		 *   5 = unset()    → offsetUnset` |
|      - |  935 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit` |
|      - |  936 | ``		 *   8 = `??` read  → same probe as 6 (offsetExists, then offsetGet on a`` |
|      - |  937 | `		 *                    hit) and no diagnostics anywhere in this op: php` |
|      - |  938 | ``		 *                    evaluates the whole left operand of `??` in`` |
|      - |  939 | `		 *                    isset-context, and calling offsetGet blindly also` |
|      - |  940 | `		 *                    surfaced warnings raised INSIDE a userland` |
|      - |  941 | `		 *                    offsetGet (e.g. ArrayObject's own array read). */` |
|    183 |  942 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|    183 |  943 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|    183 |  944 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|      - |  945 | `			ph7_class_method *pMeth;` |
|      - |  946 | `			ph7_value sResult;` |
|      - |  947 | `			ph7_value *apArg[1];` |
|    181 |  948 | `			if( (iP2 == 0 \|\| iP2 == 3 \|\| iP2 == 8) && pIdx == 0 ){` |
|      - |  949 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|    ! 0 |  950 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|      - |  951 | `					"Cannot use [] for reading");` |
|    ! 0 |  952 | `				PH7_MemObjRelease(pTos);` |
|    ! 0 |  953 | `				pTos->nIdx = SXU32_HIGH;` |
|    ! 0 |  954 | `				VM_EXIT_BREAK;` |
|      - |  955 | `			}` |
|    181 |  956 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|    181 |  957 | `			if( iP2 == 4 \|\| iP2 == 6 \|\| iP2 == 3 \|\| iP2 == 8 ){` |
|      - |  958 | ``				/* isset, empty, ??= and `??` all start with offsetExists. */`` |
|     81 |  959 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  960 | `					"offsetExists",sizeof("offsetExists")-1);` |
|     81 |  961 | `				apArg[0] = pIdx;` |
|     81 |  962 | `				if( pMeth ){` |
|     81 |  963 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     43 |  964 | `				}` |
|    143 |  965 | `			}else if( iP2 == 5 ){` |
|     20 |  966 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  967 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|     20 |  968 | `				apArg[0] = pIdx;` |
|     20 |  969 | `				if( pMeth ){` |
|     20 |  970 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      8 |  971 | `				}` |
|     12 |  972 | `			}else{` |
|     89 |  973 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  974 | `					"offsetGet",sizeof("offsetGet")-1);` |
|     89 |  975 | `				apArg[0] = pIdx;` |
|     89 |  976 | `				if( pMeth ){` |
|     89 |  977 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     42 |  978 | `				}` |
|      - |  979 | `			}` |
|    181 |  980 | `			if( iP2 == 4 ){` |
|      - |  981 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|      - |  982 | `				 * right truth value AND skips its "Expecting a variable not` |
|      - |  983 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|     53 |  984 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     53 |  985 | `				PH7_MemObjRelease(pTos);` |
|     53 |  986 | `				pTos->nIdx = SXU32_HIGH;` |
|     53 |  987 | `				if( bExists ){` |
|     28 |  988 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     28 |  989 | `					pTos->x.iVal = 1;` |
|     16 |  990 | `				}else{` |
|     29 |  991 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  992 | `				}` |
|    157 |  993 | `			}else if( iP2 == 5 ){` |
|      - |  994 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|      - |  995 | `				 * vm_builtin_unset is a harmless no-op. */` |
|     20 |  996 | `				PH7_MemObjRelease(pTos);` |
|     20 |  997 | `				pTos->nIdx = SXU32_HIGH;` |
|     20 |  998 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    125 |  999 | `			}else if( iP2 == 6 \|\| iP2 == 8 ){` |
|      - | 1000 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|      - | 1001 | `				 * without calling offsetGet. If true, call offsetGet and` |
|      - | 1002 | `				 * push the value so PH7_builtin_empty evaluates emptiness.` |
|      - | 1003 | ``				 * `??` (8) needs the identical shape: NULL on a miss so the`` |
|      - | 1004 | `				 * coalesce takes the default, the real value on a hit. */` |
|     22 | 1005 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     22 | 1006 | `				PH7_MemObjRelease(&sResult);` |
|     22 | 1007 | `				PH7_MemObjRelease(pTos);` |
|     22 | 1008 | `				pTos->nIdx = SXU32_HIGH;` |
|     22 | 1009 | `				if( !bExists ){` |
|      8 | 1010 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 | 1011 | `				}else{` |
|     16 | 1012 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 1013 | `						"offsetGet",sizeof("offsetGet")-1);` |
|      - | 1014 | `					ph7_value sValue;` |
|     16 | 1015 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|     16 | 1016 | `					apArg[0] = pIdx;` |
|     16 | 1017 | `					if( pGet ){` |
|     16 | 1018 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      7 | 1019 | `					}` |
|     16 | 1020 | `					PH7_MemObjStore(&sValue,pTos);` |
|     16 | 1021 | `					PH7_MemObjRelease(&sValue);` |
|      - | 1022 | `				}` |
|     22 | 1023 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|     22 | 1024 | `				VM_EXIT_BREAK; /* skip the duplicate sResult release below */` |
|     97 | 1025 | `			}else if( iP2 == 3 ){` |
|      - | 1026 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|      - | 1027 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|      - | 1028 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|      - | 1029 | `				 *     and push NULL.` |
|      - | 1030 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|     10 | 1031 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     10 | 1032 | `				int bShouldArm = !bExists;` |
|      - | 1033 | `				ph7_value sValue;` |
|     10 | 1034 | `				PH7_MemObjRelease(&sResult);` |
|      - | 1035 | `				/* Reset any prior arming defensively */` |
|     10 | 1036 | `				VmCoalesceDisarm(pVm);` |
|     10 | 1037 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|     10 | 1038 | `				if( bExists ){` |
|      5 | 1039 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 1040 | `						"offsetGet",sizeof("offsetGet")-1);` |
|      5 | 1041 | `					apArg[0] = pIdx;` |
|      5 | 1042 | `					if( pGet ){` |
|      5 | 1043 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      2 | 1044 | `					}` |
|      5 | 1045 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|      3 | 1046 | `						bShouldArm = 1;` |
|      1 | 1047 | `					}` |
|      2 | 1048 | `				}` |
|     10 | 1049 | `				PH7_MemObjRelease(pTos);` |
|     10 | 1050 | `				pTos->nIdx = SXU32_HIGH;` |
|     10 | 1051 | `				if( bShouldArm ){` |
|      - | 1052 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|      - | 1053 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|      - | 1054 | `					 * intervening expression evaluation. */` |
|      8 | 1055 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      8 | 1056 | `					if( pIdx ){` |
|      8 | 1057 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|      3 | 1058 | `					}` |
|      8 | 1059 | `					pVm->pCoalesceObj = pInst;` |
|      8 | 1060 | `					pInst->iRef++;` |
|      8 | 1061 | `					pVm->bCoalesceArmed = 1;` |
|      5 | 1062 | `				}else{` |
|      3 | 1063 | `					PH7_MemObjStore(&sValue,pTos);` |
|      - | 1064 | `				}` |
|     10 | 1065 | `				PH7_MemObjRelease(&sValue);` |
|     10 | 1066 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|     10 | 1067 | `				VM_EXIT_BREAK;` |
|    ! 0 | 1068 | `			}else{` |
|      - | 1069 | `				/* offsetGet: replace pTos with the returned value. */` |
|     89 | 1070 | `				PH7_MemObjRelease(pTos);` |
|     89 | 1071 | `				PH7_MemObjStore(&sResult,pTos);` |
|     89 | 1072 | `				pTos->nIdx = SXU32_HIGH;` |
|      - | 1073 | `			}` |
|    153 | 1074 | `			PH7_MemObjRelease(&sResult);` |
|    153 | 1075 | `			if( pIdx ){` |
|    153 | 1076 | `				PH7_MemObjRelease(pIdx);` |
|     74 | 1077 | `			}` |
|    153 | 1078 | `			VM_EXIT_BREAK;` |
|      - | 1079 | `		}` |
|      - | 1080 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|      - | 1081 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|      3 | 1082 | `		if( pInst ){` |
|      - | 1083 | `			char zMsg[256];` |
|      3 | 1084 | `			SyString *pName = &pInst->pClass->sName;` |
|      4 | 1085 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 1086 | `				"Cannot use object of type %.*s as array",` |
|      2 | 1087 | `				(int)pName->nByte,pName->zString);` |
|      3 | 1088 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      3 | 1089 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      3 | 1090 | `			PH7_MemObjRelease(pTos);` |
|      3 | 1091 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 | 1092 | `			pTos->nIdx = SXU32_HIGH;` |
|      3 | 1093 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      - | 1094 | ``			/* `break` used to resume at the NEXT instruction: the catch ran and then`` |
|      - | 1095 | `			 * execution carried on inside the try block. */` |
|      5 | 1096 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - | 1097 | `		}` |
|    ! 0 | 1098 | `	}` |
| 159759 | 1099 | `	if( (iP2 == 1 \|\| iP2 == 3 \|\| iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     31 | 1100 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|      - | 1101 | `			ph7_value *pObj;` |
|     31 | 1102 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      - | 1103 | `				/* php 8 write-context auto-vivify rules: NULL converts to array` |
|      - | 1104 | `				 * silently; FALSE converts with the 8.1 deprecation; any other` |
|      - | 1105 | `				 * scalar base — int/float/true/resource — is php's catchable` |
|      - | 1106 | `				 * "Cannot use a scalar value as an array" Error and the variable` |
|      - | 1107 | `				 * stays untouched (pre-fix the base was silently CONVERTED,` |
|      - | 1108 | ``				 * corrupting e.g. `$i = 5; $i[0]++` into array(0 => 6); string`` |
|      - | 1109 | `				 * bases were intercepted by the string-offset paths above). */` |
|      - | 1110 | `				/* php auto-converts false to an array with an 8.1 DEPRECATION; PHL` |
|      - | 1111 | `				 * rejects it like any other scalar base (null still auto-vivifies —` |
|      - | 1112 | `				 * it is not a bool). */` |
|     31 | 1113 | `				if( (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0 ){` |
|      - | 1114 | `					SyBlob sErrMsg;` |
|      7 | 1115 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      7 | 1116 | `					SyBlobAppend(&sErrMsg,"Cannot use a scalar value as an array",` |
|      - | 1117 | `						sizeof("Cannot use a scalar value as an array")-1);` |
|      7 | 1118 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      7 | 1119 | `					if( pIdx ){` |
|      7 | 1120 | `						PH7_MemObjRelease(pIdx);` |
|      3 | 1121 | `					}` |
|      7 | 1122 | `					PH7_MemObjRelease(pTos);` |
|      7 | 1123 | `					pTos->nIdx = SXU32_HIGH;` |
|      7 | 1124 | `					VM_EXIT_BREAK;` |
|      - | 1125 | `				}` |
|     25 | 1126 | `				PH7_MemObjToHashmap(pObj);` |
|     25 | 1127 | `				PH7_MemObjLoad(pObj,pTos);` |
|     11 | 1128 | `			}` |
|     11 | 1129 | `		}` |
|     11 | 1130 | `	}` |
| 159753 | 1131 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|      - | 1132 | `	/* php DEPRECATES both a null offset and a lossy-float subscript (then normalizes` |
|      - | 1133 | `	 * "" / truncates). PHL now matches php on the NULL offset (deprecate + coerce to` |
|      - | 1134 | `	 * the "" key, §2) but still rejects the lossy-FLOAT subscript with a TypeError on` |
|      - | 1135 | `	 * a READ or WRITE (iP2 0/1) — the recorded non-deprecated-surface policy. Both` |
|      - | 1136 | ``	 * stay lenient in isset()/empty()/`??`/unset(), where a throw would be wrong. */`` |
|      - | 1137 | `	/* An object/array key is rejected in EVERY context, including isset()/empty()/` |
|      - | 1138 | `	 * unset() where php still throws (only the wording changes) — unlike the` |
|      - | 1139 | `	 * null/float deprecations below, which stay lenient there. A resource key is` |
|      - | 1140 | `	 * accepted with a warning and becomes its integer id. */` |
| 159753 | 1141 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|      - | 1142 | `		SyBlob sTypeMsg;` |
| 159729 | 1143 | `		if( VmOffsetTypeRejected(&(*pVm),pIdx,iP2,&sTypeMsg) ){` |
|     13 | 1144 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg));` |
|     13 | 1145 | `			PH7_MemObjRelease(pIdx);` |
|     13 | 1146 | `			PH7_MemObjRelease(pTos);` |
|     13 | 1147 | `			pTos->nIdx = SXU32_HIGH;` |
|     13 | 1148 | `			VM_EXIT_BREAK;` |
|      - | 1149 | `		}` |
| 159717 | 1150 | `		VmOffsetResourceWarn(&(*pVm),pIdx);` |
|  79856 | 1151 | `	}` |
| 159741 | 1152 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|      - | 1153 | `		/* php DEPRECATES a null offset in EVERY subscript context except unset()` |
|      - | 1154 | `		 * (iP2==5), then normalizes it to the "" key — read, write, isset (4),` |
|      - | 1155 | ``		 * empty (6), `??`/`??=` (3), the coalesce-chain probe (8), destructure`` |
|      - | 1156 | `		 * (2/7). Emit it here so all of them get it, not just read/write; the` |
|      - | 1157 | `		 * lookup/insert below casts NULL->"", and a plain read miss then warns` |
|      - | 1158 | ``		 * `Undefined array key ""` on the now-string pIdx (php's exact pair). */`` |
| 159717 | 1159 | `		if( (pIdx->iFlags & MEMOBJ_NULL) && iP2 != 5 ){` |
|     15 | 1160 | `			VmNullOffsetDeprecate(&(*pVm),pIdx);` |
|      7 | 1161 | `		}` |
|      - | 1162 | `		/* A lossy-FLOAT subscript stays a rejected TypeError on a READ or WRITE` |
|      - | 1163 | `		 * (iP2 0/1) — the recorded non-deprecated-surface policy (§2); the lenient` |
|      - | 1164 | `		 * contexts (isset/empty/??/unset) truncate quietly as php's value does. */` |
| 159712 | 1165 | `		if( (iP2 == 0 \|\| iP2 == 1)` |
|  97682 | 1166 | `		 && (pIdx->iFlags & MEMOBJ_REAL)` |
|  79864 | 1167 | `		 && pIdx->rVal != (ph7_real)(sxi64)pIdx->rVal ){` |
|      - | 1168 | `			SyBlob sErrMsg;` |
|      3 | 1169 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      3 | 1170 | `			SyBlobAppend(&sErrMsg,"Cannot access offset of type float on array",` |
|      - | 1171 | `				sizeof("Cannot access offset of type float on array")-1);` |
|      3 | 1172 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|      3 | 1173 | `			PH7_MemObjRelease(pIdx);` |
|      3 | 1174 | `			PH7_MemObjRelease(pTos);` |
|      3 | 1175 | `			pTos->nIdx = SXU32_HIGH;` |
|      3 | 1176 | `			VM_EXIT_BREAK;` |
|      - | 1177 | `		}` |
|  79855 | 1178 | `	}` |
| 159739 | 1179 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
| 159717 | 1180 | `		if( iP2 == 1 \|\| iP2 == 5 ){` |
|      - | 1181 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|      - | 1182 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|      - | 1183 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|      - | 1184 | `			 * NOT separate — that would defeat COW on every element read.` |
|      - | 1185 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|      - | 1186 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|   1427 | 1187 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|    711 | 1188 | `		}` |
|      - | 1189 | `		/* Point to the hashmap */` |
| 159717 | 1190 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
| 159717 | 1191 | `		if( pIdx ){` |
|      - | 1192 | `			/* Load the desired entry */` |
| 159715 | 1193 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|  79855 | 1194 | `		}` |
| 159717 | 1195 | `		if( iP2 == 3 ){` |
|      - | 1196 | `			/* Null coalescing assign peek mode: separate only when we will` |
|      - | 1197 | `			 * actually write back. If the looked-up value is non-null, the` |
|      - | 1198 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|      - | 1199 | `			 * the parent can stay shared. If the value is null or the key is` |
|      - | 1200 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|      - | 1201 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|      - | 1202 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|      - | 1203 | `			 * correct for the outermost write. */` |
|     21 | 1204 | `			int needWrite = (rc != SXRET_OK);` |
|     21 | 1205 | `			if( !needWrite && pNode ){` |
|     13 | 1206 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|     13 | 1207 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|      7 | 1208 | `					needWrite = 1;` |
|      3 | 1209 | `				}` |
|      6 | 1210 | `			}` |
|     21 | 1211 | `			if( needWrite ){` |
|     15 | 1212 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|     15 | 1213 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|      - | 1214 | `					/* The map was actually copied — re-lookup so pNode points` |
|      - | 1215 | `					 * into the new map's storage. */` |
|      7 | 1216 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      7 | 1217 | `					if( pIdx ){` |
|      7 | 1218 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|      3 | 1219 | `					}` |
|      3 | 1220 | `				}` |
|      7 | 1221 | `			}` |
|     10 | 1222 | `		}` |
| 159717 | 1223 | `		if( rc != SXRET_OK && (iP2 == 1 \|\| iP2 == 3 \|\| iP2 == 5) ){` |
|      - | 1224 | `			/* Create a new empty entry */` |
|    349 | 1225 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|    349 | 1226 | `			if( rc == SXRET_OK ){` |
|      - | 1227 | `				/* Point to the last inserted entry */` |
|    347 | 1228 | `				pNode = pMap->pLast;` |
|    175 | 1229 | `			}else{` |
|      - | 1230 | ``				/* An append lvalue (`$a[][...] = v`) whose saturated auto-index`` |
|      - | 1231 | `				 * is occupied threw php's catchable Error. Dispatch it here —` |
|      - | 1232 | `				 * falling through with a stale pMap->pLast is what silently` |
|      - | 1233 | `				 * overwrote $a[PHP_INT_MAX]. */` |
|      7 | 1234 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|      - | 1235 | `			}` |
|    172 | 1236 | `		}` |
|  79855 | 1237 | `	}` |
| 159732 | 1238 | `	if( rc != SXRET_OK && pIdx && (iP2 == 2 \|\| iP2 == 0)` |
|  45507 | 1239 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP)` |
|     16 | 1240 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|      - | 1241 | ``		/* `$a['k'] ?? $d` compiles its LHS as a plain read (iP2 == 0) followed`` |
|      - | 1242 | `		 * by NULLC/NULLC_JMP — php does NOT warn there, so peek ahead and stay` |
|      - | 1243 | `		 * silent (same guard the magic-accessor read path uses). */` |
|      - | 1244 | `		/* php warns when a missing key is READ (iP2 == 0) or destructured` |
|      - | 1245 | `		 * (iP2 == 2). isset/empty/??/unset (iP2 3-6) and write-context` |
|      - | 1246 | `		 * vivification (iP2 == 1) stay silent, as does a read on a non-array` |
|      - | 1247 | `		 * base (already diagnosed above). php prints an INT key bare and a` |
|      - | 1248 | `		 * STRING key quoted. */` |
|      - | 1249 | `		SyBlob sMsg;` |
|     27 | 1250 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     27 | 1251 | `		if( pIdx->iFlags & (MEMOBJ_STRING\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|      - | 1252 | `			SyString sKey;` |
|     25 | 1253 | `			if( (pIdx->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 1254 | `				PH7_MemObjToString(pIdx);` |
|    ! 0 | 1255 | `			}` |
|     25 | 1256 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|     25 | 1257 | `			SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);` |
|     15 | 1258 | `		}else{` |
|      3 | 1259 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 | 1260 | `				PH7_MemObjToInteger(pIdx);` |
|    ! 0 | 1261 | `			}` |
|      3 | 1262 | `			SyBlobFormat(&sMsg,"Undefined array key %qd",pIdx->x.iVal);` |
|      - | 1263 | `		}` |
|     27 | 1264 | `		SyBlobNullAppend(&sMsg);` |
|     27 | 1265 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|     27 | 1266 | `		SyBlobRelease(&sMsg);` |
|     11 | 1267 | `	}` |
| 159726 | 1268 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0` |
|  79871 | 1269 | `	 && (iP2 == 0 \|\| iP2 == 2)` |
|     16 | 1270 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|      - | 1271 | `		/* Subscripting a scalar base is a WARNING in php ("Trying to access array offset` |
|      - | 1272 | `		 * on int") that yields NULL. PH7 yielded NULL in silence. */` |
|     11 | 1273 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Trying to access array offset on %s",` |
|      3 | 1274 | `			VmArithTypeName(pTos));` |
|      3 | 1275 | `	}` |
| 159731 | 1276 | `	if( pIdx ){` |
| 159731 | 1277 | `		PH7_MemObjRelease(pIdx);` |
|  79863 | 1278 | `	}` |
| 159731 | 1279 | `	if( rc == SXRET_OK ){` |
|      - | 1280 | `		/* Load entry contents */` |
|  68745 | 1281 | `		if( pMap->iRef < 2 ){` |
|      - | 1282 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|      - | 1283 | `			 * of the entry value,rather than pointing to it.` |
|      - | 1284 | `			 */` |
|    143 | 1285 | `			pTos->nIdx = SXU32_HIGH;` |
|    143 | 1286 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|     73 | 1287 | `		}else{` |
|  68605 | 1288 | `			pTos->nIdx = pNode->nValIdx;` |
|  68605 | 1289 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|  68605 | 1290 | `			PH7_HashmapUnref(pMap);` |
|      - | 1291 | `		}` |
|  34375 | 1292 | `	}else{` |
|      - | 1293 | `		/* No such entry,load NULL */` |
|  90991 | 1294 | `		PH7_MemObjRelease(pTos);` |
|  90991 | 1295 | `		pTos->nIdx = SXU32_HIGH;` |
|      - | 1296 | `	}` |
| 159731 | 1297 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1298 | `	VM_EXIT_BREAK;` |
| 377054 | 1299 | `}` |
|      - | 1300 |  |
|      - | 1301 | `/*` |
|      - | 1302 | ` * OP_LOAD_MAP: body moved verbatim from the OP_LOAD_MAP arm of` |
|      - | 1303 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1304 | ` */` |
|  78026 | 1305 | `PH7_PRIVATE VmOpRc VmExecOpLoadMap(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1306 | `{` |
|  78031 | 1307 | `	ph7_value *pTos = pState->pTos;` |
|  78031 | 1308 | `	ph7_value *pStack = pState->pStack;` |
|  78031 | 1309 | `	VmInstr *aInstr = pState->aInstr;` |
|  78031 | 1310 | `	sxi32 pc = pState->pc;` |
|      - | 1311 | `	sxi32 rc;` |
|  39013 | 1312 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1313 | `	ph7_hashmap *pMap;` |
|      - | 1314 | `	/* Allocate a new hashmap instance */` |
|  78031 | 1315 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|  78031 | 1316 | `	if( pMap == 0 ){` |
|    ! 0 | 1317 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|    ! 0 | 1318 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|    ! 0 | 1319 | `		VM_EXIT_ABORT;` |
|      - | 1320 | `	}` |
|  78031 | 1321 | `	if( pInstr->iP1 > 0 ){` |
|  12145 | 1322 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|  12145 | 1323 | `		sxi32 rcSpread = SXRET_OK;` |
|      - | 1324 | `		/* Perform the insertion */` |
|  45265 | 1325 | `		while( pEntry < pTos ){` |
|  33143 | 1326 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|      - | 1327 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|      - | 1328 | `				 * semantics — string keys preserved (later wins), int keys` |
|      - | 1329 | `				 * renumbered. Same routine that backs array_merge. */` |
|    683 | 1330 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|    659 | 1331 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|    659 | 1332 | `					if( rcMerge != SXRET_OK ){` |
|      - | 1333 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|      - | 1334 | `						 * path — emit fatal and abort, leaving no partial` |
|      - | 1335 | `						 * map dangling. */` |
|    ! 0 | 1336 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|    ! 0 | 1337 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|    ! 0 | 1338 | `						rcSpread = PH7_ABORT;` |
|    ! 0 | 1339 | `						break;` |
|      1 | 1340 | `					}` |
|    354 | 1341 | `				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){` |
|      - | 1342 | `					/* Traversable unpacking (PHP 8.1): walk it into the map using the` |
|      - | 1343 | `					 * same key rules as array spread (string keys kept, int renumbered). */` |
|      5 | 1344 | `					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);` |
|      5 | 1345 | `					if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|    ! 0 | 1346 | `						rcSpread = rcW;` |
|    ! 0 | 1347 | `						break;` |
|      - | 1348 | `					}` |
|      3 | 1349 | `				}else{` |
|      - | 1350 | `					/* Throw a catchable Error matching PHP semantics. */` |
|     21 | 1351 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|     21 | 1352 | `					break;` |
|      1 | 1353 | `				}` |
|  32794 | 1354 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|      - | 1355 | `				/* Insertion by reference */` |
|    181 | 1356 | `				PH7_HashmapInsertByRef(pMap,` |
|    120 | 1357 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|    120 | 1358 | `					(sxu32)pEntry[1].x.iVal` |
|      - | 1359 | `					);` |
|     61 | 1360 | `			}else{` |
|      - | 1361 | `				/* An explicit key in an array LITERAL gets the same php diagnostics a` |
|      - | 1362 | `				 * subscript does — a float key that truncates deprecates, and so does an` |
|      - | 1363 | `				 * explicit null key. Only the subscript sites (LOAD_IDX/STORE_IDX) used to` |
|      - | 1364 | ``				 * emit these, so `[1.5 => "v"]` and `[null => "v"]` were silent. A key that`` |
|      - | 1365 | `				 * is ABSENT (auto-index) is a NULL slot here, not a null key, so the` |
|      - | 1366 | `				 * MEMOBJ_NULL check below is what tells the two apart. */` |
|  32343 | 1367 | `					if( (pEntry->iFlags & MEMOBJ_AUX_NOKEY) == 0 ){` |
|      - | 1368 | `						/* An object/array literal key is php's TypeError, a resource one` |
|      - | 1369 | `						 * warns and becomes its id — same rules as a subscript. */` |
|      - | 1370 | `						SyBlob sTypeMsg;` |
|  12347 | 1371 | `						if( VmOffsetTypeRejected(&(*pVm),pEntry,0,&sTypeMsg) ){` |
|      3 | 1372 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg));` |
|      2 | 1373 | `						}else{` |
|  12345 | 1374 | `							VmOffsetResourceWarn(&(*pVm),pEntry);` |
|      - | 1375 | `						}` |
|      - | 1376 | `						/* php DEPRECATES a null literal key (then normalizes to "") and` |
|      - | 1377 | `						 * rejects nothing there; PHL matches that (deprecate + fall` |
|      - | 1378 | `						 * through, PH7_HashmapInsert casts NULL->""). A lossy-FLOAT` |
|      - | 1379 | `						 * literal key still rejects with a TypeError — the recorded` |
|      - | 1380 | `						 * non-deprecated-surface policy, same as the subscript site. */` |
|  12347 | 1381 | `						int bNull = (pEntry->iFlags & MEMOBJ_NULL) != 0;` |
|  18519 | 1382 | `						int bLossyFloat = (pEntry->iFlags & MEMOBJ_REAL) != 0` |
|  12342 | 1383 | `							&& pEntry->rVal != (ph7_real)(sxi64)pEntry->rVal;` |
|  12347 | 1384 | `						if( bNull ){` |
|      3 | 1385 | `							VmNullOffsetDeprecate(&(*pVm),pEntry);` |
|  12346 | 1386 | `						}else if( bLossyFloat ){` |
|      3 | 1387 | `							const char *zErr = "Cannot access offset of type float on array";` |
|      - | 1388 | `							SyBlob sErrMsg;` |
|      3 | 1389 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      3 | 1390 | `							SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|      3 | 1391 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|      1 | 1392 | `						}` |
|   6171 | 1393 | `					}` |
|      - | 1394 | `				/* Standard insertion */` |
|  48512 | 1395 | `				PH7_HashmapInsert(pMap,` |
|  32338 | 1396 | `					(pEntry->iFlags & MEMOBJ_AUX_NOKEY) ? 0 /* Automatic index assign */ : pEntry,` |
|  16169 | 1397 | `					&pEntry[1]` |
|      - | 1398 | `				);` |
|      - | 1399 | `			}` |
|      - | 1400 | `			/* Next pair on the stack */` |
|  33125 | 1401 | `			pEntry += 2;` |
|      5 | 1402 | `		}` |
|      - | 1403 | `		/* Pop P1 elements */` |
|  12145 | 1404 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|  12145 | 1405 | `		if( rcSpread != SXRET_OK ){` |
|      - | 1406 | `			/* Discard the partially-built map and propagate the exception. */` |
|     21 | 1407 | `			PH7_HashmapRelease(pMap,TRUE);` |
|     21 | 1408 | `			if( rcSpread == PH7_ABORT ){` |
|    ! 0 | 1409 | `				VM_EXIT_ABORT;` |
|      - | 1410 | `			}` |
|      - | 1411 | `			{` |
|      - | 1412 | `				sxi32 iRp;` |
|     21 | 1413 | `				if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      6 | 1414 | `					pc = iRp;` |
|      6 | 1415 | `					VM_EXIT_BREAK;` |
|      - | 1416 | `				}` |
|      - | 1417 | `			}` |
|     15 | 1418 | `			VM_EXIT_EXCEPTION;` |
|      - | 1419 | `		}` |
|   6061 | 1420 | `	}` |
|      - | 1421 | `	/* Push the hashmap */` |
|  78013 | 1422 | `	pTos++;` |
|  78013 | 1423 | `	pTos->nIdx = SXU32_HIGH;` |
|  78013 | 1424 | `	pTos->x.pOther = pMap;` |
|  78013 | 1425 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|  78013 | 1426 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1427 | `	VM_EXIT_BREAK;` |
|  39018 | 1428 | `}` |
|      - | 1429 |  |
|      - | 1430 | `/*` |
|      - | 1431 | ` * OP_LOAD_LIST: body moved verbatim from the OP_LOAD_LIST arm of` |
|      - | 1432 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1433 | ` */` |
|    370 | 1434 | `PH7_PRIVATE VmOpRc VmExecOpLoadList(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1435 | `{` |
|    375 | 1436 | `	ph7_value *pTos = pState->pTos;` |
|    375 | 1437 | `	ph7_value *pStack = pState->pStack;` |
|    375 | 1438 | `	VmInstr *aInstr = pState->aInstr;` |
|    375 | 1439 | `	sxi32 pc = pState->pc;` |
|      - | 1440 | `	sxi32 rc;` |
|    185 | 1441 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1442 | `	ph7_value *pEntry;` |
|    375 | 1443 | `	sxi32 rcEnforce = SXRET_OK;` |
|    375 | 1444 | `	if( pInstr->iP1 <= 0 ){` |
|      - | 1445 | `		/* Empty list,break immediately */` |
|    ! 0 | 1446 | `		VM_EXIT_BREAK;` |
|      - | 1447 | `	}` |
|    375 | 1448 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|      - | 1449 | `#ifdef UNTRUST` |
|      - | 1450 | `	if( &pEntry[-1] < pStack ){` |
|      - | 1451 | `		VM_EXIT_ABORT;` |
|      - | 1452 | `	}` |
|      - | 1453 | `#endif` |
|    375 | 1454 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|    359 | 1455 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|      - | 1456 | `		ph7_hashmap_node *pNode;` |
|      - | 1457 | `		ph7_value sKey,*pObj;` |
|      - | 1458 | `		/* Start Copying */` |
|    359 | 1459 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|   1043 | 1460 | `		while( pEntry <= pTos ){` |
|    705 | 1461 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|    675 | 1462 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|    675 | 1463 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|   1281 | 1464 | `					int bTyped = SyHashTotalEntry(&pVm->hTypedSlot) > 0` |
|    670 | 1465 | `						&& SyHashGet(&pVm->hTypedSlot,(const void *)&pEntry->nIdx,sizeof(sxu32)) != 0;` |
|    675 | 1466 | `					if( rc != SXRET_OK ){` |
|      - | 1467 | `						/* Undefined array key */` |
|      - | 1468 | `						char zMsg[128];` |
|      5 | 1469 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      5 | 1470 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      2 | 1471 | `					}` |
|    675 | 1472 | `					if( !bTyped ){` |
|    645 | 1473 | `						if( rc == SXRET_OK ){` |
|      - | 1474 | `							/* Store node value */` |
|    645 | 1475 | `							PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|    325 | 1476 | `						}else{` |
|    ! 0 | 1477 | `							PH7_MemObjRelease(pObj);` |
|      - | 1478 | `						}` |
|    325 | 1479 | `					}else{` |
|      - | 1480 | ``						/* Typed/readonly property target (`[$o->p] = [...]`): a`` |
|      - | 1481 | `						 * direct slot write would bypass the typed-slot table, so` |
|      - | 1482 | `						 * enforce on a temp first — a TypeError leaves the property` |
|      - | 1483 | `						 * untouched, and a missing key assigns null, which a` |
|      - | 1484 | `						 * non-nullable type rejects exactly like php (warning, then` |
|      - | 1485 | `						 * "Cannot assign null to property ... of type ..."). */` |
|      - | 1486 | `						ph7_value sVal;` |
|     31 | 1487 | `						PH7_MemObjInit(&(*pVm),&sVal);` |
|     31 | 1488 | `						if( rc == SXRET_OK ){` |
|     27 | 1489 | `							PH7_HashmapExtractNodeValue(pNode,&sVal,TRUE);` |
|     13 | 1490 | `						}` |
|     31 | 1491 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),pEntry->nIdx,&sVal,0);` |
|     31 | 1492 | `						if( rcEnforce != SXRET_OK ){` |
|      - | 1493 | `							/* Thrown: stop assigning (php aborts the list at the` |
|      - | 1494 | `							 * first failing element), settle the stack, route. */` |
|     17 | 1495 | `							PH7_MemObjRelease(&sVal);` |
|     17 | 1496 | `							break;` |
|      - | 1497 | `						}` |
|     15 | 1498 | `						PH7_MemObjStore(&sVal,pObj);` |
|     15 | 1499 | `						PH7_MemObjRelease(&sVal);` |
|      - | 1500 | `					}` |
|    327 | 1501 | `				}` |
|    327 | 1502 | `			}` |
|    689 | 1503 | `			sKey.x.iVal++; /* Next numeric index */` |
|    689 | 1504 | `			pEntry++;` |
|      5 | 1505 | `		}` |
|    182 | 1506 | `	}else{` |
|      - | 1507 | `		/* Source is not an array: php warns first (silencing ONLY null — a bool` |
|      - | 1508 | `		 * source warns too, php 8), then assigns null to every target. A typed` |
|      - | 1509 | `		 * property target receives that null THROUGH enforcement, so a` |
|      - | 1510 | `		 * non-nullable type throws "Cannot assign null to property ..." exactly` |
|      - | 1511 | `		 * like php instead of silently nulling the slot. PHL DIVERGENCE: bool` |
|      - | 1512 | `		 * FALSE stays silent (php warns) — it is the end-of-array sentinel of` |
|      - | 1513 | `` 		 * the documented each() extension's `while (list(..) = each($a))` `` |
|      - | 1514 | `		 * idiom, which would otherwise warn on every normal loop exit. */` |
|      - | 1515 | `		ph7_value *pObj;` |
|     30 | 1516 | `		int bFalseSrc = (pTos[-pInstr->iP1].iFlags & MEMOBJ_BOOL) != 0` |
|     16 | 1517 | `			&& pTos[-pInstr->iP1].x.iVal == 0;` |
|     19 | 1518 | `		if( (pTos[-pInstr->iP1].iFlags & MEMOBJ_NULL) == 0 && !bFalseSrc ){` |
|     12 | 1519 | `			VmWarnCannotUseAsArray(&(*pVm),pTos[-pInstr->iP1].iFlags);` |
|      5 | 1520 | `		}` |
|     33 | 1521 | `		while( pEntry <= pTos ){` |
|     23 | 1522 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|     23 | 1523 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|     42 | 1524 | `					int bTyped = SyHashTotalEntry(&pVm->hTypedSlot) > 0` |
|     20 | 1525 | `						&& SyHashGet(&pVm->hTypedSlot,(const void *)&pEntry->nIdx,sizeof(sxu32)) != 0;` |
|     23 | 1526 | `					if( !bTyped ){` |
|     15 | 1527 | `						PH7_MemObjRelease(pObj);` |
|      9 | 1528 | `					}else{` |
|      - | 1529 | `						ph7_value sVal;` |
|      9 | 1530 | `						PH7_MemObjInit(&(*pVm),&sVal);` |
|      9 | 1531 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),pEntry->nIdx,&sVal,0);` |
|      9 | 1532 | `						if( rcEnforce != SXRET_OK ){` |
|      7 | 1533 | `							PH7_MemObjRelease(&sVal);` |
|      7 | 1534 | `							break;` |
|      - | 1535 | `						}` |
|      3 | 1536 | `						PH7_MemObjStore(&sVal,pObj);` |
|      3 | 1537 | `						PH7_MemObjRelease(&sVal);` |
|      - | 1538 | `					}` |
|      7 | 1539 | `				}` |
|      7 | 1540 | `			}` |
|     17 | 1541 | `			pEntry++;` |
|      3 | 1542 | `		}` |
|      - | 1543 | `	}` |
|    375 | 1544 | `	if( rcEnforce != SXRET_OK ){` |
|      - | 1545 | `		/* Settle this op's own operands: the P1 entries AND the source value —` |
|      - | 1546 | `		 * its statement-level OP_POP is skipped when a catch resumes at the` |
|      - | 1547 | `		 * landing pad, so leaving it would leak one operand slot per caught` |
|      - | 1548 | `		 * throw. A NESTED destructure can still have the outer list's operands` |
|      - | 1549 | `		 * abandoned above the try's base, so on an in-place catch drain to the` |
|      - | 1550 | `		 * catching try's recorded depth (like the fetch-point router and the` |
|      - | 1551 | `		 * generator inject path), not just our own pops. */` |
|     23 | 1552 | `		VmPopOperand(&pTos,pInstr->iP1 + 1);` |
|     23 | 1553 | `		if( rcEnforce == PH7_ABORT ){` |
|    ! 0 | 1554 | `			VM_EXIT_ABORT;` |
|      - | 1555 | `		}` |
|      - | 1556 | `		{` |
|      - | 1557 | `			sxi32 _iRpL;` |
|     34 | 1558 | `			PH7_INLINE_RESUME_BREAK()` |
|     23 | 1559 | `			if( VmRecordedResume(pVm,&_iRpL,pState->pEntryFrame,aInstr) ){` |
|     25 | 1560 | `				PH7_RESUME_DRAIN()` |
|     23 | 1561 | `				pc = _iRpL;` |
|     23 | 1562 | `				VM_EXIT_BREAK;` |
|      - | 1563 | `			}` |
|      - | 1564 | `		}` |
|    ! 0 | 1565 | `		VM_EXIT_EXCEPTION;` |
|      - | 1566 | `	}` |
|    353 | 1567 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|    353 | 1568 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1569 | `	VM_EXIT_BREAK;` |
|    190 | 1570 | `}` |
|      - | 1571 |  |
|      - | 1572 | `/*` |
|      - | 1573 | ` * OP_UNSET_VAR: body moved verbatim from the OP_UNSET_VAR arm of` |
|      - | 1574 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1575 | ` */` |
|   6904 | 1576 | `PH7_PRIVATE VmOpRc VmExecOpUnsetVar(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1577 | `{` |
|   6909 | 1578 | `	ph7_value *pTos = pState->pTos;` |
|   6909 | 1579 | `	ph7_value *pStack = pState->pStack;` |
|   6909 | 1580 | `	VmInstr *aInstr = pState->aInstr;` |
|   6909 | 1581 | `	sxi32 pc = pState->pc;` |
|      - | 1582 | `	sxi32 rc;` |
|   3452 | 1583 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1584 | `	/* unset($name): p3 is the variable name. Drops the NAME only — see VmUnsetVarByName */` |
|   6909 | 1585 | `	SyString *pName = (SyString *)pInstr->p3;` |
|   6909 | 1586 | `	if( pName && pVm->pFrame ){` |
|      - | 1587 | `		/* Inside a try{} the VM pushes an EXCEPTION frame; variables live in the body` |
|      - | 1588 | `		 * frame below it, so skip past it exactly as every other variable path does.` |
|      - | 1589 | `		 * Without this, unset($x) inside a try silently found nothing and did nothing. */` |
|   6909 | 1590 | `		VmFrame *pVarFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|   6909 | 1591 | `		sxi32 rcU = VmUnsetVarByName(&(*pVm),pVarFrame,pName->zString,pName->nByte);` |
|   6909 | 1592 | `		if( rcU == PH7_ABORT ){` |
|      3 | 1593 | `			VM_EXIT_ABORT;` |
|      - | 1594 | `		}` |
|      - | 1595 | `		/* Releasing the last holder can run a __destruct(), and that destructor may` |
|      - | 1596 | `		 * throw. Such a throw is PARKED in nBoundaryRc by the boundary rail; consume it` |
|      - | 1597 | `		 * here and route it, or the catch runs and execution resumes inside the try` |
|      - | 1598 | `		 * ("resumed-dtor" instead of php's "caught-dtor"). */` |
|   6907 | 1599 | `		if( pVm->nBoundaryRc != 0 ){` |
|      3 | 1600 | `			rc = pVm->nBoundaryRc;` |
|      3 | 1601 | `			pVm->nBoundaryRc = 0;` |
|      3 | 1602 | `			if( rc == PH7_ABORT ){` |
|    ! 0 | 1603 | `				VM_EXIT_ABORT;` |
|      - | 1604 | `			}` |
|      3 | 1605 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - | 1606 | `		}` |
|   3450 | 1607 | `	}` |
|   6905 | 1608 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1609 | `	VM_EXIT_BREAK;` |
|   3457 | 1610 | `}` |
|      - | 1611 |  |
