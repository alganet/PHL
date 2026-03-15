/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * Allocate and initialize an XML engine.
 */
static ph7_xml_engine * VmCreateXMLEngine(ph7_context *pCtx,int process_ns,int ns_sep)
{
	ph7_xml_engine *pEngine;
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pValue;
	sxu32 n;
	/* Allocate a new instance */
	pEngine = (ph7_xml_engine *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_xml_engine));
	if( pEngine == 0 ){
		/* Out of memory */
		return 0;
	}
	/* Zero the structure */
	SyZero(pEngine,sizeof(ph7_xml_engine));
	/* Initialize fields */
	pEngine->pVm = pVm;
	pEngine->pCtx = 0;
	pEngine->ns_sep = ns_sep;
	SyXMLParserInit(&pEngine->sParser,&pVm->sAllocator,process_ns ? SXML_ENABLE_NAMESPACE : 0);
	SyBlobInit(&pEngine->sErr,&pVm->sAllocator);
	PH7_MemObjInit(pVm,&pEngine->sParserValue);
	for( n = 0 ; n < SX_ARRAYSIZE(pEngine->aCB) ; ++n ){
		pValue = &pEngine->aCB[n];
		/* NULLIFY the array entries,until someone register an event handler */
		PH7_MemObjInit(&(*pVm),pValue);
	}
	ph7_value_resource(&pEngine->sParserValue,pEngine);
	pEngine->iErrCode = SXML_ERROR_NONE;
	/* Finally set the magic number */
	pEngine->nMagic = XML_ENGINE_MAGIC;
	return pEngine;
}
/*
 * Release an XML engine.
 */
static void VmReleaseXMLEngine(ph7_xml_engine *pEngine)
{
	ph7_vm *pVm = pEngine->pVm;
	ph7_value *pValue;
	sxu32 n;
	/* Release fields */
	SyBlobRelease(&pEngine->sErr);
	SyXMLParserRelease(&pEngine->sParser);
	PH7_MemObjRelease(&pEngine->sParserValue);
	for( n = 0 ; n < SX_ARRAYSIZE(pEngine->aCB) ; ++n ){
		pValue = &pEngine->aCB[n];
		PH7_MemObjRelease(pValue);
	}
	pEngine->nMagic = 0x2621;
	/* Finally,release the whole instance */
	SyMemBackendFree(&pVm->sAllocator,pEngine);
}
/*
 * resource xml_parser_create([ string $encoding ])
 *  Create an UTF-8 XML parser.
 * Parameter
 *  $encoding
 *   (Only UTF-8 encoding is used)
 * Return
 *  Returns a resource handle for the new XML parser.
 */
PH7_PRIVATE int vm_builtin_xml_parser_create(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	/* Allocate a new instance */
	pEngine = VmCreateXMLEngine(&(*pCtx),0,':');
	if( pEngine == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		/* Return null */
		ph7_result_null(pCtx);
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		return PH7_OK;
	}
	/* Return the engine as a resource */
	ph7_result_resource(pCtx,pEngine);
	return PH7_OK;
}
/*
 * resource xml_parser_create_ns([ string $encoding[,string $separator = ':']])
 *  Create an UTF-8 XML parser with namespace support.
 * Parameter
 *  $encoding
 *   (Only UTF-8 encoding is supported)
 *  $separtor
 *   Namespace separator (a single character)
 * Return
 *  Returns a resource handle for the new XML parser.
 */
PH7_PRIVATE int vm_builtin_xml_parser_create_ns(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	int ns_sep = ':';
	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){
		const char *zSep = ph7_value_to_string(apArg[1],0);
		if( zSep[0] != 0 ){
			ns_sep = zSep[0];
		}
	}
	/* Allocate a new instance */
	pEngine = VmCreateXMLEngine(&(*pCtx),TRUE,ns_sep);
	if( pEngine == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		/* Return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Return the engine as a resource */
	ph7_result_resource(pCtx,pEngine);
	return PH7_OK;
}
/*
 * bool xml_parser_free(resource $parser)
 *  Release an XML engine.
 * Parameter
 *  $parser
 *   A reference to the XML parser to free.
 * Return
 *  This function returns FALSE if parser does not refer
 *  to a valid parser, or else it frees the parser and returns TRUE.
 */
