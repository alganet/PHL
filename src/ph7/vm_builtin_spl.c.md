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
|    - |  348 | `;` |
|    - |  349 |  |
| 3876 |  350 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm)` |
|    5 |  351 | `{` |
| 3881 |  352 | `	ph7_create_function(&(*pVm),"__spl_deprecated",vm_builtin_spl_deprecated,0);` |
| 3881 |  353 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zSplLib,sizeof(zSplLib)-1);` |
|    5 |  354 | `}` |
|    - |  355 |  |
|    - |  356 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - |  357 |  |
|    - |  358 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|    - |  359 | `/* Tiny build: no SPL (builtin layer disabled) */` |
|    - |  360 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|    - |  361 | `#endif` |
|    - |  362 |  |
