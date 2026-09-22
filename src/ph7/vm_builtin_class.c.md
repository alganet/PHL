# src/ph7/vm_builtin_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 659/747 lines (88.22%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|    1816 |    7 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |    8 | `{` |
|       - |    9 | `	ph7_class *pClass;` |
|       - |   10 | `	SyString *pName;` |
|    1821 |   11 | `	if( nArg < 1 ){` |
|       - |   12 | `		/* Check if we are inside a class */` |
|     ! 0 |   13 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|     ! 0 |   14 | `		if( pClass ){` |
|       - |   15 | `			/* Point to the class name */` |
|     ! 0 |   16 | `			pName = &pClass->sName;` |
|     ! 0 |   17 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|     ! 0 |   18 | `		}else{` |
|       - |   19 | `			/* Not inside class,return FALSE */` |
|     ! 0 |   20 | `			ph7_result_bool(pCtx,0);` |
|       - |   21 | `		}` |
|     ! 0 |   22 | `	}else{` |
|       - |   23 | `		/* Extract the target class */` |
|    1821 |   24 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    1821 |   25 | `		if( pClass ){` |
|    1821 |   26 | `			pName = &pClass->sName;` |
|       - |   27 | `			/* Return the class name */` |
|    1821 |   28 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|     913 |   29 | `		}else{` |
|       - |   30 | `			/* Not a class instance,return FALSE */` |
|     ! 0 |   31 | `			ph7_result_bool(pCtx,0);` |
|       - |   32 | `		}` |
|       - |   33 | `	}` |
|    1821 |   34 | `	return PH7_OK;` |
|       5 |   35 | `}` |
|       - |   36 | `/*` |
|       - |   37 | ` * string get_parent_class([object $object = NULL ] )` |
|       - |   38 | ` *   Returns the name of the parent class of an object` |
|       - |   39 | ` * Parameters` |
|       - |   40 | ` *  object` |
|       - |   41 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|       - |   42 | ` * Return` |
|       - |   43 | ` *  The name of the parent class of which object is an instance.` |
|       - |   44 | ` *  Returns FALSE if object is not an object or if the object does` |
|       - |   45 | ` *  not have a parent.` |
|       - |   46 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|       - |   47 | ` */` |
|      52 |   48 | `PH7_PRIVATE int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |   49 | `{` |
|       - |   50 | `	ph7_class *pClass;` |
|       - |   51 | `	SyString *pName;` |
|      56 |   52 | `	if( nArg < 1 ){` |
|       - |   53 | `		/* Check if we are inside a class [i.e: a method call]*/` |
|       3 |   54 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|       3 |   55 | `		if( pClass && pClass->pBase ){` |
|       - |   56 | `			/* Point to the class name */` |
|       3 |   57 | `			pName = &pClass->pBase->sName;` |
|       3 |   58 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|       2 |   59 | `		}else{` |
|       - |   60 | `			/* Not inside class,return FALSE */` |
|     ! 0 |   61 | `			ph7_result_bool(pCtx,0);` |
|       - |   62 | `		}` |
|       2 |   63 | `	}else{` |
|       - |   64 | `		/* Extract the target class */` |
|      54 |   65 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      54 |   66 | `		if( pClass ){` |
|      54 |   67 | `			if( pClass->pBase ){` |
|      52 |   68 | `				pName = &pClass->pBase->sName;` |
|       - |   69 | `				/* Return the parent class name */` |
|      52 |   70 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|      28 |   71 | `			}else{` |
|       - |   72 | `				/* Object does not have a parent class */` |
|       3 |   73 | `				ph7_result_bool(pCtx,0);` |
|       - |   74 | `			}` |
|      29 |   75 | `		}else{` |
|       - |   76 | `			/* Not a class instance,return FALSE */` |
|     ! 0 |   77 | `			ph7_result_bool(pCtx,0);` |
|       - |   78 | `		}` |
|       - |   79 | `	}` |
|      56 |   80 | `	return PH7_OK;` |
|       4 |   81 | `}` |
|       - |   82 | `/*` |
|       - |   83 | ` * string get_called_class(void)` |
|       - |   84 | ` *   Gets the name of the class the static method is called in.` |
|       - |   85 | ` * Parameters` |
|       - |   86 | ` *  None.` |
|       - |   87 | ` * Return` |
|       - |   88 | ` *  Returns the class name. Returns FALSE if called from outside a class.` |
|       - |   89 | ` */` |
|       4 |   90 | `PH7_PRIVATE int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |   91 | `{` |
|       - |   92 | `	ph7_class *pClass;` |
|       - |   93 | `	/* Check if we are inside a class [i.e: a method call] */` |
|       5 |   94 | `	pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|       5 |   95 | `	if( pClass ){` |
|       - |   96 | `		SyString *pName;` |
|       - |   97 | `		/* Point to the class name */` |
|       5 |   98 | `		pName = &pClass->sName;` |
|       5 |   99 | `		ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|       3 |  100 | `	}else{` |
|     ! 0 |  101 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 |  102 | `		SXUNUSED(apArg);` |
|       - |  103 | `		/* Not inside class,return FALSE */` |
|     ! 0 |  104 | `		ph7_result_bool(pCtx,0);` |
|       - |  105 | `	}` |
|       5 |  106 | `	return PH7_OK;` |
|       1 |  107 | `}` |
|       - |  108 | `/*` |
|       - |  109 | ` * Extract a ph7_class from the given ph7_value.` |
|       - |  110 | ` * The given value must be of type object [i.e: class instance] or` |
|       - |  111 | ` * string which hold the class name.` |
|       - |  112 | ` */` |
|  103056 |  113 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|       5 |  114 | `{` |
|  103061 |  115 | `	ph7_class *pClass = 0;` |
|  103061 |  116 | `	if( ph7_value_is_object(pArg) ){` |
|       - |  117 | `		/* Class instance already loaded,no need to perform a lookup */` |
|  101929 |  118 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|   52099 |  119 | `	}else if( ph7_value_is_string(pArg) ){` |
|       - |  120 | `		const char *zClass;` |
|       - |  121 | `		int nLen;` |
|       - |  122 | `		/* Extract class name */` |
|    1137 |  123 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|       - |  124 | `		/* A leading '\' (the global-namespace anchor) is stripped by` |
|       - |  125 | `		 * PH7_VmExtractClass itself now (PH7_VmClassNameAnchor), so do not` |
|       - |  126 | `		 * strip here too — a second strip would wrongly resolve "\\Foo". */` |
|    1137 |  127 | `		if( nLen > 0 ){` |
|       - |  128 | `			/* Resolve through PH7_VmExtractClass so a class named by STRING is` |
|       - |  129 | `			 * autoloaded on a miss — php autoloads the class of a [class,method]` |
|       - |  130 | `			 * callable (is_callable/array_map/call_user_func), and of the class` |
|       - |  131 | `			 * argument to method_exists()/property_exists(). iLoadable=FALSE keeps` |
|       - |  132 | `			 * abstract classes and interfaces (a static method on an abstract class` |
|       - |  133 | `			 * is a valid callable). */` |
|    1135 |  134 | `			pClass = PH7_VmExtractClass(pVm,zClass,(sxu32)nLen,FALSE,0);` |
|     565 |  135 | `		}` |
|     566 |  136 | `	}` |
|  103061 |  137 | `	return pClass;` |
|       5 |  138 | `}` |
|       - |  139 | `/*` |
|       - |  140 | ` * bool property_exists(mixed $class,string $property)` |
|       - |  141 | ` *   Checks if the object or class has a property.` |
|       - |  142 | ` * Parameters` |
|       - |  143 | ` *  class` |
|       - |  144 | ` *   The class name or an object of the class to test for` |
|       - |  145 | ` * property` |
|       - |  146 | ` *  The name of the property` |
|       - |  147 | ` * Return` |
|       - |  148 | ` *   Returns TRUE if the property exists,FALSE otherwise.` |
|       - |  149 | ` */` |
|      20 |  150 | `PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  151 | `{` |
|      23 |  152 | `	int res = 0; /* Assume attribute does not exists */` |
|      23 |  153 | `	if( nArg > 1 ){` |
|       - |  154 | `		ph7_class *pClass;` |
|      23 |  155 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      23 |  156 | `		if( pClass ){` |
|       - |  157 | `			const char *zName;` |
|       - |  158 | `			int nLen;` |
|       - |  159 | `			/* Extract attribute name */` |
|      23 |  160 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|      23 |  161 | `			if( nLen > 0 ){` |
|       - |  162 | `				/* Perform the lookup in the attribute and method table */` |
|      20 |  163 | `				if( SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen) != 0` |
|      16 |  164 | `					\|\| SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|       - |  165 | `						/* property exists,flag that */` |
|      16 |  166 | `						res = 1;` |
|       7 |  167 | `				}` |
|       - |  168 | `				/* A DYNAMIC (runtime-added) property lives on the INSTANCE's` |
|       - |  169 | `				 * attribute table, not the class's — php reports those too` |
|       - |  170 | `				 * (band A #3b; pre-fix property_exists() was blind to them). */` |
|      23 |  171 | `				if( res == 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|       3 |  172 | `					ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       3 |  173 | `					if( pThis && SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nLen) != 0 ){` |
|     ! 0 |  174 | `						res = 1;` |
|     ! 0 |  175 | `					}` |
|       1 |  176 | `				}` |
|      10 |  177 | `			}` |
|      10 |  178 | `		}` |
|      10 |  179 | `	}` |
|      23 |  180 | `	ph7_result_bool(pCtx,res);` |
|      23 |  181 | `	return PH7_OK;` |
|       3 |  182 | `}` |
|       - |  183 | `/*` |
|       - |  184 | ` * bool method_exists(mixed $class,string $method)` |
|       - |  185 | ` *   Checks if the given method is a class member.` |
|       - |  186 | ` * Parameters` |
|       - |  187 | ` *  class` |
|       - |  188 | ` *   The class name or an object of the class to test for` |
|       - |  189 | ` * property` |
|       - |  190 | ` *  The name of the method` |
|       - |  191 | ` * Return` |
|       - |  192 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|       - |  193 | ` */` |
|      26 |  194 | `PH7_PRIVATE int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  195 | `{` |
|      29 |  196 | `	int res = 0; /* Assume method does not exists */` |
|      29 |  197 | `	if( nArg > 1 ){` |
|       - |  198 | `		ph7_class *pClass;` |
|      29 |  199 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      29 |  200 | `		if( pClass ){` |
|       - |  201 | `			const char *zName;` |
|       - |  202 | `			int nLen;` |
|       - |  203 | `			/* Extract method name */` |
|      25 |  204 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|      25 |  205 | `			if( nLen > 0 ){` |
|       - |  206 | `				/* Perform the lookup in the method table */` |
|      25 |  207 | `				if( SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|       - |  208 | `					/* method exists,flag that */` |
|      19 |  209 | `					res = 1;` |
|       8 |  210 | `				}` |
|      11 |  211 | `			}` |
|      11 |  212 | `		}` |
|      13 |  213 | `	}` |
|      29 |  214 | `	ph7_result_bool(pCtx,res);` |
|      29 |  215 | `	return PH7_OK;` |
|       3 |  216 | `}` |
|       - |  217 | `/*` |
|       - |  218 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|       - |  219 | ` *   Checks if the class has been defined.` |
|       - |  220 | ` * Parameters` |
|       - |  221 | ` *  class_name` |
|       - |  222 | ` *   The class name. The name is matched in a case-sensitive manner` |
|       - |  223 | ` *   unlinke the standard PHP engine.` |
|       - |  224 | ` *  autoload` |
|       - |  225 | ` *   Whether or not to call __autoload by default.` |
|       - |  226 | ` * Return` |
|       - |  227 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|       - |  228 | ` */` |
|     100 |  229 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  230 | `{` |
|     105 |  231 | `	int res = 0; /* Assume class does not exist */` |
|     105 |  232 | `	if( nArg > 0 ){` |
|     105 |  233 | `		SyHashEntry *pEntry = 0;` |
|       - |  234 | `		const char *zName;` |
|       - |  235 | `		int nLen;` |
|     105 |  236 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  237 | `		sxu32 nName;` |
|       - |  238 | `		/* Extract given name */` |
|     105 |  239 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|     105 |  240 | `		if( nArg >= 2 ){` |
|       6 |  241 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|       2 |  242 | `		}` |
|       - |  243 | `		/* Strip a leading '\' (global-namespace anchor) — this builtin hashes` |
|       - |  244 | `		 * hClass directly, bypassing PH7_VmExtractClass, so anchor here. */` |
|     105 |  245 | `		nName = (sxu32)nLen;` |
|     105 |  246 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|     105 |  247 | `		if( nName > 0 ){` |
|       - |  248 | `			/* Perform a hash lookup first */` |
|     101 |  249 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|      48 |  250 | `		}` |
|       - |  251 | `		/* Gate autoload on the ORIGINAL length (nLen), not the stripped nName:` |
|       - |  252 | `		 * php autoloads a lone "\" (with the empty stripped name) but not "". */` |
|     105 |  253 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|       - |  254 | `			/* Try autoload, then re-check */` |
|      28 |  255 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|      28 |  256 | `			if( pClass ){` |
|       9 |  257 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|       3 |  258 | `			}` |
|      12 |  259 | `		}` |
|     105 |  260 | `		if( pEntry ){` |
|       - |  261 | `			/* Walk the collision chain: return TRUE only for concrete or abstract classes,` |
|       - |  262 | `			 * not for interfaces or traits (matching PHP behavior). */` |
|      83 |  263 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|      85 |  264 | `			while( pClass ){` |
|      83 |  265 | `				if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|      81 |  266 | `					res = 1;` |
|      81 |  267 | `					break;` |
|       - |  268 | `				}` |
|       3 |  269 | `				pClass = pClass->pNextName;` |
|       1 |  270 | `			}` |
|      39 |  271 | `		}` |
|      50 |  272 | `	}` |
|     105 |  273 | `	ph7_result_bool(pCtx,res);` |
|     105 |  274 | `	return PH7_OK;` |
|       5 |  275 | `}` |
|       - |  276 | `/*` |
|       - |  277 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|       - |  278 | ` *   Checks if the interface has been defined.` |
|       - |  279 | ` * Parameters` |
|       - |  280 | ` *  class_name` |
|       - |  281 | ` *   The class name. The name is matched in a case-sensitive manner` |
|       - |  282 | ` *   unlinke the standard PHP engine.` |
|       - |  283 | ` *  autoload` |
|       - |  284 | ` *   Whether or not to call __autoload by default.` |
|       - |  285 | ` * Return` |
|       - |  286 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|       - |  287 | ` */` |
|      32 |  288 | `PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  289 | `{` |
|      34 |  290 | `	int res = 0; /* Assume interface does not exist */` |
|      34 |  291 | `	if( nArg > 0 ){` |
|      34 |  292 | `		SyHashEntry *pEntry = 0;` |
|       - |  293 | `		const char *zName;` |
|       - |  294 | `		int nLen;` |
|      34 |  295 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  296 | `		sxu32 nName;` |
|       - |  297 | `		/* Extract given name */` |
|      34 |  298 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|      34 |  299 | `		if( nArg >= 2 ){` |
|     ! 0 |  300 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|     ! 0 |  301 | `		}` |
|       - |  302 | `		/* Strip a leading '\' (global-namespace anchor); this builtin bypasses` |
|       - |  303 | `		 * PH7_VmExtractClass and hashes hClass directly. */` |
|      34 |  304 | `		nName = (sxu32)nLen;` |
|      34 |  305 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|       - |  306 | `		/* Perform a hash lookup */` |
|      34 |  307 | `		if( nName > 0 ){` |
|      34 |  308 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|      16 |  309 | `		}` |
|       - |  310 | `		/* Gate autoload on the ORIGINAL length (nLen): php autoloads a lone` |
|       - |  311 | `		 * "\" with the empty stripped name, but not a truly empty "". */` |
|      34 |  312 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|       - |  313 | `			/* Try autoload — pass iLoadable=FALSE so we get interfaces too */` |
|       3 |  314 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|       3 |  315 | `			if( pClass ){` |
|     ! 0 |  316 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|     ! 0 |  317 | `			}` |
|       1 |  318 | `		}` |
|      34 |  319 | `		if( pEntry ){` |
|      32 |  320 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|      34 |  321 | `			while( pClass ){` |
|      32 |  322 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|       - |  323 | `					/* interface is available */` |
|      30 |  324 | `					res = 1;` |
|      30 |  325 | `					break;` |
|       - |  326 | `				}` |
|       - |  327 | `				/* Next with the same name */` |
|       3 |  328 | `				pClass = pClass->pNextName;` |
|       1 |  329 | `			}` |
|      15 |  330 | `		}` |
|      16 |  331 | `	}` |
|      34 |  332 | `	ph7_result_bool(pCtx,res);` |
|      34 |  333 | `	return PH7_OK;` |
|       2 |  334 | `}` |
|       - |  335 | `/*` |
|       - |  336 | ` * bool trait_exists(string $trait [, bool $autoload = true ] )` |
|       - |  337 | ` *   Checks if the trait has been defined.` |
|       - |  338 | ` * Parameters` |
|       - |  339 | ` *  trait` |
|       - |  340 | ` *   The trait name (case-sensitive here, unlike the standard PHP engine).` |
|       - |  341 | ` *  autoload` |
|       - |  342 | ` *   Whether to invoke autoloading if the trait is not yet defined.` |
|       - |  343 | ` * Return` |
|       - |  344 | ` *   TRUE if trait is a defined trait, FALSE otherwise.` |
|       - |  345 | ` */` |
|      14 |  346 | `PH7_PRIVATE int vm_builtin_trait_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  347 | `{` |
|      16 |  348 | `	int res = 0; /* Assume trait does not exist */` |
|      16 |  349 | `	if( nArg > 0 ){` |
|      16 |  350 | `		SyHashEntry *pEntry = 0;` |
|       - |  351 | `		const char *zName;` |
|       - |  352 | `		int nLen;` |
|      16 |  353 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  354 | `		sxu32 nName;` |
|       - |  355 | `		/* Extract given name */` |
|      16 |  356 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|      16 |  357 | `		if( nArg >= 2 ){` |
|       3 |  358 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|       1 |  359 | `		}` |
|       - |  360 | `		/* Strip a leading '\' (global-namespace anchor); this builtin bypasses` |
|       - |  361 | `		 * PH7_VmExtractClass and hashes hClass directly. */` |
|      16 |  362 | `		nName = (sxu32)nLen;` |
|      16 |  363 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|       - |  364 | `		/* Perform a hash lookup */` |
|      16 |  365 | `		if( nName > 0 ){` |
|      16 |  366 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|       7 |  367 | `		}` |
|       - |  368 | `		/* Gate autoload on the ORIGINAL length (nLen): php autoloads a lone` |
|       - |  369 | `		 * "\" with the empty stripped name, but not a truly empty "". */` |
|      16 |  370 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|       - |  371 | `			/* Try autoload — pass iLoadable=FALSE so we get traits too */` |
|       3 |  372 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|       3 |  373 | `			if( pClass ){` |
|     ! 0 |  374 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|     ! 0 |  375 | `			}` |
|       1 |  376 | `		}` |
|      16 |  377 | `		if( pEntry ){` |
|      12 |  378 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|      18 |  379 | `			while( pClass ){` |
|      12 |  380 | `				if( pClass->iFlags & PH7_CLASS_TRAIT ){` |
|       - |  381 | `					/* trait is available */` |
|       6 |  382 | `					res = 1;` |
|       6 |  383 | `					break;` |
|       - |  384 | `				}` |
|       - |  385 | `				/* Next with the same name */` |
|       7 |  386 | `				pClass = pClass->pNextName;` |
|       1 |  387 | `			}` |
|       5 |  388 | `		}` |
|       7 |  389 | `	}` |
|      16 |  390 | `	ph7_result_bool(pCtx,res);` |
|      16 |  391 | `	return PH7_OK;` |
|       2 |  392 | `}` |
|       - |  393 | `/*` |
|       - |  394 | ` * bool class_alias([string $original[,string $alias ]])` |
|       - |  395 | ` *   Creates an alias for a class.` |
|       - |  396 | ` * Parameters` |
|       - |  397 | ` *  original` |
|       - |  398 | ` *    The original class.` |
|       - |  399 | ` *  alias` |
|       - |  400 | ` *   The alias name for the class.` |
|       - |  401 | ` * Return` |
|       - |  402 | ` *   Returns TRUE on success or FALSE on failure.` |
|       - |  403 | ` */` |
|       4 |  404 | `PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  405 | `{` |
|       - |  406 | `	const char *zOld,*zNew;` |
|       - |  407 | `	int nOldLen,nNewLen;` |
|       - |  408 | `	sxu32 nOld,nNew;` |
|       - |  409 | `	SyHashEntry *pEntry;` |
|       - |  410 | `	ph7_class *pClass;` |
|       - |  411 | `	char *zDup;` |
|       - |  412 | `	sxi32 rc;` |
|       6 |  413 | `	if( nArg < 2 ){` |
|       - |  414 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  415 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  416 | `		return PH7_OK;` |
|       - |  417 | `	}` |
|       - |  418 | `	/* Extract old class name */` |
|       6 |  419 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|       - |  420 | `	/* Extract alias name */` |
|       6 |  421 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|       - |  422 | `	/* Strip a leading '\' (global-namespace anchor) from BOTH names: php` |
|       - |  423 | `	 * resolves the target and stores the alias without it, so class_exists()` |
|       - |  424 | `	 * on the plain name then matches. */` |
|       6 |  425 | `	nOld = (sxu32)nOldLen;` |
|       6 |  426 | `	nNew = (sxu32)nNewLen;` |
|       6 |  427 | `	PH7_VmClassNameAnchor(&zOld,&nOld);` |
|       6 |  428 | `	PH7_VmClassNameAnchor(&zNew,&nNew);` |
|       6 |  429 | `	if( nNew < 1 ){` |
|       - |  430 | `		/* Invalid alias name,return FALSE */` |
|     ! 0 |  431 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  432 | `		return PH7_OK;` |
|       - |  433 | `	}` |
|       - |  434 | `	/* Perform a hash lookup */` |
|       6 |  435 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,nOld);` |
|       6 |  436 | `	if( pEntry ==  0 ){` |
|       - |  437 | `		/* No such class,return FALSE */` |
|     ! 0 |  438 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  439 | `		return PH7_OK;` |
|       - |  440 | `	}` |
|       - |  441 | `	/* Point to the class */` |
|       6 |  442 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|       - |  443 | `	/* Duplicate alias name */` |
|       6 |  444 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,nNew);` |
|       6 |  445 | `	if( zDup == 0 ){` |
|       - |  446 | `		/* Out of memory,return FALSE */` |
|     ! 0 |  447 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  448 | `		return PH7_OK;` |
|       - |  449 | `	}` |
|       - |  450 | `	/* Create the alias */` |
|       6 |  451 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,nNew,pClass);` |
|       6 |  452 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  453 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|     ! 0 |  454 | `	}` |
|       6 |  455 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|       6 |  456 | `	return PH7_OK;` |
|       4 |  457 | `}` |
|       - |  458 | `/*` |
|       - |  459 | ` * array get_declared_classes(void)` |
|       - |  460 | ` *   Returns an array with the name of the defined classes` |
|       - |  461 | ` * Parameters` |
|       - |  462 | ` *  None` |
|       - |  463 | ` * Return` |
|       - |  464 | ` *   Returns an array of the names of the declared classes` |
|       - |  465 | ` *   in the current script.` |
|       - |  466 | ` * Note:` |
|       - |  467 | ` *   NULL is returned on failure.` |
|       - |  468 | ` */` |
|       2 |  469 | `PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  470 | `{` |
|       - |  471 | `	ph7_value *pName,*pArray;` |
|       - |  472 | `	SyHashEntry *pEntry;` |
|       - |  473 | `	/* Create a new array first */` |
|       3 |  474 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 |  475 | `	pName = ph7_context_new_scalar(pCtx);` |
|       3 |  476 | `	if( pArray == 0 \|\| pName == 0){` |
|     ! 0 |  477 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 |  478 | `		SXUNUSED(apArg);` |
|       - |  479 | `		/* Out of memory,return NULL */` |
|     ! 0 |  480 | `		ph7_result_null(pCtx);` |
|     ! 0 |  481 | `		return PH7_OK;` |
|       - |  482 | `	}` |
|       - |  483 | `	/* Fill the array with the defined classes */` |
|       3 |  484 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|     388 |  485 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|     385 |  486 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|       - |  487 | `		/* Do not register classes defined as interfaces */` |
|     385 |  488 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     347 |  489 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|       - |  490 | `			/* insert class name */` |
|     347 |  491 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|       - |  492 | `			/* Reset the cursor */` |
|     347 |  493 | `			ph7_value_reset_string_cursor(pName);` |
|     173 |  494 | `		}` |
|       1 |  495 | `	}` |
|       - |  496 | `	/* Return the created array */` |
|       3 |  497 | `	ph7_result_value(pCtx,pArray);` |
|       3 |  498 | `	return PH7_OK;` |
|       2 |  499 | `}` |
|       - |  500 | `/*` |
|       - |  501 | ` * array get_declared_interfaces(void)` |
|       - |  502 | ` *   Returns an array with the name of the defined interfaces` |
|       - |  503 | ` * Parameters` |
|       - |  504 | ` *  None` |
|       - |  505 | ` * Return` |
|       - |  506 | ` *   Returns an array of the names of the declared interfaces` |
|       - |  507 | ` *   in the current script.` |
|       - |  508 | ` * Note:` |
|       - |  509 | ` *   NULL is returned on failure.` |
|       - |  510 | ` */` |
|       2 |  511 | `PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  512 | `{` |
|       - |  513 | `	ph7_value *pName,*pArray;` |
|       - |  514 | `	SyHashEntry *pEntry;` |
|       - |  515 | `	/* Create a new array first */` |
|       3 |  516 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 |  517 | `	pName = ph7_context_new_scalar(pCtx);` |
|       3 |  518 | `	if( pArray == 0 \|\| pName == 0 ){` |
|     ! 0 |  519 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 |  520 | `		SXUNUSED(apArg);` |
|       - |  521 | `		/* Out of memory,return NULL */` |
|     ! 0 |  522 | `		ph7_result_null(pCtx);` |
|     ! 0 |  523 | `		return PH7_OK;` |
|       - |  524 | `	}` |
|       - |  525 | `	/* Fill the array with the defined classes */` |
|       3 |  526 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|     390 |  527 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|     387 |  528 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|       - |  529 | `		/* Register classes defined as interfaces only */` |
|     387 |  530 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|      41 |  531 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|       - |  532 | `			/* insert interface name */` |
|      41 |  533 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|       - |  534 | `			/* Reset the cursor */` |
|      41 |  535 | `			ph7_value_reset_string_cursor(pName);` |
|      20 |  536 | `		}` |
|       1 |  537 | `	}` |
|       - |  538 | `	/* Return the created array */` |
|       3 |  539 | `	ph7_result_value(pCtx,pArray);` |
|       3 |  540 | `	return PH7_OK;` |
|       2 |  541 | `}` |
|       - |  542 | `/*` |
|       - |  543 | ` * array get_class_methods(string/object $class_name)` |
|       - |  544 | ` *   Returns an array with the name of the class methods` |
|       - |  545 | ` * Parameters` |
|       - |  546 | ` *  class_name` |
|       - |  547 | ` *  The class name or class instance` |
|       - |  548 | ` * Return` |
|       - |  549 | ` *  Returns an array of method names defined for the class specified by class_name.` |
|       - |  550 | ` *  In case of an error, it returns NULL.` |
|       - |  551 | ` * Note:` |
|       - |  552 | ` *   NULL is returned on failure.` |
|       - |  553 | ` */` |
|      10 |  554 | `PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  555 | `{` |
|       - |  556 | `	ph7_value *pName,*pArray;` |
|       - |  557 | `	SyHashEntry *pEntry;` |
|       - |  558 | `	ph7_class *pClass;` |
|       - |  559 | `	/* Extract the target class first */` |
|      12 |  560 | `	pClass = 0;` |
|      12 |  561 | `	if( nArg > 0 ){` |
|      12 |  562 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       5 |  563 | `	}` |
|      12 |  564 | `	if( pClass == 0 ){` |
|       - |  565 | `		/* No such class,return NULL */` |
|       3 |  566 | `		ph7_result_null(pCtx);` |
|       3 |  567 | `		return PH7_OK;` |
|       - |  568 | `	}` |
|       - |  569 | `	/* Create a new array  */` |
|      10 |  570 | `	pArray = ph7_context_new_array(pCtx);` |
|      10 |  571 | `	pName = ph7_context_new_scalar(pCtx);` |
|      10 |  572 | `	if( pArray == 0 \|\| pName == 0){` |
|       - |  573 | `		/* Out of memory,return NULL */` |
|     ! 0 |  574 | `		ph7_result_null(pCtx);` |
|     ! 0 |  575 | `		return PH7_OK;` |
|       - |  576 | `	}` |
|       - |  577 | `	/* Fill the array with the defined methods, in php's order: the class's own` |
|       - |  578 | `	 * methods in DECLARATION order, then each ancestor's in ITS declaration` |
|       - |  579 | `	 * order (band A #4 — the raw hash walk returned reverse-insertion/LIFO` |
|       - |  580 | `	 * order). SyHash iterates newest-first, so a reversed walk restores` |
|       - |  581 | `	 * insertion order; grouping by declaring class (sFunc.pUserData, the class` |
|       - |  582 | `	 * a method was compiled into) walks own-then-parent like php. An override` |
|       - |  583 | `	 * lives once in the hash under the subclass, so no dedup is needed. */` |
|       - |  584 | `	{` |
|       - |  585 | `		SySet aTmp;` |
|       - |  586 | `		SyHashEntry **apEntry;` |
|       - |  587 | `		ph7_class *pLevel;` |
|       - |  588 | `		sxu32 n;` |
|       - |  589 | `		/* Names are emitted from each entry's HASH KEY, not sFunc.sName: a trait` |
|       - |  590 | ``		 * adaptation alias (`hi as bHi`) keeps the original name in its method`` |
|       - |  591 | `		 * struct while the key carries the alias — php lists the alias. */` |
|      10 |  592 | `		SySetInit(&aTmp,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|      10 |  593 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  594 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      28 |  595 | `			SySetPut(&aTmp,(const void *)&pEntry);` |
|       2 |  596 | `		}` |
|      10 |  597 | `		apEntry = (SyHashEntry **)SySetBasePtr(&aTmp);` |
|      20 |  598 | `		for( pLevel = pClass; pLevel; pLevel = pLevel->pBase ){` |
|       - |  599 | `			/* Collect this level's methods, then emit in DECLARATION order` |
|       - |  600 | `			 * (sorted by nLine — same-level methods share a source file; a` |
|       - |  601 | `			 * hash-order fallback covers line-less internal methods). */` |
|       - |  602 | `			SySet aLvl;` |
|       - |  603 | `			SyHashEntry **apLvl;` |
|       - |  604 | `			sxu32 i,j;` |
|      12 |  605 | `			SySetInit(&aLvl,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|       - |  606 | `			/* Hash-order fallback for same-line methods: the class's OWN entries` |
|       - |  607 | `			 * come out in declaration order when walked newest-first, while` |
|       - |  608 | `			 * inherited copies (inserted by PH7_ClassInherit's walk of the base` |
|       - |  609 | `			 * hash) come out in declaration order walked oldest-first. */` |
|      46 |  610 | `			for( n = 0; n < SySetUsed(&aTmp); n++ ){` |
|      36 |  611 | `				sxu32 nPick = (pLevel == pClass) ? (SySetUsed(&aTmp) - 1 - n) : n;` |
|      36 |  612 | `				ph7_class_method *pMethod = (ph7_class_method *)apEntry[nPick]->pUserData;` |
|      36 |  613 | `				ph7_class *pDecl = (ph7_class *)pMethod->sFunc.pUserData;` |
|      36 |  614 | `				if( pDecl != pLevel ){` |
|       - |  615 | `					/* A declarer outside the base chain (a used trait, or none)` |
|       - |  616 | `					 * counts as the class's own level, like php. */` |
|       - |  617 | `					ph7_class *pWalk;` |
|      16 |  618 | `					if( pLevel != pClass \|\| pDecl == pClass ){` |
|       5 |  619 | `						continue;` |
|       - |  620 | `					}` |
|      22 |  621 | `					for( pWalk = pClass; pWalk; pWalk = pWalk->pBase ){` |
|      16 |  622 | `						if( pWalk == pDecl ){` |
|       5 |  623 | `							break;` |
|       - |  624 | `						}` |
|       7 |  625 | `					}` |
|      12 |  626 | `					if( pWalk != 0 ){` |
|       5 |  627 | `						continue; /* in-chain: its own level emits it */` |
|       - |  628 | `					}` |
|       3 |  629 | `				}` |
|      28 |  630 | `				SySetPut(&aLvl,(const void *)&apEntry[nPick]);` |
|      15 |  631 | `			}` |
|      12 |  632 | `			apLvl = (SyHashEntry **)SySetBasePtr(&aLvl);` |
|       - |  633 | `			/* Insertion sort by declaration line (stable) */` |
|      28 |  634 | `			for( i = 1; i < SySetUsed(&aLvl); i++ ){` |
|      18 |  635 | `				SyHashEntry *pKey = apLvl[i];` |
|      18 |  636 | `				for( j = i; j > 0 && ((ph7_class_method *)apLvl[j-1]->pUserData)->nLine` |
|      16 |  637 | `						> ((ph7_class_method *)pKey->pUserData)->nLine; j-- ){` |
|     ! 0 |  638 | `					apLvl[j] = apLvl[j-1];` |
|     ! 0 |  639 | `				}` |
|      18 |  640 | `				apLvl[j] = pKey;` |
|      10 |  641 | `			}` |
|      38 |  642 | `			for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|       - |  643 | `				/* Insert method name (the hash key: alias-aware) */` |
|      28 |  644 | `				ph7_value_string(pName,(const char *)apLvl[i]->pKey,(int)apLvl[i]->nKeyLen);` |
|      28 |  645 | `				ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|       - |  646 | `				/* Reset the cursor */` |
|      28 |  647 | `				ph7_value_reset_string_cursor(pName);` |
|      15 |  648 | `			}` |
|      12 |  649 | `			SySetRelease(&aLvl);` |
|       7 |  650 | `		}` |
|      10 |  651 | `		SySetRelease(&aTmp);` |
|       - |  652 | `	}` |
|       - |  653 | `	/* Return the created array */` |
|      10 |  654 | `	ph7_result_value(pCtx,pArray);` |
|       - |  655 | `	/*` |
|       - |  656 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - |  657 | `	 * automatically as soon we return from this foreign function.` |
|       - |  658 | `	 */` |
|      10 |  659 | `	return PH7_OK;` |
|       7 |  660 | `}` |
|       - |  661 | `/*` |
|       - |  662 | ` * This function return TRUE(1) if the given class attribute stored` |
|       - |  663 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|       - |  664 | ` * from the current scope.Otherwise FALSE is returned.` |
|       - |  665 | ` */` |
| 3137730 |  666 | `PH7_PRIVATE int PH7_VmClassMemberAccess(` |
|       - |  667 | `	ph7_vm *pVm,               /* Target VM */` |
|       - |  668 | `	ph7_class *pClass,         /* Target Class */` |
|       - |  669 | `	const SyString *pAttrName, /* Attribute name */` |
|       - |  670 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|       - |  671 | `	int bLog                   /* TRUE to log forbidden access. */` |
|       - |  672 | `	)` |
|       5 |  673 | `{` |
| 3137735 |  674 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
| 2925067 |  675 | `		VmFrame *pFrame = pVm->pFrame;` |
|       - |  676 | `		ph7_vm_func *pVmFunc;` |
|       - |  677 | `		ph7_class *pCallerScope;` |
| 2925125 |  678 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|       - |  679 | `			/* Safely ignore the exception frame */` |
|      62 |  680 | `			pFrame = pFrame->pParent;` |
|       4 |  681 | `		}` |
| 2925067 |  682 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       - |  683 | `		/* The calling scope is the executing method's declaring class — OR, for a bound closure` |
|       - |  684 | `		 * (Closure::bindTo/call), the explicit scope override carried on the frame (Increment 2). */` |
| 2925067 |  685 | `		if( pFrame->pBoundScope ){` |
|      19 |  686 | `			pCallerScope = pFrame->pBoundScope;` |
| 2925058 |  687 | `		}else if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
| 2924831 |  688 | `			pCallerScope = (ph7_class *)pVmFunc->pUserData;` |
| 1462636 |  689 | `		}else if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|       - |  690 | `			/* A closure/arrow-fn defined inside a class carries its creation-site` |
|       - |  691 | `			 * class in pUserData (stamped by OP_LOAD_CLOSURE via` |
|       - |  692 | ``			 * PH7_VmPeekDeclaringClass, the same scope `self::`/`parent::` resolve`` |
|       - |  693 | `			 * against inside the body). php binds that class as the closure's scope,` |
|       - |  694 | ``			 * so `$this->privateMethod()` / `self::$private` inside the closure are`` |
|       - |  695 | `			 * allowed — an explicit Closure::bindTo/bind rebind still wins above via` |
|       - |  696 | `			 * pBoundScope. */` |
|      39 |  697 | `			pCallerScope = (ph7_class *)pVmFunc->pUserData;` |
|     204 |  698 | `		}else if( pVm->pConstEvalClass ){` |
|       - |  699 | `			/* Constant/property initializer bytecode runs without a method` |
|       - |  700 | `			 * frame; its scope is the class being initialized (php: a private` |
|       - |  701 | `			 * constant is reachable from its own class's initializers). */` |
|       3 |  702 | `			pCallerScope = pVm->pConstEvalClass;` |
|       2 |  703 | `		}else{` |
|     183 |  704 | `			goto dis; /* Not in a class scope: access is forbidden */` |
|       - |  705 | `		}` |
| 2924889 |  706 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       - |  707 | `			/* php grants private access by DECLARING class: the caller's own` |
|       - |  708 | `			 * class must declare a private attribute of this name (a base` |
|       - |  709 | `			 * method touching its own private on a CHILD instance passes; a` |
|       - |  710 | `			 * child method touching an inherited base-private fails). An attr` |
|       - |  711 | `			 * whose declaring "class" is a TRAIT behaves as if declared by the` |
|       - |  712 | `			 * adopting class. Fallbacks: the caller being a trait used by the` |
|       - |  713 | `			 * instance's class (legacy trait-body scope), or — when the caller` |
|       - |  714 | `			 * class carries no such attr entry at all — the legacy exact-class` |
|       - |  715 | `			 * match (dynamic props and other non-declared shapes). */` |
|   12765 |  716 | `			ph7_class *pCaller = pCallerScope;` |
|   19145 |  717 | `			SyHashEntry *pOwnE = SyHashGet(&pCaller->hAttr,` |
|   12760 |  718 | `				(const void *)pAttrName->zString,pAttrName->nByte);` |
|   12765 |  719 | `			ph7_class_attr *pOwn = pOwnE ? (ph7_class_attr *)pOwnE->pUserData : 0;` |
|   12765 |  720 | `			int bGranted = 0;` |
|   12765 |  721 | `			if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|   10754 |  722 | `				if( pOwn->pDeclClass == 0` |
|   10754 |  723 | `				 \|\| pOwn->pDeclClass == pCaller` |
|    6340 |  724 | `				 \|\| (pOwn->pDeclClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|   10737 |  725 | `					bGranted = 1;` |
|    5371 |  726 | `				}` |
|    7388 |  727 | `			}else if( pOwn == 0 && pCaller == pClass ){` |
|     998 |  728 | `				bGranted = 1;` |
|     497 |  729 | `			}` |
|   12765 |  730 | `			if( !bGranted ){` |
|       - |  731 | `				/* Check if the caller is a trait used by pClass */` |
|       - |  732 | `				ph7_class **apTrait;` |
|       - |  733 | `				sxu32 nTrait,k;` |
|    1039 |  734 | `				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|    1039 |  735 | `				nTrait = SySetUsed(&pClass->aTrait);` |
|    1039 |  736 | `				for(k = 0; k < nTrait; k++){` |
|     ! 0 |  737 | `					if( apTrait[k] == pCaller ){` |
|     ! 0 |  738 | `						bGranted = 1;` |
|     ! 0 |  739 | `						break;` |
|       - |  740 | `					}` |
|     ! 0 |  741 | `				}` |
|     517 |  742 | `			}` |
|   12765 |  743 | `			if( !bGranted && (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|       - |  744 | `				/* The target "class" is itself a trait: a trait-copied private` |
|       - |  745 | `				 * member behaves as if declared in the adopting class, so a` |
|       - |  746 | `` 				 * caller that USES the trait gets access (php: `self::s()` `` |
|       - |  747 | `				 * from a using class's static method reaching a trait-private` |
|       - |  748 | `				 * static — the callee resolves via the shared trait VmFunc` |
|       - |  749 | `				 * whose owner is the trait, not the class). */` |
|       - |  750 | `				ph7_class **apTrait;` |
|       - |  751 | `				sxu32 nTrait,k;` |
|    1005 |  752 | `				apTrait = (ph7_class **)SySetBasePtr(&pCaller->aTrait);` |
|    1005 |  753 | `				nTrait = SySetUsed(&pCaller->aTrait);` |
|    1005 |  754 | `				for(k = 0; k < nTrait; k++){` |
|    1005 |  755 | `					if( apTrait[k] == pClass ){` |
|    1005 |  756 | `						bGranted = 1;` |
|    1005 |  757 | `						break;` |
|       - |  758 | `					}` |
|     ! 0 |  759 | `				}` |
|     500 |  760 | `			}` |
|   12765 |  761 | `			if( !bGranted ){` |
|      38 |  762 | `				goto dis; /* Access is forbidden */` |
|       - |  763 | `			}` |
|    6368 |  764 | `		}else{` |
|       - |  765 | `			/* Protected */` |
| 2912129 |  766 | `			ph7_class *pBase = pCallerScope;` |
|       - |  767 | `			/* php checks the hierarchy against the class that INTRODUCES the member,` |
|       - |  768 | `			 * not the one that (re)declares the override we resolved. A protected` |
|       - |  769 | `			 * member declared in a common ancestor B and overridden in a child C is` |
|       - |  770 | `			 * still reachable from a SIBLING scope S (also extending B) — S and C both` |
|       - |  771 | `			 * descend from B. Walk pClass up to the top-most ancestor that genuinely` |
|       - |  772 | `			 * declares a member of this name (a method via sFunc.pUserData, or an attr` |
|       - |  773 | `			 * via pDeclClass — hMethod/hAttr also carry inherited copies, so match on the` |
|       - |  774 | `			 * true declaring class) and test the hierarchy against that introducing` |
|       - |  775 | ``			 * class. `child_only` (declared solely in C) keeps pClass and stays denied`` |
|       - |  776 | `			 * from a sibling, matching php. */` |
| 2912129 |  777 | `			ph7_class *pIntro = pClass;` |
|       - |  778 | `			ph7_class *pAnc;` |
| 6048331 |  779 | `			for( pAnc = pClass ; pAnc ; pAnc = pAnc->pBase ){` |
| 3136207 |  780 | `				ph7_class_method *pAncMeth = PH7_ClassExtractMethod(pAnc,pAttrName->zString,pAttrName->nByte);` |
| 3136207 |  781 | `				SyHashEntry *pAncAttrE = SyHashGet(&pAnc->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
| 3136207 |  782 | `				ph7_class_attr *pAncAttr = pAncAttrE ? (ph7_class_attr *)pAncAttrE->pUserData : 0;` |
| 3136207 |  783 | `				int bHere = 0;` |
| 3136207 |  784 | `				if( pAncMeth && (ph7_class *)pAncMeth->sFunc.pUserData == pAnc ){` |
|    4987 |  785 | `					bHere = 1;` |
|    2491 |  786 | `				}` |
| 3136207 |  787 | `				if( pAncAttr && (pAncAttr->pDeclClass == pAnc \|\| pAncAttr->pDeclClass == 0) ){` |
| 2907739 |  788 | `					bHere = 1;` |
| 1453867 |  789 | `				}` |
| 3136207 |  790 | `				if( bHere ){` |
| 2912721 |  791 | `					pIntro = pAnc; /* keep climbing: the LAST (highest) match wins */` |
| 1456358 |  792 | `				}` |
| 1568106 |  793 | `			}` |
|       - |  794 | `			/* Must be in the same class hierarchy as the introducing class */` |
| 2912129 |  795 | `			if( !PH7_VmInstanceOf(pIntro,pBase) && !PH7_VmInstanceOf(pBase,pIntro) ){` |
|      17 |  796 | `				int bTraitGrant = 0;` |
|      17 |  797 | `				if( (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|       - |  798 | `					/* Same trait-target rule as the private branch above */` |
|       - |  799 | `					ph7_class **apTrait;` |
|       - |  800 | `					sxu32 nTrait,k;` |
|      13 |  801 | `					apTrait = (ph7_class **)SySetBasePtr(&pBase->aTrait);` |
|      13 |  802 | `					nTrait = SySetUsed(&pBase->aTrait);` |
|      13 |  803 | `					for(k = 0; k < nTrait; k++){` |
|      11 |  804 | `						if( apTrait[k] == pClass ){` |
|      11 |  805 | `							bTraitGrant = 1;` |
|      11 |  806 | `							break;` |
|       - |  807 | `						}` |
|     ! 0 |  808 | `					}` |
|       5 |  809 | `				}` |
|      17 |  810 | `				if( !bTraitGrant ){` |
|       8 |  811 | `					goto dis; /* Access is forbidden */` |
|       - |  812 | `				}` |
|       4 |  813 | `			}` |
|       - |  814 | `		}` |
| 1462422 |  815 | `	}` |
| 3137517 |  816 | `	return 1; /* Access is granted */` |
|     109 |  817 | `dis:` |
|     223 |  818 | `	if( bLog ){` |
|     ! 0 |  819 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  820 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|     ! 0 |  821 | `			&pClass->sName,pAttrName);` |
|     ! 0 |  822 | `	}` |
|     223 |  823 | `	return 0; /* Access is forbidden */` |
| 1568870 |  824 | `}` |
|       - |  825 | `/*` |
|       - |  826 | ` * array get_class_vars(string/object $class_name)` |
|       - |  827 | ` *   Get the default properties of the class` |
|       - |  828 | ` * Parameters` |
|       - |  829 | ` *  class_name` |
|       - |  830 | ` *   The class name or class instance` |
|       - |  831 | ` * Return` |
|       - |  832 | ` *  Returns an associative array of declared properties visible from the current scope` |
|       - |  833 | ` *  with their default value. The resulting array elements are in the form` |
|       - |  834 | ` *  of varname => value.` |
|       - |  835 | ` * Note:` |
|       - |  836 | ` *   NULL is returned on failure.` |
|       - |  837 | ` */` |
|      12 |  838 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  839 | `{` |
|       - |  840 | `	ph7_value *pName,*pArray,sValue;` |
|       - |  841 | `	SyHashEntry *pEntry;` |
|       - |  842 | `	ph7_class *pClass;` |
|       - |  843 | `	/* Extract the target class first */` |
|      15 |  844 | `	pClass = 0;` |
|      15 |  845 | `	if( nArg > 0 ){` |
|      15 |  846 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       6 |  847 | `	}` |
|      15 |  848 | `	if( pClass == 0 ){` |
|       - |  849 | `		/* php screens the VALUE, not the type: anything stringifiable is accepted,` |
|       - |  850 | `		 * and a name that does not resolve to a class is a TypeError quoting the` |
|       - |  851 | `		 * stringified argument ("...must be a valid class name, Array given"). This` |
|       - |  852 | `		 * is why get_class_vars() opts out of the shared ZPP type screen in vm.c. */` |
|     ! 0 |  853 | `		int nLen = 0;` |
|     ! 0 |  854 | `		const char *zVal = "";` |
|     ! 0 |  855 | `		if( nArg > 0 ){` |
|     ! 0 |  856 | `			if( (apArg[0]->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|     ! 0 |  857 | `				zVal = "Array";` |
|     ! 0 |  858 | `				nLen = (int)sizeof("Array") - 1;` |
|     ! 0 |  859 | `			}else{` |
|     ! 0 |  860 | `				zVal = ph7_value_to_string(apArg[0],&nLen);` |
|       - |  861 | `			}` |
|     ! 0 |  862 | `		}` |
|     ! 0 |  863 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  864 | `			"get_class_vars(): Argument #1 ($class) must be a valid class name, %.*s given",` |
|     ! 0 |  865 | `			nLen,zVal);` |
|       - |  866 | `	}` |
|      15 |  867 | `	if( VmClassStaticDeferPending(pClass) ){` |
|       - |  868 | `		/* Listing the properties reads every static slot, which materializes the` |
|       - |  869 | `		 * class's static table: a default that threw at the declaration raises` |
|       - |  870 | `		 * here, as it does in php. */` |
|       5 |  871 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm,pClass);` |
|       5 |  872 | `		if( rcMat != SXRET_OK ){` |
|       5 |  873 | `			return rcMat;` |
|       - |  874 | `		}` |
|     ! 0 |  875 | `	}` |
|       - |  876 | `	/* Create a new array  */` |
|      11 |  877 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 |  878 | `	pName = ph7_context_new_scalar(pCtx);` |
|      11 |  879 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|      11 |  880 | `	if( pArray == 0 \|\| pName == 0){` |
|       - |  881 | `		/* Out of memory,return NULL */` |
|     ! 0 |  882 | `		ph7_result_null(pCtx);` |
|     ! 0 |  883 | `		return PH7_OK;` |
|       - |  884 | `	}` |
|       - |  885 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|      11 |  886 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|      23 |  887 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      15 |  888 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      15 |  889 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - |  890 | `			/* php 8.4: VIRTUAL hooked properties have no backing store —` |
|       - |  891 | `			 * get_class_vars() excludes them (raw surface) */` |
|       3 |  892 | `			continue;` |
|       - |  893 | `		}` |
|       - |  894 | `		/* Check if the access is allowed */` |
|      13 |  895 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|      13 |  896 | `			SyString *pAttrName = &pAttr->sName;` |
|      13 |  897 | `			ph7_value *pValue = 0;` |
|      13 |  898 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|       - |  899 | `				/* Static slots are computed at mount; constants lazily */` |
|       8 |  900 | `				PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr);` |
|       8 |  901 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|       5 |  902 | `			}else{` |
|       6 |  903 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       6 |  904 | `					PH7_MemObjRelease(&sValue);` |
|       - |  905 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|       6 |  906 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue,FALSE);` |
|       6 |  907 | `					pValue = &sValue;` |
|       2 |  908 | `				}` |
|       - |  909 | `			}` |
|       - |  910 | `			/* Fill in the array */` |
|      13 |  911 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|      13 |  912 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|       - |  913 | `			/* Reset the cursor */` |
|      13 |  914 | `			ph7_value_reset_string_cursor(pName);` |
|       5 |  915 | `		}` |
|       3 |  916 | `	}` |
|      11 |  917 | `	PH7_MemObjRelease(&sValue);` |
|       - |  918 | `	/* Return the created array */` |
|      11 |  919 | `	ph7_result_value(pCtx,pArray);` |
|       - |  920 | `	/*` |
|       - |  921 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - |  922 | `	 * automatically as soon we return from this foreign function.` |
|       - |  923 | `	 */` |
|      11 |  924 | `	return PH7_OK;` |
|       9 |  925 | `}` |
|       - |  926 | `/*` |
|       - |  927 | ` * array get_object_vars(object $this)` |
|       - |  928 | ` *   Gets the properties of the given object` |
|       - |  929 | ` * Parameters` |
|       - |  930 | ` *  this` |
|       - |  931 | ` *   A class instance` |
|       - |  932 | ` * Return` |
|       - |  933 | ` *  Returns an associative array of defined object accessible non-static properties` |
|       - |  934 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|       - |  935 | ` *  it will be returned with a NULL value.` |
|       - |  936 | ` * Note:` |
|       - |  937 | ` *   NULL is returned on failure.` |
|       - |  938 | ` */` |
|      50 |  939 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  940 | `{` |
|      54 |  941 | `	ph7_class_instance *pThis = 0;` |
|       - |  942 | `	ph7_value *pName,*pArray;` |
|       - |  943 | `	SyHashEntry *pEntry;` |
|      54 |  944 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|       - |  945 | `		/* Extract the target instance */` |
|      54 |  946 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      25 |  947 | `	}` |
|      54 |  948 | `	if( pThis == 0 ){` |
|       - |  949 | `		/* No such instance,return NULL */` |
|     ! 0 |  950 | `		ph7_result_null(pCtx);` |
|     ! 0 |  951 | `		return PH7_OK;` |
|       - |  952 | `	}` |
|       - |  953 | `	/* Create a new array  */` |
|      54 |  954 | `	pArray = ph7_context_new_array(pCtx);` |
|      54 |  955 | `	pName = ph7_context_new_scalar(pCtx);` |
|      54 |  956 | `	if( pArray == 0 \|\| pName == 0){` |
|       - |  957 | `		/* Out of memory,return NULL */` |
|     ! 0 |  958 | `		ph7_result_null(pCtx);` |
|     ! 0 |  959 | `		return PH7_OK;` |
|       - |  960 | `	}` |
|       - |  961 | `	/* Fill the array with the defined attribute visible from the current scope.` |
|       - |  962 | `	 * SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched mid-walk` |
|       - |  963 | `	 * runs user code that may re-enter an hAttr walk on this instance (resetting` |
|       - |  964 | `	 * the hash's single embedded loop cursor) or unset()/create properties. The` |
|       - |  965 | `	 * names point into CLASS-owned attr storage (they outlive instance mutation);` |
|       - |  966 | `	 * each is re-looked-up before use so an entry unset by an earlier hook is` |
|       - |  967 | `	 * skipped instead of read after free. */` |
|       - |  968 | `	{` |
|       - |  969 | `		SySet sNames;` |
|       - |  970 | `		SyString *aName;` |
|       - |  971 | `		sxu32 iName,nName;` |
|      54 |  972 | `		SySetInit(&sNames,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|      54 |  973 | `		SyHashResetLoopCursor(&pThis->hAttr);` |
|     228 |  974 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     178 |  975 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     178 |  976 | `			if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|       - |  977 | `				/* Only non-static/constant attributes are extracted */` |
|       6 |  978 | `				continue;` |
|       - |  979 | `			}` |
|     170 |  980 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|      89 |  981 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       3 |  982 | `				continue; /* virtual set-only property: no value to expose (php) */` |
|       - |  983 | `			}` |
|     172 |  984 | `			SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|       4 |  985 | `		}` |
|      54 |  986 | `		aName = (SyString *)SySetBasePtr(&sNames);` |
|      54 |  987 | `		nName = SySetUsed(&sNames);` |
|     222 |  988 | `		for( iName = 0 ; iName < nName ; ++iName ){` |
|     172 |  989 | `			SyString *pAttrName = &aName[iName];` |
|       - |  990 | `			VmClassAttr *pVmAttr;` |
|     172 |  991 | `			pEntry = SyHashGet(&pThis->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|     172 |  992 | `			if( pEntry == 0 ){` |
|     ! 0 |  993 | `				continue; /* unset by an earlier hook */` |
|       - |  994 | `			}` |
|     172 |  995 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - |  996 | `			/* Check if the access is allowed */` |
|     172 |  997 | `			if( PH7_VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|     136 |  998 | `				ph7_value *pValue = 0;` |
|       - |  999 | `				ph7_value sHookVal;` |
|       - | 1000 | `				sxi32 rcHk;` |
|       - | 1001 | `				/* PHP 8.4 property hooks: get_object_vars() reads through the get` |
|       - | 1002 | `				 * hook (virtual properties included); raw slot otherwise. */` |
|     136 | 1003 | `				PH7_MemObjInit(pCtx->pVm,&sHookVal);` |
|     136 | 1004 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|     136 | 1005 | `				if( rcHk == SXRET_OK ){` |
|      15 | 1006 | `					pValue = &sHookVal;` |
|     129 | 1007 | `				}else if( rcHk == SXERR_NOTFOUND ){` |
|       - | 1008 | `					/* Extract attribute */` |
|     122 | 1009 | `					pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|      63 | 1010 | `				}else{` |
|       - | 1011 | `					/* the hook threw — parked on the boundary rail; php aborts the` |
|       - | 1012 | `					 * whole builtin at the first throw (the helper's boundary gate` |
|       - | 1013 | `					 * keeps LATER hooks from running; raw values it falls back to` |
|       - | 1014 | `					 * are discarded when the throw routes) */` |
|     ! 0 | 1015 | `					PH7_MemObjRelease(&sHookVal);` |
|     ! 0 | 1016 | `					break;` |
|       - | 1017 | `				}` |
|     136 | 1018 | `				if( pValue ){` |
|       - | 1019 | `					/* Insert attribute name in the array */` |
|     136 | 1020 | `					ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|     136 | 1021 | `					ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|      66 | 1022 | `				}` |
|     136 | 1023 | `				PH7_MemObjRelease(&sHookVal);` |
|       - | 1024 | `				/* Reset the cursor */` |
|     136 | 1025 | `				ph7_value_reset_string_cursor(pName);` |
|      66 | 1026 | `			}` |
|      88 | 1027 | `		}` |
|      54 | 1028 | `		SySetRelease(&sNames);` |
|       - | 1029 | `	}` |
|       - | 1030 | `	/* Return the created array */` |
|      54 | 1031 | `	ph7_result_value(pCtx,pArray);` |
|       - | 1032 | `	/*` |
|       - | 1033 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - | 1034 | `	 * automatically as soon we return from this foreign function.` |
|       - | 1035 | `	 */` |
|      54 | 1036 | `	return PH7_OK;` |
|      29 | 1037 | `}` |
|       - | 1038 | ``/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in`` |
|       - | 1039 | ` * compile.c. Defends against compiler cycles even though interface cycle` |
|       - | 1040 | ` * detection should reject them up front. */` |
|       - | 1041 | `#define PH7_INTERFACE_WALK_MAX_DEPTH 64` |
|       - | 1042 | `/*` |
|       - | 1043 | ` * TRUE if pTarget is reachable from pIface as an ancestor interface: walk the` |
|       - | 1044 | `` * `extends` chain (pBase) AND each additional parent-interface set (aInterface),`` |
|       - | 1045 | `` * so a multiple-interface `interface C extends A, B` is recognized through BOTH`` |
|       - | 1046 | ` * A and B (php allows an interface to extend several interfaces). Recursion is` |
|       - | 1047 | ` * depth-bounded — a malformed cycle cannot run unbounded.` |
|       - | 1048 | ` */` |
| 2571468 | 1049 | `static int VmInterfaceReaches(ph7_class *pIface,ph7_class *pTarget,int iDepth)` |
|       5 | 1050 | `{` |
| 2693363 | 1051 | `	while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){` |
|       - | 1052 | `		ph7_class **apParent;` |
|       - | 1053 | `		sxu32 n;` |
| 2574649 | 1054 | `		if( pIface == pTarget ){` |
| 2452757 | 1055 | `			return TRUE;` |
|       - | 1056 | `		}` |
|       - | 1057 | `		/* Additional parent interfaces (interface X extends A, B, …) live in` |
|       - | 1058 | `		 * aInterface; the first parent stays on the pBase chain below. */` |
|  121897 | 1059 | `		apParent = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
|  121901 | 1060 | `		for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){` |
|       7 | 1061 | `			if( VmInterfaceReaches(apParent[n],pTarget,iDepth+1) ){` |
|       3 | 1062 | `				return TRUE;` |
|       - | 1063 | `			}` |
|       3 | 1064 | `		}` |
|  121895 | 1065 | `		pIface = pIface->pBase;` |
|  121895 | 1066 | `		iDepth++;` |
|       5 | 1067 | `	}` |
|  118719 | 1068 | `	return FALSE;` |
| 1285739 | 1069 | `}` |
|       - | 1070 | `/*` |
|       - | 1071 | ` * This function returns TRUE if the given class is an implemented` |
|       - | 1072 | ` * interface.Otherwise FALSE is returned.` |
|       - | 1073 | ` */` |
| 2678126 | 1074 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|       5 | 1075 | `{` |
|       - | 1076 | `	ph7_class **apInterface;` |
|       - | 1077 | `	sxu32 n;` |
| 2678131 | 1078 | `	if( SySetUsed(pSet) < 1 ){` |
|       - | 1079 | `		/* Empty interface container */` |
|  109179 | 1080 | `		return FALSE;` |
|       - | 1081 | `	}` |
|       - | 1082 | `	/* Point to the set of implemented interfaces */` |
| 2568957 | 1083 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|       - | 1084 | `	/* Perform the lookup, walking each interface's parent chain so that` |
|       - | 1085 | `	 * Iterator extends Traversable (and similar) is recognized. */` |
| 2687667 | 1086 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
| 2571467 | 1087 | `		if( VmInterfaceReaches(apInterface[n],pClass,0) ){` |
| 2452757 | 1088 | `			return TRUE;` |
|       - | 1089 | `		}` |
|   59360 | 1090 | `	}` |
|  116205 | 1091 | `	return FALSE;` |
| 1339068 | 1092 | `}` |
|       - | 1093 | `/*` |
|       - | 1094 | ` * This function returns TRUE if the given class (first argument)` |
|       - | 1095 | ` * is an instance of the main class (second argument).` |
|       - | 1096 | ` * Otherwise FALSE is returned.` |
|       - | 1097 | ` */` |
| 6905448 | 1098 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|       5 | 1099 | `{` |
|       - | 1100 | `	ph7_class *pParent;` |
|       - | 1101 | `	sxi32 rc;` |
| 6905453 | 1102 | `	if( pThis == pClass ){` |
|       - | 1103 | `		/* Instance of the same class */` |
| 4338891 | 1104 | `		return TRUE;` |
|       - | 1105 | `	}` |
|       - | 1106 | `	/* Check implemented interfaces */` |
| 2566567 | 1107 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
| 2566567 | 1108 | `	if( rc ){` |
| 2343787 | 1109 | `		return TRUE;` |
|       - | 1110 | `	}` |
|       - | 1111 | `	/* Check parent classes */` |
|  222785 | 1112 | `	pParent = pThis->pBase;` |
|  225345 | 1113 | `	while( pParent ){` |
|  113067 | 1114 | `		if( pParent == pClass ){` |
|       - | 1115 | `			/* Same instance */` |
|    1547 | 1116 | `			return TRUE;` |
|       - | 1117 | `		}` |
|       - | 1118 | `		/* Check the implemented interfaces */` |
|  111525 | 1119 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|  111525 | 1120 | `		if( rc ){` |
|  108965 | 1121 | `			return TRUE;` |
|       - | 1122 | `		}` |
|       - | 1123 | `		/* Point to the parent class */` |
|    2565 | 1124 | `		pParent = pParent->pBase;` |
|       5 | 1125 | `	}` |
|       - | 1126 | `	/* Not an instance of the the given class */` |
|  112283 | 1127 | `	return FALSE;` |
| 3452729 | 1128 | `}` |
|       - | 1129 | `/*` |
|       - | 1130 | ` * This function returns TRUE if the given class (first argument)` |
|       - | 1131 | ` * is a subclass of the main class (second argument).` |
|       - | 1132 | ` * Otherwise FALSE is returned.` |
|       - | 1133 | ` */` |
|      46 | 1134 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|       2 | 1135 | `{` |
|       - | 1136 | `	SyHashEntry *pEntry;` |
|       - | 1137 | `	SyString *pName;` |
|      82 | 1138 | `	while( pClass ){` |
|      68 | 1139 | `		pName = &pClass->sName;` |
|       - | 1140 | `		/* Query the derived hashtable for a class-hierarchy match */` |
|      68 | 1141 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|      68 | 1142 | `		if( pEntry ){` |
|      24 | 1143 | `			return TRUE;` |
|       - | 1144 | `		}` |
|       - | 1145 | `		/* Query the interfaces implemented by THIS class in the chain — so an` |
|       - | 1146 | `		 * interface implemented by a PARENT (B extends A implements I) is found,` |
|       - | 1147 | `		 * mirroring PH7_VmInstanceOf. The original code queried only the first` |
|       - | 1148 | `		 * class's aInterface, missing inherited interfaces. */` |
|      46 | 1149 | `		if( VmQueryInterfaceSet(pBase,&pClass->aInterface) ){` |
|      11 | 1150 | `			return TRUE;` |
|       - | 1151 | `		}` |
|      36 | 1152 | `		pClass = pClass->pBase;` |
|       2 | 1153 | `	}` |
|       - | 1154 | `	/* Not a subclass */` |
|      16 | 1155 | `	return FALSE;` |
|      25 | 1156 | `}` |
|       - | 1157 | `/*` |
|       - | 1158 | ` * bool is_a(object\|string $object_or_class,string $class,bool $allow_string = false)` |
|       - | 1159 | ` *   Checks if the object/class is of this class or has this class (or interface)` |
|       - | 1160 | ` *   as one of its parents.` |
|       - | 1161 | ` * Parameters` |
|       - | 1162 | ` *  object_or_class` |
|       - | 1163 | ` *   The tested object, or a class name string (only honored when allow_string is TRUE).` |
|       - | 1164 | ` * class` |
|       - | 1165 | ` *  The class or interface name to test against.` |
|       - | 1166 | ` * allow_string` |
|       - | 1167 | ` *  When TRUE, a string first argument is resolved as a class name; php's default` |
|       - | 1168 | ` *  is FALSE, so is_a() returns FALSE for a string first argument without it. The` |
|       - | 1169 | ` *  flag is IGNORED for an object first argument (php).` |
|       - | 1170 | ` * Return` |
|       - | 1171 | ` *   Returns TRUE if object_or_class is of class class or has class as one of its` |
|       - | 1172 | ` *   parents (or implements it as an interface), FALSE otherwise.` |
|       - | 1173 | ` */` |
|      48 | 1174 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1175 | `{` |
|      51 | 1176 | `	int res = 0; /* Assume FALSE by default */` |
|      51 | 1177 | `	if( nArg > 1 ){` |
|      51 | 1178 | `		ph7_class *pThisClass = 0;` |
|      51 | 1179 | `		if( ph7_value_is_object(apArg[0]) ){` |
|       - | 1180 | `			/* An object first argument: allow_string is ignored (php). */` |
|      33 | 1181 | `			pThisClass = ((ph7_class_instance *)apArg[0]->x.pOther)->pClass;` |
|      34 | 1182 | `		}else if( ph7_value_is_string(apArg[0]) && nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|       - | 1183 | `			/* A string first argument is resolved to a class ONLY when allow_string` |
|       - | 1184 | `			 * (the 3rd argument) is TRUE — valid php since 5.3.9, and a sibling of` |
|       - | 1185 | `			 * is_subclass_of()'s string form. Autoloads on a miss via` |
|       - | 1186 | `			 * PH7_VmExtractClassFromValue; a leading '\' is anchored there. */` |
|      15 | 1187 | `			pThisClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       7 | 1188 | `		}` |
|      51 | 1189 | `		if( pThisClass ){` |
|       - | 1190 | `			/* Extract the given class */` |
|      45 | 1191 | `			ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|      45 | 1192 | `			if( pClass ){` |
|       - | 1193 | `				/* Perform the query — instanceof, so the class ITSELF matches` |
|       - | 1194 | `				 * (unlike is_subclass_of, which excludes self). */` |
|      45 | 1195 | `				res = PH7_VmInstanceOf(pThisClass,pClass);` |
|      21 | 1196 | `			}` |
|      21 | 1197 | `		}` |
|      24 | 1198 | `	}` |
|       - | 1199 | `	/* Query result */` |
|      51 | 1200 | `	ph7_result_bool(pCtx,res);` |
|      51 | 1201 | `	return PH7_OK;` |
|       3 | 1202 | `}` |
|       - | 1203 | `/*` |
|       - | 1204 | ` * int spl_object_id(object $object)` |
|       - | 1205 | ` *  Return the integer object handle (per-instance id) of the given object.` |
|       - | 1206 | ` * PHL note: PHP 8 throws a TypeError when passed a non-object; PHL returns NULL` |
|       - | 1207 | ` * to stay consistent with the engine's graceful-degradation convention.` |
|       - | 1208 | ` */` |
|      58 | 1209 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 1210 | `{` |
|       - | 1211 | `	ph7_class_instance *pThis;` |
|      62 | 1212 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 1213 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1214 | `		return PH7_OK;` |
|       - | 1215 | `	}` |
|      62 | 1216 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      62 | 1217 | `	ph7_result_int64(pCtx,(ph7_int64)pThis->nObjId);` |
|      62 | 1218 | `	return PH7_OK;` |
|      33 | 1219 | `}` |
|       - | 1220 | `/*` |
|       - | 1221 | ` * string spl_object_hash(object $object)` |
|       - | 1222 | ` *  Return a 32-char hex identifier, unique and stable per live object.` |
|       - | 1223 | ` * PHL note: PHP derives this from the internal handle plus a per-process key, so` |
|       - | 1224 | ` * the exact value is NOT reproducible. PHL returns the zero-padded object id,` |
|       - | 1225 | ` * which preserves the only guaranteed properties: unique per live object, stable` |
|       - | 1226 | ` * across calls, and distinct objects -> distinct strings. A non-object returns` |
|       - | 1227 | ` * NULL (PHP 8 throws a TypeError; see spl_object_id above).` |
|       - | 1228 | ` */` |
|      14 | 1229 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1230 | `{` |
|       - | 1231 | `	ph7_class_instance *pThis;` |
|      16 | 1232 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 1233 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1234 | `		return PH7_OK;` |
|       - | 1235 | `	}` |
|      16 | 1236 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      16 | 1237 | `	ph7_result_string_format(pCtx,"%08x%08x%08x%08x",0,0,0,(unsigned int)pThis->nObjId);` |
|      16 | 1238 | `	return PH7_OK;` |
|       9 | 1239 | `}` |
|       - | 1240 | `/*` |
|       - | 1241 | ` * bool is_subclass_of(object\|string $object_or_class,string $class,bool $allow_string = true)` |
|       - | 1242 | ` *   Checks if the object/class has this class (or interface) as one of its` |
|       - | 1243 | ` *   parents — the subclass relation, which EXCLUDES the class itself.` |
|       - | 1244 | ` * Parameters` |
|       - | 1245 | ` *  object_or_class` |
|       - | 1246 | ` *   The tested object, or a class name string (honored unless allow_string is FALSE).` |
|       - | 1247 | ` * class` |
|       - | 1248 | ` *  The class or interface name to test against.` |
|       - | 1249 | ` * allow_string` |
|       - | 1250 | ` *  When FALSE, a string first argument is NOT resolved and the call returns FALSE.` |
|       - | 1251 | ` *  php's default here is TRUE (unlike is_a's FALSE). The flag is IGNORED for an` |
|       - | 1252 | ` *  object first argument (php).` |
|       - | 1253 | ` * Return` |
|       - | 1254 | ` *  Returns TRUE if object_or_class is a proper subclass of class (or implements it` |
|       - | 1255 | ` *  as an interface, directly or via a parent), FALSE otherwise.` |
|       - | 1256 | ` */` |
|      58 | 1257 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1258 | `{` |
|      60 | 1259 | `	int res = 0; /* Assume FALSE by default */` |
|      60 | 1260 | `	if( nArg > 1 ){` |
|      60 | 1261 | `		ph7_class *pClass = 0;` |
|      60 | 1262 | `		if( ph7_value_is_object(apArg[0]) ){` |
|       - | 1263 | `			/* An object first argument: allow_string is ignored (php). */` |
|      20 | 1264 | `			pClass = ((ph7_class_instance *)apArg[0]->x.pOther)->pClass;` |
|      51 | 1265 | `		}else if( ph7_value_is_string(apArg[0]) && (nArg < 3 \|\| ph7_value_to_bool(apArg[2])) ){` |
|       - | 1266 | `			/* A string first argument is resolved as a class name UNLESS allow_string` |
|       - | 1267 | `			 * (the 3rd argument) is explicitly FALSE — php's default here is TRUE.` |
|       - | 1268 | `			 * Autoloads on a miss via PH7_VmExtractClassFromValue; a leading '\' is` |
|       - | 1269 | `			 * anchored there. Sibling of is_a()'s string form (which defaults OFF). */` |
|      34 | 1270 | `			pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      16 | 1271 | `		}` |
|      60 | 1272 | `		if( pClass ){` |
|       - | 1273 | `			/* Extract the target class */` |
|      50 | 1274 | `			ph7_class *pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|      50 | 1275 | `			if( pMain ){` |
|       - | 1276 | `				/* Perform the query — subclass-only (excludes self, unlike is_a). */` |
|      48 | 1277 | `				res = VmSubclassOf(pClass,pMain);` |
|      23 | 1278 | `			}` |
|      24 | 1279 | `		}` |
|      29 | 1280 | `	}` |
|       - | 1281 | `	/* Query result */` |
|      60 | 1282 | `	ph7_result_bool(pCtx,res);` |
|      60 | 1283 | `	return PH7_OK;` |
|       2 | 1284 | `}` |
|     160 | 1285 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1286 | `{` |
|       - | 1287 | `	ph7_value sResult; /* Store callback return value here */` |
|       - | 1288 | `	sxi32 rc;` |
|     162 | 1289 | `	if( nArg < 1 ){` |
|       - | 1290 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 1291 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1292 | `		return PH7_OK;` |
|       - | 1293 | `	}` |
|       - | 1294 | `	{` |
|       - | 1295 | `		/* php validates the callback BEFORE calling anything; the dispatcher below would` |
|       - | 1296 | `		 * otherwise answer NULL in silence for an unresolvable one. */` |
|     162 | 1297 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|     162 | 1298 | `		if( rcCb != PH7_OK ){` |
|      55 | 1299 | `			return rcCb;` |
|       - | 1300 | `		}` |
|       - | 1301 | `	}` |
|     108 | 1302 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     108 | 1303 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 1304 | `	/* php passes call_user_func()'s arguments BY VALUE: warn on a by-ref formal and copy */` |
|     108 | 1305 | `	PH7_VmCufDropByRefArgs(pCtx,apArg[0],nArg - 1,&apArg[1]);` |
|       - | 1306 | `	/* Try to invoke the callback. If the call_user_func() call site used` |
|       - | 1307 | `	 * name: arguments (e.g. call_user_func('f', b: 9)), forward them to the` |
|       - | 1308 | `	 * callback. The inner call's argument i is the outer argument i+1 (outer` |
|       - | 1309 | `	 * argument 0 is the callback), so the inner name array is simply the outer` |
|       - | 1310 | `	 * names shifted by one — no copy needed: VmResolveNamedArgs treats any index` |
|       - | 1311 | `	 * >= nTotal as positional, so a shorter map covers the callback's args. */` |
|     117 | 1312 | `	if( pCtx->pArgMap && pCtx->pArgMap->bHasNamed && nArg > 1 ){` |
|      19 | 1313 | `		VmCallArgMap *pOuter = pCtx->pArgMap;` |
|       - | 1314 | `		VmCallArgMap sInner;` |
|       - | 1315 | `		/* Zero first: a field added to the map (sAssertSrc, ...) must read as` |
|       - | 1316 | `		 * unset when forwarded, not as stack garbage. */` |
|      19 | 1317 | `		SyZero(&sInner,sizeof(sInner));` |
|      19 | 1318 | `		sInner.bHasNamed = 1;` |
|      19 | 1319 | `		sInner.bIsNamespaced = 0;` |
|       - | 1320 | `		/* Named args to call_user_func coerce in WEAK mode even from a` |
|       - | 1321 | `		 * strict_types=1 caller (verified vs php 8.5.7): a name: argument` |
|       - | 1322 | `		 * collected into the variadic and re-spread loses the strict context.` |
|       - | 1323 | `		 * call_user_func_array does NOT share this quirk (it stays strict). */` |
|      19 | 1324 | `		sInner.bStrict = 0;` |
|      19 | 1325 | `		sInner.nTotal = pOuter->nTotal > 1 ? pOuter->nTotal - 1 : 0;` |
|      19 | 1326 | `		sInner.aNames = sInner.nTotal > 0 ? &pOuter->aNames[1] : 0;` |
|      19 | 1327 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sInner);` |
|      10 | 1328 | `	}else{` |
|      90 | 1329 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|       - | 1330 | `	}` |
|     108 | 1331 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 1332 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds` |
|       - | 1333 | `		 * through the nearest try/catch instead of returning FALSE. */` |
|      12 | 1334 | `		PH7_MemObjRelease(&sResult);` |
|      12 | 1335 | `		return PH7_EXCEPTION;` |
|       - | 1336 | `	}` |
|      97 | 1337 | `	if( rc != SXRET_OK ){` |
|       - | 1338 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|     ! 0 | 1339 | `		ph7_result_bool(pCtx,0); /* return false */` |
|     ! 0 | 1340 | `	}else{` |
|       - | 1341 | `		/* Callback result */` |
|      97 | 1342 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       - | 1343 | `	}` |
|      97 | 1344 | `	PH7_MemObjRelease(&sResult);` |
|      97 | 1345 | `	return PH7_OK;` |
|      82 | 1346 | `}` |
|       - | 1347 | `/*` |
|       - | 1348 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|       - | 1349 | ` *  Call a callback with an array of parameters.` |
|       - | 1350 | ` * Parameter` |
|       - | 1351 | ` *  $callback` |
|       - | 1352 | ` *   The callable to be called.` |
|       - | 1353 | ` * $param_arr` |
|       - | 1354 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|       - | 1355 | ` * Return` |
|       - | 1356 | ` *  Returns the return value of the callback, or FALSE on error.` |
|       - | 1357 | ` */` |
|     138 | 1358 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1359 | `{` |
|       - | 1360 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|       - | 1361 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|       - | 1362 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|       - | 1363 | `	SySet aArg;               /* Argument value pointers */` |
|     139 | 1364 | `	SyString *aNames = 0;     /* Name map, lazily allocated when a string key appears */` |
|     139 | 1365 | `	sxu32 nSlot = 0;          /* Number of collected arguments */` |
|       - | 1366 | `	sxi32 rc;` |
|       - | 1367 | `	sxu32 n;` |
|     139 | 1368 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 1369 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1370 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1371 | `		return PH7_OK;` |
|       - | 1372 | `	}` |
|       - | 1373 | `	{` |
|     139 | 1374 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|     139 | 1375 | `		if( rcCb != PH7_OK ){` |
|       5 | 1376 | `			return rcCb;` |
|       - | 1377 | `		}` |
|       - | 1378 | `	}` |
|     135 | 1379 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|     135 | 1380 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 1381 | `	/* Initialize the arguments container */` |
|     135 | 1382 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|       - | 1383 | `	/* Turn hashmap entries into callback arguments. A string key becomes a` |
|       - | 1384 | `	 * named argument (PHP 8: call_user_func_array($cb, ['b' => 9])), an integer` |
|       - | 1385 | `	 * key stays positional. The name map points straight at each node's key` |
|       - | 1386 | `	 * blob: the source array stays pinned on the operand stack for the whole` |
|       - | 1387 | `	 * call, so the blobs outlive argument binding. A pure list array (no string` |
|       - | 1388 | `	 * keys) never allocates aNames and takes the plain positional path. */` |
|     135 | 1389 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|     135 | 1390 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|     427 | 1391 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 1392 | `		/* Extract node value */` |
|     293 | 1393 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|     293 | 1394 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      23 | 1395 | `				if( aNames == 0 ){` |
|       - | 1396 | `					/* First string key: allocate the whole map, zeroed so every` |
|       - | 1397 | `					 * not-yet-seen slot defaults to positional. */` |
|      13 | 1398 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,pMap->nEntry * sizeof(SyString));` |
|      13 | 1399 | `					if( aNames == 0 ){` |
|     ! 0 | 1400 | `						SySetRelease(&aArg);` |
|     ! 0 | 1401 | `						PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1402 | `						return PH7_ContextMemoryError(pCtx);` |
|       - | 1403 | `					}` |
|      13 | 1404 | `					SyZero(aNames,pMap->nEntry * sizeof(SyString));` |
|       6 | 1405 | `				}` |
|      23 | 1406 | `				SyStringInitFromBuf(&aNames[nSlot],SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      11 | 1407 | `			}` |
|     293 | 1408 | `			SySetPut(&aArg,(const void *)&pValue);` |
|     293 | 1409 | `			nSlot++;` |
|     146 | 1410 | `		}` |
|       - | 1411 | `		/* Point to the next entry */` |
|     293 | 1412 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     147 | 1413 | `	}` |
|       - | 1414 | `	/* Try to invoke the callback */` |
|     135 | 1415 | `	if( aNames ){` |
|       - | 1416 | `		VmCallArgMap sMap;` |
|      13 | 1417 | `		SyZero(&sMap,sizeof(sMap)); /* new map fields must read unset, not stack garbage */` |
|      13 | 1418 | `		sMap.bHasNamed = 1;` |
|      13 | 1419 | `		sMap.bIsNamespaced = 0;` |
|       - | 1420 | `		/* Coercion strictness follows the caller's file; the OP_CALL dispatcher` |
|       - | 1421 | `		 * forwards the call site's map on pArgMap (0 only at non-OP_CALL sites). */` |
|      13 | 1422 | `		sMap.bStrict = (pCtx->pArgMap ? pCtx->pArgMap->bStrict : 0);` |
|      13 | 1423 | `		sMap.nTotal = nSlot;` |
|      13 | 1424 | `		sMap.aNames = aNames;` |
|      19 | 1425 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|      12 | 1426 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sMap);` |
|      13 | 1427 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aNames);` |
|       7 | 1428 | `	}else{` |
|     184 | 1429 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)nSlot,` |
|     122 | 1430 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|       - | 1431 | `	}` |
|     135 | 1432 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 1433 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds. */` |
|     101 | 1434 | `		PH7_MemObjRelease(&sResult);` |
|     101 | 1435 | `		SySetRelease(&aArg);` |
|     101 | 1436 | `		return PH7_EXCEPTION;` |
|       - | 1437 | `	}` |
|      35 | 1438 | `	if( rc != SXRET_OK ){` |
|       - | 1439 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|     ! 0 | 1440 | `		ph7_result_bool(pCtx,0); /* return false */` |
|     ! 0 | 1441 | `	}else{` |
|       - | 1442 | `		/* Callback result */` |
|      35 | 1443 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       - | 1444 | `	}` |
|       - | 1445 | `	/* Cleanup the mess left behind */` |
|      35 | 1446 | `	PH7_MemObjRelease(&sResult);` |
|      35 | 1447 | `	SySetRelease(&aArg);` |
|      35 | 1448 | `	return PH7_OK;` |
|      70 | 1449 | `}` |
|       - | 1450 |  |