PH7_PRIVATE int vm_builtin_xml_parser_free(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Safely release the engine */
	VmReleaseXMLEngine(pEngine);
	/* Return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool xml_set_element_handler(resource $parser,callback $start_element_handler,[callback $end_element_handler])
 * Sets the element handler functions for the XML parser. start_element_handler and end_element_handler
 * are strings containing the names of functions.
 * Parameters
 *  $parser
 *   A reference to the XML parser to set up start and end element handler functions.
 *  $start_element_handler
 *    The function named by start_element_handler must accept three parameters:
 *    start_element_handler(resource $parser,string $name,array $attribs)
 *    $parser
 *      The first parameter, parser, is a reference to the XML parser calling the handler.
 *   $name
 *      The second parameter, name, contains the name of the element for which this handler
 *		is called.If case-folding is in effect for this parser, the element name will be in uppercase letters.
 *  $attribs
 *      The third parameter, attribs, contains an associative array with the element's attributes (if any).
 *		The keys of this array are the attribute names, the values are the attribute values.
 *      Attribute names are case-folded on the same criteria as element names.Attribute values are not case-folded.
 *      The original order of the attributes can be retrieved by walking through attribs the normal way, using each().
 *      The first key in the array was the first attribute, and so on.
 *      Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.
 * $end_element_handler
 *     The function named by end_element_handler must accept two parameters:
 *     end_element_handler(resource $parser,string $name)
 *    $parser
 *      The first parameter, parser, is a reference to the XML parser calling the handler.
 *   $name
 *      The second parameter, name, contains the name of the element for which this handler
 *      is called.If case-folding is in effect for this parser, the element name will be in uppercase
 *      letters.
 *      If a handler function is set to an empty string, or FALSE, the handler in question is disabled.
 * Return
 * TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int vm_builtin_xml_set_element_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* Save the start_element_handler callback for later invocation */
		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_START_TAG]);
		if( nArg > 2 ){
			/* Save the end_element_handler callback for later invocation */
			PH7_MemObjStore(apArg[2]/* User callback*/,&pEngine->aCB[PH7_XML_END_TAG]);
		}
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool xml_set_character_data_handler(resource $parser,callback $handler)
 *  Sets the character data handler function for the XML parser parser.
 * Parameters
 * $parser
 *   A reference to the XML parser to set up character data handler function.
 * $handler
 *  handler is a string containing the name of the callback.
 *  The function named by handler must accept two parameters:
 *   handler(resource $parser,string $data)
 *  $parser
 *    The first parameter, parser, is a reference to the XML parser calling the handler.
 *  $data
 *   The second parameter, data, contains the character data as a string.
 *   Character data handler is called for every piece of a text in the XML document.
 *   It can be called multiple times inside each fragment (e.g. for non-ASCII strings).
 *   If a handler function is set to an empty string, or FALSE, the handler in question is disabled.
 *   Note: Instead of a function name, an array containing an object reference and a method name
 *   can also be supplied.
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int vm_builtin_xml_set_character_data_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* Save the user callback for later invocation */
		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_CDATA]);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool xml_set_default_handler(resource $parser,callback $handler)
 *  Set up default handler.
 * Parameters
 * $parser
 *   A reference to the XML parser to set up character data handler function.
 * $handler
 *  handler is a string containing the name of the callback.
 *  The function named by handler must accept two parameters:
 *   handler(resource $parser,string $data)
 *  $parser
 *    The first parameter, parser, is a reference to the XML parser calling the handler.
 *  $data
 *   The second parameter, data, contains the character data.This may be the XML declaration
 *   document type declaration, entities or other data for which no other handler exists.
 *   Note: Instead of a function name, an array containing an object reference and a method name
 *   can also be supplied.
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int vm_builtin_xml_set_default_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* Save the user callback for later invocation */
		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_DEF]);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool xml_set_end_namespace_decl_handler(resource $parser,callback $handler)
 *  Set up end namespace declaration handler.
 * Parameters
 * $parser
 *   A reference to the XML parser to set up character data handler function.
 * $handler
 *  handler is a string containing the name of the callback.
 *  The function named by handler must accept two parameters:
 *   handler(resource $parser,string $prefix)
 *  $parser
 *    The first parameter, parser, is a reference to the XML parser calling the handler.
 *  $prefix
 *   The prefix is a string used to reference the namespace within an XML object.
 *   Note: Instead of a function name, an array containing an object reference and a method name
 *   can also be supplied.
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int vm_builtin_xml_set_end_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* Save the user callback for later invocation */
		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_NS_END]);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool xml_set_start_namespace_decl_handler(resource $parser,callback $handler)
 *  Set up start namespace declaration handler.
 * Parameters
 * $parser
 *   A reference to the XML parser to set up character data handler function.
 * $handler
 *  handler is a string containing the name of the callback.
 *  The function named by handler must accept two parameters:
 *   handler(resource $parser,string $prefix,string $uri)
 *  $parser
 *    The first parameter, parser, is a reference to the XML parser calling the handler.
 *  $prefix
 *   The prefix is a string used to reference the namespace within an XML object.
 *  $uri
 *    Uniform Resource Identifier (URI) of namespace.
 *   Note: Instead of a function name, an array containing an object reference and a method name
 *   can also be supplied.
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int vm_builtin_xml_set_start_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* Save the user callback for later invocation */
		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_NS_START]);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool xml_set_processing_instruction_handler(resource $parser,callback $handler)
 *  Set up processing instruction (PI) handler.
 * Parameters
 * $parser
 *   A reference to the XML parser to set up character data handler function.
 * $handler
 *  handler is a string containing the name of the callback.
 *  The function named by handler must accept three parameters:
 *   handler(resource $parser,string $target,string $data)
 *  $parser
 *    The first parameter, parser, is a reference to the XML parser calling the handler.
 *  $target
 *   The second parameter, target, contains the PI target.
 *  $data
     The third parameter, data, contains the PI data.
 *   Note: Instead of a function name, an array containing an object reference and a method name
 *   can also be supplied.
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int vm_builtin_xml_set_processing_instruction_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* Save the user callback for later invocation */
		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_PI]);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool xml_set_unparsed_entity_decl_handler(resource $parser,callback $handler)
 *  Set up unparsed entity declaration handler.
 * Parameters
 * $parser
 *   A reference to the XML parser to set up character data handler function.
 * $handler
 *  handler is a string containing the name of the callback.
 *  The function named by handler must accept six parameters:
 *  handler(resource $parser,string $entity_name,string $base,string $system_id,string $public_id,string $notation_name)
 *  $parser
 *   The first parameter, parser, is a reference to the XML parser calling the handler.
 *  $entity_name
 *   The name of the entity that is about to be defined.
 *  $base
 *   This is the base for resolving the system identifier (systemId) of the external entity.
 *   Currently this parameter will always be set to an empty string.
 *  $system_id
 *   System identifier for the external entity.
 *  $public_id
 *    Public identifier for the external entity.
 *  $notation_name
 *    Name of the notation of this entity (see xml_set_notation_decl_handler()).
 *   Note: Instead of a function name, an array containing an object reference and a method name
 *   can also be supplied.
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int vm_builtin_xml_set_unparsed_entity_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* Save the user callback for later invocation */
		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_UNPED]);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool xml_set_notation_decl_handler(resource $parser,callback $handler)
 *  Set up notation declaration handler.
 * Parameters
 * $parser
 *   A reference to the XML parser to set up character data handler function.
 * $handler
 *  handler is a string containing the name of the callback.
 *  The function named by handler must accept five parameters:
 *  handler(resource $parser,string $entity_name,string $base,string $system_id,string $public_id)
 *  $parser
 *   The first parameter, parser, is a reference to the XML parser calling the handler.
 *  $entity_name
 *   The name of the entity that is about to be defined.
 *  $base
 *   This is the base for resolving the system identifier (systemId) of the external entity.
 *   Currently this parameter will always be set to an empty string.
 *  $system_id
 *   System identifier for the external entity.
 *  $public_id
 *    Public identifier for the external entity.
 *  Note: Instead of a function name, an array containing an object reference and a method name
 *  can also be supplied.
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int vm_builtin_xml_set_notation_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* Save the user callback for later invocation */
		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_ND]);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool xml_set_external_entity_ref_handler(resource $parser,callback $handler)
 *  Set up external entity reference handler.
 * Parameters
 * $parser
 *   A reference to the XML parser to set up character data handler function.
 * $handler
 *  handler is a string containing the name of the callback.
 *  The function named by handler must accept five parameters:
 *   handler(resource $parser,string $open_entity_names,string $base,string $system_id,string $public_id)
 *  $parser
 *   The first parameter, parser, is a reference to the XML parser calling the handler.
 *  $open_entity_names
 *   The second parameter, open_entity_names, is a space-separated list of the names
 *   of the entities that are open for the parse of this entity (including the name of the referenced entity).
 *  $base
 *   This is the base for resolving the system identifier (system_id) of the external entity.
 *   Currently this parameter will always be set to an empty string.
 *  $system_id
 *   The fourth parameter, system_id, is the system identifier as specified in the entity declaration.
 *  $public_id
 *   The fifth parameter, public_id, is the public identifier as specified in the entity declaration
 *   or an empty string if none was specified; the whitespace in the public identifier will have been
 *   normalized as required by the XML spec.
 * Note: Instead of a function name, an array containing an object reference and a method name
 * can also be supplied.
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int vm_builtin_xml_set_external_entity_ref_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* Save the user callback for later invocation */
		PH7_MemObjStore(apArg[1]/* User callback*/,&pEngine->aCB[PH7_XML_EER]);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * int xml_get_current_line_number(resource $parser)
 *  Gets the current line number for the given XML parser.
 * Parameters
 * $parser
 *   A reference to the XML parser.
 * Return
 *  This function returns FALSE if parser does not refer
 *  to a valid parser, or else it returns which line the parser
 *  is currently at in its data buffer.
 */
