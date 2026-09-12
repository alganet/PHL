# src/ph7/vm_json.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 511/642 lines (79.60%)

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
|    - |   72 | ` * Returns the JSON representation of a value.In other word perform a JSON encoding operation.` |
|    - |   73 | ` * According to wikipedia` |
|    - |   74 | ` * JSON's basic types are:` |
|    - |   75 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |   76 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |   77 | ` *   Boolean (true or false)` |
|    - |   78 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |   79 | ` *    do not need to be of the same type)` |
|    - |   80 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |   81 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |   82 | ` *     be distinct from each other)` |
|    - |   83 | ` *   null (empty)` |
|    - |   84 | ` * Non-significant white space may be added freely around the "structural characters"` |
|    - |   85 | ` * (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |   86 | ` */` |
| 1130 |   87 | `static sxi32 VmJsonEncode(` |
|    - |   88 | `	ph7_value *pIn,          /* Encode this value */` |
|    - |   89 | `	json_private_data *pData /* Context data */` |
|    2 |   90 | `	){` |
| 1132 |   91 | `		ph7_context *pCtx = pData->pCtx;` |
| 1132 |   92 | `		int iFlags = pData->iFlags;` |
|    - |   93 | `		int nByte;` |
| 1132 |   94 | `		if( ph7_value_is_null(pIn) \|\| ph7_value_is_resource(pIn)){` |
|    - |   95 | `			/* null */` |
|    7 |   96 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
| 1128 |   97 | `		}else if( ph7_value_is_bool(pIn) ){` |
|   15 |   98 | `			int iBool = ph7_value_to_bool(pIn);` |
|    - |   99 | `			int iLen;` |
|    - |  100 | `			/* true/false */` |
|   15 |  101 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|   15 |  102 | `			JSON_EMIT(pData,ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1));` |
| 1118 |  103 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|  559 |  104 | `			if( ph7_value_is_float(pIn) ){` |
|   31 |  105 | `				double rVal = ph7_value_to_double(pIn);` |
|    - |  106 | `				/* php rejects Inf/NaN: json_encode returns FALSE with` |
|    - |  107 | `				 * json_last_error() == JSON_ERROR_INF_OR_NAN (they have no JSON` |
|    - |  108 | `				 * representation), instead of emitting the invalid bare token. */` |
|   31 |  109 | `				if( PH7_IS_NAN(rVal) \|\| PH7_IS_INF(rVal) ){` |
|    7 |  110 | `					pData->fail = 1;` |
|    7 |  111 | `					pData->failRc = JSON_ERROR_INF_OR_NAN;` |
|    7 |  112 | `					return PH7_OK;` |
|    - |  113 | `				}` |
|    - |  114 | `				/* php's json float output follows serialize_precision` |
|    - |  115 | `				 * (shortest round-trip, like serialize/var_export), NOT the` |
|    - |  116 | `				 * echo/cast precision of 14 — with a lowercase exponent` |
|    - |  117 | `				 * marker: 1/3 -> 0.3333333333333333, 1e17 -> 1.0e+17,` |
|    - |  118 | `				 * 1.0 -> 1, -0.0 -> -0. */` |
|   25 |  119 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,rVal));` |
|   13 |  120 | `			}else{` |
|    - |  121 | `				const char *zNum;` |
|    - |  122 | `				/* Get a string representation of the number */` |
|  345 |  123 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|  345 |  124 | `				JSON_EMIT(pData,ph7_result_string(pCtx,zNum,nByte));` |
|    1 |  125 | `			}` |
|  922 |  126 | `		}else if( ph7_value_is_string(pIn) ){` |
|  360 |  127 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|    - |  128 | `				/* Encodes numeric strings as numbers (same float shapes). */` |
|    5 |  129 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|    5 |  130 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,ph7_value_to_double(pIn)));` |
|    3 |  131 | `			}else{` |
|    - |  132 | `				const char *zIn,*zEnd;` |
|    - |  133 | `				int c;` |
|    - |  134 | `				/* Encode the string */` |
|  356 |  135 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|  356 |  136 | `				zEnd = &zIn[nByte];` |
|    - |  137 | `				/* Append the double quote */` |
|  356 |  138 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"\"",(int)sizeof(char)));` |
|  663 |  139 | `				for(;;){` |
| 1334 |  140 | `					if( zIn >= zEnd ){` |
|    - |  141 | `						/* No more input to process */` |
|  356 |  142 | `						break;` |
|    - |  143 | `					}` |
|  980 |  144 | `					c = zIn[0];` |
|    - |  145 | `					/* Advance the stream cursor */` |
|  980 |  146 | `					zIn++;` |
|  980 |  147 | `					if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|    - |  148 | `						/* All < and > are converted to \u003C and \u003E */` |
|  ! 0 |  149 | `						if( c == '<' ){` |
|  ! 0 |  150 | `							JSON_EMIT(pData,ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1));` |
|  ! 0 |  151 | `						}else{` |
|  ! 0 |  152 | `							JSON_EMIT(pData,ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1));` |
|    - |  153 | `						}` |
|  ! 0 |  154 | `						continue;` |
|  980 |  155 | `					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|    - |  156 | `						/* All &s are converted to \u0026.  */` |
|  ! 0 |  157 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1));` |
|  ! 0 |  158 | `						continue;` |
|  980 |  159 | `					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|    - |  160 | `						/* All ' are converted to \u0027.   */` |
|  ! 0 |  161 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1));` |
|  ! 0 |  162 | `						continue;` |
|  980 |  163 | `					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|    - |  164 | `						/* All " are converted to \u0022. */` |
|  ! 0 |  165 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1));` |
|  ! 0 |  166 | `						continue;` |
|    - |  167 | `					}` |
|  980 |  168 | `					if( c == '"' \|\| c == '\\' ){` |
|    - |  169 | `						/* Escape the quote/backslash (php escapes the backslash` |
|    - |  170 | `						 * unconditionally — the old code wrongly tied it to` |
|    - |  171 | `						 * JSON_UNESCAPED_SLASHES, which governs '/' below) */` |
|    3 |  172 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\",(int)sizeof(char)));` |
|  978 |  173 | `					}else if( c == '/' && (iFlags & JSON_UNESCAPED_SLASHES) == 0 ){` |
|    - |  174 | `						/* php escapes forward slashes by default */` |
|    7 |  175 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\",(int)sizeof(char)));` |
|  974 |  176 | `					}else if( (unsigned char)c < 0x20 ){` |
|    - |  177 | `						/* Control characters (band A #4): php emits the short` |
|    - |  178 | `						 * escapes for \b \f \n \r \t and \u00xx for the rest —` |
|    - |  179 | `						 * pre-fix these were emitted RAW (invalid JSON). */` |
|    - |  180 | `						static const char zHex[] = "0123456789abcdef";` |
|    7 |  181 | `						char zEsc[6] = { '\\', 'u', '0', '0', 0, 0 };` |
|    7 |  182 | `						switch(c){` |
|  ! 0 |  183 | `						case '\b': JSON_EMIT(pData,ph7_result_string(pCtx,"\\b",2)); break;` |
|  ! 0 |  184 | `						case '\f': JSON_EMIT(pData,ph7_result_string(pCtx,"\\f",2)); break;` |
|    3 |  185 | `						case '\n': JSON_EMIT(pData,ph7_result_string(pCtx,"\\n",2)); break;` |
|  ! 0 |  186 | `						case '\r': JSON_EMIT(pData,ph7_result_string(pCtx,"\\r",2)); break;` |
|    3 |  187 | `						case '\t': JSON_EMIT(pData,ph7_result_string(pCtx,"\\t",2)); break;` |
|    1 |  188 | `						default:` |
|    3 |  189 | `							zEsc[4] = zHex[(c >> 4) & 0x0F];` |
|    3 |  190 | `							zEsc[5] = zHex[c & 0x0F];` |
|    3 |  191 | `							JSON_EMIT(pData,ph7_result_string(pCtx,zEsc,6));` |
|    2 |  192 | `							break;` |
|    - |  193 | `						}` |
|    7 |  194 | `						continue;` |
|    - |  195 | `					}` |
|    - |  196 | `					/* Append character verbatim */` |
|  974 |  197 | `					JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));` |
|    2 |  198 | `				}` |
|    - |  199 | `				/* Append the double quote */` |
|  356 |  200 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"\"",(int)sizeof(char)));` |
|    2 |  201 | `			}` |
|  559 |  202 | `		}else if( ph7_value_is_array(pIn) ){` |
|    - |  203 | `			/* An array encodes as a JSON array iff it is a "list" [consecutive` |
|    - |  204 | `			 * 0-based int keys]; otherwise [or under JSON_FORCE_OBJECT] as an` |
|    - |  205 | `			 * object with stringified keys (PHP semantics). */` |
|  620 |  206 | `			int isObject = (iFlags & JSON_FORCE_OBJECT)` |
|  310 |  207 | `				\|\| !PH7_HashmapIsList((ph7_hashmap *)pIn->x.pOther);` |
|  311 |  208 | `			int savedObject = pData->isObject; /* restore for sibling entries after recursion */` |
|  311 |  209 | `			int c = isObject ? '{' : '[';` |
|  311 |  210 | `			int d = isObject ? '}' : ']';` |
|    - |  211 | `			/* Encode the array */` |
|  311 |  212 | `			pData->isObject = isObject;` |
|  311 |  213 | `			pData->isFirst = 1;` |
|    - |  214 | `			/* Append the square bracket or curly braces */` |
|  311 |  215 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));` |
|    - |  216 | `			/* Iterate throw array entries */` |
|  311 |  217 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|    - |  218 | `			/* Bail if a nested append ran out of memory before the closer */` |
|  311 |  219 | `			if( pData->oom ){` |
|  ! 0 |  220 | `				return PH7_OK;` |
|    - |  221 | `			}` |
|    - |  222 | `			/* Append the closing square bracket or curly braces */` |
|  311 |  223 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char)));` |
|  311 |  224 | `			pData->isObject = savedObject;` |
|  225 |  225 | `		}else if( ph7_value_is_object(pIn) ){` |
|   70 |  226 | `			ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   70 |  227 | `			ph7_vm *pVm = pIn->pVm;` |
|   70 |  228 | `			ph7_class_method *pMethod = 0;` |
|    - |  229 | `			/* If the object implements JsonSerializable, encode the value` |
|    - |  230 | `			 * returned by jsonSerialize() instead of its public properties.` |
|    - |  231 | `			 * An enum implementing it explicitly also takes this path (php). */` |
|   68 |  232 | `			if( pVm->pJsonSerializableClass` |
|   70 |  233 | `				&& PH7_VmInstanceOf(pThis->pClass,pVm->pJsonSerializableClass) ){` |
|   17 |  234 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"jsonSerialize",sizeof("jsonSerialize")-1);` |
|    8 |  235 | `			}` |
|   70 |  236 | `			if( pMethod == 0 && (pThis->pClass->iFlags & PH7_CLASS_ENUM) != 0 ){` |
|    - |  237 | `				/* php 8.1: a BACKED enum case encodes as its backing value; a` |
|    - |  238 | `				 * pure enum case has no default serialization — json_encode` |
|    - |  239 | `				 * returns false. */` |
|    9 |  240 | `				ph7_value *pBacking = PH7_EnumCaseBackingValueOf(pThis);` |
|    9 |  241 | `				if( pBacking ){` |
|    7 |  242 | `					pData->nRecCount++;` |
|    7 |  243 | `					VmJsonEncode(pBacking,pData);` |
|    7 |  244 | `					pData->nRecCount--;` |
|    4 |  245 | `				}else{` |
|    3 |  246 | `					pData->fail = 1;` |
|    3 |  247 | `					pData->failRc = JSON_ERROR_NON_BACKED_ENUM;` |
|    - |  248 | `				}` |
|    9 |  249 | `				return PH7_OK;` |
|    - |  250 | `			}` |
|   62 |  251 | `			if( pMethod ){` |
|    - |  252 | `				ph7_value sResult;` |
|    - |  253 | `				sxi32 rc;` |
|   17 |  254 | `				PH7_MemObjInit(pVm,&sResult);` |
|   17 |  255 | `				rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|   17 |  256 | `				if( rc == PH7_EXCEPTION ){` |
|    - |  257 | `					/* Let jsonSerialize()'s throw propagate */` |
|    5 |  258 | `					PH7_MemObjRelease(&sResult);` |
|    5 |  259 | `					pData->exc = 1;` |
|    5 |  260 | `					return PH7_EXCEPTION;` |
|    - |  261 | `				}` |
|    - |  262 | `				/* Encode the returned value [scalar/array/object] */` |
|   13 |  263 | `				pData->nRecCount++;` |
|   13 |  264 | `				VmJsonEncode(&sResult,pData);` |
|   13 |  265 | `				pData->nRecCount--;` |
|   13 |  266 | `				PH7_MemObjRelease(&sResult);` |
|   13 |  267 | `				if( pData->exc ){` |
|  ! 0 |  268 | `					return PH7_EXCEPTION;` |
|    - |  269 | `				}` |
|   13 |  270 | `				if( pData->oom ){` |
|  ! 0 |  271 | `					return PH7_OK;` |
|    - |  272 | `				}` |
|    7 |  273 | `			}else{` |
|    - |  274 | `				SyHashEntry *pAttrEntry;` |
|    - |  275 | `				SySet sNames;` |
|    - |  276 | `				SyString *aName;` |
|    - |  277 | `				sxu32 iName,nName;` |
|    - |  278 | `				/* Encode the class instance: php serializes only PUBLIC` |
|    - |  279 | `				 * non-static properties, reading through a PHP 8.4 get hook` |
|    - |  280 | `				 * when one is declared (virtual properties included). The` |
|    - |  281 | `				 * names are SNAPSHOTTED first — a hook dispatched mid-walk may` |
|    - |  282 | `				 * re-enter an hAttr walk on this instance (the hash has a` |
|    - |  283 | `				 * single embedded loop cursor) or unset()/create properties;` |
|    - |  284 | `				 * names point into class-owned attr storage and each is` |
|    - |  285 | `				 * re-looked-up before use. */` |
|   46 |  286 | `				pData->isFirst = 1;` |
|    - |  287 | `				/* Append the curly braces */` |
|   46 |  288 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"{",(int)sizeof(char)));` |
|   46 |  289 | `				SySetInit(&sNames,&pVm->sAllocator,sizeof(SyString));` |
|   46 |  290 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|  124 |  291 | `				while( (pAttrEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   80 |  292 | `					VmClassAttr *pVmAttr = (VmClassAttr *)pAttrEntry->pUserData;` |
|   78 |  293 | `					if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT))` |
|   80 |  294 | `					 \|\| pVmAttr->pAttr->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|    3 |  295 | `						continue;` |
|    - |  296 | `					}` |
|   76 |  297 | `					if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|   40 |  298 | `					 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|    3 |  299 | `						continue; /* virtual set-only property: no value to encode (php) */` |
|    - |  300 | `					}` |
|   76 |  301 | `					SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|    2 |  302 | `				}` |
|   46 |  303 | `				aName = (SyString *)SySetBasePtr(&sNames);` |
|   46 |  304 | `				nName = SySetUsed(&sNames);` |
|  120 |  305 | `				for( iName = 0 ; iName < nName ; ++iName ){` |
|    - |  306 | `					VmClassAttr *pVmAttr;` |
|   76 |  307 | `					ph7_value *pAttrVal = 0;` |
|    - |  308 | `					ph7_value sHookVal;` |
|    - |  309 | `					sxi32 rcHk;` |
|   76 |  310 | `					pAttrEntry = SyHashGet(&pThis->hAttr,(const void *)aName[iName].zString,aName[iName].nByte);` |
|   76 |  311 | `					if( pAttrEntry == 0 ){` |
|  ! 0 |  312 | `						continue; /* unset by an earlier hook */` |
|    - |  313 | `					}` |
|   76 |  314 | `					pVmAttr = (VmClassAttr *)pAttrEntry->pUserData;` |
|   76 |  315 | `					PH7_MemObjInit(pVm,&sHookVal);` |
|   76 |  316 | `					rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|   76 |  317 | `					if( rcHk == SXRET_OK ){` |
|   11 |  318 | `						pAttrVal = &sHookVal;` |
|   71 |  319 | `					}else if( rcHk == SXERR_NOTFOUND ){` |
|    - |  320 | `						/* Encode a COPY: the encoder casts scalars in place` |
|    - |  321 | `						 * (ph7_value_to_string), which must not corrupt the` |
|    - |  322 | `						 * live attribute slot. */` |
|   66 |  323 | `						ph7_value *pRaw = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   66 |  324 | `						if( pRaw ){` |
|   66 |  325 | `							PH7_MemObjStore(pRaw,&sHookVal);` |
|   66 |  326 | `							pAttrVal = &sHookVal;` |
|   32 |  327 | `						}` |
|   34 |  328 | `					}else{` |
|    - |  329 | `						/* the get hook threw — propagate like jsonSerialize() */` |
|  ! 0 |  330 | `						PH7_MemObjRelease(&sHookVal);` |
|  ! 0 |  331 | `						SySetRelease(&sNames);` |
|  ! 0 |  332 | `						pData->exc = 1;` |
|  ! 0 |  333 | `						return PH7_EXCEPTION;` |
|    - |  334 | `					}` |
|   76 |  335 | `					if( pAttrVal ){` |
|   76 |  336 | `						VmJsonObjectEncode(SyStringData(&pVmAttr->pAttr->sName),pAttrVal,pData);` |
|   37 |  337 | `					}` |
|   76 |  338 | `					PH7_MemObjRelease(&sHookVal);` |
|   76 |  339 | `					if( pData->exc ){` |
|  ! 0 |  340 | `						SySetRelease(&sNames);` |
|  ! 0 |  341 | `						return PH7_EXCEPTION; /* a nested jsonSerialize()/hook threw */` |
|    - |  342 | `					}` |
|   76 |  343 | `					if( pData->oom ){` |
|  ! 0 |  344 | `						SySetRelease(&sNames);` |
|  ! 0 |  345 | `						return PH7_OK;` |
|    - |  346 | `					}` |
|   39 |  347 | `				}` |
|   46 |  348 | `				SySetRelease(&sNames);` |
|    - |  349 | `				/* Append the closing curly braces  */` |
|   46 |  350 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"}",(int)sizeof(char)));` |
|    - |  351 | `			}` |
|   30 |  352 | `		}else{` |
|    - |  353 | `			/* Can't happen */` |
|  ! 0 |  354 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|    - |  355 | `		}` |
|    - |  356 | `		/* All done */` |
| 1114 |  357 | `		return PH7_OK;` |
|  567 |  358 | `}` |
|    - |  359 | `/*` |
|    - |  360 | ` * The following walker callback is invoked each time we need` |
|    - |  361 | ` * to encode an array to JSON.` |
|    - |  362 | ` */` |
|  706 |  363 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|    1 |  364 | `{` |
|  707 |  365 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|  707 |  366 | `	if( pJson->nRecCount > 31 \|\| pJson->exc \|\| pJson->oom ){` |
|    - |  367 | `		/* Recursion limit reached, a callback threw, or OOM — return immediately */` |
|  ! 0 |  368 | `		return PH7_OK;` |
|    - |  369 | `	}` |
|  707 |  370 | `	if( !pJson->isFirst ){` |
|    - |  371 | `		/* Append the colon first */` |
|  427 |  372 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|  213 |  373 | `	}` |
|  707 |  374 | `	if( pJson->isObject ){` |
|    - |  375 | `		/* Outputs an object rather than an array */` |
|    - |  376 | `		const char *zKey;` |
|    - |  377 | `		int nByte;` |
|    - |  378 | `		/* Extract a string representation of the key */` |
|  343 |  379 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|    - |  380 | `		/* Append the quoted key and the colon (checked, so an OOM here is caught` |
|    - |  381 | `		 * rather than silently truncating; matches the prior "%.*s" emit byte for` |
|    - |  382 | `		 * byte — keys are not JSON-escaped, a pre-existing behavior). */` |
|  343 |  383 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\"",(int)sizeof(char)));` |
|  343 |  384 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,zKey,nByte));` |
|  343 |  385 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\":",(int)sizeof("\":")-1));` |
|  171 |  386 | `	}` |
|    - |  387 | `	/* Encode the value */` |
|  707 |  388 | `	pJson->nRecCount++;` |
|  707 |  389 | `	VmJsonEncode(pValue,pJson);` |
|  707 |  390 | `	pJson->nRecCount--;` |
|  707 |  391 | `	pJson->isFirst = 0;` |
|  707 |  392 | `	return PH7_OK;` |
|  354 |  393 | `}` |
|    - |  394 | `/*` |
|    - |  395 | ` * The following walker callback is invoked each time we need to encode` |
|    - |  396 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|    - |  397 | ` */` |
|   74 |  398 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData)` |
|    2 |  399 | `{` |
|   76 |  400 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|   76 |  401 | `	if( pJson->nRecCount > 31 \|\| pJson->exc \|\| pJson->oom ){` |
|    - |  402 | `		/* Recursion limit reached, a callback threw, or OOM — return immediately */` |
|  ! 0 |  403 | `		return PH7_OK;` |
|    - |  404 | `	}` |
|   76 |  405 | `	if( !pJson->isFirst ){` |
|    - |  406 | `		/* Append the colon first */` |
|   33 |  407 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|   16 |  408 | `	}` |
|    - |  409 | `	/* Append the quoted attribute name and the colon (checked; matches the prior` |
|    - |  410 | `	 * "%s" emit byte for byte — attribute names are not JSON-escaped). */` |
|   76 |  411 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\"",(int)sizeof(char)));` |
|   76 |  412 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,zAttr,-1));` |
|   76 |  413 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\":",(int)sizeof("\":")-1));` |
|    - |  414 | `	/* Encode the value */` |
|   76 |  415 | `	pJson->nRecCount++;` |
|   76 |  416 | `	VmJsonEncode(pValue,pJson);` |
|   76 |  417 | `	pJson->nRecCount--;` |
|   76 |  418 | `	pJson->isFirst = 0;` |
|   76 |  419 | `	return PH7_OK;` |
|   39 |  420 | `}` |
|    - |  421 | `/*` |
|    - |  422 | ` * string json_encode(mixed $value [, int $options = 0 ])` |
|    - |  423 | ` *  Returns a string containing the JSON representation of value.` |
|    - |  424 | ` * Parameters` |
|    - |  425 | ` *  $value` |
|    - |  426 | ` *  The value being encoded. Can be any type except a resource.` |
|    - |  427 | ` * $options` |
|    - |  428 | ` *  Bitmask consisting of:` |
|    - |  429 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|    - |  430 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|    - |  431 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|    - |  432 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|    - |  433 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|    - |  434 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|    - |  435 | ` *  JSON_BIGINT_AS_STRING   Not used` |
|    - |  436 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|    - |  437 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|    - |  438 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|    - |  439 | ` * Return` |
|    - |  440 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|    - |  441 | ` */` |
|    - |  442 | `static const char * JsonErrorMsg(int rc); /* defined below, near json_last_error_msg */` |
|  332 |  443 | `PH7_PRIVATE int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  444 | `{` |
|    - |  445 | `	json_private_data sJson;` |
|    - |  446 | `	sxi32 rc;` |
|  334 |  447 | `	if( nArg < 1 ){` |
|    - |  448 | `		/* Missing arguments,return FALSE */` |
|  ! 0 |  449 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  450 | `		return PH7_OK;` |
|    - |  451 | `	}` |
|    - |  452 | `	/* Prepare the JSON data */` |
|  334 |  453 | `	sJson.nRecCount = 0;` |
|  334 |  454 | `	sJson.pCtx = pCtx;` |
|  334 |  455 | `	sJson.isFirst = 1;` |
|  334 |  456 | `	sJson.iFlags = 0;` |
|  334 |  457 | `	sJson.exc = 0;` |
|  334 |  458 | `	sJson.oom = 0;` |
|  334 |  459 | `	sJson.fail = 0;` |
|  334 |  460 | `	sJson.failRc = JSON_ERROR_NON_BACKED_ENUM;` |
|  334 |  461 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|    - |  462 | `		/* Extract option flags */` |
|    9 |  463 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|    4 |  464 | `	}` |
|  334 |  465 | `	pCtx->pVm->json_rc = JSON_ERROR_NONE;` |
|    - |  466 | `	/* Perform the encoding operation */` |
|  334 |  467 | `	rc = VmJsonEncode(apArg[0],&sJson);` |
|  334 |  468 | `	if( sJson.oom ){` |
|    - |  469 | `		/* A result append ran out of memory: raise a non-catchable fatal,` |
|    - |  470 | `		 * distinct from a JSON-encoding error (json_last_error untouched). */` |
|  ! 0 |  471 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  472 | `	}` |
|  334 |  473 | `	if( rc == PH7_EXCEPTION \|\| sJson.exc ){` |
|    - |  474 | `		/* A jsonSerialize() callback threw — propagate so the exception unwinds */` |
|    5 |  475 | `		return PH7_EXCEPTION;` |
|    - |  476 | `	}` |
|  330 |  477 | `	if( sJson.fail ){` |
|    - |  478 | `		/* Unencodable value (Inf/NaN, or a php 8.1 non-backed enum case): the` |
|    - |  479 | `		 * whole encode fails — discard whatever was emitted and return FALSE. */` |
|    9 |  480 | `		pCtx->pVm->json_rc = sJson.failRc;` |
|    9 |  481 | `		if( sJson.iFlags & JSON_THROW_ON_ERROR ){` |
|    - |  482 | `			/* php: raise a JsonException carrying json_last_error_msg() instead` |
|    - |  483 | `			 * of returning FALSE. */` |
|    4 |  484 | `			return PH7_VmThrowException(pCtx,"JsonException","%s",` |
|    2 |  485 | `				JsonErrorMsg(pCtx->pVm->json_rc));` |
|    - |  486 | `		}` |
|    7 |  487 | `		ph7_result_bool(pCtx,0);` |
|    7 |  488 | `		return PH7_OK;` |
|    - |  489 | `	}` |
|    - |  490 | `	/* All done */` |
|  322 |  491 | `	return PH7_OK;` |
|  168 |  492 | `}` |
|    - |  493 | `#undef JSON_EMIT` |
|    - |  494 | `/*` |
|    - |  495 | ` * int json_last_error(void)` |
|    - |  496 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|    - |  497 | ` * Parameters` |
|    - |  498 | ` *  None` |
|    - |  499 | ` * Return` |
|    - |  500 | ` *  Returns an integer, the value can be one of the following constants:` |
|    - |  501 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|    - |  502 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|    - |  503 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|    - |  504 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|    - |  505 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|    - |  506 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|    - |  507 | ` */` |
|   18 |  508 | `PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  509 | `{` |
|   20 |  510 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  511 | `	/* Return the error code */` |
|   20 |  512 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|    9 |  513 | `	SXUNUSED(nArg); /* cc warning */` |
|    9 |  514 | `	SXUNUSED(apArg);` |
|   20 |  515 | `	return PH7_OK;` |
|    2 |  516 | `}` |
|    - |  517 | `/*` |
|    - |  518 | ` * string json_last_error_msg(void)` |
|    - |  519 | ` *  Returns the error string of the last JSON encoding/decoding operation.` |
|    - |  520 | ` * Parameters` |
|    - |  521 | ` *  None` |
|    - |  522 | ` * Return` |
|    - |  523 | ` *  Returns the human-readable message corresponding to the last json_last_error()` |
|    - |  524 | ` *  code, or "No error" if no error has occurred.` |
|    - |  525 | ` */` |
|    - |  526 | `/* Human-readable message for a json_rc code. Shared by json_last_error_msg()` |
|    - |  527 | ` * and the JSON_THROW_ON_ERROR path (php's JsonException message is exactly this` |
|    - |  528 | ` * text). */` |
|   10 |  529 | `static const char * JsonErrorMsg(int rc)` |
|    1 |  530 | `{` |
|   11 |  531 | `	switch( rc ){` |
|    3 |  532 | `	case JSON_ERROR_NONE:            return "No error";` |
|  ! 0 |  533 | `	case JSON_ERROR_DEPTH:           return "Maximum stack depth exceeded";` |
|  ! 0 |  534 | `	case JSON_ERROR_STATE_MISMATCH:  return "State mismatch (invalid or malformed JSON)";` |
|  ! 0 |  535 | `	case JSON_ERROR_CTRL_CHAR:       return "Control character error, possibly incorrectly encoded";` |
|    7 |  536 | `	case JSON_ERROR_SYNTAX:          return "Syntax error";` |
|  ! 0 |  537 | `	case JSON_ERROR_UTF8:            return "Malformed UTF-8 characters, possibly incorrectly encoded";` |
|    3 |  538 | `	case JSON_ERROR_INF_OR_NAN:     return "Inf and NaN cannot be JSON encoded";` |
|  ! 0 |  539 | `	case JSON_ERROR_NON_BACKED_ENUM: return "Non-backed enums have no default serialization";` |
|  ! 0 |  540 | `	default:                         return "Unknown error";` |
|    - |  541 | `	}` |
|    6 |  542 | `}` |
|    6 |  543 | `PH7_PRIVATE int vm_builtin_json_last_error_msg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  544 | `{` |
|    7 |  545 | `	ph7_result_string(pCtx,JsonErrorMsg(pCtx->pVm->json_rc),-1/* auto length */);` |
|    3 |  546 | `	SXUNUSED(nArg); /* cc warning */` |
|    3 |  547 | `	SXUNUSED(apArg);` |
|    7 |  548 | `	return PH7_OK;` |
|    1 |  549 | `}` |
|    - |  550 | `/* Possible tokens from the JSON tokenization process */` |
|    - |  551 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|    - |  552 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|    - |  553 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|    - |  554 | `#define JSON_TK_NULL    0x008 /* null */` |
|    - |  555 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|    - |  556 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|    - |  557 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|    - |  558 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|    - |  559 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|    - |  560 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|    - |  561 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|    - |  562 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|    - |  563 | `/*` |
|    - |  564 | ` * Tokenize an entire JSON input.` |
|    - |  565 | ` * Get a single low-level token from the input file.` |
|    - |  566 | ` * Update the stream pointer so that it points to the first` |
|    - |  567 | ` * character beyond the extracted token.` |
|    - |  568 | ` */` |
|  182 |  569 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|    2 |  570 | `{` |
|  184 |  571 | `	int *pJsonErr = (int *)pUserData;` |
|    - |  572 | `	SyString *pStr;` |
|    - |  573 | `	int c;` |
|    - |  574 | `	/* Ignore leading white spaces */` |
|  188 |  575 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|    - |  576 | `		/* Advance the stream cursor */` |
|    6 |  577 | `		if( pStream->zText[0] == '\n' ){` |
|    - |  578 | `			/* Update line counter */` |
|  ! 0 |  579 | `			pStream->nLine++;` |
|  ! 0 |  580 | `		}` |
|    6 |  581 | `		pStream->zText++;` |
|    2 |  582 | `	}` |
|  184 |  583 | `	if( pStream->zText >= pStream->zEnd ){` |
|    - |  584 | `		/* End of input reached */` |
|  ! 0 |  585 | `		SXUNUSED(pCtxData); /* cc warning */` |
|  ! 0 |  586 | `		return SXERR_EOF;` |
|    - |  587 | `	}` |
|    - |  588 | `	/* Record token starting position and line */` |
|  184 |  589 | `	pToken->nLine = pStream->nLine;` |
|  184 |  590 | `	pToken->pUserData = 0;` |
|  184 |  591 | `	pStr = &pToken->sData;` |
|  184 |  592 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  182 |  593 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|  125 |  594 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|    - |  595 | `			/* Single character */` |
|  106 |  596 | `			c = pStream->zText[0];` |
|    - |  597 | `			/* Set token type */` |
|  106 |  598 | `			switch(c){` |
|   15 |  599 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|   26 |  600 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|   16 |  601 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|   15 |  602 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|   20 |  603 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|   21 |  604 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|  ! 0 |  605 | `			default:` |
|  ! 0 |  606 | `				break;` |
|    - |  607 | `			}` |
|    - |  608 | `			/* Advance the stream cursor */` |
|  106 |  609 | `			pStream->zText++;` |
|  132 |  610 | `	}else if( pStream->zText[0] == '"') {` |
|    - |  611 | `		/* JSON string */` |
|   26 |  612 | `		pStream->zText++;` |
|   26 |  613 | `		pStr->zString++;` |
|    - |  614 | `		/* Delimit the string */` |
|   72 |  615 | `		while( pStream->zText < pStream->zEnd ){` |
|   72 |  616 | `			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){` |
|   26 |  617 | `				break;` |
|    - |  618 | `			}` |
|   48 |  619 | `			if( pStream->zText[0] == '\n' ){` |
|    - |  620 | `				/* Update line counter */` |
|  ! 0 |  621 | `				pStream->nLine++;` |
|  ! 0 |  622 | `			}` |
|   48 |  623 | `			pStream->zText++;` |
|    2 |  624 | `		}` |
|   26 |  625 | `		if( pStream->zText >= pStream->zEnd ){` |
|    - |  626 | `			/* Missing closing '"' */` |
|  ! 0 |  627 | `			pToken->nType = JSON_TK_INVALID;` |
|  ! 0 |  628 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  629 | `		}else{` |
|   26 |  630 | `			pToken->nType = JSON_TK_STR;` |
|   26 |  631 | `			pStream->zText++; /* Jump the closing double quotes */` |
|    2 |  632 | `		}` |
|   68 |  633 | `	}else if( pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    - |  634 | `		/* Number */` |
|   37 |  635 | `		pStream->zText++;` |
|   37 |  636 | `		pToken->nType = JSON_TK_NUM;` |
|   37 |  637 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  638 | `			pStream->zText++;` |
|  ! 0 |  639 | `		}` |
|   37 |  640 | `		if( pStream->zText < pStream->zEnd ){` |
|   37 |  641 | `			c = pStream->zText[0];` |
|   37 |  642 | `			if( c == '.' ){` |
|    - |  643 | `					/* Real number */` |
|  ! 0 |  644 | `					pStream->zText++;` |
|  ! 0 |  645 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  646 | `						pStream->zText++;` |
|  ! 0 |  647 | `					}` |
|  ! 0 |  648 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  649 | `						c = pStream->zText[0];` |
|  ! 0 |  650 | `						if( c=='e' \|\| c=='E' ){` |
|  ! 0 |  651 | `							pStream->zText++;` |
|  ! 0 |  652 | `							if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  653 | `								c = pStream->zText[0];` |
|  ! 0 |  654 | `								if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  655 | `									pStream->zText++;` |
|  ! 0 |  656 | `								}` |
|  ! 0 |  657 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  658 | `									pStream->zText++;` |
|  ! 0 |  659 | `								}` |
|  ! 0 |  660 | `							}` |
|  ! 0 |  661 | `						}` |
|  ! 0 |  662 | `					}` |
|   37 |  663 | `				}else if( c=='e' \|\| c=='E' ){` |
|    - |  664 | `					/* Real number */` |
|  ! 0 |  665 | `					pStream->zText++;` |
|  ! 0 |  666 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  667 | `						c = pStream->zText[0];` |
|  ! 0 |  668 | `						if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  669 | `							pStream->zText++;` |
|  ! 0 |  670 | `						}` |
|  ! 0 |  671 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  672 | `							pStream->zText++;` |
|  ! 0 |  673 | `						}` |
|  ! 0 |  674 | `					}` |
|  ! 0 |  675 | `				}` |
|   19 |  676 | `			}` |
|   44 |  677 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|   12 |  678 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|    - |  679 | `			/* boolean true */` |
|  ! 0 |  680 | `			pToken->nType = JSON_TK_TRUE;` |
|    - |  681 | `			/* Advance the stream cursor */` |
|  ! 0 |  682 | `			pStream->zText += sizeof("true")-1;` |
|   26 |  683 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|   12 |  684 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|    - |  685 | `			/* boolean false */` |
|  ! 0 |  686 | `			pToken->nType = JSON_TK_FALSE;` |
|    - |  687 | `			/* Advance the stream cursor */` |
|  ! 0 |  688 | `			pStream->zText += sizeof("false")-1;` |
|   26 |  689 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|   12 |  690 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|    - |  691 | `			/* NULL */` |
|  ! 0 |  692 | `			pToken->nType = JSON_TK_NULL;` |
|    - |  693 | `			/* Advance the stream cursor */` |
|  ! 0 |  694 | `			pStream->zText += sizeof("null")-1;` |
|  ! 0 |  695 | `	}else{` |
|    - |  696 | `		/* Unexpected token */` |
|   20 |  697 | `		pToken->nType = JSON_TK_INVALID;` |
|    - |  698 | `		/* Advance the stream cursor */` |
|   20 |  699 | `		pStream->zText++;` |
|   20 |  700 | `		*pJsonErr = JSON_ERROR_SYNTAX;` |
|    - |  701 | `		/* Abort processing immediatley */` |
|   20 |  702 | `		return SXERR_ABORT;` |
|    - |  703 | `	}` |
|    - |  704 | `	/* record token length */` |
|  166 |  705 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  166 |  706 | `	if( pToken->nType == JSON_TK_STR ){` |
|   26 |  707 | `		pStr->nByte--;` |
|   12 |  708 | `	}` |
|    - |  709 | `	/* Return to the lexer */` |
|  166 |  710 | `	return SXRET_OK;` |
|   93 |  711 | `}` |
|    - |  712 | `/*` |
|    - |  713 | ` * JSON decoded input consumer callback signature.` |
|    - |  714 | ` */` |
|    - |  715 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|    - |  716 | `/*` |
|    - |  717 | ` * JSON decoder state is kept in the following structure.` |
|    - |  718 | ` */` |
|    - |  719 | `typedef struct json_decoder json_decoder;` |
|    - |  720 | `struct json_decoder` |
|    - |  721 | `{` |
|    - |  722 | `	ph7_context *pCtx; /* Call context */` |
|    - |  723 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|    - |  724 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|    - |  725 | `	int iFlags;        /* Configuration flags */` |
|    - |  726 | `	SyToken *pIn;      /* Token stream */` |
|    - |  727 | `	SyToken *pEnd;     /* End of the token stream */` |
|    - |  728 | `	int rec_depth;     /* Recursion limit */` |
|    - |  729 | `	int rec_count;     /* Current nesting level */` |
|    - |  730 | `	int *pErr;         /* JSON decoding error if any */` |
|    - |  731 | `};` |
|    - |  732 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|    - |  733 | `/* Forward declaration */` |
|    - |  734 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|    - |  735 | `/*` |
|    - |  736 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|    - |  737 | ` * the result in the given ph7_value.` |
|    - |  738 | ` */` |
|   24 |  739 | `static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)` |
|    2 |  740 | `{` |
|   26 |  741 | `	const char *zIn = pStr->zString;` |
|   26 |  742 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|    - |  743 | `	const char *zCur;` |
|    - |  744 | `	int c;` |
|    - |  745 | `	/* Mark the value as a string */` |
|   26 |  746 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|   12 |  747 | `	for(;;){` |
|   26 |  748 | `		zCur = zIn;` |
|   72 |  749 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|   48 |  750 | `			zIn++;` |
|    2 |  751 | `		}` |
|   26 |  752 | `		if( zIn > zCur ){` |
|    - |  753 | `			/* Append chunk verbatim */` |
|   26 |  754 | `			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));` |
|   12 |  755 | `		}` |
|   26 |  756 | `		zIn++;` |
|   26 |  757 | `		if( zIn >= zEnd ){` |
|    - |  758 | `			/* End of the input reached */` |
|   26 |  759 | `			break;` |
|    - |  760 | `		}` |
|  ! 0 |  761 | `		c = zIn[0];` |
|    - |  762 | `		/* Unescape the character */` |
|  ! 0 |  763 | `		switch(c){` |
|  ! 0 |  764 | `		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  765 | `		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  766 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|  ! 0 |  767 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|  ! 0 |  768 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|  ! 0 |  769 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|  ! 0 |  770 | `		default:` |
|  ! 0 |  771 | `			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));` |
|  ! 0 |  772 | `			break;` |
|    - |  773 | `		}` |
|    - |  774 | `		/* Advance the stream cursor */` |
|  ! 0 |  775 | `		zIn++;` |
|  ! 0 |  776 | `	}` |
|   26 |  777 | `}` |
|    - |  778 | `/*` |
|    - |  779 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|    - |  780 | ` * According to wikipedia` |
|    - |  781 | ` * JSON's basic types are:` |
|    - |  782 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |  783 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |  784 | ` *   Boolean (true or false)` |
|    - |  785 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |  786 | ` *    do not need to be of the same type)` |
|    - |  787 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |  788 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |  789 | ` *     be distinct from each other)` |
|    - |  790 | ` *   null (empty)` |
|    - |  791 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |  792 | ` */` |
|   72 |  793 | `static sxi32 VmJsonDecode(` |
|    - |  794 | `	json_decoder *pDecoder, /* JSON decoder */` |
|    - |  795 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|    2 |  796 | `	){` |
|    - |  797 | `	ph7_value *pWorker; /* Worker variable */` |
|    - |  798 | `	sxi32 rc;` |
|    - |  799 | `	/* Check if we do not nest to much */` |
|   74 |  800 | `	if( pDecoder->rec_count >= pDecoder->rec_depth ){` |
|    - |  801 | `		/* Nesting limit reached,abort decoding immediately */` |
|  ! 0 |  802 | `		*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|  ! 0 |  803 | `		return SXERR_ABORT;` |
|    - |  804 | `	}` |
|   74 |  805 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|    - |  806 | `		/* Scalar value */` |
|   44 |  807 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|   44 |  808 | `		if( pWorker == 0 ){` |
|  ! 0 |  809 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  810 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  811 | `			return SXERR_ABORT;` |
|    - |  812 | `		}` |
|    - |  813 | `		/* Reflect the JSON image */` |
|   44 |  814 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|    - |  815 | `			/* Nullify the value.*/` |
|  ! 0 |  816 | `			ph7_value_null(pWorker);` |
|   44 |  817 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|    - |  818 | `			/* Boolean value */` |
|  ! 0 |  819 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|   44 |  820 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|   37 |  821 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|    - |  822 | `			/*` |
|    - |  823 | `			 * Numeric value.` |
|    - |  824 | `			 * Get a string representation first then try to get a numeric` |
|    - |  825 | `			 * value.` |
|    - |  826 | `			 */` |
|   37 |  827 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|    - |  828 | `			/* Obtain a numeric representation */` |
|   37 |  829 | `			PH7_MemObjToNumeric(pWorker);` |
|   19 |  830 | `		}else{` |
|    - |  831 | `			/* Dequote the string */` |
|    8 |  832 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);` |
|    - |  833 | `		}` |
|    - |  834 | `		/* Invoke the consumer callback */` |
|   44 |  835 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|   44 |  836 | `		if( rc == SXERR_ABORT ){` |
|  ! 0 |  837 | `			return SXERR_ABORT;` |
|    - |  838 | `		}` |
|    - |  839 | `		/* All done,advance the stream cursor */` |
|   44 |  840 | `		pDecoder->pIn++;` |
|   53 |  841 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|    - |  842 | `		ProcJsonConsumer xOld;` |
|    - |  843 | `		void *pOld;` |
|    - |  844 | `		/* Array representation*/` |
|   15 |  845 | `		pDecoder->pIn++;` |
|    - |  846 | `		/* Create a working array */` |
|   15 |  847 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   15 |  848 | `		if( pWorker == 0 ){` |
|  ! 0 |  849 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  850 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  851 | `			return SXERR_ABORT;` |
|    - |  852 | `		}` |
|    - |  853 | `		/* Save the old consumer */` |
|   15 |  854 | `		xOld = pDecoder->xConsumer;` |
|   15 |  855 | `		pOld = pDecoder->pUserData;` |
|    - |  856 | `		/* Set the new consumer */` |
|   15 |  857 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   15 |  858 | `		pDecoder->pUserData = pWorker;` |
|    - |  859 | `		/* Decode the array */` |
|   22 |  860 | `		for(;;){` |
|    - |  861 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  862 | `			 * do this.` |
|    - |  863 | `			 */` |
|   61 |  864 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|   17 |  865 | `				pDecoder->pIn++;` |
|    1 |  866 | `			}` |
|   45 |  867 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CSB) /*']'*/ ){` |
|   15 |  868 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|   15 |  869 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    7 |  870 | `				}` |
|   15 |  871 | `				break;` |
|    - |  872 | `			}` |
|    - |  873 | `			/* Recurse and decode the entry */` |
|   31 |  874 | `			pDecoder->rec_count++;` |
|   31 |  875 | `			rc = VmJsonDecode(pDecoder,0);` |
|   31 |  876 | `			pDecoder->rec_count--;` |
|   31 |  877 | `			if( rc == SXERR_ABORT ){` |
|    - |  878 | `				/* Abort processing immediately */` |
|  ! 0 |  879 | `				return SXERR_ABORT;` |
|    - |  880 | `			}` |
|    - |  881 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|   31 |  882 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|   30 |  883 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|    - |  884 | `					/* Unexpected token,abort immediatley */` |
|  ! 0 |  885 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  886 | `					return SXERR_ABORT;` |
|    - |  887 | `			}` |
|    1 |  888 | `		}` |
|    - |  889 | `		/* Restore the old consumer */` |
|   15 |  890 | `		pDecoder->xConsumer = xOld;` |
|   15 |  891 | `		pDecoder->pUserData = pOld;` |
|    - |  892 | `		/* Invoke the old consumer on the decoded array */` |
|   15 |  893 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|   25 |  894 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|    - |  895 | `		ProcJsonConsumer xOld;` |
|    - |  896 | `		ph7_value *pKey;` |
|    - |  897 | `		void *pOld;` |
|    - |  898 | `		/* Object representation*/` |
|   18 |  899 | `		pDecoder->pIn++;` |
|    - |  900 | `		/* Return the object as an associative array */` |
|   18 |  901 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|    3 |  902 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_WARNING,` |
|    - |  903 | `				"JSON Objects are always returned as an associative array"` |
|    - |  904 | `				);` |
|    1 |  905 | `		}` |
|    - |  906 | `		/* Create a working array */` |
|   18 |  907 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   18 |  908 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|   18 |  909 | `		if( pWorker == 0 \|\| pKey == 0){` |
|  ! 0 |  910 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  911 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  912 | `			return SXERR_ABORT;` |
|    - |  913 | `		}` |
|    - |  914 | `		/* Save the old consumer */` |
|   18 |  915 | `		xOld = pDecoder->xConsumer;` |
|   18 |  916 | `		pOld = pDecoder->pUserData;` |
|    - |  917 | `		/* Set the new consumer */` |
|   18 |  918 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   18 |  919 | `		pDecoder->pUserData = pWorker;` |
|    - |  920 | `		/* Decode the object */` |
|   17 |  921 | `		for(;;){` |
|    - |  922 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  923 | `			 * do this.` |
|    - |  924 | `			 */` |
|   40 |  925 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|    5 |  926 | `				pDecoder->pIn++;` |
|    1 |  927 | `			}` |
|   36 |  928 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CCB) /*'}'*/ ){` |
|   18 |  929 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|   16 |  930 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    7 |  931 | `				}` |
|   18 |  932 | `				break;` |
|    - |  933 | `			}` |
|   18 |  934 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|   20 |  935 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|    - |  936 | `					/* Syntax error,return immediately */` |
|  ! 0 |  937 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  938 | `					return SXERR_ABORT;` |
|    - |  939 | `			}` |
|    - |  940 | `			/* Dequote the key */` |
|   20 |  941 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);` |
|    - |  942 | `			/* Jump the key and the colon */` |
|   20 |  943 | `			pDecoder->pIn += 2;` |
|    - |  944 | `			/* Recurse and decode the value */` |
|   20 |  945 | `			pDecoder->rec_count++;` |
|   20 |  946 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|   20 |  947 | `			pDecoder->rec_count--;` |
|   20 |  948 | `			if( rc == SXERR_ABORT ){` |
|    - |  949 | `				/* Abort processing immediately */` |
|  ! 0 |  950 | `				return SXERR_ABORT;` |
|    - |  951 | `			}` |
|    - |  952 | `			/* Reset the internal buffer of the key */` |
|   20 |  953 | `			ph7_value_reset_string_cursor(pKey);` |
|    - |  954 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|    2 |  955 | `		}` |
|    - |  956 | `		/* Restore the old consumer */` |
|   18 |  957 | `		pDecoder->xConsumer = xOld;` |
|   18 |  958 | `		pDecoder->pUserData = pOld;` |
|    - |  959 | `		/* Invoke the old consumer on the decoded object*/` |
|   18 |  960 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|    - |  961 | `		/* Release the key */` |
|   18 |  962 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|   10 |  963 | `	}else{` |
|    - |  964 | `		/* Unexpected token */` |
|  ! 0 |  965 | `		return SXERR_ABORT; /* Abort immediately */` |
|    - |  966 | `	}` |
|    - |  967 | `	/* Release the worker variable */` |
|   74 |  968 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|   74 |  969 | `	return SXRET_OK;` |
|   38 |  970 | `}` |
|    - |  971 | `/*` |
|    - |  972 | ` * The following JSON decoder callback is invoked each time` |
|    - |  973 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|    - |  974 | ` * is being decoded.` |
|    - |  975 | ` */` |
|   48 |  976 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  977 | `{` |
|   50 |  978 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|    - |  979 | `	/* Insert the entry */` |
|   50 |  980 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|   24 |  981 | `	SXUNUSED(pCtx); /* cc warning */` |
|    - |  982 | `	/* All done */` |
|   50 |  983 | `	return SXRET_OK;` |
|    2 |  984 | `}` |
|    - |  985 | `/*` |
|    - |  986 | ` * Standard JSON decoder callback.` |
|    - |  987 | ` */` |
|   24 |  988 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  989 | `{` |
|    - |  990 | `	/* Return the value directly */` |
|   26 |  991 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|   12 |  992 | `	SXUNUSED(pKey); /* cc warning */` |
|   12 |  993 | `	SXUNUSED(pUserData);` |
|    - |  994 | `	/* All done */` |
|   26 |  995 | `	return SXRET_OK;` |
|    2 |  996 | `}` |
|    - |  997 | `/*` |
|    - |  998 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])` |
|    - |  999 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|    - | 1000 | ` * Parameters` |
|    - | 1001 | ` *  $json` |
|    - | 1002 | ` *    The json string being decoded.` |
|    - | 1003 | ` * $assoc` |
|    - | 1004 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|    - | 1005 | ` * $depth` |
|    - | 1006 | ` *   User specified recursion depth.` |
|    - | 1007 | ` * $options` |
|    - | 1008 | ` *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported` |
|    - | 1009 | ` * (default is to cast large integers as floats)` |
|    - | 1010 | ` * Return` |
|    - | 1011 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|    - | 1012 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|    - | 1013 | ` *  or if the encoded data is deeper than the recursion limit.` |
|    - | 1014 | ` */` |
|    - | 1015 | `/*` |
|    - | 1016 | ` * Tokenize and decode a JSON input. Shared core of json_decode() and json_validate().` |
|    - | 1017 | ` * On success the decoded value is delivered through the default decoder (i.e: it becomes` |
|    - | 1018 | ` * the call-context result, which json_validate's caller then overwrites with a boolean).` |
|    - | 1019 | ` * Returns the resulting JSON error code (pVm->json_rc): JSON_ERROR_NONE on success, a` |
|    - | 1020 | ` * non-zero json_err_code otherwise. A generic decoder abort without a specific code` |
|    - | 1021 | ` * (e.g: out of memory) is reported as JSON_ERROR_SYNTAX so callers can branch on a single` |
|    - | 1022 | ` * value, preserving the original "abort \|\| error => failure" json_decode semantics.` |
|    - | 1023 | ` */` |
|   42 | 1024 | `static int VmJsonDecodeInput(ph7_context *pCtx,const char *zIn,int nByte,int iAssoc,int nDepth)` |
|    2 | 1025 | `{` |
|   44 | 1026 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - | 1027 | `	json_decoder sDecoder;` |
|    - | 1028 | `	SySet sToken;` |
|    - | 1029 | `	SyLex sLex;` |
|    - | 1030 | `	sxi32 rc;` |
|    - | 1031 | `	/* Clear JSON error code */` |
|   44 | 1032 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|    - | 1033 | `	/* Tokenize the input */` |
|   44 | 1034 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|   44 | 1035 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|   44 | 1036 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|   44 | 1037 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|    - | 1038 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|   20 | 1039 | `		SyLexRelease(&sLex);` |
|   20 | 1040 | `		SySetRelease(&sToken);` |
|   20 | 1041 | `		return pVm->json_rc;` |
|    - | 1042 | `	}` |
|    - | 1043 | `	/* Fill the decoder */` |
|   26 | 1044 | `	sDecoder.pCtx = pCtx;` |
|   26 | 1045 | `	sDecoder.pErr = &pVm->json_rc;` |
|   26 | 1046 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|   26 | 1047 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|   26 | 1048 | `	sDecoder.iFlags = 0;` |
|   26 | 1049 | `	if( iAssoc ){` |
|    - | 1050 | `		/* Returned objects will be converted into associative arrays */` |
|   24 | 1051 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|   11 | 1052 | `	}` |
|   26 | 1053 | `	sDecoder.rec_depth = 32;` |
|   26 | 1054 | `	if( nDepth > 1 && nDepth < 32 ){` |
|    3 | 1055 | `		sDecoder.rec_depth = nDepth;` |
|    1 | 1056 | `	}` |
|   26 | 1057 | `	sDecoder.rec_count = 0;` |
|    - | 1058 | `	/* Set a default consumer */` |
|   26 | 1059 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|   26 | 1060 | `	sDecoder.pUserData = 0;` |
|    - | 1061 | `	/* Decode the raw JSON input */` |
|   26 | 1062 | `	rc = VmJsonDecode(&sDecoder,0);` |
|   26 | 1063 | `	if( rc == SXERR_ABORT && pVm->json_rc == JSON_ERROR_NONE ){` |
|    - | 1064 | `		/* Generic abort with no specific code: treat as a syntax error */` |
|  ! 0 | 1065 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 | 1066 | `	}` |
|    - | 1067 | `	/* Clean-up the mess left behind */` |
|   26 | 1068 | `	SyLexRelease(&sLex);` |
|   26 | 1069 | `	SySetRelease(&sToken);` |
|   26 | 1070 | `	return pVm->json_rc;` |
|   23 | 1071 | `}` |
|   44 | 1072 | `PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1073 | `{` |
|    - | 1074 | `	const char *zIn;` |
|    - | 1075 | `	int nByte;` |
|   46 | 1076 | `	int iAssoc = 0;` |
|   46 | 1077 | `	int nDepth = 32;` |
|   46 | 1078 | `	int iFlags = 0;` |
|    - | 1079 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|    - | 1080 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|   46 | 1081 | `	if( nArg < 1 ){` |
|    - | 1082 | `		/* Missing/Invalid arguments, return NULL */` |
|  ! 0 | 1083 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1084 | `		return PH7_OK;` |
|    - | 1085 | `	}` |
|   46 | 1086 | `	if( nArg > 3 && ph7_value_is_int(apArg[3]) ){` |
|    - | 1087 | `		/* $flags — only JSON_THROW_ON_ERROR is honored (see NEWPLAN §5). */` |
|    5 | 1088 | `		iFlags = ph7_value_to_int(apArg[3]);` |
|    2 | 1089 | `	}` |
|    - | 1090 | `	/* Extract the JSON string */` |
|   46 | 1091 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   46 | 1092 | `	if( nByte < 1 ){` |
|    - | 1093 | `		/* Empty string: php records a syntax error. Without the throw flag it` |
|    - | 1094 | `		 * returns NULL (leaving json_last_error untouched, as PHL always has);` |
|    - | 1095 | `		 * with JSON_THROW_ON_ERROR it raises a JsonException. */` |
|    6 | 1096 | `		if( iFlags & JSON_THROW_ON_ERROR ){` |
|  ! 0 | 1097 | `			pCtx->pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 | 1098 | `			return PH7_VmThrowException(pCtx,"JsonException","%s",` |
|  ! 0 | 1099 | `				JsonErrorMsg(JSON_ERROR_SYNTAX));` |
|    - | 1100 | `		}` |
|    6 | 1101 | `		ph7_result_null(pCtx);` |
|    6 | 1102 | `		return PH7_OK;` |
|    - | 1103 | `	}` |
|   42 | 1104 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){` |
|   28 | 1105 | `		iAssoc = 1;` |
|   13 | 1106 | `	}` |
|   42 | 1107 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|    - | 1108 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise);` |
|    - | 1109 | `		 * read as int64 so a value above INT_MAX is detected, not truncated. */` |
|   17 | 1110 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[2]);` |
|    - | 1111 | `		/* php clears the json error state before validating $depth, so a caught` |
|    - | 1112 | `		 * depth ValueError leaves json_last_error() == JSON_ERROR_NONE (the normal` |
|    - | 1113 | `		 * path resets it again inside VmJsonDecodeInput). */` |
|   17 | 1114 | `		pCtx->pVm->json_rc = JSON_ERROR_NONE;` |
|   17 | 1115 | `		if( nWant <= 0 ){` |
|    9 | 1116 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1117 | `				"json_decode(): Argument #3 ($depth) must be greater than 0");` |
|    - | 1118 | `		}` |
|    9 | 1119 | `		if( nWant > 2147483647 ){` |
|    3 | 1120 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1121 | `				"json_decode(): Argument #3 ($depth) must be less than 2147483647");` |
|    - | 1122 | `		}` |
|    7 | 1123 | `		nDepth = (int)nWant;` |
|    3 | 1124 | `	}` |
|    - | 1125 | `	/* Decode the raw JSON input.The default consumer sets the decoded value as the` |
|    - | 1126 | `	 * call-context result; on failure we replace it with NULL (or throw). */` |
|   32 | 1127 | `	if( VmJsonDecodeInput(pCtx,zIn,nByte,iAssoc,nDepth) != JSON_ERROR_NONE ){` |
|    - | 1128 | `		/* Something goes wrong while decoding JSON input. */` |
|   16 | 1129 | `		if( iFlags & JSON_THROW_ON_ERROR ){` |
|    - | 1130 | `			/* php: raise a JsonException carrying json_last_error_msg() text. */` |
|    4 | 1131 | `			return PH7_VmThrowException(pCtx,"JsonException","%s",` |
|    2 | 1132 | `				JsonErrorMsg(pCtx->pVm->json_rc));` |
|    - | 1133 | `		}` |
|   14 | 1134 | `		ph7_result_null(pCtx);` |
|    6 | 1135 | `	}` |
|    - | 1136 | `	/* All done */` |
|   30 | 1137 | `	return PH7_OK;` |
|   24 | 1138 | `}` |
|    - | 1139 | `/*` |
|    - | 1140 | ` * bool json_validate(string $json[,int $depth = 512[,int $flags = 0]])` |
|    - | 1141 | ` *  Validates whether a string is valid JSON without materializing a value.` |
|    - | 1142 | ` * Parameters` |
|    - | 1143 | ` *  $json   The string to validate.` |
|    - | 1144 | ` *  $depth  Maximum nesting depth (clamped to the engine limit of 32).` |
|    - | 1145 | ` *  $flags  Bitmask of decode options (currently none are implemented; accepted/ignored).` |
|    - | 1146 | ` * Return` |
|    - | 1147 | ` *  TRUE if the string is valid JSON, FALSE otherwise. Updates json_last_error().` |
|    - | 1148 | ` */` |
|   20 | 1149 | `PH7_PRIVATE int vm_builtin_json_validate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1150 | `{` |
|   21 | 1151 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - | 1152 | `	const char *zIn;` |
|    - | 1153 | `	int nByte;` |
|   21 | 1154 | `	int nDepth = 32;` |
|   21 | 1155 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - | 1156 | `		/* Missing/Invalid argument: not valid JSON */` |
|  ! 0 | 1157 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 | 1158 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1159 | `		return PH7_OK;` |
|    - | 1160 | `	}` |
|    - | 1161 | `	/* Extract the JSON string */` |
|   21 | 1162 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   21 | 1163 | `	if( nByte < 1 ){` |
|    - | 1164 | `		/* The empty string is not valid JSON (unlike json_decode, which returns NULL` |
|    - | 1165 | `		 * silently, json_validate must record the syntax error) */` |
|    3 | 1166 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    3 | 1167 | `		ph7_result_bool(pCtx,0);` |
|    3 | 1168 | `		return PH7_OK;` |
|    - | 1169 | `	}` |
|   19 | 1170 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|    - | 1171 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise). */` |
|    9 | 1172 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[1]);` |
|    - | 1173 | `		/* Clear the json error state before validating $depth (php parity), so a` |
|    - | 1174 | `		 * caught depth ValueError leaves json_last_error() == JSON_ERROR_NONE. */` |
|    9 | 1175 | `		pVm->json_rc = JSON_ERROR_NONE;` |
|    9 | 1176 | `		if( nWant <= 0 ){` |
|    5 | 1177 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1178 | `				"json_validate(): Argument #2 ($depth) must be greater than 0");` |
|    - | 1179 | `		}` |
|    5 | 1180 | `		if( nWant > 2147483647 ){` |
|    3 | 1181 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1182 | `				"json_validate(): Argument #2 ($depth) must be less than 2147483647");` |
|    - | 1183 | `		}` |
|    3 | 1184 | `		nDepth = (int)nWant;` |
|    1 | 1185 | `	}` |
|    - | 1186 | `	/* apArg[2] ($flags) is accepted and ignored: no decode flag is implemented.` |
|    - | 1187 | `	 * Decode in associative mode so the "objects are returned as an array" warning is` |
|    - | 1188 | `	 * not raised - the decoded value is discarded, only its validity matters. */` |
|   13 | 1189 | `	ph7_result_bool(pCtx,VmJsonDecodeInput(pCtx,zIn,nByte,1,nDepth) == JSON_ERROR_NONE);` |
|   13 | 1190 | `	return PH7_OK;` |
|   11 | 1191 | `}` |
|    - | 1192 |  |
