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
|    - |  121 | `" private function __splInitStore($array, $flags, $owner){"` |
|    - |  122 | `"  /* $owner is the DECLARING method (php names the declaring class in these"` |
|    - |  123 | `"   * diagnostics, so a RecursiveArrayIterator misuse still says"` |
|    - |  124 | `"   * ArrayIterator::__construct) */"` |
|    - |  125 | `"  if( is_array($array) ){"` |
|    - |  126 | `"   $this->__d = $array;"` |
|    - |  127 | `"  }elseif( is_object($array) ){"` |
|    - |  128 | `"   __spl_deprecated($owner . '(): Using an object as a backing array for '"` |
|    - |  129 | `"    . get_class($this) . ' is deprecated, as it allows violating class"` |
|    - |  130 | `" constraints and invariants');"` |
|    - |  131 | `"  $this->__d = get_object_vars($array);"` |
|    - |  132 | `"  }else{"` |
|    - |  133 | `"   throw new TypeError($owner . '(): Argument #1 ($array) must be of type"` |
|    - |  134 | `" array, ' . get_debug_type($array) . ' given');"` |
|    - |  135 | `"  }"` |
|    - |  136 | `"  $this->__f = (int)$flags;"` |
|    - |  137 | `" }"` |
|    - |  138 | `" public function offsetExists($key){ return array_key_exists($key, $this->__d); }"` |
|    - |  139 | `" public function offsetGet($key){ return $this->__d[$key]; }"` |
|    - |  140 | `" public function offsetSet($key, $value){"` |
|    - |  141 | `"  if( $key === null ){ $this->__d[] = $value; }"` |
|    - |  142 | `"  else { $this->__d[$key] = $value; }"` |
|    - |  143 | `" }"` |
|    - |  144 | `" public function offsetUnset($key){ unset($this->__d[$key]); }"` |
|    - |  145 | `" public function append($value){ $this->__d[] = $value; }"` |
|    - |  146 | `" public function getArrayCopy(){ return $this->__d; }"` |
|    - |  147 | `" public function count(){ return count($this->__d); }"` |
|    - |  148 | `" public function getFlags(){ return $this->__f; }"` |
|    - |  149 | `" public function setFlags($flags){ $this->__f = (int)$flags; }"` |
|    - |  150 | `" public function asort($flags = 0){ asort($this->__d); return true; }"` |
|    - |  151 | `" public function ksort($flags = 0){ ksort($this->__d); return true; }"` |
|    - |  152 | `" public function uasort($callback){ uasort($this->__d, $callback); return true; }"` |
|    - |  153 | `" public function uksort($callback){ uksort($this->__d, $callback); return true; }"` |
|    - |  154 | `" public function natsort(){ uasort($this->__d, 'strnatcmp'); return true; }"` |
|    - |  155 | `" public function natcasesort(){ uasort($this->__d, 'strnatcasecmp'); return true; }"` |
|    - |  156 | `"}"` |
|    - |  157 | `"class ArrayIterator implements SeekableIterator, ArrayAccess, Countable {"` |
|    - |  158 | `" use __SplStoreT;"` |
|    - |  159 | `" const STD_PROP_LIST = 1;"` |
|    - |  160 | `" const ARRAY_AS_PROPS = 2;"` |
|    - |  161 | `" public function __construct($array = [], $flags = 0){"` |
|    - |  162 | `"  $this->__splInitStore($array, $flags, 'ArrayIterator::__construct');"` |
|    - |  163 | `"  reset($this->__d);"` |
|    - |  164 | `" }"` |
|    - |  165 | `" public function current(){"` |
|    - |  166 | `"  if( key($this->__d) === null ){ return null; }"` |
|    - |  167 | `"  return current($this->__d);"` |
|    - |  168 | `" }"` |
|    - |  169 | `" public function key(){ return key($this->__d); }"` |
|    - |  170 | `" public function next(){ next($this->__d); }"` |
|    - |  171 | `" public function rewind(){ reset($this->__d); }"` |
|    - |  172 | `" public function valid(){ return key($this->__d) !== null; }"` |
|    - |  173 | `" public function seek($offset){"` |
|    - |  174 | `"  $offset = (int)$offset;"` |
|    - |  175 | `"  if( $offset < 0 \|\| $offset >= count($this->__d) ){"` |
|    - |  176 | `"   throw new OutOfBoundsException('Seek position ' . $offset . ' is out of range');"` |
|    - |  177 | `"  }"` |
|    - |  178 | `"  reset($this->__d);"` |
|    - |  179 | `"  for( $i = 0; $i < $offset; $i++ ){ next($this->__d); }"` |
|    - |  180 | `" }"` |
|    - |  181 | `"}"` |
|    - |  182 | `"class ArrayObject implements IteratorAggregate, ArrayAccess, Countable {"` |
|    - |  183 | `" use __SplStoreT;"` |
|    - |  184 | `" const STD_PROP_LIST = 1;"` |
|    - |  185 | `" const ARRAY_AS_PROPS = 2;"` |
|    - |  186 | `" private $__it = 'ArrayIterator';"` |
|    - |  187 | `" public function __construct($array = [], $flags = 0, $iteratorClass = 'ArrayIterator'){"` |
|    - |  188 | `"  $this->__splInitStore($array, $flags, 'ArrayObject::__construct');"` |
|    - |  189 | `"  if( $iteratorClass !== 'ArrayIterator' ){ $this->setIteratorClass($iteratorClass); }"` |
|    - |  190 | `" }"` |
|    - |  191 | `" public function getIterator(){"` |
|    - |  192 | `"  $c = $this->__it;"` |
|    - |  193 | `"  return new $c($this->__d);"` |
|    - |  194 | `" }"` |
|    - |  195 | `" public function exchangeArray($array){"` |
|    - |  196 | `"  $old = $this->__d;"` |
|    - |  197 | `"  $this->__splInitStore($array, $this->__f, 'ArrayObject::exchangeArray');"` |
|    - |  198 | `"  return $old;"` |
|    - |  199 | `" }"` |
|    - |  200 | `" public function setIteratorClass($iteratorClass){"` |
|    - |  201 | `"  $c = (string)$iteratorClass;"` |
|    - |  202 | `"  if( $c !== 'ArrayIterator'"` |
|    - |  203 | `"   && (!class_exists($c) \|\| !is_subclass_of($c, 'ArrayIterator')) ){"` |
|    - |  204 | `"   throw new TypeError('ArrayObject::setIteratorClass(): Argument #1"` |
|    - |  205 | `" ($iteratorClass) must be a class name derived from ArrayIterator, ' . $c . ' given');"` |
|    - |  206 | `"  }"` |
|    - |  207 | `"  $this->__it = $c;"` |
|    - |  208 | `" }"` |
|    - |  209 | `" public function getIteratorClass(){ return $this->__it; }"` |
|    - |  210 | `" public function __get($name){"` |
|    - |  211 | `"  /* ?? null: a missing key must not raise php's undefined-array-key warning"` |
|    - |  212 | `"   * from INSIDE the wrapper (php's ArrayObject warns about the PROPERTY, and"` |
|    - |  213 | `"   * PHL's own magic-read path already diagnoses that) */"` |
|    - |  214 | `"  if( $this->__f & 2 ){ return $this->__d[$name] ?? null; }"` |
|    - |  215 | `"  return null;"` |
|    - |  216 | `" }"` |
|    - |  217 | `" public function __set($name, $value){"` |
|    - |  218 | `"  if( $this->__f & 2 ){ $this->__d[$name] = $value; return; }"` |
|    - |  219 | `"  $this->{$name} = $value;"` |
|    - |  220 | `" }"` |
|    - |  221 | `" public function __isset($name){"` |
|    - |  222 | `"  if( $this->__f & 2 ){ return isset($this->__d[$name]); }"` |
|    - |  223 | `"  return false;"` |
|    - |  224 | `" }"` |
|    - |  225 | `" public function __unset($name){"` |
|    - |  226 | `"  if( $this->__f & 2 ){ unset($this->__d[$name]); }"` |
|    - |  227 | `" }"` |
|    - |  228 | `"}"` |
|    - |  229 | `"function natsort(&$array){ return uasort($array, 'strnatcmp'); }"` |
|    - |  230 | `"function natcasesort(&$array){ return uasort($array, 'strnatcasecmp'); }"` |
|    - |  231 | `"interface OuterIterator extends Iterator {"` |
|    - |  232 | `" public function getInnerIterator();"` |
|    - |  233 | `"}"` |
|    - |  234 | `"class IteratorIterator implements OuterIterator {"` |
|    - |  235 | `" private $__in = null;"` |
|    - |  236 | `" public function __construct($iterator, $class = null){"` |
|    - |  237 | `"  while( $iterator instanceof IteratorAggregate ){ $iterator = $iterator->getIterator(); }"` |
|    - |  238 | `"  if( !($iterator instanceof Iterator) ){"` |
|    - |  239 | `"   throw new TypeError(get_class($this) . '::__construct(): Argument #1 ($iterator)"` |
|    - |  240 | `" must be of type Traversable, ' . get_debug_type($iterator) . ' given');"` |
|    - |  241 | `"  }"` |
|    - |  242 | `"  $this->__in = $iterator;"` |
|    - |  243 | `" }"` |
|    - |  244 | `" public function getInnerIterator(){ return $this->__in; }"` |
|    - |  245 | `" public function current(){ return $this->__in->current(); }"` |
|    - |  246 | `" public function key(){ return $this->__in->key(); }"` |
|    - |  247 | `" public function next(){ $this->__in->next(); }"` |
|    - |  248 | `" public function rewind(){ $this->__in->rewind(); }"` |
|    - |  249 | `" public function valid(){ return $this->__in->valid(); }"` |
|    - |  250 | `"}"` |
|    - |  251 | `"class LimitIterator extends IteratorIterator {"` |
|    - |  252 | `" private $__off = 0;"` |
|    - |  253 | `" private $__lim = -1;"` |
|    - |  254 | `" private $__pos = 0;"` |
|    - |  255 | `" public function __construct($iterator, $offset = 0, $limit = -1){"` |
|    - |  256 | `"  $offset = (int)$offset; $limit = (int)$limit;"` |
|    - |  257 | `"  if( $offset < 0 ){"` |
|    - |  258 | `"   throw new ValueError('LimitIterator::__construct(): Argument #2 ($offset) must be"` |
|    - |  259 | `" greater than or equal to 0');"` |
|    - |  260 | `"  }"` |
|    - |  261 | `"  if( $limit < -1 ){"` |
|    - |  262 | `"   throw new ValueError('LimitIterator::__construct(): Argument #3 ($limit) must be"` |
|    - |  263 | `" greater than or equal to -1');"` |
|    - |  264 | `"  }"` |
|    - |  265 | `"  parent::__construct($iterator);"` |
|    - |  266 | `"  $this->__off = $offset;"` |
|    - |  267 | `"  $this->__lim = $limit;"` |
|    - |  268 | `" }"` |
|    - |  269 | `" public function rewind(){"` |
|    - |  270 | `"  $in = $this->getInnerIterator();"` |
|    - |  271 | `"  $in->rewind();"` |
|    - |  272 | `"  for( $i = 0; $i < $this->__off && $in->valid(); $i++ ){ $in->next(); }"` |
|    - |  273 | `"  $this->__pos = $this->__off;"` |
|    - |  274 | `" }"` |
|    - |  275 | `" public function valid(){"` |
|    - |  276 | `"  if( $this->__lim != -1 && $this->__pos >= $this->__off + $this->__lim ){ return false; }"` |
|    - |  277 | `"  return $this->getInnerIterator()->valid();"` |
|    - |  278 | `" }"` |
|    - |  279 | `" public function next(){ $this->__pos++; $this->getInnerIterator()->next(); }"` |
|    - |  280 | `" public function getPosition(){ return $this->__pos; }"` |
|    - |  281 | `" public function seek($offset){"` |
|    - |  282 | `"  $offset = (int)$offset;"` |
|    - |  283 | `"  if( $offset < $this->__off ){"` |
|    - |  284 | `"   throw new OutOfBoundsException('Cannot seek to ' . $offset . ' which is below the"` |
|    - |  285 | `" offset ' . $this->__off);"` |
|    - |  286 | `"  }"` |
|    - |  287 | `"  if( $this->__lim != -1 && $offset >= $this->__off + $this->__lim ){"` |
|    - |  288 | `"   throw new OutOfBoundsException('Cannot seek to ' . $offset . ' which is behind or"` |
|    - |  289 | `" equal to the limit ' . $this->__lim . ' plus the offset ' . $this->__off);"` |
|    - |  290 | `"  }"` |
|    - |  291 | `"  $in = $this->getInnerIterator();"` |
|    - |  292 | `"  $in->rewind();"` |
|    - |  293 | `"  for( $i = 0; $i < $offset && $in->valid(); $i++ ){ $in->next(); }"` |
|    - |  294 | `"  $this->__pos = $offset;"` |
|    - |  295 | `"  return $this->__pos;"` |
|    - |  296 | `" }"` |
|    - |  297 | `"}"` |
|    - |  298 | `"abstract class FilterIterator extends IteratorIterator {"` |
|    - |  299 | `" abstract public function accept();"` |
|    - |  300 | `" private function __fiFetch(){"` |
|    - |  301 | `"  $in = $this->getInnerIterator();"` |
|    - |  302 | `"  while( $in->valid() && !$this->accept() ){ $in->next(); }"` |
|    - |  303 | `" }"` |
|    - |  304 | `" public function rewind(){ $this->getInnerIterator()->rewind(); $this->__fiFetch(); }"` |
|    - |  305 | `" public function next(){ $this->getInnerIterator()->next(); $this->__fiFetch(); }"` |
|    - |  306 | `"}"` |
|    - |  307 | `"class CallbackFilterIterator extends FilterIterator {"` |
|    - |  308 | `" private $__cb = null;"` |
|    - |  309 | `" public function __construct($iterator, $callback){"` |
|    - |  310 | `"  parent::__construct($iterator);"` |
|    - |  311 | `"  $this->__cb = $callback;"` |
|    - |  312 | `" }"` |
|    - |  313 | `" public function accept(){"` |
|    - |  314 | `"  $in = $this->getInnerIterator();"` |
|    - |  315 | `"  return (bool)call_user_func($this->__cb, $in->current(), $in->key(), $in);"` |
|    - |  316 | `" }"` |
|    - |  317 | `"}"` |
|    - |  318 | `"class RegexIterator extends FilterIterator {"` |
|    - |  319 | `" const USE_KEY = 1;"` |
|    - |  320 | `" const INVERT_MATCH = 2;"` |
|    - |  321 | `" const MATCH = 0;"` |
|    - |  322 | `" const GET_MATCH = 1;"` |
|    - |  323 | `" const ALL_MATCHES = 2;"` |
|    - |  324 | `" const SPLIT = 3;"` |
|    - |  325 | `" const REPLACE = 4;"` |
|    - |  326 | `" public $replacement = null;"` |
|    - |  327 | `" private $__re = '';"` |
|    - |  328 | `" private $__mode = 0;"` |
|    - |  329 | `" private $__rflags = 0;"` |
|    - |  330 | `" private $__pflags = 0;"` |
|    - |  331 | `" private $__cur = null;"` |
|    - |  332 | `" public function __construct($iterator, $pattern, $mode = 0, $flags = 0, $pregFlags = 0){"` |
|    - |  333 | `"  parent::__construct($iterator);"` |
|    - |  334 | `"  $this->__re = (string)$pattern;"` |
|    - |  335 | `"  $this->__mode = (int)$mode;"` |
|    - |  336 | `"  $this->__rflags = (int)$flags;"` |
|    - |  337 | `"  $this->__pflags = (int)$pregFlags;"` |
|    - |  338 | `" }"` |
|    - |  339 | `" public function accept(){"` |
|    - |  340 | `"  $in = $this->getInnerIterator();"` |
|    - |  341 | `"  if( !$in->valid() ){ return false; }"` |
|    - |  342 | `"  $subject = ($this->__rflags & self::USE_KEY) ? $in->key() : $in->current();"` |
|    - |  343 | `"  $subject = (string)$subject;"` |
|    - |  344 | `"  $this->__cur = null;"` |
|    - |  345 | `"  $ok = false;"` |
|    - |  346 | `"  if( $this->__mode === self::MATCH ){"` |
|    - |  347 | `"   $ok = preg_match($this->__re, $subject) > 0;"` |
|    - |  348 | `"  }elseif( $this->__mode === self::GET_MATCH ){"` |
|    - |  349 | `"   $m = null;"` |
|    - |  350 | `"   $ok = preg_match($this->__re, $subject, $m, $this->__pflags) > 0;"` |
|    - |  351 | `"   $this->__cur = $m;"` |
|    - |  352 | `"  }elseif( $this->__mode === self::ALL_MATCHES ){"` |
|    - |  353 | `"   $m = null;"` |
|    - |  354 | `"   $ok = preg_match_all($this->__re, $subject, $m, $this->__pflags) > 0;"` |
|    - |  355 | `"   $this->__cur = $m;"` |
|    - |  356 | `"  }elseif( $this->__mode === self::SPLIT ){"` |
|    - |  357 | `"   $this->__cur = preg_split($this->__re, $subject, -1, $this->__pflags);"` |
|    - |  358 | `"   $ok = is_array($this->__cur) && count($this->__cur) > 1;"` |
|    - |  359 | `"  }elseif( $this->__mode === self::REPLACE ){"` |
|    - |  360 | `"   $n = 0;"` |
|    - |  361 | `"   $this->__cur = preg_replace($this->__re, (string)$this->replacement, $subject, -1, $n);"` |
|    - |  362 | `"   $ok = $n > 0;"` |
|    - |  363 | `"  }"` |
|    - |  364 | `"  if( $this->__rflags & self::INVERT_MATCH ){ $ok = !$ok; }"` |
|    - |  365 | `"  return $ok;"` |
|    - |  366 | `" }"` |
|    - |  367 | `" public function current(){"` |
|    - |  368 | `"  if( $this->__mode === self::MATCH ){ return $this->getInnerIterator()->current(); }"` |
|    - |  369 | `"  return $this->__cur;"` |
|    - |  370 | `" }"` |
|    - |  371 | `" public function getRegex(){ return $this->__re; }"` |
|    - |  372 | `" public function getMode(){ return $this->__mode; }"` |
|    - |  373 | `" public function setMode($mode){ $this->__mode = (int)$mode; }"` |
|    - |  374 | `" public function getFlags(){ return $this->__rflags; }"` |
|    - |  375 | `" public function setFlags($flags){ $this->__rflags = (int)$flags; }"` |
|    - |  376 | `" public function getPregFlags(){ return $this->__pflags; }"` |
|    - |  377 | `" public function setPregFlags($pregFlags){ $this->__pflags = (int)$pregFlags; }"` |
|    - |  378 | `"}"` |
|    - |  379 | `"class AppendIterator implements OuterIterator {"` |
|    - |  380 | `" private $__its = [];"` |
|    - |  381 | `" private $__idx = 0;"` |
|    - |  382 | `" public function __construct(){}"` |
|    - |  383 | `" public function append($iterator){"` |
|    - |  384 | `"  $this->__its[] = $iterator;"` |
|    - |  385 | `"  if( count($this->__its) === 1 ){ $iterator->rewind(); }"` |
|    - |  386 | `" }"` |
|    - |  387 | `" public function getInnerIterator(){ return $this->__its[$this->__idx] ?? null; }"` |
|    - |  388 | `" public function getIteratorIndex(){"` |
|    - |  389 | `"  return isset($this->__its[$this->__idx]) ? $this->__idx : null;"` |
|    - |  390 | `" }"` |
|    - |  391 | `" public function getArrayIterator(){ return new ArrayIterator($this->__its); }"` |
|    - |  392 | `" private function __apAdvance(){"` |
|    - |  393 | `"  while( isset($this->__its[$this->__idx])"` |
|    - |  394 | `"   && !$this->__its[$this->__idx]->valid()"` |
|    - |  395 | `"   && isset($this->__its[$this->__idx + 1]) ){"` |
|    - |  396 | `"   $this->__idx++;"` |
|    - |  397 | `"   $this->__its[$this->__idx]->rewind();"` |
|    - |  398 | `"  }"` |
|    - |  399 | `" }"` |
|    - |  400 | `" public function rewind(){"` |
|    - |  401 | `"  $this->__idx = 0;"` |
|    - |  402 | `"  if( isset($this->__its[0]) ){ $this->__its[0]->rewind(); }"` |
|    - |  403 | `"  $this->__apAdvance();"` |
|    - |  404 | `" }"` |
|    - |  405 | `" public function valid(){"` |
|    - |  406 | `"  $in = $this->getInnerIterator();"` |
|    - |  407 | `"  return $in !== null && $in->valid();"` |
|    - |  408 | `" }"` |
|    - |  409 | `" public function current(){ $in = $this->getInnerIterator(); return $in ? $in->current() : null; }"` |
|    - |  410 | `" public function key(){ $in = $this->getInnerIterator(); return $in ? $in->key() : null; }"` |
|    - |  411 | `" public function next(){"` |
|    - |  412 | `"  $in = $this->getInnerIterator();"` |
|    - |  413 | `"  if( $in ){ $in->next(); }"` |
|    - |  414 | `"  $this->__apAdvance();"` |
|    - |  415 | `" }"` |
|    - |  416 | `"}"` |
|    - |  417 | `"class InfiniteIterator extends IteratorIterator {"` |
|    - |  418 | `" public function next(){"` |
|    - |  419 | `"  $in = $this->getInnerIterator();"` |
|    - |  420 | `"  $in->next();"` |
|    - |  421 | `"  if( !$in->valid() ){ $in->rewind(); }"` |
|    - |  422 | `" }"` |
|    - |  423 | `"}"` |
|    - |  424 | `"class NoRewindIterator extends IteratorIterator {"` |
|    - |  425 | `" public function rewind(){}"` |
|    - |  426 | `"}"` |
|    - |  427 | `"interface RecursiveIterator extends Iterator {"` |
|    - |  428 | `" public function hasChildren();"` |
|    - |  429 | `" public function getChildren();"` |
|    - |  430 | `"}"` |
|    - |  431 | `"class RecursiveArrayIterator extends ArrayIterator implements RecursiveIterator {"` |
|    - |  432 | `" const CHILD_ARRAYS_ONLY = 4;"` |
|    - |  433 | `" public function hasChildren(){"` |
|    - |  434 | `"  $c = $this->current();"` |
|    - |  435 | `"  return is_array($c) \|\| is_object($c);"` |
|    - |  436 | `" }"` |
|    - |  437 | `" public function getChildren(){"` |
|    - |  438 | `"  $c = get_class($this);"` |
|    - |  439 | `"  return new $c($this->current());"` |
|    - |  440 | `" }"` |
|    - |  441 | `"}"` |
|    - |  442 | `"class RecursiveIteratorIterator implements OuterIterator {"` |
|    - |  443 | `" const LEAVES_ONLY = 0;"` |
|    - |  444 | `" const SELF_FIRST = 1;"` |
|    - |  445 | `" const CHILD_FIRST = 2;"` |
|    - |  446 | `" const CATCH_GET_CHILD = 16;"` |
|    - |  447 | `" private $__root = null;"` |
|    - |  448 | `" private $__st = [];"` |
|    - |  449 | `" private $__mode = 0;"` |
|    - |  450 | `" private $__maxDepth = false;"` |
|    - |  451 | `" private $__post = false;"` |
|    - |  452 | `" private $__live = false;"` |
|    - |  453 | `" public function __construct($iterator, $mode = 0, $flags = 0){"` |
|    - |  454 | `"  while( $iterator instanceof IteratorAggregate ){ $iterator = $iterator->getIterator(); }"` |
|    - |  455 | `"  if( !($iterator instanceof RecursiveIterator) ){"` |
|    - |  456 | `"   throw new TypeError('RecursiveIteratorIterator::__construct(): Argument #1"` |
|    - |  457 | `" ($iterator) must be of type RecursiveIterator, ' . get_debug_type($iterator) . ' given');"` |
|    - |  458 | `"  }"` |
|    - |  459 | `"  $this->__root = $iterator;"` |
|    - |  460 | `"  $this->__mode = (int)$mode \| (int)$flags;"` |
|    - |  461 | `" }"` |
|    - |  462 | `" public function getInnerIterator(){ return end($this->__st) ?: $this->__root; }"` |
|    - |  463 | `" public function getSubIterator($level = null){"` |
|    - |  464 | `"  if( $level === null ){ $level = count($this->__st) - 1; }"` |
|    - |  465 | `"  return $this->__st[$level] ?? null;"` |
|    - |  466 | `" }"` |
|    - |  467 | `" public function getDepth(){ return count($this->__st) - 1; }"` |
|    - |  468 | `" public function getMaxDepth(){ return $this->__maxDepth; }"` |
|    - |  469 | `" public function setMaxDepth($maxDepth = -1){"` |
|    - |  470 | `"  $maxDepth = (int)$maxDepth;"` |
|    - |  471 | `"  if( $maxDepth < -1 ){"` |
|    - |  472 | `"   throw new Exception('Parameter max_depth must be >= -1');"` |
|    - |  473 | `"  }"` |
|    - |  474 | `"  $this->__maxDepth = $maxDepth === -1 ? false : $maxDepth;"` |
|    - |  475 | `" }"` |
|    - |  476 | `" public function callHasChildren(){"` |
|    - |  477 | `"  $it = end($this->__st);"` |
|    - |  478 | `"  return $it ? $it->hasChildren() : false;"` |
|    - |  479 | `" }"` |
|    - |  480 | `" public function callGetChildren(){"` |
|    - |  481 | `"  $it = end($this->__st);"` |
|    - |  482 | `"  return $it ? $it->getChildren() : null;"` |
|    - |  483 | `" }"` |
|    - |  484 | `" public function beginIteration(){}"` |
|    - |  485 | `" public function endIteration(){}"` |
|    - |  486 | `" public function beginChildren(){}"` |
|    - |  487 | `" public function endChildren(){}"` |
|    - |  488 | `" public function nextElement(){}"` |
|    - |  489 | `" private function __riDepthOk(){"` |
|    - |  490 | `"  return $this->__maxDepth === false \|\| (count($this->__st) - 1) < $this->__maxDepth;"` |
|    - |  491 | `" }"` |
|    - |  492 | `" private function __riDescend(){"` |
|    - |  493 | `"  /* push the current element's children, positioned at their start */"` |
|    - |  494 | `"  if( $this->__mode & self::CATCH_GET_CHILD ){"` |
|    - |  495 | `"   try { $child = $this->callGetChildren(); }"` |
|    - |  496 | `"   catch (Exception $e) { return false; }"` |
|    - |  497 | `"  }else{"` |
|    - |  498 | `"   $child = $this->callGetChildren();"` |
|    - |  499 | `"  }"` |
|    - |  500 | `"  if( !($child instanceof RecursiveIterator) ){ return false; }"` |
|    - |  501 | `"  $child->rewind();"` |
|    - |  502 | `"  $this->__st[] = $child;"` |
|    - |  503 | `"  $this->beginChildren();"` |
|    - |  504 | `"  return true;"` |
|    - |  505 | `" }"` |
|    - |  506 | `" private function __riFetch(){"` |
|    - |  507 | `"  $m = $this->__mode & 3;"` |
|    - |  508 | `"  for(;;){"` |
|    - |  509 | `"   if( count($this->__st) === 0 ){"` |
|    - |  510 | `"    $this->__live = false;"` |
|    - |  511 | `"    /* php keeps the root level addressable after exhaustion (getDepth 0,"` |
|    - |  512 | `"     * getSubIterator() returns the root) */"` |
|    - |  513 | `"    $this->__st = [$this->__root];"` |
|    - |  514 | `"    $this->endIteration();"` |
|    - |  515 | `"    return;"` |
|    - |  516 | `"   }"` |
|    - |  517 | `"   $it = end($this->__st);"` |
|    - |  518 | `"   if( !$it->valid() ){"` |
|    - |  519 | `"    array_pop($this->__st);"` |
|    - |  520 | `"    $this->endChildren();"` |
|    - |  521 | `"    if( count($this->__st) === 0 ){ continue; }"` |
|    - |  522 | `"    if( $m === self::CHILD_FIRST ){"` |
|    - |  523 | `"     /* the parent node yields now, after its subtree */"` |
|    - |  524 | `"     $this->__post = true;"` |
|    - |  525 | `"     $this->__live = true;"` |
|    - |  526 | `"     return;"` |
|    - |  527 | `"    }"` |
|    - |  528 | `"    end($this->__st)->next();"` |
|    - |  529 | `"    continue;"` |
|    - |  530 | `"   }"` |
|    - |  531 | `"   if( $m === self::LEAVES_ONLY && $it->hasChildren() && $this->__riDepthOk() ){"` |
|    - |  532 | `"    if( $this->__riDescend() ){ continue; }"` |
|    - |  533 | `"   }"` |
|    - |  534 | `"   if( $m === self::CHILD_FIRST && $it->hasChildren() && $this->__riDepthOk() ){"` |
|    - |  535 | `"    if( $this->__riDescend() ){ continue; }"` |
|    - |  536 | `"   }"` |
|    - |  537 | `"   $this->__post = false;"` |
|    - |  538 | `"   $this->__live = true;"` |
|    - |  539 | `"   $this->nextElement();"` |
|    - |  540 | `"   return;"` |
|    - |  541 | `"  }"` |
|    - |  542 | `" }"` |
|    - |  543 | `" public function rewind(){"` |
|    - |  544 | `"  $this->__st = [$this->__root];"` |
|    - |  545 | `"  $this->__root->rewind();"` |
|    - |  546 | `"  $this->__post = false;"` |
|    - |  547 | `"  $this->beginIteration();"` |
|    - |  548 | `"  $this->__riFetch();"` |
|    - |  549 | `" }"` |
|    - |  550 | `" public function valid(){ return $this->__live; }"` |
|    - |  551 | `" public function current(){"` |
|    - |  552 | `"  $it = end($this->__st);"` |
|    - |  553 | `"  return $it ? $it->current() : null;"` |
|    - |  554 | `" }"` |
|    - |  555 | `" public function key(){"` |
|    - |  556 | `"  $it = end($this->__st);"` |
|    - |  557 | `"  return $it ? $it->key() : null;"` |
|    - |  558 | `" }"` |
|    - |  559 | `" public function next(){"` |
|    - |  560 | `"  if( !$this->__live ){ return; }"` |
|    - |  561 | `"  $m = $this->__mode & 3;"` |
|    - |  562 | `"  $it = end($this->__st);"` |
|    - |  563 | `"  if( $this->__post ){"` |
|    - |  564 | `"   /* leaving a CHILD_FIRST post-visit: advance past the node */"` |
|    - |  565 | `"   $this->__post = false;"` |
|    - |  566 | `"   $it->next();"` |
|    - |  567 | `"   $this->__riFetch();"` |
|    - |  568 | `"   return;"` |
|    - |  569 | `"  }"` |
|    - |  570 | `"  if( $m === self::SELF_FIRST && $it->hasChildren() && $this->__riDepthOk() ){"` |
|    - |  571 | `"   if( $this->__riDescend() ){ $this->__riFetch(); return; }"` |
|    - |  572 | `"  }"` |
|    - |  573 | `"  $it->next();"` |
|    - |  574 | `"  $this->__riFetch();"` |
|    - |  575 | `" }"` |
|    - |  576 | `"}"` |
|    - |  577 | `"class WeakReference {"` |
|    - |  578 | `" private $__h = 0;"` |
|    - |  579 | `" private function __construct(){}"` |
|    - |  580 | `" public static function create($object){"` |
|    - |  581 | `"  if( !is_object($object) ){"` |
|    - |  582 | `"   throw new TypeError('WeakReference::create(): Argument #1 ($object) must be"` |
|    - |  583 | `" of type object, ' . get_debug_type($object) . ' given');"` |
|    - |  584 | `"  }"` |
|    - |  585 | `"  $w = new WeakReference();"` |
|    - |  586 | `"  $w->__h = __weak_create($object);"` |
|    - |  587 | `"  return $w;"` |
|    - |  588 | `" }"` |
|    - |  589 | `" public function get(){ return $this->__h ? __weak_get($this->__h) : null; }"` |
|    - |  590 | `" public function __destruct(){"` |
|    - |  591 | `"  if( $this->__h ){ __weak_drop($this->__h); $this->__h = 0; }"` |
|    - |  592 | `" }"` |
|    - |  593 | `"}"` |
|    - |  594 | `"class WeakMap implements ArrayAccess, Countable, IteratorAggregate {"` |
|    - |  595 | `" private $__e = [];"` |
|    - |  596 | `" private function __wmPrune(){"` |
|    - |  597 | `"  foreach( $this->__e as $id => $p ){"` |
|    - |  598 | `"   if( __weak_get($p[0]) === null ){"` |
|    - |  599 | `"    __weak_drop($p[0]);"` |
|    - |  600 | `"    unset($this->__e[$id]);"` |
|    - |  601 | `"   }"` |
|    - |  602 | `"  }"` |
|    - |  603 | `" }"` |
|    - |  604 | `" public function offsetSet($object, $value){"` |
|    - |  605 | `"  if( !is_object($object) ){"` |
|    - |  606 | `"   throw new TypeError('WeakMap key must be an object');"` |
|    - |  607 | `"  }"` |
|    - |  608 | `"  $id = spl_object_id($object);"` |
|    - |  609 | `"  if( isset($this->__e[$id]) && __weak_get($this->__e[$id][0]) !== null ){"` |
|    - |  610 | `"   $this->__e[$id][1] = $value;"` |
|    - |  611 | `"   return;"` |
|    - |  612 | `"  }"` |
|    - |  613 | `"  if( isset($this->__e[$id]) ){ __weak_drop($this->__e[$id][0]); }"` |
|    - |  614 | `"  $this->__e[$id] = [__weak_create($object), $value];"` |
|    - |  615 | `" }"` |
|    - |  616 | `" public function offsetGet($object){"` |
|    - |  617 | `"  if( !is_object($object) ){"` |
|    - |  618 | `"   throw new TypeError('WeakMap key must be an object');"` |
|    - |  619 | `"  }"` |
|    - |  620 | `"  $id = spl_object_id($object);"` |
|    - |  621 | `"  if( isset($this->__e[$id]) && __weak_get($this->__e[$id][0]) === $object ){"` |
|    - |  622 | `"   return $this->__e[$id][1];"` |
|    - |  623 | `"  }"` |
|    - |  624 | `"  throw new Error('Object ' . get_class($object) . '#' . $id . ' not contained"` |
|    - |  625 | `" in WeakMap');"` |
|    - |  626 | `" }"` |
|    - |  627 | `" public function offsetExists($object){"` |
|    - |  628 | `"  if( !is_object($object) ){"` |
|    - |  629 | `"   throw new TypeError('WeakMap key must be an object');"` |
|    - |  630 | `"  }"` |
|    - |  631 | `"  $id = spl_object_id($object);"` |
|    - |  632 | `"  return isset($this->__e[$id]) && __weak_get($this->__e[$id][0]) === $object;"` |
|    - |  633 | `" }"` |
|    - |  634 | `" public function offsetUnset($object){"` |
|    - |  635 | `"  if( !is_object($object) ){"` |
|    - |  636 | `"   throw new TypeError('WeakMap key must be an object');"` |
|    - |  637 | `"  }"` |
|    - |  638 | `"  $id = spl_object_id($object);"` |
|    - |  639 | `"  if( isset($this->__e[$id]) ){"` |
|    - |  640 | `"   __weak_drop($this->__e[$id][0]);"` |
|    - |  641 | `"   unset($this->__e[$id]);"` |
|    - |  642 | `"  }"` |
|    - |  643 | `" }"` |
|    - |  644 | `" public function count(){"` |
|    - |  645 | `"  $this->__wmPrune();"` |
|    - |  646 | `"  return count($this->__e);"` |
|    - |  647 | `" }"` |
|    - |  648 | `" public function getIterator(): Generator {"` |
|    - |  649 | `"  $this->__wmPrune();"` |
|    - |  650 | `"  foreach( $this->__e as $p ){"` |
|    - |  651 | `"   $o = __weak_get($p[0]);"` |
|    - |  652 | `"   if( $o !== null ){ yield $o => $p[1]; }"` |
|    - |  653 | `"  }"` |
|    - |  654 | `" }"` |
|    - |  655 | `" public function __destruct(){"` |
|    - |  656 | `"  foreach( $this->__e as $p ){ __weak_drop($p[0]); }"` |
|    - |  657 | `"  $this->__e = [];"` |
|    - |  658 | `" }"` |
|    - |  659 | `"}"` |
|    - |  660 | `"class EmptyIterator implements Iterator {"` |
|    - |  661 | `" public function current(){"` |
|    - |  662 | `"  throw new BadMethodCallException('Accessing the value of an EmptyIterator');"` |
|    - |  663 | `" }"` |
|    - |  664 | `" public function key(){"` |
|    - |  665 | `"  throw new BadMethodCallException('Accessing the key of an EmptyIterator');"` |
|    - |  666 | `" }"` |
|    - |  667 | `" public function next(){}"` |
|    - |  668 | `" public function rewind(){}"` |
|    - |  669 | `" public function valid(){ return false; }"` |
|    - |  670 | `"}"` |
|    - |  671 | `"class SplDoublyLinkedList implements Iterator, Countable, ArrayAccess {"` |
|    - |  672 | `" const IT_MODE_LIFO = 2;"` |
|    - |  673 | `" const IT_MODE_FIFO = 0;"` |
|    - |  674 | `" const IT_MODE_DELETE = 1;"` |
|    - |  675 | `" const IT_MODE_KEEP = 0;"` |
|    - |  676 | `" private $__q = [];"` |
|    - |  677 | `" private $__mode = 0;"` |
|    - |  678 | `" private $__i = 0;"` |
|    - |  679 | `" public function __construct(){"` |
|    - |  680 | `"  if( $this instanceof SplStack ){ $this->__mode = 2; }"` |
|    - |  681 | `" }"` |
|    - |  682 | `" public function setIteratorMode($mode){"` |
|    - |  683 | `"  $mode = (int)$mode;"` |
|    - |  684 | `"  if( ($this instanceof SplStack \|\| $this instanceof SplQueue)"` |
|    - |  685 | `"   && ($mode & 2) !== ($this->__mode & 2) ){"` |
|    - |  686 | `"   throw new RuntimeException(\"Iterators' LIFO/FIFO modes for SplStack/SplQueue"` |
|    - |  687 | `" objects are frozen\");"` |
|    - |  688 | `"  }"` |
|    - |  689 | `"  $this->__mode = $mode;"` |
|    - |  690 | `" }"` |
|    - |  691 | `" public function getIteratorMode(){ return $this->__mode; }"` |
|    - |  692 | `" public function push($value){ $this->__q[] = $value; }"` |
|    - |  693 | `" public function pop(){"` |
|    - |  694 | `"  if( count($this->__q) === 0 ){"` |
|    - |  695 | `"   throw new RuntimeException(\"Can't pop from an empty datastructure\");"` |
|    - |  696 | `"  }"` |
|    - |  697 | `"  return array_pop($this->__q);"` |
|    - |  698 | `" }"` |
|    - |  699 | `" public function shift(){"` |
|    - |  700 | `"  if( count($this->__q) === 0 ){"` |
|    - |  701 | `"   throw new RuntimeException(\"Can't shift from an empty datastructure\");"` |
|    - |  702 | `"  }"` |
|    - |  703 | `"  return array_shift($this->__q);"` |
|    - |  704 | `" }"` |
|    - |  705 | `" public function unshift($value){ array_unshift($this->__q, $value); }"` |
|    - |  706 | `" public function top(){"` |
|    - |  707 | `"  if( count($this->__q) === 0 ){"` |
|    - |  708 | `"   throw new RuntimeException(\"Can't peek at an empty datastructure\");"` |
|    - |  709 | `"  }"` |
|    - |  710 | `"  return $this->__q[count($this->__q) - 1];"` |
|    - |  711 | `" }"` |
|    - |  712 | `" public function bottom(){"` |
|    - |  713 | `"  if( count($this->__q) === 0 ){"` |
|    - |  714 | `"   throw new RuntimeException(\"Can't peek at an empty datastructure\");"` |
|    - |  715 | `"  }"` |
|    - |  716 | `"  return $this->__q[0];"` |
|    - |  717 | `" }"` |
|    - |  718 | `" public function isEmpty(){ return count($this->__q) === 0; }"` |
|    - |  719 | `" public function count(){ return count($this->__q); }"` |
|    - |  720 | `" public function toArray(){ return $this->__q; }"` |
|    - |  721 | `" public function add($index, $value){"` |
|    - |  722 | `"  $index = (int)$index;"` |
|    - |  723 | `"  if( $index < 0 \|\| $index > count($this->__q) ){"` |
|    - |  724 | `"   throw new OutOfRangeException(get_class($this) === 'SplDoublyLinkedList'"` |
|    - |  725 | `"    ? 'SplDoublyLinkedList::add(): Argument #1 ($index) is out of range'"` |
|    - |  726 | `"    : get_class($this) . '::add(): Argument #1 ($index) is out of range');"` |
|    - |  727 | `"  }"` |
|    - |  728 | `"  array_splice($this->__q, $index, 0, [$value]);"` |
|    - |  729 | `" }"` |
|    - |  730 | `" public function offsetExists($index){"` |
|    - |  731 | `"  return is_int($index) \|\| ctype_digit((string)$index)"` |
|    - |  732 | `"   ? ((int)$index >= 0 && (int)$index < count($this->__q)) : false;"` |
|    - |  733 | `" }"` |
|    - |  734 | `" public function offsetGet($index){"` |
|    - |  735 | `"  $index = (int)$index;"` |
|    - |  736 | `"  if( $index < 0 \|\| $index >= count($this->__q) ){"` |
|    - |  737 | `"   throw new OutOfRangeException('SplDoublyLinkedList::offsetGet(): Argument #1"` |
|    - |  738 | `" ($index) is out of range');"` |
|    - |  739 | `"  }"` |
|    - |  740 | `"  return $this->__q[$index];"` |
|    - |  741 | `" }"` |
|    - |  742 | `" public function offsetSet($index, $value){"` |
|    - |  743 | `"  if( $index === null ){ $this->__q[] = $value; return; }"` |
|    - |  744 | `"  $index = (int)$index;"` |
|    - |  745 | `"  if( $index < 0 \|\| $index >= count($this->__q) ){"` |
|    - |  746 | `"   throw new OutOfRangeException('SplDoublyLinkedList::offsetSet(): Argument #1"` |
|    - |  747 | `" ($index) is out of range');"` |
|    - |  748 | `"  }"` |
|    - |  749 | `"  $this->__q[$index] = $value;"` |
|    - |  750 | `" }"` |
|    - |  751 | `" public function offsetUnset($index){"` |
|    - |  752 | `"  $index = (int)$index;"` |
|    - |  753 | `"  if( $index < 0 \|\| $index >= count($this->__q) ){"` |
|    - |  754 | `"   throw new OutOfRangeException('SplDoublyLinkedList::offsetUnset(): Argument #1"` |
|    - |  755 | `" ($index) is out of range');"` |
|    - |  756 | `"  }"` |
|    - |  757 | `"  array_splice($this->__q, $index, 1);"` |
|    - |  758 | `" }"` |
|    - |  759 | `" public function rewind(){"` |
|    - |  760 | `"  $this->__i = ($this->__mode & 2) ? count($this->__q) - 1 : 0;"` |
|    - |  761 | `" }"` |
|    - |  762 | `" public function valid(){"` |
|    - |  763 | `"  return $this->__i >= 0 && $this->__i < count($this->__q);"` |
|    - |  764 | `" }"` |
|    - |  765 | `" public function current(){ return $this->__q[$this->__i] ?? null; }"` |
|    - |  766 | `" public function key(){ return $this->__i; }"` |
|    - |  767 | `" public function next(){"` |
|    - |  768 | `"  if( $this->__mode & 1 ){"` |
|    - |  769 | `"   /* IT_MODE_DELETE consumes the element just visited */"` |
|    - |  770 | `"   if( $this->__mode & 2 ){ array_pop($this->__q); $this->__i = count($this->__q) - 1; }"` |
|    - |  771 | `"   else { array_shift($this->__q); }"` |
|    - |  772 | `"  }else{"` |
|    - |  773 | `"   $this->__i += ($this->__mode & 2) ? -1 : 1;"` |
|    - |  774 | `"  }"` |
|    - |  775 | `" }"` |
|    - |  776 | `" public function prev(){ $this->__i += ($this->__mode & 2) ? 1 : -1; }"` |
|    - |  777 | `"}"` |
|    - |  778 | `"class SplStack extends SplDoublyLinkedList {}"` |
|    - |  779 | `"class SplQueue extends SplDoublyLinkedList {"` |
|    - |  780 | `" public function enqueue($value){ $this->push($value); }"` |
|    - |  781 | `" public function dequeue(){ return $this->shift(); }"` |
|    - |  782 | `"}"` |
|    - |  783 | `"abstract class SplHeap implements Iterator, Countable {"` |
|    - |  784 | `" private $__h = [];"` |
|    - |  785 | `" abstract protected function compare($value1, $value2);"` |
|    - |  786 | `" private function __hSiftUp($i){"` |
|    - |  787 | `"  while( $i > 0 ){"` |
|    - |  788 | `"   $p = ($i - 1) >> 1;"` |
|    - |  789 | `"   if( $this->compare($this->__h[$i], $this->__h[$p]) <= 0 ){ break; }"` |
|    - |  790 | `"   $t = $this->__h[$p]; $this->__h[$p] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  791 | `"   $i = $p;"` |
|    - |  792 | `"  }"` |
|    - |  793 | `" }"` |
|    - |  794 | `" private function __hSiftDown($i){"` |
|    - |  795 | `"  $n = count($this->__h);"` |
|    - |  796 | `"  for(;;){"` |
|    - |  797 | `"   $l = 2 * $i + 1; $r = $l + 1; $b = $i;"` |
|    - |  798 | `"   if( $l < $n && $this->compare($this->__h[$l], $this->__h[$b]) > 0 ){ $b = $l; }"` |
|    - |  799 | `"   if( $r < $n && $this->compare($this->__h[$r], $this->__h[$b]) > 0 ){ $b = $r; }"` |
|    - |  800 | `"   if( $b === $i ){ break; }"` |
|    - |  801 | `"   $t = $this->__h[$b]; $this->__h[$b] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  802 | `"   $i = $b;"` |
|    - |  803 | `"  }"` |
|    - |  804 | `" }"` |
|    - |  805 | `" public function insert($value){"` |
|    - |  806 | `"  $this->__h[] = $value;"` |
|    - |  807 | `"  $this->__hSiftUp(count($this->__h) - 1);"` |
|    - |  808 | `"  return true;"` |
|    - |  809 | `" }"` |
|    - |  810 | `" public function extract(){"` |
|    - |  811 | `"  $n = count($this->__h);"` |
|    - |  812 | `"  if( $n === 0 ){"` |
|    - |  813 | `"   throw new RuntimeException(\"Can't extract from an empty heap\");"` |
|    - |  814 | `"  }"` |
|    - |  815 | `"  $top = $this->__h[0];"` |
|    - |  816 | `"  $last = array_pop($this->__h);"` |
|    - |  817 | `"  if( $n > 1 ){"` |
|    - |  818 | `"   $this->__h[0] = $last;"` |
|    - |  819 | `"   $this->__hSiftDown(0);"` |
|    - |  820 | `"  }"` |
|    - |  821 | `"  return $top;"` |
|    - |  822 | `" }"` |
|    - |  823 | `" public function top(){"` |
|    - |  824 | `"  if( count($this->__h) === 0 ){"` |
|    - |  825 | `"   throw new RuntimeException(\"Can't peek at an empty heap\");"` |
|    - |  826 | `"  }"` |
|    - |  827 | `"  return $this->__h[0];"` |
|    - |  828 | `" }"` |
|    - |  829 | `" public function isEmpty(){ return count($this->__h) === 0; }"` |
|    - |  830 | `" public function count(){ return count($this->__h); }"` |
|    - |  831 | `" public function isCorrupted(){ return false; }"` |
|    - |  832 | `" public function recoverFromCorruption(){ return true; }"` |
|    - |  833 | `" public function rewind(){}"` |
|    - |  834 | `" public function valid(){ return count($this->__h) > 0; }"` |
|    - |  835 | `" public function current(){ return count($this->__h) ? $this->__h[0] : null; }"` |
|    - |  836 | `" public function key(){ return count($this->__h) - 1; }"` |
|    - |  837 | `" public function next(){ if( count($this->__h) ){ $this->extract(); } }"` |
|    - |  838 | `"}"` |
|    - |  839 | `"class SplMinHeap extends SplHeap {"` |
|    - |  840 | `" protected function compare($value1, $value2){ return $value2 <=> $value1; }"` |
|    - |  841 | `"}"` |
|    - |  842 | `"class SplMaxHeap extends SplHeap {"` |
|    - |  843 | `" protected function compare($value1, $value2){ return $value1 <=> $value2; }"` |
|    - |  844 | `"}"` |
|    - |  845 | `"class SplPriorityQueue implements Iterator, Countable {"` |
|    - |  846 | `" const EXTR_DATA = 1;"` |
|    - |  847 | `" const EXTR_PRIORITY = 2;"` |
|    - |  848 | `" const EXTR_BOTH = 3;"` |
|    - |  849 | `" private $__h = [];"` |
|    - |  850 | `" private $__serial = PHP_INT_MAX;"` |
|    - |  851 | `" private $__flags = 1;"` |
|    - |  852 | `" public function compare($priority1, $priority2){ return $priority1 <=> $priority2; }"` |
|    - |  853 | `" private function __pqCmp($a, $b){"` |
|    - |  854 | `"  /* NO tie-break: php's heap swaps only on strictly-greater, which fixes"` |
|    - |  855 | `"   * the (documented-as-undefined) equal-priority order it exhibits */"` |
|    - |  856 | `"  return $this->compare($a[0], $b[0]);"` |
|    - |  857 | `" }"` |
|    - |  858 | `" private function __pqSiftUp($i){"` |
|    - |  859 | `"  while( $i > 0 ){"` |
|    - |  860 | `"   $p = ($i - 1) >> 1;"` |
|    - |  861 | `"   if( $this->__pqCmp($this->__h[$i], $this->__h[$p]) <= 0 ){ break; }"` |
|    - |  862 | `"   $t = $this->__h[$p]; $this->__h[$p] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  863 | `"   $i = $p;"` |
|    - |  864 | `"  }"` |
|    - |  865 | `" }"` |
|    - |  866 | `" private function __pqSiftDown($i){"` |
|    - |  867 | `"  $n = count($this->__h);"` |
|    - |  868 | `"  for(;;){"` |
|    - |  869 | `"   $l = 2 * $i + 1; $r = $l + 1; $b = $i;"` |
|    - |  870 | `"   if( $l < $n && $this->__pqCmp($this->__h[$l], $this->__h[$b]) > 0 ){ $b = $l; }"` |
|    - |  871 | `"   if( $r < $n && $this->__pqCmp($this->__h[$r], $this->__h[$b]) > 0 ){ $b = $r; }"` |
|    - |  872 | `"   if( $b === $i ){ break; }"` |
|    - |  873 | `"   $t = $this->__h[$b]; $this->__h[$b] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  874 | `"   $i = $b;"` |
|    - |  875 | `"  }"` |
|    - |  876 | `" }"` |
|    - |  877 | `" public function insert($value, $priority){"` |
|    - |  878 | `"  $this->__h[] = [$priority, $this->__serial--, $value];"` |
|    - |  879 | `"  $this->__pqSiftUp(count($this->__h) - 1);"` |
|    - |  880 | `"  return true;"` |
|    - |  881 | `" }"` |
|    - |  882 | `" private function __pqShape($node){"` |
|    - |  883 | `"  if( $this->__flags === self::EXTR_BOTH ){"` |
|    - |  884 | `"   return ['data' => $node[2], 'priority' => $node[0]];"` |
|    - |  885 | `"  }"` |
|    - |  886 | `"  if( $this->__flags === self::EXTR_PRIORITY ){ return $node[0]; }"` |
|    - |  887 | `"  return $node[2];"` |
|    - |  888 | `" }"` |
|    - |  889 | `" public function extract(){"` |
|    - |  890 | `"  $n = count($this->__h);"` |
|    - |  891 | `"  if( $n === 0 ){"` |
|    - |  892 | `"   throw new RuntimeException(\"Can't extract from an empty heap\");"` |
|    - |  893 | `"  }"` |
|    - |  894 | `"  $top = $this->__h[0];"` |
|    - |  895 | `"  $last = array_pop($this->__h);"` |
|    - |  896 | `"  if( $n > 1 ){"` |
|    - |  897 | `"   $this->__h[0] = $last;"` |
|    - |  898 | `"   $this->__pqSiftDown(0);"` |
|    - |  899 | `"  }"` |
|    - |  900 | `"  return $this->__pqShape($top);"` |
|    - |  901 | `" }"` |
|    - |  902 | `" public function top(){"` |
|    - |  903 | `"  if( count($this->__h) === 0 ){"` |
|    - |  904 | `"   throw new RuntimeException(\"Can't peek at an empty heap\");"` |
|    - |  905 | `"  }"` |
|    - |  906 | `"  return $this->__pqShape($this->__h[0]);"` |
|    - |  907 | `" }"` |
|    - |  908 | `" public function setExtractFlags($flags){ $this->__flags = (int)$flags; }"` |
|    - |  909 | `" public function getExtractFlags(){ return $this->__flags; }"` |
|    - |  910 | `" public function isEmpty(){ return count($this->__h) === 0; }"` |
|    - |  911 | `" public function count(){ return count($this->__h); }"` |
|    - |  912 | `" public function isCorrupted(){ return false; }"` |
|    - |  913 | `" public function recoverFromCorruption(){ return true; }"` |
|    - |  914 | `" public function rewind(){}"` |
|    - |  915 | `" public function valid(){ return count($this->__h) > 0; }"` |
|    - |  916 | `" public function current(){ return count($this->__h) ? $this->__pqShape($this->__h[0]) : null; }"` |
|    - |  917 | `" public function key(){ return count($this->__h) - 1; }"` |
|    - |  918 | `" public function next(){ if( count($this->__h) ){ $this->extract(); } }"` |
|    - |  919 | `"}"` |
|    - |  920 | `"class SplFixedArray implements ArrayAccess, Countable, IteratorAggregate, JsonSerializable {"` |
|    - |  921 | `" private $__a = [];"` |
|    - |  922 | `" private $__n = 0;"` |
|    - |  923 | `" public function __construct($size = 0){"` |
|    - |  924 | `"  $this->setSize((int)$size);"` |
|    - |  925 | `" }"` |
|    - |  926 | `" private function __faIdx($index, $method){"` |
|    - |  927 | `"  if( !is_int($index) ){"` |
|    - |  928 | `"   if( is_string($index) && ctype_digit($index) ){"` |
|    - |  929 | `"    $index = (int)$index;"` |
|    - |  930 | `"   }else{"` |
|    - |  931 | `"    throw new TypeError('Cannot access offset of type ' . get_debug_type($index)"` |
|    - |  932 | `"     . ' on SplFixedArray');"` |
|    - |  933 | `"   }"` |
|    - |  934 | `"  }"` |
|    - |  935 | `"  if( $index < 0 \|\| $index >= $this->__n ){"` |
|    - |  936 | `"   throw new OutOfBoundsException('Index invalid or out of range');"` |
|    - |  937 | `"  }"` |
|    - |  938 | `"  return $index;"` |
|    - |  939 | `" }"` |
|    - |  940 | `" public function offsetExists($index){"` |
|    - |  941 | `"  if( !is_int($index) && !(is_string($index) && ctype_digit($index)) ){ return false; }"` |
|    - |  942 | `"  $index = (int)$index;"` |
|    - |  943 | `"  return $index >= 0 && $index < $this->__n && $this->__a[$index] !== null;"` |
|    - |  944 | `" }"` |
|    - |  945 | `" public function offsetGet($index){ return $this->__a[$this->__faIdx($index, 'offsetGet')]; }"` |
|    - |  946 | `" public function offsetSet($index, $value){ $this->__a[$this->__faIdx($index, 'offsetSet')] = $value; }"` |
|    - |  947 | `" public function offsetUnset($index){ $this->__a[$this->__faIdx($index, 'offsetUnset')] = null; }"` |
|    - |  948 | `" public function getSize(){ return $this->__n; }"` |
|    - |  949 | `" public function setSize($size){"` |
|    - |  950 | `"  $size = (int)$size;"` |
|    - |  951 | `"  if( $size < 0 ){"` |
|    - |  952 | `"   throw new ValueError('SplFixedArray::setSize(): Argument #1 ($size) must be"` |
|    - |  953 | `" greater than or equal to 0');"` |
|    - |  954 | `"  }"` |
|    - |  955 | `"  if( $size < $this->__n ){"` |
|    - |  956 | `"   $this->__a = array_slice($this->__a, 0, $size);"` |
|    - |  957 | `"  }else{"` |
|    - |  958 | `"   for( $i = $this->__n; $i < $size; $i++ ){ $this->__a[$i] = null; }"` |
|    - |  959 | `"  }"` |
|    - |  960 | `"  $this->__n = $size;"` |
|    - |  961 | `"  return true;"` |
|    - |  962 | `" }"` |
|    - |  963 | `" public function count(){ return $this->__n; }"` |
|    - |  964 | `" public function toArray(){ return $this->__a; }"` |
|    - |  965 | `" public static function fromArray($array, $preserveKeys = true){"` |
|    - |  966 | `"  $f = new SplFixedArray(0);"` |
|    - |  967 | `"  if( $preserveKeys ){"` |
|    - |  968 | `"   $max = -1;"` |
|    - |  969 | `"   foreach( $array as $k => $v ){"` |
|    - |  970 | `"    if( !is_int($k) \|\| $k < 0 ){"` |
|    - |  971 | `"     throw new InvalidArgumentException('array must contain only positive integer keys');"` |
|    - |  972 | `"    }"` |
|    - |  973 | `"    if( $k > $max ){ $max = $k; }"` |
|    - |  974 | `"   }"` |
|    - |  975 | `"   $f->setSize($max + 1);"` |
|    - |  976 | `"   foreach( $array as $k => $v ){ $f[$k] = $v; }"` |
|    - |  977 | `"  }else{"` |
|    - |  978 | `"   $vals = array_values($array);"` |
|    - |  979 | `"   $f->setSize(count($vals));"` |
|    - |  980 | `"   foreach( $vals as $k => $v ){ $f[$k] = $v; }"` |
|    - |  981 | `"  }"` |
|    - |  982 | `"  return $f;"` |
|    - |  983 | `" }"` |
|    - |  984 | `" public function getIterator(): Generator {"` |
|    - |  985 | `"  for( $i = 0; $i < $this->__n; $i++ ){ yield $i => $this->__a[$i]; }"` |
|    - |  986 | `" }"` |
|    - |  987 | `" public function jsonSerialize(){ return $this->__a; }"` |
|    - |  988 | `"}"` |
|    - |  989 | `"class SplObjectStorage implements Countable, Iterator, ArrayAccess {"` |
|    - |  990 | `" private $__o = [];"` |
|    - |  991 | `" private $__i = 0;"` |
|    - |  992 | `" public function attach($object, $info = null){"` |
|    - |  993 | `"  __spl_deprecated('Method SplObjectStorage::attach() is deprecated since 8.5, use"` |
|    - |  994 | `" method SplObjectStorage::offsetSet() instead');"` |
|    - |  995 | `"  $this->offsetSet($object, $info);"` |
|    - |  996 | `" }"` |
|    - |  997 | `" public function detach($object){"` |
|    - |  998 | `"  __spl_deprecated('Method SplObjectStorage::detach() is deprecated since 8.5, use"` |
|    - |  999 | `" method SplObjectStorage::offsetUnset() instead');"` |
|    - | 1000 | `"  $this->offsetUnset($object);"` |
|    - | 1001 | `" }"` |
|    - | 1002 | `" public function contains($object){"` |
|    - | 1003 | `"  __spl_deprecated('Method SplObjectStorage::contains() is deprecated since 8.5, use"` |
|    - | 1004 | `" method SplObjectStorage::offsetExists() instead');"` |
|    - | 1005 | `"  return $this->offsetExists($object);"` |
|    - | 1006 | `" }"` |
|    - | 1007 | `" public function offsetSet($object, $info = null){"` |
|    - | 1008 | `"  $this->__o[spl_object_id($object)] = [$object, $info];"` |
|    - | 1009 | `" }"` |
|    - | 1010 | `" public function offsetExists($object){"` |
|    - | 1011 | `"  return isset($this->__o[spl_object_id($object)]);"` |
|    - | 1012 | `" }"` |
|    - | 1013 | `" public function offsetGet($object){"` |
|    - | 1014 | `"  $id = spl_object_id($object);"` |
|    - | 1015 | `"  if( !isset($this->__o[$id]) ){"` |
|    - | 1016 | `"   throw new UnexpectedValueException('Object not found');"` |
|    - | 1017 | `"  }"` |
|    - | 1018 | `"  return $this->__o[$id][1];"` |
|    - | 1019 | `" }"` |
|    - | 1020 | `" public function offsetUnset($object){"` |
|    - | 1021 | `"  unset($this->__o[spl_object_id($object)]);"` |
|    - | 1022 | `" }"` |
|    - | 1023 | `" public function addAll($storage){"` |
|    - | 1024 | `"  foreach( $storage as $obj ){"` |
|    - | 1025 | `"   $this->offsetSet($obj, $storage[$obj]);"` |
|    - | 1026 | `"  }"` |
|    - | 1027 | `"  return $this->count();"` |
|    - | 1028 | `" }"` |
|    - | 1029 | `" public function removeAll($storage){"` |
|    - | 1030 | `"  foreach( $storage as $obj ){ $this->offsetUnset($obj); }"` |
|    - | 1031 | `"  return $this->count();"` |
|    - | 1032 | `" }"` |
|    - | 1033 | `" public function removeAllExcept($storage){"` |
|    - | 1034 | `"  foreach( $this->__o as $id => $pair ){"` |
|    - | 1035 | `"   if( !$storage->offsetExists($pair[0]) ){ unset($this->__o[$id]); }"` |
|    - | 1036 | `"  }"` |
|    - | 1037 | `"  return $this->count();"` |
|    - | 1038 | `" }"` |
|    - | 1039 | `" public function getHash($object){ return spl_object_hash($object); }"` |
|    - | 1040 | `" public function count($mode = 0){ return count($this->__o); }"` |
|    - | 1041 | `" public function getInfo(){"` |
|    - | 1042 | `"  $pair = array_values($this->__o)[$this->__i] ?? null;"` |
|    - | 1043 | `"  return $pair === null ? null : $pair[1];"` |
|    - | 1044 | `" }"` |
|    - | 1045 | `" public function setInfo($info){"` |
|    - | 1046 | `"  $keys = array_keys($this->__o);"` |
|    - | 1047 | `"  if( isset($keys[$this->__i]) ){ $this->__o[$keys[$this->__i]][1] = $info; }"` |
|    - | 1048 | `" }"` |
|    - | 1049 | `" public function rewind(){ $this->__i = 0; }"` |
|    - | 1050 | `" public function valid(){ return $this->__i < count($this->__o); }"` |
|    - | 1051 | `" public function key(){ return $this->__i; }"` |
|    - | 1052 | `" public function current(){"` |
|    - | 1053 | `"  $pair = array_values($this->__o)[$this->__i] ?? null;"` |
|    - | 1054 | `"  return $pair === null ? null : $pair[0];"` |
|    - | 1055 | `" }"` |
|    - | 1056 | `" public function next(){ $this->__i++; }"` |
|    - | 1057 | `"}"` |
|    - | 1058 | `"interface SplObserver {"` |
|    - | 1059 | `" public function update(SplSubject $subject);"` |
|    - | 1060 | `"}"` |
|    - | 1061 | `"interface SplSubject {"` |
|    - | 1062 | `" public function attach(SplObserver $observer);"` |
|    - | 1063 | `" public function detach(SplObserver $observer);"` |
|    - | 1064 | `" public function notify();"` |
|    - | 1065 | `"}"` |
|    - | 1066 | `;` |
|    - | 1067 |  |
| 3954 | 1068 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm)` |
|    5 | 1069 | `{` |
| 3959 | 1070 | `	ph7_create_function(&(*pVm),"__spl_deprecated",vm_builtin_spl_deprecated,0);` |
| 3959 | 1071 | `	ph7_create_function(&(*pVm),"__weak_create",vm_builtin_weak_create,0);` |
| 3959 | 1072 | `	ph7_create_function(&(*pVm),"__weak_get",vm_builtin_weak_get,0);` |
| 3959 | 1073 | `	ph7_create_function(&(*pVm),"__weak_drop",vm_builtin_weak_drop,0);` |
| 3959 | 1074 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zSplLib,sizeof(zSplLib)-1);` |
|    5 | 1075 | `}` |
|    - | 1076 |  |
|    - | 1077 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1078 |  |
|    - | 1079 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|    - | 1080 | `/* Tiny build: no SPL (builtin layer disabled) */` |
|    - | 1081 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|    - | 1082 | `#endif` |
|    - | 1083 |  |
