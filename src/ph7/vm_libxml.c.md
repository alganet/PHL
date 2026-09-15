# src/ph7/vm_libxml.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 238/324 lines (73.46%)

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
|   3894 |   38 | `static void LibxmlGlobalInit(void)` |
|      5 |   39 | `{` |
|      - |   40 | `	static int bInit = 0;` |
|   3899 |   41 | `	if( !bInit ){` |
|   3899 |   42 | `		xmlInitParser();` |
|   3899 |   43 | `		bInit = 1;` |
|   1947 |   44 | `	}` |
|   3899 |   45 | `}` |
|      - |   46 | `/*` |
|      - |   47 | ` * Free one registered document: its orphaned subtrees first, then the tree` |
|      - |   48 | ` * itself.  Registry links and the phl_xmldoc shell live in SyMemBackend and` |
|      - |   49 | ` * are reclaimed with the VM allocator.` |
|      - |   50 | ` */` |
|     92 |   51 | `static void LibxmlFreeDoc(phl_xmldoc *pDoc)` |
|      1 |   52 | `{` |
|     93 |   53 | `	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pDoc->aOrphans);` |
|      - |   54 | `	sxu32 n;` |
|    103 |   55 | `	for( n = 0 ; n < SySetUsed(&pDoc->aOrphans) ; ++n ){` |
|     11 |   56 | `		xmlUnlinkNode(apOrphan[n]);` |
|     11 |   57 | `		xmlFreeNode(apOrphan[n]);` |
|      6 |   58 | `	}` |
|     93 |   59 | `	SySetRelease(&pDoc->aOrphans);` |
|     93 |   60 | `	if( pDoc->pDoc ){` |
|     93 |   61 | `		xmlFreeDoc((xmlDocPtr)pDoc->pDoc);` |
|     93 |   62 | `		pDoc->pDoc = 0;` |
|     46 |   63 | `	}` |
|     93 |   64 | `}` |
|      - |   65 | `/*` |
|      - |   66 | ` * Reset the per-VM libxml state between executions: drop the accumulated` |
|      - |   67 | ` * error queue and free every document from the previous request.  Called` |
|      - |   68 | ` * from PH7_VmMakeReady() (which runs once per exec, including on VM reuse` |
|      - |   69 | ` * by the -S server) alongside the other per-exec field resets.` |
|      - |   70 | ` */` |
|   3432 |   71 | `PH7_PRIVATE void PH7_LibxmlVmReset(ph7_vm *pVm)` |
|      5 |   72 | `{` |
|      - |   73 | `	phl_xmldoc *pDoc,*pNext;` |
|   3437 |   74 | `	PH7_LibxmlClearErrors(pVm);` |
|   3437 |   75 | `	pVm->bLibxmlInternalErr = 0;` |
|   3437 |   76 | `	pDoc = (phl_xmldoc *)pVm->pXmlDocs;` |
|   3529 |   77 | `	while( pDoc ){` |
|     93 |   78 | `		pNext = pDoc->pNext;` |
|     93 |   79 | `		LibxmlFreeDoc(pDoc);` |
|     93 |   80 | `		SyMemBackendFree(&pVm->sAllocator,pDoc);` |
|     93 |   81 | `		pDoc = pNext;` |
|      1 |   82 | `	}` |
|   3437 |   83 | `	pVm->pXmlDocs = 0;` |
|      - |   84 | `	/* XMLWriter buffers live outside SyMemBackend too (see vm_xmlwriter.c) */` |
|   3437 |   85 | `	PH7_XmlWriterVmSweep(pVm);` |
|   3437 |   86 | `}` |
|      - |   87 | `/*` |
|      - |   88 | ` * Final teardown on VM release.  Must run before SyMemBackendRelease()` |
|      - |   89 | ` * wipes the allocator that holds the registry shells.` |
|      - |   90 | ` */` |
|   3424 |   91 | `PH7_PRIVATE void PH7_LibxmlVmRelease(ph7_vm *pVm)` |
|      5 |   92 | `{` |
|   3429 |   93 | `	PH7_LibxmlVmReset(pVm);` |
|   3429 |   94 | `	SySetRelease(&pVm->aLibxmlErr);` |
|   3429 |   95 | `}` |
|      - |   96 | `/*` |
|      - |   97 | ` * Release the copied message/file strings of one queue entry.` |
|      - |   98 | ` */` |
|     28 |   99 | `static void LibxmlFreeErr(ph7_vm *pVm,phl_libxml_err *pErr)` |
|      1 |  100 | `{` |
|     29 |  101 | `	if( pErr->sMsg.zString ){` |
|     29 |  102 | `		SyMemBackendFree(&pVm->sAllocator,(void *)pErr->sMsg.zString);` |
|     14 |  103 | `	}` |
|     29 |  104 | `	if( pErr->sFile.zString ){` |
|    ! 0 |  105 | `		SyMemBackendFree(&pVm->sAllocator,(void *)pErr->sFile.zString);` |
|    ! 0 |  106 | `	}` |
|     29 |  107 | `	SyStringInitFromBuf(&pErr->sMsg,0,0);` |
|     29 |  108 | `	SyStringInitFromBuf(&pErr->sFile,0,0);` |
|     29 |  109 | `}` |
|      - |  110 | `/*` |
|      - |  111 | ` * Empty the libxml error queue, releasing the copied strings.  The` |
|      - |  112 | ` * last-error slot is kept (php parity: use_internal_errors(false) drops` |
|      - |  113 | ` * the buffer but libxml_get_last_error still reports).` |
|      - |  114 | ` */` |
|   3482 |  115 | `static void LibxmlClearQueue(ph7_vm *pVm)` |
|      5 |  116 | `{` |
|   3487 |  117 | `	phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|      - |  118 | `	sxu32 n;` |
|   3501 |  119 | `	for( n = 0 ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|     15 |  120 | `		LibxmlFreeErr(pVm,&aErr[n]);` |
|      8 |  121 | `	}` |
|   3487 |  122 | `	SySetReset(&pVm->aLibxmlErr);` |
|   3487 |  123 | `}` |
|      - |  124 | `/*` |
|      - |  125 | ` * libxml_clear_errors(): drop the queue AND the last-error slot.` |
|      - |  126 | ` */` |
|   3456 |  127 | `PH7_PRIVATE void PH7_LibxmlClearErrors(ph7_vm *pVm)` |
|      5 |  128 | `{` |
|   3461 |  129 | `	LibxmlClearQueue(pVm);` |
|   3461 |  130 | `	if( pVm->pLibxmlLastErr ){` |
|     11 |  131 | `		LibxmlFreeErr(pVm,(phl_libxml_err *)pVm->pLibxmlLastErr);` |
|     11 |  132 | `		SyMemBackendFree(&pVm->sAllocator,pVm->pLibxmlLastErr);` |
|     11 |  133 | `		pVm->pLibxmlLastErr = 0;` |
|      5 |  134 | `	}` |
|   3461 |  135 | `}` |
|      - |  136 | `/*` |
|      - |  137 | ` * Push one error onto the per-VM queue and last-error slot, copying the` |
|      - |  138 | ` * message/file strings.  The typed structured-error callback below and the` |
|      - |  139 | ` * DOM schema hooks (vm_dom.c) both funnel through this, keeping the` |
|      - |  140 | ` * queue-building logic in one place and ph7int.h free of libxml types.` |
|      - |  141 | ` */` |
|     14 |  142 | `PH7_PRIVATE void PH7_LibxmlQueueError(ph7_vm *pVm,int iLevel,int iCode,int iLine,int iColumn,` |
|      - |  143 | `	const char *zMsg,const char *zFile)` |
|      1 |  144 | `{` |
|      - |  145 | `	phl_libxml_err sEntry;` |
|      - |  146 | `	phl_libxml_err *pLast;` |
|     15 |  147 | `	if( pVm == 0 ){` |
|    ! 0 |  148 | `		return;` |
|      - |  149 | `	}` |
|     15 |  150 | `	SyZero(&sEntry,sizeof(sEntry));` |
|     15 |  151 | `	sEntry.iLevel = iLevel;` |
|     15 |  152 | `	sEntry.iCode = iCode;` |
|     15 |  153 | `	sEntry.iLine = iLine;` |
|     15 |  154 | `	sEntry.iColumn = iColumn;` |
|     15 |  155 | `	if( zMsg ){` |
|     15 |  156 | `		sxu32 nMsg = SyStrlen(zMsg);` |
|     15 |  157 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,zMsg,nMsg);` |
|     15 |  158 | `		if( zDup ){` |
|      - |  159 | `			/* Trailing newline kept: php's LibXMLError->message preserves it */` |
|     15 |  160 | `			SyStringInitFromBuf(&sEntry.sMsg,zDup,nMsg);` |
|      7 |  161 | `		}` |
|      7 |  162 | `	}` |
|     15 |  163 | `	if( zFile ){` |
|    ! 0 |  164 | `		sxu32 nFile = SyStrlen(zFile);` |
|    ! 0 |  165 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,zFile,nFile);` |
|    ! 0 |  166 | `		if( zDup ){` |
|    ! 0 |  167 | `			SyStringInitFromBuf(&sEntry.sFile,zDup,nFile);` |
|    ! 0 |  168 | `		}` |
|    ! 0 |  169 | `	}` |
|     15 |  170 | `	SySetPut(&pVm->aLibxmlErr,(const void *)&sEntry);` |
|      - |  171 | `	/* Mirror into the last-error slot (independent string copies so queue` |
|      - |  172 | `	 * draining cannot invalidate it). */` |
|     15 |  173 | `	pLast = (phl_libxml_err *)pVm->pLibxmlLastErr;` |
|     15 |  174 | `	if( pLast == 0 ){` |
|     11 |  175 | `		pLast = (phl_libxml_err *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_libxml_err));` |
|     11 |  176 | `		if( pLast == 0 ){` |
|    ! 0 |  177 | `			return;` |
|      - |  178 | `		}` |
|     11 |  179 | `		SyZero(pLast,sizeof(phl_libxml_err));` |
|     11 |  180 | `		pVm->pLibxmlLastErr = (void *)pLast;` |
|      6 |  181 | `	}else{` |
|      5 |  182 | `		LibxmlFreeErr(pVm,pLast);` |
|      - |  183 | `	}` |
|     15 |  184 | `	pLast->iLevel = iLevel;` |
|     15 |  185 | `	pLast->iCode = iCode;` |
|     15 |  186 | `	pLast->iLine = iLine;` |
|     15 |  187 | `	pLast->iColumn = iColumn;` |
|     15 |  188 | `	if( sEntry.sMsg.zString ){` |
|     15 |  189 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,sEntry.sMsg.zString,sEntry.sMsg.nByte);` |
|     15 |  190 | `		if( zDup ){` |
|     15 |  191 | `			SyStringInitFromBuf(&pLast->sMsg,zDup,sEntry.sMsg.nByte);` |
|      7 |  192 | `		}` |
|      7 |  193 | `	}` |
|     15 |  194 | `	if( sEntry.sFile.zString ){` |
|    ! 0 |  195 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,sEntry.sFile.zString,sEntry.sFile.nByte);` |
|    ! 0 |  196 | `		if( zDup ){` |
|    ! 0 |  197 | `			SyStringInitFromBuf(&pLast->sFile,zDup,sEntry.sFile.nByte);` |
|    ! 0 |  198 | `		}` |
|    ! 0 |  199 | `	}` |
|      8 |  200 | `}` |
|      - |  201 | `/*` |
|      - |  202 | ` * Structured-error callback installed while a libxml2 entry point runs` |
|      - |  203 | ` * between PH7_LibxmlCaptureBegin/End.  Forwards to PH7_LibxmlQueueError;` |
|      - |  204 | ` * the capture-end decides whether entries stay queued or drain as` |
|      - |  205 | ` * php-style warnings.` |
|      - |  206 | ` */` |
|      - |  207 | `#if LIBXML_VERSION >= 21200` |
|      5 |  208 | `static void LibxmlStructuredErr(void *pUserData,const xmlError *pErr)` |
|      - |  209 | `#else` |
|      5 |  210 | `static void LibxmlStructuredErr(void *pUserData,xmlErrorPtr pErr)` |
|      - |  211 | `#endif` |
|      1 |  212 | `{` |
|     11 |  213 | `	if( pErr == 0 ){` |
|    ! 0 |  214 | `		return;` |
|      - |  215 | `	}` |
|      - |  216 | `	/* libxml keeps the column in int2 */` |
|     16 |  217 | `	PH7_LibxmlQueueError((ph7_vm *)pUserData,(int)pErr->level,pErr->code,pErr->line,` |
|     10 |  218 | `		pErr->int2,pErr->message,pErr->file);` |
|      6 |  219 | `}` |
|      - |  220 | `/*` |
|      - |  221 | ` * Bracket a libxml2 entry point.  Begin installs the structured handler` |
|      - |  222 | ` * routed at this VM and returns the current queue depth; End restores the` |
|      - |  223 | ` * default handler and, when libxml_use_internal_errors() is OFF, drains` |
|      - |  224 | ` * every entry recorded since the mark as php-style warnings:` |
|      - |  225 | ` *   funcname(): <message> in <Entity\|file>, line: <n>` |
|      - |  226 | ` * (php's exact wording for the memory-parser case).` |
|      - |  227 | ` */` |
|    138 |  228 | `PH7_PRIVATE sxu32 PH7_LibxmlCaptureBegin(ph7_vm *pVm)` |
|      1 |  229 | `{` |
|    139 |  230 | `	xmlSetStructuredErrorFunc(pVm,LibxmlStructuredErr);` |
|    139 |  231 | `	return SySetUsed(&pVm->aLibxmlErr);` |
|      1 |  232 | `}` |
|    138 |  233 | `PH7_PRIVATE void PH7_LibxmlCaptureEnd(ph7_vm *pVm,sxu32 nMark,const char *zFnName)` |
|      1 |  234 | `{` |
|    139 |  235 | `	xmlSetStructuredErrorFunc(0,0);` |
|    139 |  236 | `	if( pVm->bLibxmlInternalErr ){` |
|      - |  237 | `		/* Internal capture on: entries stay queued for libxml_get_errors() */` |
|     13 |  238 | `		return;` |
|      - |  239 | `	}` |
|    127 |  240 | `	if( SySetUsed(&pVm->aLibxmlErr) > nMark ){` |
|    ! 0 |  241 | `		phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|      - |  242 | `		sxu32 n;` |
|    ! 0 |  243 | `		for( n = nMark ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|      - |  244 | `			SyString sFunc;` |
|      - |  245 | `			SyBlob sMsg;` |
|    ! 0 |  246 | `			sxu32 nTrim = aErr[n].sMsg.nByte;` |
|      - |  247 | `			/* php trims the trailing newline off the warning copy */` |
|    ! 0 |  248 | `			while( nTrim > 0 && (aErr[n].sMsg.zString[nTrim-1] == '\n' \|\| aErr[n].sMsg.zString[nTrim-1] == '\r') ){` |
|    ! 0 |  249 | `				nTrim--;` |
|    ! 0 |  250 | `			}` |
|    ! 0 |  251 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    ! 0 |  252 | `			SyBlobAppend(&sMsg,aErr[n].sMsg.zString,nTrim);` |
|      - |  253 | `			/* php appends the source location only for parser errors that` |
|      - |  254 | `			 * carry a real line; generic libxml errors print bare. */` |
|    ! 0 |  255 | `			if( aErr[n].iLine > 0 ){` |
|    ! 0 |  256 | `				if( aErr[n].sFile.nByte > 0 ){` |
|    ! 0 |  257 | `					SyBlobFormat(&sMsg," in %z, line: %d",&aErr[n].sFile,aErr[n].iLine);` |
|    ! 0 |  258 | `				}else{` |
|    ! 0 |  259 | `					SyBlobFormat(&sMsg," in Entity, line: %d",aErr[n].iLine);` |
|      - |  260 | `				}` |
|    ! 0 |  261 | `			}` |
|    ! 0 |  262 | `			SyBlobAppend(&sMsg,"\0",1);` |
|    ! 0 |  263 | `			SyStringInitFromBuf(&sFunc,zFnName,SyStrlen(zFnName));` |
|    ! 0 |  264 | `			PH7_VmThrowError(&(*pVm),zFnName ? &sFunc : 0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|    ! 0 |  265 | `			SyBlobRelease(&sMsg);` |
|    ! 0 |  266 | `			LibxmlFreeErr(pVm,&aErr[n]);` |
|    ! 0 |  267 | `		}` |
|    ! 0 |  268 | `		SySetTruncate(&pVm->aLibxmlErr,nMark);` |
|    ! 0 |  269 | `	}` |
|     70 |  270 | `}` |
|      - |  271 | `/*` |
|      - |  272 | ` * Allocate and register a new document shell on the per-VM registry.` |
|      - |  273 | ` * Returns NULL on allocation failure (the caller reports OOM).` |
|      - |  274 | ` */` |
|     92 |  275 | `PH7_PRIVATE phl_xmldoc * PH7_LibxmlNewDoc(ph7_vm *pVm,void *pXmlDocPtr)` |
|      1 |  276 | `{` |
|      - |  277 | `	phl_xmldoc *pDoc;` |
|     93 |  278 | `	pDoc = (phl_xmldoc *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_xmldoc));` |
|     93 |  279 | `	if( pDoc == 0 ){` |
|    ! 0 |  280 | `		return 0;` |
|      - |  281 | `	}` |
|     93 |  282 | `	SyZero(pDoc,sizeof(phl_xmldoc));` |
|     93 |  283 | `	SySetInit(&pDoc->aOrphans,&pVm->sAllocator,sizeof(void *));` |
|     93 |  284 | `	pDoc->pDoc = pXmlDocPtr;` |
|     93 |  285 | `	pDoc->pVm = &(*pVm);` |
|     93 |  286 | `	pDoc->bPreserveWS = 1;  /* DOMDocument->preserveWhiteSpace default */` |
|     93 |  287 | `	pDoc->bFormatOutput = 0;` |
|     93 |  288 | `	pDoc->pNext = (phl_xmldoc *)pVm->pXmlDocs;` |
|     93 |  289 | `	pVm->pXmlDocs = (void *)pDoc;` |
|     93 |  290 | `	return pDoc;` |
|     47 |  291 | `}` |
|      - |  292 |  |
|      - |  293 | `/* ===== Constants (php ext/libxml + ext/dom node types) ===== */` |
|      - |  294 |  |
|      - |  295 | `#define LIBXML_INT_CONST(FN,VALUE) \` |
|      - |  296 | `	static void FN(ph7_value *pVal,void *pUnused){ \` |
|      - |  297 | `		SXUNUSED(pUnused); \` |
|      - |  298 | `		ph7_value_int64(pVal,(ph7_int64)(VALUE)); \` |
|      - |  299 | `	}` |
|    ! 0 |  300 | `LIBXML_INT_CONST(LibxmlConst_VERSION,        LIBXML_VERSION)` |
|    ! 0 |  301 | `LIBXML_INT_CONST(LibxmlConst_NOENT,          XML_PARSE_NOENT)` |
|    ! 0 |  302 | `LIBXML_INT_CONST(LibxmlConst_DTDLOAD,        XML_PARSE_DTDLOAD)` |
|    ! 0 |  303 | `LIBXML_INT_CONST(LibxmlConst_DTDATTR,        XML_PARSE_DTDATTR)` |
|    ! 0 |  304 | `LIBXML_INT_CONST(LibxmlConst_DTDVALID,       XML_PARSE_DTDVALID)` |
|    ! 0 |  305 | `LIBXML_INT_CONST(LibxmlConst_NOERROR,        XML_PARSE_NOERROR)` |
|    ! 0 |  306 | `LIBXML_INT_CONST(LibxmlConst_NOWARNING,      XML_PARSE_NOWARNING)` |
|    ! 0 |  307 | `LIBXML_INT_CONST(LibxmlConst_NOBLANKS,       XML_PARSE_NOBLANKS)` |
|    ! 0 |  308 | `LIBXML_INT_CONST(LibxmlConst_XINCLUDE,       XML_PARSE_XINCLUDE)` |
|    ! 0 |  309 | `LIBXML_INT_CONST(LibxmlConst_NSCLEAN,        XML_PARSE_NSCLEAN)` |
|    ! 0 |  310 | `LIBXML_INT_CONST(LibxmlConst_NOCDATA,        XML_PARSE_NOCDATA)` |
|    ! 0 |  311 | `LIBXML_INT_CONST(LibxmlConst_NONET,          XML_PARSE_NONET)` |
|    ! 0 |  312 | `LIBXML_INT_CONST(LibxmlConst_PEDANTIC,       XML_PARSE_PEDANTIC)` |
|    ! 0 |  313 | `LIBXML_INT_CONST(LibxmlConst_COMPACT,        XML_PARSE_COMPACT)` |
|    ! 0 |  314 | `LIBXML_INT_CONST(LibxmlConst_PARSEHUGE,      XML_PARSE_HUGE)` |
|    ! 0 |  315 | `LIBXML_INT_CONST(LibxmlConst_BIGLINES,       XML_PARSE_BIG_LINES)` |
|    ! 0 |  316 | `LIBXML_INT_CONST(LibxmlConst_NOXMLDECL,      2)      /* XML_SAVE_NO_DECL */` |
|    ! 0 |  317 | `LIBXML_INT_CONST(LibxmlConst_NOEMPTYTAG,     4)      /* XML_SAVE_NO_EMPTY */` |
|    ! 0 |  318 | `LIBXML_INT_CONST(LibxmlConst_SCHEMA_CREATE,  1)      /* XML_SCHEMA_VAL_VC_I_CREATE */` |
|    ! 0 |  319 | `LIBXML_INT_CONST(LibxmlConst_ERR_NONE,       XML_ERR_NONE)` |
|    ! 0 |  320 | `LIBXML_INT_CONST(LibxmlConst_ERR_WARNING,    XML_ERR_WARNING)` |
|    ! 0 |  321 | `LIBXML_INT_CONST(LibxmlConst_ERR_ERROR,      XML_ERR_ERROR)` |
|      3 |  322 | `LIBXML_INT_CONST(LibxmlConst_ERR_FATAL,      XML_ERR_FATAL)` |
|      - |  323 | `/* ext/dom node-type constants (values fixed by the DOM spec / libxml enums) */` |
|    107 |  324 | `LIBXML_INT_CONST(LibxmlConst_ELEMENT_NODE,        XML_ELEMENT_NODE)` |
|     27 |  325 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_NODE,      XML_ATTRIBUTE_NODE)` |
|     19 |  326 | `LIBXML_INT_CONST(LibxmlConst_TEXT_NODE,           XML_TEXT_NODE)` |
|     13 |  327 | `LIBXML_INT_CONST(LibxmlConst_CDATA_SECTION_NODE,  XML_CDATA_SECTION_NODE)` |
|    ! 0 |  328 | `LIBXML_INT_CONST(LibxmlConst_ENTITY_REF_NODE,     XML_ENTITY_REF_NODE)` |
|    ! 0 |  329 | `LIBXML_INT_CONST(LibxmlConst_ENTITY_NODE,         XML_ENTITY_NODE)` |
|    ! 0 |  330 | `LIBXML_INT_CONST(LibxmlConst_PI_NODE,             XML_PI_NODE)` |
|      9 |  331 | `LIBXML_INT_CONST(LibxmlConst_COMMENT_NODE,        XML_COMMENT_NODE)` |
|      3 |  332 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_NODE,       XML_DOCUMENT_NODE)` |
|    ! 0 |  333 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_TYPE_NODE,  XML_DOCUMENT_TYPE_NODE)` |
|    ! 0 |  334 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_FRAG_NODE,  XML_DOCUMENT_FRAG_NODE)` |
|    ! 0 |  335 | `LIBXML_INT_CONST(LibxmlConst_NOTATION_NODE,       XML_NOTATION_NODE)` |
|    ! 0 |  336 | `LIBXML_INT_CONST(LibxmlConst_HTML_DOCUMENT_NODE,  XML_HTML_DOCUMENT_NODE)` |
|    ! 0 |  337 | `LIBXML_INT_CONST(LibxmlConst_DTD_NODE,            XML_DTD_NODE)` |
|      - |  338 |  |
|    ! 0 |  339 | `static void LibxmlConst_DOTTED_VERSION(ph7_value *pVal,void *pUnused)` |
|    ! 0 |  340 | `{` |
|    ! 0 |  341 | `	SXUNUSED(pUnused);` |
|    ! 0 |  342 | `	ph7_value_string(pVal,LIBXML_DOTTED_VERSION,-1);` |
|    ! 0 |  343 | `}` |
|    ! 0 |  344 | `static void LibxmlConst_LOADED_VERSION(ph7_value *pVal,void *pUnused)` |
|    ! 0 |  345 | `{` |
|    ! 0 |  346 | `	SXUNUSED(pUnused);` |
|      - |  347 | `	/* php exposes the runtime-loaded version string here */` |
|    ! 0 |  348 | `	ph7_value_string(pVal,(const char *)xmlParserVersion,-1);` |
|    ! 0 |  349 | `}` |
|      - |  350 |  |
|   3426 |  351 | `PH7_PRIVATE void PH7_RegisterLibxmlConstants(ph7_vm *pVm)` |
|      5 |  352 | `{` |
|      - |  353 | `	static const struct {` |
|      - |  354 | `		const char *zName;` |
|      - |  355 | `		void (*xExpand)(ph7_value *,void *);` |
|      - |  356 | `	} aConst[] = {` |
|      - |  357 | `		{ "LIBXML_VERSION",        LibxmlConst_VERSION        },` |
|      - |  358 | `		{ "LIBXML_DOTTED_VERSION", LibxmlConst_DOTTED_VERSION },` |
|      - |  359 | `		{ "LIBXML_LOADED_VERSION", LibxmlConst_LOADED_VERSION },` |
|      - |  360 | `		{ "LIBXML_NOENT",          LibxmlConst_NOENT          },` |
|      - |  361 | `		{ "LIBXML_DTDLOAD",        LibxmlConst_DTDLOAD        },` |
|      - |  362 | `		{ "LIBXML_DTDATTR",        LibxmlConst_DTDATTR        },` |
|      - |  363 | `		{ "LIBXML_DTDVALID",       LibxmlConst_DTDVALID       },` |
|      - |  364 | `		{ "LIBXML_NOERROR",        LibxmlConst_NOERROR        },` |
|      - |  365 | `		{ "LIBXML_NOWARNING",      LibxmlConst_NOWARNING      },` |
|      - |  366 | `		{ "LIBXML_NOBLANKS",       LibxmlConst_NOBLANKS       },` |
|      - |  367 | `		{ "LIBXML_XINCLUDE",       LibxmlConst_XINCLUDE       },` |
|      - |  368 | `		{ "LIBXML_NSCLEAN",        LibxmlConst_NSCLEAN        },` |
|      - |  369 | `		{ "LIBXML_NOCDATA",        LibxmlConst_NOCDATA        },` |
|      - |  370 | `		{ "LIBXML_NONET",          LibxmlConst_NONET          },` |
|      - |  371 | `		{ "LIBXML_PEDANTIC",       LibxmlConst_PEDANTIC       },` |
|      - |  372 | `		{ "LIBXML_COMPACT",        LibxmlConst_COMPACT        },` |
|      - |  373 | `		{ "LIBXML_PARSEHUGE",      LibxmlConst_PARSEHUGE      },` |
|      - |  374 | `		{ "LIBXML_BIGLINES",       LibxmlConst_BIGLINES       },` |
|      - |  375 | `		{ "LIBXML_NOXMLDECL",      LibxmlConst_NOXMLDECL      },` |
|      - |  376 | `		{ "LIBXML_NOEMPTYTAG",     LibxmlConst_NOEMPTYTAG     },` |
|      - |  377 | `		{ "LIBXML_SCHEMA_CREATE",  LibxmlConst_SCHEMA_CREATE  },` |
|      - |  378 | `		{ "LIBXML_ERR_NONE",       LibxmlConst_ERR_NONE       },` |
|      - |  379 | `		{ "LIBXML_ERR_WARNING",    LibxmlConst_ERR_WARNING    },` |
|      - |  380 | `		{ "LIBXML_ERR_ERROR",      LibxmlConst_ERR_ERROR      },` |
|      - |  381 | `		{ "LIBXML_ERR_FATAL",      LibxmlConst_ERR_FATAL      },` |
|      - |  382 | `		{ "XML_ELEMENT_NODE",       LibxmlConst_ELEMENT_NODE       },` |
|      - |  383 | `		{ "XML_ATTRIBUTE_NODE",     LibxmlConst_ATTRIBUTE_NODE     },` |
|      - |  384 | `		{ "XML_TEXT_NODE",          LibxmlConst_TEXT_NODE          },` |
|      - |  385 | `		{ "XML_CDATA_SECTION_NODE", LibxmlConst_CDATA_SECTION_NODE },` |
|      - |  386 | `		{ "XML_ENTITY_REF_NODE",    LibxmlConst_ENTITY_REF_NODE    },` |
|      - |  387 | `		{ "XML_ENTITY_NODE",        LibxmlConst_ENTITY_NODE        },` |
|      - |  388 | `		{ "XML_PI_NODE",            LibxmlConst_PI_NODE            },` |
|      - |  389 | `		{ "XML_COMMENT_NODE",       LibxmlConst_COMMENT_NODE       },` |
|      - |  390 | `		{ "XML_DOCUMENT_NODE",      LibxmlConst_DOCUMENT_NODE      },` |
|      - |  391 | `		{ "XML_DOCUMENT_TYPE_NODE", LibxmlConst_DOCUMENT_TYPE_NODE },` |
|      - |  392 | `		{ "XML_DOCUMENT_FRAG_NODE", LibxmlConst_DOCUMENT_FRAG_NODE },` |
|      - |  393 | `		{ "XML_NOTATION_NODE",      LibxmlConst_NOTATION_NODE      },` |
|      - |  394 | `		{ "XML_HTML_DOCUMENT_NODE", LibxmlConst_HTML_DOCUMENT_NODE },` |
|      - |  395 | `		{ "XML_DTD_NODE",           LibxmlConst_DTD_NODE           },` |
|      - |  396 | `	};` |
|      - |  397 | `	sxu32 n;` |
| 137045 |  398 | `	for( n = 0 ; n < sizeof(aConst)/sizeof(aConst[0]) ; n++ ){` |
| 133619 |  399 | `		ph7_create_constant(&(*pVm),aConst[n].zName,aConst[n].xExpand,0);` |
|  66812 |  400 | `	}` |
|   3431 |  401 | `}` |
|      - |  402 | `/* ===== libxml_* native thunks ===== */` |
|      - |  403 |  |
|      - |  404 | `/*` |
|      - |  405 | ` * Serialize one queue entry into a PHP assoc array with the LibXMLError` |
|      - |  406 | ` * property shape; the prelude wraps it into a LibXMLError instance.` |
|      - |  407 | ` */` |
|     12 |  408 | `static void LibxmlErrToArray(ph7_context *pCtx,const phl_libxml_err *pErr,ph7_value *pArray,ph7_value *pWorker)` |
|      1 |  409 | `{` |
|     13 |  410 | `	ph7_value_reset_string_cursor(pWorker);` |
|     13 |  411 | `	ph7_value_int(pWorker,pErr->iLevel);` |
|     13 |  412 | `	ph7_array_add_strkey_elem(pArray,"level",pWorker);` |
|     13 |  413 | `	ph7_value_reset_string_cursor(pWorker);` |
|     13 |  414 | `	ph7_value_int(pWorker,pErr->iCode);` |
|     13 |  415 | `	ph7_array_add_strkey_elem(pArray,"code",pWorker);` |
|     13 |  416 | `	ph7_value_reset_string_cursor(pWorker);` |
|     13 |  417 | `	ph7_value_int(pWorker,pErr->iColumn);` |
|     13 |  418 | `	ph7_array_add_strkey_elem(pArray,"column",pWorker);` |
|     13 |  419 | `	ph7_value_reset_string_cursor(pWorker);` |
|     13 |  420 | `	ph7_value_string(pWorker,pErr->sMsg.zString ? pErr->sMsg.zString : "",(int)pErr->sMsg.nByte);` |
|     13 |  421 | `	ph7_array_add_strkey_elem(pArray,"message",pWorker);` |
|     13 |  422 | `	ph7_value_reset_string_cursor(pWorker);` |
|     13 |  423 | `	ph7_value_string(pWorker,pErr->sFile.zString ? pErr->sFile.zString : "",(int)pErr->sFile.nByte);` |
|     13 |  424 | `	ph7_array_add_strkey_elem(pArray,"file",pWorker);` |
|     13 |  425 | `	ph7_value_reset_string_cursor(pWorker);` |
|     13 |  426 | `	ph7_value_int(pWorker,pErr->iLine);` |
|     13 |  427 | `	ph7_array_add_strkey_elem(pArray,"line",pWorker);` |
|      6 |  428 | `	SXUNUSED(pCtx);` |
|     13 |  429 | `}` |
|      - |  430 | `/*` |
|      - |  431 | ` * bool __libxml_use_internal_errors(?bool $use_errors = null)` |
|      - |  432 | ` *  Flip (or just report, on null/omitted) the internal-capture flag and` |
|      - |  433 | ` *  return the PREVIOUS state -- php semantics.` |
|      - |  434 | ` */` |
|     44 |  435 | `static int vm_builtin_libxml_use_internal_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  436 | `{` |
|     45 |  437 | `	ph7_vm *pVm = pCtx->pVm;` |
|     45 |  438 | `	int bPrev = pVm->bLibxmlInternalErr;` |
|     45 |  439 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|     39 |  440 | `		pVm->bLibxmlInternalErr = ph7_value_to_bool(apArg[0]) ? 1 : 0;` |
|     39 |  441 | `		if( !pVm->bLibxmlInternalErr ){` |
|      - |  442 | `			/* php frees the accumulated buffer when capture turns OFF */` |
|     27 |  443 | `			LibxmlClearQueue(pVm);` |
|     13 |  444 | `		}` |
|     19 |  445 | `	}` |
|     45 |  446 | `	ph7_result_bool(pCtx,bPrev);` |
|     45 |  447 | `	return PH7_OK;` |
|      1 |  448 | `}` |
|      - |  449 | `/*` |
|      - |  450 | ` * array __libxml_get_errors_raw()` |
|      - |  451 | ` *  The queued errors as raw assoc arrays, oldest first.` |
|      - |  452 | ` */` |
|     10 |  453 | `static int vm_builtin_libxml_get_errors_raw(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  454 | `{` |
|     11 |  455 | `	ph7_vm *pVm = pCtx->pVm;` |
|     11 |  456 | `	phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|     11 |  457 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|     11 |  458 | `	ph7_value *pWorker = ph7_context_new_scalar(pCtx);` |
|      - |  459 | `	sxu32 n;` |
|      5 |  460 | `	SXUNUSED(nArg);` |
|      5 |  461 | `	SXUNUSED(apArg);` |
|     11 |  462 | `	if( pList == 0 \|\| pWorker == 0 ){` |
|    ! 0 |  463 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  464 | `		ph7_result_null(pCtx);` |
|    ! 0 |  465 | `		return PH7_OK;` |
|      - |  466 | `	}` |
|     21 |  467 | `	for( n = 0 ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|     11 |  468 | `		ph7_value *pEntry = ph7_context_new_array(pCtx);` |
|     11 |  469 | `		if( pEntry == 0 ){` |
|    ! 0 |  470 | `			break;` |
|      - |  471 | `		}` |
|     11 |  472 | `		LibxmlErrToArray(pCtx,&aErr[n],pEntry,pWorker);` |
|     11 |  473 | `		ph7_array_add_elem(pList,0,pEntry);` |
|      6 |  474 | `	}` |
|     11 |  475 | `	ph7_result_value(pCtx,pList);` |
|     11 |  476 | `	return PH7_OK;` |
|      6 |  477 | `}` |
|      - |  478 | `/*` |
|      - |  479 | ` * void __libxml_clear_errors()` |
|      - |  480 | ` */` |
|     24 |  481 | `static int vm_builtin_libxml_clear_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  482 | `{` |
|     12 |  483 | `	SXUNUSED(nArg);` |
|     12 |  484 | `	SXUNUSED(apArg);` |
|     25 |  485 | `	PH7_LibxmlClearErrors(pCtx->pVm);` |
|     25 |  486 | `	ph7_result_null(pCtx);` |
|     25 |  487 | `	return PH7_OK;` |
|      1 |  488 | `}` |
|      - |  489 | `/*` |
|      - |  490 | ` * array\|false __libxml_get_last_error_raw()` |
|      - |  491 | ` */` |
|      4 |  492 | `static int vm_builtin_libxml_get_last_error_raw(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  493 | `{` |
|      5 |  494 | `	ph7_vm *pVm = pCtx->pVm;` |
|      5 |  495 | `	phl_libxml_err *pLast = (phl_libxml_err *)pVm->pLibxmlLastErr;` |
|      - |  496 | `	ph7_value *pEntry;` |
|      - |  497 | `	ph7_value *pWorker;` |
|      2 |  498 | `	SXUNUSED(nArg);` |
|      2 |  499 | `	SXUNUSED(apArg);` |
|      5 |  500 | `	if( pLast == 0 ){` |
|      3 |  501 | `		ph7_result_bool(pCtx,0);` |
|      3 |  502 | `		return PH7_OK;` |
|      - |  503 | `	}` |
|      3 |  504 | `	pEntry = ph7_context_new_array(pCtx);` |
|      3 |  505 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|      3 |  506 | `	if( pEntry == 0 \|\| pWorker == 0 ){` |
|    ! 0 |  507 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  508 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  509 | `		return PH7_OK;` |
|      - |  510 | `	}` |
|      3 |  511 | `	LibxmlErrToArray(pCtx,pLast,pEntry,pWorker);` |
|      3 |  512 | `	ph7_result_value(pCtx,pEntry);` |
|      3 |  513 | `	return PH7_OK;` |
|      3 |  514 | `}` |
|      - |  515 |  |
|      - |  516 | `/*` |
|      - |  517 | ` * The libxml PHP-visible surface: the LibXMLError class plus the four` |
|      - |  518 | ` * libxml_* functions, delegating to the __libxml_* thunks above.` |
|      - |  519 | ` */` |
|      - |  520 | `static const char zLibxmlLib[] =` |
|      - |  521 | `	"class LibXMLError"` |
|      - |  522 | `	"{"` |
|      - |  523 | `	"  public $level;"` |
|      - |  524 | `	"  public $code;"` |
|      - |  525 | `	"  public $column;"` |
|      - |  526 | `	"  public $message;"` |
|      - |  527 | `	"  public $file;"` |
|      - |  528 | `	"  public $line;"` |
|      - |  529 | `	"}"` |
|      - |  530 | `	"function __phl_libxml_err_obj($a)"` |
|      - |  531 | `	"{"` |
|      - |  532 | `	"  $e = new LibXMLError;"` |
|      - |  533 | `	"  $e->level = $a['level']; $e->code = $a['code']; $e->column = $a['column'];"` |
|      - |  534 | `	"  $e->message = $a['message']; $e->file = $a['file']; $e->line = $a['line'];"` |
|      - |  535 | `	"  return $e;"` |
|      - |  536 | `	"}"` |
|      - |  537 | `	"function libxml_use_internal_errors($use_errors = null)"` |
|      - |  538 | `	"{"` |
|      - |  539 | `	"  return __libxml_use_internal_errors($use_errors);"` |
|      - |  540 | `	"}"` |
|      - |  541 | `	"function libxml_clear_errors()"` |
|      - |  542 | `	"{"` |
|      - |  543 | `	"  __libxml_clear_errors();"` |
|      - |  544 | `	"}"` |
|      - |  545 | `	"function libxml_get_errors()"` |
|      - |  546 | `	"{"` |
|      - |  547 | `	"  $out = array();"` |
|      - |  548 | `	"  foreach( __libxml_get_errors_raw() as $a ){"` |
|      - |  549 | `	"    $out[] = __phl_libxml_err_obj($a);"` |
|      - |  550 | `	"  }"` |
|      - |  551 | `	"  return $out;"` |
|      - |  552 | `	"}"` |
|      - |  553 | `	"function libxml_get_last_error()"` |
|      - |  554 | `	"{"` |
|      - |  555 | `	"  $a = __libxml_get_last_error_raw();"` |
|      - |  556 | `	"  if( $a === false ){ return false; }"` |
|      - |  557 | `	"  return __phl_libxml_err_obj($a);"` |
|      - |  558 | `	"}";` |
|      - |  559 |  |
|      - |  560 | `/*` |
|      - |  561 | ` * Install the shared libxml layer: one-time global init, the per-VM state` |
|      - |  562 | ` * the DOM/XMLWriter surfaces build on, the __libxml_* thunks and the` |
|      - |  563 | ` * PHP-visible libxml_* functions + LibXMLError class.  Called from` |
|      - |  564 | ` * PH7_VmInit inside the bCompilingBuiltin window.` |
|      - |  565 | ` */` |
|   3894 |  566 | `PH7_PRIVATE sxi32 PH7_VmInstallLibxml(ph7_vm *pVm)` |
|      5 |  567 | `{` |
|      - |  568 | `	static const struct {` |
|      - |  569 | `		const char *zName;` |
|      - |  570 | `		ProchHostFunction xFunc;` |
|      - |  571 | `	} aFunc[] = {` |
|      - |  572 | `		{ "__libxml_use_internal_errors", vm_builtin_libxml_use_internal_errors },` |
|      - |  573 | `		{ "__libxml_get_errors_raw",      vm_builtin_libxml_get_errors_raw      },` |
|      - |  574 | `		{ "__libxml_clear_errors",        vm_builtin_libxml_clear_errors        },` |
|      - |  575 | `		{ "__libxml_get_last_error_raw",  vm_builtin_libxml_get_last_error_raw  },` |
|      - |  576 | `	};` |
|      - |  577 | `	sxu32 n;` |
|   3899 |  578 | `	LibxmlGlobalInit();` |
|   3899 |  579 | `	SySetInit(&pVm->aLibxmlErr,&pVm->sAllocator,sizeof(phl_libxml_err));` |
|   3899 |  580 | `	pVm->bLibxmlInternalErr = 0;` |
|   3899 |  581 | `	pVm->pLibxmlLastErr = 0;` |
|   3899 |  582 | `	pVm->pXmlDocs = 0;` |
|   3899 |  583 | `	pVm->pXmlWriters = 0;` |
|  19475 |  584 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
|  15581 |  585 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|   7793 |  586 | `	}` |
|   3899 |  587 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zLibxmlLib,sizeof(zLibxmlLib)-1);` |
|      5 |  588 | `}` |
|      - |  589 |  |
|      - |  590 | `#else` |
|      - |  591 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|      - |  592 | `typedef int vm_libxml_unused;` |
|      - |  593 | `#endif /* PH7_ENABLE_LIBXML */` |
|      - |  594 |  |
