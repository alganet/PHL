# src/ph7/vm_xml.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1090/1185 lines (91.98%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    4 | ` */` |
|      - |    5 | `#ifdef PH7_ENABLE_LIBXML` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/*` |
|      - |    8 | ` * libxml2 2.14 marks most xmlParserCtxt members deprecated, among them the` |
|      - |    9 | ` * seven this port reads or writes exactly where php's compat.c does` |
|      - |   10 | ` * (instate, loadsubset, validate, pedantic, linenumbers, keepBlanks,` |
|      - |   11 | ` * replaceEntities). Each access is then a -Wdeprecated-declarations, which` |
|      - |   12 | ` * -Werror turns into a failed build on any toolchain with a current libxml` |
|      - |   13 | ` * (Homebrew's). php silences the same warning around the same accesses.` |
|      - |   14 | ` * Blanking the attribute through libxml's own #ifndef guard does that for` |
|      - |   15 | ` * every compiler at once: a GCC pragma would be an unknown pragma to MSVC` |
|      - |   16 | ` * under /WX, where libxml already defines the macro empty.` |
|      - |   17 | ` */` |
|      - |   18 | `#define XML_DEPRECATED_MEMBER` |
|      - |   19 | `#include <libxml/parser.h>` |
|      - |   20 | `#include <libxml/parserInternals.h>` |
|      - |   21 | `#include <libxml/entities.h>` |
|      - |   22 | `#include <libxml/dict.h>` |
|      - |   23 |  |
|      - |   24 | `/*` |
|      - |   25 | ` * php's ext/xml: the expat-style PUSH parser (xml_parser_create, the handler` |
|      - |   26 | ` * setters, xml_parse) over libxml2.` |
|      - |   27 | ` *` |
|      - |   28 | ` * php has not linked real expat for two decades: its ext/xml is a COMPAT` |
|      - |   29 | ` * layer (ext/xml/compat.c) that answers the expat API from libxml2's push` |
|      - |   30 | ` * parser, plus the PHP-facing half (ext/xml/xml.c) that folds case, converts` |
|      - |   31 | ` * encodings and builds xml_parse_into_struct()'s arrays. This unit is a port` |
|      - |   32 | ` * of that PAIR into one file, because the split exists only to imitate` |
|      - |   33 | ` * expat's header. Every routing rule below that looks arbitrary is php's,` |
|      - |   34 | ` * verified against the php 8.5 oracle:` |
|      - |   35 | ` *` |
|      - |   36 | ` *  - the parser runs with XML_PARSE_OLDSAX\|XML_PARSE_NOENT after zeroing the` |
|      - |   37 | ` *    ctxt options (php_libxml_sanitize_parse_ctxt_options), so ATTRIBUTE` |
|      - |   38 | ` *    values arrive entity-expanded while CONTENT entity references produce` |
|      - |   39 | ` *    no SAX events at all -- what a reference becomes is decided entirely` |
|      - |   40 | ` *    inside the getEntity hook (XmlSaxGetEntity below);` |
|      - |   41 | ` *  - a tag/PI/comment with NO handler for it is rebuilt as raw text for the` |
|      - |   42 | ` *    DEFAULT handler -- start tags by scanning the parser's input buffer` |
|      - |   43 | ` *    BACKWARD to the nearest '<', which is also why the raw text shows the` |
|      - |   44 | ` *    original entity spelling php's own SAX layer never sees;` |
|      - |   45 | ` *  - each xml_set_*_handler() call INSTALLS its libxml-level route once and` |
|      - |   46 | ` *    forever: setting a handler and then unsetting it (null) leaves the` |
|      - |   47 | ` *    route claimed, so those events are swallowed rather than falling back` |
|      - |   48 | ` *    to the default handler. bInstalled mirrors php's compat-level handler` |
|      - |   49 | ` *    pointers, the value slots mirror the PHP-level ones;` |
|      - |   50 | ` *  - error CODES are raw libxml errNo values while xml_error_string() maps` |
|      - |   51 | ` *    through php's own expat-flavoured table, so the two disagree on what` |
|      - |   52 | ` *    each number means -- xml_get_error_code() after a mismatched tag is 76` |
|      - |   53 | ` *    and XML_ERROR_TAG_MISMATCH is 7. Reproduced as-is;` |
|      - |   54 | ` *  - an external-entity-ref handler that answers false stops the parser and` |
|      - |   55 | ` *    forces errNo to expat's XML_ERROR_EXTERNAL_ENTITY_HANDLING (21), a` |
|      - |   56 | ` *    number that means something else in the libxml table. Also php's.` |
|      - |   57 | ` *` |
|      - |   58 | ` * The deprecated surface is NOT here, per the scope policy:` |
|      - |   59 | ` * xml_parser_free() (E_DEPRECATED 8.5, a no-op since 8.0) and` |
|      - |   60 | ` * xml_set_object() (E_DEPRECATED 8.4) stay loud undefined functions, and the` |
|      - |   61 | ` * method-name-string handler spelling (E_DEPRECATED 8.4) is refused as a` |
|      - |   62 | ` * TypeError -- each twin-paired under 002-integration/function/xml/.` |
|      - |   63 | ` *` |
|      - |   64 | ` * Lifetime: an XMLParser instance owns a phl_xmlparser through a hidden slot;` |
|      - |   65 | ` * the C shell (parser ctxt, its myDoc, the handler VALUES) is chained on the` |
|      - |   66 | ` * per-VM registry (pVm->pXmlParsers) and freed only at VM reset/release,` |
|      - |   67 | ` * like the DOM/XMLWriter registries above it -- PH7 resources carry no` |
|      - |   68 | ` * destructor hook. libxml2 memory stays on the system allocator on purpose` |
|      - |   69 | ` * (see vm_libxml.c's note on PHL_MAX_ALLOC fault injection).` |
|      - |   70 | ` */` |
|      - |   71 |  |
|      - |   72 | `#define PHL_XML_MAXLEVEL 255  /* php's XML_MAXLEVEL: into_struct records no row deeper */` |
|      - |   73 |  |
|      - |   74 | `#ifndef XML_MAX_DICTIONARY_LIMIT` |
|      - |   75 | `#define XML_MAX_DICTIONARY_LIMIT 10000000` |
|      - |   76 | `#endif` |
|      - |   77 |  |
|      - |   78 | `/* Handler slots, one per event php exposes a setter for. */` |
|      - |   79 | `enum {` |
|      - |   80 | `	PHL_XML_H_START = 0,   /* xml_set_element_handler arg #2 */` |
|      - |   81 | `	PHL_XML_H_END,         /* xml_set_element_handler arg #3 */` |
|      - |   82 | `	PHL_XML_H_CDATA,       /* xml_set_character_data_handler */` |
|      - |   83 | `	PHL_XML_H_PI,          /* xml_set_processing_instruction_handler */` |
|      - |   84 | `	PHL_XML_H_DEFAULT,     /* xml_set_default_handler */` |
|      - |   85 | `	PHL_XML_H_UNPARSED,    /* xml_set_unparsed_entity_decl_handler */` |
|      - |   86 | `	PHL_XML_H_NOTATION,    /* xml_set_notation_decl_handler */` |
|      - |   87 | `	PHL_XML_H_EXTENT,      /* xml_set_external_entity_ref_handler */` |
|      - |   88 | `	PHL_XML_H_NSSTART,     /* xml_set_start_namespace_decl_handler */` |
|      - |   89 | `	PHL_XML_H_NSEND,       /* xml_set_end_namespace_decl_handler (php never fires it) */` |
|      - |   90 | `	PHL_XML_H_COUNT` |
|      - |   91 | `};` |
|      - |   92 |  |
|      - |   93 | `/* Target encodings: the three php supports, nothing else can be named. */` |
|      - |   94 | `enum { PHL_XML_ENC_UTF8 = 0, PHL_XML_ENC_ISO88591, PHL_XML_ENC_ASCII };` |
|      - |   95 |  |
|      - |   96 | `typedef struct phl_xmlparser phl_xmlparser;` |
|      - |   97 | `struct phl_xmlparser {` |
|      - |   98 | `	ph7_vm *pVm;` |
|      - |   99 | `	xmlParserCtxtPtr pCtxt;` |
|      - |  100 | `	/* Live only while xml_parse()/xml_parse_into_struct() runs: the calling` |
|      - |  101 | `	 * context (whose value-tracking frees per-event temporaries) and the` |
|      - |  102 | `	 * XMLParser instance handlers receive as their first argument. Both are` |
|      - |  103 | `	 * borrowed -- the builtin's own argument keeps the instance alive. */` |
|      - |  104 | `	ph7_context *pCallCtx;` |
|      - |  105 | `	ph7_class_instance *pObj;` |
|      - |  106 | `	int bNs;                  /* created by xml_parser_create_ns() */` |
|      - |  107 | `	xmlChar cSep;             /* first byte of the separator; '\0' joins URI and name directly */` |
|      - |  108 | `	int bCaseFolding;         /* XML_OPTION_CASE_FOLDING, default ON */` |
|      - |  109 | `	int bSkipWhite;           /* XML_OPTION_SKIP_WHITE (into_struct only, like php) */` |
|      - |  110 | `	int nSkipTagstart;        /* XML_OPTION_SKIP_TAGSTART */` |
|      - |  111 | `	int bParseHuge;           /* XML_OPTION_PARSE_HUGE */` |
|      - |  112 | `	int iTargetEnc;           /* PHL_XML_ENC_* */` |
|      - |  113 | `	int bParsing;             /* recursion guard ("Parser must not be called recursively") */` |
|      - |  114 | `	sxi32 iCbRc;              /* PH7_EXCEPTION/PH7_ABORT parked by a handler: further` |
|      - |  115 | `	                           * handler calls and into_struct recording are suppressed,` |
|      - |  116 | `	                           * the parse runs out, and xml_parse returns this status so` |
|      - |  117 | `	                           * the dispatcher unwinds -- the builtin-throw rail. */` |
|      - |  118 | `	int aInstalled[PHL_XML_H_COUNT]; /* the compat-level route was claimed once (see header note) */` |
|      - |  119 | `	ph7_value aHandler[PHL_XML_H_COUNT];` |
|      - |  120 | `	/* xml_parse_into_struct state; pStructData NULL when a plain xml_parse runs. */` |
|      - |  121 | `	ph7_value *pStructData;   /* the working $values array (owned by the builtin frame) */` |
|      - |  122 | `	ph7_value *pStructIndex;  /* the working $index array, or 0 when not asked for */` |
|      - |  123 | `	int nLevel;` |
|      - |  124 | `	int nCurtag;              /* row counter feeding $index (php never resets it either) */` |
|      - |  125 | `	int bLastWasOpen;` |
|      - |  126 | `	ph7_hashmap *pCtagMap;    /* the current OPEN row; rows are handles, so the pointer` |
|      - |  127 | `	                           * stays valid inside the data array (no user can touch the` |
|      - |  128 | `	                           * working array until it is stored back) */` |
|      - |  129 | `	SyString aLtag[PHL_XML_MAXLEVEL]; /* UNSTRIPPED decoded tag per recorded level, because` |
|      - |  130 | `	                           * SKIP_TAGSTART may change between open and the cdata rows */` |
|      - |  131 | `	phl_xmlparser *pNext;     /* per-VM registry chain */` |
|      - |  132 | `};` |
|      - |  133 |  |
|      - |  134 | `/* ------------------------------------------------------------------------` |
|      - |  135 | ` * Encoding + case folding.` |
|      - |  136 | ` *` |
|      - |  137 | ` * libxml hands every SAX string as UTF-8. php converts to the TARGET` |
|      - |  138 | ` * encoding on the way out (xml_utf8_decode): ISO-8859-1 keeps code points` |
|      - |  139 | ` * <= 0xFF as bytes, US-ASCII <= 0x7F, everything else -- including an` |
|      - |  140 | ` * invalid UTF-8 byte, consumed one byte at a time -- becomes '?'. Case` |
|      - |  141 | ` * folding is zend_str_toupper on the CONVERTED bytes: ASCII-only, so` |
|      - |  142 | ` * an accented name keeps its case (oracle: '<Aé/>' folds to 'Aé').` |
|      - |  143 | ` * ------------------------------------------------------------------------ */` |
|   1370 |  144 | `static void XmlDecodeAppend(int iTargetEnc,const char *zIn,sxu32 nIn,SyBlob *pOut)` |
|      2 |  145 | `{` |
|   1372 |  146 | `	sxu32 i = 0;` |
|   1372 |  147 | `	if( iTargetEnc == PHL_XML_ENC_UTF8 ){` |
|   1346 |  148 | `		SyBlobAppend(pOut,zIn,nIn);` |
|   1346 |  149 | `		return;` |
|      - |  150 | `	}` |
|     91 |  151 | `	while( i < nIn ){` |
|     65 |  152 | `		const unsigned char *z = (const unsigned char *)zIn;` |
|     65 |  153 | `		unsigned int c = z[i];` |
|      - |  154 | `		char cOut;` |
|     65 |  155 | `		if( c < 0x80 ){` |
|     45 |  156 | `			i++;` |
|     43 |  157 | `		}else if( (c & 0xE0) == 0xC0 && i + 1 < nIn && (z[i+1] & 0xC0) == 0x80 ){` |
|     17 |  158 | `			c = ((c & 0x1F) << 6) \| (z[i+1] & 0x3F);` |
|     17 |  159 | `			if( c < 0x80 ){ c = (unsigned int)'?'; } /* overlong */` |
|     17 |  160 | `			i += 2;` |
|     13 |  161 | `		}else if( (c & 0xF0) == 0xE0 && i + 2 < nIn` |
|      5 |  162 | `		       && (z[i+1] & 0xC0) == 0x80 && (z[i+2] & 0xC0) == 0x80 ){` |
|      5 |  163 | `			c = ((c & 0x0F) << 12) \| ((z[i+1] & 0x3F) << 6) \| (z[i+2] & 0x3F);` |
|      5 |  164 | `			if( c < 0x800 \|\| (c >= 0xD800 && c <= 0xDFFF) ){ c = (unsigned int)'?'; }` |
|      5 |  165 | `			i += 3;` |
|      3 |  166 | `		}else if( (c & 0xF8) == 0xF0 && i + 3 < nIn && (z[i+1] & 0xC0) == 0x80` |
|    ! 0 |  167 | `		       && (z[i+2] & 0xC0) == 0x80 && (z[i+3] & 0xC0) == 0x80 ){` |
|    ! 0 |  168 | `			c = ((c & 0x07) << 18) \| ((z[i+1] & 0x3F) << 12)` |
|    ! 0 |  169 | `			  \| ((z[i+2] & 0x3F) << 6) \| (z[i+3] & 0x3F);` |
|    ! 0 |  170 | `			if( c < 0x10000 \|\| c > 0x10FFFF ){ c = (unsigned int)'?'; }` |
|    ! 0 |  171 | `			i += 4;` |
|    ! 0 |  172 | `		}else{` |
|      - |  173 | `			/* Invalid lead/truncated sequence: php consumes ONE byte per '?'. */` |
|    ! 0 |  174 | `			c = (unsigned int)'?';` |
|    ! 0 |  175 | `			i++;` |
|      - |  176 | `		}` |
|     65 |  177 | `		if( iTargetEnc == PHL_XML_ENC_ISO88591 ){` |
|     35 |  178 | `			cOut = (char)(c > 0xFF ? '?' : c);` |
|     18 |  179 | `		}else{` |
|     31 |  180 | `			cOut = (char)(c > 0x7F ? '?' : c);` |
|      - |  181 | `		}` |
|     65 |  182 | `		SyBlobAppend(pOut,&cOut,1);` |
|      1 |  183 | `	}` |
|    687 |  184 | `}` |
|      - |  185 | `/* Decode a NUL-terminated SAX string into pOut; fold when asked. */` |
|   1216 |  186 | `static void XmlDecodeName(phl_xmlparser *p,const xmlChar *zName,SyBlob *pOut)` |
|      2 |  187 | `{` |
|      - |  188 | `	sxu32 n;` |
|   1218 |  189 | `	XmlDecodeAppend(p->iTargetEnc,(const char *)zName,SyStrlen((const char *)zName),pOut);` |
|   1218 |  190 | `	if( p->bCaseFolding ){` |
|   1204 |  191 | `		char *z = (char *)SyBlobData(pOut);` |
|   2660 |  192 | `		for( n = 0 ; n < SyBlobLength(pOut) ; ++n ){` |
|   1458 |  193 | `			if( z[n] >= 'a' && z[n] <= 'z' ){` |
|   1370 |  194 | `				z[n] = (char)(z[n] - ('a' - 'A'));` |
|    684 |  195 | `			}` |
|    730 |  196 | `		}` |
|    601 |  197 | `	}` |
|   1218 |  198 | `}` |
|      - |  199 | `/* php's xml_stripped_tag: SKIP_TAGSTART drops the first N bytes of a` |
|      - |  200 | ` * DECODED tag name; an offset past the end is the empty string. */` |
|   1188 |  201 | `static void XmlStrippedTag(const char *zTag,sxu32 nTag,int nOffset,SyString *pOut)` |
|      2 |  202 | `{` |
|   1190 |  203 | `	if( nOffset <= 0 ){` |
|   1174 |  204 | `		SyStringInitFromBuf(pOut,zTag,nTag);` |
|    603 |  205 | `	}else if( (sxu32)nOffset >= nTag ){` |
|      5 |  206 | `		SyStringInitFromBuf(pOut,zTag,0);` |
|      3 |  207 | `	}else{` |
|     13 |  208 | `		SyStringInitFromBuf(pOut,&zTag[nOffset],nTag - (sxu32)nOffset);` |
|      - |  209 | `	}` |
|   1190 |  210 | `}` |
|      - |  211 |  |
|      - |  212 | `/* ------------------------------------------------------------------------` |
|      - |  213 | ` * Handler plumbing.` |
|      - |  214 | ` * ------------------------------------------------------------------------ */` |
|   2736 |  215 | `static int XmlHandlerSet(phl_xmlparser *p,int iH)` |
|      3 |  216 | `{` |
|   2739 |  217 | `	return (p->aHandler[iH].iFlags & MEMOBJ_NULL) == 0;` |
|      3 |  218 | `}` |
|      - |  219 | `/*` |
|      - |  220 | ` * Run one user handler. A throw (or exit) comes back as PH7_EXCEPTION/` |
|      - |  221 | ` * PH7_ABORT; php does not STOP the parser for it -- the pending exception` |
|      - |  222 | ` * makes every later zend_call a no-op and suppresses recording while libxml` |
|      - |  223 | ` * runs the rest of the document -- so iCbRc mirrors exactly that, and the` |
|      - |  224 | ` * raise itself is deferred to xml_parse's return (the builtin-throw rail:` |
|      - |  225 | ` * an in-flight throw from a C builtin would run the enclosing catch NOW).` |
|      - |  226 | ` */` |
|    156 |  227 | `static void XmlCallHandler(phl_xmlparser *p,int iH,int nArg,ph7_value **apArg,ph7_value *pRet)` |
|      2 |  228 | `{` |
|      - |  229 | `	ph7_value sRes;` |
|      - |  230 | `	sxi32 rc;` |
|    158 |  231 | `	if( p->iCbRc \|\| !XmlHandlerSet(p,iH) ){` |
|    ! 0 |  232 | `		return;` |
|      - |  233 | `	}` |
|    158 |  234 | `	PH7_MemObjInit(p->pVm,&sRes);` |
|    158 |  235 | `	rc = PH7_VmCallCallbackByValue(p->pVm,&p->aHandler[iH],nArg,apArg,&sRes,0);` |
|    158 |  236 | `	if( PH7_CALLBACK_UNWOUND(rc) ){` |
|      3 |  237 | `		p->iCbRc = rc;` |
|    157 |  238 | `	}else if( pRet && rc == SXRET_OK ){` |
|      5 |  239 | `		PH7_MemObjStore(&sRes,pRet);` |
|      2 |  240 | `	}` |
|    158 |  241 | `	PH7_MemObjRelease(&sRes);` |
|     80 |  242 | `}` |
|      - |  243 | `/* The $parser argument every handler receives first: the instance the` |
|      - |  244 | ` * running xml_parse() was handed. The wrapper takes its OWN reference,` |
|      - |  245 | ` * because the PH7_MemObjRelease that ends every argument's life drops one. */` |
|    156 |  246 | `static void XmlParserValue(phl_xmlparser *p,ph7_value *pOut)` |
|      2 |  247 | `{` |
|    158 |  248 | `	PH7_MemObjInit(p->pVm,pOut);` |
|    158 |  249 | `	if( p->pObj ){` |
|    158 |  250 | `		p->pObj->iRef++;` |
|    158 |  251 | `		pOut->x.pOther = p->pObj;` |
|    158 |  252 | `		MemObjSetType(pOut,MEMOBJ_OBJ);` |
|     78 |  253 | `	}` |
|    158 |  254 | `}` |
|      - |  255 | `/* A decoded-string argument (no folding). NULL becomes bool FALSE -- php's` |
|      - |  256 | ` * xml_xmlchar_zval, which is why a notation with no PUBLIC id hands false. */` |
|    142 |  257 | `static void XmlStringValue(phl_xmlparser *p,const char *zIn,sxu32 nIn,ph7_value *pOut)` |
|      1 |  258 | `{` |
|      - |  259 | `	SyBlob sBlob;` |
|      - |  260 | `	SyString sStr;` |
|    143 |  261 | `	PH7_MemObjInit(p->pVm,pOut);` |
|    143 |  262 | `	if( zIn == 0 ){` |
|     15 |  263 | `		PH7_MemObjInitFromBool(p->pVm,pOut,0);` |
|     15 |  264 | `		return;` |
|      - |  265 | `	}` |
|    129 |  266 | `	SyBlobInit(&sBlob,&p->pVm->sAllocator);` |
|    129 |  267 | `	XmlDecodeAppend(p->iTargetEnc,zIn,nIn,&sBlob);` |
|    129 |  268 | `	SyStringInitFromBuf(&sStr,SyBlobData(&sBlob),SyBlobLength(&sBlob));` |
|    129 |  269 | `	PH7_MemObjInitFromString(p->pVm,pOut,&sStr);` |
|    129 |  270 | `	SyBlobRelease(&sBlob);` |
|     72 |  271 | `}` |
|     60 |  272 | `static void XmlStringValueZ(phl_xmlparser *p,const xmlChar *zIn,ph7_value *pOut)` |
|      1 |  273 | `{` |
|     61 |  274 | `	XmlStringValue(p,(const char *)zIn,zIn ? SyStrlen((const char *)zIn) : 0,pOut);` |
|     61 |  275 | `}` |
|      - |  276 |  |
|      - |  277 | `/* ------------------------------------------------------------------------` |
|      - |  278 | ` * xml_parse_into_struct recording -- the port of php's data/info halves.` |
|      - |  279 | ` * Rows are hashmaps INSIDE the working array; hashmaps are handles under` |
|      - |  280 | ` * MemObjStore, so pCtagMap can keep pointing at the open row after insert.` |
|      - |  281 | ` * ------------------------------------------------------------------------ */` |
|   2148 |  282 | `static void XmlRowAddStr(ph7_vm *pVm,ph7_hashmap *pRow,const char *zKey,const char *zVal,sxu32 nVal)` |
|      1 |  283 | `{` |
|      - |  284 | `	ph7_value sVal;` |
|      - |  285 | `	SyString sStr;` |
|   2149 |  286 | `	PH7_MemObjInit(pVm,&sVal);` |
|   2149 |  287 | `	SyStringInitFromBuf(&sStr,zVal,nVal);` |
|   2149 |  288 | `	PH7_MemObjInitFromString(pVm,&sVal,&sStr);` |
|   2149 |  289 | `	PH7_HashmapInsertRawKey(pRow,zKey,SyStrlen(zKey),&sVal);` |
|   2149 |  290 | `	PH7_MemObjRelease(&sVal);` |
|   2149 |  291 | `}` |
|   1056 |  292 | `static void XmlRowAddInt(ph7_vm *pVm,ph7_hashmap *pRow,const char *zKey,sxi64 iVal)` |
|      1 |  293 | `{` |
|      - |  294 | `	ph7_value sVal;` |
|   1057 |  295 | `	PH7_MemObjInit(pVm,&sVal);` |
|   1057 |  296 | `	PH7_MemObjInitFromInt(pVm,&sVal,iVal);` |
|   1057 |  297 | `	PH7_HashmapInsertRawKey(pRow,zKey,SyStrlen(zKey),&sVal);` |
|   1057 |  298 | `	PH7_MemObjRelease(&sVal);` |
|   1057 |  299 | `}` |
|      - |  300 | `/* Fetch a row's entry, or 0. The returned value is a COPY handle: for a` |
|      - |  301 | ` * string entry the caller may not mutate through it, but reading and` |
|      - |  302 | ` * re-inserting under the same key is php's add_assoc overwrite. */` |
|     26 |  303 | `static ph7_hashmap_node * XmlRowFind(ph7_vm *pVm,ph7_hashmap *pRow,const char *zKey)` |
|      1 |  304 | `{` |
|      - |  305 | `	ph7_value sKey;` |
|     27 |  306 | `	ph7_hashmap_node *pNode = 0;` |
|      - |  307 | `	SyString sStr;` |
|     27 |  308 | `	PH7_MemObjInit(pVm,&sKey);` |
|     27 |  309 | `	SyStringInitFromBuf(&sStr,zKey,SyStrlen(zKey));` |
|     27 |  310 | `	PH7_MemObjInitFromString(pVm,&sKey,&sStr);` |
|     27 |  311 | `	if( SXRET_OK != PH7_HashmapLookup(pRow,&sKey,&pNode) ){` |
|     15 |  312 | `		pNode = 0;` |
|      7 |  313 | `	}` |
|     27 |  314 | `	PH7_MemObjRelease(&sKey);` |
|     27 |  315 | `	return pNode;` |
|      1 |  316 | `}` |
|      - |  317 | `/* php's xml_add_to_info: $index[$tag][] = curtag++ */` |
|   1056 |  318 | `static void XmlAddToInfo(phl_xmlparser *p,const SyString *pTag)` |
|      1 |  319 | `{` |
|   1057 |  320 | `	ph7_vm *pVm = p->pVm;` |
|      - |  321 | `	ph7_hashmap *pInfo;` |
|      - |  322 | `	ph7_hashmap_node *pNode;` |
|      - |  323 | `	ph7_value sKey,sVal;` |
|   1057 |  324 | `	if( p->pStructIndex == 0 ){` |
|      5 |  325 | `		return;` |
|      - |  326 | `	}` |
|   1053 |  327 | `	pInfo = (ph7_hashmap *)p->pStructIndex->x.pOther;` |
|   1053 |  328 | `	PH7_MemObjInit(pVm,&sKey);` |
|   1053 |  329 | `	PH7_MemObjInitFromString(pVm,&sKey,pTag);` |
|   1053 |  330 | `	if( SXRET_OK != PH7_HashmapLookup(pInfo,&sKey,&pNode) ){` |
|      - |  331 | `		ph7_value sList;` |
|     21 |  332 | `		PH7_MemObjInit(pVm,&sList);` |
|     21 |  333 | `		if( SXRET_OK != PH7_MemObjToHashmap(&sList) ){` |
|    ! 0 |  334 | `			PH7_MemObjRelease(&sKey);` |
|    ! 0 |  335 | `			return;` |
|      - |  336 | `		}` |
|     21 |  337 | `		PH7_HashmapInsert(pInfo,&sKey,&sList);` |
|     21 |  338 | `		PH7_MemObjRelease(&sList);` |
|     21 |  339 | `		if( SXRET_OK != PH7_HashmapLookup(pInfo,&sKey,&pNode) ){` |
|    ! 0 |  340 | `			PH7_MemObjRelease(&sKey);` |
|    ! 0 |  341 | `			return;` |
|      - |  342 | `		}` |
|     10 |  343 | `	}` |
|   1053 |  344 | `	PH7_MemObjInit(pVm,&sVal);` |
|      - |  345 | `	{` |
|      - |  346 | `		ph7_value sEntry;` |
|      - |  347 | `		/* A Load view still takes a reference on the list it points at, so` |
|      - |  348 | `		 * the release below is what keeps the per-tag lists from leaking` |
|      - |  349 | `		 * one count per recorded row. */` |
|   1053 |  350 | `		PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|   1053 |  351 | `		if( (sVal.iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|   1053 |  352 | `			PH7_MemObjInit(pVm,&sEntry);` |
|   1053 |  353 | `			PH7_MemObjInitFromInt(pVm,&sEntry,p->nCurtag);` |
|   1053 |  354 | `			PH7_HashmapInsert((ph7_hashmap *)sVal.x.pOther,0,&sEntry);` |
|   1053 |  355 | `			PH7_MemObjRelease(&sEntry);` |
|    526 |  356 | `		}` |
|   1053 |  357 | `		PH7_MemObjRelease(&sVal);` |
|      - |  358 | `	}` |
|   1053 |  359 | `	PH7_MemObjRelease(&sKey);` |
|   1053 |  360 | `	p->nCurtag++;` |
|    529 |  361 | `}` |
|      - |  362 | `/* Append a finished row to the working $values array and answer its map. */` |
|   1056 |  363 | `static ph7_hashmap * XmlRowAppend(phl_xmlparser *p,ph7_value *pRow)` |
|      1 |  364 | `{` |
|   1057 |  365 | `	ph7_hashmap *pMap = (ph7_hashmap *)p->pStructData->x.pOther;` |
|   1057 |  366 | `	if( SXRET_OK != PH7_HashmapInsert(pMap,0,pRow) ){` |
|    ! 0 |  367 | `		return 0;` |
|      - |  368 | `	}` |
|   1057 |  369 | `	return (ph7_hashmap *)pRow->x.pOther;` |
|    529 |  370 | `}` |
|      - |  371 | `/* Release the recorded per-level tag names (php's xml_parser_free_ltags). */` |
|     94 |  372 | `static void XmlFreeLtags(phl_xmlparser *p)` |
|      3 |  373 | `{` |
|      - |  374 | `	int i;` |
|  24067 |  375 | `	for( i = 0 ; i < PHL_XML_MAXLEVEL ; ++i ){` |
|  23973 |  376 | `		if( p->aLtag[i].zString ){` |
|      5 |  377 | `			SyMemBackendFree(&p->pVm->sAllocator,(void *)p->aLtag[i].zString);` |
|      5 |  378 | `			SyStringInitFromBuf(&p->aLtag[i],0,0);` |
|      2 |  379 | `		}` |
|  11988 |  380 | `	}` |
|     97 |  381 | `}` |
|      - |  382 |  |
|      - |  383 | `/* ------------------------------------------------------------------------` |
|      - |  384 | ` * SAX callbacks: the compat.c + xml.c pair folded together.` |
|      - |  385 | ` * ------------------------------------------------------------------------ */` |
|      - |  386 |  |
|      - |  387 | `/*` |
|      - |  388 | ` * The raw-text route for a start tag nothing handles: scan the parser's` |
|      - |  389 | ` * input buffer BACKWARD from the cursor to the nearest '<' and hand the` |
|      - |  390 | ` * slice to the default handler verbatim -- original entity spellings and` |
|      - |  391 | ` * all. A self-closing tag's cursor sits on the '/', which php patches to` |
|      - |  392 | ` * '>' so the event reads like an open tag (the matching end event is built` |
|      - |  393 | ` * separately). Straight from php's start_element_emit_default().` |
|      - |  394 | ` */` |
|     22 |  395 | `static void XmlEmitRawStartTag(phl_xmlparser *p)` |
|      2 |  396 | `{` |
|      - |  397 | `	const xmlChar *zCur,*zEnd,*zBase;` |
|     24 |  398 | `	if( p->iCbRc \|\| !p->pCtxt \|\| !p->pCtxt->input \|\| !XmlHandlerSet(p,PHL_XML_H_DEFAULT) ){` |
|     16 |  399 | `		return;` |
|      - |  400 | `	}` |
|      9 |  401 | `	zCur = p->pCtxt->input->cur;` |
|      9 |  402 | `	zEnd = zCur;` |
|      9 |  403 | `	zBase = p->pCtxt->input->base;` |
|     47 |  404 | `	while( zCur > zBase && *zCur != '<' ){` |
|     39 |  405 | `		zCur--;` |
|      1 |  406 | `	}` |
|      - |  407 | `	{` |
|      - |  408 | `		ph7_value sParser,sData;` |
|      - |  409 | `		ph7_value *apArg[2];` |
|      9 |  410 | `		XmlParserValue(p,&sParser);` |
|      9 |  411 | `		if( *zEnd == '/' ){` |
|      - |  412 | `			SyBlob sBlob;` |
|      3 |  413 | `			SyBlobInit(&sBlob,&p->pVm->sAllocator);` |
|      3 |  414 | `			SyBlobAppend(&sBlob,zCur,(sxu32)(zEnd - zCur));` |
|      3 |  415 | `			SyBlobAppend(&sBlob,">",1);` |
|      3 |  416 | `			XmlStringValue(p,(const char *)SyBlobData(&sBlob),SyBlobLength(&sBlob),&sData);` |
|      3 |  417 | `			SyBlobRelease(&sBlob);` |
|      2 |  418 | `		}else{` |
|      7 |  419 | `			XmlStringValue(p,(const char *)zCur,(sxu32)(zEnd - zCur) + 1,&sData);` |
|      - |  420 | `		}` |
|      9 |  421 | `		apArg[0] = &sParser;` |
|      9 |  422 | `		apArg[1] = &sData;` |
|      9 |  423 | `		XmlCallHandler(p,PHL_XML_H_DEFAULT,2,apArg,0);` |
|      9 |  424 | `		PH7_MemObjRelease(&sParser);` |
|      9 |  425 | `		PH7_MemObjRelease(&sData);` |
|      - |  426 | `	}` |
|     13 |  427 | `}` |
|      - |  428 | `/* "</name>" for the default handler when no end handler exists. */` |
|     14 |  429 | `static void XmlEmitRawEndTag(phl_xmlparser *p,const xmlChar *zPrefix,const xmlChar *zName)` |
|      2 |  430 | `{` |
|      - |  431 | `	SyBlob sBlob;` |
|      - |  432 | `	ph7_value sParser,sData;` |
|      - |  433 | `	ph7_value *apArg[2];` |
|     16 |  434 | `	if( p->iCbRc \|\| !XmlHandlerSet(p,PHL_XML_H_DEFAULT) ){` |
|     10 |  435 | `		return;` |
|      - |  436 | `	}` |
|      7 |  437 | `	SyBlobInit(&sBlob,&p->pVm->sAllocator);` |
|      7 |  438 | `	SyBlobAppend(&sBlob,"</",2);` |
|      7 |  439 | `	if( zPrefix ){` |
|    ! 0 |  440 | `		SyBlobAppend(&sBlob,zPrefix,SyStrlen((const char *)zPrefix));` |
|    ! 0 |  441 | `		SyBlobAppend(&sBlob,":",1);` |
|    ! 0 |  442 | `	}` |
|      7 |  443 | `	SyBlobAppend(&sBlob,zName,SyStrlen((const char *)zName));` |
|      7 |  444 | `	SyBlobAppend(&sBlob,">",1);` |
|      7 |  445 | `	XmlParserValue(p,&sParser);` |
|      7 |  446 | `	XmlStringValue(p,(const char *)SyBlobData(&sBlob),SyBlobLength(&sBlob),&sData);` |
|      7 |  447 | `	SyBlobRelease(&sBlob);` |
|      7 |  448 | `	apArg[0] = &sParser;` |
|      7 |  449 | `	apArg[1] = &sData;` |
|      7 |  450 | `	XmlCallHandler(p,PHL_XML_H_DEFAULT,2,apArg,0);` |
|      7 |  451 | `	PH7_MemObjRelease(&sParser);` |
|      7 |  452 | `	PH7_MemObjRelease(&sData);` |
|      9 |  453 | `}` |
|      - |  454 | `/*` |
|      - |  455 | ` * The shared start-element body: attributes arrive as decoded (name,value)` |
|      - |  456 | ` * pairs already collected into pAttrArr by the SAX1/SAX2 fronts below.` |
|      - |  457 | ` * Ports php's xml_startElementHandler: the user handler runs first, then` |
|      - |  458 | ` * the into_struct row is recorded -- both against the same folded name,` |
|      - |  459 | ` * both skipping the first SKIP_TAGSTART bytes at PRESENTATION time.` |
|      - |  460 | ` */` |
|    594 |  461 | `static void XmlStartElementCommon(phl_xmlparser *p,const char *zTag,sxu32 nTag,ph7_value *pAttrArr,int nAttr)` |
|      2 |  462 | `{` |
|    596 |  463 | `	ph7_vm *pVm = p->pVm;` |
|      - |  464 | `	SyString sStripped;` |
|    596 |  465 | `	p->nLevel++;` |
|    596 |  466 | `	XmlStrippedTag(zTag,nTag,p->nSkipTagstart,&sStripped);` |
|    596 |  467 | `	if( XmlHandlerSet(p,PHL_XML_H_START) && !p->iCbRc ){` |
|      - |  468 | `		ph7_value sParser,sName;` |
|      - |  469 | `		ph7_value *apArg[3];` |
|     58 |  470 | `		XmlParserValue(p,&sParser);` |
|     58 |  471 | `		PH7_MemObjInit(pVm,&sName);` |
|     58 |  472 | `		PH7_MemObjInitFromString(pVm,&sName,&sStripped);` |
|     58 |  473 | `		apArg[0] = &sParser;` |
|     58 |  474 | `		apArg[1] = &sName;` |
|     58 |  475 | `		apArg[2] = pAttrArr;` |
|     58 |  476 | `		XmlCallHandler(p,PHL_XML_H_START,3,apArg,0);` |
|     58 |  477 | `		PH7_MemObjRelease(&sParser);` |
|     58 |  478 | `		PH7_MemObjRelease(&sName);` |
|     28 |  479 | `	}` |
|    596 |  480 | `	if( p->pStructData && !p->iCbRc ){` |
|    539 |  481 | `		if( p->nLevel <= PHL_XML_MAXLEVEL ){` |
|      - |  482 | `			ph7_value sRow;` |
|      - |  483 | `			ph7_hashmap *pRow;` |
|      - |  484 | `			char *zDup;` |
|    533 |  485 | `			PH7_MemObjInit(pVm,&sRow);` |
|    533 |  486 | `			if( SXRET_OK != PH7_MemObjToHashmap(&sRow) ){` |
|    ! 0 |  487 | `				return;` |
|      - |  488 | `			}` |
|    533 |  489 | `			pRow = (ph7_hashmap *)sRow.x.pOther;` |
|    533 |  490 | `			XmlAddToInfo(p,&sStripped);` |
|    533 |  491 | `			XmlRowAddStr(pVm,pRow,"tag",sStripped.zString,sStripped.nByte);` |
|    533 |  492 | `			XmlRowAddStr(pVm,pRow,"type","open",sizeof("open")-1);` |
|    533 |  493 | `			XmlRowAddInt(pVm,pRow,"level",p->nLevel);` |
|      - |  494 | `			/* The UNSTRIPPED name is what later cdata rows strip live,` |
|      - |  495 | `			 * because SKIP_TAGSTART may change mid-parse (php's comment). */` |
|    533 |  496 | `			zDup = SyMemBackendStrDup(&pVm->sAllocator,zTag,nTag);` |
|    533 |  497 | `			if( p->aLtag[p->nLevel - 1].zString ){` |
|    ! 0 |  498 | `				SyMemBackendFree(&pVm->sAllocator,(void *)p->aLtag[p->nLevel - 1].zString);` |
|    ! 0 |  499 | `			}` |
|    533 |  500 | `			SyStringInitFromBuf(&p->aLtag[p->nLevel - 1],zDup,zDup ? nTag : 0);` |
|    533 |  501 | `			p->bLastWasOpen = 1;` |
|    533 |  502 | `			if( nAttr > 0 ){` |
|      - |  503 | `				ph7_value sKey;` |
|      - |  504 | `				SyString sStr;` |
|      3 |  505 | `				PH7_MemObjInit(pVm,&sKey);` |
|      3 |  506 | `				SyStringInitFromBuf(&sStr,"attributes",sizeof("attributes")-1);` |
|      3 |  507 | `				PH7_MemObjInitFromString(pVm,&sKey,&sStr);` |
|      3 |  508 | `				PH7_HashmapInsert(pRow,&sKey,pAttrArr);` |
|      3 |  509 | `				PH7_MemObjRelease(&sKey);` |
|      1 |  510 | `			}` |
|    533 |  511 | `			p->pCtagMap = XmlRowAppend(p,&sRow);` |
|    533 |  512 | `			PH7_MemObjRelease(&sRow);` |
|    273 |  513 | `		}else if( p->nLevel == PHL_XML_MAXLEVEL + 1 ){` |
|      3 |  514 | `			ph7_context_throw_error(p->pCallCtx,PH7_CTX_WARNING,` |
|      - |  515 | `				"Maximum depth exceeded - Results truncated");` |
|      1 |  516 | `		}` |
|    269 |  517 | `	}` |
|    299 |  518 | `}` |
|      - |  519 | `/* The shared end-element body (php's xml_endElementHandler). */` |
|    590 |  520 | `static void XmlEndElementCommon(phl_xmlparser *p,const char *zTag,sxu32 nTag)` |
|      2 |  521 | `{` |
|    592 |  522 | `	ph7_vm *pVm = p->pVm;` |
|      - |  523 | `	SyString sStripped;` |
|    592 |  524 | `	XmlStrippedTag(zTag,nTag,p->nSkipTagstart,&sStripped);` |
|    592 |  525 | `	if( XmlHandlerSet(p,PHL_XML_H_END) && !p->iCbRc ){` |
|      - |  526 | `		ph7_value sParser,sName;` |
|      - |  527 | `		ph7_value *apArg[2];` |
|     24 |  528 | `		XmlParserValue(p,&sParser);` |
|     24 |  529 | `		PH7_MemObjInit(pVm,&sName);` |
|     24 |  530 | `		PH7_MemObjInitFromString(pVm,&sName,&sStripped);` |
|     24 |  531 | `		apArg[0] = &sParser;` |
|     24 |  532 | `		apArg[1] = &sName;` |
|     24 |  533 | `		XmlCallHandler(p,PHL_XML_H_END,2,apArg,0);` |
|     24 |  534 | `		PH7_MemObjRelease(&sParser);` |
|     24 |  535 | `		PH7_MemObjRelease(&sName);` |
|     11 |  536 | `	}` |
|    592 |  537 | `	if( p->pStructData && !p->iCbRc ){` |
|    535 |  538 | `		if( p->bLastWasOpen && p->pCtagMap ){` |
|      - |  539 | `			/* Open with nothing recorded since: the row BECOMES the close. */` |
|     15 |  540 | `			XmlRowAddStr(pVm,p->pCtagMap,"type","complete",sizeof("complete")-1);` |
|      8 |  541 | `		}else{` |
|      - |  542 | `			ph7_value sRow;` |
|    521 |  543 | `			PH7_MemObjInit(pVm,&sRow);` |
|    521 |  544 | `			if( SXRET_OK == PH7_MemObjToHashmap(&sRow) ){` |
|    521 |  545 | `				ph7_hashmap *pRow = (ph7_hashmap *)sRow.x.pOther;` |
|    521 |  546 | `				XmlAddToInfo(p,&sStripped);` |
|    521 |  547 | `				XmlRowAddStr(pVm,pRow,"tag",sStripped.zString,sStripped.nByte);` |
|    521 |  548 | `				XmlRowAddStr(pVm,pRow,"type","close",sizeof("close")-1);` |
|    521 |  549 | `				XmlRowAddInt(pVm,pRow,"level",p->nLevel);` |
|    521 |  550 | `				XmlRowAppend(p,&sRow);` |
|    521 |  551 | `				PH7_MemObjRelease(&sRow);` |
|    260 |  552 | `			}` |
|      - |  553 | `		}` |
|    535 |  554 | `		p->bLastWasOpen = 0;` |
|    267 |  555 | `	}` |
|    592 |  556 | `	if( p->nLevel > 0 && p->nLevel <= PHL_XML_MAXLEVEL ){` |
|    586 |  557 | `		if( p->aLtag[p->nLevel - 1].zString ){` |
|    529 |  558 | `			SyMemBackendFree(&pVm->sAllocator,(void *)p->aLtag[p->nLevel - 1].zString);` |
|    529 |  559 | `			SyStringInitFromBuf(&p->aLtag[p->nLevel - 1],0,0);` |
|    264 |  560 | `		}` |
|    292 |  561 | `	}` |
|    592 |  562 | `	p->nLevel--;` |
|    592 |  563 | `}` |
|      - |  564 | `/* SAX1 start (non-namespace parser): raw names, xmlns attrs included. */` |
|    602 |  565 | `static void XmlSaxStartElement(void *pUser,const xmlChar *zName,const xmlChar **azAttr)` |
|      3 |  566 | `{` |
|    605 |  567 | `	phl_xmlparser *p = (phl_xmlparser *)pUser;` |
|      - |  568 | `	SyBlob sTag;` |
|      - |  569 | `	ph7_value *pAttrArr;` |
|    605 |  570 | `	int nAttr = 0;` |
|    605 |  571 | `	if( p == 0 ){` |
|    ! 0 |  572 | `		return;` |
|      - |  573 | `	}` |
|    605 |  574 | `	if( !XmlHandlerSet(p,PHL_XML_H_START) && !p->aInstalled[PHL_XML_H_START] && !p->pStructData ){` |
|     24 |  575 | `		XmlEmitRawStartTag(p);` |
|     24 |  576 | `		return;` |
|      - |  577 | `	}` |
|    582 |  578 | `	SyBlobInit(&sTag,&p->pVm->sAllocator);` |
|    582 |  579 | `	XmlDecodeName(p,zName,&sTag);` |
|    582 |  580 | `	pAttrArr = ph7_context_new_array(p->pCallCtx);` |
|    582 |  581 | `	if( pAttrArr ){` |
|    600 |  582 | `		while( azAttr && azAttr[0] ){` |
|      - |  583 | `			SyBlob sAtt;` |
|      - |  584 | `			ph7_value sVal;` |
|     19 |  585 | `			SyBlobInit(&sAtt,&p->pVm->sAllocator);` |
|     19 |  586 | `			XmlDecodeName(p,azAttr[0],&sAtt);` |
|     19 |  587 | `			XmlStringValueZ(p,azAttr[1],&sVal);` |
|     28 |  588 | `			PH7_HashmapInsertRawKey((ph7_hashmap *)pAttrArr->x.pOther,` |
|     18 |  589 | `				(const char *)SyBlobData(&sAtt),SyBlobLength(&sAtt),&sVal);` |
|     19 |  590 | `			PH7_MemObjRelease(&sVal);` |
|     19 |  591 | `			SyBlobRelease(&sAtt);` |
|     19 |  592 | `			nAttr++;` |
|     19 |  593 | `			azAttr += 2;` |
|      1 |  594 | `		}` |
|    582 |  595 | `		XmlStartElementCommon(p,(const char *)SyBlobData(&sTag),SyBlobLength(&sTag),pAttrArr,nAttr);` |
|    582 |  596 | `		ph7_context_release_value(p->pCallCtx,pAttrArr);` |
|    290 |  597 | `	}` |
|    582 |  598 | `	SyBlobRelease(&sTag);` |
|    304 |  599 | `}` |
|    590 |  600 | `static void XmlSaxEndElement(void *pUser,const xmlChar *zName)` |
|      3 |  601 | `{` |
|    593 |  602 | `	phl_xmlparser *p = (phl_xmlparser *)pUser;` |
|      - |  603 | `	SyBlob sTag;` |
|    593 |  604 | `	if( p == 0 ){` |
|    ! 0 |  605 | `		return;` |
|      - |  606 | `	}` |
|    593 |  607 | `	if( !XmlHandlerSet(p,PHL_XML_H_END) && !p->aInstalled[PHL_XML_H_END] && !p->pStructData ){` |
|     16 |  608 | `		XmlEmitRawEndTag(p,0,zName);` |
|     16 |  609 | `		return;` |
|      - |  610 | `	}` |
|    578 |  611 | `	SyBlobInit(&sTag,&p->pVm->sAllocator);` |
|    578 |  612 | `	XmlDecodeName(p,zName,&sTag);` |
|    578 |  613 | `	XmlEndElementCommon(p,(const char *)SyBlobData(&sTag),SyBlobLength(&sTag));` |
|    578 |  614 | `	SyBlobRelease(&sTag);` |
|    298 |  615 | `}` |
|      - |  616 | `/* URI + separator + local name, php's qualify_namespace: the separator is` |
|      - |  617 | ` * ONE byte and an empty one joins the parts directly (an artifact of php's` |
|      - |  618 | ` * xmlStrncat over a "" separator that the oracle confirms). */` |
|     30 |  619 | `static void XmlQualifyName(phl_xmlparser *p,const xmlChar *zLocal,const xmlChar *zUri,SyBlob *pOut)` |
|      1 |  620 | `{` |
|     31 |  621 | `	if( zUri ){` |
|     31 |  622 | `		SyBlobAppend(pOut,zUri,SyStrlen((const char *)zUri));` |
|     31 |  623 | `		if( p->cSep ){` |
|     27 |  624 | `			SyBlobAppend(pOut,&p->cSep,1);` |
|     13 |  625 | `		}` |
|     15 |  626 | `	}` |
|     31 |  627 | `	SyBlobAppend(pOut,zLocal,SyStrlen((const char *)zLocal));` |
|     31 |  628 | `}` |
|      - |  629 | `/* SAX2 start (namespace parser). */` |
|     14 |  630 | `static void XmlSaxStartElementNs(void *pUser,const xmlChar *zLocal,const xmlChar *zPrefix,` |
|      - |  631 | `	const xmlChar *zUri,int nNs,const xmlChar **azNs,int nAttrIn,int nDefaulted,` |
|      - |  632 | `	const xmlChar **azAttr)` |
|      1 |  633 | `{` |
|     15 |  634 | `	phl_xmlparser *p = (phl_xmlparser *)pUser;` |
|      - |  635 | `	int i;` |
|      7 |  636 | `	SXUNUSED(nDefaulted);` |
|      7 |  637 | `	SXUNUSED(zPrefix);` |
|     15 |  638 | `	if( p == 0 ){` |
|    ! 0 |  639 | `		return;` |
|      - |  640 | `	}` |
|      - |  641 | `	/* Namespace-declaration events fire BEFORE the element, php's order. */` |
|     15 |  642 | `	if( nNs > 0 && (XmlHandlerSet(p,PHL_XML_H_NSSTART) \|\| p->aInstalled[PHL_XML_H_NSSTART]) ){` |
|      7 |  643 | `		for( i = 0 ; i < nNs ; ++i ){` |
|      - |  644 | `			ph7_value sParser,sPrefix,sUri;` |
|      - |  645 | `			ph7_value *apArg[3];` |
|      5 |  646 | `			XmlParserValue(p,&sParser);` |
|      5 |  647 | `			XmlStringValueZ(p,azNs[i*2],&sPrefix);       /* NULL prefix -> false */` |
|      5 |  648 | `			XmlStringValueZ(p,azNs[i*2+1],&sUri);` |
|      5 |  649 | `			apArg[0] = &sParser;` |
|      5 |  650 | `			apArg[1] = &sPrefix;` |
|      5 |  651 | `			apArg[2] = &sUri;` |
|      5 |  652 | `			XmlCallHandler(p,PHL_XML_H_NSSTART,3,apArg,0);` |
|      5 |  653 | `			PH7_MemObjRelease(&sParser);` |
|      5 |  654 | `			PH7_MemObjRelease(&sPrefix);` |
|      5 |  655 | `			PH7_MemObjRelease(&sUri);` |
|      3 |  656 | `		}` |
|      1 |  657 | `	}` |
|     15 |  658 | `	if( !XmlHandlerSet(p,PHL_XML_H_START) && !p->aInstalled[PHL_XML_H_START] && !p->pStructData ){` |
|    ! 0 |  659 | `		XmlEmitRawStartTag(p);` |
|    ! 0 |  660 | `		return;` |
|      - |  661 | `	}` |
|      - |  662 | `	{` |
|      - |  663 | `		SyBlob sRaw,sTag;` |
|      - |  664 | `		ph7_value *pAttrArr;` |
|     15 |  665 | `		int nAttr = 0;` |
|     15 |  666 | `		SyBlobInit(&sRaw,&p->pVm->sAllocator);` |
|     15 |  667 | `		SyBlobInit(&sTag,&p->pVm->sAllocator);` |
|     15 |  668 | `		XmlQualifyName(p,zLocal,zUri,&sRaw);` |
|     15 |  669 | `		SyBlobAppend(&sRaw,"",1);` |
|     15 |  670 | `		XmlDecodeName(p,(const xmlChar *)SyBlobData(&sRaw),&sTag);` |
|     15 |  671 | `		pAttrArr = ph7_context_new_array(p->pCallCtx);` |
|     15 |  672 | `		if( pAttrArr ){` |
|     29 |  673 | `			for( i = 0 ; i < nAttrIn ; ++i ){` |
|      - |  674 | `				/* attributes: localname, prefix, uri, value, valueend */` |
|     15 |  675 | `				const xmlChar *zAl = azAttr[i*5];` |
|     15 |  676 | `				const xmlChar *zAp = azAttr[i*5+1];` |
|     15 |  677 | `				const xmlChar *zAu = azAttr[i*5+2];` |
|     15 |  678 | `				const xmlChar *zVs = azAttr[i*5+3];` |
|     15 |  679 | `				const xmlChar *zVe = azAttr[i*5+4];` |
|      - |  680 | `				SyBlob sAr,sAn;` |
|      - |  681 | `				ph7_value sVal;` |
|     15 |  682 | `				SyBlobInit(&sAr,&p->pVm->sAllocator);` |
|     15 |  683 | `				SyBlobInit(&sAn,&p->pVm->sAllocator);` |
|     15 |  684 | `				if( zAp ){` |
|      3 |  685 | `					XmlQualifyName(p,zAl,zAu,&sAr);` |
|      2 |  686 | `				}else{` |
|     13 |  687 | `					SyBlobAppend(&sAr,zAl,SyStrlen((const char *)zAl));` |
|      - |  688 | `				}` |
|     15 |  689 | `				SyBlobAppend(&sAr,"",1);` |
|     15 |  690 | `				XmlDecodeName(p,(const xmlChar *)SyBlobData(&sAr),&sAn);` |
|     15 |  691 | `				XmlStringValue(p,(const char *)zVs,(sxu32)(zVe - zVs),&sVal);` |
|     22 |  692 | `				PH7_HashmapInsertRawKey((ph7_hashmap *)pAttrArr->x.pOther,` |
|     14 |  693 | `					(const char *)SyBlobData(&sAn),SyBlobLength(&sAn),&sVal);` |
|     15 |  694 | `				PH7_MemObjRelease(&sVal);` |
|     15 |  695 | `				SyBlobRelease(&sAn);` |
|     15 |  696 | `				SyBlobRelease(&sAr);` |
|     15 |  697 | `				nAttr++;` |
|      8 |  698 | `			}` |
|     15 |  699 | `			XmlStartElementCommon(p,(const char *)SyBlobData(&sTag),SyBlobLength(&sTag),pAttrArr,nAttr);` |
|     15 |  700 | `			ph7_context_release_value(p->pCallCtx,pAttrArr);` |
|      7 |  701 | `		}` |
|     15 |  702 | `		SyBlobRelease(&sTag);` |
|     15 |  703 | `		SyBlobRelease(&sRaw);` |
|      - |  704 | `	}` |
|      8 |  705 | `}` |
|     14 |  706 | `static void XmlSaxEndElementNs(void *pUser,const xmlChar *zLocal,const xmlChar *zPrefix,` |
|      - |  707 | `	const xmlChar *zUri)` |
|      1 |  708 | `{` |
|     15 |  709 | `	phl_xmlparser *p = (phl_xmlparser *)pUser;` |
|      - |  710 | `	SyBlob sRaw,sTag;` |
|     15 |  711 | `	if( p == 0 ){` |
|    ! 0 |  712 | `		return;` |
|      - |  713 | `	}` |
|     15 |  714 | `	if( !XmlHandlerSet(p,PHL_XML_H_END) && !p->aInstalled[PHL_XML_H_END] && !p->pStructData ){` |
|    ! 0 |  715 | `		XmlEmitRawEndTag(p,zPrefix,zLocal);` |
|    ! 0 |  716 | `		return;` |
|      - |  717 | `	}` |
|     15 |  718 | `	SyBlobInit(&sRaw,&p->pVm->sAllocator);` |
|     15 |  719 | `	SyBlobInit(&sTag,&p->pVm->sAllocator);` |
|     15 |  720 | `	XmlQualifyName(p,zLocal,zUri,&sRaw);` |
|     15 |  721 | `	SyBlobAppend(&sRaw,"",1);` |
|     15 |  722 | `	XmlDecodeName(p,(const xmlChar *)SyBlobData(&sRaw),&sTag);` |
|     15 |  723 | `	XmlEndElementCommon(p,(const char *)SyBlobData(&sTag),SyBlobLength(&sTag));` |
|     15 |  724 | `	SyBlobRelease(&sTag);` |
|     15 |  725 | `	SyBlobRelease(&sRaw);` |
|      8 |  726 | `}` |
|      - |  727 | `/*` |
|      - |  728 | ` * Character data (also CDATA blocks: php wires cdataBlock to the same` |
|      - |  729 | ` * routine). Routes to the cdata handler; with none INSTALLED it falls` |
|      - |  730 | ` * through to the default handler. The into_struct half appends to the` |
|      - |  731 | ` * current open row's "value", or to a trailing cdata row, or starts one --` |
|      - |  732 | ` * with SKIP_WHITE dropping only whitespace-only NEW rows, exactly php.` |
|      - |  733 | ` */` |
|     74 |  734 | `static void XmlSaxCharacters(void *pUser,const xmlChar *zCh,int nLen)` |
|      1 |  735 | `{` |
|     75 |  736 | `	phl_xmlparser *p = (phl_xmlparser *)pUser;` |
|      - |  737 | `	ph7_vm *pVm;` |
|     75 |  738 | `	if( p == 0 \|\| nLen < 0 ){` |
|    ! 0 |  739 | `		return;` |
|      - |  740 | `	}` |
|     75 |  741 | `	pVm = p->pVm;` |
|     92 |  742 | `	if( XmlHandlerSet(p,PHL_XML_H_CDATA) && !p->iCbRc ){` |
|      - |  743 | `		ph7_value sParser,sData;` |
|      - |  744 | `		ph7_value *apArg[2];` |
|     35 |  745 | `		XmlParserValue(p,&sParser);` |
|     35 |  746 | `		XmlStringValue(p,(const char *)zCh,(sxu32)nLen,&sData);` |
|     35 |  747 | `		apArg[0] = &sParser;` |
|     35 |  748 | `		apArg[1] = &sData;` |
|     35 |  749 | `		XmlCallHandler(p,PHL_XML_H_CDATA,2,apArg,0);` |
|     35 |  750 | `		PH7_MemObjRelease(&sParser);` |
|     35 |  751 | `		PH7_MemObjRelease(&sData);` |
|     58 |  752 | `	}else if( !p->aInstalled[PHL_XML_H_CDATA] && !p->pStructData && !p->iCbRc ){` |
|      - |  753 | `		/* No cdata route claimed: the bytes reach the default handler raw. */` |
|      - |  754 | `		ph7_value sParser,sData;` |
|      - |  755 | `		ph7_value *apArg[2];` |
|     15 |  756 | `		if( XmlHandlerSet(p,PHL_XML_H_DEFAULT) ){` |
|      5 |  757 | `			XmlParserValue(p,&sParser);` |
|      5 |  758 | `			XmlStringValue(p,(const char *)zCh,(sxu32)nLen,&sData);` |
|      5 |  759 | `			apArg[0] = &sParser;` |
|      5 |  760 | `			apArg[1] = &sData;` |
|      5 |  761 | `			XmlCallHandler(p,PHL_XML_H_DEFAULT,2,apArg,0);` |
|      5 |  762 | `			PH7_MemObjRelease(&sParser);` |
|      5 |  763 | `			PH7_MemObjRelease(&sData);` |
|      2 |  764 | `		}` |
|      7 |  765 | `	}` |
|     75 |  766 | `	if( p->pStructData == 0 \|\| p->iCbRc ){` |
|     49 |  767 | `		return;` |
|      - |  768 | `	}` |
|      - |  769 | `	{` |
|      - |  770 | `		SyBlob sDec;` |
|     27 |  771 | `		int bPrint = 0;` |
|      - |  772 | `		sxu32 n;` |
|     27 |  773 | `		SyBlobInit(&sDec,&pVm->sAllocator);` |
|     27 |  774 | `		XmlDecodeAppend(p->iTargetEnc,(const char *)zCh,(sxu32)nLen,&sDec);` |
|     27 |  775 | `		if( p->bSkipWhite ){` |
|      7 |  776 | `			const char *z = (const char *)SyBlobData(&sDec);` |
|     15 |  777 | `			for( n = 0 ; n < SyBlobLength(&sDec) ; ++n ){` |
|     11 |  778 | `				if( z[n] != ' ' && z[n] != '\t' && z[n] != '\n' ){` |
|      3 |  779 | `					bPrint = 1;` |
|      3 |  780 | `					break;` |
|      - |  781 | `				}` |
|      5 |  782 | `			}` |
|      3 |  783 | `		}` |
|     37 |  784 | `		if( p->bLastWasOpen && p->pCtagMap ){` |
|     21 |  785 | `			ph7_hashmap_node *pNode = XmlRowFind(pVm,p->pCtagMap,"value");` |
|     21 |  786 | `			if( pNode ){` |
|      - |  787 | `				/* Append to the existing value (php realloc+strncpy). */` |
|      - |  788 | `				ph7_value sOld;` |
|      7 |  789 | `				PH7_MemObjInit(pVm,&sOld);` |
|      7 |  790 | `				PH7_HashmapExtractNodeValue(pNode,&sOld,FALSE);` |
|      7 |  791 | `				if( (sOld.iFlags & MEMOBJ_STRING) != 0 ){` |
|      - |  792 | `					SyBlob sNew;` |
|      7 |  793 | `					SyBlobInit(&sNew,&pVm->sAllocator);` |
|      7 |  794 | `					SyBlobAppend(&sNew,SyBlobData(&sOld.sBlob),SyBlobLength(&sOld.sBlob));` |
|      7 |  795 | `					SyBlobAppend(&sNew,SyBlobData(&sDec),SyBlobLength(&sDec));` |
|     10 |  796 | `					XmlRowAddStr(pVm,p->pCtagMap,"value",` |
|      6 |  797 | `						(const char *)SyBlobData(&sNew),SyBlobLength(&sNew));` |
|      7 |  798 | `					SyBlobRelease(&sNew);` |
|      3 |  799 | `				}` |
|      7 |  800 | `				PH7_MemObjRelease(&sOld);` |
|     18 |  801 | `			}else if( bPrint \|\| !p->bSkipWhite ){` |
|     19 |  802 | `				XmlRowAddStr(pVm,p->pCtagMap,"value",` |
|     12 |  803 | `					(const char *)SyBlobData(&sDec),SyBlobLength(&sDec));` |
|      6 |  804 | `			}` |
|     11 |  805 | `		}else{` |
|      - |  806 | `			/* Not directly after an open: append to a trailing cdata row of the` |
|      - |  807 | `			 * SAME run, else start a new row tagged with the enclosing element. */` |
|      7 |  808 | `			ph7_hashmap *pData = (ph7_hashmap *)p->pStructData->x.pOther;` |
|      7 |  809 | `			ph7_hashmap_node *pLast = pData ? pData->pLast : 0;` |
|      7 |  810 | `			int bAppended = 0;` |
|      7 |  811 | `			if( pLast ){` |
|      - |  812 | `				ph7_value sLastRow;` |
|      7 |  813 | `				PH7_MemObjInit(pVm,&sLastRow);` |
|      7 |  814 | `				PH7_HashmapExtractNodeValue(pLast,&sLastRow,FALSE);` |
|      7 |  815 | `				if( (sLastRow.iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|      7 |  816 | `					ph7_hashmap *pRowMap = (ph7_hashmap *)sLastRow.x.pOther;` |
|      7 |  817 | `					ph7_hashmap_node *pType = XmlRowFind(pVm,pRowMap,"type");` |
|      7 |  818 | `					if( pType ){` |
|      - |  819 | `						ph7_value sType;` |
|      7 |  820 | `						PH7_MemObjInit(pVm,&sType);` |
|      7 |  821 | `						PH7_HashmapExtractNodeValue(pType,&sType,FALSE);` |
|      6 |  822 | `						if( (sType.iFlags & MEMOBJ_STRING) != 0` |
|      6 |  823 | `						 && SyBlobLength(&sType.sBlob) == sizeof("cdata")-1` |
|      5 |  824 | `						 && SyMemcmp(SyBlobData(&sType.sBlob),"cdata",sizeof("cdata")-1) == 0 ){` |
|    ! 0 |  825 | `							ph7_hashmap_node *pVal = XmlRowFind(pVm,pRowMap,"value");` |
|    ! 0 |  826 | `							if( pVal ){` |
|      - |  827 | `								ph7_value sOld;` |
|    ! 0 |  828 | `								PH7_MemObjInit(pVm,&sOld);` |
|    ! 0 |  829 | `								PH7_HashmapExtractNodeValue(pVal,&sOld,FALSE);` |
|    ! 0 |  830 | `								if( (sOld.iFlags & MEMOBJ_STRING) != 0 ){` |
|      - |  831 | `									SyBlob sNew;` |
|    ! 0 |  832 | `									SyBlobInit(&sNew,&pVm->sAllocator);` |
|    ! 0 |  833 | `									SyBlobAppend(&sNew,SyBlobData(&sOld.sBlob),SyBlobLength(&sOld.sBlob));` |
|    ! 0 |  834 | `									SyBlobAppend(&sNew,SyBlobData(&sDec),SyBlobLength(&sDec));` |
|    ! 0 |  835 | `									XmlRowAddStr(pVm,pRowMap,"value",` |
|    ! 0 |  836 | `										(const char *)SyBlobData(&sNew),SyBlobLength(&sNew));` |
|    ! 0 |  837 | `									SyBlobRelease(&sNew);` |
|    ! 0 |  838 | `									bAppended = 1;` |
|    ! 0 |  839 | `								}` |
|    ! 0 |  840 | `								PH7_MemObjRelease(&sOld);` |
|    ! 0 |  841 | `							}` |
|    ! 0 |  842 | `						}` |
|      7 |  843 | `						PH7_MemObjRelease(&sType);` |
|      3 |  844 | `					}` |
|      3 |  845 | `				}` |
|      7 |  846 | `				PH7_MemObjRelease(&sLastRow);` |
|      3 |  847 | `			}` |
|      7 |  848 | `			if( !bAppended ){` |
|      6 |  849 | `				if( p->nLevel > 0 && p->nLevel <= PHL_XML_MAXLEVEL` |
|      9 |  850 | `				 && (bPrint \|\| !p->bSkipWhite) ){` |
|      - |  851 | `					ph7_value sRow;` |
|      5 |  852 | `					PH7_MemObjInit(pVm,&sRow);` |
|      5 |  853 | `					if( SXRET_OK == PH7_MemObjToHashmap(&sRow) ){` |
|      5 |  854 | `						ph7_hashmap *pRow = (ph7_hashmap *)sRow.x.pOther;` |
|      - |  855 | `						SyString sStripped;` |
|      3 |  856 | `						XmlStrippedTag(p->aLtag[p->nLevel - 1].zString ? p->aLtag[p->nLevel - 1].zString : "",` |
|      4 |  857 | `							p->aLtag[p->nLevel - 1].nByte,p->nSkipTagstart,&sStripped);` |
|      5 |  858 | `						XmlAddToInfo(p,&sStripped);` |
|      5 |  859 | `						XmlRowAddStr(pVm,pRow,"tag",sStripped.zString ? sStripped.zString : "",sStripped.nByte);` |
|      7 |  860 | `						XmlRowAddStr(pVm,pRow,"value",` |
|      4 |  861 | `							(const char *)SyBlobData(&sDec),SyBlobLength(&sDec));` |
|      5 |  862 | `						XmlRowAddStr(pVm,pRow,"type","cdata",sizeof("cdata")-1);` |
|      5 |  863 | `						XmlRowAddInt(pVm,pRow,"level",p->nLevel);` |
|      5 |  864 | `						XmlRowAppend(p,&sRow);` |
|      5 |  865 | `						PH7_MemObjRelease(&sRow);` |
|      3 |  866 | `					}` |
|      5 |  867 | `				}else if( p->nLevel == PHL_XML_MAXLEVEL + 1 ){` |
|    ! 0 |  868 | `					ph7_context_throw_error(p->pCallCtx,PH7_CTX_WARNING,` |
|      - |  869 | `						"Maximum depth exceeded - Results truncated");` |
|    ! 0 |  870 | `				}` |
|      3 |  871 | `			}` |
|      - |  872 | `		}` |
|     27 |  873 | `		SyBlobRelease(&sDec);` |
|      - |  874 | `	}` |
|     38 |  875 | `}` |
|      - |  876 | `/* Processing instruction: PI handler, else "<?target data?>" to default. */` |
|      4 |  877 | `static void XmlSaxPi(void *pUser,const xmlChar *zTarget,const xmlChar *zData)` |
|      1 |  878 | `{` |
|      5 |  879 | `	phl_xmlparser *p = (phl_xmlparser *)pUser;` |
|      5 |  880 | `	if( p == 0 ){` |
|    ! 0 |  881 | `		return;` |
|      - |  882 | `	}` |
|      6 |  883 | `	if( XmlHandlerSet(p,PHL_XML_H_PI) \|\| p->aInstalled[PHL_XML_H_PI] ){` |
|      - |  884 | `		ph7_value sParser,sTarget,sData;` |
|      - |  885 | `		ph7_value *apArg[3];` |
|      3 |  886 | `		XmlParserValue(p,&sParser);` |
|      3 |  887 | `		XmlStringValueZ(p,zTarget,&sTarget);` |
|      3 |  888 | `		XmlStringValueZ(p,zData,&sData);` |
|      3 |  889 | `		apArg[0] = &sParser;` |
|      3 |  890 | `		apArg[1] = &sTarget;` |
|      3 |  891 | `		apArg[2] = &sData;` |
|      3 |  892 | `		XmlCallHandler(p,PHL_XML_H_PI,3,apArg,0);` |
|      3 |  893 | `		PH7_MemObjRelease(&sParser);` |
|      3 |  894 | `		PH7_MemObjRelease(&sTarget);` |
|      3 |  895 | `		PH7_MemObjRelease(&sData);` |
|      4 |  896 | `	}else if( XmlHandlerSet(p,PHL_XML_H_DEFAULT) ){` |
|      - |  897 | `		SyBlob sBlob;` |
|      - |  898 | `		ph7_value sParser,sData;` |
|      - |  899 | `		ph7_value *apArg[2];` |
|      3 |  900 | `		SyBlobInit(&sBlob,&p->pVm->sAllocator);` |
|      4 |  901 | `		SyBlobFormat(&sBlob,"<?%s %s?>",zTarget ? (const char *)zTarget : "",` |
|      1 |  902 | `			zData ? (const char *)zData : "");` |
|      3 |  903 | `		XmlParserValue(p,&sParser);` |
|      3 |  904 | `		XmlStringValue(p,(const char *)SyBlobData(&sBlob),SyBlobLength(&sBlob),&sData);` |
|      3 |  905 | `		SyBlobRelease(&sBlob);` |
|      3 |  906 | `		apArg[0] = &sParser;` |
|      3 |  907 | `		apArg[1] = &sData;` |
|      3 |  908 | `		XmlCallHandler(p,PHL_XML_H_DEFAULT,2,apArg,0);` |
|      3 |  909 | `		PH7_MemObjRelease(&sParser);` |
|      3 |  910 | `		PH7_MemObjRelease(&sData);` |
|      1 |  911 | `	}` |
|      3 |  912 | `}` |
|      - |  913 | `/* Comment: "<!--data-->" to the default handler; there is no comment setter. */` |
|      4 |  914 | `static void XmlSaxComment(void *pUser,const xmlChar *zVal)` |
|      1 |  915 | `{` |
|      5 |  916 | `	phl_xmlparser *p = (phl_xmlparser *)pUser;` |
|      - |  917 | `	SyBlob sBlob;` |
|      - |  918 | `	ph7_value sParser,sData;` |
|      - |  919 | `	ph7_value *apArg[2];` |
|      5 |  920 | `	if( p == 0 \|\| !XmlHandlerSet(p,PHL_XML_H_DEFAULT) ){` |
|    ! 0 |  921 | `		return;` |
|      - |  922 | `	}` |
|      5 |  923 | `	SyBlobInit(&sBlob,&p->pVm->sAllocator);` |
|      5 |  924 | `	SyBlobFormat(&sBlob,"<!--%s-->",zVal ? (const char *)zVal : "");` |
|      5 |  925 | `	XmlParserValue(p,&sParser);` |
|      5 |  926 | `	XmlStringValue(p,(const char *)SyBlobData(&sBlob),SyBlobLength(&sBlob),&sData);` |
|      5 |  927 | `	SyBlobRelease(&sBlob);` |
|      5 |  928 | `	apArg[0] = &sParser;` |
|      5 |  929 | `	apArg[1] = &sData;` |
|      5 |  930 | `	XmlCallHandler(p,PHL_XML_H_DEFAULT,2,apArg,0);` |
|      5 |  931 | `	PH7_MemObjRelease(&sParser);` |
|      5 |  932 | `	PH7_MemObjRelease(&sData);` |
|      3 |  933 | `}` |
|      - |  934 | `/* Notation declaration: (parser, name, base=false, systemId, publicId). */` |
|      4 |  935 | `static void XmlSaxNotationDecl(void *pUser,const xmlChar *zName,const xmlChar *zPubId,` |
|      - |  936 | `	const xmlChar *zSysId)` |
|      1 |  937 | `{` |
|      5 |  938 | `	phl_xmlparser *p = (phl_xmlparser *)pUser;` |
|      - |  939 | `	ph7_value sParser,sName,sBase,sSys,sPub;` |
|      - |  940 | `	ph7_value *apArg[5];` |
|      5 |  941 | `	if( p == 0 \|\| !XmlHandlerSet(p,PHL_XML_H_NOTATION) ){` |
|      3 |  942 | `		return;` |
|      - |  943 | `	}` |
|      3 |  944 | `	XmlParserValue(p,&sParser);` |
|      3 |  945 | `	XmlStringValueZ(p,zName,&sName);` |
|      3 |  946 | `	XmlStringValueZ(p,0,&sBase);       /* php hands NULL -> false */` |
|      3 |  947 | `	XmlStringValueZ(p,zSysId,&sSys);` |
|      3 |  948 | `	XmlStringValueZ(p,zPubId,&sPub);` |
|      3 |  949 | `	apArg[0] = &sParser;` |
|      3 |  950 | `	apArg[1] = &sName;` |
|      3 |  951 | `	apArg[2] = &sBase;` |
|      3 |  952 | `	apArg[3] = &sSys;` |
|      3 |  953 | `	apArg[4] = &sPub;` |
|      3 |  954 | `	XmlCallHandler(p,PHL_XML_H_NOTATION,5,apArg,0);` |
|      3 |  955 | `	PH7_MemObjRelease(&sParser);` |
|      3 |  956 | `	PH7_MemObjRelease(&sName);` |
|      3 |  957 | `	PH7_MemObjRelease(&sBase);` |
|      3 |  958 | `	PH7_MemObjRelease(&sSys);` |
|      3 |  959 | `	PH7_MemObjRelease(&sPub);` |
|      3 |  960 | `}` |
|      - |  961 | `/* Unparsed (NDATA) entity: (parser, name, base=false, sysId, pubId, notation). */` |
|      4 |  962 | `static void XmlSaxUnparsedEntityDecl(void *pUser,const xmlChar *zName,const xmlChar *zPubId,` |
|      - |  963 | `	const xmlChar *zSysId,const xmlChar *zNotation)` |
|      1 |  964 | `{` |
|      5 |  965 | `	phl_xmlparser *p = (phl_xmlparser *)pUser;` |
|      - |  966 | `	ph7_value sParser,sName,sBase,sSys,sPub,sNot;` |
|      - |  967 | `	ph7_value *apArg[6];` |
|      5 |  968 | `	if( p == 0 \|\| !XmlHandlerSet(p,PHL_XML_H_UNPARSED) ){` |
|      3 |  969 | `		return;` |
|      - |  970 | `	}` |
|      3 |  971 | `	XmlParserValue(p,&sParser);` |
|      3 |  972 | `	XmlStringValueZ(p,zName,&sName);` |
|      3 |  973 | `	XmlStringValueZ(p,0,&sBase);` |
|      3 |  974 | `	XmlStringValueZ(p,zSysId,&sSys);` |
|      3 |  975 | `	XmlStringValueZ(p,zPubId,&sPub);` |
|      3 |  976 | `	XmlStringValueZ(p,zNotation,&sNot);` |
|      3 |  977 | `	apArg[0] = &sParser;` |
|      3 |  978 | `	apArg[1] = &sName;` |
|      3 |  979 | `	apArg[2] = &sBase;` |
|      3 |  980 | `	apArg[3] = &sSys;` |
|      3 |  981 | `	apArg[4] = &sPub;` |
|      3 |  982 | `	apArg[5] = &sNot;` |
|      3 |  983 | `	XmlCallHandler(p,PHL_XML_H_UNPARSED,6,apArg,0);` |
|      3 |  984 | `	PH7_MemObjRelease(&sParser);` |
|      3 |  985 | `	PH7_MemObjRelease(&sName);` |
|      3 |  986 | `	PH7_MemObjRelease(&sBase);` |
|      3 |  987 | `	PH7_MemObjRelease(&sSys);` |
|      3 |  988 | `	PH7_MemObjRelease(&sPub);` |
|      3 |  989 | `	PH7_MemObjRelease(&sNot);` |
|      3 |  990 | `}` |
|      - |  991 | `/*` |
|      - |  992 | ` * External general entity reference. Fired from the getEntity hook (php's` |
|      - |  993 | ` * compat routes it there, not through resolveEntity). The handler answers` |
|      - |  994 | ` * an int: zero stops the parser and forces expat's error 21, php verbatim` |
|      - |  995 | ` * -- including when the route was claimed and the handler later unset.` |
|      - |  996 | ` */` |
|      4 |  997 | `static void XmlExternalEntityRef(phl_xmlparser *p,const xmlChar *zName,const xmlChar *zSysId,` |
|      - |  998 | `	const xmlChar *zPubId)` |
|      1 |  999 | `{` |
|      5 | 1000 | `	int bOk = 0;` |
|      5 | 1001 | `	if( !p->aInstalled[PHL_XML_H_EXTENT] ){` |
|    ! 0 | 1002 | `		return;` |
|      - | 1003 | `	}` |
|      5 | 1004 | `	if( XmlHandlerSet(p,PHL_XML_H_EXTENT) && !p->iCbRc ){` |
|      - | 1005 | `		ph7_value sParser,sNames,sBase,sSys,sPub,sRet;` |
|      - | 1006 | `		ph7_value *apArg[5];` |
|      5 | 1007 | `		XmlParserValue(p,&sParser);` |
|      5 | 1008 | `		XmlStringValueZ(p,zName,&sNames);` |
|      5 | 1009 | `		XmlStringValue(p,"",0,&sBase);     /* php hands the empty base string */` |
|      5 | 1010 | `		XmlStringValueZ(p,zSysId,&sSys);` |
|      5 | 1011 | `		XmlStringValueZ(p,zPubId,&sPub);` |
|      5 | 1012 | `		PH7_MemObjInit(p->pVm,&sRet);` |
|      5 | 1013 | `		apArg[0] = &sParser;` |
|      5 | 1014 | `		apArg[1] = &sNames;` |
|      5 | 1015 | `		apArg[2] = &sBase;` |
|      5 | 1016 | `		apArg[3] = &sSys;` |
|      5 | 1017 | `		apArg[4] = &sPub;` |
|      5 | 1018 | `		XmlCallHandler(p,PHL_XML_H_EXTENT,5,apArg,&sRet);` |
|      5 | 1019 | `		if( p->iCbRc == 0 ){` |
|      5 | 1020 | `			bOk = ph7_value_to_int64(&sRet) != 0;` |
|      2 | 1021 | `		}` |
|      5 | 1022 | `		PH7_MemObjRelease(&sParser);` |
|      5 | 1023 | `		PH7_MemObjRelease(&sNames);` |
|      5 | 1024 | `		PH7_MemObjRelease(&sBase);` |
|      5 | 1025 | `		PH7_MemObjRelease(&sSys);` |
|      5 | 1026 | `		PH7_MemObjRelease(&sPub);` |
|      5 | 1027 | `		PH7_MemObjRelease(&sRet);` |
|      2 | 1028 | `	}` |
|      5 | 1029 | `	if( !bOk && p->iCbRc == 0 ){` |
|      3 | 1030 | `		xmlStopParser(p->pCtxt);` |
|      3 | 1031 | `		p->pCtxt->errNo = 21; /* expat's XML_ERROR_EXTERNAL_ENTITY_HANDLING */` |
|      1 | 1032 | `	}` |
|      3 | 1033 | `}` |
|      - | 1034 | `/*` |
|      - | 1035 | ` * The entity hook, php's get_entity(): what a reference BECOMES is decided` |
|      - | 1036 | ` * here. Predefined entities expand through the cdata route (libxml inlines` |
|      - | 1037 | ` * them) unless only a default handler listens, in which case the RAW` |
|      - | 1038 | ` * "&name;" goes there; internal entities are never expanded -- raw to the` |
|      - | 1039 | ` * default handler, or their replacement text to the cdata handler when no` |
|      - | 1040 | ` * default one listens; an unknown name also defaults raw and then fails the` |
|      - | 1041 | ` * parse (libxml raises 26 on the NULL return); an external entity routes` |
|      - | 1042 | ` * to the external-entity-ref handler.` |
|      - | 1043 | ` */` |
|     38 | 1044 | `static xmlEntityPtr XmlSaxGetEntity(void *pUser,const xmlChar *zName)` |
|      1 | 1045 | `{` |
|     39 | 1046 | `	phl_xmlparser *p = (phl_xmlparser *)pUser;` |
|     39 | 1047 | `	xmlEntityPtr pEnt = 0;` |
|     39 | 1048 | `	if( p == 0 \|\| p->pCtxt == 0 ){` |
|    ! 0 | 1049 | `		return 0;` |
|      - | 1050 | `	}` |
|     39 | 1051 | `	if( p->pCtxt->inSubset == 0 ){` |
|     27 | 1052 | `		pEnt = xmlGetPredefinedEntity(zName);` |
|     27 | 1053 | `		if( pEnt == 0 ){` |
|     13 | 1054 | `			pEnt = xmlGetDocEntity(p->pCtxt->myDoc,zName);` |
|      6 | 1055 | `		}` |
|     27 | 1056 | `		if( pEnt == 0 \|\| p->pCtxt->instate == XML_PARSER_CONTENT ){` |
|     14 | 1057 | `			if( pEnt == 0` |
|     13 | 1058 | `			 \|\| pEnt->etype == XML_INTERNAL_GENERAL_ENTITY` |
|     11 | 1059 | `			 \|\| pEnt->etype == XML_INTERNAL_PARAMETER_ENTITY` |
|     16 | 1060 | `			 \|\| pEnt->etype == XML_INTERNAL_PREDEFINED_ENTITY ){` |
|     11 | 1061 | `				int bDefault = XmlHandlerSet(p,PHL_XML_H_DEFAULT) \|\| p->aInstalled[PHL_XML_H_DEFAULT];` |
|     11 | 1062 | `				int bCdata = XmlHandlerSet(p,PHL_XML_H_CDATA) \|\| p->aInstalled[PHL_XML_H_CDATA];` |
|     10 | 1063 | `				if( bDefault` |
|     13 | 1064 | `				 && !(pEnt && pEnt->etype == XML_INTERNAL_PREDEFINED_ENTITY && bCdata) ){` |
|      - | 1065 | `					SyBlob sBlob;` |
|      - | 1066 | `					ph7_value sParser,sData;` |
|      - | 1067 | `					ph7_value *apArg[2];` |
|      7 | 1068 | `					SyBlobInit(&sBlob,&p->pVm->sAllocator);` |
|      7 | 1069 | `					SyBlobFormat(&sBlob,"&%s;",(const char *)zName);` |
|      7 | 1070 | `					XmlParserValue(p,&sParser);` |
|      7 | 1071 | `					XmlStringValue(p,(const char *)SyBlobData(&sBlob),SyBlobLength(&sBlob),&sData);` |
|      7 | 1072 | `					SyBlobRelease(&sBlob);` |
|      7 | 1073 | `					apArg[0] = &sParser;` |
|      7 | 1074 | `					apArg[1] = &sData;` |
|      7 | 1075 | `					XmlCallHandler(p,PHL_XML_H_DEFAULT,2,apArg,0);` |
|      7 | 1076 | `					PH7_MemObjRelease(&sParser);` |
|      7 | 1077 | `					PH7_MemObjRelease(&sData);` |
|      8 | 1078 | `				}else if( bCdata && pEnt && pEnt->content ){` |
|      - | 1079 | `					/* No default route: the entity's replacement text reaches` |
|      - | 1080 | `					 * the cdata ROUTE from here -- under OLDSAX libxml inlines` |
|      - | 1081 | `					 * nothing itself, predefined entities included (php's` |
|      - | 1082 | `					 * comment: expat expands and hands it over). Through the` |
|      - | 1083 | `					 * full character-data front so an into_struct value picks` |
|      - | 1084 | `					 * the text up too, exactly as php's h_cdata pointer does. */` |
|      7 | 1085 | `					XmlSaxCharacters(pUser,pEnt->content,` |
|      4 | 1086 | `						(int)SyStrlen((const char *)pEnt->content));` |
|      3 | 1087 | `				}` |
|     10 | 1088 | `			}else if( pEnt->etype == XML_EXTERNAL_GENERAL_PARSED_ENTITY ){` |
|      5 | 1089 | `				XmlExternalEntityRef(p,pEnt->name,pEnt->SystemID,pEnt->ExternalID);` |
|      2 | 1090 | `			}` |
|      7 | 1091 | `		}` |
|     13 | 1092 | `	}` |
|     39 | 1093 | `	return pEnt;` |
|     20 | 1094 | `}` |
|      - | 1095 |  |
|      - | 1096 | `/* ------------------------------------------------------------------------` |
|      - | 1097 | ` * Parser lifecycle.` |
|      - | 1098 | ` * ------------------------------------------------------------------------ */` |
|     82 | 1099 | `static void XmlInitSaxHandler(xmlSAXHandler *pSax)` |
|      3 | 1100 | `{` |
|     85 | 1101 | `	SyZero(pSax,sizeof(xmlSAXHandler));` |
|     85 | 1102 | `	pSax->getEntity = XmlSaxGetEntity;` |
|     85 | 1103 | `	pSax->notationDecl = XmlSaxNotationDecl;` |
|     85 | 1104 | `	pSax->unparsedEntityDecl = XmlSaxUnparsedEntityDecl;` |
|     85 | 1105 | `	pSax->startElement = XmlSaxStartElement;` |
|     85 | 1106 | `	pSax->endElement = XmlSaxEndElement;` |
|     85 | 1107 | `	pSax->characters = XmlSaxCharacters;` |
|     85 | 1108 | `	pSax->processingInstruction = XmlSaxPi;` |
|     85 | 1109 | `	pSax->comment = XmlSaxComment;` |
|     85 | 1110 | `	pSax->cdataBlock = XmlSaxCharacters;` |
|     85 | 1111 | `	pSax->initialized = XML_SAX2_MAGIC;` |
|     85 | 1112 | `	pSax->startElementNs = XmlSaxStartElementNs;` |
|     85 | 1113 | `	pSax->endElementNs = XmlSaxEndElementNs;` |
|     85 | 1114 | `}` |
|      - | 1115 | `/* Free one parser's libxml half (php's XML_ParserFree order: doc, ctxt). */` |
|     82 | 1116 | `static void XmlParserFreeC(phl_xmlparser *p)` |
|      3 | 1117 | `{` |
|     85 | 1118 | `	if( p->pCtxt ){` |
|     85 | 1119 | `		if( p->pCtxt->myDoc ){` |
|     10 | 1120 | `			xmlFreeDoc(p->pCtxt->myDoc);` |
|     10 | 1121 | `			p->pCtxt->myDoc = 0;` |
|      1 | 1122 | `		}` |
|     85 | 1123 | `		xmlFreeParserCtxt(p->pCtxt);` |
|     85 | 1124 | `		p->pCtxt = 0;` |
|     41 | 1125 | `	}` |
|     85 | 1126 | `}` |
|      - | 1127 | `/* Registry sweep, called from PH7_LibxmlVmReset (VM reset AND release). */` |
|   6717 | 1128 | `PH7_PRIVATE void PH7_XmlParserVmSweep(ph7_vm *pVm)` |
|      5 | 1129 | `{` |
|   6722 | 1130 | `	phl_xmlparser *p = (phl_xmlparser *)pVm->pXmlParsers;` |
|   6804 | 1131 | `	while( p ){` |
|     85 | 1132 | `		phl_xmlparser *pNext = p->pNext;` |
|      - | 1133 | `		int i;` |
|    905 | 1134 | `		for( i = 0 ; i < PHL_XML_H_COUNT ; ++i ){` |
|    823 | 1135 | `			PH7_MemObjRelease(&p->aHandler[i]);` |
|    413 | 1136 | `		}` |
|     85 | 1137 | `		XmlFreeLtags(p);` |
|     85 | 1138 | `		XmlParserFreeC(p);` |
|     85 | 1139 | `		SyMemBackendFree(&pVm->sAllocator,p);` |
|     85 | 1140 | `		p = pNext;` |
|      3 | 1141 | `	}` |
|   6722 | 1142 | `	pVm->pXmlParsers = 0;` |
|   6722 | 1143 | `}` |
|      - | 1144 | `/* The struct behind an XMLParser argument's hidden slot. The declared` |
|      - | 1145 | `` * `XMLParser $parser` type has already refused everything else. */`` |
|    270 | 1146 | `static phl_xmlparser * XmlParserOf(ph7_value *pArg,ph7_class_instance **ppObj)` |
|      3 | 1147 | `{` |
|      - | 1148 | `	ph7_class_instance *pThis;` |
|      - | 1149 | `	SyString sAttr;` |
|      - | 1150 | `	ph7_value *pRes;` |
|    273 | 1151 | `	if( pArg == 0 \|\| (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 1152 | `		return 0;` |
|      - | 1153 | `	}` |
|    273 | 1154 | `	pThis = (ph7_class_instance *)pArg->x.pOther;` |
|    273 | 1155 | `	SyStringInitFromBuf(&sAttr,"__p",sizeof("__p")-1);` |
|    273 | 1156 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|    273 | 1157 | `	if( pRes == 0 \|\| (pRes->iFlags & MEMOBJ_RES) == 0 ){` |
|    ! 0 | 1158 | `		return 0;` |
|      - | 1159 | `	}` |
|    273 | 1160 | `	if( ppObj ){` |
|     85 | 1161 | `		*ppObj = pThis;` |
|     41 | 1162 | `	}` |
|    273 | 1163 | `	return (phl_xmlparser *)pRes->x.pOther;` |
|    138 | 1164 | `}` |
|      - | 1165 | `/*` |
|      - | 1166 | ` * xml_parser_create() / xml_parser_create_ns() -- shared body.` |
|      - | 1167 | ` * The $encoding names one of the three php supports (or nothing, which is` |
|      - | 1168 | ` * UTF-8); it decides the TARGET encoding, the input side is auto-detected` |
|      - | 1169 | ` * by libxml exactly as in php.` |
|      - | 1170 | ` */` |
|     84 | 1171 | `static int XmlParserCreateImpl(ph7_context *pCtx,int nArg,ph7_value **apArg,int bNs)` |
|      3 | 1172 | `{` |
|     87 | 1173 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1174 | `	phl_xmlparser *p;` |
|      - | 1175 | `	ph7_class *pClass;` |
|      - | 1176 | `	ph7_class_instance *pThis;` |
|      - | 1177 | `	xmlSAXHandler sSax;` |
|     87 | 1178 | `	int iTargetEnc = PHL_XML_ENC_UTF8;` |
|     87 | 1179 | `	xmlChar cSep = ':';` |
|     87 | 1180 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|     19 | 1181 | `		int nEnc = 0;` |
|     19 | 1182 | `		const char *zEnc = ph7_value_to_string(apArg[0],&nEnc);` |
|     19 | 1183 | `		if( nEnc == 0 ){` |
|      3 | 1184 | `			iTargetEnc = PHL_XML_ENC_UTF8;` |
|     18 | 1185 | `		}else if( nEnc == (int)sizeof("ISO-8859-1")-1 && SyStrnicmp(zEnc,"ISO-8859-1",(sxu32)nEnc) == 0 ){` |
|      5 | 1186 | `			iTargetEnc = PHL_XML_ENC_ISO88591;` |
|     15 | 1187 | `		}else if( nEnc == (int)sizeof("UTF-8")-1 && SyStrnicmp(zEnc,"UTF-8",(sxu32)nEnc) == 0 ){` |
|      9 | 1188 | `			iTargetEnc = PHL_XML_ENC_UTF8;` |
|      9 | 1189 | `		}else if( nEnc == (int)sizeof("US-ASCII")-1 && SyStrnicmp(zEnc,"US-ASCII",(sxu32)nEnc) == 0 ){` |
|      3 | 1190 | `			iTargetEnc = PHL_XML_ENC_ASCII;` |
|      2 | 1191 | `		}else{` |
|      4 | 1192 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1193 | `				"%s(): Argument #1 ($encoding) is not a supported source encoding",` |
|      1 | 1194 | `				ph7_function_name(pCtx));` |
|      - | 1195 | `		}` |
|      8 | 1196 | `	}` |
|     85 | 1197 | `	if( bNs && nArg > 1 ){` |
|      7 | 1198 | `		int nSep = 0;` |
|      7 | 1199 | `		const char *zSep = ph7_value_to_string(apArg[1],&nSep);` |
|      - | 1200 | `		/* php reads ONE byte: xml_parser_create_ns($e,"##") separates with` |
|      - | 1201 | `		 * '#', and the empty string joins URI and local name directly. */` |
|      7 | 1202 | `		cSep = nSep > 0 ? (xmlChar)zSep[0] : (xmlChar)'\0';` |
|      3 | 1203 | `	}` |
|     85 | 1204 | `	p = (phl_xmlparser *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_xmlparser));` |
|     85 | 1205 | `	if( p == 0 ){` |
|    ! 0 | 1206 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1207 | `	}` |
|     85 | 1208 | `	SyZero(p,sizeof(phl_xmlparser));` |
|     85 | 1209 | `	p->pVm = pVm;` |
|     85 | 1210 | `	p->bNs = bNs;` |
|     85 | 1211 | `	p->cSep = bNs ? cSep : (xmlChar)0;` |
|     85 | 1212 | `	p->bCaseFolding = 1;` |
|     85 | 1213 | `	p->iTargetEnc = iTargetEnc;` |
|      - | 1214 | `	{` |
|      - | 1215 | `		int i;` |
|    905 | 1216 | `		for( i = 0 ; i < PHL_XML_H_COUNT ; ++i ){` |
|    823 | 1217 | `			PH7_MemObjInit(pVm,&p->aHandler[i]);` |
|    413 | 1218 | `		}` |
|      - | 1219 | `	}` |
|     85 | 1220 | `	XmlInitSaxHandler(&sSax);` |
|     85 | 1221 | `	p->pCtxt = xmlCreatePushParserCtxt(&sSax,(void *)p,0,0,0);` |
|     85 | 1222 | `	if( p->pCtxt == 0 ){` |
|    ! 0 | 1223 | `		SyMemBackendFree(&pVm->sAllocator,p);` |
|    ! 0 | 1224 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1225 | `	}` |
|      - | 1226 | `	/* php's setup: sanitize the inherited ctxt state, then run with` |
|      - | 1227 | `	 * OLDSAX+NOENT -- attributes expand entities, content references do` |
|      - | 1228 | `	 * not, and the getEntity hook decides everything else. Fields are set` |
|      - | 1229 | `	 * directly rather than through xmlCtxtUseOptions() so newer libxml's` |
|      - | 1230 | `	 * deprecation attribute on that function cannot break a /WX build. */` |
|     85 | 1231 | `	p->pCtxt->loadsubset = 0;` |
|     85 | 1232 | `	p->pCtxt->validate = 0;` |
|     85 | 1233 | `	p->pCtxt->pedantic = 0;` |
|     85 | 1234 | `	p->pCtxt->linenumbers = 0;` |
|     85 | 1235 | `	p->pCtxt->keepBlanks = 1;` |
|     85 | 1236 | `	p->pCtxt->options = XML_PARSE_OLDSAX \| XML_PARSE_NOENT;` |
|     85 | 1237 | `	p->pCtxt->replaceEntities = 1;` |
|     85 | 1238 | `	p->pCtxt->wellFormed = 0;` |
|     85 | 1239 | `	if( !bNs ){` |
|      - | 1240 | `		/* SAX1 dispatch: clearing the magic makes libxml use the raw` |
|      - | 1241 | `		 * startElement/endElement pair, which is where document-order` |
|      - | 1242 | `		 * attributes (xmlns included) come from. php's exact move. */` |
|     75 | 1243 | `		p->pCtxt->sax->initialized = 1;` |
|     36 | 1244 | `	}` |
|     85 | 1245 | `	pClass = PH7_VmExtractClass(pVm,"XMLParser",sizeof("XMLParser")-1,FALSE,0);` |
|     85 | 1246 | `	pThis = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|     85 | 1247 | `	if( pThis == 0 ){` |
|    ! 0 | 1248 | `		XmlParserFreeC(p);` |
|    ! 0 | 1249 | `		SyMemBackendFree(&pVm->sAllocator,p);` |
|    ! 0 | 1250 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1251 | `	}` |
|      - | 1252 | `	{` |
|      - | 1253 | `		SyString sAttr;` |
|      - | 1254 | `		ph7_value *pRes;` |
|     85 | 1255 | `		SyStringInitFromBuf(&sAttr,"__p",sizeof("__p")-1);` |
|     85 | 1256 | `		pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|     85 | 1257 | `		if( pRes == 0 ){` |
|    ! 0 | 1258 | `			XmlParserFreeC(p);` |
|    ! 0 | 1259 | `			SyMemBackendFree(&pVm->sAllocator,p);` |
|    ! 0 | 1260 | `			PH7_ClassInstanceUnref(pThis);` |
|    ! 0 | 1261 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1262 | `		}` |
|     85 | 1263 | `		PH7_MemObjRelease(pRes);` |
|     85 | 1264 | `		pRes->x.pOther = p;` |
|     85 | 1265 | `		MemObjSetType(pRes,MEMOBJ_RES);` |
|      - | 1266 | `	}` |
|     85 | 1267 | `	p->pNext = (phl_xmlparser *)pVm->pXmlParsers;` |
|     85 | 1268 | `	pVm->pXmlParsers = (void *)p;` |
|     85 | 1269 | `	PH7_NativeResultObject(pCtx,pThis);` |
|     85 | 1270 | `	return PH7_OK;` |
|     45 | 1271 | `}` |
|      - | 1272 | `/* XMLParser xml_parser_create(?string $encoding = null) */` |
|     74 | 1273 | `static int vm_builtin_xml_parser_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1274 | `{` |
|     77 | 1275 | `	return XmlParserCreateImpl(pCtx,nArg,apArg,0);` |
|      3 | 1276 | `}` |
|      - | 1277 | `/* XMLParser xml_parser_create_ns(?string $encoding = null, string $separator = ":") */` |
|     10 | 1278 | `static int vm_builtin_xml_parser_create_ns(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1279 | `{` |
|     11 | 1280 | `	return XmlParserCreateImpl(pCtx,nArg,apArg,1);` |
|      1 | 1281 | `}` |
|      - | 1282 |  |
|      - | 1283 | `/* ------------------------------------------------------------------------` |
|      - | 1284 | ` * Handler setters.` |
|      - | 1285 | ` * ------------------------------------------------------------------------ */` |
|      - | 1286 | `/*` |
|      - | 1287 | ` * Screen ONE handler argument and store it. php 8.4 deprecates the` |
|      - | 1288 | ` * method-name-string spelling (looked up on an xml_set_object() receiver);` |
|      - | 1289 | ` * PHL removes xml_set_object() and refuses a non-callable string with` |
|      - | 1290 | `` * php's callback TypeError instead (scope policy `10, twin-paired). A`` |
|      - | 1291 | ` * callable STRING like "strlen" is an ordinary callable and passes.` |
|      - | 1292 | ` */` |
|    124 | 1293 | `static sxi32 XmlStoreHandlerArg(ph7_context *pCtx,phl_xmlparser *p,int iH,` |
|      - | 1294 | `	ph7_value *pArg,int iArgPos,const char *zParam)` |
|      2 | 1295 | `{` |
|    126 | 1296 | `	if( !ph7_value_is_null(pArg) ){` |
|     98 | 1297 | `		sxi32 rc = PH7_CheckCallbackArg(pCtx,pArg,iArgPos,zParam,1);` |
|     98 | 1298 | `		if( rc != PH7_OK ){` |
|      5 | 1299 | `			return rc;` |
|      - | 1300 | `		}` |
|     46 | 1301 | `	}` |
|    122 | 1302 | `	PH7_MemObjRelease(&p->aHandler[iH]);` |
|    122 | 1303 | `	if( !ph7_value_is_null(pArg) ){` |
|     94 | 1304 | `		PH7_MemObjStore(pArg,&p->aHandler[iH]);` |
|     46 | 1305 | `	}` |
|    122 | 1306 | `	p->aInstalled[iH] = 1;` |
|    122 | 1307 | `	return PH7_OK;` |
|     64 | 1308 | `}` |
|      - | 1309 | `/* true xml_set_element_handler(XMLParser $parser, callable\|string\|null $start_handler,` |
|      - | 1310 | ` *                              callable\|string\|null $end_handler) */` |
|     40 | 1311 | `static int vm_builtin_xml_set_element_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1312 | `{` |
|     42 | 1313 | `	phl_xmlparser *p = nArg > 0 ? XmlParserOf(apArg[0],0) : 0;` |
|      - | 1314 | `	sxi32 rc;` |
|     42 | 1315 | `	if( p == 0 \|\| nArg < 3 ){` |
|    ! 0 | 1316 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1317 | `		return PH7_OK;` |
|      - | 1318 | `	}` |
|     42 | 1319 | `	rc = XmlStoreHandlerArg(pCtx,p,PHL_XML_H_START,apArg[1],2,"start_handler");` |
|     42 | 1320 | `	if( rc != PH7_OK ){` |
|      3 | 1321 | `		return rc;` |
|      - | 1322 | `	}` |
|     40 | 1323 | `	rc = XmlStoreHandlerArg(pCtx,p,PHL_XML_H_END,apArg[2],3,"end_handler");` |
|     40 | 1324 | `	if( rc != PH7_OK ){` |
|    ! 0 | 1325 | `		return rc;` |
|      - | 1326 | `	}` |
|     40 | 1327 | `	ph7_result_bool(pCtx,1);` |
|     40 | 1328 | `	return PH7_OK;` |
|     22 | 1329 | `}` |
|     46 | 1330 | `static int XmlSetOneHandler(ph7_context *pCtx,int nArg,ph7_value **apArg,int iH)` |
|      2 | 1331 | `{` |
|     48 | 1332 | `	phl_xmlparser *p = nArg > 0 ? XmlParserOf(apArg[0],0) : 0;` |
|      - | 1333 | `	sxi32 rc;` |
|     48 | 1334 | `	if( p == 0 \|\| nArg < 2 ){` |
|    ! 0 | 1335 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1336 | `		return PH7_OK;` |
|      - | 1337 | `	}` |
|     48 | 1338 | `	rc = XmlStoreHandlerArg(pCtx,p,iH,apArg[1],2,"handler");` |
|     48 | 1339 | `	if( rc != PH7_OK ){` |
|      3 | 1340 | `		return rc;` |
|      - | 1341 | `	}` |
|     45 | 1342 | `	ph7_result_bool(pCtx,1);` |
|     45 | 1343 | `	return PH7_OK;` |
|     25 | 1344 | `}` |
|     24 | 1345 | `static int vm_builtin_xml_set_character_data_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1346 | `{` |
|     26 | 1347 | `	return XmlSetOneHandler(pCtx,nArg,apArg,PHL_XML_H_CDATA);` |
|      2 | 1348 | `}` |
|      2 | 1349 | `static int vm_builtin_xml_set_processing_instruction_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1350 | `{` |
|      3 | 1351 | `	return XmlSetOneHandler(pCtx,nArg,apArg,PHL_XML_H_PI);` |
|      1 | 1352 | `}` |
|      8 | 1353 | `static int vm_builtin_xml_set_default_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1354 | `{` |
|      9 | 1355 | `	return XmlSetOneHandler(pCtx,nArg,apArg,PHL_XML_H_DEFAULT);` |
|      1 | 1356 | `}` |
|      2 | 1357 | `static int vm_builtin_xml_set_unparsed_entity_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1358 | `{` |
|      3 | 1359 | `	return XmlSetOneHandler(pCtx,nArg,apArg,PHL_XML_H_UNPARSED);` |
|      1 | 1360 | `}` |
|      2 | 1361 | `static int vm_builtin_xml_set_notation_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1362 | `{` |
|      3 | 1363 | `	return XmlSetOneHandler(pCtx,nArg,apArg,PHL_XML_H_NOTATION);` |
|      1 | 1364 | `}` |
|      4 | 1365 | `static int vm_builtin_xml_set_external_entity_ref_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1366 | `{` |
|      5 | 1367 | `	return XmlSetOneHandler(pCtx,nArg,apArg,PHL_XML_H_EXTENT);` |
|      1 | 1368 | `}` |
|      2 | 1369 | `static int vm_builtin_xml_set_start_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1370 | `{` |
|      3 | 1371 | `	return XmlSetOneHandler(pCtx,nArg,apArg,PHL_XML_H_NSSTART);` |
|      1 | 1372 | `}` |
|      2 | 1373 | `static int vm_builtin_xml_set_end_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1374 | `{` |
|      - | 1375 | `	/* Accepted and stored; php's libxml layer never fires this event. */` |
|      3 | 1376 | `	return XmlSetOneHandler(pCtx,nArg,apArg,PHL_XML_H_NSEND);` |
|      1 | 1377 | `}` |
|      - | 1378 |  |
|      - | 1379 | `/* ------------------------------------------------------------------------` |
|      - | 1380 | ` * Parsing.` |
|      - | 1381 | ` * ------------------------------------------------------------------------ */` |
|      - | 1382 | `/*` |
|      - | 1383 | ` * php's XML_Parse + xml_parse_helper: apply the PARSE_HUGE decision, feed` |
|      - | 1384 | ` * the chunk, then answer 1 only when neither the chunk call nor the ctxt's` |
|      - | 1385 | ` * recorded last error says otherwise. A handler that threw makes the whole` |
|      - | 1386 | ` * builtin return the parked status so the throw resumes at the CALLER of` |
|      - | 1387 | ` * xml_parse, php's timing.` |
|      - | 1388 | ` */` |
|     78 | 1389 | `static sxi32 XmlParseChunkImpl(ph7_context *pCtx,phl_xmlparser *p,ph7_class_instance *pThis,` |
|      - | 1390 | `	const char *zData,int nData,int bFinal,int *pAnswer)` |
|      3 | 1391 | `{` |
|      - | 1392 | `	int iErr;` |
|     81 | 1393 | `	if( p->bParseHuge ){` |
|    ! 0 | 1394 | `		p->pCtxt->options \|= XML_PARSE_HUGE;` |
|    ! 0 | 1395 | `		xmlDictSetLimit(p->pCtxt->dict,0);` |
|    ! 0 | 1396 | `	}else{` |
|     81 | 1397 | `		p->pCtxt->options &= ~XML_PARSE_HUGE;` |
|     81 | 1398 | `		xmlDictSetLimit(p->pCtxt->dict,XML_MAX_DICTIONARY_LIMIT);` |
|      - | 1399 | `	}` |
|     81 | 1400 | `	p->bParsing = 1;` |
|     81 | 1401 | `	p->pCallCtx = pCtx;` |
|     81 | 1402 | `	p->pObj = pThis;` |
|     81 | 1403 | `	p->iCbRc = 0;` |
|     81 | 1404 | `	iErr = xmlParseChunk(p->pCtxt,zData,nData,bFinal);` |
|     81 | 1405 | `	p->bParsing = 0;` |
|     81 | 1406 | `	p->pCallCtx = 0;` |
|     81 | 1407 | `	p->pObj = 0;` |
|     81 | 1408 | `	if( p->iCbRc ){` |
|      3 | 1409 | `		return p->iCbRc;` |
|      - | 1410 | `	}` |
|     79 | 1411 | `	if( iErr ){` |
|     13 | 1412 | `		*pAnswer = 0;` |
|      7 | 1413 | `	}else{` |
|     67 | 1414 | `		const xmlError *pLast = xmlCtxtGetLastError(p->pCtxt);` |
|     67 | 1415 | `		*pAnswer = (pLast == 0 \|\| pLast->level <= XML_ERR_WARNING) ? 1 : 0;` |
|      - | 1416 | `	}` |
|     79 | 1417 | `	return PH7_OK;` |
|     42 | 1418 | `}` |
|      - | 1419 | `/* int xml_parse(XMLParser $parser, string $data, bool $is_final = false) */` |
|     68 | 1420 | `static int vm_builtin_xml_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1421 | `{` |
|     71 | 1422 | `	ph7_class_instance *pThis = 0;` |
|     71 | 1423 | `	phl_xmlparser *p = nArg > 0 ? XmlParserOf(apArg[0],&pThis) : 0;` |
|      - | 1424 | `	const char *zData;` |
|     71 | 1425 | `	int nData = 0;` |
|      - | 1426 | `	int bFinal;` |
|     71 | 1427 | `	int iAnswer = 0;` |
|      - | 1428 | `	sxi32 rc;` |
|     71 | 1429 | `	if( p == 0 \|\| nArg < 2 ){` |
|    ! 0 | 1430 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 1431 | `		return PH7_OK;` |
|      - | 1432 | `	}` |
|     71 | 1433 | `	if( p->bParsing ){` |
|      3 | 1434 | `		return PH7_VmThrowException(pCtx,"Error","Parser must not be called recursively");` |
|      - | 1435 | `	}` |
|     69 | 1436 | `	zData = ph7_value_to_string(apArg[1],&nData);` |
|     69 | 1437 | `	bFinal = nArg > 2 ? ph7_value_to_bool(apArg[2]) : 0;` |
|     69 | 1438 | `	rc = XmlParseChunkImpl(pCtx,p,pThis,zData,nData,bFinal,&iAnswer);` |
|     69 | 1439 | `	if( rc != PH7_OK ){` |
|      3 | 1440 | `		return rc;` |
|      - | 1441 | `	}` |
|     67 | 1442 | `	ph7_result_int(pCtx,iAnswer);` |
|     67 | 1443 | `	return PH7_OK;` |
|     37 | 1444 | `}` |
|      - | 1445 | `/* int\|false xml_parse_into_struct(XMLParser $parser, string $data, &$values, &$index = null) */` |
|     14 | 1446 | `static int vm_builtin_xml_parse_into_struct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1447 | `{` |
|     15 | 1448 | `	ph7_vm *pVm = pCtx->pVm;` |
|     15 | 1449 | `	ph7_class_instance *pThis = 0;` |
|     15 | 1450 | `	phl_xmlparser *p = nArg > 0 ? XmlParserOf(apArg[0],&pThis) : 0;` |
|      - | 1451 | `	const char *zData;` |
|     15 | 1452 | `	int nData = 0;` |
|     15 | 1453 | `	int iAnswer = 0;` |
|      - | 1454 | `	sxi32 rc;` |
|     15 | 1455 | `	ph7_value *pValues,*pIndex = 0;` |
|     15 | 1456 | `	if( p == 0 \|\| nArg < 3 ){` |
|    ! 0 | 1457 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1458 | `		return PH7_OK;` |
|      - | 1459 | `	}` |
|     15 | 1460 | `	if( p->bParsing ){` |
|      - | 1461 | `		/* php demotes THIS spelling of the recursion refusal to a warning. */` |
|      3 | 1462 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Parser must not be called recursively");` |
|      3 | 1463 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1464 | `		return PH7_OK;` |
|      - | 1465 | `	}` |
|     13 | 1466 | `	zData = ph7_value_to_string(apArg[1],&nData);` |
|     13 | 1467 | `	pValues = ph7_context_new_array(pCtx);` |
|     13 | 1468 | `	if( nArg > 3 ){` |
|      9 | 1469 | `		pIndex = ph7_context_new_array(pCtx);` |
|      4 | 1470 | `	}` |
|     13 | 1471 | `	if( pValues == 0 \|\| (nArg > 3 && pIndex == 0) ){` |
|    ! 0 | 1472 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1473 | `	}` |
|     13 | 1474 | `	p->pStructData = pValues;` |
|     13 | 1475 | `	p->pStructIndex = pIndex;` |
|     13 | 1476 | `	p->nLevel = 0;` |
|     13 | 1477 | `	p->bLastWasOpen = 0;` |
|     13 | 1478 | `	p->pCtagMap = 0;` |
|     13 | 1479 | `	XmlFreeLtags(p);` |
|      - | 1480 | `	/* php's into_struct claims the element and cdata routes for good. */` |
|     13 | 1481 | `	p->aInstalled[PHL_XML_H_START] = 1;` |
|     13 | 1482 | `	p->aInstalled[PHL_XML_H_END] = 1;` |
|     13 | 1483 | `	p->aInstalled[PHL_XML_H_CDATA] = 1;` |
|     13 | 1484 | `	rc = XmlParseChunkImpl(pCtx,p,pThis,zData,nData,1,&iAnswer);` |
|     13 | 1485 | `	p->pStructData = 0;` |
|     13 | 1486 | `	p->pStructIndex = 0;` |
|     13 | 1487 | `	p->pCtagMap = 0;` |
|      - | 1488 | `	/* The by-ref answers are written even when a handler threw: php builds` |
|      - | 1489 | `	 * them live in the caller's variable, so a caught throw still leaves` |
|      - | 1490 | `	 * the rows recorded so far. */` |
|     13 | 1491 | `	PH7_VmStoreArgByRef(pVm,apArg[2],pValues);` |
|     13 | 1492 | `	if( pIndex ){` |
|      9 | 1493 | `		PH7_VmStoreArgByRef(pVm,apArg[3],pIndex);` |
|      4 | 1494 | `	}` |
|     13 | 1495 | `	if( rc != PH7_OK ){` |
|    ! 0 | 1496 | `		return rc;` |
|      - | 1497 | `	}` |
|     13 | 1498 | `	ph7_result_int(pCtx,iAnswer);` |
|     13 | 1499 | `	return PH7_OK;` |
|      8 | 1500 | `}` |
|      - | 1501 |  |
|      - | 1502 | `/* ------------------------------------------------------------------------` |
|      - | 1503 | ` * Errors, positions, options.` |
|      - | 1504 | ` * ------------------------------------------------------------------------ */` |
|      - | 1505 | `/* int xml_get_error_code(XMLParser $parser) -- the RAW libxml errNo. */` |
|     20 | 1506 | `static int vm_builtin_xml_get_error_code(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1507 | `{` |
|     21 | 1508 | `	phl_xmlparser *p = nArg > 0 ? XmlParserOf(apArg[0],0) : 0;` |
|     21 | 1509 | `	ph7_result_int(pCtx,p && p->pCtxt ? p->pCtxt->errNo : 0);` |
|     21 | 1510 | `	return PH7_OK;` |
|      1 | 1511 | `}` |
|      - | 1512 | `/*` |
|      - | 1513 | ` * php's expat-flavoured error table, indexed by LIBXML error code (see the` |
|      - | 1514 | ` * header note: the XML_ERROR_* constants and this table disagree past the` |
|      - | 1515 | ` * first few entries, and that mismatch is php's shipped behaviour). Entries` |
|      - | 1516 | ` * whose libxml condition php never translated keep the raw enum spelling.` |
|      - | 1517 | ` */` |
|      - | 1518 | `static const char * const azXmlErrorMap[] = {` |
|      - | 1519 | `	"No error", "No memory", "Invalid document start", "Empty document",` |
|      - | 1520 | `	"Not well-formed (invalid token)", "Invalid document end",` |
|      - | 1521 | `	"Invalid hexadecimal character reference", "Invalid decimal character reference",` |
|      - | 1522 | `	"Invalid character reference", "Invalid character",` |
|      - | 1523 | `	"XML_ERR_CHARREF_AT_EOF", "XML_ERR_CHARREF_IN_PROLOG", "XML_ERR_CHARREF_IN_EPILOG",` |
|      - | 1524 | `	"XML_ERR_CHARREF_IN_DTD", "XML_ERR_ENTITYREF_AT_EOF", "XML_ERR_ENTITYREF_IN_PROLOG",` |
|      - | 1525 | `	"XML_ERR_ENTITYREF_IN_EPILOG", "XML_ERR_ENTITYREF_IN_DTD",` |
|      - | 1526 | `	"PEReference at end of document", "PEReference in prolog", "PEReference in epilog",` |
|      - | 1527 | `	"PEReference: forbidden within markup decl in internal subset",` |
|      - | 1528 | `	"XML_ERR_ENTITYREF_NO_NAME", "EntityRef: expecting ';'", "PEReference: no name",` |
|      - | 1529 | `	"PEReference: expecting ';'", "Undeclared entity error", "Undeclared entity warning",` |
|      - | 1530 | `	"Unparsed Entity", "XML_ERR_ENTITY_IS_EXTERNAL", "XML_ERR_ENTITY_IS_PARAMETER",` |
|      - | 1531 | `	"Unknown encoding", "Unsupported encoding", "String not started expecting ' or \"",` |
|      - | 1532 | `	"String not closed expecting \" or '", "Namespace declaration error",` |
|      - | 1533 | `	"EntityValue: \" or ' expected", "EntityValue: \" or ' expected", "< in attribute",` |
|      - | 1534 | `	"Attribute not started", "Attribute not finished", "Attribute without value",` |
|      - | 1535 | `	"Attribute redefined", "SystemLiteral \" or ' expected", "SystemLiteral \" or ' expected",` |
|      - | 1536 | `	"Comment not finished", "Processing Instruction not started",` |
|      - | 1537 | `	"Processing Instruction not finished", "NOTATION: Name expected here",` |
|      - | 1538 | `	"'>' required to close NOTATION declaration",` |
|      - | 1539 | `	"'(' required to start ATTLIST enumeration", "'(' required to start ATTLIST enumeration",` |
|      - | 1540 | `	"MixedContentDecl : '\|' or ')*' expected", "XML_ERR_MIXED_NOT_FINISHED",` |
|      - | 1541 | `	"ELEMENT in DTD not started", "ELEMENT in DTD not finished",` |
|      - | 1542 | `	"XML declaration not started", "XML declaration not finished",` |
|      - | 1543 | `	"XML_ERR_CONDSEC_NOT_STARTED", "XML conditional section not closed",` |
|      - | 1544 | `	"Content error in the external subset", "DOCTYPE not finished",` |
|      - | 1545 | `	"Sequence ']]>' not allowed in content", "CDATA not finished", "Reserved XML Name",` |
|      - | 1546 | `	"Space required", "XML_ERR_SEPARATOR_REQUIRED", "NmToken expected in ATTLIST enumeration",` |
|      - | 1547 | `	"XML_ERR_NAME_REQUIRED", "MixedContentDecl : '#PCDATA' expected",` |
|      - | 1548 | `	"SYSTEM or PUBLIC, the URI is missing", "PUBLIC, the Public Identifier is missing",` |
|      - | 1549 | `	"< required", "> required", "</ required", "= required", "Mismatched tag",` |
|      - | 1550 | `	"Tag not finished", "standalone accepts only 'yes' or 'no'",` |
|      - | 1551 | `	"Invalid XML encoding name", "Comment must not contain '--' (double-hyphen)",` |
|      - | 1552 | `	"Invalid encoding", "external parsed entities cannot be standalone",` |
|      - | 1553 | `	"XML conditional section '[' expected", "Entity value required",` |
|      - | 1554 | `	"chunk is not well balanced", "extra content at the end of well balanced chunk",` |
|      - | 1555 | `	"XML_ERR_ENTITY_CHAR_ERROR", "PEReferences forbidden in internal subset",` |
|      - | 1556 | `	"Detected an entity reference loop", "XML_ERR_ENTITY_BOUNDARY", "Invalid URI",` |
|      - | 1557 | `	"Fragment not allowed", "XML_WAR_CATALOG_PI", "XML_ERR_NO_DTD",` |
|      - | 1558 | `	"conditional section INCLUDE or IGNORE keyword expected",` |
|      - | 1559 | `	"Version in XML Declaration missing", "XML_WAR_UNKNOWN_VERSION", "XML_WAR_LANG_VALUE",` |
|      - | 1560 | `	"XML_WAR_NS_URI", "XML_WAR_NS_URI_RELATIVE", "Missing encoding in text declaration"` |
|      - | 1561 | `};` |
|      - | 1562 | `/* ?string xml_error_string(int $error_code) */` |
|     14 | 1563 | `static int vm_builtin_xml_error_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1564 | `{` |
|     15 | 1565 | `	sxi64 iCode = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     15 | 1566 | `	if( iCode < 0 \|\| iCode >= (sxi64)SX_ARRAYSIZE(azXmlErrorMap) ){` |
|      5 | 1567 | `		ph7_result_string(pCtx,"Unknown",(int)sizeof("Unknown")-1);` |
|      5 | 1568 | `		return PH7_OK;` |
|      - | 1569 | `	}` |
|     11 | 1570 | `	ph7_result_string(pCtx,azXmlErrorMap[iCode],-1);` |
|     11 | 1571 | `	return PH7_OK;` |
|      8 | 1572 | `}` |
|      - | 1573 | `/* int xml_get_current_line_number(XMLParser $parser) */` |
|     10 | 1574 | `static int vm_builtin_xml_get_current_line_number(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1575 | `{` |
|     11 | 1576 | `	phl_xmlparser *p = nArg > 0 ? XmlParserOf(apArg[0],0) : 0;` |
|     11 | 1577 | `	ph7_result_int(pCtx,(p && p->pCtxt && p->pCtxt->input) ? p->pCtxt->input->line : 0);` |
|     11 | 1578 | `	return PH7_OK;` |
|      1 | 1579 | `}` |
|      - | 1580 | `/* int xml_get_current_column_number(XMLParser $parser) */` |
|      4 | 1581 | `static int vm_builtin_xml_get_current_column_number(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1582 | `{` |
|      5 | 1583 | `	phl_xmlparser *p = nArg > 0 ? XmlParserOf(apArg[0],0) : 0;` |
|      5 | 1584 | `	ph7_result_int(pCtx,(p && p->pCtxt && p->pCtxt->input) ? p->pCtxt->input->col : 0);` |
|      5 | 1585 | `	return PH7_OK;` |
|      1 | 1586 | `}` |
|      - | 1587 | `/* int xml_get_current_byte_index(XMLParser $parser) */` |
|     10 | 1588 | `static int vm_builtin_xml_get_current_byte_index(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1589 | `{` |
|     11 | 1590 | `	phl_xmlparser *p = nArg > 0 ? XmlParserOf(apArg[0],0) : 0;` |
|     11 | 1591 | `	sxi64 iByte = 0;` |
|     11 | 1592 | `	if( p && p->pCtxt && p->pCtxt->input ){` |
|     16 | 1593 | `		iByte = (sxi64)p->pCtxt->input->consumed` |
|     10 | 1594 | `			+ (sxi64)(p->pCtxt->input->cur - p->pCtxt->input->base);` |
|      5 | 1595 | `	}` |
|     11 | 1596 | `	ph7_result_int64(pCtx,iByte);` |
|     11 | 1597 | `	return PH7_OK;` |
|      1 | 1598 | `}` |
|      - | 1599 | `/* The XML_OPTION_* numbers (php's, not libxml's). */` |
|      - | 1600 | `#define PHL_XML_OPTION_CASE_FOLDING    1` |
|      - | 1601 | `#define PHL_XML_OPTION_TARGET_ENCODING 2` |
|      - | 1602 | `#define PHL_XML_OPTION_SKIP_TAGSTART   3` |
|      - | 1603 | `#define PHL_XML_OPTION_SKIP_WHITE      4` |
|      - | 1604 | `#define PHL_XML_OPTION_PARSE_HUGE      5` |
|      6 | 1605 | `static const char * XmlTargetEncName(int iEnc)` |
|      1 | 1606 | `{` |
|      7 | 1607 | `	switch( iEnc ){` |
|      5 | 1608 | `		case PHL_XML_ENC_ISO88591: return "ISO-8859-1";` |
|    ! 0 | 1609 | `		case PHL_XML_ENC_ASCII:    return "US-ASCII";` |
|      3 | 1610 | `		default:                   return "UTF-8";` |
|      - | 1611 | `	}` |
|      4 | 1612 | `}` |
|      - | 1613 | `/* bool xml_parser_set_option(XMLParser $parser, int $option, $value) */` |
|     34 | 1614 | `static int vm_builtin_xml_parser_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1615 | `{` |
|     35 | 1616 | `	phl_xmlparser *p = nArg > 0 ? XmlParserOf(apArg[0],0) : 0;` |
|      - | 1617 | `	sxi64 iOpt;` |
|      - | 1618 | `	ph7_value *pValue;` |
|     35 | 1619 | `	if( p == 0 \|\| nArg < 3 ){` |
|    ! 0 | 1620 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1621 | `		return PH7_OK;` |
|      - | 1622 | `	}` |
|     35 | 1623 | `	iOpt = ph7_value_to_int64(apArg[1]);` |
|     35 | 1624 | `	pValue = apArg[2];` |
|      - | 1625 | `	/* php only WARNS about a $value outside string\|int\|bool and reads it` |
|      - | 1626 | `	 * anyway; float slides through the warning too. */` |
|     35 | 1627 | `	if( (pValue->iFlags & (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_BOOL)) == 0 ){` |
|    ! 0 | 1628 | `		const char *zType = "null";` |
|    ! 0 | 1629 | `		if( pValue->iFlags & MEMOBJ_REAL ){ zType = "float"; }` |
|    ! 0 | 1630 | `		else if( pValue->iFlags & MEMOBJ_HASHMAP ){ zType = "array"; }` |
|    ! 0 | 1631 | `		else if( pValue->iFlags & MEMOBJ_OBJ ){ zType = "object"; }` |
|    ! 0 | 1632 | `		else if( pValue->iFlags & MEMOBJ_RES ){ zType = "resource"; }` |
|    ! 0 | 1633 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 1634 | `			"Argument #3 ($value) must be of type string\|int\|bool, %s given",zType);` |
|    ! 0 | 1635 | `	}` |
|     35 | 1636 | `	switch( (int)iOpt ){` |
|      4 | 1637 | `		case PHL_XML_OPTION_CASE_FOLDING:` |
|      9 | 1638 | `			p->bCaseFolding = ph7_value_to_bool(pValue) ? 1 : 0;` |
|      9 | 1639 | `			break;` |
|      2 | 1640 | `		case PHL_XML_OPTION_SKIP_WHITE:` |
|      5 | 1641 | `			p->bSkipWhite = ph7_value_to_bool(pValue) ? 1 : 0;` |
|      5 | 1642 | `			break;` |
|    ! 0 | 1643 | `		case PHL_XML_OPTION_PARSE_HUGE:` |
|    ! 0 | 1644 | `			if( p->bParsing ){` |
|    ! 0 | 1645 | `				return PH7_VmThrowException(pCtx,"Error",` |
|      - | 1646 | `					"Cannot change option XML_OPTION_PARSE_HUGE while parsing");` |
|      - | 1647 | `			}` |
|    ! 0 | 1648 | `			p->bParseHuge = ph7_value_to_bool(pValue) ? 1 : 0;` |
|    ! 0 | 1649 | `			break;` |
|      4 | 1650 | `		case PHL_XML_OPTION_SKIP_TAGSTART: {` |
|      9 | 1651 | `			sxi64 iVal = ph7_value_to_int64(pValue);` |
|      9 | 1652 | `			if( iVal < 0 \|\| iVal > 2147483647 ){` |
|      3 | 1653 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1654 | `					"Argument #3 ($value) must be between 0 and 2147483647 for option XML_OPTION_SKIP_TAGSTART");` |
|      3 | 1655 | `				ph7_result_bool(pCtx,0);` |
|      3 | 1656 | `				return PH7_OK;` |
|      - | 1657 | `			}` |
|      7 | 1658 | `			p->nSkipTagstart = (int)iVal;` |
|      7 | 1659 | `			break;` |
|      - | 1660 | `		}` |
|      6 | 1661 | `		case PHL_XML_OPTION_TARGET_ENCODING: {` |
|     13 | 1662 | `			int nEnc = 0;` |
|     13 | 1663 | `			const char *zEnc = ph7_value_to_string(pValue,&nEnc);` |
|     13 | 1664 | `			if( nEnc == (int)sizeof("ISO-8859-1")-1 && SyStrnicmp(zEnc,"ISO-8859-1",(sxu32)nEnc) == 0 ){` |
|      7 | 1665 | `				p->iTargetEnc = PHL_XML_ENC_ISO88591;` |
|     10 | 1666 | `			}else if( nEnc == (int)sizeof("UTF-8")-1 && SyStrnicmp(zEnc,"UTF-8",(sxu32)nEnc) == 0 ){` |
|      3 | 1667 | `				p->iTargetEnc = PHL_XML_ENC_UTF8;` |
|      6 | 1668 | `			}else if( nEnc == (int)sizeof("US-ASCII")-1 && SyStrnicmp(zEnc,"US-ASCII",(sxu32)nEnc) == 0 ){` |
|      3 | 1669 | `				p->iTargetEnc = PHL_XML_ENC_ASCII;` |
|      2 | 1670 | `			}else{` |
|      4 | 1671 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1672 | `					"%s(): Argument #3 ($value) is not a supported target encoding",` |
|      1 | 1673 | `					ph7_function_name(pCtx));` |
|      - | 1674 | `			}` |
|     11 | 1675 | `			break;` |
|      - | 1676 | `		}` |
|      1 | 1677 | `		default:` |
|      4 | 1678 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1679 | `				"%s(): Argument #2 ($option) must be a XML_OPTION_* constant",` |
|      1 | 1680 | `				ph7_function_name(pCtx));` |
|      - | 1681 | `	}` |
|     29 | 1682 | `	ph7_result_bool(pCtx,1);` |
|     29 | 1683 | `	return PH7_OK;` |
|     18 | 1684 | `}` |
|      - | 1685 | `/* string\|int\|bool xml_parser_get_option(XMLParser $parser, int $option) */` |
|     24 | 1686 | `static int vm_builtin_xml_parser_get_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1687 | `{` |
|     25 | 1688 | `	phl_xmlparser *p = nArg > 0 ? XmlParserOf(apArg[0],0) : 0;` |
|      - | 1689 | `	sxi64 iOpt;` |
|     25 | 1690 | `	if( p == 0 \|\| nArg < 2 ){` |
|    ! 0 | 1691 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1692 | `		return PH7_OK;` |
|      - | 1693 | `	}` |
|     25 | 1694 | `	iOpt = ph7_value_to_int64(apArg[1]);` |
|     25 | 1695 | `	switch( (int)iOpt ){` |
|      7 | 1696 | `		case PHL_XML_OPTION_CASE_FOLDING:    ph7_result_bool(pCtx,p->bCaseFolding); break;` |
|      7 | 1697 | `		case PHL_XML_OPTION_SKIP_TAGSTART:   ph7_result_int(pCtx,p->nSkipTagstart); break;` |
|      3 | 1698 | `		case PHL_XML_OPTION_SKIP_WHITE:      ph7_result_bool(pCtx,p->bSkipWhite); break;` |
|      3 | 1699 | `		case PHL_XML_OPTION_PARSE_HUGE:      ph7_result_bool(pCtx,p->bParseHuge); break;` |
|      3 | 1700 | `		case PHL_XML_OPTION_TARGET_ENCODING:` |
|      7 | 1701 | `			ph7_result_string(pCtx,XmlTargetEncName(p->iTargetEnc),-1);` |
|      7 | 1702 | `			break;` |
|      1 | 1703 | `		default:` |
|      4 | 1704 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1705 | `				"%s(): Argument #2 ($option) must be a XML_OPTION_* constant",` |
|      1 | 1706 | `				ph7_function_name(pCtx));` |
|      - | 1707 | `	}` |
|     23 | 1708 | `	return PH7_OK;` |
|     13 | 1709 | `}` |
|      - | 1710 |  |
|      - | 1711 | `/* ------------------------------------------------------------------------` |
|      - | 1712 | ` * Installation.` |
|      - | 1713 | ` * ------------------------------------------------------------------------ */` |
|      - | 1714 | `` /* XMLParser has no reachable constructor; the class object refuses `new` `` |
|      - | 1715 | ` * with php's own sentence (PH7_CLASS_NOINSTANTIATE + zNewRefusal below). */` |
|      - | 1716 | `#define XML_INT_CONST(FN,VALUE) \` |
|      - | 1717 | `	static void FN(ph7_value *pVal,void *pUnused){ \` |
|      - | 1718 | `		SXUNUSED(pUnused); \` |
|      - | 1719 | `		ph7_value_int64(pVal,(ph7_int64)(VALUE)); \` |
|      - | 1720 | `	}` |
|     77 | 1721 | `XML_INT_CONST(XmlConst_ERROR_NONE,                          0)` |
|     75 | 1722 | `XML_INT_CONST(XmlConst_ERROR_NO_MEMORY,                     1)` |
|     75 | 1723 | `XML_INT_CONST(XmlConst_ERROR_SYNTAX,                        2)` |
|     75 | 1724 | `XML_INT_CONST(XmlConst_ERROR_NO_ELEMENTS,                   3)` |
|     73 | 1725 | `XML_INT_CONST(XmlConst_ERROR_INVALID_TOKEN,                 4)` |
|     73 | 1726 | `XML_INT_CONST(XmlConst_ERROR_UNCLOSED_TOKEN,                5)` |
|     73 | 1727 | `XML_INT_CONST(XmlConst_ERROR_PARTIAL_CHAR,                  6)` |
|     75 | 1728 | `XML_INT_CONST(XmlConst_ERROR_TAG_MISMATCH,                  7)` |
|     73 | 1729 | `XML_INT_CONST(XmlConst_ERROR_DUPLICATE_ATTRIBUTE,           8)` |
|     73 | 1730 | `XML_INT_CONST(XmlConst_ERROR_JUNK_AFTER_DOC_ELEMENT,        9)` |
|     73 | 1731 | `XML_INT_CONST(XmlConst_ERROR_PARAM_ENTITY_REF,             10)` |
|     73 | 1732 | `XML_INT_CONST(XmlConst_ERROR_UNDEFINED_ENTITY,             11)` |
|     73 | 1733 | `XML_INT_CONST(XmlConst_ERROR_RECURSIVE_ENTITY_REF,         12)` |
|     73 | 1734 | `XML_INT_CONST(XmlConst_ERROR_ASYNC_ENTITY,                 13)` |
|     73 | 1735 | `XML_INT_CONST(XmlConst_ERROR_BAD_CHAR_REF,                 14)` |
|     73 | 1736 | `XML_INT_CONST(XmlConst_ERROR_BINARY_ENTITY_REF,            15)` |
|     73 | 1737 | `XML_INT_CONST(XmlConst_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF,16)` |
|     73 | 1738 | `XML_INT_CONST(XmlConst_ERROR_MISPLACED_XML_PI,             17)` |
|     75 | 1739 | `XML_INT_CONST(XmlConst_ERROR_UNKNOWN_ENCODING,             18)` |
|     73 | 1740 | `XML_INT_CONST(XmlConst_ERROR_INCORRECT_ENCODING,           19)` |
|     73 | 1741 | `XML_INT_CONST(XmlConst_ERROR_UNCLOSED_CDATA_SECTION,       20)` |
|     75 | 1742 | `XML_INT_CONST(XmlConst_ERROR_EXTERNAL_ENTITY_HANDLING,     21)` |
|     89 | 1743 | `XML_INT_CONST(XmlConst_OPTION_CASE_FOLDING,    PHL_XML_OPTION_CASE_FOLDING)` |
|     93 | 1744 | `XML_INT_CONST(XmlConst_OPTION_TARGET_ENCODING, PHL_XML_OPTION_TARGET_ENCODING)` |
|     89 | 1745 | `XML_INT_CONST(XmlConst_OPTION_SKIP_TAGSTART,   PHL_XML_OPTION_SKIP_TAGSTART)` |
|     81 | 1746 | `XML_INT_CONST(XmlConst_OPTION_SKIP_WHITE,      PHL_XML_OPTION_SKIP_WHITE)` |
|     77 | 1747 | `XML_INT_CONST(XmlConst_OPTION_PARSE_HUGE,      PHL_XML_OPTION_PARSE_HUGE)` |
|     72 | 1748 | `static void XmlConst_SAX_IMPL(ph7_value *pVal,void *pUnused)` |
|      3 | 1749 | `{` |
|     36 | 1750 | `	SXUNUSED(pUnused);` |
|     75 | 1751 | `	ph7_value_string(pVal,"libxml",(int)sizeof("libxml")-1);` |
|     75 | 1752 | `}` |
|      - | 1753 | `/*` |
|      - | 1754 | ` * Install php's ext/xml surface: the XMLParser class, the 19 functions of` |
|      - | 1755 | ` * its NON-deprecated half, and the XML_* constants. Called from PH7_VmInit` |
|      - | 1756 | ` * inside the bCompilingBuiltin window, after PH7_VmInstallLibxml.` |
|      - | 1757 | ` */` |
|   7925 | 1758 | `PH7_PRIVATE sxi32 PH7_VmInstallXml(ph7_vm *pVm)` |
|      5 | 1759 | `{` |
|      - | 1760 | `	static const struct {` |
|      - | 1761 | `		const char *zName;` |
|      - | 1762 | `		ProchHostFunction xFunc;` |
|      - | 1763 | `	} aFunc[] = {` |
|      - | 1764 | `		{ "xml_parser_create",       vm_builtin_xml_parser_create },` |
|      - | 1765 | `		{ "xml_parser_create_ns",    vm_builtin_xml_parser_create_ns },` |
|      - | 1766 | `		{ "xml_parse",               vm_builtin_xml_parse },` |
|      - | 1767 | `		{ "xml_parse_into_struct",   vm_builtin_xml_parse_into_struct },` |
|      - | 1768 | `		{ "xml_parser_get_option",   vm_builtin_xml_parser_get_option },` |
|      - | 1769 | `		{ "xml_parser_set_option",   vm_builtin_xml_parser_set_option },` |
|      - | 1770 | `		{ "xml_error_string",        vm_builtin_xml_error_string },` |
|      - | 1771 | `		{ "xml_get_error_code",      vm_builtin_xml_get_error_code },` |
|      - | 1772 | `		{ "xml_get_current_line_number",   vm_builtin_xml_get_current_line_number },` |
|      - | 1773 | `		{ "xml_get_current_column_number", vm_builtin_xml_get_current_column_number },` |
|      - | 1774 | `		{ "xml_get_current_byte_index",    vm_builtin_xml_get_current_byte_index },` |
|      - | 1775 | `		{ "xml_set_element_handler",       vm_builtin_xml_set_element_handler },` |
|      - | 1776 | `		{ "xml_set_character_data_handler",vm_builtin_xml_set_character_data_handler },` |
|      - | 1777 | `		{ "xml_set_processing_instruction_handler", vm_builtin_xml_set_processing_instruction_handler },` |
|      - | 1778 | `		{ "xml_set_default_handler",       vm_builtin_xml_set_default_handler },` |
|      - | 1779 | `		{ "xml_set_unparsed_entity_decl_handler", vm_builtin_xml_set_unparsed_entity_decl_handler },` |
|      - | 1780 | `		{ "xml_set_notation_decl_handler", vm_builtin_xml_set_notation_decl_handler },` |
|      - | 1781 | `		{ "xml_set_external_entity_ref_handler", vm_builtin_xml_set_external_entity_ref_handler },` |
|      - | 1782 | `		{ "xml_set_start_namespace_decl_handler", vm_builtin_xml_set_start_namespace_decl_handler },` |
|      - | 1783 | `		{ "xml_set_end_namespace_decl_handler",   vm_builtin_xml_set_end_namespace_decl_handler },` |
|      - | 1784 | `	};` |
|      - | 1785 | `	static const struct {` |
|      - | 1786 | `		const char *zName;` |
|      - | 1787 | `		void (*xExpand)(ph7_value *,void *);` |
|      - | 1788 | `	} aConst[] = {` |
|      - | 1789 | `		{ "XML_ERROR_NONE",                XmlConst_ERROR_NONE },` |
|      - | 1790 | `		{ "XML_ERROR_NO_MEMORY",           XmlConst_ERROR_NO_MEMORY },` |
|      - | 1791 | `		{ "XML_ERROR_SYNTAX",              XmlConst_ERROR_SYNTAX },` |
|      - | 1792 | `		{ "XML_ERROR_NO_ELEMENTS",         XmlConst_ERROR_NO_ELEMENTS },` |
|      - | 1793 | `		{ "XML_ERROR_INVALID_TOKEN",       XmlConst_ERROR_INVALID_TOKEN },` |
|      - | 1794 | `		{ "XML_ERROR_UNCLOSED_TOKEN",      XmlConst_ERROR_UNCLOSED_TOKEN },` |
|      - | 1795 | `		{ "XML_ERROR_PARTIAL_CHAR",        XmlConst_ERROR_PARTIAL_CHAR },` |
|      - | 1796 | `		{ "XML_ERROR_TAG_MISMATCH",        XmlConst_ERROR_TAG_MISMATCH },` |
|      - | 1797 | `		{ "XML_ERROR_DUPLICATE_ATTRIBUTE", XmlConst_ERROR_DUPLICATE_ATTRIBUTE },` |
|      - | 1798 | `		{ "XML_ERROR_JUNK_AFTER_DOC_ELEMENT", XmlConst_ERROR_JUNK_AFTER_DOC_ELEMENT },` |
|      - | 1799 | `		{ "XML_ERROR_PARAM_ENTITY_REF",    XmlConst_ERROR_PARAM_ENTITY_REF },` |
|      - | 1800 | `		{ "XML_ERROR_UNDEFINED_ENTITY",    XmlConst_ERROR_UNDEFINED_ENTITY },` |
|      - | 1801 | `		{ "XML_ERROR_RECURSIVE_ENTITY_REF",XmlConst_ERROR_RECURSIVE_ENTITY_REF },` |
|      - | 1802 | `		{ "XML_ERROR_ASYNC_ENTITY",        XmlConst_ERROR_ASYNC_ENTITY },` |
|      - | 1803 | `		{ "XML_ERROR_BAD_CHAR_REF",        XmlConst_ERROR_BAD_CHAR_REF },` |
|      - | 1804 | `		{ "XML_ERROR_BINARY_ENTITY_REF",   XmlConst_ERROR_BINARY_ENTITY_REF },` |
|      - | 1805 | `		{ "XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF", XmlConst_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF },` |
|      - | 1806 | `		{ "XML_ERROR_MISPLACED_XML_PI",    XmlConst_ERROR_MISPLACED_XML_PI },` |
|      - | 1807 | `		{ "XML_ERROR_UNKNOWN_ENCODING",    XmlConst_ERROR_UNKNOWN_ENCODING },` |
|      - | 1808 | `		{ "XML_ERROR_INCORRECT_ENCODING",  XmlConst_ERROR_INCORRECT_ENCODING },` |
|      - | 1809 | `		{ "XML_ERROR_UNCLOSED_CDATA_SECTION", XmlConst_ERROR_UNCLOSED_CDATA_SECTION },` |
|      - | 1810 | `		{ "XML_ERROR_EXTERNAL_ENTITY_HANDLING", XmlConst_ERROR_EXTERNAL_ENTITY_HANDLING },` |
|      - | 1811 | `		{ "XML_OPTION_CASE_FOLDING",       XmlConst_OPTION_CASE_FOLDING },` |
|      - | 1812 | `		{ "XML_OPTION_TARGET_ENCODING",    XmlConst_OPTION_TARGET_ENCODING },` |
|      - | 1813 | `		{ "XML_OPTION_SKIP_TAGSTART",      XmlConst_OPTION_SKIP_TAGSTART },` |
|      - | 1814 | `		{ "XML_OPTION_SKIP_WHITE",         XmlConst_OPTION_SKIP_WHITE },` |
|      - | 1815 | `		{ "XML_OPTION_PARSE_HUGE",         XmlConst_OPTION_PARSE_HUGE },` |
|      - | 1816 | `		{ "XML_SAX_IMPL",                  XmlConst_SAX_IMPL },` |
|      - | 1817 | `	};` |
|      - | 1818 | `	/* The libxml parser handle: storage the class owns and never presents. */` |
|      - | 1819 | `	static const PH7_NativePropDef aProp[] = {` |
|      - | 1820 | `		{ "__p", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 1821 | `	};` |
|      - | 1822 | `	static const PH7_NativeClassSpec sSpec = {` |
|      - | 1823 | `		"XMLParser", 0, 0,` |
|      - | 1824 | `		PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 1825 | `		0, 0, 0, 0,` |
|      - | 1826 | `		aProp, SX_ARRAYSIZE(aProp),` |
|      - | 1827 | `		0, 0, 0` |
|      - | 1828 | `	};` |
|      - | 1829 | `	sxu32 n;` |
|      - | 1830 | `	sxi32 rc;` |
|   7930 | 1831 | `	pVm->pXmlParsers = 0;` |
| 166430 | 1832 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
| 158505 | 1833 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|  79145 | 1834 | `	}` |
| 229830 | 1835 | `	for( n = 0 ; n < SX_ARRAYSIZE(aConst) ; n++ ){` |
| 221905 | 1836 | `		ph7_create_constant(&(*pVm),aConst[n].zName,aConst[n].xExpand,0);` |
| 110801 | 1837 | `	}` |
|   7930 | 1838 | `	rc = PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|   7930 | 1839 | `	if( rc == SXRET_OK ){` |
|   7930 | 1840 | `		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"XMLParser",sizeof("XMLParser")-1,FALSE,0);` |
|   7930 | 1841 | `		if( pClass ){` |
|   7930 | 1842 | `			pClass->zNewRefusal =` |
|      - | 1843 | `				"Cannot directly construct XMLParser, use xml_parser_create() or xml_parser_create_ns() instead";` |
|   3957 | 1844 | `		}` |
|   3957 | 1845 | `	}` |
|   7930 | 1846 | `	return rc;` |
|      5 | 1847 | `}` |
|      - | 1848 |  |
|      - | 1849 | `#else` |
|      - | 1850 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|      - | 1851 | `typedef int vm_xml_unused;` |
|      - | 1852 | `#endif /* PH7_ENABLE_LIBXML */` |
|      - | 1853 |  |