PH7_PRIVATE int vm_builtin_xml_get_current_line_number(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Return the line number */
	ph7_result_int(pCtx,(int)pEngine->nLine);
	return PH7_OK;
}
/*
 * int xml_get_current_byte_index(resource $parser)
 *  Gets the current byte index of the given XML parser.
 * Parameters
 * $parser
 *   A reference to the XML parser.
 * Return
 *  This function returns FALSE if parser does not refer to a valid
 *  parser, or else it returns which byte index the parser is currently
 *  at in its data buffer (starting at 0).
 */
PH7_PRIVATE int vm_builtin_xml_get_current_byte_index(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	SyStream *pStream;
	SyToken *pToken;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the current processed token */
	pToken = (SyToken *)SySetPeekCurrentEntry(&pEngine->sParser.sToken);
	if( pToken == 0 ){
		/* Stream not yet processed */
		ph7_result_int(pCtx,0);
		return 0;
	}
	/* Point to the input stream */
	pStream = &pEngine->sParser.sLex.sStream;
	/* Return the byte index */
	ph7_result_int64(pCtx,(ph7_int64)(pToken->sData.zString-(const char *)pStream->zInput));
	return PH7_OK;
}
/*
 * bool xml_set_object(resource $parser,object &$object)
 *  Use XML Parser within an object.
 * NOTE
 *  This function is depreceated and is a no-op.
 * Parameters
 * $parser
 *   A reference to the XML parser.
 * $object
 *  The object where to use the XML parser.
 * Return
 * Always FALSE.
 */
