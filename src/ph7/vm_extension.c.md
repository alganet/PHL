# src/ph7/vm_extension.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 230/245 lines (93.88%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    4 | ` */` |
|        - |    5 | `#include "ph7int.h"` |
|        - |    6 | `/*` |
|        - |    7 | ` * Section:` |
|        - |    8 | ` *    The extension partition: which extension every internal name belongs to,` |
|        - |    9 | ` *    which extensions this build reports as loaded, and the order each one` |
|        - |   10 | ` *    lists its own names in. extension_loaded(), get_loaded_extensions() and` |
|        - |   11 | ` *    get_extension_funcs() live here, and Reflection's four extension surfaces` |
|        - |   12 | ` *    read the same tables.` |
|        - |   13 | ` * Status:` |
|        - |   14 | ` *    Stable.` |
|        - |   15 | ` */` |
|        - |   16 | `#include "vm_extension_names.h"` |
|        - |   17 | `/*` |
|        - |   18 | ` * Is this extension part of THIS build? The four XML names, the two PDO ones,` |
|        - |   19 | ` * curl, zlib and openssl are compile-time options, and posix and pcntl are PLATFORM` |
|        - |   20 | `` * ones -- php builds neither for Windows, so `extension_loaded('posix')` and`` |
|        - |   21 | `` * `extension_loaded('pcntl')` are false there under php and under this.`` |
|        - |   22 | ` * Everything else is always here.` |
|        - |   23 | ` */` |
|     2672 |   24 | `static int VmExtAvailable(int iExt)` |
|        5 |   25 | `{` |
|        - |   26 | `#ifndef PH7_ENABLE_LIBXML` |
|        - |   27 | `	if( iExt == PH7_EXT_LIBXML \|\| iExt == PH7_EXT_XML` |
|        - |   28 | `	 \|\| iExt == PH7_EXT_DOM \|\| iExt == PH7_EXT_XMLWRITER` |
|        - |   29 | `	 \|\| iExt == PH7_EXT_SIMPLEXML ){` |
|        - |   30 | `		return 0;` |
|        - |   31 | `	}` |
|        - |   32 | `#endif` |
|        - |   33 | `#ifndef PH7_ENABLE_SQLITE` |
|        - |   34 | `	if( iExt == PH7_EXT_PDO \|\| iExt == PH7_EXT_PDO_SQLITE \|\| iExt == PH7_EXT_SQLITE3 ){` |
|        - |   35 | `		return 0;` |
|        - |   36 | `	}` |
|        - |   37 | `#endif` |
|        - |   38 | `#ifndef PH7_ENABLE_CURL` |
|        - |   39 | `	if( iExt == PH7_EXT_CURL ){` |
|        - |   40 | `		return 0;` |
|        - |   41 | `	}` |
|        - |   42 | `#endif` |
|        - |   43 | `#ifndef PH7_ENABLE_ZLIB` |
|        - |   44 | `	/* ext/zip rides the same guard: php's own requires zlib, and so does the` |
|        - |   45 | `	 * derivation here. */` |
|        - |   46 | `	if( iExt == PH7_EXT_ZLIB \|\| iExt == PH7_EXT_ZIP ){` |
|        - |   47 | `		return 0;` |
|        - |   48 | `	}` |
|        - |   49 | `#endif` |
|        - |   50 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|        - |   51 | `	if( iExt == PH7_EXT_ZIP \|\| iExt == PH7_EXT_PCNTL \|\| iExt == PH7_EXT_SOCKETS ){` |
|        - |   52 | `		return 0;` |
|        - |   53 | `	}` |
|        - |   54 | `#endif` |
|        - |   55 | `#ifndef PH7_ENABLE_NET` |
|        - |   56 | `	/* ext/sockets IS the descriptor layer net.c compiles to nothing without. */` |
|        - |   57 | `	if( iExt == PH7_EXT_SOCKETS ){` |
|        - |   58 | `		return 0;` |
|        - |   59 | `	}` |
|        - |   60 | `#endif` |
|        - |   61 | `#ifndef PH7_ENABLE_OPENSSL` |
|        - |   62 | `	if( iExt == PH7_EXT_OPENSSL ){` |
|        - |   63 | `		return 0;` |
|        - |   64 | `	}` |
|        - |   65 | `#endif` |
|        - |   66 | `#ifdef __WINNT__` |
|        5 |   67 | `	if( iExt == PH7_EXT_POSIX \|\| iExt == PH7_EXT_PCNTL ){` |
|        5 |   68 | `		return 0;` |
|        - |   69 | `	}` |
|        - |   70 | `#endif` |
|     1329 |   71 | `	SXUNUSED(iExt);   /* every option on: nothing above reads it */` |
|     2677 |   72 | `	return 1;` |
|        5 |   73 | `}` |
|        - |   74 | `/*` |
|        - |   75 | ` * The extension an INTERNAL name belongs to. A function or a class name folds;` |
|        - |   76 | ` * a constant and an ini directive do not, which is the same case rule the` |
|        - |   77 | ` * engine's own lookups use for each. A name the table has no row for is the` |
|        - |   78 | ` * engine's own -- Core -- which is what every one of these answered before the` |
|        - |   79 | ` * partition existed, so a caller never has to spell that fallback itself.` |
|        - |   80 | ` */` |
|     7885 |   81 | `static int VmExtOfName(const VmExtName *aRow,sxu32 nRow,const char *zName,int nName,int bFold)` |
|        5 |   82 | `{` |
|        - |   83 | `	sxu32 n;` |
|     7890 |   84 | `	if( nName < 1 ){` |
|      ! 0 |   85 | `		return PH7_EXT_CORE;` |
|        - |   86 | `	}` |
|  1494188 |   87 | `	for( n = 0 ; n < nRow ; ++n ){` |
|  1493684 |   88 | `		const char *z = aRow[n].zName;` |
|  1493684 |   89 | `		if( (int)SyStrlen(z) != nName ){` |
|  1446392 |   90 | `			continue;` |
|        - |   91 | `		}` |
|    52563 |   92 | `		if( bFold ? (SyStrnicmp(z,zName,(sxu32)nName) == 0)` |
|    10532 |   93 | `		          : (SyMemcmp(z,zName,(sxu32)nName) == 0) ){` |
|     7386 |   94 | `			return (int)aRow[n].iExt;` |
|        - |   95 | `		}` |
|    19954 |   96 | `	}` |
|      509 |   97 | `	return PH7_EXT_CORE;` |
|     3947 |   98 | `}` |
|      547 |   99 | `PH7_PRIVATE int PH7_VmExtOfFunc(const char *zName,int nName)` |
|        4 |  100 | `{` |
|      551 |  101 | `	return VmExtOfName(aExtFunc,SX_ARRAYSIZE(aExtFunc),zName,nName,1);` |
|        4 |  102 | `}` |
|     4252 |  103 | `PH7_PRIVATE int PH7_VmExtOfClass(const char *zName,int nName)` |
|        4 |  104 | `{` |
|     4256 |  105 | `	return VmExtOfName(aExtClass,SX_ARRAYSIZE(aExtClass),zName,nName,1);` |
|        4 |  106 | `}` |
|      458 |  107 | `PH7_PRIVATE int PH7_VmExtOfConstant(const char *zName,int nName)` |
|        5 |  108 | `{` |
|      463 |  109 | `	return VmExtOfName(aExtConst,SX_ARRAYSIZE(aExtConst),zName,nName,0);` |
|        5 |  110 | `}` |
|     2628 |  111 | `PH7_PRIVATE int PH7_VmExtOfIni(const char *zName,int nName)` |
|        3 |  112 | `{` |
|     2631 |  113 | `	return VmExtOfName(aExtIni,SX_ARRAYSIZE(aExtIni),zName,nName,0);` |
|        3 |  114 | `}` |
|        - |  115 | `/*` |
|        - |  116 | ` * Does the partition carry a ROW for this name? A caller that walks the engine's` |
|        - |  117 | ` * own table rather than this one needs to tell "belongs to Core" from "the` |
|        - |  118 | ` * partition has never heard of it", so that a name added to the engine after the` |
|        - |  119 | ` * table was derived is still reported somewhere instead of silently vanishing.` |
|        - |  120 | ` */` |
|    23214 |  121 | `PH7_PRIVATE int PH7_VmExtHasName(int iKind,const char *zName,int nName)` |
|        3 |  122 | `{` |
|        - |  123 | `	const VmExtName *aRow;` |
|        - |  124 | `	sxu32 n,nRow;` |
|        - |  125 | `	int bFold;` |
|    23217 |  126 | `	switch( iKind ){` |
|      ! 0 |  127 | `		case PH7_EXT_KIND_CLASS: aRow = aExtClass; nRow = SX_ARRAYSIZE(aExtClass); bFold = 1; break;` |
|    23217 |  128 | `		case PH7_EXT_KIND_CONST: aRow = aExtConst; nRow = SX_ARRAYSIZE(aExtConst); bFold = 0; break;` |
|      ! 0 |  129 | `		case PH7_EXT_KIND_INI:   aRow = aExtIni;   nRow = SX_ARRAYSIZE(aExtIni);   bFold = 0; break;` |
|      ! 0 |  130 | `		default:                 aRow = aExtFunc;  nRow = SX_ARRAYSIZE(aExtFunc);  bFold = 1; break;` |
|        - |  131 | `	}` |
|    23217 |  132 | `	if( nName < 1 ){` |
|      ! 0 |  133 | `		return 0;` |
|        - |  134 | `	}` |
| 22626237 |  135 | `	for( n = 0 ; n < nRow ; ++n ){` |
| 22603701 |  136 | `		const char *z = aRow[n].zName;` |
| 22603701 |  137 | `		if( (int)SyStrlen(z) != nName ){` |
| 21593361 |  138 | `			continue;` |
|        - |  139 | `		}` |
|  1544409 |  140 | `		if( bFold ? (SyStrnicmp(z,zName,(sxu32)nName) == 0)` |
|  1010340 |  141 | `		          : (SyMemcmp(z,zName,(sxu32)nName) == 0) ){` |
|    22209 |  142 | `			return 1;` |
|        - |  143 | `		}` |
|   487041 |  144 | `	}` |
|    22539 |  145 | `	return 0;` |
|    32799 |  146 | `}` |
|        - |  147 | `/*` |
|        - |  148 | ` * The number of extensions this build reports, so a caller can walk them in the` |
|        - |  149 | ` * order get_loaded_extensions() lists them.` |
|        - |  150 | ` */` |
|      990 |  151 | `PH7_PRIVATE int PH7_VmExtensionCount(void)` |
|        4 |  152 | `{` |
|      994 |  153 | `	return (int)SX_ARRAYSIZE(azExtName);` |
|        4 |  154 | `}` |
|     1382 |  155 | `PH7_PRIVATE int PH7_VmExtensionAvailable(int iExt)` |
|        4 |  156 | `{` |
|     1386 |  157 | `	return iExt >= 0 && iExt < (int)SX_ARRAYSIZE(azExtName) && VmExtAvailable(iExt);` |
|        4 |  158 | `}` |
|        - |  159 | `/*` |
|        - |  160 | ` * Walk the names ONE extension carries, in php's own registration order --` |
|        - |  161 | `` * which is the order `get_extension_funcs()` and every ReflectionExtension`` |
|        - |  162 | ` * listing answer in, and is not alphabetical. xVisit stops the walk when it` |
|        - |  163 | ` * answers non-zero, and that answer is this function's.` |
|        - |  164 | ` *` |
|        - |  165 | ` * The table describes a FULL build, so nothing here asserts a name exists:` |
|        - |  166 | ` * every caller filters against the live VM, which is what makes a build` |
|        - |  167 | ` * without libxml, sqlite or curl -- and MODE=tiny, which drops most of the` |
|        - |  168 | ` * library -- answer the shorter list without a second table.` |
|        - |  169 | ` */` |
|      597 |  170 | `PH7_PRIVATE int PH7_VmExtWalk(int iExt,int iKind,int (*xVisit)(const char *,int,void *),void *pData)` |
|        5 |  171 | `{` |
|        - |  172 | `	const VmExtName *aRow;` |
|        - |  173 | `	sxu32 n,nRow;` |
|      602 |  174 | `	switch( iKind ){` |
|       44 |  175 | `		case PH7_EXT_KIND_CLASS: aRow = aExtClass; nRow = SX_ARRAYSIZE(aExtClass); break;` |
|      459 |  176 | `		case PH7_EXT_KIND_CONST: aRow = aExtConst; nRow = SX_ARRAYSIZE(aExtConst); break;` |
|       35 |  177 | `		case PH7_EXT_KIND_INI:   aRow = aExtIni;   nRow = SX_ARRAYSIZE(aExtIni);   break;` |
|       73 |  178 | `		default:                 aRow = aExtFunc;  nRow = SX_ARRAYSIZE(aExtFunc);  break;` |
|        - |  179 | `	}` |
|   963732 |  180 | `	for( n = 0 ; n < nRow ; ++n ){` |
|   963135 |  181 | `		if( (int)aRow[n].iExt == iExt ){` |
|    27267 |  182 | `			int rc = xVisit(aRow[n].zName,(int)SyStrlen(aRow[n].zName),pData);` |
|    27267 |  183 | `			if( rc != 0 ){` |
|      ! 0 |  184 | `				return rc;` |
|        - |  185 | `			}` |
|    13461 |  186 | `		}` |
|   478719 |  187 | `	}` |
|      602 |  188 | `	return 0;` |
|      300 |  189 | `}` |
|        - |  190 | `/*` |
|        - |  191 | ` * The name of an extension id, and the id of a name. php matches an extension` |
|        - |  192 | ` * name case-insensitively everywhere it takes one.` |
|        - |  193 | ` */` |
|     6164 |  194 | `PH7_PRIVATE const char * PH7_VmExtensionName(int iExt)` |
|        4 |  195 | `{` |
|     6168 |  196 | `	if( iExt < 0 \|\| iExt >= (int)SX_ARRAYSIZE(azExtName) ){` |
|      ! 0 |  197 | `		return "Core";` |
|        - |  198 | `	}` |
|     6168 |  199 | `	return azExtName[iExt];` |
|     3084 |  200 | `}` |
|      712 |  201 | `PH7_PRIVATE int PH7_VmExtensionLookup(const char *zName,int nName)` |
|        5 |  202 | `{` |
|        - |  203 | `	sxu32 n;` |
|    12839 |  204 | `	for( n = 0 ; n < SX_ARRAYSIZE(azExtName) ; ++n ){` |
|    12782 |  205 | `		if( nName == (int)SyStrlen(azExtName[n])` |
|     7367 |  206 | `		 && SyStrnicmp(zName,azExtName[n],(sxu32)nName) == 0 ){` |
|      665 |  207 | `			return VmExtAvailable((int)n) ? (int)n : -1;` |
|        - |  208 | `		}` |
|     5927 |  209 | `	}` |
|       55 |  210 | `	return -1;` |
|      354 |  211 | `}` |
|        - |  212 | `/*` |
|        - |  213 | `` * `phl.stub_extensions` is a PHL-only directive: a comma-separated list of`` |
|        - |  214 | ` * extensions PHL does NOT implement but reports as LOADED, so software that` |
|        - |  215 | ` * only GATES on extension_loaded() (PHPUnit's dom/xmlwriter check) runs` |
|        - |  216 | ` * unmodified. It synthesizes nothing -- no class, no function, no constant,` |
|        - |  217 | ` * so a stub name has no id and Reflection never attributes anything to it.` |
|        - |  218 | ` *` |
|        - |  219 | ` * Walk it, handing each trimmed name to xVisit until one answers non-zero.` |
|        - |  220 | ` */` |
|       58 |  221 | `static int VmStubExtWalk(ph7_vm *pVm,int (*xVisit)(const char *,int,void *),void *pData)` |
|        5 |  222 | `{` |
|        - |  223 | `	SyBlob sList;` |
|        - |  224 | `	const char *z;` |
|       63 |  225 | `	int nByte,i = 0,rc = 0;` |
|       63 |  226 | `	SyBlobInit(&sList,&pVm->sAllocator);` |
|       63 |  227 | `	PH7_VmIniGetStr(pVm,"phl.stub_extensions",&sList);` |
|       63 |  228 | `	z = (const char *)SyBlobData(&sList);` |
|       63 |  229 | `	nByte = (int)SyBlobLength(&sList);` |
|       81 |  230 | `	while( rc == 0 && i < nByte ){` |
|        - |  231 | `		int iStart,iEnd;` |
|       27 |  232 | `		while( i < nByte && z[i] == ',' ){ i++; }` |
|       19 |  233 | `		iStart = i;` |
|      223 |  234 | `		while( i < nByte && z[i] != ',' ){ i++; }` |
|       19 |  235 | `		iEnd = i;` |
|       36 |  236 | `		while( iStart < iEnd && (z[iStart] == ' ' \|\| z[iStart] == '\t') ){ iStart++; }` |
|       28 |  237 | `		while( iEnd > iStart && (z[iEnd-1] == ' ' \|\| z[iEnd-1] == '\t') ){ iEnd--; }` |
|       19 |  238 | `		if( iEnd > iStart ){` |
|       19 |  239 | `			rc = xVisit(&z[iStart],iEnd - iStart,pData);` |
|        9 |  240 | `		}` |
|        1 |  241 | `	}` |
|       63 |  242 | `	SyBlobRelease(&sList);` |
|       63 |  243 | `	return rc;` |
|        5 |  244 | `}` |
|        - |  245 | `typedef struct vm_ext_match vm_ext_match;` |
|        - |  246 | `struct vm_ext_match {` |
|        - |  247 | `	const char *zName;` |
|        - |  248 | `	int nName;` |
|        - |  249 | `};` |
|       14 |  250 | `static int VmStubExtMatch(const char *zName,int nName,void *pData)` |
|        1 |  251 | `{` |
|       15 |  252 | `	vm_ext_match *p = (vm_ext_match *)pData;` |
|       15 |  253 | `	return nName == p->nName && SyStrnicmp(zName,p->zName,(sxu32)nName) == 0;` |
|        1 |  254 | `}` |
|        - |  255 | `/*` |
|        - |  256 | ` * Is this name one of the extensions this engine reports as loaded? Shared by` |
|        - |  257 | ` * extension_loaded() and phpversion(), which php answers from the same list.` |
|        - |  258 | ` */` |
|      490 |  259 | `PH7_PRIVATE int PH7_VmExtensionIsLoaded(ph7_vm *pVm,const char *zName,int nName)` |
|        5 |  260 | `{` |
|        - |  261 | `	vm_ext_match sMatch;` |
|      495 |  262 | `	if( PH7_VmExtensionLookup(zName,nName) >= 0 ){` |
|      455 |  263 | `		return 1;` |
|        - |  264 | `	}` |
|       45 |  265 | `	sMatch.zName = zName;` |
|       45 |  266 | `	sMatch.nName = nName;` |
|       45 |  267 | `	return VmStubExtWalk(pVm,VmStubExtMatch,&sMatch);` |
|      248 |  268 | `}` |
|        - |  269 | `/*` |
|        - |  270 | ` * bool extension_loaded(string $extension)` |
|        - |  271 | ` *  php matches the name case-insensitively.` |
|        - |  272 | ` */` |
|      244 |  273 | `PH7_PRIVATE int vm_builtin_extension_loaded(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  274 | `{` |
|        - |  275 | `	const char *zName;` |
|        - |  276 | `	int nName;` |
|      249 |  277 | `	if( nArg < 1 ){` |
|      ! 0 |  278 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  279 | `		return PH7_OK;` |
|        - |  280 | `	}` |
|      249 |  281 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|      249 |  282 | `	ph7_result_bool(pCtx,PH7_VmExtensionIsLoaded(pCtx->pVm,zName,nName));` |
|      249 |  283 | `	return PH7_OK;` |
|      125 |  284 | `}` |
|        4 |  285 | `static int VmStubExtCollect(const char *zName,int nName,void *pData)` |
|        1 |  286 | `{` |
|        5 |  287 | `	ph7_context *pCtx = (ph7_context *)((void **)pData)[0];` |
|        5 |  288 | `	ph7_value *pArray = (ph7_value *)((void **)pData)[1];` |
|        5 |  289 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|        5 |  290 | `	if( pVal ){` |
|        5 |  291 | `		ph7_value_string(pVal,zName,nName);` |
|        5 |  292 | `		ph7_array_add_elem(pArray,0,pVal);` |
|        5 |  293 | `		ph7_context_release_value(pCtx,pVal);` |
|        2 |  294 | `	}` |
|        5 |  295 | `	return 0;` |
|        1 |  296 | `}` |
|        - |  297 | `/*` |
|        - |  298 | ` * array get_loaded_extensions(bool $zend_extensions = false)` |
|        - |  299 | ` *  PHL loads no Zend extension, so the zend list is always empty.` |
|        - |  300 | ` */` |
|       20 |  301 | `PH7_PRIVATE int vm_builtin_get_loaded_extensions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  302 | `{` |
|       24 |  303 | `	ph7_value *pArray = ph7_context_new_array(pCtx);` |
|        - |  304 | `	void *apData[2];` |
|        - |  305 | `	sxu32 n;` |
|       24 |  306 | `	if( pArray == 0 ){` |
|      ! 0 |  307 | `		return PH7_ContextMemoryError(pCtx);` |
|        - |  308 | `	}` |
|       24 |  309 | `	if( nArg > 0 && ph7_value_to_bool(apArg[0]) ){` |
|        3 |  310 | `		ph7_result_value(pCtx,pArray);` |
|        3 |  311 | `		return PH7_OK;` |
|        - |  312 | `	}` |
|      652 |  313 | `	for( n = 0 ; n < SX_ARRAYSIZE(azExtName) ; ++n ){` |
|        - |  314 | `		ph7_value *pVal;` |
|      634 |  315 | `		if( !VmExtAvailable((int)n) ){` |
|        4 |  316 | `			continue;` |
|        - |  317 | `		}` |
|      634 |  318 | `		pVal = ph7_context_new_scalar(pCtx);` |
|      634 |  319 | `		if( pVal ){` |
|      634 |  320 | `			ph7_value_string(pVal,azExtName[n],-1);` |
|      634 |  321 | `			ph7_array_add_elem(pArray,0,pVal);` |
|      634 |  322 | `			ph7_context_release_value(pCtx,pVal);` |
|      315 |  323 | `		}` |
|      319 |  324 | `	}` |
|       22 |  325 | `	apData[0] = pCtx;` |
|       22 |  326 | `	apData[1] = pArray;` |
|       22 |  327 | `	VmStubExtWalk(pCtx->pVm,VmStubExtCollect,apData);` |
|       22 |  328 | `	ph7_result_value(pCtx,pArray);` |
|       22 |  329 | `	return PH7_OK;` |
|       14 |  330 | `}` |
|        - |  331 | `/*` |
|        - |  332 | ` * Does the live VM carry this INTERNAL name? The tables describe a FULL build,` |
|        - |  333 | ` * so every listing filters through here: a build without libxml, sqlite or curl` |
|        - |  334 | ` * -- and MODE=tiny, which drops most of the library -- answers the shorter list` |
|        - |  335 | ` * without a second table. An ini directive is registered by the engine itself` |
|        - |  336 | ` * and always present, so it is the one kind with nothing to ask.` |
|        - |  337 | ` */` |
|    27262 |  338 | `PH7_PRIVATE int PH7_VmInternalNameExists(ph7_vm *pVm,int iKind,const char *zName,int nName)` |
|        5 |  339 | `{` |
|    27267 |  340 | `	if( nName < 1 ){` |
|      ! 0 |  341 | `		return 0;` |
|        - |  342 | `	}` |
|    27267 |  343 | `	switch( iKind ){` |
|      149 |  344 | `		case PH7_EXT_KIND_CLASS:` |
|      297 |  345 | `			return PH7_VmExtractClass(pVm,zName,(sxu32)nName,FALSE,0) != 0;` |
|    12311 |  346 | `		case PH7_EXT_KIND_CONST: {` |
|    24404 |  347 | `			SyHashEntry *pEntry = SyHashGet(&pVm->hConstant,(const void *)zName,(sxu32)nName);` |
|    24404 |  348 | `			return pEntry != 0;` |
|        - |  349 | `		}` |
|      122 |  350 | `		case PH7_EXT_KIND_INI:` |
|      213 |  351 | `			return 1;` |
|     1219 |  352 | `		default:` |
|     2358 |  353 | `			break;` |
|        - |  354 | `	}` |
|        - |  355 | `	/* A builtin is either a C host function or a prelude body the engine marks` |
|        - |  356 | `	 * internal -- the same pair function_exists() consults. */` |
|     3480 |  357 | `	return PH7_VmGetUserFunction(pVm,(const void *)zName,(sxu32)nName,FALSE) != 0` |
|     2358 |  358 | `		\|\| PH7_VmGetHostFunction(pVm,(const void *)zName,(sxu32)nName,FALSE) != 0;` |
|    13466 |  359 | `}` |
|        - |  360 | `/*` |
|        - |  361 | ` * What one extension DECLARES about the others, which is what php's` |
|        - |  362 | `` * `ReflectionExtension::getDependencies()` answers -- a map from the module`` |
|        - |  363 | ` * registry's own (lower-case) key to "Required", "Optional" or "Conflicts".` |
|        - |  364 | ` * php's declarations are reproduced here, restricted to the extensions this` |
|        - |  365 | `` * engine actually has: php's `dom` also names `lexbor` and `domxml` and its`` |
|        - |  366 | `` * `standard` names `uri`, none of which exist in any PHL build, so a program`` |
|        - |  367 | ` * reading the map is told only about names it can go on to check. Every` |
|        - |  368 | ` * dependency left IS satisfied, since this engine is one binary and its` |
|        - |  369 | ` * extensions are all loaded together.` |
|        - |  370 | ` */` |
|        - |  371 | `static const struct VmExtDep {` |
|        - |  372 | `	sxu8 iExt;           /* the extension declaring it */` |
|        - |  373 | `	const char *zOn;     /* the module registry key it names */` |
|        - |  374 | `	const char *zKind;   /* php's own word for the relationship */` |
|        - |  375 | `} aExtDep[] = {` |
|        - |  376 | `	{ PH7_EXT_SPL,        "json",     "Required" },` |
|        - |  377 | `	{ PH7_EXT_STANDARD,   "session",  "Optional" },` |
|        - |  378 | `	{ PH7_EXT_SESSION,    "spl",      "Optional" },` |
|        - |  379 | `	{ PH7_EXT_MBSTRING,   "pcre",     "Required" },` |
|        - |  380 | `	{ PH7_EXT_LIBXML,     "standard", "Required" },` |
|        - |  381 | `	{ PH7_EXT_XML,        "libxml",   "Required" },` |
|        - |  382 | `	{ PH7_EXT_DOM,        "libxml",   "Required" },` |
|        - |  383 | `	{ PH7_EXT_XMLWRITER,  "libxml",   "Required" },` |
|        - |  384 | `	{ PH7_EXT_PDO,        "spl",      "Required" },` |
|        - |  385 | `	{ PH7_EXT_PDO_SQLITE, "pdo",      "Required" },` |
|        - |  386 | `};` |
|       28 |  387 | `PH7_PRIVATE int PH7_VmExtWalkDep(int iExt,int (*xVisit)(const char *,const char *,void *),void *pData)` |
|        2 |  388 | `{` |
|        - |  389 | `	sxu32 n;` |
|      310 |  390 | `	for( n = 0 ; n < SX_ARRAYSIZE(aExtDep) ; ++n ){` |
|      282 |  391 | `		if( (int)aExtDep[n].iExt == iExt ){` |
|        9 |  392 | `			int rc = xVisit(aExtDep[n].zOn,aExtDep[n].zKind,pData);` |
|        9 |  393 | `			if( rc != 0 ){` |
|      ! 0 |  394 | `				return rc;` |
|        - |  395 | `			}` |
|        4 |  396 | `		}` |
|      142 |  397 | `	}` |
|       30 |  398 | `	return 0;` |
|       16 |  399 | `}` |
|        - |  400 | `/*` |
|        - |  401 | ` * array\|false get_extension_funcs(string $extension)` |
|        - |  402 | ` *  The functions one extension registers, LOWER-cased, in php's own order.` |
|        - |  403 | ` * Return` |
|        - |  404 | ` *  php answers FALSE for a name it does not know AND for one that registers no` |
|        - |  405 | ``  *  function at all -- the same answer for both, so `get_extension_funcs('Reflection')` `` |
|        - |  406 | ``  *  is false on php while the extension is plainly loaded. A `phl.stub_extensions` `` |
|        - |  407 | ` *  name synthesizes nothing, so it is false for the second reason.` |
|        - |  408 | ` */` |
|     1834 |  409 | `static int VmExtFuncCollect(const char *zName,int nName,void *pData)` |
|        4 |  410 | `{` |
|     1838 |  411 | `	ph7_context *pCtx = (ph7_context *)((void **)pData)[0];` |
|     1838 |  412 | `	ph7_value *pArray = (ph7_value *)((void **)pData)[1];` |
|        - |  413 | `	ph7_value *pVal;` |
|     1838 |  414 | `	if( !PH7_VmInternalNameExists(pCtx->pVm,PH7_EXT_KIND_FUNC,zName,nName) ){` |
|        7 |  415 | `		return 0;` |
|        - |  416 | `	}` |
|     1832 |  417 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     1832 |  418 | `	if( pVal ){` |
|        - |  419 | `		/* php hands back the name the way the engine STORES it, which is folded. */` |
|        - |  420 | `		SyBlob sLower;` |
|        - |  421 | `		char *zBuf;` |
|        - |  422 | `		int i;` |
|     1832 |  423 | `		SyBlobInit(&sLower,&pCtx->pVm->sAllocator);` |
|     1832 |  424 | `		SyBlobAppend(&sLower,(const void *)zName,(sxu32)nName);` |
|     1832 |  425 | `		zBuf = (char *)SyBlobData(&sLower);` |
|    25406 |  426 | `		for( i = 0 ; i < (int)SyBlobLength(&sLower) ; ++i ){` |
|    23578 |  427 | `			zBuf[i] = (char)SyToLower(zBuf[i]);` |
|    11249 |  428 | `		}` |
|     1832 |  429 | `		ph7_value_string(pVal,zBuf,(int)SyBlobLength(&sLower));` |
|     1832 |  430 | `		SyBlobRelease(&sLower);` |
|     1832 |  431 | `		ph7_array_add_elem(pArray,0,pVal);` |
|     1832 |  432 | `		ph7_context_release_value(pCtx,pVal);` |
|      874 |  433 | `	}` |
|     1832 |  434 | `	return 0;` |
|      881 |  435 | `}` |
|       39 |  436 | `PH7_PRIVATE int vm_builtin_get_extension_funcs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  437 | `{` |
|        - |  438 | `	ph7_value *pArray;` |
|        - |  439 | `	void *apData[2];` |
|        - |  440 | `	const char *zName;` |
|        - |  441 | `	int nName,iExt;` |
|       43 |  442 | `	if( nArg < 1 ){` |
|      ! 0 |  443 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  444 | `		return PH7_OK;` |
|        - |  445 | `	}` |
|       43 |  446 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|       43 |  447 | `	iExt = PH7_VmExtensionLookup(zName,nName);` |
|       43 |  448 | `	if( iExt < 0 ){` |
|        3 |  449 | `		ph7_result_bool(pCtx,0);` |
|        3 |  450 | `		return PH7_OK;` |
|        - |  451 | `	}` |
|       41 |  452 | `	pArray = ph7_context_new_array(pCtx);` |
|       41 |  453 | `	if( pArray == 0 ){` |
|      ! 0 |  454 | `		return PH7_ContextMemoryError(pCtx);` |
|        - |  455 | `	}` |
|       41 |  456 | `	apData[0] = pCtx;` |
|       41 |  457 | `	apData[1] = pArray;` |
|       41 |  458 | `	PH7_VmExtWalk(iExt,PH7_EXT_KIND_FUNC,VmExtFuncCollect,apData);` |
|       41 |  459 | `	if( ph7_array_count(pArray) < 1 ){` |
|        3 |  460 | `		ph7_result_bool(pCtx,0);` |
|        3 |  461 | `		return PH7_OK;` |
|        - |  462 | `	}` |
|       39 |  463 | `	ph7_result_value(pCtx,pArray);` |
|       39 |  464 | `	return PH7_OK;` |
|       22 |  465 | `}` |
|        - |  466 |  |
