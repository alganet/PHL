# src/ph7/vm_builtin_ob.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 504/538 lines (93.68%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|     - |    7 | `/*` |
|     - |    8 | ` * Output buffering, php's model.` |
|     - |    9 | ` *` |
|     - |   10 | ` * A buffer holds the RAW bytes written into it and its handler runs on the way` |
|     - |   11 | ` * OUT — at a flush, a clean, or when the buffer is removed — not on the way in.` |
|     - |   12 | ` * The engine used to filter at WRITE time, which is a different program in four` |
|     - |   13 | ` * visible ways: ob_get_contents() answered filtered text php answers raw,` |
|     - |   14 | ``  * a handler saw one call per echo instead of one per operation, the `$phase` `` |
|     - |   15 | ` * argument that tells it WHICH operation was always 0, and $chunk_size (which` |
|     - |   16 | ` * only means anything to a write-out) was declared and never read.` |
|     - |   17 | ` *` |
|     - |   18 | ` * VmObPerform() is that operation: it takes the raw bytes out of the buffer, runs` |
|     - |   19 | ` * the handler over them once with the phase php would pass, and hands the answer` |
|     - |   20 | ` * to VmObDeliver(), which is the buffer BELOW or the real output.` |
|     - |   21 | ` */` |
|     - |   22 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry);` |
|     - |   23 | `static sxi32 VmObDeliver(ph7_vm *pVm,sxu32 nIdx,const void *pData,sxu32 nLen);` |
|     - |   24 | `static sxi32 VmObSink(ph7_vm *pVm,sxi32 iIdx,const void *pData,sxu32 nLen);` |
|     - |   25 | `static ph7_int64 VmObInitSize(VmObEntry *pEntry);` |
|     - |   26 | `static void VmObGrow(VmObEntry *pEntry,sxu32 nIncoming);` |
|     - |   27 | `/*` |
|     - |   28 | ` * TRUE while an output handler's own body is running.` |
|     - |   29 | ` *` |
|     - |   30 | ` * php discards everything a handler prints and refuses every ob call that would` |
|     - |   31 | ` * mutate the stack under it. The test is not merely "a handler is running": when` |
|     - |   32 | ` * the handler THROWS, this engine runs the enclosing catch IN PLACE, before the` |
|     - |   33 | ` * dispatch call returns — and that catch is ordinary code in the frame that` |
|     - |   34 | ` * called ob_flush(), so what IT prints belongs in the buffer and the ob calls it` |
|     - |   35 | ` * makes are allowed. Comparing the running frame against the one that entered` |
|     - |   36 | ` * the handler tells the two apart.` |
|     - |   37 | ` */` |
| 98858 |   38 | `static int VmObInHandler(ph7_vm *pVm)` |
|     5 |   39 | `{` |
|     - |   40 | `	VmFrame *pCur;` |
| 98863 |   41 | `	if( pVm->nObDepth < 1 ){` |
| 98813 |   42 | `		return 0;` |
|     - |   43 | `	}` |
|    53 |   44 | `	if( (pVm->pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|     - |   45 | `		/* The handler's own body — a php function running in its own frame, or a C` |
|     - |   46 | `		 * builtin handler running in the caller's, which pushes none at all. */` |
|    53 |   47 | `		return 1;` |
|     - |   48 | `	}` |
|     - |   49 | `	/* A CATCH body, which runs in an exception frame. This engine runs the catch` |
|     - |   50 | `	 * for a throw inside the handler IN PLACE, before the dispatch call returns, so` |
|     - |   51 | `	 * a catch is only "inside the handler" when it belongs to a frame BELOW the one` |
|     - |   52 | `	 * that entered it — the handler catching its own throw. A catch in that frame,` |
|     - |   53 | `	 * or in any frame above it, is ordinary code: what it prints belongs in the` |
|     - |   54 | `	 * buffer and the ob calls it makes are allowed. */` |
|   ! 0 |   55 | `	pCur = VmSkipExceptionFrames(pVm->pFrame);` |
|   ! 0 |   56 | `	if( pCur == pVm->pObFrame ){` |
|   ! 0 |   57 | `		return 0;` |
|     - |   58 | `	}` |
|   ! 0 |   59 | `	while( pCur ){` |
|   ! 0 |   60 | `		if( pCur == pVm->pObFrame ){` |
|   ! 0 |   61 | `			return 1;` |
|     - |   62 | `		}` |
|   ! 0 |   63 | `		pCur = pCur->pParent;` |
|   ! 0 |   64 | `	}` |
|   ! 0 |   65 | `	return 0;` |
| 49434 |   66 | `}` |
|     - |   67 | `/*` |
|     - |   68 | ` * How many buffers the ob functions can SEE right now. php truncates the stack at` |
|     - |   69 | ` * the buffer whose handler is running: from inside one, ob_get_level() answers` |
|     - |   70 | ` * that buffer's level and ob_get_contents() its bytes, not those of whatever was` |
|     - |   71 | ` * stacked on top of it (reachable when a chunked buffer writes out while an inner` |
|     - |   72 | ` * buffer is open).` |
|     - |   73 | ` */` |
|   406 |   74 | `static sxu32 VmObVisible(ph7_vm *pVm)` |
|     4 |   75 | `{` |
|   410 |   76 | `	sxu32 nUsed = SySetUsed(&pVm->aOB);` |
|   410 |   77 | `	if( VmObInHandler(pVm) && pVm->nObActive > 0 && pVm->nObActive < nUsed ){` |
|     9 |   78 | `		return pVm->nObActive;` |
|     - |   79 | `	}` |
|   402 |   80 | `	return nUsed;` |
|   207 |   81 | `}` |
|     - |   82 | `static sxi32 VmObPerform(ph7_vm *pVm,sxu32 nIdx,int iOp,SyBlob *pRaw);` |
|     - |   83 | `/*` |
|     - |   84 | ` * Perform one output-buffer operation on the buffer at index nIdx.` |
|     - |   85 | ` *` |
|     - |   86 | ` *   iOp   one of PH7_OB_WRITE / PH7_OB_FLUSH / PH7_OB_CLEAN, optionally with` |
|     - |   87 | ` *         PH7_OB_FINAL (the buffer is going away). PH7_OB_START is added here` |
|     - |   88 | ` *         while the handler has not run yet, exactly as php does.` |
|     - |   89 | ` *   pRaw  when non-NULL, receives a copy of the buffer as it was BEFORE the` |
|     - |   90 | ` *         handler ran — ob_get_clean()/ob_get_flush() answer that, not the` |
|     - |   91 | ` *         handler's output.` |
|     - |   92 | ` *` |
|     - |   93 | ` * The buffer is left empty; removing it is the caller's job. A CLEAN still runs` |
|     - |   94 | ` * the handler (php gives it the chance to reset its own state) and then throws` |
|     - |   95 | ` * the answer away.` |
|     - |   96 | ` */` |
| 10574 |   97 | `static sxi32 VmObPerform(ph7_vm *pVm,sxu32 nIdx,int iOp,SyBlob *pRaw)` |
|     5 |   98 | `{` |
| 10579 |   99 | `	VmObEntry *pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nIdx);` |
|     - |  100 | `	SyBlob sData;` |
| 10579 |  101 | `	sxi32 rc = PH7_OK;` |
|     - |  102 | `	sxu32 nRawLen;` |
| 10579 |  103 | `	int bDrop = 0;` |
| 10579 |  104 | `	if( pEntry == 0 ){` |
|   ! 0 |  105 | `		return PH7_OK;` |
|     - |  106 | `	}` |
|     - |  107 | `	/* Take the bytes OUT of the entry before anything else runs: the handler is` |
|     - |  108 | `	 * php code, and php code reaching back into the buffer stack reallocates it —` |
|     - |  109 | `	 * every pointer into the set, this entry's own blob included, dies with it. */` |
| 10579 |  110 | `	SyBlobInit(&sData,&pVm->sAllocator);` |
| 10579 |  111 | `	if( SyBlobLength(&pEntry->sOB) > 0 ){` |
|  5761 |  112 | `		SyBlobDup(&pEntry->sOB,&sData);` |
|  5761 |  113 | `		if( pRaw ){` |
|  5578 |  114 | `			SyBlobDup(&pEntry->sOB,pRaw);` |
|  2787 |  115 | `		}` |
|  2878 |  116 | `	}` |
| 10579 |  117 | `	nRawLen = SyBlobLength(&sData);` |
| 10579 |  118 | `	if( !ph7_value_is_callable(&pEntry->sCallback) ){` |
|     - |  119 | `		/* No handler: php's internal one, which "runs" for every operation and is` |
|     - |  120 | `		 * always taken to have produced its output. ob_get_status() reports both` |
|     - |  121 | `		 * bits from the first operation on, empty buffer included. */` |
| 10371 |  122 | `		pEntry->iFlags \|= PH7_OB_STARTED \| PH7_OB_PROCESSED;` |
|  5183 |  123 | `	}` |
| 10574 |  124 | `	if( ph7_value_is_callable(&pEntry->sCallback)` |
|  5396 |  125 | `		&& (pEntry->iFlags & PH7_OB_DISABLED) == 0 ){` |
|     - |  126 | `		ph7_value sArg,sPhase,sResult,*apArg[2];` |
|   206 |  127 | `		int iPhase = iOp \| ((pEntry->iFlags & PH7_OB_STARTED) ? 0 : PH7_OB_START);` |
|   206 |  128 | `		int bRefused = 0;` |
|     - |  129 | `		ph7_value sCallback;` |
|     - |  130 | `		sxi32 rcCall;` |
|     - |  131 | `		/* The callback is copied out for the same reason the bytes are. */` |
|   206 |  132 | `		PH7_MemObjInit(pVm,&sCallback);` |
|   206 |  133 | `		PH7_MemObjStore(&pEntry->sCallback,&sCallback);` |
|     - |  134 | `		/* Marked failed for the DURATION of the call, and cleared again when it comes` |
|     - |  135 | `		 * back with an answer. A disabled buffer is transparent, and that is exactly` |
|     - |  136 | `		 * what this buffer is while its handler runs: whatever the in-place catch for` |
|     - |  137 | `		 * a throwing handler prints belongs to the level BELOW, which is where php —` |
|     - |  138 | `		 * whose catch runs after the operation finished — puts it too. */` |
|   206 |  139 | `		pEntry->iFlags \|= PH7_OB_DISABLED;` |
|   206 |  140 | `		PH7_MemObjInitFromString(pVm,&sArg,0);` |
|   206 |  141 | `		PH7_MemObjStringAppend(&sArg,(const char *)SyBlobData(&sData),SyBlobLength(&sData));` |
|     - |  142 | `		/* php calls the handler as ($buffer, int $phase) — a handler declaring` |
|     - |  143 | `		 * both as required must not trip the arity check. */` |
|   206 |  144 | `		PH7_MemObjInitFromInt(pVm,&sPhase,iPhase);` |
|   206 |  145 | `		apArg[0] = &sArg;` |
|   206 |  146 | `		apArg[1] = &sPhase;` |
|   206 |  147 | `		PH7_MemObjInit(pVm,&sResult);` |
|     - |  148 | `		/* Everything the handler prints is DISCARDED (php has no buffer to put it` |
|     - |  149 | `		 * in — this one is mid-operation), and it may not open one either.` |
|     - |  150 | `		 * Through the callback dispatcher, not PH7_VmCallUserFunction: php builds` |
|     - |  151 | `		 * both arguments itself and passes them BY VALUE, so a handler declaring` |
|     - |  152 | ``		 * `&$buffer` gets php's warning and a copy rather than the fatal a direct`` |
|     - |  153 | `		 * call raises — and a throw from inside reaches the enclosing catch. */` |
|     - |  154 | `		{` |
|   206 |  155 | `			VmFrame *pSaveFrame = pVm->pObFrame;` |
|   206 |  156 | `			sxu32 nSaveActive = pVm->nObActive;` |
|   206 |  157 | `			int bSaveRefused = pVm->bObRefused;` |
|   206 |  158 | `			pVm->pObFrame = pVm->pFrame;` |
|   206 |  159 | `			pVm->nObActive = nIdx + 1;` |
|   206 |  160 | `			pVm->bObRefused = 0;` |
|   206 |  161 | `			pVm->nObDepth++;` |
|   206 |  162 | `			rcCall = PH7_VmCallCallbackByValue(pVm,&sCallback,2,apArg,&sResult,0);` |
|   206 |  163 | `			pVm->nObDepth--;` |
|   206 |  164 | `			bRefused = pVm->bObRefused;` |
|   206 |  165 | `			pVm->bObRefused = bSaveRefused;` |
|   206 |  166 | `			pVm->nObActive = nSaveActive;` |
|   206 |  167 | `			pVm->pObFrame = pSaveFrame;` |
|     - |  168 | `		}` |
|     - |  169 | `		/* php code ran: the slot may have moved, or gone. */` |
|   206 |  170 | `		pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nIdx);` |
|   206 |  171 | `		if( pEntry ){` |
|     - |  172 | `			/* Set AFTER the call: php's own STARTED is not visible to the first` |
|     - |  173 | `			 * invocation, only to the ones that follow it. */` |
|   206 |  174 | `			pEntry->iFlags \|= PH7_OB_STARTED;` |
|   101 |  175 | `		}` |
|   202 |  176 | `		if( PH7_CALLBACK_UNWOUND(rcCall)` |
|   202 |  177 | `			\|\| (ph7_value_is_bool(&sResult) && !ph7_value_to_bool(&sResult)) ){` |
|     - |  178 | `			/* php's two FAILURE shapes — the handler answered FALSE, or it threw` |
|     - |  179 | `			 * (or exited) and never answered at all. Both send the ORIGINAL bytes,` |
|     - |  180 | ``			 * so `sData` is left exactly as it was, and both leave the handler`` |
|     - |  181 | `			 * DISABLED: it is not called again (which is what keeps a throwing` |
|     - |  182 | `			 * handler from throwing a second time out of the shutdown flush) and the` |
|     - |  183 | `			 * buffer stops buffering. */` |
|   200 |  184 | `		}else if( ph7_value_is_bool(&sResult) ){` |
|     - |  185 | `			/* TRUE: "no data" — the operation produces nothing at all, and php` |
|     - |  186 | `			 * counts that as the handler having processed the buffer. */` |
|    10 |  187 | `			if( pEntry ){` |
|    10 |  188 | `				pEntry->iFlags &= ~PH7_OB_DISABLED;` |
|    10 |  189 | `				pEntry->iFlags \|= PH7_OB_PROCESSED;` |
|     4 |  190 | `			}` |
|    10 |  191 | `			SyBlobReset(&sData);` |
|     6 |  192 | `		}else{` |
|     - |  193 | `			/* Anything else is cast to a string, NULL included — php's own` |
|     - |  194 | `			 * user-visible conversion, so an ARRAY comes out as "Array" WITH the` |
|     - |  195 | `			 * warning and an object with no __toString() throws. */` |
|     - |  196 | `			const char *zOut;` |
|     - |  197 | `			int nOut;` |
|   188 |  198 | `			PH7_MemObjToStringUV(&sResult);` |
|   188 |  199 | `			zOut = ph7_value_to_string(&sResult,&nOut);` |
|   188 |  200 | `			SyBlobReset(&sData);` |
|   188 |  201 | `			if( nOut > 0 ){` |
|   142 |  202 | `				SyBlobAppend(&sData,zOut,(sxu32)nOut);` |
|    69 |  203 | `			}` |
|   188 |  204 | `			if( pEntry ){` |
|   188 |  205 | `				pEntry->iFlags &= ~PH7_OB_DISABLED;` |
|   188 |  206 | `				pEntry->iFlags \|= PH7_OB_PROCESSED;` |
|    92 |  207 | `			}` |
|     - |  208 | `		}` |
|   206 |  209 | `		PH7_MemObjRelease(&sArg);` |
|   206 |  210 | `		PH7_MemObjRelease(&sPhase);` |
|   206 |  211 | `		PH7_MemObjRelease(&sResult);` |
|   206 |  212 | `		PH7_MemObjRelease(&sCallback);` |
|     - |  213 | `		/* A throw or an exit() inside the handler is the CALLER's to act on: the` |
|     - |  214 | `		 * enclosing catch runs, or the program ends. */` |
|   206 |  215 | `		if( PH7_CALLBACK_UNWOUND(rcCall) ){` |
|     8 |  216 | `			rc = rcCall;` |
|     3 |  217 | `		}` |
|     - |  218 | `		/* An ob call the handler was not allowed to make ends the request, and php` |
|     - |  219 | `		 * delivers nothing more. An ordinary exit() from inside one is NOT that:` |
|     - |  220 | `		 * php still sends what the buffer held. */` |
|   206 |  221 | `		if( bRefused ){` |
|     6 |  222 | `			bDrop = 1;` |
|     2 |  223 | `		}` |
|   101 |  224 | `	}` |
|     - |  225 | `	/* The bytes this operation took are gone from the buffer now — but only those:` |
|     - |  226 | `	 * an in-place catch for a throw inside the handler runs before the dispatch` |
|     - |  227 | `	 * returns and may have written MORE into this still-open buffer, and that tail` |
|     - |  228 | `	 * is the caller's output, not this operation's. Re-resolved because php code` |
|     - |  229 | `	 * may have moved the set. */` |
| 10579 |  230 | `	pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nIdx);` |
| 10579 |  231 | `	if( pEntry ){` |
| 10579 |  232 | `		sxu32 nHave = SyBlobLength(&pEntry->sOB);` |
| 10579 |  233 | `		if( nHave > nRawLen ){` |
|     - |  234 | `			SyBlob sTail;` |
|   ! 0 |  235 | `			SyBlobInit(&sTail,&pVm->sAllocator);` |
|   ! 0 |  236 | `			SyBlobAppend(&sTail,(const char *)SyBlobData(&pEntry->sOB) + nRawLen,nHave - nRawLen);` |
|   ! 0 |  237 | `			SyBlobReset(&pEntry->sOB);` |
|   ! 0 |  238 | `			SyBlobAppend(&pEntry->sOB,SyBlobData(&sTail),SyBlobLength(&sTail));` |
|   ! 0 |  239 | `			SyBlobRelease(&sTail);` |
|   ! 0 |  240 | `		}else{` |
| 10579 |  241 | `			SyBlobReset(&pEntry->sOB);` |
|     - |  242 | `		}` |
|  5287 |  243 | `	}` |
| 10579 |  244 | `	if( (iOp & PH7_OB_CLEAN) == 0 && SyBlobLength(&sData) > 0 && !bDrop ){` |
|   154 |  245 | `		sxi32 rcOut = VmObDeliver(pVm,nIdx,SyBlobData(&sData),SyBlobLength(&sData));` |
|   154 |  246 | `		if( rc == PH7_OK ){` |
|   152 |  247 | `			rc = rcOut;` |
|    74 |  248 | `		}` |
|    75 |  249 | `	}` |
| 10579 |  250 | `	SyBlobRelease(&sData);` |
| 10579 |  251 | `	return rc;` |
|  5292 |  252 | `}` |
|     - |  253 | `/*` |
|     - |  254 | ` * Hand nLen bytes to the buffer at index iIdx, or to whatever is under it.` |
|     - |  255 | ` *` |
|     - |  256 | ` * php's stack is a stack — a nested flush lands in the enclosing buffer, not on` |
|     - |  257 | ` * stdout — and a DISABLED buffer is transparent: once a handler has failed php` |
|     - |  258 | ` * stops buffering through it (the level is still there and still counts, but` |
|     - |  259 | ` * everything written to it passes straight down), which is why the catch that` |
|     - |  260 | ` * follows a throwing handler prints immediately and ob_get_contents() answers "".` |
|     - |  261 | ` */` |
| 77372 |  262 | `static sxi32 VmObSink(ph7_vm *pVm,sxi32 iIdx,const void *pData,sxu32 nLen)` |
|     5 |  263 | `{` |
|     - |  264 | `	sxi32 rc;` |
| 77381 |  265 | `	while( iIdx >= 0 ){` |
| 77365 |  266 | `		VmObEntry *pEntry = (VmObEntry *)SySetAt(&pVm->aOB,(sxu32)iIdx);` |
| 77365 |  267 | `		if( pEntry == 0 ){` |
|   ! 0 |  268 | `			break; /* the buffer went away underneath: fall through to the output */` |
|     - |  269 | `		}` |
| 77365 |  270 | `		if( (pEntry->iFlags & PH7_OB_DISABLED) == 0 ){` |
| 77361 |  271 | `			VmObGrow(pEntry,nLen);` |
| 77361 |  272 | `			SyBlobAppend(&pEntry->sOB,pData,nLen);` |
|     - |  273 | `			/* A buffer with a chunk size writes out as soon as it holds one. */` |
| 77361 |  274 | `			if( pEntry->nChunk > 0 && SyBlobLength(&pEntry->sOB) >= pEntry->nChunk ){` |
|    10 |  275 | `				return VmObPerform(pVm,(sxu32)iIdx,PH7_OB_WRITE,0);` |
|     - |  276 | `			}` |
| 77353 |  277 | `			return PH7_OK;` |
|     - |  278 | `		}` |
|     5 |  279 | `		iIdx--;` |
|     1 |  280 | `	}` |
|     - |  281 | `	/* Call the VM output consumer */` |
|    19 |  282 | `	rc = pVm->sVmConsumer.xDef(pData,(unsigned int)nLen,pVm->sVmConsumer.pDefData);` |
|     - |  283 | `	/* Increment VM output counter */` |
|    19 |  284 | `	pVm->nOutputLen += nLen;` |
|    19 |  285 | `	if( rc != PH7_ABORT ){` |
|    19 |  286 | `		rc = PH7_OK;` |
|     8 |  287 | `	}` |
|    19 |  288 | `	return rc;` |
| 38691 |  289 | `}` |
|     - |  290 | `/*` |
|     - |  291 | ` * Deliver what is leaving the buffer at nIdx to whatever is under it.` |
|     - |  292 | ` */` |
|   150 |  293 | `static sxi32 VmObDeliver(ph7_vm *pVm,sxu32 nIdx,const void *pData,sxu32 nLen)` |
|     4 |  294 | `{` |
|   154 |  295 | `	return VmObSink(pVm,(sxi32)nIdx - 1,pData,nLen);` |
|     4 |  296 | `}` |
|     - |  297 | `/*` |
|     - |  298 | ` * Output Buffer(OB) default VM consumer routine.All VM output is now redirected` |
|     - |  299 | ` * to a stackable internal buffer,until the user call [ob_get_clean(),ob_end_clean(),...].` |
|     - |  300 | ` * Refer to the implementation of [ob_start()] for more information.` |
|     - |  301 | ` */` |
| 77224 |  302 | `PH7_PRIVATE int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData)` |
|     5 |  303 | `{` |
| 77229 |  304 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
| 77229 |  305 | `	sxu32 nUsed = SySetUsed(&pVm->aOB);` |
| 77229 |  306 | `	if( nUsed < 1 ){` |
|     - |  307 | `		/* CAN'T HAPPEN */` |
|   ! 0 |  308 | `		return PH7_OK;` |
|     - |  309 | `	}` |
| 77229 |  310 | `	if( VmObInHandler(pVm) ){` |
|     - |  311 | `		/* Inside a handler: php has nowhere to put this and drops it. */` |
|     3 |  312 | `		return PH7_OK;` |
|     - |  313 | `	}` |
| 77227 |  314 | `	return VmObSink(pVm,(sxi32)nUsed - 1,pData,nDataLen);` |
| 38617 |  315 | `}` |
|     - |  316 | `/*` |
|     - |  317 | ` * Pop the topmost buffer and release it, restoring the default consumer when the` |
|     - |  318 | ` * stack empties out.` |
|     - |  319 | ` */` |
| 10520 |  320 | `static void VmObPop(ph7_vm *pVm)` |
|     5 |  321 | `{` |
| 10525 |  322 | `	VmObEntry *pEntry = (VmObEntry *)SySetPop(&pVm->aOB);` |
| 10525 |  323 | `	if( pEntry ){` |
| 10525 |  324 | `		VmObRestore(pVm,pEntry);` |
|  5260 |  325 | `	}` |
| 10525 |  326 | `}` |
|     - |  327 | `/*` |
|     - |  328 | ` * Restore the default consumer.` |
|     - |  329 | ` * Refer to the implementation of [ob_end_clean()] for more` |
|     - |  330 | ` * information.` |
|     - |  331 | ` */` |
| 10520 |  332 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry)` |
|     5 |  333 | `{` |
| 10525 |  334 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
| 10525 |  335 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|     - |  336 | `		/* No more stackable OB */` |
| 10181 |  337 | `		pCons->xConsumer = pCons->xDef;` |
| 10181 |  338 | `		pCons->pUserData = pCons->pDefData;` |
|  5088 |  339 | `	}` |
|     - |  340 | `	/* Release OB data */` |
| 10525 |  341 | `	PH7_MemObjRelease(&pEntry->sCallback);` |
| 10525 |  342 | `	SyBlobRelease(&pEntry->sOB);` |
| 10525 |  343 | `}` |
|     - |  344 | `/*` |
|     - |  345 | ` * php ends and FLUSHES every still-open buffer at shutdown — innermost first, so` |
|     - |  346 | ` * an inner handler's answer is what the outer one is handed. A script that never` |
|     - |  347 | ` * called ob_end_flush() (PHPUnit, which buffers its summary and then exit()s with` |
|     - |  348 | ` * a non-zero status) would otherwise lose that output entirely.` |
|     - |  349 | ` */` |
|  4562 |  350 | `PH7_PRIVATE void PH7_VmObFlushAll(ph7_vm *pVm)` |
|     5 |  351 | `{` |
|  4643 |  352 | `	while( SySetUsed(&pVm->aOB) > 0 ){` |
|    78 |  353 | `		VmObPerform(pVm,SySetUsed(&pVm->aOB) - 1,PH7_OB_FINAL,0);` |
|    78 |  354 | `		VmObPop(pVm);` |
|     2 |  355 | `	}` |
|  4567 |  356 | `	pVm->nObDepth = 0;` |
|  4567 |  357 | `}` |
|     - |  358 | `/*` |
|     - |  359 | ` * php refuses every ob call that MUTATES the stack while a handler is running —` |
|     - |  360 | ` * the handler IS an operation on that stack, so cleaning, flushing, removing or` |
|     - |  361 | ` * pushing under it has nowhere sane to land. The read-only members` |
|     - |  362 | ` * (ob_get_contents/ob_get_length/ob_get_level/ob_list_handlers) are allowed and` |
|     - |  363 | ` * still answer for the buffer being processed.` |
|     - |  364 | ` *` |
|     - |  365 | ` * Returns TRUE when the call was refused; the caller returns PH7_ABORT.` |
|     - |  366 | ` */` |
| 21128 |  367 | `static int VmObRefuseInHandler(ph7_context *pCtx)` |
|     5 |  368 | `{` |
| 21133 |  369 | `	ph7_vm *pVm = pCtx->pVm;` |
| 21133 |  370 | `	if( !VmObInHandler(pVm) ){` |
| 21129 |  371 | `		return 0;` |
|     - |  372 | `	}` |
|     - |  373 | `	/* The context prefixes "name(): " itself, so each member names itself. */` |
|     6 |  374 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|     - |  375 | `		"Cannot use output buffering in output buffering display handlers");` |
|     6 |  376 | `	ph7_result_bool(pCtx,0);` |
|     6 |  377 | `	pVm->iExitStatus = 255;` |
|     6 |  378 | `	pVm->bHaltRequested = 1;` |
|     6 |  379 | `	pVm->bObRefused = 1;` |
|     6 |  380 | `	return 1;` |
| 10569 |  381 | `}` |
|     - |  382 | `/*` |
|     - |  383 | ` * php's name for one buffer's handler: the callable's own display name (a plain` |
|     - |  384 | `` * function name, `Class::method`, `{closure:file:line}`) or, with no handler at`` |
|     - |  385 | ` * all, the literal "default output handler". It reaches ob_list_handlers(),` |
|     - |  386 | ` * ob_get_status() and the refusal notices below — where PHL used to answer` |
|     - |  387 | ` * "Class Method" for every array callback and "default output handler" for every` |
|     - |  388 | ` * CLOSURE, so a closure handler was indistinguishable from none.` |
|     - |  389 | ` */` |
|   226 |  390 | `static void VmObHandlerName(ph7_vm *pVm,VmObEntry *pEntry,SyBlob *pOut)` |
|     4 |  391 | `{` |
|   230 |  392 | `	if( ph7_value_is_callable(&pEntry->sCallback) ){` |
|    99 |  393 | `		PH7_VmCallableName(pVm,&pEntry->sCallback,pOut);` |
|    99 |  394 | `		if( SyBlobLength(pOut) > 0 ){` |
|    99 |  395 | `			return;` |
|     - |  396 | `		}` |
|   ! 0 |  397 | `	}` |
|   133 |  398 | `	SyBlobAppend(pOut,"default output handler",sizeof("default output handler")-1);` |
|   117 |  399 | `}` |
|     - |  400 | `/*` |
|     - |  401 | ` * Copy one buffer's bytes out. Anything that can run php code — a notice reaching` |
|     - |  402 | ` * a user error handler included — may realloc the buffer stack, so nothing holds` |
|     - |  403 | ` * a VmObEntry pointer across it.` |
|     - |  404 | ` */` |
|  5768 |  405 | `static void VmObSnapshot(ph7_vm *pVm,sxu32 nIdx,SyBlob *pOut)` |
|     5 |  406 | `{` |
|  5773 |  407 | `	VmObEntry *pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nIdx);` |
|  5773 |  408 | `	if( pEntry && SyBlobLength(&pEntry->sOB) > 0 ){` |
|  5583 |  409 | `		SyBlobAppend(pOut,SyBlobData(&pEntry->sOB),SyBlobLength(&pEntry->sOB));` |
|  2789 |  410 | `	}` |
|  5773 |  411 | `}` |
|     - |  412 | `/*` |
|     - |  413 | ` * ob_start()'s $flags decide what may be done to the buffer afterwards, and every` |
|     - |  414 | ` * member tests its own bit before it touches anything: CLEANABLE for ob_clean(),` |
|     - |  415 | ` * FLUSHABLE for ob_flush(), REMOVABLE for the four that take the buffer away. A` |
|     - |  416 | ` * refused operation does NOT run the handler and leaves the buffer exactly as it` |
|     - |  417 | `` * was. The argument was declared in `aBuiltinSig[]` and read by nothing, so a`` |
|     - |  418 | ` * buffer opened as un-removable — the standard way a framework pins its own` |
|     - |  419 | ` * output layer in place — could be torn out by any library that called` |
|     - |  420 | ` * ob_end_clean().` |
|     - |  421 | ` *` |
|     - |  422 | `` * `zWhat` is php's verb for this member ("delete"/"flush"/"discard"/"send").`` |
|     - |  423 | ` * Returns TRUE when the operation may proceed.` |
|     - |  424 | ` */` |
| 10598 |  425 | `static int VmObAllows(ph7_context *pCtx,sxu32 nIdx,int iNeed,const char *zWhat)` |
|     5 |  426 | `{` |
| 10603 |  427 | `	ph7_vm *pVm = pCtx->pVm;` |
| 10603 |  428 | `	VmObEntry *pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nIdx);` |
|     - |  429 | `	SyBlob sName;` |
| 10603 |  430 | `	if( pEntry == 0 \|\| (pEntry->iFlags & iNeed) != 0 ){` |
| 10495 |  431 | `		return 1;` |
|     - |  432 | `	}` |
|   110 |  433 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|   110 |  434 | `	VmObHandlerName(pVm,pEntry,&sName);` |
|   164 |  435 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|    54 |  436 | `		"Failed to %s buffer of %.*s (%u)",zWhat,` |
|   108 |  437 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),nIdx);` |
|   110 |  438 | `	SyBlobRelease(&sName);` |
|   110 |  439 | `	return 0;` |
|  5304 |  440 | `}` |
|     - |  441 | `/*` |
|     - |  442 | ` * bool ob_clean(void)` |
|     - |  443 | ` *  This function discards the contents of the output buffer.` |
|     - |  444 | ` *  This function does not destroy the output buffer like ob_end_clean() does.` |
|     - |  445 | ` * Parameter` |
|     - |  446 | ` *  None` |
|     - |  447 | ` * Return` |
|     - |  448 | ` *  TRUE on success, FALSE (with a notice) when no buffer is active. This used to` |
|     - |  449 | `` *  return NOTHING, so `if (!ob_clean())` fired on the successful call.`` |
|     - |  450 | ` */` |
|    24 |  451 | `PH7_PRIVATE int vm_builtin_ob_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  452 | `{` |
|    27 |  453 | `	ph7_vm *pVm = pCtx->pVm;` |
|    27 |  454 | `	sxu32 nUsed = SySetUsed(&pVm->aOB);` |
|     - |  455 | `	sxi32 rc;` |
|    12 |  456 | `	SXUNUSED(nArg); /* cc warning */` |
|    12 |  457 | `	SXUNUSED(apArg);` |
|    27 |  458 | `	if( VmObRefuseInHandler(pCtx) ){` |
|   ! 0 |  459 | `		return PH7_ABORT;` |
|     - |  460 | `	}` |
|    27 |  461 | `	if( nUsed < 1 ){` |
|     3 |  462 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|     - |  463 | `			"Failed to delete buffer. No buffer to delete");` |
|     3 |  464 | `		ph7_result_bool(pCtx,0);` |
|     3 |  465 | `		return PH7_OK;` |
|     - |  466 | `	}` |
|    25 |  467 | `	if( !VmObAllows(pCtx,nUsed - 1,PH7_OB_CLEANABLE,"delete") ){` |
|     9 |  468 | `		ph7_result_bool(pCtx,0);` |
|     9 |  469 | `		return PH7_OK;` |
|     - |  470 | `	}` |
|    17 |  471 | `	rc = VmObPerform(pVm,nUsed - 1,PH7_OB_CLEAN,0);` |
|    17 |  472 | `	ph7_result_bool(pCtx,1);` |
|    17 |  473 | `	return rc;` |
|    15 |  474 | `}` |
|     - |  475 | `/*` |
|     - |  476 | ` * bool ob_end_clean(void)` |
|     - |  477 | ` *  Clean (erase) the output buffer and turn off output buffering` |
|     - |  478 | ` *  This function discards the contents of the topmost output buffer and turns` |
|     - |  479 | ` *  off this output buffering. If you want to further process the buffer's contents` |
|     - |  480 | ` *  you have to call ob_get_contents() before ob_end_clean() as the buffer contents` |
|     - |  481 | ` *  are discarded when ob_end_clean() is called.` |
|     - |  482 | ` * Parameter` |
|     - |  483 | ` *  None` |
|     - |  484 | ` * Return` |
|     - |  485 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first that you called` |
|     - |  486 | ` *  the function without an active buffer or that for some reason a buffer could not be deleted` |
|     - |  487 | ` * (possible for special buffer)` |
|     - |  488 | ` */` |
|  4678 |  489 | `PH7_PRIVATE int vm_builtin_ob_end_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  490 | `{` |
|  4683 |  491 | `	ph7_vm *pVm = pCtx->pVm;` |
|  4683 |  492 | `	sxu32 nUsed = SySetUsed(&pVm->aOB);` |
|     - |  493 | `	sxi32 rc;` |
|  2339 |  494 | `	SXUNUSED(nArg); /* cc warning */` |
|  2339 |  495 | `	SXUNUSED(apArg);` |
|  4683 |  496 | `	if( VmObRefuseInHandler(pCtx) ){` |
|   ! 0 |  497 | `		return PH7_ABORT;` |
|     - |  498 | `	}` |
|  4683 |  499 | `	if( nUsed < 1 ){` |
|     - |  500 | `		/* No such OB,return FALSE */` |
|     6 |  501 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|     - |  502 | `			"Failed to delete buffer. No buffer to delete");` |
|     6 |  503 | `		ph7_result_bool(pCtx,0);` |
|     6 |  504 | `		return PH7_OK;` |
|     - |  505 | `	}` |
|  4679 |  506 | `	if( !VmObAllows(pCtx,nUsed - 1,PH7_OB_REMOVABLE,"discard") ){` |
|    24 |  507 | `		ph7_result_bool(pCtx,0);` |
|    24 |  508 | `		return PH7_OK;` |
|     - |  509 | `	}` |
|  4657 |  510 | `	rc = VmObPerform(pVm,nUsed - 1,PH7_OB_CLEAN\|PH7_OB_FINAL,0);` |
|  4657 |  511 | `	VmObPop(pVm);` |
|  4657 |  512 | `	ph7_result_bool(pCtx,1);` |
|  4657 |  513 | `	return rc;` |
|  2344 |  514 | `}` |
|     - |  515 | `/*` |
|     - |  516 | ` * string ob_get_contents(void)` |
|     - |  517 | ` *  Gets the contents of the output buffer without clearing it.` |
|     - |  518 | ` * Parameter` |
|     - |  519 | ` *  None` |
|     - |  520 | ` * Return` |
|     - |  521 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|     - |  522 | ` *  The bytes are the RAW ones: php runs the handler on the way out, so what a` |
|     - |  523 | ` *  handler will make of them has not happened yet.` |
|     - |  524 | ` */` |
|    24 |  525 | `PH7_PRIVATE int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  526 | `{` |
|    28 |  527 | `	ph7_vm *pVm = pCtx->pVm;` |
|    28 |  528 | `	sxu32 nSeen = VmObVisible(pVm);` |
|    28 |  529 | `	VmObEntry *pOb = nSeen > 0 ? (VmObEntry *)SySetAt(&pVm->aOB,nSeen - 1) : 0;` |
|    28 |  530 | `	if( pOb == 0 ){` |
|     - |  531 | `		/* No active OB,return FALSE */` |
|     3 |  532 | `		ph7_result_bool(pCtx,0);` |
|     1 |  533 | `		SXUNUSED(nArg); /* cc warning */` |
|     1 |  534 | `		SXUNUSED(apArg);` |
|     2 |  535 | `	}else{` |
|     - |  536 | `		/* Return contents */` |
|    26 |  537 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB));` |
|     - |  538 | `	}` |
|    28 |  539 | `	return PH7_OK;` |
|     4 |  540 | `}` |
|     - |  541 | `/*` |
|     - |  542 | ` * string ob_get_clean(void)` |
|     - |  543 | ` *  Get current buffer contents and delete current output buffer.` |
|     - |  544 | ` * Parameter` |
|     - |  545 | ` *  None` |
|     - |  546 | ` * Return` |
|     - |  547 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|     - |  548 | ` */` |
|  5750 |  549 | `PH7_PRIVATE int vm_builtin_ob_get_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  550 | `{` |
|  5755 |  551 | `	ph7_vm *pVm = pCtx->pVm;` |
|  5755 |  552 | `	sxu32 nUsed = SySetUsed(&pVm->aOB);` |
|     - |  553 | `	SyBlob sRaw;` |
|     - |  554 | `	sxi32 rc;` |
|  5755 |  555 | `	if( VmObRefuseInHandler(pCtx) ){` |
|   ! 0 |  556 | `		return PH7_ABORT;` |
|     - |  557 | `	}` |
|  5755 |  558 | `	if( nUsed < 1 ){` |
|     - |  559 | `		/* No active OB,return FALSE. php reports every other empty-stack call and` |
|     - |  560 | `		 * stays silent for this one. */` |
|     3 |  561 | `		ph7_result_bool(pCtx,0);` |
|     1 |  562 | `		SXUNUSED(nArg); /* cc warning */` |
|     1 |  563 | `		SXUNUSED(apArg);` |
|     3 |  564 | `		return PH7_OK;` |
|     - |  565 | `	}` |
|  5753 |  566 | `	SyBlobInit(&sRaw,&pVm->sAllocator);` |
|     - |  567 | `	/* Snapshot BEFORE the refusal is even tested: the notices below reach a user` |
|     - |  568 | `	 * error handler, which is php code that may print into this very buffer, and` |
|     - |  569 | `	 * php answers the contents as they were when the call was made. */` |
|  5753 |  570 | `	VmObSnapshot(pVm,nUsed - 1,&sRaw);` |
|  5753 |  571 | `	if( !VmObAllows(pCtx,nUsed - 1,PH7_OB_REMOVABLE,"discard") ){` |
|     - |  572 | `		/* php reports the CLEAN and the REMOVAL separately, and still answers the` |
|     - |  573 | `		 * contents it could not take away — as they were BEFORE those reports,` |
|     - |  574 | `		 * which may run a user error handler that writes into this very buffer. */` |
|    26 |  575 | `		VmObAllows(pCtx,nUsed - 1,0,"delete");` |
|    26 |  576 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sRaw),(int)SyBlobLength(&sRaw));` |
|    26 |  577 | `		SyBlobRelease(&sRaw);` |
|    26 |  578 | `		return PH7_OK;` |
|     - |  579 | `	}` |
|  5729 |  580 | `	SyBlobReset(&sRaw);` |
|  5729 |  581 | `	rc = VmObPerform(pVm,nUsed - 1,PH7_OB_CLEAN\|PH7_OB_FINAL,&sRaw);` |
|  5729 |  582 | `	VmObPop(pVm);` |
|  5729 |  583 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sRaw),(int)SyBlobLength(&sRaw)); /* Will make it's own copy */` |
|  5729 |  584 | `	SyBlobRelease(&sRaw);` |
|  5729 |  585 | `	return rc;` |
|  2880 |  586 | `}` |
|     - |  587 | `/*` |
|     - |  588 | ` * string ob_get_flush(void)` |
|     - |  589 | ` *  Flush the output buffer, return it as a string and turn off output buffering.` |
|     - |  590 | ` * Parameter` |
|     - |  591 | ` *  None` |
|     - |  592 | ` * Return` |
|     - |  593 | ` *  The contents of the output buffer, or FALSE if output buffering isn't active.` |
|     - |  594 | ` * Note` |
|     - |  595 | ` *  This used to be registered as ob_get_clean(), which DISCARDS the buffer: the` |
|     - |  596 | ` *  string came back correctly and the output it names never reached the terminal.` |
|     - |  597 | ` */` |
|    22 |  598 | `PH7_PRIVATE int vm_builtin_ob_get_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  599 | `{` |
|    25 |  600 | `	ph7_vm *pVm = pCtx->pVm;` |
|    25 |  601 | `	sxu32 nUsed = SySetUsed(&pVm->aOB);` |
|     - |  602 | `	SyBlob sRaw;` |
|     - |  603 | `	sxi32 rc;` |
|    25 |  604 | `	if( VmObRefuseInHandler(pCtx) ){` |
|   ! 0 |  605 | `		return PH7_ABORT;` |
|     - |  606 | `	}` |
|    25 |  607 | `	if( nUsed < 1 ){` |
|     - |  608 | `		/* No active OB,return FALSE. php's silent member here is ob_get_CLEAN;` |
|     - |  609 | `		 * this one reports, and the two wordings are php's own. */` |
|     3 |  610 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|     - |  611 | `			"Failed to delete and flush buffer. No buffer to delete or flush");` |
|     3 |  612 | `		ph7_result_bool(pCtx,0);` |
|     1 |  613 | `		SXUNUSED(nArg); /* cc warning */` |
|     1 |  614 | `		SXUNUSED(apArg);` |
|     3 |  615 | `		return PH7_OK;` |
|     - |  616 | `	}` |
|    23 |  617 | `	SyBlobInit(&sRaw,&pVm->sAllocator);` |
|     - |  618 | `	/* Snapshot BEFORE the refusal is even tested: the notices below reach a user` |
|     - |  619 | `	 * error handler, which is php code that may print into this very buffer, and` |
|     - |  620 | `	 * php answers the contents as they were when the call was made. */` |
|    23 |  621 | `	VmObSnapshot(pVm,nUsed - 1,&sRaw);` |
|    23 |  622 | `	if( !VmObAllows(pCtx,nUsed - 1,PH7_OB_REMOVABLE,"send") ){` |
|    10 |  623 | `		VmObAllows(pCtx,nUsed - 1,0,"delete");` |
|    10 |  624 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sRaw),(int)SyBlobLength(&sRaw));` |
|    10 |  625 | `		SyBlobRelease(&sRaw);` |
|    10 |  626 | `		return PH7_OK;` |
|     - |  627 | `	}` |
|    14 |  628 | `	SyBlobReset(&sRaw);` |
|    14 |  629 | `	rc = VmObPerform(pVm,nUsed - 1,PH7_OB_FINAL,&sRaw);` |
|    14 |  630 | `	VmObPop(pVm);` |
|     - |  631 | `	/* The answer is the RAW buffer, not what the handler made of it */` |
|    14 |  632 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sRaw),(int)SyBlobLength(&sRaw));` |
|    14 |  633 | `	SyBlobRelease(&sRaw);` |
|    14 |  634 | `	return rc;` |
|    14 |  635 | `}` |
|     - |  636 | `/*` |
|     - |  637 | ` * int ob_get_length(void)` |
|     - |  638 | ` *  Return the length of the output buffer.` |
|     - |  639 | ` * Parameter` |
|     - |  640 | ` *  None` |
|     - |  641 | ` * Return` |
|     - |  642 | ` *  Returns the length of the output buffer contents or FALSE if no buffering is active.` |
|     - |  643 | ` */` |
|    10 |  644 | `PH7_PRIVATE int vm_builtin_ob_get_length(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  645 | `{` |
|    12 |  646 | `	ph7_vm *pVm = pCtx->pVm;` |
|    12 |  647 | `	sxu32 nSeen = VmObVisible(pVm);` |
|    12 |  648 | `	VmObEntry *pOb = nSeen > 0 ? (VmObEntry *)SySetAt(&pVm->aOB,nSeen - 1) : 0;` |
|    12 |  649 | `	if( pOb == 0 ){` |
|     - |  650 | `		/* No active OB,return FALSE */` |
|     3 |  651 | `		ph7_result_bool(pCtx,0);` |
|     1 |  652 | `		SXUNUSED(nArg); /* cc warning */` |
|     1 |  653 | `		SXUNUSED(apArg);` |
|     2 |  654 | `	}else{` |
|     - |  655 | `		/* Return OB length */` |
|    10 |  656 | `		ph7_result_int64(pCtx,(ph7_int64)SyBlobLength(&pOb->sOB));` |
|     - |  657 | `	}` |
|    12 |  658 | `	return PH7_OK;` |
|     2 |  659 | `}` |
|     - |  660 | `/*` |
|     - |  661 | ` * int ob_get_level(void)` |
|     - |  662 | ` *  Returns the nesting level of the output buffering mechanism.` |
|     - |  663 | ` * Parameter` |
|     - |  664 | ` *  None` |
|     - |  665 | ` * Return` |
|     - |  666 | ` *  Returns the level of nested output buffering handlers or zero if output buffering is not active.` |
|     - |  667 | ` */` |
|   246 |  668 | `PH7_PRIVATE int vm_builtin_ob_get_level(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  669 | `{` |
|   250 |  670 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  671 | `	int iNest;` |
|   123 |  672 | `	SXUNUSED(nArg); /* cc warning */` |
|   123 |  673 | `	SXUNUSED(apArg);` |
|     - |  674 | `	/* Nesting level */` |
|   250 |  675 | `	iNest = (int)VmObVisible(pVm);` |
|     - |  676 | `	/* Return the nesting value */` |
|   250 |  677 | `	ph7_result_int(pCtx,iNest);` |
|   250 |  678 | `	return PH7_OK;` |
|     4 |  679 | `}` |
|     - |  680 | `/*` |
|     - |  681 | ` * bool ob_start([ callback $output_callback[, int $chunk_size = 0[, int $flags]]] )` |
|     - |  682 | ` * This function will turn output buffering on. While output buffering is active no output` |
|     - |  683 | ` *  is sent from the script (other than headers), instead the output is stored in an internal` |
|     - |  684 | ` *  buffer.` |
|     - |  685 | ` * Parameter` |
|     - |  686 | ` *  $output_callback` |
|     - |  687 | ` *   An optional output_callback function may be specified. This function takes a string` |
|     - |  688 | ` *   as a parameter and should return a string. The function is called when the buffer is` |
|     - |  689 | ` *   flushed, cleaned or removed, and its second argument says WHICH of those it is (the` |
|     - |  690 | ` *   PHP_OUTPUT_HANDLER_* phase bits). Returning FALSE sends the original bytes and` |
|     - |  691 | ` *   disables the handler for good; returning TRUE sends nothing.` |
|     - |  692 | ` *  $chunk_size` |
|     - |  693 | ` *   Write out as soon as the buffer holds this many bytes.` |
|     - |  694 | ` * Return` |
|     - |  695 | ` *   Returns TRUE on success or FALSE on failure.` |
|     - |  696 | ` */` |
| 10546 |  697 | `PH7_PRIVATE int vm_builtin_ob_start(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  698 | `{` |
| 10551 |  699 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  700 | `	VmObEntry sOb;` |
|     - |  701 | `	sxi32 rc;` |
|     - |  702 | `	/* php has nowhere to put a buffer opened from inside a handler — that handler` |
|     - |  703 | `	 * is mid-operation on the stack this would push onto — and refuses outright. */` |
| 10551 |  704 | `	if( VmObRefuseInHandler(pCtx) ){` |
|     3 |  705 | `		return PH7_ABORT;` |
|     - |  706 | `	}` |
|     - |  707 | `	/* php screens the handler BEFORE it opens the buffer, and a handler it cannot` |
|     - |  708 | `	 * call is a refusal rather than a silent downgrade to the default one: the` |
|     - |  709 | `	 * warning names why (zend's own callable reason) and the notice says no buffer` |
|     - |  710 | `	 * was created. This used to answer TRUE with a buffer whose handler never ran,` |
|     - |  711 | ``	 * so `if (!ob_start('my_filter'))` never fired on a misspelled name and the`` |
|     - |  712 | `	 * output came out unfiltered. */` |
| 10549 |  713 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|   194 |  714 | `		if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|     - |  715 | `			char zBuf[256];` |
|    25 |  716 | `			const char *zReason = PH7_VmCallableReason(pVm,apArg[0],zBuf,(int)sizeof(zBuf));` |
|    25 |  717 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zReason);` |
|    25 |  718 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"Failed to create buffer");` |
|    25 |  719 | `			ph7_result_bool(pCtx,0);` |
|    25 |  720 | `			return PH7_OK;` |
|     - |  721 | `		}` |
|    83 |  722 | `	}` |
|     - |  723 | `	/* Initialize the OB entry */` |
| 10525 |  724 | `	PH7_MemObjInit(pCtx->pVm,&sOb.sCallback);` |
| 10525 |  725 | `	SyBlobInit(&sOb.sOB,&pVm->sAllocator);` |
|     - |  726 | `	/* php keeps whatever it is given except the two nibbles it reserves for` |
|     - |  727 | `	 * itself — the phase bits and the STARTED/DISABLED/PROCESSED state — and` |
|     - |  728 | `	 * reports the rest back verbatim, sign included. */` |
| 10525 |  729 | `	sOb.iFlags = nArg > 2 ? (ph7_value_to_int64(apArg[2]) & PH7_OB_FLAGMASK) : PH7_OB_STDFLAGS;` |
| 10525 |  730 | `	sOb.nChunk = 0;` |
| 10525 |  731 | `	sOb.nSize = 0;` |
| 10525 |  732 | `	if( nArg > 0 && (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) ){` |
|     - |  733 | `		/* Save the callback name for later invocation (MEMOBJ_OBJ = a Closure callback). */` |
|   170 |  734 | `		PH7_MemObjStore(apArg[0],&sOb.sCallback);` |
|   170 |  735 | `		sOb.iFlags \|= PH7_OB_USER;` |
|    83 |  736 | `	}` |
| 10525 |  737 | `	if( nArg > 1 ){` |
|   144 |  738 | `		ph7_int64 nChunk = ph7_value_to_int64(apArg[1]);` |
|     - |  739 | `		/* A negative chunk size is no chunk size at all, which is what php` |
|     - |  740 | `		 * reports back for one. */` |
|   144 |  741 | `		if( nChunk > 0 ){` |
|    23 |  742 | `			sOb.nChunk = nChunk;` |
|    10 |  743 | `		}` |
|    70 |  744 | `	}` |
| 10525 |  745 | `	sOb.nSize = VmObInitSize(&sOb);` |
|     - |  746 | `	/* Push in the stack */` |
| 10525 |  747 | `	rc = SySetPut(&pVm->aOB,(const void *)&sOb);` |
| 10525 |  748 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  749 | `		PH7_MemObjRelease(&sOb.sCallback);` |
|   ! 0 |  750 | `	}else{` |
| 10525 |  751 | `		ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|     - |  752 | `		/* Substitute the default VM consumer */` |
| 10525 |  753 | `		if( pCons->xConsumer != VmObConsumer ){` |
| 10181 |  754 | `			pCons->xDef = pCons->xConsumer;` |
| 10181 |  755 | `			pCons->pDefData = pCons->pUserData;` |
|     - |  756 | `			/* Install the new consumer */` |
| 10181 |  757 | `			pCons->xConsumer = VmObConsumer;` |
| 10181 |  758 | `			pCons->pUserData = pVm;` |
|  5088 |  759 | `		}` |
|     - |  760 | `	}` |
| 10525 |  761 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
| 10525 |  762 | `	return PH7_OK;` |
|  5278 |  763 | `}` |
|     - |  764 | `/*` |
|     - |  765 | ` * bool ob_flush(void)` |
|     - |  766 | ` *  Flush (send) the output buffer.` |
|     - |  767 | ` * Parameter` |
|     - |  768 | ` *  None` |
|     - |  769 | ` * Return` |
|     - |  770 | ` *  TRUE on success, FALSE (with a notice) when no buffer is active.` |
|     - |  771 | ` */` |
|    44 |  772 | `PH7_PRIVATE int vm_builtin_ob_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  773 | `{` |
|    48 |  774 | `	ph7_vm *pVm = pCtx->pVm;` |
|    48 |  775 | `	sxu32 nUsed = SySetUsed(&pVm->aOB);` |
|     - |  776 | `	sxi32 rc;` |
|    22 |  777 | `	SXUNUSED(nArg); /* cc warning */` |
|    22 |  778 | `	SXUNUSED(apArg);` |
|    48 |  779 | `	if( VmObRefuseInHandler(pCtx) ){` |
|     3 |  780 | `		return PH7_ABORT;` |
|     - |  781 | `	}` |
|    46 |  782 | `	if( nUsed < 1 ){` |
|     3 |  783 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|     - |  784 | `			"Failed to flush buffer. No buffer to flush");` |
|     3 |  785 | `		ph7_result_bool(pCtx,0);` |
|     3 |  786 | `		return PH7_OK;` |
|     - |  787 | `	}` |
|    44 |  788 | `	if( !VmObAllows(pCtx,nUsed - 1,PH7_OB_FLUSHABLE,"flush") ){` |
|     9 |  789 | `		ph7_result_bool(pCtx,0);` |
|     9 |  790 | `		return PH7_OK;` |
|     - |  791 | `	}` |
|    36 |  792 | `	rc = VmObPerform(pVm,nUsed - 1,PH7_OB_FLUSH,0);` |
|    36 |  793 | `	ph7_result_bool(pCtx,1);` |
|    36 |  794 | `	return rc;` |
|    26 |  795 | `}` |
|     - |  796 | `/*` |
|     - |  797 | ` * void flush(void)` |
|     - |  798 | ` *  Flush the output layer to the SAPI. It does NOT touch the userland output` |
|     - |  799 | `` *  buffers: `ob_start(); echo "a"; flush();` leaves "a" in the buffer under php,`` |
|     - |  800 | ` *  where this used to be registered as ob_flush() and SENT it — so a progress-bar` |
|     - |  801 | ` *  idiom (echo, flush(), keep working) emptied a buffer the script meant to read` |
|     - |  802 | ` *  back later, and ob_get_contents() answered "".` |
|     - |  803 | ` *  This engine's own consumer writes with an unbuffered write(2)/WriteFile(), so` |
|     - |  804 | ` *  there is nothing left to push and the call is a no-op that answers nothing,` |
|     - |  805 | `` *  which is php's `void` return.`` |
|     - |  806 | ` * Parameter` |
|     - |  807 | ` *  None` |
|     - |  808 | ` * Return` |
|     - |  809 | ` *  No value is returned.` |
|     - |  810 | ` */` |
|     2 |  811 | `PH7_PRIVATE int vm_builtin_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  812 | `{` |
|     1 |  813 | `	SXUNUSED(pCtx);` |
|     1 |  814 | `	SXUNUSED(nArg); /* cc warning */` |
|     1 |  815 | `	SXUNUSED(apArg);` |
|     3 |  816 | `	return PH7_OK;` |
|     1 |  817 | `}` |
|     - |  818 | `/*` |
|     - |  819 | ` * bool ob_end_flush(void)` |
|     - |  820 | ` *  Flush (send) the output buffer and turn off output buffering.` |
|     - |  821 | ` * Parameter` |
|     - |  822 | ` *  None` |
|     - |  823 | ` * Return` |
|     - |  824 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first` |
|     - |  825 | ` *  that you called the function without an active buffer or that for some reason` |
|     - |  826 | ` *  a buffer could not be deleted (possible for special buffer).` |
|     - |  827 | ` */` |
|    64 |  828 | `PH7_PRIVATE int vm_builtin_ob_end_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  829 | `{` |
|    68 |  830 | `	ph7_vm *pVm = pCtx->pVm;` |
|    68 |  831 | `	sxu32 nUsed = SySetUsed(&pVm->aOB);` |
|     - |  832 | `	sxi32 rc;` |
|    32 |  833 | `	SXUNUSED(nArg); /* cc warning */` |
|    32 |  834 | `	SXUNUSED(apArg);` |
|    68 |  835 | `	if( VmObRefuseInHandler(pCtx) ){` |
|   ! 0 |  836 | `		return PH7_ABORT;` |
|     - |  837 | `	}` |
|    68 |  838 | `	if( nUsed < 1 ){` |
|     - |  839 | `		/* Empty stack,return FALSE */` |
|     3 |  840 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|     - |  841 | `			"Failed to delete and flush buffer. No buffer to delete or flush");` |
|     3 |  842 | `		ph7_result_bool(pCtx,0);` |
|     3 |  843 | `		return PH7_OK;` |
|     - |  844 | `	}` |
|    66 |  845 | `	if( !VmObAllows(pCtx,nUsed - 1,PH7_OB_REMOVABLE,"send") ){` |
|     7 |  846 | `		ph7_result_bool(pCtx,0);` |
|     7 |  847 | `		return PH7_OK;` |
|     - |  848 | `	}` |
|    60 |  849 | `	rc = VmObPerform(pVm,nUsed - 1,PH7_OB_FINAL,0);` |
|    60 |  850 | `	VmObPop(pVm);` |
|     - |  851 | `	/* Return true */` |
|    60 |  852 | `	ph7_result_bool(pCtx,1);` |
|    60 |  853 | `	return rc;` |
|    36 |  854 | `}` |
|     - |  855 | `/*` |
|     - |  856 | ` * void ob_implicit_flush([int $flag = true ])` |
|     - |  857 | ` *  ob_implicit_flush() will turn implicit flushing on or off.` |
|     - |  858 | ` *  Implicit flushing will result in a flush operation after every` |
|     - |  859 | ` *  output call, so that explicit calls to flush() will no longer be needed.` |
|     - |  860 | ` * Parameter` |
|     - |  861 | ` *  $flag` |
|     - |  862 | ` *   TRUE to turn implicit flushing on, FALSE otherwise.` |
|     - |  863 | ` * Return` |
|     - |  864 | ` *   Nothing` |
|     - |  865 | ` */` |
|     4 |  866 | `PH7_PRIVATE int vm_builtin_ob_implicit_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  867 | `{` |
|     - |  868 | `	/* NOTE: As of this version,this function is a no-op.` |
|     - |  869 | `	 * PH7 is smart enough to flush it's internal buffer when appropriate.` |
|     - |  870 | `	 */` |
|     2 |  871 | `	SXUNUSED(pCtx);` |
|     2 |  872 | `	SXUNUSED(nArg); /* cc warning */` |
|     2 |  873 | `	SXUNUSED(apArg);` |
|     5 |  874 | `	return PH7_OK;` |
|     1 |  875 | `}` |
|     - |  876 | `/*` |
|     - |  877 | ` * php's buffer SIZE bookkeeping, reproduced so ob_get_status() answers php's` |
|     - |  878 | ` * number rather than this engine's blob capacity. The initial allocation is` |
|     - |  879 | ` * 16 KB, or the chunk size rounded up to a 4 KB boundary when one was asked for;` |
|     - |  880 | ` * a write that would not fit grows it by whichever is larger of that initial size` |
|     - |  881 | ` * and the shortfall rounded the same way.` |
|     - |  882 | ` */` |
|     - |  883 | `#define PH7_OB_ALIGN(n)   ((((ph7_int64)(n)) + 0xFFF) & ~(ph7_int64)0xFFF)` |
|     - |  884 | `#define PH7_OB_DEFSIZE    0x4000` |
| 10548 |  885 | `static ph7_int64 VmObInitSize(VmObEntry *pEntry)` |
|     5 |  886 | `{` |
| 10553 |  887 | `	return pEntry->nChunk > 0 ? PH7_OB_ALIGN(pEntry->nChunk) : PH7_OB_DEFSIZE;` |
|     5 |  888 | `}` |
| 77356 |  889 | `static void VmObGrow(VmObEntry *pEntry,sxu32 nIncoming)` |
|     5 |  890 | `{` |
| 77361 |  891 | `	ph7_int64 nUsed = (ph7_int64)SyBlobLength(&pEntry->sOB);` |
| 77361 |  892 | `	ph7_int64 nFree = pEntry->nSize > nUsed ? pEntry->nSize - nUsed : 0;` |
| 77361 |  893 | `	if( nFree <= (ph7_int64)nIncoming ){` |
|    29 |  894 | `		ph7_int64 nInit = VmObInitSize(pEntry);` |
|    29 |  895 | `		ph7_int64 nGrow = PH7_OB_ALIGN((ph7_int64)nIncoming - nFree);` |
|    29 |  896 | `		pEntry->nSize += nGrow > nInit ? nGrow : nInit;` |
|    14 |  897 | `	}` |
| 77361 |  898 | `}` |
|     - |  899 | `/*` |
|     - |  900 | ` * Describe one buffer the way ob_get_status() does.` |
|     - |  901 | ` */` |
|   100 |  902 | `static void VmObStatusEntry(ph7_context *pCtx,VmObEntry *pEntry,sxu32 nIdx,ph7_value *pOut)` |
|     1 |  903 | `{` |
|   101 |  904 | `	ph7_vm *pVm = pCtx->pVm;` |
|   101 |  905 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|   101 |  906 | `	ph7_int64 iFlags = pEntry->iFlags;` |
|     - |  907 | `	SyBlob sName;` |
|   101 |  908 | `	if( pVal == 0 ){` |
|   ! 0 |  909 | `		return;` |
|     - |  910 | `	}` |
|   101 |  911 | `	if( VmObInHandler(pVm) && pVm->nObActive == nIdx + 1 ){` |
|     - |  912 | `		/* Asking from inside this buffer's own handler: the DISABLED mark it wears` |
|     - |  913 | `		 * for the duration of the call is bookkeeping, not an answer. */` |
|     5 |  914 | `		iFlags &= ~PH7_OB_DISABLED;` |
|     2 |  915 | `	}` |
|   101 |  916 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|   101 |  917 | `	VmObHandlerName(pVm,pEntry,&sName);` |
|   101 |  918 | `	ph7_value_string_format(pVal,"%.*s",(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName));` |
|   101 |  919 | `	ph7_array_add_strkey_elem(pOut,"name",pVal);` |
|   101 |  920 | `	SyBlobRelease(&sName);` |
|   101 |  921 | `	ph7_value_int(pVal,(iFlags & PH7_OB_USER) ? 1 : 0);` |
|   101 |  922 | `	ph7_array_add_strkey_elem(pOut,"type",pVal);` |
|   101 |  923 | `	ph7_value_int64(pVal,iFlags);` |
|   101 |  924 | `	ph7_array_add_strkey_elem(pOut,"flags",pVal);` |
|   101 |  925 | `	ph7_value_int64(pVal,(ph7_int64)nIdx);` |
|   101 |  926 | `	ph7_array_add_strkey_elem(pOut,"level",pVal);` |
|   101 |  927 | `	ph7_value_int64(pVal,pEntry->nChunk);` |
|   101 |  928 | `	ph7_array_add_strkey_elem(pOut,"chunk_size",pVal);` |
|     - |  929 | `	/* A buffer whose handler failed is not buffering at all, and php reports the` |
|     - |  930 | `	 * allocation it dropped: 0. */` |
|   101 |  931 | `	ph7_value_int64(pVal,(iFlags & PH7_OB_DISABLED) ? 0 : pEntry->nSize);` |
|   101 |  932 | `	ph7_array_add_strkey_elem(pOut,"buffer_size",pVal);` |
|   101 |  933 | `	ph7_value_int64(pVal,(ph7_int64)SyBlobLength(&pEntry->sOB));` |
|   101 |  934 | `	ph7_array_add_strkey_elem(pOut,"buffer_used",pVal);` |
|   101 |  935 | `	ph7_context_release_value(pCtx,pVal);` |
|    51 |  936 | `}` |
|     - |  937 | `/*` |
|     - |  938 | ` * array ob_get_status([bool $full_status = false])` |
|     - |  939 | ` *  Describe the active output buffers: the TOPMOST one by default (an empty array` |
|     - |  940 | ` *  when nothing is buffering), or every one of them, outermost first, when asked` |
|     - |  941 | ` *  for the full status.` |
|     - |  942 | ` * Note` |
|     - |  943 | ` *  This function did not exist here at all, so the standard way to ask what an` |
|     - |  944 | `` *  output handler is and what it may do — `ob_get_status()['flags']` — was an`` |
|     - |  945 | ` *  undefined-function fatal, and so was every framework probe that guards on it.` |
|     - |  946 | ` */` |
|    94 |  947 | `PH7_PRIVATE int vm_builtin_ob_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  948 | `{` |
|    95 |  949 | `	ph7_vm *pVm = pCtx->pVm;` |
|    95 |  950 | `	sxu32 nSeen = VmObVisible(pVm);` |
|    95 |  951 | `	int bFull = nArg > 0 && ph7_value_to_bool(apArg[0]);` |
|    95 |  952 | `	ph7_value *pArray = ph7_context_new_array(pCtx);` |
|    95 |  953 | `	if( pArray == 0 ){` |
|   ! 0 |  954 | `		ph7_result_null(pCtx);` |
|   ! 0 |  955 | `		return PH7_OK;` |
|     - |  956 | `	}` |
|    95 |  957 | `	if( bFull ){` |
|     - |  958 | `		sxu32 n;` |
|    17 |  959 | `		for( n = 0 ; n < nSeen ; ++n ){` |
|    13 |  960 | `			VmObEntry *pEntry = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|    13 |  961 | `			ph7_value *pOne = ph7_context_new_array(pCtx);` |
|    13 |  962 | `			if( pEntry == 0 \|\| pOne == 0 ){` |
|   ! 0 |  963 | `				continue;` |
|     - |  964 | `			}` |
|    13 |  965 | `			VmObStatusEntry(pCtx,pEntry,n,pOne);` |
|    13 |  966 | `			ph7_array_add_elem(pArray,0,pOne);` |
|    13 |  967 | `			ph7_context_release_value(pCtx,pOne);` |
|     7 |  968 | `		}` |
|    93 |  969 | `	}else if( nSeen > 0 ){` |
|    89 |  970 | `		VmObEntry *pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nSeen - 1);` |
|    89 |  971 | `		if( pEntry ){` |
|    89 |  972 | `			VmObStatusEntry(pCtx,pEntry,nSeen - 1,pArray);` |
|    44 |  973 | `		}` |
|    44 |  974 | `	}` |
|    95 |  975 | `	ph7_result_value(pCtx,pArray);` |
|    95 |  976 | `	return PH7_OK;` |
|    48 |  977 | `}` |
|     - |  978 | `/*` |
|     - |  979 | ` * array ob_list_handlers(void)` |
|     - |  980 | ` *  Lists all output handlers in use.` |
|     - |  981 | ` * Parameter` |
|     - |  982 | ` *  None` |
|     - |  983 | ` * Return` |
|     - |  984 | ` *  This will return an array with the output handlers in use (if any).` |
|     - |  985 | ` */` |
|    14 |  986 | `PH7_PRIVATE int vm_builtin_ob_list_handlers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  987 | `{` |
|    17 |  988 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  989 | `	ph7_value *pArray;` |
|     - |  990 | `	VmObEntry *aEntry;` |
|     - |  991 | `	ph7_value sVal;` |
|     - |  992 | `	sxu32 n;` |
|     - |  993 | `	/* Create a new array. php answers an EMPTY array when nothing is buffering` |
|     - |  994 | ``	 * (`foreach (ob_list_handlers() as ...)` over the NULL this used to answer is`` |
|     - |  995 | `	 * a TypeError), so the array is built before the stack is looked at. */` |
|    17 |  996 | `	pArray = ph7_context_new_array(pCtx);` |
|    17 |  997 | `	if( pArray == 0 ){` |
|     - |  998 | `		/* Out of memory,return NULL */` |
|   ! 0 |  999 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 | 1000 | `		SXUNUSED(apArg);` |
|   ! 0 | 1001 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1002 | `		return PH7_OK;` |
|     - | 1003 | `	}` |
|    17 | 1004 | `	PH7_MemObjInit(pVm,&sVal);` |
|     - | 1005 | `	/* Point to the installed OB entries */` |
|    17 | 1006 | `	aEntry = (VmObEntry *)SySetBasePtr(&pVm->aOB);` |
|     - | 1007 | `	/* Perform the requested operation */` |
|    35 | 1008 | `	for( n = 0 ; n < VmObVisible(pVm) ; n++ ){` |
|    21 | 1009 | `		VmObEntry *pEntry = &aEntry[n];` |
|     - | 1010 | `		/* Extract handler name */` |
|    21 | 1011 | `		SyBlobReset(&sVal.sBlob);` |
|    21 | 1012 | `		VmObHandlerName(pVm,pEntry,&sVal.sBlob);` |
|    21 | 1013 | `		sVal.iFlags = MEMOBJ_STRING;` |
|     - | 1014 | `		/* Perform the insertion */` |
|    21 | 1015 | `		ph7_array_add_elem(pArray,0/* Automatic index assign */,&sVal /* Will make it's own copy */);` |
|    12 | 1016 | `	}` |
|    17 | 1017 | `	PH7_MemObjRelease(&sVal);` |
|     - | 1018 | `	/* Return the freshly created array */` |
|    17 | 1019 | `	ph7_result_value(pCtx,pArray);` |
|    17 | 1020 | `	return PH7_OK;` |
|    10 | 1021 | `}` |
|     - | 1022 |  |
