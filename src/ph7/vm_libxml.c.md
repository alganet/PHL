# src/ph7/vm_libxml.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 232/317 lines (73.19%)

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
|   3884 |   38 | `static void LibxmlGlobalInit(void)` |
|      5 |   39 | `{` |
|      - |   40 | `	static int bInit = 0;` |
|   3889 |   41 | `	if( !bInit ){` |
|   3889 |   42 | `		xmlInitParser();` |
|   3889 |   43 | `		bInit = 1;` |
|   1942 |   44 | `	}` |
|   3889 |   45 | `}` |
|      - |   46 | `/*` |
|      - |   47 | ` * Free one registered document: its orphaned subtrees first, then the tree` |
|      - |   48 | ` * itself.  Registry links and the phl_xmldoc shell live in SyMemBackend and` |
|      - |   49 | ` * are reclaimed with the VM allocator.` |
|      - |   50 | ` */` |
|     72 |   51 | `static void LibxmlFreeDoc(phl_xmldoc *pDoc)` |
|      1 |   52 | `{` |
|     73 |   53 | `	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pDoc->aOrphans);` |
|      - |   54 | `	sxu32 n;` |
|     83 |   55 | `	for( n = 0 ; n < SySetUsed(&pDoc->aOrphans) ; ++n ){` |
|     11 |   56 | `		xmlUnlinkNode(apOrphan[n]);` |
|     11 |   57 | `		xmlFreeNode(apOrphan[n]);` |
|      6 |   58 | `	}` |
|     73 |   59 | `	SySetRelease(&pDoc->aOrphans);` |
|     73 |   60 | `	if( pDoc->pDoc ){` |
|     73 |   61 | `		xmlFreeDoc((xmlDocPtr)pDoc->pDoc);` |
|     73 |   62 | `		pDoc->pDoc = 0;` |
|     36 |   63 | `	}` |
|     73 |   64 | `}` |
|      - |   65 | `/*` |
|      - |   66 | ` * Reset the per-VM libxml state between executions: drop the accumulated` |
|      - |   67 | ` * error queue and free every document from the previous request.  Called` |
|      - |   68 | ` * from PH7_VmMakeReady() (which runs once per exec, including on VM reuse` |
|      - |   69 | ` * by the -S server) alongside the other per-exec field resets.` |
|      - |   70 | ` */` |
|   3434 |   71 | `PH7_PRIVATE void PH7_LibxmlVmReset(ph7_vm *pVm)` |
|      5 |   72 | `{` |
|      - |   73 | `	phl_xmldoc *pDoc,*pNext;` |
|   3439 |   74 | `	PH7_LibxmlClearErrors(pVm);` |
|   3439 |   75 | `	pVm->bLibxmlInternalErr = 0;` |
|   3439 |   76 | `	pDoc = (phl_xmldoc *)pVm->pXmlDocs;` |
|   3511 |   77 | `	while( pDoc ){` |
|     73 |   78 | `		pNext = pDoc->pNext;` |
|     73 |   79 | `		LibxmlFreeDoc(pDoc);` |
|     73 |   80 | `		SyMemBackendFree(&pVm->sAllocator,pDoc);` |
|     73 |   81 | `		pDoc = pNext;` |
|      1 |   82 | `	}` |
|   3439 |   83 | `	pVm->pXmlDocs = 0;` |
|   3439 |   84 | `}` |
|      - |   85 | `/*` |
|      - |   86 | ` * Final teardown on VM release.  Must run before SyMemBackendRelease()` |
|      - |   87 | ` * wipes the allocator that holds the registry shells.` |
|      - |   88 | ` */` |
|   3426 |   89 | `PH7_PRIVATE void PH7_LibxmlVmRelease(ph7_vm *pVm)` |
|      5 |   90 | `{` |
|   3431 |   91 | `	PH7_LibxmlVmReset(pVm);` |
|   3431 |   92 | `	SySetRelease(&pVm->aLibxmlErr);` |
|   3431 |   93 | `}` |
|      - |   94 | `/*` |
|      - |   95 | ` * Release the copied message/file strings of one queue entry.` |
|      - |   96 | ` */` |
|     16 |   97 | `static void LibxmlFreeErr(ph7_vm *pVm,phl_libxml_err *pErr)` |
|      1 |   98 | `{` |
|     17 |   99 | `	if( pErr->sMsg.zString ){` |
|     17 |  100 | `		SyMemBackendFree(&pVm->sAllocator,(void *)pErr->sMsg.zString);` |
|      8 |  101 | `	}` |
|     17 |  102 | `	if( pErr->sFile.zString ){` |
|    ! 0 |  103 | `		SyMemBackendFree(&pVm->sAllocator,(void *)pErr->sFile.zString);` |
|    ! 0 |  104 | `	}` |
|     17 |  105 | `	SyStringInitFromBuf(&pErr->sMsg,0,0);` |
|     17 |  106 | `	SyStringInitFromBuf(&pErr->sFile,0,0);` |
|     17 |  107 | `}` |
|      - |  108 | `/*` |
|      - |  109 | ` * Empty the libxml error queue, releasing the copied strings.  The` |
|      - |  110 | ` * last-error slot is kept (php parity: use_internal_errors(false) drops` |
|      - |  111 | ` * the buffer but libxml_get_last_error still reports).` |
|      - |  112 | ` */` |
|   3466 |  113 | `static void LibxmlClearQueue(ph7_vm *pVm)` |
|      5 |  114 | `{` |
|   3471 |  115 | `	phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|      - |  116 | `	sxu32 n;` |
|   3479 |  117 | `	for( n = 0 ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|      9 |  118 | `		LibxmlFreeErr(pVm,&aErr[n]);` |
|      5 |  119 | `	}` |
|   3471 |  120 | `	SySetReset(&pVm->aLibxmlErr);` |
|   3471 |  121 | `}` |
|      - |  122 | `/*` |
|      - |  123 | ` * libxml_clear_errors(): drop the queue AND the last-error slot.` |
|      - |  124 | ` */` |
|   3448 |  125 | `PH7_PRIVATE void PH7_LibxmlClearErrors(ph7_vm *pVm)` |
|      5 |  126 | `{` |
|   3453 |  127 | `	LibxmlClearQueue(pVm);` |
|   3453 |  128 | `	if( pVm->pLibxmlLastErr ){` |
|      7 |  129 | `		LibxmlFreeErr(pVm,(phl_libxml_err *)pVm->pLibxmlLastErr);` |
|      7 |  130 | `		SyMemBackendFree(&pVm->sAllocator,pVm->pLibxmlLastErr);` |
|      7 |  131 | `		pVm->pLibxmlLastErr = 0;` |
|      3 |  132 | `	}` |
|   3453 |  133 | `}` |
|      - |  134 | `/*` |
|      - |  135 | ` * Structured-error callback installed while a libxml2 entry point runs` |
|      - |  136 | ` * between PH7_LibxmlCaptureBegin/End.  Every reported error is queued on` |
|      - |  137 | ` * the per-VM list (the capture-end decides whether it stays there or is` |
|      - |  138 | ` * drained as a PHP warning) and copied into the last-error slot.` |
|      - |  139 | ` */` |
|      - |  140 | `#if LIBXML_VERSION >= 21200` |
|      4 |  141 | `static void LibxmlStructuredErrHandler(void *pUserData,const xmlError *pErr)` |
|      - |  142 | `#else` |
|      4 |  143 | `static void LibxmlStructuredErrHandler(void *pUserData,xmlErrorPtr pErr)` |
|      - |  144 | `#endif` |
|      1 |  145 | `{` |
|      9 |  146 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  147 | `	phl_libxml_err sEntry;` |
|      - |  148 | `	phl_libxml_err *pLast;` |
|      9 |  149 | `	if( pErr == 0 \|\| pVm == 0 ){` |
|    ! 0 |  150 | `		return;` |
|      - |  151 | `	}` |
|      9 |  152 | `	SyZero(&sEntry,sizeof(sEntry));` |
|      9 |  153 | `	sEntry.iLevel = (int)pErr->level;` |
|      9 |  154 | `	sEntry.iCode = pErr->code;` |
|      9 |  155 | `	sEntry.iLine = pErr->line;` |
|      9 |  156 | `	sEntry.iColumn = pErr->int2; /* libxml keeps the column in int2 */` |
|      9 |  157 | `	if( pErr->message ){` |
|      9 |  158 | `		sxu32 nMsg = SyStrlen(pErr->message);` |
|      9 |  159 | `		char *zMsg = SyMemBackendStrDup(&pVm->sAllocator,pErr->message,nMsg);` |
|      9 |  160 | `		if( zMsg ){` |
|      - |  161 | `			/* Trailing newline kept: php's LibXMLError->message preserves it */` |
|      9 |  162 | `			SyStringInitFromBuf(&sEntry.sMsg,zMsg,nMsg);` |
|      4 |  163 | `		}` |
|      4 |  164 | `	}` |
|      9 |  165 | `	if( pErr->file ){` |
|    ! 0 |  166 | `		sxu32 nFile = SyStrlen(pErr->file);` |
|    ! 0 |  167 | `		char *zFile = SyMemBackendStrDup(&pVm->sAllocator,pErr->file,nFile);` |
|    ! 0 |  168 | `		if( zFile ){` |
|    ! 0 |  169 | `			SyStringInitFromBuf(&sEntry.sFile,zFile,nFile);` |
|    ! 0 |  170 | `		}` |
|    ! 0 |  171 | `	}` |
|      9 |  172 | `	SySetPut(&pVm->aLibxmlErr,(const void *)&sEntry);` |
|      - |  173 | `	/* Mirror into the last-error slot (its strings are independent copies` |
|      - |  174 | `	 * so queue draining cannot invalidate it). */` |
|      9 |  175 | `	pLast = (phl_libxml_err *)pVm->pLibxmlLastErr;` |
|      9 |  176 | `	if( pLast == 0 ){` |
|      7 |  177 | `		pLast = (phl_libxml_err *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_libxml_err));` |
|      7 |  178 | `		if( pLast == 0 ){` |
|    ! 0 |  179 | `			return;` |
|      - |  180 | `		}` |
|      7 |  181 | `		SyZero(pLast,sizeof(phl_libxml_err));` |
|      7 |  182 | `		pVm->pLibxmlLastErr = (void *)pLast;` |
|      4 |  183 | `	}else{` |
|      3 |  184 | `		LibxmlFreeErr(pVm,pLast);` |
|      - |  185 | `	}` |
|      9 |  186 | `	pLast->iLevel = sEntry.iLevel;` |
|      9 |  187 | `	pLast->iCode = sEntry.iCode;` |
|      9 |  188 | `	pLast->iLine = sEntry.iLine;` |
|      9 |  189 | `	pLast->iColumn = sEntry.iColumn;` |
|      9 |  190 | `	if( sEntry.sMsg.zString ){` |
|      9 |  191 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,sEntry.sMsg.zString,sEntry.sMsg.nByte);` |
|      9 |  192 | `		if( zDup ){` |
|      9 |  193 | `			SyStringInitFromBuf(&pLast->sMsg,zDup,sEntry.sMsg.nByte);` |
|      4 |  194 | `		}` |
|      4 |  195 | `	}` |
|      9 |  196 | `	if( sEntry.sFile.zString ){` |
|    ! 0 |  197 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,sEntry.sFile.zString,sEntry.sFile.nByte);` |
|    ! 0 |  198 | `		if( zDup ){` |
|    ! 0 |  199 | `			SyStringInitFromBuf(&pLast->sFile,zDup,sEntry.sFile.nByte);` |
|    ! 0 |  200 | `		}` |
|    ! 0 |  201 | `	}` |
|      5 |  202 | `}` |
|      - |  203 | `/*` |
|      - |  204 | ` * Bracket a libxml2 entry point.  Begin installs the structured handler` |
|      - |  205 | ` * routed at this VM and returns the current queue depth; End restores the` |
|      - |  206 | ` * default handler and, when libxml_use_internal_errors() is OFF, drains` |
|      - |  207 | ` * every entry recorded since the mark as php-style warnings:` |
|      - |  208 | ` *   funcname(): <message> in <Entity\|file>, line: <n>` |
|      - |  209 | ` * (php's exact wording for the memory-parser case).` |
|      - |  210 | ` */` |
|     82 |  211 | `PH7_PRIVATE sxu32 PH7_LibxmlCaptureBegin(ph7_vm *pVm)` |
|      1 |  212 | `{` |
|     83 |  213 | `	xmlSetStructuredErrorFunc(pVm,LibxmlStructuredErrHandler);` |
|     83 |  214 | `	return SySetUsed(&pVm->aLibxmlErr);` |
|      1 |  215 | `}` |
|     82 |  216 | `PH7_PRIVATE void PH7_LibxmlCaptureEnd(ph7_vm *pVm,sxu32 nMark,const char *zFnName)` |
|      1 |  217 | `{` |
|     83 |  218 | `	xmlSetStructuredErrorFunc(0,0);` |
|     83 |  219 | `	if( pVm->bLibxmlInternalErr ){` |
|      - |  220 | `		/* Internal capture on: entries stay queued for libxml_get_errors() */` |
|      9 |  221 | `		return;` |
|      - |  222 | `	}` |
|     75 |  223 | `	if( SySetUsed(&pVm->aLibxmlErr) > nMark ){` |
|    ! 0 |  224 | `		phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|      - |  225 | `		sxu32 n;` |
|    ! 0 |  226 | `		for( n = nMark ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|      - |  227 | `			SyString sFunc;` |
|      - |  228 | `			SyBlob sMsg;` |
|    ! 0 |  229 | `			sxu32 nTrim = aErr[n].sMsg.nByte;` |
|      - |  230 | `			/* php trims the trailing newline off the warning copy */` |
|    ! 0 |  231 | `			while( nTrim > 0 && (aErr[n].sMsg.zString[nTrim-1] == '\n' \|\| aErr[n].sMsg.zString[nTrim-1] == '\r') ){` |
|    ! 0 |  232 | `				nTrim--;` |
|    ! 0 |  233 | `			}` |
|    ! 0 |  234 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    ! 0 |  235 | `			SyBlobAppend(&sMsg,aErr[n].sMsg.zString,nTrim);` |
|      - |  236 | `			/* php appends the source location only for parser errors that` |
|      - |  237 | `			 * carry a real line; generic libxml errors print bare. */` |
|    ! 0 |  238 | `			if( aErr[n].iLine > 0 ){` |
|    ! 0 |  239 | `				if( aErr[n].sFile.nByte > 0 ){` |
|    ! 0 |  240 | `					SyBlobFormat(&sMsg," in %z, line: %d",&aErr[n].sFile,aErr[n].iLine);` |
|    ! 0 |  241 | `				}else{` |
|    ! 0 |  242 | `					SyBlobFormat(&sMsg," in Entity, line: %d",aErr[n].iLine);` |
|      - |  243 | `				}` |
|    ! 0 |  244 | `			}` |
|    ! 0 |  245 | `			SyBlobAppend(&sMsg,"\0",1);` |
|    ! 0 |  246 | `			SyStringInitFromBuf(&sFunc,zFnName,SyStrlen(zFnName));` |
|    ! 0 |  247 | `			PH7_VmThrowError(&(*pVm),zFnName ? &sFunc : 0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|    ! 0 |  248 | `			SyBlobRelease(&sMsg);` |
|    ! 0 |  249 | `			LibxmlFreeErr(pVm,&aErr[n]);` |
|    ! 0 |  250 | `		}` |
|    ! 0 |  251 | `		SySetTruncate(&pVm->aLibxmlErr,nMark);` |
|    ! 0 |  252 | `	}` |
|     42 |  253 | `}` |
|      - |  254 | `/*` |
|      - |  255 | ` * Allocate and register a new document shell on the per-VM registry.` |
|      - |  256 | ` * Returns NULL on allocation failure (the caller reports OOM).` |
|      - |  257 | ` */` |
|     72 |  258 | `PH7_PRIVATE phl_xmldoc * PH7_LibxmlNewDoc(ph7_vm *pVm,void *pXmlDocPtr)` |
|      1 |  259 | `{` |
|      - |  260 | `	phl_xmldoc *pDoc;` |
|     73 |  261 | `	pDoc = (phl_xmldoc *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_xmldoc));` |
|     73 |  262 | `	if( pDoc == 0 ){` |
|    ! 0 |  263 | `		return 0;` |
|      - |  264 | `	}` |
|     73 |  265 | `	SyZero(pDoc,sizeof(phl_xmldoc));` |
|     73 |  266 | `	SySetInit(&pDoc->aOrphans,&pVm->sAllocator,sizeof(void *));` |
|     73 |  267 | `	pDoc->pDoc = pXmlDocPtr;` |
|     73 |  268 | `	pDoc->pVm = &(*pVm);` |
|     73 |  269 | `	pDoc->bPreserveWS = 1;  /* DOMDocument->preserveWhiteSpace default */` |
|     73 |  270 | `	pDoc->bFormatOutput = 0;` |
|     73 |  271 | `	pDoc->pNext = (phl_xmldoc *)pVm->pXmlDocs;` |
|     73 |  272 | `	pVm->pXmlDocs = (void *)pDoc;` |
|     73 |  273 | `	return pDoc;` |
|     37 |  274 | `}` |
|      - |  275 |  |
|      - |  276 | `/* ===== Constants (php ext/libxml + ext/dom node types) ===== */` |
|      - |  277 |  |
|      - |  278 | `#define LIBXML_INT_CONST(FN,VALUE) \` |
|      - |  279 | `	static void FN(ph7_value *pVal,void *pUnused){ \` |
|      - |  280 | `		SXUNUSED(pUnused); \` |
|      - |  281 | `		ph7_value_int64(pVal,(ph7_int64)(VALUE)); \` |
|      - |  282 | `	}` |
|    ! 0 |  283 | `LIBXML_INT_CONST(LibxmlConst_VERSION,        LIBXML_VERSION)` |
|    ! 0 |  284 | `LIBXML_INT_CONST(LibxmlConst_NOENT,          XML_PARSE_NOENT)` |
|    ! 0 |  285 | `LIBXML_INT_CONST(LibxmlConst_DTDLOAD,        XML_PARSE_DTDLOAD)` |
|    ! 0 |  286 | `LIBXML_INT_CONST(LibxmlConst_DTDATTR,        XML_PARSE_DTDATTR)` |
|    ! 0 |  287 | `LIBXML_INT_CONST(LibxmlConst_DTDVALID,       XML_PARSE_DTDVALID)` |
|    ! 0 |  288 | `LIBXML_INT_CONST(LibxmlConst_NOERROR,        XML_PARSE_NOERROR)` |
|    ! 0 |  289 | `LIBXML_INT_CONST(LibxmlConst_NOWARNING,      XML_PARSE_NOWARNING)` |
|    ! 0 |  290 | `LIBXML_INT_CONST(LibxmlConst_NOBLANKS,       XML_PARSE_NOBLANKS)` |
|    ! 0 |  291 | `LIBXML_INT_CONST(LibxmlConst_XINCLUDE,       XML_PARSE_XINCLUDE)` |
|    ! 0 |  292 | `LIBXML_INT_CONST(LibxmlConst_NSCLEAN,        XML_PARSE_NSCLEAN)` |
|    ! 0 |  293 | `LIBXML_INT_CONST(LibxmlConst_NOCDATA,        XML_PARSE_NOCDATA)` |
|    ! 0 |  294 | `LIBXML_INT_CONST(LibxmlConst_NONET,          XML_PARSE_NONET)` |
|    ! 0 |  295 | `LIBXML_INT_CONST(LibxmlConst_PEDANTIC,       XML_PARSE_PEDANTIC)` |
|    ! 0 |  296 | `LIBXML_INT_CONST(LibxmlConst_COMPACT,        XML_PARSE_COMPACT)` |
|    ! 0 |  297 | `LIBXML_INT_CONST(LibxmlConst_PARSEHUGE,      XML_PARSE_HUGE)` |
|    ! 0 |  298 | `LIBXML_INT_CONST(LibxmlConst_BIGLINES,       XML_PARSE_BIG_LINES)` |
|    ! 0 |  299 | `LIBXML_INT_CONST(LibxmlConst_NOXMLDECL,      2)      /* XML_SAVE_NO_DECL */` |
|    ! 0 |  300 | `LIBXML_INT_CONST(LibxmlConst_NOEMPTYTAG,     4)      /* XML_SAVE_NO_EMPTY */` |
|    ! 0 |  301 | `LIBXML_INT_CONST(LibxmlConst_SCHEMA_CREATE,  1)      /* XML_SCHEMA_VAL_VC_I_CREATE */` |
|    ! 0 |  302 | `LIBXML_INT_CONST(LibxmlConst_ERR_NONE,       XML_ERR_NONE)` |
|    ! 0 |  303 | `LIBXML_INT_CONST(LibxmlConst_ERR_WARNING,    XML_ERR_WARNING)` |
|    ! 0 |  304 | `LIBXML_INT_CONST(LibxmlConst_ERR_ERROR,      XML_ERR_ERROR)` |
|      3 |  305 | `LIBXML_INT_CONST(LibxmlConst_ERR_FATAL,      XML_ERR_FATAL)` |
|      - |  306 | `/* ext/dom node-type constants (values fixed by the DOM spec / libxml enums) */` |
|     89 |  307 | `LIBXML_INT_CONST(LibxmlConst_ELEMENT_NODE,        XML_ELEMENT_NODE)` |
|     27 |  308 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_NODE,      XML_ATTRIBUTE_NODE)` |
|     19 |  309 | `LIBXML_INT_CONST(LibxmlConst_TEXT_NODE,           XML_TEXT_NODE)` |
|     13 |  310 | `LIBXML_INT_CONST(LibxmlConst_CDATA_SECTION_NODE,  XML_CDATA_SECTION_NODE)` |
|    ! 0 |  311 | `LIBXML_INT_CONST(LibxmlConst_ENTITY_REF_NODE,     XML_ENTITY_REF_NODE)` |
|    ! 0 |  312 | `LIBXML_INT_CONST(LibxmlConst_ENTITY_NODE,         XML_ENTITY_NODE)` |
|    ! 0 |  313 | `LIBXML_INT_CONST(LibxmlConst_PI_NODE,             XML_PI_NODE)` |
|      9 |  314 | `LIBXML_INT_CONST(LibxmlConst_COMMENT_NODE,        XML_COMMENT_NODE)` |
|      3 |  315 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_NODE,       XML_DOCUMENT_NODE)` |
|    ! 0 |  316 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_TYPE_NODE,  XML_DOCUMENT_TYPE_NODE)` |
|    ! 0 |  317 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_FRAG_NODE,  XML_DOCUMENT_FRAG_NODE)` |
|    ! 0 |  318 | `LIBXML_INT_CONST(LibxmlConst_NOTATION_NODE,       XML_NOTATION_NODE)` |
|    ! 0 |  319 | `LIBXML_INT_CONST(LibxmlConst_HTML_DOCUMENT_NODE,  XML_HTML_DOCUMENT_NODE)` |
|    ! 0 |  320 | `LIBXML_INT_CONST(LibxmlConst_DTD_NODE,            XML_DTD_NODE)` |
|      - |  321 |  |
|    ! 0 |  322 | `static void LibxmlConst_DOTTED_VERSION(ph7_value *pVal,void *pUnused)` |
|    ! 0 |  323 | `{` |
|    ! 0 |  324 | `	SXUNUSED(pUnused);` |
|    ! 0 |  325 | `	ph7_value_string(pVal,LIBXML_DOTTED_VERSION,-1);` |
|    ! 0 |  326 | `}` |
|    ! 0 |  327 | `static void LibxmlConst_LOADED_VERSION(ph7_value *pVal,void *pUnused)` |
|    ! 0 |  328 | `{` |
|    ! 0 |  329 | `	SXUNUSED(pUnused);` |
|      - |  330 | `	/* php exposes the runtime-loaded version string here */` |
|    ! 0 |  331 | `	ph7_value_string(pVal,(const char *)xmlParserVersion,-1);` |
|    ! 0 |  332 | `}` |
|      - |  333 |  |
|   3428 |  334 | `PH7_PRIVATE void PH7_RegisterLibxmlConstants(ph7_vm *pVm)` |
|      5 |  335 | `{` |
|      - |  336 | `	static const struct {` |
|      - |  337 | `		const char *zName;` |
|      - |  338 | `		void (*xExpand)(ph7_value *,void *);` |
|      - |  339 | `	} aConst[] = {` |
|      - |  340 | `		{ "LIBXML_VERSION",        LibxmlConst_VERSION        },` |
|      - |  341 | `		{ "LIBXML_DOTTED_VERSION", LibxmlConst_DOTTED_VERSION },` |
|      - |  342 | `		{ "LIBXML_LOADED_VERSION", LibxmlConst_LOADED_VERSION },` |
|      - |  343 | `		{ "LIBXML_NOENT",          LibxmlConst_NOENT          },` |
|      - |  344 | `		{ "LIBXML_DTDLOAD",        LibxmlConst_DTDLOAD        },` |
|      - |  345 | `		{ "LIBXML_DTDATTR",        LibxmlConst_DTDATTR        },` |
|      - |  346 | `		{ "LIBXML_DTDVALID",       LibxmlConst_DTDVALID       },` |
|      - |  347 | `		{ "LIBXML_NOERROR",        LibxmlConst_NOERROR        },` |
|      - |  348 | `		{ "LIBXML_NOWARNING",      LibxmlConst_NOWARNING      },` |
|      - |  349 | `		{ "LIBXML_NOBLANKS",       LibxmlConst_NOBLANKS       },` |
|      - |  350 | `		{ "LIBXML_XINCLUDE",       LibxmlConst_XINCLUDE       },` |
|      - |  351 | `		{ "LIBXML_NSCLEAN",        LibxmlConst_NSCLEAN        },` |
|      - |  352 | `		{ "LIBXML_NOCDATA",        LibxmlConst_NOCDATA        },` |
|      - |  353 | `		{ "LIBXML_NONET",          LibxmlConst_NONET          },` |
|      - |  354 | `		{ "LIBXML_PEDANTIC",       LibxmlConst_PEDANTIC       },` |
|      - |  355 | `		{ "LIBXML_COMPACT",        LibxmlConst_COMPACT        },` |
|      - |  356 | `		{ "LIBXML_PARSEHUGE",      LibxmlConst_PARSEHUGE      },` |
|      - |  357 | `		{ "LIBXML_BIGLINES",       LibxmlConst_BIGLINES       },` |
|      - |  358 | `		{ "LIBXML_NOXMLDECL",      LibxmlConst_NOXMLDECL      },` |
|      - |  359 | `		{ "LIBXML_NOEMPTYTAG",     LibxmlConst_NOEMPTYTAG     },` |
|      - |  360 | `		{ "LIBXML_SCHEMA_CREATE",  LibxmlConst_SCHEMA_CREATE  },` |
|      - |  361 | `		{ "LIBXML_ERR_NONE",       LibxmlConst_ERR_NONE       },` |
|      - |  362 | `		{ "LIBXML_ERR_WARNING",    LibxmlConst_ERR_WARNING    },` |
|      - |  363 | `		{ "LIBXML_ERR_ERROR",      LibxmlConst_ERR_ERROR      },` |
|      - |  364 | `		{ "LIBXML_ERR_FATAL",      LibxmlConst_ERR_FATAL      },` |
|      - |  365 | `		{ "XML_ELEMENT_NODE",       LibxmlConst_ELEMENT_NODE       },` |
|      - |  366 | `		{ "XML_ATTRIBUTE_NODE",     LibxmlConst_ATTRIBUTE_NODE     },` |
|      - |  367 | `		{ "XML_TEXT_NODE",          LibxmlConst_TEXT_NODE          },` |
|      - |  368 | `		{ "XML_CDATA_SECTION_NODE", LibxmlConst_CDATA_SECTION_NODE },` |
|      - |  369 | `		{ "XML_ENTITY_REF_NODE",    LibxmlConst_ENTITY_REF_NODE    },` |
|      - |  370 | `		{ "XML_ENTITY_NODE",        LibxmlConst_ENTITY_NODE        },` |
|      - |  371 | `		{ "XML_PI_NODE",            LibxmlConst_PI_NODE            },` |
|      - |  372 | `		{ "XML_COMMENT_NODE",       LibxmlConst_COMMENT_NODE       },` |
|      - |  373 | `		{ "XML_DOCUMENT_NODE",      LibxmlConst_DOCUMENT_NODE      },` |
|      - |  374 | `		{ "XML_DOCUMENT_TYPE_NODE", LibxmlConst_DOCUMENT_TYPE_NODE },` |
|      - |  375 | `		{ "XML_DOCUMENT_FRAG_NODE", LibxmlConst_DOCUMENT_FRAG_NODE },` |
|      - |  376 | `		{ "XML_NOTATION_NODE",      LibxmlConst_NOTATION_NODE      },` |
|      - |  377 | `		{ "XML_HTML_DOCUMENT_NODE", LibxmlConst_HTML_DOCUMENT_NODE },` |
|      - |  378 | `		{ "XML_DTD_NODE",           LibxmlConst_DTD_NODE           },` |
|      - |  379 | `	};` |
|      - |  380 | `	sxu32 n;` |
| 137125 |  381 | `	for( n = 0 ; n < sizeof(aConst)/sizeof(aConst[0]) ; n++ ){` |
| 133697 |  382 | `		ph7_create_constant(&(*pVm),aConst[n].zName,aConst[n].xExpand,0);` |
|  66851 |  383 | `	}` |
|   3433 |  384 | `}` |
|      - |  385 | `/* ===== libxml_* native thunks ===== */` |
|      - |  386 |  |
|      - |  387 | `/*` |
|      - |  388 | ` * Serialize one queue entry into a PHP assoc array with the LibXMLError` |
|      - |  389 | ` * property shape; the prelude wraps it into a LibXMLError instance.` |
|      - |  390 | ` */` |
|      8 |  391 | `static void LibxmlErrToArray(ph7_context *pCtx,const phl_libxml_err *pErr,ph7_value *pArray,ph7_value *pWorker)` |
|      1 |  392 | `{` |
|      9 |  393 | `	ph7_value_reset_string_cursor(pWorker);` |
|      9 |  394 | `	ph7_value_int(pWorker,pErr->iLevel);` |
|      9 |  395 | `	ph7_array_add_strkey_elem(pArray,"level",pWorker);` |
|      9 |  396 | `	ph7_value_reset_string_cursor(pWorker);` |
|      9 |  397 | `	ph7_value_int(pWorker,pErr->iCode);` |
|      9 |  398 | `	ph7_array_add_strkey_elem(pArray,"code",pWorker);` |
|      9 |  399 | `	ph7_value_reset_string_cursor(pWorker);` |
|      9 |  400 | `	ph7_value_int(pWorker,pErr->iColumn);` |
|      9 |  401 | `	ph7_array_add_strkey_elem(pArray,"column",pWorker);` |
|      9 |  402 | `	ph7_value_reset_string_cursor(pWorker);` |
|      9 |  403 | `	ph7_value_string(pWorker,pErr->sMsg.zString ? pErr->sMsg.zString : "",(int)pErr->sMsg.nByte);` |
|      9 |  404 | `	ph7_array_add_strkey_elem(pArray,"message",pWorker);` |
|      9 |  405 | `	ph7_value_reset_string_cursor(pWorker);` |
|      9 |  406 | `	ph7_value_string(pWorker,pErr->sFile.zString ? pErr->sFile.zString : "",(int)pErr->sFile.nByte);` |
|      9 |  407 | `	ph7_array_add_strkey_elem(pArray,"file",pWorker);` |
|      9 |  408 | `	ph7_value_reset_string_cursor(pWorker);` |
|      9 |  409 | `	ph7_value_int(pWorker,pErr->iLine);` |
|      9 |  410 | `	ph7_array_add_strkey_elem(pArray,"line",pWorker);` |
|      4 |  411 | `	SXUNUSED(pCtx);` |
|      9 |  412 | `}` |
|      - |  413 | `/*` |
|      - |  414 | ` * bool __libxml_use_internal_errors(?bool $use_errors = null)` |
|      - |  415 | ` *  Flip (or just report, on null/omitted) the internal-capture flag and` |
|      - |  416 | ` *  return the PREVIOUS state -- php semantics.` |
|      - |  417 | ` */` |
|     32 |  418 | `static int vm_builtin_libxml_use_internal_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  419 | `{` |
|     33 |  420 | `	ph7_vm *pVm = pCtx->pVm;` |
|     33 |  421 | `	int bPrev = pVm->bLibxmlInternalErr;` |
|     33 |  422 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|     27 |  423 | `		pVm->bLibxmlInternalErr = ph7_value_to_bool(apArg[0]) ? 1 : 0;` |
|     27 |  424 | `		if( !pVm->bLibxmlInternalErr ){` |
|      - |  425 | `			/* php frees the accumulated buffer when capture turns OFF */` |
|     19 |  426 | `			LibxmlClearQueue(pVm);` |
|      9 |  427 | `		}` |
|     13 |  428 | `	}` |
|     33 |  429 | `	ph7_result_bool(pCtx,bPrev);` |
|     33 |  430 | `	return PH7_OK;` |
|      1 |  431 | `}` |
|      - |  432 | `/*` |
|      - |  433 | ` * array __libxml_get_errors_raw()` |
|      - |  434 | ` *  The queued errors as raw assoc arrays, oldest first.` |
|      - |  435 | ` */` |
|      8 |  436 | `static int vm_builtin_libxml_get_errors_raw(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  437 | `{` |
|      9 |  438 | `	ph7_vm *pVm = pCtx->pVm;` |
|      9 |  439 | `	phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|      9 |  440 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|      9 |  441 | `	ph7_value *pWorker = ph7_context_new_scalar(pCtx);` |
|      - |  442 | `	sxu32 n;` |
|      4 |  443 | `	SXUNUSED(nArg);` |
|      4 |  444 | `	SXUNUSED(apArg);` |
|      9 |  445 | `	if( pList == 0 \|\| pWorker == 0 ){` |
|    ! 0 |  446 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  447 | `		ph7_result_null(pCtx);` |
|    ! 0 |  448 | `		return PH7_OK;` |
|      - |  449 | `	}` |
|     15 |  450 | `	for( n = 0 ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|      7 |  451 | `		ph7_value *pEntry = ph7_context_new_array(pCtx);` |
|      7 |  452 | `		if( pEntry == 0 ){` |
|    ! 0 |  453 | `			break;` |
|      - |  454 | `		}` |
|      7 |  455 | `		LibxmlErrToArray(pCtx,&aErr[n],pEntry,pWorker);` |
|      7 |  456 | `		ph7_array_add_elem(pList,0,pEntry);` |
|      4 |  457 | `	}` |
|      9 |  458 | `	ph7_result_value(pCtx,pList);` |
|      9 |  459 | `	return PH7_OK;` |
|      5 |  460 | `}` |
|      - |  461 | `/*` |
|      - |  462 | ` * void __libxml_clear_errors()` |
|      - |  463 | ` */` |
|     14 |  464 | `static int vm_builtin_libxml_clear_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  465 | `{` |
|      7 |  466 | `	SXUNUSED(nArg);` |
|      7 |  467 | `	SXUNUSED(apArg);` |
|     15 |  468 | `	PH7_LibxmlClearErrors(pCtx->pVm);` |
|     15 |  469 | `	ph7_result_null(pCtx);` |
|     15 |  470 | `	return PH7_OK;` |
|      1 |  471 | `}` |
|      - |  472 | `/*` |
|      - |  473 | ` * array\|false __libxml_get_last_error_raw()` |
|      - |  474 | ` */` |
|      4 |  475 | `static int vm_builtin_libxml_get_last_error_raw(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  476 | `{` |
|      5 |  477 | `	ph7_vm *pVm = pCtx->pVm;` |
|      5 |  478 | `	phl_libxml_err *pLast = (phl_libxml_err *)pVm->pLibxmlLastErr;` |
|      - |  479 | `	ph7_value *pEntry;` |
|      - |  480 | `	ph7_value *pWorker;` |
|      2 |  481 | `	SXUNUSED(nArg);` |
|      2 |  482 | `	SXUNUSED(apArg);` |
|      5 |  483 | `	if( pLast == 0 ){` |
|      3 |  484 | `		ph7_result_bool(pCtx,0);` |
|      3 |  485 | `		return PH7_OK;` |
|      - |  486 | `	}` |
|      3 |  487 | `	pEntry = ph7_context_new_array(pCtx);` |
|      3 |  488 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|      3 |  489 | `	if( pEntry == 0 \|\| pWorker == 0 ){` |
|    ! 0 |  490 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  491 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  492 | `		return PH7_OK;` |
|      - |  493 | `	}` |
|      3 |  494 | `	LibxmlErrToArray(pCtx,pLast,pEntry,pWorker);` |
|      3 |  495 | `	ph7_result_value(pCtx,pEntry);` |
|      3 |  496 | `	return PH7_OK;` |
|      3 |  497 | `}` |
|      - |  498 |  |
|      - |  499 | `/*` |
|      - |  500 | ` * The libxml PHP-visible surface: the LibXMLError class plus the four` |
|      - |  501 | ` * libxml_* functions, delegating to the __libxml_* thunks above.` |
|      - |  502 | ` */` |
|      - |  503 | `static const char zLibxmlLib[] =` |
|      - |  504 | `	"class LibXMLError"` |
|      - |  505 | `	"{"` |
|      - |  506 | `	"  public $level;"` |
|      - |  507 | `	"  public $code;"` |
|      - |  508 | `	"  public $column;"` |
|      - |  509 | `	"  public $message;"` |
|      - |  510 | `	"  public $file;"` |
|      - |  511 | `	"  public $line;"` |
|      - |  512 | `	"}"` |
|      - |  513 | `	"function __phl_libxml_err_obj($a)"` |
|      - |  514 | `	"{"` |
|      - |  515 | `	"  $e = new LibXMLError;"` |
|      - |  516 | `	"  $e->level = $a['level']; $e->code = $a['code']; $e->column = $a['column'];"` |
|      - |  517 | `	"  $e->message = $a['message']; $e->file = $a['file']; $e->line = $a['line'];"` |
|      - |  518 | `	"  return $e;"` |
|      - |  519 | `	"}"` |
|      - |  520 | `	"function libxml_use_internal_errors($use_errors = null)"` |
|      - |  521 | `	"{"` |
|      - |  522 | `	"  return __libxml_use_internal_errors($use_errors);"` |
|      - |  523 | `	"}"` |
|      - |  524 | `	"function libxml_clear_errors()"` |
|      - |  525 | `	"{"` |
|      - |  526 | `	"  __libxml_clear_errors();"` |
|      - |  527 | `	"}"` |
|      - |  528 | `	"function libxml_get_errors()"` |
|      - |  529 | `	"{"` |
|      - |  530 | `	"  $out = array();"` |
|      - |  531 | `	"  foreach( __libxml_get_errors_raw() as $a ){"` |
|      - |  532 | `	"    $out[] = __phl_libxml_err_obj($a);"` |
|      - |  533 | `	"  }"` |
|      - |  534 | `	"  return $out;"` |
|      - |  535 | `	"}"` |
|      - |  536 | `	"function libxml_get_last_error()"` |
|      - |  537 | `	"{"` |
|      - |  538 | `	"  $a = __libxml_get_last_error_raw();"` |
|      - |  539 | `	"  if( $a === false ){ return false; }"` |
|      - |  540 | `	"  return __phl_libxml_err_obj($a);"` |
|      - |  541 | `	"}";` |
|      - |  542 |  |
|      - |  543 | `/*` |
|      - |  544 | ` * Install the shared libxml layer: one-time global init, the per-VM state` |
|      - |  545 | ` * the DOM/XMLWriter surfaces build on, the __libxml_* thunks and the` |
|      - |  546 | ` * PHP-visible libxml_* functions + LibXMLError class.  Called from` |
|      - |  547 | ` * PH7_VmInit inside the bCompilingBuiltin window.` |
|      - |  548 | ` */` |
|   3884 |  549 | `PH7_PRIVATE sxi32 PH7_VmInstallLibxml(ph7_vm *pVm)` |
|      5 |  550 | `{` |
|      - |  551 | `	static const struct {` |
|      - |  552 | `		const char *zName;` |
|      - |  553 | `		ProchHostFunction xFunc;` |
|      - |  554 | `	} aFunc[] = {` |
|      - |  555 | `		{ "__libxml_use_internal_errors", vm_builtin_libxml_use_internal_errors },` |
|      - |  556 | `		{ "__libxml_get_errors_raw",      vm_builtin_libxml_get_errors_raw      },` |
|      - |  557 | `		{ "__libxml_clear_errors",        vm_builtin_libxml_clear_errors        },` |
|      - |  558 | `		{ "__libxml_get_last_error_raw",  vm_builtin_libxml_get_last_error_raw  },` |
|      - |  559 | `	};` |
|      - |  560 | `	sxu32 n;` |
|   3889 |  561 | `	LibxmlGlobalInit();` |
|   3889 |  562 | `	SySetInit(&pVm->aLibxmlErr,&pVm->sAllocator,sizeof(phl_libxml_err));` |
|   3889 |  563 | `	pVm->bLibxmlInternalErr = 0;` |
|   3889 |  564 | `	pVm->pLibxmlLastErr = 0;` |
|   3889 |  565 | `	pVm->pXmlDocs = 0;` |
|   3889 |  566 | `	pVm->pXmlWriters = 0;` |
|  19425 |  567 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
|  15541 |  568 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|   7773 |  569 | `	}` |
|   3889 |  570 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zLibxmlLib,sizeof(zLibxmlLib)-1);` |
|      5 |  571 | `}` |
|      - |  572 |  |
|      - |  573 | `#else` |
|      - |  574 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|      - |  575 | `typedef int vm_libxml_unused;` |
|      - |  576 | `#endif /* PH7_ENABLE_LIBXML */` |
|      - |  577 |  |
