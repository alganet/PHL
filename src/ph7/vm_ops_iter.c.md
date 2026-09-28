# src/ph7/vm_ops_iter.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 295/349 lines (84.53%)

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
|      - |    9 | ` *    Iteration opcode handlers extracted from vm.c's dispatch loop. Each` |
|      - |   10 | ` *    handler runs one opcode arm against the caller's VmExecState: the loop` |
|      - |   11 | ` *    syncs pTos/pc in, calls the handler, reloads them and routes the` |
|      - |   12 | ` *    returned VmOpRc onto its labels (same idiom as VmCallFinish).` |
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
|      - |   25 | ` * OP_FOREACH_STEP: advance one foreach iteration (hashmap cursor, Iterator` |
|      - |   26 | ` * protocol, or object-attribute walk). Body moved verbatim from the` |
|      - |   27 | ` * OP_FOREACH_STEP arm of VmByteCodeExecBody; arm-terminal breaks became` |
|      - |   28 | ` * VM_EXIT_BREAK.` |
|      - |   29 | ` */` |
| 475983 |   30 | `PH7_PRIVATE VmOpRc VmExecOpForeachStep(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |   31 | `{` |
| 475988 |   32 | `	ph7_value *pTos = pState->pTos;` |
| 475988 |   33 | `	ph7_value *pStack = pState->pStack;` |
| 475988 |   34 | `	VmInstr *aInstr = pState->aInstr;` |
| 475988 |   35 | `	sxi32 pc = pState->pc;` |
|      - |   36 | `	sxi32 rc;` |
| 475988 |   37 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|      - |   38 | `	ph7_foreach_step **apStep,*pStep;` |
|      - |   39 | `	ph7_value *pValue;` |
|      - |   40 | `	VmFrame *pFrameLocal;` |
|      - |   41 | `	sxu32 nStep;` |
| 475988 |   42 | `	pFrameLocal = pVm->pFrame;` |
| 475988 |   43 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      - |   44 | `	/* Select THIS activation's step. aStep is per-STATEMENT and shared by every` |
|      - |   45 | `	 * activation, so peeking the last entry resumes onto a sibling's cursor when` |
|      - |   46 | `	 * two instances of one generator/fiber are suspended in the same textual` |
|      - |   47 | `	 * foreach. Scan from the top (most-recent push) for the step whose owning` |
|      - |   48 | `	 * frame matches the running activation; top-down makes the current push win` |
|      - |   49 | `	 * over any leaked older step that happens to share a recycled frame address. */` |
| 475988 |   50 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
| 475988 |   51 | `	nStep = SySetUsed(&pInfo->aStep);` |
| 475988 |   52 | `	if( nStep < 1 ){` |
|      - |   53 | `		/* Defensive: OP_FOREACH_INIT always pushes this activation's step before` |
|      - |   54 | `		 * STEP runs (and jumps past the loop when the push fails), so an empty` |
|      - |   55 | `		 * set is unreachable — guard the apStep[-1] read anyway. Jump out. */` |
|      3 |   56 | `		pc = pInstr->iP2 - 1;` |
|      3 |   57 | `		VM_EXIT_BREAK;` |
|      - |   58 | `	}` |
| 475986 |   59 | `	pStep = apStep[nStep - 1];` |
| 476012 |   60 | `	while( nStep > 0 ){` |
| 476012 |   61 | `		if( apStep[nStep - 1]->pFrame == pFrameLocal ){` |
| 475986 |   62 | `			pStep = apStep[nStep - 1];` |
| 475986 |   63 | `			break;` |
|      - |   64 | `		}` |
|     27 |   65 | `		nStep--;` |
|      1 |   66 | `	}` |
| 475986 |   67 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|      - |   68 | `		ph7_hashmap_node *pNode;` |
|      - |   69 | `		/* Extract the current node via this loop's PRIVATE cursor (php:` |
|      - |   70 | `		 * nested foreach over the same array are independent iterations) */` |
| 473187 |   71 | `		pNode = pStep->pCursor;` |
| 473187 |   72 | `		if( pNode == 0 ){` |
|      - |   73 | `			/* No more entry to process */` |
|  34589 |   74 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|      - |   75 | `			/* php does NOT break the binding: the value variable stays a reference to the` |
|      - |   76 | `` 			 * LAST element after the loop — that is what makes a second `foreach ($a as $v)` `` |
|      - |   77 | ``			 * write through it (the famous gotcha), and what the `unset($v)` idiom exists to`` |
|      - |   78 | `			 * undo. Deleting the name here left $v undefined instead. */` |
|      - |   79 | `			/* Cleanup the mess left behind */` |
|  34589 |   80 | `			VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,TRUE);` |
|  17297 |   81 | `		}else{` |
|      - |   82 | `			/* Advance the private cursor */` |
| 438603 |   83 | `			pStep->pCursor = pNode->pPrev; /* Reverse link */` |
|      - |   84 | `			/* Bind the VALUE before the KEY: on the first iteration this is where` |
|      - |   85 | `			 * both locals are created in the frame table, and php's symbol table` |
|      - |   86 | `			 * lists the value ahead of the key (get_defined_vars() order). Only the` |
|      - |   87 | `			 * creation ORDER matters here; the stored values are independent. */` |
| 438603 |   88 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|      - |   89 | `				/* Pass by reference — a REGISTERED binding (PH7_VmBindVarSlot), so the element` |
|      - |   90 | `				 * counts the loop variable as a holder for as long as it is bound, exactly as` |
|      - |   91 | `				 * php's reference does. */` |
|    213 |   92 | `				PH7_VmBindVarSlot(&(*pVm),pFrameLocal,SyStringData(&pInfo->sValue),` |
|     70 |   93 | `					SyStringLength(&pInfo->sValue),pNode->nValIdx);` |
|     73 |   94 | `			}else{` |
|      - |   95 | `				/* Make a copy of the entry value */` |
| 438463 |   96 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
| 438463 |   97 | `				if( pValue ){` |
| 438463 |   98 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
| 219233 |   99 | `				}` |
|      - |  100 | `			}` |
| 438603 |  101 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|  11604 |  102 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|  11604 |  103 | `				if( pKey ){` |
|  11604 |  104 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|   5807 |  105 | `				}` |
|   5807 |  106 | `			}` |
|      5 |  107 | `		}` |
| 239399 |  108 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|      - |  109 | `		/* Iterator-based iteration.` |
|      - |  110 | `		 * Sequence: on first call just check valid/current/key.` |
|      - |  111 | `		 * On subsequent calls, advance with next() first, then check.` |
|      - |  112 | `		 */` |
|   2662 |  113 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|      - |  114 | `		ph7_class_method *pMethod;` |
|      - |  115 | `		ph7_value sResult;` |
|   2662 |  116 | `		int isValid = 0;` |
|      - |  117 | `		/* Call next() to advance — but skip on the first iteration */` |
|   2662 |  118 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|    623 |  119 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|    314 |  120 | `		}else{` |
|   2044 |  121 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|   2044 |  122 | `			if( pMethod ){` |
|   2044 |  123 | `				rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|   2044 |  124 | `				if( VmIterCallThrew(rc) ){` |
|      - |  125 | `					/* next() threw (generator body / userland Iterator): tear the` |
|      - |  126 | `					 * step down like exhaustion does, then route the exception —` |
|      - |  127 | `					 * the loop must not silently end with execution continuing. */` |
|     19 |  128 | `					VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|     19 |  129 | `					PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  130 | `				}` |
|   1013 |  131 | `			}` |
|      - |  132 | `		}` |
|      - |  133 | `		/* Call valid() */` |
|   2648 |  134 | `		PH7_MemObjInit(pVm,&sResult);` |
|   2648 |  135 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|   2648 |  136 | `		if( pMethod ){` |
|   2648 |  137 | `			rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|   2648 |  138 | `			if( VmIterCallThrew(rc) ){` |
|      - |  139 | `				/* valid() threw: same teardown-and-route as next() above. */` |
|    ! 0 |  140 | `				PH7_MemObjRelease(&sResult);` |
|    ! 0 |  141 | `				VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    ! 0 |  142 | `				PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  143 | `			}` |
|   2648 |  144 | `			PH7_MemObjToBool(&sResult);` |
|   2648 |  145 | `			isValid = (sResult.x.iVal != 0);` |
|   1322 |  146 | `		}` |
|   2648 |  147 | `		PH7_MemObjRelease(&sResult);` |
|   2648 |  148 | `		if( !isValid ){` |
|      - |  149 | `			/* Iterator exhausted */` |
|    589 |  150 | `			pc = pInstr->iP2 - 1;` |
|      - |  151 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|    589 |  152 | `			VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    297 |  153 | `		}else{` |
|      - |  154 | `			/* Call current() to get value */` |
|   2064 |  155 | `			PH7_MemObjInit(pVm,&sResult);` |
|   2064 |  156 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|   2064 |  157 | `			if( pMethod ){` |
|   2064 |  158 | `				rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|   2064 |  159 | `				if( VmIterCallThrew(rc) ){` |
|      - |  160 | `					/* current() threw: same teardown-and-route as next() above. */` |
|    ! 0 |  161 | `					PH7_MemObjRelease(&sResult);` |
|    ! 0 |  162 | `					VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    ! 0 |  163 | `					PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  164 | `				}` |
|   1030 |  165 | `			}` |
|   2064 |  166 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   2064 |  167 | `			if( pValue ){` |
|   2064 |  168 | `				PH7_MemObjStore(&sResult,pValue);` |
|   1030 |  169 | `			}` |
|   2064 |  170 | `			PH7_MemObjRelease(&sResult);` |
|      - |  171 | `			/* Call key() if needed */` |
|   2064 |  172 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      - |  173 | `				ph7_value sKey;` |
|    673 |  174 | `				PH7_MemObjInit(pVm,&sKey);` |
|    673 |  175 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|    673 |  176 | `				if( pMethod ){` |
|    673 |  177 | `					rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|    673 |  178 | `					if( VmIterCallThrew(rc) ){` |
|      - |  179 | `						/* key() threw: same teardown-and-route as next() above. */` |
|    ! 0 |  180 | `						PH7_MemObjRelease(&sKey);` |
|    ! 0 |  181 | `						VmForeachStepAbandon(pVm,pInfo,pStep,pThis);` |
|    ! 0 |  182 | `						PH7_DISPATCH_ITER_RC(rc,0)` |
|    ! 0 |  183 | `					}` |
|    334 |  184 | `				}` |
|    673 |  185 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|    673 |  186 | `				if( pValue ){` |
|    673 |  187 | `					PH7_MemObjStore(&sKey,pValue);` |
|    334 |  188 | `				}` |
|    673 |  189 | `				PH7_MemObjRelease(&sKey);` |
|    334 |  190 | `			}` |
|      - |  191 | `		}` |
|   1327 |  192 | `	}else{` |
|    147 |  193 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|    147 |  194 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|      - |  195 | `		SyHashEntry *pEntry;` |
|      - |  196 | `		/* Point to the next attribute */` |
|    309 |  197 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    243 |  198 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    243 |  199 | `			if( PH7_ATTR_UNPRESENTED(pVmAttr) ){` |
|      - |  200 | `				/* A static property belongs to the CLASS, never to an object: php` |
|      - |  201 | `				 * iterates only the instance's own properties. PHL's instance` |
|      - |  202 | `				 * attribute table carries an entry for every declared member` |
|      - |  203 | `				 * (statics share the class slot), so it has to filter here — the` |
|      - |  204 | `				 * same test var_dump/get_object_vars/json/serialize already make. */` |
|    128 |  205 | `				continue;` |
|      - |  206 | `			}` |
|    112 |  207 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|     61 |  208 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|      3 |  209 | `				continue; /* virtual set-only property: iteration skips it (php) */` |
|      - |  210 | `			}` |
|    115 |  211 | `			if( PH7_ClassAttrUninitializedForRead(pVmAttr) ){` |
|     24 |  212 | `				continue; /* typed, never written: not there yet (php) */` |
|      - |  213 | `			}` |
|      - |  214 | `			/* Check access permission */` |
|    137 |  215 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|     88 |  216 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|     81 |  217 | `					break; /* Access is granted */` |
|      - |  218 | `			}` |
|      3 |  219 | `		}` |
|    147 |  220 | `		if( pEntry == 0 ){` |
|      - |  221 | `			/* Clean up the mess left behind */` |
|     71 |  222 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|      - |  223 | `			/* The binding survives the loop (see the hashmap step) */` |
|     71 |  224 | `			VmForeachStepUnlink(pInfo,pStep);` |
|     71 |  225 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     71 |  226 | `			PH7_ClassInstanceUnref(pThis);` |
|     38 |  227 | `		}else{` |
|     81 |  228 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|      - |  229 | `			ph7_value *pAttrValue;` |
|     81 |  230 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|      - |  231 | `				/* Fill with the current attribute name. A MANGLED name — only the` |
|      - |  232 | `				 * __PHP_Incomplete_Class carrier stores those — yields its plain` |
|      - |  233 | `				 * part: php's iterator unmangles the key it hands out. */` |
|     77 |  234 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|     77 |  235 | `				if( pKey ){` |
|      - |  236 | `					SyString sUnmCls, sUnmName;` |
|     77 |  237 | `					SyStringInitFromBuf(&sUnmName,pAttrName->zString,pAttrName->nByte);` |
|     77 |  238 | `					if( pAttrName->nByte > 0 && pAttrName->zString[0] == 0 ){` |
|      5 |  239 | `						PH7_UnmangleAttrName(pAttrName->zString,pAttrName->nByte,&sUnmCls,&sUnmName);` |
|      2 |  240 | `					}` |
|     77 |  241 | `					SyBlobReset(&pKey->sBlob);` |
|     77 |  242 | `					SyBlobAppend(&pKey->sBlob,sUnmName.zString,sUnmName.nByte);` |
|     77 |  243 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|     36 |  244 | `				}` |
|     36 |  245 | `			}` |
|     76 |  246 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET))` |
|     44 |  247 | `			 && (pStep->iFlags & PH7_4EACH_STEP_REF)` |
|     12 |  248 | `			 && !VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName) ){` |
|      - |  249 | `				/* php: a hooked property (virtual or backed) cannot be iterated` |
|      - |  250 | `				 * by reference — catchable Error. Tear the step down like the` |
|      - |  251 | `				 * exhausted-iteration path (break the by-ref binding, unlink,` |
|      - |  252 | `				 * free, drop the instance retain) so nothing leaks and a` |
|      - |  253 | `				 * re-entered foreach starts fresh; the fetch-point router lands` |
|      - |  254 | `				 * the parked throw right after this op. Inside the property's` |
|      - |  255 | `				 * own hook body the guard keeps raw semantics (no Error). */` |
|      - |  256 | `				SyBlob sErrMsg;` |
|      3 |  257 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      3 |  258 | `				SyBlobFormat(&sErrMsg,"Cannot create reference to property %z::$%z",` |
|      2 |  259 | `					&pThis->pClass->sName,&pVmAttr->pAttr->sName);` |
|      3 |  260 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      3 |  261 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|      3 |  262 | `				VmForeachStepUnlink(pInfo,pStep);` |
|      3 |  263 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      3 |  264 | `				PH7_ClassInstanceUnref(pThis);` |
|      3 |  265 | `				VM_EXIT_BREAK;` |
|      - |  266 | `			}` |
|     74 |  267 | `			if( (pStep->iFlags & PH7_4EACH_STEP_REF) == 0` |
|     76 |  268 | `			 && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) != 0 ){` |
|      - |  269 | `				/* PHP 8.4 property hooks: object iteration reads through the` |
|      - |  270 | `				 * get hook (virtual properties included; the flag gate keeps` |
|      - |  271 | `				 * hook-free classes on the raw zero-copy path below). The` |
|      - |  272 | `				 * object step walks hAttr with the hash's single EMBEDDED` |
|      - |  273 | `				 * cursor — save/restore it around the dispatch so a hook that` |
|      - |  274 | `				 * re-enters an hAttr walk on this instance (get_object_vars,` |
|      - |  275 | `				 * json_encode of $this) can't truncate THIS iteration. A hook` |
|      - |  276 | `				 * that unset()s the property the saved cursor points at stays` |
|      - |  277 | `				 * a recorded hazard (php's own semantics there are murky). */` |
|      - |  278 | `				ph7_value sHookVal;` |
|      - |  279 | `				sxi32 rcHk;` |
|     11 |  280 | `				SyHashEntry_Pr *pSavedCur = pThis->hAttr.pCurrent;` |
|     11 |  281 | `				PH7_MemObjInit(pVm,&sHookVal);` |
|     11 |  282 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|     11 |  283 | `				pThis->hAttr.pCurrent = pSavedCur;` |
|     11 |  284 | `				if( rcHk != SXERR_NOTFOUND ){` |
|     11 |  285 | `					if( rcHk == SXRET_OK ){` |
|     11 |  286 | `						pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|     11 |  287 | `						if( pValue ){` |
|     11 |  288 | `							PH7_MemObjStore(&sHookVal,pValue);` |
|      5 |  289 | `						}` |
|      5 |  290 | `					}` |
|      - |  291 | `					/* a throw parked on the boundary rail: the fetch-point` |
|      - |  292 | `					 * router lands it right after this op */` |
|     11 |  293 | `					PH7_MemObjRelease(&sHookVal);` |
|     11 |  294 | `					VM_EXIT_BREAK;` |
|      - |  295 | `				}` |
|    ! 0 |  296 | `				PH7_MemObjRelease(&sHookVal);` |
|    ! 0 |  297 | `			}` |
|      - |  298 | `			/* Extract attribute value */` |
|     69 |  299 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|     69 |  300 | `			if( pAttrValue ){` |
|     69 |  301 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|      - |  302 | `					/* Pass by reference (registered — see the hashmap step) */` |
|     10 |  303 | `					PH7_VmBindVarSlot(&(*pVm),pFrameLocal,SyStringData(&pInfo->sValue),` |
|      3 |  304 | `						SyStringLength(&pInfo->sValue),pVmAttr->nIdx);` |
|      4 |  305 | `				}else{` |
|      - |  306 | `					/* Make a copy of the attribute value */` |
|     63 |  307 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|     63 |  308 | `					if( pValue ){` |
|     63 |  309 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|     29 |  310 | `					}` |
|      - |  311 | `				}` |
|     32 |  312 | `			}` |
|      - |  313 | `		}` |
|      - |  314 | `	}` |
| 475960 |  315 | `	VM_EXIT_BREAK;` |
| 238001 |  316 | `}` |
|      - |  317 |  |
|      - |  318 | `/*` |
|      - |  319 | ` * OP_FOREACH_INIT: body moved verbatim from the OP_FOREACH_INIT arm of` |
|      - |  320 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  321 | ` */` |
|  35432 |  322 | `PH7_PRIVATE VmOpRc VmExecOpForeachInit(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  323 | `{` |
|  35437 |  324 | `	ph7_value *pTos = pState->pTos;` |
|  35437 |  325 | `	ph7_value *pStack = pState->pStack;` |
|  35437 |  326 | `	VmInstr *aInstr = pState->aInstr;` |
|  35437 |  327 | `	sxi32 pc = pState->pc;` |
|      - |  328 | `	sxi32 rc;` |
|  35437 |  329 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|      - |  330 | `	void *pName;` |
|      - |  331 | `#ifdef UNTRUST` |
|      - |  332 | `	if( pTos < pStack ){` |
|      - |  333 | `		VM_EXIT_ABORT;` |
|      - |  334 | `	}` |
|      - |  335 | `#endif` |
|  35437 |  336 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|      - |  337 | `		/* Take the variable name from the top of the stack */` |
|    ! 0 |  338 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  339 | `			/* Force a string cast */` |
|    ! 0 |  340 | `			PH7_MemObjToString(pTos);` |
|    ! 0 |  341 | `		}` |
|      - |  342 | `		/* Duplicate name */` |
|    ! 0 |  343 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    ! 0 |  344 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  345 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  346 | `		}` |
|    ! 0 |  347 | `		VmPopOperand(&pTos,1);` |
|    ! 0 |  348 | `	}` |
|  35437 |  349 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|    ! 0 |  350 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  351 | `			/* Force a string cast */` |
|    ! 0 |  352 | `			PH7_MemObjToString(pTos);` |
|    ! 0 |  353 | `		}` |
|      - |  354 | `		/* Duplicate name */` |
|    ! 0 |  355 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    ! 0 |  356 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  357 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  358 | `		}` |
|    ! 0 |  359 | `		VmPopOperand(&pTos,1);` |
|    ! 0 |  360 | `	}` |
|  35437 |  361 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_REF) && (pTos->iFlags & MEMOBJ_AUX_STROFFSET) ){` |
|      - |  362 | ``		/* `foreach ($s[0] as &$v)`: the subject is a string OFFSET and the loop wants`` |
|      - |  363 | `		 * to ALIAS its elements. php screens that before it asks whether the subject is` |
|      - |  364 | `		 * iterable at all, and a string offset is never a reference — same Error the` |
|      - |  365 | ``		 * `=&` and by-ref-argument paths raise, not the not-iterable warning PHL`` |
|      - |  366 | `		 * answered (whose slot index is the BASE STRING's, so binding would have` |
|      - |  367 | `		 * aliased the whole string). */` |
|      3 |  368 | `		sxi32 rcSo = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|      - |  369 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|      3 |  370 | `		if( rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  371 | `		PH7_THROW_ROUTE_MIDEXPR(rcSo)` |
|      - |  372 | `	}` |
|  35435 |  373 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_REF) && SyStringLength(&pInfo->sValue) > 0 ){` |
|      - |  374 | ``		/* `foreach ($o->p as &$v)` on a property no write may reach: the loop hands`` |
|      - |  375 | `		 * out an ALIAS of every element, so php screens the property where it` |
|      - |  376 | `		 * screens a store -- and it screens it BEFORE deciding the subject is` |
|      - |  377 | `		 * iterable at all, which is why an int or a string property is this Error` |
|      - |  378 | `		 * there and a "must be of type array\|object" warning here. PHL walked an` |
|      - |  379 | `		 * array one and let the body rewrite it. */` |
|     73 |  380 | `		sxi32 rcInd = PH7_VmCheckIndirectModify(&(*pVm),pTos->nIdx);` |
|     73 |  381 | `		if( rcInd != SXRET_OK ){` |
|      7 |  382 | `			if( rcInd == PH7_ABORT ){ VM_EXIT_ABORT; }` |
|      7 |  383 | `			PH7_THROW_ROUTE_MIDEXPR(rcInd)` |
|      - |  384 | `		}` |
|     32 |  385 | `	}` |
|      - |  386 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|  35429 |  387 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|      - |  388 | `		/* Jump out of the loop */` |
|      7 |  389 | `		if( SyStringLength(&pInfo->sValue) > 0 ){` |
|      - |  390 | `			/* php warns for EVERY non-iterable, null included (PH7 exempted null). */` |
|     10 |  391 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      - |  392 | `				"foreach() argument must be of type array\|object, %s given",` |
|      3 |  393 | `				VmArithValueName(pTos));` |
|      3 |  394 | `		}` |
|      7 |  395 | `		pc = pInstr->iP2 - 1;` |
|      4 |  396 | `	}else{` |
|      - |  397 | `		ph7_foreach_step *pStep;` |
|  35423 |  398 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|  35423 |  399 | `		if( pStep == 0 ){` |
|    ! 0 |  400 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      - |  401 | `			/* Jump out of the loop */` |
|    ! 0 |  402 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  403 | `		}else{` |
|      - |  404 | `			/* Zero the structure */` |
|  35423 |  405 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|      - |  406 | `			/* Prepare the step */` |
|  35423 |  407 | `			pStep->iFlags = pInfo->iFlags;` |
|      - |  408 | `			/* Record the owning activation so OP_FOREACH_STEP can pick THIS` |
|      - |  409 | `			 * activation's step out of the per-statement stack — two suspended` |
|      - |  410 | `			 * generator/fiber instances (or a recursive call) paused in the same` |
|      - |  411 | `			 * textual foreach otherwise resume onto each other's cursor. */` |
|  35423 |  412 | `			pStep->pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  35423 |  413 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      - |  414 | `				ph7_hashmap *pMap,*pIterMap;` |
|      - |  415 | `				/* COW: For by-reference foreach, eagerly separate the` |
|      - |  416 | `				 * source array so mutations don't affect other sharers. */` |
|  34703 |  417 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|     55 |  418 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|     55 |  419 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|     55 |  420 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  421 | `						/* Only adjust refcounts/separate if the backing` |
|      - |  422 | `						 * variable still points at the same hashmap as` |
|      - |  423 | `						 * the stack value. */` |
|     55 |  424 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|     55 |  425 | `							pCur->iRef--;` |
|      - |  426 | `							/* Use the returned map, not pBacking->x.pOther: PH7_HashmapDup` |
|      - |  427 | `							 * inside CowSeparate can reallocate (move) pVm->aMemObj and leave` |
|      - |  428 | `							 * pBacking dangling. The return value is the post-separation map. */` |
|     55 |  429 | `							pTos->x.pOther = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|     55 |  430 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|     26 |  431 | `						}` |
|     26 |  432 | `					}` |
|     26 |  433 | `				}` |
|  34703 |  434 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|  34703 |  435 | `				pIterMap = pMap;` |
|  34703 |  436 | `				if( pMap == pVm->pGlobal && (pStep->iFlags & PH7_4EACH_STEP_REF) == 0 ){` |
|      - |  437 | `					/* php 8.1: foreach ($GLOBALS as ...) by value iterates a` |
|      - |  438 | `					 * SNAPSHOT of the symbol table — globals created inside` |
|      - |  439 | `					 * the loop body must not be visited (the live map would` |
|      - |  440 | `					 * grow under the cursor). By-ref foreach keeps the live` |
|      - |  441 | `					 * map, like php. On OOM fall back to the live map. */` |
|      5 |  442 | `					ph7_hashmap *pSnap = PH7_NewHashmap(&(*pVm),0,0);` |
|      5 |  443 | `					if( pSnap && PH7_HashmapDupMaterialized(pMap,pSnap) == SXRET_OK ){` |
|      - |  444 | `						/* The step consumes the snapshot's initial reference */` |
|      5 |  445 | `						pIterMap = pSnap;` |
|      2 |  446 | `					}else if( pSnap ){` |
|    ! 0 |  447 | `						PH7_HashmapUnref(pSnap);` |
|    ! 0 |  448 | `					}` |
|      2 |  449 | `				}` |
|  34703 |  450 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|  34703 |  451 | `				pStep->xIter.pMap = pIterMap;` |
|  34703 |  452 | `				if( pIterMap == pMap ){` |
|  34699 |  453 | `					pMap->iRef++;` |
|  17347 |  454 | `				}` |
|      - |  455 | `				/* Private cursor + registry (php: nested foreach over one` |
|      - |  456 | `				 * array are independent; foreach never moves the internal` |
|      - |  457 | `				 * pointer — see PH7_HashmapRegisterForeachStep) */` |
|  34703 |  458 | `				PH7_HashmapRegisterForeachStep(pIterMap,pStep);` |
|  17354 |  459 | `			}else{` |
|    725 |  460 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|      - |  461 | `				ph7_class *pIteratorClass;` |
|      - |  462 | `				/* Check if the object implements Iterator */` |
|    725 |  463 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|    725 |  464 | `				if( PH7_VmGeneratorIsClosed(&(*pVm),pThis) ){` |
|      - |  465 | `					/* php refuses to START a foreach over a generator that has already` |
|      - |  466 | `					 * run to its end, and says so BEFORE the rewind that would report` |
|      - |  467 | `					 * the coarser "already run". PHL walked an EMPTY loop instead, so a` |
|      - |  468 | `					 * second foreach over the same generator silently did nothing. */` |
|      3 |  469 | `					SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      3 |  470 | `					pStep = 0;` |
|      3 |  471 | `					rc = VmThrowFromVm(&(*pVm),"Exception",` |
|      - |  472 | `						"Cannot traverse an already closed generator",` |
|      - |  473 | `						(sxu32)sizeof("Cannot traverse an already closed generator")-1);` |
|     19 |  474 | `					PH7_DISPATCH_ITER_RC(rc,1)` |
|    912 |  475 | `				}else if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|      - |  476 | `					/* Iterator-based iteration: call rewind() */` |
|      - |  477 | `					ph7_class_method *pRewind;` |
|    395 |  478 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|    395 |  479 | `					pStep->xIter.pThis = pThis;` |
|    395 |  480 | `					pThis->iRef++;` |
|    395 |  481 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|    395 |  482 | `					if( pRewind ){` |
|    395 |  483 | `						rc = PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|    395 |  484 | `						if( VmIterCallThrew(rc) ){` |
|      - |  485 | `							/* rewind() threw (a generator body or userland Iterator):` |
|      - |  486 | `							 * undo this step's retain, drop the step, and route the` |
|      - |  487 | `							 * exception instead of silently starting the loop. */` |
|     18 |  488 | `							pThis->iRef--;` |
|     18 |  489 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     18 |  490 | `							pStep = 0;` |
|     18 |  491 | `							PH7_DISPATCH_ITER_RC(rc,1)` |
|    ! 0 |  492 | `						}` |
|    188 |  493 | `					}` |
|    193 |  494 | `				}else{` |
|      - |  495 | `					/* Check if the object implements IteratorAggregate */` |
|      - |  496 | `					ph7_class *pIterAggClass;` |
|    333 |  497 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|      - |  498 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|    454 |  499 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|      - |  500 | `						/* Call getIterator() and use the returned Iterator object */` |
|      - |  501 | `						ph7_class_method *pGetIter;` |
|    264 |  502 | `						int iterAggOk = 0;` |
|    264 |  503 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|    264 |  504 | `						if( pGetIter ){` |
|      - |  505 | `							ph7_value sResult;` |
|    264 |  506 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|    264 |  507 | `							rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|    264 |  508 | `							if( VmIterCallThrew(rc) ){` |
|      - |  509 | `								/* getIterator() threw: drop the step and route the` |
|      - |  510 | `								 * exception (don't pile the "must implement Iterator"` |
|      - |  511 | `								 * error on top of it). */` |
|      9 |  512 | `								PH7_MemObjRelease(&sResult);` |
|      9 |  513 | `								SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      9 |  514 | `								pStep = 0;` |
|     14 |  515 | `								PH7_DISPATCH_ITER_RC(rc,1)` |
|    ! 0 |  516 | `							}` |
|    256 |  517 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|    256 |  518 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|    256 |  519 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|      - |  520 | `									ph7_class_method *pRewind;` |
|    256 |  521 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|    256 |  522 | `									pStep->xIter.pThis = pIterObj;` |
|    256 |  523 | `									pIterObj->iRef++;` |
|      - |  524 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|    256 |  525 | `									pStep->pOwner = pThis;` |
|    256 |  526 | `									pThis->iRef++;` |
|    256 |  527 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|    256 |  528 | `									if( pRewind ){` |
|    256 |  529 | `										rc = PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|    256 |  530 | `										if( VmIterCallThrew(rc) ){` |
|      - |  531 | `											/* The aggregate's iterator rewind() threw: undo` |
|      - |  532 | `											 * both retains, drop the step, route the exception. */` |
|     11 |  533 | `											pIterObj->iRef--;` |
|     11 |  534 | `											pThis->iRef--;` |
|     11 |  535 | `											PH7_MemObjRelease(&sResult);` |
|     11 |  536 | `											SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     11 |  537 | `											pStep = 0;` |
|     11 |  538 | `											PH7_DISPATCH_ITER_RC(rc,1)` |
|    ! 0 |  539 | `										}` |
|    121 |  540 | `									}` |
|    246 |  541 | `									iterAggOk = 1;` |
|    121 |  542 | `								}` |
|    121 |  543 | `							}` |
|    246 |  544 | `							PH7_MemObjRelease(&sResult);` |
|    121 |  545 | `						}` |
|    246 |  546 | `						if( !iterAggOk ){` |
|      - |  547 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|    ! 0 |  548 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |  549 | `								"Object returned by getIterator() must implement Iterator");` |
|    ! 0 |  550 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    ! 0 |  551 | `							pStep = 0; /* Signal: do not store this step */` |
|    ! 0 |  552 | `							pc = pInstr->iP2 - 1;` |
|    ! 0 |  553 | `						}` |
|    125 |  554 | `					}else{` |
|      - |  555 | `						/* Plain object iteration via hAttr */` |
|     73 |  556 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|     73 |  557 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|     73 |  558 | `						pStep->xIter.pThis = pThis;` |
|     73 |  559 | `						pThis->iRef++;` |
|      - |  560 | `					}` |
|      - |  561 | `				}` |
|      - |  562 | `			}` |
|      - |  563 | `		}` |
|  35391 |  564 | `		if( pStep ){` |
|  35389 |  565 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|    ! 0 |  566 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|    ! 0 |  567 | `				if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|    ! 0 |  568 | `					VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,FALSE/*never made it onto aStep*/);` |
|    ! 0 |  569 | `				}else{` |
|    ! 0 |  570 | `					SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      - |  571 | `				}` |
|      - |  572 | `				/* Jump out of the loop */` |
|    ! 0 |  573 | `				pc = pInstr->iP2 - 1;` |
|    ! 0 |  574 | `			}` |
|  17692 |  575 | `		}` |
|      - |  576 | `	}` |
|  35397 |  577 | `	VmPopOperand(&pTos,1);` |
|  35397 |  578 | `	VM_EXIT_BREAK;` |
|    ! 0 |  579 | `	VM_EXIT_BREAK;` |
|  17721 |  580 | `}` |
|      - |  581 |  |
