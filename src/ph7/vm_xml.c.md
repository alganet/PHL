# src/ph7/vm_xml.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 392/578 lines (67.82%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `#include "ph7int.h"` |
|    - |    7 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |    8 | `/*` |
|    - |    9 | ` * Allocate and initialize an XML engine.` |
|    - |   10 | ` */` |
|   84 |   11 | `static ph7_xml_engine * VmCreateXMLEngine(ph7_context *pCtx,int process_ns,int ns_sep)` |
|    1 |   12 |  |
|    - |   13 | `	ph7_xml_engine *pEngine;` |
|   85 |   14 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |   15 | `	ph7_value *pValue;` |
|    - |   16 | `	sxu32 n;` |
|    - |   17 | `	/* Allocate a new instance */` |
|   85 |   18 | `	pEngine = (ph7_xml_engine *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_xml_engine));` |
|   85 |   19 | `	if( pEngine == 0 ){` |
|    - |   20 | `		/* Out of memory */` |
|  ! 0 |   21 | `		return 0;` |
|    - |   22 | `	}` |
|    - |   23 | `	/* Zero the structure */` |
|   85 |   24 | `	SyZero(pEngine,sizeof(ph7_xml_engine));` |
|    - |   25 | `	/* Initialize fields */` |
|   85 |   26 | `	pEngine->pVm = pVm;` |
|   85 |   27 | `	pEngine->pCtx = 0;` |
|   85 |   28 | `	pEngine->ns_sep = ns_sep;` |
|   85 |   29 | `	SyXMLParserInit(&pEngine->sParser,&pVm->sAllocator,process_ns ? SXML_ENABLE_NAMESPACE : 0);` |
|   85 |   30 | `	SyBlobInit(&pEngine->sErr,&pVm->sAllocator);` |
|   85 |   31 | `	PH7_MemObjInit(pVm,&pEngine->sParserValue);` |
|  925 |   32 | `	for( n = 0 ; n < SX_ARRAYSIZE(pEngine->aCB) ; ++n ){` |
|  841 |   33 | `		pValue = &pEngine->aCB[n];` |
|    - |   34 | `		/* NULLIFY the array entries,until someone register an event handler */` |
|  841 |   35 | `		PH7_MemObjInit(&(*pVm),pValue);` |
|  421 |   36 | `	}` |
|   85 |   37 | `	ph7_value_resource(&pEngine->sParserValue,pEngine);` |
|   85 |   38 | `	pEngine->iErrCode = SXML_ERROR_NONE;` |
|    - |   39 | `	/* Finally set the magic number */` |
|   85 |   40 | `	pEngine->nMagic = XML_ENGINE_MAGIC;` |
|   85 |   41 | `	return pEngine;` |
|   43 |   42 |  |
|    - |   43 | `/*` |
|    - |   44 | ` * Release an XML engine.` |
|    - |   45 | ` */` |
|   84 |   46 | `static void VmReleaseXMLEngine(ph7_xml_engine *pEngine)` |
|    1 |   47 |  |
|   85 |   48 | `	ph7_vm *pVm = pEngine->pVm;` |
|    - |   49 | `	ph7_value *pValue;` |
|    - |   50 | `	sxu32 n;` |
|    - |   51 | `	/* Release fields */` |
|   85 |   52 | `	SyBlobRelease(&pEngine->sErr);` |
|   85 |   53 | `	SyXMLParserRelease(&pEngine->sParser);` |
|   85 |   54 | `	PH7_MemObjRelease(&pEngine->sParserValue);` |
|  925 |   55 | `	for( n = 0 ; n < SX_ARRAYSIZE(pEngine->aCB) ; ++n ){` |
|  841 |   56 | `		pValue = &pEngine->aCB[n];` |
|  841 |   57 | `		PH7_MemObjRelease(pValue);` |
|  421 |   58 | `	}` |
|   85 |   59 | `	pEngine->nMagic = 0x2621;` |
|    - |   60 | `	/* Finally,release the whole instance */` |
|   85 |   61 | `	SyMemBackendFree(&pVm->sAllocator,pEngine);` |
|   85 |   62 |  |
|    - |   63 | `/*` |
|    - |   64 | ` * resource xml_parser_create([ string $encoding ])` |
|    - |   65 | ` *  Create an UTF-8 XML parser.` |
|    - |   66 | ` * Parameter` |
|    - |   67 | ` *  $encoding` |
|    - |   68 | ` *   (Only UTF-8 encoding is used)` |
|    - |   69 | ` * Return` |
|    - |   70 | ` *  Returns a resource handle for the new XML parser.` |
|    - |   71 | ` */` |
|   80 |   72 | `PH7_PRIVATE int vm_builtin_xml_parser_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   73 |  |
|    - |   74 | `	ph7_xml_engine *pEngine;` |
|    - |   75 | `	/* Allocate a new instance */` |
|   81 |   76 | `	pEngine = VmCreateXMLEngine(&(*pCtx),0,':');` |
|   81 |   77 | `	if( pEngine == 0 ){` |
|  ! 0 |   78 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |   79 | `		/* Return null */` |
|  ! 0 |   80 | `		ph7_result_null(pCtx);` |
|  ! 0 |   81 | `		SXUNUSED(nArg); /* cc warning */` |
|  ! 0 |   82 | `		SXUNUSED(apArg);` |
|  ! 0 |   83 | `		return PH7_OK;` |
|    - |   84 | `	}` |
|    - |   85 | `	/* Return the engine as a resource */` |
|   81 |   86 | `	ph7_result_resource(pCtx,pEngine);` |
|   81 |   87 | `	return PH7_OK;` |
|   41 |   88 |  |
|    - |   89 | `/*` |
|    - |   90 | ` * resource xml_parser_create_ns([ string $encoding[,string $separator = ':']])` |
|    - |   91 | ` *  Create an UTF-8 XML parser with namespace support.` |
|    - |   92 | ` * Parameter` |
|    - |   93 | ` *  $encoding` |
|    - |   94 | ` *   (Only UTF-8 encoding is supported)` |
|    - |   95 | ` *  $separtor` |
|    - |   96 | ` *   Namespace separator (a single character)` |
|    - |   97 | ` * Return` |
|    - |   98 | ` *  Returns a resource handle for the new XML parser.` |
|    - |   99 | ` */` |
|    4 |  100 | `PH7_PRIVATE int vm_builtin_xml_parser_create_ns(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  101 |  |
|    - |  102 | `	ph7_xml_engine *pEngine;` |
|    5 |  103 | `	int ns_sep = ':';` |
|    5 |  104 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|  ! 0 |  105 | `		const char *zSep = ph7_value_to_string(apArg[1],0);` |
|  ! 0 |  106 | `		if( zSep[0] != 0 ){` |
|  ! 0 |  107 | `			ns_sep = zSep[0];` |
|  ! 0 |  108 | `		}` |
|  ! 0 |  109 | `	}` |
|    - |  110 | `	/* Allocate a new instance */` |
|    5 |  111 | `	pEngine = VmCreateXMLEngine(&(*pCtx),TRUE,ns_sep);` |
|    5 |  112 | `	if( pEngine == 0 ){` |
|  ! 0 |  113 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  114 | `		/* Return null */` |
|  ! 0 |  115 | `		ph7_result_null(pCtx);` |
|  ! 0 |  116 | `		return PH7_OK;` |
|    - |  117 | `	}` |
|    - |  118 | `	/* Return the engine as a resource */` |
|    5 |  119 | `	ph7_result_resource(pCtx,pEngine);` |
|    5 |  120 | `	return PH7_OK;` |
|    3 |  121 |  |
|    - |  122 | `/*` |
|    - |  123 | ` * bool xml_parser_free(resource $parser)` |
|    - |  124 | ` *  Release an XML engine.` |
|    - |  125 | ` * Parameter` |
|    - |  126 | ` *  $parser` |
|    - |  127 | ` *   A reference to the XML parser to free.` |
|    - |  128 | ` * Return` |
|    - |  129 | ` *  This function returns FALSE if parser does not refer` |
|    - |  130 | ` *  to a valid parser, or else it frees the parser and returns TRUE.` |
|    - |  131 | ` */` |
|   84 |  132 | `PH7_PRIVATE int vm_builtin_xml_parser_free(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  133 |  |
|    - |  134 | `	ph7_xml_engine *pEngine;` |
|   85 |  135 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  136 | `		/* Missing/Ivalid argument,return FALSE */` |
|  ! 0 |  137 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  138 | `		return PH7_OK;` |
|    - |  139 | `	}` |
|    - |  140 | `	/* Point to the XML engine */` |
|   85 |  141 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|   85 |  142 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|    - |  143 | `		/* Corrupt engine,return FALSE */` |
|  ! 0 |  144 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  145 | `		return PH7_OK;` |
|    - |  146 | `	}` |
|    - |  147 | `	/* Safely release the engine */` |
|   85 |  148 | `	VmReleaseXMLEngine(pEngine);` |
|    - |  149 | `	/* Return TRUE */` |
|   85 |  150 | `	ph7_result_bool(pCtx,1);` |
|   85 |  151 | `	return PH7_OK;` |
|   43 |  152 |  |
|    - |  153 | `/*` |
|    - |  154 | ` * bool xml_set_element_handler(resource $parser,callback $start_element_handler,[callback $end_element_handler])` |
|    - |  155 | ` * Sets the element handler functions for the XML parser. start_element_handler and end_element_handler` |
|    - |  156 | ` * are strings containing the names of functions.` |
|    - |  157 | ` * Parameters` |
|    - |  158 | ` *  $parser` |
|    - |  159 | ` *   A reference to the XML parser to set up start and end element handler functions.` |
|    - |  160 | ` *  $start_element_handler` |
|    - |  161 | ` *    The function named by start_element_handler must accept three parameters:` |
|    - |  162 | ` *    start_element_handler(resource $parser,string $name,array $attribs)` |
|    - |  163 | ` *    $parser` |
|    - |  164 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|    - |  165 | ` *   $name` |
|    - |  166 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|    - |  167 | ` *		is called.If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|    - |  168 | ` *  $attribs` |
|    - |  169 | ` *      The third parameter, attribs, contains an associative array with the element's attributes (if any).` |
|    - |  170 | ` *		The keys of this array are the attribute names, the values are the attribute values.` |
|    - |  171 | ` *      Attribute names are case-folded on the same criteria as element names.Attribute values are not case-folded.` |
|    - |  172 | ` *      The original order of the attributes can be retrieved by walking through attribs the normal way, using each().` |
|    - |  173 | ` *      The first key in the array was the first attribute, and so on.` |
|    - |  174 | ` *      Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|    - |  175 | ` * $end_element_handler` |
|    - |  176 | ` *     The function named by end_element_handler must accept two parameters:` |
|    - |  177 | ` *     end_element_handler(resource $parser,string $name)` |
|    - |  178 | ` *    $parser` |
|    - |  179 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|    - |  180 | ` *   $name` |
|    - |  181 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|    - |  182 | ` *      is called.If case-folding is in effect for this parser, the element name will be in uppercase` |
|    - |  183 | ` *      letters.` |
|    - |  184 | ` *      If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|    - |  185 | ` * Return` |
|    - |  186 | ` * TRUE on success or FALSE on failure.` |
|    - |  187 | ` */` |
|   66 |  188 | `PH7_PRIVATE int vm_builtin_xml_set_element_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  189 |  |
|    - |  190 | `	ph7_xml_engine *pEngine;` |
|   67 |  191 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  192 | `		/* Missing/Ivalid argument,return FALSE */` |
|  ! 0 |  193 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  194 | `		return PH7_OK;` |
|    - |  195 | `	}` |
|    - |  196 | `	/* Point to the XML engine */` |
|   67 |  197 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|   67 |  198 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|    - |  199 | `		/* Corrupt engine,return FALSE */` |
|  ! 0 |  200 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  201 | `		return PH7_OK;` |
|    - |  202 | `	}` |
|   67 |  203 | `	if( nArg > 1 ){` |
|    - |  204 | `		/* Save the start_element_handler callback for later invocation */` |
|   67 |  205 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_START_TAG]);` |
|   67 |  206 | `		if( nArg > 2 ){` |
|    - |  207 | `			/* Save the end_element_handler callback for later invocation */` |
|   67 |  208 | `			PH7_MemObjStore(apArg[2]/* User callback*/,&pEngine->aCB[PH7_XML_END_TAG]);` |
|   33 |  209 | `		}` |
|   33 |  210 | `	}` |
|    - |  211 | `	/* All done,return TRUE */` |
|   67 |  212 | `	ph7_result_bool(pCtx,1);` |
|   67 |  213 | `	return PH7_OK;` |
|   34 |  214 |  |
|    - |  215 | `/*` |
|    - |  216 | ` * bool xml_set_character_data_handler(resource $parser,callback $handler)` |
|    - |  217 | ` *  Sets the character data handler function for the XML parser parser.` |
|    - |  218 | ` * Parameters` |
|    - |  219 | ` * $parser` |
|    - |  220 | ` *   A reference to the XML parser to set up character data handler function.` |
|    - |  221 | ` * $handler` |
|    - |  222 | ` *  handler is a string containing the name of the callback.` |
|    - |  223 | ` *  The function named by handler must accept two parameters:` |
|    - |  224 | ` *   handler(resource $parser,string $data)` |
|    - |  225 | ` *  $parser` |
|    - |  226 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|    - |  227 | ` *  $data` |
|    - |  228 | ` *   The second parameter, data, contains the character data as a string.` |
|    - |  229 | ` *   Character data handler is called for every piece of a text in the XML document.` |
|    - |  230 | ` *   It can be called multiple times inside each fragment (e.g. for non-ASCII strings).` |
|    - |  231 | ` *   If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|    - |  232 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|    - |  233 | ` *   can also be supplied.` |
|    - |  234 | ` * Return` |
|    - |  235 | ` *  TRUE on success or FALSE on failure.` |
|    - |  236 | ` */` |
|   40 |  237 | `PH7_PRIVATE int vm_builtin_xml_set_character_data_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  238 |  |
|    - |  239 | `	ph7_xml_engine *pEngine;` |
|   41 |  240 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  241 | `		/* Missing/Ivalid argument,return FALSE */` |
|  ! 0 |  242 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  243 | `		return PH7_OK;` |
|    - |  244 | `	}` |
|    - |  245 | `	/* Point to the XML engine */` |
|   41 |  246 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|   41 |  247 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|    - |  248 | `		/* Corrupt engine,return FALSE */` |
|  ! 0 |  249 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  250 | `		return PH7_OK;` |
|    - |  251 | `	}` |
|   41 |  252 | `	if( nArg > 1 ){` |
|    - |  253 | `		/* Save the user callback for later invocation */` |
|   41 |  254 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_CDATA]);` |
|   20 |  255 | `	}` |
|    - |  256 | `	/* All done,return TRUE */` |
|   41 |  257 | `	ph7_result_bool(pCtx,1);` |
|   41 |  258 | `	return PH7_OK;` |
|   21 |  259 |  |
|    - |  260 | `/*` |
|    - |  261 | ` * bool xml_set_default_handler(resource $parser,callback $handler)` |
|    - |  262 | ` *  Set up default handler.` |
|    - |  263 | ` * Parameters` |
|    - |  264 | ` * $parser` |
|    - |  265 | ` *   A reference to the XML parser to set up character data handler function.` |
|    - |  266 | ` * $handler` |
|    - |  267 | ` *  handler is a string containing the name of the callback.` |
|    - |  268 | ` *  The function named by handler must accept two parameters:` |
|    - |  269 | ` *   handler(resource $parser,string $data)` |
|    - |  270 | ` *  $parser` |
|    - |  271 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|    - |  272 | ` *  $data` |
|    - |  273 | ` *   The second parameter, data, contains the character data.This may be the XML declaration` |
|    - |  274 | ` *   document type declaration, entities or other data for which no other handler exists.` |
|    - |  275 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|    - |  276 | ` *   can also be supplied.` |
|    - |  277 | ` * Return` |
|    - |  278 | ` *  TRUE on success or FALSE on failure.` |
|    - |  279 | ` */` |
|    2 |  280 | `PH7_PRIVATE int vm_builtin_xml_set_default_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  281 |  |
|    - |  282 | `	ph7_xml_engine *pEngine;` |
|    3 |  283 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  284 | `		/* Missing/Ivalid argument,return FALSE */` |
|  ! 0 |  285 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  286 | `		return PH7_OK;` |
|    - |  287 | `	}` |
|    - |  288 | `	/* Point to the XML engine */` |
|    3 |  289 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|    3 |  290 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|    - |  291 | `		/* Corrupt engine,return FALSE */` |
|  ! 0 |  292 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  293 | `		return PH7_OK;` |
|    - |  294 | `	}` |
|    3 |  295 | `	if( nArg > 1 ){` |
|    - |  296 | `		/* Save the user callback for later invocation */` |
|    3 |  297 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_DEF]);` |
|    1 |  298 | `	}` |
|    - |  299 | `	/* All done,return TRUE */` |
|    3 |  300 | `	ph7_result_bool(pCtx,1);` |
|    3 |  301 | `	return PH7_OK;` |
|    2 |  302 |  |
|    - |  303 | `/*` |
|    - |  304 | ` * bool xml_set_end_namespace_decl_handler(resource $parser,callback $handler)` |
|    - |  305 | ` *  Set up end namespace declaration handler.` |
|    - |  306 | ` * Parameters` |
|    - |  307 | ` * $parser` |
|    - |  308 | ` *   A reference to the XML parser to set up character data handler function.` |
|    - |  309 | ` * $handler` |
|    - |  310 | ` *  handler is a string containing the name of the callback.` |
|    - |  311 | ` *  The function named by handler must accept two parameters:` |
|    - |  312 | ` *   handler(resource $parser,string $prefix)` |
|    - |  313 | ` *  $parser` |
|    - |  314 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|    - |  315 | ` *  $prefix` |
|    - |  316 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|    - |  317 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|    - |  318 | ` *   can also be supplied.` |
|    - |  319 | ` * Return` |
|    - |  320 | ` *  TRUE on success or FALSE on failure.` |
|    - |  321 | ` */` |
|    2 |  322 | `PH7_PRIVATE int vm_builtin_xml_set_end_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  323 |  |
|    - |  324 | `	ph7_xml_engine *pEngine;` |
|    3 |  325 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  326 | `		/* Missing/Ivalid argument,return FALSE */` |
|  ! 0 |  327 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  328 | `		return PH7_OK;` |
|    - |  329 | `	}` |
|    - |  330 | `	/* Point to the XML engine */` |
|    3 |  331 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|    3 |  332 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|    - |  333 | `		/* Corrupt engine,return FALSE */` |
|  ! 0 |  334 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  335 | `		return PH7_OK;` |
|    - |  336 | `	}` |
|    3 |  337 | `	if( nArg > 1 ){` |
|    - |  338 | `		/* Save the user callback for later invocation */` |
|    3 |  339 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_NS_END]);` |
|    1 |  340 | `	}` |
|    - |  341 | `	/* All done,return TRUE */` |
|    3 |  342 | `	ph7_result_bool(pCtx,1);` |
|    3 |  343 | `	return PH7_OK;` |
|    2 |  344 |  |
|    - |  345 | `/*` |
|    - |  346 | ` * bool xml_set_start_namespace_decl_handler(resource $parser,callback $handler)` |
|    - |  347 | ` *  Set up start namespace declaration handler.` |
|    - |  348 | ` * Parameters` |
|    - |  349 | ` * $parser` |
|    - |  350 | ` *   A reference to the XML parser to set up character data handler function.` |
|    - |  351 | ` * $handler` |
|    - |  352 | ` *  handler is a string containing the name of the callback.` |
|    - |  353 | ` *  The function named by handler must accept two parameters:` |
|    - |  354 | ` *   handler(resource $parser,string $prefix,string $uri)` |
|    - |  355 | ` *  $parser` |
|    - |  356 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|    - |  357 | ` *  $prefix` |
|    - |  358 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|    - |  359 | ` *  $uri` |
|    - |  360 | ` *    Uniform Resource Identifier (URI) of namespace.` |
|    - |  361 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|    - |  362 | ` *   can also be supplied.` |
|    - |  363 | ` * Return` |
|    - |  364 | ` *  TRUE on success or FALSE on failure.` |
|    - |  365 | ` */` |
|    2 |  366 | `PH7_PRIVATE int vm_builtin_xml_set_start_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  367 |  |
|    - |  368 | `	ph7_xml_engine *pEngine;` |
|    3 |  369 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  370 | `		/* Missing/Ivalid argument,return FALSE */` |
|  ! 0 |  371 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  372 | `		return PH7_OK;` |
|    - |  373 | `	}` |
|    - |  374 | `	/* Point to the XML engine */` |
|    3 |  375 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|    3 |  376 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|    - |  377 | `		/* Corrupt engine,return FALSE */` |
|  ! 0 |  378 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  379 | `		return PH7_OK;` |
|    - |  380 | `	}` |
|    3 |  381 | `	if( nArg > 1 ){` |
|    - |  382 | `		/* Save the user callback for later invocation */` |
|    3 |  383 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_NS_START]);` |
|    1 |  384 | `	}` |
|    - |  385 | `	/* All done,return TRUE */` |
|    3 |  386 | `	ph7_result_bool(pCtx,1);` |
|    3 |  387 | `	return PH7_OK;` |
|    2 |  388 |  |
|    - |  389 | `/*` |
|    - |  390 | ` * bool xml_set_processing_instruction_handler(resource $parser,callback $handler)` |
|    - |  391 | ` *  Set up processing instruction (PI) handler.` |
|    - |  392 | ` * Parameters` |
|    - |  393 | ` * $parser` |
|    - |  394 | ` *   A reference to the XML parser to set up character data handler function.` |
|    - |  395 | ` * $handler` |
|    - |  396 | ` *  handler is a string containing the name of the callback.` |
|    - |  397 | ` *  The function named by handler must accept three parameters:` |
|    - |  398 | ` *   handler(resource $parser,string $target,string $data)` |
|    - |  399 | ` *  $parser` |
|    - |  400 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|    - |  401 | ` *  $target` |
|    - |  402 | ` *   The second parameter, target, contains the PI target.` |
|    - |  403 | ` *  $data` |
|    - |  404 | `     The third parameter, data, contains the PI data.` |
|    - |  405 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|    - |  406 | ` *   can also be supplied.` |
|    - |  407 | ` * Return` |
|    - |  408 | ` *  TRUE on success or FALSE on failure.` |
|    - |  409 | ` */` |
|    8 |  410 | `PH7_PRIVATE int vm_builtin_xml_set_processing_instruction_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  411 |  |
|    - |  412 | `	ph7_xml_engine *pEngine;` |
|    9 |  413 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  414 | `		/* Missing/Ivalid argument,return FALSE */` |
|  ! 0 |  415 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  416 | `		return PH7_OK;` |
|    - |  417 | `	}` |
|    - |  418 | `	/* Point to the XML engine */` |
|    9 |  419 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|    9 |  420 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|    - |  421 | `		/* Corrupt engine,return FALSE */` |
|  ! 0 |  422 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  423 | `		return PH7_OK;` |
|    - |  424 | `	}` |
|    9 |  425 | `	if( nArg > 1 ){` |
|    - |  426 | `		/* Save the user callback for later invocation */` |
|    9 |  427 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_PI]);` |
|    4 |  428 | `	}` |
|    - |  429 | `	/* All done,return TRUE */` |
|    9 |  430 | `	ph7_result_bool(pCtx,1);` |
|    9 |  431 | `	return PH7_OK;` |
|    5 |  432 |  |
|    - |  433 | `/*` |
|    - |  434 | ` * bool xml_set_unparsed_entity_decl_handler(resource $parser,callback $handler)` |
|    - |  435 | ` *  Set up unparsed entity declaration handler.` |
|    - |  436 | ` * Parameters` |
|    - |  437 | ` * $parser` |
|    - |  438 | ` *   A reference to the XML parser to set up character data handler function.` |
|    - |  439 | ` * $handler` |
|    - |  440 | ` *  handler is a string containing the name of the callback.` |
|    - |  441 | ` *  The function named by handler must accept six parameters:` |
|    - |  442 | ` *  handler(resource $parser,string $entity_name,string $base,string $system_id,string $public_id,string $notation_name)` |
|    - |  443 | ` *  $parser` |
|    - |  444 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|    - |  445 | ` *  $entity_name` |
|    - |  446 | ` *   The name of the entity that is about to be defined.` |
|    - |  447 | ` *  $base` |
|    - |  448 | ` *   This is the base for resolving the system identifier (systemId) of the external entity.` |
|    - |  449 | ` *   Currently this parameter will always be set to an empty string.` |
|    - |  450 | ` *  $system_id` |
|    - |  451 | ` *   System identifier for the external entity.` |
|    - |  452 | ` *  $public_id` |
|    - |  453 | ` *    Public identifier for the external entity.` |
|    - |  454 | ` *  $notation_name` |
|    - |  455 | ` *    Name of the notation of this entity (see xml_set_notation_decl_handler()).` |
|    - |  456 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|    - |  457 | ` *   can also be supplied.` |
|    - |  458 | ` * Return` |
|    - |  459 | ` *  TRUE on success or FALSE on failure.` |
|    - |  460 | ` */` |
|    2 |  461 | `PH7_PRIVATE int vm_builtin_xml_set_unparsed_entity_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  462 |  |
|    - |  463 | `	ph7_xml_engine *pEngine;` |
|    3 |  464 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  465 | `		/* Missing/Ivalid argument,return FALSE */` |
|  ! 0 |  466 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  467 | `		return PH7_OK;` |
|    - |  468 | `	}` |
|    - |  469 | `	/* Point to the XML engine */` |
|    3 |  470 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|    3 |  471 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|    - |  472 | `		/* Corrupt engine,return FALSE */` |
|  ! 0 |  473 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  474 | `		return PH7_OK;` |
|    - |  475 | `	}` |
|    3 |  476 | `	if( nArg > 1 ){` |
|    - |  477 | `		/* Save the user callback for later invocation */` |
|    3 |  478 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_UNPED]);` |
|    1 |  479 | `	}` |
|    - |  480 | `	/* All done,return TRUE */` |
|    3 |  481 | `	ph7_result_bool(pCtx,1);` |
|    3 |  482 | `	return PH7_OK;` |
|    2 |  483 |  |
|    - |  484 | `/*` |
|    - |  485 | ` * bool xml_set_notation_decl_handler(resource $parser,callback $handler)` |
|    - |  486 | ` *  Set up notation declaration handler.` |
|    - |  487 | ` * Parameters` |
|    - |  488 | ` * $parser` |
|    - |  489 | ` *   A reference to the XML parser to set up character data handler function.` |
|    - |  490 | ` * $handler` |
|    - |  491 | ` *  handler is a string containing the name of the callback.` |
|    - |  492 | ` *  The function named by handler must accept five parameters:` |
|    - |  493 | ` *  handler(resource $parser,string $entity_name,string $base,string $system_id,string $public_id)` |
|    - |  494 | ` *  $parser` |
|    - |  495 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|    - |  496 | ` *  $entity_name` |
|    - |  497 | ` *   The name of the entity that is about to be defined.` |
|    - |  498 | ` *  $base` |
|    - |  499 | ` *   This is the base for resolving the system identifier (systemId) of the external entity.` |
|    - |  500 | ` *   Currently this parameter will always be set to an empty string.` |
|    - |  501 | ` *  $system_id` |
|    - |  502 | ` *   System identifier for the external entity.` |
|    - |  503 | ` *  $public_id` |
|    - |  504 | ` *    Public identifier for the external entity.` |
|    - |  505 | ` *  Note: Instead of a function name, an array containing an object reference and a method name` |
|    - |  506 | ` *  can also be supplied.` |
|    - |  507 | ` * Return` |
|    - |  508 | ` *  TRUE on success or FALSE on failure.` |
|    - |  509 | ` */` |
|    2 |  510 | `PH7_PRIVATE int vm_builtin_xml_set_notation_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  511 |  |
|    - |  512 | `	ph7_xml_engine *pEngine;` |
|    3 |  513 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  514 | `		/* Missing/Ivalid argument,return FALSE */` |
|  ! 0 |  515 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  516 | `		return PH7_OK;` |
|    - |  517 | `	}` |
|    - |  518 | `	/* Point to the XML engine */` |
|    3 |  519 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|    3 |  520 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|    - |  521 | `		/* Corrupt engine,return FALSE */` |
|  ! 0 |  522 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  523 | `		return PH7_OK;` |
|    - |  524 | `	}` |
|    3 |  525 | `	if( nArg > 1 ){` |
|    - |  526 | `		/* Save the user callback for later invocation */` |
|    3 |  527 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_ND]);` |
|    1 |  528 | `	}` |
|    - |  529 | `	/* All done,return TRUE */` |
|    3 |  530 | `	ph7_result_bool(pCtx,1);` |
|    3 |  531 | `	return PH7_OK;` |
|    2 |  532 |  |
|    - |  533 | `/*` |
|    - |  534 | ` * bool xml_set_external_entity_ref_handler(resource $parser,callback $handler)` |
|    - |  535 | ` *  Set up external entity reference handler.` |
|    - |  536 | ` * Parameters` |
|    - |  537 | ` * $parser` |
|    - |  538 | ` *   A reference to the XML parser to set up character data handler function.` |
|    - |  539 | ` * $handler` |
|    - |  540 | ` *  handler is a string containing the name of the callback.` |
|    - |  541 | ` *  The function named by handler must accept five parameters:` |
|    - |  542 | ` *   handler(resource $parser,string $open_entity_names,string $base,string $system_id,string $public_id)` |
|    - |  543 | ` *  $parser` |
|    - |  544 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|    - |  545 | ` *  $open_entity_names` |
|    - |  546 | ` *   The second parameter, open_entity_names, is a space-separated list of the names` |
|    - |  547 | ` *   of the entities that are open for the parse of this entity (including the name of the referenced entity).` |
|    - |  548 | ` *  $base` |
|    - |  549 | ` *   This is the base for resolving the system identifier (system_id) of the external entity.` |
|    - |  550 | ` *   Currently this parameter will always be set to an empty string.` |
|    - |  551 | ` *  $system_id` |
|    - |  552 | ` *   The fourth parameter, system_id, is the system identifier as specified in the entity declaration.` |
|    - |  553 | ` *  $public_id` |
|    - |  554 | ` *   The fifth parameter, public_id, is the public identifier as specified in the entity declaration` |
|    - |  555 | ` *   or an empty string if none was specified; the whitespace in the public identifier will have been` |
|    - |  556 | ` *   normalized as required by the XML spec.` |
|    - |  557 | ` * Note: Instead of a function name, an array containing an object reference and a method name` |
|    - |  558 | ` * can also be supplied.` |
|    - |  559 | ` * Return` |
|    - |  560 | ` *  TRUE on success or FALSE on failure.` |
|    - |  561 | ` */` |
|    2 |  562 | `PH7_PRIVATE int vm_builtin_xml_set_external_entity_ref_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  563 |  |
|    - |  564 | `	ph7_xml_engine *pEngine;` |
|    3 |  565 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  566 | `		/* Missing/Ivalid argument,return FALSE */` |
|  ! 0 |  567 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  568 | `		return PH7_OK;` |
|    - |  569 | `	}` |
|    - |  570 | `	/* Point to the XML engine */` |
|    3 |  571 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|    3 |  572 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|    - |  573 | `		/* Corrupt engine,return FALSE */` |
|  ! 0 |  574 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  575 | `		return PH7_OK;` |
|    - |  576 | `	}` |
|    3 |  577 | `	if( nArg > 1 ){` |
|    - |  578 | `		/* Save the user callback for later invocation */` |
|    3 |  579 | `		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_EER]);` |
|    1 |  580 | `	}` |
|    - |  581 | `	/* All done,return TRUE */` |
|    3 |  582 | `	ph7_result_bool(pCtx,1);` |
|    3 |  583 | `	return PH7_OK;` |
|    2 |  584 |  |
|    - |  585 | `/*` |
|    - |  586 | ` * int xml_get_current_line_number(resource $parser)` |
|    - |  587 | ` *  Gets the current line number for the given XML parser.` |
|    - |  588 | ` * Parameters` |
|    - |  589 | ` * $parser` |
|    - |  590 | ` *   A reference to the XML parser.` |
|    - |  591 | ` * Return` |
|    - |  592 | ` *  This function returns FALSE if parser does not refer` |
|    - |  593 | ` *  to a valid parser, or else it returns which line the parser` |
|    - |  594 | ` *  is currently at in its data buffer.` |
|    - |  595 | ` */` |
|    8 |  596 | `PH7_PRIVATE int vm_builtin_xml_get_current_line_number(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  597 |  |
|    - |  598 | `	ph7_xml_engine *pEngine;` |
|    9 |  599 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  600 | `		/* Missing/Ivalid argument,return FALSE */` |
|  ! 0 |  601 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  602 | `		return PH7_OK;` |
|    - |  603 | `	}` |
|    - |  604 | `	/* Point to the XML engine */` |
|    9 |  605 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|    9 |  606 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|    - |  607 | `		/* Corrupt engine,return FALSE */` |
|  ! 0 |  608 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  609 | `		return PH7_OK;` |
|    - |  610 | `	}` |
|    - |  611 | `	/* Return the line number */` |
|    9 |  612 | `	ph7_result_int(pCtx,(int)pEngine->nLine);` |
|    9 |  613 | `	return PH7_OK;` |
|    5 |  614 |  |
|    - |  615 | `/*` |
|    - |  616 | ` * int xml_get_current_byte_index(resource $parser)` |
|    - |  617 | ` *  Gets the current byte index of the given XML parser.` |
|    - |  618 | ` * Parameters` |
|    - |  619 | ` * $parser` |
|    - |  620 | ` *   A reference to the XML parser.` |
|    - |  621 | ` * Return` |
|    - |  622 | ` *  This function returns FALSE if parser does not refer to a valid` |
|    - |  623 | ` *  parser, or else it returns which byte index the parser is currently` |
|    - |  624 | ` *  at in its data buffer (starting at 0).` |
|    - |  625 | ` */` |
|    4 |  626 | `PH7_PRIVATE int vm_builtin_xml_get_current_byte_index(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  627 |  |
|    - |  628 | `	ph7_xml_engine *pEngine;` |
|    - |  629 | `	SyStream *pStream;` |
|    - |  630 | `	SyToken *pToken;` |
|    5 |  631 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  632 | `		/* Missing/Ivalid argument,return FALSE */` |
|  ! 0 |  633 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  634 | `		return PH7_OK;` |
|    - |  635 | `	}` |
|    - |  636 | `	/* Point to the XML engine */` |
|    5 |  637 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|    5 |  638 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|    - |  639 | `		/* Corrupt engine,return FALSE */` |
|  ! 0 |  640 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  641 | `		return PH7_OK;` |
|    - |  642 | `	}` |
|    - |  643 | `	/* Point to the current processed token */` |
|    5 |  644 | `	pToken = (SyToken *)SySetPeekCurrentEntry(&pEngine->sParser.sToken);` |
|    5 |  645 | `	if( pToken == 0 ){` |
|    - |  646 | `		/* Stream not yet processed */` |
|    3 |  647 | `		ph7_result_int(pCtx,0);` |
|    3 |  648 | `		return 0;` |
|    - |  649 | `	}` |
|    - |  650 | `	/* Point to the input stream */` |
|    3 |  651 | `	pStream = &pEngine->sParser.sLex.sStream;` |
|    - |  652 | `	/* Return the byte index */` |
|    3 |  653 | `	ph7_result_int64(pCtx,(ph7_int64)(pToken->sData.zString-(const char *)pStream->zInput));` |
|    3 |  654 | `	return PH7_OK;` |
|    3 |  655 |  |
|    - |  656 | `/*` |
|    - |  657 | ` * bool xml_set_object(resource $parser,object &$object)` |
|    - |  658 | ` *  Use XML Parser within an object.` |
|    - |  659 | ` * NOTE` |
|    - |  660 | ` *  This function is depreceated and is a no-op.` |
|    - |  661 | ` * Parameters` |
|    - |  662 | ` * $parser` |
|    - |  663 | ` *   A reference to the XML parser.` |
|    - |  664 | ` * $object` |
|    - |  665 | ` *  The object where to use the XML parser.` |
|    - |  666 | ` * Return` |
|    - |  667 | ` * Always FALSE.` |
|    - |  668 | ` */` |
|    2 |  669 | `PH7_PRIVATE int vm_builtin_xml_set_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  670 |  |
|    - |  671 | `	ph7_xml_engine *pEngine;` |
|    3 |  672 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_object(apArg[1]) ){` |
|    - |  673 | `		/* Missing/Ivalid argument,return FALSE */` |
|    3 |  674 | `		ph7_result_bool(pCtx,0);` |
|    3 |  675 | `		return PH7_OK;` |
|    - |  676 | `	}` |
|    - |  677 | `	/* Point to the XML engine */` |
|  ! 0 |  678 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|  ! 0 |  679 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|    - |  680 | `		/* Corrupt engine,return FALSE */` |
|  ! 0 |  681 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  682 | `		return PH7_OK;` |
|    - |  683 | `	}` |
|    - |  684 | `	/*  Throw a notice and return */` |
|  ! 0 |  685 | `	ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"This function is depreceated and is a no-op."` |
|    - |  686 | `		"In order to mimic this behaviour,you can supply instead of a function name an array "` |
|    - |  687 | `		"containing an object reference and a method name."` |
|    - |  688 | `		);` |
|    - |  689 | `	/* Return FALSE */` |
|  ! 0 |  690 | `	ph7_result_bool(pCtx,0);` |
|  ! 0 |  691 | `	return PH7_OK;` |
|    2 |  692 |  |
|    - |  693 | `/*` |
|    - |  694 | ` * int xml_get_current_column_number(resource $parser)` |
|    - |  695 | ` *  Gets the current column number of the given XML parser.` |
|    - |  696 | ` * Parameters` |
|    - |  697 | ` * $parser` |
|    - |  698 | ` *   A reference to the XML parser.` |
|    - |  699 | ` * Return` |
|    - |  700 | ` *  This function returns FALSE if parser does not refer to a valid parser, or else it returns` |
|    - |  701 | ` *  which column on the current line (as given by xml_get_current_line_number()) the parser` |
|    - |  702 | ` *  is currently at.` |
|    - |  703 | ` */` |
|    4 |  704 | `PH7_PRIVATE int vm_builtin_xml_get_current_column_number(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  705 |  |
|    - |  706 | `	ph7_xml_engine *pEngine;` |
|    - |  707 | `	SyStream *pStream;` |
|    - |  708 | `	SyToken *pToken;` |
|    5 |  709 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  710 | `		/* Missing/Ivalid argument,return FALSE */` |
|  ! 0 |  711 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  712 | `		return PH7_OK;` |
|    - |  713 | `	}` |
|    - |  714 | `	/* Point to the XML engine */` |
|    5 |  715 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|    5 |  716 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|    - |  717 | `		/* Corrupt engine,return FALSE */` |
|  ! 0 |  718 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  719 | `		return PH7_OK;` |
|    - |  720 | `	}` |
|    - |  721 | `	/* Point to the current processed token */` |
|    5 |  722 | `	pToken = (SyToken *)SySetPeekCurrentEntry(&pEngine->sParser.sToken);` |
|    5 |  723 | `	if( pToken == 0 ){` |
|    - |  724 | `		/* Stream not yet processed */` |
|  ! 0 |  725 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  726 | `		return 0;` |
|    - |  727 | `	}` |
|    - |  728 | `	/* Point to the input stream */` |
|    5 |  729 | `	pStream = &pEngine->sParser.sLex.sStream;` |
|    - |  730 | `	/* Return the byte index */` |
|    5 |  731 | `	ph7_result_int64(pCtx,(ph7_int64)(pToken->sData.zString-(const char *)pStream->zInput)/80);` |
|    5 |  732 | `	return PH7_OK;` |
|    3 |  733 |  |
|    - |  734 | `/*` |
|    - |  735 | ` * int xml_get_error_code(resource $parser)` |
|    - |  736 | ` *  Get XML parser error code.` |
|    - |  737 | ` * Parameters` |
|    - |  738 | ` * $parser` |
|    - |  739 | ` *   A reference to the XML parser.` |
|    - |  740 | ` * Return` |
|    - |  741 | ` *  This function returns FALSE if parser does not refer to a valid` |
|    - |  742 | ` *  parser, or else it returns one of the error codes listed in the error` |
|    - |  743 | ` *  codes section.` |
|    - |  744 | ` */` |
|   32 |  745 | `PH7_PRIVATE int vm_builtin_xml_get_error_code(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  746 |  |
|    - |  747 | `	ph7_xml_engine *pEngine;` |
|   33 |  748 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  749 | `		/* Missing/Ivalid argument,return FALSE */` |
|  ! 0 |  750 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  751 | `		return PH7_OK;` |
|    - |  752 | `	}` |
|    - |  753 | `	/* Point to the XML engine */` |
|   33 |  754 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|   33 |  755 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|    - |  756 | `		/* Corrupt engine,return FALSE */` |
|  ! 0 |  757 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  758 | `		return PH7_OK;` |
|    - |  759 | `	}` |
|    - |  760 | `	/* Return the error code if any */` |
|   33 |  761 | `	ph7_result_int(pCtx,pEngine->iErrCode);` |
|   33 |  762 | `	return PH7_OK;` |
|   17 |  763 |  |
|    - |  764 | `/*` |
|    - |  765 | ` * XML parser event callbacks` |
|    - |  766 | ` * Each time the unserlying XML parser extract a single token` |
|    - |  767 | ` * from the input,one of the following callbacks are invoked.` |
|    - |  768 | ` * IMP-XML-ENGINE-07-07-2012 22:02 FreeBSD [chm@symisc.net]` |
|    - |  769 | ` */` |
|    - |  770 | `/*` |
|    - |  771 | ` * Create a scalar ph7_value holding the value` |
|    - |  772 | ` * of an XML tag/attribute/CDATA and so on.` |
|    - |  773 | ` */` |
|  148 |  774 | `static ph7_value * VmXMLValue(ph7_xml_engine *pEngine,SyXMLRawStr *pXML,SyXMLRawStr *pNsUri)` |
|    1 |  775 |  |
|    - |  776 | `	ph7_value *pValue;` |
|    - |  777 | `	/* Allocate a new scalar variable */` |
|  149 |  778 | `	pValue = ph7_context_new_scalar(pEngine->pCtx);` |
|  149 |  779 | `	if( pValue == 0 ){` |
|  ! 0 |  780 | `		ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|  ! 0 |  781 | `		return 0;` |
|    - |  782 | `	}` |
|  149 |  783 | `	if( pNsUri && pNsUri->nByte > 0 ){` |
|    - |  784 | `		/* Append namespace URI and the separator */` |
|    9 |  785 | `		ph7_value_string_format(pValue,"%.*s%c",pNsUri->nByte,pNsUri->zString,pEngine->ns_sep);` |
|    4 |  786 | `	}` |
|    - |  787 | `	/* Copy the tag value */` |
|  149 |  788 | `	ph7_value_string(pValue,pXML->zString,(int)pXML->nByte);` |
|  149 |  789 | `	return pValue;` |
|   75 |  790 |  |
|    - |  791 | `/*` |
|    - |  792 | ` * Create a 'ph7_value' of type array holding the values` |
|    - |  793 | ` * of an XML tag attributes.` |
|    - |  794 | ` */` |
|   62 |  795 | `static ph7_value * VmXMLAttrValue(ph7_xml_engine *pEngine,SyXMLRawStr *aAttr,sxu32 nAttr)` |
|    1 |  796 |  |
|    - |  797 | `	ph7_value *pArray;` |
|    - |  798 | `	/* Create an empty array */` |
|   63 |  799 | `	pArray = ph7_context_new_array(pEngine->pCtx);` |
|   63 |  800 | `	if( pArray == 0 ){` |
|  ! 0 |  801 | `		ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|  ! 0 |  802 | `		return 0;` |
|    - |  803 | `	}` |
|   63 |  804 | `	if( nAttr > 0 ){` |
|    - |  805 | `		ph7_value *pKey,*pValue;` |
|    - |  806 | `		sxu32 n;` |
|    - |  807 | `		/* Create worker variables */` |
|    5 |  808 | `		pKey = ph7_context_new_scalar(pEngine->pCtx);` |
|    5 |  809 | `		pValue = ph7_context_new_scalar(pEngine->pCtx);` |
|    5 |  810 | `		if( pKey == 0 \|\| pValue == 0 ){` |
|  ! 0 |  811 | `			ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|  ! 0 |  812 | `			return 0;` |
|    - |  813 | `		}` |
|    - |  814 | `		/* Copy attributes */` |
|    9 |  815 | `		for( n = 0 ; n < nAttr ; n += 2 ){` |
|    - |  816 | `			/* Reset string cursors */` |
|    5 |  817 | `			ph7_value_reset_string_cursor(pKey);` |
|    5 |  818 | `			ph7_value_reset_string_cursor(pValue);` |
|    - |  819 | `			/* Copy attribute name and it's associated value */` |
|    5 |  820 | `			ph7_value_string(pKey,aAttr[n].zString,(int)aAttr[n].nByte); /* Attribute name */` |
|    5 |  821 | `			ph7_value_string(pValue,aAttr[n+1].zString,(int)aAttr[n+1].nByte); /* Attribute value */` |
|    - |  822 | `			/* Insert in the array */` |
|    5 |  823 | `			ph7_array_add_elem(pArray,pKey,pValue); /* Will make it's own copy */` |
|    3 |  824 | `		}` |
|    - |  825 | `		/* Release the worker variables */` |
|    5 |  826 | `		ph7_context_release_value(pEngine->pCtx,pKey);` |
|    5 |  827 | `		ph7_context_release_value(pEngine->pCtx,pValue);` |
|    2 |  828 | `	}` |
|    - |  829 | `	/* Return the freshly created array */` |
|   63 |  830 | `	return pArray;` |
|   32 |  831 |  |
|    - |  832 | `/*` |
|    - |  833 | ` * Start element handler.` |
|    - |  834 | ` * The user defined callback must accept three parameters:` |
|    - |  835 | ` *    start_element_handler(resource $parser,string $name,array $attribs )` |
|    - |  836 | ` *    $parser` |
|    - |  837 | ` *      The first parameter, parser, is a reference to the XML parser calling the handler.` |
|    - |  838 | ` *    $name` |
|    - |  839 | ` *      The second parameter, name, contains the name of the element for which this handler` |
|    - |  840 | ` *		is called.If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|    - |  841 | ` *    $attribs` |
|    - |  842 | ` *      The third parameter, attribs, contains an associative array with the element's attributes (if any).` |
|    - |  843 | ` *		The keys of this array are the attribute names, the values are the attribute values.` |
|    - |  844 | ` *      Attribute names are case-folded on the same criteria as element names.Attribute values are not case-folded.` |
|    - |  845 | ` *      The original order of the attributes can be retrieved by walking through attribs the normal way, using each().` |
|    - |  846 | ` *      The first key in the array was the first attribute, and so on.` |
|    - |  847 | ` *      Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|    - |  848 | ` */` |
|   78 |  849 | `static sxi32 VmXMLStartElementHandler(SyXMLRawStr *pStart,SyXMLRawStr *pNS,sxu32 nAttr,SyXMLRawStr *aAttr,void *pUserData)` |
|    1 |  850 |  |
|   79 |  851 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|    - |  852 | `	ph7_value *pCallback,*pTag,*pAttr;` |
|    - |  853 | `	/* Point to the target user defined callback */` |
|   79 |  854 | `	pCallback = &pEngine->aCB[PH7_XML_START_TAG];` |
|    - |  855 | `	/* Make sure the given callback is callable */` |
|   79 |  856 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|    - |  857 | `		/* Not callable,return immediately*/` |
|   17 |  858 | `		return SXRET_OK;` |
|    - |  859 | `	}` |
|    - |  860 | `	/* Create a ph7_value holding the tag name */` |
|   63 |  861 | `	pTag = VmXMLValue(pEngine,pStart,pNS);` |
|    - |  862 | `	/* Create a ph7_value holding the tag attributes */` |
|   63 |  863 | `	pAttr = VmXMLAttrValue(pEngine,aAttr,nAttr);` |
|   63 |  864 | `	if( pTag == 0  \|\| pAttr == 0 ){` |
|  ! 0 |  865 | `		SXUNUSED(pNS); /* cc warning */` |
|    - |  866 | `		/* Out of mem,return immediately */` |
|  ! 0 |  867 | `		return SXRET_OK;` |
|    - |  868 | `	}` |
|    - |  869 | `	/* Invoke the user callback */` |
|   63 |  870 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTag,pAttr,(ph7_value*)0);` |
|    - |  871 | `	/* Clean-up the mess left behind */` |
|   63 |  872 | `	ph7_context_release_value(pEngine->pCtx,pTag);` |
|   63 |  873 | `	ph7_context_release_value(pEngine->pCtx,pAttr);` |
|   63 |  874 | `	return SXRET_OK;` |
|   40 |  875 |  |
|    - |  876 | `/*` |
|    - |  877 | ` * End element handler.` |
|    - |  878 | ` * The user defined callback must accept two parameters:` |
|    - |  879 | ` *  end_element_handler(resource $parser,string $name)` |
|    - |  880 | ` *  $parser` |
|    - |  881 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|    - |  882 | ` *  $name` |
|    - |  883 | ` *   The second parameter, name, contains the name of the element for which this handler is called.` |
|    - |  884 | ` *   If case-folding is in effect for this parser, the element name will be in uppercase letters.` |
|    - |  885 | ` *   Note: Instead of a function name, an array containing an object reference and a method name` |
|    - |  886 | ` *   can also be supplied.` |
|    - |  887 | ` */` |
|   62 |  888 | `static sxi32 VmXMLEndElementHandler(SyXMLRawStr *pEnd,SyXMLRawStr *pNS,void *pUserData)` |
|    1 |  889 |  |
|   63 |  890 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|    - |  891 | `	ph7_value *pCallback,*pTag;` |
|    - |  892 | `	/* Point to the target user defined callback */` |
|   63 |  893 | `	pCallback = &pEngine->aCB[PH7_XML_END_TAG];` |
|    - |  894 | `	/* Make sure the given callback is callable */` |
|   63 |  895 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|    - |  896 | `		/* Not callable,return immediately*/` |
|    9 |  897 | `		return SXRET_OK;` |
|    - |  898 | `	}` |
|    - |  899 | `	/* Create a ph7_value holding the tag name */` |
|   55 |  900 | `	pTag = VmXMLValue(pEngine,pEnd,pNS);` |
|   55 |  901 | `	if( pTag == 0  ){` |
|  ! 0 |  902 | `		SXUNUSED(pNS); /* cc warning */` |
|    - |  903 | `		/* Out of mem,return immediately */` |
|  ! 0 |  904 | `		return SXRET_OK;` |
|    - |  905 | `	}` |
|    - |  906 | `	/* Invoke the user callback */` |
|   55 |  907 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTag,(ph7_value*)0);` |
|    - |  908 | `	/* Clean-up the mess left behind */` |
|   55 |  909 | `	ph7_context_release_value(pEngine->pCtx,pTag);` |
|   55 |  910 | `	return SXRET_OK;` |
|   32 |  911 |  |
|    - |  912 | `/*` |
|    - |  913 | ` * Character data handler.` |
|    - |  914 | ` *  The user defined callback must accept two parameters:` |
|    - |  915 | ` *  handler(resource $parser,string $data)` |
|    - |  916 | ` *  $parser` |
|    - |  917 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|    - |  918 | ` *  $data` |
|    - |  919 | ` *   The second parameter, data, contains the character data as a string.` |
|    - |  920 | ` *   Character data handler is called for every piece of a text in the XML document.` |
|    - |  921 | ` *   It can be called multiple times inside each fragment (e.g. for non-ASCII strings).` |
|    - |  922 | ` *   If a handler function is set to an empty string, or FALSE, the handler in question is disabled.` |
|    - |  923 | ` *   Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.` |
|    - |  924 | ` */` |
|   28 |  925 | `static sxi32 VmXMLTextHandler(SyXMLRawStr *pText,void *pUserData)` |
|    1 |  926 |  |
|   29 |  927 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|    - |  928 | `	ph7_value *pCallback,*pData;` |
|    - |  929 | `	/* Point to the target user defined callback */` |
|   29 |  930 | `	pCallback = &pEngine->aCB[PH7_XML_CDATA];` |
|    - |  931 | `	/* Make sure the given callback is callable */` |
|   29 |  932 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|    - |  933 | `		/* Not callable,return immediately*/` |
|   11 |  934 | `		return SXRET_OK;` |
|    - |  935 | `	}` |
|    - |  936 | `	/* Create a ph7_value holding the data */` |
|   19 |  937 | `	pData = VmXMLValue(pEngine,&(*pText),0);` |
|   19 |  938 | `	if( pData == 0  ){` |
|    - |  939 | `		/* Out of mem,return immediately */` |
|  ! 0 |  940 | `		return SXRET_OK;` |
|    - |  941 | `	}` |
|    - |  942 | `	/* Invoke the user callback */` |
|   19 |  943 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pData,(ph7_value*)0);` |
|    - |  944 | `	/* Clean-up the mess left behind */` |
|   19 |  945 | `	ph7_context_release_value(pEngine->pCtx,pData);` |
|   19 |  946 | `	return SXRET_OK;` |
|   15 |  947 |  |
|    - |  948 | `/*` |
|    - |  949 | ` * Processing instruction (PI) handler.` |
|    - |  950 | ` * The user defined callback must accept two parameters:` |
|    - |  951 | ` *   handler(resource $parser,string $target,string $data)` |
|    - |  952 | ` *  $parser` |
|    - |  953 | ` *    The first parameter, parser, is a reference to the XML parser calling the handler.` |
|    - |  954 | ` *  $target` |
|    - |  955 | ` *   The second parameter, target, contains the PI target.` |
|    - |  956 | ` *  $data` |
|    - |  957 | ` *    The third parameter, data, contains the PI data.` |
|    - |  958 | ` *    Note: Instead of a function name, an array containing an object reference` |
|    - |  959 | ` *    and a method name can also be supplied.` |
|    - |  960 | ` */` |
|    8 |  961 | `static sxi32 VmXMLPIHandler(SyXMLRawStr *pTargetStr,SyXMLRawStr *pDataStr,void *pUserData)` |
|    1 |  962 |  |
|    9 |  963 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|    - |  964 | `	ph7_value *pCallback,*pTarget,*pData;` |
|    - |  965 | `	/* Point to the target user defined callback */` |
|    9 |  966 | `	pCallback = &pEngine->aCB[PH7_XML_PI];` |
|    - |  967 | `	/* Make sure the given callback is callable */` |
|    9 |  968 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|    - |  969 | `		/* Not callable,return immediately*/` |
|    5 |  970 | `		return SXRET_OK;` |
|    - |  971 | `	}` |
|    - |  972 | `	/* Get a ph7_value holding the data */` |
|    5 |  973 | `	pTarget = VmXMLValue(pEngine,&(*pTargetStr),0);` |
|    5 |  974 | `	pData = VmXMLValue(pEngine,&(*pDataStr),0);` |
|    5 |  975 | `	if( pTarget == 0 \|\| pData == 0  ){` |
|    - |  976 | `		/* Out of mem,return immediately */` |
|  ! 0 |  977 | `		return SXRET_OK;` |
|    - |  978 | `	}` |
|    - |  979 | `	/* Invoke the user callback */` |
|    5 |  980 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTarget,pData,(ph7_value*)0);` |
|    - |  981 | `	/* Clean-up the mess left behind */` |
|    5 |  982 | `	ph7_context_release_value(pEngine->pCtx,pTarget);` |
|    5 |  983 | `	ph7_context_release_value(pEngine->pCtx,pData);` |
|    5 |  984 | `	return SXRET_OK;` |
|    5 |  985 |  |
|    - |  986 | `/*` |
|    - |  987 | ` * Namespace declaration handler.` |
|    - |  988 | ` * The user defined callback must accept two parameters:` |
|    - |  989 | ` *    handler(resource $parser,string $prefix,string $uri)` |
|    - |  990 | ` * $parser` |
|    - |  991 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|    - |  992 | ` * $prefix` |
|    - |  993 | ` *   The prefix is a string used to reference the namespace within an XML object.` |
|    - |  994 | ` * $uri` |
|    - |  995 | ` *   Uniform Resource Identifier (URI) of namespace.` |
|    - |  996 | ` *   Note: Instead of a function name, an array containing an object reference` |
|    - |  997 | ` *   and a method name can also be supplied.` |
|    - |  998 | ` */` |
|    4 |  999 | `static sxi32 VmXMLNSStartHandler(SyXMLRawStr *pUriStr,SyXMLRawStr *pPrefixStr,void *pUserData)` |
|    1 | 1000 |  |
|    5 | 1001 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|    - | 1002 | `	ph7_value *pCallback,*pUri,*pPrefix;` |
|    - | 1003 | `	/* Point to the target user defined callback */` |
|    5 | 1004 | `	pCallback = &pEngine->aCB[PH7_XML_NS_START];` |
|    - | 1005 | `	/* Make sure the given callback is callable */` |
|    5 | 1006 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|    - | 1007 | `		/* Not callable,return immediately*/` |
|    3 | 1008 | `		return SXRET_OK;` |
|    - | 1009 | `	}` |
|    - | 1010 | `	/* Get a ph7_value holding the PREFIX/URI */` |
|    3 | 1011 | `	pUri = VmXMLValue(pEngine,pUriStr,0);` |
|    3 | 1012 | `	pPrefix = VmXMLValue(pEngine,pPrefixStr,0);` |
|    3 | 1013 | `	if( pUri == 0 \|\| pPrefix == 0  ){` |
|    - | 1014 | `		/* Out of mem,return immediately */` |
|  ! 0 | 1015 | `		return SXRET_OK;` |
|    - | 1016 | `	}` |
|    - | 1017 | `	/* Invoke the user callback */` |
|    3 | 1018 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pUri,pPrefix,(ph7_value*)0);` |
|    - | 1019 | `	/* Clean-up the mess left behind */` |
|    3 | 1020 | `	ph7_context_release_value(pEngine->pCtx,pUri);` |
|    3 | 1021 | `	ph7_context_release_value(pEngine->pCtx,pPrefix);` |
|    3 | 1022 | `	return SXRET_OK;` |
|    3 | 1023 |  |
|    - | 1024 | `/*` |
|    - | 1025 | ` * Namespace end declaration handler.` |
|    - | 1026 | ` * The user defined callback must accept two parameters:` |
|    - | 1027 | ` *    handler(resource $parser,string $prefix)` |
|    - | 1028 | ` * $parser` |
|    - | 1029 | ` *   The first parameter, parser, is a reference to the XML parser calling the handler.` |
|    - | 1030 | ` * $prefix` |
|    - | 1031 | ` *  The prefix is a string used to reference the namespace within an XML object.` |
|    - | 1032 | ` *   Note: Instead of a function name, an array containing an object reference` |
|    - | 1033 | ` *   and a method name can also be supplied.` |
|    - | 1034 | ` */` |
|    4 | 1035 | `static sxi32 VmXMLNSEndHandler(SyXMLRawStr *pPrefixStr,void *pUserData)` |
|    1 | 1036 |  |
|    5 | 1037 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|    - | 1038 | `	ph7_value *pCallback,*pPrefix;` |
|    - | 1039 | `	/* Point to the target user defined callback */` |
|    5 | 1040 | `	pCallback = &pEngine->aCB[PH7_XML_NS_END];` |
|    - | 1041 | `	/* Make sure the given callback is callable */` |
|    5 | 1042 | `	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){` |
|    - | 1043 | `		/* Not callable,return immediately*/` |
|    3 | 1044 | `		return SXRET_OK;` |
|    - | 1045 | `	}` |
|    - | 1046 | `	/* Get a ph7_value holding the prefix */` |
|    3 | 1047 | `	pPrefix = VmXMLValue(pEngine,pPrefixStr,0);` |
|    3 | 1048 | `	if( pPrefix == 0 ){` |
|    - | 1049 | `		/* Out of mem,return immediately */` |
|  ! 0 | 1050 | `		return SXRET_OK;` |
|    - | 1051 | `	}` |
|    - | 1052 | `	/* Invoke the user callback */` |
|    3 | 1053 | `	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pPrefix,(ph7_value*)0);` |
|    - | 1054 | `	/* Clean-up the mess left behind */` |
|    3 | 1055 | `	ph7_context_release_value(pEngine->pCtx,pPrefix);` |
|    3 | 1056 | `	return SXRET_OK;` |
|    3 | 1057 |  |
|    - | 1058 | `/*` |
|    - | 1059 | ` * Error Message consumer handler.` |
|    - | 1060 | ` * Each time the XML parser encounter a syntaxt error or any other error` |
|    - | 1061 | ` * related to XML processing,the following callback is invoked by the` |
|    - | 1062 | ` * underlying XML parser.` |
|    - | 1063 | ` */` |
|   34 | 1064 | `static sxi32 VmXMLErrorHandler(const char *zMessage,sxi32 iErrCode,SyToken *pToken,void *pUserData)` |
|    1 | 1065 |  |
|   35 | 1066 | `	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;` |
|    - | 1067 | `	/* Save the error code */` |
|   35 | 1068 | `	pEngine->iErrCode = iErrCode;` |
|   17 | 1069 | `	SXUNUSED(zMessage); /* cc warning */` |
|   35 | 1070 | `	if( pToken ){` |
|   35 | 1071 | `		pEngine->nLine = pToken->nLine;` |
|   17 | 1072 | `	}` |
|    - | 1073 | `	/* Abort XML processing immediately */` |
|   35 | 1074 | `	return SXERR_ABORT;` |
|    1 | 1075 |  |
|    - | 1076 | `/*` |
|    - | 1077 | ` * int xml_parse(resource $parser,string $data[,bool $is_final = false ])` |
|    - | 1078 | ` *  Parses an XML document. The handlers for the configured events are called` |
|    - | 1079 | ` *  as many times as necessary.` |
|    - | 1080 | ` * Parameters` |
|    - | 1081 | ` *  $parser` |
|    - | 1082 | ` *   A reference to the XML parser.` |
|    - | 1083 | ` *  $data` |
|    - | 1084 | ` *   Chunk of data to parse. A document may be parsed piece-wise by calling` |
|    - | 1085 | ` *   xml_parse() several times with new data, as long as the is_final parameter` |
|    - | 1086 | ` *   is set and TRUE when the last data is parsed.` |
|    - | 1087 | ` * $is_final` |
|    - | 1088 | ` *   NOT USED. This implementation require that all the processed input be` |
|    - | 1089 | ` *   entirely loaded in memory.` |
|    - | 1090 | ` * Return` |
|    - | 1091 | ` *  Returns 1 on success or 0 on failure.` |
|    - | 1092 | ` */` |
|   74 | 1093 | `PH7_PRIVATE int vm_builtin_xml_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1094 |  |
|    - | 1095 | `	ph7_xml_engine *pEngine;` |
|    - | 1096 | `	SyXMLParser *pParser;` |
|    - | 1097 | `	const char *zData;` |
|    - | 1098 | `	int nByte;` |
|   75 | 1099 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|    - | 1100 | `		/* Missing/Ivalid arguments,return FALSE */` |
|  ! 0 | 1101 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1102 | `		return PH7_OK;` |
|    - | 1103 | `	}` |
|    - | 1104 | `	/* Point to the XML engine */` |
|   75 | 1105 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|   75 | 1106 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|    - | 1107 | `		/* Corrupt engine,return FALSE */` |
|  ! 0 | 1108 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1109 | `		return PH7_OK;` |
|    - | 1110 | `	}` |
|   75 | 1111 | `	if( pEngine->iNest > 0 ){` |
|    - | 1112 | `		/* This can happen when the user callback call xml_parse() again` |
|    - | 1113 | `		 * in it's body which is forbidden.` |
|    - | 1114 | `		 */` |
|  ! 0 | 1115 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|    - | 1116 | `			"Recursive call to %s,PH7 is returning false",` |
|  ! 0 | 1117 | `			ph7_function_name(pCtx)` |
|    - | 1118 | `			);` |
|    - | 1119 | `		/* Return FALSE */` |
|  ! 0 | 1120 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1121 | `		return PH7_OK;` |
|    - | 1122 | `	}` |
|   75 | 1123 | `	pEngine->pCtx = pCtx;` |
|    - | 1124 | `	/* Point to the underlying XML parser */` |
|   75 | 1125 | `	pParser = &pEngine->sParser;` |
|    - | 1126 | `	/* Register elements handler */` |
|   75 | 1127 | `	SyXMLParserSetEventHandler(pParser,pEngine,` |
|    - | 1128 | `		VmXMLStartElementHandler,` |
|    - | 1129 | `		VmXMLTextHandler,` |
|    - | 1130 | `		VmXMLErrorHandler,` |
|    - | 1131 |  |
|    - | 1132 | `		VmXMLEndElementHandler,` |
|    - | 1133 | `		VmXMLPIHandler,` |
|    - | 1134 |  |
|    - | 1135 |  |
|    - | 1136 | `		VmXMLNSStartHandler,` |
|    - | 1137 | `		VmXMLNSEndHandler` |
|    - | 1138 | `		);` |
|   75 | 1139 | `	pEngine->iErrCode = SXML_ERROR_NONE;` |
|    - | 1140 | `	/* Extract the raw XML input */` |
|   75 | 1141 | `	zData = ph7_value_to_string(apArg[1],&nByte);` |
|    - | 1142 | `	/* Start the parse process */` |
|   75 | 1143 | `	pEngine->iNest++;` |
|   75 | 1144 | `	SyXMLProcess(pParser,zData,(sxu32)nByte);` |
|   75 | 1145 | `	pEngine->iNest--;` |
|    - | 1146 | `	/* Return the parse result */` |
|   75 | 1147 | `	ph7_result_int(pCtx,pEngine->iErrCode == SXML_ERROR_NONE ? 1 : 0);` |
|   75 | 1148 | `	return PH7_OK;` |
|   38 | 1149 |  |
|    - | 1150 | `/*` |
|    - | 1151 | ` * bool xml_parser_set_option(resource $parser,int $option,mixed $value)` |
|    - | 1152 | ` *  Sets an option in an XML parser.` |
|    - | 1153 | ` * Parameters` |
|    - | 1154 | ` *  $parser` |
|    - | 1155 | ` *   A reference to the XML parser to set an option in.` |
|    - | 1156 | ` *  $option` |
|    - | 1157 | ` *    Which option to set. See below.` |
|    - | 1158 | ` *   The following options are available:` |
|    - | 1159 | ` *   XML_OPTION_CASE_FOLDING 	integer  Controls whether case-folding is enabled for this XML parser.` |
|    - | 1160 | ` *   XML_OPTION_SKIP_TAGSTART 	integer  Specify how many characters should be skipped in the beginning of a tag name.` |
|    - | 1161 | ` *   XML_OPTION_SKIP_WHITE 	    integer  Whether to skip values consisting of whitespace characters.` |
|    - | 1162 | ` *   XML_OPTION_TARGET_ENCODING string 	 Sets which target encoding to use in this XML parser.` |
|    - | 1163 | ` * $value` |
|    - | 1164 | ` *   The option's new value.` |
|    - | 1165 | ` * Return` |
|    - | 1166 | ` *  Returns 1 on success or 0 on failure.` |
|    - | 1167 | ` * Note:` |
|    - | 1168 | ` *  Well,none of these options have meaning under the built-in XML parser so a call to this` |
|    - | 1169 | ` *  function is a no-op.` |
|    - | 1170 | ` */` |
|    6 | 1171 | `PH7_PRIVATE int vm_builtin_xml_parser_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1172 |  |
|    - | 1173 | `	ph7_xml_engine *pEngine;` |
|    7 | 1174 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - | 1175 | `		/* Missing/Ivalid argument,return FALSE */` |
|  ! 0 | 1176 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1177 | `		return PH7_OK;` |
|    - | 1178 | `	}` |
|    - | 1179 | `	/* Point to the XML engine */` |
|    7 | 1180 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|    7 | 1181 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|    - | 1182 | `		/* Corrupt engine,return FALSE */` |
|  ! 0 | 1183 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1184 | `		return PH7_OK;` |
|    - | 1185 | `	}` |
|    - | 1186 | `	/* Always return FALSE */` |
|    7 | 1187 | `	ph7_result_bool(pCtx,0);` |
|    7 | 1188 | `	return PH7_OK;` |
|    4 | 1189 |  |
|    - | 1190 | `/*` |
|    - | 1191 | ` * mixed xml_parser_get_option(resource $parser,int $option)` |
|    - | 1192 | ` *  Get options from an XML parser.` |
|    - | 1193 | ` * Parameters` |
|    - | 1194 | ` *  $parser` |
|    - | 1195 | ` *   A reference to the XML parser to set an option in.` |
|    - | 1196 | ` * $option` |
|    - | 1197 | ` *   Which option to fetch.` |
|    - | 1198 | ` * Return` |
|    - | 1199 | ` *  This function returns FALSE if parser does not refer to a valid parser` |
|    - | 1200 | ` *  or if option isn't valid.Else the option's value is returned.` |
|    - | 1201 | ` */` |
|    2 | 1202 | `PH7_PRIVATE int vm_builtin_xml_parser_get_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1203 |  |
|    - | 1204 | `	ph7_xml_engine *pEngine;` |
|    - | 1205 | `	int nOp;` |
|    3 | 1206 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - | 1207 | `		/* Missing/Ivalid argument,return FALSE */` |
|  ! 0 | 1208 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1209 | `		return PH7_OK;` |
|    - | 1210 | `	}` |
|    - | 1211 | `	/* Point to the XML engine */` |
|    3 | 1212 | `	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);` |
|    3 | 1213 | `	if( IS_INVALID_XML_ENGINE(pEngine) ){` |
|    - | 1214 | `		/* Corrupt engine,return FALSE */` |
|  ! 0 | 1215 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1216 | `		return PH7_OK;` |
|    - | 1217 | `	}` |
|    - | 1218 | `	/* Extract the option */` |
|    3 | 1219 | `	nOp = ph7_value_to_int(apArg[1]);` |
|    3 | 1220 | `	switch(nOp){` |
|  ! 0 | 1221 | `	case SXML_OPTION_SKIP_TAGSTART:` |
|    - | 1222 | `	case SXML_OPTION_SKIP_WHITE:` |
|    - | 1223 | `	case SXML_OPTION_CASE_FOLDING:` |
|  ! 0 | 1224 | `		ph7_result_int(pCtx,0); break;` |
|  ! 0 | 1225 | `	case SXML_OPTION_TARGET_ENCODING:` |
|  ! 0 | 1226 | `		ph7_result_string(pCtx,"UTF-8",(int)sizeof("UTF-8")-1);` |
|  ! 0 | 1227 | `		break;` |
|    1 | 1228 | `	default:` |
|    - | 1229 | `		/* Unknown option,return FALSE*/` |
|    3 | 1230 | `		ph7_result_bool(pCtx,0);` |
|    2 | 1231 | `		break;` |
|    - | 1232 | `	}` |
|    3 | 1233 | `	return PH7_OK;` |
|    2 | 1234 |  |
|    - | 1235 | `/*` |
|    - | 1236 | ` * string xml_error_string(int $code)` |
|    - | 1237 | ` *  Gets the XML parser error string associated with the given code.` |
|    - | 1238 | ` * Parameters` |
|    - | 1239 | ` *  $code` |
|    - | 1240 | ` *   An error code from xml_get_error_code().` |
|    - | 1241 | ` * Return` |
|    - | 1242 | ` *  Returns a string with a textual description of the error` |
|    - | 1243 | ` *  code, or FALSE if no description was found.` |
|    - | 1244 | ` */` |
|   30 | 1245 | `PH7_PRIVATE int vm_builtin_xml_error_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1246 |  |
|   31 | 1247 | `	int nErr = -1;` |
|   31 | 1248 | `	if( nArg > 0 ){` |
|   31 | 1249 | `		nErr = ph7_value_to_int(apArg[0]);` |
|   15 | 1250 | `	}` |
|   31 | 1251 | `	switch(nErr){` |
|    1 | 1252 | `	case SXML_ERROR_DUPLICATE_ATTRIBUTE:` |
|    3 | 1253 | `		ph7_result_string(pCtx,"Duplicate attribute",-1/*Compute length automatically*/);` |
|    3 | 1254 | `		break;` |
|  ! 0 | 1255 | `	case SXML_ERROR_INCORRECT_ENCODING:` |
|  ! 0 | 1256 | `		ph7_result_string(pCtx,"Incorrect encoding",-1);` |
|  ! 0 | 1257 | `		break;` |
|  ! 0 | 1258 | `	case SXML_ERROR_INVALID_TOKEN:` |
|  ! 0 | 1259 | `		ph7_result_string(pCtx,"Unexpected token",-1);` |
|  ! 0 | 1260 | `		break;` |
|    3 | 1261 | `	case SXML_ERROR_MISPLACED_XML_PI:` |
|    7 | 1262 | `		ph7_result_string(pCtx,"Misplaced processing instruction",-1);` |
|    7 | 1263 | `		break;` |
|  ! 0 | 1264 | `	case SXML_ERROR_NO_MEMORY:` |
|  ! 0 | 1265 | `		ph7_result_string(pCtx,"Out of memory",-1);` |
|  ! 0 | 1266 | `		break;` |
|    1 | 1267 | `	case SXML_ERROR_NONE:` |
|    3 | 1268 | `		ph7_result_string(pCtx,"Not an error",-1);` |
|    3 | 1269 | `		break;` |
|    1 | 1270 | `	case SXML_ERROR_TAG_MISMATCH:` |
|    3 | 1271 | `		ph7_result_string(pCtx,"Tag mismatch",-1);` |
|    3 | 1272 | `		break;` |
|  ! 0 | 1273 | `	case -1:` |
|  ! 0 | 1274 | `		ph7_result_string(pCtx,"Unknown error code",-1);` |
|  ! 0 | 1275 | `		break;` |
|    9 | 1276 | `	default:` |
|   19 | 1277 | `		ph7_result_string(pCtx,"Syntax error",-1);` |
|   18 | 1278 | `		break;` |
|    - | 1279 | `	}` |
|   31 | 1280 | `	return PH7_OK;` |
|    1 | 1281 |  |
|    - | 1282 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1283 | `/*` |
|    - | 1284 | ` * int utf8_encode(string $input)` |
|    - | 1285 | ` *  UTF-8 encoding.` |
|    - | 1286 | ` *  This function encodes the string data to UTF-8, and returns the encoded version.` |
|    - | 1287 | ` *  UTF-8 is a standard mechanism used by Unicode for encoding wide character values` |
|    - | 1288 | ` * into a byte stream. UTF-8 is transparent to plain ASCII characters, is self-synchronized` |
|    - | 1289 | ` * (meaning it is possible for a program to figure out where in the bytestream characters start)` |
|    - | 1290 | ` * and can be used with normal string comparison functions for sorting and such.` |
|    - | 1291 | ` *  Notes on UTF-8 (According to SQLite3 authors):` |
|    - | 1292 | ` *  Byte-0    Byte-1    Byte-2    Byte-3    Value` |
|    - | 1293 | ` *  0xxxxxxx                                 00000000 00000000 0xxxxxxx` |
|    - | 1294 | ` *  110yyyyy  10xxxxxx                       00000000 00000yyy yyxxxxxx` |
|    - | 1295 | ` *  1110zzzz  10yyyyyy  10xxxxxx             00000000 zzzzyyyy yyxxxxxx` |
|    - | 1296 | ` *  11110uuu  10uuzzzz  10yyyyyy  10xxxxxx   000uuuuu zzzzyyyy yyxxxxxx` |
|    - | 1297 | ` * Parameters` |
|    - | 1298 | ` * $input` |
|    - | 1299 | ` *   String to encode or NULL on failure.` |
|    - | 1300 | ` * Return` |
|    - | 1301 | ` *  An UTF-8 encoded string.` |
|    - | 1302 | ` */` |
|  ! 0 | 1303 | `PH7_PRIVATE int vm_builtin_utf8_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 1304 |  |
|    - | 1305 | `	const unsigned char *zIn,*zEnd;` |
|    - | 1306 | `	int nByte,c,e;` |
|  ! 0 | 1307 | `	if( nArg < 1 ){` |
|    - | 1308 | `		/* Missing arguments,return null */` |
|  ! 0 | 1309 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1310 | `		return PH7_OK;` |
|    - | 1311 | `	}` |
|    - | 1312 | `	/* Extract the target string */` |
|  ! 0 | 1313 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|  ! 0 | 1314 | `	if( nByte < 1 ){` |
|    - | 1315 | `		/* Empty string,return null */` |
|  ! 0 | 1316 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1317 | `		return PH7_OK;` |
|    - | 1318 | `	}` |
|  ! 0 | 1319 | `	zEnd = &zIn[nByte];` |
|    - | 1320 | `	/* Start the encoding process */` |
|  ! 0 | 1321 | `	for(;;){` |
|  ! 0 | 1322 | `		if( zIn >= zEnd ){` |
|    - | 1323 | `			/* End of input */` |
|  ! 0 | 1324 | `			break;` |
|    - | 1325 | `		}` |
|  ! 0 | 1326 | `		c = zIn[0];` |
|    - | 1327 | `		/* Advance the stream cursor */` |
|  ! 0 | 1328 | `		zIn++;` |
|    - | 1329 | `		/* Encode */` |
|  ! 0 | 1330 | `		if( c<0x00080 ){` |
|  ! 0 | 1331 | `			e = (c&0xFF);` |
|  ! 0 | 1332 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|  ! 0 | 1333 | `		}else if( c<0x00800 ){` |
|  ! 0 | 1334 | `			e = 0xC0 + ((c>>6)&0x1F);` |
|  ! 0 | 1335 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|  ! 0 | 1336 | `			e = 0x80 + (c & 0x3F);` |
|  ! 0 | 1337 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|  ! 0 | 1338 | `		}else if( c<0x10000 ){` |
|  ! 0 | 1339 | `			e = 0xE0 + ((c>>12)&0x0F);` |
|  ! 0 | 1340 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|  ! 0 | 1341 | `			e = 0x80 + ((c>>6) & 0x3F);` |
|  ! 0 | 1342 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|  ! 0 | 1343 | `			e = 0x80 + (c & 0x3F);` |
|  ! 0 | 1344 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|  ! 0 | 1345 | `		}else{` |
|  ! 0 | 1346 | `			e = 0xF0 + ((c>>18) & 0x07);` |
|  ! 0 | 1347 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|  ! 0 | 1348 | `			e = 0x80 + ((c>>12) & 0x3F);` |
|  ! 0 | 1349 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|  ! 0 | 1350 | `			e = 0x80 + ((c>>6) & 0x3F);` |
|  ! 0 | 1351 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|  ! 0 | 1352 | `			e = 0x80 + (c & 0x3F);` |
|  ! 0 | 1353 | `			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));` |
|    - | 1354 | `		}` |
|  ! 0 | 1355 | `	}` |
|    - | 1356 | `	/* All done */` |
|  ! 0 | 1357 | `	return PH7_OK;` |
|  ! 0 | 1358 |  |
|    - | 1359 | `/* SPDX-SnippetBegin */` |
|    - | 1360 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|    - | 1361 | `/* SPDX-License-Identifier: blessing */` |
|    - | 1362 | `/*` |
|    - | 1363 | ` * UTF-8 decoding routine extracted from the sqlite3 source tree.` |
|    - | 1364 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|    - | 1365 | ` * Status: Public Domain` |
|    - | 1366 | ` */` |
|    - | 1367 | `/*` |
|    - | 1368 | `** This lookup table is used to help decode the first byte of` |
|    - | 1369 | `** a multi-byte UTF8 character.` |
|    - | 1370 | `*/` |
|    - | 1371 | `static const unsigned char UtfTrans1[] = {` |
|    - | 1372 |  |
|    - | 1373 |  |
|    - | 1374 |  |
|    - | 1375 |  |
|    - | 1376 |  |
|    - | 1377 |  |
|    - | 1378 |  |
|    - | 1379 |  |
|    - | 1380 | `};` |
|    - | 1381 | `/*` |
|    - | 1382 | `** Translate a single UTF-8 character.  Return the unicode value.` |
|    - | 1383 | `**` |
|    - | 1384 | `** During translation, assume that the byte that zTerm points` |
|    - | 1385 | `** is a 0x00.` |
|    - | 1386 | `**` |
|    - | 1387 | `** Write a pointer to the next unread byte back into *pzNext.` |
|    - | 1388 | `**` |
|    - | 1389 | `** Notes On Invalid UTF-8:` |
|    - | 1390 | `**` |
|    - | 1391 | `**  *  This routine never allows a 7-bit character (0x00 through 0x7f) to` |
|    - | 1392 | `**     be encoded as a multi-byte character.  Any multi-byte character that` |
|    - | 1393 | `**     attempts to encode a value between 0x00 and 0x7f is rendered as 0xfffd.` |
|    - | 1394 | `**` |
|    - | 1395 | `**  *  This routine never allows a UTF16 surrogate value to be encoded.` |
|    - | 1396 | `**     If a multi-byte character attempts to encode a value between` |
|    - | 1397 | `**     0xd800 and 0xe000 then it is rendered as 0xfffd.` |
|    - | 1398 | `**` |
|    - | 1399 | `**  *  Bytes in the range of 0x80 through 0xbf which occur as the first` |
|    - | 1400 | `**     byte of a character are interpreted as single-byte characters` |
|    - | 1401 | `**     and rendered as themselves even though they are technically` |
|    - | 1402 | `**     invalid characters.` |
|    - | 1403 | `**` |
|    - | 1404 | `**  *  This routine accepts an infinite number of different UTF8 encodings` |
|    - | 1405 | `**     for unicode values 0x80 and greater.  It do not change over-length` |
|    - | 1406 | `**     encodings to 0xfffd as some systems recommend.` |
|    - | 1407 | `*/` |
|    - | 1408 | `#define READ_UTF8(zIn, zTerm, c)                           \` |
|    - | 1409 | `  c = *(zIn++);                                            \` |
|    - | 1410 | `  if( c>=0xc0 ){                                           \` |
|    - | 1411 | `    c = UtfTrans1[c-0xc0];                                 \` |
|    - | 1412 | `    while( zIn!=zTerm && (*zIn & 0xc0)==0x80 ){            \` |
|    - | 1413 | `      c = (c<<6) + (0x3f & *(zIn++));                      \` |
|    - | 1414 | `    }                                                      \` |
|    - | 1415 | `    if( c<0x80                                             \` |
|    - | 1416 | `        \|\| (c&0xFFFFF800)==0xD800                          \` |
|    - | 1417 | `        \|\| (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }        \` |
|    - | 1418 | `  }` |
|  148 | 1419 | `PH7_PRIVATE int PH7_Utf8Read(` |
|    - | 1420 | `  const unsigned char *z,         /* First byte of UTF-8 character */` |
|    - | 1421 | `  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */` |
|    - | 1422 | `  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */` |
|    1 | 1423 | `){` |
|    - | 1424 | `  int c;` |
|  149 | 1425 | `  READ_UTF8(z, zTerm, c);` |
|  149 | 1426 | `  *pzNext = z;` |
|  149 | 1427 | `  return c;` |
|    1 | 1428 |  |
|    - | 1429 | `/* SPDX-SnippetEnd */` |
|    - | 1430 | `/*` |
|    - | 1431 | ` * string utf8_decode(string $data)` |
|    - | 1432 | ` *  This function decodes data, assumed to be UTF-8 encoded, to unicode.` |
|    - | 1433 | ` * Parameters` |
|    - | 1434 | ` * data` |
|    - | 1435 | ` *  An UTF-8 encoded string.` |
|    - | 1436 | ` * Return` |
|    - | 1437 | ` *  Unicode decoded string or NULL on failure.` |
|    - | 1438 | ` */` |
|  ! 0 | 1439 | `PH7_PRIVATE int vm_builtin_utf8_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 1440 |  |
|    - | 1441 | `	const unsigned char *zIn,*zEnd;` |
|    - | 1442 | `	int nByte,c;` |
|  ! 0 | 1443 | `	if( nArg < 1 ){` |
|    - | 1444 | `		/* Missing arguments,return null */` |
|  ! 0 | 1445 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1446 | `		return PH7_OK;` |
|    - | 1447 | `	}` |
|    - | 1448 | `	/* Extract the target string */` |
|  ! 0 | 1449 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|  ! 0 | 1450 | `	if( nByte < 1 ){` |
|    - | 1451 | `		/* Empty string,return null */` |
|  ! 0 | 1452 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1453 | `		return PH7_OK;` |
|    - | 1454 | `	}` |
|  ! 0 | 1455 | `	zEnd = &zIn[nByte];` |
|    - | 1456 | `	/* Start the decoding process */` |
|  ! 0 | 1457 | `	while( zIn < zEnd ){` |
|  ! 0 | 1458 | `		c = PH7_Utf8Read(zIn,zEnd,&zIn);` |
|  ! 0 | 1459 | `		if( c == 0x0 ){` |
|  ! 0 | 1460 | `			break;` |
|    - | 1461 | `		}` |
|  ! 0 | 1462 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|  ! 0 | 1463 | `	}` |
|  ! 0 | 1464 | `	return PH7_OK;` |
|  ! 0 | 1465 |  |
|    - | 1466 |  |