PH7_PRIVATE int vm_builtin_xml_set_object(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 2 || !ph7_value_is_resource(apArg[0]) || !ph7_value_is_object(apArg[1]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/*  Throw a notice and return */
	ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"This function is depreceated and is a no-op."
		"In order to mimic this behaviour,you can supply instead of a function name an array "
		"containing an object reference and a method name."
		);
	/* Return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * int xml_get_current_column_number(resource $parser)
 *  Gets the current column number of the given XML parser.
 * Parameters
 * $parser
 *   A reference to the XML parser.
 * Return
 *  This function returns FALSE if parser does not refer to a valid parser, or else it returns
 *  which column on the current line (as given by xml_get_current_line_number()) the parser
 *  is currently at.
 */
PH7_PRIVATE int vm_builtin_xml_get_current_column_number(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	SyStream *pStream;
	SyToken *pToken;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the current processed token */
	pToken = (SyToken *)SySetPeekCurrentEntry(&pEngine->sParser.sToken);
	if( pToken == 0 ){
		/* Stream not yet processed */
		ph7_result_int(pCtx,0);
		return 0;
	}
	/* Point to the input stream */
	pStream = &pEngine->sParser.sLex.sStream;
	/* Return the byte index */
	ph7_result_int64(pCtx,(ph7_int64)(pToken->sData.zString-(const char *)pStream->zInput)/80);
	return PH7_OK;
}
/*
 * int xml_get_error_code(resource $parser)
 *  Get XML parser error code.
 * Parameters
 * $parser
 *   A reference to the XML parser.
 * Return
 *  This function returns FALSE if parser does not refer to a valid
 *  parser, or else it returns one of the error codes listed in the error
 *  codes section.
 */
PH7_PRIVATE int vm_builtin_xml_get_error_code(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Return the error code if any */
	ph7_result_int(pCtx,pEngine->iErrCode);
	return PH7_OK;
}
/*
 * XML parser event callbacks
 * Each time the unserlying XML parser extract a single token
 * from the input,one of the following callbacks are invoked.
 * IMP-XML-ENGINE-07-07-2012 22:02 FreeBSD [chm@symisc.net]
 */
/*
 * Create a scalar ph7_value holding the value
 * of an XML tag/attribute/CDATA and so on.
 */
static ph7_value * VmXMLValue(ph7_xml_engine *pEngine,SyXMLRawStr *pXML,SyXMLRawStr *pNsUri)
{
	ph7_value *pValue;
	/* Allocate a new scalar variable */
	pValue = ph7_context_new_scalar(pEngine->pCtx);
	if( pValue == 0 ){
		ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		return 0;
	}
	if( pNsUri && pNsUri->nByte > 0 ){
		/* Append namespace URI and the separator */
		ph7_value_string_format(pValue,"%.*s%c",pNsUri->nByte,pNsUri->zString,pEngine->ns_sep);
	}
	/* Copy the tag value */
	ph7_value_string(pValue,pXML->zString,(int)pXML->nByte);
	return pValue;
}
/*
 * Create a 'ph7_value' of type array holding the values
 * of an XML tag attributes.
 */
static ph7_value * VmXMLAttrValue(ph7_xml_engine *pEngine,SyXMLRawStr *aAttr,sxu32 nAttr)
{
	ph7_value *pArray;
	/* Create an empty array */
	pArray = ph7_context_new_array(pEngine->pCtx);
	if( pArray == 0 ){
		ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		return 0;
	}
	if( nAttr > 0 ){
		ph7_value *pKey,*pValue;
		sxu32 n;
		/* Create worker variables */
		pKey = ph7_context_new_scalar(pEngine->pCtx);
		pValue = ph7_context_new_scalar(pEngine->pCtx);
		if( pKey == 0 || pValue == 0 ){
			ph7_context_throw_error(pEngine->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
			return 0;
		}
		/* Copy attributes */
		for( n = 0 ; n < nAttr ; n += 2 ){
			/* Reset string cursors */
			ph7_value_reset_string_cursor(pKey);
			ph7_value_reset_string_cursor(pValue);
			/* Copy attribute name and it's associated value */
			ph7_value_string(pKey,aAttr[n].zString,(int)aAttr[n].nByte); /* Attribute name */
			ph7_value_string(pValue,aAttr[n+1].zString,(int)aAttr[n+1].nByte); /* Attribute value */
			/* Insert in the array */
			ph7_array_add_elem(pArray,pKey,pValue); /* Will make it's own copy */
		}
		/* Release the worker variables */
		ph7_context_release_value(pEngine->pCtx,pKey);
		ph7_context_release_value(pEngine->pCtx,pValue);
	}
	/* Return the freshly created array */
	return pArray;
}
/*
 * Start element handler.
 * The user defined callback must accept three parameters:
 *    start_element_handler(resource $parser,string $name,array $attribs )
 *    $parser
 *      The first parameter, parser, is a reference to the XML parser calling the handler.
 *    $name
 *      The second parameter, name, contains the name of the element for which this handler
 *		is called.If case-folding is in effect for this parser, the element name will be in uppercase letters.
 *    $attribs
 *      The third parameter, attribs, contains an associative array with the element's attributes (if any).
 *		The keys of this array are the attribute names, the values are the attribute values.
 *      Attribute names are case-folded on the same criteria as element names.Attribute values are not case-folded.
 *      The original order of the attributes can be retrieved by walking through attribs the normal way, using each().
 *      The first key in the array was the first attribute, and so on.
 *      Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.
 */
static sxi32 VmXMLStartElementHandler(SyXMLRawStr *pStart,SyXMLRawStr *pNS,sxu32 nAttr,SyXMLRawStr *aAttr,void *pUserData)
{
	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;
	ph7_value *pCallback,*pTag,*pAttr;
	/* Point to the target user defined callback */
	pCallback = &pEngine->aCB[PH7_XML_START_TAG];
	/* Make sure the given callback is callable */
	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){
		/* Not callable,return immediately*/
		return SXRET_OK;
	}
	/* Create a ph7_value holding the tag name */
	pTag = VmXMLValue(pEngine,pStart,pNS);
	/* Create a ph7_value holding the tag attributes */
	pAttr = VmXMLAttrValue(pEngine,aAttr,nAttr);
	if( pTag == 0  || pAttr == 0 ){
		SXUNUSED(pNS); /* cc warning */
		/* Out of mem,return immediately */
		return SXRET_OK;
	}
	/* Invoke the user callback */
	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTag,pAttr,(ph7_value*)0);
	/* Clean-up the mess left behind */
	ph7_context_release_value(pEngine->pCtx,pTag);
	ph7_context_release_value(pEngine->pCtx,pAttr);
	return SXRET_OK;
}
/*
 * End element handler.
 * The user defined callback must accept two parameters:
 *  end_element_handler(resource $parser,string $name)
 *  $parser
 *   The first parameter, parser, is a reference to the XML parser calling the handler.
 *  $name
 *   The second parameter, name, contains the name of the element for which this handler is called.
 *   If case-folding is in effect for this parser, the element name will be in uppercase letters.
 *   Note: Instead of a function name, an array containing an object reference and a method name
 *   can also be supplied.
 */
