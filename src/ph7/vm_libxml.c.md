# src/ph7/vm_libxml.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 525/544 lines (96.51%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    4 | ` */` |
|      - |    5 | `#ifdef PH7_ENABLE_LIBXML` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `#include <libxml/parser.h>` |
|      - |    8 | `#include <libxml/xmlerror.h>` |
|      - |    9 | `#include <libxml/tree.h>` |
|      - |   10 |  |
|      - |   11 | `/*` |
|      - |   12 | ` * Shared libxml2 plumbing for the DOM / XMLWriter / libxml_* surfaces.` |
|      - |   13 | ` *` |
|      - |   14 | ` * Memory model: libxml2 stays on its own (system) allocator on purpose.` |
|      - |   15 | ` * Routing it through SyMemBackend would subject libxml2 internals to the` |
|      - |   16 | ` * PHL_MAX_ALLOC fault-injection used by the stress tier, and libxml2 does` |
|      - |   17 | ` * not tolerate mid-parse OOM injection the way the engine's own code does.` |
|      - |   18 | ` * The cost is that allocation-stress tests do not exercise libxml2 OOM` |
|      - |   19 | ` * paths.` |
|      - |   20 | ` *` |
|      - |   21 | ` * Lifetime model: PH7 resources (MEMOBJ_RES) carry no destructor hook, so` |
|      - |   22 | ` * every xmlDoc created on behalf of PHP code is owned by a phl_xmldoc entry` |
|      - |   23 | ` * chained on the per-VM registry (pVm->pXmlDocs) and freed only when the VM` |
|      - |   24 | ` * is reset (a new request on a reused VM) or released -- never while PHP` |
|      - |   25 | ` * code could still hold a node wrapper into it.  Nodes unlinked from their` |
|      - |   26 | ` * tree (removeChild/replaceChild) are parked on the owning phl_xmldoc's` |
|      - |   27 | ` * aOrphans set and freed with the doc.  Docs therefore accumulate until VM` |
|      - |   28 | ` * teardown; acceptable for CLI/per-request VMs, revisit with __destruct-` |
|      - |   29 | ` * driven refcounting if long-lived embeddings ever need early release.` |
|      - |   30 | ` */` |
|      - |   31 |  |
|      - |   32 | `/*` |
|      - |   33 | ` * One-time process-global libxml2 initialization.  Safe as a plain static` |
|      - |   34 | ` * flag under PHL's current single-threaded-execution model (see the` |
|      - |   35 | ` * equivalent note in vm_pcre.c); xmlCleanupParser() is deliberately never` |
|      - |   36 | ` * called -- it is unsafe with threads and process exit reclaims everything.` |
|      - |   37 | ` */` |
|   8445 |   38 | `static void LibxmlGlobalInit(void)` |
|      5 |   39 | `{` |
|      - |   40 | `	static int bInit = 0;` |
|   8450 |   41 | `	if( !bInit ){` |
|   8450 |   42 | `		xmlInitParser();` |
|   8450 |   43 | `		bInit = 1;` |
|   4217 |   44 | `	}` |
|   8450 |   45 | `}` |
|      - |   46 | `/*` |
|      - |   47 | ` * Free one registered document: its orphaned subtrees first, then the tree` |
|      - |   48 | ` * itself.  Registry links and the phl_xmldoc shell live in SyMemBackend and` |
|      - |   49 | ` * are reclaimed with the VM allocator.` |
|      - |   50 | ` */` |
|  18624 |   51 | `static void LibxmlFreeDoc(phl_xmldoc *pDoc)` |
|      5 |   52 | `{` |
|  18629 |   53 | `	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pDoc->aOrphans);` |
|      - |   54 | `	xmlEntityPtr *apNot;` |
|      - |   55 | `	xmlAttrPtr *apNsAttr;` |
|      - |   56 | `	phl_domtpl *apTpl;` |
|      - |   57 | `	sxu32 n;` |
|  19229 |   58 | `	for( n = 0 ; n < SySetUsed(&pDoc->aOrphans) ; ++n ){` |
|    605 |   59 | `		xmlUnlinkNode(apOrphan[n]);` |
|    605 |   60 | `		xmlFreeNode(apOrphan[n]);` |
|    305 |   61 | `	}` |
|  18629 |   62 | `	SySetRelease(&pDoc->aOrphans);` |
|      - |   63 | `	/* The synthesized notation nodes, freed field by field as php frees its` |
|      - |   64 | ``	 * own: xmlFreeNode would read the entity's `length`/`etype` pair as a`` |
|      - |   65 | `	 * node's property list and walk into it. */` |
|  18629 |   66 | `	apNot = (xmlEntityPtr *)SySetBasePtr(&pDoc->aNotations);` |
|  18649 |   67 | `	for( n = 0 ; n < SySetUsed(&pDoc->aNotations) ; ++n ){` |
|     22 |   68 | `		if( apNot[n]->name ){` |
|     22 |   69 | `			xmlFree((xmlChar *)apNot[n]->name);` |
|     10 |   70 | `		}` |
|     22 |   71 | `		if( apNot[n]->ExternalID ){` |
|      5 |   72 | `			xmlFree((xmlChar *)apNot[n]->ExternalID);` |
|      2 |   73 | `		}` |
|     22 |   74 | `		if( apNot[n]->SystemID ){` |
|     20 |   75 | `			xmlFree((xmlChar *)apNot[n]->SystemID);` |
|      9 |   76 | `		}` |
|     22 |   77 | `		xmlFree(apNot[n]);` |
|     12 |   78 | `	}` |
|  18629 |   79 | `	SySetRelease(&pDoc->aNotations);` |
|      - |   80 | `	/* The stand-in attributes for this tree's namespace declarations. Each is` |
|      - |   81 | `	 * parented on its element but never spliced into its property chain, so` |
|      - |   82 | `	 * xmlFreeDoc below never reaches one; and each owns the xmlns-namespace` |
|      - |   83 | `	 * handle it carries, which xmlFreeProp does not free. */` |
|  18629 |   84 | `	apNsAttr = (xmlAttrPtr *)SySetBasePtr(&pDoc->aNsAttrs);` |
|  18689 |   85 | `	for( n = 0 ; n < SySetUsed(&pDoc->aNsAttrs) ; ++n ){` |
|     63 |   86 | `		xmlAttrPtr pAttr = apNsAttr[n];` |
|     63 |   87 | `		if( pAttr->ns ){` |
|     63 |   88 | `			xmlFreeNs(pAttr->ns);` |
|     63 |   89 | `			pAttr->ns = 0;` |
|     30 |   90 | `		}` |
|     63 |   91 | `		pAttr->parent = 0;` |
|     63 |   92 | `		pAttr->psvi = 0;` |
|     63 |   93 | `		xmlFreeProp(pAttr);` |
|     33 |   94 | `	}` |
|  18629 |   95 | `	SySetRelease(&pDoc->aNsAttrs);` |
|      - |   96 | ``	/* The content fragments of this tree's `<template>` elements. None is`` |
|      - |   97 | `	 * linked into the tree, so xmlFreeDoc below never reaches one; the entries` |
|      - |   98 | `	 * are flat, so a fragment holding a nested template frees that element` |
|      - |   99 | `	 * while the nested one's OWN fragment is freed by its own entry. */` |
|  18629 |  100 | `	apTpl = (phl_domtpl *)SySetBasePtr(&pDoc->aTemplates);` |
|  18667 |  101 | `	for( n = 0 ; n < SySetUsed(&pDoc->aTemplates) ; ++n ){` |
|     39 |  102 | `		xmlFreeNode((xmlNodePtr)apTpl[n].pFrag);` |
|     20 |  103 | `	}` |
|  18629 |  104 | `	SySetRelease(&pDoc->aTemplates);` |
|  18629 |  105 | `	if( pDoc->pDoc ){` |
|  18627 |  106 | `		xmlFreeDoc((xmlDocPtr)pDoc->pDoc);` |
|  18627 |  107 | `		pDoc->pDoc = 0;` |
|   9311 |  108 | `	}` |
|  18629 |  109 | `}` |
|      - |  110 | `/*` |
|      - |  111 | ` * Reset the per-VM libxml state between executions: drop the accumulated` |
|      - |  112 | ` * error queue and free every document from the previous request.  Called` |
|      - |  113 | ` * from PH7_VmMakeReady() (which runs once per exec, including on VM reuse` |
|      - |  114 | ` * by the -S server) alongside the other per-exec field resets.` |
|      - |  115 | ` */` |
|   7011 |  116 | `PH7_PRIVATE void PH7_LibxmlVmReset(ph7_vm *pVm)` |
|      5 |  117 | `{` |
|      - |  118 | `	phl_xmldoc *pDoc,*pNext;` |
|   7016 |  119 | `	PH7_LibxmlClearErrors(pVm);` |
|      - |  120 | `	/* A held message fragment is per-request state: a reused VM must not print` |
|      - |  121 | `	 * the previous request's tail joined to this one's first diagnostic. */` |
|   7016 |  122 | `	SyBlobReset(&pVm->sLibxmlPend);` |
|   7016 |  123 | `	pVm->bLibxmlInternalErr = 0;` |
|   7016 |  124 | `	pDoc = (phl_xmldoc *)pVm->pXmlDocs;` |
|  25640 |  125 | `	while( pDoc ){` |
|  18629 |  126 | `		pNext = pDoc->pNext;` |
|  18629 |  127 | `		LibxmlFreeDoc(pDoc);` |
|  18629 |  128 | `		SyMemBackendFree(&pVm->sAllocator,pDoc);` |
|  18629 |  129 | `		pDoc = pNext;` |
|      5 |  130 | `	}` |
|   7016 |  131 | `	pVm->pXmlDocs = 0;` |
|      - |  132 | `	/* The ownerless shell rode the chain just freed. */` |
|   7016 |  133 | `	pVm->pXmlLimbo = 0;` |
|      - |  134 | `	/* XMLWriter buffers live outside SyMemBackend too (see vm_xmlwriter.c) */` |
|   7016 |  135 | `	PH7_XmlWriterVmSweep(pVm);` |
|      - |  136 | `	/* ext/xml push parsers: their ctxt/myDoc are libxml allocations and the` |
|      - |  137 | `	 * handler VALUES hold references that must drop before the allocator goes. */` |
|   7016 |  138 | `	PH7_XmlParserVmSweep(pVm);` |
|      - |  139 | `	/* The entity-loader/streams-context slots hold per-request VALUES (a` |
|      - |  140 | `	 * closure, a context resource): drop them so a reused VM starts default. */` |
|   7016 |  141 | `	PH7_MemObjRelease(&pVm->sXmlEntLoader);` |
|   7016 |  142 | `	PH7_MemObjRelease(&pVm->sXmlStreamsCtx);` |
|   7016 |  143 | `}` |
|      - |  144 | `/*` |
|      - |  145 | ` * Final teardown on VM release.  Must run before SyMemBackendRelease()` |
|      - |  146 | ` * wipes the allocator that holds the registry shells.` |
|      - |  147 | ` */` |
|   6995 |  148 | `PH7_PRIVATE void PH7_LibxmlVmRelease(ph7_vm *pVm)` |
|      5 |  149 | `{` |
|   7000 |  150 | `	PH7_LibxmlVmReset(pVm);` |
|   7000 |  151 | `	SySetRelease(&pVm->aLibxmlErr);` |
|   7000 |  152 | `	SyBlobRelease(&pVm->sLibxmlPend);` |
|   7000 |  153 | `}` |
|      - |  154 | `/*` |
|      - |  155 | ` * Release the copied message/file strings of one queue entry.` |
|      - |  156 | ` */` |
|    588 |  157 | `static void LibxmlFreeErr(ph7_vm *pVm,phl_libxml_err *pErr)` |
|      2 |  158 | `{` |
|    590 |  159 | `	if( pErr->sMsg.zString ){` |
|    590 |  160 | `		SyMemBackendFree(&pVm->sAllocator,(void *)pErr->sMsg.zString);` |
|    274 |  161 | `	}` |
|    590 |  162 | `	if( pErr->sFile.zString ){` |
|    180 |  163 | `		SyMemBackendFree(&pVm->sAllocator,(void *)pErr->sFile.zString);` |
|     84 |  164 | `	}` |
|    590 |  165 | `	SyStringInitFromBuf(&pErr->sMsg,0,0);` |
|    590 |  166 | `	SyStringInitFromBuf(&pErr->sFile,0,0);` |
|    590 |  167 | `}` |
|      - |  168 | `/*` |
|      - |  169 | ` * Empty the libxml error queue, releasing the copied strings.  The` |
|      - |  170 | ` * last-error slot is kept (php parity: use_internal_errors(false) drops` |
|      - |  171 | ` * the buffer but libxml_get_last_error still reports).` |
|      - |  172 | ` */` |
|   7371 |  173 | `static void LibxmlClearQueue(ph7_vm *pVm)` |
|      5 |  174 | `{` |
|   7376 |  175 | `	phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|      - |  176 | `	sxu32 n;` |
|   7475 |  177 | `	for( n = 0 ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|    101 |  178 | `		LibxmlFreeErr(pVm,&aErr[n]);` |
|     49 |  179 | `	}` |
|   7376 |  180 | `	SySetReset(&pVm->aLibxmlErr);` |
|   7376 |  181 | `}` |
|      - |  182 | `/*` |
|      - |  183 | ` * libxml_clear_errors(): drop the queue AND the last-error slot.` |
|      - |  184 | ` */` |
|   7231 |  185 | `PH7_PRIVATE void PH7_LibxmlClearErrors(ph7_vm *pVm)` |
|      5 |  186 | `{` |
|   7236 |  187 | `	LibxmlClearQueue(pVm);` |
|   7236 |  188 | `	if( pVm->pLibxmlLastErr ){` |
|     73 |  189 | `		LibxmlFreeErr(pVm,(phl_libxml_err *)pVm->pLibxmlLastErr);` |
|     73 |  190 | `		SyMemBackendFree(&pVm->sAllocator,pVm->pLibxmlLastErr);` |
|     73 |  191 | `		pVm->pLibxmlLastErr = 0;` |
|     35 |  192 | `	}` |
|   7236 |  193 | `}` |
|      - |  194 | `/*` |
|      - |  195 | ` * Push one error onto the per-VM queue and last-error slot, copying the` |
|      - |  196 | ` * message/file strings.  The typed structured-error callback below and the` |
|      - |  197 | ` * DOM schema hooks (vm_dom.c) both funnel through this, keeping the` |
|      - |  198 | ` * queue-building logic in one place and ph7int.h free of libxml types.` |
|      - |  199 | ` */` |
|    294 |  200 | `PH7_PRIVATE void PH7_LibxmlQueueError(ph7_vm *pVm,int iLevel,int iCode,int iLine,int iColumn,` |
|      - |  201 | `	const char *zMsg,const char *zFile)` |
|      2 |  202 | `{` |
|      - |  203 | `	phl_libxml_err sEntry;` |
|      - |  204 | `	phl_libxml_err *pLast;` |
|    296 |  205 | `	if( pVm == 0 ){` |
|    ! 0 |  206 | `		return;` |
|      - |  207 | `	}` |
|    296 |  208 | `	SyZero(&sEntry,sizeof(sEntry));` |
|    296 |  209 | `	sEntry.iLevel = iLevel;` |
|    296 |  210 | `	sEntry.iCode = iCode;` |
|    296 |  211 | `	sEntry.iLine = iLine;` |
|    296 |  212 | `	sEntry.iColumn = iColumn;` |
|    296 |  213 | `	if( zMsg ){` |
|    296 |  214 | `		sxu32 nMsg = SyStrlen(zMsg);` |
|    296 |  215 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,zMsg,nMsg);` |
|    296 |  216 | `		if( zDup ){` |
|      - |  217 | `			/* Trailing newline kept: php's LibXMLError->message preserves it */` |
|    296 |  218 | `			SyStringInitFromBuf(&sEntry.sMsg,zDup,nMsg);` |
|    137 |  219 | `		}` |
|    137 |  220 | `	}` |
|    296 |  221 | `	if( zFile ){` |
|     91 |  222 | `		sxu32 nFile = SyStrlen(zFile);` |
|     91 |  223 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,zFile,nFile);` |
|     91 |  224 | `		if( zDup ){` |
|     91 |  225 | `			SyStringInitFromBuf(&sEntry.sFile,zDup,nFile);` |
|     42 |  226 | `		}` |
|     42 |  227 | `	}` |
|    296 |  228 | `	SySetPut(&pVm->aLibxmlErr,(const void *)&sEntry);` |
|      - |  229 | `	/* Mirror into the last-error slot (independent string copies so queue` |
|      - |  230 | `	 * draining cannot invalidate it). */` |
|    296 |  231 | `	pLast = (phl_libxml_err *)pVm->pLibxmlLastErr;` |
|    296 |  232 | `	if( pLast == 0 ){` |
|     73 |  233 | `		pLast = (phl_libxml_err *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_libxml_err));` |
|     73 |  234 | `		if( pLast == 0 ){` |
|    ! 0 |  235 | `			return;` |
|      - |  236 | `		}` |
|     73 |  237 | `		SyZero(pLast,sizeof(phl_libxml_err));` |
|     73 |  238 | `		pVm->pLibxmlLastErr = (void *)pLast;` |
|     37 |  239 | `	}else{` |
|    225 |  240 | `		LibxmlFreeErr(pVm,pLast);` |
|      - |  241 | `	}` |
|    296 |  242 | `	pLast->iLevel = iLevel;` |
|    296 |  243 | `	pLast->iCode = iCode;` |
|    296 |  244 | `	pLast->iLine = iLine;` |
|    296 |  245 | `	pLast->iColumn = iColumn;` |
|    296 |  246 | `	if( sEntry.sMsg.zString ){` |
|    296 |  247 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,sEntry.sMsg.zString,sEntry.sMsg.nByte);` |
|    296 |  248 | `		if( zDup ){` |
|    296 |  249 | `			SyStringInitFromBuf(&pLast->sMsg,zDup,sEntry.sMsg.nByte);` |
|    137 |  250 | `		}` |
|    137 |  251 | `	}` |
|    296 |  252 | `	if( sEntry.sFile.zString ){` |
|     91 |  253 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,sEntry.sFile.zString,sEntry.sFile.nByte);` |
|     91 |  254 | `		if( zDup ){` |
|     91 |  255 | `			SyStringInitFromBuf(&pLast->sFile,zDup,sEntry.sFile.nByte);` |
|     42 |  256 | `		}` |
|     42 |  257 | `	}` |
|    139 |  258 | `}` |
|      - |  259 | `/*` |
|      - |  260 | ` * Structured-error callback installed while a libxml2 entry point runs` |
|      - |  261 | ` * between PH7_LibxmlCaptureBegin/End.  Forwards to PH7_LibxmlQueueError;` |
|      - |  262 | ` * the capture-end decides whether entries stay queued or drain as` |
|      - |  263 | ` * php-style warnings.` |
|      - |  264 | ` */` |
|      - |  265 | `#if LIBXML_VERSION >= 21200` |
|     93 |  266 | `static void LibxmlStructuredErr(void *pUserData,const xmlError *pErr)` |
|      - |  267 | `#else` |
|    112 |  268 | `static void LibxmlStructuredErr(void *pUserData,xmlErrorPtr pErr)` |
|      - |  269 | `#endif` |
|      2 |  270 | `{` |
|    207 |  271 | `	if( pErr == 0 ){` |
|    ! 0 |  272 | `		return;` |
|      - |  273 | `	}` |
|      - |  274 | `	/* libxml keeps the column in int2 */` |
|    300 |  275 | `	PH7_LibxmlQueueError((ph7_vm *)pUserData,(int)pErr->level,pErr->code,pErr->line,` |
|    205 |  276 | `		pErr->int2,pErr->message,pErr->file);` |
|     95 |  277 | `}` |
|      - |  278 | `/*` |
|      - |  279 | ` * ...and libxml's OTHER error channel. A handful of diagnostics never reach` |
|      - |  280 | ` * the structured handler at all: they are printed with xmlGenericError()` |
|      - |  281 | ` * directly, whose default writes them to stderr. XPath is where a program` |
|      - |  282 | `` * meets them -- `zz:nope()` under an unbound prefix says "xmlXPathCompOpEval:`` |
|      - |  283 | ` * function nope bound to undefined prefix zz" through this channel -- and` |
|      - |  284 | ` * before this handler they went to the terminal, invisible to` |
|      - |  285 | ` * libxml_get_errors() and to a program's error handler alike, in the middle` |
|      - |  286 | ` * of whatever the script was writing. php captures them, at libxml's ERROR` |
|      - |  287 | ` * level under code 1 with no file or line, and this says the same.` |
|      - |  288 | ` *` |
|      - |  289 | ` * The signature is printf-style, so the message is formatted here.` |
|      - |  290 | ` */` |
|     53 |  291 | `static void LibxmlGenericErr(void *pUserData,const char *zFmt,...)` |
|      1 |  292 | `{` |
|     54 |  293 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  294 | `	SyBlob sMsg;` |
|      - |  295 | `	va_list ap;` |
|     54 |  296 | `	if( pVm == 0 \|\| zFmt == 0 ){` |
|    ! 0 |  297 | `		return;` |
|      - |  298 | `	}` |
|     54 |  299 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     54 |  300 | `	va_start(ap,zFmt);` |
|     54 |  301 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|     54 |  302 | `	va_end(ap);` |
|      - |  303 | `	/* The message keeps its newline: php's own generic handler is LINE-buffered` |
|      - |  304 | `	 * and this channel does not deliver whole lines. libxml's default` |
|      - |  305 | ``	 * parser-error routine calls it FOUR times for one error -- `Entity: line`` |
|      - |  306 | ``	 * 1: `, `parser `, `error : `, then the message and a newline -- and php`` |
|      - |  307 | `	 * prints the four as the one line they are. Trimming each fragment here` |
|      - |  308 | `	 * instead printed four warnings for one diagnostic; the drain does the` |
|      - |  309 | `	 * flushing, exactly as it does for a structured message. */` |
|     54 |  310 | `	SyBlobAppend(&sMsg,"",1);   /* the queue copies a C string */` |
|     54 |  311 | `	PH7_LibxmlQueueError(pVm,XML_ERR_ERROR,1,0,0,(const char *)SyBlobData(&sMsg),"");` |
|     54 |  312 | `	SyBlobRelease(&sMsg);` |
|     25 |  313 | `}` |
|      - |  314 | `/* xmlSetGenericErrorFunc is deprecated from libxml 2.12 and the MSVC gate` |
|      - |  315 | ` * refuses a deprecated symbol under /WX. It is still the only door onto that` |
|      - |  316 | ` * channel, so the deprecation is suppressed at this one call pair. */` |
|  16742 |  317 | `static void LibxmlGenericSet(ph7_vm *pVm,int bOn)` |
|      5 |  318 | `{` |
|      - |  319 | `#if defined(_MSC_VER)` |
|      - |  320 | `#pragma warning(push)` |
|      - |  321 | `#pragma warning(disable:4996)` |
|      - |  322 | `#endif` |
|  16747 |  323 | `	xmlSetGenericErrorFunc(bOn ? (void *)pVm : 0,bOn ? LibxmlGenericErr : 0);` |
|      - |  324 | `#if defined(_MSC_VER)` |
|      - |  325 | `#pragma warning(pop)` |
|      - |  326 | `#endif` |
|  16747 |  327 | `}` |
|      - |  328 | `/*` |
|      - |  329 | ` * Bracket a libxml2 entry point.  Begin installs the structured handler` |
|      - |  330 | ` * routed at this VM and returns the current queue depth; End restores the` |
|      - |  331 | ` * default handler and, when libxml_use_internal_errors() is OFF, drains` |
|      - |  332 | ` * every entry recorded since the mark as php-style warnings:` |
|      - |  333 | ` *   funcname(): <message> in <Entity\|file>, line: <n>` |
|      - |  334 | ` * (php's exact wording for the memory-parser case).` |
|      - |  335 | ` */` |
|   8198 |  336 | `PH7_PRIVATE sxu32 PH7_LibxmlCaptureBegin(ph7_vm *pVm)` |
|      5 |  337 | `{` |
|   8203 |  338 | `	xmlSetStructuredErrorFunc(pVm,LibxmlStructuredErr);` |
|   8203 |  339 | `	LibxmlGenericSet(pVm,1);` |
|   8203 |  340 | `	return SySetUsed(&pVm->aLibxmlErr);` |
|      5 |  341 | `}` |
|      - |  342 | `/*` |
|      - |  343 | ` * The same window for an entry point that leaves libxml's OWN formatting in` |
|      - |  344 | ` * place -- ext/simplexml's parse, which php reports through the generic channel` |
|      - |  345 | ` * and not through the structured one.` |
|      - |  346 | ` *` |
|      - |  347 | ` * The difference is visible in every parse diagnostic the two extensions raise:` |
|      - |  348 | `` * ext/dom's is php's one-line `StartTag: invalid element name in Entity,`` |
|      - |  349 | `` * line: 1`, while ext/simplexml's is libxml's own three lines --`` |
|      - |  350 | `` * `Entity: line 1: parser error : StartTag: invalid element name`, the offending`` |
|      - |  351 | ` * source line, and a caret under it. That is what libxml's default parser-error` |
|      - |  352 | ` * routine prints, and it only runs when no structured handler is installed.` |
|      - |  353 | `` * With `libxml_use_internal_errors()` ON php's structured handler takes`` |
|      - |  354 | `` * precedence there too, and the queue `libxml_get_errors()` answers carries the`` |
|      - |  355 | ` * error CODES -- so the structured handler is still installed for that case.` |
|      - |  356 | ` */` |
|    174 |  357 | `PH7_PRIVATE sxu32 PH7_LibxmlCaptureBeginRaw(ph7_vm *pVm)` |
|      4 |  358 | `{` |
|    178 |  359 | `	xmlSetStructuredErrorFunc(pVm->bLibxmlInternalErr ? pVm : 0,` |
|    174 |  360 | `		pVm->bLibxmlInternalErr ? LibxmlStructuredErr : 0);` |
|    178 |  361 | `	LibxmlGenericSet(pVm,1);` |
|    178 |  362 | `	return SySetUsed(&pVm->aLibxmlErr);` |
|      4 |  363 | `}` |
|      - |  364 | `/*` |
|      - |  365 | ` * End a capture window WITHOUT reporting what it caught: for an entry point` |
|      - |  366 | ` * whose failure php reports through the return value alone (a stream write` |
|      - |  367 | ` * that could not land under XMLWriter), where libxml's own message would be a` |
|      - |  368 | ` * warning php never raises. Entries stay queued when internal capture is on --` |
|      - |  369 | `` * `libxml_get_errors()` is the one place php does show them.`` |
|      - |  370 | ` */` |
|    200 |  371 | `PH7_PRIVATE void PH7_LibxmlDropErrors(ph7_vm *pVm,sxu32 nMark)` |
|      3 |  372 | `{` |
|    203 |  373 | `	xmlSetStructuredErrorFunc(0,0);` |
|    203 |  374 | `	LibxmlGenericSet(pVm,0);` |
|    203 |  375 | `	if( pVm->bLibxmlInternalErr ){` |
|    ! 0 |  376 | `		return;` |
|      - |  377 | `	}` |
|    203 |  378 | `	if( SySetUsed(&pVm->aLibxmlErr) > nMark ){` |
|     15 |  379 | `		phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|      - |  380 | `		sxu32 n;` |
|     31 |  381 | `		for( n = nMark ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|     17 |  382 | `			LibxmlFreeErr(pVm,&aErr[n]);` |
|      8 |  383 | `		}` |
|     15 |  384 | `		SySetTruncate(&pVm->aLibxmlErr,nMark);` |
|      7 |  385 | `	}` |
|    103 |  386 | `}` |
|   4910 |  387 | `PH7_PRIVATE void PH7_LibxmlCaptureEnd(ph7_vm *pVm,sxu32 nMark,const char *zFnName)` |
|      5 |  388 | `{` |
|   4915 |  389 | `	PH7_LibxmlCaptureEndOpts(pVm,nMark,zFnName,0);` |
|   4915 |  390 | `}` |
|      - |  391 | `/*` |
|      - |  392 | ` * The same drain for a parse that was given OPTIONS, two of which are about` |
|      - |  393 | `` * these very diagnostics: `LIBXML_NOERROR` silences the errors and the fatals,`` |
|      - |  394 | `` * `LIBXML_NOWARNING` the warnings. php silences them at the PRINT, not at the`` |
|      - |  395 | `` * source -- the queue `libxml_get_errors()` answers still holds them, and so`` |
|      - |  396 | `` * does the slot `libxml_get_last_error()` reads -- so this skips them here,`` |
|      - |  397 | ` * after they have been recorded, and the line buffer never sees them either` |
|      - |  398 | ` * (php's does not: libxml's message never reaches the printer at all).` |
|      - |  399 | ` */` |
|   8170 |  400 | `PH7_PRIVATE void PH7_LibxmlCaptureEndOpts(ph7_vm *pVm,sxu32 nMark,const char *zFnName,int iOpts)` |
|      5 |  401 | `{` |
|   8175 |  402 | `	xmlSetStructuredErrorFunc(0,0);` |
|   8175 |  403 | `	LibxmlGenericSet(pVm,0);` |
|   8175 |  404 | `	if( pVm->bLibxmlInternalErr ){` |
|      - |  405 | `		/* Internal capture on: entries stay queued for libxml_get_errors() */` |
|    124 |  406 | `		return;` |
|      - |  407 | `	}` |
|   8053 |  408 | `	if( SySetUsed(&pVm->aLibxmlErr) > nMark ){` |
|    235 |  409 | `		phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|      - |  410 | `		sxu32 n;` |
|    414 |  411 | `		for( n = nMark ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|      - |  412 | `			SyString sFunc;` |
|      - |  413 | `			SyBlob sMsg;` |
|      - |  414 | `			const char *zPend;` |
|      - |  415 | `			sxu32 nPend,nTrim;` |
|    407 |  416 | `			if( (aErr[n].iLevel == XML_ERR_WARNING)` |
|    148 |  417 | `			 ? (iOpts & XML_PARSE_NOWARNING) != 0` |
|    165 |  418 | `			 : (iOpts & XML_PARSE_NOERROR) != 0 ){` |
|    154 |  419 | `				LibxmlFreeErr(pVm,&aErr[n]);` |
|    171 |  420 | `				continue;` |
|      - |  421 | `			}` |
|      - |  422 | `			/* php's libxml diagnostics are LINE-buffered: each message is` |
|      - |  423 | `			 * appended to one buffer and the diagnostic is only raised when the` |
|      - |  424 | `			 * accumulated text ends in a newline. Most of libxml's messages do,` |
|      - |  425 | `			 * so most of them stand alone -- but the ones that do not (libxml's` |
|      - |  426 | `			 * "Validation failed: no DTD found !" is one) are held back and` |
|      - |  427 | `			 * printed JOINED to whatever comes next, even from a later parse of` |
|      - |  428 | `			 * a different document, and are never printed at all if nothing` |
|      - |  429 | `			 * else follows. Reproduced rather than tidied up: a program's` |
|      - |  430 | `			 * output is what it is. */` |
|    160 |  431 | `			SyBlobAppend(&pVm->sLibxmlPend,aErr[n].sMsg.zString,aErr[n].sMsg.nByte);` |
|    160 |  432 | `			zPend = (const char *)SyBlobData(&pVm->sLibxmlPend);` |
|    160 |  433 | `			nPend = SyBlobLength(&pVm->sLibxmlPend);` |
|    160 |  434 | `			if( nPend < 1 \|\| zPend[nPend-1] != '\n' ){` |
|      - |  435 | `				/* no line yet: hold it for the next message */` |
|     36 |  436 | `				LibxmlFreeErr(pVm,&aErr[n]);` |
|     36 |  437 | `				continue;` |
|      - |  438 | `			}` |
|    126 |  439 | `			nTrim = nPend;` |
|      - |  440 | `			/* php trims the trailing newline off the warning copy */` |
|    306 |  441 | `			while( nTrim > 0 && (zPend[nTrim-1] == '\n' \|\| zPend[nTrim-1] == '\r') ){` |
|    126 |  442 | `				nTrim--;` |
|      2 |  443 | `			}` |
|    126 |  444 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    126 |  445 | `			SyBlobAppend(&sMsg,zPend,nTrim);` |
|      - |  446 | `			/* php appends the source location only for parser errors that` |
|      - |  447 | `			 * carry a real line; generic libxml errors print bare. The location` |
|      - |  448 | `			 * and the LEVEL are the flushing message's, not the held one's. */` |
|    126 |  449 | `			if( aErr[n].iLine > 0 ){` |
|     65 |  450 | `				if( aErr[n].sFile.nByte > 0 ){` |
|      7 |  451 | `					SyBlobFormat(&sMsg," in %z, line: %d",&aErr[n].sFile,aErr[n].iLine);` |
|      4 |  452 | `				}else{` |
|     59 |  453 | `					SyBlobFormat(&sMsg," in Entity, line: %d",aErr[n].iLine);` |
|      - |  454 | `				}` |
|     29 |  455 | `			}` |
|    126 |  456 | `			SyBlobAppend(&sMsg,"\0",1);` |
|    126 |  457 | `			if( zFnName ){` |
|    126 |  458 | `				SyStringInitFromBuf(&sFunc,zFnName,SyStrlen(zFnName));` |
|     56 |  459 | `			}` |
|      - |  460 | `			/* php reports libxml's own SEVERITY: a warning (an unsupported XML` |
|      - |  461 | `			 * version, a DTD the content does not follow) is an E_NOTICE there` |
|      - |  462 | `			 * and only an error or a fatal is an E_WARNING. The distinction is` |
|      - |  463 | ``			 * visible to any set_error_handler() and to `error_reporting` --`` |
|      - |  464 | `			 * a handler screening on E_WARNING must not see the warnings. */` |
|    126 |  465 | `			PH7_VmThrowError(&(*pVm),zFnName ? &sFunc : 0,` |
|    124 |  466 | `				aErr[n].iLevel == XML_ERR_WARNING ? PH7_CTX_NOTICE : PH7_CTX_WARNING,` |
|    124 |  467 | `				(const char *)SyBlobData(&sMsg));` |
|    126 |  468 | `			SyBlobRelease(&sMsg);` |
|    126 |  469 | `			SyBlobReset(&pVm->sLibxmlPend);` |
|    126 |  470 | `			LibxmlFreeErr(pVm,&aErr[n]);` |
|     58 |  471 | `		}` |
|    103 |  472 | `		SySetTruncate(&pVm->aLibxmlErr,nMark);` |
|     51 |  473 | `	}` |
|   3958 |  474 | `}` |
|      - |  475 | `/*` |
|      - |  476 | ` * php's OTHER libxml channel.` |
|      - |  477 | ` *` |
|      - |  478 | ` * libxml reports an I/O failure through its GENERIC error function rather than` |
|      - |  479 | ` * the structured one, with the severity spelled INTO the text ("I/O warning :` |
|      - |  480 | ` * ..."), and php prints that text at E_WARNING whatever the severity says --` |
|      - |  481 | `` * while the structured copy, which is what `libxml_get_errors()` reports, keeps`` |
|      - |  482 | ` * libxml's own level and carries no such prefix. A caller with a message from` |
|      - |  483 | ` * that channel hands it here: it goes through the same line buffer as every` |
|      - |  484 | ` * other diagnostic (so a held fragment is printed in front of it) and takes no` |
|      - |  485 | ` * source location, because that channel has none.` |
|      - |  486 | ` */` |
|      6 |  487 | `PH7_PRIVATE void PH7_LibxmlRaiseGeneric(ph7_vm *pVm,const char *zFnName,const char *zMsg)` |
|      1 |  488 | `{` |
|      - |  489 | `	SyString sFunc;` |
|      - |  490 | `	SyBlob sOut;` |
|      - |  491 | `	const char *zPend;` |
|      - |  492 | `	sxu32 nPend,nTrim;` |
|      7 |  493 | `	SyBlobAppend(&pVm->sLibxmlPend,zMsg,SyStrlen(zMsg));` |
|      7 |  494 | `	zPend = (const char *)SyBlobData(&pVm->sLibxmlPend);` |
|      7 |  495 | `	nPend = SyBlobLength(&pVm->sLibxmlPend);` |
|      7 |  496 | `	if( nPend < 1 \|\| zPend[nPend-1] != '\n' ){` |
|    ! 0 |  497 | `		return;` |
|      - |  498 | `	}` |
|      7 |  499 | `	nTrim = nPend;` |
|     16 |  500 | `	while( nTrim > 0 && (zPend[nTrim-1] == '\n' \|\| zPend[nTrim-1] == '\r') ){` |
|      7 |  501 | `		nTrim--;` |
|      1 |  502 | `	}` |
|      7 |  503 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      7 |  504 | `	SyBlobAppend(&sOut,zPend,nTrim);` |
|      7 |  505 | `	SyBlobAppend(&sOut,"\0",sizeof(char));` |
|      7 |  506 | `	SyStringInitFromBuf(&sFunc,zFnName,zFnName ? SyStrlen(zFnName) : 0);` |
|      4 |  507 | `	PH7_VmThrowError(&(*pVm),zFnName ? &sFunc : 0,PH7_CTX_WARNING,` |
|      6 |  508 | `		(const char *)SyBlobData(&sOut));` |
|      7 |  509 | `	SyBlobRelease(&sOut);` |
|      7 |  510 | `	SyBlobReset(&pVm->sLibxmlPend);` |
|      4 |  511 | `}` |
|      - |  512 | `/*` |
|      - |  513 | ` * Allocate and register a new document shell on the per-VM registry.` |
|      - |  514 | ` * Returns NULL on allocation failure (the caller reports OOM).` |
|      - |  515 | ` */` |
|  18624 |  516 | `PH7_PRIVATE phl_xmldoc * PH7_LibxmlNewDoc(ph7_vm *pVm,void *pXmlDocPtr)` |
|      5 |  517 | `{` |
|      - |  518 | `	phl_xmldoc *pDoc;` |
|  18629 |  519 | `	pDoc = (phl_xmldoc *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_xmldoc));` |
|  18629 |  520 | `	if( pDoc == 0 ){` |
|    ! 0 |  521 | `		return 0;` |
|      - |  522 | `	}` |
|  18629 |  523 | `	SyZero(pDoc,sizeof(phl_xmldoc));` |
|  18629 |  524 | `	SySetInit(&pDoc->aOrphans,&pVm->sAllocator,sizeof(void *));` |
|  18629 |  525 | `	SySetInit(&pDoc->aNotations,&pVm->sAllocator,sizeof(void *));` |
|  18629 |  526 | `	SySetInit(&pDoc->aNsAttrs,&pVm->sAllocator,sizeof(void *));` |
|  18629 |  527 | `	SySetInit(&pDoc->aTemplates,&pVm->sAllocator,sizeof(phl_domtpl));` |
|  18629 |  528 | `	pDoc->pDoc = pXmlDocPtr;` |
|  18629 |  529 | `	pDoc->pVm = &(*pVm);` |
|  18629 |  530 | `	pDoc->bPreserveWS = 1;  /* DOMDocument->preserveWhiteSpace default */` |
|  18629 |  531 | `	pDoc->bFormatOutput = 0;` |
|  18629 |  532 | `	pDoc->pNext = (phl_xmldoc *)pVm->pXmlDocs;` |
|  18629 |  533 | `	pVm->pXmlDocs = (void *)pDoc;` |
|  18629 |  534 | `	return pDoc;` |
|   9317 |  535 | `}` |
|      - |  536 |  |
|      - |  537 | `/* ===== Constants (php ext/libxml + ext/dom node types) ===== */` |
|      - |  538 |  |
|      - |  539 | `#define LIBXML_INT_CONST(FN,VALUE) \` |
|      - |  540 | `	static void FN(ph7_value *pVal,void *pUnused){ \` |
|      - |  541 | `		SXUNUSED(pUnused); \` |
|      - |  542 | `		ph7_value_int64(pVal,(ph7_int64)(VALUE)); \` |
|      - |  543 | `	}` |
|     85 |  544 | `LIBXML_INT_CONST(LibxmlConst_VERSION,        LIBXML_VERSION)` |
|     89 |  545 | `LIBXML_INT_CONST(LibxmlConst_RECOVER,        XML_PARSE_RECOVER)` |
|     91 |  546 | `LIBXML_INT_CONST(LibxmlConst_HTML_NOIMPLIED, 8192)   /* HTML_PARSE_NOIMPLIED */` |
|     91 |  547 | `LIBXML_INT_CONST(LibxmlConst_HTML_NODEFDTD,  4)      /* HTML_PARSE_NODEFDTD */` |
|     85 |  548 | `LIBXML_INT_CONST(LibxmlConst_NOENT,          XML_PARSE_NOENT)` |
|     85 |  549 | `LIBXML_INT_CONST(LibxmlConst_DTDLOAD,        XML_PARSE_DTDLOAD)` |
|     85 |  550 | `LIBXML_INT_CONST(LibxmlConst_DTDATTR,        XML_PARSE_DTDATTR)` |
|     85 |  551 | `LIBXML_INT_CONST(LibxmlConst_DTDVALID,       XML_PARSE_DTDVALID)` |
|  13089 |  552 | `LIBXML_INT_CONST(LibxmlConst_NOERROR,        XML_PARSE_NOERROR)` |
|    103 |  553 | `LIBXML_INT_CONST(LibxmlConst_NOWARNING,      XML_PARSE_NOWARNING)` |
|     85 |  554 | `LIBXML_INT_CONST(LibxmlConst_NOBLANKS,       XML_PARSE_NOBLANKS)` |
|     85 |  555 | `LIBXML_INT_CONST(LibxmlConst_XINCLUDE,       XML_PARSE_XINCLUDE)` |
|     85 |  556 | `LIBXML_INT_CONST(LibxmlConst_NSCLEAN,        XML_PARSE_NSCLEAN)` |
|     87 |  557 | `LIBXML_INT_CONST(LibxmlConst_NOCDATA,        XML_PARSE_NOCDATA)` |
|     85 |  558 | `LIBXML_INT_CONST(LibxmlConst_NONET,          XML_PARSE_NONET)` |
|     85 |  559 | `LIBXML_INT_CONST(LibxmlConst_PEDANTIC,       XML_PARSE_PEDANTIC)` |
|     87 |  560 | `LIBXML_INT_CONST(LibxmlConst_COMPACT,        XML_PARSE_COMPACT)` |
|     85 |  561 | `LIBXML_INT_CONST(LibxmlConst_PARSEHUGE,      XML_PARSE_HUGE)` |
|     85 |  562 | `LIBXML_INT_CONST(LibxmlConst_BIGLINES,       XML_PARSE_BIG_LINES)` |
|     95 |  563 | `LIBXML_INT_CONST(LibxmlConst_NOXMLDECL,      2)      /* XML_SAVE_NO_DECL */` |
|     93 |  564 | `LIBXML_INT_CONST(LibxmlConst_NOEMPTYTAG,     4)      /* XML_SAVE_NO_EMPTY */` |
|     89 |  565 | `LIBXML_INT_CONST(LibxmlConst_SCHEMA_CREATE,  1)      /* XML_SCHEMA_VAL_VC_I_CREATE */` |
|     85 |  566 | `LIBXML_INT_CONST(LibxmlConst_ERR_NONE,       XML_ERR_NONE)` |
|     85 |  567 | `LIBXML_INT_CONST(LibxmlConst_ERR_WARNING,    XML_ERR_WARNING)` |
|     85 |  568 | `LIBXML_INT_CONST(LibxmlConst_ERR_ERROR,      XML_ERR_ERROR)` |
|     87 |  569 | `LIBXML_INT_CONST(LibxmlConst_ERR_FATAL,      XML_ERR_FATAL)` |
|      - |  570 | `/* ext/dom node-type constants (values fixed by the DOM spec / libxml enums) */` |
|    409 |  571 | `LIBXML_INT_CONST(LibxmlConst_ELEMENT_NODE,        XML_ELEMENT_NODE)` |
|     87 |  572 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_NODE,      XML_ATTRIBUTE_NODE)` |
|    351 |  573 | `LIBXML_INT_CONST(LibxmlConst_TEXT_NODE,           XML_TEXT_NODE)` |
|     87 |  574 | `LIBXML_INT_CONST(LibxmlConst_CDATA_SECTION_NODE,  XML_CDATA_SECTION_NODE)` |
|     87 |  575 | `LIBXML_INT_CONST(LibxmlConst_ENTITY_REF_NODE,     XML_ENTITY_REF_NODE)` |
|     87 |  576 | `LIBXML_INT_CONST(LibxmlConst_ENTITY_NODE,         XML_ENTITY_NODE)` |
|     87 |  577 | `LIBXML_INT_CONST(LibxmlConst_PI_NODE,             XML_PI_NODE)` |
|    157 |  578 | `LIBXML_INT_CONST(LibxmlConst_COMMENT_NODE,        XML_COMMENT_NODE)` |
|     87 |  579 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_NODE,       XML_DOCUMENT_NODE)` |
|     89 |  580 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_TYPE_NODE,  XML_DOCUMENT_TYPE_NODE)` |
|     87 |  581 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_FRAG_NODE,  XML_DOCUMENT_FRAG_NODE)` |
|     87 |  582 | `LIBXML_INT_CONST(LibxmlConst_NOTATION_NODE,       XML_NOTATION_NODE)` |
|     87 |  583 | `LIBXML_INT_CONST(LibxmlConst_HTML_DOCUMENT_NODE,  XML_HTML_DOCUMENT_NODE)` |
|     87 |  584 | `LIBXML_INT_CONST(LibxmlConst_DTD_NODE,            XML_DTD_NODE)` |
|     89 |  585 | `LIBXML_INT_CONST(LibxmlConst_ELEMENT_DECL_NODE,   XML_ELEMENT_DECL)` |
|     89 |  586 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_DECL_NODE, XML_ATTRIBUTE_DECL)` |
|     89 |  587 | `LIBXML_INT_CONST(LibxmlConst_ENTITY_DECL_NODE,    XML_ENTITY_DECL)` |
|      - |  588 | `/* php spells libxml's XML_NAMESPACE_DECL twice, under both DOM's name for a` |
|      - |  589 | ` * namespace node and libxml's own. */` |
|     89 |  590 | `LIBXML_INT_CONST(LibxmlConst_NAMESPACE_DECL_NODE, XML_NAMESPACE_DECL)` |
|     89 |  591 | `LIBXML_INT_CONST(LibxmlConst_LOCAL_NAMESPACE,     XML_NAMESPACE_DECL)` |
|      - |  592 | `/*` |
|      - |  593 | ` * The DTD attribute-TYPE enum. php's numbers are libxml's xmlAttributeType with` |
|      - |  594 | ` * one deliberate hole: php has no XML_ATTRIBUTE_ENTITIES and gives the name` |
|      - |  595 | ` * XML_ATTRIBUTE_ENTITY libxml's ENTITIES value (6), so the two disagree about` |
|      - |  596 | ` * what "entity" means by one.  php's numbering is the contract.` |
|      - |  597 | ` */` |
|     89 |  598 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_CDATA,       XML_ATTRIBUTE_CDATA)` |
|     89 |  599 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_ID,          XML_ATTRIBUTE_ID)` |
|     89 |  600 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_IDREF,       XML_ATTRIBUTE_IDREF)` |
|     89 |  601 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_IDREFS,      XML_ATTRIBUTE_IDREFS)` |
|     89 |  602 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_ENTITY,      XML_ATTRIBUTE_ENTITIES)` |
|     89 |  603 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_NMTOKEN,     XML_ATTRIBUTE_NMTOKEN)` |
|     89 |  604 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_NMTOKENS,    XML_ATTRIBUTE_NMTOKENS)` |
|     89 |  605 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_ENUMERATION, XML_ATTRIBUTE_ENUMERATION)` |
|     89 |  606 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_NOTATION,    XML_ATTRIBUTE_NOTATION)` |
|      - |  607 | `/*` |
|      - |  608 | ` * ext/dom's DOMException codes -- the DOM level-2 numbering an exception's` |
|      - |  609 | ` * getCode() answers, which is what a catch tests to tell one refusal from` |
|      - |  610 | ` * another (php's own zero, DOM_PHP_ERR, is the code for everything that is not` |
|      - |  611 | ` * a DOM error).` |
|      - |  612 | ` */` |
|     95 |  613 | `LIBXML_INT_CONST(LibxmlConst_PHP_ERR,                  0)` |
|    191 |  614 | `LIBXML_INT_CONST(LibxmlConst_INDEX_SIZE_ERR,           1)` |
|    191 |  615 | `LIBXML_INT_CONST(LibxmlConst_DOMSTRING_SIZE_ERR,       2)` |
|    191 |  616 | `LIBXML_INT_CONST(LibxmlConst_HIERARCHY_REQUEST_ERR,    3)` |
|    191 |  617 | `LIBXML_INT_CONST(LibxmlConst_WRONG_DOCUMENT_ERR,       4)` |
|    191 |  618 | `LIBXML_INT_CONST(LibxmlConst_INVALID_CHARACTER_ERR,    5)` |
|    191 |  619 | `LIBXML_INT_CONST(LibxmlConst_NO_DATA_ALLOWED_ERR,      6)` |
|    191 |  620 | `LIBXML_INT_CONST(LibxmlConst_NO_MODIFICATION_ALLOWED_ERR, 7)` |
|    193 |  621 | `LIBXML_INT_CONST(LibxmlConst_NOT_FOUND_ERR,            8)` |
|    191 |  622 | `LIBXML_INT_CONST(LibxmlConst_NOT_SUPPORTED_ERR,        9)` |
|    191 |  623 | `LIBXML_INT_CONST(LibxmlConst_INUSE_ATTRIBUTE_ERR,     10)` |
|    191 |  624 | `LIBXML_INT_CONST(LibxmlConst_INVALID_STATE_ERR,       11)` |
|    191 |  625 | `LIBXML_INT_CONST(LibxmlConst_SYNTAX_ERR,              12)` |
|    191 |  626 | `LIBXML_INT_CONST(LibxmlConst_INVALID_MODIFICATION_ERR,13)` |
|    191 |  627 | `LIBXML_INT_CONST(LibxmlConst_NAMESPACE_ERR,           14)` |
|     89 |  628 | `LIBXML_INT_CONST(LibxmlConst_INVALID_ACCESS_ERR,      15)` |
|    191 |  629 | `LIBXML_INT_CONST(LibxmlConst_VALIDATION_ERR,          16)` |
|      - |  630 | `/*` |
|      - |  631 | ` * php 8.4's Dom\ namespace re-exports the same numbering under short names --` |
|      - |  632 | ` * no PHP_ERR and no INVALID_ACCESS_ERR, and DOMSTRING_SIZE_ERR's 2 is spelt` |
|      - |  633 | ` * STRING_SIZE_ERR -- so every code below is the expand function already written` |
|      - |  634 | ` * above.  HTML_NO_DEFAULT_NS is the one name that is not a DOMException code:` |
|      - |  635 | ` * it is the html parser option that leaves the document element in no` |
|      - |  636 | ` * namespace, and it does not fit a signed 32-bit int.` |
|      - |  637 | ` */` |
|    111 |  638 | `LIBXML_INT_CONST(LibxmlConst_HTML_NO_DEFAULT_NS, 2147483648LL)` |
|      - |  639 |  |
|     80 |  640 | `static void LibxmlConst_DOTTED_VERSION(ph7_value *pVal,void *pUnused)` |
|      5 |  641 | `{` |
|     40 |  642 | `	SXUNUSED(pUnused);` |
|     85 |  643 | `	ph7_value_string(pVal,LIBXML_DOTTED_VERSION,-1);` |
|     85 |  644 | `}` |
|     80 |  645 | `static void LibxmlConst_LOADED_VERSION(ph7_value *pVal,void *pUnused)` |
|      5 |  646 | `{` |
|     40 |  647 | `	SXUNUSED(pUnused);` |
|      - |  648 | `	/* php exposes the runtime-loaded version string here */` |
|     85 |  649 | `	ph7_value_string(pVal,(const char *)xmlParserVersion,-1);` |
|     85 |  650 | `}` |
|      - |  651 |  |
|   6985 |  652 | `PH7_PRIVATE void PH7_RegisterLibxmlConstants(ph7_vm *pVm)` |
|      5 |  653 | `{` |
|      - |  654 | `	static const struct {` |
|      - |  655 | `		const char *zName;` |
|      - |  656 | `		void (*xExpand)(ph7_value *,void *);` |
|      - |  657 | `	} aConst[] = {` |
|      - |  658 | `		{ "LIBXML_VERSION",        LibxmlConst_VERSION        },` |
|      - |  659 | `		{ "LIBXML_DOTTED_VERSION", LibxmlConst_DOTTED_VERSION },` |
|      - |  660 | `		{ "LIBXML_LOADED_VERSION", LibxmlConst_LOADED_VERSION },` |
|      - |  661 | `		{ "LIBXML_RECOVER",        LibxmlConst_RECOVER        },` |
|      - |  662 | `		{ "LIBXML_HTML_NOIMPLIED", LibxmlConst_HTML_NOIMPLIED },` |
|      - |  663 | `		{ "LIBXML_HTML_NODEFDTD",  LibxmlConst_HTML_NODEFDTD  },` |
|      - |  664 | `		{ "LIBXML_NOENT",          LibxmlConst_NOENT          },` |
|      - |  665 | `		{ "LIBXML_DTDLOAD",        LibxmlConst_DTDLOAD        },` |
|      - |  666 | `		{ "LIBXML_DTDATTR",        LibxmlConst_DTDATTR        },` |
|      - |  667 | `		{ "LIBXML_DTDVALID",       LibxmlConst_DTDVALID       },` |
|      - |  668 | `		{ "LIBXML_NOERROR",        LibxmlConst_NOERROR        },` |
|      - |  669 | `		{ "LIBXML_NOWARNING",      LibxmlConst_NOWARNING      },` |
|      - |  670 | `		{ "LIBXML_NOBLANKS",       LibxmlConst_NOBLANKS       },` |
|      - |  671 | `		{ "LIBXML_XINCLUDE",       LibxmlConst_XINCLUDE       },` |
|      - |  672 | `		{ "LIBXML_NSCLEAN",        LibxmlConst_NSCLEAN        },` |
|      - |  673 | `		{ "LIBXML_NOCDATA",        LibxmlConst_NOCDATA        },` |
|      - |  674 | `		{ "LIBXML_NONET",          LibxmlConst_NONET          },` |
|      - |  675 | `		{ "LIBXML_PEDANTIC",       LibxmlConst_PEDANTIC       },` |
|      - |  676 | `		{ "LIBXML_COMPACT",        LibxmlConst_COMPACT        },` |
|      - |  677 | `		{ "LIBXML_PARSEHUGE",      LibxmlConst_PARSEHUGE      },` |
|      - |  678 | `		{ "LIBXML_BIGLINES",       LibxmlConst_BIGLINES       },` |
|      - |  679 | `		{ "LIBXML_NOXMLDECL",      LibxmlConst_NOXMLDECL      },` |
|      - |  680 | `		{ "LIBXML_NOEMPTYTAG",     LibxmlConst_NOEMPTYTAG     },` |
|      - |  681 | `		{ "LIBXML_SCHEMA_CREATE",  LibxmlConst_SCHEMA_CREATE  },` |
|      - |  682 | `		{ "LIBXML_ERR_NONE",       LibxmlConst_ERR_NONE       },` |
|      - |  683 | `		{ "LIBXML_ERR_WARNING",    LibxmlConst_ERR_WARNING    },` |
|      - |  684 | `		{ "LIBXML_ERR_ERROR",      LibxmlConst_ERR_ERROR      },` |
|      - |  685 | `		{ "LIBXML_ERR_FATAL",      LibxmlConst_ERR_FATAL      },` |
|      - |  686 | `		{ "XML_ELEMENT_NODE",       LibxmlConst_ELEMENT_NODE       },` |
|      - |  687 | `		{ "XML_ATTRIBUTE_NODE",     LibxmlConst_ATTRIBUTE_NODE     },` |
|      - |  688 | `		{ "XML_TEXT_NODE",          LibxmlConst_TEXT_NODE          },` |
|      - |  689 | `		{ "XML_CDATA_SECTION_NODE", LibxmlConst_CDATA_SECTION_NODE },` |
|      - |  690 | `		{ "XML_ENTITY_REF_NODE",    LibxmlConst_ENTITY_REF_NODE    },` |
|      - |  691 | `		{ "XML_ENTITY_NODE",        LibxmlConst_ENTITY_NODE        },` |
|      - |  692 | `		{ "XML_PI_NODE",            LibxmlConst_PI_NODE            },` |
|      - |  693 | `		{ "XML_COMMENT_NODE",       LibxmlConst_COMMENT_NODE       },` |
|      - |  694 | `		{ "XML_DOCUMENT_NODE",      LibxmlConst_DOCUMENT_NODE      },` |
|      - |  695 | `		{ "XML_DOCUMENT_TYPE_NODE", LibxmlConst_DOCUMENT_TYPE_NODE },` |
|      - |  696 | `		{ "XML_DOCUMENT_FRAG_NODE", LibxmlConst_DOCUMENT_FRAG_NODE },` |
|      - |  697 | `		{ "XML_NOTATION_NODE",      LibxmlConst_NOTATION_NODE      },` |
|      - |  698 | `		{ "XML_HTML_DOCUMENT_NODE", LibxmlConst_HTML_DOCUMENT_NODE },` |
|      - |  699 | `		{ "XML_DTD_NODE",           LibxmlConst_DTD_NODE           },` |
|      - |  700 | `		{ "XML_ELEMENT_DECL_NODE",  LibxmlConst_ELEMENT_DECL_NODE  },` |
|      - |  701 | `		{ "XML_ATTRIBUTE_DECL_NODE",LibxmlConst_ATTRIBUTE_DECL_NODE},` |
|      - |  702 | `		{ "XML_ENTITY_DECL_NODE",   LibxmlConst_ENTITY_DECL_NODE   },` |
|      - |  703 | `		{ "XML_NAMESPACE_DECL_NODE",LibxmlConst_NAMESPACE_DECL_NODE},` |
|      - |  704 | `		{ "XML_LOCAL_NAMESPACE",    LibxmlConst_LOCAL_NAMESPACE    },` |
|      - |  705 | `		{ "XML_ATTRIBUTE_CDATA",       LibxmlConst_ATTRIBUTE_CDATA       },` |
|      - |  706 | `		{ "XML_ATTRIBUTE_ID",          LibxmlConst_ATTRIBUTE_ID          },` |
|      - |  707 | `		{ "XML_ATTRIBUTE_IDREF",       LibxmlConst_ATTRIBUTE_IDREF       },` |
|      - |  708 | `		{ "XML_ATTRIBUTE_IDREFS",      LibxmlConst_ATTRIBUTE_IDREFS      },` |
|      - |  709 | `		{ "XML_ATTRIBUTE_ENTITY",      LibxmlConst_ATTRIBUTE_ENTITY      },` |
|      - |  710 | `		{ "XML_ATTRIBUTE_NMTOKEN",     LibxmlConst_ATTRIBUTE_NMTOKEN     },` |
|      - |  711 | `		{ "XML_ATTRIBUTE_NMTOKENS",    LibxmlConst_ATTRIBUTE_NMTOKENS    },` |
|      - |  712 | `		{ "XML_ATTRIBUTE_ENUMERATION", LibxmlConst_ATTRIBUTE_ENUMERATION },` |
|      - |  713 | `		{ "XML_ATTRIBUTE_NOTATION",    LibxmlConst_ATTRIBUTE_NOTATION    },` |
|      - |  714 | `		{ "DOM_PHP_ERR",                    LibxmlConst_PHP_ERR                    },` |
|      - |  715 | `		{ "DOM_INDEX_SIZE_ERR",             LibxmlConst_INDEX_SIZE_ERR             },` |
|      - |  716 | `		{ "DOMSTRING_SIZE_ERR",             LibxmlConst_DOMSTRING_SIZE_ERR         },` |
|      - |  717 | `		{ "DOM_HIERARCHY_REQUEST_ERR",      LibxmlConst_HIERARCHY_REQUEST_ERR      },` |
|      - |  718 | `		{ "DOM_WRONG_DOCUMENT_ERR",         LibxmlConst_WRONG_DOCUMENT_ERR         },` |
|      - |  719 | `		{ "DOM_INVALID_CHARACTER_ERR",      LibxmlConst_INVALID_CHARACTER_ERR      },` |
|      - |  720 | `		{ "DOM_NO_DATA_ALLOWED_ERR",        LibxmlConst_NO_DATA_ALLOWED_ERR        },` |
|      - |  721 | `		{ "DOM_NO_MODIFICATION_ALLOWED_ERR",LibxmlConst_NO_MODIFICATION_ALLOWED_ERR},` |
|      - |  722 | `		{ "DOM_NOT_FOUND_ERR",              LibxmlConst_NOT_FOUND_ERR              },` |
|      - |  723 | `		{ "DOM_NOT_SUPPORTED_ERR",          LibxmlConst_NOT_SUPPORTED_ERR          },` |
|      - |  724 | `		{ "DOM_INUSE_ATTRIBUTE_ERR",        LibxmlConst_INUSE_ATTRIBUTE_ERR        },` |
|      - |  725 | `		{ "DOM_INVALID_STATE_ERR",          LibxmlConst_INVALID_STATE_ERR          },` |
|      - |  726 | `		{ "DOM_SYNTAX_ERR",                 LibxmlConst_SYNTAX_ERR                 },` |
|      - |  727 | `		{ "DOM_INVALID_MODIFICATION_ERR",   LibxmlConst_INVALID_MODIFICATION_ERR   },` |
|      - |  728 | `		{ "DOM_NAMESPACE_ERR",              LibxmlConst_NAMESPACE_ERR              },` |
|      - |  729 | `		{ "DOM_INVALID_ACCESS_ERR",         LibxmlConst_INVALID_ACCESS_ERR         },` |
|      - |  730 | `		{ "DOM_VALIDATION_ERR",             LibxmlConst_VALIDATION_ERR             },` |
|      - |  731 | `		{ "Dom\\INDEX_SIZE_ERR",                  LibxmlConst_INDEX_SIZE_ERR                         },` |
|      - |  732 | `		{ "Dom\\STRING_SIZE_ERR",                 LibxmlConst_DOMSTRING_SIZE_ERR                     },` |
|      - |  733 | `		{ "Dom\\HIERARCHY_REQUEST_ERR",           LibxmlConst_HIERARCHY_REQUEST_ERR                  },` |
|      - |  734 | `		{ "Dom\\WRONG_DOCUMENT_ERR",              LibxmlConst_WRONG_DOCUMENT_ERR                     },` |
|      - |  735 | `		{ "Dom\\INVALID_CHARACTER_ERR",           LibxmlConst_INVALID_CHARACTER_ERR                  },` |
|      - |  736 | `		{ "Dom\\NO_DATA_ALLOWED_ERR",             LibxmlConst_NO_DATA_ALLOWED_ERR                    },` |
|      - |  737 | `		{ "Dom\\NO_MODIFICATION_ALLOWED_ERR",     LibxmlConst_NO_MODIFICATION_ALLOWED_ERR            },` |
|      - |  738 | `		{ "Dom\\NOT_FOUND_ERR",                   LibxmlConst_NOT_FOUND_ERR                          },` |
|      - |  739 | `		{ "Dom\\NOT_SUPPORTED_ERR",               LibxmlConst_NOT_SUPPORTED_ERR                      },` |
|      - |  740 | `		{ "Dom\\INUSE_ATTRIBUTE_ERR",             LibxmlConst_INUSE_ATTRIBUTE_ERR                    },` |
|      - |  741 | `		{ "Dom\\INVALID_STATE_ERR",               LibxmlConst_INVALID_STATE_ERR                      },` |
|      - |  742 | `		{ "Dom\\SYNTAX_ERR",                      LibxmlConst_SYNTAX_ERR                             },` |
|      - |  743 | `		{ "Dom\\INVALID_MODIFICATION_ERR",        LibxmlConst_INVALID_MODIFICATION_ERR               },` |
|      - |  744 | `		{ "Dom\\NAMESPACE_ERR",                   LibxmlConst_NAMESPACE_ERR                          },` |
|      - |  745 | `		{ "Dom\\VALIDATION_ERR",                  LibxmlConst_VALIDATION_ERR                         },` |
|      - |  746 | `		{ "Dom\\HTML_NO_DEFAULT_NS",              LibxmlConst_HTML_NO_DEFAULT_NS                     },` |
|      - |  747 | `	};` |
|      - |  748 | `	sxu32 n;` |
| 628655 |  749 | `	for( n = 0 ; n < sizeof(aConst)/sizeof(aConst[0]) ; n++ ){` |
| 621670 |  750 | `		ph7_create_constant(&(*pVm),aConst[n].zName,aConst[n].xExpand,0);` |
| 310348 |  751 | `	}` |
|   6990 |  752 | `}` |
|      - |  753 | `/* ===== libxml_* native thunks ===== */` |
|      - |  754 |  |
|      - |  755 | `/*` |
|      - |  756 | ` * bool __libxml_use_internal_errors(?bool $use_errors = null)` |
|      - |  757 | ` *  Flip (or just report, on null/omitted) the internal-capture flag and` |
|      - |  758 | ` *  return the PREVIOUS state -- php semantics.` |
|      - |  759 | ` */` |
|    256 |  760 | `static int vm_builtin_libxml_use_internal_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  761 | `{` |
|    258 |  762 | `	ph7_vm *pVm = pCtx->pVm;` |
|    258 |  763 | `	int bPrev = pVm->bLibxmlInternalErr;` |
|    258 |  764 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|    252 |  765 | `		pVm->bLibxmlInternalErr = ph7_value_to_bool(apArg[0]) ? 1 : 0;` |
|    252 |  766 | `		if( !pVm->bLibxmlInternalErr ){` |
|      - |  767 | `			/* php frees the accumulated buffer when capture turns OFF */` |
|    142 |  768 | `			LibxmlClearQueue(pVm);` |
|     70 |  769 | `		}` |
|    125 |  770 | `	}` |
|    258 |  771 | `	ph7_result_bool(pCtx,bPrev);` |
|    258 |  772 | `	return PH7_OK;` |
|      2 |  773 | `}` |
|      - |  774 | `/*` |
|      - |  775 | ` * array __libxml_get_errors_raw()` |
|      - |  776 | ` *  The queued errors as raw assoc arrays, oldest first.` |
|      - |  777 | ` */` |
|      - |  778 | `/*` |
|      - |  779 | ` * Build one LibXMLError from a recorded error.` |
|      - |  780 | ` *` |
|      - |  781 | `` * The prelude did this in PHP (`__phl_libxml_err_obj()` copying six array keys`` |
|      - |  782 | `` * onto a `new LibXMLError`), over an array these accessors returned purely so`` |
|      - |  783 | ` * that PHP could reshape it. Both the array hop and the helper are gone: the` |
|      - |  784 | ` * accessors below now answer the objects php answers.` |
|      - |  785 | ` */` |
|     75 |  786 | `static ph7_class_instance * LibxmlErrObject(ph7_context *pCtx,phl_libxml_err *pErr)` |
|      2 |  787 | `{` |
|     77 |  788 | `	ph7_vm *pVm = pCtx->pVm;` |
|     77 |  789 | `	ph7_class *pClass = PH7_VmExtractClass(pVm,"LibXMLError",sizeof("LibXMLError")-1,0,0);` |
|      - |  790 | `	ph7_class_instance *pObj;` |
|      - |  791 | `	ph7_value sVal;` |
|      - |  792 | `	SyString sStr;` |
|     77 |  793 | `	if( pClass == 0 ){` |
|    ! 0 |  794 | `		return 0;` |
|      - |  795 | `	}` |
|     77 |  796 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|     77 |  797 | `	if( pObj == 0 ){` |
|    ! 0 |  798 | `		return 0;` |
|      - |  799 | `	}` |
|     77 |  800 | `	PH7_MemObjInit(pVm,&sVal);` |
|     77 |  801 | `	PH7_MemObjInitFromInt(pVm,&sVal,pErr->iLevel);` |
|     77 |  802 | `	PH7_NativeSetProp(pVm,pObj,"level",sizeof("level")-1,&sVal);` |
|     77 |  803 | `	PH7_MemObjInitFromInt(pVm,&sVal,pErr->iCode);` |
|     77 |  804 | `	PH7_NativeSetProp(pVm,pObj,"code",sizeof("code")-1,&sVal);` |
|     77 |  805 | `	PH7_MemObjInitFromInt(pVm,&sVal,pErr->iColumn);` |
|     77 |  806 | `	PH7_NativeSetProp(pVm,pObj,"column",sizeof("column")-1,&sVal);` |
|     77 |  807 | `	PH7_MemObjInitFromInt(pVm,&sVal,pErr->iLine);` |
|     77 |  808 | `	PH7_NativeSetProp(pVm,pObj,"line",sizeof("line")-1,&sVal);` |
|     77 |  809 | `	SyStringInitFromBuf(&sStr,pErr->sMsg.zString ? pErr->sMsg.zString : "",pErr->sMsg.nByte);` |
|     77 |  810 | `	PH7_MemObjInitFromString(pVm,&sVal,&sStr);` |
|     77 |  811 | `	PH7_NativeSetProp(pVm,pObj,"message",sizeof("message")-1,&sVal);` |
|     77 |  812 | `	SyStringInitFromBuf(&sStr,pErr->sFile.zString ? pErr->sFile.zString : "",pErr->sFile.nByte);` |
|     77 |  813 | `	PH7_MemObjInitFromString(pVm,&sVal,&sStr);` |
|     77 |  814 | `	PH7_NativeSetProp(pVm,pObj,"file",sizeof("file")-1,&sVal);` |
|     77 |  815 | `	PH7_MemObjRelease(&sVal);` |
|     77 |  816 | `	return pObj;` |
|     39 |  817 | `}` |
|      - |  818 | `/* array libxml_get_errors() */` |
|    100 |  819 | `static int vm_builtin_libxml_get_errors_raw(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  820 | `{` |
|    102 |  821 | `	ph7_vm *pVm = pCtx->pVm;` |
|    102 |  822 | `	phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|    102 |  823 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|    102 |  824 | `	ph7_value *pWorker = ph7_context_new_scalar(pCtx);` |
|      - |  825 | `	sxu32 n;` |
|     50 |  826 | `	SXUNUSED(nArg);` |
|     50 |  827 | `	SXUNUSED(apArg);` |
|    102 |  828 | `	if( pList == 0 \|\| pWorker == 0 ){` |
|    ! 0 |  829 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  830 | `		ph7_result_null(pCtx);` |
|    ! 0 |  831 | `		return PH7_OK;` |
|      - |  832 | `	}` |
|    169 |  833 | `	for( n = 0 ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|     69 |  834 | `		ph7_class_instance *pObj = LibxmlErrObject(pCtx,&aErr[n]);` |
|      - |  835 | `		ph7_value sEntry;` |
|     69 |  836 | `		if( pObj == 0 ){` |
|    ! 0 |  837 | `			break;` |
|      - |  838 | `		}` |
|      - |  839 | `		/* The list takes over the instance's initial iRef=1: ph7_array_add_elem` |
|      - |  840 | `		 * copies the slot (taking its own reference), so the local one is dropped. */` |
|     69 |  841 | `		PH7_MemObjInit(pVm,&sEntry);` |
|     69 |  842 | `		sEntry.x.pOther = pObj;` |
|     69 |  843 | `		sEntry.iFlags = MEMOBJ_OBJ;` |
|     69 |  844 | `		ph7_array_add_elem(pList,0,&sEntry);` |
|     69 |  845 | `		PH7_ClassInstanceUnref(pObj);` |
|     35 |  846 | `	}` |
|    102 |  847 | `	ph7_result_value(pCtx,pList);` |
|    102 |  848 | `	return PH7_OK;` |
|     52 |  849 | `}` |
|      - |  850 | `/*` |
|      - |  851 | ` * void __libxml_clear_errors()` |
|      - |  852 | ` */` |
|    220 |  853 | `static int vm_builtin_libxml_clear_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  854 | `{` |
|    110 |  855 | `	SXUNUSED(nArg);` |
|    110 |  856 | `	SXUNUSED(apArg);` |
|    222 |  857 | `	PH7_LibxmlClearErrors(pCtx->pVm);` |
|    222 |  858 | `	ph7_result_null(pCtx);` |
|    222 |  859 | `	return PH7_OK;` |
|      2 |  860 | `}` |
|      - |  861 | `/*` |
|      - |  862 | ` * array\|false __libxml_get_last_error_raw()` |
|      - |  863 | ` */` |
|     12 |  864 | `static int vm_builtin_libxml_get_last_error_raw(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  865 | `{` |
|     14 |  866 | `	ph7_vm *pVm = pCtx->pVm;` |
|     14 |  867 | `	phl_libxml_err *pLast = (phl_libxml_err *)pVm->pLibxmlLastErr;` |
|      6 |  868 | `	SXUNUSED(nArg);` |
|      6 |  869 | `	SXUNUSED(apArg);` |
|     14 |  870 | `	if( pLast == 0 ){` |
|      6 |  871 | `		ph7_result_bool(pCtx,0);` |
|      6 |  872 | `		return PH7_OK;` |
|      - |  873 | `	}` |
|      - |  874 | `	{` |
|     10 |  875 | `		ph7_class_instance *pObj = LibxmlErrObject(pCtx,pLast);` |
|      - |  876 | `		ph7_value sRes;` |
|     10 |  877 | `		if( pObj == 0 ){` |
|    ! 0 |  878 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  879 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  880 | `			return PH7_OK;` |
|      - |  881 | `		}` |
|     10 |  882 | `		PH7_MemObjInit(pVm,&sRes);` |
|     10 |  883 | `		sRes.x.pOther = pObj;` |
|     10 |  884 | `		sRes.iFlags = MEMOBJ_OBJ;` |
|     10 |  885 | `		ph7_result_value(pCtx,&sRes);   /* takes its own reference */` |
|     10 |  886 | `		PH7_ClassInstanceUnref(pObj);` |
|      - |  887 | `	}` |
|     10 |  888 | `	return PH7_OK;` |
|      8 |  889 | `}` |
|      - |  890 |  |
|      - |  891 | `/*` |
|      - |  892 | ` * ?callable libxml_get_external_entity_loader()` |
|      - |  893 | ` *  The stored resolver VERBATIM (a callable string answers as that string),` |
|      - |  894 | ` *  or null for the default loader -- php's answer shape.` |
|      - |  895 | ` */` |
|      8 |  896 | `static int vm_builtin_libxml_get_external_entity_loader(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  897 | `{` |
|      9 |  898 | `	ph7_vm *pVm = pCtx->pVm;` |
|      4 |  899 | `	SXUNUSED(nArg);` |
|      4 |  900 | `	SXUNUSED(apArg);` |
|      9 |  901 | `	if( (pVm->sXmlEntLoader.iFlags & MEMOBJ_NULL) == 0 ){` |
|      5 |  902 | `		ph7_result_value(pCtx,&pVm->sXmlEntLoader); /* makes its own copy */` |
|      3 |  903 | `	}else{` |
|      5 |  904 | `		ph7_result_null(pCtx);` |
|      - |  905 | `	}` |
|      9 |  906 | `	return PH7_OK;` |
|      1 |  907 | `}` |
|      - |  908 | `/*` |
|      - |  909 | ` * true libxml_set_external_entity_loader(?callable $resolver_function)` |
|      - |  910 | ` *  Store the resolver (null restores the default). The slot is never` |
|      - |  911 | ` *  INVOKED here -- no PHL parse path loads an external entity, the same` |
|      - |  912 | ` *  off-by-default php's sanitized parser options enforce -- so the` |
|      - |  913 | ` *  round-trip contract is the whole observable surface.` |
|      - |  914 | ` */` |
|     10 |  915 | `static int vm_builtin_libxml_set_external_entity_loader(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  916 | `{` |
|     11 |  917 | `	ph7_vm *pVm = pCtx->pVm;` |
|     11 |  918 | `	if( nArg < 1 ){` |
|    ! 0 |  919 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  920 | `		return PH7_OK;` |
|      - |  921 | `	}` |
|     11 |  922 | `	if( !ph7_value_is_null(apArg[0]) ){` |
|      - |  923 | `		/* php screens through the FCC machinery: a string must resolve as a` |
|      - |  924 | `		 * CALLABLE, and the refusal names the callback rule. */` |
|      9 |  925 | `		sxi32 rc = PH7_CheckCallbackArg(pCtx,apArg[0],1,"resolver_function",1);` |
|      9 |  926 | `		if( rc != PH7_OK ){` |
|      5 |  927 | `			return rc;` |
|      - |  928 | `		}` |
|      2 |  929 | `	}` |
|      7 |  930 | `	PH7_MemObjRelease(&pVm->sXmlEntLoader);` |
|      7 |  931 | `	if( !ph7_value_is_null(apArg[0]) ){` |
|      5 |  932 | `		PH7_MemObjStore(apArg[0],&pVm->sXmlEntLoader);` |
|      2 |  933 | `	}` |
|      7 |  934 | `	ph7_result_bool(pCtx,1);` |
|      7 |  935 | `	return PH7_OK;` |
|      6 |  936 | `}` |
|      - |  937 | `/*` |
|      - |  938 | ` * void libxml_set_streams_context($context)` |
|      - |  939 | ` *  Take a stream-context RESOURCE and keep it for the document loaders.` |
|      - |  940 | ` *  php validates lazily at the next load; PHL has no loader that would` |
|      - |  941 | ` *  ever read it (the consumer is the http:// wrapper), so` |
|      - |  942 | ` *  the check runs here, with php's own two messages.` |
|      - |  943 | ` */` |
|      8 |  944 | `static int vm_builtin_libxml_set_streams_context(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  945 | `{` |
|      9 |  946 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  947 | `	const io_private *pDev;` |
|      9 |  948 | `	if( nArg < 1 ){` |
|    ! 0 |  949 | `		return PH7_OK;` |
|      - |  950 | `	}` |
|      9 |  951 | `	if( (apArg[0]->iFlags & MEMOBJ_RES) == 0 ){` |
|      3 |  952 | `		const char *zType = "null";` |
|      3 |  953 | `		if( apArg[0]->iFlags & MEMOBJ_HASHMAP ){ zType = "array"; }` |
|      3 |  954 | `		else if( apArg[0]->iFlags & MEMOBJ_OBJ ){ zType = "object"; }` |
|      3 |  955 | `		else if( apArg[0]->iFlags & MEMOBJ_STRING ){ zType = "string"; }` |
|      3 |  956 | `		else if( apArg[0]->iFlags & MEMOBJ_BOOL ){ zType = "bool"; }` |
|      3 |  957 | `		else if( apArg[0]->iFlags & MEMOBJ_REAL ){ zType = "float"; }` |
|      3 |  958 | `		else if( apArg[0]->iFlags & MEMOBJ_INT ){ zType = "int"; }` |
|      4 |  959 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  960 | `			"libxml_set_streams_context(): Argument #1 ($context) must be of type resource, %s given",` |
|      1 |  961 | `			zType);` |
|      - |  962 | `	}` |
|      7 |  963 | `	pDev = (const io_private *)apArg[0]->x.pOther;` |
|      7 |  964 | `	if( pDev == 0 \|\| pDev->iMagic != STREAM_CTX_MAGIC ){` |
|      3 |  965 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  966 | `			"libxml_set_streams_context(): supplied resource is not a valid Stream-Context resource");` |
|      - |  967 | `	}` |
|      5 |  968 | `	PH7_MemObjRelease(&pVm->sXmlStreamsCtx);` |
|      5 |  969 | `	PH7_MemObjStore(apArg[0],&pVm->sXmlStreamsCtx);` |
|      5 |  970 | `	ph7_result_null(pCtx);` |
|      5 |  971 | `	return PH7_OK;` |
|      5 |  972 | `}` |
|      - |  973 |  |
|      - |  974 | `/*` |
|      - |  975 | ` * The libxml PHP-visible surface: the LibXMLError class plus the seven` |
|      - |  976 | ` * libxml_* functions (php's eighth, libxml_disable_entity_loader(), is` |
|      - |  977 | ` * E_DEPRECATED since 8.0 and stays removed under the scope policy --` |
|      - |  978 | ` * twin-paired in 002-integration/function/libxml/).` |
|      - |  979 | ` */` |
|      - |  980 | `/* LibXMLError is declared from C below, and the four libxml_* functions ARE the` |
|      - |  981 | ` * C routines -- they used to be PHP wrappers over __libxml_* thunks, with a PHP` |
|      - |  982 | ` * helper reshaping an array into the object. */` |
|      - |  983 |  |
|      - |  984 | `/*` |
|      - |  985 | ` * Install the shared libxml layer: one-time global init, the per-VM state` |
|      - |  986 | ` * the DOM/XMLWriter surfaces build on, the __libxml_* thunks and the` |
|      - |  987 | ` * PHP-visible libxml_* functions + LibXMLError class.  Called from` |
|      - |  988 | ` * PH7_VmInit inside the bCompilingBuiltin window.` |
|      - |  989 | ` */` |
|   8445 |  990 | `PH7_PRIVATE sxi32 PH7_VmInstallLibxml(ph7_vm *pVm)` |
|      5 |  991 | `{` |
|      - |  992 | `	/* Registered under the names php exposes. There is no thunk and no PHP` |
|      - |  993 | `	 * wrapper any more: these are the functions, so they are internal for` |
|      - |  994 | `	 * reflection and get the arity enforcement a prelude wrapper never had. */` |
|      - |  995 | `	static const struct {` |
|      - |  996 | `		const char *zName;` |
|      - |  997 | `		ProchHostFunction xFunc;` |
|      - |  998 | `	} aFunc[] = {` |
|      - |  999 | `		{ "libxml_use_internal_errors", vm_builtin_libxml_use_internal_errors },` |
|      - | 1000 | `		{ "libxml_get_errors",          vm_builtin_libxml_get_errors_raw      },` |
|      - | 1001 | `		{ "libxml_clear_errors",        vm_builtin_libxml_clear_errors        },` |
|      - | 1002 | `		{ "libxml_get_last_error",      vm_builtin_libxml_get_last_error_raw  },` |
|      - | 1003 | `		{ "libxml_get_external_entity_loader", vm_builtin_libxml_get_external_entity_loader },` |
|      - | 1004 | `		{ "libxml_set_external_entity_loader", vm_builtin_libxml_set_external_entity_loader },` |
|      - | 1005 | `		{ "libxml_set_streams_context", vm_builtin_libxml_set_streams_context },` |
|      - | 1006 | `	};` |
|      - | 1007 | `	/* Plain data carrier; php declares no methods on it. */` |
|      - | 1008 | `	static const PH7_NativePropDef aProp[] = {` |
|      - | 1009 | `		{ "level",   PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|      - | 1010 | `		{ "code",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|      - | 1011 | `		{ "column",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|      - | 1012 | `		{ "message", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 1013 | `		{ "file",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 1014 | `		{ "line",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|      - | 1015 | `	};` |
|      - | 1016 | `	static const PH7_NativeClassSpec sSpec = {` |
|      - | 1017 | `		"LibXMLError", 0, 0, 0, 0, 0, 0, 0, aProp, SX_ARRAYSIZE(aProp), 0, 0, 0` |
|      - | 1018 | `	};` |
|      - | 1019 | `	sxu32 n;` |
|   8450 | 1020 | `	LibxmlGlobalInit();` |
|   8450 | 1021 | `	SySetInit(&pVm->aLibxmlErr,&pVm->sAllocator,sizeof(phl_libxml_err));` |
|   8450 | 1022 | `	SyBlobInit(&pVm->sLibxmlPend,&pVm->sAllocator);` |
|   8450 | 1023 | `	pVm->bLibxmlInternalErr = 0;` |
|   8450 | 1024 | `	pVm->pLibxmlLastErr = 0;` |
|   8450 | 1025 | `	pVm->pXmlDocs = 0;` |
|   8450 | 1026 | `	pVm->pXmlWriters = 0;` |
|   8450 | 1027 | `	PH7_MemObjInit(pVm,&pVm->sXmlEntLoader);` |
|   8450 | 1028 | `	PH7_MemObjInit(pVm,&pVm->sXmlStreamsCtx);` |
|  67565 | 1029 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
|  59120 | 1030 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|  29524 | 1031 | `	}` |
|      - | 1032 | `	/* The class must exist before the accessors can build one. */` |
|   8450 | 1033 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|      5 | 1034 | `}` |
|      - | 1035 |  |
|      - | 1036 | `#else` |
|      - | 1037 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|      - | 1038 | `typedef int vm_libxml_unused;` |
|      - | 1039 | `#endif /* PH7_ENABLE_LIBXML */` |
|      - | 1040 |  |
