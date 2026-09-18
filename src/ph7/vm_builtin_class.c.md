# src/ph7/vm_builtin_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 651/739 lines (88.09%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|   878 |    7 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |    8 | `{` |
|     - |    9 | `	ph7_class *pClass;` |
|     - |   10 | `	SyString *pName;` |
|   883 |   11 | `	if( nArg < 1 ){` |
|     - |   12 | `		/* Check if we are inside a class */` |
|   ! 0 |   13 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|   ! 0 |   14 | `		if( pClass ){` |
|     - |   15 | `			/* Point to the class name */` |
|   ! 0 |   16 | `			pName = &pClass->sName;` |
|   ! 0 |   17 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|   ! 0 |   18 | `		}else{` |
|     - |   19 | `			/* Not inside class,return FALSE */` |
|   ! 0 |   20 | `			ph7_result_bool(pCtx,0);` |
|     - |   21 | `		}` |
|   ! 0 |   22 | `	}else{` |
|     - |   23 | `		/* Extract the target class */` |
|   883 |   24 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|   883 |   25 | `		if( pClass ){` |
|   883 |   26 | `			pName = &pClass->sName;` |
|     - |   27 | `			/* Return the class name */` |
|   883 |   28 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|   444 |   29 | `		}else{` |
|     - |   30 | `			/* Not a class instance,return FALSE */` |
|   ! 0 |   31 | `			ph7_result_bool(pCtx,0);` |
|     - |   32 | `		}` |
|     - |   33 | `	}` |
|   883 |   34 | `	return PH7_OK;` |
|     5 |   35 | `}` |
|     - |   36 | `/*` |
|     - |   37 | ` * string get_parent_class([object $object = NULL ] )` |
|     - |   38 | ` *   Returns the name of the parent class of an object` |
|     - |   39 | ` * Parameters` |
|     - |   40 | ` *  object` |
|     - |   41 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|     - |   42 | ` * Return` |
|     - |   43 | ` *  The name of the parent class of which object is an instance.` |
|     - |   44 | ` *  Returns FALSE if object is not an object or if the object does` |
|     - |   45 | ` *  not have a parent.` |
|     - |   46 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|     - |   47 | ` */` |
|    46 |   48 | `PH7_PRIVATE int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |   49 | `{` |
|     - |   50 | `	ph7_class *pClass;` |
|     - |   51 | `	SyString *pName;` |
|    48 |   52 | `	if( nArg < 1 ){` |
|     - |   53 | `		/* Check if we are inside a class [i.e: a method call]*/` |
|     3 |   54 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|     3 |   55 | `		if( pClass && pClass->pBase ){` |
|     - |   56 | `			/* Point to the class name */` |
|     3 |   57 | `			pName = &pClass->pBase->sName;` |
|     3 |   58 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|     2 |   59 | `		}else{` |
|     - |   60 | `			/* Not inside class,return FALSE */` |
|   ! 0 |   61 | `			ph7_result_bool(pCtx,0);` |
|     - |   62 | `		}` |
|     2 |   63 | `	}else{` |
|     - |   64 | `		/* Extract the target class */` |
|    46 |   65 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    46 |   66 | `		if( pClass ){` |
|    46 |   67 | `			if( pClass->pBase ){` |
|    44 |   68 | `				pName = &pClass->pBase->sName;` |
|     - |   69 | `				/* Return the parent class name */` |
|    44 |   70 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|    23 |   71 | `			}else{` |
|     - |   72 | `				/* Object does not have a parent class */` |
|     3 |   73 | `				ph7_result_bool(pCtx,0);` |
|     - |   74 | `			}` |
|    24 |   75 | `		}else{` |
|     - |   76 | `			/* Not a class instance,return FALSE */` |
|   ! 0 |   77 | `			ph7_result_bool(pCtx,0);` |
|     - |   78 | `		}` |
|     - |   79 | `	}` |
|    48 |   80 | `	return PH7_OK;` |
|     2 |   81 | `}` |
|     - |   82 | `/*` |
|     - |   83 | ` * string get_called_class(void)` |
|     - |   84 | ` *   Gets the name of the class the static method is called in.` |
|     - |   85 | ` * Parameters` |
|     - |   86 | ` *  None.` |
|     - |   87 | ` * Return` |
|     - |   88 | ` *  Returns the class name. Returns FALSE if called from outside a class.` |
|     - |   89 | ` */` |
|     4 |   90 | `PH7_PRIVATE int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |   91 | `{` |
|     - |   92 | `	ph7_class *pClass;` |
|     - |   93 | `	/* Check if we are inside a class [i.e: a method call] */` |
|     5 |   94 | `	pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|     5 |   95 | `	if( pClass ){` |
|     - |   96 | `		SyString *pName;` |
|     - |   97 | `		/* Point to the class name */` |
|     5 |   98 | `		pName = &pClass->sName;` |
|     5 |   99 | `		ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|     3 |  100 | `	}else{` |
|   ! 0 |  101 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  102 | `		SXUNUSED(apArg);` |
|     - |  103 | `		/* Not inside class,return FALSE */` |
|   ! 0 |  104 | `		ph7_result_bool(pCtx,0);` |
|     - |  105 | `	}` |
|     5 |  106 | `	return PH7_OK;` |
|     1 |  107 | `}` |
|     - |  108 | `/*` |
|     - |  109 | ` * Extract a ph7_class from the given ph7_value.` |
|     - |  110 | ` * The given value must be of type object [i.e: class instance] or` |
|     - |  111 | ` * string which hold the class name.` |
|     - |  112 | ` */` |
|  2032 |  113 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|     5 |  114 | `{` |
|  2037 |  115 | `	ph7_class *pClass = 0;` |
|  2037 |  116 | `	if( ph7_value_is_object(pArg) ){` |
|     - |  117 | `		/* Class instance already loaded,no need to perform a lookup */` |
|  1047 |  118 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|  1515 |  119 | `	}else if( ph7_value_is_string(pArg) ){` |
|     - |  120 | `		const char *zClass;` |
|     - |  121 | `		int nLen;` |
|     - |  122 | `		/* Extract class name */` |
|   994 |  123 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|     - |  124 | `		/* A leading '\' (the global-namespace anchor) is stripped by` |
|     - |  125 | `		 * PH7_VmExtractClass itself now (PH7_VmClassNameAnchor), so do not` |
|     - |  126 | `		 * strip here too — a second strip would wrongly resolve "\\Foo". */` |
|   994 |  127 | `		if( nLen > 0 ){` |
|     - |  128 | `			/* Resolve through PH7_VmExtractClass so a class named by STRING is` |
|     - |  129 | `			 * autoloaded on a miss — php autoloads the class of a [class,method]` |
|     - |  130 | `			 * callable (is_callable/array_map/call_user_func), and of the class` |
|     - |  131 | `			 * argument to method_exists()/property_exists(). iLoadable=FALSE keeps` |
|     - |  132 | `			 * abstract classes and interfaces (a static method on an abstract class` |
|     - |  133 | `			 * is a valid callable). */` |
|   994 |  134 | `			pClass = PH7_VmExtractClass(pVm,zClass,(sxu32)nLen,FALSE,0);` |
|   495 |  135 | `		}` |
|   495 |  136 | `	}` |
|  2037 |  137 | `	return pClass;` |
|     5 |  138 | `}` |
|     - |  139 | `/*` |
|     - |  140 | ` * bool property_exists(mixed $class,string $property)` |
|     - |  141 | ` *   Checks if the object or class has a property.` |
|     - |  142 | ` * Parameters` |
|     - |  143 | ` *  class` |
|     - |  144 | ` *   The class name or an object of the class to test for` |
|     - |  145 | ` * property` |
|     - |  146 | ` *  The name of the property` |
|     - |  147 | ` * Return` |
|     - |  148 | ` *   Returns TRUE if the property exists,FALSE otherwise.` |
|     - |  149 | ` */` |
|    18 |  150 | `PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  151 | `{` |
|    20 |  152 | `	int res = 0; /* Assume attribute does not exists */` |
|    20 |  153 | `	if( nArg > 1 ){` |
|     - |  154 | `		ph7_class *pClass;` |
|    20 |  155 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    20 |  156 | `		if( pClass ){` |
|     - |  157 | `			const char *zName;` |
|     - |  158 | `			int nLen;` |
|     - |  159 | `			/* Extract attribute name */` |
|    20 |  160 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|    20 |  161 | `			if( nLen > 0 ){` |
|     - |  162 | `				/* Perform the lookup in the attribute and method table */` |
|    18 |  163 | `				if( SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen) != 0` |
|    14 |  164 | `					\|\| SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|     - |  165 | `						/* property exists,flag that */` |
|    13 |  166 | `						res = 1;` |
|     6 |  167 | `				}` |
|     - |  168 | `				/* A DYNAMIC (runtime-added) property lives on the INSTANCE's` |
|     - |  169 | `				 * attribute table, not the class's — php reports those too` |
|     - |  170 | `				 * (band A #3b; pre-fix property_exists() was blind to them). */` |
|    20 |  171 | `				if( res == 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|     3 |  172 | `					ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     3 |  173 | `					if( pThis && SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nLen) != 0 ){` |
|   ! 0 |  174 | `						res = 1;` |
|   ! 0 |  175 | `					}` |
|     1 |  176 | `				}` |
|     9 |  177 | `			}` |
|     9 |  178 | `		}` |
|     9 |  179 | `	}` |
|    20 |  180 | `	ph7_result_bool(pCtx,res);` |
|    20 |  181 | `	return PH7_OK;` |
|     2 |  182 | `}` |
|     - |  183 | `/*` |
|     - |  184 | ` * bool method_exists(mixed $class,string $method)` |
|     - |  185 | ` *   Checks if the given method is a class member.` |
|     - |  186 | ` * Parameters` |
|     - |  187 | ` *  class` |
|     - |  188 | ` *   The class name or an object of the class to test for` |
|     - |  189 | ` * property` |
|     - |  190 | ` *  The name of the method` |
|     - |  191 | ` * Return` |
|     - |  192 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|     - |  193 | ` */` |
|    14 |  194 | `PH7_PRIVATE int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  195 | `{` |
|    16 |  196 | `	int res = 0; /* Assume method does not exists */` |
|    16 |  197 | `	if( nArg > 1 ){` |
|     - |  198 | `		ph7_class *pClass;` |
|    16 |  199 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    16 |  200 | `		if( pClass ){` |
|     - |  201 | `			const char *zName;` |
|     - |  202 | `			int nLen;` |
|     - |  203 | `			/* Extract method name */` |
|    12 |  204 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|    12 |  205 | `			if( nLen > 0 ){` |
|     - |  206 | `				/* Perform the lookup in the method table */` |
|    12 |  207 | `				if( SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|     - |  208 | `					/* method exists,flag that */` |
|    10 |  209 | `					res = 1;` |
|     4 |  210 | `				}` |
|     5 |  211 | `			}` |
|     5 |  212 | `		}` |
|     7 |  213 | `	}` |
|    16 |  214 | `	ph7_result_bool(pCtx,res);` |
|    16 |  215 | `	return PH7_OK;` |
|     2 |  216 | `}` |
|     - |  217 | `/*` |
|     - |  218 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|     - |  219 | ` *   Checks if the class has been defined.` |
|     - |  220 | ` * Parameters` |
|     - |  221 | ` *  class_name` |
|     - |  222 | ` *   The class name. The name is matched in a case-sensitive manner` |
|     - |  223 | ` *   unlinke the standard PHP engine.` |
|     - |  224 | ` *  autoload` |
|     - |  225 | ` *   Whether or not to call __autoload by default.` |
|     - |  226 | ` * Return` |
|     - |  227 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|     - |  228 | ` */` |
|    96 |  229 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  230 | `{` |
|   101 |  231 | `	int res = 0; /* Assume class does not exist */` |
|   101 |  232 | `	if( nArg > 0 ){` |
|   101 |  233 | `		SyHashEntry *pEntry = 0;` |
|     - |  234 | `		const char *zName;` |
|     - |  235 | `		int nLen;` |
|   101 |  236 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|     - |  237 | `		sxu32 nName;` |
|     - |  238 | `		/* Extract given name */` |
|   101 |  239 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|   101 |  240 | `		if( nArg >= 2 ){` |
|     6 |  241 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|     2 |  242 | `		}` |
|     - |  243 | `		/* Strip a leading '\' (global-namespace anchor) — this builtin hashes` |
|     - |  244 | `		 * hClass directly, bypassing PH7_VmExtractClass, so anchor here. */` |
|   101 |  245 | `		nName = (sxu32)nLen;` |
|   101 |  246 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|   101 |  247 | `		if( nName > 0 ){` |
|     - |  248 | `			/* Perform a hash lookup first */` |
|    97 |  249 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|    46 |  250 | `		}` |
|     - |  251 | `		/* Gate autoload on the ORIGINAL length (nLen), not the stripped nName:` |
|     - |  252 | `		 * php autoloads a lone "\" (with the empty stripped name) but not "". */` |
|   101 |  253 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|     - |  254 | `			/* Try autoload, then re-check */` |
|    29 |  255 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|    29 |  256 | `			if( pClass ){` |
|     9 |  257 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|     3 |  258 | `			}` |
|    12 |  259 | `		}` |
|   101 |  260 | `		if( pEntry ){` |
|     - |  261 | `			/* Walk the collision chain: return TRUE only for concrete or abstract classes,` |
|     - |  262 | `			 * not for interfaces or traits (matching PHP behavior). */` |
|    79 |  263 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    81 |  264 | `			while( pClass ){` |
|    79 |  265 | `				if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|    77 |  266 | `					res = 1;` |
|    77 |  267 | `					break;` |
|     - |  268 | `				}` |
|     3 |  269 | `				pClass = pClass->pNextName;` |
|     1 |  270 | `			}` |
|    37 |  271 | `		}` |
|    48 |  272 | `	}` |
|   101 |  273 | `	ph7_result_bool(pCtx,res);` |
|   101 |  274 | `	return PH7_OK;` |
|     5 |  275 | `}` |
|     - |  276 | `/*` |
|     - |  277 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|     - |  278 | ` *   Checks if the interface has been defined.` |
|     - |  279 | ` * Parameters` |
|     - |  280 | ` *  class_name` |
|     - |  281 | ` *   The class name. The name is matched in a case-sensitive manner` |
|     - |  282 | ` *   unlinke the standard PHP engine.` |
|     - |  283 | ` *  autoload` |
|     - |  284 | ` *   Whether or not to call __autoload by default.` |
|     - |  285 | ` * Return` |
|     - |  286 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|     - |  287 | ` */` |
|    30 |  288 | `PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  289 | `{` |
|    32 |  290 | `	int res = 0; /* Assume interface does not exist */` |
|    32 |  291 | `	if( nArg > 0 ){` |
|    32 |  292 | `		SyHashEntry *pEntry = 0;` |
|     - |  293 | `		const char *zName;` |
|     - |  294 | `		int nLen;` |
|    32 |  295 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|     - |  296 | `		sxu32 nName;` |
|     - |  297 | `		/* Extract given name */` |
|    32 |  298 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    32 |  299 | `		if( nArg >= 2 ){` |
|   ! 0 |  300 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|   ! 0 |  301 | `		}` |
|     - |  302 | `		/* Strip a leading '\' (global-namespace anchor); this builtin bypasses` |
|     - |  303 | `		 * PH7_VmExtractClass and hashes hClass directly. */` |
|    32 |  304 | `		nName = (sxu32)nLen;` |
|    32 |  305 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|     - |  306 | `		/* Perform a hash lookup */` |
|    32 |  307 | `		if( nName > 0 ){` |
|    32 |  308 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|    15 |  309 | `		}` |
|     - |  310 | `		/* Gate autoload on the ORIGINAL length (nLen): php autoloads a lone` |
|     - |  311 | `		 * "\" with the empty stripped name, but not a truly empty "". */` |
|    32 |  312 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|     - |  313 | `			/* Try autoload — pass iLoadable=FALSE so we get interfaces too */` |
|     3 |  314 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|     3 |  315 | `			if( pClass ){` |
|   ! 0 |  316 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|   ! 0 |  317 | `			}` |
|     1 |  318 | `		}` |
|    32 |  319 | `		if( pEntry ){` |
|    30 |  320 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    32 |  321 | `			while( pClass ){` |
|    30 |  322 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     - |  323 | `					/* interface is available */` |
|    28 |  324 | `					res = 1;` |
|    28 |  325 | `					break;` |
|     - |  326 | `				}` |
|     - |  327 | `				/* Next with the same name */` |
|     3 |  328 | `				pClass = pClass->pNextName;` |
|     1 |  329 | `			}` |
|    14 |  330 | `		}` |
|    15 |  331 | `	}` |
|    32 |  332 | `	ph7_result_bool(pCtx,res);` |
|    32 |  333 | `	return PH7_OK;` |
|     2 |  334 | `}` |
|     - |  335 | `/*` |
|     - |  336 | ` * bool trait_exists(string $trait [, bool $autoload = true ] )` |
|     - |  337 | ` *   Checks if the trait has been defined.` |
|     - |  338 | ` * Parameters` |
|     - |  339 | ` *  trait` |
|     - |  340 | ` *   The trait name (case-sensitive here, unlike the standard PHP engine).` |
|     - |  341 | ` *  autoload` |
|     - |  342 | ` *   Whether to invoke autoloading if the trait is not yet defined.` |
|     - |  343 | ` * Return` |
|     - |  344 | ` *   TRUE if trait is a defined trait, FALSE otherwise.` |
|     - |  345 | ` */` |
|    14 |  346 | `PH7_PRIVATE int vm_builtin_trait_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  347 | `{` |
|    16 |  348 | `	int res = 0; /* Assume trait does not exist */` |
|    16 |  349 | `	if( nArg > 0 ){` |
|    16 |  350 | `		SyHashEntry *pEntry = 0;` |
|     - |  351 | `		const char *zName;` |
|     - |  352 | `		int nLen;` |
|    16 |  353 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|     - |  354 | `		sxu32 nName;` |
|     - |  355 | `		/* Extract given name */` |
|    16 |  356 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    16 |  357 | `		if( nArg >= 2 ){` |
|     3 |  358 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|     1 |  359 | `		}` |
|     - |  360 | `		/* Strip a leading '\' (global-namespace anchor); this builtin bypasses` |
|     - |  361 | `		 * PH7_VmExtractClass and hashes hClass directly. */` |
|    16 |  362 | `		nName = (sxu32)nLen;` |
|    16 |  363 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|     - |  364 | `		/* Perform a hash lookup */` |
|    16 |  365 | `		if( nName > 0 ){` |
|    16 |  366 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|     7 |  367 | `		}` |
|     - |  368 | `		/* Gate autoload on the ORIGINAL length (nLen): php autoloads a lone` |
|     - |  369 | `		 * "\" with the empty stripped name, but not a truly empty "". */` |
|    16 |  370 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|     - |  371 | `			/* Try autoload — pass iLoadable=FALSE so we get traits too */` |
|     3 |  372 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|     3 |  373 | `			if( pClass ){` |
|   ! 0 |  374 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|   ! 0 |  375 | `			}` |
|     1 |  376 | `		}` |
|    16 |  377 | `		if( pEntry ){` |
|    12 |  378 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    18 |  379 | `			while( pClass ){` |
|    12 |  380 | `				if( pClass->iFlags & PH7_CLASS_TRAIT ){` |
|     - |  381 | `					/* trait is available */` |
|     6 |  382 | `					res = 1;` |
|     6 |  383 | `					break;` |
|     - |  384 | `				}` |
|     - |  385 | `				/* Next with the same name */` |
|     7 |  386 | `				pClass = pClass->pNextName;` |
|     1 |  387 | `			}` |
|     5 |  388 | `		}` |
|     7 |  389 | `	}` |
|    16 |  390 | `	ph7_result_bool(pCtx,res);` |
|    16 |  391 | `	return PH7_OK;` |
|     2 |  392 | `}` |
|     - |  393 | `/*` |
|     - |  394 | ` * bool class_alias([string $original[,string $alias ]])` |
|     - |  395 | ` *   Creates an alias for a class.` |
|     - |  396 | ` * Parameters` |
|     - |  397 | ` *  original` |
|     - |  398 | ` *    The original class.` |
|     - |  399 | ` *  alias` |
|     - |  400 | ` *   The alias name for the class.` |
|     - |  401 | ` * Return` |
|     - |  402 | ` *   Returns TRUE on success or FALSE on failure.` |
|     - |  403 | ` */` |
|     4 |  404 | `PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  405 | `{` |
|     - |  406 | `	const char *zOld,*zNew;` |
|     - |  407 | `	int nOldLen,nNewLen;` |
|     - |  408 | `	sxu32 nOld,nNew;` |
|     - |  409 | `	SyHashEntry *pEntry;` |
|     - |  410 | `	ph7_class *pClass;` |
|     - |  411 | `	char *zDup;` |
|     - |  412 | `	sxi32 rc;` |
|     6 |  413 | `	if( nArg < 2 ){` |
|     - |  414 | `		/* Missing arguments,return FALSE */` |
|   ! 0 |  415 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  416 | `		return PH7_OK;` |
|     - |  417 | `	}` |
|     - |  418 | `	/* Extract old class name */` |
|     6 |  419 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|     - |  420 | `	/* Extract alias name */` |
|     6 |  421 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|     - |  422 | `	/* Strip a leading '\' (global-namespace anchor) from BOTH names: php` |
|     - |  423 | `	 * resolves the target and stores the alias without it, so class_exists()` |
|     - |  424 | `	 * on the plain name then matches. */` |
|     6 |  425 | `	nOld = (sxu32)nOldLen;` |
|     6 |  426 | `	nNew = (sxu32)nNewLen;` |
|     6 |  427 | `	PH7_VmClassNameAnchor(&zOld,&nOld);` |
|     6 |  428 | `	PH7_VmClassNameAnchor(&zNew,&nNew);` |
|     6 |  429 | `	if( nNew < 1 ){` |
|     - |  430 | `		/* Invalid alias name,return FALSE */` |
|   ! 0 |  431 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  432 | `		return PH7_OK;` |
|     - |  433 | `	}` |
|     - |  434 | `	/* Perform a hash lookup */` |
|     6 |  435 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,nOld);` |
|     6 |  436 | `	if( pEntry ==  0 ){` |
|     - |  437 | `		/* No such class,return FALSE */` |
|   ! 0 |  438 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  439 | `		return PH7_OK;` |
|     - |  440 | `	}` |
|     - |  441 | `	/* Point to the class */` |
|     6 |  442 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  443 | `	/* Duplicate alias name */` |
|     6 |  444 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,nNew);` |
|     6 |  445 | `	if( zDup == 0 ){` |
|     - |  446 | `		/* Out of memory,return FALSE */` |
|   ! 0 |  447 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  448 | `		return PH7_OK;` |
|     - |  449 | `	}` |
|     - |  450 | `	/* Create the alias */` |
|     6 |  451 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,nNew,pClass);` |
|     6 |  452 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  453 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|   ! 0 |  454 | `	}` |
|     6 |  455 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     6 |  456 | `	return PH7_OK;` |
|     4 |  457 | `}` |
|     - |  458 | `/*` |
|     - |  459 | ` * array get_declared_classes(void)` |
|     - |  460 | ` *   Returns an array with the name of the defined classes` |
|     - |  461 | ` * Parameters` |
|     - |  462 | ` *  None` |
|     - |  463 | ` * Return` |
|     - |  464 | ` *   Returns an array of the names of the declared classes` |
|     - |  465 | ` *   in the current script.` |
|     - |  466 | ` * Note:` |
|     - |  467 | ` *   NULL is returned on failure.` |
|     - |  468 | ` */` |
|     2 |  469 | `PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  470 | `{` |
|     - |  471 | `	ph7_value *pName,*pArray;` |
|     - |  472 | `	SyHashEntry *pEntry;` |
|     - |  473 | `	/* Create a new array first */` |
|     3 |  474 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  475 | `	pName = ph7_context_new_scalar(pCtx);` |
|     3 |  476 | `	if( pArray == 0 \|\| pName == 0){` |
|   ! 0 |  477 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  478 | `		SXUNUSED(apArg);` |
|     - |  479 | `		/* Out of memory,return NULL */` |
|   ! 0 |  480 | `		ph7_result_null(pCtx);` |
|   ! 0 |  481 | `		return PH7_OK;` |
|     - |  482 | `	}` |
|     - |  483 | `	/* Fill the array with the defined classes */` |
|     3 |  484 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|   384 |  485 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   381 |  486 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  487 | `		/* Do not register classes defined as interfaces */` |
|   381 |  488 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|   343 |  489 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|     - |  490 | `			/* insert class name */` |
|   343 |  491 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  492 | `			/* Reset the cursor */` |
|   343 |  493 | `			ph7_value_reset_string_cursor(pName);` |
|   171 |  494 | `		}` |
|     1 |  495 | `	}` |
|     - |  496 | `	/* Return the created array */` |
|     3 |  497 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  498 | `	return PH7_OK;` |
|     2 |  499 | `}` |
|     - |  500 | `/*` |
|     - |  501 | ` * array get_declared_interfaces(void)` |
|     - |  502 | ` *   Returns an array with the name of the defined interfaces` |
|     - |  503 | ` * Parameters` |
|     - |  504 | ` *  None` |
|     - |  505 | ` * Return` |
|     - |  506 | ` *   Returns an array of the names of the declared interfaces` |
|     - |  507 | ` *   in the current script.` |
|     - |  508 | ` * Note:` |
|     - |  509 | ` *   NULL is returned on failure.` |
|     - |  510 | ` */` |
|     2 |  511 | `PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  512 | `{` |
|     - |  513 | `	ph7_value *pName,*pArray;` |
|     - |  514 | `	SyHashEntry *pEntry;` |
|     - |  515 | `	/* Create a new array first */` |
|     3 |  516 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  517 | `	pName = ph7_context_new_scalar(pCtx);` |
|     3 |  518 | `	if( pArray == 0 \|\| pName == 0 ){` |
|   ! 0 |  519 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  520 | `		SXUNUSED(apArg);` |
|     - |  521 | `		/* Out of memory,return NULL */` |
|   ! 0 |  522 | `		ph7_result_null(pCtx);` |
|   ! 0 |  523 | `		return PH7_OK;` |
|     - |  524 | `	}` |
|     - |  525 | `	/* Fill the array with the defined classes */` |
|     3 |  526 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|   386 |  527 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   383 |  528 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  529 | `		/* Register classes defined as interfaces only */` |
|   383 |  530 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|    41 |  531 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|     - |  532 | `			/* insert interface name */` |
|    41 |  533 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  534 | `			/* Reset the cursor */` |
|    41 |  535 | `			ph7_value_reset_string_cursor(pName);` |
|    20 |  536 | `		}` |
|     1 |  537 | `	}` |
|     - |  538 | `	/* Return the created array */` |
|     3 |  539 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  540 | `	return PH7_OK;` |
|     2 |  541 | `}` |
|     - |  542 | `/*` |
|     - |  543 | ` * array get_class_methods(string/object $class_name)` |
|     - |  544 | ` *   Returns an array with the name of the class methods` |
|     - |  545 | ` * Parameters` |
|     - |  546 | ` *  class_name` |
|     - |  547 | ` *  The class name or class instance` |
|     - |  548 | ` * Return` |
|     - |  549 | ` *  Returns an array of method names defined for the class specified by class_name.` |
|     - |  550 | ` *  In case of an error, it returns NULL.` |
|     - |  551 | ` * Note:` |
|     - |  552 | ` *   NULL is returned on failure.` |
|     - |  553 | ` */` |
|     8 |  554 | `PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  555 | `{` |
|     - |  556 | `	ph7_value *pName,*pArray;` |
|     - |  557 | `	SyHashEntry *pEntry;` |
|     - |  558 | `	ph7_class *pClass;` |
|     - |  559 | `	/* Extract the target class first */` |
|     9 |  560 | `	pClass = 0;` |
|     9 |  561 | `	if( nArg > 0 ){` |
|     9 |  562 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     4 |  563 | `	}` |
|     9 |  564 | `	if( pClass == 0 ){` |
|     - |  565 | `		/* No such class,return NULL */` |
|     3 |  566 | `		ph7_result_null(pCtx);` |
|     3 |  567 | `		return PH7_OK;` |
|     - |  568 | `	}` |
|     - |  569 | `	/* Create a new array  */` |
|     7 |  570 | `	pArray = ph7_context_new_array(pCtx);` |
|     7 |  571 | `	pName = ph7_context_new_scalar(pCtx);` |
|     7 |  572 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  573 | `		/* Out of memory,return NULL */` |
|   ! 0 |  574 | `		ph7_result_null(pCtx);` |
|   ! 0 |  575 | `		return PH7_OK;` |
|     - |  576 | `	}` |
|     - |  577 | `	/* Fill the array with the defined methods, in php's order: the class's own` |
|     - |  578 | `	 * methods in DECLARATION order, then each ancestor's in ITS declaration` |
|     - |  579 | `	 * order (band A #4 — the raw hash walk returned reverse-insertion/LIFO` |
|     - |  580 | `	 * order). SyHash iterates newest-first, so a reversed walk restores` |
|     - |  581 | `	 * insertion order; grouping by declaring class (sFunc.pUserData, the class` |
|     - |  582 | `	 * a method was compiled into) walks own-then-parent like php. An override` |
|     - |  583 | `	 * lives once in the hash under the subclass, so no dedup is needed. */` |
|     - |  584 | `	{` |
|     - |  585 | `		SySet aTmp;` |
|     - |  586 | `		SyHashEntry **apEntry;` |
|     - |  587 | `		ph7_class *pLevel;` |
|     - |  588 | `		sxu32 n;` |
|     7 |  589 | `		SySetInit(&aTmp,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|     7 |  590 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|    27 |  591 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|    21 |  592 | `			SySetPut(&aTmp,(const void *)&pEntry);` |
|     1 |  593 | `		}` |
|     7 |  594 | `		apEntry = (SyHashEntry **)SySetBasePtr(&aTmp);` |
|    15 |  595 | `		for( pLevel = pClass; pLevel; pLevel = pLevel->pBase ){` |
|     - |  596 | `			/* Collect this level's methods, then emit in DECLARATION order` |
|     - |  597 | `			 * (sorted by nLine — same-level methods share a source file; a` |
|     - |  598 | `			 * hash-order fallback covers line-less internal methods). */` |
|     - |  599 | `			SySet aLvl;` |
|     - |  600 | `			ph7_class_method **apLvl;` |
|     - |  601 | `			sxu32 i,j;` |
|     9 |  602 | `			SySetInit(&aLvl,&pCtx->pVm->sAllocator,sizeof(ph7_class_method *));` |
|     - |  603 | `			/* Hash-order fallback for same-line methods: the class's OWN entries` |
|     - |  604 | `			 * come out in declaration order when walked newest-first, while` |
|     - |  605 | `			 * inherited copies (inserted by PH7_ClassInherit's walk of the base` |
|     - |  606 | `			 * hash) come out in declaration order walked oldest-first. */` |
|    37 |  607 | `			for( n = 0; n < SySetUsed(&aTmp); n++ ){` |
|    29 |  608 | `				sxu32 nPick = (pLevel == pClass) ? (SySetUsed(&aTmp) - 1 - n) : n;` |
|    29 |  609 | `				ph7_class_method *pMethod = (ph7_class_method *)apEntry[nPick]->pUserData;` |
|    29 |  610 | `				ph7_class *pDecl = (ph7_class *)pMethod->sFunc.pUserData;` |
|    29 |  611 | `				if( pDecl != pLevel ){` |
|     - |  612 | `					/* A declarer outside the base chain (a used trait, or none)` |
|     - |  613 | `					 * counts as the class's own level, like php. */` |
|     - |  614 | `					ph7_class *pWalk;` |
|     9 |  615 | `					if( pLevel != pClass \|\| pDecl == pClass ){` |
|     7 |  616 | `						continue;` |
|     - |  617 | `					}` |
|     9 |  618 | `					for( pWalk = pClass; pWalk; pWalk = pWalk->pBase ){` |
|     9 |  619 | `						if( pWalk == pDecl ){` |
|     5 |  620 | `							break;` |
|     - |  621 | `						}` |
|     3 |  622 | `					}` |
|     5 |  623 | `					if( pWalk != 0 ){` |
|     5 |  624 | `						continue; /* in-chain: its own level emits it */` |
|     - |  625 | `					}` |
|   ! 0 |  626 | `				}` |
|    21 |  627 | `				SySetPut(&aLvl,(const void *)&pMethod);` |
|    11 |  628 | `			}` |
|     9 |  629 | `			apLvl = (ph7_class_method **)SySetBasePtr(&aLvl);` |
|     - |  630 | `			/* Insertion sort by declaration line (stable) */` |
|    21 |  631 | `			for( i = 1; i < SySetUsed(&aLvl); i++ ){` |
|    13 |  632 | `				ph7_class_method *pKey = apLvl[i];` |
|    13 |  633 | `				for( j = i; j > 0 && apLvl[j-1]->nLine > pKey->nLine; j-- ){` |
|   ! 0 |  634 | `					apLvl[j] = apLvl[j-1];` |
|   ! 0 |  635 | `				}` |
|    13 |  636 | `				apLvl[j] = pKey;` |
|     7 |  637 | `			}` |
|    29 |  638 | `			for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|     - |  639 | `				/* Insert method name */` |
|    21 |  640 | `				ph7_value_string(pName,SyStringData(&apLvl[i]->sFunc.sName),(int)SyStringLength(&apLvl[i]->sFunc.sName));` |
|    21 |  641 | `				ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  642 | `				/* Reset the cursor */` |
|    21 |  643 | `				ph7_value_reset_string_cursor(pName);` |
|    11 |  644 | `			}` |
|     9 |  645 | `			SySetRelease(&aLvl);` |
|     5 |  646 | `		}` |
|     7 |  647 | `		SySetRelease(&aTmp);` |
|     - |  648 | `	}` |
|     - |  649 | `	/* Return the created array */` |
|     7 |  650 | `	ph7_result_value(pCtx,pArray);` |
|     - |  651 | `	/*` |
|     - |  652 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  653 | `	 * automatically as soon we return from this foreign function.` |
|     - |  654 | `	 */` |
|     7 |  655 | `	return PH7_OK;` |
|     5 |  656 | `}` |
|     - |  657 | `/*` |
|     - |  658 | ` * This function return TRUE(1) if the given class attribute stored` |
|     - |  659 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|     - |  660 | ` * from the current scope.Otherwise FALSE is returned.` |
|     - |  661 | ` */` |
| 37874 |  662 | `PH7_PRIVATE int PH7_VmClassMemberAccess(` |
|     - |  663 | `	ph7_vm *pVm,               /* Target VM */` |
|     - |  664 | `	ph7_class *pClass,         /* Target Class */` |
|     - |  665 | `	const SyString *pAttrName, /* Attribute name */` |
|     - |  666 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|     - |  667 | `	int bLog                   /* TRUE to log forbidden access. */` |
|     - |  668 | `	)` |
|     5 |  669 | `{` |
| 37879 |  670 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
| 26991 |  671 | `		VmFrame *pFrame = pVm->pFrame;` |
|     - |  672 | `		ph7_vm_func *pVmFunc;` |
|     - |  673 | `		ph7_class *pCallerScope;` |
| 27005 |  674 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|     - |  675 | `			/* Safely ignore the exception frame */` |
|    16 |  676 | `			pFrame = pFrame->pParent;` |
|     2 |  677 | `		}` |
| 26991 |  678 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     - |  679 | `		/* The calling scope is the executing method's declaring class — OR, for a bound closure` |
|     - |  680 | `		 * (Closure::bindTo/call), the explicit scope override carried on the frame (Increment 2). */` |
| 26991 |  681 | `		if( pFrame->pBoundScope ){` |
|    15 |  682 | `			pCallerScope = pFrame->pBoundScope;` |
| 26984 |  683 | `		}else if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
| 26873 |  684 | `			pCallerScope = (ph7_class *)pVmFunc->pUserData;` |
| 13541 |  685 | `		}else if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|     - |  686 | `			/* A closure/arrow-fn defined inside a class carries its creation-site` |
|     - |  687 | `			 * class in pUserData (stamped by OP_LOAD_CLOSURE via` |
|     - |  688 | ``			 * PH7_VmPeekDeclaringClass, the same scope `self::`/`parent::` resolve`` |
|     - |  689 | `			 * against inside the body). php binds that class as the closure's scope,` |
|     - |  690 | ``			 * so `$this->privateMethod()` / `self::$private` inside the closure are`` |
|     - |  691 | `			 * allowed — an explicit Closure::bindTo/bind rebind still wins above via` |
|     - |  692 | `			 * pBoundScope. */` |
|    39 |  693 | `			pCallerScope = (ph7_class *)pVmFunc->pUserData;` |
|    88 |  694 | `		}else if( pVm->pConstEvalClass ){` |
|     - |  695 | `			/* Constant/property initializer bytecode runs without a method` |
|     - |  696 | `			 * frame; its scope is the class being initialized (php: a private` |
|     - |  697 | `			 * constant is reachable from its own class's initializers). */` |
|     3 |  698 | `			pCallerScope = pVm->pConstEvalClass;` |
|     2 |  699 | `		}else{` |
|    67 |  700 | `			goto dis; /* Not in a class scope: access is forbidden */` |
|     - |  701 | `		}` |
| 26927 |  702 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     - |  703 | `			/* php grants private access by DECLARING class: the caller's own` |
|     - |  704 | `			 * class must declare a private attribute of this name (a base` |
|     - |  705 | `			 * method touching its own private on a CHILD instance passes; a` |
|     - |  706 | `			 * child method touching an inherited base-private fails). An attr` |
|     - |  707 | `			 * whose declaring "class" is a TRAIT behaves as if declared by the` |
|     - |  708 | `			 * adopting class. Fallbacks: the caller being a trait used by the` |
|     - |  709 | `			 * instance's class (legacy trait-body scope), or — when the caller` |
|     - |  710 | `			 * class carries no such attr entry at all — the legacy exact-class` |
|     - |  711 | `			 * match (dynamic props and other non-declared shapes). */` |
| 12473 |  712 | `			ph7_class *pCaller = pCallerScope;` |
| 18707 |  713 | `			SyHashEntry *pOwnE = SyHashGet(&pCaller->hAttr,` |
| 12468 |  714 | `				(const void *)pAttrName->zString,pAttrName->nByte);` |
| 12473 |  715 | `			ph7_class_attr *pOwn = pOwnE ? (ph7_class_attr *)pOwnE->pUserData : 0;` |
| 12473 |  716 | `			int bGranted = 0;` |
| 12473 |  717 | `			if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
| 10622 |  718 | `				if( pOwn->pDeclClass == 0` |
| 10622 |  719 | `				 \|\| pOwn->pDeclClass == pCaller` |
|  6266 |  720 | `				 \|\| (pOwn->pDeclClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
| 10605 |  721 | `					bGranted = 1;` |
|  5305 |  722 | `				}` |
|  7160 |  723 | `			}else if( pOwn == 0 && pCaller == pClass ){` |
|   963 |  724 | `				bGranted = 1;` |
|   481 |  725 | `			}` |
| 12473 |  726 | `			if( !bGranted ){` |
|     - |  727 | `				/* Check if the caller is a trait used by pClass */` |
|     - |  728 | `				ph7_class **apTrait;` |
|     - |  729 | `				sxu32 nTrait,k;` |
|   910 |  730 | `				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|   910 |  731 | `				nTrait = SySetUsed(&pClass->aTrait);` |
|   910 |  732 | `				for(k = 0; k < nTrait; k++){` |
|   ! 0 |  733 | `					if( apTrait[k] == pCaller ){` |
|   ! 0 |  734 | `						bGranted = 1;` |
|   ! 0 |  735 | `						break;` |
|     - |  736 | `					}` |
|   ! 0 |  737 | `				}` |
|   453 |  738 | `			}` |
| 12473 |  739 | `			if( !bGranted && (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|     - |  740 | `				/* The target "class" is itself a trait: a trait-copied private` |
|     - |  741 | `				 * member behaves as if declared in the adopting class, so a` |
|     - |  742 | `` 				 * caller that USES the trait gets access (php: `self::s()` `` |
|     - |  743 | `				 * from a using class's static method reaching a trait-private` |
|     - |  744 | `				 * static — the callee resolves via the shared trait VmFunc` |
|     - |  745 | `				 * whose owner is the trait, not the class). */` |
|     - |  746 | `				ph7_class **apTrait;` |
|     - |  747 | `				sxu32 nTrait,k;` |
|   882 |  748 | `				apTrait = (ph7_class **)SySetBasePtr(&pCaller->aTrait);` |
|   882 |  749 | `				nTrait = SySetUsed(&pCaller->aTrait);` |
|   882 |  750 | `				for(k = 0; k < nTrait; k++){` |
|   882 |  751 | `					if( apTrait[k] == pClass ){` |
|   882 |  752 | `						bGranted = 1;` |
|   882 |  753 | `						break;` |
|     - |  754 | `					}` |
|   ! 0 |  755 | `				}` |
|   440 |  756 | `			}` |
| 12473 |  757 | `			if( !bGranted ){` |
|    29 |  758 | `				goto dis; /* Access is forbidden */` |
|     - |  759 | `			}` |
|  6226 |  760 | `		}else{` |
|     - |  761 | `			/* Protected */` |
| 14459 |  762 | `			ph7_class *pBase = pCallerScope;` |
|     - |  763 | `			/* php checks the hierarchy against the class that INTRODUCES the member,` |
|     - |  764 | `			 * not the one that (re)declares the override we resolved. A protected` |
|     - |  765 | `			 * member declared in a common ancestor B and overridden in a child C is` |
|     - |  766 | `			 * still reachable from a SIBLING scope S (also extending B) — S and C both` |
|     - |  767 | `			 * descend from B. Walk pClass up to the top-most ancestor that genuinely` |
|     - |  768 | `			 * declares a member of this name (a method via sFunc.pUserData, or an attr` |
|     - |  769 | `			 * via pDeclClass — hMethod/hAttr also carry inherited copies, so match on the` |
|     - |  770 | `			 * true declaring class) and test the hierarchy against that introducing` |
|     - |  771 | ``			 * class. `child_only` (declared solely in C) keeps pClass and stays denied`` |
|     - |  772 | `			 * from a sibling, matching php. */` |
| 14459 |  773 | `			ph7_class *pIntro = pClass;` |
|     - |  774 | `			ph7_class *pAnc;` |
| 39451 |  775 | `			for( pAnc = pClass ; pAnc ; pAnc = pAnc->pBase ){` |
| 24997 |  776 | `				ph7_class_method *pAncMeth = PH7_ClassExtractMethod(pAnc,pAttrName->zString,pAttrName->nByte);` |
| 24997 |  777 | `				SyHashEntry *pAncAttrE = SyHashGet(&pAnc->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
| 24997 |  778 | `				ph7_class_attr *pAncAttr = pAncAttrE ? (ph7_class_attr *)pAncAttrE->pUserData : 0;` |
| 24997 |  779 | `				int bHere = 0;` |
| 24997 |  780 | `				if( pAncMeth && (ph7_class *)pAncMeth->sFunc.pUserData == pAnc ){` |
|  3827 |  781 | `					bHere = 1;` |
|  1911 |  782 | `				}` |
| 24997 |  783 | `				if( pAncAttr && (pAncAttr->pDeclClass == pAnc \|\| pAncAttr->pDeclClass == 0) ){` |
| 11181 |  784 | `					bHere = 1;` |
|  5588 |  785 | `				}` |
| 24997 |  786 | `				if( bHere ){` |
| 15003 |  787 | `					pIntro = pAnc; /* keep climbing: the LAST (highest) match wins */` |
|  7499 |  788 | `				}` |
| 12501 |  789 | `			}` |
|     - |  790 | `			/* Must be in the same class hierarchy as the introducing class */` |
| 14459 |  791 | `			if( !PH7_VmInstanceOf(pIntro,pBase) && !PH7_VmInstanceOf(pBase,pIntro) ){` |
|    12 |  792 | `				int bTraitGrant = 0;` |
|    12 |  793 | `				if( (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|     - |  794 | `					/* Same trait-target rule as the private branch above */` |
|     - |  795 | `					ph7_class **apTrait;` |
|     - |  796 | `					sxu32 nTrait,k;` |
|     8 |  797 | `					apTrait = (ph7_class **)SySetBasePtr(&pBase->aTrait);` |
|     8 |  798 | `					nTrait = SySetUsed(&pBase->aTrait);` |
|     8 |  799 | `					for(k = 0; k < nTrait; k++){` |
|     6 |  800 | `						if( apTrait[k] == pClass ){` |
|     6 |  801 | `							bTraitGrant = 1;` |
|     6 |  802 | `							break;` |
|     - |  803 | `						}` |
|   ! 0 |  804 | `					}` |
|     3 |  805 | `				}` |
|    12 |  806 | `				if( !bTraitGrant ){` |
|     8 |  807 | `					goto dis; /* Access is forbidden */` |
|     - |  808 | `				}` |
|     2 |  809 | `			}` |
|     - |  810 | `		}` |
| 13445 |  811 | `	}` |
| 37783 |  812 | `	return 1; /* Access is granted */` |
|    48 |  813 | `dis:` |
|   100 |  814 | `	if( bLog ){` |
|   ! 0 |  815 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     - |  816 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|   ! 0 |  817 | `			&pClass->sName,pAttrName);` |
|   ! 0 |  818 | `	}` |
|   100 |  819 | `	return 0; /* Access is forbidden */` |
| 18942 |  820 | `}` |
|     - |  821 | `/*` |
|     - |  822 | ` * array get_class_vars(string/object $class_name)` |
|     - |  823 | ` *   Get the default properties of the class` |
|     - |  824 | ` * Parameters` |
|     - |  825 | ` *  class_name` |
|     - |  826 | ` *   The class name or class instance` |
|     - |  827 | ` * Return` |
|     - |  828 | ` *  Returns an associative array of declared properties visible from the current scope` |
|     - |  829 | ` *  with their default value. The resulting array elements are in the form` |
|     - |  830 | ` *  of varname => value.` |
|     - |  831 | ` * Note:` |
|     - |  832 | ` *   NULL is returned on failure.` |
|     - |  833 | ` */` |
|     4 |  834 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  835 | `{` |
|     - |  836 | `	ph7_value *pName,*pArray,sValue;` |
|     - |  837 | `	SyHashEntry *pEntry;` |
|     - |  838 | `	ph7_class *pClass;` |
|     - |  839 | `	/* Extract the target class first */` |
|     5 |  840 | `	pClass = 0;` |
|     5 |  841 | `	if( nArg > 0 ){` |
|     5 |  842 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     2 |  843 | `	}` |
|     5 |  844 | `	if( pClass == 0 ){` |
|     - |  845 | `		/* php screens the VALUE, not the type: anything stringifiable is accepted,` |
|     - |  846 | `		 * and a name that does not resolve to a class is a TypeError quoting the` |
|     - |  847 | `		 * stringified argument ("...must be a valid class name, Array given"). This` |
|     - |  848 | `		 * is why get_class_vars() opts out of the shared ZPP type screen in vm.c. */` |
|   ! 0 |  849 | `		int nLen = 0;` |
|   ! 0 |  850 | `		const char *zVal = "";` |
|   ! 0 |  851 | `		if( nArg > 0 ){` |
|   ! 0 |  852 | `			if( (apArg[0]->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|   ! 0 |  853 | `				zVal = "Array";` |
|   ! 0 |  854 | `				nLen = (int)sizeof("Array") - 1;` |
|   ! 0 |  855 | `			}else{` |
|   ! 0 |  856 | `				zVal = ph7_value_to_string(apArg[0],&nLen);` |
|     - |  857 | `			}` |
|   ! 0 |  858 | `		}` |
|   ! 0 |  859 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  860 | `			"get_class_vars(): Argument #1 ($class) must be a valid class name, %.*s given",` |
|   ! 0 |  861 | `			nLen,zVal);` |
|     - |  862 | `	}` |
|     - |  863 | `	/* Create a new array  */` |
|     5 |  864 | `	pArray = ph7_context_new_array(pCtx);` |
|     5 |  865 | `	pName = ph7_context_new_scalar(pCtx);` |
|     5 |  866 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|     5 |  867 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  868 | `		/* Out of memory,return NULL */` |
|   ! 0 |  869 | `		ph7_result_null(pCtx);` |
|   ! 0 |  870 | `		return PH7_OK;` |
|     - |  871 | `	}` |
|     - |  872 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|     5 |  873 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    13 |  874 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     9 |  875 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     9 |  876 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     - |  877 | `			/* php 8.4: VIRTUAL hooked properties have no backing store —` |
|     - |  878 | `			 * get_class_vars() excludes them (raw surface) */` |
|     3 |  879 | `			continue;` |
|     - |  880 | `		}` |
|     - |  881 | `		/* Check if the access is allowed */` |
|     7 |  882 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|     7 |  883 | `			SyString *pAttrName = &pAttr->sName;` |
|     7 |  884 | `			ph7_value *pValue = 0;` |
|     7 |  885 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     - |  886 | `				/* Static slots are computed at mount; constants lazily */` |
|     5 |  887 | `				PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr);` |
|     5 |  888 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|     3 |  889 | `			}else{` |
|     3 |  890 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|     3 |  891 | `					PH7_MemObjRelease(&sValue);` |
|     - |  892 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|     3 |  893 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue,FALSE);` |
|     3 |  894 | `					pValue = &sValue;` |
|     1 |  895 | `				}` |
|     - |  896 | `			}` |
|     - |  897 | `			/* Fill in the array */` |
|     7 |  898 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|     7 |  899 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|     - |  900 | `			/* Reset the cursor */` |
|     7 |  901 | `			ph7_value_reset_string_cursor(pName);` |
|     3 |  902 | `		}` |
|     1 |  903 | `	}` |
|     5 |  904 | `	PH7_MemObjRelease(&sValue);` |
|     - |  905 | `	/* Return the created array */` |
|     5 |  906 | `	ph7_result_value(pCtx,pArray);` |
|     - |  907 | `	/*` |
|     - |  908 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  909 | `	 * automatically as soon we return from this foreign function.` |
|     - |  910 | `	 */` |
|     5 |  911 | `	return PH7_OK;` |
|     3 |  912 | `}` |
|     - |  913 | `/*` |
|     - |  914 | ` * array get_object_vars(object $this)` |
|     - |  915 | ` *   Gets the properties of the given object` |
|     - |  916 | ` * Parameters` |
|     - |  917 | ` *  this` |
|     - |  918 | ` *   A class instance` |
|     - |  919 | ` * Return` |
|     - |  920 | ` *  Returns an associative array of defined object accessible non-static properties` |
|     - |  921 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|     - |  922 | ` *  it will be returned with a NULL value.` |
|     - |  923 | ` * Note:` |
|     - |  924 | ` *   NULL is returned on failure.` |
|     - |  925 | ` */` |
|    48 |  926 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  927 | `{` |
|    50 |  928 | `	ph7_class_instance *pThis = 0;` |
|     - |  929 | `	ph7_value *pName,*pArray;` |
|     - |  930 | `	SyHashEntry *pEntry;` |
|    50 |  931 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|     - |  932 | `		/* Extract the target instance */` |
|    50 |  933 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    24 |  934 | `	}` |
|    50 |  935 | `	if( pThis == 0 ){` |
|     - |  936 | `		/* No such instance,return NULL */` |
|   ! 0 |  937 | `		ph7_result_null(pCtx);` |
|   ! 0 |  938 | `		return PH7_OK;` |
|     - |  939 | `	}` |
|     - |  940 | `	/* Create a new array  */` |
|    50 |  941 | `	pArray = ph7_context_new_array(pCtx);` |
|    50 |  942 | `	pName = ph7_context_new_scalar(pCtx);` |
|    50 |  943 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  944 | `		/* Out of memory,return NULL */` |
|   ! 0 |  945 | `		ph7_result_null(pCtx);` |
|   ! 0 |  946 | `		return PH7_OK;` |
|     - |  947 | `	}` |
|     - |  948 | `	/* Fill the array with the defined attribute visible from the current scope.` |
|     - |  949 | `	 * SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched mid-walk` |
|     - |  950 | `	 * runs user code that may re-enter an hAttr walk on this instance (resetting` |
|     - |  951 | `	 * the hash's single embedded loop cursor) or unset()/create properties. The` |
|     - |  952 | `	 * names point into CLASS-owned attr storage (they outlive instance mutation);` |
|     - |  953 | `	 * each is re-looked-up before use so an entry unset by an earlier hook is` |
|     - |  954 | `	 * skipped instead of read after free. */` |
|     - |  955 | `	{` |
|     - |  956 | `		SySet sNames;` |
|     - |  957 | `		SyString *aName;` |
|     - |  958 | `		sxu32 iName,nName;` |
|    50 |  959 | `		SySetInit(&sNames,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|    50 |  960 | `		SyHashResetLoopCursor(&pThis->hAttr);` |
|   226 |  961 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   178 |  962 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   178 |  963 | `			if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|     - |  964 | `				/* Only non-static/constant attributes are extracted */` |
|    11 |  965 | `				continue;` |
|     - |  966 | `			}` |
|   166 |  967 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|    85 |  968 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     3 |  969 | `				continue; /* virtual set-only property: no value to expose (php) */` |
|     - |  970 | `			}` |
|   166 |  971 | `			SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|     2 |  972 | `		}` |
|    50 |  973 | `		aName = (SyString *)SySetBasePtr(&sNames);` |
|    50 |  974 | `		nName = SySetUsed(&sNames);` |
|   214 |  975 | `		for( iName = 0 ; iName < nName ; ++iName ){` |
|   166 |  976 | `			SyString *pAttrName = &aName[iName];` |
|     - |  977 | `			VmClassAttr *pVmAttr;` |
|   166 |  978 | `			pEntry = SyHashGet(&pThis->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|   166 |  979 | `			if( pEntry == 0 ){` |
|   ! 0 |  980 | `				continue; /* unset by an earlier hook */` |
|     - |  981 | `			}` |
|   166 |  982 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     - |  983 | `			/* Check if the access is allowed */` |
|   166 |  984 | `			if( PH7_VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|   132 |  985 | `				ph7_value *pValue = 0;` |
|     - |  986 | `				ph7_value sHookVal;` |
|     - |  987 | `				sxi32 rcHk;` |
|     - |  988 | `				/* PHP 8.4 property hooks: get_object_vars() reads through the get` |
|     - |  989 | `				 * hook (virtual properties included); raw slot otherwise. */` |
|   132 |  990 | `				PH7_MemObjInit(pCtx->pVm,&sHookVal);` |
|   132 |  991 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|   132 |  992 | `				if( rcHk == SXRET_OK ){` |
|    15 |  993 | `					pValue = &sHookVal;` |
|   125 |  994 | `				}else if( rcHk == SXERR_NOTFOUND ){` |
|     - |  995 | `					/* Extract attribute */` |
|   118 |  996 | `					pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    60 |  997 | `				}else{` |
|     - |  998 | `					/* the hook threw — parked on the boundary rail; php aborts the` |
|     - |  999 | `					 * whole builtin at the first throw (the helper's boundary gate` |
|     - | 1000 | `					 * keeps LATER hooks from running; raw values it falls back to` |
|     - | 1001 | `					 * are discarded when the throw routes) */` |
|   ! 0 | 1002 | `					PH7_MemObjRelease(&sHookVal);` |
|   ! 0 | 1003 | `					break;` |
|     - | 1004 | `				}` |
|   132 | 1005 | `				if( pValue ){` |
|     - | 1006 | `					/* Insert attribute name in the array */` |
|   132 | 1007 | `					ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|   132 | 1008 | `					ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|    65 | 1009 | `				}` |
|   132 | 1010 | `				PH7_MemObjRelease(&sHookVal);` |
|     - | 1011 | `				/* Reset the cursor */` |
|   132 | 1012 | `				ph7_value_reset_string_cursor(pName);` |
|    65 | 1013 | `			}` |
|    84 | 1014 | `		}` |
|    50 | 1015 | `		SySetRelease(&sNames);` |
|     - | 1016 | `	}` |
|     - | 1017 | `	/* Return the created array */` |
|    50 | 1018 | `	ph7_result_value(pCtx,pArray);` |
|     - | 1019 | `	/*` |
|     - | 1020 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - | 1021 | `	 * automatically as soon we return from this foreign function.` |
|     - | 1022 | `	 */` |
|    50 | 1023 | `	return PH7_OK;` |
|    26 | 1024 | `}` |
|     - | 1025 | ``/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in`` |
|     - | 1026 | ` * compile.c. Defends against compiler cycles even though interface cycle` |
|     - | 1027 | ` * detection should reject them up front. */` |
|     - | 1028 | `#define PH7_INTERFACE_WALK_MAX_DEPTH 64` |
|     - | 1029 | `/*` |
|     - | 1030 | ` * TRUE if pTarget is reachable from pIface as an ancestor interface: walk the` |
|     - | 1031 | `` * `extends` chain (pBase) AND each additional parent-interface set (aInterface),`` |
|     - | 1032 | `` * so a multiple-interface `interface C extends A, B` is recognized through BOTH`` |
|     - | 1033 | ` * A and B (php allows an interface to extend several interfaces). Recursion is` |
|     - | 1034 | ` * depth-bounded — a malformed cycle cannot run unbounded.` |
|     - | 1035 | ` */` |
| 14954 | 1036 | `static int VmInterfaceReaches(ph7_class *pIface,ph7_class *pTarget,int iDepth)` |
|     5 | 1037 | `{` |
| 27507 | 1038 | `	while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){` |
|     - | 1039 | `		ph7_class **apParent;` |
|     - | 1040 | `		sxu32 n;` |
| 17651 | 1041 | `		if( pIface == pTarget ){` |
|  5101 | 1042 | `			return TRUE;` |
|     - | 1043 | `		}` |
|     - | 1044 | `		/* Additional parent interfaces (interface X extends A, B, …) live in` |
|     - | 1045 | `		 * aInterface; the first parent stays on the pBase chain below. */` |
| 12555 | 1046 | `		apParent = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
| 12559 | 1047 | `		for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){` |
|     7 | 1048 | `			if( VmInterfaceReaches(apParent[n],pTarget,iDepth+1) ){` |
|     3 | 1049 | `				return TRUE;` |
|     - | 1050 | `			}` |
|     3 | 1051 | `		}` |
| 12553 | 1052 | `		pIface = pIface->pBase;` |
| 12553 | 1053 | `		iDepth++;` |
|     5 | 1054 | `	}` |
|  9861 | 1055 | `	return FALSE;` |
|  7482 | 1056 | `}` |
|     - | 1057 | `/*` |
|     - | 1058 | ` * This function returns TRUE if the given class is an implemented` |
|     - | 1059 | ` * interface.Otherwise FALSE is returned.` |
|     - | 1060 | ` */` |
| 18238 | 1061 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|     5 | 1062 | `{` |
|     - | 1063 | `	ph7_class **apInterface;` |
|     - | 1064 | `	sxu32 n;` |
| 18243 | 1065 | `	if( SySetUsed(pSet) < 1 ){` |
|     - | 1066 | `		/* Empty interface container */` |
|  5345 | 1067 | `		return FALSE;` |
|     - | 1068 | `	}` |
|     - | 1069 | `	/* Point to the set of implemented interfaces */` |
| 12903 | 1070 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|     - | 1071 | `	/* Perform the lookup, walking each interface's parent chain so that` |
|     - | 1072 | `	 * Iterator extends Traversable (and similar) is recognized. */` |
| 22755 | 1073 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
| 14953 | 1074 | `		if( VmInterfaceReaches(apInterface[n],pClass,0) ){` |
|  5101 | 1075 | `			return TRUE;` |
|     - | 1076 | `		}` |
|  4931 | 1077 | `	}` |
|  7807 | 1078 | `	return FALSE;` |
|  9124 | 1079 | `}` |
|     - | 1080 | `/*` |
|     - | 1081 | ` * This function returns TRUE if the given class (first argument)` |
|     - | 1082 | ` * is an instance of the main class (second argument).` |
|     - | 1083 | ` * Otherwise FALSE is returned.` |
|     - | 1084 | ` */` |
| 29038 | 1085 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|     5 | 1086 | `{` |
|     - | 1087 | `	ph7_class *pParent;` |
|     - | 1088 | `	sxi32 rc;` |
| 29043 | 1089 | `	if( pThis == pClass ){` |
|     - | 1090 | `		/* Instance of the same class */` |
| 15763 | 1091 | `		return TRUE;` |
|     - | 1092 | `	}` |
|     - | 1093 | `	/* Check implemented interfaces */` |
| 13285 | 1094 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
| 13285 | 1095 | `	if( rc ){` |
|  2115 | 1096 | `		return TRUE;` |
|     - | 1097 | `	}` |
|     - | 1098 | `	/* Check parent classes */` |
| 11175 | 1099 | `	pParent = pThis->pBase;` |
| 13113 | 1100 | `	while( pParent ){` |
|  5963 | 1101 | `		if( pParent == pClass ){` |
|     - | 1102 | `			/* Same instance */` |
|  1049 | 1103 | `			return TRUE;` |
|     - | 1104 | `		}` |
|     - | 1105 | `		/* Check the implemented interfaces */` |
|  4919 | 1106 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|  4919 | 1107 | `		if( rc ){` |
|  2981 | 1108 | `			return TRUE;` |
|     - | 1109 | `		}` |
|     - | 1110 | `		/* Point to the parent class */` |
|  1943 | 1111 | `		pParent = pParent->pBase;` |
|     5 | 1112 | `	}` |
|     - | 1113 | `	/* Not an instance of the the given class */` |
|  7155 | 1114 | `	return FALSE;` |
| 14524 | 1115 | `}` |
|     - | 1116 | `/*` |
|     - | 1117 | ` * This function returns TRUE if the given class (first argument)` |
|     - | 1118 | ` * is a subclass of the main class (second argument).` |
|     - | 1119 | ` * Otherwise FALSE is returned.` |
|     - | 1120 | ` */` |
|    46 | 1121 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|     3 | 1122 | `{` |
|     - | 1123 | `	SyHashEntry *pEntry;` |
|     - | 1124 | `	SyString *pName;` |
|    83 | 1125 | `	while( pClass ){` |
|    69 | 1126 | `		pName = &pClass->sName;` |
|     - | 1127 | `		/* Query the derived hashtable for a class-hierarchy match */` |
|    69 | 1128 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|    69 | 1129 | `		if( pEntry ){` |
|    25 | 1130 | `			return TRUE;` |
|     - | 1131 | `		}` |
|     - | 1132 | `		/* Query the interfaces implemented by THIS class in the chain — so an` |
|     - | 1133 | `		 * interface implemented by a PARENT (B extends A implements I) is found,` |
|     - | 1134 | `		 * mirroring PH7_VmInstanceOf. The original code queried only the first` |
|     - | 1135 | `		 * class's aInterface, missing inherited interfaces. */` |
|    46 | 1136 | `		if( VmQueryInterfaceSet(pBase,&pClass->aInterface) ){` |
|    11 | 1137 | `			return TRUE;` |
|     - | 1138 | `		}` |
|    36 | 1139 | `		pClass = pClass->pBase;` |
|     2 | 1140 | `	}` |
|     - | 1141 | `	/* Not a subclass */` |
|    16 | 1142 | `	return FALSE;` |
|    26 | 1143 | `}` |
|     - | 1144 | `/*` |
|     - | 1145 | ` * bool is_a(object\|string $object_or_class,string $class,bool $allow_string = false)` |
|     - | 1146 | ` *   Checks if the object/class is of this class or has this class (or interface)` |
|     - | 1147 | ` *   as one of its parents.` |
|     - | 1148 | ` * Parameters` |
|     - | 1149 | ` *  object_or_class` |
|     - | 1150 | ` *   The tested object, or a class name string (only honored when allow_string is TRUE).` |
|     - | 1151 | ` * class` |
|     - | 1152 | ` *  The class or interface name to test against.` |
|     - | 1153 | ` * allow_string` |
|     - | 1154 | ` *  When TRUE, a string first argument is resolved as a class name; php's default` |
|     - | 1155 | ` *  is FALSE, so is_a() returns FALSE for a string first argument without it. The` |
|     - | 1156 | ` *  flag is IGNORED for an object first argument (php).` |
|     - | 1157 | ` * Return` |
|     - | 1158 | ` *   Returns TRUE if object_or_class is of class class or has class as one of its` |
|     - | 1159 | ` *   parents (or implements it as an interface), FALSE otherwise.` |
|     - | 1160 | ` */` |
|    48 | 1161 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 1162 | `{` |
|    51 | 1163 | `	int res = 0; /* Assume FALSE by default */` |
|    51 | 1164 | `	if( nArg > 1 ){` |
|    51 | 1165 | `		ph7_class *pThisClass = 0;` |
|    51 | 1166 | `		if( ph7_value_is_object(apArg[0]) ){` |
|     - | 1167 | `			/* An object first argument: allow_string is ignored (php). */` |
|    33 | 1168 | `			pThisClass = ((ph7_class_instance *)apArg[0]->x.pOther)->pClass;` |
|    34 | 1169 | `		}else if( ph7_value_is_string(apArg[0]) && nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|     - | 1170 | `			/* A string first argument is resolved to a class ONLY when allow_string` |
|     - | 1171 | `			 * (the 3rd argument) is TRUE — valid php since 5.3.9, and a sibling of` |
|     - | 1172 | `			 * is_subclass_of()'s string form. Autoloads on a miss via` |
|     - | 1173 | `			 * PH7_VmExtractClassFromValue; a leading '\' is anchored there. */` |
|    15 | 1174 | `			pThisClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     7 | 1175 | `		}` |
|    51 | 1176 | `		if( pThisClass ){` |
|     - | 1177 | `			/* Extract the given class */` |
|    45 | 1178 | `			ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    45 | 1179 | `			if( pClass ){` |
|     - | 1180 | `				/* Perform the query — instanceof, so the class ITSELF matches` |
|     - | 1181 | `				 * (unlike is_subclass_of, which excludes self). */` |
|    45 | 1182 | `				res = PH7_VmInstanceOf(pThisClass,pClass);` |
|    21 | 1183 | `			}` |
|    21 | 1184 | `		}` |
|    24 | 1185 | `	}` |
|     - | 1186 | `	/* Query result */` |
|    51 | 1187 | `	ph7_result_bool(pCtx,res);` |
|    51 | 1188 | `	return PH7_OK;` |
|     3 | 1189 | `}` |
|     - | 1190 | `/*` |
|     - | 1191 | ` * int spl_object_id(object $object)` |
|     - | 1192 | ` *  Return the integer object handle (per-instance id) of the given object.` |
|     - | 1193 | ` * PHL note: PHP 8 throws a TypeError when passed a non-object; PHL returns NULL` |
|     - | 1194 | ` * to stay consistent with the engine's graceful-degradation convention.` |
|     - | 1195 | ` */` |
|    58 | 1196 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 | 1197 | `{` |
|     - | 1198 | `	ph7_class_instance *pThis;` |
|    62 | 1199 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 | 1200 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1201 | `		return PH7_OK;` |
|     - | 1202 | `	}` |
|    62 | 1203 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    62 | 1204 | `	ph7_result_int64(pCtx,(ph7_int64)pThis->nObjId);` |
|    62 | 1205 | `	return PH7_OK;` |
|    33 | 1206 | `}` |
|     - | 1207 | `/*` |
|     - | 1208 | ` * string spl_object_hash(object $object)` |
|     - | 1209 | ` *  Return a 32-char hex identifier, unique and stable per live object.` |
|     - | 1210 | ` * PHL note: PHP derives this from the internal handle plus a per-process key, so` |
|     - | 1211 | ` * the exact value is NOT reproducible. PHL returns the zero-padded object id,` |
|     - | 1212 | ` * which preserves the only guaranteed properties: unique per live object, stable` |
|     - | 1213 | ` * across calls, and distinct objects -> distinct strings. A non-object returns` |
|     - | 1214 | ` * NULL (PHP 8 throws a TypeError; see spl_object_id above).` |
|     - | 1215 | ` */` |
|    14 | 1216 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1217 | `{` |
|     - | 1218 | `	ph7_class_instance *pThis;` |
|    16 | 1219 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 | 1220 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1221 | `		return PH7_OK;` |
|     - | 1222 | `	}` |
|    16 | 1223 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    16 | 1224 | `	ph7_result_string_format(pCtx,"%08x%08x%08x%08x",0,0,0,(unsigned int)pThis->nObjId);` |
|    16 | 1225 | `	return PH7_OK;` |
|     9 | 1226 | `}` |
|     - | 1227 | `/*` |
|     - | 1228 | ` * bool is_subclass_of(object\|string $object_or_class,string $class,bool $allow_string = true)` |
|     - | 1229 | ` *   Checks if the object/class has this class (or interface) as one of its` |
|     - | 1230 | ` *   parents — the subclass relation, which EXCLUDES the class itself.` |
|     - | 1231 | ` * Parameters` |
|     - | 1232 | ` *  object_or_class` |
|     - | 1233 | ` *   The tested object, or a class name string (honored unless allow_string is FALSE).` |
|     - | 1234 | ` * class` |
|     - | 1235 | ` *  The class or interface name to test against.` |
|     - | 1236 | ` * allow_string` |
|     - | 1237 | ` *  When FALSE, a string first argument is NOT resolved and the call returns FALSE.` |
|     - | 1238 | ` *  php's default here is TRUE (unlike is_a's FALSE). The flag is IGNORED for an` |
|     - | 1239 | ` *  object first argument (php).` |
|     - | 1240 | ` * Return` |
|     - | 1241 | ` *  Returns TRUE if object_or_class is a proper subclass of class (or implements it` |
|     - | 1242 | ` *  as an interface, directly or via a parent), FALSE otherwise.` |
|     - | 1243 | ` */` |
|    58 | 1244 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 1245 | `{` |
|    61 | 1246 | `	int res = 0; /* Assume FALSE by default */` |
|    61 | 1247 | `	if( nArg > 1 ){` |
|    61 | 1248 | `		ph7_class *pClass = 0;` |
|    61 | 1249 | `		if( ph7_value_is_object(apArg[0]) ){` |
|     - | 1250 | `			/* An object first argument: allow_string is ignored (php). */` |
|    21 | 1251 | `			pClass = ((ph7_class_instance *)apArg[0]->x.pOther)->pClass;` |
|    52 | 1252 | `		}else if( ph7_value_is_string(apArg[0]) && (nArg < 3 \|\| ph7_value_to_bool(apArg[2])) ){` |
|     - | 1253 | `			/* A string first argument is resolved as a class name UNLESS allow_string` |
|     - | 1254 | `			 * (the 3rd argument) is explicitly FALSE — php's default here is TRUE.` |
|     - | 1255 | `			 * Autoloads on a miss via PH7_VmExtractClassFromValue; a leading '\' is` |
|     - | 1256 | `			 * anchored there. Sibling of is_a()'s string form (which defaults OFF). */` |
|    35 | 1257 | `			pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    16 | 1258 | `		}` |
|    61 | 1259 | `		if( pClass ){` |
|     - | 1260 | `			/* Extract the target class */` |
|    51 | 1261 | `			ph7_class *pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    51 | 1262 | `			if( pMain ){` |
|     - | 1263 | `				/* Perform the query — subclass-only (excludes self, unlike is_a). */` |
|    49 | 1264 | `				res = VmSubclassOf(pClass,pMain);` |
|    23 | 1265 | `			}` |
|    24 | 1266 | `		}` |
|    29 | 1267 | `	}` |
|     - | 1268 | `	/* Query result */` |
|    61 | 1269 | `	ph7_result_bool(pCtx,res);` |
|    61 | 1270 | `	return PH7_OK;` |
|     3 | 1271 | `}` |
|    80 | 1272 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1273 | `{` |
|     - | 1274 | `	ph7_value sResult; /* Store callback return value here */` |
|     - | 1275 | `	sxi32 rc;` |
|    81 | 1276 | `	if( nArg < 1 ){` |
|     - | 1277 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 1278 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1279 | `		return PH7_OK;` |
|     - | 1280 | `	}` |
|     - | 1281 | `	{` |
|     - | 1282 | `		/* php validates the callback BEFORE calling anything; the dispatcher below would` |
|     - | 1283 | `		 * otherwise answer NULL in silence for an unresolvable one. */` |
|    81 | 1284 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|    81 | 1285 | `		if( rcCb != PH7_OK ){` |
|    11 | 1286 | `			return rcCb;` |
|     - | 1287 | `		}` |
|     - | 1288 | `	}` |
|    71 | 1289 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    71 | 1290 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - | 1291 | `	/* php passes call_user_func()'s arguments BY VALUE: warn on a by-ref formal and copy */` |
|    71 | 1292 | `	PH7_VmCufDropByRefArgs(pCtx,apArg[0],nArg - 1,&apArg[1]);` |
|     - | 1293 | `	/* Try to invoke the callback. If the call_user_func() call site used` |
|     - | 1294 | `	 * name: arguments (e.g. call_user_func('f', b: 9)), forward them to the` |
|     - | 1295 | `	 * callback. The inner call's argument i is the outer argument i+1 (outer` |
|     - | 1296 | `	 * argument 0 is the callback), so the inner name array is simply the outer` |
|     - | 1297 | `	 * names shifted by one — no copy needed: VmResolveNamedArgs treats any index` |
|     - | 1298 | `	 * >= nTotal as positional, so a shorter map covers the callback's args. */` |
|    80 | 1299 | `	if( pCtx->pArgMap && pCtx->pArgMap->bHasNamed && nArg > 1 ){` |
|    19 | 1300 | `		VmCallArgMap *pOuter = pCtx->pArgMap;` |
|     - | 1301 | `		VmCallArgMap sInner;` |
|    19 | 1302 | `		sInner.bHasNamed = 1;` |
|    19 | 1303 | `		sInner.bIsNamespaced = 0;` |
|     - | 1304 | `		/* Named args to call_user_func coerce in WEAK mode even from a` |
|     - | 1305 | `		 * strict_types=1 caller (verified vs php 8.5.7): a name: argument` |
|     - | 1306 | `		 * collected into the variadic and re-spread loses the strict context.` |
|     - | 1307 | `		 * call_user_func_array does NOT share this quirk (it stays strict). */` |
|    19 | 1308 | `		sInner.bStrict = 0;` |
|    19 | 1309 | `		sInner.nTotal = pOuter->nTotal > 1 ? pOuter->nTotal - 1 : 0;` |
|    19 | 1310 | `		sInner.aNames = sInner.nTotal > 0 ? &pOuter->aNames[1] : 0;` |
|    19 | 1311 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sInner);` |
|    10 | 1312 | `	}else{` |
|    53 | 1313 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|     - | 1314 | `	}` |
|    71 | 1315 | `	if( rc == PH7_EXCEPTION ){` |
|     - | 1316 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds` |
|     - | 1317 | `		 * through the nearest try/catch instead of returning FALSE. */` |
|     7 | 1318 | `		PH7_MemObjRelease(&sResult);` |
|     7 | 1319 | `		return PH7_EXCEPTION;` |
|     - | 1320 | `	}` |
|    65 | 1321 | `	if( rc != SXRET_OK ){` |
|     - | 1322 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 | 1323 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 | 1324 | `	}else{` |
|     - | 1325 | `		/* Callback result */` |
|    65 | 1326 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - | 1327 | `	}` |
|    65 | 1328 | `	PH7_MemObjRelease(&sResult);` |
|    65 | 1329 | `	return PH7_OK;` |
|    41 | 1330 | `}` |
|     - | 1331 | `/*` |
|     - | 1332 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|     - | 1333 | ` *  Call a callback with an array of parameters.` |
|     - | 1334 | ` * Parameter` |
|     - | 1335 | ` *  $callback` |
|     - | 1336 | ` *   The callable to be called.` |
|     - | 1337 | ` * $param_arr` |
|     - | 1338 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|     - | 1339 | ` * Return` |
|     - | 1340 | ` *  Returns the return value of the callback, or FALSE on error.` |
|     - | 1341 | ` */` |
|    36 | 1342 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1343 | `{` |
|     - | 1344 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|     - | 1345 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|     - | 1346 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|     - | 1347 | `	SySet aArg;               /* Argument value pointers */` |
|    37 | 1348 | `	SyString *aNames = 0;     /* Name map, lazily allocated when a string key appears */` |
|    37 | 1349 | `	sxu32 nSlot = 0;          /* Number of collected arguments */` |
|     - | 1350 | `	sxi32 rc;` |
|     - | 1351 | `	sxu32 n;` |
|    37 | 1352 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|     - | 1353 | `		/* Missing/Invalid arguments,return FALSE */` |
|   ! 0 | 1354 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1355 | `		return PH7_OK;` |
|     - | 1356 | `	}` |
|     - | 1357 | `	{` |
|    37 | 1358 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|    37 | 1359 | `		if( rcCb != PH7_OK ){` |
|     3 | 1360 | `			return rcCb;` |
|     - | 1361 | `		}` |
|     - | 1362 | `	}` |
|    35 | 1363 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    35 | 1364 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - | 1365 | `	/* Initialize the arguments container */` |
|    35 | 1366 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|     - | 1367 | `	/* Turn hashmap entries into callback arguments. A string key becomes a` |
|     - | 1368 | `	 * named argument (PHP 8: call_user_func_array($cb, ['b' => 9])), an integer` |
|     - | 1369 | `	 * key stays positional. The name map points straight at each node's key` |
|     - | 1370 | `	 * blob: the source array stays pinned on the operand stack for the whole` |
|     - | 1371 | `	 * call, so the blobs outlive argument binding. A pure list array (no string` |
|     - | 1372 | `	 * keys) never allocates aNames and takes the plain positional path. */` |
|    35 | 1373 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|    35 | 1374 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|   189 | 1375 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     - | 1376 | `		/* Extract node value */` |
|   155 | 1377 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|   155 | 1378 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    23 | 1379 | `				if( aNames == 0 ){` |
|     - | 1380 | `					/* First string key: allocate the whole map, zeroed so every` |
|     - | 1381 | `					 * not-yet-seen slot defaults to positional. */` |
|    13 | 1382 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,pMap->nEntry * sizeof(SyString));` |
|    13 | 1383 | `					if( aNames == 0 ){` |
|   ! 0 | 1384 | `						SySetRelease(&aArg);` |
|   ! 0 | 1385 | `						PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1386 | `						return PH7_ContextMemoryError(pCtx);` |
|     - | 1387 | `					}` |
|    13 | 1388 | `					SyZero(aNames,pMap->nEntry * sizeof(SyString));` |
|     6 | 1389 | `				}` |
|    23 | 1390 | `				SyStringInitFromBuf(&aNames[nSlot],SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    11 | 1391 | `			}` |
|   155 | 1392 | `			SySetPut(&aArg,(const void *)&pValue);` |
|   155 | 1393 | `			nSlot++;` |
|    77 | 1394 | `		}` |
|     - | 1395 | `		/* Point to the next entry */` |
|   155 | 1396 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    78 | 1397 | `	}` |
|     - | 1398 | `	/* Try to invoke the callback */` |
|    35 | 1399 | `	if( aNames ){` |
|     - | 1400 | `		VmCallArgMap sMap;` |
|    13 | 1401 | `		sMap.bHasNamed = 1;` |
|    13 | 1402 | `		sMap.bIsNamespaced = 0;` |
|     - | 1403 | `		/* Coercion strictness follows the caller's file; the OP_CALL dispatcher` |
|     - | 1404 | `		 * forwards the call site's map on pArgMap (0 only at non-OP_CALL sites). */` |
|    13 | 1405 | `		sMap.bStrict = (pCtx->pArgMap ? pCtx->pArgMap->bStrict : 0);` |
|    13 | 1406 | `		sMap.nTotal = nSlot;` |
|    13 | 1407 | `		sMap.aNames = aNames;` |
|    19 | 1408 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|    12 | 1409 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sMap);` |
|    13 | 1410 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aNames);` |
|     7 | 1411 | `	}else{` |
|    34 | 1412 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)nSlot,` |
|    22 | 1413 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|     - | 1414 | `	}` |
|    35 | 1415 | `	if( rc == PH7_EXCEPTION ){` |
|     - | 1416 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds. */` |
|     5 | 1417 | `		PH7_MemObjRelease(&sResult);` |
|     5 | 1418 | `		SySetRelease(&aArg);` |
|     5 | 1419 | `		return PH7_EXCEPTION;` |
|     - | 1420 | `	}` |
|    31 | 1421 | `	if( rc != SXRET_OK ){` |
|     - | 1422 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 | 1423 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 | 1424 | `	}else{` |
|     - | 1425 | `		/* Callback result */` |
|    31 | 1426 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - | 1427 | `	}` |
|     - | 1428 | `	/* Cleanup the mess left behind */` |
|    31 | 1429 | `	PH7_MemObjRelease(&sResult);` |
|    31 | 1430 | `	SySetRelease(&aArg);` |
|    31 | 1431 | `	return PH7_OK;` |
|    19 | 1432 | `}` |
|     - | 1433 |  |