static sxi32 VmXMLEndElementHandler(SyXMLRawStr *pEnd,SyXMLRawStr *pNS,void *pUserData)
{
	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;
	ph7_value *pCallback,*pTag;
	/* Point to the target user defined callback */
	pCallback = &pEngine->aCB[PH7_XML_END_TAG];
	/* Make sure the given callback is callable */
	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){
		/* Not callable,return immediately*/
		return SXRET_OK;
	}
	/* Create a ph7_value holding the tag name */
	pTag = VmXMLValue(pEngine,pEnd,pNS);
	if( pTag == 0  ){
		SXUNUSED(pNS); /* cc warning */
		/* Out of mem,return immediately */
		return SXRET_OK;
	}
	/* Invoke the user callback */
	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTag,(ph7_value*)0);
	/* Clean-up the mess left behind */
	ph7_context_release_value(pEngine->pCtx,pTag);
	return SXRET_OK;
}
/*
 * Character data handler.
 *  The user defined callback must accept two parameters:
 *  handler(resource $parser,string $data)
 *  $parser
 *    The first parameter, parser, is a reference to the XML parser calling the handler.
 *  $data
 *   The second parameter, data, contains the character data as a string.
 *   Character data handler is called for every piece of a text in the XML document.
 *   It can be called multiple times inside each fragment (e.g. for non-ASCII strings).
 *   If a handler function is set to an empty string, or FALSE, the handler in question is disabled.
 *   Note: Instead of a function name, an array containing an object reference and a method name can also be supplied.
 */
static sxi32 VmXMLTextHandler(SyXMLRawStr *pText,void *pUserData)
{
	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;
	ph7_value *pCallback,*pData;
	/* Point to the target user defined callback */
	pCallback = &pEngine->aCB[PH7_XML_CDATA];
	/* Make sure the given callback is callable */
	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){
		/* Not callable,return immediately*/
		return SXRET_OK;
	}
	/* Create a ph7_value holding the data */
	pData = VmXMLValue(pEngine,&(*pText),0);
	if( pData == 0  ){
		/* Out of mem,return immediately */
		return SXRET_OK;
	}
	/* Invoke the user callback */
	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pData,(ph7_value*)0);
	/* Clean-up the mess left behind */
	ph7_context_release_value(pEngine->pCtx,pData);
	return SXRET_OK;
}
/*
 * Processing instruction (PI) handler.
 * The user defined callback must accept two parameters:
 *   handler(resource $parser,string $target,string $data)
 *  $parser
 *    The first parameter, parser, is a reference to the XML parser calling the handler.
 *  $target
 *   The second parameter, target, contains the PI target.
 *  $data
 *    The third parameter, data, contains the PI data.
 *    Note: Instead of a function name, an array containing an object reference
 *    and a method name can also be supplied.
 */
