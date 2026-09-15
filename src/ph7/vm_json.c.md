# src/ph7/vm_json.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 561/673 lines (83.36%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `#include "ph7int.h"` |
|    - |    7 | `/*` |
|    - |    8 | ` * Section:` |
|    - |    9 | ` *  JSON encoding/decoding routines.` |
|    - |   10 | ` * Status:` |
|    - |   11 | ` *    Devel.` |
|    - |   12 | ` */` |
|    - |   13 | `/* Forward reference */` |
|    - |   14 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|    - |   15 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData);` |
|    - |   16 | `/*` |
|    - |   17 | ` * JSON encoder state is stored in an instance` |
|    - |   18 | ` * of the following structure.` |
|    - |   19 | ` */` |
|    - |   20 | `typedef struct json_private_data json_private_data;` |
|    - |   21 | `struct json_private_data` |
|    - |   22 | `{` |
|    - |   23 | `	ph7_context *pCtx; /* Call context */` |
|    - |   24 | `	int isFirst;       /* True if first encoded entry */` |
|    - |   25 | `	int isObject;      /* True if the current array level is encoded as a JSON object */` |
|    - |   26 | `	int iFlags;        /* JSON encoding flags */` |
|    - |   27 | `	int nRecCount;     /* Recursion count */` |
|    - |   28 | `	int exc;           /* True if a jsonSerialize() callback threw an exception */` |
|    - |   29 | `	int oom;           /* True if a result append ran out of memory (raises a fatal) */` |
|    - |   30 | `	int fail;          /* True if the value is unencodable — json_encode returns` |
|    - |   31 | `	                    * FALSE (or throws under JSON_THROW_ON_ERROR) */` |
|    - |   32 | `	int failRc;        /* json_rc to report for a ->fail (INF_OR_NAN vs` |
|    - |   33 | `	                    * NON_BACKED_ENUM) */` |
|    - |   34 | `};` |
|    - |   35 | `/*` |
|    - |   36 | ` * Emit into the JSON result, flagging OOM on the shared data and bailing out` |
|    - |   37 | ` * of the current encode function (which returns PH7_OK; the top-level` |
|    - |   38 | ` * vm_builtin_json_encode checks ->oom and raises a non-catchable fatal). Used` |
|    - |   39 | ` * for every ph7_result_string/ph7_result_string_format append below.` |
|    - |   40 | ` */` |
|    - |   41 | `#define JSON_EMIT(pD, call) do { if( (call) != SXRET_OK ){ (pD)->oom = 1; return PH7_OK; } } while(0)` |
|    - |   42 | `/*` |
|    - |   43 | ` * Emit a float in php's json shape: PH7_AppendShortestReal (the shared` |
|    - |   44 | ` * serialize/var_export shortest-round-trip formatter, php's` |
|    - |   45 | ` * serialize_precision=-1) with the exponent marker lowercased (json prints` |
|    - |   46 | ` * 1.0e+17 where serialize prints 1.0E+17).` |
|    - |   47 | ` */` |
|   28 |   48 | `static sxi32 VmJsonEmitReal(ph7_context *pCtx,double rVal)` |
|    1 |   49 | `{` |
|    - |   50 | `	SyBlob sNum;` |
|    - |   51 | `	char *z;` |
|    - |   52 | `	sxu32 i,n;` |
|    - |   53 | `	sxi32 rc;` |
|   29 |   54 | `	SyBlobInit(&sNum,&pCtx->pVm->sAllocator);` |
|   29 |   55 | `	PH7_AppendShortestReal(&sNum,rVal);` |
|   29 |   56 | `	z = (char *)SyBlobData(&sNum);` |
|   29 |   57 | `	n = SyBlobLength(&sNum);` |
|   29 |   58 | `	if( z == 0 \|\| n < 1 ){` |
|  ! 0 |   59 | `		SyBlobRelease(&sNum);` |
|  ! 0 |   60 | `		return SXERR_MEM; /* treated as OOM by JSON_EMIT */` |
|    - |   61 | `	}` |
|  243 |   62 | `	for( i = 0 ; i < n ; i++ ){` |
|  215 |   63 | `		if( z[i] == 'E' ){` |
|    7 |   64 | `			z[i] = 'e';` |
|    3 |   65 | `		}` |
|  108 |   66 | `	}` |
|   29 |   67 | `	rc = ph7_result_string(pCtx,(const char *)z,(int)n);` |
|   29 |   68 | `	SyBlobRelease(&sNum);` |
|   29 |   69 | `	return rc;` |
|   15 |   70 | `}` |
|    - |   71 | `/*` |
|    - |   72 | ` * JSON_PRETTY_PRINT helper: emit a newline followed by (depth * 4) spaces, so a` |
|    - |   73 | ` * container's members are laid out one-per-line and indented like php. A no-op` |
|    - |   74 | ` * unless JSON_PRETTY_PRINT is set. Returns SXRET_OK or an OOM status; callers` |
|    - |   75 | ` * wrap it in JSON_EMIT so an allocation failure trips the ->oom rail.` |
|    - |   76 | ` */` |
| 1162 |   77 | `static sxi32 VmJsonPretty(json_private_data *pJson,int depth)` |
|    4 |   78 | `{` |
| 1166 |   79 | `	ph7_context *pCtx = pJson->pCtx;` |
|    - |   80 | `	sxi32 rc;` |
|    - |   81 | `	int i;` |
| 1166 |   82 | `	if( (pJson->iFlags & JSON_PRETTY_PRINT) == 0 ){` |
| 1117 |   83 | `		return SXRET_OK;` |
|    - |   84 | `	}` |
|   49 |   85 | `	rc = ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|  121 |   86 | `	for( i = 0 ; i < depth && rc == SXRET_OK ; ++i ){` |
|   73 |   87 | `		rc = ph7_result_string(pCtx,"    ",(int)sizeof("    ")-1);` |
|   37 |   88 | `	}` |
|   49 |   89 | `	return rc;` |
|  585 |   90 | `}` |
|    - |   91 | `/*` |
|    - |   92 | ` * Returns the JSON representation of a value.In other word perform a JSON encoding operation.` |
|    - |   93 | ` * According to wikipedia` |
|    - |   94 | ` * JSON's basic types are:` |
|    - |   95 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |   96 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |   97 | ` *   Boolean (true or false)` |
|    - |   98 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |   99 | ` *    do not need to be of the same type)` |
|    - |  100 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |  101 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |  102 | ` *     be distinct from each other)` |
|    - |  103 | ` *   null (empty)` |
|    - |  104 | ` * Non-significant white space may be added freely around the "structural characters"` |
|    - |  105 | ` * (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |  106 | ` */` |
| 1178 |  107 | `static sxi32 VmJsonEncode(` |
|    - |  108 | `	ph7_value *pIn,          /* Encode this value */` |
|    - |  109 | `	json_private_data *pData /* Context data */` |
|    4 |  110 | `	){` |
| 1182 |  111 | `		ph7_context *pCtx = pData->pCtx;` |
| 1182 |  112 | `		int iFlags = pData->iFlags;` |
|    - |  113 | `		int nByte;` |
| 1182 |  114 | `		if( ph7_value_is_null(pIn) \|\| ph7_value_is_resource(pIn)){` |
|    - |  115 | `			/* null */` |
|    7 |  116 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
| 1176 |  117 | `		}else if( ph7_value_is_bool(pIn) ){` |
|   15 |  118 | `			int iBool = ph7_value_to_bool(pIn);` |
|    - |  119 | `			int iLen;` |
|    - |  120 | `			/* true/false */` |
|   15 |  121 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|   15 |  122 | `			JSON_EMIT(pData,ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1));` |
| 1166 |  123 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|  591 |  124 | `			if( ph7_value_is_float(pIn) ){` |
|   31 |  125 | `				double rVal = ph7_value_to_double(pIn);` |
|    - |  126 | `				/* php rejects Inf/NaN: json_encode returns FALSE with` |
|    - |  127 | `				 * json_last_error() == JSON_ERROR_INF_OR_NAN (they have no JSON` |
|    - |  128 | `				 * representation), instead of emitting the invalid bare token. */` |
|   31 |  129 | `				if( PH7_IS_NAN(rVal) \|\| PH7_IS_INF(rVal) ){` |
|    7 |  130 | `					pData->fail = 1;` |
|    7 |  131 | `					pData->failRc = JSON_ERROR_INF_OR_NAN;` |
|    7 |  132 | `					return PH7_OK;` |
|    - |  133 | `				}` |
|    - |  134 | `				/* php's json float output follows serialize_precision` |
|    - |  135 | `				 * (shortest round-trip, like serialize/var_export), NOT the` |
|    - |  136 | `				 * echo/cast precision of 14 — with a lowercase exponent` |
|    - |  137 | `				 * marker: 1/3 -> 0.3333333333333333, 1e17 -> 1.0e+17,` |
|    - |  138 | `				 * 1.0 -> 1, -0.0 -> -0. */` |
|   25 |  139 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,rVal));` |
|   13 |  140 | `			}else{` |
|    - |  141 | `				const char *zNum;` |
|    - |  142 | `				/* Get a string representation of the number */` |
|  367 |  143 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|  367 |  144 | `				JSON_EMIT(pData,ph7_result_string(pCtx,zNum,nByte));` |
|    3 |  145 | `			}` |
|  962 |  146 | `		}else if( ph7_value_is_string(pIn) ){` |
|  367 |  147 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|    - |  148 | `				/* Encodes numeric strings as numbers (same float shapes). */` |
|    5 |  149 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|    5 |  150 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,ph7_value_to_double(pIn)));` |
|    3 |  151 | `			}else{` |
|    - |  152 | `				const char *zIn,*zEnd;` |
|    - |  153 | `				int c;` |
|    - |  154 | `				/* Encode the string */` |
|  363 |  155 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|  363 |  156 | `				zEnd = &zIn[nByte];` |
|    - |  157 | `				/* Append the double quote */` |
|  363 |  158 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"\"",(int)sizeof(char)));` |
|  672 |  159 | `				for(;;){` |
| 1353 |  160 | `					if( zIn >= zEnd ){` |
|    - |  161 | `						/* No more input to process */` |
|  363 |  162 | `						break;` |
|    - |  163 | `					}` |
|  993 |  164 | `					c = zIn[0];` |
|    - |  165 | `					/* Advance the stream cursor */` |
|  993 |  166 | `					zIn++;` |
|  993 |  167 | `					if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|    - |  168 | `						/* All < and > are converted to \u003C and \u003E */` |
|  ! 0 |  169 | `						if( c == '<' ){` |
|  ! 0 |  170 | `							JSON_EMIT(pData,ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1));` |
|  ! 0 |  171 | `						}else{` |
|  ! 0 |  172 | `							JSON_EMIT(pData,ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1));` |
|    - |  173 | `						}` |
|  ! 0 |  174 | `						continue;` |
|  993 |  175 | `					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|    - |  176 | `						/* All &s are converted to \u0026.  */` |
|  ! 0 |  177 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1));` |
|  ! 0 |  178 | `						continue;` |
|  993 |  179 | `					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|    - |  180 | `						/* All ' are converted to \u0027.   */` |
|  ! 0 |  181 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1));` |
|  ! 0 |  182 | `						continue;` |
|  993 |  183 | `					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|    - |  184 | `						/* All " are converted to \u0022. */` |
|  ! 0 |  185 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1));` |
|  ! 0 |  186 | `						continue;` |
|    - |  187 | `					}` |
|  993 |  188 | `					if( c == '"' \|\| c == '\\' ){` |
|    - |  189 | `						/* Escape the quote/backslash (php escapes the backslash` |
|    - |  190 | `						 * unconditionally — the old code wrongly tied it to` |
|    - |  191 | `						 * JSON_UNESCAPED_SLASHES, which governs '/' below) */` |
|    3 |  192 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\",(int)sizeof(char)));` |
|  990 |  193 | `					}else if( c == '/' && (iFlags & JSON_UNESCAPED_SLASHES) == 0 ){` |
|    - |  194 | `						/* php escapes forward slashes by default */` |
|    7 |  195 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\",(int)sizeof(char)));` |
|  986 |  196 | `					}else if( (unsigned char)c < 0x20 ){` |
|    - |  197 | `						/* Control characters (band A #4): php emits the short` |
|    - |  198 | `						 * escapes for \b \f \n \r \t and \u00xx for the rest —` |
|    - |  199 | `						 * pre-fix these were emitted RAW (invalid JSON). */` |
|    - |  200 | `						static const char zHex[] = "0123456789abcdef";` |
|    7 |  201 | `						char zEsc[6] = { '\\', 'u', '0', '0', 0, 0 };` |
|    7 |  202 | `						switch(c){` |
|  ! 0 |  203 | `						case '\b': JSON_EMIT(pData,ph7_result_string(pCtx,"\\b",2)); break;` |
|  ! 0 |  204 | `						case '\f': JSON_EMIT(pData,ph7_result_string(pCtx,"\\f",2)); break;` |
|    3 |  205 | `						case '\n': JSON_EMIT(pData,ph7_result_string(pCtx,"\\n",2)); break;` |
|  ! 0 |  206 | `						case '\r': JSON_EMIT(pData,ph7_result_string(pCtx,"\\r",2)); break;` |
|    3 |  207 | `						case '\t': JSON_EMIT(pData,ph7_result_string(pCtx,"\\t",2)); break;` |
|    1 |  208 | `						default:` |
|    3 |  209 | `							zEsc[4] = zHex[(c >> 4) & 0x0F];` |
|    3 |  210 | `							zEsc[5] = zHex[c & 0x0F];` |
|    3 |  211 | `							JSON_EMIT(pData,ph7_result_string(pCtx,zEsc,6));` |
|    2 |  212 | `							break;` |
|    - |  213 | `						}` |
|    7 |  214 | `						continue;` |
|    - |  215 | `					}` |
|    - |  216 | `					/* Append character verbatim */` |
|  987 |  217 | `					JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));` |
|    3 |  218 | `				}` |
|    - |  219 | `				/* Append the double quote */` |
|  363 |  220 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"\"",(int)sizeof(char)));` |
|    3 |  221 | `			}` |
|  586 |  222 | `		}else if( ph7_value_is_array(pIn) ){` |
|    - |  223 | `			/* An array encodes as a JSON array iff it is a "list" [consecutive` |
|    - |  224 | `			 * 0-based int keys]; otherwise [or under JSON_FORCE_OBJECT] as an` |
|    - |  225 | `			 * object with stringified keys (PHP semantics). */` |
|  654 |  226 | `			int isObject = (iFlags & JSON_FORCE_OBJECT)` |
|  326 |  227 | `				\|\| !PH7_HashmapIsList((ph7_hashmap *)pIn->x.pOther);` |
|  329 |  228 | `			int savedObject = pData->isObject; /* restore for sibling entries after recursion */` |
|  329 |  229 | `			int c = isObject ? '{' : '[';` |
|  329 |  230 | `			int d = isObject ? '}' : ']';` |
|    - |  231 | `			/* Encode the array */` |
|  329 |  232 | `			pData->isObject = isObject;` |
|  329 |  233 | `			pData->isFirst = 1;` |
|    - |  234 | `			/* Append the square bracket or curly braces */` |
|  329 |  235 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));` |
|    - |  236 | `			/* Iterate throw array entries */` |
|  329 |  237 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|    - |  238 | `			/* Bail if a nested append ran out of memory before the closer */` |
|  329 |  239 | `			if( pData->oom ){` |
|  ! 0 |  240 | `				return PH7_OK;` |
|    - |  241 | `			}` |
|    - |  242 | `			/* Pretty-print: a non-empty container closes on its own line,` |
|    - |  243 | `			 * indented one level less than its members (isFirst is still 1` |
|    - |  244 | `			 * only when no entry was emitted -> keep "[]"/"{}" tight). */` |
|  329 |  245 | `			if( !pData->isFirst ){` |
|  297 |  246 | `				JSON_EMIT(pData,VmJsonPretty(pData,pData->nRecCount));` |
|  147 |  247 | `			}` |
|    - |  248 | `			/* Append the closing square bracket or curly braces */` |
|  329 |  249 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char)));` |
|  329 |  250 | `			pData->isObject = savedObject;` |
|  240 |  251 | `		}else if( ph7_value_is_object(pIn) ){` |
|   77 |  252 | `			ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   77 |  253 | `			ph7_vm *pVm = pIn->pVm;` |
|   77 |  254 | `			ph7_class_method *pMethod = 0;` |
|    - |  255 | `			/* If the object implements JsonSerializable, encode the value` |
|    - |  256 | `			 * returned by jsonSerialize() instead of its public properties.` |
|    - |  257 | `			 * An enum implementing it explicitly also takes this path (php). */` |
|   74 |  258 | `			if( pVm->pJsonSerializableClass` |
|   77 |  259 | `				&& PH7_VmInstanceOf(pThis->pClass,pVm->pJsonSerializableClass) ){` |
|   17 |  260 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"jsonSerialize",sizeof("jsonSerialize")-1);` |
|    8 |  261 | `			}` |
|   77 |  262 | `			if( pMethod == 0 && (pThis->pClass->iFlags & PH7_CLASS_ENUM) != 0 ){` |
|    - |  263 | `				/* php 8.1: a BACKED enum case encodes as its backing value; a` |
|    - |  264 | `				 * pure enum case has no default serialization — json_encode` |
|    - |  265 | `				 * returns false. */` |
|    9 |  266 | `				ph7_value *pBacking = PH7_EnumCaseBackingValueOf(pThis);` |
|    9 |  267 | `				if( pBacking ){` |
|    7 |  268 | `					pData->nRecCount++;` |
|    7 |  269 | `					VmJsonEncode(pBacking,pData);` |
|    7 |  270 | `					pData->nRecCount--;` |
|    4 |  271 | `				}else{` |
|    3 |  272 | `					pData->fail = 1;` |
|    3 |  273 | `					pData->failRc = JSON_ERROR_NON_BACKED_ENUM;` |
|    - |  274 | `				}` |
|    9 |  275 | `				return PH7_OK;` |
|    - |  276 | `			}` |
|   69 |  277 | `			if( pMethod ){` |
|    - |  278 | `				ph7_value sResult;` |
|    - |  279 | `				sxi32 rc;` |
|   17 |  280 | `				PH7_MemObjInit(pVm,&sResult);` |
|   17 |  281 | `				rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|   17 |  282 | `				if( rc == PH7_EXCEPTION ){` |
|    - |  283 | `					/* Let jsonSerialize()'s throw propagate */` |
|    5 |  284 | `					PH7_MemObjRelease(&sResult);` |
|    5 |  285 | `					pData->exc = 1;` |
|    5 |  286 | `					return PH7_EXCEPTION;` |
|    - |  287 | `				}` |
|    - |  288 | `				/* Encode the returned value [scalar/array/object] */` |
|   13 |  289 | `				pData->nRecCount++;` |
|   13 |  290 | `				VmJsonEncode(&sResult,pData);` |
|   13 |  291 | `				pData->nRecCount--;` |
|   13 |  292 | `				PH7_MemObjRelease(&sResult);` |
|   13 |  293 | `				if( pData->exc ){` |
|  ! 0 |  294 | `					return PH7_EXCEPTION;` |
|    - |  295 | `				}` |
|   13 |  296 | `				if( pData->oom ){` |
|  ! 0 |  297 | `					return PH7_OK;` |
|    - |  298 | `				}` |
|    7 |  299 | `			}else{` |
|    - |  300 | `				SyHashEntry *pAttrEntry;` |
|    - |  301 | `				SySet sNames;` |
|    - |  302 | `				SyString *aName;` |
|    - |  303 | `				sxu32 iName,nName;` |
|    - |  304 | `				/* Encode the class instance: php serializes only PUBLIC` |
|    - |  305 | `				 * non-static properties, reading through a PHP 8.4 get hook` |
|    - |  306 | `				 * when one is declared (virtual properties included). The` |
|    - |  307 | `				 * names are SNAPSHOTTED first — a hook dispatched mid-walk may` |
|    - |  308 | `				 * re-enter an hAttr walk on this instance (the hash has a` |
|    - |  309 | `				 * single embedded loop cursor) or unset()/create properties;` |
|    - |  310 | `				 * names point into class-owned attr storage and each is` |
|    - |  311 | `				 * re-looked-up before use. */` |
|   53 |  312 | `				pData->isFirst = 1;` |
|    - |  313 | `				/* Append the curly braces */` |
|   53 |  314 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"{",(int)sizeof(char)));` |
|   53 |  315 | `				SySetInit(&sNames,&pVm->sAllocator,sizeof(SyString));` |
|   53 |  316 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|  139 |  317 | `				while( (pAttrEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   89 |  318 | `					VmClassAttr *pVmAttr = (VmClassAttr *)pAttrEntry->pUserData;` |
|   86 |  319 | `					if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT))` |
|   89 |  320 | `					 \|\| pVmAttr->pAttr->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|    3 |  321 | `						continue;` |
|    - |  322 | `					}` |
|   84 |  323 | `					if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|   45 |  324 | `					 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|    3 |  325 | `						continue; /* virtual set-only property: no value to encode (php) */` |
|    - |  326 | `					}` |
|   85 |  327 | `					SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|    3 |  328 | `				}` |
|   53 |  329 | `				aName = (SyString *)SySetBasePtr(&sNames);` |
|   53 |  330 | `				nName = SySetUsed(&sNames);` |
|  135 |  331 | `				for( iName = 0 ; iName < nName ; ++iName ){` |
|    - |  332 | `					VmClassAttr *pVmAttr;` |
|   85 |  333 | `					ph7_value *pAttrVal = 0;` |
|    - |  334 | `					ph7_value sHookVal;` |
|    - |  335 | `					sxi32 rcHk;` |
|   85 |  336 | `					pAttrEntry = SyHashGet(&pThis->hAttr,(const void *)aName[iName].zString,aName[iName].nByte);` |
|   85 |  337 | `					if( pAttrEntry == 0 ){` |
|  ! 0 |  338 | `						continue; /* unset by an earlier hook */` |
|    - |  339 | `					}` |
|   85 |  340 | `					pVmAttr = (VmClassAttr *)pAttrEntry->pUserData;` |
|   85 |  341 | `					PH7_MemObjInit(pVm,&sHookVal);` |
|   85 |  342 | `					rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|   85 |  343 | `					if( rcHk == SXRET_OK ){` |
|   11 |  344 | `						pAttrVal = &sHookVal;` |
|   80 |  345 | `					}else if( rcHk == SXERR_NOTFOUND ){` |
|    - |  346 | `						/* Encode a COPY: the encoder casts scalars in place` |
|    - |  347 | `						 * (ph7_value_to_string), which must not corrupt the` |
|    - |  348 | `						 * live attribute slot. */` |
|   75 |  349 | `						ph7_value *pRaw = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   75 |  350 | `						if( pRaw ){` |
|   75 |  351 | `							PH7_MemObjStore(pRaw,&sHookVal);` |
|   75 |  352 | `							pAttrVal = &sHookVal;` |
|   36 |  353 | `						}` |
|   39 |  354 | `					}else{` |
|    - |  355 | `						/* the get hook threw — propagate like jsonSerialize() */` |
|  ! 0 |  356 | `						PH7_MemObjRelease(&sHookVal);` |
|  ! 0 |  357 | `						SySetRelease(&sNames);` |
|  ! 0 |  358 | `						pData->exc = 1;` |
|  ! 0 |  359 | `						return PH7_EXCEPTION;` |
|    - |  360 | `					}` |
|   85 |  361 | `					if( pAttrVal ){` |
|   85 |  362 | `						VmJsonObjectEncode(SyStringData(&pVmAttr->pAttr->sName),pAttrVal,pData);` |
|   41 |  363 | `					}` |
|   85 |  364 | `					PH7_MemObjRelease(&sHookVal);` |
|   85 |  365 | `					if( pData->exc ){` |
|  ! 0 |  366 | `						SySetRelease(&sNames);` |
|  ! 0 |  367 | `						return PH7_EXCEPTION; /* a nested jsonSerialize()/hook threw */` |
|    - |  368 | `					}` |
|   85 |  369 | `					if( pData->oom ){` |
|  ! 0 |  370 | `						SySetRelease(&sNames);` |
|  ! 0 |  371 | `						return PH7_OK;` |
|    - |  372 | `					}` |
|   44 |  373 | `				}` |
|   53 |  374 | `				SySetRelease(&sNames);` |
|    - |  375 | `				/* Pretty-print: non-empty object closes on its own indented line. */` |
|   53 |  376 | `				if( !pData->isFirst ){` |
|   49 |  377 | `					JSON_EMIT(pData,VmJsonPretty(pData,pData->nRecCount));` |
|   23 |  378 | `				}` |
|    - |  379 | `				/* Append the closing curly braces  */` |
|   53 |  380 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"}",(int)sizeof(char)));` |
|    - |  381 | `			}` |
|   34 |  382 | `		}else{` |
|    - |  383 | `			/* Can't happen */` |
|  ! 0 |  384 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|    - |  385 | `		}` |
|    - |  386 | `		/* All done */` |
| 1164 |  387 | `		return PH7_OK;` |
|  593 |  388 | `}` |
|    - |  389 | `/*` |
|    - |  390 | ` * The following walker callback is invoked each time we need` |
|    - |  391 | ` * to encode an array to JSON.` |
|    - |  392 | ` */` |
|  740 |  393 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|    3 |  394 | `{` |
|  743 |  395 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|  743 |  396 | `	if( pJson->nRecCount > 31 \|\| pJson->exc \|\| pJson->oom ){` |
|    - |  397 | `		/* Recursion limit reached, a callback threw, or OOM — return immediately */` |
|  ! 0 |  398 | `		return PH7_OK;` |
|    - |  399 | `	}` |
|  743 |  400 | `	if( !pJson->isFirst ){` |
|    - |  401 | `		/* Append the comma separating this entry from the previous one */` |
|  449 |  402 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|  223 |  403 | `	}` |
|    - |  404 | `	/* Pretty-print: every member starts on its own indented line (one level` |
|    - |  405 | `	 * deeper than the enclosing container). */` |
|  743 |  406 | `	JSON_EMIT(pJson,VmJsonPretty(pJson,pJson->nRecCount + 1));` |
|  743 |  407 | `	if( pJson->isObject ){` |
|    - |  408 | `		/* Outputs an object rather than an array */` |
|    - |  409 | `		const char *zKey;` |
|    - |  410 | `		int nByte;` |
|    - |  411 | `		/* Extract a string representation of the key */` |
|  365 |  412 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|    - |  413 | `		/* Append the quoted key and the colon (checked, so an OOM here is caught` |
|    - |  414 | `		 * rather than silently truncating; matches the prior "%.*s" emit byte for` |
|    - |  415 | `		 * byte — keys are not JSON-escaped, a pre-existing behavior). */` |
|  365 |  416 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\"",(int)sizeof(char)));` |
|  365 |  417 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,zKey,nByte));` |
|  365 |  418 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\":",(int)sizeof("\":")-1));` |
|    - |  419 | `		/* php puts a space after the colon in pretty mode */` |
|  365 |  420 | `		if( pJson->iFlags & JSON_PRETTY_PRINT ){` |
|   19 |  421 | `			JSON_EMIT(pJson,ph7_result_string(pJson->pCtx," ",(int)sizeof(char)));` |
|    9 |  422 | `		}` |
|  181 |  423 | `	}` |
|    - |  424 | `	/* Encode the value */` |
|  743 |  425 | `	pJson->nRecCount++;` |
|  743 |  426 | `	VmJsonEncode(pValue,pJson);` |
|  743 |  427 | `	pJson->nRecCount--;` |
|  743 |  428 | `	pJson->isFirst = 0;` |
|  743 |  429 | `	return PH7_OK;` |
|  373 |  430 | `}` |
|    - |  431 | `/*` |
|    - |  432 | ` * The following walker callback is invoked each time we need to encode` |
|    - |  433 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|    - |  434 | ` */` |
|   82 |  435 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData)` |
|    3 |  436 | `{` |
|   85 |  437 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|   85 |  438 | `	if( pJson->nRecCount > 31 \|\| pJson->exc \|\| pJson->oom ){` |
|    - |  439 | `		/* Recursion limit reached, a callback threw, or OOM — return immediately */` |
|  ! 0 |  440 | `		return PH7_OK;` |
|    - |  441 | `	}` |
|   85 |  442 | `	if( !pJson->isFirst ){` |
|    - |  443 | `		/* Append the comma separating this entry from the previous one */` |
|   38 |  444 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|   18 |  445 | `	}` |
|    - |  446 | `	/* Pretty-print: member on its own indented line, one level deeper. */` |
|   85 |  447 | `	JSON_EMIT(pJson,VmJsonPretty(pJson,pJson->nRecCount + 1));` |
|    - |  448 | `	/* Append the quoted attribute name and the colon (checked; matches the prior` |
|    - |  449 | `	 * "%s" emit byte for byte — attribute names are not JSON-escaped). */` |
|   85 |  450 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\"",(int)sizeof(char)));` |
|   85 |  451 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,zAttr,-1));` |
|   85 |  452 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\":",(int)sizeof("\":")-1));` |
|    - |  453 | `	/* php puts a space after the colon in pretty mode */` |
|   85 |  454 | `	if( pJson->iFlags & JSON_PRETTY_PRINT ){` |
|    9 |  455 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx," ",(int)sizeof(char)));` |
|    4 |  456 | `	}` |
|    - |  457 | `	/* Encode the value */` |
|   85 |  458 | `	pJson->nRecCount++;` |
|   85 |  459 | `	VmJsonEncode(pValue,pJson);` |
|   85 |  460 | `	pJson->nRecCount--;` |
|   85 |  461 | `	pJson->isFirst = 0;` |
|   85 |  462 | `	return PH7_OK;` |
|   44 |  463 | `}` |
|    - |  464 | `/*` |
|    - |  465 | ` * string json_encode(mixed $value [, int $options = 0 ])` |
|    - |  466 | ` *  Returns a string containing the JSON representation of value.` |
|    - |  467 | ` * Parameters` |
|    - |  468 | ` *  $value` |
|    - |  469 | ` *  The value being encoded. Can be any type except a resource.` |
|    - |  470 | ` * $options` |
|    - |  471 | ` *  Bitmask consisting of:` |
|    - |  472 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|    - |  473 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|    - |  474 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|    - |  475 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|    - |  476 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|    - |  477 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|    - |  478 | ` *  JSON_BIGINT_AS_STRING   Not used` |
|    - |  479 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|    - |  480 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|    - |  481 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|    - |  482 | ` * Return` |
|    - |  483 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|    - |  484 | ` */` |
|    - |  485 | `static const char * JsonErrorMsg(int rc); /* defined below, near json_last_error_msg */` |
|  338 |  486 | `PH7_PRIVATE int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 |  487 | `{` |
|    - |  488 | `	json_private_data sJson;` |
|    - |  489 | `	sxi32 rc;` |
|  342 |  490 | `	if( nArg < 1 ){` |
|    - |  491 | `		/* Missing arguments,return FALSE */` |
|  ! 0 |  492 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  493 | `		return PH7_OK;` |
|    - |  494 | `	}` |
|    - |  495 | `	/* Prepare the JSON data */` |
|  342 |  496 | `	sJson.nRecCount = 0;` |
|  342 |  497 | `	sJson.pCtx = pCtx;` |
|  342 |  498 | `	sJson.isFirst = 1;` |
|  342 |  499 | `	sJson.iFlags = 0;` |
|  342 |  500 | `	sJson.exc = 0;` |
|  342 |  501 | `	sJson.oom = 0;` |
|  342 |  502 | `	sJson.fail = 0;` |
|  342 |  503 | `	sJson.failRc = JSON_ERROR_NON_BACKED_ENUM;` |
|  342 |  504 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|    - |  505 | `		/* Extract option flags */` |
|   14 |  506 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|    6 |  507 | `	}` |
|  342 |  508 | `	pCtx->pVm->json_rc = JSON_ERROR_NONE;` |
|    - |  509 | `	/* Perform the encoding operation */` |
|  342 |  510 | `	rc = VmJsonEncode(apArg[0],&sJson);` |
|  342 |  511 | `	if( sJson.oom ){` |
|    - |  512 | `		/* A result append ran out of memory: raise a non-catchable fatal,` |
|    - |  513 | `		 * distinct from a JSON-encoding error (json_last_error untouched). */` |
|  ! 0 |  514 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  515 | `	}` |
|  342 |  516 | `	if( rc == PH7_EXCEPTION \|\| sJson.exc ){` |
|    - |  517 | `		/* A jsonSerialize() callback threw — propagate so the exception unwinds */` |
|    5 |  518 | `		return PH7_EXCEPTION;` |
|    - |  519 | `	}` |
|  338 |  520 | `	if( sJson.fail ){` |
|    - |  521 | `		/* Unencodable value (Inf/NaN, or a php 8.1 non-backed enum case): the` |
|    - |  522 | `		 * whole encode fails — discard whatever was emitted and return FALSE. */` |
|    9 |  523 | `		pCtx->pVm->json_rc = sJson.failRc;` |
|    9 |  524 | `		if( sJson.iFlags & JSON_THROW_ON_ERROR ){` |
|    - |  525 | `			/* php: raise a JsonException carrying json_last_error_msg() instead` |
|    - |  526 | `			 * of returning FALSE. */` |
|    4 |  527 | `			return PH7_VmThrowException(pCtx,"JsonException","%s",` |
|    2 |  528 | `				JsonErrorMsg(pCtx->pVm->json_rc));` |
|    - |  529 | `		}` |
|    7 |  530 | `		ph7_result_bool(pCtx,0);` |
|    7 |  531 | `		return PH7_OK;` |
|    - |  532 | `	}` |
|    - |  533 | `	/* All done */` |
|  330 |  534 | `	return PH7_OK;` |
|  173 |  535 | `}` |
|    - |  536 | `#undef JSON_EMIT` |
|    - |  537 | `/*` |
|    - |  538 | ` * int json_last_error(void)` |
|    - |  539 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|    - |  540 | ` * Parameters` |
|    - |  541 | ` *  None` |
|    - |  542 | ` * Return` |
|    - |  543 | ` *  Returns an integer, the value can be one of the following constants:` |
|    - |  544 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|    - |  545 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|    - |  546 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|    - |  547 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|    - |  548 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|    - |  549 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|    - |  550 | ` */` |
|   26 |  551 | `PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  552 | `{` |
|   28 |  553 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  554 | `	/* Return the error code */` |
|   28 |  555 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|   13 |  556 | `	SXUNUSED(nArg); /* cc warning */` |
|   13 |  557 | `	SXUNUSED(apArg);` |
|   28 |  558 | `	return PH7_OK;` |
|    2 |  559 | `}` |
|    - |  560 | `/*` |
|    - |  561 | ` * string json_last_error_msg(void)` |
|    - |  562 | ` *  Returns the error string of the last JSON encoding/decoding operation.` |
|    - |  563 | ` * Parameters` |
|    - |  564 | ` *  None` |
|    - |  565 | ` * Return` |
|    - |  566 | ` *  Returns the human-readable message corresponding to the last json_last_error()` |
|    - |  567 | ` *  code, or "No error" if no error has occurred.` |
|    - |  568 | ` */` |
|    - |  569 | `/* Human-readable message for a json_rc code. Shared by json_last_error_msg()` |
|    - |  570 | ` * and the JSON_THROW_ON_ERROR path (php's JsonException message is exactly this` |
|    - |  571 | ` * text). */` |
|   10 |  572 | `static const char * JsonErrorMsg(int rc)` |
|    1 |  573 | `{` |
|   11 |  574 | `	switch( rc ){` |
|    3 |  575 | `	case JSON_ERROR_NONE:            return "No error";` |
|  ! 0 |  576 | `	case JSON_ERROR_DEPTH:           return "Maximum stack depth exceeded";` |
|  ! 0 |  577 | `	case JSON_ERROR_STATE_MISMATCH:  return "State mismatch (invalid or malformed JSON)";` |
|  ! 0 |  578 | `	case JSON_ERROR_CTRL_CHAR:       return "Control character error, possibly incorrectly encoded";` |
|    7 |  579 | `	case JSON_ERROR_SYNTAX:          return "Syntax error";` |
|  ! 0 |  580 | `	case JSON_ERROR_UTF8:            return "Malformed UTF-8 characters, possibly incorrectly encoded";` |
|    3 |  581 | `	case JSON_ERROR_INF_OR_NAN:     return "Inf and NaN cannot be JSON encoded";` |
|  ! 0 |  582 | `	case JSON_ERROR_NON_BACKED_ENUM: return "Non-backed enums have no default serialization";` |
|  ! 0 |  583 | `	default:                         return "Unknown error";` |
|    - |  584 | `	}` |
|    6 |  585 | `}` |
|    6 |  586 | `PH7_PRIVATE int vm_builtin_json_last_error_msg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  587 | `{` |
|    7 |  588 | `	ph7_result_string(pCtx,JsonErrorMsg(pCtx->pVm->json_rc),-1/* auto length */);` |
|    3 |  589 | `	SXUNUSED(nArg); /* cc warning */` |
|    3 |  590 | `	SXUNUSED(apArg);` |
|    7 |  591 | `	return PH7_OK;` |
|    1 |  592 | `}` |
|    - |  593 | `/* Possible tokens from the JSON tokenization process */` |
|    - |  594 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|    - |  595 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|    - |  596 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|    - |  597 | `#define JSON_TK_NULL    0x008 /* null */` |
|    - |  598 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|    - |  599 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|    - |  600 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|    - |  601 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|    - |  602 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|    - |  603 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|    - |  604 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|    - |  605 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|    - |  606 | `/*` |
|    - |  607 | ` * Tokenize an entire JSON input.` |
|    - |  608 | ` * Get a single low-level token from the input file.` |
|    - |  609 | ` * Update the stream pointer so that it points to the first` |
|    - |  610 | ` * character beyond the extracted token.` |
|    - |  611 | ` */` |
|  300 |  612 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|    3 |  613 | `{` |
|  303 |  614 | `	int *pJsonErr = (int *)pUserData;` |
|    - |  615 | `	SyString *pStr;` |
|    - |  616 | `	int c;` |
|    - |  617 | `	/* Ignore leading white spaces */` |
|  305 |  618 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|    - |  619 | `		/* Advance the stream cursor */` |
|    3 |  620 | `		if( pStream->zText[0] == '\n' ){` |
|    - |  621 | `			/* Update line counter */` |
|  ! 0 |  622 | `			pStream->nLine++;` |
|  ! 0 |  623 | `		}` |
|    3 |  624 | `		pStream->zText++;` |
|    1 |  625 | `	}` |
|  303 |  626 | `	if( pStream->zText >= pStream->zEnd ){` |
|    - |  627 | `		/* End of input reached */` |
|  ! 0 |  628 | `		SXUNUSED(pCtxData); /* cc warning */` |
|  ! 0 |  629 | `		return SXERR_EOF;` |
|    - |  630 | `	}` |
|    - |  631 | `	/* Record token starting position and line */` |
|  303 |  632 | `	pToken->nLine = pStream->nLine;` |
|  303 |  633 | `	pToken->pUserData = 0;` |
|  303 |  634 | `	pStr = &pToken->sData;` |
|  303 |  635 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  300 |  636 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|  214 |  637 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|    - |  638 | `			/* Single character */` |
|  177 |  639 | `			c = pStream->zText[0];` |
|    - |  640 | `			/* Set token type */` |
|  177 |  641 | `			switch(c){` |
|   23 |  642 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|   39 |  643 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|   27 |  644 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|   21 |  645 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|   43 |  646 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|   39 |  647 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|  ! 0 |  648 | `			default:` |
|  ! 0 |  649 | `				break;` |
|    - |  650 | `			}` |
|    - |  651 | `			/* Advance the stream cursor */` |
|  177 |  652 | `			pStream->zText++;` |
|  216 |  653 | `	}else if( pStream->zText[0] == '"') {` |
|    - |  654 | `		/* JSON string */` |
|   47 |  655 | `		pStream->zText++;` |
|   47 |  656 | `		pStr->zString++;` |
|    - |  657 | `		/* Delimit the string */` |
|  109 |  658 | `		while( pStream->zText < pStream->zEnd ){` |
|  109 |  659 | `			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){` |
|   47 |  660 | `				break;` |
|    - |  661 | `			}` |
|   65 |  662 | `			if( pStream->zText[0] == '\n' ){` |
|    - |  663 | `				/* Update line counter */` |
|  ! 0 |  664 | `				pStream->nLine++;` |
|  ! 0 |  665 | `			}` |
|   65 |  666 | `			pStream->zText++;` |
|    3 |  667 | `		}` |
|   47 |  668 | `		if( pStream->zText >= pStream->zEnd ){` |
|    - |  669 | `			/* Missing closing '"' */` |
|  ! 0 |  670 | `			pToken->nType = JSON_TK_INVALID;` |
|  ! 0 |  671 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  672 | `		}else{` |
|   47 |  673 | `			pToken->nType = JSON_TK_STR;` |
|   47 |  674 | `			pStream->zText++; /* Jump the closing double quotes */` |
|    - |  675 | `		}` |
|  107 |  676 | `	}else if( (pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]))` |
|   66 |  677 | `		\|\| (pStream->zText[0] == '-' && &pStream->zText[1] < pStream->zEnd` |
|   12 |  678 | `			&& pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1])) ){` |
|    - |  679 | `		/* Number (JSON allows an optional leading minus). Consuming the first` |
|    - |  680 | `		 * character here covers both the '-' and a leading digit; the digit run` |
|    - |  681 | `		 * below then eats the integer part. */` |
|   65 |  682 | `		pStream->zText++;` |
|   65 |  683 | `		pToken->nType = JSON_TK_NUM;` |
|   77 |  684 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|   14 |  685 | `			pStream->zText++;` |
|    2 |  686 | `		}` |
|   65 |  687 | `		if( pStream->zText < pStream->zEnd ){` |
|   61 |  688 | `			c = pStream->zText[0];` |
|   61 |  689 | `			if( c == '.' ){` |
|    - |  690 | `					/* Real number */` |
|    3 |  691 | `					pStream->zText++;` |
|    5 |  692 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    3 |  693 | `						pStream->zText++;` |
|    1 |  694 | `					}` |
|    3 |  695 | `					if( pStream->zText < pStream->zEnd ){` |
|    3 |  696 | `						c = pStream->zText[0];` |
|    3 |  697 | `						if( c=='e' \|\| c=='E' ){` |
|  ! 0 |  698 | `							pStream->zText++;` |
|  ! 0 |  699 | `							if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  700 | `								c = pStream->zText[0];` |
|  ! 0 |  701 | `								if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  702 | `									pStream->zText++;` |
|  ! 0 |  703 | `								}` |
|  ! 0 |  704 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  705 | `									pStream->zText++;` |
|  ! 0 |  706 | `								}` |
|  ! 0 |  707 | `							}` |
|  ! 0 |  708 | `						}` |
|    2 |  709 | `					}` |
|   60 |  710 | `				}else if( c=='e' \|\| c=='E' ){` |
|    - |  711 | `					/* Real number */` |
|  ! 0 |  712 | `					pStream->zText++;` |
|  ! 0 |  713 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  714 | `						c = pStream->zText[0];` |
|  ! 0 |  715 | `						if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  716 | `							pStream->zText++;` |
|  ! 0 |  717 | `						}` |
|  ! 0 |  718 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  719 | `							pStream->zText++;` |
|  ! 0 |  720 | `						}` |
|  ! 0 |  721 | `					}` |
|  ! 0 |  722 | `				}` |
|   32 |  723 | `			}` |
|   60 |  724 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|   14 |  725 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|    - |  726 | `			/* boolean true */` |
|    3 |  727 | `			pToken->nType = JSON_TK_TRUE;` |
|    - |  728 | `			/* Advance the stream cursor */` |
|    3 |  729 | `			pStream->zText += sizeof("true")-1;` |
|   27 |  730 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|   12 |  731 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|    - |  732 | `			/* boolean false */` |
|  ! 0 |  733 | `			pToken->nType = JSON_TK_FALSE;` |
|    - |  734 | `			/* Advance the stream cursor */` |
|  ! 0 |  735 | `			pStream->zText += sizeof("false")-1;` |
|   26 |  736 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|   12 |  737 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|    - |  738 | `			/* NULL */` |
|  ! 0 |  739 | `			pToken->nType = JSON_TK_NULL;` |
|    - |  740 | `			/* Advance the stream cursor */` |
|  ! 0 |  741 | `			pStream->zText += sizeof("null")-1;` |
|  ! 0 |  742 | `	}else{` |
|    - |  743 | `		/* Unexpected token */` |
|   20 |  744 | `		pToken->nType = JSON_TK_INVALID;` |
|    - |  745 | `		/* Advance the stream cursor */` |
|   20 |  746 | `		pStream->zText++;` |
|   20 |  747 | `		*pJsonErr = JSON_ERROR_SYNTAX;` |
|    - |  748 | `		/* Abort processing immediatley */` |
|   20 |  749 | `		return SXERR_ABORT;` |
|    - |  750 | `	}` |
|    - |  751 | `	/* record token length */` |
|  285 |  752 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  285 |  753 | `	if( pToken->nType == JSON_TK_STR ){` |
|   47 |  754 | `		pStr->nByte--;` |
|   22 |  755 | `	}` |
|    - |  756 | `	/* Return to the lexer */` |
|  285 |  757 | `	return SXRET_OK;` |
|  153 |  758 | `}` |
|    - |  759 | `/*` |
|    - |  760 | ` * JSON decoded input consumer callback signature.` |
|    - |  761 | ` */` |
|    - |  762 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|    - |  763 | `/*` |
|    - |  764 | ` * JSON decoder state is kept in the following structure.` |
|    - |  765 | ` */` |
|    - |  766 | `typedef struct json_decoder json_decoder;` |
|    - |  767 | `struct json_decoder` |
|    - |  768 | `{` |
|    - |  769 | `	ph7_context *pCtx; /* Call context */` |
|    - |  770 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|    - |  771 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|    - |  772 | `	int iFlags;        /* Configuration flags */` |
|    - |  773 | `	SyToken *pIn;      /* Token stream */` |
|    - |  774 | `	SyToken *pEnd;     /* End of the token stream */` |
|    - |  775 | `	int rec_depth;     /* Recursion limit */` |
|    - |  776 | `	int rec_count;     /* Current nesting level */` |
|    - |  777 | `	int *pErr;         /* JSON decoding error if any */` |
|    - |  778 | `};` |
|    - |  779 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|    - |  780 | `/* Forward declaration */` |
|    - |  781 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|    - |  782 | `/*` |
|    - |  783 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|    - |  784 | ` * the result in the given ph7_value.` |
|    - |  785 | ` */` |
|   44 |  786 | `static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)` |
|    3 |  787 | `{` |
|   47 |  788 | `	const char *zIn = pStr->zString;` |
|   47 |  789 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|    - |  790 | `	const char *zCur;` |
|    - |  791 | `	int c;` |
|    - |  792 | `	/* Mark the value as a string */` |
|   47 |  793 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|   22 |  794 | `	for(;;){` |
|   47 |  795 | `		zCur = zIn;` |
|  109 |  796 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|   65 |  797 | `			zIn++;` |
|    3 |  798 | `		}` |
|   47 |  799 | `		if( zIn > zCur ){` |
|    - |  800 | `			/* Append chunk verbatim */` |
|   47 |  801 | `			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));` |
|   22 |  802 | `		}` |
|   47 |  803 | `		zIn++;` |
|   47 |  804 | `		if( zIn >= zEnd ){` |
|    - |  805 | `			/* End of the input reached */` |
|   47 |  806 | `			break;` |
|    - |  807 | `		}` |
|  ! 0 |  808 | `		c = zIn[0];` |
|    - |  809 | `		/* Unescape the character */` |
|  ! 0 |  810 | `		switch(c){` |
|  ! 0 |  811 | `		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  812 | `		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  813 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|  ! 0 |  814 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|  ! 0 |  815 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|  ! 0 |  816 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|  ! 0 |  817 | `		default:` |
|  ! 0 |  818 | `			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));` |
|  ! 0 |  819 | `			break;` |
|    - |  820 | `		}` |
|    - |  821 | `		/* Advance the stream cursor */` |
|  ! 0 |  822 | `		zIn++;` |
|  ! 0 |  823 | `	}` |
|   47 |  824 | `}` |
|    - |  825 | `/*` |
|    - |  826 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|    - |  827 | ` * According to wikipedia` |
|    - |  828 | ` * JSON's basic types are:` |
|    - |  829 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |  830 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |  831 | ` *   Boolean (true or false)` |
|    - |  832 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |  833 | ` *    do not need to be of the same type)` |
|    - |  834 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |  835 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |  836 | ` *     be distinct from each other)` |
|    - |  837 | ` *   null (empty)` |
|    - |  838 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |  839 | ` */` |
|  118 |  840 | `static sxi32 VmJsonDecode(` |
|    - |  841 | `	json_decoder *pDecoder, /* JSON decoder */` |
|    - |  842 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|    3 |  843 | `	){` |
|    - |  844 | `	ph7_value *pWorker; /* Worker variable */` |
|    - |  845 | `	sxi32 rc;` |
|    - |  846 | `	/* Check if we do not nest to much */` |
|  121 |  847 | `	if( pDecoder->rec_count >= pDecoder->rec_depth ){` |
|    - |  848 | `		/* Nesting limit reached,abort decoding immediately */` |
|  ! 0 |  849 | `		*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|  ! 0 |  850 | `		return SXERR_ABORT;` |
|    - |  851 | `	}` |
|  121 |  852 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|    - |  853 | `		/* Scalar value */` |
|   72 |  854 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|   72 |  855 | `		if( pWorker == 0 ){` |
|  ! 0 |  856 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  857 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  858 | `			return SXERR_ABORT;` |
|    - |  859 | `		}` |
|    - |  860 | `		/* Reflect the JSON image */` |
|   72 |  861 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|    - |  862 | `			/* Nullify the value.*/` |
|    1 |  863 | `			ph7_value_null(pWorker);` |
|   72 |  864 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|    - |  865 | `			/* Boolean value */` |
|    3 |  866 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|   71 |  867 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|   64 |  868 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|    - |  869 | `			/*` |
|    - |  870 | `			 * Numeric value.` |
|    - |  871 | `			 * Get a string representation first then try to get a numeric` |
|    - |  872 | `			 * value.` |
|    - |  873 | `			 */` |
|   64 |  874 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|    - |  875 | `			/* Obtain a numeric representation */` |
|   64 |  876 | `			PH7_MemObjToNumeric(pWorker);` |
|   33 |  877 | `		}else{` |
|    - |  878 | `			/* Dequote the string */` |
|    8 |  879 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);` |
|    - |  880 | `		}` |
|    - |  881 | `		/* Invoke the consumer callback */` |
|   72 |  882 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|   72 |  883 | `		if( rc == SXERR_ABORT ){` |
|  ! 0 |  884 | `			return SXERR_ABORT;` |
|    - |  885 | `		}` |
|    - |  886 | `		/* All done,advance the stream cursor */` |
|   72 |  887 | `		pDecoder->pIn++;` |
|   86 |  888 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|    - |  889 | `		ProcJsonConsumer xOld;` |
|    - |  890 | `		void *pOld;` |
|    - |  891 | `		/* Array representation*/` |
|   23 |  892 | `		pDecoder->pIn++;` |
|    - |  893 | `		/* Create a working array */` |
|   23 |  894 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   23 |  895 | `		if( pWorker == 0 ){` |
|  ! 0 |  896 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  897 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  898 | `			return SXERR_ABORT;` |
|    - |  899 | `		}` |
|    - |  900 | `		/* Save the old consumer */` |
|   23 |  901 | `		xOld = pDecoder->xConsumer;` |
|   23 |  902 | `		pOld = pDecoder->pUserData;` |
|    - |  903 | `		/* Set the new consumer */` |
|   23 |  904 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   23 |  905 | `		pDecoder->pUserData = pWorker;` |
|    - |  906 | `		/* Decode the array */` |
|   32 |  907 | `		for(;;){` |
|    - |  908 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  909 | `			 * do this.` |
|    - |  910 | `			 */` |
|   91 |  911 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|   27 |  912 | `				pDecoder->pIn++;` |
|    3 |  913 | `			}` |
|   67 |  914 | `			if( pDecoder->pIn >= pDecoder->pEnd ){` |
|    - |  915 | `				/* Ran out of tokens before the closing ']': php rejects an` |
|    - |  916 | `				 * unterminated array as a syntax error. */` |
|    3 |  917 | `				*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|    3 |  918 | `				return SXERR_ABORT;` |
|    - |  919 | `			}` |
|   65 |  920 | `			if( pDecoder->pIn->nType & JSON_TK_CSB /*']'*/ ){` |
|   21 |  921 | `				pDecoder->pIn++; /* Jump the trailing ']' */` |
|   21 |  922 | `				break;` |
|    - |  923 | `			}` |
|    - |  924 | `			/* Recurse and decode the entry */` |
|   47 |  925 | `			pDecoder->rec_count++;` |
|   47 |  926 | `			rc = VmJsonDecode(pDecoder,0);` |
|   47 |  927 | `			pDecoder->rec_count--;` |
|   47 |  928 | `			if( rc == SXERR_ABORT ){` |
|    - |  929 | `				/* Abort processing immediately */` |
|  ! 0 |  930 | `				return SXERR_ABORT;` |
|    - |  931 | `			}` |
|    - |  932 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|   47 |  933 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|   42 |  934 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|    - |  935 | `					/* Unexpected token,abort immediatley */` |
|  ! 0 |  936 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  937 | `					return SXERR_ABORT;` |
|    - |  938 | `			}` |
|    3 |  939 | `		}` |
|    - |  940 | `		/* Restore the old consumer */` |
|   21 |  941 | `		pDecoder->xConsumer = xOld;` |
|   21 |  942 | `		pDecoder->pUserData = pOld;` |
|    - |  943 | `		/* Invoke the old consumer on the decoded array */` |
|   21 |  944 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|   41 |  945 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|    - |  946 | `		ProcJsonConsumer xOld;` |
|    - |  947 | `		ph7_value *pKey;` |
|    - |  948 | `		void *pOld;` |
|    - |  949 | `		/* Object representation*/` |
|   31 |  950 | `		pDecoder->pIn++;` |
|    - |  951 | `		/* Decode into a working array first; unless the caller asked for` |
|    - |  952 | `		 * associative arrays (assoc=true / JSON_OBJECT_AS_ARRAY), it is converted` |
|    - |  953 | `		 * to a stdClass below so json_decode('{...}') returns an object like php. */` |
|   31 |  954 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   31 |  955 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|   31 |  956 | `		if( pWorker == 0 \|\| pKey == 0){` |
|  ! 0 |  957 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  958 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  959 | `			return SXERR_ABORT;` |
|    - |  960 | `		}` |
|    - |  961 | `		/* Save the old consumer */` |
|   31 |  962 | `		xOld = pDecoder->xConsumer;` |
|   31 |  963 | `		pOld = pDecoder->pUserData;` |
|    - |  964 | `		/* Set the new consumer */` |
|   31 |  965 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   31 |  966 | `		pDecoder->pUserData = pWorker;` |
|    - |  967 | `		/* Decode the object */` |
|   32 |  968 | `		for(;;){` |
|    - |  969 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  970 | `			 * do this.` |
|    - |  971 | `			 */` |
|   80 |  972 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|   15 |  973 | `				pDecoder->pIn++;` |
|    3 |  974 | `			}` |
|   68 |  975 | `			if( pDecoder->pIn >= pDecoder->pEnd ){` |
|    - |  976 | `				/* Ran out of tokens before the closing '}': php rejects an` |
|    - |  977 | `				 * unterminated object as a syntax error. */` |
|    4 |  978 | `				*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|    4 |  979 | `				return SXERR_ABORT;` |
|    - |  980 | `			}` |
|   65 |  981 | `			if( pDecoder->pIn->nType & JSON_TK_CCB /*'}'*/ ){` |
|   27 |  982 | `				pDecoder->pIn++; /* Jump the trailing '}' */` |
|   27 |  983 | `				break;` |
|    - |  984 | `			}` |
|   38 |  985 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|   41 |  986 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|    - |  987 | `					/* Syntax error,return immediately */` |
|  ! 0 |  988 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  989 | `					return SXERR_ABORT;` |
|    - |  990 | `			}` |
|    - |  991 | `			/* Dequote the key */` |
|   41 |  992 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);` |
|    - |  993 | `			/* Jump the key and the colon */` |
|   41 |  994 | `			pDecoder->pIn += 2;` |
|    - |  995 | `			/* Recurse and decode the value */` |
|   41 |  996 | `			pDecoder->rec_count++;` |
|   41 |  997 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|   41 |  998 | `			pDecoder->rec_count--;` |
|   41 |  999 | `			if( rc == SXERR_ABORT ){` |
|    - | 1000 | `				/* Abort processing immediately */` |
|    1 | 1001 | `				return SXERR_ABORT;` |
|    - | 1002 | `			}` |
|    - | 1003 | `			/* Reset the internal buffer of the key */` |
|   40 | 1004 | `			ph7_value_reset_string_cursor(pKey);` |
|    - | 1005 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|    3 | 1006 | `		}` |
|    - | 1007 | `		/* Restore the old consumer */` |
|   27 | 1008 | `		pDecoder->xConsumer = xOld;` |
|   27 | 1009 | `		pDecoder->pUserData = pOld;` |
|    - | 1010 | `		/* php returns a stdClass for a JSON object (one dynamic property per member,` |
|    - | 1011 | `		 * nested objects already converted by the recursion) unless assoc was asked. */` |
|   27 | 1012 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|   12 | 1013 | `			PH7_MemObjToObject(pWorker);` |
|    5 | 1014 | `		}` |
|    - | 1015 | `		/* Invoke the old consumer on the decoded object*/` |
|   27 | 1016 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|    - | 1017 | `		/* Release the key */` |
|   27 | 1018 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|   15 | 1019 | `	}else{` |
|    - | 1020 | `		/* Unexpected token */` |
|    1 | 1021 | `		return SXERR_ABORT; /* Abort immediately */` |
|    - | 1022 | `	}` |
|    - | 1023 | `	/* Release the worker variable */` |
|  114 | 1024 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|  114 | 1025 | `	return SXRET_OK;` |
|   62 | 1026 | `}` |
|    - | 1027 | `/*` |
|    - | 1028 | ` * The following JSON decoder callback is invoked each time` |
|    - | 1029 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|    - | 1030 | ` * is being decoded.` |
|    - | 1031 | ` */` |
|   81 | 1032 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    3 | 1033 | `{` |
|   84 | 1034 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|    - | 1035 | `	/* Insert the entry */` |
|   84 | 1036 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|   40 | 1037 | `	SXUNUSED(pCtx); /* cc warning */` |
|    - | 1038 | `	/* All done */` |
|   84 | 1039 | `	return SXRET_OK;` |
|    3 | 1040 | `}` |
|    - | 1041 | `/*` |
|    - | 1042 | ` * Standard JSON decoder callback.` |
|    - | 1043 | ` */` |
|   30 | 1044 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    3 | 1045 | `{` |
|    - | 1046 | `	/* Return the value directly */` |
|   33 | 1047 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|   15 | 1048 | `	SXUNUSED(pKey); /* cc warning */` |
|   15 | 1049 | `	SXUNUSED(pUserData);` |
|    - | 1050 | `	/* All done */` |
|   33 | 1051 | `	return SXRET_OK;` |
|    3 | 1052 | `}` |
|    - | 1053 | `/*` |
|    - | 1054 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])` |
|    - | 1055 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|    - | 1056 | ` * Parameters` |
|    - | 1057 | ` *  $json` |
|    - | 1058 | ` *    The json string being decoded.` |
|    - | 1059 | ` * $assoc` |
|    - | 1060 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|    - | 1061 | ` * $depth` |
|    - | 1062 | ` *   User specified recursion depth.` |
|    - | 1063 | ` * $options` |
|    - | 1064 | ` *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported` |
|    - | 1065 | ` * (default is to cast large integers as floats)` |
|    - | 1066 | ` * Return` |
|    - | 1067 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|    - | 1068 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|    - | 1069 | ` *  or if the encoded data is deeper than the recursion limit.` |
|    - | 1070 | ` */` |
|    - | 1071 | `/*` |
|    - | 1072 | ` * Tokenize and decode a JSON input. Shared core of json_decode() and json_validate().` |
|    - | 1073 | ` * On success the decoded value is delivered through the default decoder (i.e: it becomes` |
|    - | 1074 | ` * the call-context result, which json_validate's caller then overwrites with a boolean).` |
|    - | 1075 | ` * Returns the resulting JSON error code (pVm->json_rc): JSON_ERROR_NONE on success, a` |
|    - | 1076 | ` * non-zero json_err_code otherwise. A generic decoder abort without a specific code` |
|    - | 1077 | ` * (e.g: out of memory) is reported as JSON_ERROR_SYNTAX so callers can branch on a single` |
|    - | 1078 | ` * value, preserving the original "abort \|\| error => failure" json_decode semantics.` |
|    - | 1079 | ` */` |
|   54 | 1080 | `static int VmJsonDecodeInput(ph7_context *pCtx,const char *zIn,int nByte,int iAssoc,int nDepth)` |
|    3 | 1081 | `{` |
|   57 | 1082 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - | 1083 | `	json_decoder sDecoder;` |
|    - | 1084 | `	SySet sToken;` |
|    - | 1085 | `	SyLex sLex;` |
|    - | 1086 | `	sxi32 rc;` |
|    - | 1087 | `	/* Clear JSON error code */` |
|   57 | 1088 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|    - | 1089 | `	/* Tokenize the input */` |
|   57 | 1090 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|   57 | 1091 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|   57 | 1092 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|   57 | 1093 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|    - | 1094 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|   20 | 1095 | `		SyLexRelease(&sLex);` |
|   20 | 1096 | `		SySetRelease(&sToken);` |
|   20 | 1097 | `		return pVm->json_rc;` |
|    - | 1098 | `	}` |
|    - | 1099 | `	/* Fill the decoder */` |
|   39 | 1100 | `	sDecoder.pCtx = pCtx;` |
|   39 | 1101 | `	sDecoder.pErr = &pVm->json_rc;` |
|   39 | 1102 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|   39 | 1103 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|   39 | 1104 | `	sDecoder.iFlags = 0;` |
|   39 | 1105 | `	if( iAssoc ){` |
|    - | 1106 | `		/* Returned objects will be converted into associative arrays */` |
|   24 | 1107 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|   11 | 1108 | `	}` |
|   39 | 1109 | `	sDecoder.rec_depth = 32;` |
|   39 | 1110 | `	if( nDepth > 1 && nDepth < 32 ){` |
|    3 | 1111 | `		sDecoder.rec_depth = nDepth;` |
|    1 | 1112 | `	}` |
|   39 | 1113 | `	sDecoder.rec_count = 0;` |
|    - | 1114 | `	/* Set a default consumer */` |
|   39 | 1115 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|   39 | 1116 | `	sDecoder.pUserData = 0;` |
|    - | 1117 | `	/* Decode the raw JSON input */` |
|   39 | 1118 | `	rc = VmJsonDecode(&sDecoder,0);` |
|   39 | 1119 | `	if( rc == SXERR_ABORT && pVm->json_rc == JSON_ERROR_NONE ){` |
|    - | 1120 | `		/* Generic abort with no specific code: treat as a syntax error */` |
|    1 | 1121 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    1 | 1122 | `	}` |
|   39 | 1123 | `	if( pVm->json_rc == JSON_ERROR_NONE && sDecoder.pIn < sDecoder.pEnd ){` |
|    - | 1124 | `		/* php requires the whole input to be ONE JSON value; tokens left after a` |
|    - | 1125 | `		 * complete value (e.g. '"a":1', '{}x', '1 2') are a syntax error. */` |
|    3 | 1126 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    1 | 1127 | `	}` |
|    - | 1128 | `	/* Clean-up the mess left behind */` |
|   39 | 1129 | `	SyLexRelease(&sLex);` |
|   39 | 1130 | `	SySetRelease(&sToken);` |
|   39 | 1131 | `	return pVm->json_rc;` |
|   30 | 1132 | `}` |
|   56 | 1133 | `PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 | 1134 | `{` |
|    - | 1135 | `	const char *zIn;` |
|    - | 1136 | `	int nByte;` |
|   59 | 1137 | `	int iAssoc = 0;` |
|   59 | 1138 | `	int nDepth = 32;` |
|   59 | 1139 | `	int iFlags = 0;` |
|    - | 1140 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|    - | 1141 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|   59 | 1142 | `	if( nArg < 1 ){` |
|    - | 1143 | `		/* Missing/Invalid arguments, return NULL */` |
|  ! 0 | 1144 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1145 | `		return PH7_OK;` |
|    - | 1146 | `	}` |
|   59 | 1147 | `	if( nArg > 3 && ph7_value_is_int(apArg[3]) ){` |
|    - | 1148 | `		/* $flags — only JSON_THROW_ON_ERROR is honored (see NEWPLAN §5). */` |
|    5 | 1149 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|    2 | 1150 | `	}` |
|    - | 1151 | `	/* Extract the JSON string */` |
|   59 | 1152 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   59 | 1153 | `	if( nByte < 1 ){` |
|    - | 1154 | `		/* Empty string: php records a syntax error (json_last_error() == 4) and` |
|    - | 1155 | `		 * returns NULL, or raises a JsonException with JSON_THROW_ON_ERROR. */` |
|    6 | 1156 | `		pCtx->pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    6 | 1157 | `		if( iFlags & JSON_THROW_ON_ERROR ){` |
|  ! 0 | 1158 | `			return PH7_VmThrowException(pCtx,"JsonException","%s",` |
|  ! 0 | 1159 | `				JsonErrorMsg(JSON_ERROR_SYNTAX));` |
|    - | 1160 | `		}` |
|    6 | 1161 | `		ph7_result_null(pCtx);` |
|    6 | 1162 | `		return PH7_OK;` |
|    - | 1163 | `	}` |
|   55 | 1164 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){` |
|   28 | 1165 | `		iAssoc = 1;` |
|   13 | 1166 | `	}` |
|   55 | 1167 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|    - | 1168 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise);` |
|    - | 1169 | `		 * read as int64 so a value above INT_MAX is detected, not truncated. */` |
|   17 | 1170 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[2]);` |
|    - | 1171 | `		/* php clears the json error state before validating $depth, so a caught` |
|    - | 1172 | `		 * depth ValueError leaves json_last_error() == JSON_ERROR_NONE (the normal` |
|    - | 1173 | `		 * path resets it again inside VmJsonDecodeInput). */` |
|   17 | 1174 | `		pCtx->pVm->json_rc = JSON_ERROR_NONE;` |
|   17 | 1175 | `		if( nWant <= 0 ){` |
|    9 | 1176 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1177 | `				"json_decode(): Argument #3 ($depth) must be greater than 0");` |
|    - | 1178 | `		}` |
|    9 | 1179 | `		if( nWant > 2147483647 ){` |
|    3 | 1180 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1181 | `				"json_decode(): Argument #3 ($depth) must be less than 2147483647");` |
|    - | 1182 | `		}` |
|    7 | 1183 | `		nDepth = (int)nWant;` |
|    3 | 1184 | `	}` |
|    - | 1185 | `	/* Decode the raw JSON input.The default consumer sets the decoded value as the` |
|    - | 1186 | `	 * call-context result; on failure we replace it with NULL (or throw). */` |
|   45 | 1187 | `	if( VmJsonDecodeInput(pCtx,zIn,nByte,iAssoc,nDepth) != JSON_ERROR_NONE ){` |
|    - | 1188 | `		/* Something goes wrong while decoding JSON input. */` |
|   24 | 1189 | `		if( iFlags & JSON_THROW_ON_ERROR ){` |
|    - | 1190 | `			/* php: raise a JsonException carrying json_last_error_msg() text. */` |
|    4 | 1191 | `			return PH7_VmThrowException(pCtx,"JsonException","%s",` |
|    2 | 1192 | `				JsonErrorMsg(pCtx->pVm->json_rc));` |
|    - | 1193 | `		}` |
|   22 | 1194 | `		ph7_result_null(pCtx);` |
|   10 | 1195 | `	}` |
|    - | 1196 | `	/* All done */` |
|   43 | 1197 | `	return PH7_OK;` |
|   31 | 1198 | `}` |
|    - | 1199 | `/*` |
|    - | 1200 | ` * bool json_validate(string $json[,int $depth = 512[,int $flags = 0]])` |
|    - | 1201 | ` *  Validates whether a string is valid JSON without materializing a value.` |
|    - | 1202 | ` * Parameters` |
|    - | 1203 | ` *  $json   The string to validate.` |
|    - | 1204 | ` *  $depth  Maximum nesting depth (clamped to the engine limit of 32).` |
|    - | 1205 | ` *  $flags  Bitmask of decode options (currently none are implemented; accepted/ignored).` |
|    - | 1206 | ` * Return` |
|    - | 1207 | ` *  TRUE if the string is valid JSON, FALSE otherwise. Updates json_last_error().` |
|    - | 1208 | ` */` |
|   20 | 1209 | `PH7_PRIVATE int vm_builtin_json_validate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1210 | `{` |
|   21 | 1211 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - | 1212 | `	const char *zIn;` |
|    - | 1213 | `	int nByte;` |
|   21 | 1214 | `	int nDepth = 32;` |
|   21 | 1215 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - | 1216 | `		/* Missing/Invalid argument: not valid JSON */` |
|  ! 0 | 1217 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 | 1218 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1219 | `		return PH7_OK;` |
|    - | 1220 | `	}` |
|    - | 1221 | `	/* Extract the JSON string */` |
|   21 | 1222 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   21 | 1223 | `	if( nByte < 1 ){` |
|    - | 1224 | `		/* The empty string is not valid JSON (unlike json_decode, which returns NULL` |
|    - | 1225 | `		 * silently, json_validate must record the syntax error) */` |
|    3 | 1226 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    3 | 1227 | `		ph7_result_bool(pCtx,0);` |
|    3 | 1228 | `		return PH7_OK;` |
|    - | 1229 | `	}` |
|   19 | 1230 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|    - | 1231 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise). */` |
|    9 | 1232 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[1]);` |
|    - | 1233 | `		/* Clear the json error state before validating $depth (php parity), so a` |
|    - | 1234 | `		 * caught depth ValueError leaves json_last_error() == JSON_ERROR_NONE. */` |
|    9 | 1235 | `		pVm->json_rc = JSON_ERROR_NONE;` |
|    9 | 1236 | `		if( nWant <= 0 ){` |
|    5 | 1237 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1238 | `				"json_validate(): Argument #2 ($depth) must be greater than 0");` |
|    - | 1239 | `		}` |
|    5 | 1240 | `		if( nWant > 2147483647 ){` |
|    3 | 1241 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1242 | `				"json_validate(): Argument #2 ($depth) must be less than 2147483647");` |
|    - | 1243 | `		}` |
|    3 | 1244 | `		nDepth = (int)nWant;` |
|    1 | 1245 | `	}` |
|    - | 1246 | `	/* apArg[2] ($flags) is accepted and ignored: no decode flag is implemented.` |
|    - | 1247 | `	 * Decode in associative mode so the "objects are returned as an array" warning is` |
|    - | 1248 | `	 * not raised - the decoded value is discarded, only its validity matters. */` |
|   13 | 1249 | `	ph7_result_bool(pCtx,VmJsonDecodeInput(pCtx,zIn,nByte,1,nDepth) == JSON_ERROR_NONE);` |
|   13 | 1250 | `	return PH7_OK;` |
|   11 | 1251 | `}` |
|    - | 1252 |  |
