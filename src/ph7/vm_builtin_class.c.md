# src/ph7/vm_builtin_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 476/551 lines (86.39%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|   444 |    7 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |    8 | `{` |
|     - |    9 | `	ph7_class *pClass;` |
|     - |   10 | `	SyString *pName;` |
|   449 |   11 | `	if( nArg < 1 ){` |
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
|   449 |   24 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|   449 |   25 | `		if( pClass ){` |
|   447 |   26 | `			pName = &pClass->sName;` |
|     - |   27 | `			/* Return the class name */` |
|   447 |   28 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|   226 |   29 | `		}else{` |
|     - |   30 | `			/* Not a class instance,return FALSE */` |
|     3 |   31 | `			ph7_result_bool(pCtx,0);` |
|     - |   32 | `		}` |
|     - |   33 | `	}` |
|   449 |   34 | `	return PH7_OK;` |
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
|    38 |   48 | `PH7_PRIVATE int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |   49 | `{` |
|     - |   50 | `	ph7_class *pClass;` |
|     - |   51 | `	SyString *pName;` |
|    40 |   52 | `	if( nArg < 1 ){` |
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
|    38 |   65 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    38 |   66 | `		if( pClass ){` |
|    38 |   67 | `			if( pClass->pBase ){` |
|    36 |   68 | `				pName = &pClass->pBase->sName;` |
|     - |   69 | `				/* Return the parent class name */` |
|    36 |   70 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|    19 |   71 | `			}else{` |
|     - |   72 | `				/* Object does not have a parent class */` |
|     3 |   73 | `				ph7_result_bool(pCtx,0);` |
|     - |   74 | `			}` |
|    20 |   75 | `		}else{` |
|     - |   76 | `			/* Not a class instance,return FALSE */` |
|   ! 0 |   77 | `			ph7_result_bool(pCtx,0);` |
|     - |   78 | `		}` |
|     - |   79 | `	}` |
|    40 |   80 | `	return PH7_OK;` |
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
|  1918 |  113 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|     5 |  114 | `{` |
|  1923 |  115 | `	ph7_class *pClass = 0;` |
|  1923 |  116 | `	if( ph7_value_is_object(pArg) ){` |
|     - |  117 | `		/* Class instance already loaded,no need to perform a lookup */` |
|   551 |  118 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|  1648 |  119 | `	}else if( ph7_value_is_string(pArg) ){` |
|     - |  120 | `		const char *zClass;` |
|     - |  121 | `		int nLen;` |
|     - |  122 | `		/* Extract class name */` |
|  1372 |  123 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|  1372 |  124 | `		if( nLen > 0 ){` |
|     - |  125 | `			SyHashEntry *pEntry;` |
|     - |  126 | `			/* Perform a lookup */` |
|  1372 |  127 | `			pEntry = SyHashGet(&pVm->hClass,(const void *)zClass,(sxu32)nLen);` |
|  1372 |  128 | `			if( pEntry ){` |
|     - |  129 | `				/* Point to the desired class */` |
|  1352 |  130 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|   675 |  131 | `			}` |
|   685 |  132 | `		}` |
|   685 |  133 | `	}` |
|  1923 |  134 | `	return pClass;` |
|     5 |  135 | `}` |
|     - |  136 | `/*` |
|     - |  137 | ` * bool property_exists(mixed $class,string $property)` |
|     - |  138 | ` *   Checks if the object or class has a property.` |
|     - |  139 | ` * Parameters` |
|     - |  140 | ` *  class` |
|     - |  141 | ` *   The class name or an object of the class to test for` |
|     - |  142 | ` * property` |
|     - |  143 | ` *  The name of the property` |
|     - |  144 | ` * Return` |
|     - |  145 | ` *   Returns TRUE if the property exists,FALSE otherwise.` |
|     - |  146 | ` */` |
|    14 |  147 | `PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  148 | `{` |
|    15 |  149 | `	int res = 0; /* Assume attribute does not exists */` |
|    15 |  150 | `	if( nArg > 1 ){` |
|     - |  151 | `		ph7_class *pClass;` |
|    15 |  152 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    15 |  153 | `		if( pClass ){` |
|     - |  154 | `			const char *zName;` |
|     - |  155 | `			int nLen;` |
|     - |  156 | `			/* Extract attribute name */` |
|    15 |  157 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|    15 |  158 | `			if( nLen > 0 ){` |
|     - |  159 | `				/* Perform the lookup in the attribute and method table */` |
|    14 |  160 | `				if( SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen) != 0` |
|    10 |  161 | `					\|\| SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|     - |  162 | `						/* property exists,flag that */` |
|    11 |  163 | `						res = 1;` |
|     5 |  164 | `				}` |
|     - |  165 | `				/* A DYNAMIC (runtime-added) property lives on the INSTANCE's` |
|     - |  166 | `				 * attribute table, not the class's — php reports those too` |
|     - |  167 | `				 * (band A #3b; pre-fix property_exists() was blind to them). */` |
|    15 |  168 | `				if( res == 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|     3 |  169 | `					ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     3 |  170 | `					if( pThis && SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nLen) != 0 ){` |
|   ! 0 |  171 | `						res = 1;` |
|   ! 0 |  172 | `					}` |
|     1 |  173 | `				}` |
|     7 |  174 | `			}` |
|     7 |  175 | `		}` |
|     7 |  176 | `	}` |
|    15 |  177 | `	ph7_result_bool(pCtx,res);` |
|    15 |  178 | `	return PH7_OK;` |
|     1 |  179 | `}` |
|     - |  180 | `/*` |
|     - |  181 | ` * bool method_exists(mixed $class,string $method)` |
|     - |  182 | ` *   Checks if the given method is a class member.` |
|     - |  183 | ` * Parameters` |
|     - |  184 | ` *  class` |
|     - |  185 | ` *   The class name or an object of the class to test for` |
|     - |  186 | ` * property` |
|     - |  187 | ` *  The name of the method` |
|     - |  188 | ` * Return` |
|     - |  189 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|     - |  190 | ` */` |
|     4 |  191 | `PH7_PRIVATE int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  192 | `{` |
|     5 |  193 | `	int res = 0; /* Assume method does not exists */` |
|     5 |  194 | `	if( nArg > 1 ){` |
|     - |  195 | `		ph7_class *pClass;` |
|     5 |  196 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     5 |  197 | `		if( pClass ){` |
|     - |  198 | `			const char *zName;` |
|     - |  199 | `			int nLen;` |
|     - |  200 | `			/* Extract method name */` |
|     5 |  201 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|     5 |  202 | `			if( nLen > 0 ){` |
|     - |  203 | `				/* Perform the lookup in the method table */` |
|     5 |  204 | `				if( SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|     - |  205 | `					/* method exists,flag that */` |
|     3 |  206 | `					res = 1;` |
|     1 |  207 | `				}` |
|     2 |  208 | `			}` |
|     2 |  209 | `		}` |
|     2 |  210 | `	}` |
|     5 |  211 | `	ph7_result_bool(pCtx,res);` |
|     5 |  212 | `	return PH7_OK;` |
|     1 |  213 | `}` |
|     - |  214 | `/*` |
|     - |  215 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|     - |  216 | ` *   Checks if the class has been defined.` |
|     - |  217 | ` * Parameters` |
|     - |  218 | ` *  class_name` |
|     - |  219 | ` *   The class name. The name is matched in a case-sensitive manner` |
|     - |  220 | ` *   unlinke the standard PHP engine.` |
|     - |  221 | ` *  autoload` |
|     - |  222 | ` *   Whether or not to call __autoload by default.` |
|     - |  223 | ` * Return` |
|     - |  224 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|     - |  225 | ` */` |
|    66 |  226 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  227 | `{` |
|    70 |  228 | `	int res = 0; /* Assume class does not exist */` |
|    70 |  229 | `	if( nArg > 0 ){` |
|    70 |  230 | `		SyHashEntry *pEntry = 0;` |
|     - |  231 | `		const char *zName;` |
|     - |  232 | `		int nLen;` |
|    70 |  233 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|     - |  234 | `		/* Extract given name */` |
|    70 |  235 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    70 |  236 | `		if( nArg >= 2 ){` |
|     6 |  237 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|     2 |  238 | `		}` |
|    70 |  239 | `		if( nLen > 0 ){` |
|     - |  240 | `			/* Perform a hash lookup first */` |
|    70 |  241 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|    33 |  242 | `		}` |
|    70 |  243 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|     - |  244 | `			/* Try autoload, then re-check */` |
|    20 |  245 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,(sxu32)nLen,FALSE);` |
|    20 |  246 | `			if( pClass ){` |
|     6 |  247 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|     2 |  248 | `			}` |
|     8 |  249 | `		}` |
|    70 |  250 | `		if( pEntry ){` |
|     - |  251 | `			/* Walk the collision chain: return TRUE only for concrete or abstract classes,` |
|     - |  252 | `			 * not for interfaces or traits (matching PHP behavior). */` |
|    56 |  253 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    56 |  254 | `			while( pClass ){` |
|    56 |  255 | `				if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|    56 |  256 | `					res = 1;` |
|    56 |  257 | `					break;` |
|     - |  258 | `				}` |
|   ! 0 |  259 | `				pClass = pClass->pNextName;` |
|   ! 0 |  260 | `			}` |
|    26 |  261 | `		}` |
|    33 |  262 | `	}` |
|    70 |  263 | `	ph7_result_bool(pCtx,res);` |
|    70 |  264 | `	return PH7_OK;` |
|     4 |  265 | `}` |
|     - |  266 | `/*` |
|     - |  267 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|     - |  268 | ` *   Checks if the interface has been defined.` |
|     - |  269 | ` * Parameters` |
|     - |  270 | ` *  class_name` |
|     - |  271 | ` *   The class name. The name is matched in a case-sensitive manner` |
|     - |  272 | ` *   unlinke the standard PHP engine.` |
|     - |  273 | ` *  autoload` |
|     - |  274 | ` *   Whether or not to call __autoload by default.` |
|     - |  275 | ` * Return` |
|     - |  276 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|     - |  277 | ` */` |
|    24 |  278 | `PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  279 | `{` |
|    25 |  280 | `	int res = 0; /* Assume interface does not exist */` |
|    25 |  281 | `	if( nArg > 0 ){` |
|    25 |  282 | `		SyHashEntry *pEntry = 0;` |
|     - |  283 | `		const char *zName;` |
|     - |  284 | `		int nLen;` |
|    25 |  285 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|     - |  286 | `		/* Extract given name */` |
|    25 |  287 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    25 |  288 | `		if( nArg >= 2 ){` |
|   ! 0 |  289 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|   ! 0 |  290 | `		}` |
|     - |  291 | `		/* Perform a hash lookup */` |
|    25 |  292 | `		if( nLen > 0 ){` |
|    25 |  293 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|    12 |  294 | `		}` |
|    25 |  295 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|     - |  296 | `			/* Try autoload — pass iLoadable=FALSE so we get interfaces too */` |
|     3 |  297 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,(sxu32)nLen,FALSE);` |
|     3 |  298 | `			if( pClass ){` |
|   ! 0 |  299 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|   ! 0 |  300 | `			}` |
|     1 |  301 | `		}` |
|    25 |  302 | `		if( pEntry ){` |
|    23 |  303 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    23 |  304 | `			while( pClass ){` |
|    23 |  305 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     - |  306 | `					/* interface is available */` |
|    23 |  307 | `					res = 1;` |
|    23 |  308 | `					break;` |
|     - |  309 | `				}` |
|     - |  310 | `				/* Next with the same name */` |
|   ! 0 |  311 | `				pClass = pClass->pNextName;` |
|   ! 0 |  312 | `			}` |
|    11 |  313 | `		}` |
|    12 |  314 | `	}` |
|    25 |  315 | `	ph7_result_bool(pCtx,res);` |
|    25 |  316 | `	return PH7_OK;` |
|     1 |  317 | `}` |
|     - |  318 | `/*` |
|     - |  319 | ` * bool class_alias([string $original[,string $alias ]])` |
|     - |  320 | ` *   Creates an alias for a class.` |
|     - |  321 | ` * Parameters` |
|     - |  322 | ` *  original` |
|     - |  323 | ` *    The original class.` |
|     - |  324 | ` *  alias` |
|     - |  325 | ` *   The alias name for the class.` |
|     - |  326 | ` * Return` |
|     - |  327 | ` *   Returns TRUE on success or FALSE on failure.` |
|     - |  328 | ` */` |
|     2 |  329 | `PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  330 | `{` |
|     - |  331 | `	const char *zOld,*zNew;` |
|     - |  332 | `	int nOldLen,nNewLen;` |
|     - |  333 | `	SyHashEntry *pEntry;` |
|     - |  334 | `	ph7_class *pClass;` |
|     - |  335 | `	char *zDup;` |
|     - |  336 | `	sxi32 rc;` |
|     3 |  337 | `	if( nArg < 2 ){` |
|     - |  338 | `		/* Missing arguments,return FALSE */` |
|   ! 0 |  339 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  340 | `		return PH7_OK;` |
|     - |  341 | `	}` |
|     - |  342 | `	/* Extract old class name */` |
|     3 |  343 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|     - |  344 | `	/* Extract alias name */` |
|     3 |  345 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|     3 |  346 | `	if( nNewLen < 1 ){` |
|     - |  347 | `		/* Invalid alias name,return FALSE */` |
|   ! 0 |  348 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  349 | `		return PH7_OK;` |
|     - |  350 | `	}` |
|     - |  351 | `	/* Perform a hash lookup */` |
|     3 |  352 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,(sxu32)nOldLen);` |
|     3 |  353 | `	if( pEntry ==  0 ){` |
|     - |  354 | `		/* No such class,return FALSE */` |
|   ! 0 |  355 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  356 | `		return PH7_OK;` |
|     - |  357 | `	}` |
|     - |  358 | `	/* Point to the class */` |
|     3 |  359 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  360 | `	/* Duplicate alias name */` |
|     3 |  361 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,(sxu32)nNewLen);` |
|     3 |  362 | `	if( zDup == 0 ){` |
|     - |  363 | `		/* Out of memory,return FALSE */` |
|   ! 0 |  364 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  365 | `		return PH7_OK;` |
|     - |  366 | `	}` |
|     - |  367 | `	/* Create the alias */` |
|     3 |  368 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,(sxu32)nNewLen,pClass);` |
|     3 |  369 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  370 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|   ! 0 |  371 | `	}` |
|     3 |  372 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     3 |  373 | `	return PH7_OK;` |
|     2 |  374 | `}` |
|     - |  375 | `/*` |
|     - |  376 | ` * array get_declared_classes(void)` |
|     - |  377 | ` *   Returns an array with the name of the defined classes` |
|     - |  378 | ` * Parameters` |
|     - |  379 | ` *  None` |
|     - |  380 | ` * Return` |
|     - |  381 | ` *   Returns an array of the names of the declared classes` |
|     - |  382 | ` *   in the current script.` |
|     - |  383 | ` * Note:` |
|     - |  384 | ` *   NULL is returned on failure.` |
|     - |  385 | ` */` |
|     2 |  386 | `PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  387 | `{` |
|     - |  388 | `	ph7_value *pName,*pArray;` |
|     - |  389 | `	SyHashEntry *pEntry;` |
|     - |  390 | `	/* Create a new array first */` |
|     3 |  391 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  392 | `	pName = ph7_context_new_scalar(pCtx);` |
|     3 |  393 | `	if( pArray == 0 \|\| pName == 0){` |
|   ! 0 |  394 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  395 | `		SXUNUSED(apArg);` |
|     - |  396 | `		/* Out of memory,return NULL */` |
|   ! 0 |  397 | `		ph7_result_null(pCtx);` |
|   ! 0 |  398 | `		return PH7_OK;` |
|     - |  399 | `	}` |
|     - |  400 | `	/* Fill the array with the defined classes */` |
|     3 |  401 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|   168 |  402 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   165 |  403 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  404 | `		/* Do not register classes defined as interfaces */` |
|   165 |  405 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|   141 |  406 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|     - |  407 | `			/* insert class name */` |
|   141 |  408 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  409 | `			/* Reset the cursor */` |
|   141 |  410 | `			ph7_value_reset_string_cursor(pName);` |
|    70 |  411 | `		}` |
|     1 |  412 | `	}` |
|     - |  413 | `	/* Return the created array */` |
|     3 |  414 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  415 | `	return PH7_OK;` |
|     2 |  416 | `}` |
|     - |  417 | `/*` |
|     - |  418 | ` * array get_declared_interfaces(void)` |
|     - |  419 | ` *   Returns an array with the name of the defined interfaces` |
|     - |  420 | ` * Parameters` |
|     - |  421 | ` *  None` |
|     - |  422 | ` * Return` |
|     - |  423 | ` *   Returns an array of the names of the declared interfaces` |
|     - |  424 | ` *   in the current script.` |
|     - |  425 | ` * Note:` |
|     - |  426 | ` *   NULL is returned on failure.` |
|     - |  427 | ` */` |
|     2 |  428 | `PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  429 | `{` |
|     - |  430 | `	ph7_value *pName,*pArray;` |
|     - |  431 | `	SyHashEntry *pEntry;` |
|     - |  432 | `	/* Create a new array first */` |
|     3 |  433 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  434 | `	pName = ph7_context_new_scalar(pCtx);` |
|     3 |  435 | `	if( pArray == 0 \|\| pName == 0 ){` |
|   ! 0 |  436 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  437 | `		SXUNUSED(apArg);` |
|     - |  438 | `		/* Out of memory,return NULL */` |
|   ! 0 |  439 | `		ph7_result_null(pCtx);` |
|   ! 0 |  440 | `		return PH7_OK;` |
|     - |  441 | `	}` |
|     - |  442 | `	/* Fill the array with the defined classes */` |
|     3 |  443 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|   170 |  444 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   167 |  445 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  446 | `		/* Register classes defined as interfaces only */` |
|   167 |  447 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|    27 |  448 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|     - |  449 | `			/* insert interface name */` |
|    27 |  450 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  451 | `			/* Reset the cursor */` |
|    27 |  452 | `			ph7_value_reset_string_cursor(pName);` |
|    13 |  453 | `		}` |
|     1 |  454 | `	}` |
|     - |  455 | `	/* Return the created array */` |
|     3 |  456 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  457 | `	return PH7_OK;` |
|     2 |  458 | `}` |
|     - |  459 | `/*` |
|     - |  460 | ` * array get_class_methods(string/object $class_name)` |
|     - |  461 | ` *   Returns an array with the name of the class methods` |
|     - |  462 | ` * Parameters` |
|     - |  463 | ` *  class_name` |
|     - |  464 | ` *  The class name or class instance` |
|     - |  465 | ` * Return` |
|     - |  466 | ` *  Returns an array of method names defined for the class specified by class_name.` |
|     - |  467 | ` *  In case of an error, it returns NULL.` |
|     - |  468 | ` * Note:` |
|     - |  469 | ` *   NULL is returned on failure.` |
|     - |  470 | ` */` |
|     6 |  471 | `PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  472 | `{` |
|     - |  473 | `	ph7_value *pName,*pArray;` |
|     - |  474 | `	SyHashEntry *pEntry;` |
|     - |  475 | `	ph7_class *pClass;` |
|     - |  476 | `	/* Extract the target class first */` |
|     7 |  477 | `	pClass = 0;` |
|     7 |  478 | `	if( nArg > 0 ){` |
|     7 |  479 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     3 |  480 | `	}` |
|     7 |  481 | `	if( pClass == 0 ){` |
|     - |  482 | `		/* No such class,return NULL */` |
|     3 |  483 | `		ph7_result_null(pCtx);` |
|     3 |  484 | `		return PH7_OK;` |
|     - |  485 | `	}` |
|     - |  486 | `	/* Create a new array  */` |
|     5 |  487 | `	pArray = ph7_context_new_array(pCtx);` |
|     5 |  488 | `	pName = ph7_context_new_scalar(pCtx);` |
|     5 |  489 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  490 | `		/* Out of memory,return NULL */` |
|   ! 0 |  491 | `		ph7_result_null(pCtx);` |
|   ! 0 |  492 | `		return PH7_OK;` |
|     - |  493 | `	}` |
|     - |  494 | `	/* Fill the array with the defined methods */` |
|     5 |  495 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|    17 |  496 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|    13 |  497 | `		ph7_class_method *pMethod = (ph7_class_method *)pEntry->pUserData;` |
|     - |  498 | `		/* Insert method name */` |
|    13 |  499 | `		ph7_value_string(pName,SyStringData(&pMethod->sFunc.sName),(int)SyStringLength(&pMethod->sFunc.sName));` |
|    13 |  500 | `		ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  501 | `		/* Reset the cursor */` |
|    13 |  502 | `		ph7_value_reset_string_cursor(pName);` |
|     1 |  503 | `	}` |
|     - |  504 | `	/* Return the created array */` |
|     5 |  505 | `	ph7_result_value(pCtx,pArray);` |
|     - |  506 | `	/*` |
|     - |  507 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  508 | `	 * automatically as soon we return from this foreign function.` |
|     - |  509 | `	 */` |
|     5 |  510 | `	return PH7_OK;` |
|     4 |  511 | `}` |
|     - |  512 | `/*` |
|     - |  513 | ` * This function return TRUE(1) if the given class attribute stored` |
|     - |  514 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|     - |  515 | ` * from the current scope.Otherwise FALSE is returned.` |
|     - |  516 | ` */` |
| 22420 |  517 | `PH7_PRIVATE int PH7_VmClassMemberAccess(` |
|     - |  518 | `	ph7_vm *pVm,               /* Target VM */` |
|     - |  519 | `	ph7_class *pClass,         /* Target Class */` |
|     - |  520 | `	const SyString *pAttrName, /* Attribute name */` |
|     - |  521 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|     - |  522 | `	int bLog                   /* TRUE to log forbidden access. */` |
|     - |  523 | `	)` |
|     5 |  524 | `{` |
| 22425 |  525 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
| 17649 |  526 | `		VmFrame *pFrame = pVm->pFrame;` |
|     - |  527 | `		ph7_vm_func *pVmFunc;` |
|     - |  528 | `		ph7_class *pCallerScope;` |
| 17657 |  529 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|     - |  530 | `			/* Safely ignore the exception frame */` |
|     9 |  531 | `			pFrame = pFrame->pParent;` |
|     1 |  532 | `		}` |
| 17649 |  533 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     - |  534 | `		/* The calling scope is the executing method's declaring class — OR, for a bound closure` |
|     - |  535 | `		 * (Closure::bindTo/call), the explicit scope override carried on the frame (Increment 2). */` |
| 17649 |  536 | `		if( pFrame->pBoundScope ){` |
|    11 |  537 | `			pCallerScope = pFrame->pBoundScope;` |
| 17644 |  538 | `		}else if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
| 17611 |  539 | `			pCallerScope = (ph7_class *)pVmFunc->pUserData;` |
|  8808 |  540 | `		}else{` |
|    30 |  541 | `			goto dis; /* Not in a class scope: access is forbidden */` |
|     - |  542 | `		}` |
| 17621 |  543 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     - |  544 | `			/* Must be the same instance or a trait used by the class */` |
|   583 |  545 | `			ph7_class *pCaller = pCallerScope;` |
|   583 |  546 | `			if( pCaller != pClass ){` |
|     - |  547 | `				/* Check if the caller is a trait used by pClass */` |
|     - |  548 | `				ph7_class **apTrait;` |
|     - |  549 | `				sxu32 nTrait,k;` |
|    12 |  550 | `				int iFound = 0;` |
|    12 |  551 | `				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|    12 |  552 | `				nTrait = SySetUsed(&pClass->aTrait);` |
|    20 |  553 | `				for(k = 0; k < nTrait; k++){` |
|    17 |  554 | `					if( apTrait[k] == pCaller ){` |
|     9 |  555 | `						iFound = 1;` |
|     9 |  556 | `						break;` |
|     - |  557 | `					}` |
|     5 |  558 | `				}` |
|    12 |  559 | `				if( !iFound ){` |
|     3 |  560 | `					goto dis; /* Access is forbidden */` |
|     - |  561 | `				}` |
|     4 |  562 | `			}` |
|   293 |  563 | `		}else{` |
|     - |  564 | `			/* Protected */` |
| 17043 |  565 | `			ph7_class *pBase = pCallerScope;` |
|     - |  566 | `			/* Must be in the same class hierarchy */` |
| 17043 |  567 | `			if( !PH7_VmInstanceOf(pClass,pBase) && !PH7_VmInstanceOf(pBase,pClass) ){` |
|   ! 0 |  568 | `				goto dis; /* Access is forbidden */` |
|     - |  569 | `			}` |
|     - |  570 | `		}` |
|  8807 |  571 | `	}` |
| 22395 |  572 | `	return 1; /* Access is granted */` |
|    15 |  573 | `dis:` |
|    32 |  574 | `	if( bLog ){` |
|   ! 0 |  575 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     - |  576 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|   ! 0 |  577 | `			&pClass->sName,pAttrName);` |
|   ! 0 |  578 | `	}` |
|    32 |  579 | `	return 0; /* Access is forbidden */` |
| 11215 |  580 | `}` |
|     - |  581 | `/*` |
|     - |  582 | ` * array get_class_vars(string/object $class_name)` |
|     - |  583 | ` *   Get the default properties of the class` |
|     - |  584 | ` * Parameters` |
|     - |  585 | ` *  class_name` |
|     - |  586 | ` *   The class name or class instance` |
|     - |  587 | ` * Return` |
|     - |  588 | ` *  Returns an associative array of declared properties visible from the current scope` |
|     - |  589 | ` *  with their default value. The resulting array elements are in the form` |
|     - |  590 | ` *  of varname => value.` |
|     - |  591 | ` * Note:` |
|     - |  592 | ` *   NULL is returned on failure.` |
|     - |  593 | ` */` |
|     2 |  594 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  595 | `{` |
|     - |  596 | `	ph7_value *pName,*pArray,sValue;` |
|     - |  597 | `	SyHashEntry *pEntry;` |
|     - |  598 | `	ph7_class *pClass;` |
|     - |  599 | `	/* Extract the target class first */` |
|     3 |  600 | `	pClass = 0;` |
|     3 |  601 | `	if( nArg > 0 ){` |
|     3 |  602 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     1 |  603 | `	}` |
|     3 |  604 | `	if( pClass == 0 ){` |
|     - |  605 | `		/* No such class,return NULL */` |
|   ! 0 |  606 | `		ph7_result_null(pCtx);` |
|   ! 0 |  607 | `		return PH7_OK;` |
|     - |  608 | `	}` |
|     - |  609 | `	/* Create a new array  */` |
|     3 |  610 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  611 | `	pName = ph7_context_new_scalar(pCtx);` |
|     3 |  612 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|     3 |  613 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  614 | `		/* Out of memory,return NULL */` |
|   ! 0 |  615 | `		ph7_result_null(pCtx);` |
|   ! 0 |  616 | `		return PH7_OK;` |
|     - |  617 | `	}` |
|     - |  618 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|     3 |  619 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     8 |  620 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     5 |  621 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     - |  622 | `		/* Check if the access is allowed */` |
|     5 |  623 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|     5 |  624 | `			SyString *pAttrName = &pAttr->sName;` |
|     5 |  625 | `			ph7_value *pValue = 0;` |
|     5 |  626 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     - |  627 | `				/* Extract static attribute value which is always computed */` |
|     5 |  628 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|     3 |  629 | `			}else{` |
|   ! 0 |  630 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|   ! 0 |  631 | `					PH7_MemObjRelease(&sValue);` |
|     - |  632 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|   ! 0 |  633 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue,FALSE);` |
|   ! 0 |  634 | `					pValue = &sValue;` |
|   ! 0 |  635 | `				}` |
|     - |  636 | `			}` |
|     - |  637 | `			/* Fill in the array */` |
|     5 |  638 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|     5 |  639 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|     - |  640 | `			/* Reset the cursor */` |
|     5 |  641 | `			ph7_value_reset_string_cursor(pName);` |
|     2 |  642 | `		}` |
|     1 |  643 | `	}` |
|     3 |  644 | `	PH7_MemObjRelease(&sValue);` |
|     - |  645 | `	/* Return the created array */` |
|     3 |  646 | `	ph7_result_value(pCtx,pArray);` |
|     - |  647 | `	/*` |
|     - |  648 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  649 | `	 * automatically as soon we return from this foreign function.` |
|     - |  650 | `	 */` |
|     3 |  651 | `	return PH7_OK;` |
|     2 |  652 | `}` |
|     - |  653 | `/*` |
|     - |  654 | ` * array get_object_vars(object $this)` |
|     - |  655 | ` *   Gets the properties of the given object` |
|     - |  656 | ` * Parameters` |
|     - |  657 | ` *  this` |
|     - |  658 | ` *   A class instance` |
|     - |  659 | ` * Return` |
|     - |  660 | ` *  Returns an associative array of defined object accessible non-static properties` |
|     - |  661 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|     - |  662 | ` *  it will be returned with a NULL value.` |
|     - |  663 | ` * Note:` |
|     - |  664 | ` *   NULL is returned on failure.` |
|     - |  665 | ` */` |
|    14 |  666 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  667 | `{` |
|    15 |  668 | `	ph7_class_instance *pThis = 0;` |
|     - |  669 | `	ph7_value *pName,*pArray;` |
|     - |  670 | `	SyHashEntry *pEntry;` |
|    15 |  671 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|     - |  672 | `		/* Extract the target instance */` |
|    15 |  673 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     7 |  674 | `	}` |
|    15 |  675 | `	if( pThis == 0 ){` |
|     - |  676 | `		/* No such instance,return NULL */` |
|   ! 0 |  677 | `		ph7_result_null(pCtx);` |
|   ! 0 |  678 | `		return PH7_OK;` |
|     - |  679 | `	}` |
|     - |  680 | `	/* Create a new array  */` |
|    15 |  681 | `	pArray = ph7_context_new_array(pCtx);` |
|    15 |  682 | `	pName = ph7_context_new_scalar(pCtx);` |
|    15 |  683 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  684 | `		/* Out of memory,return NULL */` |
|   ! 0 |  685 | `		ph7_result_null(pCtx);` |
|   ! 0 |  686 | `		return PH7_OK;` |
|     - |  687 | `	}` |
|     - |  688 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|    15 |  689 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    51 |  690 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    37 |  691 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     - |  692 | `		SyString *pAttrName;` |
|    37 |  693 | `		if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|     - |  694 | `			/* Only non-static/constant attributes are extracted */` |
|     3 |  695 | `			continue;` |
|     - |  696 | `		}` |
|    35 |  697 | `		pAttrName = &pVmAttr->pAttr->sName;` |
|     - |  698 | `		/* Check if the access is allowed */` |
|    35 |  699 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|    27 |  700 | `			ph7_value *pValue = 0;` |
|     - |  701 | `			/* Extract attribute */` |
|    27 |  702 | `			pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    27 |  703 | `			if( pValue ){` |
|     - |  704 | `				/* Insert attribute name in the array */` |
|    27 |  705 | `				ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|    27 |  706 | `				ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|    13 |  707 | `			}` |
|     - |  708 | `			/* Reset the cursor */` |
|    27 |  709 | `			ph7_value_reset_string_cursor(pName);` |
|    13 |  710 | `		}` |
|     1 |  711 | `	}` |
|     - |  712 | `	/* Return the created array */` |
|    15 |  713 | `	ph7_result_value(pCtx,pArray);` |
|     - |  714 | `	/*` |
|     - |  715 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  716 | `	 * automatically as soon we return from this foreign function.` |
|     - |  717 | `	 */` |
|    15 |  718 | `	return PH7_OK;` |
|     8 |  719 | `}` |
|     - |  720 | ``/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in`` |
|     - |  721 | ` * compile.c. Defends against compiler cycles even though interface cycle` |
|     - |  722 | ` * detection should reject them up front. */` |
|     - |  723 | `#define PH7_INTERFACE_WALK_MAX_DEPTH 64` |
|     - |  724 | `/*` |
|     - |  725 | ` * This function returns TRUE if the given class is an implemented` |
|     - |  726 | ` * interface.Otherwise FALSE is returned.` |
|     - |  727 | ` */` |
| 16380 |  728 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|     5 |  729 | `{` |
|     - |  730 | `	ph7_class **apInterface;` |
|     - |  731 | `	sxu32 n;` |
| 16385 |  732 | `	if( SySetUsed(pSet) < 1 ){` |
|     - |  733 | `		/* Empty interface container */` |
|   243 |  734 | `		return FALSE;` |
|     - |  735 | `	}` |
|     - |  736 | `	/* Point to the set of implemented interfaces */` |
| 16147 |  737 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|     - |  738 | `	/* Perform the lookup, walking each interface's parent chain so that` |
|     - |  739 | `	 * Iterator extends Traversable (and similar) is recognized. */` |
| 31029 |  740 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
| 16269 |  741 | `		ph7_class *pIface = apInterface[n];` |
| 16269 |  742 | `		int iDepth = 0;` |
| 31243 |  743 | `		while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){` |
| 16361 |  744 | `			if( pIface == pClass ){` |
|  1387 |  745 | `				return TRUE;` |
|     - |  746 | `			}` |
| 14979 |  747 | `			pIface = pIface->pBase;` |
| 14979 |  748 | `			iDepth++;` |
|     5 |  749 | `		}` |
|  7446 |  750 | `	}` |
| 14765 |  751 | `	return FALSE;` |
|  8195 |  752 | `}` |
|     - |  753 | `/*` |
|     - |  754 | ` * This function returns TRUE if the given class (first argument)` |
|     - |  755 | ` * is an instance of the main class (second argument).` |
|     - |  756 | ` * Otherwise FALSE is returned.` |
|     - |  757 | ` */` |
| 20428 |  758 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|     5 |  759 | `{` |
|     - |  760 | `	ph7_class *pParent;` |
|     - |  761 | `	sxi32 rc;` |
| 20433 |  762 | `	if( pThis == pClass ){` |
|     - |  763 | `		/* Instance of the same class */` |
|  8245 |  764 | `		return TRUE;` |
|     - |  765 | `	}` |
|     - |  766 | `	/* Check implemented interfaces */` |
| 12193 |  767 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
| 12193 |  768 | `	if( rc ){` |
|   945 |  769 | `		return TRUE;` |
|     - |  770 | `	}` |
|     - |  771 | `	/* Check parent classes */` |
| 11253 |  772 | `	pParent = pThis->pBase;` |
| 14995 |  773 | `	while( pParent ){` |
| 14747 |  774 | `		if( pParent == pClass ){` |
|     - |  775 | `			/* Same instance */` |
| 10563 |  776 | `			return TRUE;` |
|     - |  777 | `		}` |
|     - |  778 | `		/* Check the implemented interfaces */` |
|  4189 |  779 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|  4189 |  780 | `		if( rc ){` |
|   447 |  781 | `			return TRUE;` |
|     - |  782 | `		}` |
|     - |  783 | `		/* Point to the parent class */` |
|  3747 |  784 | `		pParent = pParent->pBase;` |
|     5 |  785 | `	}` |
|     - |  786 | `	/* Not an instance of the the given class */` |
|   253 |  787 | `	return FALSE;` |
| 10219 |  788 | `}` |
|     - |  789 | `/*` |
|     - |  790 | ` * This function returns TRUE if the given class (first argument)` |
|     - |  791 | ` * is a subclass of the main class (second argument).` |
|     - |  792 | ` * Otherwise FALSE is returned.` |
|     - |  793 | ` */` |
|    12 |  794 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|     1 |  795 | `{` |
|    13 |  796 | `	SySet *pInterface = &pClass->aInterface;` |
|     - |  797 | `	SyHashEntry *pEntry;` |
|     - |  798 | `	SyString *pName;` |
|     - |  799 | `	sxi32 rc;` |
|    21 |  800 | `	while( pClass ){` |
|    13 |  801 | `		pName = &pClass->sName;` |
|     - |  802 | `		/* Query the derived hashtable */` |
|    13 |  803 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|    13 |  804 | `		if( pEntry ){` |
|     5 |  805 | `			return TRUE;` |
|     - |  806 | `		}` |
|     9 |  807 | `		pClass = pClass->pBase;` |
|     1 |  808 | `	}` |
|     9 |  809 | `	rc = VmQueryInterfaceSet(pBase,pInterface);` |
|     9 |  810 | `	if( rc ){` |
|   ! 0 |  811 | `		return TRUE;` |
|     - |  812 | `	}` |
|     - |  813 | `	/* Not a subclass */` |
|     9 |  814 | `	return FALSE;` |
|     7 |  815 | `}` |
|     - |  816 | `/*` |
|     - |  817 | ` * bool is_a(object $object,string $class_name)` |
|     - |  818 | ` *   Checks if the object is of this class or has this class as one of its parents.` |
|     - |  819 | ` * Parameters` |
|     - |  820 | ` *  object` |
|     - |  821 | ` *   The tested object` |
|     - |  822 | ` * class_name` |
|     - |  823 | ` *  The class name` |
|     - |  824 | ` * Return` |
|     - |  825 | ` *   Returns TRUE if the object is of this class or has this class as one of its` |
|     - |  826 | ` *   parents, FALSE otherwise.` |
|     - |  827 | ` */` |
|    18 |  828 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  829 | `{` |
|    19 |  830 | `	int res = 0; /* Assume FALSE by default */` |
|    19 |  831 | `	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){` |
|    19 |  832 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     - |  833 | `		ph7_class *pClass;` |
|     - |  834 | `		/* Extract the given class */` |
|    19 |  835 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    19 |  836 | `		if( pClass ){` |
|     - |  837 | `			/* Perform the query */` |
|    19 |  838 | `			res = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|     9 |  839 | `		}` |
|     9 |  840 | `	}` |
|     - |  841 | `	/* Query result */` |
|    19 |  842 | `	ph7_result_bool(pCtx,res);` |
|    19 |  843 | `	return PH7_OK;` |
|     1 |  844 | `}` |
|     - |  845 | `/*` |
|     - |  846 | ` * int spl_object_id(object $object)` |
|     - |  847 | ` *  Return the integer object handle (per-instance id) of the given object.` |
|     - |  848 | ` * PHL note: PHP 8 throws a TypeError when passed a non-object; PHL returns NULL` |
|     - |  849 | ` * to stay consistent with the engine's graceful-degradation convention.` |
|     - |  850 | ` */` |
|    18 |  851 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  852 | `{` |
|     - |  853 | `	ph7_class_instance *pThis;` |
|    21 |  854 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 |  855 | `		ph7_result_null(pCtx);` |
|   ! 0 |  856 | `		return PH7_OK;` |
|     - |  857 | `	}` |
|    21 |  858 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    21 |  859 | `	ph7_result_int64(pCtx,(ph7_int64)pThis->nObjId);` |
|    21 |  860 | `	return PH7_OK;` |
|    12 |  861 | `}` |
|     - |  862 | `/*` |
|     - |  863 | ` * string spl_object_hash(object $object)` |
|     - |  864 | ` *  Return a 32-char hex identifier, unique and stable per live object.` |
|     - |  865 | ` * PHL note: PHP derives this from the internal handle plus a per-process key, so` |
|     - |  866 | ` * the exact value is NOT reproducible. PHL returns the zero-padded object id,` |
|     - |  867 | ` * which preserves the only guaranteed properties: unique per live object, stable` |
|     - |  868 | ` * across calls, and distinct objects -> distinct strings. A non-object returns` |
|     - |  869 | ` * NULL (PHP 8 throws a TypeError; see spl_object_id above).` |
|     - |  870 | ` */` |
|    10 |  871 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  872 | `{` |
|     - |  873 | `	ph7_class_instance *pThis;` |
|    11 |  874 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 |  875 | `		ph7_result_null(pCtx);` |
|   ! 0 |  876 | `		return PH7_OK;` |
|     - |  877 | `	}` |
|    11 |  878 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    11 |  879 | `	ph7_result_string_format(pCtx,"%08x%08x%08x%08x",0,0,0,(unsigned int)pThis->nObjId);` |
|    11 |  880 | `	return PH7_OK;` |
|     6 |  881 | `}` |
|     - |  882 | `/*` |
|     - |  883 | ` * bool is_subclass_of(object/string $object,object/string $class_name)` |
|     - |  884 | ` *   Checks if the object has this class as one of its parents.` |
|     - |  885 | ` * Parameters` |
|     - |  886 | ` *  object` |
|     - |  887 | ` *   The tested object` |
|     - |  888 | ` * class_name` |
|     - |  889 | ` *  The class name` |
|     - |  890 | ` * Return` |
|     - |  891 | ` *  This function returns TRUE if the object , belongs to a class` |
|     - |  892 | ` *  which is a subclass of class_name, FALSE otherwise.` |
|     - |  893 | ` */` |
|    14 |  894 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  895 | `{` |
|    15 |  896 | `	int res = 0; /* Assume FALSE by default */` |
|    15 |  897 | `	if( nArg > 1 ){` |
|     - |  898 | `		ph7_class *pClass,*pMain;` |
|     - |  899 | `		/* Extract the given classes */` |
|    15 |  900 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    15 |  901 | `		pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    15 |  902 | `		if( pClass && pMain ){` |
|     - |  903 | `			/* Perform the query */` |
|    13 |  904 | `			res = VmSubclassOf(pClass,pMain);` |
|     6 |  905 | `		}` |
|     7 |  906 | `	}` |
|     - |  907 | `	/* Query result */` |
|    15 |  908 | `	ph7_result_bool(pCtx,res);` |
|    15 |  909 | `	return PH7_OK;` |
|     1 |  910 | `}` |
|    44 |  911 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  912 | `{` |
|     - |  913 | `	ph7_value sResult; /* Store callback return value here */` |
|     - |  914 | `	sxi32 rc;` |
|    45 |  915 | `	if( nArg < 1 ){` |
|     - |  916 | `		/* Missing arguments,return FALSE */` |
|   ! 0 |  917 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  918 | `		return PH7_OK;` |
|     - |  919 | `	}` |
|    45 |  920 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    45 |  921 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - |  922 | `	/* Try to invoke the callback. If the call_user_func() call site used` |
|     - |  923 | `	 * name: arguments (e.g. call_user_func('f', b: 9)), forward them to the` |
|     - |  924 | `	 * callback. The inner call's argument i is the outer argument i+1 (outer` |
|     - |  925 | `	 * argument 0 is the callback), so the inner name array is simply the outer` |
|     - |  926 | `	 * names shifted by one — no copy needed: VmResolveNamedArgs treats any index` |
|     - |  927 | `	 * >= nTotal as positional, so a shorter map covers the callback's args. */` |
|    53 |  928 | `	if( pCtx->pArgMap && pCtx->pArgMap->bHasNamed && nArg > 1 ){` |
|    17 |  929 | `		VmCallArgMap *pOuter = pCtx->pArgMap;` |
|     - |  930 | `		VmCallArgMap sInner;` |
|    17 |  931 | `		sInner.bHasNamed = 1;` |
|    17 |  932 | `		sInner.bIsNamespaced = 0;` |
|     - |  933 | `		/* Named args to call_user_func coerce in WEAK mode even from a` |
|     - |  934 | `		 * strict_types=1 caller (verified vs php 8.5.7): a name: argument` |
|     - |  935 | `		 * collected into the variadic and re-spread loses the strict context.` |
|     - |  936 | `		 * call_user_func_array does NOT share this quirk (it stays strict). */` |
|    17 |  937 | `		sInner.bStrict = 0;` |
|    17 |  938 | `		sInner.nTotal = pOuter->nTotal > 1 ? pOuter->nTotal - 1 : 0;` |
|    17 |  939 | `		sInner.aNames = sInner.nTotal > 0 ? &pOuter->aNames[1] : 0;` |
|    17 |  940 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sInner);` |
|     9 |  941 | `	}else{` |
|    29 |  942 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|     - |  943 | `	}` |
|    45 |  944 | `	if( rc == PH7_EXCEPTION ){` |
|     - |  945 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds` |
|     - |  946 | `		 * through the nearest try/catch instead of returning FALSE. */` |
|     5 |  947 | `		PH7_MemObjRelease(&sResult);` |
|     5 |  948 | `		return PH7_EXCEPTION;` |
|     - |  949 | `	}` |
|    41 |  950 | `	if( rc != SXRET_OK ){` |
|     - |  951 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 |  952 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 |  953 | `	}else{` |
|     - |  954 | `		/* Callback result */` |
|    41 |  955 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - |  956 | `	}` |
|    41 |  957 | `	PH7_MemObjRelease(&sResult);` |
|    41 |  958 | `	return PH7_OK;` |
|    23 |  959 | `}` |
|     - |  960 | `/*` |
|     - |  961 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|     - |  962 | ` *  Call a callback with an array of parameters.` |
|     - |  963 | ` * Parameter` |
|     - |  964 | ` *  $callback` |
|     - |  965 | ` *   The callable to be called.` |
|     - |  966 | ` * $param_arr` |
|     - |  967 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|     - |  968 | ` * Return` |
|     - |  969 | ` *  Returns the return value of the callback, or FALSE on error.` |
|     - |  970 | ` */` |
|    28 |  971 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  972 | `{` |
|     - |  973 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|     - |  974 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|     - |  975 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|     - |  976 | `	SySet aArg;               /* Argument value pointers */` |
|    29 |  977 | `	SyString *aNames = 0;     /* Name map, lazily allocated when a string key appears */` |
|    29 |  978 | `	sxu32 nSlot = 0;          /* Number of collected arguments */` |
|     - |  979 | `	sxi32 rc;` |
|     - |  980 | `	sxu32 n;` |
|    29 |  981 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|     - |  982 | `		/* Missing/Invalid arguments,return FALSE */` |
|   ! 0 |  983 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  984 | `		return PH7_OK;` |
|     - |  985 | `	}` |
|    29 |  986 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    29 |  987 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - |  988 | `	/* Initialize the arguments container */` |
|    29 |  989 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|     - |  990 | `	/* Turn hashmap entries into callback arguments. A string key becomes a` |
|     - |  991 | `	 * named argument (PHP 8: call_user_func_array($cb, ['b' => 9])), an integer` |
|     - |  992 | `	 * key stays positional. The name map points straight at each node's key` |
|     - |  993 | `	 * blob: the source array stays pinned on the operand stack for the whole` |
|     - |  994 | `	 * call, so the blobs outlive argument binding. A pure list array (no string` |
|     - |  995 | `	 * keys) never allocates aNames and takes the plain positional path. */` |
|    29 |  996 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|    29 |  997 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|    77 |  998 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     - |  999 | `		/* Extract node value */` |
|    49 | 1000 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|    49 | 1001 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    23 | 1002 | `				if( aNames == 0 ){` |
|     - | 1003 | `					/* First string key: allocate the whole map, zeroed so every` |
|     - | 1004 | `					 * not-yet-seen slot defaults to positional. */` |
|    13 | 1005 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,pMap->nEntry * sizeof(SyString));` |
|    13 | 1006 | `					if( aNames == 0 ){` |
|   ! 0 | 1007 | `						SySetRelease(&aArg);` |
|   ! 0 | 1008 | `						PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1009 | `						return PH7_ContextMemoryError(pCtx);` |
|     - | 1010 | `					}` |
|    13 | 1011 | `					SyZero(aNames,pMap->nEntry * sizeof(SyString));` |
|     6 | 1012 | `				}` |
|    23 | 1013 | `				SyStringInitFromBuf(&aNames[nSlot],SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    11 | 1014 | `			}` |
|    49 | 1015 | `			SySetPut(&aArg,(const void *)&pValue);` |
|    49 | 1016 | `			nSlot++;` |
|    24 | 1017 | `		}` |
|     - | 1018 | `		/* Point to the next entry */` |
|    49 | 1019 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    25 | 1020 | `	}` |
|     - | 1021 | `	/* Try to invoke the callback */` |
|    29 | 1022 | `	if( aNames ){` |
|     - | 1023 | `		VmCallArgMap sMap;` |
|    13 | 1024 | `		sMap.bHasNamed = 1;` |
|    13 | 1025 | `		sMap.bIsNamespaced = 0;` |
|     - | 1026 | `		/* Coercion strictness follows the caller's file; the OP_CALL dispatcher` |
|     - | 1027 | `		 * forwards the call site's map on pArgMap (0 only at non-OP_CALL sites). */` |
|    13 | 1028 | `		sMap.bStrict = (pCtx->pArgMap ? pCtx->pArgMap->bStrict : 0);` |
|    13 | 1029 | `		sMap.nTotal = nSlot;` |
|    13 | 1030 | `		sMap.aNames = aNames;` |
|    19 | 1031 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|    12 | 1032 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sMap);` |
|    13 | 1033 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aNames);` |
|     7 | 1034 | `	}else{` |
|    25 | 1035 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)nSlot,` |
|    16 | 1036 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|     - | 1037 | `	}` |
|    29 | 1038 | `	if( rc == PH7_EXCEPTION ){` |
|     - | 1039 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds. */` |
|     5 | 1040 | `		PH7_MemObjRelease(&sResult);` |
|     5 | 1041 | `		SySetRelease(&aArg);` |
|     5 | 1042 | `		return PH7_EXCEPTION;` |
|     - | 1043 | `	}` |
|    25 | 1044 | `	if( rc != SXRET_OK ){` |
|     - | 1045 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 | 1046 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 | 1047 | `	}else{` |
|     - | 1048 | `		/* Callback result */` |
|    25 | 1049 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - | 1050 | `	}` |
|     - | 1051 | `	/* Cleanup the mess left behind */` |
|    25 | 1052 | `	PH7_MemObjRelease(&sResult);` |
|    25 | 1053 | `	SySetRelease(&aArg);` |
|    25 | 1054 | `	return PH7_OK;` |
|    15 | 1055 | `}` |
|     - | 1056 |  |