static sxi32 VmXMLPIHandler(SyXMLRawStr *pTargetStr,SyXMLRawStr *pDataStr,void *pUserData)
{
	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;
	ph7_value *pCallback,*pTarget,*pData;
	/* Point to the target user defined callback */
	pCallback = &pEngine->aCB[PH7_XML_PI];
	/* Make sure the given callback is callable */
	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){
		/* Not callable,return immediately*/
		return SXRET_OK;
	}
	/* Get a ph7_value holding the data */
	pTarget = VmXMLValue(pEngine,&(*pTargetStr),0);
	pData = VmXMLValue(pEngine,&(*pDataStr),0);
	if( pTarget == 0 || pData == 0  ){
		/* Out of mem,return immediately */
		return SXRET_OK;
	}
	/* Invoke the user callback */
	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pTarget,pData,(ph7_value*)0);
	/* Clean-up the mess left behind */
	ph7_context_release_value(pEngine->pCtx,pTarget);
	ph7_context_release_value(pEngine->pCtx,pData);
	return SXRET_OK;
}
/*
 * Namespace declaration handler.
 * The user defined callback must accept two parameters:
 *    handler(resource $parser,string $prefix,string $uri)
 * $parser
 *   The first parameter, parser, is a reference to the XML parser calling the handler.
 * $prefix
 *   The prefix is a string used to reference the namespace within an XML object.
 * $uri
 *   Uniform Resource Identifier (URI) of namespace.
 *   Note: Instead of a function name, an array containing an object reference
 *   and a method name can also be supplied.
 */
static sxi32 VmXMLNSStartHandler(SyXMLRawStr *pUriStr,SyXMLRawStr *pPrefixStr,void *pUserData)
{
	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;
	ph7_value *pCallback,*pUri,*pPrefix;
	/* Point to the target user defined callback */
	pCallback = &pEngine->aCB[PH7_XML_NS_START];
	/* Make sure the given callback is callable */
	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){
		/* Not callable,return immediately*/
		return SXRET_OK;
	}
	/* Get a ph7_value holding the PREFIX/URI */
	pUri = VmXMLValue(pEngine,pUriStr,0);
	pPrefix = VmXMLValue(pEngine,pPrefixStr,0);
	if( pUri == 0 || pPrefix == 0  ){
		/* Out of mem,return immediately */
		return SXRET_OK;
	}
	/* Invoke the user callback */
	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pUri,pPrefix,(ph7_value*)0);
	/* Clean-up the mess left behind */
	ph7_context_release_value(pEngine->pCtx,pUri);
	ph7_context_release_value(pEngine->pCtx,pPrefix);
	return SXRET_OK;
}
/*
 * Namespace end declaration handler.
 * The user defined callback must accept two parameters:
 *    handler(resource $parser,string $prefix)
 * $parser
 *   The first parameter, parser, is a reference to the XML parser calling the handler.
 * $prefix
 *  The prefix is a string used to reference the namespace within an XML object.
 *   Note: Instead of a function name, an array containing an object reference
 *   and a method name can also be supplied.
 */
static sxi32 VmXMLNSEndHandler(SyXMLRawStr *pPrefixStr,void *pUserData)
{
	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;
	ph7_value *pCallback,*pPrefix;
	/* Point to the target user defined callback */
	pCallback = &pEngine->aCB[PH7_XML_NS_END];
	/* Make sure the given callback is callable */
	if( !PH7_VmIsCallable(pEngine->pVm,pCallback,0) ){
		/* Not callable,return immediately*/
		return SXRET_OK;
	}
	/* Get a ph7_value holding the prefix */
	pPrefix = VmXMLValue(pEngine,pPrefixStr,0);
	if( pPrefix == 0 ){
		/* Out of mem,return immediately */
		return SXRET_OK;
	}
	/* Invoke the user callback */
	PH7_VmCallUserFunctionAp(pEngine->pVm,pCallback,0,&pEngine->sParserValue,pPrefix,(ph7_value*)0);
	/* Clean-up the mess left behind */
	ph7_context_release_value(pEngine->pCtx,pPrefix);
	return SXRET_OK;
}
/*
 * Error Message consumer handler.
 * Each time the XML parser encounter a syntaxt error or any other error
 * related to XML processing,the following callback is invoked by the
 * underlying XML parser.
 */
static sxi32 VmXMLErrorHandler(const char *zMessage,sxi32 iErrCode,SyToken *pToken,void *pUserData)
{
	ph7_xml_engine *pEngine = (ph7_xml_engine *)pUserData;
	/* Save the error code */
	pEngine->iErrCode = iErrCode;
	SXUNUSED(zMessage); /* cc warning */
	if( pToken ){
		pEngine->nLine = pToken->nLine;
	}
	/* Abort XML processing immediately */
	return SXERR_ABORT;
}
/*
 * int xml_parse(resource $parser,string $data[,bool $is_final = false ])
 *  Parses an XML document. The handlers for the configured events are called
 *  as many times as necessary.
 * Parameters
 *  $parser
 *   A reference to the XML parser.
 *  $data
 *   Chunk of data to parse. A document may be parsed piece-wise by calling
 *   xml_parse() several times with new data, as long as the is_final parameter
 *   is set and TRUE when the last data is parsed.
 * $is_final
 *   NOT USED. This implementation require that all the processed input be
 *   entirely loaded in memory.
 * Return
 *  Returns 1 on success or 0 on failure.
 */
