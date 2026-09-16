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
|    - |  193 | `	"class CompileError extends Error { }"\` |
|    - |  194 | `	"class ParseError extends CompileError { }"\` |
|    - |  195 | `	"class ErrorException extends Exception { "\` |
|    - |  196 | `	"protected $severity;"\` |
|    - |  197 | `	"public function __construct(?string $message = null,"\` |
|    - |  198 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,?Throwable $previous = null){"\` |
|    - |  199 | `	"   /* message/code/previous belong to Exception (trace/previous are private"\` |
|    - |  200 | `	"    * to it); delegate, then set our own severity plus the caller-supplied"\` |
|    - |  201 | `	"    * file/line, which are protected and stay writable here. */"\` |
|    - |  202 | `	"   parent::__construct($message, $code, $previous);"\` |
|    - |  203 | `	"   $this->severity = $severity;"\` |
|    - |  204 | `	"   $this->file = $filename;"\` |
|    - |  205 | `	"   $this->line = $lineno;"\` |
|    - |  206 | `	"}"\` |
|    - |  207 | `	"public function getSeverity(){"\` |
|    - |  208 | `	"   return $this->severity;"\` |
|    - |  209 | `    "}"\` |
|    - |  210 | `	"}"\` |
|    - |  211 | `	"/* SPL exceptions: thin tree, inherit Exception's ctor+getters. Roots first. */"\` |
|    - |  212 | `	"class LogicException extends Exception { }"\` |
|    - |  213 | `	"class RuntimeException extends Exception { }"\` |
|    - |  214 | `	"class BadFunctionCallException extends LogicException { }"\` |
|    - |  215 | `	"class BadMethodCallException extends BadFunctionCallException { }"\` |
|    - |  216 | `	"class DomainException extends LogicException { }"\` |
|    - |  217 | `	"class InvalidArgumentException extends LogicException { }"\` |
|    - |  218 | `	"class LengthException extends LogicException { }"\` |
|    - |  219 | `	"class OutOfRangeException extends LogicException { }"\` |
|    - |  220 | `	"class OutOfBoundsException extends RuntimeException { }"\` |
|    - |  221 | `	"class OverflowException extends RuntimeException { }"\` |
|    - |  222 | `	"class RangeException extends RuntimeException { }"\` |
|    - |  223 | `	"class UnderflowException extends RuntimeException { }"\` |
|    - |  224 | `	"class UnexpectedValueException extends RuntimeException { }"\` |
|    - |  225 | `	"class JsonException extends Exception { }"\` |
|    - |  226 | `	"interface Iterator extends Traversable {"\` |
|    - |  227 | `	"public function current();"\` |
|    - |  228 | `	"public function key();"\` |
|    - |  229 | `	"public function next();"\` |
|    - |  230 | `	"public function rewind();"\` |
|    - |  231 | `	"public function valid();"\` |
|    - |  232 | `	"}"\` |
|    - |  233 | `	"interface IteratorAggregate extends Traversable {"\` |
|    - |  234 | `	"public function getIterator();"\` |
|    - |  235 | `	"}"\` |
|    - |  236 | `	"interface Serializable {"\` |
|    - |  237 | `	"public function serialize();"\` |
|    - |  238 | `	"public function unserialize(string $serialized);"\` |
|    - |  239 | `	"}"\` |
|    - |  240 | `	"/* Directory releated IO */"\` |
|    - |  241 | `	"class Directory {"\` |
|    - |  242 | `	"public $handle = null;"\` |
|    - |  243 | `	"public $path  = null;"\` |
|    - |  244 | `	"public function __construct(string $path)"\` |
|    - |  245 | `	"{"\` |
|    - |  246 | `	"   $this->handle = opendir($path);"\` |
|    - |  247 | `	"   if( $this->handle !== FALSE ){"\` |
|    - |  248 | `	"      $this->path = $path;"\` |
|    - |  249 | `	"   }"\` |
|    - |  250 | `	"}"\` |
|    - |  251 | `	"public function __destruct()"\` |
|    - |  252 | `	"{"\` |
|    - |  253 | `	"  if( $this->handle != null ){"\` |
|    - |  254 | `	"       closedir($this->handle);"\` |
|    - |  255 | `	"  }"\` |
|    - |  256 | `	"}"\` |
|    - |  257 | `	"public function read()"\` |
|    - |  258 | `	"{"\` |
|    - |  259 | `	"    return readdir($this->handle);"\` |
|    - |  260 | `	"}"\` |
|    - |  261 | `	"public function rewind()"\` |
|    - |  262 | `	"{"\` |
|    - |  263 | `	"    rewinddir($this->handle);"\` |
|    - |  264 | `	"}"\` |
|    - |  265 | `	"public function close()"\` |
|    - |  266 | `	"{"\` |
|    - |  267 | `	"    closedir($this->handle);"\` |
|    - |  268 | `	"    $this->handle = null;"\` |
|    - |  269 | `	"}"\` |
|    - |  270 | `	"}"\` |
|    - |  271 | `	"class Fiber {"\` |
|    - |  272 | `	"  private $__ctx;"\` |
|    - |  273 | `	"  private $__callable;"\` |
|    - |  274 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|    - |  275 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|    - |  276 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|    - |  277 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|    - |  278 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|    - |  279 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|    - |  280 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|    - |  281 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|    - |  282 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|    - |  283 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|    - |  284 | `	"}"\` |
|    - |  285 | `	"class Generator implements Iterator {"\` |
|    - |  286 | `	"  private $__ctx;"\` |
|    - |  287 | `	"  public function current(){ return __gen_current($this); }"\` |
|    - |  288 | `	"  public function key(){ return __gen_key($this); }"\` |
|    - |  289 | `	"  public function next(){ return __gen_next($this); }"\` |
|    - |  290 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|    - |  291 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|    - |  292 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|    - |  293 | `	"  public function throw(Throwable $exception){ return __gen_throw($this,$exception); }"\` |
|    - |  294 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|    - |  295 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|    - |  296 | `	"}"\` |
|    - |  297 | `	"final class Closure {"\` |
|    - |  298 | `	"  private $__fn;"\` |
|    - |  299 | `	"  private $__this;"\` |
|    - |  300 | `	"  private $__scope;"\` |
|    - |  301 | `	"  public function __construct(){ throw new \\Error('Instantiation of class Closure is not allowed'); }"\` |
|    - |  302 | `	"  public function bindTo($newThis, $scope = 'static'){ return __closure_bindTo($this, $newThis, $scope); }"\` |
|    - |  303 | `	"  public function call($newThis, ...$args){ $bound = __closure_bindTo($this, $newThis, get_class($newThis)); return $bound(...$args); }"\` |
|    - |  304 | `	"  public static function bind($closure, $newThis, $scope = 'static'){ return __closure_bindTo($closure, $newThis, $scope); }"\` |
|    - |  305 | `	"  public static function fromCallable($callable){ return __closure_fromCallable($callable); }"\` |
|    - |  306 | `	"}"\` |
|    - |  307 | `	/* stdClass is empty (PHP-exact): holds only dynamic (runtime-added) properties. */\` |
|    - |  308 | `	"#[Attribute(Attribute::TARGET_CLASS)]"\` |
|    - |  309 | `	"final class Attribute {"\` |
|    - |  310 | `	"  const TARGET_CLASS = 1;"\` |
|    - |  311 | `	"  const TARGET_FUNCTION = 2;"\` |
|    - |  312 | `	"  const TARGET_METHOD = 4;"\` |
|    - |  313 | `	"  const TARGET_PROPERTY = 8;"\` |
|    - |  314 | `	"  const TARGET_CLASS_CONSTANT = 16;"\` |
|    - |  315 | `	"  const TARGET_PARAMETER = 32;"\` |
|    - |  316 | `	"  const TARGET_CONSTANT = 64;"\` |
|    - |  317 | `	"  const TARGET_ALL = 127;"\` |
|    - |  318 | `	"  const IS_REPEATABLE = 128;"\` |
|    - |  319 | `	"  public $flags;"\` |
|    - |  320 | `	"  public function __construct($flags = 127){ $this->flags = $flags; }"\` |
|    - |  321 | `	"}"\` |
|    - |  322 | `	"#[Attribute(Attribute::TARGET_METHOD \| Attribute::TARGET_FUNCTION \| Attribute::TARGET_CLASS_CONSTANT \| Attribute::TARGET_CONSTANT)]"\` |
|    - |  323 | `	"final class Deprecated {"\` |
|    - |  324 | `	"  public $message;"\` |
|    - |  325 | `	"  public $since;"\` |
|    - |  326 | `	"  public function __construct($message = null, $since = null){"\` |
|    - |  327 | `	"    $this->message = $message;"\` |
|    - |  328 | `	"    $this->since = $since;"\` |
|    - |  329 | `	"  }"\` |
|    - |  330 | `	"}"\` |
|    - |  331 | `	"class stdClass{"\` |
|    - |  332 | `	"}"\` |
|    - |  333 | `	"function dir(string $path){"\` |
|    - |  334 | `	"   return new Directory($path);"\` |
|    - |  335 | `	"}"\` |
|    - |  336 | `	"function Dir(string $path){"\` |
|    - |  337 | `	"   return new Directory($path);"\` |
|    - |  338 | `	"}"\` |
|    - |  339 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|    - |  340 | `    "{"\` |
|    - |  341 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|    - |  342 | `	"  $aDir = array();"\` |
|    - |  343 | `	"  $pHandle = opendir($directory);"\` |
|    - |  344 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|    - |  345 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|    - |  346 | `	"      $aDir[] = $pEntry;"\` |
|    - |  347 | `	"   }"\` |
|    - |  348 | `	"  closedir($pHandle);"\` |
|    - |  349 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|    - |  350 | `	"      rsort($aDir);"\` |
|    - |  351 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|    - |  352 | `	"      sort($aDir);"\` |
|    - |  353 | `	"  }"\` |
|    - |  354 | `	"  return $aDir;"\` |
|    - |  355 | `	"}"\` |
|    - |  356 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|    - |  357 | `	"/* php keeps the literal directory portion of the pattern in every result;"\` |
|    - |  358 | `	"   split off everything up to and including the last '/' as the prefix. */"\` |
|    - |  359 | `	"$slash = strrpos($pattern,'/');"\` |
|    - |  360 | `	"if( $slash === false ){ $zDir = '.'; $prefix = ''; $pat = $pattern; }"\` |
|    - |  361 | `	"else { $zDir = substr($pattern,0,$slash); if( $zDir === '' ){ $zDir = '/'; } $prefix = substr($pattern,0,$slash+1); $pat = substr($pattern,$slash+1); }"\` |
|    - |  362 | `	"$pHandle = opendir($zDir);"\` |
|    - |  363 | `	"if( $pHandle == FALSE ){"\` |
|    - |  364 | `	"   /* IO error while opening the target directory,return FALSE */"\` |
|    - |  365 | `	"	return FALSE;"\` |
|    - |  366 | `	"}"\` |
|    - |  367 | `	"$pArray = array(); /* Empty array */"\` |
|    - |  368 | `	"/* Loop throw available entries */"\` |
|    - |  369 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|    - |  370 | `	" /* php's glob() never matches a leading-dot entry (incl. '.' and '..') unless"\` |
|    - |  371 | `	"    the pattern itself starts with a dot */"\` |
|    - |  372 | `	"	if( strlen($pEntry) > 0 && $pEntry[0] === '.' && (strlen($pat) < 1 \|\| $pat[0] !== '.') ){ continue; }"\` |
|    - |  373 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|    - |  374 | `	"	$rc = strglob($pat,$pEntry);"\` |
|    - |  375 | `	"	if( $rc ){"\` |
|    - |  376 | `	"	   $zFull = $prefix . $pEntry;"\` |
|    - |  377 | `	"	   if( is_dir($zDir . '/' . $pEntry) ){"\` |
|    - |  378 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|    - |  379 | `	"		     /* Adds a slash to each directory returned */"\` |
|    - |  380 | `	"			 $zFull .= DIRECTORY_SEPARATOR;"\` |
|    - |  381 | `	"		  }"\` |
|    - |  382 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|    - |  383 | `	"	     /* Not a directory,ignore */"\` |
|    - |  384 | `	"		 continue;"\` |
|    - |  385 | `	"	   }"\` |
|    - |  386 | `	"	   /* Add the entry (with its literal directory prefix, php-style) */"\` |
|    - |  387 | `	"	   $pArray[] = $zFull;"\` |
|    - |  388 | `	"	}"\` |
|    - |  389 | `	" }"\` |
|    - |  390 | `	"/* Close the handle */"\` |
|    - |  391 | `	"closedir($pHandle);"\` |
|    - |  392 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|    - |  393 | `	"  /* Sort the array */"\` |
|    - |  394 | `	"  sort($pArray);"\` |
|    - |  395 | `	"}"\` |
|    - |  396 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|    - |  397 | `	"  /* Return the search pattern if no files matching were found */"\` |
|    - |  398 | `	"  $pArray[] = $pattern;"\` |
|    - |  399 | `	"}"\` |
|    - |  400 | `	"/* Return the created array */"\` |
|    - |  401 | `	"return $pArray;"\` |
|    - |  402 | `   "}"\` |
|    - |  403 | `   "/* Creates a temporary file */"\` |
|    - |  404 | `   "function tmpfile(){"\` |
|    - |  405 | `   "  /* Extract the temp directory */"\` |
|    - |  406 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|    - |  407 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|    - |  408 | `   "    /* Use the current dir */"\` |
|    - |  409 | `   "    $zTempDir = '.';"\` |
|    - |  410 | `   "  }"\` |
|    - |  411 | `   "  /* Create the file */"\` |
|    - |  412 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|    - |  413 | `   "  return $pHandle;"\` |
|    - |  414 | `   "}"\` |
|    - |  415 | `   "/* php's number_format(): missing entirely from PH7. */"\` |
|    - |  416 | `   "function number_format($num, $decimals = 0, $dec_point = '.', $thousands_sep = ','){"\` |
|    - |  417 | `   "  $num = (float)$num;"\` |
|    - |  418 | `   "  $decimals = (int)$decimals;"\` |
|    - |  419 | `   "  if( $decimals < 0 ){ $decimals = 0; }"\` |
|    - |  420 | `   "  if( $dec_point === null ){ $dec_point = '.'; }"\` |
|    - |  421 | `   "  if( $thousands_sep === null ){ $thousands_sep = ','; }"\` |
|    - |  422 | `   "  /* round() first: sprintf uses banker's rounding, php's number_format rounds"\` |
|    - |  423 | `   "   * half AWAY FROM ZERO (number_format(0.5) is '1', not '0'). */"\` |
|    - |  424 | `   "  $num = round($num, $decimals);"\` |
|    - |  425 | `   "  $s = sprintf('%.' . $decimals . 'f', $num);"\` |
|    - |  426 | `   "  $neg = false;"\` |
|    - |  427 | `   "  if( substr($s, 0, 1) === '-' ){ $neg = true; $s = substr($s, 1); }"\` |
|    - |  428 | `   "  $parts = explode('.', $s);"\` |
|    - |  429 | `   "  $int = $parts[0];"\` |
|    - |  430 | `   "  $frac = count($parts) > 1 ? $parts[1] : '';"\` |
|    - |  431 | `   "  $out = '';"\` |
|    - |  432 | `   "  $len = strlen($int);"\` |
|    - |  433 | `   "  $c = 0;"\` |
|    - |  434 | `   "  for( $i = $len - 1 ; $i >= 0 ; $i-- ){"\` |
|    - |  435 | `   "    $out = $int[$i] . $out;"\` |
|    - |  436 | `   "    $c++;"\` |
|    - |  437 | `   "    if( $c % 3 === 0 && $i > 0 ){ $out = $thousands_sep . $out; }"\` |
|    - |  438 | `   "  }"\` |
|    - |  439 | `   "  if( $decimals > 0 ){ $out = $out . $dec_point . $frac; }"\` |
|    - |  440 | `   "  if( $neg ){ $out = '-' . $out; }"\` |
|    - |  441 | `   "  return $out;"\` |
|    - |  442 | `   "}"\` |
|    - |  443 | `   "function is_nan($v){ $v = (float)$v; return $v != $v; }"\` |
|    - |  444 | `   "function is_infinite($v){ $v = (float)$v; return $v == INF \|\| $v == -INF; }"\` |
|    - |  445 | `   "function is_finite($v){ $v = (float)$v; return !is_nan($v) && !is_infinite($v); }"\` |
|    - |  446 | `   "/* php's version_compare: canonicalise (separators + digit/alpha boundaries all"\` |
|    - |  447 | `   " * become '.'), then compare parts with the special dev<alpha<beta<RC<#<pl ordering. */"\` |
|    - |  448 | `   "function __phl_vcanon($v){"\` |
|    - |  449 | `   "  $v = (string)$v; $len = strlen($v); $out = '';"\` |
|    - |  450 | `   "  for( $i = 0; $i < $len; $i++ ){"\` |
|    - |  451 | `   "   $c = $v[$i]; $rp = $i + 1 < $len ? $v[$i + 1] : '';"\` |
|    - |  452 | `   "   $cd = ($c >= '0' && $c <= '9');"\` |
|    - |  453 | `   "   $ca = $cd \|\| ($c >= 'a' && $c <= 'z') \|\| ($c >= 'A' && $c <= 'Z');"\` |
|    - |  454 | `   "   if( !$ca ){"\` |
|    - |  455 | `   "    /* any non-alphanumeric (., -, _, +, ...) is a separator: emit one '.' */"\` |
|    - |  456 | `   "    if( $out !== '' && substr($out, -1) !== '.' ){ $out .= '.'; }"\` |
|    - |  457 | `   "   }else{"\` |
|    - |  458 | `   "    $out .= $c;"\` |
|    - |  459 | `   "    $rd = ($rp >= '0' && $rp <= '9');"\` |
|    - |  460 | `   "    $ra = $rd \|\| ($rp >= 'a' && $rp <= 'z') \|\| ($rp >= 'A' && $rp <= 'Z');"\` |
|    - |  461 | `   "    if( $rp !== '' && $ra && ($cd !== $rd) ){ $out .= '.'; }"\` |
|    - |  462 | `   "   }"\` |
|    - |  463 | `   "  }"\` |
|    - |  464 | `   "  return explode('.', $out);"\` |
|    - |  465 | `   "}"\` |
|    - |  466 | `   "function __phl_vform($s){"\` |
|    - |  467 | `   "  if( $s === '' ){ return -1; }"\` |
|    - |  468 | `   "  if( ctype_digit($s) ){ return 4; }"\` |
|    - |  469 | `   "  $f = array('dev' => 0, 'alpha' => 1, 'a' => 1, 'beta' => 2, 'b' => 2, 'RC' => 3, 'rc' => 3, 'pl' => 5, 'p' => 5);"\` |
|    - |  470 | `   "  foreach( $f as $name => $ord ){ if( strncmp($s, $name, strlen($name)) === 0 ){ return $ord; } }"\` |
|    - |  471 | `   "  return -1;"\` |
|    - |  472 | `   "}"\` |
|    - |  473 | `   "function version_compare($version1, $version2, $operator = null){"\` |
|    - |  474 | `   "  $v1 = __phl_vcanon($version1); $v2 = __phl_vcanon($version2);"\` |
|    - |  475 | `   "  $n1 = count($v1); $n2 = count($v2); $n = $n1 > $n2 ? $n1 : $n2; $cmp = 0;"\` |
|    - |  476 | `   "  for( $i = 0; $i < $n; $i++ ){"\` |
|    - |  477 | `   "   $a = $i < $n1 ? $v1[$i] : null; $b = $i < $n2 ? $v2[$i] : null;"\` |
|    - |  478 | `   "   if( $a === null ){ $cmp = ctype_digit($b) ? -1 : (4 <=> __phl_vform($b)); }"\` |
|    - |  479 | `   "   elseif( $b === null ){ $cmp = ctype_digit($a) ? 1 : (__phl_vform($a) <=> 4); }"\` |
|    - |  480 | `   "   elseif( ctype_digit($a) && ctype_digit($b) ){ $cmp = (int)$a <=> (int)$b; }"\` |
|    - |  481 | `   "   else{ $cmp = __phl_vform($a) <=> __phl_vform($b); }"\` |
|    - |  482 | `   "   if( $cmp !== 0 ){ break; }"\` |
|    - |  483 | `   "  }"\` |
|    - |  484 | `   "  if( $operator === null ){ return $cmp; }"\` |
|    - |  485 | `   "  switch( (string)$operator ){"\` |
|    - |  486 | `   "   case '<': case 'lt': return $cmp < 0;"\` |
|    - |  487 | `   "   case '<=': case 'le': return $cmp <= 0;"\` |
|    - |  488 | `   "   case '>': case 'gt': return $cmp > 0;"\` |
|    - |  489 | `   "   case '>=': case 'ge': return $cmp >= 0;"\` |
|    - |  490 | `   "   case '==': case '=': case 'eq': return $cmp === 0;"\` |
|    - |  491 | `   "   case '!=': case '<>': case 'ne': return $cmp !== 0;"\` |
|    - |  492 | `   "  }"\` |
|    - |  493 | `   "  return null;"\` |
|    - |  494 | `   "}"\` |
|    - |  495 | `   "/* phl.stub_extensions (a -d/php.ini list, comma-separated) declares extensions"\` |
|    - |  496 | `   " * PHL does not implement as LOADED, backed by no-op behaviour, so software that"\` |
|    - |  497 | `   " * only GATES on extension_loaded() (e.g. PHPUnit's dom/xmlwriter check) runs"\` |
|    - |  498 | `   " * unmodified. It does NOT synthesize the extension's classes/functions. */"\` |
|    - |  499 | `   "function __phl_stub_exts(){"\` |
|    - |  500 | `   "  $s = ini_get('phl.stub_extensions');"\` |
|    - |  501 | `   "  if( $s === false \|\| $s === '' ){ return array(); }"\` |
|    - |  502 | `   "  $out = array();"\` |
|    - |  503 | `   "  foreach( explode(',', (string)$s) as $e ){ $e = trim($e); if( $e !== '' ){ $out[strtolower($e)] = $e; } }"\` |
|    - |  504 | `   "  return $out;"\` |
|    - |  505 | `   "}"\` |
|    - |  506 | `   "function extension_loaded($name){"\` |
|    - |  507 | `   "  static $ext = array('core' => 1, 'standard' => 1, 'pcre' => 1, 'json' => 1,"\` |
|    - |  508 | `   "   'ctype' => 1, 'date' => 1, 'spl' => 1, 'reflection' => 1, 'mbstring' => 1,"\` |
|    - |  509 | `   "   'hash' => 1, 'filter' => 1, 'session' => 1" PHL_EXT_LOADED_LIBXML ");"\` |
|    - |  510 | `   "  $n = strtolower((string)$name);"\` |
|    - |  511 | `   "  if( isset($ext[$n]) ){ return true; }"\` |
|    - |  512 | `   "  $stub = __phl_stub_exts();"\` |
|    - |  513 | `   "  return isset($stub[$n]);"\` |
|    - |  514 | `   "}"\` |
|    - |  515 | `   "function get_loaded_extensions($zend_extensions = false){"\` |
|    - |  516 | `   "  if( $zend_extensions ){ return array(); }"\` |
|    - |  517 | `   "  $base = array('Core','date','pcre','SPL','json','standard',"\` |
|    - |  518 | `   "   'ctype','filter','hash','Reflection','session','mbstring'" PHL_EXT_LIST_LIBXML ");"\` |
|    - |  519 | `   "  foreach( __phl_stub_exts() as $e ){ $base[] = $e; }"\` |
|    - |  520 | `   "  return $base;"\` |
|    - |  521 | `   "}"\` |
|    - |  522 | `   "/* Inverse of bin2hex() */"\` |
|    - |  523 | `   "function hex2bin($str){"\` |
|    - |  524 | `   "  $str = (string)$str;"\` |
|    - |  525 | `   "  $len = strlen($str);"\` |
|    - |  526 | `   "  if( $len % 2 !== 0 ){"\` |
|    - |  527 | `   "    trigger_error('hex2bin(): Hexadecimal input string must have an even length', E_USER_WARNING);"\` |
|    - |  528 | `   "    return false;"\` |
|    - |  529 | `   "  }"\` |
|    - |  530 | `   "  $out = '';"\` |
|    - |  531 | `   "  for( $i = 0 ; $i < $len ; $i += 2 ){"\` |
|    - |  532 | `   "    $pair = substr($str, $i, 2);"\` |
|    - |  533 | `   "    if( !ctype_xdigit($pair) ){"\` |
|    - |  534 | `   "      trigger_error('hex2bin(): Input string must be hexadecimal string', E_USER_WARNING);"\` |
|    - |  535 | `   "      return false;"\` |
|    - |  536 | `   "    }"\` |
|    - |  537 | `   "    $out = $out . chr(hexdec($pair));"\` |
|    - |  538 | `   "  }"\` |
|    - |  539 | `   "  return $out;"\` |
|    - |  540 | `   "}"\` |
|    - |  541 | `   "/* Division that never throws: INF/-INF/NAN like php */"\` |
|    - |  542 | `   "function fdiv($a, $b){"\` |
|    - |  543 | `   "  $a = (float)$a;"\` |
|    - |  544 | `   "  $b = (float)$b;"\` |
|    - |  545 | `   "  if( $b == 0.0 ){"\` |
|    - |  546 | `   "    if( $a == 0.0 \|\| is_nan($a) ){ return NAN; }"\` |
|    - |  547 | `   "    return $a > 0 ? INF : -INF;"\` |
|    - |  548 | `   "  }"\` |
|    - |  549 | `   "  return $a / $b;"\` |
|    - |  550 | `   "}"\` |
|    - |  551 | `   "function checkdate($month, $day, $year){"\` |
|    - |  552 | `   "  $month = (int)$month; $day = (int)$day; $year = (int)$year;"\` |
|    - |  553 | `   "  if( $month < 1 \|\| $month > 12 \|\| $year < 1 \|\| $year > 32767 \|\| $day < 1 ){ return false; }"\` |
|    - |  554 | `   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\` |
|    - |  555 | `   "  $max = $days[$month - 1];"\` |
|    - |  556 | `   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) \|\| ($year % 400 === 0)) ){"\` |
|    - |  557 | `   "    $max = 29;"\` |
|    - |  558 | `   "  }"\` |
|    - |  559 | `   "  return $day <= $max;"\` |
|    - |  560 | `   "}"\` |
|    - |  561 | `   "function is_iterable($v){ return is_array($v) \|\| ($v instanceof Traversable); }"\` |
|    - |  562 | `   "function is_countable($v){ return is_array($v) \|\| ($v instanceof Countable); }"\` |
|    - |  563 | `   "function key_exists($key, $array){ return array_key_exists($key, $array); }"\` |
|    - |  564 | `   "function doubleval($v){ return (float)$v; }"\` |
|    - |  565 | `   "function array_count_values($array){"\` |
|    - |  566 | `   "  $out = array();"\` |
|    - |  567 | `   "  foreach( $array as $v ){"\` |
|    - |  568 | `   "    if( !is_int($v) && !is_string($v) ){"\` |
|    - |  569 | `   "      trigger_error('array_count_values(): Can only count string and integer values, entry skipped', E_USER_WARNING);"\` |
|    - |  570 | `   "      continue;"\` |
|    - |  571 | `   "    }"\` |
|    - |  572 | `   "    if( isset($out[$v]) ){ $out[$v] = $out[$v] + 1; } else { $out[$v] = 1; }"\` |
|    - |  573 | `   "  }"\` |
|    - |  574 | `   "  return $out;"\` |
|    - |  575 | `   "}"\` |
|    - |  576 | `   "function array_change_key_case($array, $case = CASE_LOWER){"\` |
|    - |  577 | `   "  $out = array();"\` |
|    - |  578 | `   "  foreach( $array as $k => $v ){"\` |
|    - |  579 | `   "    if( is_string($k) ){ $k = ($case == CASE_UPPER) ? strtoupper($k) : strtolower($k); }"\` |
|    - |  580 | `   "    $out[$k] = $v;"\` |
|    - |  581 | `   "  }"\` |
|    - |  582 | `   "  return $out;"\` |
|    - |  583 | `   "}"\` |
|    - |  584 | `   "function array_replace_recursive($array, ...$others){"\` |
|    - |  585 | `   "  foreach( $others as $o ){"\` |
|    - |  586 | `   "    foreach( $o as $k => $v ){"\` |
|    - |  587 | `   "      if( is_array($v) && isset($array[$k]) && is_array($array[$k]) ){"\` |
|    - |  588 | `   "        $array[$k] = array_replace_recursive($array[$k], $v);"\` |
|    - |  589 | `   "      }else{"\` |
|    - |  590 | `   "        $array[$k] = $v;"\` |
|    - |  591 | `   "      }"\` |
|    - |  592 | `   "    }"\` |
|    - |  593 | `   "  }"\` |
|    - |  594 | `   "  return $array;"\` |
|    - |  595 | `   "}"\` |
|    - |  596 | `   "function class_uses($what, $autoload = true){"\` |
|    - |  597 | `   "  $c = is_object($what) ? get_class($what) : (string)$what;"\` |
|    - |  598 | `   "  if( !class_exists($c) ){ return false; }"\` |
|    - |  599 | `   "  return array();  /* PHL has no traits yet -- always the empty set */"\` |
|    - |  600 | `   "}"\` |
|    - |  601 | `   "function count_chars($str, $mode = 0){"\` |
|    - |  602 | `   "  $str = (string)$str;"\` |
|    - |  603 | `   "  $counts = array();"\` |
|    - |  604 | `   "  for( $i = 0 ; $i < 256 ; $i++ ){ $counts[$i] = 0; }"\` |
|    - |  605 | `   "  $len = strlen($str);"\` |
|    - |  606 | `   "  for( $i = 0 ; $i < $len ; $i++ ){ $b = ord($str[$i]); $counts[$b] = $counts[$b] + 1; }"\` |
|    - |  607 | `   "  if( $mode == 1 ){"\` |
|    - |  608 | `   "    $out = array();"\` |
|    - |  609 | `   "    foreach( $counts as $b => $n ){ if( $n > 0 ){ $out[$b] = $n; } }"\` |
|    - |  610 | `   "    return $out;"\` |
|    - |  611 | `   "  }"\` |
|    - |  612 | `   "  if( $mode == 3 ){"\` |
|    - |  613 | `   "    $out = '';"\` |
|    - |  614 | `   "    foreach( $counts as $b => $n ){ if( $n > 0 ){ $out = $out . chr($b); } }"\` |
|    - |  615 | `   "    return $out;"\` |
|    - |  616 | `   "  }"\` |
|    - |  617 | `   "  return $counts;"\` |
|    - |  618 | `   "}"\` |
|    - |  619 | `   "function ip2long($ip){"\` |
|    - |  620 | `   "  $p = explode('.', (string)$ip);"\` |
|    - |  621 | `   "  if( count($p) !== 4 ){ return false; }"\` |
|    - |  622 | `   "  $n = 0;"\` |
|    - |  623 | `   "  foreach( $p as $o ){"\` |
|    - |  624 | `   "    if( !ctype_digit($o) \|\| (int)$o < 0 \|\| (int)$o > 255 ){ return false; }"\` |
|    - |  625 | `   "    $n = $n * 256 + (int)$o;"\` |
|    - |  626 | `   "  }"\` |
|    - |  627 | `   "  return $n;"\` |
|    - |  628 | `   "}"\` |
|    - |  629 | `   "function long2ip($n){"\` |
|    - |  630 | `   "  $n = (int)$n;"\` |
|    - |  631 | `   "  return (($n >> 24) & 255) . '.' . (($n >> 16) & 255) . '.' . (($n >> 8) & 255) . '.' . ($n & 255);"\` |
|    - |  632 | `   "}"\` |
|    - |  633 | `   "function preg_filter($pattern, $replacement, $subject, $limit = -1){"\` |
|    - |  634 | `   "  if( is_array($subject) ){"\` |
|    - |  635 | `   "    $out = array();"\` |
|    - |  636 | `   "    foreach( $subject as $k => $v ){"\` |
|    - |  637 | `   "      $r = preg_replace($pattern, $replacement, (string)$v, $limit, $cnt);"\` |
|    - |  638 | `   "      if( $cnt > 0 ){ $out[$k] = $r; }"\` |
|    - |  639 | `   "    }"\` |
|    - |  640 | `   "    return $out;"\` |
|    - |  641 | `   "  }"\` |
|    - |  642 | `   "  $r = preg_replace($pattern, $replacement, (string)$subject, $limit, $cnt);"\` |
|    - |  643 | `   "  return $cnt > 0 ? $r : null;"\` |
|    - |  644 | `   "}"\` |
|    - |  645 | `   "function preg_replace_callback_array($patterns, $subject, $limit = -1){"\` |
|    - |  646 | `   "  foreach( $patterns as $pat => $cb ){"\` |
|    - |  647 | `   "    $subject = preg_replace_callback($pat, $cb, $subject, $limit);"\` |
|    - |  648 | `   "  }"\` |
|    - |  649 | `   "  return $subject;"\` |
|    - |  650 | `   "}"\` |
|    - |  651 | `   "function cal_days_in_month($calendar, $month, $year){"\` |
|    - |  652 | `   "  $month = (int)$month; $year = (int)$year;"\` |
|    - |  653 | `   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\` |
|    - |  654 | `   "  if( $month < 1 \|\| $month > 12 ){"\` |
|    - |  655 | `   "    throw new ValueError('cal_days_in_month(): Argument #2 ($month) must be a valid month');"\` |
|    - |  656 | `   "  }"\` |
|    - |  657 | `   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) \|\| ($year % 400 === 0)) ){"\` |
|    - |  658 | `   "    return 29;"\` |
|    - |  659 | `   "  }"\` |
|    - |  660 | `   "  return $days[$month - 1];"\` |
|    - |  661 | `   "}"\` |
|    - |  662 | `   "function preg_grep($pattern, $array, $flags = 0){"\` |
|    - |  663 | `   "  $out = array();"\` |
|    - |  664 | `   "  foreach( $array as $k => $v ){"\` |
|    - |  665 | `   "    $m = preg_match($pattern, (string)$v);"\` |
|    - |  666 | `   "    if( $flags & PREG_GREP_INVERT ){ $m = !$m; }"\` |
|    - |  667 | `   "    if( $m ){ $out[$k] = $v; }"\` |
|    - |  668 | `   "  }"\` |
|    - |  669 | `   "  return $out;"\` |
|    - |  670 | `   "}"\` |
|    - |  671 | `   "function class_implements($what, $autoload = true){"\` |
|    - |  672 | `   "  $c = is_object($what) ? get_class($what) : (string)$what;"\` |
|    - |  673 | `   "  if( !class_exists($c) && !interface_exists($c) ){ return false; }"\` |
|    - |  674 | `   "  $out = array();"\` |
|    - |  675 | `   "  $r = new ReflectionClass($c);"\` |
|    - |  676 | `   "  foreach( $r->getInterfaceNames() as $i ){ $out[$i] = $i; }"\` |
|    - |  677 | `   "  return $out;"\` |
|    - |  678 | `   "}"\` |
|    - |  679 | `   "function class_parents($what, $autoload = true){"\` |
|    - |  680 | `   "  $c = is_object($what) ? get_class($what) : (string)$what;"\` |
|    - |  681 | `   "  if( !class_exists($c) ){ return false; }"\` |
|    - |  682 | `   "  $out = array();"\` |
|    - |  683 | `   "  $r = new ReflectionClass($c);"\` |
|    - |  684 | `   "  while( ($p = $r->getParentClass()) ){"\` |
|    - |  685 | `   "    $n = $p->getName();"\` |
|    - |  686 | `   "    $out[$n] = $n;"\` |
|    - |  687 | `   "    $r = $p;"\` |
|    - |  688 | `   "  }"\` |
|    - |  689 | `   "  return $out;"\` |
|    - |  690 | `   "}"\` |
|    - |  691 | `   "/* php's http_build_query() -- missing from PH7. Skips null values, casts"\` |
|    - |  692 | `   " * bool to 1/0, prefixes numeric top-level keys, urlencodes per RFC. */"\` |
|    - |  693 | `   "function __phl_hbq_enc($s, $enc){"\` |
|    - |  694 | `   "  return $enc == PHP_QUERY_RFC3986 ? rawurlencode((string)$s) : urlencode((string)$s);"\` |
|    - |  695 | `   "}"\` |
|    - |  696 | `   "function __phl_hbq(&$pairs, $data, $key_prefix, $numeric_prefix, $sep, $enc){"\` |
|    - |  697 | `   "  foreach( $data as $k => $v ){"\` |
|    - |  698 | `   "    if( $v === null ){ continue; }"\` |
|    - |  699 | `   "    if( $key_prefix === '' ){"\` |
|    - |  700 | `   "      $ek = is_int($k) ? __phl_hbq_enc($numeric_prefix . $k, $enc) : __phl_hbq_enc($k, $enc);"\` |
|    - |  701 | `   "    } else {"\` |
|    - |  702 | `   "      $ek = $key_prefix . '%5B' . __phl_hbq_enc($k, $enc) . '%5D';"\` |
|    - |  703 | `   "    }"\` |
|    - |  704 | `   "    if( is_array($v) ){"\` |
|    - |  705 | `   "      __phl_hbq($pairs, $v, $ek, $numeric_prefix, $sep, $enc);"\` |
|    - |  706 | `   "    } elseif( is_object($v) ){"\` |
|    - |  707 | `   "      __phl_hbq($pairs, get_object_vars($v), $ek, $numeric_prefix, $sep, $enc);"\` |
|    - |  708 | `   "    } else {"\` |
|    - |  709 | `   "      if( $v === true ){ $v = '1'; } elseif( $v === false ){ $v = '0'; }"\` |
|    - |  710 | `   "      $pairs[] = $ek . '=' . __phl_hbq_enc($v, $enc);"\` |
|    - |  711 | `   "    }"\` |
|    - |  712 | `   "  }"\` |
|    - |  713 | `   "}"\` |
|    - |  714 | `   "function http_build_query($data, $numeric_prefix = '', $arg_separator = null, $encoding_type = PHP_QUERY_RFC1738){"\` |
|    - |  715 | `   "  if( !is_array($data) && !is_object($data) ){"\` |
|    - |  716 | `   "    throw new TypeError('http_build_query(): Argument #1 ($data) must be of type array\|object, ' . gettype($data) . ' given');"\` |
|    - |  717 | `   "  }"\` |
|    - |  718 | `   "  if( $arg_separator === null ){ $arg_separator = '&'; }"\` |
|    - |  719 | `   "  $pairs = array();"\` |
|    - |  720 | `   "  __phl_hbq($pairs, is_object($data) ? get_object_vars($data) : $data, '', (string)$numeric_prefix, $arg_separator, $encoding_type);"\` |
|    - |  721 | `   "  return implode($arg_separator, $pairs);"\` |
|    - |  722 | `   "}"\` |
|    - |  723 | `   "/* php's parse_str() -- missing from PH7. Mangles the base name ('.'/' ' -> '_'),"\` |
|    - |  724 | `   " * parses [key] nesting and [] appends, urldecodes keys and values. */"\` |
|    - |  725 | `   "function __phl_parsestr_assign(&$arr, $segments, $i, $val){"\` |
|    - |  726 | `   "  $seg = $segments[$i];"\` |
|    - |  727 | `   "  $last = ($i === count($segments) - 1);"\` |
|    - |  728 | `   "  if( $seg === '' ){"\` |
|    - |  729 | `   "    if( $last ){ $arr[] = $val; return; }"\` |
|    - |  730 | `   "    $arr[] = array();"\` |
|    - |  731 | `   "    $k = array_key_last($arr);"\` |
|    - |  732 | `   "    __phl_parsestr_assign($arr[$k], $segments, $i + 1, $val);"\` |
|    - |  733 | `   "  } else {"\` |
|    - |  734 | `   "    if( $last ){ $arr[$seg] = $val; return; }"\` |
|    - |  735 | `   "    if( !isset($arr[$seg]) \|\| !is_array($arr[$seg]) ){ $arr[$seg] = array(); }"\` |
|    - |  736 | `   "    __phl_parsestr_assign($arr[$seg], $segments, $i + 1, $val);"\` |
|    - |  737 | `   "  }"\` |
|    - |  738 | `   "}"\` |
|    - |  739 | `   "function parse_str($string, &$result){"\` |
|    - |  740 | `   "  $result = array();"\` |
|    - |  741 | `   "  $string = (string)$string;"\` |
|    - |  742 | `   "  if( $string === '' ){ return; }"\` |
|    - |  743 | `   "  foreach( explode('&', $string) as $pair ){"\` |
|    - |  744 | `   "    if( $pair === '' ){ continue; }"\` |
|    - |  745 | `   "    $eq = strpos($pair, '=');"\` |
|    - |  746 | `   "    if( $eq === false ){ $rawkey = $pair; $val = ''; }"\` |
|    - |  747 | `   "    else { $rawkey = substr($pair, 0, $eq); $val = urldecode(substr($pair, $eq + 1)); }"\` |
|    - |  748 | `   "    if( $rawkey === '' ){ continue; }"\` |
|    - |  749 | `   "    $bpos = strpos($rawkey, '[');"\` |
|    - |  750 | `   "    if( $bpos === false ){ $base = $rawkey; $subs = array(); }"\` |
|    - |  751 | `   "    else {"\` |
|    - |  752 | `   "      $base = substr($rawkey, 0, $bpos);"\` |
|    - |  753 | `   "      preg_match_all('/\\[([^\\]]*)\\]/', substr($rawkey, $bpos), $m);"\` |
|    - |  754 | `   "      $subs = $m[1];"\` |
|    - |  755 | `   "    }"\` |
|    - |  756 | `   "    $base = str_replace(array(' ', '.'), '_', urldecode($base));"\` |
|    - |  757 | `   "    if( $base === '' ){ continue; }"\` |
|    - |  758 | `   "    $segs = array($base);"\` |
|    - |  759 | `   "    foreach( $subs as $s ){ $segs[] = urldecode($s); }"\` |
|    - |  760 | `   "    __phl_parsestr_assign($result, $segs, 0, $val);"\` |
|    - |  761 | `   "  }"\` |
|    - |  762 | `   "}"\` |
|    - |  763 | `   "/* php 8.3 str_increment(): Perl-style alphanumeric increment. */"\` |
|    - |  764 | `   "function str_increment($string){"\` |
|    - |  765 | `   "  $string = (string)$string;"\` |
|    - |  766 | `   "  if( $string === '' ){ throw new ValueError('str_increment(): Argument #1 ($string) must not be empty'); }"\` |
|    - |  767 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_increment(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|    - |  768 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|    - |  769 | `   "    $c = $string[$i];"\` |
|    - |  770 | `   "    if( $c === 'z' ){ $string[$i] = 'a'; }"\` |
|    - |  771 | `   "    elseif( $c === 'Z' ){ $string[$i] = 'A'; }"\` |
|    - |  772 | `   "    elseif( $c === '9' ){ $string[$i] = '0'; }"\` |
|    - |  773 | `   "    else { $string[$i] = chr(ord($c) + 1); return $string; }"\` |
|    - |  774 | `   "  }"\` |
|    - |  775 | `   "  $first = $string[0];"\` |
|    - |  776 | `   "  if( $first === '0' ){ return '1' . $string; }"\` |
|    - |  777 | `   "  if( $first === 'a' ){ return 'a' . $string; }"\` |
|    - |  778 | `   "  return 'A' . $string;"\` |
|    - |  779 | `   "}"\` |
|    - |  780 | `   "/* php 8.3 str_decrement(): inverse of str_increment(); throws out of range"\` |
|    - |  781 | `   " * at the bottom of the counting sequence. */"\` |
|    - |  782 | `   "function str_decrement($string){"\` |
|    - |  783 | `   "  $string = (string)$string;"\` |
|    - |  784 | `   "  if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) must not be empty'); }"\` |
|    - |  785 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_decrement(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|    - |  786 | `   "  $orig = $string;"\` |
|    - |  787 | `   "  $borrowed = false;"\` |
|    - |  788 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|    - |  789 | `   "    $c = $string[$i];"\` |
|    - |  790 | `   "    if( $c === 'a' ){ $string[$i] = 'z'; }"\` |
|    - |  791 | `   "    elseif( $c === 'A' ){ $string[$i] = 'Z'; }"\` |
|    - |  792 | `   "    elseif( $c === '0' ){ $string[$i] = '9'; }"\` |
|    - |  793 | `   "    else { $string[$i] = chr(ord($c) - 1); $borrowed = false; break; }"\` |
|    - |  794 | `   "    if( $i === 0 ){ $borrowed = true; }"\` |
|    - |  795 | `   "  }"\` |
|    - |  796 | `   "  if( $borrowed ){"\` |
|    - |  797 | `   "    if( $string[0] === '9' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|    - |  798 | `   "    $string = substr($string, 1);"\` |
|    - |  799 | `   "    if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|    - |  800 | `   "  } elseif( strlen($string) > 1 && $string[0] === '0' ){"\` |
|    - |  801 | `   "    $string = substr($string, 1);"\` |
|    - |  802 | `   "  }"\` |
|    - |  803 | `   "  return $string;"\` |
|    - |  804 | `   "}"\` |
|    - |  805 | `   "/* Permission bits via stat(); false + warning when stat fails, like php. */"\` |
|    - |  806 | `   "function fileperms($filename){"\` |
|    - |  807 | `   "  $s = @stat($filename);"\` |
|    - |  808 | `   "  if( $s === false ){"\` |
|    - |  809 | `   "    trigger_error('fileperms(): stat failed for ' . $filename, E_USER_WARNING);"\` |
|    - |  810 | `   "    return false;"\` |
|    - |  811 | `   "  }"\` |
|    - |  812 | `   "  return $s['mode'];"\` |
|    - |  813 | `   "}"\` |
|    - |  814 | `   "/* PH7 keeps no stat cache, so this is a no-op like php on a clean cache. */"\` |
|    - |  815 | `   "function clearstatcache($clear_realpath_cache = false, $filename = ''){}"\` |
|    - |  816 | `   "/* php 8.4 mb_ucfirst/mb_lcfirst: case-map only the first multibyte char. */"\` |
|    - |  817 | `   "function mb_ucfirst($string, $encoding = null){"\` |
|    - |  818 | `   "  $string = (string)$string;"\` |
|    - |  819 | `   "  if( $string === '' ){ return ''; }"\` |
|    - |  820 | `   "  return mb_strtoupper(mb_substr($string, 0, 1)) . mb_substr($string, 1);"\` |
|    - |  821 | `   "}"\` |
|    - |  822 | `   "function mb_lcfirst($string, $encoding = null){"\` |
|    - |  823 | `   "  $string = (string)$string;"\` |
|    - |  824 | `   "  if( $string === '' ){ return ''; }"\` |
|    - |  825 | `   "  return mb_strtolower(mb_substr($string, 0, 1)) . mb_substr($string, 1);"\` |
|    - |  826 | `   "}"\` |
|    - |  827 | `   "/* php 8.4 mb_trim family: strip leading/trailing characters (whole"\` |
|    - |  828 | `   " * multibyte chars, NO range syntax), defaulting to php's Unicode"\` |
|    - |  829 | `   " * whitespace set. */"\` |
|    - |  830 | `   "function __phl_mb_ws(){"\` |
|    - |  831 | `   "  static $set = null;"\` |
|    - |  832 | `   "  if( $set === null ){"\` |
|    - |  833 | `   "    $set = array();"\` |
|    - |  834 | `   "    foreach( array(0x00,0x09,0x0A,0x0B,0x0C,0x0D,0x20,0x85,0xA0,0x1680,"\` |
|    - |  835 | `   "      0x180E,0x2000,0x2001,0x2002,0x2003,0x2004,0x2005,0x2006,0x2007,0x2008,"\` |
|    - |  836 | `   "      0x2009,0x200A,0x2028,0x2029,0x202F,0x205F,0x3000) as $cp ){"\` |
|    - |  837 | `   "      $set[mb_chr($cp)] = true;"\` |
|    - |  838 | `   "    }"\` |
|    - |  839 | `   "  }"\` |
|    - |  840 | `   "  return $set;"\` |
|    - |  841 | `   "}"\` |
|    - |  842 | `   "function __phl_mb_trim($string, $characters, $left, $right){"\` |
|    - |  843 | `   "  $string = (string)$string;"\` |
|    - |  844 | `   "  if( $string === '' ){ return ''; }"\` |
|    - |  845 | `   "  if( $characters === null ){"\` |
|    - |  846 | `   "    $set = __phl_mb_ws();"\` |
|    - |  847 | `   "  } else {"\` |
|    - |  848 | `   "    $set = array();"\` |
|    - |  849 | `   "    foreach( mb_str_split((string)$characters) as $c ){ $set[$c] = true; }"\` |
|    - |  850 | `   "  }"\` |
|    - |  851 | `   "  $chars = mb_str_split($string);"\` |
|    - |  852 | `   "  $n = count($chars);"\` |
|    - |  853 | `   "  $i = 0; $j = $n;"\` |
|    - |  854 | `   "  if( $left ){ while( $i < $j && isset($set[$chars[$i]]) ){ $i++; } }"\` |
|    - |  855 | `   "  if( $right ){ while( $j > $i && isset($set[$chars[$j - 1]]) ){ $j--; } }"\` |
|    - |  856 | `   "  return implode('', array_slice($chars, $i, $j - $i));"\` |
|    - |  857 | `   "}"\` |
|    - |  858 | `   "function mb_trim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, true, true); }"\` |
|    - |  859 | `   "function mb_ltrim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, true, false); }"\` |
|    - |  860 | `   "function mb_rtrim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, false, true); }"\` |
|    - |  861 | `   "/* Creates a temporary file and returns its name */"\` |
|    - |  862 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|    - |  863 | `   "{"\` |
|    - |  864 | `   "   /* php CREATES the file (empty, mode 0600) and guarantees the name is unique --"\` |
|    - |  865 | `   "    * returning a bare name left the caller with a path that does not exist, so"\` |
|    - |  866 | `   "    * file_exists() was false and unlink() failed on it. */"\` |
|    - |  867 | `   "   $zDir = rtrim($zDir, DIRECTORY_SEPARATOR);"\` |
|    - |  868 | `   "   for( $i = 0 ; $i < 64 ; ++$i ){"\` |
|    - |  869 | `   "     $zPath = $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|    - |  870 | `   "     if( file_exists($zPath) ){ continue; }"\` |
|    - |  871 | `   "     $pHandle = @fopen($zPath,'x');"\` |
|    - |  872 | `   "     if( $pHandle === false ){ continue; }"\` |
|    - |  873 | `   "     fclose($pHandle);"\` |
|    - |  874 | `   "     @chmod($zPath, 0600);"\` |
|    - |  875 | `   "     return $zPath;"\` |
|    - |  876 | `   "   }"\` |
|    - |  877 | `   "   return false;"\` |
|    - |  878 | `   "}"\` |
|    - |  879 | `   "function array_unshift(&$pArray ){"\` |
|    - |  880 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|    - |  881 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|    - |  882 | `   "/* Copy arguments */"\` |
|    - |  883 | `   "$nArgs = func_num_args();"\` |
|    - |  884 | `   "$pNew = array();"\` |
|    - |  885 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|    - |  886 | `    " $pNew[] = func_get_arg($i);"\` |
|    - |  887 | `    "}"\` |
|    - |  888 | `   	"/* Make a copy of the old entries */"\` |
|    - |  889 | `	"$pOld = array_copy($pArray);"\` |
|    - |  890 | `	"/* Erase */"\` |
|    - |  891 | `	"array_erase($pArray);"\` |
|    - |  892 | `	"/* Unshift */"\` |
|    - |  893 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|    - |  894 | `	"return sizeof($pArray);"\` |
|    - |  895 | `    "}"\` |
|    - |  896 | `	"function array_merge_recursive(){"\` |
|    - |  897 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|    - |  898 | `    "$arrays = func_get_args();"\` |
|    - |  899 | `    "$narrays = count($arrays);"\` |
|    - |  900 | `    "$ret = array();"\` |
|    - |  901 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|    - |  902 | `	 " if( !is_array($arrays[$i]) ){"\` |
|    - |  903 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|    - |  904 | `	 " }"\` |
|    - |  905 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|    - |  906 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|    - |  907 | `     "  if( $keyIsInt ) {"\` |
|    - |  908 | `     "   $ret[] = $value;"\` |
|    - |  909 | `     "  } else {"\` |
|    - |  910 | `     "   if (array_key_exists($key, $ret)) {"\` |
|    - |  911 | `     "    $cur = $ret[$key];"\` |
|    - |  912 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|    - |  913 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|    - |  914 | `     "    } elseif (is_array($cur)) {"\` |
|    - |  915 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|    - |  916 | `     "    } elseif (is_array($value)) {"\` |
|    - |  917 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|    - |  918 | `     "    } else {"\` |
|    - |  919 | `     "     $ret[$key] = array($cur, $value);"\` |
|    - |  920 | `     "    }"\` |
|    - |  921 | `     "   } else {"\` |
|    - |  922 | `     "    $ret[$key] = $value;"\` |
|    - |  923 | `     "   }"\` |
|    - |  924 | `     "  }"\` |
|    - |  925 | `     " }"\` |
|    - |  926 | `	 " }"\` |
|    - |  927 | `	 " return $ret;"\` |
|    - |  928 | `    "}"\` |
|    - |  929 | `	/* __php_zpp_type: php's ZPP value-name for TypeError messages */\` |
|    - |  930 | `	"function __php_zpp_type($v){"\` |
|    - |  931 | `	" if( is_object($v) ){ return get_class($v); }"\` |
|    - |  932 | `	" if( is_int($v) ){ return 'int'; }"\` |
|    - |  933 | `	" if( is_float($v) ){ return 'float'; }"\` |
|    - |  934 | `	" if( is_string($v) ){ return 'string'; }"\` |
|    - |  935 | `	" if( is_bool($v) ){ return $v ? 'true' : 'false'; }"\` |
|    - |  936 | `	" if( is_null($v) ){ return 'null'; }"\` |
|    - |  937 | `	" if( is_array($v) ){ return 'array'; }"\` |
|    - |  938 | `	" if( is_resource($v) ){ return 'resource'; }"\` |
|    - |  939 | `	" return 'mixed';"\` |
|    - |  940 | `	"}"\` |
|    - |  941 | `	"function max(){"\` |
|    - |  942 | `    "  $pArgs = func_get_args();"\` |
|    - |  943 | `    " if( sizeof($pArgs) < 1 ){"\` |
|    - |  944 | `	"  throw new ArgumentCountError('max() expects at least 1 argument, 0 given');"\` |
|    - |  945 | `    " }"\` |
|    - |  946 | `    " if( sizeof($pArgs) < 2 ){"\` |
|    - |  947 | `    " $pArg = $pArgs[0];"\` |
|    - |  948 | `	" if( !is_array($pArg) ){"\` |
|    - |  949 | `	"   throw new TypeError('max(): Argument #1 ($value) must be of type array, ' . __php_zpp_type($pArg) . ' given');"\` |
|    - |  950 | `	" }"\` |
|    - |  951 | `	" if( sizeof($pArg) < 1 ){"\` |
|    - |  952 | `	"   throw new ValueError('max(): Argument #1 ($value) must contain at least one element');"\` |
|    - |  953 | `	" }"\` |
|    - |  954 | `	" $max = null; $first = true;"\` |
|    - |  955 | `	" foreach( $pArgs[0] as $val ){"\` |
|    - |  956 | `	"   if( $first ){ $max = $val; $first = false; }"\` |
|    - |  957 | `	"   else if( $val > $max ){ $max = $val; }"\` |
|    - |  958 | `	" }"\` |
|    - |  959 | `	" return $max;"\` |
|    - |  960 | `    " }"\` |
|    - |  961 | `    " $max = $pArgs[0];"\` |
|    - |  962 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|    - |  963 | `    " $val = $pArgs[$i];"\` |
|    - |  964 | `	"if( $val > $max ){"\` |
|    - |  965 | `	" $max = $val;"\` |
|    - |  966 | `	"}"\` |
|    - |  967 | `    " }"\` |
|    - |  968 | `	" return $max;"\` |
|    - |  969 | `    "}"\` |
|    - |  970 | `	"function min(){"\` |
|    - |  971 | `    "  $pArgs = func_get_args();"\` |
|    - |  972 | `    " if( sizeof($pArgs) < 1 ){"\` |
|    - |  973 | `	"  throw new ArgumentCountError('min() expects at least 1 argument, 0 given');"\` |
|    - |  974 | `    " }"\` |
|    - |  975 | `    " if( sizeof($pArgs) < 2 ){"\` |
|    - |  976 | `    " $pArg = $pArgs[0];"\` |
|    - |  977 | `	" if( !is_array($pArg) ){"\` |
|    - |  978 | `	"   throw new TypeError('min(): Argument #1 ($value) must be of type array, ' . __php_zpp_type($pArg) . ' given');"\` |
|    - |  979 | `	" }"\` |
|    - |  980 | `	" if( sizeof($pArg) < 1 ){"\` |
|    - |  981 | `	"   throw new ValueError('min(): Argument #1 ($value) must contain at least one element');"\` |
|    - |  982 | `	" }"\` |
|    - |  983 | `	" $min = null; $first = true;"\` |
|    - |  984 | `	" foreach( $pArgs[0] as $val ){"\` |
|    - |  985 | `	"   if( $first ){ $min = $val; $first = false; }"\` |
|    - |  986 | `	"   else if( $val < $min ){ $min = $val; }"\` |
|    - |  987 | `	" }"\` |
|    - |  988 | `	" return $min;"\` |
|    - |  989 | `    " }"\` |
|    - |  990 | `    " $min = $pArgs[0];"\` |
|    - |  991 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|    - |  992 | `    " $val = $pArgs[$i];"\` |
|    - |  993 | `	"if( $val < $min ){"\` |
|    - |  994 | `	" $min = $val;"\` |
|    - |  995 | `	" }"\` |
|    - |  996 | `    " }"\` |
|    - |  997 | `	" return $min;"\` |
|    - |  998 | `	"}"\` |
|    - |  999 | `	"function fileowner(string $file){"\` |
|    - | 1000 | `    " $a = stat($file);"\` |
|    - | 1001 | `	" if( !is_array($a) ){"\` |
|    - | 1002 | `	"	return false;"\` |
|    - | 1003 | `	" }"\` |
|    - | 1004 | `	" return $a['uid'];"\` |
|    - | 1005 | `    "}"\` |
|    - | 1006 | `    "function filegroup(string $file){"\` |
|    - | 1007 | `	" $a = stat($file);"\` |
|    - | 1008 | `	" if( !is_array($a) ){"\` |
|    - | 1009 | `	"	return false;"\` |
|    - | 1010 | `	" }"\` |
|    - | 1011 | `	" return $a['gid'];"\` |
|    - | 1012 | `    "}"\` |
|    - | 1013 | `	 "function fileinode(string $file){"\` |
|    - | 1014 | `	" $a = stat($file);"\` |
|    - | 1015 | `	" if( !is_array($a) ){"\` |
|    - | 1016 | `	"	return false;"\` |
|    - | 1017 | `	" }"\` |
|    - | 1018 | `	" return $a['ino'];"\` |
|    - | 1019 | `    "}"` |
|    - | 1020 |  |
| 3874 | 1021 | `PH7_PRIVATE sxi32 PH7_VmInstallBuiltinLib(ph7_vm *pVm)` |
|    5 | 1022 | `{` |
|    - | 1023 | `	SyString sBuiltin;` |
|    - | 1024 | `	SyString sRandom;` |
| 3879 | 1025 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|    - | 1026 | `	/* Compile the built-in library */` |
| 3879 | 1027 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|    - | 1028 | `	/* Register the Random\RandomException namespaced class (PHP 8.2+).` |
|    - | 1029 | `	 * Kept in its own VmEvalChunk (not appended to PH7_BUILTIN_LIB): a namespace` |
|    - | 1030 | `	 * declaration is NOT reset at the block's closing brace in this engine, so` |
|    - | 1031 | `	 * anything following it in the same chunk would leak into the Random` |
|    - | 1032 | `	 * namespace. Isolation instead comes from VmEvalChunk saving/restoring` |
|    - | 1033 | `	 * pVm->sNamespace (and PH7_ResetCodeGenerator clearing the compiler` |
|    - | 1034 | `	 * namespace) per chunk, so this lands as Random\RandomException while later` |
|    - | 1035 | `	 * user code still compiles in the global namespace. */` |
|    - | 1036 | `	{` |
|    - | 1037 | `		static const char zRandomLib[] =` |
|    - | 1038 | `			"namespace Random { class RandomException extends \\Exception { } }";` |
| 3879 | 1039 | `		SyStringInitFromBuf(&sRandom,zRandomLib,sizeof(zRandomLib)-1);` |
| 3879 | 1040 | `		VmEvalChunk(&(*pVm),0,&sRandom,PH7_PHP_ONLY,FALSE);` |
|    - | 1041 | `	}` |
| 3879 | 1042 | `	return SXRET_OK;` |
|    5 | 1043 | `}` |
|    - | 1044 |  |
