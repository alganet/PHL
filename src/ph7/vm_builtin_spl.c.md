# src/ph7/vm_builtin_spl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 56/70 lines (80.00%)

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
|    - |   20 | `	/* php DEPRECATES a handful of SPL methods (SplObjectStorage attach/detach/…);` |
|    - |   21 | `	 * PHL keeps them (they ADD surface, they don't change valid-php meaning) but does` |
|    - |   22 | `	 * not mimic php's E_DEPRECATED notice. So this helper is now a no-op. */` |
|  ! 0 |   23 | `	SXUNUSED(pCtx); SXUNUSED(nArg); SXUNUSED(apArg);` |
|  ! 0 |   24 | `	return PH7_OK;` |
|  ! 0 |   25 | `}` |
|    - |   26 |  |
|    - |   27 | `/* int __weak_create(object $obj) — register/share the weak cell for $obj,` |
|    - |   28 | ` * returning the cell pointer as an opaque int handle */` |
|   14 |   29 | `static int vm_builtin_weak_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   30 | `{` |
|   15 |   31 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |   32 | `	ph7_class_instance *pObj;` |
|   15 |   33 | `	VmWeakCell *pCell = 0;` |
|    - |   34 | `	SyHashEntry *pEntry;` |
|   15 |   35 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|  ! 0 |   36 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |   37 | `		return PH7_OK;` |
|    - |   38 | `	}` |
|   15 |   39 | `	pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|   15 |   40 | `	pEntry = SyHashGet(&pVm->hWeakCell,(const void *)&pObj,sizeof(void *));` |
|   15 |   41 | `	if( pEntry ){` |
|    3 |   42 | `		pCell = (VmWeakCell *)pEntry->pUserData;` |
|    3 |   43 | `		pCell->nRef++;` |
|    2 |   44 | `	}else{` |
|   13 |   45 | `		pCell = (VmWeakCell *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmWeakCell));` |
|   13 |   46 | `		if( pCell == 0 ){` |
|  ! 0 |   47 | `			return PH7_ContextMemoryError(pCtx);` |
|    - |   48 | `		}` |
|   13 |   49 | `		pCell->pObj = pObj;` |
|   13 |   50 | `		pCell->nRef = 1;` |
|    - |   51 | `		/* SyHash stores the key POINTER (no copy): key off the cell's own` |
|    - |   52 | `		 * pObj field — heap-stable for the entry's whole lifetime, and it` |
|    - |   53 | `		 * holds the live pointer bytes until the release hook nulls it` |
|    - |   54 | `		 * (which happens only after the entry is deleted). */` |
|   13 |   55 | `		if( SyHashInsert(&pVm->hWeakCell,(const void *)&pCell->pObj,sizeof(void *),pCell) != SXRET_OK ){` |
|  ! 0 |   56 | `			SyMemBackendFree(&pVm->sAllocator,pCell);` |
|  ! 0 |   57 | `			return PH7_ContextMemoryError(pCtx);` |
|    - |   58 | `		}` |
|    - |   59 | `	}` |
|   15 |   60 | `	ph7_result_int64(pCtx,(ph7_int64)(sxu64)(sxuptr)pCell);` |
|   15 |   61 | `	return PH7_OK;` |
|    8 |   62 | `}` |
|    - |   63 | `/* ?object __weak_get(int $handle) — the target instance, or null once dead */` |
|   28 |   64 | `static int vm_builtin_weak_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   65 | `{` |
|    - |   66 | `	VmWeakCell *pCell;` |
|   29 |   67 | `	if( nArg < 1 ){` |
|  ! 0 |   68 | `		ph7_result_null(pCtx);` |
|  ! 0 |   69 | `		return PH7_OK;` |
|    - |   70 | `	}` |
|   29 |   71 | `	pCell = (VmWeakCell *)(sxuptr)(sxu64)ph7_value_to_int64(apArg[0]);` |
|   29 |   72 | `	if( pCell == 0 \|\| pCell->pObj == 0 ){` |
|   11 |   73 | `		ph7_result_null(pCtx);` |
|   11 |   74 | `		return PH7_OK;` |
|    - |   75 | `	}` |
|    - |   76 | `	{` |
|    - |   77 | `		/* Hand the instance back: ph7_result_value's MemObjStore takes the` |
|    - |   78 | `		 * reference, so the temp holds none of its own. */` |
|    - |   79 | `		ph7_value sObj;` |
|   19 |   80 | `		PH7_MemObjInit(pCtx->pVm,&sObj);` |
|   19 |   81 | `		sObj.x.pOther = pCell->pObj;` |
|   19 |   82 | `		MemObjSetType(&sObj,MEMOBJ_OBJ);` |
|   19 |   83 | `		ph7_result_value(pCtx,&sObj);` |
|    - |   84 | `	}` |
|   19 |   85 | `	return PH7_OK;` |
|   15 |   86 | `}` |
|    - |   87 | `/* void __weak_drop(int $handle) — release one PHP-side handle */` |
|   14 |   88 | `static int vm_builtin_weak_drop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   89 | `{` |
|   15 |   90 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |   91 | `	VmWeakCell *pCell;` |
|   15 |   92 | `	if( nArg < 1 ){` |
|  ! 0 |   93 | `		return PH7_OK;` |
|    - |   94 | `	}` |
|   15 |   95 | `	pCell = (VmWeakCell *)(sxuptr)(sxu64)ph7_value_to_int64(apArg[0]);` |
|   15 |   96 | `	if( pCell == 0 \|\| pCell->nRef == 0 ){` |
|  ! 0 |   97 | `		return PH7_OK;` |
|    - |   98 | `	}` |
|   15 |   99 | `	pCell->nRef--;` |
|   15 |  100 | `	if( pCell->nRef == 0 ){` |
|   13 |  101 | `		if( pCell->pObj ){` |
|    - |  102 | `			/* Still alive: unhook the registry entry before freeing */` |
|    5 |  103 | `			void *pDummy = 0;` |
|    5 |  104 | `			SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pCell->pObj,sizeof(void *),&pDummy);` |
|    2 |  105 | `		}` |
|   13 |  106 | `		SyMemBackendFree(&pVm->sAllocator,pCell);` |
|    6 |  107 | `	}` |
|   15 |  108 | `	return PH7_OK;` |
|    8 |  109 | `}` |
|    - |  110 |  |
|    - |  111 | `static const char zSplLib[] =` |
|    - |  112 | `"interface SeekableIterator extends Iterator {"` |
|    - |  113 | `" public function seek($offset);"` |
|    - |  114 | `"}"` |
|    - |  115 | `"trait __SplStoreT {"` |
|    - |  116 | `" private $__d = [];"` |
|    - |  117 | `" private $__f = 0;"` |
|    - |  118 | `" private function __splInitStore($array, $flags, $owner){"` |
|    - |  119 | `"  /* $owner is the DECLARING method (php names the declaring class in these"` |
|    - |  120 | `"   * diagnostics, so a RecursiveArrayIterator misuse still says"` |
|    - |  121 | `"   * ArrayIterator::__construct) */"` |
|    - |  122 | `"  if( is_array($array) ){"` |
|    - |  123 | `"   $this->__d = $array;"` |
|    - |  124 | `"  }elseif( is_object($array) ){"` |
|    - |  125 | `"   __spl_deprecated($owner . '(): Using an object as a backing array for '"` |
|    - |  126 | `"    . get_class($this) . ' is deprecated, as it allows violating class"` |
|    - |  127 | `" constraints and invariants');"` |
|    - |  128 | `"  $this->__d = get_object_vars($array);"` |
|    - |  129 | `"  }else{"` |
|    - |  130 | `"   throw new TypeError($owner . '(): Argument #1 ($array) must be of type"` |
|    - |  131 | `" array, ' . get_debug_type($array) . ' given');"` |
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
|    - |  159 | `"  $this->__splInitStore($array, $flags, 'ArrayIterator::__construct');"` |
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
|    - |  185 | `"  $this->__splInitStore($array, $flags, 'ArrayObject::__construct');"` |
|    - |  186 | `"  if( $iteratorClass !== 'ArrayIterator' ){ $this->setIteratorClass($iteratorClass); }"` |
|    - |  187 | `" }"` |
|    - |  188 | `" public function getIterator(){"` |
|    - |  189 | `"  $c = $this->__it;"` |
|    - |  190 | `"  return new $c($this->__d);"` |
|    - |  191 | `" }"` |
|    - |  192 | `" public function exchangeArray($array){"` |
|    - |  193 | `"  $old = $this->__d;"` |
|    - |  194 | `"  $this->__splInitStore($array, $this->__f, 'ArrayObject::exchangeArray');"` |
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
|    - |  208 | `"  /* ?? null: a missing key must not raise php's undefined-array-key warning"` |
|    - |  209 | `"   * from INSIDE the wrapper (php's ArrayObject warns about the PROPERTY, and"` |
|    - |  210 | `"   * PHL's own magic-read path already diagnoses that) */"` |
|    - |  211 | `"  if( $this->__f & 2 ){ return $this->__d[$name] ?? null; }"` |
|    - |  212 | `"  return null;"` |
|    - |  213 | `" }"` |
|    - |  214 | `" public function __set($name, $value){"` |
|    - |  215 | `"  if( $this->__f & 2 ){ $this->__d[$name] = $value; return; }"` |
|    - |  216 | `"  $this->{$name} = $value;"` |
|    - |  217 | `" }"` |
|    - |  218 | `" public function __isset($name){"` |
|    - |  219 | `"  if( $this->__f & 2 ){ return isset($this->__d[$name]); }"` |
|    - |  220 | `"  return false;"` |
|    - |  221 | `" }"` |
|    - |  222 | `" public function __unset($name){"` |
|    - |  223 | `"  if( $this->__f & 2 ){ unset($this->__d[$name]); }"` |
|    - |  224 | `" }"` |
|    - |  225 | `"}"` |
|    - |  226 | `"function natsort(&$array){ return uasort($array, 'strnatcmp'); }"` |
|    - |  227 | `"function natcasesort(&$array){ return uasort($array, 'strnatcasecmp'); }"` |
|    - |  228 | `"interface OuterIterator extends Iterator {"` |
|    - |  229 | `" public function getInnerIterator();"` |
|    - |  230 | `"}"` |
|    - |  231 | `"class IteratorIterator implements OuterIterator {"` |
|    - |  232 | `" private $__in = null;"` |
|    - |  233 | `" public function __construct($iterator, $class = null){"` |
|    - |  234 | `"  while( $iterator instanceof IteratorAggregate ){ $iterator = $iterator->getIterator(); }"` |
|    - |  235 | `"  if( !($iterator instanceof Iterator) ){"` |
|    - |  236 | `"   throw new TypeError(get_class($this) . '::__construct(): Argument #1 ($iterator)"` |
|    - |  237 | `" must be of type Traversable, ' . get_debug_type($iterator) . ' given');"` |
|    - |  238 | `"  }"` |
|    - |  239 | `"  $this->__in = $iterator;"` |
|    - |  240 | `" }"` |
|    - |  241 | `" public function getInnerIterator(){ return $this->__in; }"` |
|    - |  242 | `" public function current(){ return $this->__in->current(); }"` |
|    - |  243 | `" public function key(){ return $this->__in->key(); }"` |
|    - |  244 | `" public function next(){ $this->__in->next(); }"` |
|    - |  245 | `" public function rewind(){ $this->__in->rewind(); }"` |
|    - |  246 | `" public function valid(){ return $this->__in->valid(); }"` |
|    - |  247 | `"}"` |
|    - |  248 | `"class LimitIterator extends IteratorIterator {"` |
|    - |  249 | `" private $__off = 0;"` |
|    - |  250 | `" private $__lim = -1;"` |
|    - |  251 | `" private $__pos = 0;"` |
|    - |  252 | `" public function __construct($iterator, $offset = 0, $limit = -1){"` |
|    - |  253 | `"  $offset = (int)$offset; $limit = (int)$limit;"` |
|    - |  254 | `"  if( $offset < 0 ){"` |
|    - |  255 | `"   throw new ValueError('LimitIterator::__construct(): Argument #2 ($offset) must be"` |
|    - |  256 | `" greater than or equal to 0');"` |
|    - |  257 | `"  }"` |
|    - |  258 | `"  if( $limit < -1 ){"` |
|    - |  259 | `"   throw new ValueError('LimitIterator::__construct(): Argument #3 ($limit) must be"` |
|    - |  260 | `" greater than or equal to -1');"` |
|    - |  261 | `"  }"` |
|    - |  262 | `"  parent::__construct($iterator);"` |
|    - |  263 | `"  $this->__off = $offset;"` |
|    - |  264 | `"  $this->__lim = $limit;"` |
|    - |  265 | `" }"` |
|    - |  266 | `" public function rewind(){"` |
|    - |  267 | `"  $in = $this->getInnerIterator();"` |
|    - |  268 | `"  $in->rewind();"` |
|    - |  269 | `"  for( $i = 0; $i < $this->__off && $in->valid(); $i++ ){ $in->next(); }"` |
|    - |  270 | `"  $this->__pos = $this->__off;"` |
|    - |  271 | `" }"` |
|    - |  272 | `" public function valid(){"` |
|    - |  273 | `"  if( $this->__lim != -1 && $this->__pos >= $this->__off + $this->__lim ){ return false; }"` |
|    - |  274 | `"  return $this->getInnerIterator()->valid();"` |
|    - |  275 | `" }"` |
|    - |  276 | `" public function next(){ $this->__pos++; $this->getInnerIterator()->next(); }"` |
|    - |  277 | `" public function getPosition(){ return $this->__pos; }"` |
|    - |  278 | `" public function seek($offset){"` |
|    - |  279 | `"  $offset = (int)$offset;"` |
|    - |  280 | `"  if( $offset < $this->__off ){"` |
|    - |  281 | `"   throw new OutOfBoundsException('Cannot seek to ' . $offset . ' which is below the"` |
|    - |  282 | `" offset ' . $this->__off);"` |
|    - |  283 | `"  }"` |
|    - |  284 | `"  if( $this->__lim != -1 && $offset >= $this->__off + $this->__lim ){"` |
|    - |  285 | `"   throw new OutOfBoundsException('Cannot seek to ' . $offset . ' which is behind or"` |
|    - |  286 | `" equal to the limit ' . $this->__lim . ' plus the offset ' . $this->__off);"` |
|    - |  287 | `"  }"` |
|    - |  288 | `"  $in = $this->getInnerIterator();"` |
|    - |  289 | `"  $in->rewind();"` |
|    - |  290 | `"  for( $i = 0; $i < $offset && $in->valid(); $i++ ){ $in->next(); }"` |
|    - |  291 | `"  $this->__pos = $offset;"` |
|    - |  292 | `"  return $this->__pos;"` |
|    - |  293 | `" }"` |
|    - |  294 | `"}"` |
|    - |  295 | `"abstract class FilterIterator extends IteratorIterator {"` |
|    - |  296 | `" abstract public function accept();"` |
|    - |  297 | `" private function __fiFetch(){"` |
|    - |  298 | `"  $in = $this->getInnerIterator();"` |
|    - |  299 | `"  while( $in->valid() && !$this->accept() ){ $in->next(); }"` |
|    - |  300 | `" }"` |
|    - |  301 | `" public function rewind(){ $this->getInnerIterator()->rewind(); $this->__fiFetch(); }"` |
|    - |  302 | `" public function next(){ $this->getInnerIterator()->next(); $this->__fiFetch(); }"` |
|    - |  303 | `"}"` |
|    - |  304 | `"class CallbackFilterIterator extends FilterIterator {"` |
|    - |  305 | `" private $__cb = null;"` |
|    - |  306 | `" public function __construct($iterator, $callback){"` |
|    - |  307 | `"  parent::__construct($iterator);"` |
|    - |  308 | `"  $this->__cb = $callback;"` |
|    - |  309 | `" }"` |
|    - |  310 | `" public function accept(){"` |
|    - |  311 | `"  $in = $this->getInnerIterator();"` |
|    - |  312 | `"  return (bool)call_user_func($this->__cb, $in->current(), $in->key(), $in);"` |
|    - |  313 | `" }"` |
|    - |  314 | `"}"` |
|    - |  315 | `"class RegexIterator extends FilterIterator {"` |
|    - |  316 | `" const USE_KEY = 1;"` |
|    - |  317 | `" const INVERT_MATCH = 2;"` |
|    - |  318 | `" const MATCH = 0;"` |
|    - |  319 | `" const GET_MATCH = 1;"` |
|    - |  320 | `" const ALL_MATCHES = 2;"` |
|    - |  321 | `" const SPLIT = 3;"` |
|    - |  322 | `" const REPLACE = 4;"` |
|    - |  323 | `" public $replacement = null;"` |
|    - |  324 | `" private $__re = '';"` |
|    - |  325 | `" private $__mode = 0;"` |
|    - |  326 | `" private $__rflags = 0;"` |
|    - |  327 | `" private $__pflags = 0;"` |
|    - |  328 | `" private $__cur = null;"` |
|    - |  329 | `" public function __construct($iterator, $pattern, $mode = 0, $flags = 0, $pregFlags = 0){"` |
|    - |  330 | `"  parent::__construct($iterator);"` |
|    - |  331 | `"  $this->__re = (string)$pattern;"` |
|    - |  332 | `"  $this->__mode = (int)$mode;"` |
|    - |  333 | `"  $this->__rflags = (int)$flags;"` |
|    - |  334 | `"  $this->__pflags = (int)$pregFlags;"` |
|    - |  335 | `" }"` |
|    - |  336 | `" public function accept(){"` |
|    - |  337 | `"  $in = $this->getInnerIterator();"` |
|    - |  338 | `"  if( !$in->valid() ){ return false; }"` |
|    - |  339 | `"  $subject = ($this->__rflags & self::USE_KEY) ? $in->key() : $in->current();"` |
|    - |  340 | `"  $subject = (string)$subject;"` |
|    - |  341 | `"  $this->__cur = null;"` |
|    - |  342 | `"  $ok = false;"` |
|    - |  343 | `"  if( $this->__mode === self::MATCH ){"` |
|    - |  344 | `"   $ok = preg_match($this->__re, $subject) > 0;"` |
|    - |  345 | `"  }elseif( $this->__mode === self::GET_MATCH ){"` |
|    - |  346 | `"   $m = null;"` |
|    - |  347 | `"   $ok = preg_match($this->__re, $subject, $m, $this->__pflags) > 0;"` |
|    - |  348 | `"   $this->__cur = $m;"` |
|    - |  349 | `"  }elseif( $this->__mode === self::ALL_MATCHES ){"` |
|    - |  350 | `"   $m = null;"` |
|    - |  351 | `"   $ok = preg_match_all($this->__re, $subject, $m, $this->__pflags) > 0;"` |
|    - |  352 | `"   $this->__cur = $m;"` |
|    - |  353 | `"  }elseif( $this->__mode === self::SPLIT ){"` |
|    - |  354 | `"   $this->__cur = preg_split($this->__re, $subject, -1, $this->__pflags);"` |
|    - |  355 | `"   $ok = is_array($this->__cur) && count($this->__cur) > 1;"` |
|    - |  356 | `"  }elseif( $this->__mode === self::REPLACE ){"` |
|    - |  357 | `"   $n = 0;"` |
|    - |  358 | `"   $this->__cur = preg_replace($this->__re, (string)$this->replacement, $subject, -1, $n);"` |
|    - |  359 | `"   $ok = $n > 0;"` |
|    - |  360 | `"  }"` |
|    - |  361 | `"  if( $this->__rflags & self::INVERT_MATCH ){ $ok = !$ok; }"` |
|    - |  362 | `"  return $ok;"` |
|    - |  363 | `" }"` |
|    - |  364 | `" public function current(){"` |
|    - |  365 | `"  if( $this->__mode === self::MATCH ){ return $this->getInnerIterator()->current(); }"` |
|    - |  366 | `"  return $this->__cur;"` |
|    - |  367 | `" }"` |
|    - |  368 | `" public function getRegex(){ return $this->__re; }"` |
|    - |  369 | `" public function getMode(){ return $this->__mode; }"` |
|    - |  370 | `" public function setMode($mode){ $this->__mode = (int)$mode; }"` |
|    - |  371 | `" public function getFlags(){ return $this->__rflags; }"` |
|    - |  372 | `" public function setFlags($flags){ $this->__rflags = (int)$flags; }"` |
|    - |  373 | `" public function getPregFlags(){ return $this->__pflags; }"` |
|    - |  374 | `" public function setPregFlags($pregFlags){ $this->__pflags = (int)$pregFlags; }"` |
|    - |  375 | `"}"` |
|    - |  376 | `"class AppendIterator implements OuterIterator {"` |
|    - |  377 | `" private $__its = [];"` |
|    - |  378 | `" private $__idx = 0;"` |
|    - |  379 | `" public function __construct(){}"` |
|    - |  380 | `" public function append($iterator){"` |
|    - |  381 | `"  $this->__its[] = $iterator;"` |
|    - |  382 | `"  if( count($this->__its) === 1 ){ $iterator->rewind(); }"` |
|    - |  383 | `" }"` |
|    - |  384 | `" public function getInnerIterator(){ return $this->__its[$this->__idx] ?? null; }"` |
|    - |  385 | `" public function getIteratorIndex(){"` |
|    - |  386 | `"  return isset($this->__its[$this->__idx]) ? $this->__idx : null;"` |
|    - |  387 | `" }"` |
|    - |  388 | `" public function getArrayIterator(){ return new ArrayIterator($this->__its); }"` |
|    - |  389 | `" private function __apAdvance(){"` |
|    - |  390 | `"  while( isset($this->__its[$this->__idx])"` |
|    - |  391 | `"   && !$this->__its[$this->__idx]->valid()"` |
|    - |  392 | `"   && isset($this->__its[$this->__idx + 1]) ){"` |
|    - |  393 | `"   $this->__idx++;"` |
|    - |  394 | `"   $this->__its[$this->__idx]->rewind();"` |
|    - |  395 | `"  }"` |
|    - |  396 | `" }"` |
|    - |  397 | `" public function rewind(){"` |
|    - |  398 | `"  $this->__idx = 0;"` |
|    - |  399 | `"  if( isset($this->__its[0]) ){ $this->__its[0]->rewind(); }"` |
|    - |  400 | `"  $this->__apAdvance();"` |
|    - |  401 | `" }"` |
|    - |  402 | `" public function valid(){"` |
|    - |  403 | `"  $in = $this->getInnerIterator();"` |
|    - |  404 | `"  return $in !== null && $in->valid();"` |
|    - |  405 | `" }"` |
|    - |  406 | `" public function current(){ $in = $this->getInnerIterator(); return $in ? $in->current() : null; }"` |
|    - |  407 | `" public function key(){ $in = $this->getInnerIterator(); return $in ? $in->key() : null; }"` |
|    - |  408 | `" public function next(){"` |
|    - |  409 | `"  $in = $this->getInnerIterator();"` |
|    - |  410 | `"  if( $in ){ $in->next(); }"` |
|    - |  411 | `"  $this->__apAdvance();"` |
|    - |  412 | `" }"` |
|    - |  413 | `"}"` |
|    - |  414 | `"class InfiniteIterator extends IteratorIterator {"` |
|    - |  415 | `" public function next(){"` |
|    - |  416 | `"  $in = $this->getInnerIterator();"` |
|    - |  417 | `"  $in->next();"` |
|    - |  418 | `"  if( !$in->valid() ){ $in->rewind(); }"` |
|    - |  419 | `" }"` |
|    - |  420 | `"}"` |
|    - |  421 | `"class NoRewindIterator extends IteratorIterator {"` |
|    - |  422 | `" public function rewind(){}"` |
|    - |  423 | `"}"` |
|    - |  424 | `"interface RecursiveIterator extends Iterator {"` |
|    - |  425 | `" public function hasChildren();"` |
|    - |  426 | `" public function getChildren();"` |
|    - |  427 | `"}"` |
|    - |  428 | `"class RecursiveArrayIterator extends ArrayIterator implements RecursiveIterator {"` |
|    - |  429 | `" const CHILD_ARRAYS_ONLY = 4;"` |
|    - |  430 | `" public function hasChildren(){"` |
|    - |  431 | `"  $c = $this->current();"` |
|    - |  432 | `"  return is_array($c) \|\| is_object($c);"` |
|    - |  433 | `" }"` |
|    - |  434 | `" public function getChildren(){"` |
|    - |  435 | `"  $c = get_class($this);"` |
|    - |  436 | `"  return new $c($this->current());"` |
|    - |  437 | `" }"` |
|    - |  438 | `"}"` |
|    - |  439 | `"abstract class RecursiveFilterIterator extends FilterIterator implements RecursiveIterator {"` |
|    - |  440 | `" public function __construct(RecursiveIterator $iterator){"` |
|    - |  441 | `"  parent::__construct($iterator);"` |
|    - |  442 | `" }"` |
|    - |  443 | `" public function hasChildren(){"` |
|    - |  444 | `"  return $this->getInnerIterator()->hasChildren();"` |
|    - |  445 | `" }"` |
|    - |  446 | `" public function getChildren(){"` |
|    - |  447 | `"  return new static($this->getInnerIterator()->getChildren());"` |
|    - |  448 | `" }"` |
|    - |  449 | `"}"` |
|    - |  450 | `"class RecursiveIteratorIterator implements OuterIterator {"` |
|    - |  451 | `" const LEAVES_ONLY = 0;"` |
|    - |  452 | `" const SELF_FIRST = 1;"` |
|    - |  453 | `" const CHILD_FIRST = 2;"` |
|    - |  454 | `" const CATCH_GET_CHILD = 16;"` |
|    - |  455 | `" private $__root = null;"` |
|    - |  456 | `" private $__st = [];"` |
|    - |  457 | `" private $__mode = 0;"` |
|    - |  458 | `" private $__maxDepth = false;"` |
|    - |  459 | `" private $__post = false;"` |
|    - |  460 | `" private $__live = false;"` |
|    - |  461 | `" public function __construct($iterator, $mode = 0, $flags = 0){"` |
|    - |  462 | `"  while( $iterator instanceof IteratorAggregate ){ $iterator = $iterator->getIterator(); }"` |
|    - |  463 | `"  if( !($iterator instanceof RecursiveIterator) ){"` |
|    - |  464 | `"   throw new TypeError('RecursiveIteratorIterator::__construct(): Argument #1"` |
|    - |  465 | `" ($iterator) must be of type RecursiveIterator, ' . get_debug_type($iterator) . ' given');"` |
|    - |  466 | `"  }"` |
|    - |  467 | `"  $this->__root = $iterator;"` |
|    - |  468 | `"  $this->__mode = (int)$mode \| (int)$flags;"` |
|    - |  469 | `" }"` |
|    - |  470 | `" public function getInnerIterator(){ return end($this->__st) ?: $this->__root; }"` |
|    - |  471 | `" public function getSubIterator($level = null){"` |
|    - |  472 | `"  if( $level === null ){ $level = count($this->__st) - 1; }"` |
|    - |  473 | `"  return $this->__st[$level] ?? null;"` |
|    - |  474 | `" }"` |
|    - |  475 | `" public function getDepth(){ return count($this->__st) - 1; }"` |
|    - |  476 | `" public function getMaxDepth(){ return $this->__maxDepth; }"` |
|    - |  477 | `" public function setMaxDepth($maxDepth = -1){"` |
|    - |  478 | `"  $maxDepth = (int)$maxDepth;"` |
|    - |  479 | `"  if( $maxDepth < -1 ){"` |
|    - |  480 | `"   throw new Exception('Parameter max_depth must be >= -1');"` |
|    - |  481 | `"  }"` |
|    - |  482 | `"  $this->__maxDepth = $maxDepth === -1 ? false : $maxDepth;"` |
|    - |  483 | `" }"` |
|    - |  484 | `" public function callHasChildren(){"` |
|    - |  485 | `"  $it = end($this->__st);"` |
|    - |  486 | `"  return $it ? $it->hasChildren() : false;"` |
|    - |  487 | `" }"` |
|    - |  488 | `" public function callGetChildren(){"` |
|    - |  489 | `"  $it = end($this->__st);"` |
|    - |  490 | `"  return $it ? $it->getChildren() : null;"` |
|    - |  491 | `" }"` |
|    - |  492 | `" public function beginIteration(){}"` |
|    - |  493 | `" public function endIteration(){}"` |
|    - |  494 | `" public function beginChildren(){}"` |
|    - |  495 | `" public function endChildren(){}"` |
|    - |  496 | `" public function nextElement(){}"` |
|    - |  497 | `" private function __riDepthOk(){"` |
|    - |  498 | `"  return $this->__maxDepth === false \|\| (count($this->__st) - 1) < $this->__maxDepth;"` |
|    - |  499 | `" }"` |
|    - |  500 | `" private function __riDescend(){"` |
|    - |  501 | `"  /* push the current element's children, positioned at their start */"` |
|    - |  502 | `"  if( $this->__mode & self::CATCH_GET_CHILD ){"` |
|    - |  503 | `"   try { $child = $this->callGetChildren(); }"` |
|    - |  504 | `"   catch (Exception $e) { return false; }"` |
|    - |  505 | `"  }else{"` |
|    - |  506 | `"   $child = $this->callGetChildren();"` |
|    - |  507 | `"  }"` |
|    - |  508 | `"  if( !($child instanceof RecursiveIterator) ){ return false; }"` |
|    - |  509 | `"  $child->rewind();"` |
|    - |  510 | `"  $this->__st[] = $child;"` |
|    - |  511 | `"  $this->beginChildren();"` |
|    - |  512 | `"  return true;"` |
|    - |  513 | `" }"` |
|    - |  514 | `" private function __riFetch(){"` |
|    - |  515 | `"  $m = $this->__mode & 3;"` |
|    - |  516 | `"  for(;;){"` |
|    - |  517 | `"   if( count($this->__st) === 0 ){"` |
|    - |  518 | `"    $this->__live = false;"` |
|    - |  519 | `"    /* php keeps the root level addressable after exhaustion (getDepth 0,"` |
|    - |  520 | `"     * getSubIterator() returns the root) */"` |
|    - |  521 | `"    $this->__st = [$this->__root];"` |
|    - |  522 | `"    $this->endIteration();"` |
|    - |  523 | `"    return;"` |
|    - |  524 | `"   }"` |
|    - |  525 | `"   $it = end($this->__st);"` |
|    - |  526 | `"   if( !$it->valid() ){"` |
|    - |  527 | `"    array_pop($this->__st);"` |
|    - |  528 | `"    $this->endChildren();"` |
|    - |  529 | `"    if( count($this->__st) === 0 ){ continue; }"` |
|    - |  530 | `"    if( $m === self::CHILD_FIRST ){"` |
|    - |  531 | `"     /* the parent node yields now, after its subtree */"` |
|    - |  532 | `"     $this->__post = true;"` |
|    - |  533 | `"     $this->__live = true;"` |
|    - |  534 | `"     return;"` |
|    - |  535 | `"    }"` |
|    - |  536 | `"    end($this->__st)->next();"` |
|    - |  537 | `"    continue;"` |
|    - |  538 | `"   }"` |
|    - |  539 | `"   if( $m === self::LEAVES_ONLY && $it->hasChildren() && $this->__riDepthOk() ){"` |
|    - |  540 | `"    if( $this->__riDescend() ){ continue; }"` |
|    - |  541 | `"   }"` |
|    - |  542 | `"   if( $m === self::CHILD_FIRST && $it->hasChildren() && $this->__riDepthOk() ){"` |
|    - |  543 | `"    if( $this->__riDescend() ){ continue; }"` |
|    - |  544 | `"   }"` |
|    - |  545 | `"   $this->__post = false;"` |
|    - |  546 | `"   $this->__live = true;"` |
|    - |  547 | `"   $this->nextElement();"` |
|    - |  548 | `"   return;"` |
|    - |  549 | `"  }"` |
|    - |  550 | `" }"` |
|    - |  551 | `" public function rewind(){"` |
|    - |  552 | `"  $this->__st = [$this->__root];"` |
|    - |  553 | `"  $this->__root->rewind();"` |
|    - |  554 | `"  $this->__post = false;"` |
|    - |  555 | `"  $this->beginIteration();"` |
|    - |  556 | `"  $this->__riFetch();"` |
|    - |  557 | `" }"` |
|    - |  558 | `" public function valid(){ return $this->__live; }"` |
|    - |  559 | `" public function current(){"` |
|    - |  560 | `"  $it = end($this->__st);"` |
|    - |  561 | `"  return $it ? $it->current() : null;"` |
|    - |  562 | `" }"` |
|    - |  563 | `" public function key(){"` |
|    - |  564 | `"  $it = end($this->__st);"` |
|    - |  565 | `"  return $it ? $it->key() : null;"` |
|    - |  566 | `" }"` |
|    - |  567 | `" public function next(){"` |
|    - |  568 | `"  if( !$this->__live ){ return; }"` |
|    - |  569 | `"  $m = $this->__mode & 3;"` |
|    - |  570 | `"  $it = end($this->__st);"` |
|    - |  571 | `"  if( $this->__post ){"` |
|    - |  572 | `"   /* leaving a CHILD_FIRST post-visit: advance past the node */"` |
|    - |  573 | `"   $this->__post = false;"` |
|    - |  574 | `"   $it->next();"` |
|    - |  575 | `"   $this->__riFetch();"` |
|    - |  576 | `"   return;"` |
|    - |  577 | `"  }"` |
|    - |  578 | `"  if( $m === self::SELF_FIRST && $it->hasChildren() && $this->__riDepthOk() ){"` |
|    - |  579 | `"   if( $this->__riDescend() ){ $this->__riFetch(); return; }"` |
|    - |  580 | `"  }"` |
|    - |  581 | `"  $it->next();"` |
|    - |  582 | `"  $this->__riFetch();"` |
|    - |  583 | `" }"` |
|    - |  584 | `"}"` |
|    - |  585 | `"class WeakReference {"` |
|    - |  586 | `" private $__h = 0;"` |
|    - |  587 | `" private function __construct(){}"` |
|    - |  588 | `" public static function create($object){"` |
|    - |  589 | `"  if( !is_object($object) ){"` |
|    - |  590 | `"   throw new TypeError('WeakReference::create(): Argument #1 ($object) must be"` |
|    - |  591 | `" of type object, ' . get_debug_type($object) . ' given');"` |
|    - |  592 | `"  }"` |
|    - |  593 | `"  $w = new WeakReference();"` |
|    - |  594 | `"  $w->__h = __weak_create($object);"` |
|    - |  595 | `"  return $w;"` |
|    - |  596 | `" }"` |
|    - |  597 | `" public function get(){ return $this->__h ? __weak_get($this->__h) : null; }"` |
|    - |  598 | `" public function __destruct(){"` |
|    - |  599 | `"  if( $this->__h ){ __weak_drop($this->__h); $this->__h = 0; }"` |
|    - |  600 | `" }"` |
|    - |  601 | `"}"` |
|    - |  602 | `"class WeakMap implements ArrayAccess, Countable, IteratorAggregate {"` |
|    - |  603 | `" private $__e = [];"` |
|    - |  604 | `" private function __wmPrune(){"` |
|    - |  605 | `"  foreach( $this->__e as $id => $p ){"` |
|    - |  606 | `"   if( __weak_get($p[0]) === null ){"` |
|    - |  607 | `"    __weak_drop($p[0]);"` |
|    - |  608 | `"    unset($this->__e[$id]);"` |
|    - |  609 | `"   }"` |
|    - |  610 | `"  }"` |
|    - |  611 | `" }"` |
|    - |  612 | `" public function offsetSet($object, $value){"` |
|    - |  613 | `"  if( !is_object($object) ){"` |
|    - |  614 | `"   throw new TypeError('WeakMap key must be an object');"` |
|    - |  615 | `"  }"` |
|    - |  616 | `"  $id = spl_object_id($object);"` |
|    - |  617 | `"  if( isset($this->__e[$id]) && __weak_get($this->__e[$id][0]) !== null ){"` |
|    - |  618 | `"   $this->__e[$id][1] = $value;"` |
|    - |  619 | `"   return;"` |
|    - |  620 | `"  }"` |
|    - |  621 | `"  if( isset($this->__e[$id]) ){ __weak_drop($this->__e[$id][0]); }"` |
|    - |  622 | `"  $this->__e[$id] = [__weak_create($object), $value];"` |
|    - |  623 | `" }"` |
|    - |  624 | `" public function offsetGet($object){"` |
|    - |  625 | `"  if( !is_object($object) ){"` |
|    - |  626 | `"   throw new TypeError('WeakMap key must be an object');"` |
|    - |  627 | `"  }"` |
|    - |  628 | `"  $id = spl_object_id($object);"` |
|    - |  629 | `"  if( isset($this->__e[$id]) && __weak_get($this->__e[$id][0]) === $object ){"` |
|    - |  630 | `"   return $this->__e[$id][1];"` |
|    - |  631 | `"  }"` |
|    - |  632 | `"  throw new Error('Object ' . get_class($object) . '#' . $id . ' not contained"` |
|    - |  633 | `" in WeakMap');"` |
|    - |  634 | `" }"` |
|    - |  635 | `" public function offsetExists($object){"` |
|    - |  636 | `"  if( !is_object($object) ){"` |
|    - |  637 | `"   throw new TypeError('WeakMap key must be an object');"` |
|    - |  638 | `"  }"` |
|    - |  639 | `"  $id = spl_object_id($object);"` |
|    - |  640 | `"  return isset($this->__e[$id]) && __weak_get($this->__e[$id][0]) === $object;"` |
|    - |  641 | `" }"` |
|    - |  642 | `" public function offsetUnset($object){"` |
|    - |  643 | `"  if( !is_object($object) ){"` |
|    - |  644 | `"   throw new TypeError('WeakMap key must be an object');"` |
|    - |  645 | `"  }"` |
|    - |  646 | `"  $id = spl_object_id($object);"` |
|    - |  647 | `"  if( isset($this->__e[$id]) ){"` |
|    - |  648 | `"   __weak_drop($this->__e[$id][0]);"` |
|    - |  649 | `"   unset($this->__e[$id]);"` |
|    - |  650 | `"  }"` |
|    - |  651 | `" }"` |
|    - |  652 | `" public function count(){"` |
|    - |  653 | `"  $this->__wmPrune();"` |
|    - |  654 | `"  return count($this->__e);"` |
|    - |  655 | `" }"` |
|    - |  656 | `" public function getIterator(): Generator {"` |
|    - |  657 | `"  $this->__wmPrune();"` |
|    - |  658 | `"  foreach( $this->__e as $p ){"` |
|    - |  659 | `"   $o = __weak_get($p[0]);"` |
|    - |  660 | `"   if( $o !== null ){ yield $o => $p[1]; }"` |
|    - |  661 | `"  }"` |
|    - |  662 | `" }"` |
|    - |  663 | `" public function __destruct(){"` |
|    - |  664 | `"  foreach( $this->__e as $p ){ __weak_drop($p[0]); }"` |
|    - |  665 | `"  $this->__e = [];"` |
|    - |  666 | `" }"` |
|    - |  667 | `"}"` |
|    - |  668 | `"class EmptyIterator implements Iterator {"` |
|    - |  669 | `" public function current(){"` |
|    - |  670 | `"  throw new BadMethodCallException('Accessing the value of an EmptyIterator');"` |
|    - |  671 | `" }"` |
|    - |  672 | `" public function key(){"` |
|    - |  673 | `"  throw new BadMethodCallException('Accessing the key of an EmptyIterator');"` |
|    - |  674 | `" }"` |
|    - |  675 | `" public function next(){}"` |
|    - |  676 | `" public function rewind(){}"` |
|    - |  677 | `" public function valid(){ return false; }"` |
|    - |  678 | `"}"` |
|    - |  679 | `"class SplDoublyLinkedList implements Iterator, Countable, ArrayAccess {"` |
|    - |  680 | `" const IT_MODE_LIFO = 2;"` |
|    - |  681 | `" const IT_MODE_FIFO = 0;"` |
|    - |  682 | `" const IT_MODE_DELETE = 1;"` |
|    - |  683 | `" const IT_MODE_KEEP = 0;"` |
|    - |  684 | `" private $__q = [];"` |
|    - |  685 | `" private $__mode = 0;"` |
|    - |  686 | `" private $__i = 0;"` |
|    - |  687 | `" public function __construct(){"` |
|    - |  688 | `"  if( $this instanceof SplStack ){ $this->__mode = 2; }"` |
|    - |  689 | `" }"` |
|    - |  690 | `" public function setIteratorMode($mode){"` |
|    - |  691 | `"  $mode = (int)$mode;"` |
|    - |  692 | `"  if( ($this instanceof SplStack \|\| $this instanceof SplQueue)"` |
|    - |  693 | `"   && ($mode & 2) !== ($this->__mode & 2) ){"` |
|    - |  694 | `"   throw new RuntimeException(\"Iterators' LIFO/FIFO modes for SplStack/SplQueue"` |
|    - |  695 | `" objects are frozen\");"` |
|    - |  696 | `"  }"` |
|    - |  697 | `"  $this->__mode = $mode;"` |
|    - |  698 | `" }"` |
|    - |  699 | `" public function getIteratorMode(){ return $this->__mode; }"` |
|    - |  700 | `" public function push($value){ $this->__q[] = $value; }"` |
|    - |  701 | `" public function pop(){"` |
|    - |  702 | `"  if( count($this->__q) === 0 ){"` |
|    - |  703 | `"   throw new RuntimeException(\"Can't pop from an empty datastructure\");"` |
|    - |  704 | `"  }"` |
|    - |  705 | `"  return array_pop($this->__q);"` |
|    - |  706 | `" }"` |
|    - |  707 | `" public function shift(){"` |
|    - |  708 | `"  if( count($this->__q) === 0 ){"` |
|    - |  709 | `"   throw new RuntimeException(\"Can't shift from an empty datastructure\");"` |
|    - |  710 | `"  }"` |
|    - |  711 | `"  return array_shift($this->__q);"` |
|    - |  712 | `" }"` |
|    - |  713 | `" public function unshift($value){ array_unshift($this->__q, $value); }"` |
|    - |  714 | `" public function top(){"` |
|    - |  715 | `"  if( count($this->__q) === 0 ){"` |
|    - |  716 | `"   throw new RuntimeException(\"Can't peek at an empty datastructure\");"` |
|    - |  717 | `"  }"` |
|    - |  718 | `"  return $this->__q[count($this->__q) - 1];"` |
|    - |  719 | `" }"` |
|    - |  720 | `" public function bottom(){"` |
|    - |  721 | `"  if( count($this->__q) === 0 ){"` |
|    - |  722 | `"   throw new RuntimeException(\"Can't peek at an empty datastructure\");"` |
|    - |  723 | `"  }"` |
|    - |  724 | `"  return $this->__q[0];"` |
|    - |  725 | `" }"` |
|    - |  726 | `" public function isEmpty(){ return count($this->__q) === 0; }"` |
|    - |  727 | `" public function count(){ return count($this->__q); }"` |
|    - |  728 | `" public function toArray(){ return $this->__q; }"` |
|    - |  729 | `" public function add($index, $value){"` |
|    - |  730 | `"  $index = (int)$index;"` |
|    - |  731 | `"  if( $index < 0 \|\| $index > count($this->__q) ){"` |
|    - |  732 | `"   throw new OutOfRangeException(get_class($this) === 'SplDoublyLinkedList'"` |
|    - |  733 | `"    ? 'SplDoublyLinkedList::add(): Argument #1 ($index) is out of range'"` |
|    - |  734 | `"    : get_class($this) . '::add(): Argument #1 ($index) is out of range');"` |
|    - |  735 | `"  }"` |
|    - |  736 | `"  array_splice($this->__q, $index, 0, [$value]);"` |
|    - |  737 | `" }"` |
|    - |  738 | `" public function offsetExists($index){"` |
|    - |  739 | `"  return is_int($index) \|\| ctype_digit((string)$index)"` |
|    - |  740 | `"   ? ((int)$index >= 0 && (int)$index < count($this->__q)) : false;"` |
|    - |  741 | `" }"` |
|    - |  742 | `" public function offsetGet($index){"` |
|    - |  743 | `"  $index = (int)$index;"` |
|    - |  744 | `"  if( $index < 0 \|\| $index >= count($this->__q) ){"` |
|    - |  745 | `"   throw new OutOfRangeException('SplDoublyLinkedList::offsetGet(): Argument #1"` |
|    - |  746 | `" ($index) is out of range');"` |
|    - |  747 | `"  }"` |
|    - |  748 | `"  return $this->__q[$index];"` |
|    - |  749 | `" }"` |
|    - |  750 | `" public function offsetSet($index, $value){"` |
|    - |  751 | `"  if( $index === null ){ $this->__q[] = $value; return; }"` |
|    - |  752 | `"  $index = (int)$index;"` |
|    - |  753 | `"  if( $index < 0 \|\| $index >= count($this->__q) ){"` |
|    - |  754 | `"   throw new OutOfRangeException('SplDoublyLinkedList::offsetSet(): Argument #1"` |
|    - |  755 | `" ($index) is out of range');"` |
|    - |  756 | `"  }"` |
|    - |  757 | `"  $this->__q[$index] = $value;"` |
|    - |  758 | `" }"` |
|    - |  759 | `" public function offsetUnset($index){"` |
|    - |  760 | `"  $index = (int)$index;"` |
|    - |  761 | `"  if( $index < 0 \|\| $index >= count($this->__q) ){"` |
|    - |  762 | `"   throw new OutOfRangeException('SplDoublyLinkedList::offsetUnset(): Argument #1"` |
|    - |  763 | `" ($index) is out of range');"` |
|    - |  764 | `"  }"` |
|    - |  765 | `"  array_splice($this->__q, $index, 1);"` |
|    - |  766 | `" }"` |
|    - |  767 | `" public function rewind(){"` |
|    - |  768 | `"  $this->__i = ($this->__mode & 2) ? count($this->__q) - 1 : 0;"` |
|    - |  769 | `" }"` |
|    - |  770 | `" public function valid(){"` |
|    - |  771 | `"  return $this->__i >= 0 && $this->__i < count($this->__q);"` |
|    - |  772 | `" }"` |
|    - |  773 | `" public function current(){ return $this->__q[$this->__i] ?? null; }"` |
|    - |  774 | `" public function key(){ return $this->__i; }"` |
|    - |  775 | `" public function next(){"` |
|    - |  776 | `"  if( $this->__mode & 1 ){"` |
|    - |  777 | `"   /* IT_MODE_DELETE consumes the element just visited */"` |
|    - |  778 | `"   if( $this->__mode & 2 ){ array_pop($this->__q); $this->__i = count($this->__q) - 1; }"` |
|    - |  779 | `"   else { array_shift($this->__q); }"` |
|    - |  780 | `"  }else{"` |
|    - |  781 | `"   $this->__i += ($this->__mode & 2) ? -1 : 1;"` |
|    - |  782 | `"  }"` |
|    - |  783 | `" }"` |
|    - |  784 | `" public function prev(){ $this->__i += ($this->__mode & 2) ? 1 : -1; }"` |
|    - |  785 | `"}"` |
|    - |  786 | `"class SplStack extends SplDoublyLinkedList {}"` |
|    - |  787 | `"class SplQueue extends SplDoublyLinkedList {"` |
|    - |  788 | `" public function enqueue($value){ $this->push($value); }"` |
|    - |  789 | `" public function dequeue(){ return $this->shift(); }"` |
|    - |  790 | `"}"` |
|    - |  791 | `"abstract class SplHeap implements Iterator, Countable {"` |
|    - |  792 | `" private $__h = [];"` |
|    - |  793 | `" abstract protected function compare($value1, $value2);"` |
|    - |  794 | `" private function __hSiftUp($i){"` |
|    - |  795 | `"  while( $i > 0 ){"` |
|    - |  796 | `"   $p = ($i - 1) >> 1;"` |
|    - |  797 | `"   if( $this->compare($this->__h[$i], $this->__h[$p]) <= 0 ){ break; }"` |
|    - |  798 | `"   $t = $this->__h[$p]; $this->__h[$p] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  799 | `"   $i = $p;"` |
|    - |  800 | `"  }"` |
|    - |  801 | `" }"` |
|    - |  802 | `" private function __hSiftDown($i){"` |
|    - |  803 | `"  $n = count($this->__h);"` |
|    - |  804 | `"  for(;;){"` |
|    - |  805 | `"   $l = 2 * $i + 1; $r = $l + 1; $b = $i;"` |
|    - |  806 | `"   if( $l < $n && $this->compare($this->__h[$l], $this->__h[$b]) > 0 ){ $b = $l; }"` |
|    - |  807 | `"   if( $r < $n && $this->compare($this->__h[$r], $this->__h[$b]) > 0 ){ $b = $r; }"` |
|    - |  808 | `"   if( $b === $i ){ break; }"` |
|    - |  809 | `"   $t = $this->__h[$b]; $this->__h[$b] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  810 | `"   $i = $b;"` |
|    - |  811 | `"  }"` |
|    - |  812 | `" }"` |
|    - |  813 | `" public function insert($value){"` |
|    - |  814 | `"  $this->__h[] = $value;"` |
|    - |  815 | `"  $this->__hSiftUp(count($this->__h) - 1);"` |
|    - |  816 | `"  return true;"` |
|    - |  817 | `" }"` |
|    - |  818 | `" public function extract(){"` |
|    - |  819 | `"  $n = count($this->__h);"` |
|    - |  820 | `"  if( $n === 0 ){"` |
|    - |  821 | `"   throw new RuntimeException(\"Can't extract from an empty heap\");"` |
|    - |  822 | `"  }"` |
|    - |  823 | `"  $top = $this->__h[0];"` |
|    - |  824 | `"  $last = array_pop($this->__h);"` |
|    - |  825 | `"  if( $n > 1 ){"` |
|    - |  826 | `"   $this->__h[0] = $last;"` |
|    - |  827 | `"   $this->__hSiftDown(0);"` |
|    - |  828 | `"  }"` |
|    - |  829 | `"  return $top;"` |
|    - |  830 | `" }"` |
|    - |  831 | `" public function top(){"` |
|    - |  832 | `"  if( count($this->__h) === 0 ){"` |
|    - |  833 | `"   throw new RuntimeException(\"Can't peek at an empty heap\");"` |
|    - |  834 | `"  }"` |
|    - |  835 | `"  return $this->__h[0];"` |
|    - |  836 | `" }"` |
|    - |  837 | `" public function isEmpty(){ return count($this->__h) === 0; }"` |
|    - |  838 | `" public function count(){ return count($this->__h); }"` |
|    - |  839 | `" public function isCorrupted(){ return false; }"` |
|    - |  840 | `" public function recoverFromCorruption(){ return true; }"` |
|    - |  841 | `" public function rewind(){}"` |
|    - |  842 | `" public function valid(){ return count($this->__h) > 0; }"` |
|    - |  843 | `" public function current(){ return count($this->__h) ? $this->__h[0] : null; }"` |
|    - |  844 | `" public function key(){ return count($this->__h) - 1; }"` |
|    - |  845 | `" public function next(){ if( count($this->__h) ){ $this->extract(); } }"` |
|    - |  846 | `"}"` |
|    - |  847 | `"class SplMinHeap extends SplHeap {"` |
|    - |  848 | `" protected function compare($value1, $value2){ return $value2 <=> $value1; }"` |
|    - |  849 | `"}"` |
|    - |  850 | `"class SplMaxHeap extends SplHeap {"` |
|    - |  851 | `" protected function compare($value1, $value2){ return $value1 <=> $value2; }"` |
|    - |  852 | `"}"` |
|    - |  853 | `"class SplPriorityQueue implements Iterator, Countable {"` |
|    - |  854 | `" const EXTR_DATA = 1;"` |
|    - |  855 | `" const EXTR_PRIORITY = 2;"` |
|    - |  856 | `" const EXTR_BOTH = 3;"` |
|    - |  857 | `" private $__h = [];"` |
|    - |  858 | `" private $__serial = PHP_INT_MAX;"` |
|    - |  859 | `" private $__flags = 1;"` |
|    - |  860 | `" public function compare($priority1, $priority2){ return $priority1 <=> $priority2; }"` |
|    - |  861 | `" private function __pqCmp($a, $b){"` |
|    - |  862 | `"  /* NO tie-break: php's heap swaps only on strictly-greater, which fixes"` |
|    - |  863 | `"   * the (documented-as-undefined) equal-priority order it exhibits */"` |
|    - |  864 | `"  return $this->compare($a[0], $b[0]);"` |
|    - |  865 | `" }"` |
|    - |  866 | `" private function __pqSiftUp($i){"` |
|    - |  867 | `"  while( $i > 0 ){"` |
|    - |  868 | `"   $p = ($i - 1) >> 1;"` |
|    - |  869 | `"   if( $this->__pqCmp($this->__h[$i], $this->__h[$p]) <= 0 ){ break; }"` |
|    - |  870 | `"   $t = $this->__h[$p]; $this->__h[$p] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  871 | `"   $i = $p;"` |
|    - |  872 | `"  }"` |
|    - |  873 | `" }"` |
|    - |  874 | `" private function __pqSiftDown($i){"` |
|    - |  875 | `"  $n = count($this->__h);"` |
|    - |  876 | `"  for(;;){"` |
|    - |  877 | `"   $l = 2 * $i + 1; $r = $l + 1; $b = $i;"` |
|    - |  878 | `"   if( $l < $n && $this->__pqCmp($this->__h[$l], $this->__h[$b]) > 0 ){ $b = $l; }"` |
|    - |  879 | `"   if( $r < $n && $this->__pqCmp($this->__h[$r], $this->__h[$b]) > 0 ){ $b = $r; }"` |
|    - |  880 | `"   if( $b === $i ){ break; }"` |
|    - |  881 | `"   $t = $this->__h[$b]; $this->__h[$b] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  882 | `"   $i = $b;"` |
|    - |  883 | `"  }"` |
|    - |  884 | `" }"` |
|    - |  885 | `" public function insert($value, $priority){"` |
|    - |  886 | `"  $this->__h[] = [$priority, $this->__serial--, $value];"` |
|    - |  887 | `"  $this->__pqSiftUp(count($this->__h) - 1);"` |
|    - |  888 | `"  return true;"` |
|    - |  889 | `" }"` |
|    - |  890 | `" private function __pqShape($node){"` |
|    - |  891 | `"  if( $this->__flags === self::EXTR_BOTH ){"` |
|    - |  892 | `"   return ['data' => $node[2], 'priority' => $node[0]];"` |
|    - |  893 | `"  }"` |
|    - |  894 | `"  if( $this->__flags === self::EXTR_PRIORITY ){ return $node[0]; }"` |
|    - |  895 | `"  return $node[2];"` |
|    - |  896 | `" }"` |
|    - |  897 | `" public function extract(){"` |
|    - |  898 | `"  $n = count($this->__h);"` |
|    - |  899 | `"  if( $n === 0 ){"` |
|    - |  900 | `"   throw new RuntimeException(\"Can't extract from an empty heap\");"` |
|    - |  901 | `"  }"` |
|    - |  902 | `"  $top = $this->__h[0];"` |
|    - |  903 | `"  $last = array_pop($this->__h);"` |
|    - |  904 | `"  if( $n > 1 ){"` |
|    - |  905 | `"   $this->__h[0] = $last;"` |
|    - |  906 | `"   $this->__pqSiftDown(0);"` |
|    - |  907 | `"  }"` |
|    - |  908 | `"  return $this->__pqShape($top);"` |
|    - |  909 | `" }"` |
|    - |  910 | `" public function top(){"` |
|    - |  911 | `"  if( count($this->__h) === 0 ){"` |
|    - |  912 | `"   throw new RuntimeException(\"Can't peek at an empty heap\");"` |
|    - |  913 | `"  }"` |
|    - |  914 | `"  return $this->__pqShape($this->__h[0]);"` |
|    - |  915 | `" }"` |
|    - |  916 | `" public function setExtractFlags($flags){ $this->__flags = (int)$flags; }"` |
|    - |  917 | `" public function getExtractFlags(){ return $this->__flags; }"` |
|    - |  918 | `" public function isEmpty(){ return count($this->__h) === 0; }"` |
|    - |  919 | `" public function count(){ return count($this->__h); }"` |
|    - |  920 | `" public function isCorrupted(){ return false; }"` |
|    - |  921 | `" public function recoverFromCorruption(){ return true; }"` |
|    - |  922 | `" public function rewind(){}"` |
|    - |  923 | `" public function valid(){ return count($this->__h) > 0; }"` |
|    - |  924 | `" public function current(){ return count($this->__h) ? $this->__pqShape($this->__h[0]) : null; }"` |
|    - |  925 | `" public function key(){ return count($this->__h) - 1; }"` |
|    - |  926 | `" public function next(){ if( count($this->__h) ){ $this->extract(); } }"` |
|    - |  927 | `"}"` |
|    - |  928 | `"class SplFixedArray implements ArrayAccess, Countable, IteratorAggregate, JsonSerializable {"` |
|    - |  929 | `" private $__a = [];"` |
|    - |  930 | `" private $__n = 0;"` |
|    - |  931 | `" public function __construct($size = 0){"` |
|    - |  932 | `"  $this->setSize((int)$size);"` |
|    - |  933 | `" }"` |
|    - |  934 | `" private function __faIdx($index, $method){"` |
|    - |  935 | `"  if( !is_int($index) ){"` |
|    - |  936 | `"   if( is_string($index) && ctype_digit($index) ){"` |
|    - |  937 | `"    $index = (int)$index;"` |
|    - |  938 | `"   }else{"` |
|    - |  939 | `"    throw new TypeError('Cannot access offset of type ' . get_debug_type($index)"` |
|    - |  940 | `"     . ' on SplFixedArray');"` |
|    - |  941 | `"   }"` |
|    - |  942 | `"  }"` |
|    - |  943 | `"  if( $index < 0 \|\| $index >= $this->__n ){"` |
|    - |  944 | `"   throw new OutOfBoundsException('Index invalid or out of range');"` |
|    - |  945 | `"  }"` |
|    - |  946 | `"  return $index;"` |
|    - |  947 | `" }"` |
|    - |  948 | `" public function offsetExists($index){"` |
|    - |  949 | `"  if( !is_int($index) && !(is_string($index) && ctype_digit($index)) ){ return false; }"` |
|    - |  950 | `"  $index = (int)$index;"` |
|    - |  951 | `"  return $index >= 0 && $index < $this->__n && $this->__a[$index] !== null;"` |
|    - |  952 | `" }"` |
|    - |  953 | `" public function offsetGet($index){ return $this->__a[$this->__faIdx($index, 'offsetGet')]; }"` |
|    - |  954 | `" public function offsetSet($index, $value){ $this->__a[$this->__faIdx($index, 'offsetSet')] = $value; }"` |
|    - |  955 | `" public function offsetUnset($index){ $this->__a[$this->__faIdx($index, 'offsetUnset')] = null; }"` |
|    - |  956 | `" public function getSize(){ return $this->__n; }"` |
|    - |  957 | `" public function setSize($size){"` |
|    - |  958 | `"  $size = (int)$size;"` |
|    - |  959 | `"  if( $size < 0 ){"` |
|    - |  960 | `"   throw new ValueError('SplFixedArray::setSize(): Argument #1 ($size) must be"` |
|    - |  961 | `" greater than or equal to 0');"` |
|    - |  962 | `"  }"` |
|    - |  963 | `"  if( $size < $this->__n ){"` |
|    - |  964 | `"   $this->__a = array_slice($this->__a, 0, $size);"` |
|    - |  965 | `"  }else{"` |
|    - |  966 | `"   for( $i = $this->__n; $i < $size; $i++ ){ $this->__a[$i] = null; }"` |
|    - |  967 | `"  }"` |
|    - |  968 | `"  $this->__n = $size;"` |
|    - |  969 | `"  return true;"` |
|    - |  970 | `" }"` |
|    - |  971 | `" public function count(){ return $this->__n; }"` |
|    - |  972 | `" public function toArray(){ return $this->__a; }"` |
|    - |  973 | `" public static function fromArray($array, $preserveKeys = true){"` |
|    - |  974 | `"  $f = new SplFixedArray(0);"` |
|    - |  975 | `"  if( $preserveKeys ){"` |
|    - |  976 | `"   $max = -1;"` |
|    - |  977 | `"   foreach( $array as $k => $v ){"` |
|    - |  978 | `"    if( !is_int($k) \|\| $k < 0 ){"` |
|    - |  979 | `"     throw new InvalidArgumentException('array must contain only positive integer keys');"` |
|    - |  980 | `"    }"` |
|    - |  981 | `"    if( $k > $max ){ $max = $k; }"` |
|    - |  982 | `"   }"` |
|    - |  983 | `"   $f->setSize($max + 1);"` |
|    - |  984 | `"   foreach( $array as $k => $v ){ $f[$k] = $v; }"` |
|    - |  985 | `"  }else{"` |
|    - |  986 | `"   $vals = array_values($array);"` |
|    - |  987 | `"   $f->setSize(count($vals));"` |
|    - |  988 | `"   foreach( $vals as $k => $v ){ $f[$k] = $v; }"` |
|    - |  989 | `"  }"` |
|    - |  990 | `"  return $f;"` |
|    - |  991 | `" }"` |
|    - |  992 | `" public function getIterator(): Generator {"` |
|    - |  993 | `"  for( $i = 0; $i < $this->__n; $i++ ){ yield $i => $this->__a[$i]; }"` |
|    - |  994 | `" }"` |
|    - |  995 | `" public function jsonSerialize(){ return $this->__a; }"` |
|    - |  996 | `"}"` |
|    - |  997 | `"class SplObjectStorage implements Countable, Iterator, ArrayAccess {"` |
|    - |  998 | `" private $__o = [];"` |
|    - |  999 | `" private $__i = 0;"` |
|    - | 1000 | `" public function attach($object, $info = null){"` |
|    - | 1001 | `"  __spl_deprecated('Method SplObjectStorage::attach() is deprecated since 8.5, use"` |
|    - | 1002 | `" method SplObjectStorage::offsetSet() instead');"` |
|    - | 1003 | `"  $this->offsetSet($object, $info);"` |
|    - | 1004 | `" }"` |
|    - | 1005 | `" public function detach($object){"` |
|    - | 1006 | `"  __spl_deprecated('Method SplObjectStorage::detach() is deprecated since 8.5, use"` |
|    - | 1007 | `" method SplObjectStorage::offsetUnset() instead');"` |
|    - | 1008 | `"  $this->offsetUnset($object);"` |
|    - | 1009 | `" }"` |
|    - | 1010 | `" public function contains($object){"` |
|    - | 1011 | `"  __spl_deprecated('Method SplObjectStorage::contains() is deprecated since 8.5, use"` |
|    - | 1012 | `" method SplObjectStorage::offsetExists() instead');"` |
|    - | 1013 | `"  return $this->offsetExists($object);"` |
|    - | 1014 | `" }"` |
|    - | 1015 | `" public function offsetSet($object, $info = null){"` |
|    - | 1016 | `"  $this->__o[spl_object_id($object)] = [$object, $info];"` |
|    - | 1017 | `" }"` |
|    - | 1018 | `" public function offsetExists($object){"` |
|    - | 1019 | `"  return isset($this->__o[spl_object_id($object)]);"` |
|    - | 1020 | `" }"` |
|    - | 1021 | `" public function offsetGet($object){"` |
|    - | 1022 | `"  $id = spl_object_id($object);"` |
|    - | 1023 | `"  if( !isset($this->__o[$id]) ){"` |
|    - | 1024 | `"   throw new UnexpectedValueException('Object not found');"` |
|    - | 1025 | `"  }"` |
|    - | 1026 | `"  return $this->__o[$id][1];"` |
|    - | 1027 | `" }"` |
|    - | 1028 | `" public function offsetUnset($object){"` |
|    - | 1029 | `"  unset($this->__o[spl_object_id($object)]);"` |
|    - | 1030 | `" }"` |
|    - | 1031 | `" public function addAll($storage){"` |
|    - | 1032 | `"  foreach( $storage as $obj ){"` |
|    - | 1033 | `"   $this->offsetSet($obj, $storage[$obj]);"` |
|    - | 1034 | `"  }"` |
|    - | 1035 | `"  return $this->count();"` |
|    - | 1036 | `" }"` |
|    - | 1037 | `" public function removeAll($storage){"` |
|    - | 1038 | `"  foreach( $storage as $obj ){ $this->offsetUnset($obj); }"` |
|    - | 1039 | `"  return $this->count();"` |
|    - | 1040 | `" }"` |
|    - | 1041 | `" public function removeAllExcept($storage){"` |
|    - | 1042 | `"  foreach( $this->__o as $id => $pair ){"` |
|    - | 1043 | `"   if( !$storage->offsetExists($pair[0]) ){ unset($this->__o[$id]); }"` |
|    - | 1044 | `"  }"` |
|    - | 1045 | `"  return $this->count();"` |
|    - | 1046 | `" }"` |
|    - | 1047 | `" public function getHash($object){ return spl_object_hash($object); }"` |
|    - | 1048 | `" public function count($mode = 0){ return count($this->__o); }"` |
|    - | 1049 | `" public function getInfo(){"` |
|    - | 1050 | `"  $pair = array_values($this->__o)[$this->__i] ?? null;"` |
|    - | 1051 | `"  return $pair === null ? null : $pair[1];"` |
|    - | 1052 | `" }"` |
|    - | 1053 | `" public function setInfo($info){"` |
|    - | 1054 | `"  $keys = array_keys($this->__o);"` |
|    - | 1055 | `"  if( isset($keys[$this->__i]) ){ $this->__o[$keys[$this->__i]][1] = $info; }"` |
|    - | 1056 | `" }"` |
|    - | 1057 | `" public function rewind(){ $this->__i = 0; }"` |
|    - | 1058 | `" public function valid(){ return $this->__i < count($this->__o); }"` |
|    - | 1059 | `" public function key(){ return $this->__i; }"` |
|    - | 1060 | `" public function current(){"` |
|    - | 1061 | `"  $pair = array_values($this->__o)[$this->__i] ?? null;"` |
|    - | 1062 | `"  return $pair === null ? null : $pair[0];"` |
|    - | 1063 | `" }"` |
|    - | 1064 | `" public function next(){ $this->__i++; }"` |
|    - | 1065 | `"}"` |
|    - | 1066 | `"interface SplObserver {"` |
|    - | 1067 | `" public function update(SplSubject $subject);"` |
|    - | 1068 | `"}"` |
|    - | 1069 | `"interface SplSubject {"` |
|    - | 1070 | `" public function attach(SplObserver $observer);"` |
|    - | 1071 | `" public function detach(SplObserver $observer);"` |
|    - | 1072 | `" public function notify();"` |
|    - | 1073 | `"}"` |
|    - | 1074 | `"class SplFileInfo implements Stringable {"` |
|    - | 1075 | `" protected $__pathName = '';"` |
|    - | 1076 | `" protected $__fileName = '';"` |
|    - | 1077 | `" public function __construct($path){"` |
|    - | 1078 | `"  $this->__pathName = (string)$path;"` |
|    - | 1079 | `"  $this->__fileName = basename($this->__pathName);"` |
|    - | 1080 | `" }"` |
|    - | 1081 | `" public function getPathname(){ return $this->__pathName; }"` |
|    - | 1082 | `" public function getFilename(){ return $this->__fileName; }"` |
|    - | 1083 | `" public function getPath(){ return dirname($this->__pathName); }"` |
|    - | 1084 | `" public function getBasename($suffix = ''){"` |
|    - | 1085 | `"  $b = basename($this->__pathName);"` |
|    - | 1086 | `"  if( $suffix !== '' && strlen($suffix) < strlen($b) && substr($b, -strlen($suffix)) === $suffix ){"` |
|    - | 1087 | `"   $b = substr($b, 0, -strlen($suffix));"` |
|    - | 1088 | `"  }"` |
|    - | 1089 | `"  return $b;"` |
|    - | 1090 | `" }"` |
|    - | 1091 | `" public function getExtension(){ return pathinfo($this->__pathName, PATHINFO_EXTENSION); }"` |
|    - | 1092 | `" public function getRealPath(){ return realpath($this->__pathName); }"` |
|    - | 1093 | `" public function isDir(){ return is_dir($this->__pathName); }"` |
|    - | 1094 | `" public function isFile(){ return is_file($this->__pathName); }"` |
|    - | 1095 | `" public function isLink(){ return is_link($this->__pathName); }"` |
|    - | 1096 | `" public function isReadable(){ return is_readable($this->__pathName); }"` |
|    - | 1097 | `" public function isWritable(){ return is_writable($this->__pathName); }"` |
|    - | 1098 | `" public function getSize(){ return filesize($this->__pathName); }"` |
|    - | 1099 | `" public function getMTime(){ return filemtime($this->__pathName); }"` |
|    - | 1100 | `" public function getATime(){ return fileatime($this->__pathName); }"` |
|    - | 1101 | `" public function getCTime(){ return filectime($this->__pathName); }"` |
|    - | 1102 | `" public function getType(){ return filetype($this->__pathName); }"` |
|    - | 1103 | `" public function getFileInfo(){ return new SplFileInfo($this->__pathName); }"` |
|    - | 1104 | `" public function getPathInfo(){ return new SplFileInfo(dirname($this->__pathName)); }"` |
|    - | 1105 | `" public function __toString(){ return $this->__pathName; }"` |
|    - | 1106 | `"}"` |
|    - | 1107 | `"class DirectoryIterator extends SplFileInfo implements SeekableIterator {"` |
|    - | 1108 | `" protected $__dir = '';"` |
|    - | 1109 | `" protected $__entries = array();"` |
|    - | 1110 | `" protected $__pos = 0;"` |
|    - | 1111 | `" public function __construct($path){"` |
|    - | 1112 | `"  $this->__dir = (string)$path;"` |
|    - | 1113 | `"  parent::__construct($this->__dir);"` |
|    - | 1114 | `"  $this->__load();"` |
|    - | 1115 | `" }"` |
|    - | 1116 | `" protected function __load(){"` |
|    - | 1117 | `"  $this->__entries = array();"` |
|    - | 1118 | `"  $h = @opendir($this->__dir);"` |
|    - | 1119 | `"  if( $h !== false ){"` |
|    - | 1120 | `"   while( ($e = readdir($h)) !== false ){ $this->__entries[] = $e; }"` |
|    - | 1121 | `"   closedir($h);"` |
|    - | 1122 | `"  }"` |
|    - | 1123 | `"  $this->__pos = 0;"` |
|    - | 1124 | `"  $this->__sync();"` |
|    - | 1125 | `" }"` |
|    - | 1126 | `" protected function __join($name){"` |
|    - | 1127 | `"  $d = $this->__dir;"` |
|    - | 1128 | `"  $last = substr($d, -1);"` |
|    - | 1129 | `"  $sep = ($last === '/' \|\| $last === '\\\\' \|\| $d === '') ? '' : '/';"` |
|    - | 1130 | `"  return $d . $sep . $name;"` |
|    - | 1131 | `" }"` |
|    - | 1132 | `" protected function __sync(){"` |
|    - | 1133 | `"  if( $this->__pos >= 0 && $this->__pos < count($this->__entries) ){"` |
|    - | 1134 | `"   $name = $this->__entries[$this->__pos];"` |
|    - | 1135 | `"   $this->__fileName = $name;"` |
|    - | 1136 | `"   $this->__pathName = $this->__join($name);"` |
|    - | 1137 | `"  }"` |
|    - | 1138 | `" }"` |
|    - | 1139 | `" public function isDot(){ $n = $this->__fileName; return $n === '.' \|\| $n === '..'; }"` |
|    - | 1140 | `" public function getFilename(){ return $this->__fileName; }"` |
|    - | 1141 | `" public function current(){ return $this; }"` |
|    - | 1142 | `" public function key(){ return $this->__pos; }"` |
|    - | 1143 | `" public function next(){ $this->__pos++; $this->__sync(); }"` |
|    - | 1144 | `" public function rewind(){ $this->__pos = 0; $this->__sync(); }"` |
|    - | 1145 | `" public function valid(){ return $this->__pos < count($this->__entries); }"` |
|    - | 1146 | `" public function seek($position){ $this->__pos = (int)$position; $this->__sync(); }"` |
|    - | 1147 | `" public function getFlags(){ return 0; }"` |
|    - | 1148 | `"}"` |
|    - | 1149 | `"class FilesystemIterator extends DirectoryIterator {"` |
|    - | 1150 | `" const CURRENT_AS_PATHNAME = 32;"` |
|    - | 1151 | `" const CURRENT_AS_FILEINFO = 0;"` |
|    - | 1152 | `" const CURRENT_AS_SELF = 16;"` |
|    - | 1153 | `" const CURRENT_MODE_MASK = 240;"` |
|    - | 1154 | `" const KEY_AS_PATHNAME = 0;"` |
|    - | 1155 | `" const KEY_AS_FILENAME = 256;"` |
|    - | 1156 | `" const FOLLOW_SYMLINKS = 512;"` |
|    - | 1157 | `" const KEY_MODE_MASK = 3840;"` |
|    - | 1158 | `" const NEW_CURRENT_AND_KEY = 256;"` |
|    - | 1159 | `" const OTHER_MODE_MASK = 12288;"` |
|    - | 1160 | `" const SKIP_DOTS = 4096;"` |
|    - | 1161 | `" const UNIX_PATHS = 8192;"` |
|    - | 1162 | `" protected $__flags = 4096;"` |
|    - | 1163 | `" public function __construct($path, $flags = 4096){"` |
|    - | 1164 | `"  $this->__flags = (int)$flags;"` |
|    - | 1165 | `"  parent::__construct($path);"` |
|    - | 1166 | `" }"` |
|    - | 1167 | `" protected function __skipDots(){"` |
|    - | 1168 | `"  if( $this->__flags & self::SKIP_DOTS ){"` |
|    - | 1169 | `"   while( ($this->__pos < count($this->__entries)) && $this->isDot() ){ $this->__pos++; $this->__sync(); }"` |
|    - | 1170 | `"  }"` |
|    - | 1171 | `" }"` |
|    - | 1172 | `" public function rewind(){ $this->__pos = 0; $this->__sync(); $this->__skipDots(); }"` |
|    - | 1173 | `" public function next(){ $this->__pos++; $this->__sync(); $this->__skipDots(); }"` |
|    - | 1174 | `" public function current(){"` |
|    - | 1175 | `"  $mode = $this->__flags & self::CURRENT_MODE_MASK;"` |
|    - | 1176 | `"  if( $mode === self::CURRENT_AS_PATHNAME ){ return $this->getPathname(); }"` |
|    - | 1177 | `"  if( $mode === self::CURRENT_AS_SELF ){ return $this; }"` |
|    - | 1178 | `"  return new SplFileInfo($this->getPathname());"` |
|    - | 1179 | `" }"` |
|    - | 1180 | `" public function key(){"` |
|    - | 1181 | `"  if( $this->__flags & self::KEY_AS_FILENAME ){ return $this->getFilename(); }"` |
|    - | 1182 | `"  return $this->getPathname();"` |
|    - | 1183 | `" }"` |
|    - | 1184 | `" public function getFlags(){ return $this->__flags; }"` |
|    - | 1185 | `" public function setFlags($flags){ $this->__flags = (int)$flags; }"` |
|    - | 1186 | `"}"` |
|    - | 1187 | `"class RecursiveDirectoryIterator extends FilesystemIterator implements RecursiveIterator {"` |
|    - | 1188 | `" public function hasChildren(){"` |
|    - | 1189 | `"  if( $this->isDot() ){ return false; }"` |
|    - | 1190 | `"  return $this->isDir();"` |
|    - | 1191 | `" }"` |
|    - | 1192 | `" public function getChildren(){"` |
|    - | 1193 | `"  return new RecursiveDirectoryIterator($this->getPathname(), $this->__flags);"` |
|    - | 1194 | `" }"` |
|    - | 1195 | `" public function getSubPath(){ return ''; }"` |
|    - | 1196 | `" public function getSubPathname(){ return $this->getFilename(); }"` |
|    - | 1197 | `"}"` |
|    - | 1198 | `;` |
|    - | 1199 |  |
| 3874 | 1200 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm)` |
|    5 | 1201 | `{` |
| 3879 | 1202 | `	ph7_create_function(&(*pVm),"__spl_deprecated",vm_builtin_spl_deprecated,0);` |
| 3879 | 1203 | `	ph7_create_function(&(*pVm),"__weak_create",vm_builtin_weak_create,0);` |
| 3879 | 1204 | `	ph7_create_function(&(*pVm),"__weak_get",vm_builtin_weak_get,0);` |
| 3879 | 1205 | `	ph7_create_function(&(*pVm),"__weak_drop",vm_builtin_weak_drop,0);` |
| 3879 | 1206 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zSplLib,sizeof(zSplLib)-1);` |
|    5 | 1207 | `}` |
|    - | 1208 |  |
|    - | 1209 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1210 |  |
|    - | 1211 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|    - | 1212 | `/* Tiny build: no SPL (builtin layer disabled) */` |
|    - | 1213 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|    - | 1214 | `#endif` |
|    - | 1215 |  |