PH7_PRIVATE int vm_builtin_xml_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	SyXMLParser *pParser;
	const char *zData;
	int nByte;
	if( nArg < 2 || !ph7_value_is_resource(apArg[0]) || !ph7_value_is_string(apArg[1]) ){
		/* Missing/Ivalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( pEngine->iNest > 0 ){
		/* This can happen when the user callback call xml_parse() again
		 * in it's body which is forbidden.
		 */
		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,
			"Recursive call to %s,PH7 is returning false",
			ph7_function_name(pCtx)
			);
		/* Return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pEngine->pCtx = pCtx;
	/* Point to the underlying XML parser */
	pParser = &pEngine->sParser;
	/* Register elements handler */
	SyXMLParserSetEventHandler(pParser,pEngine,
		VmXMLStartElementHandler,
		VmXMLTextHandler,
		VmXMLErrorHandler,
		0,
		VmXMLEndElementHandler,
		VmXMLPIHandler,
		0,
		0,
		VmXMLNSStartHandler,
		VmXMLNSEndHandler
		);
	pEngine->iErrCode = SXML_ERROR_NONE;
	/* Extract the raw XML input */
	zData = ph7_value_to_string(apArg[1],&nByte);
	/* Start the parse process */
	pEngine->iNest++;
	SyXMLProcess(pParser,zData,(sxu32)nByte);
	pEngine->iNest--;
	/* Return the parse result */
	ph7_result_int(pCtx,pEngine->iErrCode == SXML_ERROR_NONE ? 1 : 0);
	return PH7_OK;
}
/*
 * bool xml_parser_set_option(resource $parser,int $option,mixed $value)
 *  Sets an option in an XML parser.
 * Parameters
 *  $parser
 *   A reference to the XML parser to set an option in.
 *  $option
 *    Which option to set. See below.
 *   The following options are available:
 *   XML_OPTION_CASE_FOLDING 	integer  Controls whether case-folding is enabled for this XML parser.
 *   XML_OPTION_SKIP_TAGSTART 	integer  Specify how many characters should be skipped in the beginning of a tag name.
 *   XML_OPTION_SKIP_WHITE 	    integer  Whether to skip values consisting of whitespace characters.
 *   XML_OPTION_TARGET_ENCODING string 	 Sets which target encoding to use in this XML parser.
 * $value
 *   The option's new value.
 * Return
 *  Returns 1 on success or 0 on failure.
 * Note:
 *  Well,none of these options have meaning under the built-in XML parser so a call to this
 *  function is a no-op.
 */
PH7_PRIVATE int vm_builtin_xml_parser_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	if( nArg < 2 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Always return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * mixed xml_parser_get_option(resource $parser,int $option)
 *  Get options from an XML parser.
 * Parameters
 *  $parser
 *   A reference to the XML parser to set an option in.
 * $option
 *   Which option to fetch.
 * Return
 *  This function returns FALSE if parser does not refer to a valid parser
 *  or if option isn't valid.Else the option's value is returned.
 */
PH7_PRIVATE int vm_builtin_xml_parser_get_option(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_xml_engine *pEngine;
	int nOp;
	if( nArg < 2 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Ivalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the XML engine */
	pEngine = (ph7_xml_engine *)ph7_value_to_resource(apArg[0]);
	if( IS_INVALID_XML_ENGINE(pEngine) ){
		/* Corrupt engine,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the option */
	nOp = ph7_value_to_int(apArg[1]);
	switch(nOp){
	case SXML_OPTION_SKIP_TAGSTART:
	case SXML_OPTION_SKIP_WHITE:
	case SXML_OPTION_CASE_FOLDING:
		ph7_result_int(pCtx,0); break;
	case SXML_OPTION_TARGET_ENCODING:
		ph7_result_string(pCtx,"UTF-8",(int)sizeof("UTF-8")-1);
		break;
	default:
		/* Unknown option,return FALSE*/
		ph7_result_bool(pCtx,0);
		break;
	}
	return PH7_OK;
}
/*
 * string xml_error_string(int $code)
 *  Gets the XML parser error string associated with the given code.
 * Parameters
 *  $code
 *   An error code from xml_get_error_code().
 * Return
 *  Returns a string with a textual description of the error
 *  code, or FALSE if no description was found.
 */
PH7_PRIVATE int vm_builtin_xml_error_string(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int nErr = -1;
	if( nArg > 0 ){
		nErr = ph7_value_to_int(apArg[0]);
	}
	switch(nErr){
	case SXML_ERROR_DUPLICATE_ATTRIBUTE:
		ph7_result_string(pCtx,"Duplicate attribute",-1/*Compute length automatically*/);
		break;
	case SXML_ERROR_INCORRECT_ENCODING:
		ph7_result_string(pCtx,"Incorrect encoding",-1);
		break;
	case SXML_ERROR_INVALID_TOKEN:
		ph7_result_string(pCtx,"Unexpected token",-1);
		break;
	case SXML_ERROR_MISPLACED_XML_PI:
		ph7_result_string(pCtx,"Misplaced processing instruction",-1);
		break;
	case SXML_ERROR_NO_MEMORY:
		ph7_result_string(pCtx,"Out of memory",-1);
		break;
	case SXML_ERROR_NONE:
		ph7_result_string(pCtx,"Not an error",-1);
		break;
	case SXML_ERROR_TAG_MISMATCH:
		ph7_result_string(pCtx,"Tag mismatch",-1);
		break;
	case -1:
		ph7_result_string(pCtx,"Unknown error code",-1);
		break;
	default:
		ph7_result_string(pCtx,"Syntax error",-1);
		break;
	}
	return PH7_OK;
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
/*
 * int utf8_encode(string $input)
 *  UTF-8 encoding.
 *  This function encodes the string data to UTF-8, and returns the encoded version.
 *  UTF-8 is a standard mechanism used by Unicode for encoding wide character values
 * into a byte stream. UTF-8 is transparent to plain ASCII characters, is self-synchronized
 * (meaning it is possible for a program to figure out where in the bytestream characters start)
 * and can be used with normal string comparison functions for sorting and such.
 *  Notes on UTF-8 (According to SQLite3 authors):
 *  Byte-0    Byte-1    Byte-2    Byte-3    Value
 *  0xxxxxxx                                 00000000 00000000 0xxxxxxx
 *  110yyyyy  10xxxxxx                       00000000 00000yyy yyxxxxxx
 *  1110zzzz  10yyyyyy  10xxxxxx             00000000 zzzzyyyy yyxxxxxx
 *  11110uuu  10uuzzzz  10yyyyyy  10xxxxxx   000uuuuu zzzzyyyy yyxxxxxx
 * Parameters
 * $input
 *   String to encode or NULL on failure.
 * Return
 *  An UTF-8 encoded string.
 */
PH7_PRIVATE int vm_builtin_utf8_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nByte,c,e;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);
	if( nByte < 1 ){
		/* Empty string,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	zEnd = &zIn[nByte];
	/* Start the encoding process */
	for(;;){
		if( zIn >= zEnd ){
			/* End of input */
			break;
		}
		c = zIn[0];
		/* Advance the stream cursor */
		zIn++;
		/* Encode */
		if( c<0x00080 ){
			e = (c&0xFF);
			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));
		}else if( c<0x00800 ){
			e = 0xC0 + ((c>>6)&0x1F);
			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));
			e = 0x80 + (c & 0x3F);
			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));
		}else if( c<0x10000 ){
			e = 0xE0 + ((c>>12)&0x0F);
			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));
			e = 0x80 + ((c>>6) & 0x3F);
			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));
			e = 0x80 + (c & 0x3F);
			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));
		}else{
			e = 0xF0 + ((c>>18) & 0x07);
			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));
			e = 0x80 + ((c>>12) & 0x3F);
			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));
			e = 0x80 + ((c>>6) & 0x3F);
			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));
			e = 0x80 + (c & 0x3F);
			ph7_result_string(pCtx,(const char *)&e,(int)sizeof(char));
		}
	}
	/* All done */
	return PH7_OK;
}
/*
 * UTF-8 decoding routine extracted from the sqlite3 source tree.
 * Original author: D. Richard Hipp (http://www.sqlite.org)
 * Status: Public Domain
 */
