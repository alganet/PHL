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
|    - |  254 | `"  __reflect_static_materialize($this->name);"` |
|    - |  255 | `"  $i = $this->__rinfo();"` |
|    - |  256 | `"  $out = array();"` |
|    - |  257 | `"  foreach($i['props'] as $k => $p){"` |
|    - |  258 | `"   if($p['static']){ $out[$k] = __reflect_static_value($this->name,$k); }"` |
|    - |  259 | `"  }"` |
|    - |  260 | `"  return $out;"` |
|    - |  261 | `" }"` |
|    - |  262 | `" public function getStaticPropertyValue($name, ...$def){"` |
|    - |  263 | `"  __reflect_static_materialize($this->name);"` |
|    - |  264 | `"  $i = $this->__rinfo();"` |
|    - |  265 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|    - |  266 | `"   if(count($def) > 0){ return $def[0]; }"` |
|    - |  267 | `"   throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|    - |  268 | `"  }"` |
|    - |  269 | `"  return __reflect_static_value($this->name,$name);"` |
|    - |  270 | `" }"` |
|    - |  271 | `" public function setStaticPropertyValue($name,$value){"` |
|    - |  272 | `"  __reflect_static_materialize($this->name);"` |
|    - |  273 | `"  $i = $this->__rinfo();"` |
|    - |  274 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|    - |  275 | `"   throw new ReflectionException('Class '.$this->name.' does not have a property named '.$name);"` |
|    - |  276 | `"  }"` |
|    - |  277 | `"  __reflect_static_set($this->name,$name,$value);"` |
|    - |  278 | `" }"` |
|    - |  279 | `" public function getDefaultProperties(){"` |
|    - |  280 | `"  $i = $this->__rinfo();"` |
|    - |  281 | `"  $out = array();"` |
|    - |  282 | `"  foreach($i['props'] as $k => $p){"` |
|    - |  283 | `"   if($p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|    - |  284 | `"  }"` |
|    - |  285 | `"  foreach($i['props'] as $k => $p){"` |
|    - |  286 | `"   if(!$p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|    - |  287 | `"  }"` |
|    - |  288 | `"  return $out;"` |
|    - |  289 | `" }"` |
|    - |  290 | `" public function getProperty($name){"` |
|    - |  291 | `"  $i = $this->__rinfo();"` |
|    - |  292 | `"  if(isset($i['props'][$name])){"` |
|    - |  293 | `"   return new ReflectionProperty($this->name, $name);"` |
|    - |  294 | `"  }"` |
|    - |  295 | `"  if($this->__obj !== null && (__reflect_prop_state($this->__obj, $name) & 1)){"` |
|    - |  296 | `"   return new ReflectionProperty($this->__obj, $name);"` |
|    - |  297 | `"  }"` |
|    - |  298 | `"  throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|    - |  299 | `" }"` |
|    - |  300 | `" public function getProperties($filter = null){"` |
|    - |  301 | `"  $i = $this->__rinfo();"` |
|    - |  302 | `"  $out = array();"` |
|    - |  303 | `"  foreach($i['props'] as $k => $p){"` |
|    - |  304 | `"   if($filter !== null){"` |
|    - |  305 | `"    $m = ($p['vis'] === 1 ? 1 : ($p['vis'] === 2 ? 2 : 4));"` |
|    - |  306 | `"    if($p['static']){ $m \|= 16; }"` |
|    - |  307 | `"    if($p['readonly']){ $m \|= 128; }"` |
|    - |  308 | `"    if(($m & $filter) === 0){ continue; }"` |
|    - |  309 | `"   }"` |
|    - |  310 | `"   $out[] = new ReflectionProperty($this->name, $k);"` |
|    - |  311 | `"  }"` |
|    - |  312 | `"  if($this->__obj !== null){"` |
|    - |  313 | `"   foreach(__reflect_dyn_props($this->__obj) as $k){"` |
|    - |  314 | `"    if(isset($i['props'][$k])){ continue; }"` |
|    - |  315 | `"    if($filter !== null && ($filter & 1) === 0){ continue; }"` |
|    - |  316 | `"    $out[] = new ReflectionProperty($this->__obj, $k);"` |
|    - |  317 | `"   }"` |
|    - |  318 | `"  }"` |
|    - |  319 | `"  return $out;"` |
|    - |  320 | `" }"` |
|    - |  321 | `" public function getMethod($name){"` |
|    - |  322 | `"  $i = $this->__rinfo();"` |
|    - |  323 | `"  $found = null;"` |
|    - |  324 | `"  if(isset($i['methods'][$name])){"` |
|    - |  325 | `"   $found = $name;"` |
|    - |  326 | `"  }else{"` |
|    - |  327 | `"   $l = strtolower($name);"` |
|    - |  328 | `"   foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ $found = $k; break; } }"` |
|    - |  329 | `"  }"` |
|    - |  330 | `"  if($found === null){"` |
|    - |  331 | `"   throw new ReflectionException('Method '.$this->name.'::'.$name.'() does not exist');"` |
|    - |  332 | `"  }"` |
|    - |  333 | `"  return new ReflectionMethod($this->name, $found);"` |
|    - |  334 | `" }"` |
|    - |  335 | `" public function getMethods($filter = null){"` |
|    - |  336 | `"  $i = $this->__rinfo();"` |
|    - |  337 | `"  $out = array();"` |
|    - |  338 | `"  foreach($i['methods'] as $k => $m){"` |
|    - |  339 | `"   if($filter !== null){"` |
|    - |  340 | `"    $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|    - |  341 | `"    if($m['static']){ $mod \|= 16; }"` |
|    - |  342 | `"    if($m['abstract']){ $mod \|= 64; }"` |
|    - |  343 | `"    if($m['final']){ $mod \|= 32; }"` |
|    - |  344 | `"    if(($mod & $filter) === 0){ continue; }"` |
|    - |  345 | `"   }"` |
|    - |  346 | `"   $out[] = new ReflectionMethod($this->name, $k);"` |
|    - |  347 | `"  }"` |
|    - |  348 | `"  return $out;"` |
|    - |  349 | `" }"` |
|    - |  350 | `" public function getConstructor(){"` |
|    - |  351 | `"  $i = $this->__rinfo();"` |
|    - |  352 | `"  if(isset($i['methods']['__construct'])){"` |
|    - |  353 | `"   return new ReflectionMethod($this->name, '__construct');"` |
|    - |  354 | `"  }"` |
|    - |  355 | `"  foreach($i['methods'] as $k => $m){"` |
|    - |  356 | `"   if(strtolower($k) === '__construct'){ return new ReflectionMethod($this->name, $k); }"` |
|    - |  357 | `"  }"` |
|    - |  358 | `"  if($i['ctorvis'] !== 0 && isset($i['methods'][$this->name])){"` |
|    - |  359 | `"   return new ReflectionMethod($this->name, $this->name);"` |
|    - |  360 | `"  }"` |
|    - |  361 | `"  return null;"` |
|    - |  362 | `" }"` |
|    - |  363 | `" public function getReflectionConstant($name){"` |
|    - |  364 | `"  $i = $this->__rinfo();"` |
|    - |  365 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|    - |  366 | `"  return new ReflectionClassConstant($this->name, $name);"` |
|    - |  367 | `" }"` |
|    - |  368 | `" public function getReflectionConstants($filter = null){"` |
|    - |  369 | `"  $i = $this->__rinfo();"` |
|    - |  370 | `"  $out = array();"` |
|    - |  371 | `"  foreach($i['consts'] as $k => $c){"` |
|    - |  372 | `"   if($filter !== null){"` |
|    - |  373 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|    - |  374 | `"    if($c['final']){ $m \|= 32; }"` |
|    - |  375 | `"    if(($m & $filter) === 0){ continue; }"` |
|    - |  376 | `"   }"` |
|    - |  377 | `"   $out[] = new ReflectionClassConstant($this->name, $k);"` |
|    - |  378 | `"  }"` |
|    - |  379 | `"  return $out;"` |
|    - |  380 | `" }"` |
|    - |  381 | `" public function getAttributes($name = null, $flags = 0){"` |
|    - |  382 | `"  $i = $this->__rinfo();"` |
|    - |  383 | `"  return __reflect_build_attrs($i['attrs'], array('class', $this->name, null, 0), 1, $name, $flags);"` |
|    - |  384 | `" }"` |
|    - |  385 | `" public function getExtensionName(){ $i = $this->__rinfo(); return $i['internal'] ? 'Core' : false; }"` |
|    - |  386 | `" public function getExtension(){ $i = $this->__rinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|    - |  387 | `" public function newLazyGhost($initializer, $options = 0){"` |
|    - |  388 | `"  throw new Error('ReflectionClass::newLazyGhost() is not supported by PHL (no lazy objects)');"` |
|    - |  389 | `" }"` |
|    - |  390 | `" public function newLazyProxy($factory, $options = 0){"` |
|    - |  391 | `"  throw new Error('ReflectionClass::newLazyProxy() is not supported by PHL (no lazy objects)');"` |
|    - |  392 | `" }"` |
|    - |  393 | `" public function resetAsLazyGhost($object, $initializer, $options = 0){"` |
|    - |  394 | `"  throw new Error('ReflectionClass::resetAsLazyGhost() is not supported by PHL (no lazy objects)');"` |
|    - |  395 | `" }"` |
|    - |  396 | `" public function resetAsLazyProxy($object, $factory, $options = 0){"` |
|    - |  397 | `"  throw new Error('ReflectionClass::resetAsLazyProxy() is not supported by PHL (no lazy objects)');"` |
|    - |  398 | `" }"` |
|    - |  399 | `" public function getLazyInitializer($object){ return null; }"` |
|    - |  400 | `" public function initializeLazyObject($object){ return $object; }"` |
|    - |  401 | `" public function markLazyObjectAsInitialized($object){ return $object; }"` |
|    - |  402 | `" public function isUninitializedLazyObject($object){ return false; }"` |
|    - |  403 | `" public function __toString(){ return __reflect_export_class($this); }"` |
|    - |  404 | `"}"` |
|    - |  405 | `"class ReflectionObject extends ReflectionClass {"` |
|    - |  406 | `" public function __construct($object){"` |
|    - |  407 | `"  if(!is_object($object)){"` |
|    - |  408 | `"   throw new TypeError('ReflectionObject::__construct(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|    - |  409 | `"  }"` |
|    - |  410 | `"  parent::__construct($object);"` |
|    - |  411 | `"  $this->__obj = $object;"` |
|    - |  412 | `" }"` |
|    - |  413 | `"}"` |
|    - |  414 | `;` |
|    - |  415 | `/*` |
|    - |  416 | ` * Chunk 2: ReflectionFunctionAbstract, ReflectionFunction, ReflectionMethod,` |
|    - |  417 | ` * ReflectionParameter.` |
|    - |  418 | ` */` |
|    - |  419 | `static const char zReflectLib2[] =` |
|    - |  420 | `"abstract class ReflectionFunctionAbstract implements Reflector {"` |
|    - |  421 | `" public $name;"` |
|    - |  422 | `" protected $__cl = null;"` |
|    - |  423 | `" protected function __rfinfo(){"` |
|    - |  424 | `"  if($this->__cl !== null){ return __reflect_sig_fixup(__reflect_func_info($this->__cl)); }"` |
|    - |  425 | `"  return __reflect_sig_fixup(__reflect_func_info($this->name));"` |
|    - |  426 | `" }"` |
|    - |  427 | `" protected function __rftarget(){ return $this->__cl !== null ? $this->__cl : $this->name; }"` |
|    - |  428 | `" protected function __rpspec(){ return $this->__rftarget(); }"` |
|    - |  429 | `" public function getName(){ return $this->name; }"` |
|    - |  430 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|    - |  431 | `" public function getNamespaceName(){"` |
|    - |  432 | `"  $p = strrpos($this->name,'\\\\');"` |
|    - |  433 | `"  if($p === false){ return ''; }"` |
|    - |  434 | `"  return substr($this->name,0,$p);"` |
|    - |  435 | `" }"` |
|    - |  436 | `" public function getShortName(){"` |
|    - |  437 | `"  $p = strrpos($this->name,'\\\\');"` |
|    - |  438 | `"  if($p === false){ return $this->name; }"` |
|    - |  439 | `"  return substr($this->name,$p+1);"` |
|    - |  440 | `" }"` |
|    - |  441 | `" public function isClosure(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|    - |  442 | `" public function isGenerator(){ $i = $this->__rfinfo(); return $i['generator']; }"` |
|    - |  443 | `" public function isVariadic(){ $i = $this->__rfinfo(); return $i['variadic']; }"` |
|    - |  444 | `" public function returnsReference(){ $i = $this->__rfinfo(); return $i['byref']; }"` |
|    - |  445 | `" public function isInternal(){ $i = $this->__rfinfo(); return $i['internal']; }"` |
|    - |  446 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|    - |  447 | `" public function isDeprecated(){ $i = $this->__rfinfo(); return __reflect_has_deprecated($i['attrs']); }"` |
|    - |  448 | `" public function isStatic(){ $i = $this->__rfinfo(); return $i['fstatic']; }"` |
|    - |  449 | `" public function getFileName(){ $i = $this->__rfinfo(); return $i['file']; }"` |
|    - |  450 | `" public function getStartLine(){"` |
|    - |  451 | `"  $i = $this->__rfinfo();"` |
|    - |  452 | `"  if($i['internal']){ return false; }"` |
|    - |  453 | `"  return $i['line'];"` |
|    - |  454 | `" }"` |
|    - |  455 | `" public function getEndLine(){"` |
|    - |  456 | `"  $i = $this->__rfinfo();"` |
|    - |  457 | `"  if($i['internal']){ return false; }"` |
|    - |  458 | `"  return $i['endline'];"` |
|    - |  459 | `" }"` |
|    - |  460 | `" public function getDocComment(){ $i = $this->__rfinfo(); return $i['doc']; }"` |
|    - |  461 | `" public function hasReturnType(){ $i = $this->__rfinfo(); return $i['rettext'] !== null; }"` |
|    - |  462 | `" public function getReturnType(){ $i = $this->__rfinfo(); return __reflect_make_type($i['rettext']); }"` |
|    - |  463 | `" public function hasTentativeReturnType(){ return false; }"` |
|    - |  464 | `" public function getTentativeReturnType(){ return null; }"` |
|    - |  465 | `" public function getNumberOfParameters(){"` |
|    - |  466 | `"  $i = $this->__rfinfo();"` |
|    - |  467 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|    - |  468 | `"  return count($i['params']);"` |
|    - |  469 | `" }"` |
|    - |  470 | `" public function getNumberOfRequiredParameters(){"` |
|    - |  471 | `"  $i = $this->__rfinfo();"` |
|    - |  472 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|    - |  473 | `"  $req = 0;"` |
|    - |  474 | `"  $n = count($i['params']);"` |
|    - |  475 | `"  for($k = $n - 1; $k >= 0; $k--){"` |
|    - |  476 | `"   $p = $i['params'][$k];"` |
|    - |  477 | `"   if(!$p['variadic'] && !$p['hasdef']){ $req = $k + 1; break; }"` |
|    - |  478 | `"  }"` |
|    - |  479 | `"  return $req;"` |
|    - |  480 | `" }"` |
|    - |  481 | `" public function getParameters(){"` |
|    - |  482 | `"  $i = $this->__rfinfo();"` |
|    - |  483 | `"  $out = array();"` |
|    - |  484 | `"  $spec = $this->__rpspec();"` |
|    - |  485 | `"  foreach($i['params'] as $p){"` |
|    - |  486 | `"   $out[] = new ReflectionParameter($spec, $p['pos']);"` |
|    - |  487 | `"  }"` |
|    - |  488 | `"  return $out;"` |
|    - |  489 | `" }"` |
|    - |  490 | `" public function getStaticVariables(){ $i = $this->__rfinfo(); return $i['statics']; }"` |
|    - |  491 | `" public function getClosureThis(){"` |
|    - |  492 | `"  $i = $this->__rfinfo();"` |
|    - |  493 | `"  return isset($i['this']) ? $i['this'] : null;"` |
|    - |  494 | `" }"` |
|    - |  495 | `" public function getClosureScopeClass(){"` |
|    - |  496 | `"  $i = $this->__rfinfo();"` |
|    - |  497 | `"  if(isset($i['scope'])){ return new ReflectionClass($i['scope']); }"` |
|    - |  498 | `"  if(isset($i['this'])){ return new ReflectionClass(get_class($i['this'])); }"` |
|    - |  499 | `"  return null;"` |
|    - |  500 | `" }"` |
|    - |  501 | `" public function getClosureCalledClass(){ return $this->getClosureScopeClass(); }"` |
|    - |  502 | `" public function getClosureUsedVariables(){"` |
|    - |  503 | `"  $i = $this->__rfinfo();"` |
|    - |  504 | `"  return isset($i['used']) ? $i['used'] : array();"` |
|    - |  505 | `" }"` |
|    - |  506 | `" public function getExtensionName(){ $i = $this->__rfinfo(); return $i['internal'] ? 'Core' : false; }"` |
|    - |  507 | `" public function getExtension(){ $i = $this->__rfinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|    - |  508 | `" public function getAttributes($name = null, $flags = 0){"` |
|    - |  509 | `"  $i = $this->__rfinfo();"` |
|    - |  510 | `"  if($this instanceof ReflectionMethod){"` |
|    - |  511 | `"   $spec = array('method', $this->class, $this->name, 0);"` |
|    - |  512 | `"   $target = 4;"` |
|    - |  513 | `"  }else{"` |
|    - |  514 | `"   $spec = array('fn', $this->__rftarget(), null, 0);"` |
|    - |  515 | `"   $target = 2;"` |
|    - |  516 | `"  }"` |
|    - |  517 | `"  return __reflect_build_attrs($i['attrs'], $spec, $target, $name, $flags);"` |
|    - |  518 | `" }"` |
|    - |  519 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|    - |  520 | `"}"` |
|    - |  521 | `"class ReflectionFunction extends ReflectionFunctionAbstract {"` |
|    - |  522 | `" const IS_DEPRECATED = 2048;"` |
|    - |  523 | `" public function __construct($function){"` |
|    - |  524 | `"  if($function instanceof Closure){"` |
|    - |  525 | `"   $this->__cl = $function;"` |
|    - |  526 | `"   $i = $this->__rfinfo();"` |
|    - |  527 | `"   if($i['closure']){"` |
|    - |  528 | `"    $f = $i['file'] === false ? '' : $i['file'];"` |
|    - |  529 | `"    $this->name = '{closure:'.$f.':'.$i['line'].'}';"` |
|    - |  530 | `"   }else{"` |
|    - |  531 | `"    $this->name = $i['name'];"` |
|    - |  532 | `"   }"` |
|    - |  533 | `"   return;"` |
|    - |  534 | `"  }"` |
|    - |  535 | `"  if(!is_string($function)){"` |
|    - |  536 | `"   throw new TypeError('ReflectionFunction::__construct(): Argument #1 ($function) must be of type Closure\|string, '.get_debug_type($function).' given');"` |
|    - |  537 | `"  }"` |
|    - |  538 | `"  $i = __reflect_func_info($function);"` |
|    - |  539 | `"  if($i === null){"` |
|    - |  540 | `"   throw new ReflectionException('Function '.$function.'() does not exist');"` |
|    - |  541 | `"  }"` |
|    - |  542 | `"  if($i['closure']){"` |
|    - |  543 | `"   $this->name = '{closure:'.($i['file'] === false ? '' : $i['file']).':'.$i['line'].'}';"` |
|    - |  544 | `"   $this->__cl = __reflect_closure($function, null, null);"` |
|    - |  545 | `"  }else{"` |
|    - |  546 | `"   $this->name = $i['name'];"` |
|    - |  547 | `"  }"` |
|    - |  548 | `" }"` |
|    - |  549 | `" public function invoke(...$args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|    - |  550 | `" public function invokeArgs(array $args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|    - |  551 | `" public function getClosure(){"` |
|    - |  552 | `"  if($this->__cl !== null){ return $this->__cl; }"` |
|    - |  553 | `"  return __reflect_closure($this->name, null, null);"` |
|    - |  554 | `" }"` |
|    - |  555 | `" public function isAnonymous(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|    - |  556 | `" public function isDisabled(){ return false; }"` |
|    - |  557 | `"}"` |
|    - |  558 | `"class ReflectionMethod extends ReflectionFunctionAbstract {"` |
|    - |  559 | `" const IS_PUBLIC = 1;"` |
|    - |  560 | `" const IS_PROTECTED = 2;"` |
|    - |  561 | `" const IS_PRIVATE = 4;"` |
|    - |  562 | `" const IS_STATIC = 16;"` |
|    - |  563 | `" const IS_FINAL = 32;"` |
|    - |  564 | `" const IS_ABSTRACT = 64;"` |
|    - |  565 | `" public $class;"` |
|    - |  566 | `" public function __construct($objectOrMethod, $method = null){"` |
|    - |  567 | `"  if($method === null){"` |
|    - |  568 | `"   if(!is_string($objectOrMethod) \|\| strpos($objectOrMethod,'::') === false){"` |
|    - |  569 | `"    throw new TypeError('ReflectionMethod::__construct(): Argument #1 ($objectOrMethod) must be of type object\|string, '.get_debug_type($objectOrMethod).' given');"` |
|    - |  570 | `"   }"` |
|    - |  571 | `"   $p = strpos($objectOrMethod,'::');"` |
|    - |  572 | `"   $method = substr($objectOrMethod,$p+2);"` |
|    - |  573 | `"   $objectOrMethod = substr($objectOrMethod,0,$p);"` |
|    - |  574 | `"  }"` |
|    - |  575 | `"  $ci = __phl_rcinfo($objectOrMethod);"` |
|    - |  576 | `"  if($ci === null){"` |
|    - |  577 | `"   throw new ReflectionException('Class \"'.$objectOrMethod.'\" does not exist');"` |
|    - |  578 | `"  }"` |
|    - |  579 | `"  $this->class = $ci['name'];"` |
|    - |  580 | `"  $found = null;"` |
|    - |  581 | `"  if(isset($ci['methods'][$method])){"` |
|    - |  582 | `"   $found = $method;"` |
|    - |  583 | `"  }else{"` |
|    - |  584 | `"   $l = strtolower($method);"` |
|    - |  585 | `"   foreach($ci['methods'] as $k => $m){"` |
|    - |  586 | `"    if(strtolower($k) === $l){ $found = $k; break; }"` |
|    - |  587 | `"   }"` |
|    - |  588 | `"  }"` |
|    - |  589 | `"  if($found === null){"` |
|    - |  590 | `"   throw new ReflectionException('Method '.$this->class.'::'.$method.'() does not exist');"` |
|    - |  591 | `"  }"` |
|    - |  592 | `"  $this->name = $found;"` |
|    - |  593 | `" }"` |
|    - |  594 | `" public static function createFromMethodName($name){"` |
|    - |  595 | `"  return new ReflectionMethod($name);"` |
|    - |  596 | `" }"` |
|    - |  597 | `" protected function __rfinfo(){ return __reflect_func_info($this->class, $this->name); }"` |
|    - |  598 | `" protected function __rpspec(){ return array($this->class, $this->name); }"` |
|    - |  599 | `" public function getDeclaringClass(){"` |
|    - |  600 | `"  $i = $this->__rfinfo();"` |
|    - |  601 | `"  return new ReflectionClass($i['decl']);"` |
|    - |  602 | `" }"` |
|    - |  603 | `" public function getModifiers(){"` |
|    - |  604 | `"  $i = $this->__rfinfo();"` |
|    - |  605 | `"  $m = ($i['vis'] === 1 ? 1 : ($i['vis'] === 2 ? 2 : 4));"` |
|    - |  606 | `"  if($i['mstatic']){ $m \|= 16; }"` |
|    - |  607 | `"  if($i['abstract']){ $m \|= 64; }"` |
|    - |  608 | `"  if($i['final']){ $m \|= 32; }"` |
|    - |  609 | `"  return $m;"` |
|    - |  610 | `" }"` |
|    - |  611 | `" public function isPublic(){ $i = $this->__rfinfo(); return $i['vis'] === 1; }"` |
|    - |  612 | `" public function isProtected(){ $i = $this->__rfinfo(); return $i['vis'] === 2; }"` |
|    - |  613 | `" public function isPrivate(){ $i = $this->__rfinfo(); return $i['vis'] === 3; }"` |
|    - |  614 | `" public function isStatic(){ $i = $this->__rfinfo(); return $i['mstatic']; }"` |
|    - |  615 | `" public function isAbstract(){ $i = $this->__rfinfo(); return $i['abstract']; }"` |
|    - |  616 | `" public function isFinal(){ $i = $this->__rfinfo(); return $i['final']; }"` |
|    - |  617 | `" public function isConstructor(){ return strtolower($this->name) === '__construct'; }"` |
|    - |  618 | `" public function isDestructor(){ return strtolower($this->name) === '__destruct'; }"` |
|    - |  619 | `" public function invoke($object = null, ...$args){ return $this->__rinvoke($object, $args); }"` |
|    - |  620 | `" public function invokeArgs($object, array $args){ return $this->__rinvoke($object, $args); }"` |
|    - |  621 | `" protected function __rinvoke($object, $args){"` |
|    - |  622 | `"  $i = $this->__rfinfo();"` |
|    - |  623 | `"  if(!$i['mstatic']){"` |
|    - |  624 | `"   if(!is_object($object)){"` |
|    - |  625 | `"    throw new ReflectionException('Trying to invoke non static method '.$this->class.'::'.$this->name.'() without an object');"` |
|    - |  626 | `"   }"` |
|    - |  627 | `"   if(!is_a($object, $i['decl'])){"` |
|    - |  628 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|    - |  629 | `"   }"` |
|    - |  630 | `"  }else{"` |
|    - |  631 | `"   $object = null;"` |
|    - |  632 | `"  }"` |
|    - |  633 | `"  return __reflect_invoke($this->class, $this->name, $object, $args);"` |
|    - |  634 | `" }"` |
|    - |  635 | `" public function getClosure($object = null){"` |
|    - |  636 | `"  $i = $this->__rfinfo();"` |
|    - |  637 | `"  if(!$i['mstatic']){"` |
|    - |  638 | `"   if($object === null){"` |
|    - |  639 | `"    throw new ValueError('ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods');"` |
|    - |  640 | `"   }"` |
|    - |  641 | `"   if(!is_a($object, $i['decl'])){"` |
|    - |  642 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|    - |  643 | `"   }"` |
|    - |  644 | `"  }else{"` |
|    - |  645 | `"   $object = null;"` |
|    - |  646 | `"  }"` |
|    - |  647 | `"  return __reflect_closure($this->class, $this->name, $object);"` |
|    - |  648 | `" }"` |
|    - |  649 | `" public function setAccessible($accessible){ }"` |
|    - |  650 | `" public function hasPrototype(){ return $this->__rproto() !== null; }"` |
|    - |  651 | `" public function getPrototype(){"` |
|    - |  652 | `"  $p = $this->__rproto();"` |
|    - |  653 | `"  if($p === null){"` |
|    - |  654 | `"   throw new ReflectionException('Method '.$this->class.'::'.$this->name.' does not have a prototype');"` |
|    - |  655 | `"  }"` |
|    - |  656 | `"  return new ReflectionMethod($p, $this->name);"` |
|    - |  657 | `" }"` |
|    - |  658 | `" protected function __rproto(){"` |
|    - |  659 | `"  $ci = __phl_rcinfo($this->class);"` |
|    - |  660 | `"  $l = strtolower($this->name);"` |
|    - |  661 | `"  $p = $ci['parent'];"` |
|    - |  662 | `"  while($p !== null){"` |
|    - |  663 | `"   $pi = __phl_rcinfo($p);"` |
|    - |  664 | `"   foreach($pi['methods'] as $k => $m){"` |
|    - |  665 | `"    if(strtolower($k) === $l && $m['vis'] !== 3){ return $m['decl']; }"` |
|    - |  666 | `"   }"` |
|    - |  667 | `"   $p = $pi['parent'];"` |
|    - |  668 | `"  }"` |
|    - |  669 | `"  foreach($ci['interfaces'] as $if){"` |
|    - |  670 | `"   $ii = __phl_rcinfo($if);"` |
|    - |  671 | `"   foreach($ii['methods'] as $k => $m){"` |
|    - |  672 | `"    if(strtolower($k) === $l){ return $ii['name']; }"` |
|    - |  673 | `"   }"` |
|    - |  674 | `"  }"` |
|    - |  675 | `"  return null;"` |
|    - |  676 | `" }"` |
|    - |  677 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|    - |  678 | `"}"` |
|    - |  679 | `"class ReflectionParameter implements Reflector {"` |
|    - |  680 | `" public $name;"` |
|    - |  681 | `" protected $__t;"` |
|    - |  682 | `" protected $__m = null;"` |
|    - |  683 | `" protected $__p = 0;"` |
|    - |  684 | `" public function __construct($function, $param){"` |
|    - |  685 | `"  $m = null;"` |
|    - |  686 | `"  $t = $function;"` |
|    - |  687 | `"  if(is_array($function)){"` |
|    - |  688 | `"   $t = $function[0];"` |
|    - |  689 | `"   $m = $function[1];"` |
|    - |  690 | `"   if(is_object($t)){ $t = get_class($t); }"` |
|    - |  691 | `"  }else if(is_string($function) && strpos($function,'::') !== false){"` |
|    - |  692 | `"   $p = strpos($function,'::');"` |
|    - |  693 | `"   $m = substr($function,$p+2);"` |
|    - |  694 | `"   $t = substr($function,0,$p);"` |
|    - |  695 | `"  }"` |
|    - |  696 | `"  if($m !== null){"` |
|    - |  697 | `"   $rm = new ReflectionMethod($t, $m);"` |
|    - |  698 | `"   $t = $rm->class;"` |
|    - |  699 | `"   $m = $rm->name;"` |
|    - |  700 | `"   $i = __reflect_func_info($t, $m);"` |
|    - |  701 | `"  }else if($function instanceof Closure){"` |
|    - |  702 | `"   $t = $function;"` |
|    - |  703 | `"   $i = __reflect_func_info($function);"` |
|    - |  704 | `"  }else{"` |
|    - |  705 | `"   $i = __reflect_sig_fixup(__reflect_func_info($t));"` |
|    - |  706 | `"   if($i === null){"` |
|    - |  707 | `"    throw new ReflectionException('Function '.$t.'() does not exist');"` |
|    - |  708 | `"   }"` |
|    - |  709 | `"  }"` |
|    - |  710 | `"  $found = null;"` |
|    - |  711 | `"  if(is_int($param)){"` |
|    - |  712 | `"   if(isset($i['params'][$param])){ $found = $i['params'][$param]; }"` |
|    - |  713 | `"   if($found === null){"` |
|    - |  714 | `"    throw new ReflectionException('The parameter specified by its offset could not be found');"` |
|    - |  715 | `"   }"` |
|    - |  716 | `"  }else{"` |
|    - |  717 | `"   foreach($i['params'] as $pp){"` |
|    - |  718 | `"    if($pp['name'] === $param){ $found = $pp; break; }"` |
|    - |  719 | `"   }"` |
|    - |  720 | `"   if($found === null){"` |
|    - |  721 | `"    throw new ReflectionException('The parameter specified by its name could not be found');"` |
|    - |  722 | `"   }"` |
|    - |  723 | `"  }"` |
|    - |  724 | `"  $this->name = $found['name'];"` |
|    - |  725 | `"  $this->__t = $t;"` |
|    - |  726 | `"  $this->__m = $m;"` |
|    - |  727 | `"  $this->__p = $found['pos'];"` |
|    - |  728 | `" }"` |
|    - |  729 | `" protected function __rffull(){"` |
|    - |  730 | `"  if($this->__m !== null){ return __reflect_func_info($this->__t, $this->__m); }"` |
|    - |  731 | `"  return __reflect_sig_fixup(__reflect_func_info($this->__t));"` |
|    - |  732 | `" }"` |
|    - |  733 | `" protected function __rpinfo(){"` |
|    - |  734 | `"  $i = $this->__rffull();"` |
|    - |  735 | `"  return $i['params'][$this->__p];"` |
|    - |  736 | `" }"` |
|    - |  737 | `" public function getName(){ return $this->name; }"` |
|    - |  738 | `" public function getPosition(){ return $this->__p; }"` |
|    - |  739 | `" public function isPassedByReference(){ $p = $this->__rpinfo(); return $p['byref']; }"` |
|    - |  740 | `" public function canBePassedByValue(){ return !$this->isPassedByReference(); }"` |
|    - |  741 | `" public function isVariadic(){ $p = $this->__rpinfo(); return $p['variadic']; }"` |
|    - |  742 | `" public function isPromoted(){ $p = $this->__rpinfo(); return $p['promoted']; }"` |
|    - |  743 | `" public function isDefaultValueAvailable(){ $p = $this->__rpinfo(); return $p['hasdef']; }"` |
|    - |  744 | `" public function isOptional(){"` |
|    - |  745 | `"  $i = $this->__rffull();"` |
|    - |  746 | `"  $n = count($i['params']);"` |
|    - |  747 | `"  for($k = $this->__p; $k < $n; $k++){"` |
|    - |  748 | `"   $p = $i['params'][$k];"` |
|    - |  749 | `"   if(!$p['variadic'] && !$p['hasdef']){ return false; }"` |
|    - |  750 | `"  }"` |
|    - |  751 | `"  return true;"` |
|    - |  752 | `" }"` |
|    - |  753 | `" public function getDefaultValue(){"` |
|    - |  754 | `"  if(!$this->isDefaultValueAvailable()){"` |
|    - |  755 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|    - |  756 | `"  }"` |
|    - |  757 | `"  $p = $this->__rpinfo();"` |
|    - |  758 | `"  if(isset($p['deftext'])){"` |
|    - |  759 | `"   $s = __reflect_sig_scalar($p['deftext']);"` |
|    - |  760 | `"   if($s[0]){ return $s[1]; }"` |
|    - |  761 | `"   if($p['deftext'] === 'array (' \|\| strpos($p['deftext'], '[') === 0){ return array(); }"` |
|    - |  762 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|    - |  763 | `"  }"` |
|    - |  764 | `"  return __reflect_param_default($this->__t, $this->__m, $this->__p);"` |
|    - |  765 | `" }"` |
|    - |  766 | `" public function isDefaultValueConstant(){"` |
|    - |  767 | `"  if(!$this->isDefaultValueAvailable()){ return false; }"` |
|    - |  768 | `"  $p = $this->__rpinfo();"` |
|    - |  769 | `"  if(isset($p['deftext'])){ return false; }"` |
|    - |  770 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p) !== null;"` |
|    - |  771 | `" }"` |
|    - |  772 | `" public function getDefaultValueConstantName(){"` |
|    - |  773 | `"  if(!$this->isDefaultValueAvailable()){"` |
|    - |  774 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|    - |  775 | `"  }"` |
|    - |  776 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p);"` |
|    - |  777 | `" }"` |
|    - |  778 | `" public function allowsNull(){"` |
|    - |  779 | `"  $p = $this->__rpinfo();"` |
|    - |  780 | `"  if($p['typetext'] === null){ return true; }"` |
|    - |  781 | `"  if($p['nullable']){ return true; }"` |
|    - |  782 | `"  return $p['typetext'] === 'mixed' \|\| $p['typetext'] === 'null';"` |
|    - |  783 | `" }"` |
|    - |  784 | `" public function hasType(){ $p = $this->__rpinfo(); return $p['typetext'] !== null; }"` |
|    - |  785 | `" public function getType(){ $p = $this->__rpinfo(); return __reflect_make_type($p['typetext']); }"` |
|    - |  786 | `" public function getDeclaringFunction(){"` |
|    - |  787 | `"  if($this->__m !== null){ return new ReflectionMethod($this->__t, $this->__m); }"` |
|    - |  788 | `"  return new ReflectionFunction($this->__t);"` |
|    - |  789 | `" }"` |
|    - |  790 | `" public function getDeclaringClass(){"` |
|    - |  791 | `"  if($this->__m === null){ return null; }"` |
|    - |  792 | `"  $i = $this->__rffull();"` |
|    - |  793 | `"  return new ReflectionClass($i['decl']);"` |
|    - |  794 | `" }"` |
|    - |  795 | `" public function getAttributes($name = null, $flags = 0){"` |
|    - |  796 | `"  $p = $this->__rpinfo();"` |
|    - |  797 | `"  return __reflect_build_attrs($p['attrs'], array('param', $this->__t, $this->__m, $this->__p), 32, $name, $flags);"` |
|    - |  798 | `" }"` |
|    - |  799 | `" public function __toString(){ return __reflect_export_param($this); }"` |
|    - |  800 | `"}"` |
|    - |  801 | `;` |
|    - |  802 | `/*` |
|    - |  803 | ` * Chunk 3: PropertyHookType, ReflectionProperty, ReflectionClassConstant.` |
|    - |  804 | ` */` |
|    - |  805 | `static const char zReflectLib3[] =` |
|    - |  806 | `"enum PropertyHookType: string {"` |
|    - |  807 | `" case Get = 'get';"` |
|    - |  808 | `" case Set = 'set';"` |
|    - |  809 | `"}"` |
|    - |  810 | `"class ReflectionProperty implements Reflector {"` |
|    - |  811 | `" const IS_PUBLIC = 1;"` |
|    - |  812 | `" const IS_PROTECTED = 2;"` |
|    - |  813 | `" const IS_PRIVATE = 4;"` |
|    - |  814 | `" const IS_STATIC = 16;"` |
|    - |  815 | `" const IS_FINAL = 32;"` |
|    - |  816 | `" const IS_ABSTRACT = 64;"` |
|    - |  817 | `" const IS_READONLY = 128;"` |
|    - |  818 | `" const IS_VIRTUAL = 512;"` |
|    - |  819 | `" const IS_PROTECTED_SET = 2048;"` |
|    - |  820 | `" const IS_PRIVATE_SET = 4096;"` |
|    - |  821 | `" public $name;"` |
|    - |  822 | `" public $class;"` |
|    - |  823 | `" protected $__dynobj = null;"` |
|    - |  824 | `" public function __construct($class, $property){"` |
|    - |  825 | `"  $obj = null;"` |
|    - |  826 | `"  if(is_object($class)){ $obj = $class; }"` |
|    - |  827 | `"  else if(!is_string($class)){"` |
|    - |  828 | `"   throw new TypeError('ReflectionProperty::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|    - |  829 | `"  }"` |
|    - |  830 | `"  $ci = __phl_rcinfo($class);"` |
|    - |  831 | `"  if($ci === null){"` |
|    - |  832 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|    - |  833 | `"  }"` |
|    - |  834 | `"  $this->class = $ci['name'];"` |
|    - |  835 | `"  if(isset($ci['props'][$property])){"` |
|    - |  836 | `"   $this->name = $property;"` |
|    - |  837 | `"   return;"` |
|    - |  838 | `"  }"` |
|    - |  839 | `"  if($obj !== null && (__reflect_prop_state($obj, $property) & 1)){"` |
|    - |  840 | `"   $this->name = $property;"` |
|    - |  841 | `"   $this->__dynobj = $obj;"` |
|    - |  842 | `"   return;"` |
|    - |  843 | `"  }"` |
|    - |  844 | `"  throw new ReflectionException('Property '.$this->class.'::$'.$property.' does not exist');"` |
|    - |  845 | `" }"` |
|    - |  846 | `" protected function __rpmeta(){"` |
|    - |  847 | `"  $ci = __phl_rcinfo($this->class);"` |
|    - |  848 | `"  if(isset($ci['props'][$this->name])){ return $ci['props'][$this->name]; }"` |
|    - |  849 | `"  return array('vis' => 1, 'static' => false, 'readonly' => false, 'hasdef' => false,"` |
|    - |  850 | `"   'typed' => false, 'typetext' => null, 'decl' => $this->class, 'line' => 0, 'dyn' => true);"` |
|    - |  851 | `" }"` |
|    - |  852 | `" public function getName(){ return $this->name; }"` |
|    - |  853 | `" public function getDeclaringClass(){"` |
|    - |  854 | `"  $m = $this->__rpmeta();"` |
|    - |  855 | `"  return new ReflectionClass($m['decl']);"` |
|    - |  856 | `" }"` |
|    - |  857 | `" public function getModifiers(){"` |
|    - |  858 | `"  $m = $this->__rpmeta();"` |
|    - |  859 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|    - |  860 | `"  if($m['static']){ $mod \|= 16; }"` |
|    - |  861 | `"  if($m['readonly']){ $mod \|= 128; }"` |
|    - |  862 | `"  return $mod;"` |
|    - |  863 | `" }"` |
|    - |  864 | `" public function isPublic(){ $m = $this->__rpmeta(); return $m['vis'] === 1; }"` |
|    - |  865 | `" public function isProtected(){ $m = $this->__rpmeta(); return $m['vis'] === 2; }"` |
|    - |  866 | `" public function isPrivate(){ $m = $this->__rpmeta(); return $m['vis'] === 3; }"` |
|    - |  867 | `" public function isStatic(){ $m = $this->__rpmeta(); return $m['static']; }"` |
|    - |  868 | `" public function isReadOnly(){ $m = $this->__rpmeta(); return $m['readonly']; }"` |
|    - |  869 | `" public function isPrivateSet(){ $m = $this->__rpmeta(); return isset($m['privset']) ? $m['privset'] : false; }"` |
|    - |  870 | `" public function isProtectedSet(){ $m = $this->__rpmeta(); return isset($m['protset']) ? $m['protset'] : false; }"` |
|    - |  871 | `" public function isDefault(){ $m = $this->__rpmeta(); return !isset($m['dyn']); }"` |
|    - |  872 | `" public function isDynamic(){ $m = $this->__rpmeta(); return isset($m['dyn']); }"` |
|    - |  873 | `" public function isAbstract(){ return false; }"` |
|    - |  874 | `" public function isFinal(){ return false; }"` |
|    - |  875 | `" public function isVirtual(){ $m = $this->__rpmeta(); return isset($m['virtual']) ? $m['virtual'] : false; }"` |
|    - |  876 | `" public function hasHooks(){ $m = $this->__rpmeta();"` |
|    - |  877 | `"  return (isset($m['hookget']) && $m['hookget']) \|\| (isset($m['hookset']) && $m['hookset']); }"` |
|    - |  878 | `" public function getHooks(){"` |
|    - |  879 | `"  $m = $this->__rpmeta(); $h = array();"` |
|    - |  880 | `"  if(isset($m['hookget']) && $m['hookget']){ $h['get'] = new ReflectionMethod($m['decl'], '__phl_hook_get_'.$this->name); }"` |
|    - |  881 | `"  if(isset($m['hookset']) && $m['hookset']){ $h['set'] = new ReflectionMethod($m['decl'], '__phl_hook_set_'.$this->name); }"` |
|    - |  882 | `"  return $h; }"` |
|    - |  883 | `" public function hasHook($type){"` |
|    - |  884 | `"  $t = $type instanceof PropertyHookType ? $type->value : $type;"` |
|    - |  885 | `"  $m = $this->__rpmeta();"` |
|    - |  886 | `"  if($t === 'get'){ return isset($m['hookget']) && $m['hookget']; }"` |
|    - |  887 | `"  if($t === 'set'){ return isset($m['hookset']) && $m['hookset']; }"` |
|    - |  888 | `"  return false; }"` |
|    - |  889 | `" public function getHook($type){"` |
|    - |  890 | `"  $t = $type instanceof PropertyHookType ? $type->value : $type;"` |
|    - |  891 | `"  $h = $this->getHooks();"` |
|    - |  892 | `"  return isset($h[$t]) ? $h[$t] : null; }"` |
|    - |  893 | `" public function isLazy($object){ return false; }"` |
|    - |  894 | `" public function setAccessible($accessible){ }"` |
|    - |  895 | `" public function getValue($object = null){"` |
|    - |  896 | `"  $m = $this->__rpmeta();"` |
|    - |  897 | `"  if($m['static']){ return __reflect_static_value($this->class, $this->name); }"` |
|    - |  898 | `"  if(!is_object($object)){"` |
|    - |  899 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|    - |  900 | `"  }"` |
|    - |  901 | `"  return __reflect_prop_read($object, $this->name);"` |
|    - |  902 | `" }"` |
|    - |  903 | `" public function setValue($objectOrValue = null, $value = null){"` |
|    - |  904 | `"  $m = $this->__rpmeta();"` |
|    - |  905 | `"  if($m['static']){"` |
|    - |  906 | `"   if($value === null && $objectOrValue !== null && !is_object($objectOrValue)){"` |
|    - |  907 | `"    __reflect_static_set($this->class, $this->name, $objectOrValue);"` |
|    - |  908 | `"   }else{"` |
|    - |  909 | `"    __reflect_static_set($this->class, $this->name, $value);"` |
|    - |  910 | `"   }"` |
|    - |  911 | `"   return;"` |
|    - |  912 | `"  }"` |
|    - |  913 | `"  __reflect_prop_write($objectOrValue, $this->name, $value);"` |
|    - |  914 | `" }"` |
|    - |  915 | `" public function getRawValue($object){ return $this->getValue($object); }"` |
|    - |  916 | `" public function setRawValue($object, $value){ $this->setValue($object, $value); }"` |
|    - |  917 | `" public function isInitialized($object = null){"` |
|    - |  918 | `"  $m = $this->__rpmeta();"` |
|    - |  919 | `"  if($m['static']){ return (__reflect_prop_state($this->class, $this->name) & 2) !== 0; }"` |
|    - |  920 | `"  if(!is_object($object)){"` |
|    - |  921 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|    - |  922 | `"  }"` |
|    - |  923 | `"  return (__reflect_prop_state($object, $this->name) & 2) !== 0;"` |
|    - |  924 | `" }"` |
|    - |  925 | `" public function hasDefaultValue(){"` |
|    - |  926 | `"  $m = $this->__rpmeta();"` |
|    - |  927 | `"  if(isset($m['dyn'])){ return false; }"` |
|    - |  928 | `"  if($m['hasdef']){ return true; }"` |
|    - |  929 | `"  return !$m['typed'];"` |
|    - |  930 | `" }"` |
|    - |  931 | `" public function getDefaultValue(){"` |
|    - |  932 | `"  $m = $this->__rpmeta();"` |
|    - |  933 | `"  if(isset($m['dyn']) \|\| !$m['hasdef']){ return null; }"` |
|    - |  934 | `"  return __reflect_prop_default($this->class, $this->name);"` |
|    - |  935 | `" }"` |
|    - |  936 | `" public function hasType(){ $m = $this->__rpmeta(); return $m['typed']; }"` |
|    - |  937 | `" public function getType(){ $m = $this->__rpmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|    - |  938 | `" public function getSettableType(){ return $this->getType(); }"` |
|    - |  939 | `" public function setRawValueWithoutLazyInitialization($object, $value){"` |
|    - |  940 | `"  throw new Error('ReflectionProperty::setRawValueWithoutLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|    - |  941 | `" }"` |
|    - |  942 | `" public function skipLazyInitialization($object){"` |
|    - |  943 | `"  throw new Error('ReflectionProperty::skipLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|    - |  944 | `" }"` |
|    - |  945 | `" public function getDocComment(){ $m = $this->__rpmeta(); return isset($m['doc']) ? $m['doc'] : false; }"` |
|    - |  946 | `" public function getAttributes($name = null, $flags = 0){"` |
|    - |  947 | `"  $m = $this->__rpmeta();"` |
|    - |  948 | `"  if(!isset($m['attrs'])){ return array(); }"` |
|    - |  949 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 8, $name, $flags);"` |
|    - |  950 | `" }"` |
|    - |  951 | `" public function __toString(){ return __reflect_export_prop($this); }"` |
|    - |  952 | `"}"` |
|    - |  953 | `"class ReflectionClassConstant implements Reflector {"` |
|    - |  954 | `" const IS_PUBLIC = 1;"` |
|    - |  955 | `" const IS_PROTECTED = 2;"` |
|    - |  956 | `" const IS_PRIVATE = 4;"` |
|    - |  957 | `" const IS_FINAL = 32;"` |
|    - |  958 | `" public $name;"` |
|    - |  959 | `" public $class;"` |
|    - |  960 | `" public function __construct($class, $constant){"` |
|    - |  961 | `"  if(!is_object($class) && !is_string($class)){"` |
|    - |  962 | `"   throw new TypeError('ReflectionClassConstant::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|    - |  963 | `"  }"` |
|    - |  964 | `"  $ci = __phl_rcinfo($class);"` |
|    - |  965 | `"  if($ci === null){"` |
|    - |  966 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|    - |  967 | `"  }"` |
|    - |  968 | `"  $this->class = $ci['name'];"` |
|    - |  969 | `"  if(!isset($ci['consts'][$constant])){"` |
|    - |  970 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' does not exist');"` |
|    - |  971 | `"  }"` |
|    - |  972 | `"  $this->name = $constant;"` |
|    - |  973 | `" }"` |
|    - |  974 | `" protected function __rcmeta(){"` |
|    - |  975 | `"  $ci = __phl_rcinfo($this->class);"` |
|    - |  976 | `"  return $ci['consts'][$this->name];"` |
|    - |  977 | `" }"` |
|    - |  978 | `" public function getName(){ return $this->name; }"` |
|    - |  979 | `" public function getValue(){ return __reflect_const_value($this->class, $this->name); }"` |
|    - |  980 | `" public function getDeclaringClass(){"` |
|    - |  981 | `"  $m = $this->__rcmeta();"` |
|    - |  982 | `"  return new ReflectionClass($m['decl']);"` |
|    - |  983 | `" }"` |
|    - |  984 | `" public function getModifiers(){"` |
|    - |  985 | `"  $m = $this->__rcmeta();"` |
|    - |  986 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|    - |  987 | `"  if($m['final']){ $mod \|= 32; }"` |
|    - |  988 | `"  return $mod;"` |
|    - |  989 | `" }"` |
|    - |  990 | `" public function isPublic(){ $m = $this->__rcmeta(); return $m['vis'] === 1; }"` |
|    - |  991 | `" public function isProtected(){ $m = $this->__rcmeta(); return $m['vis'] === 2; }"` |
|    - |  992 | `" public function isPrivate(){ $m = $this->__rcmeta(); return $m['vis'] === 3; }"` |
|    - |  993 | `" public function isFinal(){ $m = $this->__rcmeta(); return $m['final']; }"` |
|    - |  994 | `" public function isEnumCase(){ $m = $this->__rcmeta(); return $m['enumcase']; }"` |
|    - |  995 | `" public function isDeprecated(){ $m = $this->__rcmeta(); return __reflect_has_deprecated($m['attrs']); }"` |
|    - |  996 | `" public function hasType(){ $m = $this->__rcmeta(); return $m['typed']; }"` |
|    - |  997 | `" public function getType(){ $m = $this->__rcmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|    - |  998 | `" public function getDocComment(){ $m = $this->__rcmeta(); return $m['doc']; }"` |
|    - |  999 | `" public function getAttributes($name = null, $flags = 0){"` |
|    - | 1000 | `"  $m = $this->__rcmeta();"` |
|    - | 1001 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 16, $name, $flags);"` |
|    - | 1002 | `" }"` |
|    - | 1003 | `" public function __toString(){ return __reflect_export_cconst($this); }"` |
|    - | 1004 | `"}"` |
|    - | 1005 | `;` |
|    - | 1006 | `/*` |
|    - | 1007 | ` * Chunk 4: the ReflectionType family, built from the engine's canonical` |
|    - | 1008 | ` * type text ("?int", "string\|float", "(A&B)\|C" — normalized at compile` |
|    - | 1009 | ` * time). __reflect_make_type is the internal factory; PHP itself never` |
|    - | 1010 | ` * lets user code construct these, so the public constructors here are a` |
|    - | 1011 | ` * recorded PHL-only surface.` |
|    - | 1012 | ` */` |
|    - | 1013 | `static const char zReflectLib4[] =` |
|    - | 1014 | `"abstract class ReflectionType implements Stringable {"` |
|    - | 1015 | `" protected $__text = '';"` |
|    - | 1016 | `" protected $__nullable = false;"` |
|    - | 1017 | `" public function allowsNull(){ return $this->__nullable; }"` |
|    - | 1018 | `" public function __toString(){ return $this->__text; }"` |
|    - | 1019 | `"}"` |
|    - | 1020 | `"class ReflectionNamedType extends ReflectionType {"` |
|    - | 1021 | `" protected $__tname = '';"` |
|    - | 1022 | `" public function __construct($name = '', $nullable = false, $text = null){"` |
|    - | 1023 | `"  $this->__tname = $name;"` |
|    - | 1024 | `"  $l = strtolower($name);"` |
|    - | 1025 | `"  $this->__nullable = $nullable \|\| $l === 'null' \|\| $l === 'mixed';"` |
|    - | 1026 | `"  $this->__text = $text === null ? $name : $text;"` |
|    - | 1027 | `" }"` |
|    - | 1028 | `" public function getName(){ return $this->__tname; }"` |
|    - | 1029 | `" public function isBuiltin(){"` |
|    - | 1030 | `"  $l = strtolower($this->__tname);"` |
|    - | 1031 | `"  return in_array($l, array('int','float','string','bool','array','object','mixed',"` |
|    - | 1032 | `"   'void','never','null','callable','iterable','true','false'), true);"` |
|    - | 1033 | `" }"` |
|    - | 1034 | `"}"` |
|    - | 1035 | `"class ReflectionUnionType extends ReflectionType {"` |
|    - | 1036 | `" protected $__types = array();"` |
|    - | 1037 | `" public function __construct($text = '', $nullable = false, $types = array()){"` |
|    - | 1038 | `"  $this->__text = $text;"` |
|    - | 1039 | `"  $this->__nullable = $nullable;"` |
|    - | 1040 | `"  $this->__types = $types;"` |
|    - | 1041 | `" }"` |
|    - | 1042 | `" public function getTypes(){ return $this->__types; }"` |
|    - | 1043 | `"}"` |
|    - | 1044 | `"class ReflectionIntersectionType extends ReflectionType {"` |
|    - | 1045 | `" protected $__types = array();"` |
|    - | 1046 | `" public function __construct($text = '', $types = array()){"` |
|    - | 1047 | `"  $this->__text = $text;"` |
|    - | 1048 | `"  $this->__nullable = false;"` |
|    - | 1049 | `"  $this->__types = $types;"` |
|    - | 1050 | `" }"` |
|    - | 1051 | `" public function getTypes(){ return $this->__types; }"` |
|    - | 1052 | `"}"` |
|    - | 1053 | `"function __reflect_make_atom($p){"` |
|    - | 1054 | `" $nullable = false;"` |
|    - | 1055 | `" if($p !== '' && $p[0] === '?'){ $nullable = true; $p = substr($p, 1); }"` |
|    - | 1056 | `" if($p !== '' && $p[0] === '('){ $p = substr($p, 1, strlen($p) - 2); }"` |
|    - | 1057 | `" if(strpos($p, '&') !== false){"` |
|    - | 1058 | `"  $subs = array();"` |
|    - | 1059 | `"  foreach(explode('&', $p) as $s){ $subs[] = new ReflectionNamedType($s, false, $s); }"` |
|    - | 1060 | `"  return new ReflectionIntersectionType($p, $subs);"` |
|    - | 1061 | `" }"` |
|    - | 1062 | `" return new ReflectionNamedType($p, $nullable, $nullable ? '?'.$p : $p);"` |
|    - | 1063 | `"}"` |
|    - | 1064 | `"function __reflect_make_type($text){"` |
|    - | 1065 | `" if($text === null \|\| $text === ''){ return null; }"` |
|    - | 1066 | `" $nullable = false;"` |
|    - | 1067 | `" $body = $text;"` |
|    - | 1068 | `" if($body[0] === '?'){ $nullable = true; $body = substr($body, 1); }"` |
|    - | 1069 | `" $parts = array();"` |
|    - | 1070 | `" $depth = 0;"` |
|    - | 1071 | `" $cur = '';"` |
|    - | 1072 | `" $n = strlen($body);"` |
|    - | 1073 | `" for($k = 0; $k < $n; $k++){"` |
|    - | 1074 | `"  $ch = $body[$k];"` |
|    - | 1075 | `"  if($ch === '('){ $depth++; $cur .= $ch; }"` |
|    - | 1076 | `"  else if($ch === ')'){ $depth--; $cur .= $ch; }"` |
|    - | 1077 | `"  else if($ch === '\|' && $depth === 0){ $parts[] = $cur; $cur = ''; }"` |
|    - | 1078 | `"  else{ $cur .= $ch; }"` |
|    - | 1079 | `" }"` |
|    - | 1080 | `" $parts[] = $cur;"` |
|    - | 1081 | `" if(count($parts) > 1){"` |
|    - | 1082 | `"  $nonNull = array();"` |
|    - | 1083 | `"  $hasNull = false;"` |
|    - | 1084 | `"  foreach($parts as $p){"` |
|    - | 1085 | `"   if(strtolower($p) === 'null'){ $hasNull = true; }"` |
|    - | 1086 | `"   else{ $nonNull[] = $p; }"` |
|    - | 1087 | `"  }"` |
|    - | 1088 | `"  if($hasNull && count($nonNull) === 1 && strpos($nonNull[0], '&') === false){"` |
|    - | 1089 | `"   return new ReflectionNamedType($nonNull[0], true, '?'.$nonNull[0]);"` |
|    - | 1090 | `"  }"` |
|    - | 1091 | `"  $types = array();"` |
|    - | 1092 | `"  foreach($parts as $p){ $types[] = __reflect_make_atom($p); }"` |
|    - | 1093 | `"  return new ReflectionUnionType($body, $nullable \|\| $hasNull, $types);"` |
|    - | 1094 | `" }"` |
|    - | 1095 | `" if(strpos($body, '&') !== false){ return __reflect_make_atom($body); }"` |
|    - | 1096 | `" return __reflect_make_atom($nullable ? '?'.$body : $body);"` |
|    - | 1097 | `"}"` |
|    - | 1098 | `;` |
|    - | 1099 | `/*` |
|    - | 1100 | ` * Chunk 5: ReflectionGenerator, ReflectionFiber. Executing line/file and` |
|    - | 1101 | ` * traces need runtime line tracking the VM does not have (same gap as` |
|    - | 1102 | ` * debug_backtrace's line numbers) — those throw a loud Error, recorded in` |
|    - | 1103 | ` * the plan ledger.` |
|    - | 1104 | ` */` |
|    - | 1105 | `static const char zReflectLib5[] =` |
|    - | 1106 | `"class ReflectionGenerator {"` |
|    - | 1107 | `" protected $__gen;"` |
|    - | 1108 | `" public function __construct($generator){"` |
|    - | 1109 | `"  if(!($generator instanceof Generator)){"` |
|    - | 1110 | `"   throw new TypeError('ReflectionGenerator::__construct(): Argument #1 ($generator) must be of type Generator, '.get_debug_type($generator).' given');"` |
|    - | 1111 | `"  }"` |
|    - | 1112 | `"  $this->__gen = $generator;"` |
|    - | 1113 | `" }"` |
|    - | 1114 | `" protected function __rginfo(){ return __reflect_gen_info($this->__gen); }"` |
|    - | 1115 | `" public function getFunction(){"` |
|    - | 1116 | `"  $i = $this->__rginfo();"` |
|    - | 1117 | `"  if($i['kind'] === 'method'){ return new ReflectionMethod($i['class'], $i['name']); }"` |
|    - | 1118 | `"  return new ReflectionFunction($i['name']);"` |
|    - | 1119 | `" }"` |
|    - | 1120 | `" public function getThis(){ $i = $this->__rginfo(); return isset($i['this']) ? $i['this'] : null; }"` |
|    - | 1121 | `" public function getExecutingGenerator(){ return __reflect_gen_exec($this->__gen); }"` |
|    - | 1122 | `" public function isClosed(){ $i = $this->__rginfo(); return $i['closed']; }"` |
|    - | 1123 | `" public function getExecutingLine(){"` |
|    - | 1124 | `"  throw new Error('ReflectionGenerator::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|    - | 1125 | `" }"` |
|    - | 1126 | `" public function getExecutingFile(){"` |
|    - | 1127 | `"  throw new Error('ReflectionGenerator::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|    - | 1128 | `" }"` |
|    - | 1129 | `" public function getTrace($options = 1){"` |
|    - | 1130 | `"  throw new Error('ReflectionGenerator::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|    - | 1131 | `" }"` |
|    - | 1132 | `"}"` |
|    - | 1133 | `"class ReflectionFiber {"` |
|    - | 1134 | `" protected $__fiber;"` |
|    - | 1135 | `" public function __construct($fiber){"` |
|    - | 1136 | `"  if(!($fiber instanceof Fiber)){"` |
|    - | 1137 | `"   throw new TypeError('ReflectionFiber::__construct(): Argument #1 ($fiber) must be of type Fiber, '.get_debug_type($fiber).' given');"` |
|    - | 1138 | `"  }"` |
|    - | 1139 | `"  $this->__fiber = $fiber;"` |
|    - | 1140 | `" }"` |
|    - | 1141 | `" public function getFiber(){ return $this->__fiber; }"` |
|    - | 1142 | `" public function getCallable(){ return __reflect_prop_read($this->__fiber, '__callable'); }"` |
|    - | 1143 | `" public function getExecutingLine(){"` |
|    - | 1144 | `"  throw new Error('ReflectionFiber::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|    - | 1145 | `" }"` |
|    - | 1146 | `" public function getExecutingFile(){"` |
|    - | 1147 | `"  throw new Error('ReflectionFiber::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|    - | 1148 | `" }"` |
|    - | 1149 | `" public function getTrace($options = 1){"` |
|    - | 1150 | `"  throw new Error('ReflectionFiber::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|    - | 1151 | `" }"` |
|    - | 1152 | `"}"` |
|    - | 1153 | `;` |
|    - | 1154 | `/*` |
|    - | 1155 | ` * Chunk 6: the long tail — ReflectionConstant (PHP 8.5), the synthetic` |
|    - | 1156 | ` * "Core" ReflectionExtension, ReflectionZendExtension (throws: no Zend` |
|    - | 1157 | ` * extensions exist), the ReflectionEnum family (throws: enums are not a` |
|    - | 1158 | ` * PHL language feature yet), and ReflectionReference.` |
|    - | 1159 | ` */` |
|    - | 1160 | `static const char zReflectLib6[] =` |
|    - | 1161 | `"class ReflectionConstant implements Reflector {"` |
|    - | 1162 | `" public $name;"` |
|    - | 1163 | `" public function __construct($name){"` |
|    - | 1164 | `"  if(!is_string($name)){"` |
|    - | 1165 | `"   throw new TypeError('ReflectionConstant::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|    - | 1166 | `"  }"` |
|    - | 1167 | `"  $i = __reflect_const_info($name);"` |
|    - | 1168 | `"  if($i === null){"` |
|    - | 1169 | `"   throw new ReflectionException('Constant \"'.$name.'\" does not exist');"` |
|    - | 1170 | `"  }"` |
|    - | 1171 | `"  $this->name = $name;"` |
|    - | 1172 | `" }"` |
|    - | 1173 | `" public function getName(){ return $this->name; }"` |
|    - | 1174 | `" public function getNamespaceName(){"` |
|    - | 1175 | `"  $p = strrpos($this->name,'\\\\');"` |
|    - | 1176 | `"  if($p === false){ return ''; }"` |
|    - | 1177 | `"  return substr($this->name,0,$p);"` |
|    - | 1178 | `" }"` |
|    - | 1179 | `" public function getShortName(){"` |
|    - | 1180 | `"  $p = strrpos($this->name,'\\\\');"` |
|    - | 1181 | `"  if($p === false){ return $this->name; }"` |
|    - | 1182 | `"  return substr($this->name,$p+1);"` |
|    - | 1183 | `" }"` |
|    - | 1184 | `" public function getValue(){"` |
|    - | 1185 | `"  $i = __reflect_const_info($this->name);"` |
|    - | 1186 | `"  return $i['value'];"` |
|    - | 1187 | `" }"` |
|    - | 1188 | `" public function isDeprecated(){ return false; }"` |
|    - | 1189 | `" public function getFileName(){"` |
|    - | 1190 | `"  $i = __reflect_const_info($this->name);"` |
|    - | 1191 | `"  return $i['file'];"` |
|    - | 1192 | `" }"` |
|    - | 1193 | `" public function getExtension(){"` |
|    - | 1194 | `"  $i = __reflect_const_info($this->name);"` |
|    - | 1195 | `"  return $i['internal'] ? new ReflectionExtension('Core') : null;"` |
|    - | 1196 | `" }"` |
|    - | 1197 | `" public function getExtensionName(){"` |
|    - | 1198 | `"  $i = __reflect_const_info($this->name);"` |
|    - | 1199 | `"  return $i['internal'] ? 'Core' : false;"` |
|    - | 1200 | `" }"` |
|    - | 1201 | `" public function getAttributes($name = null, $flags = 0){"` |
|    - | 1202 | `"  $i = __reflect_const_info($this->name);"` |
|    - | 1203 | `"  if($i === null){ return array(); }"` |
|    - | 1204 | `"  return __reflect_build_attrs($i['attrs'], array('const', $this->name, null, 0), 64, $name, $flags);"` |
|    - | 1205 | `" }"` |
|    - | 1206 | `" public function __toString(){"` |
|    - | 1207 | `"  return 'Constant [ '.$this->name.' ]'.\"\\n\";"` |
|    - | 1208 | `" }"` |
|    - | 1209 | `"}"` |
|    - | 1210 | `"class ReflectionExtension implements Reflector {"` |
|    - | 1211 | `" public $name;"` |
|    - | 1212 | `" public function __construct($name){"` |
|    - | 1213 | `"  if(!is_string($name)){"` |
|    - | 1214 | `"   throw new TypeError('ReflectionExtension::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|    - | 1215 | `"  }"` |
|    - | 1216 | `"  if(strtolower($name) !== 'core'){"` |
|    - | 1217 | `"   throw new ReflectionException('Extension \"'.$name.'\" does not exist');"` |
|    - | 1218 | `"  }"` |
|    - | 1219 | `"  $this->name = 'Core';"` |
|    - | 1220 | `" }"` |
|    - | 1221 | `" public function getName(){ return $this->name; }"` |
|    - | 1222 | `" public function getVersion(){ return phpversion(); }"` |
|    - | 1223 | `" public function getFunctions(){ return array(); }"` |
|    - | 1224 | `" public function getClasses(){ return array(); }"` |
|    - | 1225 | `" public function getClassNames(){ return array(); }"` |
|    - | 1226 | `" public function getConstants(){ return array(); }"` |
|    - | 1227 | `" public function getINIEntries(){ return array(); }"` |
|    - | 1228 | `" public function getDependencies(){ return array(); }"` |
|    - | 1229 | `" public function isPersistent(){ return true; }"` |
|    - | 1230 | `" public function isTemporary(){ return false; }"` |
|    - | 1231 | `" public function info(){ }"` |
|    - | 1232 | `" public function __toString(){"` |
|    - | 1233 | `"  return 'Extension [ extension #1 '.$this->name.' ]'.\"\\n\";"` |
|    - | 1234 | `" }"` |
|    - | 1235 | `"}"` |
|    - | 1236 | `"class ReflectionZendExtension implements Reflector {"` |
|    - | 1237 | `" public $name;"` |
|    - | 1238 | `" public function __construct($name){"` |
|    - | 1239 | `"  throw new ReflectionException('Zend Extension \"'.$name.'\" does not exist');"` |
|    - | 1240 | `" }"` |
|    - | 1241 | `" public function getName(){ return $this->name; }"` |
|    - | 1242 | `" public function __toString(){ return ''; }"` |
|    - | 1243 | `"}"` |
|    - | 1244 | `"class ReflectionEnum extends ReflectionClass {"` |
|    - | 1245 | `" public function __construct($objectOrClass){"` |
|    - | 1246 | `"  $info = __phl_rcinfo($objectOrClass);"` |
|    - | 1247 | `"  if($info === null){"` |
|    - | 1248 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|    - | 1249 | `"  }"` |
|    - | 1250 | `"  if(!$info['enum']){"` |
|    - | 1251 | `"   throw new ReflectionException('Class \"'.$info['name'].'\" is not an enum');"` |
|    - | 1252 | `"  }"` |
|    - | 1253 | `"  parent::__construct($objectOrClass);"` |
|    - | 1254 | `" }"` |
|    - | 1255 | `" public function hasCase($name){"` |
|    - | 1256 | `"  $i = $this->__rinfo();"` |
|    - | 1257 | `"  return in_array($name, $i['cases'], true);"` |
|    - | 1258 | `" }"` |
|    - | 1259 | `" public function getCase($name){"` |
|    - | 1260 | `"  if(!$this->hasCase($name)){"` |
|    - | 1261 | `"   throw new ReflectionException('Case '.$this->name.'::'.$name.' does not exist');"` |
|    - | 1262 | `"  }"` |
|    - | 1263 | `"  if($this->isBacked()){ return new ReflectionEnumBackedCase($this->name, $name); }"` |
|    - | 1264 | `"  return new ReflectionEnumUnitCase($this->name, $name);"` |
|    - | 1265 | `" }"` |
|    - | 1266 | `" public function getCases(){"` |
|    - | 1267 | `"  $i = $this->__rinfo();"` |
|    - | 1268 | `"  $out = array();"` |
|    - | 1269 | `"  foreach($i['cases'] as $c){"` |
|    - | 1270 | `"   $out[] = $this->isBacked()"` |
|    - | 1271 | `"    ? new ReflectionEnumBackedCase($this->name, $c)"` |
|    - | 1272 | `"    : new ReflectionEnumUnitCase($this->name, $c);"` |
|    - | 1273 | `"  }"` |
|    - | 1274 | `"  return $out;"` |
|    - | 1275 | `" }"` |
|    - | 1276 | `" public function isBacked(){ $i = $this->__rinfo(); return $i['enumbacking'] !== ''; }"` |
|    - | 1277 | `" public function getBackingType(){"` |
|    - | 1278 | `"  $i = $this->__rinfo();"` |
|    - | 1279 | `"  if($i['enumbacking'] === ''){ return null; }"` |
|    - | 1280 | `"  return __reflect_make_type($i['enumbacking']);"` |
|    - | 1281 | `" }"` |
|    - | 1282 | `"}"` |
|    - | 1283 | `"class ReflectionEnumUnitCase extends ReflectionClassConstant {"` |
|    - | 1284 | `" public function __construct($class, $constant){"` |
|    - | 1285 | `"  parent::__construct($class, $constant);"` |
|    - | 1286 | `"  $ci = __phl_rcinfo($class);"` |
|    - | 1287 | `"  if(!$ci['enum']){"` |
|    - | 1288 | `"   throw new ReflectionException('Class \"'.$this->class.'\" is not an enum');"` |
|    - | 1289 | `"  }"` |
|    - | 1290 | `"  $m = $this->__rcmeta();"` |
|    - | 1291 | `"  if(!$m['enumcase']){"` |
|    - | 1292 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' is not a case');"` |
|    - | 1293 | `"  }"` |
|    - | 1294 | `" }"` |
|    - | 1295 | `" public function getEnum(){ return new ReflectionEnum($this->class); }"` |
|    - | 1296 | `"}"` |
|    - | 1297 | `"class ReflectionEnumBackedCase extends ReflectionEnumUnitCase {"` |
|    - | 1298 | `" public function getBackingValue(){ return $this->getValue()->value; }"` |
|    - | 1299 | `"}"` |
|    - | 1300 | `"final class ReflectionReference {"` |
|    - | 1301 | `" protected $__id = '';"` |
|    - | 1302 | `" public function __construct(){"` |
|    - | 1303 | `"  throw new Error('Call to private ReflectionReference::__construct() from global scope');"` |
|    - | 1304 | `" }"` |
|    - | 1305 | `" public static function fromArrayElement($array, $key){"` |
|    - | 1306 | `"  if(!is_array($array)){"` |
|    - | 1307 | `"   throw new TypeError('ReflectionReference::fromArrayElement(): Argument #1 ($array) must be of type array, '.get_debug_type($array).' given');"` |
|    - | 1308 | `"  }"` |
|    - | 1309 | `"  $id = __reflect_ref_id($array, $key);"` |
|    - | 1310 | `"  if($id === null){ return null; }"` |
|    - | 1311 | `"  $r = __reflect_new_no_ctor('ReflectionReference');"` |
|    - | 1312 | `"  $r->__setId('phlref'.$id);"` |
|    - | 1313 | `"  return $r;"` |
|    - | 1314 | `" }"` |
|    - | 1315 | `" public function __setId($id){ $this->__id = $id; }"` |
|    - | 1316 | `" public function getId(){ return $this->__id; }"` |
|    - | 1317 | `"}"` |
|    - | 1318 | `;` |
|    - | 1319 | `/*` |
|    - | 1320 | ` * Chunk 7: ReflectionAttribute and the shared getAttributes() builder.` |
|    - | 1321 | ` * The spec array rides as [kind, target, member, paramIdx]; argument` |
|    - | 1322 | ` * values evaluate lazily through __reflect_attr_args (PHP semantics).` |
|    - | 1323 | ` */` |
|    - | 1324 | `static const char zReflectLib7[] =` |
|    - | 1325 | `"function __reflect_has_deprecated($meta){"` |
|    - | 1326 | `" foreach($meta as $a){"` |
|    - | 1327 | `"  if(strtolower($a['name']) === 'deprecated'){ return true; }"` |
|    - | 1328 | `" }"` |
|    - | 1329 | `" return false;"` |
|    - | 1330 | `"}"` |
|    - | 1331 | `"function __reflect_target_names($mask){"` |
|    - | 1332 | `" $parts = array();"` |
|    - | 1333 | `" foreach(array('class' => 1, 'function' => 2, 'method' => 4, 'property' => 8,"` |
|    - | 1334 | `"  'class constant' => 16, 'parameter' => 32, 'constant' => 64) as $nm => $bit){"` |
|    - | 1335 | `"  if($mask & $bit){ $parts[] = $nm; }"` |
|    - | 1336 | `" }"` |
|    - | 1337 | `" return implode(', ', $parts);"` |
|    - | 1338 | `"}"` |
|    - | 1339 | `"function __reflect_build_attrs($meta, $spec, $target, $name, $flags){"` |
|    - | 1340 | `" $out = array();"` |
|    - | 1341 | `" $counts = array();"` |
|    - | 1342 | `" foreach($meta as $a){"` |
|    - | 1343 | `"  $k = strtolower($a['name']);"` |
|    - | 1344 | `"  $counts[$k] = isset($counts[$k]) ? $counts[$k] + 1 : 1;"` |
|    - | 1345 | `" }"` |
|    - | 1346 | `" $idx = 0;"` |
|    - | 1347 | `" foreach($meta as $a){"` |
|    - | 1348 | `"  $keep = true;"` |
|    - | 1349 | `"  if($name !== null){"` |
|    - | 1350 | `"   $keep = strtolower($a['name']) === strtolower($name);"` |
|    - | 1351 | `"   if(!$keep && ($flags & 2)){"` |
|    - | 1352 | `"    $keep = is_subclass_of($a['name'], $name);"` |
|    - | 1353 | `"   }"` |
|    - | 1354 | `"  }"` |
|    - | 1355 | `"  if($keep){"` |
|    - | 1356 | `"   $r = __reflect_new_no_ctor('ReflectionAttribute');"` |
|    - | 1357 | `"   $r->__init($a['name'], $spec, $idx, $target, $counts[strtolower($a['name'])] > 1);"` |
|    - | 1358 | `"   $out[] = $r;"` |
|    - | 1359 | `"  }"` |
|    - | 1360 | `"  $idx++;"` |
|    - | 1361 | `" }"` |
|    - | 1362 | `" return $out;"` |
|    - | 1363 | `"}"` |
|    - | 1364 | `"final class ReflectionAttribute {"` |
|    - | 1365 | `" const IS_INSTANCEOF = 2;"` |
|    - | 1366 | `" protected $__name = '';"` |
|    - | 1367 | `" protected $__spec = null;"` |
|    - | 1368 | `" protected $__idx = 0;"` |
|    - | 1369 | `" protected $__target = 0;"` |
|    - | 1370 | `" protected $__rep = false;"` |
|    - | 1371 | `" public function __construct(){"` |
|    - | 1372 | `"  throw new Error('Call to private ReflectionAttribute::__construct() from global scope');"` |
|    - | 1373 | `" }"` |
|    - | 1374 | `" public function __init($name, $spec, $idx, $target, $rep){"` |
|    - | 1375 | `"  $this->__name = $name;"` |
|    - | 1376 | `"  $this->__spec = $spec;"` |
|    - | 1377 | `"  $this->__idx = $idx;"` |
|    - | 1378 | `"  $this->__target = $target;"` |
|    - | 1379 | `"  $this->__rep = $rep;"` |
|    - | 1380 | `" }"` |
|    - | 1381 | `" public function getName(){ return $this->__name; }"` |
|    - | 1382 | `" public function getTarget(){ return $this->__target; }"` |
|    - | 1383 | `" public function isRepeated(){ return $this->__rep; }"` |
|    - | 1384 | `" public function getArguments(){"` |
|    - | 1385 | `"  $a = __reflect_attr_args($this->__spec[0], $this->__spec[1], $this->__spec[2], $this->__spec[3], $this->__idx);"` |
|    - | 1386 | `"  return $a === null ? array() : $a;"` |
|    - | 1387 | `" }"` |
|    - | 1388 | `" public function newInstance(){"` |
|    - | 1389 | `"  $name = $this->__name;"` |
|    - | 1390 | `"  $ci = __phl_rcinfo($name);"` |
|    - | 1391 | `"  if($ci === null){"` |
|    - | 1392 | `"   throw new Error('Attribute class \"'.$name.'\" not found');"` |
|    - | 1393 | `"  }"` |
|    - | 1394 | `"  $name = $ci['name'];"` |
|    - | 1395 | `"  $decl = null;"` |
|    - | 1396 | `"  $didx = 0;"` |
|    - | 1397 | `"  foreach($ci['attrs'] as $a){"` |
|    - | 1398 | `"   if(strtolower($a['name']) === 'attribute'){ $decl = $didx; break; }"` |
|    - | 1399 | `"   $didx++;"` |
|    - | 1400 | `"  }"` |
|    - | 1401 | `"  if($decl === null){"` |
|    - | 1402 | `"   throw new Error('Attempting to use non-attribute class \"'.$name.'\" as attribute');"` |
|    - | 1403 | `"  }"` |
|    - | 1404 | `"  $dargs = __reflect_attr_args('class', $name, null, 0, $decl);"` |
|    - | 1405 | `"  $flags = 127;"` |
|    - | 1406 | `"  if(is_array($dargs)){"` |
|    - | 1407 | `"   if(isset($dargs[0])){ $flags = $dargs[0]; }"` |
|    - | 1408 | `"   else if(isset($dargs['flags'])){ $flags = $dargs['flags']; }"` |
|    - | 1409 | `"  }"` |
|    - | 1410 | `"  if(($flags & $this->__target) === 0){"` |
|    - | 1411 | `"   $tnames = array(1 => 'class', 2 => 'function', 4 => 'method', 8 => 'property',"` |
|    - | 1412 | `"    16 => 'class constant', 32 => 'parameter', 64 => 'constant');"` |
|    - | 1413 | `"   throw new Error('Attribute \"'.$name.'\" cannot target '.$tnames[$this->__target]"` |
|    - | 1414 | `"    .' (allowed targets: '.__reflect_target_names($flags).')');"` |
|    - | 1415 | `"  }"` |
|    - | 1416 | `"  if($this->__rep && ($flags & 128) === 0){"` |
|    - | 1417 | `"   throw new Error('Attribute \"'.$name.'\" must not be repeated');"` |
|    - | 1418 | `"  }"` |
|    - | 1419 | `"  return __reflect_new_instance($name, $this->getArguments());"` |
|    - | 1420 | `" }"` |
|    - | 1421 | `" public function __toString(){"` |
|    - | 1422 | `"  return 'Attribute [ '.$this->__name.' ]';"` |
|    - | 1423 | `" }"` |
|    - | 1424 | `"}"` |
|    - | 1425 | `;` |
|    - | 1426 | `/*` |
|    - | 1427 | ` * Chunk 8: signature-table support. Internal (C builtin) functions carry a` |
|    - | 1428 | ` * PHP-style parameter-list string; these helpers parse it into the same` |
|    - | 1429 | ` * param-meta shape user functions get, so ReflectionFunction and` |
|    - | 1430 | ` * ReflectionParameter work uniformly over builtins.` |
|    - | 1431 | ` */` |
|    - | 1432 | `static const char zReflectLib8[] =` |
|    - | 1433 | `"function __reflect_sig_split($sig){"` |
|    - | 1434 | `" $parts = array();"` |
|    - | 1435 | `" $cur = '';"` |
|    - | 1436 | `" $q = false;"` |
|    - | 1437 | `" $n = strlen($sig);"` |
|    - | 1438 | `" for($k = 0; $k < $n; $k++){"` |
|    - | 1439 | `"  $ch = $sig[$k];"` |
|    - | 1440 | `"  if($q){"` |
|    - | 1441 | `"   $cur .= $ch;"` |
|    - | 1442 | `"   if($ch === chr(92) && $k + 1 < $n){ $cur .= $sig[$k+1]; $k++; }"` |
|    - | 1443 | `"   else if($ch === chr(39)){ $q = false; }"` |
|    - | 1444 | `"  }else if($ch === chr(39)){ $q = true; $cur .= $ch; }"` |
|    - | 1445 | `"  else if($ch === ',' ){ $parts[] = trim($cur); $cur = ''; }"` |
|    - | 1446 | `"  else{ $cur .= $ch; }"` |
|    - | 1447 | `" }"` |
|    - | 1448 | `" if(trim($cur) !== ''){ $parts[] = trim($cur); }"` |
|    - | 1449 | `" return $parts;"` |
|    - | 1450 | `"}"` |
|    - | 1451 | `"function __reflect_sig_scalar($t){"` |
|    - | 1452 | `" if($t === '?'){ return array(false, null); }"` |
|    - | 1453 | `" if($t === 'NULL' \|\| $t === 'null'){ return array(true, null); }"` |
|    - | 1454 | `" if($t === 'true'){ return array(true, true); }"` |
|    - | 1455 | `" if($t === 'false'){ return array(true, false); }"` |
|    - | 1456 | `" if(is_numeric($t)){"` |
|    - | 1457 | `"  if(strpos($t, '.') === false && stripos($t, 'e') === false && strpos($t, 'x') === false){"` |
|    - | 1458 | `"   return array(true, (int)$t);"` |
|    - | 1459 | `"  }"` |
|    - | 1460 | `"  return array(true, (float)$t);"` |
|    - | 1461 | `" }"` |
|    - | 1462 | `" if(strlen($t) >= 2 && $t[0] === chr(39) && $t[strlen($t)-1] === chr(39)){"` |
|    - | 1463 | `"  $body = substr($t, 1, strlen($t) - 2);"` |
|    - | 1464 | `"  return array(true, strtr($body, array(chr(92).chr(39) => chr(39), chr(92).chr(92) => chr(92))));"` |
|    - | 1465 | `" }"` |
|    - | 1466 | `" return array(false, null);"` |
|    - | 1467 | `"}"` |
|    - | 1468 | `"function __reflect_parse_sig($sig){"` |
|    - | 1469 | `" $params = array();"` |
|    - | 1470 | `" $pos = 0;"` |
|    - | 1471 | `" foreach(__reflect_sig_split($sig) as $part){"` |
|    - | 1472 | `"  $deftext = null;"` |
|    - | 1473 | `"  $q = false;"` |
|    - | 1474 | `"  $n = strlen($part);"` |
|    - | 1475 | `"  for($k = 0; $k < $n; $k++){"` |
|    - | 1476 | `"   $ch = $part[$k];"` |
|    - | 1477 | `"   if($q){"` |
|    - | 1478 | `"    if($ch === chr(92)){ $k++; }"` |
|    - | 1479 | `"    else if($ch === chr(39)){ $q = false; }"` |
|    - | 1480 | `"   }else if($ch === chr(39)){ $q = true; }"` |
|    - | 1481 | `"   else if($ch === '=' ){"` |
|    - | 1482 | `"    $deftext = trim(substr($part, $k + 1));"` |
|    - | 1483 | `"    $part = trim(substr($part, 0, $k));"` |
|    - | 1484 | `"    break;"` |
|    - | 1485 | `"   }"` |
|    - | 1486 | `"  }"` |
|    - | 1487 | `"  $variadic = strpos($part, '...') !== false;"` |
|    - | 1488 | `"  $byref = strpos($part, '&') !== false;"` |
|    - | 1489 | `"  $d = strpos($part, '$');"` |
|    - | 1490 | `"  $name = $d === false ? $part : substr($part, $d + 1);"` |
|    - | 1491 | `"  $typetext = null;"` |
|    - | 1492 | `"  $sp = strpos($part, ' ');"` |
|    - | 1493 | `"  if($sp !== false && $d !== false && $sp < $d){ $typetext = substr($part, 0, $sp); }"` |
|    - | 1494 | `"  $nullable = $typetext !== null && ($typetext[0] === '?' \|\| stripos($typetext, 'null') !== false);"` |
|    - | 1495 | `"  $params[] = array('name' => $name, 'pos' => $pos, 'byref' => $byref,"` |
|    - | 1496 | `"   'variadic' => $variadic, 'hasdef' => $deftext !== null, 'nullable' => $nullable,"` |
|    - | 1497 | `"   'promoted' => false, 'typetext' => $typetext, 'attrs' => array(), 'deftext' => $deftext);"` |
|    - | 1498 | `"  $pos++;"` |
|    - | 1499 | `" }"` |
|    - | 1500 | `" return $params;"` |
|    - | 1501 | `"}"` |
|    - | 1502 | `"function __reflect_sig_fixup($i){"` |
|    - | 1503 | `" if($i === null){ return $i; }"` |
|    - | 1504 | `" if(isset($i['ret2'])){ $i['rettext'] = $i['ret2']; }"` |
|    - | 1505 | `" if(!isset($i['sig']) \|\| $i['sig'] === ''){ return $i; }"` |
|    - | 1506 | `" $i['params'] = __reflect_parse_sig($i['sig']);"` |
|    - | 1507 | `" $i['minarg'] = -1;"` |
|    - | 1508 | `" $v = false;"` |
|    - | 1509 | `" foreach($i['params'] as $p){ if($p['variadic']){ $v = true; } }"` |
|    - | 1510 | `" $i['variadic'] = $v;"` |
|    - | 1511 | `" return $i;"` |
|    - | 1512 | `"}"` |
|    - | 1513 | `;` |
|    - | 1514 | `/*` |
|    - | 1515 | ` * Chunk 9: PHP's Reflection export format (__toString on every Reflector).` |
|    - | 1516 | ` * Built entirely from the public reflection API of the target objects.` |
|    - | 1517 | ` */` |
|    - | 1518 | `static const char zReflectLib9[] =` |
|    - | 1519 | `"function __reflect_export_value($v){"` |
|    - | 1520 | `" if($v === null){ return 'NULL'; }"` |
|    - | 1521 | `" if($v === true){ return 'true'; }"` |
|    - | 1522 | `" if($v === false){ return 'false'; }"` |
|    - | 1523 | `" if(is_string($v)){ return chr(39).$v.chr(39); }"` |
|    - | 1524 | `" if(is_array($v)){"` |
|    - | 1525 | `"  $parts = array();"` |
|    - | 1526 | `"  $isList = true;"` |
|    - | 1527 | `"  $next = 0;"` |
|    - | 1528 | `"  foreach($v as $k => $x){"` |
|    - | 1529 | `"   if($k !== $next){ $isList = false; break; }"` |
|    - | 1530 | `"   $next++;"` |
|    - | 1531 | `"  }"` |
|    - | 1532 | `"  foreach($v as $k => $x){"` |
|    - | 1533 | `"   $parts[] = $isList ? __reflect_export_value($x)"` |
|    - | 1534 | `"    : (__reflect_export_value($k).' => '.__reflect_export_value($x));"` |
|    - | 1535 | `"  }"` |
|    - | 1536 | `"  return '['.implode(', ', $parts).']';"` |
|    - | 1537 | `" }"` |
|    - | 1538 | `" return (string)$v;"` |
|    - | 1539 | `"}"` |
|    - | 1540 | `"function __reflect_export_param($p){"` |
|    - | 1541 | `" $s = 'Parameter #'.$p->getPosition().' [ <'.($p->isOptional() ? 'optional' : 'required').'> ';"` |
|    - | 1542 | `" $t = $p->getType();"` |
|    - | 1543 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|    - | 1544 | `" if($p->isPassedByReference()){ $s .= '&'; }"` |
|    - | 1545 | `" if($p->isVariadic()){ $s .= '...'; }"` |
|    - | 1546 | `" $s .= '$'.$p->getName();"` |
|    - | 1547 | `" if($p->isDefaultValueAvailable()){"` |
|    - | 1548 | `"  try{ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|    - | 1549 | `"  catch(ReflectionException $e){ $s .= ' = <default>'; }"` |
|    - | 1550 | `" }"` |
|    - | 1551 | `" return $s.' ]';"` |
|    - | 1552 | `"}"` |
|    - | 1553 | `"function __reflect_export_prop($p){"` |
|    - | 1554 | `" $s = 'Property [ ';"` |
|    - | 1555 | `" $s .= $p->isPrivate() ? 'private ' : ($p->isProtected() ? 'protected ' : 'public ');"` |
|    - | 1556 | `" if($p->isStatic()){ $s .= 'static '; }"` |
|    - | 1557 | `" if($p->isReadOnly()){ $s .= 'readonly '; }"` |
|    - | 1558 | `" $t = $p->getType();"` |
|    - | 1559 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|    - | 1560 | `" $s .= '$'.$p->getName();"` |
|    - | 1561 | `" if($p->hasDefaultValue()){ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|    - | 1562 | `" return $s.' ]'.chr(10);"` |
|    - | 1563 | `"}"` |
|    - | 1564 | `"function __reflect_export_cconst($c){"` |
|    - | 1565 | `" $v = $c->getValue();"` |
|    - | 1566 | `" if(is_int($v)){ $t = 'int'; }"` |
|    - | 1567 | `" else if(is_string($v)){ $t = 'string'; }"` |
|    - | 1568 | `" else if(is_float($v)){ $t = 'float'; }"` |
|    - | 1569 | `" else if(is_bool($v)){ $t = 'bool'; }"` |
|    - | 1570 | `" else if(is_array($v)){ $t = 'array'; }"` |
|    - | 1571 | `" else{ $t = 'null'; }"` |
|    - | 1572 | `" $vs = is_array($v) ? 'Array' : (is_bool($v) ? ($v ? '1' : '') : (string)$v);"` |
|    - | 1573 | `" $vis = $c->isPrivate() ? 'private' : ($c->isProtected() ? 'protected' : 'public');"` |
|    - | 1574 | `" return 'Constant [ '.$vis.' '.$t.' '.$c->name.' ] { '.$vs.' }'.chr(10);"` |
|    - | 1575 | `"}"` |
|    - | 1576 | `"function __reflect_export_fnabs($r, $indent){"` |
|    - | 1577 | `" $tags = $r->isInternal() ? 'internal:Core' : 'user';"` |
|    - | 1578 | `" if($r instanceof ReflectionMethod){"` |
|    - | 1579 | `"  if($r->isConstructor()){ $tags .= ', ctor'; }"` |
|    - | 1580 | `"  else if($r->isDestructor()){ $tags .= ', dtor'; }"` |
|    - | 1581 | `"  $decl = $r->getDeclaringClass()->name;"` |
|    - | 1582 | `"  if(strtolower($decl) !== strtolower($r->class)){ $tags .= ', inherits '.$decl; }"` |
|    - | 1583 | `"  else if($r->hasPrototype()){ $tags .= ', prototype '.$r->getPrototype()->class; }"` |
|    - | 1584 | `"  $head = 'Method [ <'.$tags.'> ';"` |
|    - | 1585 | `"  if($r->isAbstract()){ $head .= 'abstract '; }"` |
|    - | 1586 | `"  if($r->isFinal()){ $head .= 'final '; }"` |
|    - | 1587 | `"  if($r->isStatic()){ $head .= 'static '; }"` |
|    - | 1588 | `"  $head .= $r->isPrivate() ? 'private ' : ($r->isProtected() ? 'protected ' : 'public ');"` |
|    - | 1589 | `"  $head .= 'method '.$r->name.' ]';"` |
|    - | 1590 | `" }else{"` |
|    - | 1591 | `"  $kind = $r->isClosure() ? 'Closure' : 'Function';"` |
|    - | 1592 | `"  $head = $kind.' [ <'.$tags.'> function '.$r->name.' ]';"` |
|    - | 1593 | `" }"` |
|    - | 1594 | `" $s = $head.' {'.chr(10);"` |
|    - | 1595 | `" if(!$r->isInternal()){"` |
|    - | 1596 | `"  $s .= '  @@ '.$r->getFileName().' '.$r->getStartLine().' - '.$r->getEndLine().chr(10);"` |
|    - | 1597 | `" }"` |
|    - | 1598 | `" $ps = $r->getParameters();"` |
|    - | 1599 | `" $ret = $r->getReturnType();"` |
|    - | 1600 | `" if(count($ps) > 0 \|\| $ret !== null){"` |
|    - | 1601 | `"  $s .= chr(10).'  - Parameters ['.count($ps).'] {'.chr(10);"` |
|    - | 1602 | `"  foreach($ps as $p){ $s .= '    '.__reflect_export_param($p).chr(10); }"` |
|    - | 1603 | `"  $s .= '  }'.chr(10);"` |
|    - | 1604 | `" }"` |
|    - | 1605 | `" if($ret !== null){ $s .= '  - Return [ '.(string)$ret.' ]'.chr(10); }"` |
|    - | 1606 | `" $s .= '}'.chr(10);"` |
|    - | 1607 | `" if($indent === ''){ return $s; }"` |
|    - | 1608 | `" $lines = explode(chr(10), $s);"` |
|    - | 1609 | `" $out = '';"` |
|    - | 1610 | `" $n = count($lines);"` |
|    - | 1611 | `" for($k = 0; $k < $n; $k++){"` |
|    - | 1612 | `"  if($lines[$k] === '' && $k === $n - 1){ break; }"` |
|    - | 1613 | `"  $out .= ($lines[$k] === '' ? '' : $indent.$lines[$k]).chr(10);"` |
|    - | 1614 | `" }"` |
|    - | 1615 | `" return $out;"` |
|    - | 1616 | `"}"` |
|    - | 1617 | `"function __reflect_export_class($rc){"` |
|    - | 1618 | `" $tags = $rc->isInternal() ? 'internal:Core' : 'user';"` |
|    - | 1619 | `" if($rc->isInterface()){"` |
|    - | 1620 | `"  $head = 'Interface [ <'.$tags.'> interface '.$rc->name.' ]';"` |
|    - | 1621 | `" }else{"` |
|    - | 1622 | `"  $mods = '';"` |
|    - | 1623 | `"  if($rc->isAbstract()){ $mods .= 'abstract '; }"` |
|    - | 1624 | `"  if($rc->isFinal()){ $mods .= 'final '; }"` |
|    - | 1625 | `"  $head = 'Class [ <'.$tags.'> '.$mods.'class '.$rc->name;"` |
|    - | 1626 | `"  $par = $rc->getParentClass();"` |
|    - | 1627 | `"  if($par !== false){ $head .= ' extends '.$par->name; }"` |
|    - | 1628 | `"  $ifs = $rc->getInterfaceNames();"` |
|    - | 1629 | `"  if(count($ifs) > 0){ $head .= ' implements '.implode(', ', $ifs); }"` |
|    - | 1630 | `"  $head .= ' ]';"` |
|    - | 1631 | `" }"` |
|    - | 1632 | `" $s = $head.' {'.chr(10);"` |
|    - | 1633 | `" if(!$rc->isInternal()){"` |
|    - | 1634 | `"  $s .= '  @@ '.$rc->getFileName().' '.$rc->getStartLine().'-'.$rc->getEndLine().chr(10);"` |
|    - | 1635 | `" }"` |
|    - | 1636 | `" $consts = $rc->getReflectionConstants();"` |
|    - | 1637 | `" $s .= chr(10).'  - Constants ['.count($consts).'] {'.chr(10);"` |
|    - | 1638 | `" foreach($consts as $c){ $s .= '    '.__reflect_export_cconst($c); }"` |
|    - | 1639 | `" $s .= '  }'.chr(10);"` |
|    - | 1640 | `" $sp = array();"` |
|    - | 1641 | `" $ip = array();"` |
|    - | 1642 | `" foreach($rc->getProperties() as $p){"` |
|    - | 1643 | `"  if($p->isStatic()){ $sp[] = $p; }else{ $ip[] = $p; }"` |
|    - | 1644 | `" }"` |
|    - | 1645 | `" $sm = array();"` |
|    - | 1646 | `" $im = array();"` |
|    - | 1647 | `" foreach($rc->getMethods() as $m){"` |
|    - | 1648 | `"  if($m->isStatic()){ $sm[] = $m; }else{ $im[] = $m; }"` |
|    - | 1649 | `" }"` |
|    - | 1650 | `" $s .= chr(10).'  - Static properties ['.count($sp).'] {'.chr(10);"` |
|    - | 1651 | `" foreach($sp as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|    - | 1652 | `" $s .= '  }'.chr(10);"` |
|    - | 1653 | `" $s .= chr(10).'  - Static methods ['.count($sm).'] {'.chr(10);"` |
|    - | 1654 | `" $first = true;"` |
|    - | 1655 | `" foreach($sm as $m){"` |
|    - | 1656 | `"  if(!$first){ $s .= chr(10); }"` |
|    - | 1657 | `"  $first = false;"` |
|    - | 1658 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|    - | 1659 | `" }"` |
|    - | 1660 | `" $s .= '  }'.chr(10);"` |
|    - | 1661 | `" $s .= chr(10).'  - Properties ['.count($ip).'] {'.chr(10);"` |
|    - | 1662 | `" foreach($ip as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|    - | 1663 | `" $s .= '  }'.chr(10);"` |
|    - | 1664 | `" $s .= chr(10).'  - Methods ['.count($im).'] {'.chr(10);"` |
|    - | 1665 | `" $first = true;"` |
|    - | 1666 | `" foreach($im as $m){"` |
|    - | 1667 | `"  if(!$first){ $s .= chr(10); }"` |
|    - | 1668 | `"  $first = false;"` |
|    - | 1669 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|    - | 1670 | `" }"` |
|    - | 1671 | `" $s .= '  }'.chr(10);"` |
|    - | 1672 | `" return $s.'}'.chr(10);"` |
|    - | 1673 | `"}"` |
|    - | 1674 | `;` |
|    - | 1675 | `/*` |
|    - | 1676 | ` * Register the __reflect_* thunks and compile the Reflection library.` |
|    - | 1677 | ` * Called from PH7_VmInit while pVm->bCompilingBuiltin is set, right after` |
|    - | 1678 | ` * the core builtin chunks (Exception and friends must exist already).` |
|    - | 1679 | ` */` |
|    - | 1680 | `/*` |
|    - | 1681 | ` * Compile the nine reflection chunks, in order. Split from` |
|    - | 1682 | ` * PH7_VmInstallReflection so the chunk strings and their sizeof stay in` |
|    - | 1683 | ` * one translation unit.` |
|    - | 1684 | ` */` |
| 4528 | 1685 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionLib(ph7_vm *pVm)` |
|    5 | 1686 | `{` |
|    - | 1687 | `	sxi32 rc;` |
| 4533 | 1688 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib1, sizeof(zReflectLib1)-1);` |
| 4533 | 1689 | `	if( rc != SXRET_OK ){` |
|  ! 0 | 1690 | `		return rc;` |
|    - | 1691 | `	}` |
| 4533 | 1692 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib2, sizeof(zReflectLib2)-1);` |
| 4533 | 1693 | `	if( rc != SXRET_OK ){` |
|  ! 0 | 1694 | `		return rc;` |
|    - | 1695 | `	}` |
| 4533 | 1696 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib3, sizeof(zReflectLib3)-1);` |
| 4533 | 1697 | `	if( rc != SXRET_OK ){` |
|  ! 0 | 1698 | `		return rc;` |
|    - | 1699 | `	}` |
| 4533 | 1700 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib4, sizeof(zReflectLib4)-1);` |
| 4533 | 1701 | `	if( rc != SXRET_OK ){` |
|  ! 0 | 1702 | `		return rc;` |
|    - | 1703 | `	}` |
| 4533 | 1704 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib5, sizeof(zReflectLib5)-1);` |
| 4533 | 1705 | `	if( rc != SXRET_OK ){` |
|  ! 0 | 1706 | `		return rc;` |
|    - | 1707 | `	}` |
| 4533 | 1708 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib6, sizeof(zReflectLib6)-1);` |
| 4533 | 1709 | `	if( rc != SXRET_OK ){` |
|  ! 0 | 1710 | `		return rc;` |
|    - | 1711 | `	}` |
| 4533 | 1712 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib7, sizeof(zReflectLib7)-1);` |
| 4533 | 1713 | `	if( rc != SXRET_OK ){` |
|  ! 0 | 1714 | `		return rc;` |
|    - | 1715 | `	}` |
| 4533 | 1716 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib8, sizeof(zReflectLib8)-1);` |
| 4533 | 1717 | `	if( rc != SXRET_OK ){` |
|  ! 0 | 1718 | `		return rc;` |
|    - | 1719 | `	}` |
| 4533 | 1720 | `	return PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib9, sizeof(zReflectLib9)-1);` |
| 2269 | 1721 | `}` |
|    - | 1722 |  |
