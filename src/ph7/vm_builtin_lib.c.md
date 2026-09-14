# src/ph7/vm_builtin_lib.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 8/8 lines (100.00%)

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
|    - |    8 | ` * The embedded PHP source of the core built-in class library (Exception and` |
|    - |    9 | ` * friends, ArrayAccess/Countable/..., Closure, Fiber, Generator shells),` |
|    - |   10 | ` * compiled at VM init by PH7_VmInstallBuiltinLib() — the bootstrap step` |
|    - |   11 | ` * PH7_VmInit runs right after the code generator comes up. Split from vm.c` |
|    - |   12 | ` * so the chunk string and its sizeof stay in one translation unit (the` |
|    - |   13 | ` * vm_builtin_reflection_lib.c pattern).` |
|    - |   14 | ` */` |
|    - |   15 | `/* The libxml-backed extensions (libxml/dom/xmlwriter) are reported by` |
|    - |   16 | ` * extension_loaded()/get_loaded_extensions() only when compiled in. These` |
|    - |   17 | ` * string fragments splice into the prelude arrays via adjacent-literal` |
|    - |   18 | ` * concatenation; they are leading-comma so they append cleanly and vanish` |
|    - |   19 | ` * to "" in tiny/non-libxml builds. */` |
|    - |   20 | `#ifdef PH7_ENABLE_LIBXML` |
|    - |   21 | `#define PHL_EXT_LOADED_LIBXML ", 'libxml' => 1, 'dom' => 1, 'xmlwriter' => 1"` |
|    - |   22 | `#define PHL_EXT_LIST_LIBXML   ",'libxml','dom','xmlwriter'"` |
|    - |   23 | `#else` |
|    - |   24 | `#define PHL_EXT_LOADED_LIBXML ""` |
|    - |   25 | `#define PHL_EXT_LIST_LIBXML   ""` |
|    - |   26 | `#endif` |
|    - |   27 | `#define PH7_BUILTIN_LIB \` |
|    - |   28 | `	"interface Throwable {"\` |
|    - |   29 | `	"public function getMessage();"\` |
|    - |   30 | `	"public function getCode();"\` |
|    - |   31 | `	"public function getFile();"\` |
|    - |   32 | `	"public function getLine();"\` |
|    - |   33 | `	"public function getTrace();"\` |
|    - |   34 | `	"public function getTraceAsString();"\` |
|    - |   35 | `	"public function getPrevious();"\` |
|    - |   36 | `	"public function __toString();"\` |
|    - |   37 | `	"}"\` |
|    - |   38 | `	"interface Traversable {}"\` |
|    - |   39 | `	"interface ArrayAccess {"\` |
|    - |   40 | `	"public function offsetExists($offset);"\` |
|    - |   41 | `	"public function offsetGet($offset);"\` |
|    - |   42 | `	"public function offsetSet($offset, $value);"\` |
|    - |   43 | `	"public function offsetUnset($offset);"\` |
|    - |   44 | `	"}"\` |
|    - |   45 | `	"interface Countable {"\` |
|    - |   46 | `	"public function count();"\` |
|    - |   47 | `	"}"\` |
|    - |   48 | `	"interface Stringable {"\` |
|    - |   49 | `	"public function __toString();"\` |
|    - |   50 | `	"}"\` |
|    - |   51 | `	"interface JsonSerializable {"\` |
|    - |   52 | `	"public function jsonSerialize();"\` |
|    - |   53 | `	"}"\` |
|    - |   54 | `	"interface UnitEnum {"\` |
|    - |   55 | `	"public static function cases();"\` |
|    - |   56 | `	"}"\` |
|    - |   57 | `	"interface BackedEnum extends UnitEnum {"\` |
|    - |   58 | `	"public static function from($value);"\` |
|    - |   59 | `	"public static function tryFrom($value);"\` |
|    - |   60 | `	"}"\` |
|    - |   61 | `	"class Exception implements Throwable { "\` |
|    - |   62 | `    "protected $message = '';"\` |
|    - |   63 | `    "protected $code = 0;"\` |
|    - |   64 | `    "protected $file;"\` |
|    - |   65 | `    "protected $line;"\` |
|    - |   66 | `    "private $trace;"\` |
|    - |   67 | `    "private $previous;"\` |
|    - |   68 | `	"public function __construct($message = null, $code = 0, ?Throwable $previous = null){"\` |
|    - |   69 | `	"   if( isset($message) ){"\` |
|    - |   70 | `	"	  $this->message = $message;"\` |
|    - |   71 | `	"   }"\` |
|    - |   72 | `	"   $this->code = $code;"\` |
|    - |   73 | `	"   if( isset($previous) ){"\` |
|    - |   74 | `	"     $this->previous = $previous;"\` |
|    - |   75 | `	"   }"\` |
|    - |   76 | `	"}"\` |
|    - |   77 | `	"public function getMessage(){"\` |
|    - |   78 | `	"   return $this->message;"\` |
|    - |   79 | `	"}"\` |
|    - |   80 | `	" public function getCode(){"\` |
|    - |   81 | `	"  return $this->code;"\` |
|    - |   82 | `	"}"\` |
|    - |   83 | `	"public function getFile(){"\` |
|    - |   84 | `	"  return $this->file;"\` |
|    - |   85 | `	"}"\` |
|    - |   86 | `	"public function getLine(){"\` |
|    - |   87 | `	"  return $this->line;"\` |
|    - |   88 | `	"}"\` |
|    - |   89 | `	"public function getTrace(){"\` |
|    - |   90 | `	"   return $this->trace;"\` |
|    - |   91 | `	"}"\` |
|    - |   92 | `	"public function getTraceAsString(){"\` |
|    - |   93 | `	"  $s = ''; $i = 0;"\` |
|    - |   94 | `	"  if( is_array($this->trace) ){"\` |
|    - |   95 | `	"    foreach( $this->trace as $f ){"\` |
|    - |   96 | `	"      $a = array();"\` |
|    - |   97 | `	"      if( isset($f['args']) && is_array($f['args']) ){"\` |
|    - |   98 | `	"        foreach( $f['args'] as $v ){"\` |
|    - |   99 | `	"          if( is_string($v) ){"\` |
|    - |  100 | `	"            $a[] = strlen($v) > 15 ? \"'\" . substr($v, 0, 15) . \"...'\" : \"'\" . $v . \"'\";"\` |
|    - |  101 | `	"          } elseif( is_array($v) ){ $a[] = 'Array'; }"\` |
|    - |  102 | `	"          elseif( is_object($v) ){ $a[] = 'Object(' . get_class($v) . ')'; }"\` |
|    - |  103 | `	"          elseif( is_null($v) ){ $a[] = 'NULL'; }"\` |
|    - |  104 | `	"          elseif( is_bool($v) ){ $a[] = $v ? 'true' : 'false'; }"\` |
|    - |  105 | `	"          else { $a[] = (string)$v; }"\` |
|    - |  106 | `	"        }"\` |
|    - |  107 | `	"      }"\` |
|    - |  108 | `	"      $s .= '#' . $i . ' ' . $f['file'] . '(' . $f['line'] . '): '"\` |
|    - |  109 | `	"         . (isset($f['class']) ? $f['class'] . $f['type'] : '') . $f['function']"\` |
|    - |  110 | `	"         . '(' . implode(', ', $a) . \")\\n\";"\` |
|    - |  111 | `	"      $i++;"\` |
|    - |  112 | `	"    }"\` |
|    - |  113 | `	"  }"\` |
|    - |  114 | `	"  return $s . '#' . $i . ' {main}';"\` |
|    - |  115 | `	"}"\` |
|    - |  116 | `	"public function getPrevious(){"\` |
|    - |  117 | `	"    return $this->previous;"\` |
|    - |  118 | `	"}"\` |
|    - |  119 | `	"public function __toString(){"\` |
|    - |  120 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|    - |  121 | `    "}"\` |
|    - |  122 | `	"}"\` |
|    - |  123 | `	"class Error implements Throwable { "\` |
|    - |  124 | `    "protected $message = '';"\` |
|    - |  125 | `    "protected $code = 0;"\` |
|    - |  126 | `    "protected $file;"\` |
|    - |  127 | `    "protected $line;"\` |
|    - |  128 | `    "private $trace;"\` |
|    - |  129 | `    "private $previous;"\` |
|    - |  130 | `	"public function __construct($message = null, $code = 0, ?Throwable $previous = null){"\` |
|    - |  131 | `	"   if( isset($message) ){"\` |
|    - |  132 | `	"	  $this->message = $message;"\` |
|    - |  133 | `	"   }"\` |
|    - |  134 | `	"   $this->code = $code;"\` |
|    - |  135 | `	"   if( isset($previous) ){"\` |
|    - |  136 | `	"     $this->previous = $previous;"\` |
|    - |  137 | `	"   }"\` |
|    - |  138 | `	"}"\` |
|    - |  139 | `	"public function getMessage(){"\` |
|    - |  140 | `	"   return $this->message;"\` |
|    - |  141 | `	"}"\` |
|    - |  142 | `	"public function getCode(){"\` |
|    - |  143 | `	"  return $this->code;"\` |
|    - |  144 | `	"}"\` |
|    - |  145 | `	"public function getFile(){"\` |
|    - |  146 | `	"  return $this->file;"\` |
|    - |  147 | `	"}"\` |
|    - |  148 | `	"public function getLine(){"\` |
|    - |  149 | `	"  return $this->line;"\` |
|    - |  150 | `	"}"\` |
|    - |  151 | `	"public function getTrace(){"\` |
|    - |  152 | `	"   return $this->trace;"\` |
|    - |  153 | `	"}"\` |
|    - |  154 | `	"public function getTraceAsString(){"\` |
|    - |  155 | `	"  $s = ''; $i = 0;"\` |
|    - |  156 | `	"  if( is_array($this->trace) ){"\` |
|    - |  157 | `	"    foreach( $this->trace as $f ){"\` |
|    - |  158 | `	"      $a = array();"\` |
|    - |  159 | `	"      if( isset($f['args']) && is_array($f['args']) ){"\` |
|    - |  160 | `	"        foreach( $f['args'] as $v ){"\` |
|    - |  161 | `	"          if( is_string($v) ){"\` |
|    - |  162 | `	"            $a[] = strlen($v) > 15 ? \"'\" . substr($v, 0, 15) . \"...'\" : \"'\" . $v . \"'\";"\` |
|    - |  163 | `	"          } elseif( is_array($v) ){ $a[] = 'Array'; }"\` |
|    - |  164 | `	"          elseif( is_object($v) ){ $a[] = 'Object(' . get_class($v) . ')'; }"\` |
|    - |  165 | `	"          elseif( is_null($v) ){ $a[] = 'NULL'; }"\` |
|    - |  166 | `	"          elseif( is_bool($v) ){ $a[] = $v ? 'true' : 'false'; }"\` |
|    - |  167 | `	"          else { $a[] = (string)$v; }"\` |
|    - |  168 | `	"        }"\` |
|    - |  169 | `	"      }"\` |
|    - |  170 | `	"      $s .= '#' . $i . ' ' . $f['file'] . '(' . $f['line'] . '): '"\` |
|    - |  171 | `	"         . (isset($f['class']) ? $f['class'] . $f['type'] : '') . $f['function']"\` |
|    - |  172 | `	"         . '(' . implode(', ', $a) . \")\\n\";"\` |
|    - |  173 | `	"      $i++;"\` |
|    - |  174 | `	"    }"\` |
|    - |  175 | `	"  }"\` |
|    - |  176 | `	"  return $s . '#' . $i . ' {main}';"\` |
|    - |  177 | `	"}"\` |
|    - |  178 | `	"public function getPrevious(){"\` |
|    - |  179 | `	"    return $this->previous;"\` |
|    - |  180 | `	"}"\` |
|    - |  181 | `	"public function __toString(){"\` |
|    - |  182 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|    - |  183 | `	"}"\` |
|    - |  184 | `	"}"\` |
|    - |  185 | `	"class TypeError extends Error { }"\` |
|    - |  186 | `	"class ArgumentCountError extends TypeError { }"\` |
|    - |  187 | `	"class ValueError extends Error { }"\` |
|    - |  188 | `	"class FiberError extends Error { }"\` |
|    - |  189 | `	"class AssertionError extends Error { }"\` |
|    - |  190 | `	"class ArithmeticError extends Error { }"\` |
|    - |  191 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|    - |  192 | `	"class ErrorException extends Exception { "\` |
|    - |  193 | `	"protected $severity;"\` |
|    - |  194 | `	"public function __construct(?string $message = null,"\` |
|    - |  195 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,?Throwable $previous = null){"\` |
|    - |  196 | `	"   /* message/code/previous belong to Exception (trace/previous are private"\` |
|    - |  197 | `	"    * to it); delegate, then set our own severity plus the caller-supplied"\` |
|    - |  198 | `	"    * file/line, which are protected and stay writable here. */"\` |
|    - |  199 | `	"   parent::__construct($message, $code, $previous);"\` |
|    - |  200 | `	"   $this->severity = $severity;"\` |
|    - |  201 | `	"   $this->file = $filename;"\` |
|    - |  202 | `	"   $this->line = $lineno;"\` |
|    - |  203 | `	"}"\` |
|    - |  204 | `	"public function getSeverity(){"\` |
|    - |  205 | `	"   return $this->severity;"\` |
|    - |  206 | `    "}"\` |
|    - |  207 | `	"}"\` |
|    - |  208 | `	"/* SPL exceptions: thin tree, inherit Exception's ctor+getters. Roots first. */"\` |
|    - |  209 | `	"class LogicException extends Exception { }"\` |
|    - |  210 | `	"class RuntimeException extends Exception { }"\` |
|    - |  211 | `	"class BadFunctionCallException extends LogicException { }"\` |
|    - |  212 | `	"class BadMethodCallException extends BadFunctionCallException { }"\` |
|    - |  213 | `	"class DomainException extends LogicException { }"\` |
|    - |  214 | `	"class InvalidArgumentException extends LogicException { }"\` |
|    - |  215 | `	"class LengthException extends LogicException { }"\` |
|    - |  216 | `	"class OutOfRangeException extends LogicException { }"\` |
|    - |  217 | `	"class OutOfBoundsException extends RuntimeException { }"\` |
|    - |  218 | `	"class OverflowException extends RuntimeException { }"\` |
|    - |  219 | `	"class RangeException extends RuntimeException { }"\` |
|    - |  220 | `	"class UnderflowException extends RuntimeException { }"\` |
|    - |  221 | `	"class UnexpectedValueException extends RuntimeException { }"\` |
|    - |  222 | `	"class JsonException extends Exception { }"\` |
|    - |  223 | `	"interface Iterator extends Traversable {"\` |
|    - |  224 | `	"public function current();"\` |
|    - |  225 | `	"public function key();"\` |
|    - |  226 | `	"public function next();"\` |
|    - |  227 | `	"public function rewind();"\` |
|    - |  228 | `	"public function valid();"\` |
|    - |  229 | `	"}"\` |
|    - |  230 | `	"interface IteratorAggregate extends Traversable {"\` |
|    - |  231 | `	"public function getIterator();"\` |
|    - |  232 | `	"}"\` |
|    - |  233 | `	"interface Serializable {"\` |
|    - |  234 | `	"public function serialize();"\` |
|    - |  235 | `	"public function unserialize(string $serialized);"\` |
|    - |  236 | `	"}"\` |
|    - |  237 | `	"/* Directory releated IO */"\` |
|    - |  238 | `	"class Directory {"\` |
|    - |  239 | `	"public $handle = null;"\` |
|    - |  240 | `	"public $path  = null;"\` |
|    - |  241 | `	"public function __construct(string $path)"\` |
|    - |  242 | `	"{"\` |
|    - |  243 | `	"   $this->handle = opendir($path);"\` |
|    - |  244 | `	"   if( $this->handle !== FALSE ){"\` |
|    - |  245 | `	"      $this->path = $path;"\` |
|    - |  246 | `	"   }"\` |
|    - |  247 | `	"}"\` |
|    - |  248 | `	"public function __destruct()"\` |
|    - |  249 | `	"{"\` |
|    - |  250 | `	"  if( $this->handle != null ){"\` |
|    - |  251 | `	"       closedir($this->handle);"\` |
|    - |  252 | `	"  }"\` |
|    - |  253 | `	"}"\` |
|    - |  254 | `	"public function read()"\` |
|    - |  255 | `	"{"\` |
|    - |  256 | `	"    return readdir($this->handle);"\` |
|    - |  257 | `	"}"\` |
|    - |  258 | `	"public function rewind()"\` |
|    - |  259 | `	"{"\` |
|    - |  260 | `	"    rewinddir($this->handle);"\` |
|    - |  261 | `	"}"\` |
|    - |  262 | `	"public function close()"\` |
|    - |  263 | `	"{"\` |
|    - |  264 | `	"    closedir($this->handle);"\` |
|    - |  265 | `	"    $this->handle = null;"\` |
|    - |  266 | `	"}"\` |
|    - |  267 | `	"}"\` |
|    - |  268 | `	"class Fiber {"\` |
|    - |  269 | `	"  private $__ctx;"\` |
|    - |  270 | `	"  private $__callable;"\` |
|    - |  271 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|    - |  272 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|    - |  273 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|    - |  274 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|    - |  275 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|    - |  276 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|    - |  277 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|    - |  278 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|    - |  279 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|    - |  280 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|    - |  281 | `	"}"\` |
|    - |  282 | `	"class Generator implements Iterator {"\` |
|    - |  283 | `	"  private $__ctx;"\` |
|    - |  284 | `	"  public function current(){ return __gen_current($this); }"\` |
|    - |  285 | `	"  public function key(){ return __gen_key($this); }"\` |
|    - |  286 | `	"  public function next(){ return __gen_next($this); }"\` |
|    - |  287 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|    - |  288 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|    - |  289 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|    - |  290 | `	"  public function throw(Throwable $exception){ return __gen_throw($this,$exception); }"\` |
|    - |  291 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|    - |  292 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|    - |  293 | `	"}"\` |
|    - |  294 | `	"final class Closure {"\` |
|    - |  295 | `	"  private $__fn;"\` |
|    - |  296 | `	"  private $__this;"\` |
|    - |  297 | `	"  private $__scope;"\` |
|    - |  298 | `	"  public function __construct(){ throw new \\Error('Instantiation of class Closure is not allowed'); }"\` |
|    - |  299 | `	"  public function bindTo($newThis, $scope = 'static'){ return __closure_bindTo($this, $newThis, $scope); }"\` |
|    - |  300 | `	"  public function call($newThis, ...$args){ $bound = __closure_bindTo($this, $newThis, get_class($newThis)); return $bound(...$args); }"\` |
|    - |  301 | `	"  public static function bind($closure, $newThis, $scope = 'static'){ return __closure_bindTo($closure, $newThis, $scope); }"\` |
|    - |  302 | `	"  public static function fromCallable($callable){ return __closure_fromCallable($callable); }"\` |
|    - |  303 | `	"}"\` |
|    - |  304 | `	/* stdClass is empty (PHP-exact): holds only dynamic (runtime-added) properties. */\` |
|    - |  305 | `	"#[Attribute(Attribute::TARGET_CLASS)]"\` |
|    - |  306 | `	"final class Attribute {"\` |
|    - |  307 | `	"  const TARGET_CLASS = 1;"\` |
|    - |  308 | `	"  const TARGET_FUNCTION = 2;"\` |
|    - |  309 | `	"  const TARGET_METHOD = 4;"\` |
|    - |  310 | `	"  const TARGET_PROPERTY = 8;"\` |
|    - |  311 | `	"  const TARGET_CLASS_CONSTANT = 16;"\` |
|    - |  312 | `	"  const TARGET_PARAMETER = 32;"\` |
|    - |  313 | `	"  const TARGET_CONSTANT = 64;"\` |
|    - |  314 | `	"  const TARGET_ALL = 127;"\` |
|    - |  315 | `	"  const IS_REPEATABLE = 128;"\` |
|    - |  316 | `	"  public $flags;"\` |
|    - |  317 | `	"  public function __construct($flags = 127){ $this->flags = $flags; }"\` |
|    - |  318 | `	"}"\` |
|    - |  319 | `	"#[Attribute(Attribute::TARGET_METHOD \| Attribute::TARGET_FUNCTION \| Attribute::TARGET_CLASS_CONSTANT \| Attribute::TARGET_CONSTANT)]"\` |
|    - |  320 | `	"final class Deprecated {"\` |
|    - |  321 | `	"  public $message;"\` |
|    - |  322 | `	"  public $since;"\` |
|    - |  323 | `	"  public function __construct($message = null, $since = null){"\` |
|    - |  324 | `	"    $this->message = $message;"\` |
|    - |  325 | `	"    $this->since = $since;"\` |
|    - |  326 | `	"  }"\` |
|    - |  327 | `	"}"\` |
|    - |  328 | `	"class stdClass{"\` |
|    - |  329 | `	"}"\` |
|    - |  330 | `	"function dir(string $path){"\` |
|    - |  331 | `	"   return new Directory($path);"\` |
|    - |  332 | `	"}"\` |
|    - |  333 | `	"function Dir(string $path){"\` |
|    - |  334 | `	"   return new Directory($path);"\` |
|    - |  335 | `	"}"\` |
|    - |  336 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|    - |  337 | `    "{"\` |
|    - |  338 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|    - |  339 | `	"  $aDir = array();"\` |
|    - |  340 | `	"  $pHandle = opendir($directory);"\` |
|    - |  341 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|    - |  342 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|    - |  343 | `	"      $aDir[] = $pEntry;"\` |
|    - |  344 | `	"   }"\` |
|    - |  345 | `	"  closedir($pHandle);"\` |
|    - |  346 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|    - |  347 | `	"      rsort($aDir);"\` |
|    - |  348 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|    - |  349 | `	"      sort($aDir);"\` |
|    - |  350 | `	"  }"\` |
|    - |  351 | `	"  return $aDir;"\` |
|    - |  352 | `	"}"\` |
|    - |  353 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|    - |  354 | `	"/* php keeps the literal directory portion of the pattern in every result;"\` |
|    - |  355 | `	"   split off everything up to and including the last '/' as the prefix. */"\` |
|    - |  356 | `	"$slash = strrpos($pattern,'/');"\` |
|    - |  357 | `	"if( $slash === false ){ $zDir = '.'; $prefix = ''; $pat = $pattern; }"\` |
|    - |  358 | `	"else { $zDir = substr($pattern,0,$slash); if( $zDir === '' ){ $zDir = '/'; } $prefix = substr($pattern,0,$slash+1); $pat = substr($pattern,$slash+1); }"\` |
|    - |  359 | `	"$pHandle = opendir($zDir);"\` |
|    - |  360 | `	"if( $pHandle == FALSE ){"\` |
|    - |  361 | `	"   /* IO error while opening the target directory,return FALSE */"\` |
|    - |  362 | `	"	return FALSE;"\` |
|    - |  363 | `	"}"\` |
|    - |  364 | `	"$pArray = array(); /* Empty array */"\` |
|    - |  365 | `	"/* Loop throw available entries */"\` |
|    - |  366 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|    - |  367 | `	" /* php's glob() never matches a leading-dot entry (incl. '.' and '..') unless"\` |
|    - |  368 | `	"    the pattern itself starts with a dot */"\` |
|    - |  369 | `	"	if( strlen($pEntry) > 0 && $pEntry[0] === '.' && (strlen($pat) < 1 \|\| $pat[0] !== '.') ){ continue; }"\` |
|    - |  370 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|    - |  371 | `	"	$rc = strglob($pat,$pEntry);"\` |
|    - |  372 | `	"	if( $rc ){"\` |
|    - |  373 | `	"	   $zFull = $prefix . $pEntry;"\` |
|    - |  374 | `	"	   if( is_dir($zDir . '/' . $pEntry) ){"\` |
|    - |  375 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|    - |  376 | `	"		     /* Adds a slash to each directory returned */"\` |
|    - |  377 | `	"			 $zFull .= DIRECTORY_SEPARATOR;"\` |
|    - |  378 | `	"		  }"\` |
|    - |  379 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|    - |  380 | `	"	     /* Not a directory,ignore */"\` |
|    - |  381 | `	"		 continue;"\` |
|    - |  382 | `	"	   }"\` |
|    - |  383 | `	"	   /* Add the entry (with its literal directory prefix, php-style) */"\` |
|    - |  384 | `	"	   $pArray[] = $zFull;"\` |
|    - |  385 | `	"	}"\` |
|    - |  386 | `	" }"\` |
|    - |  387 | `	"/* Close the handle */"\` |
|    - |  388 | `	"closedir($pHandle);"\` |
|    - |  389 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|    - |  390 | `	"  /* Sort the array */"\` |
|    - |  391 | `	"  sort($pArray);"\` |
|    - |  392 | `	"}"\` |
|    - |  393 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|    - |  394 | `	"  /* Return the search pattern if no files matching were found */"\` |
|    - |  395 | `	"  $pArray[] = $pattern;"\` |
|    - |  396 | `	"}"\` |
|    - |  397 | `	"/* Return the created array */"\` |
|    - |  398 | `	"return $pArray;"\` |
|    - |  399 | `   "}"\` |
|    - |  400 | `   "/* Creates a temporary file */"\` |
|    - |  401 | `   "function tmpfile(){"\` |
|    - |  402 | `   "  /* Extract the temp directory */"\` |
|    - |  403 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|    - |  404 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|    - |  405 | `   "    /* Use the current dir */"\` |
|    - |  406 | `   "    $zTempDir = '.';"\` |
|    - |  407 | `   "  }"\` |
|    - |  408 | `   "  /* Create the file */"\` |
|    - |  409 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|    - |  410 | `   "  return $pHandle;"\` |
|    - |  411 | `   "}"\` |
|    - |  412 | `   "/* php's number_format(): missing entirely from PH7. */"\` |
|    - |  413 | `   "function number_format($num, $decimals = 0, $dec_point = '.', $thousands_sep = ','){"\` |
|    - |  414 | `   "  $num = (float)$num;"\` |
|    - |  415 | `   "  $decimals = (int)$decimals;"\` |
|    - |  416 | `   "  if( $decimals < 0 ){ $decimals = 0; }"\` |
|    - |  417 | `   "  if( $dec_point === null ){ $dec_point = '.'; }"\` |
|    - |  418 | `   "  if( $thousands_sep === null ){ $thousands_sep = ','; }"\` |
|    - |  419 | `   "  /* round() first: sprintf uses banker's rounding, php's number_format rounds"\` |
|    - |  420 | `   "   * half AWAY FROM ZERO (number_format(0.5) is '1', not '0'). */"\` |
|    - |  421 | `   "  $num = round($num, $decimals);"\` |
|    - |  422 | `   "  $s = sprintf('%.' . $decimals . 'f', $num);"\` |
|    - |  423 | `   "  $neg = false;"\` |
|    - |  424 | `   "  if( substr($s, 0, 1) === '-' ){ $neg = true; $s = substr($s, 1); }"\` |
|    - |  425 | `   "  $parts = explode('.', $s);"\` |
|    - |  426 | `   "  $int = $parts[0];"\` |
|    - |  427 | `   "  $frac = count($parts) > 1 ? $parts[1] : '';"\` |
|    - |  428 | `   "  $out = '';"\` |
|    - |  429 | `   "  $len = strlen($int);"\` |
|    - |  430 | `   "  $c = 0;"\` |
|    - |  431 | `   "  for( $i = $len - 1 ; $i >= 0 ; $i-- ){"\` |
|    - |  432 | `   "    $out = $int[$i] . $out;"\` |
|    - |  433 | `   "    $c++;"\` |
|    - |  434 | `   "    if( $c % 3 === 0 && $i > 0 ){ $out = $thousands_sep . $out; }"\` |
|    - |  435 | `   "  }"\` |
|    - |  436 | `   "  if( $decimals > 0 ){ $out = $out . $dec_point . $frac; }"\` |
|    - |  437 | `   "  if( $neg ){ $out = '-' . $out; }"\` |
|    - |  438 | `   "  return $out;"\` |
|    - |  439 | `   "}"\` |
|    - |  440 | `   "function is_nan($v){ $v = (float)$v; return $v != $v; }"\` |
|    - |  441 | `   "function is_infinite($v){ $v = (float)$v; return $v == INF \|\| $v == -INF; }"\` |
|    - |  442 | `   "function is_finite($v){ $v = (float)$v; return !is_nan($v) && !is_infinite($v); }"\` |
|    - |  443 | `   "/* php's version_compare: canonicalise (separators + digit/alpha boundaries all"\` |
|    - |  444 | `   " * become '.'), then compare parts with the special dev<alpha<beta<RC<#<pl ordering. */"\` |
|    - |  445 | `   "function __phl_vcanon($v){"\` |
|    - |  446 | `   "  $v = (string)$v; $len = strlen($v); $out = '';"\` |
|    - |  447 | `   "  for( $i = 0; $i < $len; $i++ ){"\` |
|    - |  448 | `   "   $c = $v[$i]; $rp = $i + 1 < $len ? $v[$i + 1] : '';"\` |
|    - |  449 | `   "   $cd = ($c >= '0' && $c <= '9');"\` |
|    - |  450 | `   "   $ca = $cd \|\| ($c >= 'a' && $c <= 'z') \|\| ($c >= 'A' && $c <= 'Z');"\` |
|    - |  451 | `   "   if( !$ca ){"\` |
|    - |  452 | `   "    /* any non-alphanumeric (., -, _, +, ...) is a separator: emit one '.' */"\` |
|    - |  453 | `   "    if( $out !== '' && substr($out, -1) !== '.' ){ $out .= '.'; }"\` |
|    - |  454 | `   "   }else{"\` |
|    - |  455 | `   "    $out .= $c;"\` |
|    - |  456 | `   "    $rd = ($rp >= '0' && $rp <= '9');"\` |
|    - |  457 | `   "    $ra = $rd \|\| ($rp >= 'a' && $rp <= 'z') \|\| ($rp >= 'A' && $rp <= 'Z');"\` |
|    - |  458 | `   "    if( $rp !== '' && $ra && ($cd !== $rd) ){ $out .= '.'; }"\` |
|    - |  459 | `   "   }"\` |
|    - |  460 | `   "  }"\` |
|    - |  461 | `   "  return explode('.', $out);"\` |
|    - |  462 | `   "}"\` |
|    - |  463 | `   "function __phl_vform($s){"\` |
|    - |  464 | `   "  if( $s === '' ){ return -1; }"\` |
|    - |  465 | `   "  if( ctype_digit($s) ){ return 4; }"\` |
|    - |  466 | `   "  $f = array('dev' => 0, 'alpha' => 1, 'a' => 1, 'beta' => 2, 'b' => 2, 'RC' => 3, 'rc' => 3, 'pl' => 5, 'p' => 5);"\` |
|    - |  467 | `   "  foreach( $f as $name => $ord ){ if( strncmp($s, $name, strlen($name)) === 0 ){ return $ord; } }"\` |
|    - |  468 | `   "  return -1;"\` |
|    - |  469 | `   "}"\` |
|    - |  470 | `   "function version_compare($version1, $version2, $operator = null){"\` |
|    - |  471 | `   "  $v1 = __phl_vcanon($version1); $v2 = __phl_vcanon($version2);"\` |
|    - |  472 | `   "  $n1 = count($v1); $n2 = count($v2); $n = $n1 > $n2 ? $n1 : $n2; $cmp = 0;"\` |
|    - |  473 | `   "  for( $i = 0; $i < $n; $i++ ){"\` |
|    - |  474 | `   "   $a = $i < $n1 ? $v1[$i] : null; $b = $i < $n2 ? $v2[$i] : null;"\` |
|    - |  475 | `   "   if( $a === null ){ $cmp = ctype_digit($b) ? -1 : (4 <=> __phl_vform($b)); }"\` |
|    - |  476 | `   "   elseif( $b === null ){ $cmp = ctype_digit($a) ? 1 : (__phl_vform($a) <=> 4); }"\` |
|    - |  477 | `   "   elseif( ctype_digit($a) && ctype_digit($b) ){ $cmp = (int)$a <=> (int)$b; }"\` |
|    - |  478 | `   "   else{ $cmp = __phl_vform($a) <=> __phl_vform($b); }"\` |
|    - |  479 | `   "   if( $cmp !== 0 ){ break; }"\` |
|    - |  480 | `   "  }"\` |
|    - |  481 | `   "  if( $operator === null ){ return $cmp; }"\` |
|    - |  482 | `   "  switch( (string)$operator ){"\` |
|    - |  483 | `   "   case '<': case 'lt': return $cmp < 0;"\` |
|    - |  484 | `   "   case '<=': case 'le': return $cmp <= 0;"\` |
|    - |  485 | `   "   case '>': case 'gt': return $cmp > 0;"\` |
|    - |  486 | `   "   case '>=': case 'ge': return $cmp >= 0;"\` |
|    - |  487 | `   "   case '==': case '=': case 'eq': return $cmp === 0;"\` |
|    - |  488 | `   "   case '!=': case '<>': case 'ne': return $cmp !== 0;"\` |
|    - |  489 | `   "  }"\` |
|    - |  490 | `   "  return null;"\` |
|    - |  491 | `   "}"\` |
|    - |  492 | `   "/* phl.stub_extensions (a -d/php.ini list, comma-separated) declares extensions"\` |
|    - |  493 | `   " * PHL does not implement as LOADED, backed by no-op behaviour, so software that"\` |
|    - |  494 | `   " * only GATES on extension_loaded() (e.g. PHPUnit's dom/xmlwriter check) runs"\` |
|    - |  495 | `   " * unmodified. It does NOT synthesize the extension's classes/functions. */"\` |
|    - |  496 | `   "function __phl_stub_exts(){"\` |
|    - |  497 | `   "  $s = ini_get('phl.stub_extensions');"\` |
|    - |  498 | `   "  if( $s === false \|\| $s === '' ){ return array(); }"\` |
|    - |  499 | `   "  $out = array();"\` |
|    - |  500 | `   "  foreach( explode(',', (string)$s) as $e ){ $e = trim($e); if( $e !== '' ){ $out[strtolower($e)] = $e; } }"\` |
|    - |  501 | `   "  return $out;"\` |
|    - |  502 | `   "}"\` |
|    - |  503 | `   "function extension_loaded($name){"\` |
|    - |  504 | `   "  static $ext = array('core' => 1, 'standard' => 1, 'pcre' => 1, 'json' => 1,"\` |
|    - |  505 | `   "   'ctype' => 1, 'date' => 1, 'spl' => 1, 'reflection' => 1, 'mbstring' => 1,"\` |
|    - |  506 | `   "   'hash' => 1, 'filter' => 1, 'session' => 1" PHL_EXT_LOADED_LIBXML ");"\` |
|    - |  507 | `   "  $n = strtolower((string)$name);"\` |
|    - |  508 | `   "  if( isset($ext[$n]) ){ return true; }"\` |
|    - |  509 | `   "  $stub = __phl_stub_exts();"\` |
|    - |  510 | `   "  return isset($stub[$n]);"\` |
|    - |  511 | `   "}"\` |
|    - |  512 | `   "function get_loaded_extensions($zend_extensions = false){"\` |
|    - |  513 | `   "  if( $zend_extensions ){ return array(); }"\` |
|    - |  514 | `   "  $base = array('Core','date','pcre','SPL','json','standard',"\` |
|    - |  515 | `   "   'ctype','filter','hash','Reflection','session','mbstring'" PHL_EXT_LIST_LIBXML ");"\` |
|    - |  516 | `   "  foreach( __phl_stub_exts() as $e ){ $base[] = $e; }"\` |
|    - |  517 | `   "  return $base;"\` |
|    - |  518 | `   "}"\` |
|    - |  519 | `   "/* Inverse of bin2hex() */"\` |
|    - |  520 | `   "function hex2bin($str){"\` |
|    - |  521 | `   "  $str = (string)$str;"\` |
|    - |  522 | `   "  $len = strlen($str);"\` |
|    - |  523 | `   "  if( $len % 2 !== 0 ){"\` |
|    - |  524 | `   "    trigger_error('hex2bin(): Hexadecimal input string must have an even length', E_USER_WARNING);"\` |
|    - |  525 | `   "    return false;"\` |
|    - |  526 | `   "  }"\` |
|    - |  527 | `   "  $out = '';"\` |
|    - |  528 | `   "  for( $i = 0 ; $i < $len ; $i += 2 ){"\` |
|    - |  529 | `   "    $pair = substr($str, $i, 2);"\` |
|    - |  530 | `   "    if( !ctype_xdigit($pair) ){"\` |
|    - |  531 | `   "      trigger_error('hex2bin(): Input string must be hexadecimal string', E_USER_WARNING);"\` |
|    - |  532 | `   "      return false;"\` |
|    - |  533 | `   "    }"\` |
|    - |  534 | `   "    $out = $out . chr(hexdec($pair));"\` |
|    - |  535 | `   "  }"\` |
|    - |  536 | `   "  return $out;"\` |
|    - |  537 | `   "}"\` |
|    - |  538 | `   "/* Division that never throws: INF/-INF/NAN like php */"\` |
|    - |  539 | `   "function fdiv($a, $b){"\` |
|    - |  540 | `   "  $a = (float)$a;"\` |
|    - |  541 | `   "  $b = (float)$b;"\` |
|    - |  542 | `   "  if( $b == 0.0 ){"\` |
|    - |  543 | `   "    if( $a == 0.0 \|\| is_nan($a) ){ return NAN; }"\` |
|    - |  544 | `   "    return $a > 0 ? INF : -INF;"\` |
|    - |  545 | `   "  }"\` |
|    - |  546 | `   "  return $a / $b;"\` |
|    - |  547 | `   "}"\` |
|    - |  548 | `   "function checkdate($month, $day, $year){"\` |
|    - |  549 | `   "  $month = (int)$month; $day = (int)$day; $year = (int)$year;"\` |
|    - |  550 | `   "  if( $month < 1 \|\| $month > 12 \|\| $year < 1 \|\| $year > 32767 \|\| $day < 1 ){ return false; }"\` |
|    - |  551 | `   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\` |
|    - |  552 | `   "  $max = $days[$month - 1];"\` |
|    - |  553 | `   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) \|\| ($year % 400 === 0)) ){"\` |
|    - |  554 | `   "    $max = 29;"\` |
|    - |  555 | `   "  }"\` |
|    - |  556 | `   "  return $day <= $max;"\` |
|    - |  557 | `   "}"\` |
|    - |  558 | `   "function is_iterable($v){ return is_array($v) \|\| ($v instanceof Traversable); }"\` |
|    - |  559 | `   "function is_countable($v){ return is_array($v) \|\| ($v instanceof Countable); }"\` |
|    - |  560 | `   "function key_exists($key, $array){ return array_key_exists($key, $array); }"\` |
|    - |  561 | `   "function doubleval($v){ return (float)$v; }"\` |
|    - |  562 | `   "function array_count_values($array){"\` |
|    - |  563 | `   "  $out = array();"\` |
|    - |  564 | `   "  foreach( $array as $v ){"\` |
|    - |  565 | `   "    if( !is_int($v) && !is_string($v) ){"\` |
|    - |  566 | `   "      trigger_error('array_count_values(): Can only count string and integer values, entry skipped', E_USER_WARNING);"\` |
|    - |  567 | `   "      continue;"\` |
|    - |  568 | `   "    }"\` |
|    - |  569 | `   "    if( isset($out[$v]) ){ $out[$v] = $out[$v] + 1; } else { $out[$v] = 1; }"\` |
|    - |  570 | `   "  }"\` |
|    - |  571 | `   "  return $out;"\` |
|    - |  572 | `   "}"\` |
|    - |  573 | `   "function array_change_key_case($array, $case = CASE_LOWER){"\` |
|    - |  574 | `   "  $out = array();"\` |
|    - |  575 | `   "  foreach( $array as $k => $v ){"\` |
|    - |  576 | `   "    if( is_string($k) ){ $k = ($case == CASE_UPPER) ? strtoupper($k) : strtolower($k); }"\` |
|    - |  577 | `   "    $out[$k] = $v;"\` |
|    - |  578 | `   "  }"\` |
|    - |  579 | `   "  return $out;"\` |
|    - |  580 | `   "}"\` |
|    - |  581 | `   "function array_replace_recursive($array, ...$others){"\` |
|    - |  582 | `   "  foreach( $others as $o ){"\` |
|    - |  583 | `   "    foreach( $o as $k => $v ){"\` |
|    - |  584 | `   "      if( is_array($v) && isset($array[$k]) && is_array($array[$k]) ){"\` |
|    - |  585 | `   "        $array[$k] = array_replace_recursive($array[$k], $v);"\` |
|    - |  586 | `   "      }else{"\` |
|    - |  587 | `   "        $array[$k] = $v;"\` |
|    - |  588 | `   "      }"\` |
|    - |  589 | `   "    }"\` |
|    - |  590 | `   "  }"\` |
|    - |  591 | `   "  return $array;"\` |
|    - |  592 | `   "}"\` |
|    - |  593 | `   "function class_uses($what, $autoload = true){"\` |
|    - |  594 | `   "  $c = is_object($what) ? get_class($what) : (string)$what;"\` |
|    - |  595 | `   "  if( !class_exists($c) ){ return false; }"\` |
|    - |  596 | `   "  return array();  /* PHL has no traits yet -- always the empty set */"\` |
|    - |  597 | `   "}"\` |
|    - |  598 | `   "function count_chars($str, $mode = 0){"\` |
|    - |  599 | `   "  $str = (string)$str;"\` |
|    - |  600 | `   "  $counts = array();"\` |
|    - |  601 | `   "  for( $i = 0 ; $i < 256 ; $i++ ){ $counts[$i] = 0; }"\` |
|    - |  602 | `   "  $len = strlen($str);"\` |
|    - |  603 | `   "  for( $i = 0 ; $i < $len ; $i++ ){ $b = ord($str[$i]); $counts[$b] = $counts[$b] + 1; }"\` |
|    - |  604 | `   "  if( $mode == 1 ){"\` |
|    - |  605 | `   "    $out = array();"\` |
|    - |  606 | `   "    foreach( $counts as $b => $n ){ if( $n > 0 ){ $out[$b] = $n; } }"\` |
|    - |  607 | `   "    return $out;"\` |
|    - |  608 | `   "  }"\` |
|    - |  609 | `   "  if( $mode == 3 ){"\` |
|    - |  610 | `   "    $out = '';"\` |
|    - |  611 | `   "    foreach( $counts as $b => $n ){ if( $n > 0 ){ $out = $out . chr($b); } }"\` |
|    - |  612 | `   "    return $out;"\` |
|    - |  613 | `   "  }"\` |
|    - |  614 | `   "  return $counts;"\` |
|    - |  615 | `   "}"\` |
|    - |  616 | `   "function ip2long($ip){"\` |
|    - |  617 | `   "  $p = explode('.', (string)$ip);"\` |
|    - |  618 | `   "  if( count($p) !== 4 ){ return false; }"\` |
|    - |  619 | `   "  $n = 0;"\` |
|    - |  620 | `   "  foreach( $p as $o ){"\` |
|    - |  621 | `   "    if( !ctype_digit($o) \|\| (int)$o < 0 \|\| (int)$o > 255 ){ return false; }"\` |
|    - |  622 | `   "    $n = $n * 256 + (int)$o;"\` |
|    - |  623 | `   "  }"\` |
|    - |  624 | `   "  return $n;"\` |
|    - |  625 | `   "}"\` |
|    - |  626 | `   "function long2ip($n){"\` |
|    - |  627 | `   "  $n = (int)$n;"\` |
|    - |  628 | `   "  return (($n >> 24) & 255) . '.' . (($n >> 16) & 255) . '.' . (($n >> 8) & 255) . '.' . ($n & 255);"\` |
|    - |  629 | `   "}"\` |
|    - |  630 | `   "function preg_filter($pattern, $replacement, $subject, $limit = -1){"\` |
|    - |  631 | `   "  if( is_array($subject) ){"\` |
|    - |  632 | `   "    $out = array();"\` |
|    - |  633 | `   "    foreach( $subject as $k => $v ){"\` |
|    - |  634 | `   "      $r = preg_replace($pattern, $replacement, (string)$v, $limit, $cnt);"\` |
|    - |  635 | `   "      if( $cnt > 0 ){ $out[$k] = $r; }"\` |
|    - |  636 | `   "    }"\` |
|    - |  637 | `   "    return $out;"\` |
|    - |  638 | `   "  }"\` |
|    - |  639 | `   "  $r = preg_replace($pattern, $replacement, (string)$subject, $limit, $cnt);"\` |
|    - |  640 | `   "  return $cnt > 0 ? $r : null;"\` |
|    - |  641 | `   "}"\` |
|    - |  642 | `   "function preg_replace_callback_array($patterns, $subject, $limit = -1){"\` |
|    - |  643 | `   "  foreach( $patterns as $pat => $cb ){"\` |
|    - |  644 | `   "    $subject = preg_replace_callback($pat, $cb, $subject, $limit);"\` |
|    - |  645 | `   "  }"\` |
|    - |  646 | `   "  return $subject;"\` |
|    - |  647 | `   "}"\` |
|    - |  648 | `   "function cal_days_in_month($calendar, $month, $year){"\` |
|    - |  649 | `   "  $month = (int)$month; $year = (int)$year;"\` |
|    - |  650 | `   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\` |
|    - |  651 | `   "  if( $month < 1 \|\| $month > 12 ){"\` |
|    - |  652 | `   "    throw new ValueError('cal_days_in_month(): Argument #2 ($month) must be a valid month');"\` |
|    - |  653 | `   "  }"\` |
|    - |  654 | `   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) \|\| ($year % 400 === 0)) ){"\` |
|    - |  655 | `   "    return 29;"\` |
|    - |  656 | `   "  }"\` |
|    - |  657 | `   "  return $days[$month - 1];"\` |
|    - |  658 | `   "}"\` |
|    - |  659 | `   "function preg_grep($pattern, $array, $flags = 0){"\` |
|    - |  660 | `   "  $out = array();"\` |
|    - |  661 | `   "  foreach( $array as $k => $v ){"\` |
|    - |  662 | `   "    $m = preg_match($pattern, (string)$v);"\` |
|    - |  663 | `   "    if( $flags & PREG_GREP_INVERT ){ $m = !$m; }"\` |
|    - |  664 | `   "    if( $m ){ $out[$k] = $v; }"\` |
|    - |  665 | `   "  }"\` |
|    - |  666 | `   "  return $out;"\` |
|    - |  667 | `   "}"\` |
|    - |  668 | `   "function class_implements($what, $autoload = true){"\` |
|    - |  669 | `   "  $c = is_object($what) ? get_class($what) : (string)$what;"\` |
|    - |  670 | `   "  if( !class_exists($c) && !interface_exists($c) ){ return false; }"\` |
|    - |  671 | `   "  $out = array();"\` |
|    - |  672 | `   "  $r = new ReflectionClass($c);"\` |
|    - |  673 | `   "  foreach( $r->getInterfaceNames() as $i ){ $out[$i] = $i; }"\` |
|    - |  674 | `   "  return $out;"\` |
|    - |  675 | `   "}"\` |
|    - |  676 | `   "function class_parents($what, $autoload = true){"\` |
|    - |  677 | `   "  $c = is_object($what) ? get_class($what) : (string)$what;"\` |
|    - |  678 | `   "  if( !class_exists($c) ){ return false; }"\` |
|    - |  679 | `   "  $out = array();"\` |
|    - |  680 | `   "  $r = new ReflectionClass($c);"\` |
|    - |  681 | `   "  while( ($p = $r->getParentClass()) ){"\` |
|    - |  682 | `   "    $n = $p->getName();"\` |
|    - |  683 | `   "    $out[$n] = $n;"\` |
|    - |  684 | `   "    $r = $p;"\` |
|    - |  685 | `   "  }"\` |
|    - |  686 | `   "  return $out;"\` |
|    - |  687 | `   "}"\` |
|    - |  688 | `   "/* php's http_build_query() -- missing from PH7. Skips null values, casts"\` |
|    - |  689 | `   " * bool to 1/0, prefixes numeric top-level keys, urlencodes per RFC. */"\` |
|    - |  690 | `   "function __phl_hbq_enc($s, $enc){"\` |
|    - |  691 | `   "  return $enc == PHP_QUERY_RFC3986 ? rawurlencode((string)$s) : urlencode((string)$s);"\` |
|    - |  692 | `   "}"\` |
|    - |  693 | `   "function __phl_hbq(&$pairs, $data, $key_prefix, $numeric_prefix, $sep, $enc){"\` |
|    - |  694 | `   "  foreach( $data as $k => $v ){"\` |
|    - |  695 | `   "    if( $v === null ){ continue; }"\` |
|    - |  696 | `   "    if( $key_prefix === '' ){"\` |
|    - |  697 | `   "      $ek = is_int($k) ? __phl_hbq_enc($numeric_prefix . $k, $enc) : __phl_hbq_enc($k, $enc);"\` |
|    - |  698 | `   "    } else {"\` |
|    - |  699 | `   "      $ek = $key_prefix . '%5B' . __phl_hbq_enc($k, $enc) . '%5D';"\` |
|    - |  700 | `   "    }"\` |
|    - |  701 | `   "    if( is_array($v) ){"\` |
|    - |  702 | `   "      __phl_hbq($pairs, $v, $ek, $numeric_prefix, $sep, $enc);"\` |
|    - |  703 | `   "    } elseif( is_object($v) ){"\` |
|    - |  704 | `   "      __phl_hbq($pairs, get_object_vars($v), $ek, $numeric_prefix, $sep, $enc);"\` |
|    - |  705 | `   "    } else {"\` |
|    - |  706 | `   "      if( $v === true ){ $v = '1'; } elseif( $v === false ){ $v = '0'; }"\` |
|    - |  707 | `   "      $pairs[] = $ek . '=' . __phl_hbq_enc($v, $enc);"\` |
|    - |  708 | `   "    }"\` |
|    - |  709 | `   "  }"\` |
|    - |  710 | `   "}"\` |
|    - |  711 | `   "function http_build_query($data, $numeric_prefix = '', $arg_separator = null, $encoding_type = PHP_QUERY_RFC1738){"\` |
|    - |  712 | `   "  if( !is_array($data) && !is_object($data) ){"\` |
|    - |  713 | `   "    throw new TypeError('http_build_query(): Argument #1 ($data) must be of type array\|object, ' . gettype($data) . ' given');"\` |
|    - |  714 | `   "  }"\` |
|    - |  715 | `   "  if( $arg_separator === null ){ $arg_separator = '&'; }"\` |
|    - |  716 | `   "  $pairs = array();"\` |
|    - |  717 | `   "  __phl_hbq($pairs, is_object($data) ? get_object_vars($data) : $data, '', (string)$numeric_prefix, $arg_separator, $encoding_type);"\` |
|    - |  718 | `   "  return implode($arg_separator, $pairs);"\` |
|    - |  719 | `   "}"\` |
|    - |  720 | `   "/* php's parse_str() -- missing from PH7. Mangles the base name ('.'/' ' -> '_'),"\` |
|    - |  721 | `   " * parses [key] nesting and [] appends, urldecodes keys and values. */"\` |
|    - |  722 | `   "function __phl_parsestr_assign(&$arr, $segments, $i, $val){"\` |
|    - |  723 | `   "  $seg = $segments[$i];"\` |
|    - |  724 | `   "  $last = ($i === count($segments) - 1);"\` |
|    - |  725 | `   "  if( $seg === '' ){"\` |
|    - |  726 | `   "    if( $last ){ $arr[] = $val; return; }"\` |
|    - |  727 | `   "    $arr[] = array();"\` |
|    - |  728 | `   "    $k = array_key_last($arr);"\` |
|    - |  729 | `   "    __phl_parsestr_assign($arr[$k], $segments, $i + 1, $val);"\` |
|    - |  730 | `   "  } else {"\` |
|    - |  731 | `   "    if( $last ){ $arr[$seg] = $val; return; }"\` |
|    - |  732 | `   "    if( !isset($arr[$seg]) \|\| !is_array($arr[$seg]) ){ $arr[$seg] = array(); }"\` |
|    - |  733 | `   "    __phl_parsestr_assign($arr[$seg], $segments, $i + 1, $val);"\` |
|    - |  734 | `   "  }"\` |
|    - |  735 | `   "}"\` |
|    - |  736 | `   "function parse_str($string, &$result){"\` |
|    - |  737 | `   "  $result = array();"\` |
|    - |  738 | `   "  $string = (string)$string;"\` |
|    - |  739 | `   "  if( $string === '' ){ return; }"\` |
|    - |  740 | `   "  foreach( explode('&', $string) as $pair ){"\` |
|    - |  741 | `   "    if( $pair === '' ){ continue; }"\` |
|    - |  742 | `   "    $eq = strpos($pair, '=');"\` |
|    - |  743 | `   "    if( $eq === false ){ $rawkey = $pair; $val = ''; }"\` |
|    - |  744 | `   "    else { $rawkey = substr($pair, 0, $eq); $val = urldecode(substr($pair, $eq + 1)); }"\` |
|    - |  745 | `   "    if( $rawkey === '' ){ continue; }"\` |
|    - |  746 | `   "    $bpos = strpos($rawkey, '[');"\` |
|    - |  747 | `   "    if( $bpos === false ){ $base = $rawkey; $subs = array(); }"\` |
|    - |  748 | `   "    else {"\` |
|    - |  749 | `   "      $base = substr($rawkey, 0, $bpos);"\` |
|    - |  750 | `   "      preg_match_all('/\\[([^\\]]*)\\]/', substr($rawkey, $bpos), $m);"\` |
|    - |  751 | `   "      $subs = $m[1];"\` |
|    - |  752 | `   "    }"\` |
|    - |  753 | `   "    $base = str_replace(array(' ', '.'), '_', urldecode($base));"\` |
|    - |  754 | `   "    if( $base === '' ){ continue; }"\` |
|    - |  755 | `   "    $segs = array($base);"\` |
|    - |  756 | `   "    foreach( $subs as $s ){ $segs[] = urldecode($s); }"\` |
|    - |  757 | `   "    __phl_parsestr_assign($result, $segs, 0, $val);"\` |
|    - |  758 | `   "  }"\` |
|    - |  759 | `   "}"\` |
|    - |  760 | `   "/* php 8.3 str_increment(): Perl-style alphanumeric increment. */"\` |
|    - |  761 | `   "function str_increment($string){"\` |
|    - |  762 | `   "  $string = (string)$string;"\` |
|    - |  763 | `   "  if( $string === '' ){ throw new ValueError('str_increment(): Argument #1 ($string) must not be empty'); }"\` |
|    - |  764 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_increment(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|    - |  765 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|    - |  766 | `   "    $c = $string[$i];"\` |
|    - |  767 | `   "    if( $c === 'z' ){ $string[$i] = 'a'; }"\` |
|    - |  768 | `   "    elseif( $c === 'Z' ){ $string[$i] = 'A'; }"\` |
|    - |  769 | `   "    elseif( $c === '9' ){ $string[$i] = '0'; }"\` |
|    - |  770 | `   "    else { $string[$i] = chr(ord($c) + 1); return $string; }"\` |
|    - |  771 | `   "  }"\` |
|    - |  772 | `   "  $first = $string[0];"\` |
|    - |  773 | `   "  if( $first === '0' ){ return '1' . $string; }"\` |
|    - |  774 | `   "  if( $first === 'a' ){ return 'a' . $string; }"\` |
|    - |  775 | `   "  return 'A' . $string;"\` |
|    - |  776 | `   "}"\` |
|    - |  777 | `   "/* php 8.3 str_decrement(): inverse of str_increment(); throws out of range"\` |
|    - |  778 | `   " * at the bottom of the counting sequence. */"\` |
|    - |  779 | `   "function str_decrement($string){"\` |
|    - |  780 | `   "  $string = (string)$string;"\` |
|    - |  781 | `   "  if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) must not be empty'); }"\` |
|    - |  782 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_decrement(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|    - |  783 | `   "  $orig = $string;"\` |
|    - |  784 | `   "  $borrowed = false;"\` |
|    - |  785 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|    - |  786 | `   "    $c = $string[$i];"\` |
|    - |  787 | `   "    if( $c === 'a' ){ $string[$i] = 'z'; }"\` |
|    - |  788 | `   "    elseif( $c === 'A' ){ $string[$i] = 'Z'; }"\` |
|    - |  789 | `   "    elseif( $c === '0' ){ $string[$i] = '9'; }"\` |
|    - |  790 | `   "    else { $string[$i] = chr(ord($c) - 1); $borrowed = false; break; }"\` |
|    - |  791 | `   "    if( $i === 0 ){ $borrowed = true; }"\` |
|    - |  792 | `   "  }"\` |
|    - |  793 | `   "  if( $borrowed ){"\` |
|    - |  794 | `   "    if( $string[0] === '9' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|    - |  795 | `   "    $string = substr($string, 1);"\` |
|    - |  796 | `   "    if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|    - |  797 | `   "  } elseif( strlen($string) > 1 && $string[0] === '0' ){"\` |
|    - |  798 | `   "    $string = substr($string, 1);"\` |
|    - |  799 | `   "  }"\` |
|    - |  800 | `   "  return $string;"\` |
|    - |  801 | `   "}"\` |
|    - |  802 | `   "/* Permission bits via stat(); false + warning when stat fails, like php. */"\` |
|    - |  803 | `   "function fileperms($filename){"\` |
|    - |  804 | `   "  $s = @stat($filename);"\` |
|    - |  805 | `   "  if( $s === false ){"\` |
|    - |  806 | `   "    trigger_error('fileperms(): stat failed for ' . $filename, E_USER_WARNING);"\` |
|    - |  807 | `   "    return false;"\` |
|    - |  808 | `   "  }"\` |
|    - |  809 | `   "  return $s['mode'];"\` |
|    - |  810 | `   "}"\` |
|    - |  811 | `   "/* PH7 keeps no stat cache, so this is a no-op like php on a clean cache. */"\` |
|    - |  812 | `   "function clearstatcache($clear_realpath_cache = false, $filename = ''){}"\` |
|    - |  813 | `   "/* php 8.4 mb_ucfirst/mb_lcfirst: case-map only the first multibyte char. */"\` |
|    - |  814 | `   "function mb_ucfirst($string, $encoding = null){"\` |
|    - |  815 | `   "  $string = (string)$string;"\` |
|    - |  816 | `   "  if( $string === '' ){ return ''; }"\` |
|    - |  817 | `   "  return mb_strtoupper(mb_substr($string, 0, 1)) . mb_substr($string, 1);"\` |
|    - |  818 | `   "}"\` |
|    - |  819 | `   "function mb_lcfirst($string, $encoding = null){"\` |
|    - |  820 | `   "  $string = (string)$string;"\` |
|    - |  821 | `   "  if( $string === '' ){ return ''; }"\` |
|    - |  822 | `   "  return mb_strtolower(mb_substr($string, 0, 1)) . mb_substr($string, 1);"\` |
|    - |  823 | `   "}"\` |
|    - |  824 | `   "/* php 8.4 mb_trim family: strip leading/trailing characters (whole"\` |
|    - |  825 | `   " * multibyte chars, NO range syntax), defaulting to php's Unicode"\` |
|    - |  826 | `   " * whitespace set. */"\` |
|    - |  827 | `   "function __phl_mb_ws(){"\` |
|    - |  828 | `   "  static $set = null;"\` |
|    - |  829 | `   "  if( $set === null ){"\` |
|    - |  830 | `   "    $set = array();"\` |
|    - |  831 | `   "    foreach( array(0x00,0x09,0x0A,0x0B,0x0C,0x0D,0x20,0x85,0xA0,0x1680,"\` |
|    - |  832 | `   "      0x180E,0x2000,0x2001,0x2002,0x2003,0x2004,0x2005,0x2006,0x2007,0x2008,"\` |
|    - |  833 | `   "      0x2009,0x200A,0x2028,0x2029,0x202F,0x205F,0x3000) as $cp ){"\` |
|    - |  834 | `   "      $set[mb_chr($cp)] = true;"\` |
|    - |  835 | `   "    }"\` |
|    - |  836 | `   "  }"\` |
|    - |  837 | `   "  return $set;"\` |
|    - |  838 | `   "}"\` |
|    - |  839 | `   "function __phl_mb_trim($string, $characters, $left, $right){"\` |
|    - |  840 | `   "  $string = (string)$string;"\` |
|    - |  841 | `   "  if( $string === '' ){ return ''; }"\` |
|    - |  842 | `   "  if( $characters === null ){"\` |
|    - |  843 | `   "    $set = __phl_mb_ws();"\` |
|    - |  844 | `   "  } else {"\` |
|    - |  845 | `   "    $set = array();"\` |
|    - |  846 | `   "    foreach( mb_str_split((string)$characters) as $c ){ $set[$c] = true; }"\` |
|    - |  847 | `   "  }"\` |
|    - |  848 | `   "  $chars = mb_str_split($string);"\` |
|    - |  849 | `   "  $n = count($chars);"\` |
|    - |  850 | `   "  $i = 0; $j = $n;"\` |
|    - |  851 | `   "  if( $left ){ while( $i < $j && isset($set[$chars[$i]]) ){ $i++; } }"\` |
|    - |  852 | `   "  if( $right ){ while( $j > $i && isset($set[$chars[$j - 1]]) ){ $j--; } }"\` |
|    - |  853 | `   "  return implode('', array_slice($chars, $i, $j - $i));"\` |
|    - |  854 | `   "}"\` |
|    - |  855 | `   "function mb_trim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, true, true); }"\` |
|    - |  856 | `   "function mb_ltrim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, true, false); }"\` |
|    - |  857 | `   "function mb_rtrim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, false, true); }"\` |
|    - |  858 | `   "/* Creates a temporary file and returns its name */"\` |
|    - |  859 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|    - |  860 | `   "{"\` |
|    - |  861 | `   "   /* php CREATES the file (empty, mode 0600) and guarantees the name is unique --"\` |
|    - |  862 | `   "    * returning a bare name left the caller with a path that does not exist, so"\` |
|    - |  863 | `   "    * file_exists() was false and unlink() failed on it. */"\` |
|    - |  864 | `   "   $zDir = rtrim($zDir, DIRECTORY_SEPARATOR);"\` |
|    - |  865 | `   "   for( $i = 0 ; $i < 64 ; ++$i ){"\` |
|    - |  866 | `   "     $zPath = $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|    - |  867 | `   "     if( file_exists($zPath) ){ continue; }"\` |
|    - |  868 | `   "     $pHandle = @fopen($zPath,'x');"\` |
|    - |  869 | `   "     if( $pHandle === false ){ continue; }"\` |
|    - |  870 | `   "     fclose($pHandle);"\` |
|    - |  871 | `   "     @chmod($zPath, 0600);"\` |
|    - |  872 | `   "     return $zPath;"\` |
|    - |  873 | `   "   }"\` |
|    - |  874 | `   "   return false;"\` |
|    - |  875 | `   "}"\` |
|    - |  876 | `   "function array_unshift(&$pArray ){"\` |
|    - |  877 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|    - |  878 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|    - |  879 | `   "/* Copy arguments */"\` |
|    - |  880 | `   "$nArgs = func_num_args();"\` |
|    - |  881 | `   "$pNew = array();"\` |
|    - |  882 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|    - |  883 | `    " $pNew[] = func_get_arg($i);"\` |
|    - |  884 | `    "}"\` |
|    - |  885 | `   	"/* Make a copy of the old entries */"\` |
|    - |  886 | `	"$pOld = array_copy($pArray);"\` |
|    - |  887 | `	"/* Erase */"\` |
|    - |  888 | `	"array_erase($pArray);"\` |
|    - |  889 | `	"/* Unshift */"\` |
|    - |  890 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|    - |  891 | `	"return sizeof($pArray);"\` |
|    - |  892 | `    "}"\` |
|    - |  893 | `	"function array_merge_recursive(){"\` |
|    - |  894 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|    - |  895 | `    "$arrays = func_get_args();"\` |
|    - |  896 | `    "$narrays = count($arrays);"\` |
|    - |  897 | `    "$ret = array();"\` |
|    - |  898 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|    - |  899 | `	 " if( !is_array($arrays[$i]) ){"\` |
|    - |  900 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|    - |  901 | `	 " }"\` |
|    - |  902 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|    - |  903 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|    - |  904 | `     "  if( $keyIsInt ) {"\` |
|    - |  905 | `     "   $ret[] = $value;"\` |
|    - |  906 | `     "  } else {"\` |
|    - |  907 | `     "   if (array_key_exists($key, $ret)) {"\` |
|    - |  908 | `     "    $cur = $ret[$key];"\` |
|    - |  909 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|    - |  910 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|    - |  911 | `     "    } elseif (is_array($cur)) {"\` |
|    - |  912 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|    - |  913 | `     "    } elseif (is_array($value)) {"\` |
|    - |  914 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|    - |  915 | `     "    } else {"\` |
|    - |  916 | `     "     $ret[$key] = array($cur, $value);"\` |
|    - |  917 | `     "    }"\` |
|    - |  918 | `     "   } else {"\` |
|    - |  919 | `     "    $ret[$key] = $value;"\` |
|    - |  920 | `     "   }"\` |
|    - |  921 | `     "  }"\` |
|    - |  922 | `     " }"\` |
|    - |  923 | `	 " }"\` |
|    - |  924 | `	 " return $ret;"\` |
|    - |  925 | `    "}"\` |
|    - |  926 | `	/* __php_zpp_type: php's ZPP value-name for TypeError messages */\` |
|    - |  927 | `	"function __php_zpp_type($v){"\` |
|    - |  928 | `	" if( is_object($v) ){ return get_class($v); }"\` |
|    - |  929 | `	" if( is_int($v) ){ return 'int'; }"\` |
|    - |  930 | `	" if( is_float($v) ){ return 'float'; }"\` |
|    - |  931 | `	" if( is_string($v) ){ return 'string'; }"\` |
|    - |  932 | `	" if( is_bool($v) ){ return $v ? 'true' : 'false'; }"\` |
|    - |  933 | `	" if( is_null($v) ){ return 'null'; }"\` |
|    - |  934 | `	" if( is_array($v) ){ return 'array'; }"\` |
|    - |  935 | `	" if( is_resource($v) ){ return 'resource'; }"\` |
|    - |  936 | `	" return 'mixed';"\` |
|    - |  937 | `	"}"\` |
|    - |  938 | `	"function max(){"\` |
|    - |  939 | `    "  $pArgs = func_get_args();"\` |
|    - |  940 | `    " if( sizeof($pArgs) < 1 ){"\` |
|    - |  941 | `	"  throw new ArgumentCountError('max() expects at least 1 argument, 0 given');"\` |
|    - |  942 | `    " }"\` |
|    - |  943 | `    " if( sizeof($pArgs) < 2 ){"\` |
|    - |  944 | `    " $pArg = $pArgs[0];"\` |
|    - |  945 | `	" if( !is_array($pArg) ){"\` |
|    - |  946 | `	"   throw new TypeError('max(): Argument #1 ($value) must be of type array, ' . __php_zpp_type($pArg) . ' given');"\` |
|    - |  947 | `	" }"\` |
|    - |  948 | `	" if( sizeof($pArg) < 1 ){"\` |
|    - |  949 | `	"   throw new ValueError('max(): Argument #1 ($value) must contain at least one element');"\` |
|    - |  950 | `	" }"\` |
|    - |  951 | `	" $max = null; $first = true;"\` |
|    - |  952 | `	" foreach( $pArgs[0] as $val ){"\` |
|    - |  953 | `	"   if( $first ){ $max = $val; $first = false; }"\` |
|    - |  954 | `	"   else if( $val > $max ){ $max = $val; }"\` |
|    - |  955 | `	" }"\` |
|    - |  956 | `	" return $max;"\` |
|    - |  957 | `    " }"\` |
|    - |  958 | `    " $max = $pArgs[0];"\` |
|    - |  959 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|    - |  960 | `    " $val = $pArgs[$i];"\` |
|    - |  961 | `	"if( $val > $max ){"\` |
|    - |  962 | `	" $max = $val;"\` |
|    - |  963 | `	"}"\` |
|    - |  964 | `    " }"\` |
|    - |  965 | `	" return $max;"\` |
|    - |  966 | `    "}"\` |
|    - |  967 | `	"function min(){"\` |
|    - |  968 | `    "  $pArgs = func_get_args();"\` |
|    - |  969 | `    " if( sizeof($pArgs) < 1 ){"\` |
|    - |  970 | `	"  throw new ArgumentCountError('min() expects at least 1 argument, 0 given');"\` |
|    - |  971 | `    " }"\` |
|    - |  972 | `    " if( sizeof($pArgs) < 2 ){"\` |
|    - |  973 | `    " $pArg = $pArgs[0];"\` |
|    - |  974 | `	" if( !is_array($pArg) ){"\` |
|    - |  975 | `	"   throw new TypeError('min(): Argument #1 ($value) must be of type array, ' . __php_zpp_type($pArg) . ' given');"\` |
|    - |  976 | `	" }"\` |
|    - |  977 | `	" if( sizeof($pArg) < 1 ){"\` |
|    - |  978 | `	"   throw new ValueError('min(): Argument #1 ($value) must contain at least one element');"\` |
|    - |  979 | `	" }"\` |
|    - |  980 | `	" $min = null; $first = true;"\` |
|    - |  981 | `	" foreach( $pArgs[0] as $val ){"\` |
|    - |  982 | `	"   if( $first ){ $min = $val; $first = false; }"\` |
|    - |  983 | `	"   else if( $val < $min ){ $min = $val; }"\` |
|    - |  984 | `	" }"\` |
|    - |  985 | `	" return $min;"\` |
|    - |  986 | `    " }"\` |
|    - |  987 | `    " $min = $pArgs[0];"\` |
|    - |  988 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|    - |  989 | `    " $val = $pArgs[$i];"\` |
|    - |  990 | `	"if( $val < $min ){"\` |
|    - |  991 | `	" $min = $val;"\` |
|    - |  992 | `	" }"\` |
|    - |  993 | `    " }"\` |
|    - |  994 | `	" return $min;"\` |
|    - |  995 | `	"}"\` |
|    - |  996 | `	"function fileowner(string $file){"\` |
|    - |  997 | `    " $a = stat($file);"\` |
|    - |  998 | `	" if( !is_array($a) ){"\` |
|    - |  999 | `	"	return false;"\` |
|    - | 1000 | `	" }"\` |
|    - | 1001 | `	" return $a['uid'];"\` |
|    - | 1002 | `    "}"\` |
|    - | 1003 | `    "function filegroup(string $file){"\` |
|    - | 1004 | `	" $a = stat($file);"\` |
|    - | 1005 | `	" if( !is_array($a) ){"\` |
|    - | 1006 | `	"	return false;"\` |
|    - | 1007 | `	" }"\` |
|    - | 1008 | `	" return $a['gid'];"\` |
|    - | 1009 | `    "}"\` |
|    - | 1010 | `	 "function fileinode(string $file){"\` |
|    - | 1011 | `	" $a = stat($file);"\` |
|    - | 1012 | `	" if( !is_array($a) ){"\` |
|    - | 1013 | `	"	return false;"\` |
|    - | 1014 | `	" }"\` |
|    - | 1015 | `	" return $a['ino'];"\` |
|    - | 1016 | `    "}"` |
|    - | 1017 |  |
| 3878 | 1018 | `PH7_PRIVATE sxi32 PH7_VmInstallBuiltinLib(ph7_vm *pVm)` |
|    5 | 1019 | `{` |
|    - | 1020 | `	SyString sBuiltin;` |
|    - | 1021 | `	SyString sRandom;` |
| 3883 | 1022 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|    - | 1023 | `	/* Compile the built-in library */` |
| 3883 | 1024 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|    - | 1025 | `	/* Register the Random\RandomException namespaced class (PHP 8.2+).` |
|    - | 1026 | `	 * Kept in its own VmEvalChunk (not appended to PH7_BUILTIN_LIB): a namespace` |
|    - | 1027 | `	 * declaration is NOT reset at the block's closing brace in this engine, so` |
|    - | 1028 | `	 * anything following it in the same chunk would leak into the Random` |
|    - | 1029 | `	 * namespace. Isolation instead comes from VmEvalChunk saving/restoring` |
|    - | 1030 | `	 * pVm->sNamespace (and PH7_ResetCodeGenerator clearing the compiler` |
|    - | 1031 | `	 * namespace) per chunk, so this lands as Random\RandomException while later` |
|    - | 1032 | `	 * user code still compiles in the global namespace. */` |
|    - | 1033 | `	{` |
|    - | 1034 | `		static const char zRandomLib[] =` |
|    - | 1035 | `			"namespace Random { class RandomException extends \\Exception { } }";` |
| 3883 | 1036 | `		SyStringInitFromBuf(&sRandom,zRandomLib,sizeof(zRandomLib)-1);` |
| 3883 | 1037 | `		VmEvalChunk(&(*pVm),0,&sRandom,PH7_PHP_ONLY,FALSE);` |
|    - | 1038 | `	}` |
| 3883 | 1039 | `	return SXRET_OK;` |
|    5 | 1040 | `}` |
|    - | 1041 |  |