/*
** This lookup table is used to help decode the first byte of
** a multi-byte UTF8 character.
*/
static const unsigned char UtfTrans1[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,
};
/*
** Translate a single UTF-8 character.  Return the unicode value.
**
** During translation, assume that the byte that zTerm points
** is a 0x00.
**
** Write a pointer to the next unread byte back into *pzNext.
**
** Notes On Invalid UTF-8:
**
**  *  This routine never allows a 7-bit character (0x00 through 0x7f) to
**     be encoded as a multi-byte character.  Any multi-byte character that
**     attempts to encode a value between 0x00 and 0x7f is rendered as 0xfffd.
**
**  *  This routine never allows a UTF16 surrogate value to be encoded.
**     If a multi-byte character attempts to encode a value between
**     0xd800 and 0xe000 then it is rendered as 0xfffd.
**
**  *  Bytes in the range of 0x80 through 0xbf which occur as the first
**     byte of a character are interpreted as single-byte characters
**     and rendered as themselves even though they are technically
**     invalid characters.
**
**  *  This routine accepts an infinite number of different UTF8 encodings
**     for unicode values 0x80 and greater.  It do not change over-length
**     encodings to 0xfffd as some systems recommend.
*/
#define READ_UTF8(zIn, zTerm, c)                           \
  c = *(zIn++);                                            \
  if( c>=0xc0 ){                                           \
    c = UtfTrans1[c-0xc0];                                 \
    while( zIn!=zTerm && (*zIn & 0xc0)==0x80 ){            \
      c = (c<<6) + (0x3f & *(zIn++));                      \
    }                                                      \
    if( c<0x80                                             \
        || (c&0xFFFFF800)==0xD800                          \
        || (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }        \
  }
PH7_PRIVATE int PH7_Utf8Read(
  const unsigned char *z,         /* First byte of UTF-8 character */
  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */
  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */
){
  int c;
  READ_UTF8(z, zTerm, c);
  *pzNext = z;
  return c;
}
/*
 * string utf8_decode(string $data)
 *  This function decodes data, assumed to be UTF-8 encoded, to unicode.
 * Parameters
 * data
 *  An UTF-8 encoded string.
 * Return
 *  Unicode decoded string or NULL on failure.
 */
PH7_PRIVATE int vm_builtin_utf8_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nByte,c;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);
	if( nByte < 1 ){
		/* Empty string,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	zEnd = &zIn[nByte];
	/* Start the decoding process */
	while( zIn < zEnd ){
		c = PH7_Utf8Read(zIn,zEnd,&zIn);
		if( c == 0x0 ){
			break;
		}
		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
	}
	return PH7_OK;
}
