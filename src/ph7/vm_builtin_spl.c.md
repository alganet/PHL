# src/ph7/vm_builtin_spl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 56/73 lines (76.71%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    4 | ` */` |
|    - |    5 | `#include "ph7int.h"` |
|    - |    6 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |    7 | `/*` |
|    - |    8 | ` * SPL iterators, slice 1 (NEWPLAN band D): SeekableIterator, ArrayIterator,` |
|    - |    9 | ` * ArrayObject, plus the natsort()/natcasesort() array functions they need.` |
|    - |   10 | ` * Embedded-PHP chunk following the Reflection architecture — installed` |
|    - |   11 | ` * inside the bCompilingBuiltin window, backed by the engine's native array` |
|    - |   12 | ` * internal-pointer builtins (reset/next/key/current keep their position on a` |
|    - |   13 | ` * property, so ArrayIterator's cursor IS the backing array's pointer).` |
|    - |   14 | ` */` |
|    - |   15 |  |
|    - |   16 | `/* void __spl_deprecated(string $msg) — E_DEPRECATED with php's exact text` |
|    - |   17 | ` * (no auto-prepended function name, unlike ph7_context_throw_error) */` |
|  ! 0 |   18 | `static int vm_builtin_spl_deprecated(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 |   19 | `{` |
|    - |   20 | `	const char *zMsg;` |
|    - |   21 | `	int nMsg;` |
|  ! 0 |   22 | `	if( nArg < 1 ){` |
|  ! 0 |   23 | `		return PH7_OK;` |
|    - |   24 | `	}` |
|  ! 0 |   25 | `	zMsg = ph7_value_to_string(apArg[0],&nMsg);` |
|  ! 0 |   26 | `	PH7_VmThrowDeprecatedFmt(pCtx->pVm,"%.*s",nMsg,zMsg);` |
|  ! 0 |   27 | `	return PH7_OK;` |
|  ! 0 |   28 | `}` |
|    - |   29 |  |
|    - |   30 | `/* int __weak_create(object $obj) — register/share the weak cell for $obj,` |
|    - |   31 | ` * returning the cell pointer as an opaque int handle */` |
|   14 |   32 | `static int vm_builtin_weak_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   33 | `{` |
|   15 |   34 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |   35 | `	ph7_class_instance *pObj;` |
|   15 |   36 | `	VmWeakCell *pCell = 0;` |
|    - |   37 | `	SyHashEntry *pEntry;` |
|   15 |   38 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|  ! 0 |   39 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |   40 | `		return PH7_OK;` |
|    - |   41 | `	}` |
|   15 |   42 | `	pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|   15 |   43 | `	pEntry = SyHashGet(&pVm->hWeakCell,(const void *)&pObj,sizeof(void *));` |
|   15 |   44 | `	if( pEntry ){` |
|    3 |   45 | `		pCell = (VmWeakCell *)pEntry->pUserData;` |
|    3 |   46 | `		pCell->nRef++;` |
|    2 |   47 | `	}else{` |
|   13 |   48 | `		pCell = (VmWeakCell *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmWeakCell));` |
|   13 |   49 | `		if( pCell == 0 ){` |
|  ! 0 |   50 | `			return PH7_ContextMemoryError(pCtx);` |
|    - |   51 | `		}` |
|   13 |   52 | `		pCell->pObj = pObj;` |
|   13 |   53 | `		pCell->nRef = 1;` |
|    - |   54 | `		/* SyHash stores the key POINTER (no copy): key off the cell's own` |
|    - |   55 | `		 * pObj field — heap-stable for the entry's whole lifetime, and it` |
|    - |   56 | `		 * holds the live pointer bytes until the release hook nulls it` |
|    - |   57 | `		 * (which happens only after the entry is deleted). */` |
|   13 |   58 | `		if( SyHashInsert(&pVm->hWeakCell,(const void *)&pCell->pObj,sizeof(void *),pCell) != SXRET_OK ){` |
|  ! 0 |   59 | `			SyMemBackendFree(&pVm->sAllocator,pCell);` |
|  ! 0 |   60 | `			return PH7_ContextMemoryError(pCtx);` |
|    - |   61 | `		}` |
|    - |   62 | `	}` |
|   15 |   63 | `	ph7_result_int64(pCtx,(ph7_int64)(sxu64)(sxuptr)pCell);` |
|   15 |   64 | `	return PH7_OK;` |
|    8 |   65 | `}` |
|    - |   66 | `/* ?object __weak_get(int $handle) — the target instance, or null once dead */` |
|   28 |   67 | `static int vm_builtin_weak_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   68 | `{` |
|    - |   69 | `	VmWeakCell *pCell;` |
|   29 |   70 | `	if( nArg < 1 ){` |
|  ! 0 |   71 | `		ph7_result_null(pCtx);` |
|  ! 0 |   72 | `		return PH7_OK;` |
|    - |   73 | `	}` |
|   29 |   74 | `	pCell = (VmWeakCell *)(sxuptr)(sxu64)ph7_value_to_int64(apArg[0]);` |
|   29 |   75 | `	if( pCell == 0 \|\| pCell->pObj == 0 ){` |
|   11 |   76 | `		ph7_result_null(pCtx);` |
|   11 |   77 | `		return PH7_OK;` |
|    - |   78 | `	}` |
|    - |   79 | `	{` |
|    - |   80 | `		/* Hand the instance back: ph7_result_value's MemObjStore takes the` |
|    - |   81 | `		 * reference, so the temp holds none of its own. */` |
|    - |   82 | `		ph7_value sObj;` |
|   19 |   83 | `		PH7_MemObjInit(pCtx->pVm,&sObj);` |
|   19 |   84 | `		sObj.x.pOther = pCell->pObj;` |
|   19 |   85 | `		MemObjSetType(&sObj,MEMOBJ_OBJ);` |
|   19 |   86 | `		ph7_result_value(pCtx,&sObj);` |
|    - |   87 | `	}` |
|   19 |   88 | `	return PH7_OK;` |
|   15 |   89 | `}` |
|    - |   90 | `/* void __weak_drop(int $handle) — release one PHP-side handle */` |
|   14 |   91 | `static int vm_builtin_weak_drop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   92 | `{` |
|   15 |   93 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |   94 | `	VmWeakCell *pCell;` |
|   15 |   95 | `	if( nArg < 1 ){` |
|  ! 0 |   96 | `		return PH7_OK;` |
|    - |   97 | `	}` |
|   15 |   98 | `	pCell = (VmWeakCell *)(sxuptr)(sxu64)ph7_value_to_int64(apArg[0]);` |
|   15 |   99 | `	if( pCell == 0 \|\| pCell->nRef == 0 ){` |
|  ! 0 |  100 | `		return PH7_OK;` |
|    - |  101 | `	}` |
|   15 |  102 | `	pCell->nRef--;` |
|   15 |  103 | `	if( pCell->nRef == 0 ){` |
|   13 |  104 | `		if( pCell->pObj ){` |
|    - |  105 | `			/* Still alive: unhook the registry entry before freeing */` |
|    5 |  106 | `			void *pDummy = 0;` |
|    5 |  107 | `			SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pCell->pObj,sizeof(void *),&pDummy);` |
|    2 |  108 | `		}` |
|   13 |  109 | `		SyMemBackendFree(&pVm->sAllocator,pCell);` |
|    6 |  110 | `	}` |
|   15 |  111 | `	return PH7_OK;` |
|    8 |  112 | `}` |
|    - |  113 |  |
|    - |  114 | `static const char zSplLib[] =` |
|    - |  115 | `"interface SeekableIterator extends Iterator {"` |
|    - |  116 | `" public function seek($offset);"` |
|    - |  117 | `"}"` |
|    - |  118 | `"trait __SplStoreT {"` |
|    - |  119 | `" private $__d = [];"` |
|    - |  120 | `" private $__f = 0;"` |
|    - |  121 | `" private function __splInitStore($array, $flags, $ctor){"` |
|    - |  122 | `"  if( is_array($array) ){"` |
|    - |  123 | `"   $this->__d = $array;"` |
|    - |  124 | `"  }elseif( is_object($array) ){"` |
|    - |  125 | `"   __spl_deprecated(get_class($this) . '::' . $ctor . '(): Using an object as a"` |
|    - |  126 | `" backing array for ' . get_class($this) . ' is deprecated, as it allows violating"` |
|    - |  127 | `" class constraints and invariants');"` |
|    - |  128 | `"  $this->__d = get_object_vars($array);"` |
|    - |  129 | `"  }else{"` |
|    - |  130 | `"   throw new TypeError(get_class($this) . '::' . $ctor . '(): Argument #1 ($array)"` |
|    - |  131 | `" must be of type array, ' . get_debug_type($array) . ' given');"` |
|    - |  132 | `"  }"` |
|    - |  133 | `"  $this->__f = (int)$flags;"` |
|    - |  134 | `" }"` |
|    - |  135 | `" public function offsetExists($key){ return array_key_exists($key, $this->__d); }"` |
|    - |  136 | `" public function offsetGet($key){ return $this->__d[$key]; }"` |
|    - |  137 | `" public function offsetSet($key, $value){"` |
|    - |  138 | `"  if( $key === null ){ $this->__d[] = $value; }"` |
|    - |  139 | `"  else { $this->__d[$key] = $value; }"` |
|    - |  140 | `" }"` |
|    - |  141 | `" public function offsetUnset($key){ unset($this->__d[$key]); }"` |
|    - |  142 | `" public function append($value){ $this->__d[] = $value; }"` |
|    - |  143 | `" public function getArrayCopy(){ return $this->__d; }"` |
|    - |  144 | `" public function count(){ return count($this->__d); }"` |
|    - |  145 | `" public function getFlags(){ return $this->__f; }"` |
|    - |  146 | `" public function setFlags($flags){ $this->__f = (int)$flags; }"` |
|    - |  147 | `" public function asort($flags = 0){ asort($this->__d); return true; }"` |
|    - |  148 | `" public function ksort($flags = 0){ ksort($this->__d); return true; }"` |
|    - |  149 | `" public function uasort($callback){ uasort($this->__d, $callback); return true; }"` |
|    - |  150 | `" public function uksort($callback){ uksort($this->__d, $callback); return true; }"` |
|    - |  151 | `" public function natsort(){ uasort($this->__d, 'strnatcmp'); return true; }"` |
|    - |  152 | `" public function natcasesort(){ uasort($this->__d, 'strnatcasecmp'); return true; }"` |
|    - |  153 | `"}"` |
|    - |  154 | `"class ArrayIterator implements SeekableIterator, ArrayAccess, Countable {"` |
|    - |  155 | `" use __SplStoreT;"` |
|    - |  156 | `" const STD_PROP_LIST = 1;"` |
|    - |  157 | `" const ARRAY_AS_PROPS = 2;"` |
|    - |  158 | `" public function __construct($array = [], $flags = 0){"` |
|    - |  159 | `"  $this->__splInitStore($array, $flags, '__construct');"` |
|    - |  160 | `"  reset($this->__d);"` |
|    - |  161 | `" }"` |
|    - |  162 | `" public function current(){"` |
|    - |  163 | `"  if( key($this->__d) === null ){ return null; }"` |
|    - |  164 | `"  return current($this->__d);"` |
|    - |  165 | `" }"` |
|    - |  166 | `" public function key(){ return key($this->__d); }"` |
|    - |  167 | `" public function next(){ next($this->__d); }"` |
|    - |  168 | `" public function rewind(){ reset($this->__d); }"` |
|    - |  169 | `" public function valid(){ return key($this->__d) !== null; }"` |
|    - |  170 | `" public function seek($offset){"` |
|    - |  171 | `"  $offset = (int)$offset;"` |
|    - |  172 | `"  if( $offset < 0 \|\| $offset >= count($this->__d) ){"` |
|    - |  173 | `"   throw new OutOfBoundsException('Seek position ' . $offset . ' is out of range');"` |
|    - |  174 | `"  }"` |
|    - |  175 | `"  reset($this->__d);"` |
|    - |  176 | `"  for( $i = 0; $i < $offset; $i++ ){ next($this->__d); }"` |
|    - |  177 | `" }"` |
|    - |  178 | `"}"` |
|    - |  179 | `"class ArrayObject implements IteratorAggregate, ArrayAccess, Countable {"` |
|    - |  180 | `" use __SplStoreT;"` |
|    - |  181 | `" const STD_PROP_LIST = 1;"` |
|    - |  182 | `" const ARRAY_AS_PROPS = 2;"` |
|    - |  183 | `" private $__it = 'ArrayIterator';"` |
|    - |  184 | `" public function __construct($array = [], $flags = 0, $iteratorClass = 'ArrayIterator'){"` |
|    - |  185 | `"  $this->__splInitStore($array, $flags, '__construct');"` |
|    - |  186 | `"  if( $iteratorClass !== 'ArrayIterator' ){ $this->setIteratorClass($iteratorClass); }"` |
|    - |  187 | `" }"` |
|    - |  188 | `" public function getIterator(){"` |
|    - |  189 | `"  $c = $this->__it;"` |
|    - |  190 | `"  return new $c($this->__d);"` |
|    - |  191 | `" }"` |
|    - |  192 | `" public function exchangeArray($array){"` |
|    - |  193 | `"  $old = $this->__d;"` |
|    - |  194 | `"  $this->__splInitStore($array, $this->__f, 'exchangeArray');"` |
|    - |  195 | `"  return $old;"` |
|    - |  196 | `" }"` |
|    - |  197 | `" public function setIteratorClass($iteratorClass){"` |
|    - |  198 | `"  $c = (string)$iteratorClass;"` |
|    - |  199 | `"  if( $c !== 'ArrayIterator'"` |
|    - |  200 | `"   && (!class_exists($c) \|\| !is_subclass_of($c, 'ArrayIterator')) ){"` |
|    - |  201 | `"   throw new TypeError('ArrayObject::setIteratorClass(): Argument #1"` |
|    - |  202 | `" ($iteratorClass) must be a class name derived from ArrayIterator, ' . $c . ' given');"` |
|    - |  203 | `"  }"` |
|    - |  204 | `"  $this->__it = $c;"` |
|    - |  205 | `" }"` |
|    - |  206 | `" public function getIteratorClass(){ return $this->__it; }"` |
|    - |  207 | `" public function __get($name){"` |
|    - |  208 | `"  if( $this->__f & 2 ){ return $this->__d[$name]; }"` |
|    - |  209 | `"  return null;"` |
|    - |  210 | `" }"` |
|    - |  211 | `" public function __set($name, $value){"` |
|    - |  212 | `"  if( $this->__f & 2 ){ $this->__d[$name] = $value; return; }"` |
|    - |  213 | `"  $this->{$name} = $value;"` |
|    - |  214 | `" }"` |
|    - |  215 | `" public function __isset($name){"` |
|    - |  216 | `"  if( $this->__f & 2 ){ return isset($this->__d[$name]); }"` |
|    - |  217 | `"  return false;"` |
|    - |  218 | `" }"` |
|    - |  219 | `" public function __unset($name){"` |
|    - |  220 | `"  if( $this->__f & 2 ){ unset($this->__d[$name]); }"` |
|    - |  221 | `" }"` |
|    - |  222 | `"}"` |
|    - |  223 | `"function natsort(&$array){ return uasort($array, 'strnatcmp'); }"` |
|    - |  224 | `"function natcasesort(&$array){ return uasort($array, 'strnatcasecmp'); }"` |
|    - |  225 | `"interface OuterIterator extends Iterator {"` |
|    - |  226 | `" public function getInnerIterator();"` |
|    - |  227 | `"}"` |
|    - |  228 | `"class IteratorIterator implements OuterIterator {"` |
|    - |  229 | `" private $__in = null;"` |
|    - |  230 | `" public function __construct($iterator, $class = null){"` |
|    - |  231 | `"  while( $iterator instanceof IteratorAggregate ){ $iterator = $iterator->getIterator(); }"` |
|    - |  232 | `"  if( !($iterator instanceof Iterator) ){"` |
|    - |  233 | `"   throw new TypeError(get_class($this) . '::__construct(): Argument #1 ($iterator)"` |
|    - |  234 | `" must be of type Traversable, ' . get_debug_type($iterator) . ' given');"` |
|    - |  235 | `"  }"` |
|    - |  236 | `"  $this->__in = $iterator;"` |
|    - |  237 | `" }"` |
|    - |  238 | `" public function getInnerIterator(){ return $this->__in; }"` |
|    - |  239 | `" public function current(){ return $this->__in->current(); }"` |
|    - |  240 | `" public function key(){ return $this->__in->key(); }"` |
|    - |  241 | `" public function next(){ $this->__in->next(); }"` |
|    - |  242 | `" public function rewind(){ $this->__in->rewind(); }"` |
|    - |  243 | `" public function valid(){ return $this->__in->valid(); }"` |
|    - |  244 | `"}"` |
|    - |  245 | `"class LimitIterator extends IteratorIterator {"` |
|    - |  246 | `" private $__off = 0;"` |
|    - |  247 | `" private $__lim = -1;"` |
|    - |  248 | `" private $__pos = 0;"` |
|    - |  249 | `" public function __construct($iterator, $offset = 0, $limit = -1){"` |
|    - |  250 | `"  $offset = (int)$offset; $limit = (int)$limit;"` |
|    - |  251 | `"  if( $offset < 0 ){"` |
|    - |  252 | `"   throw new ValueError('LimitIterator::__construct(): Argument #2 ($offset) must be"` |
|    - |  253 | `" greater than or equal to 0');"` |
|    - |  254 | `"  }"` |
|    - |  255 | `"  if( $limit < -1 ){"` |
|    - |  256 | `"   throw new ValueError('LimitIterator::__construct(): Argument #3 ($limit) must be"` |
|    - |  257 | `" greater than or equal to -1');"` |
|    - |  258 | `"  }"` |
|    - |  259 | `"  parent::__construct($iterator);"` |
|    - |  260 | `"  $this->__off = $offset;"` |
|    - |  261 | `"  $this->__lim = $limit;"` |
|    - |  262 | `" }"` |
|    - |  263 | `" public function rewind(){"` |
|    - |  264 | `"  $in = $this->getInnerIterator();"` |
|    - |  265 | `"  $in->rewind();"` |
|    - |  266 | `"  for( $i = 0; $i < $this->__off && $in->valid(); $i++ ){ $in->next(); }"` |
|    - |  267 | `"  $this->__pos = $this->__off;"` |
|    - |  268 | `" }"` |
|    - |  269 | `" public function valid(){"` |
|    - |  270 | `"  if( $this->__lim != -1 && $this->__pos >= $this->__off + $this->__lim ){ return false; }"` |
|    - |  271 | `"  return $this->getInnerIterator()->valid();"` |
|    - |  272 | `" }"` |
|    - |  273 | `" public function next(){ $this->__pos++; $this->getInnerIterator()->next(); }"` |
|    - |  274 | `" public function getPosition(){ return $this->__pos; }"` |
|    - |  275 | `" public function seek($offset){"` |
|    - |  276 | `"  $offset = (int)$offset;"` |
|    - |  277 | `"  if( $offset < $this->__off ){"` |
|    - |  278 | `"   throw new OutOfBoundsException('Cannot seek to ' . $offset . ' which is below the"` |
|    - |  279 | `" offset ' . $this->__off);"` |
|    - |  280 | `"  }"` |
|    - |  281 | `"  if( $this->__lim != -1 && $offset >= $this->__off + $this->__lim ){"` |
|    - |  282 | `"   throw new OutOfBoundsException('Cannot seek to ' . $offset . ' which is behind or"` |
|    - |  283 | `" equal to the limit ' . $this->__lim . ' plus the offset ' . $this->__off);"` |
|    - |  284 | `"  }"` |
|    - |  285 | `"  $in = $this->getInnerIterator();"` |
|    - |  286 | `"  $in->rewind();"` |
|    - |  287 | `"  for( $i = 0; $i < $offset && $in->valid(); $i++ ){ $in->next(); }"` |
|    - |  288 | `"  $this->__pos = $offset;"` |
|    - |  289 | `"  return $this->__pos;"` |
|    - |  290 | `" }"` |
|    - |  291 | `"}"` |
|    - |  292 | `"abstract class FilterIterator extends IteratorIterator {"` |
|    - |  293 | `" abstract public function accept();"` |
|    - |  294 | `" private function __fiFetch(){"` |
|    - |  295 | `"  $in = $this->getInnerIterator();"` |
|    - |  296 | `"  while( $in->valid() && !$this->accept() ){ $in->next(); }"` |
|    - |  297 | `" }"` |
|    - |  298 | `" public function rewind(){ $this->getInnerIterator()->rewind(); $this->__fiFetch(); }"` |
|    - |  299 | `" public function next(){ $this->getInnerIterator()->next(); $this->__fiFetch(); }"` |
|    - |  300 | `"}"` |
|    - |  301 | `"class CallbackFilterIterator extends FilterIterator {"` |
|    - |  302 | `" private $__cb = null;"` |
|    - |  303 | `" public function __construct($iterator, $callback){"` |
|    - |  304 | `"  parent::__construct($iterator);"` |
|    - |  305 | `"  $this->__cb = $callback;"` |
|    - |  306 | `" }"` |
|    - |  307 | `" public function accept(){"` |
|    - |  308 | `"  $in = $this->getInnerIterator();"` |
|    - |  309 | `"  return (bool)call_user_func($this->__cb, $in->current(), $in->key(), $in);"` |
|    - |  310 | `" }"` |
|    - |  311 | `"}"` |
|    - |  312 | `"class RegexIterator extends FilterIterator {"` |
|    - |  313 | `" const USE_KEY = 1;"` |
|    - |  314 | `" const INVERT_MATCH = 2;"` |
|    - |  315 | `" const MATCH = 0;"` |
|    - |  316 | `" const GET_MATCH = 1;"` |
|    - |  317 | `" const ALL_MATCHES = 2;"` |
|    - |  318 | `" const SPLIT = 3;"` |
|    - |  319 | `" const REPLACE = 4;"` |
|    - |  320 | `" public $replacement = null;"` |
|    - |  321 | `" private $__re = '';"` |
|    - |  322 | `" private $__mode = 0;"` |
|    - |  323 | `" private $__rflags = 0;"` |
|    - |  324 | `" private $__pflags = 0;"` |
|    - |  325 | `" private $__cur = null;"` |
|    - |  326 | `" public function __construct($iterator, $pattern, $mode = 0, $flags = 0, $pregFlags = 0){"` |
|    - |  327 | `"  parent::__construct($iterator);"` |
|    - |  328 | `"  $this->__re = (string)$pattern;"` |
|    - |  329 | `"  $this->__mode = (int)$mode;"` |
|    - |  330 | `"  $this->__rflags = (int)$flags;"` |
|    - |  331 | `"  $this->__pflags = (int)$pregFlags;"` |
|    - |  332 | `" }"` |
|    - |  333 | `" public function accept(){"` |
|    - |  334 | `"  $in = $this->getInnerIterator();"` |
|    - |  335 | `"  if( !$in->valid() ){ return false; }"` |
|    - |  336 | `"  $subject = ($this->__rflags & self::USE_KEY) ? $in->key() : $in->current();"` |
|    - |  337 | `"  $subject = (string)$subject;"` |
|    - |  338 | `"  $this->__cur = null;"` |
|    - |  339 | `"  $ok = false;"` |
|    - |  340 | `"  if( $this->__mode === self::MATCH ){"` |
|    - |  341 | `"   $ok = preg_match($this->__re, $subject) > 0;"` |
|    - |  342 | `"  }elseif( $this->__mode === self::GET_MATCH ){"` |
|    - |  343 | `"   $m = null;"` |
|    - |  344 | `"   $ok = preg_match($this->__re, $subject, $m, $this->__pflags) > 0;"` |
|    - |  345 | `"   $this->__cur = $m;"` |
|    - |  346 | `"  }elseif( $this->__mode === self::ALL_MATCHES ){"` |
|    - |  347 | `"   $m = null;"` |
|    - |  348 | `"   $ok = preg_match_all($this->__re, $subject, $m, $this->__pflags) > 0;"` |
|    - |  349 | `"   $this->__cur = $m;"` |
|    - |  350 | `"  }elseif( $this->__mode === self::SPLIT ){"` |
|    - |  351 | `"   $this->__cur = preg_split($this->__re, $subject, -1, $this->__pflags);"` |
|    - |  352 | `"   $ok = is_array($this->__cur) && count($this->__cur) > 1;"` |
|    - |  353 | `"  }elseif( $this->__mode === self::REPLACE ){"` |
|    - |  354 | `"   $n = 0;"` |
|    - |  355 | `"   $this->__cur = preg_replace($this->__re, (string)$this->replacement, $subject, -1, $n);"` |
|    - |  356 | `"   $ok = $n > 0;"` |
|    - |  357 | `"  }"` |
|    - |  358 | `"  if( $this->__rflags & self::INVERT_MATCH ){ $ok = !$ok; }"` |
|    - |  359 | `"  return $ok;"` |
|    - |  360 | `" }"` |
|    - |  361 | `" public function current(){"` |
|    - |  362 | `"  if( $this->__mode === self::MATCH ){ return $this->getInnerIterator()->current(); }"` |
|    - |  363 | `"  return $this->__cur;"` |
|    - |  364 | `" }"` |
|    - |  365 | `" public function getRegex(){ return $this->__re; }"` |
|    - |  366 | `" public function getMode(){ return $this->__mode; }"` |
|    - |  367 | `" public function setMode($mode){ $this->__mode = (int)$mode; }"` |
|    - |  368 | `" public function getFlags(){ return $this->__rflags; }"` |
|    - |  369 | `" public function setFlags($flags){ $this->__rflags = (int)$flags; }"` |
|    - |  370 | `" public function getPregFlags(){ return $this->__pflags; }"` |
|    - |  371 | `" public function setPregFlags($pregFlags){ $this->__pflags = (int)$pregFlags; }"` |
|    - |  372 | `"}"` |
|    - |  373 | `"class AppendIterator implements OuterIterator {"` |
|    - |  374 | `" private $__its = [];"` |
|    - |  375 | `" private $__idx = 0;"` |
|    - |  376 | `" public function __construct(){}"` |
|    - |  377 | `" public function append($iterator){"` |
|    - |  378 | `"  $this->__its[] = $iterator;"` |
|    - |  379 | `"  if( count($this->__its) === 1 ){ $iterator->rewind(); }"` |
|    - |  380 | `" }"` |
|    - |  381 | `" public function getInnerIterator(){ return $this->__its[$this->__idx] ?? null; }"` |
|    - |  382 | `" public function getIteratorIndex(){"` |
|    - |  383 | `"  return isset($this->__its[$this->__idx]) ? $this->__idx : null;"` |
|    - |  384 | `" }"` |
|    - |  385 | `" public function getArrayIterator(){ return new ArrayIterator($this->__its); }"` |
|    - |  386 | `" private function __apAdvance(){"` |
|    - |  387 | `"  while( isset($this->__its[$this->__idx])"` |
|    - |  388 | `"   && !$this->__its[$this->__idx]->valid()"` |
|    - |  389 | `"   && isset($this->__its[$this->__idx + 1]) ){"` |
|    - |  390 | `"   $this->__idx++;"` |
|    - |  391 | `"   $this->__its[$this->__idx]->rewind();"` |
|    - |  392 | `"  }"` |
|    - |  393 | `" }"` |
|    - |  394 | `" public function rewind(){"` |
|    - |  395 | `"  $this->__idx = 0;"` |
|    - |  396 | `"  if( isset($this->__its[0]) ){ $this->__its[0]->rewind(); }"` |
|    - |  397 | `"  $this->__apAdvance();"` |
|    - |  398 | `" }"` |
|    - |  399 | `" public function valid(){"` |
|    - |  400 | `"  $in = $this->getInnerIterator();"` |
|    - |  401 | `"  return $in !== null && $in->valid();"` |
|    - |  402 | `" }"` |
|    - |  403 | `" public function current(){ $in = $this->getInnerIterator(); return $in ? $in->current() : null; }"` |
|    - |  404 | `" public function key(){ $in = $this->getInnerIterator(); return $in ? $in->key() : null; }"` |
|    - |  405 | `" public function next(){"` |
|    - |  406 | `"  $in = $this->getInnerIterator();"` |
|    - |  407 | `"  if( $in ){ $in->next(); }"` |
|    - |  408 | `"  $this->__apAdvance();"` |
|    - |  409 | `" }"` |
|    - |  410 | `"}"` |
|    - |  411 | `"class InfiniteIterator extends IteratorIterator {"` |
|    - |  412 | `" public function next(){"` |
|    - |  413 | `"  $in = $this->getInnerIterator();"` |
|    - |  414 | `"  $in->next();"` |
|    - |  415 | `"  if( !$in->valid() ){ $in->rewind(); }"` |
|    - |  416 | `" }"` |
|    - |  417 | `"}"` |
|    - |  418 | `"class NoRewindIterator extends IteratorIterator {"` |
|    - |  419 | `" public function rewind(){}"` |
|    - |  420 | `"}"` |
|    - |  421 | `"class WeakReference {"` |
|    - |  422 | `" private $__h = 0;"` |
|    - |  423 | `" private function __construct(){}"` |
|    - |  424 | `" public static function create($object){"` |
|    - |  425 | `"  if( !is_object($object) ){"` |
|    - |  426 | `"   throw new TypeError('WeakReference::create(): Argument #1 ($object) must be"` |
|    - |  427 | `" of type object, ' . get_debug_type($object) . ' given');"` |
|    - |  428 | `"  }"` |
|    - |  429 | `"  $w = new WeakReference();"` |
|    - |  430 | `"  $w->__h = __weak_create($object);"` |
|    - |  431 | `"  return $w;"` |
|    - |  432 | `" }"` |
|    - |  433 | `" public function get(){ return $this->__h ? __weak_get($this->__h) : null; }"` |
|    - |  434 | `" public function __destruct(){"` |
|    - |  435 | `"  if( $this->__h ){ __weak_drop($this->__h); $this->__h = 0; }"` |
|    - |  436 | `" }"` |
|    - |  437 | `"}"` |
|    - |  438 | `"class WeakMap implements ArrayAccess, Countable, IteratorAggregate {"` |
|    - |  439 | `" private $__e = [];"` |
|    - |  440 | `" private function __wmPrune(){"` |
|    - |  441 | `"  foreach( $this->__e as $id => $p ){"` |
|    - |  442 | `"   if( __weak_get($p[0]) === null ){"` |
|    - |  443 | `"    __weak_drop($p[0]);"` |
|    - |  444 | `"    unset($this->__e[$id]);"` |
|    - |  445 | `"   }"` |
|    - |  446 | `"  }"` |
|    - |  447 | `" }"` |
|    - |  448 | `" public function offsetSet($object, $value){"` |
|    - |  449 | `"  if( !is_object($object) ){"` |
|    - |  450 | `"   throw new TypeError('WeakMap key must be an object');"` |
|    - |  451 | `"  }"` |
|    - |  452 | `"  $id = spl_object_id($object);"` |
|    - |  453 | `"  if( isset($this->__e[$id]) && __weak_get($this->__e[$id][0]) !== null ){"` |
|    - |  454 | `"   $this->__e[$id][1] = $value;"` |
|    - |  455 | `"   return;"` |
|    - |  456 | `"  }"` |
|    - |  457 | `"  if( isset($this->__e[$id]) ){ __weak_drop($this->__e[$id][0]); }"` |
|    - |  458 | `"  $this->__e[$id] = [__weak_create($object), $value];"` |
|    - |  459 | `" }"` |
|    - |  460 | `" public function offsetGet($object){"` |
|    - |  461 | `"  if( !is_object($object) ){"` |
|    - |  462 | `"   throw new TypeError('WeakMap key must be an object');"` |
|    - |  463 | `"  }"` |
|    - |  464 | `"  $id = spl_object_id($object);"` |
|    - |  465 | `"  if( isset($this->__e[$id]) && __weak_get($this->__e[$id][0]) === $object ){"` |
|    - |  466 | `"   return $this->__e[$id][1];"` |
|    - |  467 | `"  }"` |
|    - |  468 | `"  throw new Error('Object ' . get_class($object) . '#' . $id . ' not contained"` |
|    - |  469 | `" in WeakMap');"` |
|    - |  470 | `" }"` |
|    - |  471 | `" public function offsetExists($object){"` |
|    - |  472 | `"  if( !is_object($object) ){"` |
|    - |  473 | `"   throw new TypeError('WeakMap key must be an object');"` |
|    - |  474 | `"  }"` |
|    - |  475 | `"  $id = spl_object_id($object);"` |
|    - |  476 | `"  return isset($this->__e[$id]) && __weak_get($this->__e[$id][0]) === $object;"` |
|    - |  477 | `" }"` |
|    - |  478 | `" public function offsetUnset($object){"` |
|    - |  479 | `"  if( !is_object($object) ){"` |
|    - |  480 | `"   throw new TypeError('WeakMap key must be an object');"` |
|    - |  481 | `"  }"` |
|    - |  482 | `"  $id = spl_object_id($object);"` |
|    - |  483 | `"  if( isset($this->__e[$id]) ){"` |
|    - |  484 | `"   __weak_drop($this->__e[$id][0]);"` |
|    - |  485 | `"   unset($this->__e[$id]);"` |
|    - |  486 | `"  }"` |
|    - |  487 | `" }"` |
|    - |  488 | `" public function count(){"` |
|    - |  489 | `"  $this->__wmPrune();"` |
|    - |  490 | `"  return count($this->__e);"` |
|    - |  491 | `" }"` |
|    - |  492 | `" public function getIterator(): Generator {"` |
|    - |  493 | `"  $this->__wmPrune();"` |
|    - |  494 | `"  foreach( $this->__e as $p ){"` |
|    - |  495 | `"   $o = __weak_get($p[0]);"` |
|    - |  496 | `"   if( $o !== null ){ yield $o => $p[1]; }"` |
|    - |  497 | `"  }"` |
|    - |  498 | `" }"` |
|    - |  499 | `" public function __destruct(){"` |
|    - |  500 | `"  foreach( $this->__e as $p ){ __weak_drop($p[0]); }"` |
|    - |  501 | `"  $this->__e = [];"` |
|    - |  502 | `" }"` |
|    - |  503 | `"}"` |
|    - |  504 | `"class EmptyIterator implements Iterator {"` |
|    - |  505 | `" public function current(){"` |
|    - |  506 | `"  throw new BadMethodCallException('Accessing the value of an EmptyIterator');"` |
|    - |  507 | `" }"` |
|    - |  508 | `" public function key(){"` |
|    - |  509 | `"  throw new BadMethodCallException('Accessing the key of an EmptyIterator');"` |
|    - |  510 | `" }"` |
|    - |  511 | `" public function next(){}"` |
|    - |  512 | `" public function rewind(){}"` |
|    - |  513 | `" public function valid(){ return false; }"` |
|    - |  514 | `"}"` |
|    - |  515 | `"class SplDoublyLinkedList implements Iterator, Countable, ArrayAccess {"` |
|    - |  516 | `" const IT_MODE_LIFO = 2;"` |
|    - |  517 | `" const IT_MODE_FIFO = 0;"` |
|    - |  518 | `" const IT_MODE_DELETE = 1;"` |
|    - |  519 | `" const IT_MODE_KEEP = 0;"` |
|    - |  520 | `" private $__q = [];"` |
|    - |  521 | `" private $__mode = 0;"` |
|    - |  522 | `" private $__i = 0;"` |
|    - |  523 | `" public function __construct(){"` |
|    - |  524 | `"  if( $this instanceof SplStack ){ $this->__mode = 2; }"` |
|    - |  525 | `" }"` |
|    - |  526 | `" public function setIteratorMode($mode){"` |
|    - |  527 | `"  $mode = (int)$mode;"` |
|    - |  528 | `"  if( ($this instanceof SplStack \|\| $this instanceof SplQueue)"` |
|    - |  529 | `"   && ($mode & 2) !== ($this->__mode & 2) ){"` |
|    - |  530 | `"   throw new RuntimeException(\"Iterators' LIFO/FIFO modes for SplStack/SplQueue"` |
|    - |  531 | `" objects are frozen\");"` |
|    - |  532 | `"  }"` |
|    - |  533 | `"  $this->__mode = $mode;"` |
|    - |  534 | `" }"` |
|    - |  535 | `" public function getIteratorMode(){ return $this->__mode; }"` |
|    - |  536 | `" public function push($value){ $this->__q[] = $value; }"` |
|    - |  537 | `" public function pop(){"` |
|    - |  538 | `"  if( count($this->__q) === 0 ){"` |
|    - |  539 | `"   throw new RuntimeException(\"Can't pop from an empty datastructure\");"` |
|    - |  540 | `"  }"` |
|    - |  541 | `"  return array_pop($this->__q);"` |
|    - |  542 | `" }"` |
|    - |  543 | `" public function shift(){"` |
|    - |  544 | `"  if( count($this->__q) === 0 ){"` |
|    - |  545 | `"   throw new RuntimeException(\"Can't shift from an empty datastructure\");"` |
|    - |  546 | `"  }"` |
|    - |  547 | `"  return array_shift($this->__q);"` |
|    - |  548 | `" }"` |
|    - |  549 | `" public function unshift($value){ array_unshift($this->__q, $value); }"` |
|    - |  550 | `" public function top(){"` |
|    - |  551 | `"  if( count($this->__q) === 0 ){"` |
|    - |  552 | `"   throw new RuntimeException(\"Can't peek at an empty datastructure\");"` |
|    - |  553 | `"  }"` |
|    - |  554 | `"  return $this->__q[count($this->__q) - 1];"` |
|    - |  555 | `" }"` |
|    - |  556 | `" public function bottom(){"` |
|    - |  557 | `"  if( count($this->__q) === 0 ){"` |
|    - |  558 | `"   throw new RuntimeException(\"Can't peek at an empty datastructure\");"` |
|    - |  559 | `"  }"` |
|    - |  560 | `"  return $this->__q[0];"` |
|    - |  561 | `" }"` |
|    - |  562 | `" public function isEmpty(){ return count($this->__q) === 0; }"` |
|    - |  563 | `" public function count(){ return count($this->__q); }"` |
|    - |  564 | `" public function toArray(){ return $this->__q; }"` |
|    - |  565 | `" public function add($index, $value){"` |
|    - |  566 | `"  $index = (int)$index;"` |
|    - |  567 | `"  if( $index < 0 \|\| $index > count($this->__q) ){"` |
|    - |  568 | `"   throw new OutOfRangeException(get_class($this) === 'SplDoublyLinkedList'"` |
|    - |  569 | `"    ? 'SplDoublyLinkedList::add(): Argument #1 ($index) is out of range'"` |
|    - |  570 | `"    : get_class($this) . '::add(): Argument #1 ($index) is out of range');"` |
|    - |  571 | `"  }"` |
|    - |  572 | `"  array_splice($this->__q, $index, 0, [$value]);"` |
|    - |  573 | `" }"` |
|    - |  574 | `" public function offsetExists($index){"` |
|    - |  575 | `"  return is_int($index) \|\| ctype_digit((string)$index)"` |
|    - |  576 | `"   ? ((int)$index >= 0 && (int)$index < count($this->__q)) : false;"` |
|    - |  577 | `" }"` |
|    - |  578 | `" public function offsetGet($index){"` |
|    - |  579 | `"  $index = (int)$index;"` |
|    - |  580 | `"  if( $index < 0 \|\| $index >= count($this->__q) ){"` |
|    - |  581 | `"   throw new OutOfRangeException('SplDoublyLinkedList::offsetGet(): Argument #1"` |
|    - |  582 | `" ($index) is out of range');"` |
|    - |  583 | `"  }"` |
|    - |  584 | `"  return $this->__q[$index];"` |
|    - |  585 | `" }"` |
|    - |  586 | `" public function offsetSet($index, $value){"` |
|    - |  587 | `"  if( $index === null ){ $this->__q[] = $value; return; }"` |
|    - |  588 | `"  $index = (int)$index;"` |
|    - |  589 | `"  if( $index < 0 \|\| $index >= count($this->__q) ){"` |
|    - |  590 | `"   throw new OutOfRangeException('SplDoublyLinkedList::offsetSet(): Argument #1"` |
|    - |  591 | `" ($index) is out of range');"` |
|    - |  592 | `"  }"` |
|    - |  593 | `"  $this->__q[$index] = $value;"` |
|    - |  594 | `" }"` |
|    - |  595 | `" public function offsetUnset($index){"` |
|    - |  596 | `"  $index = (int)$index;"` |
|    - |  597 | `"  if( $index < 0 \|\| $index >= count($this->__q) ){"` |
|    - |  598 | `"   throw new OutOfRangeException('SplDoublyLinkedList::offsetUnset(): Argument #1"` |
|    - |  599 | `" ($index) is out of range');"` |
|    - |  600 | `"  }"` |
|    - |  601 | `"  array_splice($this->__q, $index, 1);"` |
|    - |  602 | `" }"` |
|    - |  603 | `" public function rewind(){"` |
|    - |  604 | `"  $this->__i = ($this->__mode & 2) ? count($this->__q) - 1 : 0;"` |
|    - |  605 | `" }"` |
|    - |  606 | `" public function valid(){"` |
|    - |  607 | `"  return $this->__i >= 0 && $this->__i < count($this->__q);"` |
|    - |  608 | `" }"` |
|    - |  609 | `" public function current(){ return $this->__q[$this->__i] ?? null; }"` |
|    - |  610 | `" public function key(){ return $this->__i; }"` |
|    - |  611 | `" public function next(){"` |
|    - |  612 | `"  if( $this->__mode & 1 ){"` |
|    - |  613 | `"   /* IT_MODE_DELETE consumes the element just visited */"` |
|    - |  614 | `"   if( $this->__mode & 2 ){ array_pop($this->__q); $this->__i = count($this->__q) - 1; }"` |
|    - |  615 | `"   else { array_shift($this->__q); }"` |
|    - |  616 | `"  }else{"` |
|    - |  617 | `"   $this->__i += ($this->__mode & 2) ? -1 : 1;"` |
|    - |  618 | `"  }"` |
|    - |  619 | `" }"` |
|    - |  620 | `" public function prev(){ $this->__i += ($this->__mode & 2) ? 1 : -1; }"` |
|    - |  621 | `"}"` |
|    - |  622 | `"class SplStack extends SplDoublyLinkedList {}"` |
|    - |  623 | `"class SplQueue extends SplDoublyLinkedList {"` |
|    - |  624 | `" public function enqueue($value){ $this->push($value); }"` |
|    - |  625 | `" public function dequeue(){ return $this->shift(); }"` |
|    - |  626 | `"}"` |
|    - |  627 | `"abstract class SplHeap implements Iterator, Countable {"` |
|    - |  628 | `" private $__h = [];"` |
|    - |  629 | `" abstract protected function compare($value1, $value2);"` |
|    - |  630 | `" private function __hSiftUp($i){"` |
|    - |  631 | `"  while( $i > 0 ){"` |
|    - |  632 | `"   $p = ($i - 1) >> 1;"` |
|    - |  633 | `"   if( $this->compare($this->__h[$i], $this->__h[$p]) <= 0 ){ break; }"` |
|    - |  634 | `"   $t = $this->__h[$p]; $this->__h[$p] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  635 | `"   $i = $p;"` |
|    - |  636 | `"  }"` |
|    - |  637 | `" }"` |
|    - |  638 | `" private function __hSiftDown($i){"` |
|    - |  639 | `"  $n = count($this->__h);"` |
|    - |  640 | `"  for(;;){"` |
|    - |  641 | `"   $l = 2 * $i + 1; $r = $l + 1; $b = $i;"` |
|    - |  642 | `"   if( $l < $n && $this->compare($this->__h[$l], $this->__h[$b]) > 0 ){ $b = $l; }"` |
|    - |  643 | `"   if( $r < $n && $this->compare($this->__h[$r], $this->__h[$b]) > 0 ){ $b = $r; }"` |
|    - |  644 | `"   if( $b === $i ){ break; }"` |
|    - |  645 | `"   $t = $this->__h[$b]; $this->__h[$b] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  646 | `"   $i = $b;"` |
|    - |  647 | `"  }"` |
|    - |  648 | `" }"` |
|    - |  649 | `" public function insert($value){"` |
|    - |  650 | `"  $this->__h[] = $value;"` |
|    - |  651 | `"  $this->__hSiftUp(count($this->__h) - 1);"` |
|    - |  652 | `"  return true;"` |
|    - |  653 | `" }"` |
|    - |  654 | `" public function extract(){"` |
|    - |  655 | `"  $n = count($this->__h);"` |
|    - |  656 | `"  if( $n === 0 ){"` |
|    - |  657 | `"   throw new RuntimeException(\"Can't extract from an empty heap\");"` |
|    - |  658 | `"  }"` |
|    - |  659 | `"  $top = $this->__h[0];"` |
|    - |  660 | `"  $last = array_pop($this->__h);"` |
|    - |  661 | `"  if( $n > 1 ){"` |
|    - |  662 | `"   $this->__h[0] = $last;"` |
|    - |  663 | `"   $this->__hSiftDown(0);"` |
|    - |  664 | `"  }"` |
|    - |  665 | `"  return $top;"` |
|    - |  666 | `" }"` |
|    - |  667 | `" public function top(){"` |
|    - |  668 | `"  if( count($this->__h) === 0 ){"` |
|    - |  669 | `"   throw new RuntimeException(\"Can't peek at an empty heap\");"` |
|    - |  670 | `"  }"` |
|    - |  671 | `"  return $this->__h[0];"` |
|    - |  672 | `" }"` |
|    - |  673 | `" public function isEmpty(){ return count($this->__h) === 0; }"` |
|    - |  674 | `" public function count(){ return count($this->__h); }"` |
|    - |  675 | `" public function isCorrupted(){ return false; }"` |
|    - |  676 | `" public function recoverFromCorruption(){ return true; }"` |
|    - |  677 | `" public function rewind(){}"` |
|    - |  678 | `" public function valid(){ return count($this->__h) > 0; }"` |
|    - |  679 | `" public function current(){ return count($this->__h) ? $this->__h[0] : null; }"` |
|    - |  680 | `" public function key(){ return count($this->__h) - 1; }"` |
|    - |  681 | `" public function next(){ if( count($this->__h) ){ $this->extract(); } }"` |
|    - |  682 | `"}"` |
|    - |  683 | `"class SplMinHeap extends SplHeap {"` |
|    - |  684 | `" protected function compare($value1, $value2){ return $value2 <=> $value1; }"` |
|    - |  685 | `"}"` |
|    - |  686 | `"class SplMaxHeap extends SplHeap {"` |
|    - |  687 | `" protected function compare($value1, $value2){ return $value1 <=> $value2; }"` |
|    - |  688 | `"}"` |
|    - |  689 | `"class SplPriorityQueue implements Iterator, Countable {"` |
|    - |  690 | `" const EXTR_DATA = 1;"` |
|    - |  691 | `" const EXTR_PRIORITY = 2;"` |
|    - |  692 | `" const EXTR_BOTH = 3;"` |
|    - |  693 | `" private $__h = [];"` |
|    - |  694 | `" private $__serial = PHP_INT_MAX;"` |
|    - |  695 | `" private $__flags = 1;"` |
|    - |  696 | `" public function compare($priority1, $priority2){ return $priority1 <=> $priority2; }"` |
|    - |  697 | `" private function __pqCmp($a, $b){"` |
|    - |  698 | `"  /* NO tie-break: php's heap swaps only on strictly-greater, which fixes"` |
|    - |  699 | `"   * the (documented-as-undefined) equal-priority order it exhibits */"` |
|    - |  700 | `"  return $this->compare($a[0], $b[0]);"` |
|    - |  701 | `" }"` |
|    - |  702 | `" private function __pqSiftUp($i){"` |
|    - |  703 | `"  while( $i > 0 ){"` |
|    - |  704 | `"   $p = ($i - 1) >> 1;"` |
|    - |  705 | `"   if( $this->__pqCmp($this->__h[$i], $this->__h[$p]) <= 0 ){ break; }"` |
|    - |  706 | `"   $t = $this->__h[$p]; $this->__h[$p] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  707 | `"   $i = $p;"` |
|    - |  708 | `"  }"` |
|    - |  709 | `" }"` |
|    - |  710 | `" private function __pqSiftDown($i){"` |
|    - |  711 | `"  $n = count($this->__h);"` |
|    - |  712 | `"  for(;;){"` |
|    - |  713 | `"   $l = 2 * $i + 1; $r = $l + 1; $b = $i;"` |
|    - |  714 | `"   if( $l < $n && $this->__pqCmp($this->__h[$l], $this->__h[$b]) > 0 ){ $b = $l; }"` |
|    - |  715 | `"   if( $r < $n && $this->__pqCmp($this->__h[$r], $this->__h[$b]) > 0 ){ $b = $r; }"` |
|    - |  716 | `"   if( $b === $i ){ break; }"` |
|    - |  717 | `"   $t = $this->__h[$b]; $this->__h[$b] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  718 | `"   $i = $b;"` |
|    - |  719 | `"  }"` |
|    - |  720 | `" }"` |
|    - |  721 | `" public function insert($value, $priority){"` |
|    - |  722 | `"  $this->__h[] = [$priority, $this->__serial--, $value];"` |
|    - |  723 | `"  $this->__pqSiftUp(count($this->__h) - 1);"` |
|    - |  724 | `"  return true;"` |
|    - |  725 | `" }"` |
|    - |  726 | `" private function __pqShape($node){"` |
|    - |  727 | `"  if( $this->__flags === self::EXTR_BOTH ){"` |
|    - |  728 | `"   return ['data' => $node[2], 'priority' => $node[0]];"` |
|    - |  729 | `"  }"` |
|    - |  730 | `"  if( $this->__flags === self::EXTR_PRIORITY ){ return $node[0]; }"` |
|    - |  731 | `"  return $node[2];"` |
|    - |  732 | `" }"` |
|    - |  733 | `" public function extract(){"` |
|    - |  734 | `"  $n = count($this->__h);"` |
|    - |  735 | `"  if( $n === 0 ){"` |
|    - |  736 | `"   throw new RuntimeException(\"Can't extract from an empty heap\");"` |
|    - |  737 | `"  }"` |
|    - |  738 | `"  $top = $this->__h[0];"` |
|    - |  739 | `"  $last = array_pop($this->__h);"` |
|    - |  740 | `"  if( $n > 1 ){"` |
|    - |  741 | `"   $this->__h[0] = $last;"` |
|    - |  742 | `"   $this->__pqSiftDown(0);"` |
|    - |  743 | `"  }"` |
|    - |  744 | `"  return $this->__pqShape($top);"` |
|    - |  745 | `" }"` |
|    - |  746 | `" public function top(){"` |
|    - |  747 | `"  if( count($this->__h) === 0 ){"` |
|    - |  748 | `"   throw new RuntimeException(\"Can't peek at an empty heap\");"` |
|    - |  749 | `"  }"` |
|    - |  750 | `"  return $this->__pqShape($this->__h[0]);"` |
|    - |  751 | `" }"` |
|    - |  752 | `" public function setExtractFlags($flags){ $this->__flags = (int)$flags; }"` |
|    - |  753 | `" public function getExtractFlags(){ return $this->__flags; }"` |
|    - |  754 | `" public function isEmpty(){ return count($this->__h) === 0; }"` |
|    - |  755 | `" public function count(){ return count($this->__h); }"` |
|    - |  756 | `" public function isCorrupted(){ return false; }"` |
|    - |  757 | `" public function recoverFromCorruption(){ return true; }"` |
|    - |  758 | `" public function rewind(){}"` |
|    - |  759 | `" public function valid(){ return count($this->__h) > 0; }"` |
|    - |  760 | `" public function current(){ return count($this->__h) ? $this->__pqShape($this->__h[0]) : null; }"` |
|    - |  761 | `" public function key(){ return count($this->__h) - 1; }"` |
|    - |  762 | `" public function next(){ if( count($this->__h) ){ $this->extract(); } }"` |
|    - |  763 | `"}"` |
|    - |  764 | `"class SplFixedArray implements ArrayAccess, Countable, IteratorAggregate, JsonSerializable {"` |
|    - |  765 | `" private $__a = [];"` |
|    - |  766 | `" private $__n = 0;"` |
|    - |  767 | `" public function __construct($size = 0){"` |
|    - |  768 | `"  $this->setSize((int)$size);"` |
|    - |  769 | `" }"` |
|    - |  770 | `" private function __faIdx($index, $method){"` |
|    - |  771 | `"  if( !is_int($index) ){"` |
|    - |  772 | `"   if( is_string($index) && ctype_digit($index) ){"` |
|    - |  773 | `"    $index = (int)$index;"` |
|    - |  774 | `"   }else{"` |
|    - |  775 | `"    throw new TypeError('Cannot access offset of type ' . get_debug_type($index)"` |
|    - |  776 | `"     . ' on SplFixedArray');"` |
|    - |  777 | `"   }"` |
|    - |  778 | `"  }"` |
|    - |  779 | `"  if( $index < 0 \|\| $index >= $this->__n ){"` |
|    - |  780 | `"   throw new OutOfBoundsException('Index invalid or out of range');"` |
|    - |  781 | `"  }"` |
|    - |  782 | `"  return $index;"` |
|    - |  783 | `" }"` |
|    - |  784 | `" public function offsetExists($index){"` |
|    - |  785 | `"  if( !is_int($index) && !(is_string($index) && ctype_digit($index)) ){ return false; }"` |
|    - |  786 | `"  $index = (int)$index;"` |
|    - |  787 | `"  return $index >= 0 && $index < $this->__n && $this->__a[$index] !== null;"` |
|    - |  788 | `" }"` |
|    - |  789 | `" public function offsetGet($index){ return $this->__a[$this->__faIdx($index, 'offsetGet')]; }"` |
|    - |  790 | `" public function offsetSet($index, $value){ $this->__a[$this->__faIdx($index, 'offsetSet')] = $value; }"` |
|    - |  791 | `" public function offsetUnset($index){ $this->__a[$this->__faIdx($index, 'offsetUnset')] = null; }"` |
|    - |  792 | `" public function getSize(){ return $this->__n; }"` |
|    - |  793 | `" public function setSize($size){"` |
|    - |  794 | `"  $size = (int)$size;"` |
|    - |  795 | `"  if( $size < 0 ){"` |
|    - |  796 | `"   throw new ValueError('SplFixedArray::setSize(): Argument #1 ($size) must be"` |
|    - |  797 | `" greater than or equal to 0');"` |
|    - |  798 | `"  }"` |
|    - |  799 | `"  if( $size < $this->__n ){"` |
|    - |  800 | `"   $this->__a = array_slice($this->__a, 0, $size);"` |
|    - |  801 | `"  }else{"` |
|    - |  802 | `"   for( $i = $this->__n; $i < $size; $i++ ){ $this->__a[$i] = null; }"` |
|    - |  803 | `"  }"` |
|    - |  804 | `"  $this->__n = $size;"` |
|    - |  805 | `"  return true;"` |
|    - |  806 | `" }"` |
|    - |  807 | `" public function count(){ return $this->__n; }"` |
|    - |  808 | `" public function toArray(){ return $this->__a; }"` |
|    - |  809 | `" public static function fromArray($array, $preserveKeys = true){"` |
|    - |  810 | `"  $f = new SplFixedArray(0);"` |
|    - |  811 | `"  if( $preserveKeys ){"` |
|    - |  812 | `"   $max = -1;"` |
|    - |  813 | `"   foreach( $array as $k => $v ){"` |
|    - |  814 | `"    if( !is_int($k) \|\| $k < 0 ){"` |
|    - |  815 | `"     throw new InvalidArgumentException('array must contain only positive integer keys');"` |
|    - |  816 | `"    }"` |
|    - |  817 | `"    if( $k > $max ){ $max = $k; }"` |
|    - |  818 | `"   }"` |
|    - |  819 | `"   $f->setSize($max + 1);"` |
|    - |  820 | `"   foreach( $array as $k => $v ){ $f[$k] = $v; }"` |
|    - |  821 | `"  }else{"` |
|    - |  822 | `"   $vals = array_values($array);"` |
|    - |  823 | `"   $f->setSize(count($vals));"` |
|    - |  824 | `"   foreach( $vals as $k => $v ){ $f[$k] = $v; }"` |
|    - |  825 | `"  }"` |
|    - |  826 | `"  return $f;"` |
|    - |  827 | `" }"` |
|    - |  828 | `" public function getIterator(): Generator {"` |
|    - |  829 | `"  for( $i = 0; $i < $this->__n; $i++ ){ yield $i => $this->__a[$i]; }"` |
|    - |  830 | `" }"` |
|    - |  831 | `" public function jsonSerialize(){ return $this->__a; }"` |
|    - |  832 | `"}"` |
|    - |  833 | `"class SplObjectStorage implements Countable, Iterator, ArrayAccess {"` |
|    - |  834 | `" private $__o = [];"` |
|    - |  835 | `" private $__i = 0;"` |
|    - |  836 | `" public function attach($object, $info = null){"` |
|    - |  837 | `"  __spl_deprecated('Method SplObjectStorage::attach() is deprecated since 8.5, use"` |
|    - |  838 | `" method SplObjectStorage::offsetSet() instead');"` |
|    - |  839 | `"  $this->offsetSet($object, $info);"` |
|    - |  840 | `" }"` |
|    - |  841 | `" public function detach($object){"` |
|    - |  842 | `"  __spl_deprecated('Method SplObjectStorage::detach() is deprecated since 8.5, use"` |
|    - |  843 | `" method SplObjectStorage::offsetUnset() instead');"` |
|    - |  844 | `"  $this->offsetUnset($object);"` |
|    - |  845 | `" }"` |
|    - |  846 | `" public function contains($object){"` |
|    - |  847 | `"  __spl_deprecated('Method SplObjectStorage::contains() is deprecated since 8.5, use"` |
|    - |  848 | `" method SplObjectStorage::offsetExists() instead');"` |
|    - |  849 | `"  return $this->offsetExists($object);"` |
|    - |  850 | `" }"` |
|    - |  851 | `" public function offsetSet($object, $info = null){"` |
|    - |  852 | `"  $this->__o[spl_object_id($object)] = [$object, $info];"` |
|    - |  853 | `" }"` |
|    - |  854 | `" public function offsetExists($object){"` |
|    - |  855 | `"  return isset($this->__o[spl_object_id($object)]);"` |
|    - |  856 | `" }"` |
|    - |  857 | `" public function offsetGet($object){"` |
|    - |  858 | `"  $id = spl_object_id($object);"` |
|    - |  859 | `"  if( !isset($this->__o[$id]) ){"` |
|    - |  860 | `"   throw new UnexpectedValueException('Object not found');"` |
|    - |  861 | `"  }"` |
|    - |  862 | `"  return $this->__o[$id][1];"` |
|    - |  863 | `" }"` |
|    - |  864 | `" public function offsetUnset($object){"` |
|    - |  865 | `"  unset($this->__o[spl_object_id($object)]);"` |
|    - |  866 | `" }"` |
|    - |  867 | `" public function addAll($storage){"` |
|    - |  868 | `"  foreach( $storage as $obj ){"` |
|    - |  869 | `"   $this->offsetSet($obj, $storage[$obj]);"` |
|    - |  870 | `"  }"` |
|    - |  871 | `"  return $this->count();"` |
|    - |  872 | `" }"` |
|    - |  873 | `" public function removeAll($storage){"` |
|    - |  874 | `"  foreach( $storage as $obj ){ $this->offsetUnset($obj); }"` |
|    - |  875 | `"  return $this->count();"` |
|    - |  876 | `" }"` |
|    - |  877 | `" public function removeAllExcept($storage){"` |
|    - |  878 | `"  foreach( $this->__o as $id => $pair ){"` |
|    - |  879 | `"   if( !$storage->offsetExists($pair[0]) ){ unset($this->__o[$id]); }"` |
|    - |  880 | `"  }"` |
|    - |  881 | `"  return $this->count();"` |
|    - |  882 | `" }"` |
|    - |  883 | `" public function getHash($object){ return spl_object_hash($object); }"` |
|    - |  884 | `" public function count($mode = 0){ return count($this->__o); }"` |
|    - |  885 | `" public function getInfo(){"` |
|    - |  886 | `"  $pair = array_values($this->__o)[$this->__i] ?? null;"` |
|    - |  887 | `"  return $pair === null ? null : $pair[1];"` |
|    - |  888 | `" }"` |
|    - |  889 | `" public function setInfo($info){"` |
|    - |  890 | `"  $keys = array_keys($this->__o);"` |
|    - |  891 | `"  if( isset($keys[$this->__i]) ){ $this->__o[$keys[$this->__i]][1] = $info; }"` |
|    - |  892 | `" }"` |
|    - |  893 | `" public function rewind(){ $this->__i = 0; }"` |
|    - |  894 | `" public function valid(){ return $this->__i < count($this->__o); }"` |
|    - |  895 | `" public function key(){ return $this->__i; }"` |
|    - |  896 | `" public function current(){"` |
|    - |  897 | `"  $pair = array_values($this->__o)[$this->__i] ?? null;"` |
|    - |  898 | `"  return $pair === null ? null : $pair[0];"` |
|    - |  899 | `" }"` |
|    - |  900 | `" public function next(){ $this->__i++; }"` |
|    - |  901 | `"}"` |
|    - |  902 | `"interface SplObserver {"` |
|    - |  903 | `" public function update(SplSubject $subject);"` |
|    - |  904 | `"}"` |
|    - |  905 | `"interface SplSubject {"` |
|    - |  906 | `" public function attach(SplObserver $observer);"` |
|    - |  907 | `" public function detach(SplObserver $observer);"` |
|    - |  908 | `" public function notify();"` |
|    - |  909 | `"}"` |
|    - |  910 | `;` |
|    - |  911 |  |
| 3904 |  912 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm)` |
|    5 |  913 | `{` |
| 3909 |  914 | `	ph7_create_function(&(*pVm),"__spl_deprecated",vm_builtin_spl_deprecated,0);` |
| 3909 |  915 | `	ph7_create_function(&(*pVm),"__weak_create",vm_builtin_weak_create,0);` |
| 3909 |  916 | `	ph7_create_function(&(*pVm),"__weak_get",vm_builtin_weak_get,0);` |
| 3909 |  917 | `	ph7_create_function(&(*pVm),"__weak_drop",vm_builtin_weak_drop,0);` |
| 3909 |  918 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zSplLib,sizeof(zSplLib)-1);` |
|    5 |  919 | `}` |
|    - |  920 |  |
|    - |  921 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - |  922 |  |
|    - |  923 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|    - |  924 | `/* Tiny build: no SPL (builtin layer disabled) */` |
|    - |  925 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|    - |  926 | `#endif` |
|    - |  927 |  |
