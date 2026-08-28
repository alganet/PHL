# src/ph7/vm_builtin_spl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5/13 lines (38.46%)

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
|    - |   30 | `static const char zSplLib[] =` |
|    - |   31 | `"interface SeekableIterator extends Iterator {"` |
|    - |   32 | `" public function seek($offset);"` |
|    - |   33 | `"}"` |
|    - |   34 | `"trait __SplStoreT {"` |
|    - |   35 | `" private $__d = [];"` |
|    - |   36 | `" private $__f = 0;"` |
|    - |   37 | `" private function __splInitStore($array, $flags, $ctor){"` |
|    - |   38 | `"  if( is_array($array) ){"` |
|    - |   39 | `"   $this->__d = $array;"` |
|    - |   40 | `"  }elseif( is_object($array) ){"` |
|    - |   41 | `"   __spl_deprecated(get_class($this) . '::' . $ctor . '(): Using an object as a"` |
|    - |   42 | `" backing array for ' . get_class($this) . ' is deprecated, as it allows violating"` |
|    - |   43 | `" class constraints and invariants');"` |
|    - |   44 | `"  $this->__d = get_object_vars($array);"` |
|    - |   45 | `"  }else{"` |
|    - |   46 | `"   throw new TypeError(get_class($this) . '::' . $ctor . '(): Argument #1 ($array)"` |
|    - |   47 | `" must be of type array, ' . get_debug_type($array) . ' given');"` |
|    - |   48 | `"  }"` |
|    - |   49 | `"  $this->__f = (int)$flags;"` |
|    - |   50 | `" }"` |
|    - |   51 | `" public function offsetExists($key){ return array_key_exists($key, $this->__d); }"` |
|    - |   52 | `" public function offsetGet($key){ return $this->__d[$key]; }"` |
|    - |   53 | `" public function offsetSet($key, $value){"` |
|    - |   54 | `"  if( $key === null ){ $this->__d[] = $value; }"` |
|    - |   55 | `"  else { $this->__d[$key] = $value; }"` |
|    - |   56 | `" }"` |
|    - |   57 | `" public function offsetUnset($key){ unset($this->__d[$key]); }"` |
|    - |   58 | `" public function append($value){ $this->__d[] = $value; }"` |
|    - |   59 | `" public function getArrayCopy(){ return $this->__d; }"` |
|    - |   60 | `" public function count(){ return count($this->__d); }"` |
|    - |   61 | `" public function getFlags(){ return $this->__f; }"` |
|    - |   62 | `" public function setFlags($flags){ $this->__f = (int)$flags; }"` |
|    - |   63 | `" public function asort($flags = 0){ asort($this->__d); return true; }"` |
|    - |   64 | `" public function ksort($flags = 0){ ksort($this->__d); return true; }"` |
|    - |   65 | `" public function uasort($callback){ uasort($this->__d, $callback); return true; }"` |
|    - |   66 | `" public function uksort($callback){ uksort($this->__d, $callback); return true; }"` |
|    - |   67 | `" public function natsort(){ uasort($this->__d, 'strnatcmp'); return true; }"` |
|    - |   68 | `" public function natcasesort(){ uasort($this->__d, 'strnatcasecmp'); return true; }"` |
|    - |   69 | `"}"` |
|    - |   70 | `"class ArrayIterator implements SeekableIterator, ArrayAccess, Countable {"` |
|    - |   71 | `" use __SplStoreT;"` |
|    - |   72 | `" const STD_PROP_LIST = 1;"` |
|    - |   73 | `" const ARRAY_AS_PROPS = 2;"` |
|    - |   74 | `" public function __construct($array = [], $flags = 0){"` |
|    - |   75 | `"  $this->__splInitStore($array, $flags, '__construct');"` |
|    - |   76 | `"  reset($this->__d);"` |
|    - |   77 | `" }"` |
|    - |   78 | `" public function current(){"` |
|    - |   79 | `"  if( key($this->__d) === null ){ return null; }"` |
|    - |   80 | `"  return current($this->__d);"` |
|    - |   81 | `" }"` |
|    - |   82 | `" public function key(){ return key($this->__d); }"` |
|    - |   83 | `" public function next(){ next($this->__d); }"` |
|    - |   84 | `" public function rewind(){ reset($this->__d); }"` |
|    - |   85 | `" public function valid(){ return key($this->__d) !== null; }"` |
|    - |   86 | `" public function seek($offset){"` |
|    - |   87 | `"  $offset = (int)$offset;"` |
|    - |   88 | `"  if( $offset < 0 \|\| $offset >= count($this->__d) ){"` |
|    - |   89 | `"   throw new OutOfBoundsException('Seek position ' . $offset . ' is out of range');"` |
|    - |   90 | `"  }"` |
|    - |   91 | `"  reset($this->__d);"` |
|    - |   92 | `"  for( $i = 0; $i < $offset; $i++ ){ next($this->__d); }"` |
|    - |   93 | `" }"` |
|    - |   94 | `"}"` |
|    - |   95 | `"class ArrayObject implements IteratorAggregate, ArrayAccess, Countable {"` |
|    - |   96 | `" use __SplStoreT;"` |
|    - |   97 | `" const STD_PROP_LIST = 1;"` |
|    - |   98 | `" const ARRAY_AS_PROPS = 2;"` |
|    - |   99 | `" private $__it = 'ArrayIterator';"` |
|    - |  100 | `" public function __construct($array = [], $flags = 0, $iteratorClass = 'ArrayIterator'){"` |
|    - |  101 | `"  $this->__splInitStore($array, $flags, '__construct');"` |
|    - |  102 | `"  if( $iteratorClass !== 'ArrayIterator' ){ $this->setIteratorClass($iteratorClass); }"` |
|    - |  103 | `" }"` |
|    - |  104 | `" public function getIterator(){"` |
|    - |  105 | `"  $c = $this->__it;"` |
|    - |  106 | `"  return new $c($this->__d);"` |
|    - |  107 | `" }"` |
|    - |  108 | `" public function exchangeArray($array){"` |
|    - |  109 | `"  $old = $this->__d;"` |
|    - |  110 | `"  $this->__splInitStore($array, $this->__f, 'exchangeArray');"` |
|    - |  111 | `"  return $old;"` |
|    - |  112 | `" }"` |
|    - |  113 | `" public function setIteratorClass($iteratorClass){"` |
|    - |  114 | `"  $c = (string)$iteratorClass;"` |
|    - |  115 | `"  if( $c !== 'ArrayIterator'"` |
|    - |  116 | `"   && (!class_exists($c) \|\| !is_subclass_of($c, 'ArrayIterator')) ){"` |
|    - |  117 | `"   throw new TypeError('ArrayObject::setIteratorClass(): Argument #1"` |
|    - |  118 | `" ($iteratorClass) must be a class name derived from ArrayIterator, ' . $c . ' given');"` |
|    - |  119 | `"  }"` |
|    - |  120 | `"  $this->__it = $c;"` |
|    - |  121 | `" }"` |
|    - |  122 | `" public function getIteratorClass(){ return $this->__it; }"` |
|    - |  123 | `" public function __get($name){"` |
|    - |  124 | `"  if( $this->__f & 2 ){ return $this->__d[$name]; }"` |
|    - |  125 | `"  return null;"` |
|    - |  126 | `" }"` |
|    - |  127 | `" public function __set($name, $value){"` |
|    - |  128 | `"  if( $this->__f & 2 ){ $this->__d[$name] = $value; return; }"` |
|    - |  129 | `"  $this->{$name} = $value;"` |
|    - |  130 | `" }"` |
|    - |  131 | `" public function __isset($name){"` |
|    - |  132 | `"  if( $this->__f & 2 ){ return isset($this->__d[$name]); }"` |
|    - |  133 | `"  return false;"` |
|    - |  134 | `" }"` |
|    - |  135 | `" public function __unset($name){"` |
|    - |  136 | `"  if( $this->__f & 2 ){ unset($this->__d[$name]); }"` |
|    - |  137 | `" }"` |
|    - |  138 | `"}"` |
|    - |  139 | `"function natsort(&$array){ return uasort($array, 'strnatcmp'); }"` |
|    - |  140 | `"function natcasesort(&$array){ return uasort($array, 'strnatcasecmp'); }"` |
|    - |  141 | `"interface OuterIterator extends Iterator {"` |
|    - |  142 | `" public function getInnerIterator();"` |
|    - |  143 | `"}"` |
|    - |  144 | `"class IteratorIterator implements OuterIterator {"` |
|    - |  145 | `" private $__in = null;"` |
|    - |  146 | `" public function __construct($iterator, $class = null){"` |
|    - |  147 | `"  while( $iterator instanceof IteratorAggregate ){ $iterator = $iterator->getIterator(); }"` |
|    - |  148 | `"  if( !($iterator instanceof Iterator) ){"` |
|    - |  149 | `"   throw new TypeError(get_class($this) . '::__construct(): Argument #1 ($iterator)"` |
|    - |  150 | `" must be of type Traversable, ' . get_debug_type($iterator) . ' given');"` |
|    - |  151 | `"  }"` |
|    - |  152 | `"  $this->__in = $iterator;"` |
|    - |  153 | `" }"` |
|    - |  154 | `" public function getInnerIterator(){ return $this->__in; }"` |
|    - |  155 | `" public function current(){ return $this->__in->current(); }"` |
|    - |  156 | `" public function key(){ return $this->__in->key(); }"` |
|    - |  157 | `" public function next(){ $this->__in->next(); }"` |
|    - |  158 | `" public function rewind(){ $this->__in->rewind(); }"` |
|    - |  159 | `" public function valid(){ return $this->__in->valid(); }"` |
|    - |  160 | `"}"` |
|    - |  161 | `"class LimitIterator extends IteratorIterator {"` |
|    - |  162 | `" private $__off = 0;"` |
|    - |  163 | `" private $__lim = -1;"` |
|    - |  164 | `" private $__pos = 0;"` |
|    - |  165 | `" public function __construct($iterator, $offset = 0, $limit = -1){"` |
|    - |  166 | `"  $offset = (int)$offset; $limit = (int)$limit;"` |
|    - |  167 | `"  if( $offset < 0 ){"` |
|    - |  168 | `"   throw new ValueError('LimitIterator::__construct(): Argument #2 ($offset) must be"` |
|    - |  169 | `" greater than or equal to 0');"` |
|    - |  170 | `"  }"` |
|    - |  171 | `"  if( $limit < -1 ){"` |
|    - |  172 | `"   throw new ValueError('LimitIterator::__construct(): Argument #3 ($limit) must be"` |
|    - |  173 | `" greater than or equal to -1');"` |
|    - |  174 | `"  }"` |
|    - |  175 | `"  parent::__construct($iterator);"` |
|    - |  176 | `"  $this->__off = $offset;"` |
|    - |  177 | `"  $this->__lim = $limit;"` |
|    - |  178 | `" }"` |
|    - |  179 | `" public function rewind(){"` |
|    - |  180 | `"  $in = $this->getInnerIterator();"` |
|    - |  181 | `"  $in->rewind();"` |
|    - |  182 | `"  for( $i = 0; $i < $this->__off && $in->valid(); $i++ ){ $in->next(); }"` |
|    - |  183 | `"  $this->__pos = $this->__off;"` |
|    - |  184 | `" }"` |
|    - |  185 | `" public function valid(){"` |
|    - |  186 | `"  if( $this->__lim != -1 && $this->__pos >= $this->__off + $this->__lim ){ return false; }"` |
|    - |  187 | `"  return $this->getInnerIterator()->valid();"` |
|    - |  188 | `" }"` |
|    - |  189 | `" public function next(){ $this->__pos++; $this->getInnerIterator()->next(); }"` |
|    - |  190 | `" public function getPosition(){ return $this->__pos; }"` |
|    - |  191 | `" public function seek($offset){"` |
|    - |  192 | `"  $offset = (int)$offset;"` |
|    - |  193 | `"  if( $offset < $this->__off ){"` |
|    - |  194 | `"   throw new OutOfBoundsException('Cannot seek to ' . $offset . ' which is below the"` |
|    - |  195 | `" offset ' . $this->__off);"` |
|    - |  196 | `"  }"` |
|    - |  197 | `"  if( $this->__lim != -1 && $offset >= $this->__off + $this->__lim ){"` |
|    - |  198 | `"   throw new OutOfBoundsException('Cannot seek to ' . $offset . ' which is behind or"` |
|    - |  199 | `" equal to the limit ' . $this->__lim . ' plus the offset ' . $this->__off);"` |
|    - |  200 | `"  }"` |
|    - |  201 | `"  $in = $this->getInnerIterator();"` |
|    - |  202 | `"  $in->rewind();"` |
|    - |  203 | `"  for( $i = 0; $i < $offset && $in->valid(); $i++ ){ $in->next(); }"` |
|    - |  204 | `"  $this->__pos = $offset;"` |
|    - |  205 | `"  return $this->__pos;"` |
|    - |  206 | `" }"` |
|    - |  207 | `"}"` |
|    - |  208 | `"abstract class FilterIterator extends IteratorIterator {"` |
|    - |  209 | `" abstract public function accept();"` |
|    - |  210 | `" private function __fiFetch(){"` |
|    - |  211 | `"  $in = $this->getInnerIterator();"` |
|    - |  212 | `"  while( $in->valid() && !$this->accept() ){ $in->next(); }"` |
|    - |  213 | `" }"` |
|    - |  214 | `" public function rewind(){ $this->getInnerIterator()->rewind(); $this->__fiFetch(); }"` |
|    - |  215 | `" public function next(){ $this->getInnerIterator()->next(); $this->__fiFetch(); }"` |
|    - |  216 | `"}"` |
|    - |  217 | `"class CallbackFilterIterator extends FilterIterator {"` |
|    - |  218 | `" private $__cb = null;"` |
|    - |  219 | `" public function __construct($iterator, $callback){"` |
|    - |  220 | `"  parent::__construct($iterator);"` |
|    - |  221 | `"  $this->__cb = $callback;"` |
|    - |  222 | `" }"` |
|    - |  223 | `" public function accept(){"` |
|    - |  224 | `"  $in = $this->getInnerIterator();"` |
|    - |  225 | `"  return (bool)call_user_func($this->__cb, $in->current(), $in->key(), $in);"` |
|    - |  226 | `" }"` |
|    - |  227 | `"}"` |
|    - |  228 | `"class RegexIterator extends FilterIterator {"` |
|    - |  229 | `" const USE_KEY = 1;"` |
|    - |  230 | `" const INVERT_MATCH = 2;"` |
|    - |  231 | `" const MATCH = 0;"` |
|    - |  232 | `" const GET_MATCH = 1;"` |
|    - |  233 | `" const ALL_MATCHES = 2;"` |
|    - |  234 | `" const SPLIT = 3;"` |
|    - |  235 | `" const REPLACE = 4;"` |
|    - |  236 | `" public $replacement = null;"` |
|    - |  237 | `" private $__re = '';"` |
|    - |  238 | `" private $__mode = 0;"` |
|    - |  239 | `" private $__rflags = 0;"` |
|    - |  240 | `" private $__pflags = 0;"` |
|    - |  241 | `" private $__cur = null;"` |
|    - |  242 | `" public function __construct($iterator, $pattern, $mode = 0, $flags = 0, $pregFlags = 0){"` |
|    - |  243 | `"  parent::__construct($iterator);"` |
|    - |  244 | `"  $this->__re = (string)$pattern;"` |
|    - |  245 | `"  $this->__mode = (int)$mode;"` |
|    - |  246 | `"  $this->__rflags = (int)$flags;"` |
|    - |  247 | `"  $this->__pflags = (int)$pregFlags;"` |
|    - |  248 | `" }"` |
|    - |  249 | `" public function accept(){"` |
|    - |  250 | `"  $in = $this->getInnerIterator();"` |
|    - |  251 | `"  if( !$in->valid() ){ return false; }"` |
|    - |  252 | `"  $subject = ($this->__rflags & self::USE_KEY) ? $in->key() : $in->current();"` |
|    - |  253 | `"  $subject = (string)$subject;"` |
|    - |  254 | `"  $this->__cur = null;"` |
|    - |  255 | `"  $ok = false;"` |
|    - |  256 | `"  if( $this->__mode === self::MATCH ){"` |
|    - |  257 | `"   $ok = preg_match($this->__re, $subject) > 0;"` |
|    - |  258 | `"  }elseif( $this->__mode === self::GET_MATCH ){"` |
|    - |  259 | `"   $m = null;"` |
|    - |  260 | `"   $ok = preg_match($this->__re, $subject, $m, $this->__pflags) > 0;"` |
|    - |  261 | `"   $this->__cur = $m;"` |
|    - |  262 | `"  }elseif( $this->__mode === self::ALL_MATCHES ){"` |
|    - |  263 | `"   $m = null;"` |
|    - |  264 | `"   $ok = preg_match_all($this->__re, $subject, $m, $this->__pflags) > 0;"` |
|    - |  265 | `"   $this->__cur = $m;"` |
|    - |  266 | `"  }elseif( $this->__mode === self::SPLIT ){"` |
|    - |  267 | `"   $this->__cur = preg_split($this->__re, $subject, -1, $this->__pflags);"` |
|    - |  268 | `"   $ok = is_array($this->__cur) && count($this->__cur) > 1;"` |
|    - |  269 | `"  }elseif( $this->__mode === self::REPLACE ){"` |
|    - |  270 | `"   $n = 0;"` |
|    - |  271 | `"   $this->__cur = preg_replace($this->__re, (string)$this->replacement, $subject, -1, $n);"` |
|    - |  272 | `"   $ok = $n > 0;"` |
|    - |  273 | `"  }"` |
|    - |  274 | `"  if( $this->__rflags & self::INVERT_MATCH ){ $ok = !$ok; }"` |
|    - |  275 | `"  return $ok;"` |
|    - |  276 | `" }"` |
|    - |  277 | `" public function current(){"` |
|    - |  278 | `"  if( $this->__mode === self::MATCH ){ return $this->getInnerIterator()->current(); }"` |
|    - |  279 | `"  return $this->__cur;"` |
|    - |  280 | `" }"` |
|    - |  281 | `" public function getRegex(){ return $this->__re; }"` |
|    - |  282 | `" public function getMode(){ return $this->__mode; }"` |
|    - |  283 | `" public function setMode($mode){ $this->__mode = (int)$mode; }"` |
|    - |  284 | `" public function getFlags(){ return $this->__rflags; }"` |
|    - |  285 | `" public function setFlags($flags){ $this->__rflags = (int)$flags; }"` |
|    - |  286 | `" public function getPregFlags(){ return $this->__pflags; }"` |
|    - |  287 | `" public function setPregFlags($pregFlags){ $this->__pflags = (int)$pregFlags; }"` |
|    - |  288 | `"}"` |
|    - |  289 | `"class AppendIterator implements OuterIterator {"` |
|    - |  290 | `" private $__its = [];"` |
|    - |  291 | `" private $__idx = 0;"` |
|    - |  292 | `" public function __construct(){}"` |
|    - |  293 | `" public function append($iterator){"` |
|    - |  294 | `"  $this->__its[] = $iterator;"` |
|    - |  295 | `"  if( count($this->__its) === 1 ){ $iterator->rewind(); }"` |
|    - |  296 | `" }"` |
|    - |  297 | `" public function getInnerIterator(){ return $this->__its[$this->__idx] ?? null; }"` |
|    - |  298 | `" public function getIteratorIndex(){"` |
|    - |  299 | `"  return isset($this->__its[$this->__idx]) ? $this->__idx : null;"` |
|    - |  300 | `" }"` |
|    - |  301 | `" public function getArrayIterator(){ return new ArrayIterator($this->__its); }"` |
|    - |  302 | `" private function __apAdvance(){"` |
|    - |  303 | `"  while( isset($this->__its[$this->__idx])"` |
|    - |  304 | `"   && !$this->__its[$this->__idx]->valid()"` |
|    - |  305 | `"   && isset($this->__its[$this->__idx + 1]) ){"` |
|    - |  306 | `"   $this->__idx++;"` |
|    - |  307 | `"   $this->__its[$this->__idx]->rewind();"` |
|    - |  308 | `"  }"` |
|    - |  309 | `" }"` |
|    - |  310 | `" public function rewind(){"` |
|    - |  311 | `"  $this->__idx = 0;"` |
|    - |  312 | `"  if( isset($this->__its[0]) ){ $this->__its[0]->rewind(); }"` |
|    - |  313 | `"  $this->__apAdvance();"` |
|    - |  314 | `" }"` |
|    - |  315 | `" public function valid(){"` |
|    - |  316 | `"  $in = $this->getInnerIterator();"` |
|    - |  317 | `"  return $in !== null && $in->valid();"` |
|    - |  318 | `" }"` |
|    - |  319 | `" public function current(){ $in = $this->getInnerIterator(); return $in ? $in->current() : null; }"` |
|    - |  320 | `" public function key(){ $in = $this->getInnerIterator(); return $in ? $in->key() : null; }"` |
|    - |  321 | `" public function next(){"` |
|    - |  322 | `"  $in = $this->getInnerIterator();"` |
|    - |  323 | `"  if( $in ){ $in->next(); }"` |
|    - |  324 | `"  $this->__apAdvance();"` |
|    - |  325 | `" }"` |
|    - |  326 | `"}"` |
|    - |  327 | `"class InfiniteIterator extends IteratorIterator {"` |
|    - |  328 | `" public function next(){"` |
|    - |  329 | `"  $in = $this->getInnerIterator();"` |
|    - |  330 | `"  $in->next();"` |
|    - |  331 | `"  if( !$in->valid() ){ $in->rewind(); }"` |
|    - |  332 | `" }"` |
|    - |  333 | `"}"` |
|    - |  334 | `"class NoRewindIterator extends IteratorIterator {"` |
|    - |  335 | `" public function rewind(){}"` |
|    - |  336 | `"}"` |
|    - |  337 | `"class EmptyIterator implements Iterator {"` |
|    - |  338 | `" public function current(){"` |
|    - |  339 | `"  throw new BadMethodCallException('Accessing the value of an EmptyIterator');"` |
|    - |  340 | `" }"` |
|    - |  341 | `" public function key(){"` |
|    - |  342 | `"  throw new BadMethodCallException('Accessing the key of an EmptyIterator');"` |
|    - |  343 | `" }"` |
|    - |  344 | `" public function next(){}"` |
|    - |  345 | `" public function rewind(){}"` |
|    - |  346 | `" public function valid(){ return false; }"` |
|    - |  347 | `"}"` |
|    - |  348 | `"class SplDoublyLinkedList implements Iterator, Countable, ArrayAccess {"` |
|    - |  349 | `" const IT_MODE_LIFO = 2;"` |
|    - |  350 | `" const IT_MODE_FIFO = 0;"` |
|    - |  351 | `" const IT_MODE_DELETE = 1;"` |
|    - |  352 | `" const IT_MODE_KEEP = 0;"` |
|    - |  353 | `" private $__q = [];"` |
|    - |  354 | `" private $__mode = 0;"` |
|    - |  355 | `" private $__i = 0;"` |
|    - |  356 | `" public function __construct(){"` |
|    - |  357 | `"  if( $this instanceof SplStack ){ $this->__mode = 2; }"` |
|    - |  358 | `" }"` |
|    - |  359 | `" public function setIteratorMode($mode){"` |
|    - |  360 | `"  $mode = (int)$mode;"` |
|    - |  361 | `"  if( ($this instanceof SplStack \|\| $this instanceof SplQueue)"` |
|    - |  362 | `"   && ($mode & 2) !== ($this->__mode & 2) ){"` |
|    - |  363 | `"   throw new RuntimeException(\"Iterators' LIFO/FIFO modes for SplStack/SplQueue"` |
|    - |  364 | `" objects are frozen\");"` |
|    - |  365 | `"  }"` |
|    - |  366 | `"  $this->__mode = $mode;"` |
|    - |  367 | `" }"` |
|    - |  368 | `" public function getIteratorMode(){ return $this->__mode; }"` |
|    - |  369 | `" public function push($value){ $this->__q[] = $value; }"` |
|    - |  370 | `" public function pop(){"` |
|    - |  371 | `"  if( count($this->__q) === 0 ){"` |
|    - |  372 | `"   throw new RuntimeException(\"Can't pop from an empty datastructure\");"` |
|    - |  373 | `"  }"` |
|    - |  374 | `"  return array_pop($this->__q);"` |
|    - |  375 | `" }"` |
|    - |  376 | `" public function shift(){"` |
|    - |  377 | `"  if( count($this->__q) === 0 ){"` |
|    - |  378 | `"   throw new RuntimeException(\"Can't shift from an empty datastructure\");"` |
|    - |  379 | `"  }"` |
|    - |  380 | `"  return array_shift($this->__q);"` |
|    - |  381 | `" }"` |
|    - |  382 | `" public function unshift($value){ array_unshift($this->__q, $value); }"` |
|    - |  383 | `" public function top(){"` |
|    - |  384 | `"  if( count($this->__q) === 0 ){"` |
|    - |  385 | `"   throw new RuntimeException(\"Can't peek at an empty datastructure\");"` |
|    - |  386 | `"  }"` |
|    - |  387 | `"  return $this->__q[count($this->__q) - 1];"` |
|    - |  388 | `" }"` |
|    - |  389 | `" public function bottom(){"` |
|    - |  390 | `"  if( count($this->__q) === 0 ){"` |
|    - |  391 | `"   throw new RuntimeException(\"Can't peek at an empty datastructure\");"` |
|    - |  392 | `"  }"` |
|    - |  393 | `"  return $this->__q[0];"` |
|    - |  394 | `" }"` |
|    - |  395 | `" public function isEmpty(){ return count($this->__q) === 0; }"` |
|    - |  396 | `" public function count(){ return count($this->__q); }"` |
|    - |  397 | `" public function toArray(){ return $this->__q; }"` |
|    - |  398 | `" public function add($index, $value){"` |
|    - |  399 | `"  $index = (int)$index;"` |
|    - |  400 | `"  if( $index < 0 \|\| $index > count($this->__q) ){"` |
|    - |  401 | `"   throw new OutOfRangeException(get_class($this) === 'SplDoublyLinkedList'"` |
|    - |  402 | `"    ? 'SplDoublyLinkedList::add(): Argument #1 ($index) is out of range'"` |
|    - |  403 | `"    : get_class($this) . '::add(): Argument #1 ($index) is out of range');"` |
|    - |  404 | `"  }"` |
|    - |  405 | `"  array_splice($this->__q, $index, 0, [$value]);"` |
|    - |  406 | `" }"` |
|    - |  407 | `" public function offsetExists($index){"` |
|    - |  408 | `"  return is_int($index) \|\| ctype_digit((string)$index)"` |
|    - |  409 | `"   ? ((int)$index >= 0 && (int)$index < count($this->__q)) : false;"` |
|    - |  410 | `" }"` |
|    - |  411 | `" public function offsetGet($index){"` |
|    - |  412 | `"  $index = (int)$index;"` |
|    - |  413 | `"  if( $index < 0 \|\| $index >= count($this->__q) ){"` |
|    - |  414 | `"   throw new OutOfRangeException('SplDoublyLinkedList::offsetGet(): Argument #1"` |
|    - |  415 | `" ($index) is out of range');"` |
|    - |  416 | `"  }"` |
|    - |  417 | `"  return $this->__q[$index];"` |
|    - |  418 | `" }"` |
|    - |  419 | `" public function offsetSet($index, $value){"` |
|    - |  420 | `"  if( $index === null ){ $this->__q[] = $value; return; }"` |
|    - |  421 | `"  $index = (int)$index;"` |
|    - |  422 | `"  if( $index < 0 \|\| $index >= count($this->__q) ){"` |
|    - |  423 | `"   throw new OutOfRangeException('SplDoublyLinkedList::offsetSet(): Argument #1"` |
|    - |  424 | `" ($index) is out of range');"` |
|    - |  425 | `"  }"` |
|    - |  426 | `"  $this->__q[$index] = $value;"` |
|    - |  427 | `" }"` |
|    - |  428 | `" public function offsetUnset($index){"` |
|    - |  429 | `"  $index = (int)$index;"` |
|    - |  430 | `"  if( $index < 0 \|\| $index >= count($this->__q) ){"` |
|    - |  431 | `"   throw new OutOfRangeException('SplDoublyLinkedList::offsetUnset(): Argument #1"` |
|    - |  432 | `" ($index) is out of range');"` |
|    - |  433 | `"  }"` |
|    - |  434 | `"  array_splice($this->__q, $index, 1);"` |
|    - |  435 | `" }"` |
|    - |  436 | `" public function rewind(){"` |
|    - |  437 | `"  $this->__i = ($this->__mode & 2) ? count($this->__q) - 1 : 0;"` |
|    - |  438 | `" }"` |
|    - |  439 | `" public function valid(){"` |
|    - |  440 | `"  return $this->__i >= 0 && $this->__i < count($this->__q);"` |
|    - |  441 | `" }"` |
|    - |  442 | `" public function current(){ return $this->__q[$this->__i] ?? null; }"` |
|    - |  443 | `" public function key(){ return $this->__i; }"` |
|    - |  444 | `" public function next(){"` |
|    - |  445 | `"  if( $this->__mode & 1 ){"` |
|    - |  446 | `"   /* IT_MODE_DELETE consumes the element just visited */"` |
|    - |  447 | `"   if( $this->__mode & 2 ){ array_pop($this->__q); $this->__i = count($this->__q) - 1; }"` |
|    - |  448 | `"   else { array_shift($this->__q); }"` |
|    - |  449 | `"  }else{"` |
|    - |  450 | `"   $this->__i += ($this->__mode & 2) ? -1 : 1;"` |
|    - |  451 | `"  }"` |
|    - |  452 | `" }"` |
|    - |  453 | `" public function prev(){ $this->__i += ($this->__mode & 2) ? 1 : -1; }"` |
|    - |  454 | `"}"` |
|    - |  455 | `"class SplStack extends SplDoublyLinkedList {}"` |
|    - |  456 | `"class SplQueue extends SplDoublyLinkedList {"` |
|    - |  457 | `" public function enqueue($value){ $this->push($value); }"` |
|    - |  458 | `" public function dequeue(){ return $this->shift(); }"` |
|    - |  459 | `"}"` |
|    - |  460 | `"abstract class SplHeap implements Iterator, Countable {"` |
|    - |  461 | `" private $__h = [];"` |
|    - |  462 | `" abstract protected function compare($value1, $value2);"` |
|    - |  463 | `" private function __hSiftUp($i){"` |
|    - |  464 | `"  while( $i > 0 ){"` |
|    - |  465 | `"   $p = ($i - 1) >> 1;"` |
|    - |  466 | `"   if( $this->compare($this->__h[$i], $this->__h[$p]) <= 0 ){ break; }"` |
|    - |  467 | `"   $t = $this->__h[$p]; $this->__h[$p] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  468 | `"   $i = $p;"` |
|    - |  469 | `"  }"` |
|    - |  470 | `" }"` |
|    - |  471 | `" private function __hSiftDown($i){"` |
|    - |  472 | `"  $n = count($this->__h);"` |
|    - |  473 | `"  for(;;){"` |
|    - |  474 | `"   $l = 2 * $i + 1; $r = $l + 1; $b = $i;"` |
|    - |  475 | `"   if( $l < $n && $this->compare($this->__h[$l], $this->__h[$b]) > 0 ){ $b = $l; }"` |
|    - |  476 | `"   if( $r < $n && $this->compare($this->__h[$r], $this->__h[$b]) > 0 ){ $b = $r; }"` |
|    - |  477 | `"   if( $b === $i ){ break; }"` |
|    - |  478 | `"   $t = $this->__h[$b]; $this->__h[$b] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  479 | `"   $i = $b;"` |
|    - |  480 | `"  }"` |
|    - |  481 | `" }"` |
|    - |  482 | `" public function insert($value){"` |
|    - |  483 | `"  $this->__h[] = $value;"` |
|    - |  484 | `"  $this->__hSiftUp(count($this->__h) - 1);"` |
|    - |  485 | `"  return true;"` |
|    - |  486 | `" }"` |
|    - |  487 | `" public function extract(){"` |
|    - |  488 | `"  $n = count($this->__h);"` |
|    - |  489 | `"  if( $n === 0 ){"` |
|    - |  490 | `"   throw new RuntimeException(\"Can't extract from an empty heap\");"` |
|    - |  491 | `"  }"` |
|    - |  492 | `"  $top = $this->__h[0];"` |
|    - |  493 | `"  $last = array_pop($this->__h);"` |
|    - |  494 | `"  if( $n > 1 ){"` |
|    - |  495 | `"   $this->__h[0] = $last;"` |
|    - |  496 | `"   $this->__hSiftDown(0);"` |
|    - |  497 | `"  }"` |
|    - |  498 | `"  return $top;"` |
|    - |  499 | `" }"` |
|    - |  500 | `" public function top(){"` |
|    - |  501 | `"  if( count($this->__h) === 0 ){"` |
|    - |  502 | `"   throw new RuntimeException(\"Can't peek at an empty heap\");"` |
|    - |  503 | `"  }"` |
|    - |  504 | `"  return $this->__h[0];"` |
|    - |  505 | `" }"` |
|    - |  506 | `" public function isEmpty(){ return count($this->__h) === 0; }"` |
|    - |  507 | `" public function count(){ return count($this->__h); }"` |
|    - |  508 | `" public function isCorrupted(){ return false; }"` |
|    - |  509 | `" public function recoverFromCorruption(){ return true; }"` |
|    - |  510 | `" public function rewind(){}"` |
|    - |  511 | `" public function valid(){ return count($this->__h) > 0; }"` |
|    - |  512 | `" public function current(){ return count($this->__h) ? $this->__h[0] : null; }"` |
|    - |  513 | `" public function key(){ return count($this->__h) - 1; }"` |
|    - |  514 | `" public function next(){ if( count($this->__h) ){ $this->extract(); } }"` |
|    - |  515 | `"}"` |
|    - |  516 | `"class SplMinHeap extends SplHeap {"` |
|    - |  517 | `" protected function compare($value1, $value2){ return $value2 <=> $value1; }"` |
|    - |  518 | `"}"` |
|    - |  519 | `"class SplMaxHeap extends SplHeap {"` |
|    - |  520 | `" protected function compare($value1, $value2){ return $value1 <=> $value2; }"` |
|    - |  521 | `"}"` |
|    - |  522 | `"class SplPriorityQueue implements Iterator, Countable {"` |
|    - |  523 | `" const EXTR_DATA = 1;"` |
|    - |  524 | `" const EXTR_PRIORITY = 2;"` |
|    - |  525 | `" const EXTR_BOTH = 3;"` |
|    - |  526 | `" private $__h = [];"` |
|    - |  527 | `" private $__serial = PHP_INT_MAX;"` |
|    - |  528 | `" private $__flags = 1;"` |
|    - |  529 | `" public function compare($priority1, $priority2){ return $priority1 <=> $priority2; }"` |
|    - |  530 | `" private function __pqCmp($a, $b){"` |
|    - |  531 | `"  /* NO tie-break: php's heap swaps only on strictly-greater, which fixes"` |
|    - |  532 | `"   * the (documented-as-undefined) equal-priority order it exhibits */"` |
|    - |  533 | `"  return $this->compare($a[0], $b[0]);"` |
|    - |  534 | `" }"` |
|    - |  535 | `" private function __pqSiftUp($i){"` |
|    - |  536 | `"  while( $i > 0 ){"` |
|    - |  537 | `"   $p = ($i - 1) >> 1;"` |
|    - |  538 | `"   if( $this->__pqCmp($this->__h[$i], $this->__h[$p]) <= 0 ){ break; }"` |
|    - |  539 | `"   $t = $this->__h[$p]; $this->__h[$p] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  540 | `"   $i = $p;"` |
|    - |  541 | `"  }"` |
|    - |  542 | `" }"` |
|    - |  543 | `" private function __pqSiftDown($i){"` |
|    - |  544 | `"  $n = count($this->__h);"` |
|    - |  545 | `"  for(;;){"` |
|    - |  546 | `"   $l = 2 * $i + 1; $r = $l + 1; $b = $i;"` |
|    - |  547 | `"   if( $l < $n && $this->__pqCmp($this->__h[$l], $this->__h[$b]) > 0 ){ $b = $l; }"` |
|    - |  548 | `"   if( $r < $n && $this->__pqCmp($this->__h[$r], $this->__h[$b]) > 0 ){ $b = $r; }"` |
|    - |  549 | `"   if( $b === $i ){ break; }"` |
|    - |  550 | `"   $t = $this->__h[$b]; $this->__h[$b] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  551 | `"   $i = $b;"` |
|    - |  552 | `"  }"` |
|    - |  553 | `" }"` |
|    - |  554 | `" public function insert($value, $priority){"` |
|    - |  555 | `"  $this->__h[] = [$priority, $this->__serial--, $value];"` |
|    - |  556 | `"  $this->__pqSiftUp(count($this->__h) - 1);"` |
|    - |  557 | `"  return true;"` |
|    - |  558 | `" }"` |
|    - |  559 | `" private function __pqShape($node){"` |
|    - |  560 | `"  if( $this->__flags === self::EXTR_BOTH ){"` |
|    - |  561 | `"   return ['data' => $node[2], 'priority' => $node[0]];"` |
|    - |  562 | `"  }"` |
|    - |  563 | `"  if( $this->__flags === self::EXTR_PRIORITY ){ return $node[0]; }"` |
|    - |  564 | `"  return $node[2];"` |
|    - |  565 | `" }"` |
|    - |  566 | `" public function extract(){"` |
|    - |  567 | `"  $n = count($this->__h);"` |
|    - |  568 | `"  if( $n === 0 ){"` |
|    - |  569 | `"   throw new RuntimeException(\"Can't extract from an empty heap\");"` |
|    - |  570 | `"  }"` |
|    - |  571 | `"  $top = $this->__h[0];"` |
|    - |  572 | `"  $last = array_pop($this->__h);"` |
|    - |  573 | `"  if( $n > 1 ){"` |
|    - |  574 | `"   $this->__h[0] = $last;"` |
|    - |  575 | `"   $this->__pqSiftDown(0);"` |
|    - |  576 | `"  }"` |
|    - |  577 | `"  return $this->__pqShape($top);"` |
|    - |  578 | `" }"` |
|    - |  579 | `" public function top(){"` |
|    - |  580 | `"  if( count($this->__h) === 0 ){"` |
|    - |  581 | `"   throw new RuntimeException(\"Can't peek at an empty heap\");"` |
|    - |  582 | `"  }"` |
|    - |  583 | `"  return $this->__pqShape($this->__h[0]);"` |
|    - |  584 | `" }"` |
|    - |  585 | `" public function setExtractFlags($flags){ $this->__flags = (int)$flags; }"` |
|    - |  586 | `" public function getExtractFlags(){ return $this->__flags; }"` |
|    - |  587 | `" public function isEmpty(){ return count($this->__h) === 0; }"` |
|    - |  588 | `" public function count(){ return count($this->__h); }"` |
|    - |  589 | `" public function isCorrupted(){ return false; }"` |
|    - |  590 | `" public function recoverFromCorruption(){ return true; }"` |
|    - |  591 | `" public function rewind(){}"` |
|    - |  592 | `" public function valid(){ return count($this->__h) > 0; }"` |
|    - |  593 | `" public function current(){ return count($this->__h) ? $this->__pqShape($this->__h[0]) : null; }"` |
|    - |  594 | `" public function key(){ return count($this->__h) - 1; }"` |
|    - |  595 | `" public function next(){ if( count($this->__h) ){ $this->extract(); } }"` |
|    - |  596 | `"}"` |
|    - |  597 | `"class SplFixedArray implements ArrayAccess, Countable, IteratorAggregate, JsonSerializable {"` |
|    - |  598 | `" private $__a = [];"` |
|    - |  599 | `" private $__n = 0;"` |
|    - |  600 | `" public function __construct($size = 0){"` |
|    - |  601 | `"  $this->setSize((int)$size);"` |
|    - |  602 | `" }"` |
|    - |  603 | `" private function __faIdx($index, $method){"` |
|    - |  604 | `"  if( !is_int($index) ){"` |
|    - |  605 | `"   if( is_string($index) && ctype_digit($index) ){"` |
|    - |  606 | `"    $index = (int)$index;"` |
|    - |  607 | `"   }else{"` |
|    - |  608 | `"    throw new TypeError('Cannot access offset of type ' . get_debug_type($index)"` |
|    - |  609 | `"     . ' on SplFixedArray');"` |
|    - |  610 | `"   }"` |
|    - |  611 | `"  }"` |
|    - |  612 | `"  if( $index < 0 \|\| $index >= $this->__n ){"` |
|    - |  613 | `"   throw new OutOfBoundsException('Index invalid or out of range');"` |
|    - |  614 | `"  }"` |
|    - |  615 | `"  return $index;"` |
|    - |  616 | `" }"` |
|    - |  617 | `" public function offsetExists($index){"` |
|    - |  618 | `"  if( !is_int($index) && !(is_string($index) && ctype_digit($index)) ){ return false; }"` |
|    - |  619 | `"  $index = (int)$index;"` |
|    - |  620 | `"  return $index >= 0 && $index < $this->__n && $this->__a[$index] !== null;"` |
|    - |  621 | `" }"` |
|    - |  622 | `" public function offsetGet($index){ return $this->__a[$this->__faIdx($index, 'offsetGet')]; }"` |
|    - |  623 | `" public function offsetSet($index, $value){ $this->__a[$this->__faIdx($index, 'offsetSet')] = $value; }"` |
|    - |  624 | `" public function offsetUnset($index){ $this->__a[$this->__faIdx($index, 'offsetUnset')] = null; }"` |
|    - |  625 | `" public function getSize(){ return $this->__n; }"` |
|    - |  626 | `" public function setSize($size){"` |
|    - |  627 | `"  $size = (int)$size;"` |
|    - |  628 | `"  if( $size < 0 ){"` |
|    - |  629 | `"   throw new ValueError('SplFixedArray::setSize(): Argument #1 ($size) must be"` |
|    - |  630 | `" greater than or equal to 0');"` |
|    - |  631 | `"  }"` |
|    - |  632 | `"  if( $size < $this->__n ){"` |
|    - |  633 | `"   $this->__a = array_slice($this->__a, 0, $size);"` |
|    - |  634 | `"  }else{"` |
|    - |  635 | `"   for( $i = $this->__n; $i < $size; $i++ ){ $this->__a[$i] = null; }"` |
|    - |  636 | `"  }"` |
|    - |  637 | `"  $this->__n = $size;"` |
|    - |  638 | `"  return true;"` |
|    - |  639 | `" }"` |
|    - |  640 | `" public function count(){ return $this->__n; }"` |
|    - |  641 | `" public function toArray(){ return $this->__a; }"` |
|    - |  642 | `" public static function fromArray($array, $preserveKeys = true){"` |
|    - |  643 | `"  $f = new SplFixedArray(0);"` |
|    - |  644 | `"  if( $preserveKeys ){"` |
|    - |  645 | `"   $max = -1;"` |
|    - |  646 | `"   foreach( $array as $k => $v ){"` |
|    - |  647 | `"    if( !is_int($k) \|\| $k < 0 ){"` |
|    - |  648 | `"     throw new InvalidArgumentException('array must contain only positive integer keys');"` |
|    - |  649 | `"    }"` |
|    - |  650 | `"    if( $k > $max ){ $max = $k; }"` |
|    - |  651 | `"   }"` |
|    - |  652 | `"   $f->setSize($max + 1);"` |
|    - |  653 | `"   foreach( $array as $k => $v ){ $f[$k] = $v; }"` |
|    - |  654 | `"  }else{"` |
|    - |  655 | `"   $vals = array_values($array);"` |
|    - |  656 | `"   $f->setSize(count($vals));"` |
|    - |  657 | `"   foreach( $vals as $k => $v ){ $f[$k] = $v; }"` |
|    - |  658 | `"  }"` |
|    - |  659 | `"  return $f;"` |
|    - |  660 | `" }"` |
|    - |  661 | `" public function getIterator(): Generator {"` |
|    - |  662 | `"  for( $i = 0; $i < $this->__n; $i++ ){ yield $i => $this->__a[$i]; }"` |
|    - |  663 | `" }"` |
|    - |  664 | `" public function jsonSerialize(){ return $this->__a; }"` |
|    - |  665 | `"}"` |
|    - |  666 | `"class SplObjectStorage implements Countable, Iterator, ArrayAccess {"` |
|    - |  667 | `" private $__o = [];"` |
|    - |  668 | `" private $__i = 0;"` |
|    - |  669 | `" public function attach($object, $info = null){"` |
|    - |  670 | `"  __spl_deprecated('Method SplObjectStorage::attach() is deprecated since 8.5, use"` |
|    - |  671 | `" method SplObjectStorage::offsetSet() instead');"` |
|    - |  672 | `"  $this->offsetSet($object, $info);"` |
|    - |  673 | `" }"` |
|    - |  674 | `" public function detach($object){"` |
|    - |  675 | `"  __spl_deprecated('Method SplObjectStorage::detach() is deprecated since 8.5, use"` |
|    - |  676 | `" method SplObjectStorage::offsetUnset() instead');"` |
|    - |  677 | `"  $this->offsetUnset($object);"` |
|    - |  678 | `" }"` |
|    - |  679 | `" public function contains($object){"` |
|    - |  680 | `"  __spl_deprecated('Method SplObjectStorage::contains() is deprecated since 8.5, use"` |
|    - |  681 | `" method SplObjectStorage::offsetExists() instead');"` |
|    - |  682 | `"  return $this->offsetExists($object);"` |
|    - |  683 | `" }"` |
|    - |  684 | `" public function offsetSet($object, $info = null){"` |
|    - |  685 | `"  $this->__o[spl_object_id($object)] = [$object, $info];"` |
|    - |  686 | `" }"` |
|    - |  687 | `" public function offsetExists($object){"` |
|    - |  688 | `"  return isset($this->__o[spl_object_id($object)]);"` |
|    - |  689 | `" }"` |
|    - |  690 | `" public function offsetGet($object){"` |
|    - |  691 | `"  $id = spl_object_id($object);"` |
|    - |  692 | `"  if( !isset($this->__o[$id]) ){"` |
|    - |  693 | `"   throw new UnexpectedValueException('Object not found');"` |
|    - |  694 | `"  }"` |
|    - |  695 | `"  return $this->__o[$id][1];"` |
|    - |  696 | `" }"` |
|    - |  697 | `" public function offsetUnset($object){"` |
|    - |  698 | `"  unset($this->__o[spl_object_id($object)]);"` |
|    - |  699 | `" }"` |
|    - |  700 | `" public function addAll($storage){"` |
|    - |  701 | `"  foreach( $storage as $obj ){"` |
|    - |  702 | `"   $this->offsetSet($obj, $storage[$obj]);"` |
|    - |  703 | `"  }"` |
|    - |  704 | `"  return $this->count();"` |
|    - |  705 | `" }"` |
|    - |  706 | `" public function removeAll($storage){"` |
|    - |  707 | `"  foreach( $storage as $obj ){ $this->offsetUnset($obj); }"` |
|    - |  708 | `"  return $this->count();"` |
|    - |  709 | `" }"` |
|    - |  710 | `" public function removeAllExcept($storage){"` |
|    - |  711 | `"  foreach( $this->__o as $id => $pair ){"` |
|    - |  712 | `"   if( !$storage->offsetExists($pair[0]) ){ unset($this->__o[$id]); }"` |
|    - |  713 | `"  }"` |
|    - |  714 | `"  return $this->count();"` |
|    - |  715 | `" }"` |
|    - |  716 | `" public function getHash($object){ return spl_object_hash($object); }"` |
|    - |  717 | `" public function count($mode = 0){ return count($this->__o); }"` |
|    - |  718 | `" public function getInfo(){"` |
|    - |  719 | `"  $pair = array_values($this->__o)[$this->__i] ?? null;"` |
|    - |  720 | `"  return $pair === null ? null : $pair[1];"` |
|    - |  721 | `" }"` |
|    - |  722 | `" public function setInfo($info){"` |
|    - |  723 | `"  $keys = array_keys($this->__o);"` |
|    - |  724 | `"  if( isset($keys[$this->__i]) ){ $this->__o[$keys[$this->__i]][1] = $info; }"` |
|    - |  725 | `" }"` |
|    - |  726 | `" public function rewind(){ $this->__i = 0; }"` |
|    - |  727 | `" public function valid(){ return $this->__i < count($this->__o); }"` |
|    - |  728 | `" public function key(){ return $this->__i; }"` |
|    - |  729 | `" public function current(){"` |
|    - |  730 | `"  $pair = array_values($this->__o)[$this->__i] ?? null;"` |
|    - |  731 | `"  return $pair === null ? null : $pair[0];"` |
|    - |  732 | `" }"` |
|    - |  733 | `" public function next(){ $this->__i++; }"` |
|    - |  734 | `"}"` |
|    - |  735 | `"interface SplObserver {"` |
|    - |  736 | `" public function update(SplSubject $subject);"` |
|    - |  737 | `"}"` |
|    - |  738 | `"interface SplSubject {"` |
|    - |  739 | `" public function attach(SplObserver $observer);"` |
|    - |  740 | `" public function detach(SplObserver $observer);"` |
|    - |  741 | `" public function notify();"` |
|    - |  742 | `"}"` |
|    - |  743 | `;` |
|    - |  744 |  |
| 3884 |  745 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm)` |
|    5 |  746 | `{` |
| 3889 |  747 | `	ph7_create_function(&(*pVm),"__spl_deprecated",vm_builtin_spl_deprecated,0);` |
| 3889 |  748 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zSplLib,sizeof(zSplLib)-1);` |
|    5 |  749 | `}` |
|    - |  750 |  |
|    - |  751 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - |  752 |  |
|    - |  753 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|    - |  754 | `/* Tiny build: no SPL (builtin layer disabled) */` |
|    - |  755 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|    - |  756 | `#endif` |
|    - |  757 |  |
