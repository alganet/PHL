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
|    - |  192 | `	"class UnhandledMatchError extends Error { }"\` |
|    - |  193 | `	"class ErrorException extends Exception { "\` |
|    - |  194 | `	"protected $severity;"\` |
|    - |  195 | `	"public function __construct(?string $message = null,"\` |
|    - |  196 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,?Throwable $previous = null){"\` |
|    - |  197 | `	"   /* message/code/previous belong to Exception (trace/previous are private"\` |
|    - |  198 | `	"    * to it); delegate, then set our own severity plus the caller-supplied"\` |
|    - |  199 | `	"    * file/line, which are protected and stay writable here. */"\` |
|    - |  200 | `	"   parent::__construct($message, $code, $previous);"\` |
|    - |  201 | `	"   $this->severity = $severity;"\` |
|    - |  202 | `	"   $this->file = $filename;"\` |
|    - |  203 | `	"   $this->line = $lineno;"\` |
|    - |  204 | `	"}"\` |
|    - |  205 | `	"public function getSeverity(){"\` |
|    - |  206 | `	"   return $this->severity;"\` |
|    - |  207 | `    "}"\` |
|    - |  208 | `	"}"\` |
|    - |  209 | `	"/* SPL exceptions: thin tree, inherit Exception's ctor+getters. Roots first. */"\` |
|    - |  210 | `	"class LogicException extends Exception { }"\` |
|    - |  211 | `	"class RuntimeException extends Exception { }"\` |
|    - |  212 | `	"class BadFunctionCallException extends LogicException { }"\` |
|    - |  213 | `	"class BadMethodCallException extends BadFunctionCallException { }"\` |
|    - |  214 | `	"class DomainException extends LogicException { }"\` |
|    - |  215 | `	"class InvalidArgumentException extends LogicException { }"\` |
|    - |  216 | `	"class LengthException extends LogicException { }"\` |
|    - |  217 | `	"class OutOfRangeException extends LogicException { }"\` |
|    - |  218 | `	"class OutOfBoundsException extends RuntimeException { }"\` |
|    - |  219 | `	"class OverflowException extends RuntimeException { }"\` |
|    - |  220 | `	"class RangeException extends RuntimeException { }"\` |
|    - |  221 | `	"class UnderflowException extends RuntimeException { }"\` |
|    - |  222 | `	"class UnexpectedValueException extends RuntimeException { }"\` |
|    - |  223 | `	"class JsonException extends Exception { }"\` |
|    - |  224 | `	"interface Iterator extends Traversable {"\` |
|    - |  225 | `	"public function current();"\` |
|    - |  226 | `	"public function key();"\` |
|    - |  227 | `	"public function next();"\` |
|    - |  228 | `	"public function rewind();"\` |
|    - |  229 | `	"public function valid();"\` |
|    - |  230 | `	"}"\` |
|    - |  231 | `	"interface IteratorAggregate extends Traversable {"\` |
|    - |  232 | `	"public function getIterator();"\` |
|    - |  233 | `	"}"\` |
|    - |  234 | `	"interface Serializable {"\` |
|    - |  235 | `	"public function serialize();"\` |
|    - |  236 | `	"public function unserialize(string $serialized);"\` |
|    - |  237 | `	"}"\` |
|    - |  238 | `	"/* Directory releated IO */"\` |
|    - |  239 | `	"class Directory {"\` |
|    - |  240 | `	"public $handle = null;"\` |
|    - |  241 | `	"public $path  = null;"\` |
|    - |  242 | `	"public function __construct(string $path)"\` |
|    - |  243 | `	"{"\` |
|    - |  244 | `	"   $this->handle = opendir($path);"\` |
|    - |  245 | `	"   if( $this->handle !== FALSE ){"\` |
|    - |  246 | `	"      $this->path = $path;"\` |
|    - |  247 | `	"   }"\` |
|    - |  248 | `	"}"\` |
|    - |  249 | `	"public function __destruct()"\` |
|    - |  250 | `	"{"\` |
|    - |  251 | `	"  if( $this->handle != null ){"\` |
|    - |  252 | `	"       closedir($this->handle);"\` |
|    - |  253 | `	"  }"\` |
|    - |  254 | `	"}"\` |
|    - |  255 | `	"public function read()"\` |
|    - |  256 | `	"{"\` |
|    - |  257 | `	"    return readdir($this->handle);"\` |
|    - |  258 | `	"}"\` |
|    - |  259 | `	"public function rewind()"\` |
|    - |  260 | `	"{"\` |
|    - |  261 | `	"    rewinddir($this->handle);"\` |
|    - |  262 | `	"}"\` |
|    - |  263 | `	"public function close()"\` |
|    - |  264 | `	"{"\` |
|    - |  265 | `	"    closedir($this->handle);"\` |
|    - |  266 | `	"    $this->handle = null;"\` |
|    - |  267 | `	"}"\` |
|    - |  268 | `	"}"\` |
|    - |  269 | `	"class Fiber {"\` |
|    - |  270 | `	"  private $__ctx;"\` |
|    - |  271 | `	"  private $__callable;"\` |
|    - |  272 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|    - |  273 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|    - |  274 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|    - |  275 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|    - |  276 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|    - |  277 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|    - |  278 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|    - |  279 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|    - |  280 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|    - |  281 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|    - |  282 | `	"}"\` |
|    - |  283 | `	"class Generator implements Iterator {"\` |
|    - |  284 | `	"  private $__ctx;"\` |
|    - |  285 | `	"  public function current(){ return __gen_current($this); }"\` |
|    - |  286 | `	"  public function key(){ return __gen_key($this); }"\` |
|    - |  287 | `	"  public function next(){ return __gen_next($this); }"\` |
|    - |  288 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|    - |  289 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|    - |  290 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|    - |  291 | `	"  public function throw(Throwable $exception){ return __gen_throw($this,$exception); }"\` |
|    - |  292 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|    - |  293 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|    - |  294 | `	"}"\` |
|    - |  295 | `	"final class Closure {"\` |
|    - |  296 | `	"  private $__fn;"\` |
|    - |  297 | `	"  private $__this;"\` |
|    - |  298 | `	"  private $__scope;"\` |
|    - |  299 | `	"  public function __construct(){ throw new \\Error('Instantiation of class Closure is not allowed'); }"\` |
|    - |  300 | `	"  public function bindTo($newThis, $scope = 'static'){ return __closure_bindTo($this, $newThis, $scope); }"\` |
|    - |  301 | `	"  public function call($newThis, ...$args){ $bound = __closure_bindTo($this, $newThis, get_class($newThis)); return $bound(...$args); }"\` |
|    - |  302 | `	"  public static function bind($closure, $newThis, $scope = 'static'){ return __closure_bindTo($closure, $newThis, $scope); }"\` |
|    - |  303 | `	"  public static function fromCallable($callable){ return __closure_fromCallable($callable); }"\` |
|    - |  304 | `	"}"\` |
|    - |  305 | `	/* stdClass is empty (PHP-exact): holds only dynamic (runtime-added) properties. */\` |
|    - |  306 | `	"#[Attribute(Attribute::TARGET_CLASS)]"\` |
|    - |  307 | `	"final class Attribute {"\` |
|    - |  308 | `	"  const TARGET_CLASS = 1;"\` |
|    - |  309 | `	"  const TARGET_FUNCTION = 2;"\` |
|    - |  310 | `	"  const TARGET_METHOD = 4;"\` |
|    - |  311 | `	"  const TARGET_PROPERTY = 8;"\` |
|    - |  312 | `	"  const TARGET_CLASS_CONSTANT = 16;"\` |
|    - |  313 | `	"  const TARGET_PARAMETER = 32;"\` |
|    - |  314 | `	"  const TARGET_CONSTANT = 64;"\` |
|    - |  315 | `	"  const TARGET_ALL = 127;"\` |
|    - |  316 | `	"  const IS_REPEATABLE = 128;"\` |
|    - |  317 | `	"  public $flags;"\` |
|    - |  318 | `	"  public function __construct($flags = 127){ $this->flags = $flags; }"\` |
|    - |  319 | `	"}"\` |
|    - |  320 | `	"#[Attribute(Attribute::TARGET_METHOD \| Attribute::TARGET_FUNCTION \| Attribute::TARGET_CLASS_CONSTANT \| Attribute::TARGET_CONSTANT)]"\` |
|    - |  321 | `	"final class Deprecated {"\` |
|    - |  322 | `	"  public $message;"\` |
|    - |  323 | `	"  public $since;"\` |
|    - |  324 | `	"  public function __construct($message = null, $since = null){"\` |
|    - |  325 | `	"    $this->message = $message;"\` |
|    - |  326 | `	"    $this->since = $since;"\` |
|    - |  327 | `	"  }"\` |
|    - |  328 | `	"}"\` |
|    - |  329 | `	"class stdClass{"\` |
|    - |  330 | `	"}"\` |
|    - |  331 | `	"function dir(string $path){"\` |
|    - |  332 | `	"   return new Directory($path);"\` |
|    - |  333 | `	"}"\` |
|    - |  334 | `	"function Dir(string $path){"\` |
|    - |  335 | `	"   return new Directory($path);"\` |
|    - |  336 | `	"}"\` |
|    - |  337 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|    - |  338 | `    "{"\` |
|    - |  339 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|    - |  340 | `	"  $aDir = array();"\` |
|    - |  341 | `	"  $pHandle = opendir($directory);"\` |
|    - |  342 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|    - |  343 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|    - |  344 | `	"      $aDir[] = $pEntry;"\` |
|    - |  345 | `	"   }"\` |
|    - |  346 | `	"  closedir($pHandle);"\` |
|    - |  347 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|    - |  348 | `	"      rsort($aDir);"\` |
|    - |  349 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|    - |  350 | `	"      sort($aDir);"\` |
|    - |  351 | `	"  }"\` |
|    - |  352 | `	"  return $aDir;"\` |
|    - |  353 | `	"}"\` |
|    - |  354 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|    - |  355 | `	"/* php keeps the literal directory portion of the pattern in every result;"\` |
|    - |  356 | `	"   split off everything up to and including the last '/' as the prefix. */"\` |
|    - |  357 | `	"$slash = strrpos($pattern,'/');"\` |
|    - |  358 | `	"if( $slash === false ){ $zDir = '.'; $prefix = ''; $pat = $pattern; }"\` |
|    - |  359 | `	"else { $zDir = substr($pattern,0,$slash); if( $zDir === '' ){ $zDir = '/'; } $prefix = substr($pattern,0,$slash+1); $pat = substr($pattern,$slash+1); }"\` |
|    - |  360 | `	"$pHandle = opendir($zDir);"\` |
|    - |  361 | `	"if( $pHandle == FALSE ){"\` |
|    - |  362 | `	"   /* IO error while opening the target directory,return FALSE */"\` |
|    - |  363 | `	"	return FALSE;"\` |
|    - |  364 | `	"}"\` |
|    - |  365 | `	"$pArray = array(); /* Empty array */"\` |
|    - |  366 | `	"/* Loop throw available entries */"\` |
|    - |  367 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|    - |  368 | `	" /* php's glob() never matches a leading-dot entry (incl. '.' and '..') unless"\` |
|    - |  369 | `	"    the pattern itself starts with a dot */"\` |
|    - |  370 | `	"	if( strlen($pEntry) > 0 && $pEntry[0] === '.' && (strlen($pat) < 1 \|\| $pat[0] !== '.') ){ continue; }"\` |
|    - |  371 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|    - |  372 | `	"	$rc = strglob($pat,$pEntry);"\` |
|    - |  373 | `	"	if( $rc ){"\` |
|    - |  374 | `	"	   $zFull = $prefix . $pEntry;"\` |
|    - |  375 | `	"	   if( is_dir($zDir . '/' . $pEntry) ){"\` |
|    - |  376 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|    - |  377 | `	"		     /* Adds a slash to each directory returned */"\` |
|    - |  378 | `	"			 $zFull .= DIRECTORY_SEPARATOR;"\` |
|    - |  379 | `	"		  }"\` |
|    - |  380 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|    - |  381 | `	"	     /* Not a directory,ignore */"\` |
|    - |  382 | `	"		 continue;"\` |
|    - |  383 | `	"	   }"\` |
|    - |  384 | `	"	   /* Add the entry (with its literal directory prefix, php-style) */"\` |
|    - |  385 | `	"	   $pArray[] = $zFull;"\` |
|    - |  386 | `	"	}"\` |
|    - |  387 | `	" }"\` |
|    - |  388 | `	"/* Close the handle */"\` |
|    - |  389 | `	"closedir($pHandle);"\` |
|    - |  390 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|    - |  391 | `	"  /* Sort the array */"\` |
|    - |  392 | `	"  sort($pArray);"\` |
|    - |  393 | `	"}"\` |
|    - |  394 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|    - |  395 | `	"  /* Return the search pattern if no files matching were found */"\` |
|    - |  396 | `	"  $pArray[] = $pattern;"\` |
|    - |  397 | `	"}"\` |
|    - |  398 | `	"/* Return the created array */"\` |
|    - |  399 | `	"return $pArray;"\` |
|    - |  400 | `   "}"\` |
|    - |  401 | `   "/* Creates a temporary file */"\` |
|    - |  402 | `   "function tmpfile(){"\` |
|    - |  403 | `   "  /* Extract the temp directory */"\` |
|    - |  404 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|    - |  405 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|    - |  406 | `   "    /* Use the current dir */"\` |
|    - |  407 | `   "    $zTempDir = '.';"\` |
|    - |  408 | `   "  }"\` |
|    - |  409 | `   "  /* Create the file */"\` |
|    - |  410 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|    - |  411 | `   "  return $pHandle;"\` |
|    - |  412 | `   "}"\` |
|    - |  413 | `   "/* php's number_format(): missing entirely from PH7. */"\` |
|    - |  414 | `   "function number_format($num, $decimals = 0, $dec_point = '.', $thousands_sep = ','){"\` |
|    - |  415 | `   "  $num = (float)$num;"\` |
|    - |  416 | `   "  $decimals = (int)$decimals;"\` |
|    - |  417 | `   "  if( $decimals < 0 ){ $decimals = 0; }"\` |
|    - |  418 | `   "  if( $dec_point === null ){ $dec_point = '.'; }"\` |
|    - |  419 | `   "  if( $thousands_sep === null ){ $thousands_sep = ','; }"\` |
|    - |  420 | `   "  /* round() first: sprintf uses banker's rounding, php's number_format rounds"\` |
|    - |  421 | `   "   * half AWAY FROM ZERO (number_format(0.5) is '1', not '0'). */"\` |
|    - |  422 | `   "  $num = round($num, $decimals);"\` |
|    - |  423 | `   "  $s = sprintf('%.' . $decimals . 'f', $num);"\` |
|    - |  424 | `   "  $neg = false;"\` |
|    - |  425 | `   "  if( substr($s, 0, 1) === '-' ){ $neg = true; $s = substr($s, 1); }"\` |
|    - |  426 | `   "  $parts = explode('.', $s);"\` |
|    - |  427 | `   "  $int = $parts[0];"\` |
|    - |  428 | `   "  $frac = count($parts) > 1 ? $parts[1] : '';"\` |
|    - |  429 | `   "  $out = '';"\` |
|    - |  430 | `   "  $len = strlen($int);"\` |
|    - |  431 | `   "  $c = 0;"\` |
|    - |  432 | `   "  for( $i = $len - 1 ; $i >= 0 ; $i-- ){"\` |
|    - |  433 | `   "    $out = $int[$i] . $out;"\` |
|    - |  434 | `   "    $c++;"\` |
|    - |  435 | `   "    if( $c % 3 === 0 && $i > 0 ){ $out = $thousands_sep . $out; }"\` |
|    - |  436 | `   "  }"\` |
|    - |  437 | `   "  if( $decimals > 0 ){ $out = $out . $dec_point . $frac; }"\` |
|    - |  438 | `   "  if( $neg ){ $out = '-' . $out; }"\` |
|    - |  439 | `   "  return $out;"\` |
|    - |  440 | `   "}"\` |
|    - |  441 | `   "function is_nan($v){ $v = (float)$v; return $v != $v; }"\` |
|    - |  442 | `   "function is_infinite($v){ $v = (float)$v; return $v == INF \|\| $v == -INF; }"\` |
|    - |  443 | `   "function is_finite($v){ $v = (float)$v; return !is_nan($v) && !is_infinite($v); }"\` |
|    - |  444 | `   "/* php's version_compare: canonicalise (separators + digit/alpha boundaries all"\` |
|    - |  445 | `   " * become '.'), then compare parts with the special dev<alpha<beta<RC<#<pl ordering. */"\` |
|    - |  446 | `   "function __phl_vcanon($v){"\` |
|    - |  447 | `   "  $v = (string)$v; $len = strlen($v); $out = '';"\` |
|    - |  448 | `   "  for( $i = 0; $i < $len; $i++ ){"\` |
|    - |  449 | `   "   $c = $v[$i]; $rp = $i + 1 < $len ? $v[$i + 1] : '';"\` |
|    - |  450 | `   "   $cd = ($c >= '0' && $c <= '9');"\` |
|    - |  451 | `   "   $ca = $cd \|\| ($c >= 'a' && $c <= 'z') \|\| ($c >= 'A' && $c <= 'Z');"\` |
|    - |  452 | `   "   if( !$ca ){"\` |
|    - |  453 | `   "    /* any non-alphanumeric (., -, _, +, ...) is a separator: emit one '.' */"\` |
|    - |  454 | `   "    if( $out !== '' && substr($out, -1) !== '.' ){ $out .= '.'; }"\` |
|    - |  455 | `   "   }else{"\` |
|    - |  456 | `   "    $out .= $c;"\` |
|    - |  457 | `   "    $rd = ($rp >= '0' && $rp <= '9');"\` |
|    - |  458 | `   "    $ra = $rd \|\| ($rp >= 'a' && $rp <= 'z') \|\| ($rp >= 'A' && $rp <= 'Z');"\` |
|    - |  459 | `   "    if( $rp !== '' && $ra && ($cd !== $rd) ){ $out .= '.'; }"\` |
|    - |  460 | `   "   }"\` |
|    - |  461 | `   "  }"\` |
|    - |  462 | `   "  return explode('.', $out);"\` |
|    - |  463 | `   "}"\` |
|    - |  464 | `   "function __phl_vform($s){"\` |
|    - |  465 | `   "  if( $s === '' ){ return -1; }"\` |
|    - |  466 | `   "  if( ctype_digit($s) ){ return 4; }"\` |
|    - |  467 | `   "  $f = array('dev' => 0, 'alpha' => 1, 'a' => 1, 'beta' => 2, 'b' => 2, 'RC' => 3, 'rc' => 3, 'pl' => 5, 'p' => 5);"\` |
|    - |  468 | `   "  foreach( $f as $name => $ord ){ if( strncmp($s, $name, strlen($name)) === 0 ){ return $ord; } }"\` |
|    - |  469 | `   "  return -1;"\` |
|    - |  470 | `   "}"\` |
|    - |  471 | `   "function version_compare($version1, $version2, $operator = null){"\` |
|    - |  472 | `   "  $v1 = __phl_vcanon($version1); $v2 = __phl_vcanon($version2);"\` |
|    - |  473 | `   "  $n1 = count($v1); $n2 = count($v2); $n = $n1 > $n2 ? $n1 : $n2; $cmp = 0;"\` |
|    - |  474 | `   "  for( $i = 0; $i < $n; $i++ ){"\` |
|    - |  475 | `   "   $a = $i < $n1 ? $v1[$i] : null; $b = $i < $n2 ? $v2[$i] : null;"\` |
|    - |  476 | `   "   if( $a === null ){ $cmp = ctype_digit($b) ? -1 : (4 <=> __phl_vform($b)); }"\` |
|    - |  477 | `   "   elseif( $b === null ){ $cmp = ctype_digit($a) ? 1 : (__phl_vform($a) <=> 4); }"\` |
|    - |  478 | `   "   elseif( ctype_digit($a) && ctype_digit($b) ){ $cmp = (int)$a <=> (int)$b; }"\` |
|    - |  479 | `   "   else{ $cmp = __phl_vform($a) <=> __phl_vform($b); }"\` |
|    - |  480 | `   "   if( $cmp !== 0 ){ break; }"\` |
|    - |  481 | `   "  }"\` |
|    - |  482 | `   "  if( $operator === null ){ return $cmp; }"\` |
|    - |  483 | `   "  switch( (string)$operator ){"\` |
|    - |  484 | `   "   case '<': case 'lt': return $cmp < 0;"\` |
|    - |  485 | `   "   case '<=': case 'le': return $cmp <= 0;"\` |
|    - |  486 | `   "   case '>': case 'gt': return $cmp > 0;"\` |
|    - |  487 | `   "   case '>=': case 'ge': return $cmp >= 0;"\` |
|    - |  488 | `   "   case '==': case '=': case 'eq': return $cmp === 0;"\` |
|    - |  489 | `   "   case '!=': case '<>': case 'ne': return $cmp !== 0;"\` |
|    - |  490 | `   "  }"\` |
|    - |  491 | `   "  return null;"\` |
|    - |  492 | `   "}"\` |
|    - |  493 | `   "/* phl.stub_extensions (a -d/php.ini list, comma-separated) declares extensions"\` |
|    - |  494 | `   " * PHL does not implement as LOADED, backed by no-op behaviour, so software that"\` |
|    - |  495 | `   " * only GATES on extension_loaded() (e.g. PHPUnit's dom/xmlwriter check) runs"\` |
|    - |  496 | `   " * unmodified. It does NOT synthesize the extension's classes/functions. */"\` |
|    - |  497 | `   "function __phl_stub_exts(){"\` |
|    - |  498 | `   "  $s = ini_get('phl.stub_extensions');"\` |
|    - |  499 | `   "  if( $s === false \|\| $s === '' ){ return array(); }"\` |
|    - |  500 | `   "  $out = array();"\` |
|    - |  501 | `   "  foreach( explode(',', (string)$s) as $e ){ $e = trim($e); if( $e !== '' ){ $out[strtolower($e)] = $e; } }"\` |
|    - |  502 | `   "  return $out;"\` |
|    - |  503 | `   "}"\` |
|    - |  504 | `   "function extension_loaded($name){"\` |
|    - |  505 | `   "  static $ext = array('core' => 1, 'standard' => 1, 'pcre' => 1, 'json' => 1,"\` |
|    - |  506 | `   "   'ctype' => 1, 'date' => 1, 'spl' => 1, 'reflection' => 1, 'mbstring' => 1,"\` |
|    - |  507 | `   "   'hash' => 1, 'filter' => 1, 'session' => 1" PHL_EXT_LOADED_LIBXML ");"\` |
|    - |  508 | `   "  $n = strtolower((string)$name);"\` |
|    - |  509 | `   "  if( isset($ext[$n]) ){ return true; }"\` |
|    - |  510 | `   "  $stub = __phl_stub_exts();"\` |
|    - |  511 | `   "  return isset($stub[$n]);"\` |
|    - |  512 | `   "}"\` |
|    - |  513 | `   "function get_loaded_extensions($zend_extensions = false){"\` |
|    - |  514 | `   "  if( $zend_extensions ){ return array(); }"\` |
|    - |  515 | `   "  $base = array('Core','date','pcre','SPL','json','standard',"\` |
|    - |  516 | `   "   'ctype','filter','hash','Reflection','session','mbstring'" PHL_EXT_LIST_LIBXML ");"\` |
|    - |  517 | `   "  foreach( __phl_stub_exts() as $e ){ $base[] = $e; }"\` |
|    - |  518 | `   "  return $base;"\` |
|    - |  519 | `   "}"\` |
|    - |  520 | `   "/* Inverse of bin2hex() */"\` |
|    - |  521 | `   "function hex2bin($str){"\` |
|    - |  522 | `   "  $str = (string)$str;"\` |
|    - |  523 | `   "  $len = strlen($str);"\` |
|    - |  524 | `   "  if( $len % 2 !== 0 ){"\` |
|    - |  525 | `   "    trigger_error('hex2bin(): Hexadecimal input string must have an even length', E_USER_WARNING);"\` |
|    - |  526 | `   "    return false;"\` |
|    - |  527 | `   "  }"\` |
|    - |  528 | `   "  $out = '';"\` |
|    - |  529 | `   "  for( $i = 0 ; $i < $len ; $i += 2 ){"\` |
|    - |  530 | `   "    $pair = substr($str, $i, 2);"\` |
|    - |  531 | `   "    if( !ctype_xdigit($pair) ){"\` |
|    - |  532 | `   "      trigger_error('hex2bin(): Input string must be hexadecimal string', E_USER_WARNING);"\` |
|    - |  533 | `   "      return false;"\` |
|    - |  534 | `   "    }"\` |
|    - |  535 | `   "    $out = $out . chr(hexdec($pair));"\` |
|    - |  536 | `   "  }"\` |
|    - |  537 | `   "  return $out;"\` |
|    - |  538 | `   "}"\` |
|    - |  539 | `   "/* Division that never throws: INF/-INF/NAN like php */"\` |
|    - |  540 | `   "function fdiv($a, $b){"\` |
|    - |  541 | `   "  $a = (float)$a;"\` |
|    - |  542 | `   "  $b = (float)$b;"\` |
|    - |  543 | `   "  if( $b == 0.0 ){"\` |
|    - |  544 | `   "    if( $a == 0.0 \|\| is_nan($a) ){ return NAN; }"\` |
|    - |  545 | `   "    return $a > 0 ? INF : -INF;"\` |
|    - |  546 | `   "  }"\` |
|    - |  547 | `   "  return $a / $b;"\` |
|    - |  548 | `   "}"\` |
|    - |  549 | `   "function checkdate($month, $day, $year){"\` |
|    - |  550 | `   "  $month = (int)$month; $day = (int)$day; $year = (int)$year;"\` |
|    - |  551 | `   "  if( $month < 1 \|\| $month > 12 \|\| $year < 1 \|\| $year > 32767 \|\| $day < 1 ){ return false; }"\` |
|    - |  552 | `   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\` |
|    - |  553 | `   "  $max = $days[$month - 1];"\` |
|    - |  554 | `   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) \|\| ($year % 400 === 0)) ){"\` |
|    - |  555 | `   "    $max = 29;"\` |
|    - |  556 | `   "  }"\` |
|    - |  557 | `   "  return $day <= $max;"\` |
|    - |  558 | `   "}"\` |
|    - |  559 | `   "function is_iterable($v){ return is_array($v) \|\| ($v instanceof Traversable); }"\` |
|    - |  560 | `   "function is_countable($v){ return is_array($v) \|\| ($v instanceof Countable); }"\` |
|    - |  561 | `   "function key_exists($key, $array){ return array_key_exists($key, $array); }"\` |
|    - |  562 | `   "function doubleval($v){ return (float)$v; }"\` |
|    - |  563 | `   "function array_count_values($array){"\` |
|    - |  564 | `   "  $out = array();"\` |
|    - |  565 | `   "  foreach( $array as $v ){"\` |
|    - |  566 | `   "    if( !is_int($v) && !is_string($v) ){"\` |
|    - |  567 | `   "      trigger_error('array_count_values(): Can only count string and integer values, entry skipped', E_USER_WARNING);"\` |
|    - |  568 | `   "      continue;"\` |
|    - |  569 | `   "    }"\` |
|    - |  570 | `   "    if( isset($out[$v]) ){ $out[$v] = $out[$v] + 1; } else { $out[$v] = 1; }"\` |
|    - |  571 | `   "  }"\` |
|    - |  572 | `   "  return $out;"\` |
|    - |  573 | `   "}"\` |
|    - |  574 | `   "function array_change_key_case($array, $case = CASE_LOWER){"\` |
|    - |  575 | `   "  $out = array();"\` |
|    - |  576 | `   "  foreach( $array as $k => $v ){"\` |
|    - |  577 | `   "    if( is_string($k) ){ $k = ($case == CASE_UPPER) ? strtoupper($k) : strtolower($k); }"\` |
|    - |  578 | `   "    $out[$k] = $v;"\` |
|    - |  579 | `   "  }"\` |
|    - |  580 | `   "  return $out;"\` |
|    - |  581 | `   "}"\` |
|    - |  582 | `   "function array_replace_recursive($array, ...$others){"\` |
|    - |  583 | `   "  foreach( $others as $o ){"\` |
|    - |  584 | `   "    foreach( $o as $k => $v ){"\` |
|    - |  585 | `   "      if( is_array($v) && isset($array[$k]) && is_array($array[$k]) ){"\` |
|    - |  586 | `   "        $array[$k] = array_replace_recursive($array[$k], $v);"\` |
|    - |  587 | `   "      }else{"\` |
|    - |  588 | `   "        $array[$k] = $v;"\` |
|    - |  589 | `   "      }"\` |
|    - |  590 | `   "    }"\` |
|    - |  591 | `   "  }"\` |
|    - |  592 | `   "  return $array;"\` |
|    - |  593 | `   "}"\` |
|    - |  594 | `   "function class_uses($what, $autoload = true){"\` |
|    - |  595 | `   "  $c = is_object($what) ? get_class($what) : (string)$what;"\` |
|    - |  596 | `   "  if( !class_exists($c) ){ return false; }"\` |
|    - |  597 | `   "  return array();  /* PHL has no traits yet -- always the empty set */"\` |
|    - |  598 | `   "}"\` |
|    - |  599 | `   "function count_chars($str, $mode = 0){"\` |
|    - |  600 | `   "  $str = (string)$str;"\` |
|    - |  601 | `   "  $counts = array();"\` |
|    - |  602 | `   "  for( $i = 0 ; $i < 256 ; $i++ ){ $counts[$i] = 0; }"\` |
|    - |  603 | `   "  $len = strlen($str);"\` |
|    - |  604 | `   "  for( $i = 0 ; $i < $len ; $i++ ){ $b = ord($str[$i]); $counts[$b] = $counts[$b] + 1; }"\` |
|    - |  605 | `   "  if( $mode == 1 ){"\` |
|    - |  606 | `   "    $out = array();"\` |
|    - |  607 | `   "    foreach( $counts as $b => $n ){ if( $n > 0 ){ $out[$b] = $n; } }"\` |
|    - |  608 | `   "    return $out;"\` |
|    - |  609 | `   "  }"\` |
|    - |  610 | `   "  if( $mode == 3 ){"\` |
|    - |  611 | `   "    $out = '';"\` |
|    - |  612 | `   "    foreach( $counts as $b => $n ){ if( $n > 0 ){ $out = $out . chr($b); } }"\` |
|    - |  613 | `   "    return $out;"\` |
|    - |  614 | `   "  }"\` |
|    - |  615 | `   "  return $counts;"\` |
|    - |  616 | `   "}"\` |
|    - |  617 | `   "function ip2long($ip){"\` |
|    - |  618 | `   "  $p = explode('.', (string)$ip);"\` |
|    - |  619 | `   "  if( count($p) !== 4 ){ return false; }"\` |
|    - |  620 | `   "  $n = 0;"\` |
|    - |  621 | `   "  foreach( $p as $o ){"\` |
|    - |  622 | `   "    if( !ctype_digit($o) \|\| (int)$o < 0 \|\| (int)$o > 255 ){ return false; }"\` |
|    - |  623 | `   "    $n = $n * 256 + (int)$o;"\` |
|    - |  624 | `   "  }"\` |
|    - |  625 | `   "  return $n;"\` |
|    - |  626 | `   "}"\` |
|    - |  627 | `   "function long2ip($n){"\` |
|    - |  628 | `   "  $n = (int)$n;"\` |
|    - |  629 | `   "  return (($n >> 24) & 255) . '.' . (($n >> 16) & 255) . '.' . (($n >> 8) & 255) . '.' . ($n & 255);"\` |
|    - |  630 | `   "}"\` |
|    - |  631 | `   "function preg_filter($pattern, $replacement, $subject, $limit = -1){"\` |
|    - |  632 | `   "  if( is_array($subject) ){"\` |
|    - |  633 | `   "    $out = array();"\` |
|    - |  634 | `   "    foreach( $subject as $k => $v ){"\` |
|    - |  635 | `   "      $r = preg_replace($pattern, $replacement, (string)$v, $limit, $cnt);"\` |
|    - |  636 | `   "      if( $cnt > 0 ){ $out[$k] = $r; }"\` |
|    - |  637 | `   "    }"\` |
|    - |  638 | `   "    return $out;"\` |
|    - |  639 | `   "  }"\` |
|    - |  640 | `   "  $r = preg_replace($pattern, $replacement, (string)$subject, $limit, $cnt);"\` |
|    - |  641 | `   "  return $cnt > 0 ? $r : null;"\` |
|    - |  642 | `   "}"\` |
|    - |  643 | `   "function preg_replace_callback_array($patterns, $subject, $limit = -1){"\` |
|    - |  644 | `   "  foreach( $patterns as $pat => $cb ){"\` |
|    - |  645 | `   "    $subject = preg_replace_callback($pat, $cb, $subject, $limit);"\` |
|    - |  646 | `   "  }"\` |
|    - |  647 | `   "  return $subject;"\` |
|    - |  648 | `   "}"\` |
|    - |  649 | `   "function cal_days_in_month($calendar, $month, $year){"\` |
|    - |  650 | `   "  $month = (int)$month; $year = (int)$year;"\` |
|    - |  651 | `   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\` |
|    - |  652 | `   "  if( $month < 1 \|\| $month > 12 ){"\` |
|    - |  653 | `   "    throw new ValueError('cal_days_in_month(): Argument #2 ($month) must be a valid month');"\` |
|    - |  654 | `   "  }"\` |
|    - |  655 | `   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) \|\| ($year % 400 === 0)) ){"\` |
|    - |  656 | `   "    return 29;"\` |
|    - |  657 | `   "  }"\` |
|    - |  658 | `   "  return $days[$month - 1];"\` |
|    - |  659 | `   "}"\` |
|    - |  660 | `   "function preg_grep($pattern, $array, $flags = 0){"\` |
|    - |  661 | `   "  $out = array();"\` |
|    - |  662 | `   "  foreach( $array as $k => $v ){"\` |
|    - |  663 | `   "    $m = preg_match($pattern, (string)$v);"\` |
|    - |  664 | `   "    if( $flags & PREG_GREP_INVERT ){ $m = !$m; }"\` |
|    - |  665 | `   "    if( $m ){ $out[$k] = $v; }"\` |
|    - |  666 | `   "  }"\` |
|    - |  667 | `   "  return $out;"\` |
|    - |  668 | `   "}"\` |
|    - |  669 | `   "function class_implements($what, $autoload = true){"\` |
|    - |  670 | `   "  $c = is_object($what) ? get_class($what) : (string)$what;"\` |
|    - |  671 | `   "  if( !class_exists($c) && !interface_exists($c) ){ return false; }"\` |
|    - |  672 | `   "  $out = array();"\` |
|    - |  673 | `   "  $r = new ReflectionClass($c);"\` |
|    - |  674 | `   "  foreach( $r->getInterfaceNames() as $i ){ $out[$i] = $i; }"\` |
|    - |  675 | `   "  return $out;"\` |
|    - |  676 | `   "}"\` |
|    - |  677 | `   "function class_parents($what, $autoload = true){"\` |
|    - |  678 | `   "  $c = is_object($what) ? get_class($what) : (string)$what;"\` |
|    - |  679 | `   "  if( !class_exists($c) ){ return false; }"\` |
|    - |  680 | `   "  $out = array();"\` |
|    - |  681 | `   "  $r = new ReflectionClass($c);"\` |
|    - |  682 | `   "  while( ($p = $r->getParentClass()) ){"\` |
|    - |  683 | `   "    $n = $p->getName();"\` |
|    - |  684 | `   "    $out[$n] = $n;"\` |
|    - |  685 | `   "    $r = $p;"\` |
|    - |  686 | `   "  }"\` |
|    - |  687 | `   "  return $out;"\` |
|    - |  688 | `   "}"\` |
|    - |  689 | `   "/* php's http_build_query() -- missing from PH7. Skips null values, casts"\` |
|    - |  690 | `   " * bool to 1/0, prefixes numeric top-level keys, urlencodes per RFC. */"\` |
|    - |  691 | `   "function __phl_hbq_enc($s, $enc){"\` |
|    - |  692 | `   "  return $enc == PHP_QUERY_RFC3986 ? rawurlencode((string)$s) : urlencode((string)$s);"\` |
|    - |  693 | `   "}"\` |
|    - |  694 | `   "function __phl_hbq(&$pairs, $data, $key_prefix, $numeric_prefix, $sep, $enc){"\` |
|    - |  695 | `   "  foreach( $data as $k => $v ){"\` |
|    - |  696 | `   "    if( $v === null ){ continue; }"\` |
|    - |  697 | `   "    if( $key_prefix === '' ){"\` |
|    - |  698 | `   "      $ek = is_int($k) ? __phl_hbq_enc($numeric_prefix . $k, $enc) : __phl_hbq_enc($k, $enc);"\` |
|    - |  699 | `   "    } else {"\` |
|    - |  700 | `   "      $ek = $key_prefix . '%5B' . __phl_hbq_enc($k, $enc) . '%5D';"\` |
|    - |  701 | `   "    }"\` |
|    - |  702 | `   "    if( is_array($v) ){"\` |
|    - |  703 | `   "      __phl_hbq($pairs, $v, $ek, $numeric_prefix, $sep, $enc);"\` |
|    - |  704 | `   "    } elseif( is_object($v) ){"\` |
|    - |  705 | `   "      __phl_hbq($pairs, get_object_vars($v), $ek, $numeric_prefix, $sep, $enc);"\` |
|    - |  706 | `   "    } else {"\` |
|    - |  707 | `   "      if( $v === true ){ $v = '1'; } elseif( $v === false ){ $v = '0'; }"\` |
|    - |  708 | `   "      $pairs[] = $ek . '=' . __phl_hbq_enc($v, $enc);"\` |
|    - |  709 | `   "    }"\` |
|    - |  710 | `   "  }"\` |
|    - |  711 | `   "}"\` |
|    - |  712 | `   "function http_build_query($data, $numeric_prefix = '', $arg_separator = null, $encoding_type = PHP_QUERY_RFC1738){"\` |
|    - |  713 | `   "  if( !is_array($data) && !is_object($data) ){"\` |
|    - |  714 | `   "    throw new TypeError('http_build_query(): Argument #1 ($data) must be of type array\|object, ' . gettype($data) . ' given');"\` |
|    - |  715 | `   "  }"\` |
|    - |  716 | `   "  if( $arg_separator === null ){ $arg_separator = '&'; }"\` |
|    - |  717 | `   "  $pairs = array();"\` |
|    - |  718 | `   "  __phl_hbq($pairs, is_object($data) ? get_object_vars($data) : $data, '', (string)$numeric_prefix, $arg_separator, $encoding_type);"\` |
|    - |  719 | `   "  return implode($arg_separator, $pairs);"\` |
|    - |  720 | `   "}"\` |
|    - |  721 | `   "/* php's parse_str() -- missing from PH7. Mangles the base name ('.'/' ' -> '_'),"\` |
|    - |  722 | `   " * parses [key] nesting and [] appends, urldecodes keys and values. */"\` |
|    - |  723 | `   "function __phl_parsestr_assign(&$arr, $segments, $i, $val){"\` |
|    - |  724 | `   "  $seg = $segments[$i];"\` |
|    - |  725 | `   "  $last = ($i === count($segments) - 1);"\` |
|    - |  726 | `   "  if( $seg === '' ){"\` |
|    - |  727 | `   "    if( $last ){ $arr[] = $val; return; }"\` |
|    - |  728 | `   "    $arr[] = array();"\` |
|    - |  729 | `   "    $k = array_key_last($arr);"\` |
|    - |  730 | `   "    __phl_parsestr_assign($arr[$k], $segments, $i + 1, $val);"\` |
|    - |  731 | `   "  } else {"\` |
|    - |  732 | `   "    if( $last ){ $arr[$seg] = $val; return; }"\` |
|    - |  733 | `   "    if( !isset($arr[$seg]) \|\| !is_array($arr[$seg]) ){ $arr[$seg] = array(); }"\` |
|    - |  734 | `   "    __phl_parsestr_assign($arr[$seg], $segments, $i + 1, $val);"\` |
|    - |  735 | `   "  }"\` |
|    - |  736 | `   "}"\` |
|    - |  737 | `   "function parse_str($string, &$result){"\` |
|    - |  738 | `   "  $result = array();"\` |
|    - |  739 | `   "  $string = (string)$string;"\` |
|    - |  740 | `   "  if( $string === '' ){ return; }"\` |
|    - |  741 | `   "  foreach( explode('&', $string) as $pair ){"\` |
|    - |  742 | `   "    if( $pair === '' ){ continue; }"\` |
|    - |  743 | `   "    $eq = strpos($pair, '=');"\` |
|    - |  744 | `   "    if( $eq === false ){ $rawkey = $pair; $val = ''; }"\` |
|    - |  745 | `   "    else { $rawkey = substr($pair, 0, $eq); $val = urldecode(substr($pair, $eq + 1)); }"\` |
|    - |  746 | `   "    if( $rawkey === '' ){ continue; }"\` |
|    - |  747 | `   "    $bpos = strpos($rawkey, '[');"\` |
|    - |  748 | `   "    if( $bpos === false ){ $base = $rawkey; $subs = array(); }"\` |
|    - |  749 | `   "    else {"\` |
|    - |  750 | `   "      $base = substr($rawkey, 0, $bpos);"\` |
|    - |  751 | `   "      preg_match_all('/\\[([^\\]]*)\\]/', substr($rawkey, $bpos), $m);"\` |
|    - |  752 | `   "      $subs = $m[1];"\` |
|    - |  753 | `   "    }"\` |
|    - |  754 | `   "    $base = str_replace(array(' ', '.'), '_', urldecode($base));"\` |
|    - |  755 | `   "    if( $base === '' ){ continue; }"\` |
|    - |  756 | `   "    $segs = array($base);"\` |
|    - |  757 | `   "    foreach( $subs as $s ){ $segs[] = urldecode($s); }"\` |
|    - |  758 | `   "    __phl_parsestr_assign($result, $segs, 0, $val);"\` |
|    - |  759 | `   "  }"\` |
|    - |  760 | `   "}"\` |
|    - |  761 | `   "/* php 8.3 str_increment(): Perl-style alphanumeric increment. */"\` |
|    - |  762 | `   "function str_increment($string){"\` |
|    - |  763 | `   "  $string = (string)$string;"\` |
|    - |  764 | `   "  if( $string === '' ){ throw new ValueError('str_increment(): Argument #1 ($string) must not be empty'); }"\` |
|    - |  765 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_increment(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|    - |  766 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|    - |  767 | `   "    $c = $string[$i];"\` |
|    - |  768 | `   "    if( $c === 'z' ){ $string[$i] = 'a'; }"\` |
|    - |  769 | `   "    elseif( $c === 'Z' ){ $string[$i] = 'A'; }"\` |
|    - |  770 | `   "    elseif( $c === '9' ){ $string[$i] = '0'; }"\` |
|    - |  771 | `   "    else { $string[$i] = chr(ord($c) + 1); return $string; }"\` |
|    - |  772 | `   "  }"\` |
|    - |  773 | `   "  $first = $string[0];"\` |
|    - |  774 | `   "  if( $first === '0' ){ return '1' . $string; }"\` |
|    - |  775 | `   "  if( $first === 'a' ){ return 'a' . $string; }"\` |
|    - |  776 | `   "  return 'A' . $string;"\` |
|    - |  777 | `   "}"\` |
|    - |  778 | `   "/* php 8.3 str_decrement(): inverse of str_increment(); throws out of range"\` |
|    - |  779 | `   " * at the bottom of the counting sequence. */"\` |
|    - |  780 | `   "function str_decrement($string){"\` |
|    - |  781 | `   "  $string = (string)$string;"\` |
|    - |  782 | `   "  if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) must not be empty'); }"\` |
|    - |  783 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_decrement(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|    - |  784 | `   "  $orig = $string;"\` |
|    - |  785 | `   "  $borrowed = false;"\` |
|    - |  786 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|    - |  787 | `   "    $c = $string[$i];"\` |
|    - |  788 | `   "    if( $c === 'a' ){ $string[$i] = 'z'; }"\` |
|    - |  789 | `   "    elseif( $c === 'A' ){ $string[$i] = 'Z'; }"\` |
|    - |  790 | `   "    elseif( $c === '0' ){ $string[$i] = '9'; }"\` |
|    - |  791 | `   "    else { $string[$i] = chr(ord($c) - 1); $borrowed = false; break; }"\` |
|    - |  792 | `   "    if( $i === 0 ){ $borrowed = true; }"\` |
|    - |  793 | `   "  }"\` |
|    - |  794 | `   "  if( $borrowed ){"\` |
|    - |  795 | `   "    if( $string[0] === '9' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|    - |  796 | `   "    $string = substr($string, 1);"\` |
|    - |  797 | `   "    if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|    - |  798 | `   "  } elseif( strlen($string) > 1 && $string[0] === '0' ){"\` |
|    - |  799 | `   "    $string = substr($string, 1);"\` |
|    - |  800 | `   "  }"\` |
|    - |  801 | `   "  return $string;"\` |
|    - |  802 | `   "}"\` |
|    - |  803 | `   "/* Permission bits via stat(); false + warning when stat fails, like php. */"\` |
|    - |  804 | `   "function fileperms($filename){"\` |
|    - |  805 | `   "  $s = @stat($filename);"\` |
|    - |  806 | `   "  if( $s === false ){"\` |
|    - |  807 | `   "    trigger_error('fileperms(): stat failed for ' . $filename, E_USER_WARNING);"\` |
|    - |  808 | `   "    return false;"\` |
|    - |  809 | `   "  }"\` |
|    - |  810 | `   "  return $s['mode'];"\` |
|    - |  811 | `   "}"\` |
|    - |  812 | `   "/* PH7 keeps no stat cache, so this is a no-op like php on a clean cache. */"\` |
|    - |  813 | `   "function clearstatcache($clear_realpath_cache = false, $filename = ''){}"\` |
|    - |  814 | `   "/* php 8.4 mb_ucfirst/mb_lcfirst: case-map only the first multibyte char. */"\` |
|    - |  815 | `   "function mb_ucfirst($string, $encoding = null){"\` |
|    - |  816 | `   "  $string = (string)$string;"\` |
|    - |  817 | `   "  if( $string === '' ){ return ''; }"\` |
|    - |  818 | `   "  return mb_strtoupper(mb_substr($string, 0, 1)) . mb_substr($string, 1);"\` |
|    - |  819 | `   "}"\` |
|    - |  820 | `   "function mb_lcfirst($string, $encoding = null){"\` |
|    - |  821 | `   "  $string = (string)$string;"\` |
|    - |  822 | `   "  if( $string === '' ){ return ''; }"\` |
|    - |  823 | `   "  return mb_strtolower(mb_substr($string, 0, 1)) . mb_substr($string, 1);"\` |
|    - |  824 | `   "}"\` |
|    - |  825 | `   "/* php 8.4 mb_trim family: strip leading/trailing characters (whole"\` |
|    - |  826 | `   " * multibyte chars, NO range syntax), defaulting to php's Unicode"\` |
|    - |  827 | `   " * whitespace set. */"\` |
|    - |  828 | `   "function __phl_mb_ws(){"\` |
|    - |  829 | `   "  static $set = null;"\` |
|    - |  830 | `   "  if( $set === null ){"\` |
|    - |  831 | `   "    $set = array();"\` |
|    - |  832 | `   "    foreach( array(0x00,0x09,0x0A,0x0B,0x0C,0x0D,0x20,0x85,0xA0,0x1680,"\` |
|    - |  833 | `   "      0x180E,0x2000,0x2001,0x2002,0x2003,0x2004,0x2005,0x2006,0x2007,0x2008,"\` |
|    - |  834 | `   "      0x2009,0x200A,0x2028,0x2029,0x202F,0x205F,0x3000) as $cp ){"\` |
|    - |  835 | `   "      $set[mb_chr($cp)] = true;"\` |
|    - |  836 | `   "    }"\` |
|    - |  837 | `   "  }"\` |
|    - |  838 | `   "  return $set;"\` |
|    - |  839 | `   "}"\` |
|    - |  840 | `   "function __phl_mb_trim($string, $characters, $left, $right){"\` |
|    - |  841 | `   "  $string = (string)$string;"\` |
|    - |  842 | `   "  if( $string === '' ){ return ''; }"\` |
|    - |  843 | `   "  if( $characters === null ){"\` |
|    - |  844 | `   "    $set = __phl_mb_ws();"\` |
|    - |  845 | `   "  } else {"\` |
|    - |  846 | `   "    $set = array();"\` |
|    - |  847 | `   "    foreach( mb_str_split((string)$characters) as $c ){ $set[$c] = true; }"\` |
|    - |  848 | `   "  }"\` |
|    - |  849 | `   "  $chars = mb_str_split($string);"\` |
|    - |  850 | `   "  $n = count($chars);"\` |
|    - |  851 | `   "  $i = 0; $j = $n;"\` |
|    - |  852 | `   "  if( $left ){ while( $i < $j && isset($set[$chars[$i]]) ){ $i++; } }"\` |
|    - |  853 | `   "  if( $right ){ while( $j > $i && isset($set[$chars[$j - 1]]) ){ $j--; } }"\` |
|    - |  854 | `   "  return implode('', array_slice($chars, $i, $j - $i));"\` |
|    - |  855 | `   "}"\` |
|    - |  856 | `   "function mb_trim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, true, true); }"\` |
|    - |  857 | `   "function mb_ltrim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, true, false); }"\` |
|    - |  858 | `   "function mb_rtrim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, false, true); }"\` |
|    - |  859 | `   "/* Creates a temporary file and returns its name */"\` |
|    - |  860 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|    - |  861 | `   "{"\` |
|    - |  862 | `   "   /* php CREATES the file (empty, mode 0600) and guarantees the name is unique --"\` |
|    - |  863 | `   "    * returning a bare name left the caller with a path that does not exist, so"\` |
|    - |  864 | `   "    * file_exists() was false and unlink() failed on it. */"\` |
|    - |  865 | `   "   $zDir = rtrim($zDir, DIRECTORY_SEPARATOR);"\` |
|    - |  866 | `   "   for( $i = 0 ; $i < 64 ; ++$i ){"\` |
|    - |  867 | `   "     $zPath = $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|    - |  868 | `   "     if( file_exists($zPath) ){ continue; }"\` |
|    - |  869 | `   "     $pHandle = @fopen($zPath,'x');"\` |
|    - |  870 | `   "     if( $pHandle === false ){ continue; }"\` |
|    - |  871 | `   "     fclose($pHandle);"\` |
|    - |  872 | `   "     @chmod($zPath, 0600);"\` |
|    - |  873 | `   "     return $zPath;"\` |
|    - |  874 | `   "   }"\` |
|    - |  875 | `   "   return false;"\` |
|    - |  876 | `   "}"\` |
|    - |  877 | `   "function array_unshift(&$pArray ){"\` |
|    - |  878 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|    - |  879 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|    - |  880 | `   "/* Copy arguments */"\` |
|    - |  881 | `   "$nArgs = func_num_args();"\` |
|    - |  882 | `   "$pNew = array();"\` |
|    - |  883 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|    - |  884 | `    " $pNew[] = func_get_arg($i);"\` |
|    - |  885 | `    "}"\` |
|    - |  886 | `   	"/* Make a copy of the old entries */"\` |
|    - |  887 | `	"$pOld = array_copy($pArray);"\` |
|    - |  888 | `	"/* Erase */"\` |
|    - |  889 | `	"array_erase($pArray);"\` |
|    - |  890 | `	"/* Unshift */"\` |
|    - |  891 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|    - |  892 | `	"return sizeof($pArray);"\` |
|    - |  893 | `    "}"\` |
|    - |  894 | `	"function array_merge_recursive(){"\` |
|    - |  895 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|    - |  896 | `    "$arrays = func_get_args();"\` |
|    - |  897 | `    "$narrays = count($arrays);"\` |
|    - |  898 | `    "$ret = array();"\` |
|    - |  899 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|    - |  900 | `	 " if( !is_array($arrays[$i]) ){"\` |
|    - |  901 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|    - |  902 | `	 " }"\` |
|    - |  903 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|    - |  904 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|    - |  905 | `     "  if( $keyIsInt ) {"\` |
|    - |  906 | `     "   $ret[] = $value;"\` |
|    - |  907 | `     "  } else {"\` |
|    - |  908 | `     "   if (array_key_exists($key, $ret)) {"\` |
|    - |  909 | `     "    $cur = $ret[$key];"\` |
|    - |  910 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|    - |  911 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|    - |  912 | `     "    } elseif (is_array($cur)) {"\` |
|    - |  913 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|    - |  914 | `     "    } elseif (is_array($value)) {"\` |
|    - |  915 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|    - |  916 | `     "    } else {"\` |
|    - |  917 | `     "     $ret[$key] = array($cur, $value);"\` |
|    - |  918 | `     "    }"\` |
|    - |  919 | `     "   } else {"\` |
|    - |  920 | `     "    $ret[$key] = $value;"\` |
|    - |  921 | `     "   }"\` |
|    - |  922 | `     "  }"\` |
|    - |  923 | `     " }"\` |
|    - |  924 | `	 " }"\` |
|    - |  925 | `	 " return $ret;"\` |
|    - |  926 | `    "}"\` |
|    - |  927 | `	/* __php_zpp_type: php's ZPP value-name for TypeError messages */\` |
|    - |  928 | `	"function __php_zpp_type($v){"\` |
|    - |  929 | `	" if( is_object($v) ){ return get_class($v); }"\` |
|    - |  930 | `	" if( is_int($v) ){ return 'int'; }"\` |
|    - |  931 | `	" if( is_float($v) ){ return 'float'; }"\` |
|    - |  932 | `	" if( is_string($v) ){ return 'string'; }"\` |
|    - |  933 | `	" if( is_bool($v) ){ return $v ? 'true' : 'false'; }"\` |
|    - |  934 | `	" if( is_null($v) ){ return 'null'; }"\` |
|    - |  935 | `	" if( is_array($v) ){ return 'array'; }"\` |
|    - |  936 | `	" if( is_resource($v) ){ return 'resource'; }"\` |
|    - |  937 | `	" return 'mixed';"\` |
|    - |  938 | `	"}"\` |
|    - |  939 | `	"function max(){"\` |
|    - |  940 | `    "  $pArgs = func_get_args();"\` |
|    - |  941 | `    " if( sizeof($pArgs) < 1 ){"\` |
|    - |  942 | `	"  throw new ArgumentCountError('max() expects at least 1 argument, 0 given');"\` |
|    - |  943 | `    " }"\` |
|    - |  944 | `    " if( sizeof($pArgs) < 2 ){"\` |
|    - |  945 | `    " $pArg = $pArgs[0];"\` |
|    - |  946 | `	" if( !is_array($pArg) ){"\` |
|    - |  947 | `	"   throw new TypeError('max(): Argument #1 ($value) must be of type array, ' . __php_zpp_type($pArg) . ' given');"\` |
|    - |  948 | `	" }"\` |
|    - |  949 | `	" if( sizeof($pArg) < 1 ){"\` |
|    - |  950 | `	"   throw new ValueError('max(): Argument #1 ($value) must contain at least one element');"\` |
|    - |  951 | `	" }"\` |
|    - |  952 | `	" $max = null; $first = true;"\` |
|    - |  953 | `	" foreach( $pArgs[0] as $val ){"\` |
|    - |  954 | `	"   if( $first ){ $max = $val; $first = false; }"\` |
|    - |  955 | `	"   else if( $val > $max ){ $max = $val; }"\` |
|    - |  956 | `	" }"\` |
|    - |  957 | `	" return $max;"\` |
|    - |  958 | `    " }"\` |
|    - |  959 | `    " $max = $pArgs[0];"\` |
|    - |  960 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|    - |  961 | `    " $val = $pArgs[$i];"\` |
|    - |  962 | `	"if( $val > $max ){"\` |
|    - |  963 | `	" $max = $val;"\` |
|    - |  964 | `	"}"\` |
|    - |  965 | `    " }"\` |
|    - |  966 | `	" return $max;"\` |
|    - |  967 | `    "}"\` |
|    - |  968 | `	"function min(){"\` |
|    - |  969 | `    "  $pArgs = func_get_args();"\` |
|    - |  970 | `    " if( sizeof($pArgs) < 1 ){"\` |
|    - |  971 | `	"  throw new ArgumentCountError('min() expects at least 1 argument, 0 given');"\` |
|    - |  972 | `    " }"\` |
|    - |  973 | `    " if( sizeof($pArgs) < 2 ){"\` |
|    - |  974 | `    " $pArg = $pArgs[0];"\` |
|    - |  975 | `	" if( !is_array($pArg) ){"\` |
|    - |  976 | `	"   throw new TypeError('min(): Argument #1 ($value) must be of type array, ' . __php_zpp_type($pArg) . ' given');"\` |
|    - |  977 | `	" }"\` |
|    - |  978 | `	" if( sizeof($pArg) < 1 ){"\` |
|    - |  979 | `	"   throw new ValueError('min(): Argument #1 ($value) must contain at least one element');"\` |
|    - |  980 | `	" }"\` |
|    - |  981 | `	" $min = null; $first = true;"\` |
|    - |  982 | `	" foreach( $pArgs[0] as $val ){"\` |
|    - |  983 | `	"   if( $first ){ $min = $val; $first = false; }"\` |
|    - |  984 | `	"   else if( $val < $min ){ $min = $val; }"\` |
|    - |  985 | `	" }"\` |
|    - |  986 | `	" return $min;"\` |
|    - |  987 | `    " }"\` |
|    - |  988 | `    " $min = $pArgs[0];"\` |
|    - |  989 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|    - |  990 | `    " $val = $pArgs[$i];"\` |
|    - |  991 | `	"if( $val < $min ){"\` |
|    - |  992 | `	" $min = $val;"\` |
|    - |  993 | `	" }"\` |
|    - |  994 | `    " }"\` |
|    - |  995 | `	" return $min;"\` |
|    - |  996 | `	"}"\` |
|    - |  997 | `	"function fileowner(string $file){"\` |
|    - |  998 | `    " $a = stat($file);"\` |
|    - |  999 | `	" if( !is_array($a) ){"\` |
|    - | 1000 | `	"	return false;"\` |
|    - | 1001 | `	" }"\` |
|    - | 1002 | `	" return $a['uid'];"\` |
|    - | 1003 | `    "}"\` |
|    - | 1004 | `    "function filegroup(string $file){"\` |
|    - | 1005 | `	" $a = stat($file);"\` |
|    - | 1006 | `	" if( !is_array($a) ){"\` |
|    - | 1007 | `	"	return false;"\` |
|    - | 1008 | `	" }"\` |
|    - | 1009 | `	" return $a['gid'];"\` |
|    - | 1010 | `    "}"\` |
|    - | 1011 | `	 "function fileinode(string $file){"\` |
|    - | 1012 | `	" $a = stat($file);"\` |
|    - | 1013 | `	" if( !is_array($a) ){"\` |
|    - | 1014 | `	"	return false;"\` |
|    - | 1015 | `	" }"\` |
|    - | 1016 | `	" return $a['ino'];"\` |
|    - | 1017 | `    "}"` |
|    - | 1018 |  |
| 3886 | 1019 | `PH7_PRIVATE sxi32 PH7_VmInstallBuiltinLib(ph7_vm *pVm)` |
|    5 | 1020 | `{` |
|    - | 1021 | `	SyString sBuiltin;` |
|    - | 1022 | `	SyString sRandom;` |
| 3891 | 1023 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|    - | 1024 | `	/* Compile the built-in library */` |
| 3891 | 1025 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|    - | 1026 | `	/* Register the Random\RandomException namespaced class (PHP 8.2+).` |
|    - | 1027 | `	 * Kept in its own VmEvalChunk (not appended to PH7_BUILTIN_LIB): a namespace` |
|    - | 1028 | `	 * declaration is NOT reset at the block's closing brace in this engine, so` |
|    - | 1029 | `	 * anything following it in the same chunk would leak into the Random` |
|    - | 1030 | `	 * namespace. Isolation instead comes from VmEvalChunk saving/restoring` |
|    - | 1031 | `	 * pVm->sNamespace (and PH7_ResetCodeGenerator clearing the compiler` |
|    - | 1032 | `	 * namespace) per chunk, so this lands as Random\RandomException while later` |
|    - | 1033 | `	 * user code still compiles in the global namespace. */` |
|    - | 1034 | `	{` |
|    - | 1035 | `		static const char zRandomLib[] =` |
|    - | 1036 | `			"namespace Random { class RandomException extends \\Exception { } }";` |
| 3891 | 1037 | `		SyStringInitFromBuf(&sRandom,zRandomLib,sizeof(zRandomLib)-1);` |
| 3891 | 1038 | `		VmEvalChunk(&(*pVm),0,&sRandom,PH7_PHP_ONLY,FALSE);` |
|    - | 1039 | `	}` |
| 3891 | 1040 | `	return SXRET_OK;` |
|    5 | 1041 | `}` |
|    - | 1042 |  |
