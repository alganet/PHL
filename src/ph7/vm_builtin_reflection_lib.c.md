# src/ph7/vm_builtin_reflection_lib.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 20/28 lines (71.43%)

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
|    - |    8 | ` * The embedded PHP source of the Reflection class library (chunks 1-9),` |
|    - |    9 | ` * compiled at VM init by PH7_VmInstallReflectionLib() — the tail step of` |
|    - |   10 | ` * PH7_VmInstallReflection() (vm_builtin_reflection.c, which keeps the C` |
|    - |   11 | ` * host-function thunks the library calls).` |
|    - |   12 | ` */` |
|    - |   13 | `/*` |
|    - |   14 | ` * The Reflection classes, in PHP. Chunk 1: exceptions, Reflector,` |
|    - |   15 | ` * Reflection, ReflectionClass, ReflectionObject (plus get_debug_type,` |
|    - |   16 | ` * which the TypeError messages need and PHP 8.0 ships natively).` |
|    - |   17 | ` */` |
|    - |   18 | `static const char zReflectLib1[] =` |
|    - |   19 | `/* Per-class memoization of the (expensive) C class descriptor. Every Reflection*` |
|    - |   20 | ` * accessor funnels through __rinfo()/the constructors, so without this a single` |
|    - |   21 | ` * test run rebuilds the full descriptor of the same class hundreds of times` |
|    - |   22 | ` * (real php caches the reflected class). Keyed by resolved class name; only` |
|    - |   23 | ` * successful lookups are cached, so a not-yet-autoloaded class is re-queried.` |
|    - |   24 | ` * The C builtin __reflect_class_info stays the real worker. */` |
|    - |   25 | `"function __phl_rcinfo($oc){"` |
|    - |   26 | `" static $c = array();"` |
|    - |   27 | `" $k = is_object($oc) ? get_class($oc) : (string)$oc;"` |
|    - |   28 | `" if( isset($c[$k]) ){ return $c[$k]; }"` |
|    - |   29 | `" $info = __reflect_class_info($oc);"` |
|    - |   30 | `" if( $info !== null ){ $c[$k] = $info; }"` |
|    - |   31 | `" return $info;"` |
|    - |   32 | `"}"` |
|    - |   33 | `"function get_debug_type($value){"` |
|    - |   34 | `" if(is_object($value)){ return get_class($value); }"` |
|    - |   35 | `" if(is_bool($value)){ return 'bool'; }"` |
|    - |   36 | `" if(is_int($value)){ return 'int'; }"` |
|    - |   37 | `" if(is_float($value)){ return 'float'; }"` |
|    - |   38 | `" if(is_string($value)){ return 'string'; }"` |
|    - |   39 | `" if(is_array($value)){ return 'array'; }"` |
|    - |   40 | `" if($value === null){ return 'null'; }"` |
|    - |   41 | `" return gettype($value);"` |
|    - |   42 | `"}"` |
|    - |   43 | `"interface Reflector extends Stringable {}"` |
|    - |   44 | `"class ReflectionException extends Exception {}"` |
|    - |   45 | `"class Reflection {"` |
|    - |   46 | `" public static function getModifierNames($modifiers){"` |
|    - |   47 | `"  $names = array();"` |
|    - |   48 | `"  if($modifiers & 64){ $names[] = 'abstract'; }"` |
|    - |   49 | `"  if($modifiers & 32){ $names[] = 'final'; }"` |
|    - |   50 | `"  if($modifiers & 1){ $names[] = 'public'; }"` |
|    - |   51 | `"  if($modifiers & 2){ $names[] = 'protected'; }"` |
|    - |   52 | `"  if($modifiers & 4){ $names[] = 'private'; }"` |
|    - |   53 | `"  if($modifiers & 16){ $names[] = 'static'; }"` |
|    - |   54 | `"  if($modifiers & 128){ $names[] = 'readonly'; }"` |
|    - |   55 | `"  return $names;"` |
|    - |   56 | `" }"` |
|    - |   57 | `"}"` |
|    - |   58 | `"class ReflectionClass implements Reflector {"` |
|    - |   59 | `" const IS_IMPLICIT_ABSTRACT = 16;"` |
|    - |   60 | `" const IS_EXPLICIT_ABSTRACT = 64;"` |
|    - |   61 | `" const IS_FINAL = 32;"` |
|    - |   62 | `" const IS_READONLY = 65536;"` |
|    - |   63 | `" const SKIP_INITIALIZATION_ON_SERIALIZE = 8;"` |
|    - |   64 | `" const SKIP_DESTRUCTOR = 16;"` |
|    - |   65 | `" public $name;"` |
|    - |   66 | `" protected $__obj = null;"` |
|    - |   67 | `" public function __construct($objectOrClass){"` |
|    - |   68 | `"  if(!is_object($objectOrClass) && !is_string($objectOrClass)){"` |
|    - |   69 | `"   if(is_int($objectOrClass) \|\| is_float($objectOrClass) \|\| is_bool($objectOrClass)){"` |
|    - |   70 | `"    $objectOrClass = (string)$objectOrClass;"` |
|    - |   71 | `"   }else{"` |
|    - |   72 | `"    throw new TypeError('ReflectionClass::__construct(): Argument #1 ($objectOrClass) must be of type object\|string, '.get_debug_type($objectOrClass).' given');"` |
|    - |   73 | `"   }"` |
|    - |   74 | `"  }"` |
|    - |   75 | `"  $info = __phl_rcinfo($objectOrClass);"` |
|    - |   76 | `"  if($info === null){"` |
|    - |   77 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|    - |   78 | `"  }"` |
|    - |   79 | `"  $this->name = $info['name'];"` |
|    - |   80 | `" }"` |
|    - |   81 | `" protected function __rinfo(){ return __phl_rcinfo($this->name); }"` |
|    - |   82 | `" public function getName(){ return $this->name; }"` |
|    - |   83 | `" public function getShortName(){"` |
|    - |   84 | `"  $p = strrpos($this->name,'\\\\');"` |
|    - |   85 | `"  if($p === false){ return $this->name; }"` |
|    - |   86 | `"  return substr($this->name,$p+1);"` |
|    - |   87 | `" }"` |
|    - |   88 | `" public function getNamespaceName(){"` |
|    - |   89 | `"  $p = strrpos($this->name,'\\\\');"` |
|    - |   90 | `"  if($p === false){ return ''; }"` |
|    - |   91 | `"  return substr($this->name,0,$p);"` |
|    - |   92 | `" }"` |
|    - |   93 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|    - |   94 | `" public function isInternal(){ $i = $this->__rinfo(); return $i['internal']; }"` |
|    - |   95 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|    - |   96 | `" public function isInterface(){ $i = $this->__rinfo(); return $i['interface']; }"` |
|    - |   97 | `" public function isTrait(){ $i = $this->__rinfo(); return $i['trait']; }"` |
|    - |   98 | `" public function isAbstract(){ $i = $this->__rinfo(); return $i['abstract']; }"` |
|    - |   99 | `" public function isFinal(){ $i = $this->__rinfo(); return $i['final']; }"` |
|    - |  100 | `" public function isReadOnly(){ $i = $this->__rinfo(); return $i['readonly']; }"` |
|    - |  101 | `" public function isEnum(){ $i = $this->__rinfo(); return $i['enum']; }"` |
|    - |  102 | `" public function isAnonymous(){ return strpos($this->name,'class@anonymous') === 0; }"` |
|    - |  103 | `" public function getModifiers(){"` |
|    - |  104 | `"  $i = $this->__rinfo();"` |
|    - |  105 | `"  $m = 0;"` |
|    - |  106 | `"  if($i['abstract']){ $m \|= 64; }"` |
|    - |  107 | `"  if($i['final']){ $m \|= 32; }"` |
|    - |  108 | `"  if($i['readonly']){ $m \|= 65536; }"` |
|    - |  109 | `"  return $m;"` |
|    - |  110 | `" }"` |
|    - |  111 | `" public function getParentClass(){"` |
|    - |  112 | `"  $i = $this->__rinfo();"` |
|    - |  113 | `"  if($i['parent'] === null){ return false; }"` |
|    - |  114 | `"  return new ReflectionClass($i['parent']);"` |
|    - |  115 | `" }"` |
|    - |  116 | `" public function getInterfaceNames(){ $i = $this->__rinfo(); return $i['interfaces']; }"` |
|    - |  117 | `" public function getInterfaces(){"` |
|    - |  118 | `"  $i = $this->__rinfo();"` |
|    - |  119 | `"  $out = array();"` |
|    - |  120 | `"  foreach($i['interfaces'] as $n){ $out[$n] = new ReflectionClass($n); }"` |
|    - |  121 | `"  return $out;"` |
|    - |  122 | `" }"` |
|    - |  123 | `" public function getTraitNames(){ $i = $this->__rinfo(); return $i['traits']; }"` |
|    - |  124 | `" public function getTraits(){"` |
|    - |  125 | `"  $i = $this->__rinfo();"` |
|    - |  126 | `"  $out = array();"` |
|    - |  127 | `"  foreach($i['traits'] as $n){ $out[$n] = new ReflectionClass($n); }"` |
|    - |  128 | `"  return $out;"` |
|    - |  129 | `" }"` |
|    - |  130 | `" public function getTraitAliases(){ return array(); }"` |
|    - |  131 | `" public function implementsInterface($interface){"` |
|    - |  132 | `"  if($interface instanceof ReflectionClass){ $interface = $interface->name; }"` |
|    - |  133 | `"  $target = __phl_rcinfo($interface);"` |
|    - |  134 | `"  if($target === null){"` |
|    - |  135 | `"   throw new ReflectionException('Interface \"'.$interface.'\" does not exist');"` |
|    - |  136 | `"  }"` |
|    - |  137 | `"  if(!$target['interface']){"` |
|    - |  138 | `"   throw new ReflectionException($target['name'].' is not an interface');"` |
|    - |  139 | `"  }"` |
|    - |  140 | `"  $name = $target['name'];"` |
|    - |  141 | `"  if($this->name === $name){ return true; }"` |
|    - |  142 | `"  $i = $this->__rinfo();"` |
|    - |  143 | `"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"` |
|    - |  144 | `"  return false;"` |
|    - |  145 | `" }"` |
|    - |  146 | `" public function isSubclassOf($class){"` |
|    - |  147 | `"  if($class instanceof ReflectionClass){ $class = $class->name; }"` |
|    - |  148 | `"  $target = __phl_rcinfo($class);"` |
|    - |  149 | `"  if($target === null){"` |
|    - |  150 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|    - |  151 | `"  }"` |
|    - |  152 | `"  $name = $target['name'];"` |
|    - |  153 | `"  if($name === $this->name){ return false; }"` |
|    - |  154 | `"  $i = $this->__rinfo();"` |
|    - |  155 | `"  $p = $i['parent'];"` |
|    - |  156 | `"  while($p !== null){"` |
|    - |  157 | `"   if($p === $name){ return true; }"` |
|    - |  158 | `"   $pi = __phl_rcinfo($p);"` |
|    - |  159 | `"   $p = $pi['parent'];"` |
|    - |  160 | `"  }"` |
|    - |  161 | `"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"` |
|    - |  162 | `"  return false;"` |
|    - |  163 | `" }"` |
|    - |  164 | `" public function isInstance($object){"` |
|    - |  165 | `"  if(!is_object($object)){"` |
|    - |  166 | `"   throw new TypeError('ReflectionClass::isInstance(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|    - |  167 | `"  }"` |
|    - |  168 | `"  return is_a($object,$this->name);"` |
|    - |  169 | `" }"` |
|    - |  170 | `" public function hasMethod($name){"` |
|    - |  171 | `"  $i = $this->__rinfo();"` |
|    - |  172 | `"  $l = strtolower($name);"` |
|    - |  173 | `"  foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ return true; } }"` |
|    - |  174 | `"  return false;"` |
|    - |  175 | `" }"` |
|    - |  176 | `" public function hasProperty($name){"` |
|    - |  177 | `"  $i = $this->__rinfo();"` |
|    - |  178 | `"  if(isset($i['props'][$name])){ return true; }"` |
|    - |  179 | `"  if($this->__obj !== null){ return (__reflect_prop_state($this->__obj, $name) & 1) !== 0; }"` |
|    - |  180 | `"  return false;"` |
|    - |  181 | `" }"` |
|    - |  182 | `" public function hasConstant($name){ $i = $this->__rinfo(); return isset($i['consts'][$name]); }"` |
|    - |  183 | `" public function getConstant($name){"` |
|    - |  184 | `"  $i = $this->__rinfo();"` |
|    - |  185 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|    - |  186 | `"  return __reflect_const_value($this->name,$name);"` |
|    - |  187 | `" }"` |
|    - |  188 | `" public function getConstants($filter = null){"` |
|    - |  189 | `"  $i = $this->__rinfo();"` |
|    - |  190 | `"  $out = array();"` |
|    - |  191 | `"  foreach($i['consts'] as $k => $c){"` |
|    - |  192 | `"   if($filter !== null){"` |
|    - |  193 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|    - |  194 | `"    if(($m & $filter) === 0){ continue; }"` |
|    - |  195 | `"   }"` |
|    - |  196 | `"   $out[$k] = __reflect_const_value($this->name,$k);"` |
|    - |  197 | `"  }"` |
|    - |  198 | `"  return $out;"` |
|    - |  199 | `" }"` |
|    - |  200 | `" public function getStartLine(){"` |
|    - |  201 | `"  $i = $this->__rinfo();"` |
|    - |  202 | `"  if($i['internal']){ return false; }"` |
|    - |  203 | `"  return $i['line'];"` |
|    - |  204 | `" }"` |
|    - |  205 | `" public function getEndLine(){"` |
|    - |  206 | `"  $i = $this->__rinfo();"` |
|    - |  207 | `"  if($i['internal']){ return false; }"` |
|    - |  208 | `"  return $i['endline'];"` |
|    - |  209 | `" }"` |
|    - |  210 | `" public function getFileName(){ $i = $this->__rinfo(); return $i['file']; }"` |
|    - |  211 | `" public function getDocComment(){ $i = $this->__rinfo(); return $i['doc']; }"` |
|    - |  212 | `" public function isInstantiable(){"` |
|    - |  213 | `"  $i = $this->__rinfo();"` |
|    - |  214 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract'] \|\| $i['enum']){ return false; }"` |
|    - |  215 | `"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){ return false; }"` |
|    - |  216 | `"  return true;"` |
|    - |  217 | `" }"` |
|    - |  218 | `" public function isCloneable(){"` |
|    - |  219 | `"  $i = $this->__rinfo();"` |
|    - |  220 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|    - |  221 | `"  if($i['clonevis'] !== 0 && $i['clonevis'] !== 1){ return false; }"` |
|    - |  222 | `"  return true;"` |
|    - |  223 | `" }"` |
|    - |  224 | `" public function isIterable(){"` |
|    - |  225 | `"  $i = $this->__rinfo();"` |
|    - |  226 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|    - |  227 | `"  return $i['iterable'];"` |
|    - |  228 | `" }"` |
|    - |  229 | `" public function isIterateable(){ return $this->isIterable(); }"` |
|    - |  230 | `" public function newInstance(...$args){ return $this->__rnew($args); }"` |
|    - |  231 | `" public function newInstanceArgs(array $args = array()){ return $this->__rnew($args); }"` |
|    - |  232 | `" protected function __rnew($args){"` |
|    - |  233 | `"  $i = $this->__rinfo();"` |
|    - |  234 | `"  $this->__rcheckInstantiable($i);"` |
|    - |  235 | `"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){"` |
|    - |  236 | `"   throw new ReflectionException('Access to non-public constructor of class '.$this->name);"` |
|    - |  237 | `"  }"` |
|    - |  238 | `"  if($i['ctorvis'] === 0 && count($args) > 0){"` |
|    - |  239 | `"   throw new ReflectionException('Class '.$this->name.' does not have a constructor, so you cannot pass any constructor arguments');"` |
|    - |  240 | `"  }"` |
|    - |  241 | `"  return __reflect_new_instance($this->name,$args);"` |
|    - |  242 | `" }"` |
|    - |  243 | `" protected function __rcheckInstantiable($i){"` |
|    - |  244 | `"  if($i['interface']){ throw new Error('Cannot instantiate interface '.$this->name); }"` |
|    - |  245 | `"  if($i['trait']){ throw new Error('Cannot instantiate trait '.$this->name); }"` |
|    - |  246 | `"  if($i['abstract']){ throw new Error('Cannot instantiate abstract class '.$this->name); }"` |
|    - |  247 | `" }"` |
|    - |  248 | `" public function newInstanceWithoutConstructor(){"` |
|    - |  249 | `"  $i = $this->__rinfo();"` |
|    - |  250 | `"  $this->__rcheckInstantiable($i);"` |
|    - |  251 | `"  return __reflect_new_no_ctor($this->name);"` |
|    - |  252 | `" }"` |
|    - |  253 | `" public function getStaticProperties(){"` |
|    - |  254 | `"  $i = $this->__rinfo();"` |
|    - |  255 | `"  $out = array();"` |
|    - |  256 | `"  foreach($i['props'] as $k => $p){"` |
|    - |  257 | `"   if($p['static']){ $out[$k] = __reflect_static_value($this->name,$k); }"` |
|    - |  258 | `"  }"` |
|    - |  259 | `"  return $out;"` |
|    - |  260 | `" }"` |
|    - |  261 | `" public function getStaticPropertyValue($name, ...$def){"` |
|    - |  262 | `"  $i = $this->__rinfo();"` |
|    - |  263 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|    - |  264 | `"   if(count($def) > 0){ return $def[0]; }"` |
|    - |  265 | `"   throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|    - |  266 | `"  }"` |
|    - |  267 | `"  return __reflect_static_value($this->name,$name);"` |
|    - |  268 | `" }"` |
|    - |  269 | `" public function setStaticPropertyValue($name,$value){"` |
|    - |  270 | `"  $i = $this->__rinfo();"` |
|    - |  271 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|    - |  272 | `"   throw new ReflectionException('Class '.$this->name.' does not have a property named '.$name);"` |
|    - |  273 | `"  }"` |
|    - |  274 | `"  __reflect_static_set($this->name,$name,$value);"` |
|    - |  275 | `" }"` |
|    - |  276 | `" public function getDefaultProperties(){"` |
|    - |  277 | `"  $i = $this->__rinfo();"` |
|    - |  278 | `"  $out = array();"` |
|    - |  279 | `"  foreach($i['props'] as $k => $p){"` |
|    - |  280 | `"   if($p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|    - |  281 | `"  }"` |
|    - |  282 | `"  foreach($i['props'] as $k => $p){"` |
|    - |  283 | `"   if(!$p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|    - |  284 | `"  }"` |
|    - |  285 | `"  return $out;"` |
|    - |  286 | `" }"` |
|    - |  287 | `" public function getProperty($name){"` |
|    - |  288 | `"  $i = $this->__rinfo();"` |
|    - |  289 | `"  if(isset($i['props'][$name])){"` |
|    - |  290 | `"   return new ReflectionProperty($this->name, $name);"` |
|    - |  291 | `"  }"` |
|    - |  292 | `"  if($this->__obj !== null && (__reflect_prop_state($this->__obj, $name) & 1)){"` |
|    - |  293 | `"   return new ReflectionProperty($this->__obj, $name);"` |
|    - |  294 | `"  }"` |
|    - |  295 | `"  throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|    - |  296 | `" }"` |
|    - |  297 | `" public function getProperties($filter = null){"` |
|    - |  298 | `"  $i = $this->__rinfo();"` |
|    - |  299 | `"  $out = array();"` |
|    - |  300 | `"  foreach($i['props'] as $k => $p){"` |
|    - |  301 | `"   if($filter !== null){"` |
|    - |  302 | `"    $m = ($p['vis'] === 1 ? 1 : ($p['vis'] === 2 ? 2 : 4));"` |
|    - |  303 | `"    if($p['static']){ $m \|= 16; }"` |
|    - |  304 | `"    if($p['readonly']){ $m \|= 128; }"` |
|    - |  305 | `"    if(($m & $filter) === 0){ continue; }"` |
|    - |  306 | `"   }"` |
|    - |  307 | `"   $out[] = new ReflectionProperty($this->name, $k);"` |
|    - |  308 | `"  }"` |
|    - |  309 | `"  if($this->__obj !== null){"` |
|    - |  310 | `"   foreach(__reflect_dyn_props($this->__obj) as $k){"` |
|    - |  311 | `"    if(isset($i['props'][$k])){ continue; }"` |
|    - |  312 | `"    if($filter !== null && ($filter & 1) === 0){ continue; }"` |
|    - |  313 | `"    $out[] = new ReflectionProperty($this->__obj, $k);"` |
|    - |  314 | `"   }"` |
|    - |  315 | `"  }"` |
|    - |  316 | `"  return $out;"` |
|    - |  317 | `" }"` |
|    - |  318 | `" public function getMethod($name){"` |
|    - |  319 | `"  $i = $this->__rinfo();"` |
|    - |  320 | `"  $found = null;"` |
|    - |  321 | `"  if(isset($i['methods'][$name])){"` |
|    - |  322 | `"   $found = $name;"` |
|    - |  323 | `"  }else{"` |
|    - |  324 | `"   $l = strtolower($name);"` |
|    - |  325 | `"   foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ $found = $k; break; } }"` |
|    - |  326 | `"  }"` |
|    - |  327 | `"  if($found === null){"` |
|    - |  328 | `"   throw new ReflectionException('Method '.$this->name.'::'.$name.'() does not exist');"` |
|    - |  329 | `"  }"` |
|    - |  330 | `"  return new ReflectionMethod($this->name, $found);"` |
|    - |  331 | `" }"` |
|    - |  332 | `" public function getMethods($filter = null){"` |
|    - |  333 | `"  $i = $this->__rinfo();"` |
|    - |  334 | `"  $out = array();"` |
|    - |  335 | `"  foreach($i['methods'] as $k => $m){"` |
|    - |  336 | `"   if($filter !== null){"` |
|    - |  337 | `"    $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|    - |  338 | `"    if($m['static']){ $mod \|= 16; }"` |
|    - |  339 | `"    if($m['abstract']){ $mod \|= 64; }"` |
|    - |  340 | `"    if($m['final']){ $mod \|= 32; }"` |
|    - |  341 | `"    if(($mod & $filter) === 0){ continue; }"` |
|    - |  342 | `"   }"` |
|    - |  343 | `"   $out[] = new ReflectionMethod($this->name, $k);"` |
|    - |  344 | `"  }"` |
|    - |  345 | `"  return $out;"` |
|    - |  346 | `" }"` |
|    - |  347 | `" public function getConstructor(){"` |
|    - |  348 | `"  $i = $this->__rinfo();"` |
|    - |  349 | `"  if(isset($i['methods']['__construct'])){"` |
|    - |  350 | `"   return new ReflectionMethod($this->name, '__construct');"` |
|    - |  351 | `"  }"` |
|    - |  352 | `"  foreach($i['methods'] as $k => $m){"` |
|    - |  353 | `"   if(strtolower($k) === '__construct'){ return new ReflectionMethod($this->name, $k); }"` |
|    - |  354 | `"  }"` |
|    - |  355 | `"  if($i['ctorvis'] !== 0 && isset($i['methods'][$this->name])){"` |
|    - |  356 | `"   return new ReflectionMethod($this->name, $this->name);"` |
|    - |  357 | `"  }"` |
|    - |  358 | `"  return null;"` |
|    - |  359 | `" }"` |
|    - |  360 | `" public function getReflectionConstant($name){"` |
|    - |  361 | `"  $i = $this->__rinfo();"` |
|    - |  362 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|    - |  363 | `"  return new ReflectionClassConstant($this->name, $name);"` |
|    - |  364 | `" }"` |
|    - |  365 | `" public function getReflectionConstants($filter = null){"` |
|    - |  366 | `"  $i = $this->__rinfo();"` |
|    - |  367 | `"  $out = array();"` |
|    - |  368 | `"  foreach($i['consts'] as $k => $c){"` |
|    - |  369 | `"   if($filter !== null){"` |
|    - |  370 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|    - |  371 | `"    if($c['final']){ $m \|= 32; }"` |
|    - |  372 | `"    if(($m & $filter) === 0){ continue; }"` |
|    - |  373 | `"   }"` |
|    - |  374 | `"   $out[] = new ReflectionClassConstant($this->name, $k);"` |
|    - |  375 | `"  }"` |
|    - |  376 | `"  return $out;"` |
|    - |  377 | `" }"` |
|    - |  378 | `" public function getAttributes($name = null, $flags = 0){"` |
|    - |  379 | `"  $i = $this->__rinfo();"` |
|    - |  380 | `"  return __reflect_build_attrs($i['attrs'], array('class', $this->name, null, 0), 1, $name, $flags);"` |
|    - |  381 | `" }"` |
|    - |  382 | `" public function getExtensionName(){ $i = $this->__rinfo(); return $i['internal'] ? 'Core' : false; }"` |
|    - |  383 | `" public function getExtension(){ $i = $this->__rinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|    - |  384 | `" public function newLazyGhost($initializer, $options = 0){"` |
|    - |  385 | `"  throw new Error('ReflectionClass::newLazyGhost() is not supported by PHL (no lazy objects)');"` |
|    - |  386 | `" }"` |
|    - |  387 | `" public function newLazyProxy($factory, $options = 0){"` |
|    - |  388 | `"  throw new Error('ReflectionClass::newLazyProxy() is not supported by PHL (no lazy objects)');"` |
|    - |  389 | `" }"` |
|    - |  390 | `" public function resetAsLazyGhost($object, $initializer, $options = 0){"` |
|    - |  391 | `"  throw new Error('ReflectionClass::resetAsLazyGhost() is not supported by PHL (no lazy objects)');"` |
|    - |  392 | `" }"` |
|    - |  393 | `" public function resetAsLazyProxy($object, $factory, $options = 0){"` |
|    - |  394 | `"  throw new Error('ReflectionClass::resetAsLazyProxy() is not supported by PHL (no lazy objects)');"` |
|    - |  395 | `" }"` |
|    - |  396 | `" public function getLazyInitializer($object){ return null; }"` |
|    - |  397 | `" public function initializeLazyObject($object){ return $object; }"` |
|    - |  398 | `" public function markLazyObjectAsInitialized($object){ return $object; }"` |
|    - |  399 | `" public function isUninitializedLazyObject($object){ return false; }"` |
|    - |  400 | `" public function __toString(){ return __reflect_export_class($this); }"` |
|    - |  401 | `"}"` |
|    - |  402 | `"class ReflectionObject extends ReflectionClass {"` |
|    - |  403 | `" public function __construct($object){"` |
|    - |  404 | `"  if(!is_object($object)){"` |
|    - |  405 | `"   throw new TypeError('ReflectionObject::__construct(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|    - |  406 | `"  }"` |
|    - |  407 | `"  parent::__construct($object);"` |
|    - |  408 | `"  $this->__obj = $object;"` |
|    - |  409 | `" }"` |
|    - |  410 | `"}"` |
|    - |  411 | `;` |
|    - |  412 | `/*` |
|    - |  413 | ` * Chunk 2: ReflectionFunctionAbstract, ReflectionFunction, ReflectionMethod,` |
|    - |  414 | ` * ReflectionParameter.` |
|    - |  415 | ` */` |
|    - |  416 | `static const char zReflectLib2[] =` |
|    - |  417 | `"abstract class ReflectionFunctionAbstract implements Reflector {"` |
|    - |  418 | `" public $name;"` |
|    - |  419 | `" protected $__cl = null;"` |
|    - |  420 | `" protected function __rfinfo(){"` |
|    - |  421 | `"  if($this->__cl !== null){ return __reflect_sig_fixup(__reflect_func_info($this->__cl)); }"` |
|    - |  422 | `"  return __reflect_sig_fixup(__reflect_func_info($this->name));"` |
|    - |  423 | `" }"` |
|    - |  424 | `" protected function __rftarget(){ return $this->__cl !== null ? $this->__cl : $this->name; }"` |
|    - |  425 | `" protected function __rpspec(){ return $this->__rftarget(); }"` |
|    - |  426 | `" public function getName(){ return $this->name; }"` |
|    - |  427 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|    - |  428 | `" public function getNamespaceName(){"` |
|    - |  429 | `"  $p = strrpos($this->name,'\\\\');"` |
|    - |  430 | `"  if($p === false){ return ''; }"` |
|    - |  431 | `"  return substr($this->name,0,$p);"` |
|    - |  432 | `" }"` |
|    - |  433 | `" public function getShortName(){"` |
|    - |  434 | `"  $p = strrpos($this->name,'\\\\');"` |
|    - |  435 | `"  if($p === false){ return $this->name; }"` |
|    - |  436 | `"  return substr($this->name,$p+1);"` |
|    - |  437 | `" }"` |
|    - |  438 | `" public function isClosure(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|    - |  439 | `" public function isGenerator(){ $i = $this->__rfinfo(); return $i['generator']; }"` |
|    - |  440 | `" public function isVariadic(){ $i = $this->__rfinfo(); return $i['variadic']; }"` |
|    - |  441 | `" public function returnsReference(){ $i = $this->__rfinfo(); return $i['byref']; }"` |
|    - |  442 | `" public function isInternal(){ $i = $this->__rfinfo(); return $i['internal']; }"` |
|    - |  443 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|    - |  444 | `" public function isDeprecated(){ $i = $this->__rfinfo(); return __reflect_has_deprecated($i['attrs']); }"` |
|    - |  445 | `" public function isStatic(){ $i = $this->__rfinfo(); return $i['fstatic']; }"` |
|    - |  446 | `" public function getFileName(){ $i = $this->__rfinfo(); return $i['file']; }"` |
|    - |  447 | `" public function getStartLine(){"` |
|    - |  448 | `"  $i = $this->__rfinfo();"` |
|    - |  449 | `"  if($i['internal']){ return false; }"` |
|    - |  450 | `"  return $i['line'];"` |
|    - |  451 | `" }"` |
|    - |  452 | `" public function getEndLine(){"` |
|    - |  453 | `"  $i = $this->__rfinfo();"` |
|    - |  454 | `"  if($i['internal']){ return false; }"` |
|    - |  455 | `"  return $i['endline'];"` |
|    - |  456 | `" }"` |
|    - |  457 | `" public function getDocComment(){ $i = $this->__rfinfo(); return $i['doc']; }"` |
|    - |  458 | `" public function hasReturnType(){ $i = $this->__rfinfo(); return $i['rettext'] !== null; }"` |
|    - |  459 | `" public function getReturnType(){ $i = $this->__rfinfo(); return __reflect_make_type($i['rettext']); }"` |
|    - |  460 | `" public function hasTentativeReturnType(){ return false; }"` |
|    - |  461 | `" public function getTentativeReturnType(){ return null; }"` |
|    - |  462 | `" public function getNumberOfParameters(){"` |
|    - |  463 | `"  $i = $this->__rfinfo();"` |
|    - |  464 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|    - |  465 | `"  return count($i['params']);"` |
|    - |  466 | `" }"` |
|    - |  467 | `" public function getNumberOfRequiredParameters(){"` |
|    - |  468 | `"  $i = $this->__rfinfo();"` |
|    - |  469 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|    - |  470 | `"  $req = 0;"` |
|    - |  471 | `"  $n = count($i['params']);"` |
|    - |  472 | `"  for($k = $n - 1; $k >= 0; $k--){"` |
|    - |  473 | `"   $p = $i['params'][$k];"` |
|    - |  474 | `"   if(!$p['variadic'] && !$p['hasdef']){ $req = $k + 1; break; }"` |
|    - |  475 | `"  }"` |
|    - |  476 | `"  return $req;"` |
|    - |  477 | `" }"` |
|    - |  478 | `" public function getParameters(){"` |
|    - |  479 | `"  $i = $this->__rfinfo();"` |
|    - |  480 | `"  $out = array();"` |
|    - |  481 | `"  $spec = $this->__rpspec();"` |
|    - |  482 | `"  foreach($i['params'] as $p){"` |
|    - |  483 | `"   $out[] = new ReflectionParameter($spec, $p['pos']);"` |
|    - |  484 | `"  }"` |
|    - |  485 | `"  return $out;"` |
|    - |  486 | `" }"` |
|    - |  487 | `" public function getStaticVariables(){ $i = $this->__rfinfo(); return $i['statics']; }"` |
|    - |  488 | `" public function getClosureThis(){"` |
|    - |  489 | `"  $i = $this->__rfinfo();"` |
|    - |  490 | `"  return isset($i['this']) ? $i['this'] : null;"` |
|    - |  491 | `" }"` |
|    - |  492 | `" public function getClosureScopeClass(){"` |
|    - |  493 | `"  $i = $this->__rfinfo();"` |
|    - |  494 | `"  if(isset($i['scope'])){ return new ReflectionClass($i['scope']); }"` |
|    - |  495 | `"  if(isset($i['this'])){ return new ReflectionClass(get_class($i['this'])); }"` |
|    - |  496 | `"  return null;"` |
|    - |  497 | `" }"` |
|    - |  498 | `" public function getClosureCalledClass(){ return $this->getClosureScopeClass(); }"` |
|    - |  499 | `" public function getClosureUsedVariables(){"` |
|    - |  500 | `"  $i = $this->__rfinfo();"` |
|    - |  501 | `"  return isset($i['used']) ? $i['used'] : array();"` |
|    - |  502 | `" }"` |
|    - |  503 | `" public function getExtensionName(){ $i = $this->__rfinfo(); return $i['internal'] ? 'Core' : false; }"` |
|    - |  504 | `" public function getExtension(){ $i = $this->__rfinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|    - |  505 | `" public function getAttributes($name = null, $flags = 0){"` |
|    - |  506 | `"  $i = $this->__rfinfo();"` |
|    - |  507 | `"  if($this instanceof ReflectionMethod){"` |
|    - |  508 | `"   $spec = array('method', $this->class, $this->name, 0);"` |
|    - |  509 | `"   $target = 4;"` |
|    - |  510 | `"  }else{"` |
|    - |  511 | `"   $spec = array('fn', $this->__rftarget(), null, 0);"` |
|    - |  512 | `"   $target = 2;"` |
|    - |  513 | `"  }"` |
|    - |  514 | `"  return __reflect_build_attrs($i['attrs'], $spec, $target, $name, $flags);"` |
|    - |  515 | `" }"` |
|    - |  516 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|    - |  517 | `"}"` |
|    - |  518 | `"class ReflectionFunction extends ReflectionFunctionAbstract {"` |
|    - |  519 | `" const IS_DEPRECATED = 2048;"` |
|    - |  520 | `" public function __construct($function){"` |
|    - |  521 | `"  if($function instanceof Closure){"` |
|    - |  522 | `"   $this->__cl = $function;"` |
|    - |  523 | `"   $i = $this->__rfinfo();"` |
|    - |  524 | `"   if($i['closure']){"` |
|    - |  525 | `"    $f = $i['file'] === false ? '' : $i['file'];"` |
|    - |  526 | `"    $this->name = '{closure:'.$f.':'.$i['line'].'}';"` |
|    - |  527 | `"   }else{"` |
|    - |  528 | `"    $this->name = $i['name'];"` |
|    - |  529 | `"   }"` |
|    - |  530 | `"   return;"` |
|    - |  531 | `"  }"` |
|    - |  532 | `"  if(!is_string($function)){"` |
|    - |  533 | `"   throw new TypeError('ReflectionFunction::__construct(): Argument #1 ($function) must be of type Closure\|string, '.get_debug_type($function).' given');"` |
|    - |  534 | `"  }"` |
|    - |  535 | `"  $i = __reflect_func_info($function);"` |
|    - |  536 | `"  if($i === null){"` |
|    - |  537 | `"   throw new ReflectionException('Function '.$function.'() does not exist');"` |
|    - |  538 | `"  }"` |
|    - |  539 | `"  if($i['closure']){"` |
|    - |  540 | `"   $this->name = '{closure:'.($i['file'] === false ? '' : $i['file']).':'.$i['line'].'}';"` |
|    - |  541 | `"   $this->__cl = __reflect_closure($function, null, null);"` |
|    - |  542 | `"  }else{"` |
|    - |  543 | `"   $this->name = $i['name'];"` |
|    - |  544 | `"  }"` |
|    - |  545 | `" }"` |
|    - |  546 | `" public function invoke(...$args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|    - |  547 | `" public function invokeArgs(array $args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|    - |  548 | `" public function getClosure(){"` |
|    - |  549 | `"  if($this->__cl !== null){ return $this->__cl; }"` |
|    - |  550 | `"  return __reflect_closure($this->name, null, null);"` |
|    - |  551 | `" }"` |
|    - |  552 | `" public function isAnonymous(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|    - |  553 | `" public function isDisabled(){ return false; }"` |
|    - |  554 | `"}"` |
|    - |  555 | `"class ReflectionMethod extends ReflectionFunctionAbstract {"` |
|    - |  556 | `" const IS_PUBLIC = 1;"` |
|    - |  557 | `" const IS_PROTECTED = 2;"` |
|    - |  558 | `" const IS_PRIVATE = 4;"` |
|    - |  559 | `" const IS_STATIC = 16;"` |
|    - |  560 | `" const IS_FINAL = 32;"` |
|    - |  561 | `" const IS_ABSTRACT = 64;"` |
|    - |  562 | `" public $class;"` |
|    - |  563 | `" public function __construct($objectOrMethod, $method = null){"` |
|    - |  564 | `"  if($method === null){"` |
|    - |  565 | `"   if(!is_string($objectOrMethod) \|\| strpos($objectOrMethod,'::') === false){"` |
|    - |  566 | `"    throw new TypeError('ReflectionMethod::__construct(): Argument #1 ($objectOrMethod) must be of type object\|string, '.get_debug_type($objectOrMethod).' given');"` |
|    - |  567 | `"   }"` |
|    - |  568 | `"   $p = strpos($objectOrMethod,'::');"` |
|    - |  569 | `"   $method = substr($objectOrMethod,$p+2);"` |
|    - |  570 | `"   $objectOrMethod = substr($objectOrMethod,0,$p);"` |
|    - |  571 | `"  }"` |
|    - |  572 | `"  $ci = __phl_rcinfo($objectOrMethod);"` |
|    - |  573 | `"  if($ci === null){"` |
|    - |  574 | `"   throw new ReflectionException('Class \"'.$objectOrMethod.'\" does not exist');"` |
|    - |  575 | `"  }"` |
|    - |  576 | `"  $this->class = $ci['name'];"` |
|    - |  577 | `"  $found = null;"` |
|    - |  578 | `"  if(isset($ci['methods'][$method])){"` |
|    - |  579 | `"   $found = $method;"` |
|    - |  580 | `"  }else{"` |
|    - |  581 | `"   $l = strtolower($method);"` |
|    - |  582 | `"   foreach($ci['methods'] as $k => $m){"` |
|    - |  583 | `"    if(strtolower($k) === $l){ $found = $k; break; }"` |
|    - |  584 | `"   }"` |
|    - |  585 | `"  }"` |
|    - |  586 | `"  if($found === null){"` |
|    - |  587 | `"   throw new ReflectionException('Method '.$this->class.'::'.$method.'() does not exist');"` |
|    - |  588 | `"  }"` |
|    - |  589 | `"  $this->name = $found;"` |
|    - |  590 | `" }"` |
|    - |  591 | `" public static function createFromMethodName($name){"` |
|    - |  592 | `"  return new ReflectionMethod($name);"` |
|    - |  593 | `" }"` |
|    - |  594 | `" protected function __rfinfo(){ return __reflect_func_info($this->class, $this->name); }"` |
|    - |  595 | `" protected function __rpspec(){ return array($this->class, $this->name); }"` |
|    - |  596 | `" public function getDeclaringClass(){"` |
|    - |  597 | `"  $i = $this->__rfinfo();"` |
|    - |  598 | `"  return new ReflectionClass($i['decl']);"` |
|    - |  599 | `" }"` |
|    - |  600 | `" public function getModifiers(){"` |
|    - |  601 | `"  $i = $this->__rfinfo();"` |
|    - |  602 | `"  $m = ($i['vis'] === 1 ? 1 : ($i['vis'] === 2 ? 2 : 4));"` |
|    - |  603 | `"  if($i['mstatic']){ $m \|= 16; }"` |
|    - |  604 | `"  if($i['abstract']){ $m \|= 64; }"` |
|    - |  605 | `"  if($i['final']){ $m \|= 32; }"` |
|    - |  606 | `"  return $m;"` |
|    - |  607 | `" }"` |
|    - |  608 | `" public function isPublic(){ $i = $this->__rfinfo(); return $i['vis'] === 1; }"` |
|    - |  609 | `" public function isProtected(){ $i = $this->__rfinfo(); return $i['vis'] === 2; }"` |
|    - |  610 | `" public function isPrivate(){ $i = $this->__rfinfo(); return $i['vis'] === 3; }"` |
|    - |  611 | `" public function isStatic(){ $i = $this->__rfinfo(); return $i['mstatic']; }"` |
|    - |  612 | `" public function isAbstract(){ $i = $this->__rfinfo(); return $i['abstract']; }"` |
|    - |  613 | `" public function isFinal(){ $i = $this->__rfinfo(); return $i['final']; }"` |
|    - |  614 | `" public function isConstructor(){ return strtolower($this->name) === '__construct'; }"` |
|    - |  615 | `" public function isDestructor(){ return strtolower($this->name) === '__destruct'; }"` |
|    - |  616 | `" public function invoke($object = null, ...$args){ return $this->__rinvoke($object, $args); }"` |
|    - |  617 | `" public function invokeArgs($object, array $args){ return $this->__rinvoke($object, $args); }"` |
|    - |  618 | `" protected function __rinvoke($object, $args){"` |
|    - |  619 | `"  $i = $this->__rfinfo();"` |
|    - |  620 | `"  if(!$i['mstatic']){"` |
|    - |  621 | `"   if(!is_object($object)){"` |
|    - |  622 | `"    throw new ReflectionException('Trying to invoke non static method '.$this->class.'::'.$this->name.'() without an object');"` |
|    - |  623 | `"   }"` |
|    - |  624 | `"   if(!is_a($object, $i['decl'])){"` |
|    - |  625 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|    - |  626 | `"   }"` |
|    - |  627 | `"  }else{"` |
|    - |  628 | `"   $object = null;"` |
|    - |  629 | `"  }"` |
|    - |  630 | `"  return __reflect_invoke($this->class, $this->name, $object, $args);"` |
|    - |  631 | `" }"` |
|    - |  632 | `" public function getClosure($object = null){"` |
|    - |  633 | `"  $i = $this->__rfinfo();"` |
|    - |  634 | `"  if(!$i['mstatic']){"` |
|    - |  635 | `"   if($object === null){"` |
|    - |  636 | `"    throw new ValueError('ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods');"` |
|    - |  637 | `"   }"` |
|    - |  638 | `"   if(!is_a($object, $i['decl'])){"` |
|    - |  639 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|    - |  640 | `"   }"` |
|    - |  641 | `"  }else{"` |
|    - |  642 | `"   $object = null;"` |
|    - |  643 | `"  }"` |
|    - |  644 | `"  return __reflect_closure($this->class, $this->name, $object);"` |
|    - |  645 | `" }"` |
|    - |  646 | `" public function setAccessible($accessible){ }"` |
|    - |  647 | `" public function hasPrototype(){ return $this->__rproto() !== null; }"` |
|    - |  648 | `" public function getPrototype(){"` |
|    - |  649 | `"  $p = $this->__rproto();"` |
|    - |  650 | `"  if($p === null){"` |
|    - |  651 | `"   throw new ReflectionException('Method '.$this->class.'::'.$this->name.' does not have a prototype');"` |
|    - |  652 | `"  }"` |
|    - |  653 | `"  return new ReflectionMethod($p, $this->name);"` |
|    - |  654 | `" }"` |
|    - |  655 | `" protected function __rproto(){"` |
|    - |  656 | `"  $ci = __phl_rcinfo($this->class);"` |
|    - |  657 | `"  $l = strtolower($this->name);"` |
|    - |  658 | `"  $p = $ci['parent'];"` |
|    - |  659 | `"  while($p !== null){"` |
|    - |  660 | `"   $pi = __phl_rcinfo($p);"` |
|    - |  661 | `"   foreach($pi['methods'] as $k => $m){"` |
|    - |  662 | `"    if(strtolower($k) === $l && $m['vis'] !== 3){ return $m['decl']; }"` |
|    - |  663 | `"   }"` |
|    - |  664 | `"   $p = $pi['parent'];"` |
|    - |  665 | `"  }"` |
|    - |  666 | `"  foreach($ci['interfaces'] as $if){"` |
|    - |  667 | `"   $ii = __phl_rcinfo($if);"` |
|    - |  668 | `"   foreach($ii['methods'] as $k => $m){"` |
|    - |  669 | `"    if(strtolower($k) === $l){ return $ii['name']; }"` |
|    - |  670 | `"   }"` |
|    - |  671 | `"  }"` |
|    - |  672 | `"  return null;"` |
|    - |  673 | `" }"` |
|    - |  674 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|    - |  675 | `"}"` |
|    - |  676 | `"class ReflectionParameter implements Reflector {"` |
|    - |  677 | `" public $name;"` |
|    - |  678 | `" protected $__t;"` |
|    - |  679 | `" protected $__m = null;"` |
|    - |  680 | `" protected $__p = 0;"` |
|    - |  681 | `" public function __construct($function, $param){"` |
|    - |  682 | `"  $m = null;"` |
|    - |  683 | `"  $t = $function;"` |
|    - |  684 | `"  if(is_array($function)){"` |
|    - |  685 | `"   $t = $function[0];"` |
|    - |  686 | `"   $m = $function[1];"` |
|    - |  687 | `"   if(is_object($t)){ $t = get_class($t); }"` |
|    - |  688 | `"  }else if(is_string($function) && strpos($function,'::') !== false){"` |
|    - |  689 | `"   $p = strpos($function,'::');"` |
|    - |  690 | `"   $m = substr($function,$p+2);"` |
|    - |  691 | `"   $t = substr($function,0,$p);"` |
|    - |  692 | `"  }"` |
|    - |  693 | `"  if($m !== null){"` |
|    - |  694 | `"   $rm = new ReflectionMethod($t, $m);"` |
|    - |  695 | `"   $t = $rm->class;"` |
|    - |  696 | `"   $m = $rm->name;"` |
|    - |  697 | `"   $i = __reflect_func_info($t, $m);"` |
|    - |  698 | `"  }else if($function instanceof Closure){"` |
|    - |  699 | `"   $t = $function;"` |
|    - |  700 | `"   $i = __reflect_func_info($function);"` |
|    - |  701 | `"  }else{"` |
|    - |  702 | `"   $i = __reflect_sig_fixup(__reflect_func_info($t));"` |
|    - |  703 | `"   if($i === null){"` |
|    - |  704 | `"    throw new ReflectionException('Function '.$t.'() does not exist');"` |
|    - |  705 | `"   }"` |
|    - |  706 | `"  }"` |
|    - |  707 | `"  $found = null;"` |
|    - |  708 | `"  if(is_int($param)){"` |
|    - |  709 | `"   if(isset($i['params'][$param])){ $found = $i['params'][$param]; }"` |
|    - |  710 | `"   if($found === null){"` |
|    - |  711 | `"    throw new ReflectionException('The parameter specified by its offset could not be found');"` |
|    - |  712 | `"   }"` |
|    - |  713 | `"  }else{"` |
|    - |  714 | `"   foreach($i['params'] as $pp){"` |
|    - |  715 | `"    if($pp['name'] === $param){ $found = $pp; break; }"` |
|    - |  716 | `"   }"` |
|    - |  717 | `"   if($found === null){"` |
|    - |  718 | `"    throw new ReflectionException('The parameter specified by its name could not be found');"` |
|    - |  719 | `"   }"` |
|    - |  720 | `"  }"` |
|    - |  721 | `"  $this->name = $found['name'];"` |
|    - |  722 | `"  $this->__t = $t;"` |
|    - |  723 | `"  $this->__m = $m;"` |
|    - |  724 | `"  $this->__p = $found['pos'];"` |
|    - |  725 | `" }"` |
|    - |  726 | `" protected function __rffull(){"` |
|    - |  727 | `"  if($this->__m !== null){ return __reflect_func_info($this->__t, $this->__m); }"` |
|    - |  728 | `"  return __reflect_sig_fixup(__reflect_func_info($this->__t));"` |
|    - |  729 | `" }"` |
|    - |  730 | `" protected function __rpinfo(){"` |
|    - |  731 | `"  $i = $this->__rffull();"` |
|    - |  732 | `"  return $i['params'][$this->__p];"` |
|    - |  733 | `" }"` |
|    - |  734 | `" public function getName(){ return $this->name; }"` |
|    - |  735 | `" public function getPosition(){ return $this->__p; }"` |
|    - |  736 | `" public function isPassedByReference(){ $p = $this->__rpinfo(); return $p['byref']; }"` |
|    - |  737 | `" public function canBePassedByValue(){ return !$this->isPassedByReference(); }"` |
|    - |  738 | `" public function isVariadic(){ $p = $this->__rpinfo(); return $p['variadic']; }"` |
|    - |  739 | `" public function isPromoted(){ $p = $this->__rpinfo(); return $p['promoted']; }"` |
|    - |  740 | `" public function isDefaultValueAvailable(){ $p = $this->__rpinfo(); return $p['hasdef']; }"` |
|    - |  741 | `" public function isOptional(){"` |
|    - |  742 | `"  $i = $this->__rffull();"` |
|    - |  743 | `"  $n = count($i['params']);"` |
|    - |  744 | `"  for($k = $this->__p; $k < $n; $k++){"` |
|    - |  745 | `"   $p = $i['params'][$k];"` |
|    - |  746 | `"   if(!$p['variadic'] && !$p['hasdef']){ return false; }"` |
|    - |  747 | `"  }"` |
|    - |  748 | `"  return true;"` |
|    - |  749 | `" }"` |
|    - |  750 | `" public function getDefaultValue(){"` |
|    - |  751 | `"  if(!$this->isDefaultValueAvailable()){"` |
|    - |  752 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|    - |  753 | `"  }"` |
|    - |  754 | `"  $p = $this->__rpinfo();"` |
|    - |  755 | `"  if(isset($p['deftext'])){"` |
|    - |  756 | `"   $s = __reflect_sig_scalar($p['deftext']);"` |
|    - |  757 | `"   if($s[0]){ return $s[1]; }"` |
|    - |  758 | `"   if($p['deftext'] === 'array (' \|\| strpos($p['deftext'], '[') === 0){ return array(); }"` |
|    - |  759 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|    - |  760 | `"  }"` |
|    - |  761 | `"  return __reflect_param_default($this->__t, $this->__m, $this->__p);"` |
|    - |  762 | `" }"` |
|    - |  763 | `" public function isDefaultValueConstant(){"` |
|    - |  764 | `"  if(!$this->isDefaultValueAvailable()){ return false; }"` |
|    - |  765 | `"  $p = $this->__rpinfo();"` |
|    - |  766 | `"  if(isset($p['deftext'])){ return false; }"` |
|    - |  767 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p) !== null;"` |
|    - |  768 | `" }"` |
|    - |  769 | `" public function getDefaultValueConstantName(){"` |
|    - |  770 | `"  if(!$this->isDefaultValueAvailable()){"` |
|    - |  771 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|    - |  772 | `"  }"` |
|    - |  773 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p);"` |
|    - |  774 | `" }"` |
|    - |  775 | `" public function allowsNull(){"` |
|    - |  776 | `"  $p = $this->__rpinfo();"` |
|    - |  777 | `"  if($p['typetext'] === null){ return true; }"` |
|    - |  778 | `"  if($p['nullable']){ return true; }"` |
|    - |  779 | `"  return $p['typetext'] === 'mixed' \|\| $p['typetext'] === 'null';"` |
|    - |  780 | `" }"` |
|    - |  781 | `" public function hasType(){ $p = $this->__rpinfo(); return $p['typetext'] !== null; }"` |
|    - |  782 | `" public function getType(){ $p = $this->__rpinfo(); return __reflect_make_type($p['typetext']); }"` |
|    - |  783 | `" public function getDeclaringFunction(){"` |
|    - |  784 | `"  if($this->__m !== null){ return new ReflectionMethod($this->__t, $this->__m); }"` |
|    - |  785 | `"  return new ReflectionFunction($this->__t);"` |
|    - |  786 | `" }"` |
|    - |  787 | `" public function getDeclaringClass(){"` |
|    - |  788 | `"  if($this->__m === null){ return null; }"` |
|    - |  789 | `"  $i = $this->__rffull();"` |
|    - |  790 | `"  return new ReflectionClass($i['decl']);"` |
|    - |  791 | `" }"` |
|    - |  792 | `" public function getAttributes($name = null, $flags = 0){"` |
|    - |  793 | `"  $p = $this->__rpinfo();"` |
|    - |  794 | `"  return __reflect_build_attrs($p['attrs'], array('param', $this->__t, $this->__m, $this->__p), 32, $name, $flags);"` |
|    - |  795 | `" }"` |
|    - |  796 | `" public function __toString(){ return __reflect_export_param($this); }"` |
|    - |  797 | `"}"` |
|    - |  798 | `;` |
|    - |  799 | `/*` |
|    - |  800 | ` * Chunk 3: PropertyHookType, ReflectionProperty, ReflectionClassConstant.` |
|    - |  801 | ` */` |
|    - |  802 | `static const char zReflectLib3[] =` |
|    - |  803 | `"enum PropertyHookType: string {"` |
|    - |  804 | `" case Get = 'get';"` |
|    - |  805 | `" case Set = 'set';"` |
|    - |  806 | `"}"` |
|    - |  807 | `"class ReflectionProperty implements Reflector {"` |
|    - |  808 | `" const IS_PUBLIC = 1;"` |
|    - |  809 | `" const IS_PROTECTED = 2;"` |
|    - |  810 | `" const IS_PRIVATE = 4;"` |
|    - |  811 | `" const IS_STATIC = 16;"` |
|    - |  812 | `" const IS_FINAL = 32;"` |
|    - |  813 | `" const IS_ABSTRACT = 64;"` |
|    - |  814 | `" const IS_READONLY = 128;"` |
|    - |  815 | `" const IS_VIRTUAL = 512;"` |
|    - |  816 | `" const IS_PROTECTED_SET = 2048;"` |
|    - |  817 | `" const IS_PRIVATE_SET = 4096;"` |
|    - |  818 | `" public $name;"` |
|    - |  819 | `" public $class;"` |
|    - |  820 | `" protected $__dynobj = null;"` |
|    - |  821 | `" public function __construct($class, $property){"` |
|    - |  822 | `"  $obj = null;"` |
|    - |  823 | `"  if(is_object($class)){ $obj = $class; }"` |
|    - |  824 | `"  else if(!is_string($class)){"` |
|    - |  825 | `"   throw new TypeError('ReflectionProperty::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|    - |  826 | `"  }"` |
|    - |  827 | `"  $ci = __phl_rcinfo($class);"` |
|    - |  828 | `"  if($ci === null){"` |
|    - |  829 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|    - |  830 | `"  }"` |
|    - |  831 | `"  $this->class = $ci['name'];"` |
|    - |  832 | `"  if(isset($ci['props'][$property])){"` |
|    - |  833 | `"   $this->name = $property;"` |
|    - |  834 | `"   return;"` |
|    - |  835 | `"  }"` |
|    - |  836 | `"  if($obj !== null && (__reflect_prop_state($obj, $property) & 1)){"` |
|    - |  837 | `"   $this->name = $property;"` |
|    - |  838 | `"   $this->__dynobj = $obj;"` |
|    - |  839 | `"   return;"` |
|    - |  840 | `"  }"` |
|    - |  841 | `"  throw new ReflectionException('Property '.$this->class.'::$'.$property.' does not exist');"` |
|    - |  842 | `" }"` |
|    - |  843 | `" protected function __rpmeta(){"` |
|    - |  844 | `"  $ci = __phl_rcinfo($this->class);"` |
|    - |  845 | `"  if(isset($ci['props'][$this->name])){ return $ci['props'][$this->name]; }"` |
|    - |  846 | `"  return array('vis' => 1, 'static' => false, 'readonly' => false, 'hasdef' => false,"` |
|    - |  847 | `"   'typed' => false, 'typetext' => null, 'decl' => $this->class, 'line' => 0, 'dyn' => true);"` |
|    - |  848 | `" }"` |
|    - |  849 | `" public function getName(){ return $this->name; }"` |
|    - |  850 | `" public function getDeclaringClass(){"` |
|    - |  851 | `"  $m = $this->__rpmeta();"` |
|    - |  852 | `"  return new ReflectionClass($m['decl']);"` |
|    - |  853 | `" }"` |
|    - |  854 | `" public function getModifiers(){"` |
|    - |  855 | `"  $m = $this->__rpmeta();"` |
|    - |  856 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|    - |  857 | `"  if($m['static']){ $mod \|= 16; }"` |
|    - |  858 | `"  if($m['readonly']){ $mod \|= 128; }"` |
|    - |  859 | `"  return $mod;"` |
|    - |  860 | `" }"` |
|    - |  861 | `" public function isPublic(){ $m = $this->__rpmeta(); return $m['vis'] === 1; }"` |
|    - |  862 | `" public function isProtected(){ $m = $this->__rpmeta(); return $m['vis'] === 2; }"` |
|    - |  863 | `" public function isPrivate(){ $m = $this->__rpmeta(); return $m['vis'] === 3; }"` |
|    - |  864 | `" public function isStatic(){ $m = $this->__rpmeta(); return $m['static']; }"` |
|    - |  865 | `" public function isReadOnly(){ $m = $this->__rpmeta(); return $m['readonly']; }"` |
|    - |  866 | `" public function isPrivateSet(){ $m = $this->__rpmeta(); return isset($m['privset']) ? $m['privset'] : false; }"` |
|    - |  867 | `" public function isProtectedSet(){ $m = $this->__rpmeta(); return isset($m['protset']) ? $m['protset'] : false; }"` |
|    - |  868 | `" public function isDefault(){ $m = $this->__rpmeta(); return !isset($m['dyn']); }"` |
|    - |  869 | `" public function isDynamic(){ $m = $this->__rpmeta(); return isset($m['dyn']); }"` |
|    - |  870 | `" public function isAbstract(){ return false; }"` |
|    - |  871 | `" public function isFinal(){ return false; }"` |
|    - |  872 | `" public function isVirtual(){ $m = $this->__rpmeta(); return isset($m['virtual']) ? $m['virtual'] : false; }"` |
|    - |  873 | `" public function hasHooks(){ $m = $this->__rpmeta();"` |
|    - |  874 | `"  return (isset($m['hookget']) && $m['hookget']) \|\| (isset($m['hookset']) && $m['hookset']); }"` |
|    - |  875 | `" public function getHooks(){"` |
|    - |  876 | `"  $m = $this->__rpmeta(); $h = array();"` |
|    - |  877 | `"  if(isset($m['hookget']) && $m['hookget']){ $h['get'] = new ReflectionMethod($m['decl'], '__phl_hook_get_'.$this->name); }"` |
|    - |  878 | `"  if(isset($m['hookset']) && $m['hookset']){ $h['set'] = new ReflectionMethod($m['decl'], '__phl_hook_set_'.$this->name); }"` |
|    - |  879 | `"  return $h; }"` |
|    - |  880 | `" public function hasHook($type){"` |
|    - |  881 | `"  $t = $type instanceof PropertyHookType ? $type->value : $type;"` |
|    - |  882 | `"  $m = $this->__rpmeta();"` |
|    - |  883 | `"  if($t === 'get'){ return isset($m['hookget']) && $m['hookget']; }"` |
|    - |  884 | `"  if($t === 'set'){ return isset($m['hookset']) && $m['hookset']; }"` |
|    - |  885 | `"  return false; }"` |
|    - |  886 | `" public function getHook($type){"` |
|    - |  887 | `"  $t = $type instanceof PropertyHookType ? $type->value : $type;"` |
|    - |  888 | `"  $h = $this->getHooks();"` |
|    - |  889 | `"  return isset($h[$t]) ? $h[$t] : null; }"` |
|    - |  890 | `" public function isLazy($object){ return false; }"` |
|    - |  891 | `" public function setAccessible($accessible){ }"` |
|    - |  892 | `" public function getValue($object = null){"` |
|    - |  893 | `"  $m = $this->__rpmeta();"` |
|    - |  894 | `"  if($m['static']){ return __reflect_static_value($this->class, $this->name); }"` |
|    - |  895 | `"  if(!is_object($object)){"` |
|    - |  896 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|    - |  897 | `"  }"` |
|    - |  898 | `"  return __reflect_prop_read($object, $this->name);"` |
|    - |  899 | `" }"` |
|    - |  900 | `" public function setValue($objectOrValue = null, $value = null){"` |
|    - |  901 | `"  $m = $this->__rpmeta();"` |
|    - |  902 | `"  if($m['static']){"` |
|    - |  903 | `"   if($value === null && $objectOrValue !== null && !is_object($objectOrValue)){"` |
|    - |  904 | `"    __reflect_static_set($this->class, $this->name, $objectOrValue);"` |
|    - |  905 | `"   }else{"` |
|    - |  906 | `"    __reflect_static_set($this->class, $this->name, $value);"` |
|    - |  907 | `"   }"` |
|    - |  908 | `"   return;"` |
|    - |  909 | `"  }"` |
|    - |  910 | `"  __reflect_prop_write($objectOrValue, $this->name, $value);"` |
|    - |  911 | `" }"` |
|    - |  912 | `" public function getRawValue($object){ return $this->getValue($object); }"` |
|    - |  913 | `" public function setRawValue($object, $value){ $this->setValue($object, $value); }"` |
|    - |  914 | `" public function isInitialized($object = null){"` |
|    - |  915 | `"  $m = $this->__rpmeta();"` |
|    - |  916 | `"  if($m['static']){ return (__reflect_prop_state($this->class, $this->name) & 2) !== 0; }"` |
|    - |  917 | `"  if(!is_object($object)){"` |
|    - |  918 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|    - |  919 | `"  }"` |
|    - |  920 | `"  return (__reflect_prop_state($object, $this->name) & 2) !== 0;"` |
|    - |  921 | `" }"` |
|    - |  922 | `" public function hasDefaultValue(){"` |
|    - |  923 | `"  $m = $this->__rpmeta();"` |
|    - |  924 | `"  if(isset($m['dyn'])){ return false; }"` |
|    - |  925 | `"  if($m['hasdef']){ return true; }"` |
|    - |  926 | `"  return !$m['typed'];"` |
|    - |  927 | `" }"` |
|    - |  928 | `" public function getDefaultValue(){"` |
|    - |  929 | `"  $m = $this->__rpmeta();"` |
|    - |  930 | `"  if(isset($m['dyn']) \|\| !$m['hasdef']){ return null; }"` |
|    - |  931 | `"  return __reflect_prop_default($this->class, $this->name);"` |
|    - |  932 | `" }"` |
|    - |  933 | `" public function hasType(){ $m = $this->__rpmeta(); return $m['typed']; }"` |
|    - |  934 | `" public function getType(){ $m = $this->__rpmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|    - |  935 | `" public function getSettableType(){ return $this->getType(); }"` |
|    - |  936 | `" public function setRawValueWithoutLazyInitialization($object, $value){"` |
|    - |  937 | `"  throw new Error('ReflectionProperty::setRawValueWithoutLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|    - |  938 | `" }"` |
|    - |  939 | `" public function skipLazyInitialization($object){"` |
|    - |  940 | `"  throw new Error('ReflectionProperty::skipLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|    - |  941 | `" }"` |
|    - |  942 | `" public function getDocComment(){ $m = $this->__rpmeta(); return isset($m['doc']) ? $m['doc'] : false; }"` |
|    - |  943 | `" public function getAttributes($name = null, $flags = 0){"` |
|    - |  944 | `"  $m = $this->__rpmeta();"` |
|    - |  945 | `"  if(!isset($m['attrs'])){ return array(); }"` |
|    - |  946 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 8, $name, $flags);"` |
|    - |  947 | `" }"` |
|    - |  948 | `" public function __toString(){ return __reflect_export_prop($this); }"` |
|    - |  949 | `"}"` |
|    - |  950 | `"class ReflectionClassConstant implements Reflector {"` |
|    - |  951 | `" const IS_PUBLIC = 1;"` |
|    - |  952 | `" const IS_PROTECTED = 2;"` |
|    - |  953 | `" const IS_PRIVATE = 4;"` |
|    - |  954 | `" const IS_FINAL = 32;"` |
|    - |  955 | `" public $name;"` |
|    - |  956 | `" public $class;"` |
|    - |  957 | `" public function __construct($class, $constant){"` |
|    - |  958 | `"  if(!is_object($class) && !is_string($class)){"` |
|    - |  959 | `"   throw new TypeError('ReflectionClassConstant::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|    - |  960 | `"  }"` |
|    - |  961 | `"  $ci = __phl_rcinfo($class);"` |
|    - |  962 | `"  if($ci === null){"` |
|    - |  963 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|    - |  964 | `"  }"` |
|    - |  965 | `"  $this->class = $ci['name'];"` |
|    - |  966 | `"  if(!isset($ci['consts'][$constant])){"` |
|    - |  967 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' does not exist');"` |
|    - |  968 | `"  }"` |
|    - |  969 | `"  $this->name = $constant;"` |
|    - |  970 | `" }"` |
|    - |  971 | `" protected function __rcmeta(){"` |
|    - |  972 | `"  $ci = __phl_rcinfo($this->class);"` |
|    - |  973 | `"  return $ci['consts'][$this->name];"` |
|    - |  974 | `" }"` |
|    - |  975 | `" public function getName(){ return $this->name; }"` |
|    - |  976 | `" public function getValue(){ return __reflect_const_value($this->class, $this->name); }"` |
|    - |  977 | `" public function getDeclaringClass(){"` |
|    - |  978 | `"  $m = $this->__rcmeta();"` |
|    - |  979 | `"  return new ReflectionClass($m['decl']);"` |
|    - |  980 | `" }"` |
|    - |  981 | `" public function getModifiers(){"` |
|    - |  982 | `"  $m = $this->__rcmeta();"` |
|    - |  983 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|    - |  984 | `"  if($m['final']){ $mod \|= 32; }"` |
|    - |  985 | `"  return $mod;"` |
|    - |  986 | `" }"` |
|    - |  987 | `" public function isPublic(){ $m = $this->__rcmeta(); return $m['vis'] === 1; }"` |
|    - |  988 | `" public function isProtected(){ $m = $this->__rcmeta(); return $m['vis'] === 2; }"` |
|    - |  989 | `" public function isPrivate(){ $m = $this->__rcmeta(); return $m['vis'] === 3; }"` |
|    - |  990 | `" public function isFinal(){ $m = $this->__rcmeta(); return $m['final']; }"` |
|    - |  991 | `" public function isEnumCase(){ $m = $this->__rcmeta(); return $m['enumcase']; }"` |
|    - |  992 | `" public function isDeprecated(){ $m = $this->__rcmeta(); return __reflect_has_deprecated($m['attrs']); }"` |
|    - |  993 | `" public function hasType(){ $m = $this->__rcmeta(); return $m['typed']; }"` |
|    - |  994 | `" public function getType(){ $m = $this->__rcmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|    - |  995 | `" public function getDocComment(){ $m = $this->__rcmeta(); return $m['doc']; }"` |
|    - |  996 | `" public function getAttributes($name = null, $flags = 0){"` |
|    - |  997 | `"  $m = $this->__rcmeta();"` |
|    - |  998 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 16, $name, $flags);"` |
|    - |  999 | `" }"` |
|    - | 1000 | `" public function __toString(){ return __reflect_export_cconst($this); }"` |
|    - | 1001 | `"}"` |
|    - | 1002 | `;` |
|    - | 1003 | `/*` |
|    - | 1004 | ` * Chunk 4: the ReflectionType family, built from the engine's canonical` |
|    - | 1005 | ` * type text ("?int", "string\|float", "(A&B)\|C" — normalized at compile` |
|    - | 1006 | ` * time). __reflect_make_type is the internal factory; PHP itself never` |
|    - | 1007 | ` * lets user code construct these, so the public constructors here are a` |
|    - | 1008 | ` * recorded PHL-only surface.` |
|    - | 1009 | ` */` |
|    - | 1010 | `static const char zReflectLib4[] =` |
|    - | 1011 | `"abstract class ReflectionType implements Stringable {"` |
|    - | 1012 | `" protected $__text = '';"` |
|    - | 1013 | `" protected $__nullable = false;"` |
|    - | 1014 | `" public function allowsNull(){ return $this->__nullable; }"` |
|    - | 1015 | `" public function __toString(){ return $this->__text; }"` |
|    - | 1016 | `"}"` |
|    - | 1017 | `"class ReflectionNamedType extends ReflectionType {"` |
|    - | 1018 | `" protected $__tname = '';"` |
|    - | 1019 | `" public function __construct($name = '', $nullable = false, $text = null){"` |
|    - | 1020 | `"  $this->__tname = $name;"` |
|    - | 1021 | `"  $l = strtolower($name);"` |
|    - | 1022 | `"  $this->__nullable = $nullable \|\| $l === 'null' \|\| $l === 'mixed';"` |
|    - | 1023 | `"  $this->__text = $text === null ? $name : $text;"` |
|    - | 1024 | `" }"` |
|    - | 1025 | `" public function getName(){ return $this->__tname; }"` |
|    - | 1026 | `" public function isBuiltin(){"` |
|    - | 1027 | `"  $l = strtolower($this->__tname);"` |
|    - | 1028 | `"  return in_array($l, array('int','float','string','bool','array','object','mixed',"` |
|    - | 1029 | `"   'void','never','null','callable','iterable','true','false'), true);"` |
|    - | 1030 | `" }"` |
|    - | 1031 | `"}"` |
|    - | 1032 | `"class ReflectionUnionType extends ReflectionType {"` |
|    - | 1033 | `" protected $__types = array();"` |
|    - | 1034 | `" public function __construct($text = '', $nullable = false, $types = array()){"` |
|    - | 1035 | `"  $this->__text = $text;"` |
|    - | 1036 | `"  $this->__nullable = $nullable;"` |
|    - | 1037 | `"  $this->__types = $types;"` |
|    - | 1038 | `" }"` |
|    - | 1039 | `" public function getTypes(){ return $this->__types; }"` |
|    - | 1040 | `"}"` |
|    - | 1041 | `"class ReflectionIntersectionType extends ReflectionType {"` |
|    - | 1042 | `" protected $__types = array();"` |
|    - | 1043 | `" public function __construct($text = '', $types = array()){"` |
|    - | 1044 | `"  $this->__text = $text;"` |
|    - | 1045 | `"  $this->__nullable = false;"` |
|    - | 1046 | `"  $this->__types = $types;"` |
|    - | 1047 | `" }"` |
|    - | 1048 | `" public function getTypes(){ return $this->__types; }"` |
|    - | 1049 | `"}"` |
|    - | 1050 | `"function __reflect_make_atom($p){"` |
|    - | 1051 | `" $nullable = false;"` |
|    - | 1052 | `" if($p !== '' && $p[0] === '?'){ $nullable = true; $p = substr($p, 1); }"` |
|    - | 1053 | `" if($p !== '' && $p[0] === '('){ $p = substr($p, 1, strlen($p) - 2); }"` |
|    - | 1054 | `" if(strpos($p, '&') !== false){"` |
|    - | 1055 | `"  $subs = array();"` |
|    - | 1056 | `"  foreach(explode('&', $p) as $s){ $subs[] = new ReflectionNamedType($s, false, $s); }"` |
|    - | 1057 | `"  return new ReflectionIntersectionType($p, $subs);"` |
|    - | 1058 | `" }"` |
|    - | 1059 | `" return new ReflectionNamedType($p, $nullable, $nullable ? '?'.$p : $p);"` |
|    - | 1060 | `"}"` |
|    - | 1061 | `"function __reflect_make_type($text){"` |
|    - | 1062 | `" if($text === null \|\| $text === ''){ return null; }"` |
|    - | 1063 | `" $nullable = false;"` |
|    - | 1064 | `" $body = $text;"` |
|    - | 1065 | `" if($body[0] === '?'){ $nullable = true; $body = substr($body, 1); }"` |
|    - | 1066 | `" $parts = array();"` |
|    - | 1067 | `" $depth = 0;"` |
|    - | 1068 | `" $cur = '';"` |
|    - | 1069 | `" $n = strlen($body);"` |
|    - | 1070 | `" for($k = 0; $k < $n; $k++){"` |
|    - | 1071 | `"  $ch = $body[$k];"` |
|    - | 1072 | `"  if($ch === '('){ $depth++; $cur .= $ch; }"` |
|    - | 1073 | `"  else if($ch === ')'){ $depth--; $cur .= $ch; }"` |
|    - | 1074 | `"  else if($ch === '\|' && $depth === 0){ $parts[] = $cur; $cur = ''; }"` |
|    - | 1075 | `"  else{ $cur .= $ch; }"` |
|    - | 1076 | `" }"` |
|    - | 1077 | `" $parts[] = $cur;"` |
|    - | 1078 | `" if(count($parts) > 1){"` |
|    - | 1079 | `"  $nonNull = array();"` |
|    - | 1080 | `"  $hasNull = false;"` |
|    - | 1081 | `"  foreach($parts as $p){"` |
|    - | 1082 | `"   if(strtolower($p) === 'null'){ $hasNull = true; }"` |
|    - | 1083 | `"   else{ $nonNull[] = $p; }"` |
|    - | 1084 | `"  }"` |
|    - | 1085 | `"  if($hasNull && count($nonNull) === 1 && strpos($nonNull[0], '&') === false){"` |
|    - | 1086 | `"   return new ReflectionNamedType($nonNull[0], true, '?'.$nonNull[0]);"` |
|    - | 1087 | `"  }"` |
|    - | 1088 | `"  $types = array();"` |
|    - | 1089 | `"  foreach($parts as $p){ $types[] = __reflect_make_atom($p); }"` |
|    - | 1090 | `"  return new ReflectionUnionType($body, $nullable \|\| $hasNull, $types);"` |
|    - | 1091 | `" }"` |
|    - | 1092 | `" if(strpos($body, '&') !== false){ return __reflect_make_atom($body); }"` |
|    - | 1093 | `" return __reflect_make_atom($nullable ? '?'.$body : $body);"` |
|    - | 1094 | `"}"` |
|    - | 1095 | `;` |
|    - | 1096 | `/*` |
|    - | 1097 | ` * Chunk 5: ReflectionGenerator, ReflectionFiber. Executing line/file and` |
|    - | 1098 | ` * traces need runtime line tracking the VM does not have (same gap as` |
|    - | 1099 | ` * debug_backtrace's line numbers) — those throw a loud Error, recorded in` |
|    - | 1100 | ` * the plan ledger.` |
|    - | 1101 | ` */` |
|    - | 1102 | `static const char zReflectLib5[] =` |
|    - | 1103 | `"class ReflectionGenerator {"` |
|    - | 1104 | `" protected $__gen;"` |
|    - | 1105 | `" public function __construct($generator){"` |
|    - | 1106 | `"  if(!($generator instanceof Generator)){"` |
|    - | 1107 | `"   throw new TypeError('ReflectionGenerator::__construct(): Argument #1 ($generator) must be of type Generator, '.get_debug_type($generator).' given');"` |
|    - | 1108 | `"  }"` |
|    - | 1109 | `"  $this->__gen = $generator;"` |
|    - | 1110 | `" }"` |
|    - | 1111 | `" protected function __rginfo(){ return __reflect_gen_info($this->__gen); }"` |
|    - | 1112 | `" public function getFunction(){"` |
|    - | 1113 | `"  $i = $this->__rginfo();"` |
|    - | 1114 | `"  if($i['kind'] === 'method'){ return new ReflectionMethod($i['class'], $i['name']); }"` |
|    - | 1115 | `"  return new ReflectionFunction($i['name']);"` |
|    - | 1116 | `" }"` |
|    - | 1117 | `" public function getThis(){ $i = $this->__rginfo(); return isset($i['this']) ? $i['this'] : null; }"` |
|    - | 1118 | `" public function getExecutingGenerator(){ return __reflect_gen_exec($this->__gen); }"` |
|    - | 1119 | `" public function isClosed(){ $i = $this->__rginfo(); return $i['closed']; }"` |
|    - | 1120 | `" public function getExecutingLine(){"` |
|    - | 1121 | `"  throw new Error('ReflectionGenerator::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|    - | 1122 | `" }"` |
|    - | 1123 | `" public function getExecutingFile(){"` |
|    - | 1124 | `"  throw new Error('ReflectionGenerator::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|    - | 1125 | `" }"` |
|    - | 1126 | `" public function getTrace($options = 1){"` |
|    - | 1127 | `"  throw new Error('ReflectionGenerator::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|    - | 1128 | `" }"` |
|    - | 1129 | `"}"` |
|    - | 1130 | `"class ReflectionFiber {"` |
|    - | 1131 | `" protected $__fiber;"` |
|    - | 1132 | `" public function __construct($fiber){"` |
|    - | 1133 | `"  if(!($fiber instanceof Fiber)){"` |
|    - | 1134 | `"   throw new TypeError('ReflectionFiber::__construct(): Argument #1 ($fiber) must be of type Fiber, '.get_debug_type($fiber).' given');"` |
|    - | 1135 | `"  }"` |
|    - | 1136 | `"  $this->__fiber = $fiber;"` |
|    - | 1137 | `" }"` |
|    - | 1138 | `" public function getFiber(){ return $this->__fiber; }"` |
|    - | 1139 | `" public function getCallable(){ return __reflect_prop_read($this->__fiber, '__callable'); }"` |
|    - | 1140 | `" public function getExecutingLine(){"` |
|    - | 1141 | `"  throw new Error('ReflectionFiber::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|    - | 1142 | `" }"` |
|    - | 1143 | `" public function getExecutingFile(){"` |
|    - | 1144 | `"  throw new Error('ReflectionFiber::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|    - | 1145 | `" }"` |
|    - | 1146 | `" public function getTrace($options = 1){"` |
|    - | 1147 | `"  throw new Error('ReflectionFiber::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|    - | 1148 | `" }"` |
|    - | 1149 | `"}"` |
|    - | 1150 | `;` |
|    - | 1151 | `/*` |
|    - | 1152 | ` * Chunk 6: the long tail — ReflectionConstant (PHP 8.5), the synthetic` |
|    - | 1153 | ` * "Core" ReflectionExtension, ReflectionZendExtension (throws: no Zend` |
|    - | 1154 | ` * extensions exist), the ReflectionEnum family (throws: enums are not a` |
|    - | 1155 | ` * PHL language feature yet), and ReflectionReference.` |
|    - | 1156 | ` */` |
|    - | 1157 | `static const char zReflectLib6[] =` |
|    - | 1158 | `"class ReflectionConstant implements Reflector {"` |
|    - | 1159 | `" public $name;"` |
|    - | 1160 | `" public function __construct($name){"` |
|    - | 1161 | `"  if(!is_string($name)){"` |
|    - | 1162 | `"   throw new TypeError('ReflectionConstant::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|    - | 1163 | `"  }"` |
|    - | 1164 | `"  $i = __reflect_const_info($name);"` |
|    - | 1165 | `"  if($i === null){"` |
|    - | 1166 | `"   throw new ReflectionException('Constant \"'.$name.'\" does not exist');"` |
|    - | 1167 | `"  }"` |
|    - | 1168 | `"  $this->name = $name;"` |
|    - | 1169 | `" }"` |
|    - | 1170 | `" public function getName(){ return $this->name; }"` |
|    - | 1171 | `" public function getNamespaceName(){"` |
|    - | 1172 | `"  $p = strrpos($this->name,'\\\\');"` |
|    - | 1173 | `"  if($p === false){ return ''; }"` |
|    - | 1174 | `"  return substr($this->name,0,$p);"` |
|    - | 1175 | `" }"` |
|    - | 1176 | `" public function getShortName(){"` |
|    - | 1177 | `"  $p = strrpos($this->name,'\\\\');"` |
|    - | 1178 | `"  if($p === false){ return $this->name; }"` |
|    - | 1179 | `"  return substr($this->name,$p+1);"` |
|    - | 1180 | `" }"` |
|    - | 1181 | `" public function getValue(){"` |
|    - | 1182 | `"  $i = __reflect_const_info($this->name);"` |
|    - | 1183 | `"  return $i['value'];"` |
|    - | 1184 | `" }"` |
|    - | 1185 | `" public function isDeprecated(){ return false; }"` |
|    - | 1186 | `" public function getFileName(){"` |
|    - | 1187 | `"  $i = __reflect_const_info($this->name);"` |
|    - | 1188 | `"  return $i['file'];"` |
|    - | 1189 | `" }"` |
|    - | 1190 | `" public function getExtension(){"` |
|    - | 1191 | `"  $i = __reflect_const_info($this->name);"` |
|    - | 1192 | `"  return $i['internal'] ? new ReflectionExtension('Core') : null;"` |
|    - | 1193 | `" }"` |
|    - | 1194 | `" public function getExtensionName(){"` |
|    - | 1195 | `"  $i = __reflect_const_info($this->name);"` |
|    - | 1196 | `"  return $i['internal'] ? 'Core' : false;"` |
|    - | 1197 | `" }"` |
|    - | 1198 | `" public function getAttributes($name = null, $flags = 0){"` |
|    - | 1199 | `"  $i = __reflect_const_info($this->name);"` |
|    - | 1200 | `"  if($i === null){ return array(); }"` |
|    - | 1201 | `"  return __reflect_build_attrs($i['attrs'], array('const', $this->name, null, 0), 64, $name, $flags);"` |
|    - | 1202 | `" }"` |
|    - | 1203 | `" public function __toString(){"` |
|    - | 1204 | `"  return 'Constant [ '.$this->name.' ]'.\"\\n\";"` |
|    - | 1205 | `" }"` |
|    - | 1206 | `"}"` |
|    - | 1207 | `"class ReflectionExtension implements Reflector {"` |
|    - | 1208 | `" public $name;"` |
|    - | 1209 | `" public function __construct($name){"` |
|    - | 1210 | `"  if(!is_string($name)){"` |
|    - | 1211 | `"   throw new TypeError('ReflectionExtension::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|    - | 1212 | `"  }"` |
|    - | 1213 | `"  if(strtolower($name) !== 'core'){"` |
|    - | 1214 | `"   throw new ReflectionException('Extension \"'.$name.'\" does not exist');"` |
|    - | 1215 | `"  }"` |
|    - | 1216 | `"  $this->name = 'Core';"` |
|    - | 1217 | `" }"` |
|    - | 1218 | `" public function getName(){ return $this->name; }"` |
|    - | 1219 | `" public function getVersion(){ return phpversion(); }"` |
|    - | 1220 | `" public function getFunctions(){ return array(); }"` |
|    - | 1221 | `" public function getClasses(){ return array(); }"` |
|    - | 1222 | `" public function getClassNames(){ return array(); }"` |
|    - | 1223 | `" public function getConstants(){ return array(); }"` |
|    - | 1224 | `" public function getINIEntries(){ return array(); }"` |
|    - | 1225 | `" public function getDependencies(){ return array(); }"` |
|    - | 1226 | `" public function isPersistent(){ return true; }"` |
|    - | 1227 | `" public function isTemporary(){ return false; }"` |
|    - | 1228 | `" public function info(){ }"` |
|    - | 1229 | `" public function __toString(){"` |
|    - | 1230 | `"  return 'Extension [ extension #1 '.$this->name.' ]'.\"\\n\";"` |
|    - | 1231 | `" }"` |
|    - | 1232 | `"}"` |
|    - | 1233 | `"class ReflectionZendExtension implements Reflector {"` |
|    - | 1234 | `" public $name;"` |
|    - | 1235 | `" public function __construct($name){"` |
|    - | 1236 | `"  throw new ReflectionException('Zend Extension \"'.$name.'\" does not exist');"` |
|    - | 1237 | `" }"` |
|    - | 1238 | `" public function getName(){ return $this->name; }"` |
|    - | 1239 | `" public function __toString(){ return ''; }"` |
|    - | 1240 | `"}"` |
|    - | 1241 | `"class ReflectionEnum extends ReflectionClass {"` |
|    - | 1242 | `" public function __construct($objectOrClass){"` |
|    - | 1243 | `"  $info = __phl_rcinfo($objectOrClass);"` |
|    - | 1244 | `"  if($info === null){"` |
|    - | 1245 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|    - | 1246 | `"  }"` |
|    - | 1247 | `"  if(!$info['enum']){"` |
|    - | 1248 | `"   throw new ReflectionException('Class \"'.$info['name'].'\" is not an enum');"` |
|    - | 1249 | `"  }"` |
|    - | 1250 | `"  parent::__construct($objectOrClass);"` |
|    - | 1251 | `" }"` |
|    - | 1252 | `" public function hasCase($name){"` |
|    - | 1253 | `"  $i = $this->__rinfo();"` |
|    - | 1254 | `"  return in_array($name, $i['cases'], true);"` |
|    - | 1255 | `" }"` |
|    - | 1256 | `" public function getCase($name){"` |
|    - | 1257 | `"  if(!$this->hasCase($name)){"` |
|    - | 1258 | `"   throw new ReflectionException('Case '.$this->name.'::'.$name.' does not exist');"` |
|    - | 1259 | `"  }"` |
|    - | 1260 | `"  if($this->isBacked()){ return new ReflectionEnumBackedCase($this->name, $name); }"` |
|    - | 1261 | `"  return new ReflectionEnumUnitCase($this->name, $name);"` |
|    - | 1262 | `" }"` |
|    - | 1263 | `" public function getCases(){"` |
|    - | 1264 | `"  $i = $this->__rinfo();"` |
|    - | 1265 | `"  $out = array();"` |
|    - | 1266 | `"  foreach($i['cases'] as $c){"` |
|    - | 1267 | `"   $out[] = $this->isBacked()"` |
|    - | 1268 | `"    ? new ReflectionEnumBackedCase($this->name, $c)"` |
|    - | 1269 | `"    : new ReflectionEnumUnitCase($this->name, $c);"` |
|    - | 1270 | `"  }"` |
|    - | 1271 | `"  return $out;"` |
|    - | 1272 | `" }"` |
|    - | 1273 | `" public function isBacked(){ $i = $this->__rinfo(); return $i['enumbacking'] !== ''; }"` |
|    - | 1274 | `" public function getBackingType(){"` |
|    - | 1275 | `"  $i = $this->__rinfo();"` |
|    - | 1276 | `"  if($i['enumbacking'] === ''){ return null; }"` |
|    - | 1277 | `"  return __reflect_make_type($i['enumbacking']);"` |
|    - | 1278 | `" }"` |
|    - | 1279 | `"}"` |
|    - | 1280 | `"class ReflectionEnumUnitCase extends ReflectionClassConstant {"` |
|    - | 1281 | `" public function __construct($class, $constant){"` |
|    - | 1282 | `"  parent::__construct($class, $constant);"` |
|    - | 1283 | `"  $ci = __phl_rcinfo($class);"` |
|    - | 1284 | `"  if(!$ci['enum']){"` |
|    - | 1285 | `"   throw new ReflectionException('Class \"'.$this->class.'\" is not an enum');"` |
|    - | 1286 | `"  }"` |
|    - | 1287 | `"  $m = $this->__rcmeta();"` |
|    - | 1288 | `"  if(!$m['enumcase']){"` |
|    - | 1289 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' is not a case');"` |
|    - | 1290 | `"  }"` |
|    - | 1291 | `" }"` |
|    - | 1292 | `" public function getEnum(){ return new ReflectionEnum($this->class); }"` |
|    - | 1293 | `"}"` |
|    - | 1294 | `"class ReflectionEnumBackedCase extends ReflectionEnumUnitCase {"` |
|    - | 1295 | `" public function getBackingValue(){ return $this->getValue()->value; }"` |
|    - | 1296 | `"}"` |
|    - | 1297 | `"final class ReflectionReference {"` |
|    - | 1298 | `" protected $__id = '';"` |
|    - | 1299 | `" public function __construct(){"` |
|    - | 1300 | `"  throw new Error('Call to private ReflectionReference::__construct() from global scope');"` |
|    - | 1301 | `" }"` |
|    - | 1302 | `" public static function fromArrayElement($array, $key){"` |
|    - | 1303 | `"  if(!is_array($array)){"` |
|    - | 1304 | `"   throw new TypeError('ReflectionReference::fromArrayElement(): Argument #1 ($array) must be of type array, '.get_debug_type($array).' given');"` |
|    - | 1305 | `"  }"` |
|    - | 1306 | `"  $id = __reflect_ref_id($array, $key);"` |
|    - | 1307 | `"  if($id === null){ return null; }"` |
|    - | 1308 | `"  $r = __reflect_new_no_ctor('ReflectionReference');"` |
|    - | 1309 | `"  $r->__setId('phlref'.$id);"` |
|    - | 1310 | `"  return $r;"` |
|    - | 1311 | `" }"` |
|    - | 1312 | `" public function __setId($id){ $this->__id = $id; }"` |
|    - | 1313 | `" public function getId(){ return $this->__id; }"` |
|    - | 1314 | `"}"` |
|    - | 1315 | `;` |
|    - | 1316 | `/*` |
|    - | 1317 | ` * Chunk 7: ReflectionAttribute and the shared getAttributes() builder.` |
|    - | 1318 | ` * The spec array rides as [kind, target, member, paramIdx]; argument` |
|    - | 1319 | ` * values evaluate lazily through __reflect_attr_args (PHP semantics).` |
|    - | 1320 | ` */` |
|    - | 1321 | `static const char zReflectLib7[] =` |
|    - | 1322 | `"function __reflect_has_deprecated($meta){"` |
|    - | 1323 | `" foreach($meta as $a){"` |
|    - | 1324 | `"  if(strtolower($a['name']) === 'deprecated'){ return true; }"` |
|    - | 1325 | `" }"` |
|    - | 1326 | `" return false;"` |
|    - | 1327 | `"}"` |
|    - | 1328 | `"function __reflect_target_names($mask){"` |
|    - | 1329 | `" $parts = array();"` |
|    - | 1330 | `" foreach(array('class' => 1, 'function' => 2, 'method' => 4, 'property' => 8,"` |
|    - | 1331 | `"  'class constant' => 16, 'parameter' => 32, 'constant' => 64) as $nm => $bit){"` |
|    - | 1332 | `"  if($mask & $bit){ $parts[] = $nm; }"` |
|    - | 1333 | `" }"` |
|    - | 1334 | `" return implode(', ', $parts);"` |
|    - | 1335 | `"}"` |
|    - | 1336 | `"function __reflect_build_attrs($meta, $spec, $target, $name, $flags){"` |
|    - | 1337 | `" $out = array();"` |
|    - | 1338 | `" $counts = array();"` |
|    - | 1339 | `" foreach($meta as $a){"` |
|    - | 1340 | `"  $k = strtolower($a['name']);"` |
|    - | 1341 | `"  $counts[$k] = isset($counts[$k]) ? $counts[$k] + 1 : 1;"` |
|    - | 1342 | `" }"` |
|    - | 1343 | `" $idx = 0;"` |
|    - | 1344 | `" foreach($meta as $a){"` |
|    - | 1345 | `"  $keep = true;"` |
|    - | 1346 | `"  if($name !== null){"` |
|    - | 1347 | `"   $keep = strtolower($a['name']) === strtolower($name);"` |
|    - | 1348 | `"   if(!$keep && ($flags & 2)){"` |
|    - | 1349 | `"    $keep = is_subclass_of($a['name'], $name);"` |
|    - | 1350 | `"   }"` |
|    - | 1351 | `"  }"` |
|    - | 1352 | `"  if($keep){"` |
|    - | 1353 | `"   $r = __reflect_new_no_ctor('ReflectionAttribute');"` |
|    - | 1354 | `"   $r->__init($a['name'], $spec, $idx, $target, $counts[strtolower($a['name'])] > 1);"` |
|    - | 1355 | `"   $out[] = $r;"` |
|    - | 1356 | `"  }"` |
|    - | 1357 | `"  $idx++;"` |
|    - | 1358 | `" }"` |
|    - | 1359 | `" return $out;"` |
|    - | 1360 | `"}"` |
|    - | 1361 | `"final class ReflectionAttribute {"` |
|    - | 1362 | `" const IS_INSTANCEOF = 2;"` |
|    - | 1363 | `" protected $__name = '';"` |
|    - | 1364 | `" protected $__spec = null;"` |
|    - | 1365 | `" protected $__idx = 0;"` |
|    - | 1366 | `" protected $__target = 0;"` |
|    - | 1367 | `" protected $__rep = false;"` |
|    - | 1368 | `" public function __construct(){"` |
|    - | 1369 | `"  throw new Error('Call to private ReflectionAttribute::__construct() from global scope');"` |
|    - | 1370 | `" }"` |
|    - | 1371 | `" public function __init($name, $spec, $idx, $target, $rep){"` |
|    - | 1372 | `"  $this->__name = $name;"` |
|    - | 1373 | `"  $this->__spec = $spec;"` |
|    - | 1374 | `"  $this->__idx = $idx;"` |
|    - | 1375 | `"  $this->__target = $target;"` |
|    - | 1376 | `"  $this->__rep = $rep;"` |
|    - | 1377 | `" }"` |
|    - | 1378 | `" public function getName(){ return $this->__name; }"` |
|    - | 1379 | `" public function getTarget(){ return $this->__target; }"` |
|    - | 1380 | `" public function isRepeated(){ return $this->__rep; }"` |
|    - | 1381 | `" public function getArguments(){"` |
|    - | 1382 | `"  $a = __reflect_attr_args($this->__spec[0], $this->__spec[1], $this->__spec[2], $this->__spec[3], $this->__idx);"` |
|    - | 1383 | `"  return $a === null ? array() : $a;"` |
|    - | 1384 | `" }"` |
|    - | 1385 | `" public function newInstance(){"` |
|    - | 1386 | `"  $name = $this->__name;"` |
|    - | 1387 | `"  $ci = __phl_rcinfo($name);"` |
|    - | 1388 | `"  if($ci === null){"` |
|    - | 1389 | `"   throw new Error('Attribute class \"'.$name.'\" not found');"` |
|    - | 1390 | `"  }"` |
|    - | 1391 | `"  $name = $ci['name'];"` |
|    - | 1392 | `"  $decl = null;"` |
|    - | 1393 | `"  $didx = 0;"` |
|    - | 1394 | `"  foreach($ci['attrs'] as $a){"` |
|    - | 1395 | `"   if(strtolower($a['name']) === 'attribute'){ $decl = $didx; break; }"` |
|    - | 1396 | `"   $didx++;"` |
|    - | 1397 | `"  }"` |
|    - | 1398 | `"  if($decl === null){"` |
|    - | 1399 | `"   throw new Error('Attempting to use non-attribute class \"'.$name.'\" as attribute');"` |
|    - | 1400 | `"  }"` |
|    - | 1401 | `"  $dargs = __reflect_attr_args('class', $name, null, 0, $decl);"` |
|    - | 1402 | `"  $flags = 127;"` |
|    - | 1403 | `"  if(is_array($dargs)){"` |
|    - | 1404 | `"   if(isset($dargs[0])){ $flags = $dargs[0]; }"` |
|    - | 1405 | `"   else if(isset($dargs['flags'])){ $flags = $dargs['flags']; }"` |
|    - | 1406 | `"  }"` |
|    - | 1407 | `"  if(($flags & $this->__target) === 0){"` |
|    - | 1408 | `"   $tnames = array(1 => 'class', 2 => 'function', 4 => 'method', 8 => 'property',"` |
|    - | 1409 | `"    16 => 'class constant', 32 => 'parameter', 64 => 'constant');"` |
|    - | 1410 | `"   throw new Error('Attribute \"'.$name.'\" cannot target '.$tnames[$this->__target]"` |
|    - | 1411 | `"    .' (allowed targets: '.__reflect_target_names($flags).')');"` |
|    - | 1412 | `"  }"` |
|    - | 1413 | `"  if($this->__rep && ($flags & 128) === 0){"` |
|    - | 1414 | `"   throw new Error('Attribute \"'.$name.'\" must not be repeated');"` |
|    - | 1415 | `"  }"` |
|    - | 1416 | `"  return __reflect_new_instance($name, $this->getArguments());"` |
|    - | 1417 | `" }"` |
|    - | 1418 | `" public function __toString(){"` |
|    - | 1419 | `"  return 'Attribute [ '.$this->__name.' ]';"` |
|    - | 1420 | `" }"` |
|    - | 1421 | `"}"` |
|    - | 1422 | `;` |
|    - | 1423 | `/*` |
|    - | 1424 | ` * Chunk 8: signature-table support. Internal (C builtin) functions carry a` |
|    - | 1425 | ` * PHP-style parameter-list string; these helpers parse it into the same` |
|    - | 1426 | ` * param-meta shape user functions get, so ReflectionFunction and` |
|    - | 1427 | ` * ReflectionParameter work uniformly over builtins.` |
|    - | 1428 | ` */` |
|    - | 1429 | `static const char zReflectLib8[] =` |
|    - | 1430 | `"function __reflect_sig_split($sig){"` |
|    - | 1431 | `" $parts = array();"` |
|    - | 1432 | `" $cur = '';"` |
|    - | 1433 | `" $q = false;"` |
|    - | 1434 | `" $n = strlen($sig);"` |
|    - | 1435 | `" for($k = 0; $k < $n; $k++){"` |
|    - | 1436 | `"  $ch = $sig[$k];"` |
|    - | 1437 | `"  if($q){"` |
|    - | 1438 | `"   $cur .= $ch;"` |
|    - | 1439 | `"   if($ch === chr(92) && $k + 1 < $n){ $cur .= $sig[$k+1]; $k++; }"` |
|    - | 1440 | `"   else if($ch === chr(39)){ $q = false; }"` |
|    - | 1441 | `"  }else if($ch === chr(39)){ $q = true; $cur .= $ch; }"` |
|    - | 1442 | `"  else if($ch === ',' ){ $parts[] = trim($cur); $cur = ''; }"` |
|    - | 1443 | `"  else{ $cur .= $ch; }"` |
|    - | 1444 | `" }"` |
|    - | 1445 | `" if(trim($cur) !== ''){ $parts[] = trim($cur); }"` |
|    - | 1446 | `" return $parts;"` |
|    - | 1447 | `"}"` |
|    - | 1448 | `"function __reflect_sig_scalar($t){"` |
|    - | 1449 | `" if($t === '?'){ return array(false, null); }"` |
|    - | 1450 | `" if($t === 'NULL' \|\| $t === 'null'){ return array(true, null); }"` |
|    - | 1451 | `" if($t === 'true'){ return array(true, true); }"` |
|    - | 1452 | `" if($t === 'false'){ return array(true, false); }"` |
|    - | 1453 | `" if(is_numeric($t)){"` |
|    - | 1454 | `"  if(strpos($t, '.') === false && stripos($t, 'e') === false && strpos($t, 'x') === false){"` |
|    - | 1455 | `"   return array(true, (int)$t);"` |
|    - | 1456 | `"  }"` |
|    - | 1457 | `"  return array(true, (float)$t);"` |
|    - | 1458 | `" }"` |
|    - | 1459 | `" if(strlen($t) >= 2 && $t[0] === chr(39) && $t[strlen($t)-1] === chr(39)){"` |
|    - | 1460 | `"  $body = substr($t, 1, strlen($t) - 2);"` |
|    - | 1461 | `"  return array(true, strtr($body, array(chr(92).chr(39) => chr(39), chr(92).chr(92) => chr(92))));"` |
|    - | 1462 | `" }"` |
|    - | 1463 | `" return array(false, null);"` |
|    - | 1464 | `"}"` |
|    - | 1465 | `"function __reflect_parse_sig($sig){"` |
|    - | 1466 | `" $params = array();"` |
|    - | 1467 | `" $pos = 0;"` |
|    - | 1468 | `" foreach(__reflect_sig_split($sig) as $part){"` |
|    - | 1469 | `"  $deftext = null;"` |
|    - | 1470 | `"  $q = false;"` |
|    - | 1471 | `"  $n = strlen($part);"` |
|    - | 1472 | `"  for($k = 0; $k < $n; $k++){"` |
|    - | 1473 | `"   $ch = $part[$k];"` |
|    - | 1474 | `"   if($q){"` |
|    - | 1475 | `"    if($ch === chr(92)){ $k++; }"` |
|    - | 1476 | `"    else if($ch === chr(39)){ $q = false; }"` |
|    - | 1477 | `"   }else if($ch === chr(39)){ $q = true; }"` |
|    - | 1478 | `"   else if($ch === '=' ){"` |
|    - | 1479 | `"    $deftext = trim(substr($part, $k + 1));"` |
|    - | 1480 | `"    $part = trim(substr($part, 0, $k));"` |
|    - | 1481 | `"    break;"` |
|    - | 1482 | `"   }"` |
|    - | 1483 | `"  }"` |
|    - | 1484 | `"  $variadic = strpos($part, '...') !== false;"` |
|    - | 1485 | `"  $byref = strpos($part, '&') !== false;"` |
|    - | 1486 | `"  $d = strpos($part, '$');"` |
|    - | 1487 | `"  $name = $d === false ? $part : substr($part, $d + 1);"` |
|    - | 1488 | `"  $typetext = null;"` |
|    - | 1489 | `"  $sp = strpos($part, ' ');"` |
|    - | 1490 | `"  if($sp !== false && $d !== false && $sp < $d){ $typetext = substr($part, 0, $sp); }"` |
|    - | 1491 | `"  $nullable = $typetext !== null && ($typetext[0] === '?' \|\| stripos($typetext, 'null') !== false);"` |
|    - | 1492 | `"  $params[] = array('name' => $name, 'pos' => $pos, 'byref' => $byref,"` |
|    - | 1493 | `"   'variadic' => $variadic, 'hasdef' => $deftext !== null, 'nullable' => $nullable,"` |
|    - | 1494 | `"   'promoted' => false, 'typetext' => $typetext, 'attrs' => array(), 'deftext' => $deftext);"` |
|    - | 1495 | `"  $pos++;"` |
|    - | 1496 | `" }"` |
|    - | 1497 | `" return $params;"` |
|    - | 1498 | `"}"` |
|    - | 1499 | `"function __reflect_sig_fixup($i){"` |
|    - | 1500 | `" if($i === null){ return $i; }"` |
|    - | 1501 | `" if(isset($i['ret2'])){ $i['rettext'] = $i['ret2']; }"` |
|    - | 1502 | `" if(!isset($i['sig']) \|\| $i['sig'] === ''){ return $i; }"` |
|    - | 1503 | `" $i['params'] = __reflect_parse_sig($i['sig']);"` |
|    - | 1504 | `" $i['minarg'] = -1;"` |
|    - | 1505 | `" $v = false;"` |
|    - | 1506 | `" foreach($i['params'] as $p){ if($p['variadic']){ $v = true; } }"` |
|    - | 1507 | `" $i['variadic'] = $v;"` |
|    - | 1508 | `" return $i;"` |
|    - | 1509 | `"}"` |
|    - | 1510 | `;` |
|    - | 1511 | `/*` |
|    - | 1512 | ` * Chunk 9: PHP's Reflection export format (__toString on every Reflector).` |
|    - | 1513 | ` * Built entirely from the public reflection API of the target objects.` |
|    - | 1514 | ` */` |
|    - | 1515 | `static const char zReflectLib9[] =` |
|    - | 1516 | `"function __reflect_export_value($v){"` |
|    - | 1517 | `" if($v === null){ return 'NULL'; }"` |
|    - | 1518 | `" if($v === true){ return 'true'; }"` |
|    - | 1519 | `" if($v === false){ return 'false'; }"` |
|    - | 1520 | `" if(is_string($v)){ return chr(39).$v.chr(39); }"` |
|    - | 1521 | `" if(is_array($v)){"` |
|    - | 1522 | `"  $parts = array();"` |
|    - | 1523 | `"  $isList = true;"` |
|    - | 1524 | `"  $next = 0;"` |
|    - | 1525 | `"  foreach($v as $k => $x){"` |
|    - | 1526 | `"   if($k !== $next){ $isList = false; break; }"` |
|    - | 1527 | `"   $next++;"` |
|    - | 1528 | `"  }"` |
|    - | 1529 | `"  foreach($v as $k => $x){"` |
|    - | 1530 | `"   $parts[] = $isList ? __reflect_export_value($x)"` |
|    - | 1531 | `"    : (__reflect_export_value($k).' => '.__reflect_export_value($x));"` |
|    - | 1532 | `"  }"` |
|    - | 1533 | `"  return '['.implode(', ', $parts).']';"` |
|    - | 1534 | `" }"` |
|    - | 1535 | `" return (string)$v;"` |
|    - | 1536 | `"}"` |
|    - | 1537 | `"function __reflect_export_param($p){"` |
|    - | 1538 | `" $s = 'Parameter #'.$p->getPosition().' [ <'.($p->isOptional() ? 'optional' : 'required').'> ';"` |
|    - | 1539 | `" $t = $p->getType();"` |
|    - | 1540 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|    - | 1541 | `" if($p->isPassedByReference()){ $s .= '&'; }"` |
|    - | 1542 | `" if($p->isVariadic()){ $s .= '...'; }"` |
|    - | 1543 | `" $s .= '$'.$p->getName();"` |
|    - | 1544 | `" if($p->isDefaultValueAvailable()){"` |
|    - | 1545 | `"  try{ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|    - | 1546 | `"  catch(ReflectionException $e){ $s .= ' = <default>'; }"` |
|    - | 1547 | `" }"` |
|    - | 1548 | `" return $s.' ]';"` |
|    - | 1549 | `"}"` |
|    - | 1550 | `"function __reflect_export_prop($p){"` |
|    - | 1551 | `" $s = 'Property [ ';"` |
|    - | 1552 | `" $s .= $p->isPrivate() ? 'private ' : ($p->isProtected() ? 'protected ' : 'public ');"` |
|    - | 1553 | `" if($p->isStatic()){ $s .= 'static '; }"` |
|    - | 1554 | `" if($p->isReadOnly()){ $s .= 'readonly '; }"` |
|    - | 1555 | `" $t = $p->getType();"` |
|    - | 1556 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|    - | 1557 | `" $s .= '$'.$p->getName();"` |
|    - | 1558 | `" if($p->hasDefaultValue()){ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|    - | 1559 | `" return $s.' ]'.chr(10);"` |
|    - | 1560 | `"}"` |
|    - | 1561 | `"function __reflect_export_cconst($c){"` |
|    - | 1562 | `" $v = $c->getValue();"` |
|    - | 1563 | `" if(is_int($v)){ $t = 'int'; }"` |
|    - | 1564 | `" else if(is_string($v)){ $t = 'string'; }"` |
|    - | 1565 | `" else if(is_float($v)){ $t = 'float'; }"` |
|    - | 1566 | `" else if(is_bool($v)){ $t = 'bool'; }"` |
|    - | 1567 | `" else if(is_array($v)){ $t = 'array'; }"` |
|    - | 1568 | `" else{ $t = 'null'; }"` |
|    - | 1569 | `" $vs = is_array($v) ? 'Array' : (is_bool($v) ? ($v ? '1' : '') : (string)$v);"` |
|    - | 1570 | `" $vis = $c->isPrivate() ? 'private' : ($c->isProtected() ? 'protected' : 'public');"` |
|    - | 1571 | `" return 'Constant [ '.$vis.' '.$t.' '.$c->name.' ] { '.$vs.' }'.chr(10);"` |
|    - | 1572 | `"}"` |
|    - | 1573 | `"function __reflect_export_fnabs($r, $indent){"` |
|    - | 1574 | `" $tags = $r->isInternal() ? 'internal:Core' : 'user';"` |
|    - | 1575 | `" if($r instanceof ReflectionMethod){"` |
|    - | 1576 | `"  if($r->isConstructor()){ $tags .= ', ctor'; }"` |
|    - | 1577 | `"  else if($r->isDestructor()){ $tags .= ', dtor'; }"` |
|    - | 1578 | `"  $decl = $r->getDeclaringClass()->name;"` |
|    - | 1579 | `"  if(strtolower($decl) !== strtolower($r->class)){ $tags .= ', inherits '.$decl; }"` |
|    - | 1580 | `"  else if($r->hasPrototype()){ $tags .= ', prototype '.$r->getPrototype()->class; }"` |
|    - | 1581 | `"  $head = 'Method [ <'.$tags.'> ';"` |
|    - | 1582 | `"  if($r->isAbstract()){ $head .= 'abstract '; }"` |
|    - | 1583 | `"  if($r->isFinal()){ $head .= 'final '; }"` |
|    - | 1584 | `"  if($r->isStatic()){ $head .= 'static '; }"` |
|    - | 1585 | `"  $head .= $r->isPrivate() ? 'private ' : ($r->isProtected() ? 'protected ' : 'public ');"` |
|    - | 1586 | `"  $head .= 'method '.$r->name.' ]';"` |
|    - | 1587 | `" }else{"` |
|    - | 1588 | `"  $kind = $r->isClosure() ? 'Closure' : 'Function';"` |
|    - | 1589 | `"  $head = $kind.' [ <'.$tags.'> function '.$r->name.' ]';"` |
|    - | 1590 | `" }"` |
|    - | 1591 | `" $s = $head.' {'.chr(10);"` |
|    - | 1592 | `" if(!$r->isInternal()){"` |
|    - | 1593 | `"  $s .= '  @@ '.$r->getFileName().' '.$r->getStartLine().' - '.$r->getEndLine().chr(10);"` |
|    - | 1594 | `" }"` |
|    - | 1595 | `" $ps = $r->getParameters();"` |
|    - | 1596 | `" $ret = $r->getReturnType();"` |
|    - | 1597 | `" if(count($ps) > 0 \|\| $ret !== null){"` |
|    - | 1598 | `"  $s .= chr(10).'  - Parameters ['.count($ps).'] {'.chr(10);"` |
|    - | 1599 | `"  foreach($ps as $p){ $s .= '    '.__reflect_export_param($p).chr(10); }"` |
|    - | 1600 | `"  $s .= '  }'.chr(10);"` |
|    - | 1601 | `" }"` |
|    - | 1602 | `" if($ret !== null){ $s .= '  - Return [ '.(string)$ret.' ]'.chr(10); }"` |
|    - | 1603 | `" $s .= '}'.chr(10);"` |
|    - | 1604 | `" if($indent === ''){ return $s; }"` |
|    - | 1605 | `" $lines = explode(chr(10), $s);"` |
|    - | 1606 | `" $out = '';"` |
|    - | 1607 | `" $n = count($lines);"` |
|    - | 1608 | `" for($k = 0; $k < $n; $k++){"` |
|    - | 1609 | `"  if($lines[$k] === '' && $k === $n - 1){ break; }"` |
|    - | 1610 | `"  $out .= ($lines[$k] === '' ? '' : $indent.$lines[$k]).chr(10);"` |
|    - | 1611 | `" }"` |
|    - | 1612 | `" return $out;"` |
|    - | 1613 | `"}"` |
|    - | 1614 | `"function __reflect_export_class($rc){"` |
|    - | 1615 | `" $tags = $rc->isInternal() ? 'internal:Core' : 'user';"` |
|    - | 1616 | `" if($rc->isInterface()){"` |
|    - | 1617 | `"  $head = 'Interface [ <'.$tags.'> interface '.$rc->name.' ]';"` |
|    - | 1618 | `" }else{"` |
|    - | 1619 | `"  $mods = '';"` |
|    - | 1620 | `"  if($rc->isAbstract()){ $mods .= 'abstract '; }"` |
|    - | 1621 | `"  if($rc->isFinal()){ $mods .= 'final '; }"` |
|    - | 1622 | `"  $head = 'Class [ <'.$tags.'> '.$mods.'class '.$rc->name;"` |
|    - | 1623 | `"  $par = $rc->getParentClass();"` |
|    - | 1624 | `"  if($par !== false){ $head .= ' extends '.$par->name; }"` |
|    - | 1625 | `"  $ifs = $rc->getInterfaceNames();"` |
|    - | 1626 | `"  if(count($ifs) > 0){ $head .= ' implements '.implode(', ', $ifs); }"` |
|    - | 1627 | `"  $head .= ' ]';"` |
|    - | 1628 | `" }"` |
|    - | 1629 | `" $s = $head.' {'.chr(10);"` |
|    - | 1630 | `" if(!$rc->isInternal()){"` |
|    - | 1631 | `"  $s .= '  @@ '.$rc->getFileName().' '.$rc->getStartLine().'-'.$rc->getEndLine().chr(10);"` |
|    - | 1632 | `" }"` |
|    - | 1633 | `" $consts = $rc->getReflectionConstants();"` |
|    - | 1634 | `" $s .= chr(10).'  - Constants ['.count($consts).'] {'.chr(10);"` |
|    - | 1635 | `" foreach($consts as $c){ $s .= '    '.__reflect_export_cconst($c); }"` |
|    - | 1636 | `" $s .= '  }'.chr(10);"` |
|    - | 1637 | `" $sp = array();"` |
|    - | 1638 | `" $ip = array();"` |
|    - | 1639 | `" foreach($rc->getProperties() as $p){"` |
|    - | 1640 | `"  if($p->isStatic()){ $sp[] = $p; }else{ $ip[] = $p; }"` |
|    - | 1641 | `" }"` |
|    - | 1642 | `" $sm = array();"` |
|    - | 1643 | `" $im = array();"` |
|    - | 1644 | `" foreach($rc->getMethods() as $m){"` |
|    - | 1645 | `"  if($m->isStatic()){ $sm[] = $m; }else{ $im[] = $m; }"` |
|    - | 1646 | `" }"` |
|    - | 1647 | `" $s .= chr(10).'  - Static properties ['.count($sp).'] {'.chr(10);"` |
|    - | 1648 | `" foreach($sp as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|    - | 1649 | `" $s .= '  }'.chr(10);"` |
|    - | 1650 | `" $s .= chr(10).'  - Static methods ['.count($sm).'] {'.chr(10);"` |
|    - | 1651 | `" $first = true;"` |
|    - | 1652 | `" foreach($sm as $m){"` |
|    - | 1653 | `"  if(!$first){ $s .= chr(10); }"` |
|    - | 1654 | `"  $first = false;"` |
|    - | 1655 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|    - | 1656 | `" }"` |
|    - | 1657 | `" $s .= '  }'.chr(10);"` |
|    - | 1658 | `" $s .= chr(10).'  - Properties ['.count($ip).'] {'.chr(10);"` |
|    - | 1659 | `" foreach($ip as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|    - | 1660 | `" $s .= '  }'.chr(10);"` |
|    - | 1661 | `" $s .= chr(10).'  - Methods ['.count($im).'] {'.chr(10);"` |
|    - | 1662 | `" $first = true;"` |
|    - | 1663 | `" foreach($im as $m){"` |
|    - | 1664 | `"  if(!$first){ $s .= chr(10); }"` |
|    - | 1665 | `"  $first = false;"` |
|    - | 1666 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|    - | 1667 | `" }"` |
|    - | 1668 | `" $s .= '  }'.chr(10);"` |
|    - | 1669 | `" return $s.'}'.chr(10);"` |
|    - | 1670 | `"}"` |
|    - | 1671 | `;` |
|    - | 1672 | `/*` |
|    - | 1673 | ` * Register the __reflect_* thunks and compile the Reflection library.` |
|    - | 1674 | ` * Called from PH7_VmInit while pVm->bCompilingBuiltin is set, right after` |
|    - | 1675 | ` * the core builtin chunks (Exception and friends must exist already).` |
|    - | 1676 | ` */` |
|    - | 1677 | `/*` |
|    - | 1678 | ` * Compile the nine reflection chunks, in order. Split from` |
|    - | 1679 | ` * PH7_VmInstallReflection so the chunk strings and their sizeof stay in` |
|    - | 1680 | ` * one translation unit.` |
|    - | 1681 | ` */` |
| 3886 | 1682 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionLib(ph7_vm *pVm)` |
|    5 | 1683 | `{` |
|    - | 1684 | `	sxi32 rc;` |
| 3891 | 1685 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib1, sizeof(zReflectLib1)-1);` |
| 3891 | 1686 | `	if( rc != SXRET_OK ){` |
|  ! 0 | 1687 | `		return rc;` |
|    - | 1688 | `	}` |
| 3891 | 1689 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib2, sizeof(zReflectLib2)-1);` |
| 3891 | 1690 | `	if( rc != SXRET_OK ){` |
|  ! 0 | 1691 | `		return rc;` |
|    - | 1692 | `	}` |
| 3891 | 1693 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib3, sizeof(zReflectLib3)-1);` |
| 3891 | 1694 | `	if( rc != SXRET_OK ){` |
|  ! 0 | 1695 | `		return rc;` |
|    - | 1696 | `	}` |
| 3891 | 1697 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib4, sizeof(zReflectLib4)-1);` |
| 3891 | 1698 | `	if( rc != SXRET_OK ){` |
|  ! 0 | 1699 | `		return rc;` |
|    - | 1700 | `	}` |
| 3891 | 1701 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib5, sizeof(zReflectLib5)-1);` |
| 3891 | 1702 | `	if( rc != SXRET_OK ){` |
|  ! 0 | 1703 | `		return rc;` |
|    - | 1704 | `	}` |
| 3891 | 1705 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib6, sizeof(zReflectLib6)-1);` |
| 3891 | 1706 | `	if( rc != SXRET_OK ){` |
|  ! 0 | 1707 | `		return rc;` |
|    - | 1708 | `	}` |
| 3891 | 1709 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib7, sizeof(zReflectLib7)-1);` |
| 3891 | 1710 | `	if( rc != SXRET_OK ){` |
|  ! 0 | 1711 | `		return rc;` |
|    - | 1712 | `	}` |
| 3891 | 1713 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib8, sizeof(zReflectLib8)-1);` |
| 3891 | 1714 | `	if( rc != SXRET_OK ){` |
|  ! 0 | 1715 | `		return rc;` |
|    - | 1716 | `	}` |
| 3891 | 1717 | `	return PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib9, sizeof(zReflectLib9)-1);` |
| 1948 | 1718 | `}` |
|    - | 1719 |  |
