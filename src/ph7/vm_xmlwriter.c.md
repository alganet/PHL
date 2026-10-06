# src/ph7/vm_xmlwriter.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 718/802 lines (89.53%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    4 | ` */` |
|      - |    5 | `#ifdef PH7_ENABLE_LIBXML` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `#include <libxml/tree.h>` |
|      - |    8 | `#include <libxml/xmlerror.h>` |
|      - |    9 | `#include <libxml/xmlwriter.h>` |
|      - |   10 |  |
|      - |   11 | `/*` |
|      - |   12 | ` * ext/xmlwriter on libxml2: __xw_* native thunks + the XMLWriter class` |
|      - |   13 | ` * prelude.  Architecture notes live in vm_libxml.c (shared registries,` |
|      - |   14 | ` * error queue, lifetime model).` |
|      - |   15 | ` *` |
|      - |   16 | ` * An XMLWriter object holds a phl_xmlwriter resource {xmlTextWriterPtr,` |
|      - |   17 | ` * xmlBufferPtr} in $__res.  In-memory writers (openMemory) own an` |
|      - |   18 | ` * xmlBuffer; outputMemory reads it back.  A writer opened on a URI or` |
|      - |   19 | ` * attached to a stream carries an io_private instead and pushes its bytes` |
|      - |   20 | ` * through PH7_StreamWrite, so every wrapper this engine has -- php://,` |
|      - |   21 | ` * data://, a userland streamWrapper -- is a destination, as in php.` |
|      - |   22 | ` * Every writer is chained on the per-VM registry (pVm->pXmlWriters) and` |
|      - |   23 | ` * freed at VM reset/release since PH7 resources have no destructor hook.` |
|      - |   24 | ` */` |
|      - |   25 |  |
|      - |   26 | `typedef struct phl_xmlwriter phl_xmlwriter;` |
|      - |   27 | `struct phl_xmlwriter {` |
|      - |   28 | `	xmlTextWriterPtr pWriter;` |
|      - |   29 | `	xmlBufferPtr pBuf;   /* non-NULL for openMemory writers */` |
|      - |   30 | `	io_private *pDev;    /* non-NULL for openUri/toUri/toStream writers */` |
|      - |   31 | `	int bOwnDev;         /* this writer opened pDev, so its close closes it */` |
|      - |   32 | `	int bIoFailed;       /* the last verb's write to pDev failed (see XwRun) */` |
|      - |   33 | `	ph7_class_instance *pOwner; /* the object whose slot holds it: the one whose` |
|      - |   34 | `	                             * death flushes it (see XmlWriterInstanceRelease) */` |
|      - |   35 | `	phl_xmlwriter *pNext;` |
|      - |   36 | `};` |
|      - |   37 |  |
|      - |   38 | `/*` |
|      - |   39 | ` * libxml reports a write that failed through the error handler, and the paths` |
|      - |   40 | ` * that run while an object or the whole VM is going away have no caller to` |
|      - |   41 | ` * warn: php frees its writers with its request's error handling already gone,` |
|      - |   42 | ` * so nothing is printed there either. Drop whatever those paths raise.` |
|      - |   43 | ` */` |
|      - |   44 | `#if LIBXML_VERSION >= 21200` |
|    ! 0 |   45 | `static void XmlWriterSilentError(void *pUser,const xmlError *pErr)` |
|      - |   46 | `#else` |
|      6 |   47 | `static void XmlWriterSilentError(void *pUser,xmlErrorPtr pErr)` |
|      - |   48 | `#endif` |
|    ! 0 |   49 | `{` |
|    ! 0 |   50 | `	SXUNUSED(pUser);` |
|    ! 0 |   51 | `	SXUNUSED(pErr);` |
|      6 |   52 | `}` |
|      - |   53 | `/*` |
|      - |   54 | ` * Free one writer (called from the registry sweep in vm_libxml.c via` |
|      - |   55 | ` * PH7_XmlWriterVmRelease).  The order matters: the text writer must be` |
|      - |   56 | ` * freed before its backing buffer.` |
|      - |   57 | ` */` |
|    132 |   58 | `static void XmlWriterFree(phl_xmlwriter *pXw)` |
|      1 |   59 | `{` |
|    133 |   60 | `	xmlSetStructuredErrorFunc(0,XmlWriterSilentError);` |
|    133 |   61 | `	if( pXw->pWriter ){` |
|    133 |   62 | `		xmlFreeTextWriter(pXw->pWriter);` |
|    133 |   63 | `		pXw->pWriter = 0;` |
|     66 |   64 | `	}` |
|    133 |   65 | `	if( pXw->pBuf ){` |
|    121 |   66 | `		xmlBufferFree(pXw->pBuf);` |
|    121 |   67 | `		pXw->pBuf = 0;` |
|     60 |   68 | `	}` |
|    133 |   69 | `	xmlSetStructuredErrorFunc(0,0);` |
|    133 |   70 | `}` |
|      - |   71 | `/*` |
|      - |   72 | ` * Free every registered writer.  Called from PH7_LibxmlVmReset /` |
|      - |   73 | ` * PH7_LibxmlVmRelease before the allocator that holds the shells is torn` |
|      - |   74 | ` * down.` |
|      - |   75 | ` */` |
|   7011 |   76 | `PH7_PRIVATE void PH7_XmlWriterVmSweep(ph7_vm *pVm)` |
|      5 |   77 | `{` |
|   7016 |   78 | `	phl_xmlwriter *pXw = (phl_xmlwriter *)pVm->pXmlWriters;` |
|   7148 |   79 | `	while( pXw ){` |
|    133 |   80 | `		phl_xmlwriter *pNext = pXw->pNext;` |
|    133 |   81 | `		XmlWriterFree(pXw);` |
|    133 |   82 | `		SyMemBackendFree(&pVm->sAllocator,pXw);` |
|    133 |   83 | `		pXw = pNext;` |
|      1 |   84 | `	}` |
|   7016 |   85 | `	pVm->pXmlWriters = 0;` |
|   7016 |   86 | `}` |
|      - |   87 |  |
|      - |   88 | `/*` |
|      - |   89 | ` * A writer's bytes on their way to a stream. libxml calls this from its own` |
|      - |   90 | ` * output buffer, so everything the engine's stream layer does for fwrite() --` |
|      - |   91 | ` * the filter chain, the buffered position -- happens here too.` |
|      - |   92 | ` */` |
|     34 |   93 | `static int XmlWriterIoWrite(void *pUser,const char *zBuf,int nLen)` |
|      1 |   94 | `{` |
|     35 |   95 | `	phl_xmlwriter *pXw = (phl_xmlwriter *)pUser;` |
|      - |   96 | `	ph7_int64 nWr;` |
|     35 |   97 | `	if( pXw->pDev == 0 \|\| IO_PRIVATE_INVALID(pXw->pDev) ){` |
|      - |   98 | `		/* The script fclose()'d the handle it handed toStream() */` |
|     13 |   99 | `		pXw->bIoFailed = 1;` |
|     13 |  100 | `		return -1;` |
|      - |  101 | `	}` |
|     23 |  102 | `	nWr = PH7_StreamWrite(pXw->pDev,(const void *)zBuf,(ph7_int64)nLen);` |
|     23 |  103 | `	if( nWr < 0 ){` |
|    ! 0 |  104 | `		pXw->bIoFailed = 1;` |
|    ! 0 |  105 | `		return -1;` |
|      - |  106 | `	}` |
|     23 |  107 | `	return (int)nWr;` |
|     18 |  108 | `}` |
|      - |  109 | `/*` |
|      - |  110 | ` * The output buffer's close, reached from xmlFreeTextWriter (so: the registry` |
|      - |  111 | ` * sweep at VM reset). A handle this writer OPENED is closed here, which is` |
|      - |  112 | ` * where a file written through openUri() gets its last bytes; a borrowed one --` |
|      - |  113 | ` * toStream()'s -- is left alone, because the script still holds it.` |
|      - |  114 | ` */` |
|    144 |  115 | `static int XmlWriterIoClose(void *pUser)` |
|      1 |  116 | `{` |
|    145 |  117 | `	phl_xmlwriter *pXw = (phl_xmlwriter *)pUser;` |
|    145 |  118 | `	if( pXw->bOwnDev && pXw->pDev && !IO_PRIVATE_INVALID(pXw->pDev) ){` |
|      9 |  119 | `		PH7_StreamFilterReleaseChains(pXw->pDev);` |
|      9 |  120 | `		PH7_StreamCloseHandle(pXw->pDev->pStream,pXw->pDev->pHandle);` |
|      9 |  121 | `		MarkIOPrivateClosed(pXw->pDev);` |
|      4 |  122 | `	}` |
|    145 |  123 | `	pXw->pDev = 0;` |
|    145 |  124 | `	return 0;` |
|      1 |  125 | `}` |
|      - |  126 | `/*` |
|      - |  127 | ` * Flush a writer and let go of its stream: what php does when the writer is` |
|      - |  128 | ` * freed, which is the point at which a file opened through openUri() is` |
|      - |  129 | ` * complete. The libxml writer itself stays alive (the registry sweep frees it),` |
|      - |  130 | ` * so a later call answers false instead of reaching released memory.` |
|      - |  131 | ` */` |
|    132 |  132 | `static void XmlWriterFlushAndDetach(phl_xmlwriter *pXw)` |
|      1 |  133 | `{` |
|    133 |  134 | `	xmlSetStructuredErrorFunc(0,XmlWriterSilentError);` |
|    133 |  135 | `	if( pXw->pWriter ){` |
|    133 |  136 | `		xmlTextWriterFlush(pXw->pWriter);` |
|     66 |  137 | `	}` |
|    133 |  138 | `	XmlWriterIoClose((void *)pXw);` |
|    133 |  139 | `	xmlSetStructuredErrorFunc(0,0);` |
|    133 |  140 | `	pXw->pOwner = 0;` |
|    133 |  141 | `}` |
|    786 |  142 | `static phl_xmlwriter * XmlWriterArg(ph7_value *pVal)` |
|      1 |  143 | `{` |
|    787 |  144 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|    175 |  145 | `		return 0;` |
|      - |  146 | `	}` |
|    613 |  147 | `	return (phl_xmlwriter *)ph7_value_to_resource(pVal);` |
|    394 |  148 | `}` |
|      - |  149 |  |
|      - |  150 | `/*` |
|      - |  151 | ` * The writer behind $this->__res.` |
|      - |  152 | ` *` |
|      - |  153 | `` * These methods were global `__xw_verb($this->__res, ...)` thunks, so each body`` |
|      - |  154 | ` * used to take the resource as argument #0. As native methods they reach it the` |
|      - |  155 | ` * way php does -- through the receiver -- and their arguments start at #0.` |
|      - |  156 | ` */` |
|    436 |  157 | `static phl_xmlwriter * XmlWriterOf(ph7_context *pCtx)` |
|      1 |  158 | `{` |
|    437 |  159 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  160 | `	SyString sAttr;` |
|      - |  161 | `	ph7_value *pRes;` |
|    437 |  162 | `	if( pThis == 0 ){` |
|    ! 0 |  163 | `		return 0;` |
|      - |  164 | `	}` |
|    437 |  165 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|    437 |  166 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|    437 |  167 | `	return XmlWriterArg(pRes);` |
|    219 |  168 | `}` |
|      - |  169 | `/*` |
|      - |  170 | ` * One call into a writer verb.` |
|      - |  171 | ` *` |
|      - |  172 | ` * php implements the class and the procedural surface with ONE C function per` |
|      - |  173 | `` * verb: the method spelling reaches the writer through `$this`, the function`` |
|      - |  174 | ` * spelling takes it as argument #1 and shifts every other argument up by one.` |
|      - |  175 | ` * The two therefore share their diagnostics, and that is visible from PHP --` |
|      - |  176 | ` * see XmlWriterCheckName for the off-by-one it leaves in the method's own` |
|      - |  177 | ` * error text. A verb body reads its arguments through this descriptor so both` |
|      - |  178 | ` * entry points can drive it.` |
|      - |  179 | ` */` |
|      - |  180 | `typedef struct xw_call xw_call;` |
|      - |  181 | `struct xw_call {` |
|      - |  182 | `	phl_xmlwriter *pXw;    /* the resolved writer; never NULL in a verb body */` |
|      - |  183 | `	const char *zFn;       /* "XMLWriter::startElement" / "xmlwriter_start_element" */` |
|      - |  184 | `	const char *zNameArg;  /* this spelling's argument text for the NAME argument */` |
|      - |  185 | `	int iArgBase;          /* 0 for a method, 1 for the function spelling: what a` |
|      - |  186 | `	                        * ZPP-style message adds to an argument's position */` |
|      - |  187 | `	int nArg;              /* how many arguments follow the writer */` |
|      - |  188 | `	ph7_value **apArg;     /* the first argument past the writer */` |
|      - |  189 | `};` |
|      - |  190 | `/*` |
|      - |  191 | ` * php's Z_XMLWRITER_P: an XMLWriter that was never opened has no writer behind` |
|      - |  192 | ` * it, and every method and every procedural entry refuses it with this Error` |
|      - |  193 | ` * rather than answering false -- which is what a caller would otherwise store` |
|      - |  194 | ` * or print as if a document had been written.` |
|      - |  195 | ` */` |
|     14 |  196 | `static int XmlWriterMissing(ph7_context *pCtx)` |
|      1 |  197 | `{` |
|     15 |  198 | `	return PH7_VmThrowException(pCtx,"Error","Invalid or uninitialized XMLWriter object");` |
|      1 |  199 | `}` |
|      - |  200 | `/*` |
|      - |  201 | `` * The writer behind an `XMLWriter $writer` argument. The declared type has`` |
|      - |  202 | ` * already refused everything that is not one.` |
|      - |  203 | ` */` |
|     58 |  204 | `static phl_xmlwriter * XmlWriterOfValue(ph7_value *pArg)` |
|      1 |  205 | `{` |
|      - |  206 | `	ph7_class_instance *pThis;` |
|      - |  207 | `	SyString sAttr;` |
|     59 |  208 | `	if( pArg == 0 \|\| (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 |  209 | `		return 0;` |
|      - |  210 | `	}` |
|     59 |  211 | `	pThis = (ph7_class_instance *)pArg->x.pOther;` |
|     59 |  212 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|     59 |  213 | `	return XmlWriterArg(PH7_ClassInstanceFetchAttr(pThis,&sAttr));` |
|     30 |  214 | `}` |
|      - |  215 | `/*` |
|      - |  216 | ` * Build the call descriptor for the METHOD spelling. Returns 0 when the verb` |
|      - |  217 | ` * may run, or the throw status when the receiver holds no writer.` |
|      - |  218 | ` */` |
|    436 |  219 | `static int XwCallFromThis(ph7_context *pCtx,xw_call *pCall,const char *zFn,` |
|      - |  220 | `	const char *zNameArg,int nArg,ph7_value **apArg)` |
|      1 |  221 | `{` |
|    437 |  222 | `	phl_xmlwriter *pXw = XmlWriterOf(pCtx);` |
|    437 |  223 | `	if( pXw == 0 \|\| pXw->pWriter == 0 ){` |
|     13 |  224 | `		return XmlWriterMissing(pCtx);` |
|      - |  225 | `	}` |
|    425 |  226 | `	pCall->pXw = pXw;` |
|    425 |  227 | `	pCall->zFn = zFn;` |
|    425 |  228 | `	pCall->zNameArg = zNameArg;` |
|    425 |  229 | `	pCall->iArgBase = 0;` |
|    425 |  230 | `	pCall->nArg = nArg;` |
|    425 |  231 | `	pCall->apArg = apArg;` |
|    425 |  232 | `	return 0;` |
|    219 |  233 | `}` |
|      - |  234 | `/* The n-th argument past the writer as a string, or "" when it is absent. */` |
|    290 |  235 | `static const char * XwStr(xw_call *pCall,int iArg)` |
|      1 |  236 | `{` |
|    291 |  237 | `	if( iArg >= pCall->nArg \|\| pCall->apArg[iArg] == 0 ){` |
|    ! 0 |  238 | `		return "";` |
|      - |  239 | `	}` |
|    291 |  240 | `	return ph7_value_to_string(pCall->apArg[iArg],0);` |
|    146 |  241 | `}` |
|      - |  242 | `/* The n-th argument past the writer as a string, or NULL for an absent/null one. */` |
|    232 |  243 | `static const char * XwStrOrNull(xw_call *pCall,int iArg)` |
|      1 |  244 | `{` |
|    232 |  245 | `	if( iArg >= pCall->nArg \|\| pCall->apArg[iArg] == 0` |
|    167 |  246 | `	 \|\| ph7_value_is_null(pCall->apArg[iArg]) ){` |
|    103 |  247 | `		return 0;` |
|      - |  248 | `	}` |
|    131 |  249 | `	return ph7_value_to_string(pCall->apArg[iArg],0);` |
|    117 |  250 | `}` |
|      - |  251 | `/* The n-th argument past the writer as a bool. */` |
|    120 |  252 | `static int XwBool(xw_call *pCall,int iArg,int bDefault)` |
|      1 |  253 | `{` |
|    121 |  254 | `	if( iArg >= pCall->nArg \|\| pCall->apArg[iArg] == 0 ){` |
|     91 |  255 | `		return bDefault;` |
|      - |  256 | `	}` |
|     31 |  257 | `	return ph7_value_to_bool(pCall->apArg[iArg]) ? 1 : 0;` |
|     61 |  258 | `}` |
|      - |  259 | `/*` |
|      - |  260 | ` * php's XMLW_NAME_CHK: the name a verb is handed is validated with libxml's own` |
|      - |  261 | ` * xmlValidateName and refused with a ValueError before anything is written --` |
|      - |  262 | ` * the alternative is what this engine used to do, which is to hand libxml a name` |
|      - |  263 | `` * it will not quote and emit a document that is not XML (`<1bad`, `<a x y="v"`).`` |
|      - |  264 | ` *` |
|      - |  265 | ` * The argument NUMBER php reports is the PROCEDURAL one, hardcoded in the macro,` |
|      - |  266 | ` * so the method spelling reports its own first argument as "#2" and prints` |
|      - |  267 | `` * whichever of its OWN parameters sits at that shifted position -- `$content`,`` |
|      - |  268 | `` * `$value`, `$isParam` -- or no name at all when it has none. Reproduced as`` |
|      - |  269 | ` * written: each entry point states its spelling's text.` |
|      - |  270 | ` */` |
|    196 |  271 | `static int XmlWriterCheckName(ph7_context *pCtx,xw_call *pCall,const char *zName,` |
|      - |  272 | `	const char *zWhat)` |
|      1 |  273 | `{` |
|    197 |  274 | `	if( xmlValidateName((const xmlChar *)zName,0) == 0 ){` |
|    149 |  275 | `		return 0;` |
|      - |  276 | `	}` |
|     73 |  277 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  278 | `		"%s(): Argument %s must be a valid %s, \"%s\" given",` |
|     24 |  279 | `		pCall->zFn,pCall->zNameArg,zWhat,zName);` |
|     99 |  280 | `}` |
|      - |  281 | `/*` |
|      - |  282 | ` * Allocate a writer shell and chain it on the per-VM registry. The shell is` |
|      - |  283 | ` * reclaimed with the allocator and the libxml writer behind it by the sweep;` |
|      - |  284 | ` * a URI writer's own handle is flushed and closed earlier, when the object` |
|      - |  285 | ` * holding it dies (XmlWriterInstanceRelease).` |
|      - |  286 | ` */` |
|    132 |  287 | `static phl_xmlwriter * XmlWriterNew(ph7_vm *pVm)` |
|      1 |  288 | `{` |
|    133 |  289 | `	phl_xmlwriter *pXw = (phl_xmlwriter *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_xmlwriter));` |
|    133 |  290 | `	if( pXw == 0 ){` |
|    ! 0 |  291 | `		return 0;` |
|      - |  292 | `	}` |
|    133 |  293 | `	SyZero(pXw,sizeof(phl_xmlwriter));` |
|    133 |  294 | `	pXw->pNext = (phl_xmlwriter *)pVm->pXmlWriters;` |
|    133 |  295 | `	pVm->pXmlWriters = (void *)pXw;` |
|    133 |  296 | `	return pXw;` |
|     67 |  297 | `}` |
|      - |  298 | `/*` |
|      - |  299 | ` * Store a writer in an object's hidden slot, flushing the one it replaces.` |
|      - |  300 | ` * The replaced SHELL is not freed: the slot is reachable from PHP, so another` |
|      - |  301 | ` * value may still name it, and the registry sweep frees every writer anyway.` |
|      - |  302 | ` */` |
|    132 |  303 | `static int XmlWriterAttach(ph7_class_instance *pThis,phl_xmlwriter *pXw)` |
|      1 |  304 | `{` |
|      - |  305 | `	SyString sAttr;` |
|      - |  306 | `	ph7_value *pRes;` |
|    133 |  307 | `	if( pThis == 0 ){` |
|    ! 0 |  308 | `		return -1;` |
|      - |  309 | `	}` |
|    133 |  310 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|    133 |  311 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|    133 |  312 | `	if( pRes == 0 ){` |
|    ! 0 |  313 | `		return -1;` |
|      - |  314 | `	}` |
|      - |  315 | `	{` |
|      - |  316 | `		/* A second open on the same object replaces its writer, and php frees` |
|      - |  317 | `		 * the old one there -- which is what flushes a file opened through` |
|      - |  318 | `		 * openUri(). Flush and close it here for the same reason; the shell` |
|      - |  319 | `		 * itself stays on the registry, since the slot is reachable from PHP. */` |
|    133 |  320 | `		phl_xmlwriter *pOld = XmlWriterArg(pRes);` |
|    133 |  321 | `		if( pOld && pOld != pXw && pOld->pOwner == pThis ){` |
|    ! 0 |  322 | `			XmlWriterFlushAndDetach(pOld);` |
|    ! 0 |  323 | `		}` |
|      - |  324 | `	}` |
|    133 |  325 | `	PH7_MemObjRelease(pRes);` |
|    133 |  326 | `	pRes->x.pOther = pXw;` |
|    133 |  327 | `	MemObjSetType(pRes,MEMOBJ_RES);` |
|    133 |  328 | `	pXw->pOwner = pThis;` |
|    133 |  329 | `	return 0;` |
|     67 |  330 | `}` |
|      - |  331 | `/*` |
|      - |  332 | ` * php frees the libxml writer with the OBJECT, and for a writer opened on a URI` |
|      - |  333 | `` * that is when the file gets its last bytes -- `unset($w)` and then reading the`` |
|      - |  334 | ` * file is how a document written to disk is finished. PH7 resources carry no` |
|      - |  335 | ` * destructor hook, but a native class does: xRelease runs while the instance's` |
|      - |  336 | ` * slots are still readable, and before the registry sweep.` |
|      - |  337 | ` *` |
|      - |  338 | ` * Only the object the writer was ATTACHED to flushes it. The hidden slot is` |
|      - |  339 | ` * assignable from PHP, so a second object can name the same writer; the shell` |
|      - |  340 | ` * is freed by the sweep and never here, and a write after the flush answers` |
|      - |  341 | ` * false rather than reaching released memory.` |
|      - |  342 | ` */` |
|    160 |  343 | `static void XmlWriterInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      1 |  344 | `{` |
|      - |  345 | `	SyString sAttr;` |
|      - |  346 | `	ph7_value *pRes;` |
|      - |  347 | `	phl_xmlwriter *pXw;` |
|     80 |  348 | `	SXUNUSED(pVm);` |
|    161 |  349 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|    161 |  350 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|    161 |  351 | `	pXw = XmlWriterArg(pRes);` |
|    161 |  352 | `	if( pXw == 0 \|\| pXw->pOwner != pThis ){` |
|     29 |  353 | `		return;` |
|      - |  354 | `	}` |
|    133 |  355 | `	XmlWriterFlushAndDetach(pXw);` |
|     81 |  356 | `}` |
|      - |  357 | `/* An in-memory writer: its own xmlBuffer, which outputMemory() reads back. */` |
|    120 |  358 | `static phl_xmlwriter * XmlWriterOpenMemory(ph7_vm *pVm)` |
|      1 |  359 | `{` |
|    121 |  360 | `	phl_xmlwriter *pXw = XmlWriterNew(pVm);` |
|    121 |  361 | `	if( pXw == 0 ){` |
|    ! 0 |  362 | `		return 0;` |
|      - |  363 | `	}` |
|    121 |  364 | `	pXw->pBuf = xmlBufferCreate();` |
|    121 |  365 | `	if( pXw->pBuf ){` |
|    121 |  366 | `		pXw->pWriter = xmlNewTextWriterMemory(pXw->pBuf,0);` |
|     60 |  367 | `	}` |
|    121 |  368 | `	if( pXw->pWriter == 0 ){` |
|    ! 0 |  369 | `		XmlWriterFree(pXw);` |
|    ! 0 |  370 | `		return 0;` |
|      - |  371 | `	}` |
|    121 |  372 | `	return pXw;` |
|     61 |  373 | `}` |
|      - |  374 | `/*` |
|      - |  375 | `` * A writer over an engine stream handle. `bOwn` says whether closing the writer`` |
|      - |  376 | ` * closes the handle -- true for openUri()/toUri(), false for the handle` |
|      - |  377 | ` * toStream() borrows from the script.` |
|      - |  378 | ` */` |
|     12 |  379 | `static phl_xmlwriter * XmlWriterOpenDevice(ph7_vm *pVm,io_private *pDev,int bOwn)` |
|      1 |  380 | `{` |
|      - |  381 | `	xmlOutputBufferPtr pOut;` |
|     13 |  382 | `	phl_xmlwriter *pXw = XmlWriterNew(pVm);` |
|     13 |  383 | `	if( pXw == 0 ){` |
|    ! 0 |  384 | `		return 0;` |
|      - |  385 | `	}` |
|     13 |  386 | `	pXw->pDev = pDev;` |
|     13 |  387 | `	pXw->bOwnDev = bOwn;` |
|     13 |  388 | `	pOut = xmlOutputBufferCreateIO(XmlWriterIoWrite,XmlWriterIoClose,(void *)pXw,0);` |
|     13 |  389 | `	if( pOut == 0 ){` |
|      - |  390 | `		/* Nothing will call the close callback, so an owned handle is closed here` |
|      - |  391 | `		 * rather than left open until the VM goes. */` |
|    ! 0 |  392 | `		XmlWriterIoClose((void *)pXw);` |
|    ! 0 |  393 | `		return 0;` |
|      - |  394 | `	}` |
|     13 |  395 | `	pXw->pWriter = xmlNewTextWriter(pOut);` |
|     13 |  396 | `	if( pXw->pWriter == 0 ){` |
|      - |  397 | `		/* xmlNewTextWriter does not take the buffer on failure */` |
|    ! 0 |  398 | `		xmlOutputBufferClose(pOut);` |
|    ! 0 |  399 | `		pXw->pDev = 0;` |
|    ! 0 |  400 | `		return 0;` |
|      - |  401 | `	}` |
|     13 |  402 | `	return pXw;` |
|      7 |  403 | `}` |
|      - |  404 | `/*` |
|      - |  405 | ` * php refuses a NUL inside the two arguments it hands to a C interface that` |
|      - |  406 | ` * would stop at one: the URI, and startDocument()'s encoding NAME. The message` |
|      - |  407 | ` * is ZPP's, so the argument number is this SPELLING's own -- unlike the` |
|      - |  408 | ` * hand-written name check, which always reports the procedural position.` |
|      - |  409 | ` */` |
|     50 |  410 | `static int XmlWriterCheckNul(ph7_context *pCtx,const char *zFn,int iArg,const char *zParam,` |
|      - |  411 | `	ph7_value *pVal)` |
|      1 |  412 | `{` |
|     51 |  413 | `	int nLen = 0;` |
|      - |  414 | `	const char *zStr;` |
|     51 |  415 | `	if( pVal == 0 \|\| ph7_value_is_null(pVal) ){` |
|      5 |  416 | `		return 0;` |
|      - |  417 | `	}` |
|     47 |  418 | `	zStr = ph7_value_to_string(pVal,&nLen);` |
|     47 |  419 | `	if( SyByteFind(zStr,(sxu32)nLen,'\0',0) != SXRET_OK ){` |
|     41 |  420 | `		return 0;` |
|      - |  421 | `	}` |
|     10 |  422 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|      3 |  423 | `		"%s(): Argument #%d ($%s) must not contain any null bytes",zFn,iArg,zParam);` |
|     26 |  424 | `}` |
|      - |  425 | `/*` |
|      - |  426 | ` * Does the FILE path a failed open was given resolve at all -- that is, does the` |
|      - |  427 | `` * directory it names exist? php's `toUri()` factory raises two different things`` |
|      - |  428 | ` * for a path it cannot write, and this is the line between them:` |
|      - |  429 | `` * `expand_filepath()` failing (no such directory) is the ValueError, and`` |
|      - |  430 | ` * everything the OPEN itself refuses -- a directory, an unwritable directory,` |
|      - |  431 | ` * a scheme no wrapper takes -- is the plain Error. A path with no separator is` |
|      - |  432 | ` * relative to a working directory that always exists, so it resolves.` |
|      - |  433 | ` */` |
|      8 |  434 | `static int XmlWriterPathResolves(ph7_vm *pVm,const char *zUri,int nUri)` |
|      1 |  435 | `{` |
|      9 |  436 | `	const ph7_vfs *pVfs = pVm->pEngine ? pVm->pEngine->pVfs : 0;` |
|      - |  437 | `	SyBlob sDir;` |
|      9 |  438 | `	int i,iCut = -1,rc;` |
|      - |  439 | `	/* php reads the path as a URI reference first, and one with a SCHEME skips` |
|      - |  440 | `	 * the directory check entirely -- so the open's own refusal, the plain` |
|      - |  441 | `	 * Error, answers for it. The exceptions are file:/// and file://localhost/,` |
|      - |  442 | `	 * which are stripped to the local path and checked like one. A Windows` |
|      - |  443 | `	 * drive path is a URI with a scheme too: "C:" parses as one. */` |
|     45 |  444 | `	for( i = 0 ; i < nUri ; ++i ){` |
|     45 |  445 | `		int c = (unsigned char)zUri[i];` |
|     45 |  446 | `		if( !(SyisAlpha(c) \|\| (i > 0 && (SyisDigit(c) \|\| c == '+' \|\| c == '-' \|\| c == '.'))) ){` |
|      5 |  447 | `			break;` |
|      - |  448 | `		}` |
|     19 |  449 | `	}` |
|      9 |  450 | `	if( i > 0 && i < nUri && zUri[i] == ':' ){` |
|      5 |  451 | `		if( nUri >= 8 && SyStrnicmp(zUri,"file:///",8) == 0 ){` |
|      3 |  452 | `			zUri += 7;` |
|      3 |  453 | `			nUri -= 7;` |
|      4 |  454 | `		}else if( nUri >= 17 && SyStrnicmp(zUri,"file://localhost/",17) == 0 ){` |
|    ! 0 |  455 | `			zUri += 16;` |
|    ! 0 |  456 | `			nUri -= 16;` |
|    ! 0 |  457 | `		}else{` |
|      3 |  458 | `			return 1;` |
|      - |  459 | `		}` |
|      3 |  460 | `		if( nUri > 2 && SyisAlpha((unsigned char)zUri[1]) && zUri[2] == ':' ){` |
|      1 |  461 | `			zUri++;   /* file:///C:/x names C:/x */` |
|      1 |  462 | `			nUri--;` |
|    ! 0 |  463 | `		}` |
|      1 |  464 | `	}` |
|      7 |  465 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|    ! 0 |  466 | `		return 1;` |
|      - |  467 | `	}` |
|    219 |  468 | `	for( i = 0 ; i < nUri ; ++i ){` |
|    213 |  469 | `		if( zUri[i] == '/' \|\| zUri[i] == '\\' ){` |
|     23 |  470 | `			iCut = i;` |
|     15 |  471 | `		}` |
|    151 |  472 | `	}` |
|      7 |  473 | `	if( iCut < 0 ){` |
|    ! 0 |  474 | `		return 1;   /* a bare name: the working directory */` |
|      - |  475 | `	}` |
|      7 |  476 | `	if( iCut == 0 ){` |
|    ! 0 |  477 | `		return 1;   /* "/name": the root */` |
|      - |  478 | `	}` |
|      7 |  479 | `	SyBlobInit(&sDir,&pVm->sAllocator);` |
|      7 |  480 | `	SyBlobAppend(&sDir,zUri,(sxu32)iCut);` |
|      7 |  481 | `	SyBlobAppend(&sDir,"\0",sizeof(char));` |
|      7 |  482 | `	rc = pVfs->xIsdir((const char *)SyBlobData(&sDir)) == PH7_OK;` |
|      7 |  483 | `	SyBlobRelease(&sDir);` |
|      7 |  484 | `	return rc;` |
|      5 |  485 | `}` |
|      - |  486 | `/*` |
|      - |  487 | ` * Open a URI for writing through the engine's stream layer and answer the` |
|      - |  488 | `` * writer, or 0 with php's own diagnostic already raised. `zFn` names the`` |
|      - |  489 | ` * spelling (method or function) every message is reported under.` |
|      - |  490 | ` */` |
|     36 |  491 | `static phl_xmlwriter * XmlWriterOpenUri(ph7_context *pCtx,ph7_value *pArg,const char *zFn,` |
|      - |  492 | `	int bStatic,int *pRc)` |
|      1 |  493 | `{` |
|     37 |  494 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  495 | `	const ph7_io_stream *pStream;` |
|      - |  496 | `	io_private *pDev;` |
|      - |  497 | `	phl_xmlwriter *pXw;` |
|      - |  498 | `	const char *zUri,*zSrc;` |
|     37 |  499 | `	int nUri = 0,nSrc;` |
|     37 |  500 | `	*pRc = PH7_OK;` |
|     37 |  501 | `	zUri = pArg ? ph7_value_to_string(pArg,&nUri) : "";` |
|     37 |  502 | `	zSrc = zUri;` |
|     37 |  503 | `	nSrc = nUri;` |
|     37 |  504 | `	if( nUri < 1 ){` |
|      7 |  505 | `		*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|      2 |  506 | `			"%s(): Argument #1 ($uri) must not be empty",zFn);` |
|      5 |  507 | `		return 0;` |
|      - |  508 | `	}` |
|     33 |  509 | `	*pRc = XmlWriterCheckNul(pCtx,zFn,1,"uri",pArg);` |
|     33 |  510 | `	if( *pRc != PH7_OK ){` |
|      5 |  511 | `		return 0;` |
|      - |  512 | `	}` |
|     29 |  513 | `	pStream = PH7_VmGetStreamDevice(pVm,&zUri,nUri);` |
|     29 |  514 | `	pDev = pStream ? (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE) : 0;` |
|     29 |  515 | `	if( pDev ){` |
|     25 |  516 | `		InitIOPrivate(pVm,pStream,pDev);` |
|      - |  517 | `		/* php opens the destination "wb" */` |
|     37 |  518 | `		pDev->pHandle = PH7_StreamOpenHandle(pVm,pStream,zUri,` |
|     12 |  519 | `			PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC,FALSE,pArg,FALSE,0,zFn);` |
|     25 |  520 | `		if( pDev->pHandle ){` |
|      9 |  521 | `			int nOrig = 0;` |
|      9 |  522 | `			const char *zOrig = ph7_value_to_string(pArg,&nOrig);` |
|      9 |  523 | `			SetIOPrivateOpenedAs(pDev,zOrig,nOrig,"wb",2);` |
|      5 |  524 | `		}else{` |
|      - |  525 | `			/* Nothing reached PHP, so the shell goes back */` |
|     17 |  526 | `			PH7_StreamReleaseUnopened(pCtx,pDev);` |
|     17 |  527 | `			pDev = 0;` |
|      - |  528 | `		}` |
|     12 |  529 | `	}` |
|     29 |  530 | `	pXw = pDev ? XmlWriterOpenDevice(pVm,pDev,TRUE) : 0;` |
|     29 |  531 | `	if( pXw == 0 ){` |
|      - |  532 | `		/* php's refusals for the same failure: the FACTORY raises -- a ValueError` |
|      - |  533 | `		 * when the path did not resolve, php's plain Error when the open itself` |
|      - |  534 | `		 * was refused -- while the opener warns and answers false. */` |
|     21 |  535 | `		if( bStatic ){` |
|     11 |  536 | `			if( pStream == 0 \|\| XmlWriterPathResolves(pVm,zSrc,nSrc) ){` |
|      7 |  537 | `				*pRc = PH7_VmThrowException(pCtx,"Error","Could not construct libxml writer");` |
|      4 |  538 | `			}else{` |
|      7 |  539 | `				*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|      2 |  540 | `					"%s(): Argument #1 ($uri) must resolve to a valid file path",zFn);` |
|      - |  541 | `			}` |
|      6 |  542 | `		}else{` |
|      - |  543 | `			SyString sFn;` |
|     11 |  544 | `			SyStringInitFromBuf(&sFn,zFn,SyStrlen(zFn));` |
|     11 |  545 | `			PH7_VmThrowError(pVm,&sFn,PH7_CTX_WARNING,"Unable to resolve file path");` |
|      - |  546 | `		}` |
|     21 |  547 | `		return 0;` |
|      - |  548 | `	}` |
|      9 |  549 | `	return pXw;` |
|     19 |  550 | `}` |
|      - |  551 | `/* bool XMLWriter::openMemory() */` |
|    110 |  552 | `static int vm_builtin_xw_open_memory(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  553 | `{` |
|      - |  554 | `	phl_xmlwriter *pXw;` |
|     55 |  555 | `	SXUNUSED(nArg);` |
|     55 |  556 | `	SXUNUSED(apArg);` |
|    111 |  557 | `	pXw = XmlWriterOpenMemory(pCtx->pVm);` |
|    111 |  558 | `	if( pXw == 0 \|\| XmlWriterAttach(PH7_ContextThis(pCtx),pXw) != 0 ){` |
|    ! 0 |  559 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  560 | `		return PH7_OK;` |
|      - |  561 | `	}` |
|    111 |  562 | `	ph7_result_bool(pCtx,1);` |
|    111 |  563 | `	return PH7_OK;` |
|     56 |  564 | `}` |
|      - |  565 | `/* bool XMLWriter::openUri(string $uri) */` |
|     18 |  566 | `static int vm_builtin_xw_open_uri(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  567 | `{` |
|      - |  568 | `	phl_xmlwriter *pXw;` |
|     19 |  569 | `	int rc = PH7_OK;` |
|     19 |  570 | `	pXw = XmlWriterOpenUri(pCtx,nArg > 0 ? apArg[0] : 0,"XMLWriter::openUri",FALSE,&rc);` |
|     19 |  571 | `	if( pXw == 0 ){` |
|     15 |  572 | `		if( rc != PH7_OK ){` |
|      5 |  573 | `			return rc;` |
|      - |  574 | `		}` |
|     11 |  575 | `		ph7_result_bool(pCtx,0);` |
|     11 |  576 | `		return PH7_OK;` |
|      - |  577 | `	}` |
|      5 |  578 | `	if( XmlWriterAttach(PH7_ContextThis(pCtx),pXw) != 0 ){` |
|    ! 0 |  579 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  580 | `		return PH7_OK;` |
|      - |  581 | `	}` |
|      5 |  582 | `	ph7_result_bool(pCtx,1);` |
|      5 |  583 | `	return PH7_OK;` |
|     10 |  584 | `}` |
|      - |  585 | `/*` |
|      - |  586 | ` * The php 8.5 factories. Each answers a NEW writer rather than configuring the` |
|      - |  587 | ` * receiver, and each builds the LATE STATIC class, so a subclass of XMLWriter` |
|      - |  588 | ` * gets one of its own.` |
|      - |  589 | ` */` |
|     18 |  590 | `static int XmlWriterFactory(ph7_context *pCtx,phl_xmlwriter *pXw)` |
|      1 |  591 | `{` |
|     19 |  592 | `	ph7_class *pClass = PH7_ContextCalledClass(pCtx);` |
|      - |  593 | `	ph7_class_instance *pObj;` |
|     19 |  594 | `	if( pClass == 0 ){` |
|      7 |  595 | `		pClass = PH7_VmExtractClass(pCtx->pVm,"XMLWriter",sizeof("XMLWriter")-1,FALSE,0);` |
|      3 |  596 | `	}` |
|     19 |  597 | `	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|     19 |  598 | `	if( pObj == 0 ){` |
|    ! 0 |  599 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  600 | `	}` |
|     19 |  601 | `	if( XmlWriterAttach(pObj,pXw) != 0 ){` |
|    ! 0 |  602 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  603 | `	}` |
|     19 |  604 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     19 |  605 | `	return PH7_OK;` |
|     10 |  606 | `}` |
|      - |  607 | `/* static XMLWriter::toMemory(): static */` |
|      6 |  608 | `static int vm_builtin_xw_to_memory(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  609 | `{` |
|      - |  610 | `	phl_xmlwriter *pXw;` |
|      3 |  611 | `	SXUNUSED(nArg);` |
|      3 |  612 | `	SXUNUSED(apArg);` |
|      7 |  613 | `	pXw = XmlWriterOpenMemory(pCtx->pVm);` |
|      7 |  614 | `	if( pXw == 0 ){` |
|    ! 0 |  615 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  616 | `	}` |
|      7 |  617 | `	return XmlWriterFactory(pCtx,pXw);` |
|      4 |  618 | `}` |
|      - |  619 | `/* static XMLWriter::toUri(string $uri): static */` |
|     14 |  620 | `static int vm_builtin_xw_to_uri(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  621 | `{` |
|     15 |  622 | `	int rc = PH7_OK;` |
|     15 |  623 | `	phl_xmlwriter *pXw = XmlWriterOpenUri(pCtx,nArg > 0 ? apArg[0] : 0,"XMLWriter::toUri",TRUE,&rc);` |
|     15 |  624 | `	if( pXw == 0 ){` |
|     13 |  625 | `		return rc;` |
|      - |  626 | `	}` |
|      3 |  627 | `	return XmlWriterFactory(pCtx,pXw);` |
|      8 |  628 | `}` |
|      - |  629 | `/*` |
|      - |  630 | ` * static XMLWriter::toStream(mixed $stream): static` |
|      - |  631 | ` *` |
|      - |  632 | ` * The handle stays the script's: the writer only pushes bytes at it, and` |
|      - |  633 | ` * closing the writer does not close it. A handle the script fcloses afterwards` |
|      - |  634 | ` * makes every later write fail, which is the answer php's own stream reference` |
|      - |  635 | ` * gives once the resource is gone.` |
|      - |  636 | ` */` |
|      8 |  637 | `static int vm_builtin_xw_to_stream(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  638 | `{` |
|      - |  639 | `	char zGiven[64];` |
|      - |  640 | `	io_private *pDev;` |
|      - |  641 | `	phl_xmlwriter *pXw;` |
|      9 |  642 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      5 |  643 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  644 | `			"XMLWriter::toStream(): Argument #1 ($stream) must be of type resource, %s given",` |
|      2 |  645 | `			nArg > 0 ? VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)) : "null");` |
|      - |  646 | `	}` |
|      7 |  647 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      7 |  648 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      3 |  649 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  650 | `			"XMLWriter::toStream(): supplied resource is not a valid stream resource");` |
|      - |  651 | `	}` |
|      5 |  652 | `	pXw = XmlWriterOpenDevice(pCtx->pVm,pDev,FALSE);` |
|      5 |  653 | `	if( pXw == 0 ){` |
|    ! 0 |  654 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  655 | `	}` |
|      5 |  656 | `	return XmlWriterFactory(pCtx,pXw);` |
|      5 |  657 | `}` |
|      - |  658 | `/*` |
|      - |  659 | ` * Run one verb inside a libxml error capture window, the way php's own` |
|      - |  660 | ` * xmlwriter does: libxml reports a refusal it can explain (a DTD with a public` |
|      - |  661 | ` * identifier and no system one, say) through the error handler rather than the` |
|      - |  662 | ` * return value, and php turns each one into a warning naming the caller.` |
|      - |  663 | ` */` |
|    480 |  664 | `static int XwRun(ph7_context *pCtx,xw_call *pCall,int (*xVerb)(ph7_context *,xw_call *))` |
|      1 |  665 | `{` |
|    481 |  666 | `	sxu32 nMark = PH7_LibxmlCaptureBegin(pCtx->pVm);` |
|      - |  667 | `	int rc;` |
|    481 |  668 | `	pCall->pXw->bIoFailed = 0;` |
|    481 |  669 | `	rc = xVerb(pCtx,pCall);` |
|    481 |  670 | `	if( pCall->pXw->bIoFailed ){` |
|      - |  671 | `		/* A write that could not land is reported by the RETURN value alone:` |
|      - |  672 | `		 * php says nothing when the stream behind a writer has gone away, and` |
|      - |  673 | `		 * libxml's own "I/O error" would be a warning php never raises. */` |
|    ! 0 |  674 | `		pCall->pXw->bIoFailed = 0;` |
|    ! 0 |  675 | `		PH7_LibxmlDropErrors(pCtx->pVm,nMark);` |
|    ! 0 |  676 | `		return rc;` |
|      - |  677 | `	}` |
|    481 |  678 | `	PH7_LibxmlCaptureEnd(pCtx->pVm,nMark,pCall->zFn);` |
|    481 |  679 | `	return rc;` |
|    241 |  680 | `}` |
|      - |  681 | `/* Report a libxml writer call the way php does: its int status as a bool. */` |
|    342 |  682 | `static int XwStatus(ph7_context *pCtx,int rc)` |
|      1 |  683 | `{` |
|    343 |  684 | `	ph7_result_bool(pCtx,rc >= 0);` |
|    343 |  685 | `	return PH7_OK;` |
|      1 |  686 | `}` |
|      - |  687 | `/*` |
|      - |  688 | ` * One method entry point. The receiver has to hold a writer (php's Error), and` |
|      - |  689 | ` * the verb then reads its arguments from #0 up -- the procedural entry, added` |
|      - |  690 | ` * with the rest of that surface, hands it the same descriptor one slot along.` |
|      - |  691 | ` */` |
|      - |  692 | `#define XW_METHOD(cfn,verb,meth,namearg)                                          \` |
|      - |  693 | `static int cfn(ph7_context *pCtx,int nArg,ph7_value **apArg)                      \` |
|      - |  694 | `{                                                                                 \` |
|      - |  695 | `	xw_call sCall;                                                            \` |
|      - |  696 | `	int rc;                                                                   \` |
|      - |  697 | `	SyZero(&sCall,sizeof(sCall));                                             \` |
|      - |  698 | `	rc = XwCallFromThis(pCtx,&sCall,"XMLWriter::" meth,namearg,nArg,apArg);   \` |
|      - |  699 | `	if( rc != 0 ){                                                            \` |
|      - |  700 | `		return rc;                                                        \` |
|      - |  701 | `	}                                                                         \` |
|      - |  702 | `	return XwRun(pCtx,&sCall,verb);                                           \` |
|      - |  703 | `}` |
|      - |  704 |  |
|      - |  705 | `/*` |
|      - |  706 | ` * Build the call descriptor for the FUNCTION spelling: the writer is argument` |
|      - |  707 | ` * #1 and everything else shifts one slot, which is where php's diagnostics get` |
|      - |  708 | ` * their numbering from.` |
|      - |  709 | ` */` |
|     58 |  710 | `static int XwCallFromArg(ph7_context *pCtx,xw_call *pCall,const char *zFn,` |
|      - |  711 | `	const char *zNameArg,int nArg,ph7_value **apArg)` |
|      1 |  712 | `{` |
|     59 |  713 | `	phl_xmlwriter *pXw = nArg > 0 ? XmlWriterOfValue(apArg[0]) : 0;` |
|     59 |  714 | `	if( pXw == 0 \|\| pXw->pWriter == 0 ){` |
|      3 |  715 | `		return XmlWriterMissing(pCtx);` |
|      - |  716 | `	}` |
|     57 |  717 | `	pCall->pXw = pXw;` |
|     57 |  718 | `	pCall->zFn = zFn;` |
|     57 |  719 | `	pCall->zNameArg = zNameArg;` |
|     57 |  720 | `	pCall->iArgBase = 1;` |
|     57 |  721 | `	pCall->nArg = nArg - 1;` |
|     57 |  722 | `	pCall->apArg = apArg + 1;` |
|     57 |  723 | `	return 0;` |
|     30 |  724 | `}` |
|      - |  725 | `/* One procedural entry point: the same verb, one argument along. */` |
|      - |  726 | `#define XW_FUNCTION(cfn,verb,fname,namearg)                                       \` |
|      - |  727 | `static int cfn(ph7_context *pCtx,int nArg,ph7_value **apArg)                      \` |
|      - |  728 | `{                                                                                 \` |
|      - |  729 | `	xw_call sCall;                                                            \` |
|      - |  730 | `	int rc;                                                                   \` |
|      - |  731 | `	SyZero(&sCall,sizeof(sCall));                                             \` |
|      - |  732 | `	rc = XwCallFromArg(pCtx,&sCall,fname,namearg,nArg,apArg);                 \` |
|      - |  733 | `	if( rc != 0 ){                                                            \` |
|      - |  734 | `		return rc;                                                        \` |
|      - |  735 | `	}                                                                         \` |
|      - |  736 | `	return XwRun(pCtx,&sCall,verb);                                           \` |
|      - |  737 | `}` |
|      - |  738 |  |
|      - |  739 | `/* bool XMLWriter::setIndent(bool $enable) */` |
|     10 |  740 | `static int XwSetIndent(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  741 | `{` |
|      - |  742 | `	/* No indent STRING is set here: libxml's own default is the single space` |
|      - |  743 | `	 * php answers with, and writing it back made setIndentString() before` |
|      - |  744 | `	 * setIndent() -- the documented order, and the one php's own examples use --` |
|      - |  745 | `	 * silently lose the string it had just been given. */` |
|     11 |  746 | `	return XwStatus(pCtx,xmlTextWriterSetIndent(pCall->pXw->pWriter,XwBool(pCall,0,0)));` |
|      1 |  747 | `}` |
|     11 |  748 | `XW_METHOD(vm_builtin_xw_set_indent,XwSetIndent,"setIndent",0)` |
|      - |  749 |  |
|      - |  750 | `/* bool XMLWriter::setIndentString(string $indentation) */` |
|      2 |  751 | `static int XwSetIndentString(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  752 | `{` |
|      4 |  753 | `	return XwStatus(pCtx,xmlTextWriterSetIndentString(pCall->pXw->pWriter,` |
|      2 |  754 | `		(const xmlChar *)XwStr(pCall,0)));` |
|      1 |  755 | `}` |
|      3 |  756 | `XW_METHOD(vm_builtin_xw_set_indent_string,XwSetIndentString,"setIndentString",0)` |
|      - |  757 |  |
|      - |  758 | `/*` |
|      - |  759 | ` * bool XMLWriter::startDocument(?string $version, ?string $encoding, ?string $standalone)` |
|      - |  760 | ` *` |
|      - |  761 | ` * The ENCODING is the one argument here php refuses a NUL in: it names a` |
|      - |  762 | ` * character set for libxml to look up, and that lookup stops at the first NUL.` |
|      - |  763 | ` * The version and the standalone flag are written out verbatim and take one.` |
|      - |  764 | ` */` |
|     18 |  765 | `static int XwStartDocument(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  766 | `{` |
|     28 |  767 | `	int rc = XmlWriterCheckNul(pCtx,pCall->zFn,pCall->iArgBase + 2,"encoding",` |
|     18 |  768 | `		pCall->nArg > 1 ? pCall->apArg[1] : 0);` |
|     19 |  769 | `	if( rc != PH7_OK ){` |
|      3 |  770 | `		return rc;` |
|      - |  771 | `	}` |
|     25 |  772 | `	return XwStatus(pCtx,xmlTextWriterStartDocument(pCall->pXw->pWriter,` |
|      8 |  773 | `		XwStrOrNull(pCall,0),XwStrOrNull(pCall,1),XwStrOrNull(pCall,2)));` |
|     10 |  774 | `}` |
|     17 |  775 | `XW_METHOD(vm_builtin_xw_start_document,XwStartDocument,"startDocument",0)` |
|      - |  776 |  |
|      - |  777 | `/* bool XMLWriter::endDocument() */` |
|     12 |  778 | `static int XwEndDocument(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  779 | `{` |
|     13 |  780 | `	return XwStatus(pCtx,xmlTextWriterEndDocument(pCall->pXw->pWriter));` |
|      1 |  781 | `}` |
|     11 |  782 | `XW_METHOD(vm_builtin_xw_end_document,XwEndDocument,"endDocument",0)` |
|      - |  783 |  |
|      - |  784 | `/* bool XMLWriter::startComment() */` |
|      6 |  785 | `static int XwStartComment(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  786 | `{` |
|      7 |  787 | `	return XwStatus(pCtx,xmlTextWriterStartComment(pCall->pXw->pWriter));` |
|      1 |  788 | `}` |
|      7 |  789 | `XW_METHOD(vm_builtin_xw_start_comment,XwStartComment,"startComment",0)` |
|      - |  790 |  |
|      - |  791 | `/* bool XMLWriter::endComment() */` |
|      6 |  792 | `static int XwEndComment(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  793 | `{` |
|      7 |  794 | `	return XwStatus(pCtx,xmlTextWriterEndComment(pCall->pXw->pWriter));` |
|      1 |  795 | `}` |
|      7 |  796 | `XW_METHOD(vm_builtin_xw_end_comment,XwEndComment,"endComment",0)` |
|      - |  797 |  |
|      - |  798 | `/* bool XMLWriter::startAttribute(string $name) */` |
|      8 |  799 | `static int XwStartAttribute(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  800 | `{` |
|      9 |  801 | `	const char *zName = XwStr(pCall,0);` |
|      9 |  802 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"attribute name");` |
|      9 |  803 | `	if( rc != 0 ){` |
|      3 |  804 | `		return rc;` |
|      - |  805 | `	}` |
|      7 |  806 | `	return XwStatus(pCtx,xmlTextWriterStartAttribute(pCall->pXw->pWriter,(const xmlChar *)zName));` |
|      5 |  807 | `}` |
|      7 |  808 | `XW_METHOD(vm_builtin_xw_start_attribute,XwStartAttribute,"startAttribute","#2")` |
|      - |  809 |  |
|      - |  810 | `/* bool XMLWriter::endAttribute() */` |
|      8 |  811 | `static int XwEndAttribute(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  812 | `{` |
|      9 |  813 | `	return XwStatus(pCtx,xmlTextWriterEndAttribute(pCall->pXw->pWriter));` |
|      1 |  814 | `}` |
|      7 |  815 | `XW_METHOD(vm_builtin_xw_end_attribute,XwEndAttribute,"endAttribute",0)` |
|      - |  816 |  |
|      - |  817 | `/*` |
|      - |  818 | ` * bool XMLWriter::startAttributeNs(?string $prefix, string $name, ?string $namespace)` |
|      - |  819 | ` *` |
|      - |  820 | ` * Only the local NAME is validated -- php checks neither the prefix nor the` |
|      - |  821 | `` * namespace, so a prefix with a space in it reaches the document (`<x y:e`),`` |
|      - |  822 | ` * which is libxml's answer and therefore php's.` |
|      - |  823 | ` */` |
|      4 |  824 | `static int XwStartAttributeNs(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  825 | `{` |
|      5 |  826 | `	const char *zName = XwStr(pCall,1);` |
|      5 |  827 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"attribute name");` |
|      5 |  828 | `	if( rc != 0 ){` |
|      3 |  829 | `		return rc;` |
|      - |  830 | `	}` |
|      4 |  831 | `	return XwStatus(pCtx,xmlTextWriterStartAttributeNS(pCall->pXw->pWriter,` |
|      2 |  832 | `		(const xmlChar *)XwStrOrNull(pCall,0),(const xmlChar *)zName,` |
|      2 |  833 | `		(const xmlChar *)XwStrOrNull(pCall,2)));` |
|      3 |  834 | `}` |
|      5 |  835 | `XW_METHOD(vm_builtin_xw_start_attribute_ns,XwStartAttributeNs,"startAttributeNs","#3 ($namespace)")` |
|      - |  836 |  |
|      - |  837 | `/* bool XMLWriter::writeAttributeNs(?string $prefix, string $name, ?string $namespace, string $value) */` |
|      4 |  838 | `static int XwWriteAttributeNs(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  839 | `{` |
|      5 |  840 | `	const char *zName = XwStr(pCall,1);` |
|      5 |  841 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"attribute name");` |
|      5 |  842 | `	if( rc != 0 ){` |
|      3 |  843 | `		return rc;` |
|      - |  844 | `	}` |
|      4 |  845 | `	return XwStatus(pCtx,xmlTextWriterWriteAttributeNS(pCall->pXw->pWriter,` |
|      2 |  846 | `		(const xmlChar *)XwStrOrNull(pCall,0),(const xmlChar *)zName,` |
|      2 |  847 | `		(const xmlChar *)XwStrOrNull(pCall,2),(const xmlChar *)XwStr(pCall,3)));` |
|      3 |  848 | `}` |
|      5 |  849 | `XW_METHOD(vm_builtin_xw_write_attribute_ns,XwWriteAttributeNs,"writeAttributeNs","#3 ($namespace)")` |
|      - |  850 |  |
|      - |  851 | `/* bool XMLWriter::startElement(string $name) */` |
|     46 |  852 | `static int XwStartElement(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  853 | `{` |
|     47 |  854 | `	const char *zName = XwStr(pCall,0);` |
|     47 |  855 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");` |
|     47 |  856 | `	if( rc != 0 ){` |
|      9 |  857 | `		return rc;` |
|      - |  858 | `	}` |
|     39 |  859 | `	return XwStatus(pCtx,xmlTextWriterStartElement(pCall->pXw->pWriter,(const xmlChar *)zName));` |
|     24 |  860 | `}` |
|     45 |  861 | `XW_METHOD(vm_builtin_xw_start_element,XwStartElement,"startElement","#2")` |
|      - |  862 |  |
|      - |  863 | `/* bool XMLWriter::endElement() */` |
|     46 |  864 | `static int XwEndElement(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  865 | `{` |
|     47 |  866 | `	return XwStatus(pCtx,xmlTextWriterEndElement(pCall->pXw->pWriter));` |
|      1 |  867 | `}` |
|     41 |  868 | `XW_METHOD(vm_builtin_xw_end_element,XwEndElement,"endElement",0)` |
|      - |  869 |  |
|      - |  870 | `/* bool XMLWriter::fullEndElement() */` |
|    ! 0 |  871 | `static int XwFullEndElement(ph7_context *pCtx,xw_call *pCall)` |
|    ! 0 |  872 | `{` |
|    ! 0 |  873 | `	return XwStatus(pCtx,xmlTextWriterFullEndElement(pCall->pXw->pWriter));` |
|    ! 0 |  874 | `}` |
|    ! 0 |  875 | `XW_METHOD(vm_builtin_xw_full_end_element,XwFullEndElement,"fullEndElement",0)` |
|      - |  876 |  |
|      - |  877 | `/* bool XMLWriter::writeAttribute(string $name, string $value) */` |
|     16 |  878 | `static int XwWriteAttribute(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  879 | `{` |
|     17 |  880 | `	const char *zName = XwStr(pCall,0);` |
|     17 |  881 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"attribute name");` |
|     17 |  882 | `	if( rc != 0 ){` |
|      3 |  883 | `		return rc;` |
|      - |  884 | `	}` |
|     22 |  885 | `	return XwStatus(pCtx,xmlTextWriterWriteAttribute(pCall->pXw->pWriter,` |
|     14 |  886 | `		(const xmlChar *)zName,(const xmlChar *)XwStr(pCall,1)));` |
|      9 |  887 | `}` |
|     15 |  888 | `XW_METHOD(vm_builtin_xw_write_attribute,XwWriteAttribute,"writeAttribute","#2 ($value)")` |
|      - |  889 |  |
|      - |  890 | `/* bool XMLWriter::writeElement(string $name, ?string $content = null) */` |
|     28 |  891 | `static int XwWriteElement(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  892 | `{` |
|     29 |  893 | `	const char *zName = XwStr(pCall,0);` |
|     29 |  894 | `	const char *zContent = XwStrOrNull(pCall,1);` |
|     29 |  895 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");` |
|     29 |  896 | `	if( rc != 0 ){` |
|      5 |  897 | `		return rc;` |
|      - |  898 | `	}` |
|     25 |  899 | `	if( zContent ){` |
|     37 |  900 | `		rc = xmlTextWriterWriteElement(pCall->pXw->pWriter,(const xmlChar *)zName,` |
|     12 |  901 | `			(const xmlChar *)zContent);` |
|     13 |  902 | `	}else{` |
|      - |  903 | `		/* Empty element: start + end so it serializes as <name/> */` |
|    ! 0 |  904 | `		rc = xmlTextWriterStartElement(pCall->pXw->pWriter,(const xmlChar *)zName);` |
|    ! 0 |  905 | `		if( rc >= 0 ){` |
|    ! 0 |  906 | `			rc = xmlTextWriterEndElement(pCall->pXw->pWriter);` |
|    ! 0 |  907 | `		}` |
|      - |  908 | `	}` |
|     25 |  909 | `	return XwStatus(pCtx,rc);` |
|     15 |  910 | `}` |
|     25 |  911 | `XW_METHOD(vm_builtin_xw_write_element,XwWriteElement,"writeElement","#2 ($content)")` |
|      - |  912 |  |
|      - |  913 | `/*` |
|      - |  914 | ` * bool XMLWriter::startElementNs(?string $prefix, string $name, ?string $namespace)` |
|      - |  915 | ` *` |
|      - |  916 | ` * libxml declares the namespace on the element it opens, so a null $namespace` |
|      - |  917 | ` * writes the prefixed name alone -- which is how a document declares a prefix` |
|      - |  918 | ` * once at the root and uses it below.` |
|      - |  919 | ` */` |
|     20 |  920 | `static int XwStartElementNs(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  921 | `{` |
|     21 |  922 | `	const char *zName = XwStr(pCall,1);` |
|     21 |  923 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");` |
|     21 |  924 | `	if( rc != 0 ){` |
|      5 |  925 | `		return rc;` |
|      - |  926 | `	}` |
|     25 |  927 | `	return XwStatus(pCtx,xmlTextWriterStartElementNS(pCall->pXw->pWriter,` |
|     16 |  928 | `		(const xmlChar *)XwStrOrNull(pCall,0),(const xmlChar *)zName,` |
|     16 |  929 | `		(const xmlChar *)XwStrOrNull(pCall,2)));` |
|     11 |  930 | `}` |
|     17 |  931 | `XW_METHOD(vm_builtin_xw_start_element_ns,XwStartElementNs,"startElementNs","#3 ($namespace)")` |
|      - |  932 |  |
|      - |  933 | `/* bool XMLWriter::writeElementNs(?string $prefix, string $name, ?string $namespace, ?string $content = null) */` |
|      8 |  934 | `static int XwWriteElementNs(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  935 | `{` |
|      9 |  936 | `	const char *zName = XwStr(pCall,1);` |
|      9 |  937 | `	const char *zContent = XwStrOrNull(pCall,3);` |
|      9 |  938 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");` |
|      9 |  939 | `	if( rc != 0 ){` |
|      3 |  940 | `		return rc;` |
|      - |  941 | `	}` |
|      7 |  942 | `	if( zContent ){` |
|      7 |  943 | `		rc = xmlTextWriterWriteElementNS(pCall->pXw->pWriter,` |
|      4 |  944 | `			(const xmlChar *)XwStrOrNull(pCall,0),(const xmlChar *)zName,` |
|      4 |  945 | `			(const xmlChar *)XwStrOrNull(pCall,2),(const xmlChar *)zContent);` |
|      3 |  946 | `	}else{` |
|      - |  947 | ``		/* No content: the empty element php writes, `<p:e xmlns:p="urn"/>` */`` |
|      4 |  948 | `		rc = xmlTextWriterStartElementNS(pCall->pXw->pWriter,` |
|      2 |  949 | `			(const xmlChar *)XwStrOrNull(pCall,0),(const xmlChar *)zName,` |
|      2 |  950 | `			(const xmlChar *)XwStrOrNull(pCall,2));` |
|      3 |  951 | `		if( rc >= 0 ){` |
|      3 |  952 | `			rc = xmlTextWriterEndElement(pCall->pXw->pWriter);` |
|      1 |  953 | `		}` |
|      - |  954 | `	}` |
|      7 |  955 | `	return XwStatus(pCtx,rc);` |
|      5 |  956 | `}` |
|      9 |  957 | `XW_METHOD(vm_builtin_xw_write_element_ns,XwWriteElementNs,"writeElementNs","#3 ($namespace)")` |
|      - |  958 |  |
|      - |  959 | `/*` |
|      - |  960 | ` * bool XMLWriter::startPi(string $target)` |
|      - |  961 | ` *` |
|      - |  962 | ` * The target is checked with the same xmlValidateName the element and attribute` |
|      - |  963 | ` * names go through -- php names it a "PI target" and nothing else changes, so` |
|      - |  964 | `` * `<?php ... ?>` is spellable and `<?x y ... ?>` is a ValueError.`` |
|      - |  965 | ` */` |
|      8 |  966 | `static int XwStartPi(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  967 | `{` |
|      9 |  968 | `	const char *zTarget = XwStr(pCall,0);` |
|      9 |  969 | `	int rc = XmlWriterCheckName(pCtx,pCall,zTarget,"PI target");` |
|      9 |  970 | `	if( rc != 0 ){` |
|      5 |  971 | `		return rc;` |
|      - |  972 | `	}` |
|      5 |  973 | `	return XwStatus(pCtx,xmlTextWriterStartPI(pCall->pXw->pWriter,(const xmlChar *)zTarget));` |
|      5 |  974 | `}` |
|      7 |  975 | `XW_METHOD(vm_builtin_xw_start_pi,XwStartPi,"startPi","#2")` |
|      - |  976 |  |
|      - |  977 | `/* bool XMLWriter::endPi() */` |
|      4 |  978 | `static int XwEndPi(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  979 | `{` |
|      5 |  980 | `	return XwStatus(pCtx,xmlTextWriterEndPI(pCall->pXw->pWriter));` |
|      1 |  981 | `}` |
|      5 |  982 | `XW_METHOD(vm_builtin_xw_end_pi,XwEndPi,"endPi",0)` |
|      - |  983 |  |
|      - |  984 | `/* bool XMLWriter::writePi(string $target, string $content) */` |
|      8 |  985 | `static int XwWritePi(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  986 | `{` |
|      9 |  987 | `	const char *zTarget = XwStr(pCall,0);` |
|      9 |  988 | `	int rc = XmlWriterCheckName(pCtx,pCall,zTarget,"PI target");` |
|      9 |  989 | `	if( rc != 0 ){` |
|      3 |  990 | `		return rc;` |
|      - |  991 | `	}` |
|     10 |  992 | `	return XwStatus(pCtx,xmlTextWriterWritePI(pCall->pXw->pWriter,` |
|      6 |  993 | `		(const xmlChar *)zTarget,(const xmlChar *)XwStr(pCall,1)));` |
|      5 |  994 | `}` |
|      7 |  995 | `XW_METHOD(vm_builtin_xw_write_pi,XwWritePi,"writePi","#2 ($content)")` |
|      - |  996 |  |
|      - |  997 | `/* bool XMLWriter::startCdata() */` |
|      6 |  998 | `static int XwStartCdata(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  999 | `{` |
|      7 | 1000 | `	return XwStatus(pCtx,xmlTextWriterStartCDATA(pCall->pXw->pWriter));` |
|      1 | 1001 | `}` |
|      7 | 1002 | `XW_METHOD(vm_builtin_xw_start_cdata,XwStartCdata,"startCdata",0)` |
|      - | 1003 |  |
|      - | 1004 | `/* bool XMLWriter::endCdata() */` |
|      6 | 1005 | `static int XwEndCdata(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1006 | `{` |
|      7 | 1007 | `	return XwStatus(pCtx,xmlTextWriterEndCDATA(pCall->pXw->pWriter));` |
|      1 | 1008 | `}` |
|      7 | 1009 | `XW_METHOD(vm_builtin_xw_end_cdata,XwEndCdata,"endCdata",0)` |
|      - | 1010 |  |
|      - | 1011 | `/* bool XMLWriter::text(string $content) */` |
|     30 | 1012 | `static int XwText(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1013 | `{` |
|     46 | 1014 | `	return XwStatus(pCtx,xmlTextWriterWriteString(pCall->pXw->pWriter,` |
|     30 | 1015 | `		(const xmlChar *)XwStr(pCall,0)));` |
|      1 | 1016 | `}` |
|     31 | 1017 | `XW_METHOD(vm_builtin_xw_text,XwText,"text",0)` |
|      - | 1018 |  |
|      - | 1019 | `/* bool XMLWriter::writeRaw(string $content) */` |
|    ! 0 | 1020 | `static int XwWriteRaw(ph7_context *pCtx,xw_call *pCall)` |
|    ! 0 | 1021 | `{` |
|    ! 0 | 1022 | `	return XwStatus(pCtx,xmlTextWriterWriteRaw(pCall->pXw->pWriter,` |
|    ! 0 | 1023 | `		(const xmlChar *)XwStr(pCall,0)));` |
|    ! 0 | 1024 | `}` |
|    ! 0 | 1025 | `XW_METHOD(vm_builtin_xw_write_raw,XwWriteRaw,"writeRaw",0)` |
|      - | 1026 |  |
|      - | 1027 | `/* bool XMLWriter::writeCdata(string $content) */` |
|      6 | 1028 | `static int XwWriteCdata(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1029 | `{` |
|     10 | 1030 | `	return XwStatus(pCtx,xmlTextWriterWriteCDATA(pCall->pXw->pWriter,` |
|      6 | 1031 | `		(const xmlChar *)XwStr(pCall,0)));` |
|      1 | 1032 | `}` |
|      5 | 1033 | `XW_METHOD(vm_builtin_xw_write_cdata,XwWriteCdata,"writeCdata",0)` |
|      - | 1034 |  |
|      - | 1035 | `/* bool XMLWriter::writeComment(string $content) */` |
|      4 | 1036 | `static int XwWriteComment(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1037 | `{` |
|      7 | 1038 | `	return XwStatus(pCtx,xmlTextWriterWriteComment(pCall->pXw->pWriter,` |
|      4 | 1039 | `		(const xmlChar *)XwStr(pCall,0)));` |
|      1 | 1040 | `}` |
|      5 | 1041 | `XW_METHOD(vm_builtin_xw_write_comment,XwWriteComment,"writeComment",0)` |
|      - | 1042 |  |
|      - | 1043 | `/*` |
|      - | 1044 | ` * The DTD twelve.` |
|      - | 1045 | ` *` |
|      - | 1046 | ` * libxml decides the shape of every one of these, and two of its decisions are` |
|      - | 1047 | ` * only reported through the error handler: a DOCTYPE with a public identifier` |
|      - | 1048 | ` * and no system one ("system identifier needed!"), and a DTD opened once the` |
|      - | 1049 | ` * root element has been written ("DTD allowed only in prolog!"). Both come out` |
|      - | 1050 | ` * as php's warning through the capture window in XwRun.` |
|      - | 1051 | ` *` |
|      - | 1052 | ` * php validates the name of the internal-subset declarations and NOT the` |
|      - | 1053 | ``  * DOCTYPE's own qualified name, so `startDtd('x y')` writes `<!DOCTYPE x y` `` |
|      - | 1054 | `` * while `startDtdElement('x y')` is a ValueError -- and the ValueError for`` |
|      - | 1055 | ` * startDtdEntity says "attribute name" where its neighbours say "element name",` |
|      - | 1056 | ` * because php reaches for a different macro there.` |
|      - | 1057 | ` */` |
|      - | 1058 | `/* bool XMLWriter::startDtd(string $qualifiedName, ?string $publicId, ?string $systemId) */` |
|     12 | 1059 | `static int XwStartDtd(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1060 | `{` |
|     19 | 1061 | `	return XwStatus(pCtx,xmlTextWriterStartDTD(pCall->pXw->pWriter,` |
|     12 | 1062 | `		(const xmlChar *)XwStr(pCall,0),(const xmlChar *)XwStrOrNull(pCall,1),` |
|     12 | 1063 | `		(const xmlChar *)XwStrOrNull(pCall,2)));` |
|      1 | 1064 | `}` |
|     11 | 1065 | `XW_METHOD(vm_builtin_xw_start_dtd,XwStartDtd,"startDtd",0)` |
|      - | 1066 |  |
|      - | 1067 | `/* bool XMLWriter::endDtd() */` |
|      6 | 1068 | `static int XwEndDtd(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1069 | `{` |
|      7 | 1070 | `	return XwStatus(pCtx,xmlTextWriterEndDTD(pCall->pXw->pWriter));` |
|      1 | 1071 | `}` |
|      5 | 1072 | `XW_METHOD(vm_builtin_xw_end_dtd,XwEndDtd,"endDtd",0)` |
|      - | 1073 |  |
|      - | 1074 | `/* bool XMLWriter::writeDtd(string $name, ?string $publicId, ?string $systemId, ?string $content) */` |
|      2 | 1075 | `static int XwWriteDtd(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1076 | `{` |
|      4 | 1077 | `	return XwStatus(pCtx,xmlTextWriterWriteDTD(pCall->pXw->pWriter,` |
|      2 | 1078 | `		(const xmlChar *)XwStr(pCall,0),(const xmlChar *)XwStrOrNull(pCall,1),` |
|      2 | 1079 | `		(const xmlChar *)XwStrOrNull(pCall,2),(const xmlChar *)XwStrOrNull(pCall,3)));` |
|      1 | 1080 | `}` |
|      3 | 1081 | `XW_METHOD(vm_builtin_xw_write_dtd,XwWriteDtd,"writeDtd",0)` |
|      - | 1082 |  |
|      - | 1083 | `/* bool XMLWriter::startDtdElement(string $qualifiedName) */` |
|      6 | 1084 | `static int XwStartDtdElement(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1085 | `{` |
|      7 | 1086 | `	const char *zName = XwStr(pCall,0);` |
|      7 | 1087 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");` |
|      7 | 1088 | `	if( rc != 0 ){` |
|      5 | 1089 | `		return rc;` |
|      - | 1090 | `	}` |
|      3 | 1091 | `	return XwStatus(pCtx,xmlTextWriterStartDTDElement(pCall->pXw->pWriter,(const xmlChar *)zName));` |
|      4 | 1092 | `}` |
|      5 | 1093 | `XW_METHOD(vm_builtin_xw_start_dtd_element,XwStartDtdElement,"startDtdElement","#2")` |
|      - | 1094 |  |
|      - | 1095 | `/* bool XMLWriter::endDtdElement() */` |
|      4 | 1096 | `static int XwEndDtdElement(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1097 | `{` |
|      5 | 1098 | `	return XwStatus(pCtx,xmlTextWriterEndDTDElement(pCall->pXw->pWriter));` |
|      1 | 1099 | `}` |
|      5 | 1100 | `XW_METHOD(vm_builtin_xw_end_dtd_element,XwEndDtdElement,"endDtdElement",0)` |
|      - | 1101 |  |
|      - | 1102 | `/* bool XMLWriter::writeDtdElement(string $name, string $content) */` |
|      4 | 1103 | `static int XwWriteDtdElement(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1104 | `{` |
|      5 | 1105 | `	const char *zName = XwStr(pCall,0);` |
|      5 | 1106 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");` |
|      5 | 1107 | `	if( rc != 0 ){` |
|      3 | 1108 | `		return rc;` |
|      - | 1109 | `	}` |
|      4 | 1110 | `	return XwStatus(pCtx,xmlTextWriterWriteDTDElement(pCall->pXw->pWriter,` |
|      2 | 1111 | `		(const xmlChar *)zName,(const xmlChar *)XwStr(pCall,1)));` |
|      3 | 1112 | `}` |
|      5 | 1113 | `XW_METHOD(vm_builtin_xw_write_dtd_element,XwWriteDtdElement,"writeDtdElement","#2 ($content)")` |
|      - | 1114 |  |
|      - | 1115 | `/* bool XMLWriter::startDtdAttlist(string $name) */` |
|      4 | 1116 | `static int XwStartDtdAttlist(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1117 | `{` |
|      5 | 1118 | `	const char *zName = XwStr(pCall,0);` |
|      5 | 1119 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");` |
|      5 | 1120 | `	if( rc != 0 ){` |
|      3 | 1121 | `		return rc;` |
|      - | 1122 | `	}` |
|      3 | 1123 | `	return XwStatus(pCtx,xmlTextWriterStartDTDAttlist(pCall->pXw->pWriter,(const xmlChar *)zName));` |
|      3 | 1124 | `}` |
|      5 | 1125 | `XW_METHOD(vm_builtin_xw_start_dtd_attlist,XwStartDtdAttlist,"startDtdAttlist","#2")` |
|      - | 1126 |  |
|      - | 1127 | `/* bool XMLWriter::endDtdAttlist() */` |
|      4 | 1128 | `static int XwEndDtdAttlist(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1129 | `{` |
|      5 | 1130 | `	return XwStatus(pCtx,xmlTextWriterEndDTDAttlist(pCall->pXw->pWriter));` |
|      1 | 1131 | `}` |
|      5 | 1132 | `XW_METHOD(vm_builtin_xw_end_dtd_attlist,XwEndDtdAttlist,"endDtdAttlist",0)` |
|      - | 1133 |  |
|      - | 1134 | `/* bool XMLWriter::writeDtdAttlist(string $name, string $content) */` |
|      4 | 1135 | `static int XwWriteDtdAttlist(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1136 | `{` |
|      5 | 1137 | `	const char *zName = XwStr(pCall,0);` |
|      5 | 1138 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");` |
|      5 | 1139 | `	if( rc != 0 ){` |
|      3 | 1140 | `		return rc;` |
|      - | 1141 | `	}` |
|      4 | 1142 | `	return XwStatus(pCtx,xmlTextWriterWriteDTDAttlist(pCall->pXw->pWriter,` |
|      2 | 1143 | `		(const xmlChar *)zName,(const xmlChar *)XwStr(pCall,1)));` |
|      3 | 1144 | `}` |
|      5 | 1145 | `XW_METHOD(vm_builtin_xw_write_dtd_attlist,XwWriteDtdAttlist,"writeDtdAttlist","#2 ($content)")` |
|      - | 1146 |  |
|      - | 1147 | `/* bool XMLWriter::startDtdEntity(string $name, bool $isParam) */` |
|      6 | 1148 | `static int XwStartDtdEntity(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1149 | `{` |
|      7 | 1150 | `	const char *zName = XwStr(pCall,0);` |
|      7 | 1151 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"attribute name");` |
|      7 | 1152 | `	if( rc != 0 ){` |
|      5 | 1153 | `		return rc;` |
|      - | 1154 | `	}` |
|      4 | 1155 | `	return XwStatus(pCtx,xmlTextWriterStartDTDEntity(pCall->pXw->pWriter,` |
|      1 | 1156 | `		XwBool(pCall,1,0),(const xmlChar *)zName));` |
|      4 | 1157 | `}` |
|      5 | 1158 | `XW_METHOD(vm_builtin_xw_start_dtd_entity,XwStartDtdEntity,"startDtdEntity","#2 ($isParam)")` |
|      - | 1159 |  |
|      - | 1160 | `/* bool XMLWriter::endDtdEntity() */` |
|      4 | 1161 | `static int XwEndDtdEntity(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1162 | `{` |
|      5 | 1163 | `	return XwStatus(pCtx,xmlTextWriterEndDTDEntity(pCall->pXw->pWriter));` |
|      1 | 1164 | `}` |
|      5 | 1165 | `XW_METHOD(vm_builtin_xw_end_dtd_entity,XwEndDtdEntity,"endDtdEntity",0)` |
|      - | 1166 |  |
|      - | 1167 | `/*` |
|      - | 1168 | ` * bool XMLWriter::writeDtdEntity(string $name, string $content, bool $isParam = false,` |
|      - | 1169 | ` *                                ?string $publicId = null, ?string $systemId = null,` |
|      - | 1170 | ` *                                ?string $notationData = null)` |
|      - | 1171 | ` *` |
|      - | 1172 | ` * php has two calls behind this one name, and picks by whether a public or` |
|      - | 1173 | ` * system identifier is there: with neither it writes the INTERNAL entity (the` |
|      - | 1174 | ` * $content), and with either it writes the EXTERNAL declaration -- where` |
|      - | 1175 | ` * $content is not written at all. $notationData does NOT decide, so a call that` |
|      - | 1176 | ` * names only a notation is still the internal entity and the notation is` |
|      - | 1177 | ` * dropped, which is php's answer and not an oversight of this port.` |
|      - | 1178 | ` */` |
|     22 | 1179 | `static int XwWriteDtdEntity(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1180 | `{` |
|     23 | 1181 | `	const char *zName = XwStr(pCall,0);` |
|     23 | 1182 | `	const char *zPub = XwStrOrNull(pCall,3);` |
|     23 | 1183 | `	const char *zSys = XwStrOrNull(pCall,4);` |
|     23 | 1184 | `	const char *zNdata = XwStrOrNull(pCall,5);` |
|     23 | 1185 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");` |
|     23 | 1186 | `	if( rc != 0 ){` |
|      3 | 1187 | `		return rc;` |
|      - | 1188 | `	}` |
|     21 | 1189 | `	if( zPub == 0 && zSys == 0 ){` |
|     19 | 1190 | `		rc = xmlTextWriterWriteDTDInternalEntity(pCall->pXw->pWriter,XwBool(pCall,2,0),` |
|     12 | 1191 | `			(const xmlChar *)zName,(const xmlChar *)XwStr(pCall,1));` |
|      7 | 1192 | `	}else{` |
|     13 | 1193 | `		rc = xmlTextWriterWriteDTDExternalEntity(pCall->pXw->pWriter,XwBool(pCall,2,0),` |
|      4 | 1194 | `			(const xmlChar *)zName,(const xmlChar *)zPub,(const xmlChar *)zSys,` |
|      4 | 1195 | `			(const xmlChar *)zNdata);` |
|      - | 1196 | `	}` |
|     21 | 1197 | `	return XwStatus(pCtx,rc);` |
|     12 | 1198 | `}` |
|     21 | 1199 | `XW_METHOD(vm_builtin_xw_write_dtd_entity,XwWriteDtdEntity,"writeDtdEntity","#2 ($content)")` |
|      - | 1200 |  |
|      - | 1201 | `/* string XMLWriter::outputMemory(bool $flush = true) -- read the buffer back */` |
|     78 | 1202 | `static int XwOutputMemory(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1203 | `{` |
|     79 | 1204 | `	phl_xmlwriter *pXw = pCall->pXw;` |
|     79 | 1205 | `	int bFlush = XwBool(pCall,0,1);` |
|     79 | 1206 | `	if( pXw->pBuf == 0 ){` |
|      - | 1207 | `		/* Not an in-memory writer: php answers the empty string */` |
|      3 | 1208 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1209 | `		return PH7_OK;` |
|      - | 1210 | `	}` |
|      - | 1211 | `	/* Flush the writer into the buffer before reading (php does this) */` |
|     77 | 1212 | `	xmlTextWriterFlush(pXw->pWriter);` |
|     77 | 1213 | `	ph7_result_string(pCtx,(const char *)xmlBufferContent(pXw->pBuf),(int)xmlBufferLength(pXw->pBuf));` |
|     77 | 1214 | `	if( bFlush ){` |
|     75 | 1215 | `		xmlBufferEmpty(pXw->pBuf);` |
|     37 | 1216 | `	}` |
|     77 | 1217 | `	return PH7_OK;` |
|     40 | 1218 | `}` |
|     77 | 1219 | `XW_METHOD(vm_builtin_xw_output_memory,XwOutputMemory,"outputMemory",0)` |
|      - | 1220 |  |
|      - | 1221 | `/* string\|int XMLWriter::flush(bool $empty = true) */` |
|     10 | 1222 | `static int XwFlush(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1223 | `{` |
|     11 | 1224 | `	phl_xmlwriter *pXw = pCall->pXw;` |
|     11 | 1225 | `	int bEmpty = XwBool(pCall,0,1);` |
|     11 | 1226 | `	int nOut = xmlTextWriterFlush(pXw->pWriter);` |
|     11 | 1227 | `	if( pXw->pBuf ){` |
|      - | 1228 | `		/* Memory writer: php returns the buffer as a string from flush() */` |
|    ! 0 | 1229 | `		ph7_result_string(pCtx,(const char *)xmlBufferContent(pXw->pBuf),(int)xmlBufferLength(pXw->pBuf));` |
|    ! 0 | 1230 | `		if( bEmpty ){` |
|    ! 0 | 1231 | `			xmlBufferEmpty(pXw->pBuf);` |
|    ! 0 | 1232 | `		}` |
|    ! 0 | 1233 | `	}else{` |
|     11 | 1234 | `		ph7_result_int(pCtx,nOut);` |
|      - | 1235 | `	}` |
|     11 | 1236 | `	return PH7_OK;` |
|      1 | 1237 | `}` |
|     11 | 1238 | `XW_METHOD(vm_builtin_xw_flush,XwFlush,"flush",0)` |
|      - | 1239 |  |
|      - | 1240 | `/*` |
|      - | 1241 | ` * The procedural surface. php's ext/xmlwriter presents every verb twice, and` |
|      - | 1242 | ` * the function spelling is the ORIGINAL one -- the class arrived in 5.1.2 --` |
|      - | 1243 | ` * so a program written against it is not using an alias for the method but the` |
|      - | 1244 | ` * name the extension was documented under. Each entry drives the same verb one` |
|      - | 1245 | ` * argument along, and states its OWN name-argument text: procedural numbering` |
|      - | 1246 | ` * is what php's macro reports, so here it names the real parameter.` |
|      - | 1247 | ` */` |
|      3 | 1248 | `XW_FUNCTION(vm_builtin_xmlwriter_set_indent,XwSetIndent,"xmlwriter_set_indent",0)` |
|    ! 0 | 1249 | `XW_FUNCTION(vm_builtin_xmlwriter_set_indent_string,XwSetIndentString,"xmlwriter_set_indent_string",0)` |
|    ! 0 | 1250 | `XW_FUNCTION(vm_builtin_xmlwriter_start_comment,XwStartComment,"xmlwriter_start_comment",0)` |
|    ! 0 | 1251 | `XW_FUNCTION(vm_builtin_xmlwriter_end_comment,XwEndComment,"xmlwriter_end_comment",0)` |
|      3 | 1252 | `XW_FUNCTION(vm_builtin_xmlwriter_start_attribute,XwStartAttribute,"xmlwriter_start_attribute","#2 ($name)")` |
|      3 | 1253 | `XW_FUNCTION(vm_builtin_xmlwriter_end_attribute,XwEndAttribute,"xmlwriter_end_attribute",0)` |
|      3 | 1254 | `XW_FUNCTION(vm_builtin_xmlwriter_write_attribute,XwWriteAttribute,"xmlwriter_write_attribute","#2 ($name)")` |
|    ! 0 | 1255 | `XW_FUNCTION(vm_builtin_xmlwriter_start_attribute_ns,XwStartAttributeNs,"xmlwriter_start_attribute_ns","#3 ($name)")` |
|    ! 0 | 1256 | `XW_FUNCTION(vm_builtin_xmlwriter_write_attribute_ns,XwWriteAttributeNs,"xmlwriter_write_attribute_ns","#3 ($name)")` |
|      7 | 1257 | `XW_FUNCTION(vm_builtin_xmlwriter_start_element,XwStartElement,"xmlwriter_start_element","#2 ($name)")` |
|      7 | 1258 | `XW_FUNCTION(vm_builtin_xmlwriter_end_element,XwEndElement,"xmlwriter_end_element",0)` |
|    ! 0 | 1259 | `XW_FUNCTION(vm_builtin_xmlwriter_full_end_element,XwFullEndElement,"xmlwriter_full_end_element",0)` |
|      5 | 1260 | `XW_FUNCTION(vm_builtin_xmlwriter_start_element_ns,XwStartElementNs,"xmlwriter_start_element_ns","#3 ($name)")` |
|      7 | 1261 | `XW_FUNCTION(vm_builtin_xmlwriter_write_element,XwWriteElement,"xmlwriter_write_element","#2 ($name)")` |
|    ! 0 | 1262 | `XW_FUNCTION(vm_builtin_xmlwriter_write_element_ns,XwWriteElementNs,"xmlwriter_write_element_ns","#3 ($name)")` |
|      3 | 1263 | `XW_FUNCTION(vm_builtin_xmlwriter_start_pi,XwStartPi,"xmlwriter_start_pi","#2 ($target)")` |
|    ! 0 | 1264 | `XW_FUNCTION(vm_builtin_xmlwriter_end_pi,XwEndPi,"xmlwriter_end_pi",0)` |
|      3 | 1265 | `XW_FUNCTION(vm_builtin_xmlwriter_write_pi,XwWritePi,"xmlwriter_write_pi","#2 ($target)")` |
|    ! 0 | 1266 | `XW_FUNCTION(vm_builtin_xmlwriter_start_cdata,XwStartCdata,"xmlwriter_start_cdata",0)` |
|    ! 0 | 1267 | `XW_FUNCTION(vm_builtin_xmlwriter_end_cdata,XwEndCdata,"xmlwriter_end_cdata",0)` |
|      3 | 1268 | `XW_FUNCTION(vm_builtin_xmlwriter_write_cdata,XwWriteCdata,"xmlwriter_write_cdata",0)` |
|      3 | 1269 | `XW_FUNCTION(vm_builtin_xmlwriter_text,XwText,"xmlwriter_text",0)` |
|    ! 0 | 1270 | `XW_FUNCTION(vm_builtin_xmlwriter_write_raw,XwWriteRaw,"xmlwriter_write_raw",0)` |
|      3 | 1271 | `XW_FUNCTION(vm_builtin_xmlwriter_start_document,XwStartDocument,"xmlwriter_start_document",0)` |
|      3 | 1272 | `XW_FUNCTION(vm_builtin_xmlwriter_end_document,XwEndDocument,"xmlwriter_end_document",0)` |
|    ! 0 | 1273 | `XW_FUNCTION(vm_builtin_xmlwriter_write_comment,XwWriteComment,"xmlwriter_write_comment",0)` |
|      3 | 1274 | `XW_FUNCTION(vm_builtin_xmlwriter_start_dtd,XwStartDtd,"xmlwriter_start_dtd",0)` |
|      3 | 1275 | `XW_FUNCTION(vm_builtin_xmlwriter_end_dtd,XwEndDtd,"xmlwriter_end_dtd",0)` |
|    ! 0 | 1276 | `XW_FUNCTION(vm_builtin_xmlwriter_write_dtd,XwWriteDtd,"xmlwriter_write_dtd",0)` |
|      3 | 1277 | `XW_FUNCTION(vm_builtin_xmlwriter_start_dtd_element,XwStartDtdElement,"xmlwriter_start_dtd_element","#2 ($qualifiedName)")` |
|    ! 0 | 1278 | `XW_FUNCTION(vm_builtin_xmlwriter_end_dtd_element,XwEndDtdElement,"xmlwriter_end_dtd_element",0)` |
|    ! 0 | 1279 | `XW_FUNCTION(vm_builtin_xmlwriter_write_dtd_element,XwWriteDtdElement,"xmlwriter_write_dtd_element","#2 ($name)")` |
|    ! 0 | 1280 | `XW_FUNCTION(vm_builtin_xmlwriter_start_dtd_attlist,XwStartDtdAttlist,"xmlwriter_start_dtd_attlist","#2 ($name)")` |
|    ! 0 | 1281 | `XW_FUNCTION(vm_builtin_xmlwriter_end_dtd_attlist,XwEndDtdAttlist,"xmlwriter_end_dtd_attlist",0)` |
|    ! 0 | 1282 | `XW_FUNCTION(vm_builtin_xmlwriter_write_dtd_attlist,XwWriteDtdAttlist,"xmlwriter_write_dtd_attlist","#2 ($name)")` |
|      3 | 1283 | `XW_FUNCTION(vm_builtin_xmlwriter_start_dtd_entity,XwStartDtdEntity,"xmlwriter_start_dtd_entity","#2 ($name)")` |
|    ! 0 | 1284 | `XW_FUNCTION(vm_builtin_xmlwriter_end_dtd_entity,XwEndDtdEntity,"xmlwriter_end_dtd_entity",0)` |
|      3 | 1285 | `XW_FUNCTION(vm_builtin_xmlwriter_write_dtd_entity,XwWriteDtdEntity,"xmlwriter_write_dtd_entity","#2 ($name)")` |
|      5 | 1286 | `XW_FUNCTION(vm_builtin_xmlwriter_output_memory,XwOutputMemory,"xmlwriter_output_memory",0)` |
|      3 | 1287 | `XW_FUNCTION(vm_builtin_xmlwriter_flush,XwFlush,"xmlwriter_flush",0)` |
|      - | 1288 |  |
|      - | 1289 | `/* XMLWriter\|false xmlwriter_open_memory() */` |
|      4 | 1290 | `static int vm_builtin_xmlwriter_open_memory(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1291 | `{` |
|      - | 1292 | `	phl_xmlwriter *pXw;` |
|      2 | 1293 | `	SXUNUSED(nArg);` |
|      2 | 1294 | `	SXUNUSED(apArg);` |
|      5 | 1295 | `	pXw = XmlWriterOpenMemory(pCtx->pVm);` |
|      5 | 1296 | `	if( pXw == 0 ){` |
|    ! 0 | 1297 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1298 | `		return PH7_OK;` |
|      - | 1299 | `	}` |
|      5 | 1300 | `	return XmlWriterFactory(pCtx,pXw);` |
|      3 | 1301 | `}` |
|      - | 1302 | `/* XMLWriter\|false xmlwriter_open_uri(string $uri) */` |
|      4 | 1303 | `static int vm_builtin_xmlwriter_open_uri(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1304 | `{` |
|      5 | 1305 | `	int rc = PH7_OK;` |
|      5 | 1306 | `	phl_xmlwriter *pXw = XmlWriterOpenUri(pCtx,nArg > 0 ? apArg[0] : 0,"xmlwriter_open_uri",FALSE,&rc);` |
|      5 | 1307 | `	if( pXw == 0 ){` |
|      3 | 1308 | `		if( rc != PH7_OK ){` |
|      3 | 1309 | `			return rc;` |
|      - | 1310 | `		}` |
|    ! 0 | 1311 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1312 | `		return PH7_OK;` |
|      - | 1313 | `	}` |
|      3 | 1314 | `	return XmlWriterFactory(pCtx,pXw);` |
|      3 | 1315 | `}` |
|      - | 1316 |  |
|      - | 1317 | `/* XMLWriter is declared entirely from C by PH7_VmInstallXmlWriter below. It was` |
|      - | 1318 | ` * an embedded PHP class whose every method forwarded to a global __xw_ thunk. */` |
|      - | 1319 |  |
|      - | 1320 | `/*` |
|      - | 1321 | ` * Install the XMLWriter library.  Called from PH7_VmInit inside the` |
|      - | 1322 | ` * bCompilingBuiltin window, after PH7_VmInstallLibxml.` |
|      - | 1323 | ` */` |
|   8445 | 1324 | `PH7_PRIVATE sxi32 PH7_VmInstallXmlWriter(ph7_vm *pVm)` |
|      5 | 1325 | `{` |
|      - | 1326 | `	/* php's own signatures. Declaring them is what gives these methods argument` |
|      - | 1327 | `	 * coercion and a too-few/too-many ArgumentCountError; the prelude hand-cast` |
|      - | 1328 | `	 * every argument ((string)$name, (bool)$enable) and enforced no arity at all. */` |
|      - | 1329 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|      - | 1330 | `		/* Declared in php's own stub order: get_class_methods() and Reflection` |
|      - | 1331 | `		 * both answer declaration order, so the two engines list one surface. */` |
|      - | 1332 | `		{ "openUri",         PH7_MOD_PUBLIC, "string $uri", "@bool", vm_builtin_xw_open_uri },` |
|      - | 1333 | `		{ "toUri",           PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string $uri", "static",` |
|      - | 1334 | `		  vm_builtin_xw_to_uri },` |
|      - | 1335 | `		{ "openMemory",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_open_memory },` |
|      - | 1336 | `		{ "toMemory",        PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "", "static", vm_builtin_xw_to_memory },` |
|      - | 1337 | ``		/* `$stream` carries no declared type: php's stub writes one, but its arginfo`` |
|      - | 1338 | `		 * is the legacy untyped form and ReflectionParameter reports none. */` |
|      - | 1339 | `		{ "toStream",        PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "$stream", "static",` |
|      - | 1340 | `		  vm_builtin_xw_to_stream },` |
|      - | 1341 | `		{ "setIndent",       PH7_MOD_PUBLIC, "bool $enable", "@bool", vm_builtin_xw_set_indent },` |
|      - | 1342 | `		{ "setIndentString", PH7_MOD_PUBLIC, "string $indentation", "@bool", vm_builtin_xw_set_indent_string },` |
|      - | 1343 | `		{ "startComment",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_start_comment },` |
|      - | 1344 | `		{ "endComment",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_comment },` |
|      - | 1345 | `		{ "startAttribute",  PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_xw_start_attribute },` |
|      - | 1346 | `		{ "endAttribute",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_attribute },` |
|      - | 1347 | `		{ "writeAttribute",  PH7_MOD_PUBLIC, "string $name, string $value", "@bool", vm_builtin_xw_write_attribute },` |
|      - | 1348 | `		{ "startAttributeNs", PH7_MOD_PUBLIC, "?string $prefix, string $name, ?string $namespace",` |
|      - | 1349 | `		  "@bool", vm_builtin_xw_start_attribute_ns },` |
|      - | 1350 | `		{ "writeAttributeNs", PH7_MOD_PUBLIC,` |
|      - | 1351 | `		  "?string $prefix, string $name, ?string $namespace, string $value",` |
|      - | 1352 | `		  "@bool", vm_builtin_xw_write_attribute_ns },` |
|      - | 1353 | `		{ "startElement",    PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_xw_start_element },` |
|      - | 1354 | `		{ "endElement",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_element },` |
|      - | 1355 | `		{ "fullEndElement",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_full_end_element },` |
|      - | 1356 | `		{ "startElementNs",  PH7_MOD_PUBLIC, "?string $prefix, string $name, ?string $namespace",` |
|      - | 1357 | `		  "@bool", vm_builtin_xw_start_element_ns },` |
|      - | 1358 | `		{ "writeElement",    PH7_MOD_PUBLIC, "string $name, ?string $content = null", "@bool", vm_builtin_xw_write_element },` |
|      - | 1359 | `		{ "writeElementNs",  PH7_MOD_PUBLIC,` |
|      - | 1360 | `		  "?string $prefix, string $name, ?string $namespace, ?string $content = null",` |
|      - | 1361 | `		  "@bool", vm_builtin_xw_write_element_ns },` |
|      - | 1362 | `		{ "startPi",         PH7_MOD_PUBLIC, "string $target", "@bool", vm_builtin_xw_start_pi },` |
|      - | 1363 | `		{ "endPi",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_pi },` |
|      - | 1364 | `		{ "writePi",         PH7_MOD_PUBLIC, "string $target, string $content", "@bool", vm_builtin_xw_write_pi },` |
|      - | 1365 | `		{ "startCdata",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_start_cdata },` |
|      - | 1366 | `		{ "endCdata",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_cdata },` |
|      - | 1367 | `		{ "writeCdata",      PH7_MOD_PUBLIC, "string $content", "@bool", vm_builtin_xw_write_cdata },` |
|      - | 1368 | `		{ "text",            PH7_MOD_PUBLIC, "string $content", "@bool", vm_builtin_xw_text },` |
|      - | 1369 | `		{ "writeRaw",        PH7_MOD_PUBLIC, "string $content", "@bool", vm_builtin_xw_write_raw },` |
|      - | 1370 | `		{ "startDocument",   PH7_MOD_PUBLIC,` |
|      - | 1371 | `		  "?string $version = '1.0', ?string $encoding = null, ?string $standalone = null",` |
|      - | 1372 | `		  "@bool", vm_builtin_xw_start_document },` |
|      - | 1373 | `		{ "endDocument",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_document },` |
|      - | 1374 | `		{ "writeComment",    PH7_MOD_PUBLIC, "string $content", "@bool", vm_builtin_xw_write_comment },` |
|      - | 1375 | `		{ "startDtd",        PH7_MOD_PUBLIC,` |
|      - | 1376 | `		  "string $qualifiedName, ?string $publicId = null, ?string $systemId = null",` |
|      - | 1377 | `		  "@bool", vm_builtin_xw_start_dtd },` |
|      - | 1378 | `		{ "endDtd",          PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_dtd },` |
|      - | 1379 | `		{ "writeDtd",        PH7_MOD_PUBLIC,` |
|      - | 1380 | `		  "string $name, ?string $publicId = null, ?string $systemId = null, ?string $content = null",` |
|      - | 1381 | `		  "@bool", vm_builtin_xw_write_dtd },` |
|      - | 1382 | `		{ "startDtdElement", PH7_MOD_PUBLIC, "string $qualifiedName", "@bool", vm_builtin_xw_start_dtd_element },` |
|      - | 1383 | `		{ "endDtdElement",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_dtd_element },` |
|      - | 1384 | `		{ "writeDtdElement", PH7_MOD_PUBLIC, "string $name, string $content", "@bool",` |
|      - | 1385 | `		  vm_builtin_xw_write_dtd_element },` |
|      - | 1386 | `		{ "startDtdAttlist", PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_xw_start_dtd_attlist },` |
|      - | 1387 | `		{ "endDtdAttlist",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_dtd_attlist },` |
|      - | 1388 | `		{ "writeDtdAttlist", PH7_MOD_PUBLIC, "string $name, string $content", "@bool",` |
|      - | 1389 | `		  vm_builtin_xw_write_dtd_attlist },` |
|      - | 1390 | `		{ "startDtdEntity",  PH7_MOD_PUBLIC, "string $name, bool $isParam", "@bool",` |
|      - | 1391 | `		  vm_builtin_xw_start_dtd_entity },` |
|      - | 1392 | `		{ "endDtdEntity",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_dtd_entity },` |
|      - | 1393 | `		{ "writeDtdEntity",  PH7_MOD_PUBLIC,` |
|      - | 1394 | `		  "string $name, string $content, bool $isParam = false, ?string $publicId = null, "` |
|      - | 1395 | `		  "?string $systemId = null, ?string $notationData = null",` |
|      - | 1396 | `		  "@bool", vm_builtin_xw_write_dtd_entity },` |
|      - | 1397 | `		{ "outputMemory",    PH7_MOD_PUBLIC, "bool $flush = true", "@string", vm_builtin_xw_output_memory },` |
|      - | 1398 | `		{ "flush",           PH7_MOD_PUBLIC, "bool $empty = true", "@string\|int", vm_builtin_xw_flush },` |
|      - | 1399 | `	};` |
|      - | 1400 | `	/* The libxml writer handle: storage the class owns, kept public because the` |
|      - | 1401 | `	 * prelude declared it so. */` |
|      - | 1402 | `	static const PH7_NativePropDef aProp[] = {` |
|      - | 1403 | `		{ "__res", PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 1404 | `	};` |
|      - | 1405 | ``	/* php's XMLWriter has no clone handler, so `clone $w` is the engine's own`` |
|      - | 1406 | `	 * refusal there -- and it has to be one here too: the copy would carry the` |
|      - | 1407 | `	 * SAME libxml writer in its hidden slot, so the two objects would interleave` |
|      - | 1408 | `	 * their output into one document and the copy's own buffer would answer "". */` |
|      - | 1409 | `	/* php's ext/xmlwriter presents every verb under a function name too; each` |
|      - | 1410 | `	 * one drives the very same body through XwCallFromArg. */` |
|      - | 1411 | `	static const struct {` |
|      - | 1412 | `		const char *zName;` |
|      - | 1413 | `		ProchHostFunction xFunc;` |
|      - | 1414 | `	} aFunc[] = {` |
|      - | 1415 | `		{ "xmlwriter_open_uri",    vm_builtin_xmlwriter_open_uri },` |
|      - | 1416 | `		{ "xmlwriter_open_memory", vm_builtin_xmlwriter_open_memory },` |
|      - | 1417 | `		{ "xmlwriter_set_indent", vm_builtin_xmlwriter_set_indent },` |
|      - | 1418 | `		{ "xmlwriter_set_indent_string", vm_builtin_xmlwriter_set_indent_string },` |
|      - | 1419 | `		{ "xmlwriter_start_comment", vm_builtin_xmlwriter_start_comment },` |
|      - | 1420 | `		{ "xmlwriter_end_comment", vm_builtin_xmlwriter_end_comment },` |
|      - | 1421 | `		{ "xmlwriter_start_attribute", vm_builtin_xmlwriter_start_attribute },` |
|      - | 1422 | `		{ "xmlwriter_end_attribute", vm_builtin_xmlwriter_end_attribute },` |
|      - | 1423 | `		{ "xmlwriter_write_attribute", vm_builtin_xmlwriter_write_attribute },` |
|      - | 1424 | `		{ "xmlwriter_start_attribute_ns", vm_builtin_xmlwriter_start_attribute_ns },` |
|      - | 1425 | `		{ "xmlwriter_write_attribute_ns", vm_builtin_xmlwriter_write_attribute_ns },` |
|      - | 1426 | `		{ "xmlwriter_start_element", vm_builtin_xmlwriter_start_element },` |
|      - | 1427 | `		{ "xmlwriter_end_element", vm_builtin_xmlwriter_end_element },` |
|      - | 1428 | `		{ "xmlwriter_full_end_element", vm_builtin_xmlwriter_full_end_element },` |
|      - | 1429 | `		{ "xmlwriter_start_element_ns", vm_builtin_xmlwriter_start_element_ns },` |
|      - | 1430 | `		{ "xmlwriter_write_element", vm_builtin_xmlwriter_write_element },` |
|      - | 1431 | `		{ "xmlwriter_write_element_ns", vm_builtin_xmlwriter_write_element_ns },` |
|      - | 1432 | `		{ "xmlwriter_start_pi", vm_builtin_xmlwriter_start_pi },` |
|      - | 1433 | `		{ "xmlwriter_end_pi", vm_builtin_xmlwriter_end_pi },` |
|      - | 1434 | `		{ "xmlwriter_write_pi", vm_builtin_xmlwriter_write_pi },` |
|      - | 1435 | `		{ "xmlwriter_start_cdata", vm_builtin_xmlwriter_start_cdata },` |
|      - | 1436 | `		{ "xmlwriter_end_cdata", vm_builtin_xmlwriter_end_cdata },` |
|      - | 1437 | `		{ "xmlwriter_write_cdata", vm_builtin_xmlwriter_write_cdata },` |
|      - | 1438 | `		{ "xmlwriter_text", vm_builtin_xmlwriter_text },` |
|      - | 1439 | `		{ "xmlwriter_write_raw", vm_builtin_xmlwriter_write_raw },` |
|      - | 1440 | `		{ "xmlwriter_start_document", vm_builtin_xmlwriter_start_document },` |
|      - | 1441 | `		{ "xmlwriter_end_document", vm_builtin_xmlwriter_end_document },` |
|      - | 1442 | `		{ "xmlwriter_write_comment", vm_builtin_xmlwriter_write_comment },` |
|      - | 1443 | `		{ "xmlwriter_start_dtd", vm_builtin_xmlwriter_start_dtd },` |
|      - | 1444 | `		{ "xmlwriter_end_dtd", vm_builtin_xmlwriter_end_dtd },` |
|      - | 1445 | `		{ "xmlwriter_write_dtd", vm_builtin_xmlwriter_write_dtd },` |
|      - | 1446 | `		{ "xmlwriter_start_dtd_element", vm_builtin_xmlwriter_start_dtd_element },` |
|      - | 1447 | `		{ "xmlwriter_end_dtd_element", vm_builtin_xmlwriter_end_dtd_element },` |
|      - | 1448 | `		{ "xmlwriter_write_dtd_element", vm_builtin_xmlwriter_write_dtd_element },` |
|      - | 1449 | `		{ "xmlwriter_start_dtd_attlist", vm_builtin_xmlwriter_start_dtd_attlist },` |
|      - | 1450 | `		{ "xmlwriter_end_dtd_attlist", vm_builtin_xmlwriter_end_dtd_attlist },` |
|      - | 1451 | `		{ "xmlwriter_write_dtd_attlist", vm_builtin_xmlwriter_write_dtd_attlist },` |
|      - | 1452 | `		{ "xmlwriter_start_dtd_entity", vm_builtin_xmlwriter_start_dtd_entity },` |
|      - | 1453 | `		{ "xmlwriter_end_dtd_entity", vm_builtin_xmlwriter_end_dtd_entity },` |
|      - | 1454 | `		{ "xmlwriter_write_dtd_entity", vm_builtin_xmlwriter_write_dtd_entity },` |
|      - | 1455 | `		{ "xmlwriter_output_memory", vm_builtin_xmlwriter_output_memory },` |
|      - | 1456 | `		{ "xmlwriter_flush", vm_builtin_xmlwriter_flush },` |
|      - | 1457 | `	};` |
|      - | 1458 | `	sxu32 n;` |
|      - | 1459 | `	static const PH7_NativeClassSpec sSpec = {` |
|      - | 1460 | `		"XMLWriter", 0, 0, PH7_CLASS_NOCLONE,` |
|      - | 1461 | `		aMethod, SX_ARRAYSIZE(aMethod),` |
|      - | 1462 | `		0, 0,` |
|      - | 1463 | `		aProp, SX_ARRAYSIZE(aProp),` |
|      - | 1464 | `		XmlWriterInstanceRelease, 0, 0` |
|      - | 1465 | `	};` |
| 363140 | 1466 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
| 354695 | 1467 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
| 177119 | 1468 | `	}` |
|   8450 | 1469 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|      5 | 1470 | `}` |
|      - | 1471 |  |
|      - | 1472 | `#else` |
|      - | 1473 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|      - | 1474 | `typedef int vm_xmlwriter_unused;` |
|      - | 1475 | `#endif /* PH7_ENABLE_LIBXML */` |
|      - | 1476 |  |
