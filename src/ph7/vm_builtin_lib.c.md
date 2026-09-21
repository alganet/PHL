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
|    - |  333 | `	/* This one definition serves every spelling — function names are case-insensitive` |
|    - |  334 | ``	   (hFunction, vm.c). The second, byte-identical `Dir()` copy that used to sit here`` |
|    - |  335 | `	   was PH7's manual hack for that, the same one the keyword table had. */\` |
|    - |  336 | `	"function dir(string $path){"\` |
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
|    - |  349 | `	"  /* php's rule is a two-way split, not a three-value enum: SORT_NONE leaves the"\` |
|    - |  350 | `	"     order alone and EVERY other value sorts -- ascending only for the exact"\` |
|    - |  351 | `	"     SORT_ASCENDING, descending otherwise. PHL left an unknown value UNSORTED,"\` |
|    - |  352 | `	"     which reads as SORT_NONE. */"\` |
|    - |  353 | `	"  if( $sort_order != SCANDIR_SORT_NONE ){"\` |
|    - |  354 | `	"      if( $sort_order == SCANDIR_SORT_ASCENDING ){ sort($aDir); }"\` |
|    - |  355 | `	"      else { rsort($aDir); }"\` |
|    - |  356 | `	"  }"\` |
|    - |  357 | `	"  return $aDir;"\` |
|    - |  358 | `	"}"\` |
|    - |  359 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|    - |  360 | `	"/* php rejects a mask holding any bit outside GLOB_AVAILABLE_FLAGS with a warning"\` |
|    - |  361 | `	"   and FALSE. PHL accepted anything and just tested the bits it knew, so a stale"\` |
|    - |  362 | `	"   script passing the OLD PHL glob values (1/2/4/...) silently got a plain glob."\` |
|    - |  363 | `	"   The literal is GLOB_AVAILABLE_FLAGS; PHL does not define that constant yet. */"\` |
|    - |  364 | `	"if( $iFlags & ~(GLOB_ERR\|GLOB_MARK\|GLOB_NOCHECK\|GLOB_NOSORT\|GLOB_BRACE\|GLOB_NOESCAPE\|GLOB_ONLYDIR) ){"\` |
|    - |  365 | `	"  trigger_error('glob(): At least one of the passed flags is invalid or not supported on this platform', E_USER_WARNING);"\` |
|    - |  366 | `	"  return FALSE;"\` |
|    - |  367 | `	"}"\` |
|    - |  368 | `	"/* php keeps the literal directory portion of the pattern in every result;"\` |
|    - |  369 | `	"   split off everything up to and including the last '/' as the prefix. */"\` |
|    - |  370 | `	"$slash = strrpos($pattern,'/');"\` |
|    - |  371 | `	"if( $slash === false ){ $zDir = '.'; $prefix = ''; $pat = $pattern; }"\` |
|    - |  372 | `	"else { $zDir = substr($pattern,0,$slash); if( $zDir === '' ){ $zDir = '/'; } $prefix = substr($pattern,0,$slash+1); $pat = substr($pattern,$slash+1); }"\` |
|    - |  373 | `	"$pHandle = opendir($zDir);"\` |
|    - |  374 | `	"if( $pHandle == FALSE ){"\` |
|    - |  375 | `	"   /* IO error while opening the target directory,return FALSE */"\` |
|    - |  376 | `	"	return FALSE;"\` |
|    - |  377 | `	"}"\` |
|    - |  378 | `	"$pArray = array(); /* Empty array */"\` |
|    - |  379 | `	"/* Loop throw available entries */"\` |
|    - |  380 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|    - |  381 | `	" /* php's glob() never matches a leading-dot entry (incl. '.' and '..') unless"\` |
|    - |  382 | `	"    the pattern itself starts with a dot */"\` |
|    - |  383 | `	"	if( strlen($pEntry) > 0 && $pEntry[0] === '.' && (strlen($pat) < 1 \|\| $pat[0] !== '.') ){ continue; }"\` |
|    - |  384 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|    - |  385 | `	"	$rc = strglob($pat,$pEntry);"\` |
|    - |  386 | `	"	if( $rc ){"\` |
|    - |  387 | `	"	   $zFull = $prefix . $pEntry;"\` |
|    - |  388 | `	"	   if( is_dir($zDir . '/' . $pEntry) ){"\` |
|    - |  389 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|    - |  390 | `	"		     /* Adds a slash to each directory returned */"\` |
|    - |  391 | `	"			 $zFull .= DIRECTORY_SEPARATOR;"\` |
|    - |  392 | `	"		  }"\` |
|    - |  393 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|    - |  394 | `	"	     /* Not a directory,ignore */"\` |
|    - |  395 | `	"		 continue;"\` |
|    - |  396 | `	"	   }"\` |
|    - |  397 | `	"	   /* Add the entry (with its literal directory prefix, php-style) */"\` |
|    - |  398 | `	"	   $pArray[] = $zFull;"\` |
|    - |  399 | `	"	}"\` |
|    - |  400 | `	" }"\` |
|    - |  401 | `	"/* Close the handle */"\` |
|    - |  402 | `	"closedir($pHandle);"\` |
|    - |  403 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|    - |  404 | `	"  /* Sort the array */"\` |
|    - |  405 | `	"  sort($pArray);"\` |
|    - |  406 | `	"}"\` |
|    - |  407 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|    - |  408 | `	"  /* Return the search pattern if no files matching were found */"\` |
|    - |  409 | `	"  $pArray[] = $pattern;"\` |
|    - |  410 | `	"}"\` |
|    - |  411 | `	"/* Return the created array */"\` |
|    - |  412 | `	"return $pArray;"\` |
|    - |  413 | `   "}"\` |
|    - |  414 | `   "/* Creates a temporary file */"\` |
|    - |  415 | `   "function tmpfile(){"\` |
|    - |  416 | `   "  /* Extract the temp directory */"\` |
|    - |  417 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|    - |  418 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|    - |  419 | `   "    /* Use the current dir */"\` |
|    - |  420 | `   "    $zTempDir = '.';"\` |
|    - |  421 | `   "  }"\` |
|    - |  422 | `   "  /* Create the file */"\` |
|    - |  423 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|    - |  424 | `   "  return $pHandle;"\` |
|    - |  425 | `   "}"\` |
|    - |  426 | `   "/* php's number_format(): missing entirely from PH7. */"\` |
|    - |  427 | `   "function number_format($num, $decimals = 0, $dec_point = '.', $thousands_sep = ','){"\` |
|    - |  428 | `   "  $num = (float)$num;"\` |
|    - |  429 | `   "  $decimals = (int)$decimals;"\` |
|    - |  430 | `   "  if( $dec_point === null ){ $dec_point = '.'; }"\` |
|    - |  431 | `   "  if( $thousands_sep === null ){ $thousands_sep = ','; }"\` |
|    - |  432 | `   "  /* round() first: sprintf uses banker's rounding, php's number_format rounds"\` |
|    - |  433 | `   "   * half AWAY FROM ZERO (number_format(0.5) is '1', not '0'). A NEGATIVE precision"\` |
|    - |  434 | `   "   * rounds to tens/hundreds (php 8: number_format(1.5,-1) is '0'); the displayed"\` |
|    - |  435 | `   "   * value never carries negative decimal places, so round with the real precision"\` |
|    - |  436 | `   "   * but format/append with max(0, $decimals). */"\` |
|    - |  437 | `   "  $num = round($num, $decimals);"\` |
|    - |  438 | `   "  $fdec = $decimals < 0 ? 0 : $decimals;"\` |
|    - |  439 | `   "  $s = sprintf('%.' . $fdec . 'f', $num);"\` |
|    - |  440 | `   "  $neg = false;"\` |
|    - |  441 | `   "  if( substr($s, 0, 1) === '-' ){ $neg = true; $s = substr($s, 1); }"\` |
|    - |  442 | `   "  $parts = explode('.', $s);"\` |
|    - |  443 | `   "  $int = $parts[0];"\` |
|    - |  444 | `   "  $frac = count($parts) > 1 ? $parts[1] : '';"\` |
|    - |  445 | `   "  $out = '';"\` |
|    - |  446 | `   "  $len = strlen($int);"\` |
|    - |  447 | `   "  $c = 0;"\` |
|    - |  448 | `   "  for( $i = $len - 1 ; $i >= 0 ; $i-- ){"\` |
|    - |  449 | `   "    $out = $int[$i] . $out;"\` |
|    - |  450 | `   "    $c++;"\` |
|    - |  451 | `   "    if( $c % 3 === 0 && $i > 0 ){ $out = $thousands_sep . $out; }"\` |
|    - |  452 | `   "  }"\` |
|    - |  453 | `   "  if( $fdec > 0 ){ $out = $out . $dec_point . $frac; }"\` |
|    - |  454 | `   "  if( $neg ){ $out = '-' . $out; }"\` |
|    - |  455 | `   "  return $out;"\` |
|    - |  456 | `   "}"\` |
|    - |  457 | `   "function is_nan($v){ $v = (float)$v; return $v != $v; }"\` |
|    - |  458 | `   "function is_infinite($v){ $v = (float)$v; return $v == INF \|\| $v == -INF; }"\` |
|    - |  459 | `   "function is_finite($v){ $v = (float)$v; return !is_nan($v) && !is_infinite($v); }"\` |
|    - |  460 | `   "/* php's version_compare: canonicalise (separators + digit/alpha boundaries all"\` |
|    - |  461 | `   " * become '.'), then compare parts with the special dev<alpha<beta<RC<#<pl ordering. */"\` |
|    - |  462 | `   "function __phl_vcanon($v){"\` |
|    - |  463 | `   "  $v = (string)$v; $len = strlen($v); $out = '';"\` |
|    - |  464 | `   "  for( $i = 0; $i < $len; $i++ ){"\` |
|    - |  465 | `   "   $c = $v[$i]; $rp = $i + 1 < $len ? $v[$i + 1] : '';"\` |
|    - |  466 | `   "   $cd = ($c >= '0' && $c <= '9');"\` |
|    - |  467 | `   "   $ca = $cd \|\| ($c >= 'a' && $c <= 'z') \|\| ($c >= 'A' && $c <= 'Z');"\` |
|    - |  468 | `   "   if( !$ca ){"\` |
|    - |  469 | `   "    /* any non-alphanumeric (., -, _, +, ...) is a separator: emit one '.' */"\` |
|    - |  470 | `   "    if( $out !== '' && substr($out, -1) !== '.' ){ $out .= '.'; }"\` |
|    - |  471 | `   "   }else{"\` |
|    - |  472 | `   "    $out .= $c;"\` |
|    - |  473 | `   "    $rd = ($rp >= '0' && $rp <= '9');"\` |
|    - |  474 | `   "    $ra = $rd \|\| ($rp >= 'a' && $rp <= 'z') \|\| ($rp >= 'A' && $rp <= 'Z');"\` |
|    - |  475 | `   "    if( $rp !== '' && $ra && ($cd !== $rd) ){ $out .= '.'; }"\` |
|    - |  476 | `   "   }"\` |
|    - |  477 | `   "  }"\` |
|    - |  478 | `   "  return explode('.', $out);"\` |
|    - |  479 | `   "}"\` |
|    - |  480 | `   "function __phl_vform($s){"\` |
|    - |  481 | `   "  if( $s === '' ){ return -1; }"\` |
|    - |  482 | `   "  if( ctype_digit($s) ){ return 4; }"\` |
|    - |  483 | `   "  $f = array('dev' => 0, 'alpha' => 1, 'a' => 1, 'beta' => 2, 'b' => 2, 'RC' => 3, 'rc' => 3, 'pl' => 5, 'p' => 5);"\` |
|    - |  484 | `   "  foreach( $f as $name => $ord ){ if( strncmp($s, $name, strlen($name)) === 0 ){ return $ord; } }"\` |
|    - |  485 | `   "  return -1;"\` |
|    - |  486 | `   "}"\` |
|    - |  487 | `   "function version_compare($version1, $version2, $operator = null){"\` |
|    - |  488 | `   "  $v1 = __phl_vcanon($version1); $v2 = __phl_vcanon($version2);"\` |
|    - |  489 | `   "  $n1 = count($v1); $n2 = count($v2); $n = $n1 > $n2 ? $n1 : $n2; $cmp = 0;"\` |
|    - |  490 | `   "  for( $i = 0; $i < $n; $i++ ){"\` |
|    - |  491 | `   "   $a = $i < $n1 ? $v1[$i] : null; $b = $i < $n2 ? $v2[$i] : null;"\` |
|    - |  492 | `   "   if( $a === null ){ $cmp = ctype_digit($b) ? -1 : (4 <=> __phl_vform($b)); }"\` |
|    - |  493 | `   "   elseif( $b === null ){ $cmp = ctype_digit($a) ? 1 : (__phl_vform($a) <=> 4); }"\` |
|    - |  494 | `   "   elseif( ctype_digit($a) && ctype_digit($b) ){ $cmp = (int)$a <=> (int)$b; }"\` |
|    - |  495 | `   "   else{ $cmp = __phl_vform($a) <=> __phl_vform($b); }"\` |
|    - |  496 | `   "   if( $cmp !== 0 ){ break; }"\` |
|    - |  497 | `   "  }"\` |
|    - |  498 | `   "  if( $operator === null ){ return $cmp; }"\` |
|    - |  499 | `   "  switch( (string)$operator ){"\` |
|    - |  500 | `   "   case '<': case 'lt': return $cmp < 0;"\` |
|    - |  501 | `   "   case '<=': case 'le': return $cmp <= 0;"\` |
|    - |  502 | `   "   case '>': case 'gt': return $cmp > 0;"\` |
|    - |  503 | `   "   case '>=': case 'ge': return $cmp >= 0;"\` |
|    - |  504 | `   "   case '==': case '=': case 'eq': return $cmp === 0;"\` |
|    - |  505 | `   "   case '!=': case '<>': case 'ne': return $cmp !== 0;"\` |
|    - |  506 | `   "  }"\` |
|    - |  507 | `   "  return null;"\` |
|    - |  508 | `   "}"\` |
|    - |  509 | `   "/* phl.stub_extensions (a -d/php.ini list, comma-separated) declares extensions"\` |
|    - |  510 | `   " * PHL does not implement as LOADED, backed by no-op behaviour, so software that"\` |
|    - |  511 | `   " * only GATES on extension_loaded() (e.g. PHPUnit's dom/xmlwriter check) runs"\` |
|    - |  512 | `   " * unmodified. It does NOT synthesize the extension's classes/functions. */"\` |
|    - |  513 | `   "function __phl_stub_exts(){"\` |
|    - |  514 | `   "  $s = ini_get('phl.stub_extensions');"\` |
|    - |  515 | `   "  if( $s === false \|\| $s === '' ){ return array(); }"\` |
|    - |  516 | `   "  $out = array();"\` |
|    - |  517 | `   "  foreach( explode(',', (string)$s) as $e ){ $e = trim($e); if( $e !== '' ){ $out[strtolower($e)] = $e; } }"\` |
|    - |  518 | `   "  return $out;"\` |
|    - |  519 | `   "}"\` |
|    - |  520 | `   "function extension_loaded($name){"\` |
|    - |  521 | `   "  static $ext = array('core' => 1, 'standard' => 1, 'pcre' => 1, 'json' => 1,"\` |
|    - |  522 | `   "   'ctype' => 1, 'date' => 1, 'spl' => 1, 'reflection' => 1, 'mbstring' => 1,"\` |
|    - |  523 | `   "   'hash' => 1, 'filter' => 1, 'session' => 1" PHL_EXT_LOADED_LIBXML ");"\` |
|    - |  524 | `   "  $n = strtolower((string)$name);"\` |
|    - |  525 | `   "  if( isset($ext[$n]) ){ return true; }"\` |
|    - |  526 | `   "  $stub = __phl_stub_exts();"\` |
|    - |  527 | `   "  return isset($stub[$n]);"\` |
|    - |  528 | `   "}"\` |
|    - |  529 | `   "function get_loaded_extensions($zend_extensions = false){"\` |
|    - |  530 | `   "  if( $zend_extensions ){ return array(); }"\` |
|    - |  531 | `   "  $base = array('Core','date','pcre','SPL','json','standard',"\` |
|    - |  532 | `   "   'ctype','filter','hash','Reflection','session','mbstring'" PHL_EXT_LIST_LIBXML ");"\` |
|    - |  533 | `   "  foreach( __phl_stub_exts() as $e ){ $base[] = $e; }"\` |
|    - |  534 | `   "  return $base;"\` |
|    - |  535 | `   "}"\` |
|    - |  536 | `   "/* Inverse of bin2hex() */"\` |
|    - |  537 | `   "function hex2bin($str){"\` |
|    - |  538 | `   "  $str = (string)$str;"\` |
|    - |  539 | `   "  $len = strlen($str);"\` |
|    - |  540 | `   "  if( $len % 2 !== 0 ){"\` |
|    - |  541 | `   "    trigger_error('hex2bin(): Hexadecimal input string must have an even length', E_USER_WARNING);"\` |
|    - |  542 | `   "    return false;"\` |
|    - |  543 | `   "  }"\` |
|    - |  544 | `   "  $out = '';"\` |
|    - |  545 | `   "  for( $i = 0 ; $i < $len ; $i += 2 ){"\` |
|    - |  546 | `   "    $pair = substr($str, $i, 2);"\` |
|    - |  547 | `   "    if( !ctype_xdigit($pair) ){"\` |
|    - |  548 | `   "      trigger_error('hex2bin(): Input string must be hexadecimal string', E_USER_WARNING);"\` |
|    - |  549 | `   "      return false;"\` |
|    - |  550 | `   "    }"\` |
|    - |  551 | `   "    $out = $out . chr(hexdec($pair));"\` |
|    - |  552 | `   "  }"\` |
|    - |  553 | `   "  return $out;"\` |
|    - |  554 | `   "}"\` |
|    - |  555 | `   "/* Division that never throws: INF/-INF/NAN like php */"\` |
|    - |  556 | `   "function fdiv($a, $b){"\` |
|    - |  557 | `   "  $a = (float)$a;"\` |
|    - |  558 | `   "  $b = (float)$b;"\` |
|    - |  559 | `   "  if( $b == 0.0 ){"\` |
|    - |  560 | `   "    if( $a == 0.0 \|\| is_nan($a) ){ return NAN; }"\` |
|    - |  561 | `   "    return $a > 0 ? INF : -INF;"\` |
|    - |  562 | `   "  }"\` |
|    - |  563 | `   "  return $a / $b;"\` |
|    - |  564 | `   "}"\` |
|    - |  565 | `   "function checkdate($month, $day, $year){"\` |
|    - |  566 | `   "  $month = (int)$month; $day = (int)$day; $year = (int)$year;"\` |
|    - |  567 | `   "  if( $month < 1 \|\| $month > 12 \|\| $year < 1 \|\| $year > 32767 \|\| $day < 1 ){ return false; }"\` |
|    - |  568 | `   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\` |
|    - |  569 | `   "  $max = $days[$month - 1];"\` |
|    - |  570 | `   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) \|\| ($year % 400 === 0)) ){"\` |
|    - |  571 | `   "    $max = 29;"\` |
|    - |  572 | `   "  }"\` |
|    - |  573 | `   "  return $day <= $max;"\` |
|    - |  574 | `   "}"\` |
|    - |  575 | `   "function is_iterable($v){ return is_array($v) \|\| ($v instanceof Traversable); }"\` |
|    - |  576 | `   "function is_countable($v){ return is_array($v) \|\| ($v instanceof Countable); }"\` |
|    - |  577 | `   "function key_exists($key, $array){ return array_key_exists($key, $array); }"\` |
|    - |  578 | `   "function doubleval($v){ return (float)$v; }"\` |
|    - |  579 | `   "function array_count_values($array){"\` |
|    - |  580 | `   "  $out = array();"\` |
|    - |  581 | `   "  foreach( $array as $v ){"\` |
|    - |  582 | `   "    if( !is_int($v) && !is_string($v) ){"\` |
|    - |  583 | `   "      trigger_error('array_count_values(): Can only count string and integer values, entry skipped', E_USER_WARNING);"\` |
|    - |  584 | `   "      continue;"\` |
|    - |  585 | `   "    }"\` |
|    - |  586 | `   "    if( isset($out[$v]) ){ $out[$v] = $out[$v] + 1; } else { $out[$v] = 1; }"\` |
|    - |  587 | `   "  }"\` |
|    - |  588 | `   "  return $out;"\` |
|    - |  589 | `   "}"\` |
|    - |  590 | `   "function array_change_key_case($array, $case = CASE_LOWER){"\` |
|    - |  591 | `   "  $out = array();"\` |
|    - |  592 | `   "  foreach( $array as $k => $v ){"\` |
|    - |  593 | `   "    if( is_string($k) ){ $k = ($case == CASE_UPPER) ? strtoupper($k) : strtolower($k); }"\` |
|    - |  594 | `   "    $out[$k] = $v;"\` |
|    - |  595 | `   "  }"\` |
|    - |  596 | `   "  return $out;"\` |
|    - |  597 | `   "}"\` |
|    - |  598 | `   "function array_replace_recursive($array, ...$others){"\` |
|    - |  599 | `   "  foreach( $others as $o ){"\` |
|    - |  600 | `   "    foreach( $o as $k => $v ){"\` |
|    - |  601 | `   "      if( is_array($v) && isset($array[$k]) && is_array($array[$k]) ){"\` |
|    - |  602 | `   "        $array[$k] = array_replace_recursive($array[$k], $v);"\` |
|    - |  603 | `   "      }else{"\` |
|    - |  604 | `   "        $array[$k] = $v;"\` |
|    - |  605 | `   "      }"\` |
|    - |  606 | `   "    }"\` |
|    - |  607 | `   "  }"\` |
|    - |  608 | `   "  return $array;"\` |
|    - |  609 | `   "}"\` |
|    - |  610 | `   "function class_uses($what, $autoload = true){"\` |
|    - |  611 | `   "  $c = is_object($what) ? get_class($what) : (string)$what;"\` |
|    - |  612 | `   "  if( !class_exists($c) ){ return false; }"\` |
|    - |  613 | `   "  return array();  /* PHL has no traits yet -- always the empty set */"\` |
|    - |  614 | `   "}"\` |
|    - |  615 | `   /* count_chars: php's five modes are a 2x2 split plus mode 0 -- 1/2 answer an \` |
|    - |  616 | `    * ARRAY, 3/4 a STRING, and the ODD modes report the bytes that WERE used \` |
|    - |  617 | `    * where the EVEN ones report the bytes that were NOT. PH7 implemented 0/1/3 \` |
|    - |  618 | `    * and let 2 and 4 fall through to mode 0's full table: the exact complement \` |
|    - |  619 | `    * of the answer asked for, silently. Any other mode is php's ValueError. */\` |
|    - |  620 | `   "function count_chars($str, $mode = 0){"\` |
|    - |  621 | `   "  if( is_array($mode) \|\| is_object($mode) \|\| is_resource($mode)"\` |
|    - |  622 | `   "   \|\| (is_string($mode) && !is_numeric($mode)) ){"\` |
|    - |  623 | `   "    throw new TypeError('count_chars(): Argument #2 ($mode) must be of type int, ' . __php_zpp_type($mode) . ' given');"\` |
|    - |  624 | `   "  }"\` |
|    - |  625 | `   "  $mode = (int)$mode;"\` |
|    - |  626 | `   "  if( $mode < 0 \|\| $mode > 4 ){"\` |
|    - |  627 | `   "    throw new ValueError('count_chars(): Argument #2 ($mode) must be between 0 and 4 (inclusive)');"\` |
|    - |  628 | `   "  }"\` |
|    - |  629 | `   "  $str = (string)$str;"\` |
|    - |  630 | `   "  $counts = array();"\` |
|    - |  631 | `   "  for( $i = 0 ; $i < 256 ; $i++ ){ $counts[$i] = 0; }"\` |
|    - |  632 | `   "  $len = strlen($str);"\` |
|    - |  633 | `   "  for( $i = 0 ; $i < $len ; $i++ ){ $b = ord($str[$i]); $counts[$b] = $counts[$b] + 1; }"\` |
|    - |  634 | `   "  if( $mode == 0 ){ return $counts; }"\` |
|    - |  635 | `   "  $used = ($mode == 1 \|\| $mode == 3);"\` |
|    - |  636 | `   "  $string = ($mode == 3 \|\| $mode == 4);"\` |
|    - |  637 | `   "  $out = $string ? '' : array();"\` |
|    - |  638 | `   "  foreach( $counts as $b => $n ){"\` |
|    - |  639 | `   "    if( ($n > 0) !== $used ){ continue; }"\` |
|    - |  640 | `   "    if( $string ){ $out = $out . chr($b); } else { $out[$b] = $n; }"\` |
|    - |  641 | `   "  }"\` |
|    - |  642 | `   "  return $out;"\` |
|    - |  643 | `   "}"\` |
|    - |  644 | `   "function ip2long($ip){"\` |
|    - |  645 | `   "  $p = explode('.', (string)$ip);"\` |
|    - |  646 | `   "  if( count($p) !== 4 ){ return false; }"\` |
|    - |  647 | `   "  $n = 0;"\` |
|    - |  648 | `   "  foreach( $p as $o ){"\` |
|    - |  649 | `   "    if( !ctype_digit($o) \|\| (int)$o < 0 \|\| (int)$o > 255 ){ return false; }"\` |
|    - |  650 | `   "    $n = $n * 256 + (int)$o;"\` |
|    - |  651 | `   "  }"\` |
|    - |  652 | `   "  return $n;"\` |
|    - |  653 | `   "}"\` |
|    - |  654 | `   "function long2ip($n){"\` |
|    - |  655 | `   "  $n = (int)$n;"\` |
|    - |  656 | `   "  return (($n >> 24) & 255) . '.' . (($n >> 16) & 255) . '.' . (($n >> 8) & 255) . '.' . ($n & 255);"\` |
|    - |  657 | `   "}"\` |
|    - |  658 | `   "function preg_filter($pattern, $replacement, $subject, $limit = -1){"\` |
|    - |  659 | `   "  if( is_array($subject) ){"\` |
|    - |  660 | `   "    $out = array();"\` |
|    - |  661 | `   "    foreach( $subject as $k => $v ){"\` |
|    - |  662 | `   "      $r = preg_replace($pattern, $replacement, (string)$v, $limit, $cnt);"\` |
|    - |  663 | `   "      if( $cnt > 0 ){ $out[$k] = $r; }"\` |
|    - |  664 | `   "    }"\` |
|    - |  665 | `   "    return $out;"\` |
|    - |  666 | `   "  }"\` |
|    - |  667 | `   "  $r = preg_replace($pattern, $replacement, (string)$subject, $limit, $cnt);"\` |
|    - |  668 | `   "  return $cnt > 0 ? $r : null;"\` |
|    - |  669 | `   "}"\` |
|    - |  670 | `   "function preg_replace_callback_array($patterns, $subject, $limit = -1){"\` |
|    - |  671 | `   "  foreach( $patterns as $pat => $cb ){"\` |
|    - |  672 | `   "    $subject = preg_replace_callback($pat, $cb, $subject, $limit);"\` |
|    - |  673 | `   "  }"\` |
|    - |  674 | `   "  return $subject;"\` |
|    - |  675 | `   "}"\` |
|    - |  676 | `   "function cal_days_in_month($calendar, $month, $year){"\` |
|    - |  677 | `   "  $month = (int)$month; $year = (int)$year;"\` |
|    - |  678 | `   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\` |
|    - |  679 | `   "  if( $month < 1 \|\| $month > 12 ){"\` |
|    - |  680 | `   "    throw new ValueError('cal_days_in_month(): Argument #2 ($month) must be a valid month');"\` |
|    - |  681 | `   "  }"\` |
|    - |  682 | `   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) \|\| ($year % 400 === 0)) ){"\` |
|    - |  683 | `   "    return 29;"\` |
|    - |  684 | `   "  }"\` |
|    - |  685 | `   "  return $days[$month - 1];"\` |
|    - |  686 | `   "}"\` |
|    - |  687 | `   "function preg_grep($pattern, $array, $flags = 0){"\` |
|    - |  688 | `   "  $out = array();"\` |
|    - |  689 | `   "  foreach( $array as $k => $v ){"\` |
|    - |  690 | `   "    $m = preg_match($pattern, (string)$v);"\` |
|    - |  691 | `   "    if( $flags & PREG_GREP_INVERT ){ $m = !$m; }"\` |
|    - |  692 | `   "    if( $m ){ $out[$k] = $v; }"\` |
|    - |  693 | `   "  }"\` |
|    - |  694 | `   "  return $out;"\` |
|    - |  695 | `   "}"\` |
|    - |  696 | `   "function class_implements($what, $autoload = true){"\` |
|    - |  697 | `   "  $c = is_object($what) ? get_class($what) : (string)$what;"\` |
|    - |  698 | `   "  if( !class_exists($c) && !interface_exists($c) ){ return false; }"\` |
|    - |  699 | `   "  $out = array();"\` |
|    - |  700 | `   "  $r = new ReflectionClass($c);"\` |
|    - |  701 | `   "  foreach( $r->getInterfaceNames() as $i ){ $out[$i] = $i; }"\` |
|    - |  702 | `   "  return $out;"\` |
|    - |  703 | `   "}"\` |
|    - |  704 | `   "function class_parents($what, $autoload = true){"\` |
|    - |  705 | `   "  $c = is_object($what) ? get_class($what) : (string)$what;"\` |
|    - |  706 | `   "  if( !class_exists($c) ){ return false; }"\` |
|    - |  707 | `   "  $out = array();"\` |
|    - |  708 | `   "  $r = new ReflectionClass($c);"\` |
|    - |  709 | `   "  while( ($p = $r->getParentClass()) ){"\` |
|    - |  710 | `   "    $n = $p->getName();"\` |
|    - |  711 | `   "    $out[$n] = $n;"\` |
|    - |  712 | `   "    $r = $p;"\` |
|    - |  713 | `   "  }"\` |
|    - |  714 | `   "  return $out;"\` |
|    - |  715 | `   "}"\` |
|    - |  716 | `   "/* php's http_build_query() -- missing from PH7. Skips null values, casts"\` |
|    - |  717 | `   " * bool to 1/0, prefixes numeric top-level keys, urlencodes per RFC. */"\` |
|    - |  718 | `   "function __phl_hbq_enc($s, $enc){"\` |
|    - |  719 | `   "  return $enc == PHP_QUERY_RFC3986 ? rawurlencode((string)$s) : urlencode((string)$s);"\` |
|    - |  720 | `   "}"\` |
|    - |  721 | `   "function __phl_hbq(&$pairs, $data, $key_prefix, $numeric_prefix, $sep, $enc){"\` |
|    - |  722 | `   "  foreach( $data as $k => $v ){"\` |
|    - |  723 | `   "    if( $v === null ){ continue; }"\` |
|    - |  724 | `   "    if( $key_prefix === '' ){"\` |
|    - |  725 | `   "      $ek = is_int($k) ? __phl_hbq_enc($numeric_prefix . $k, $enc) : __phl_hbq_enc($k, $enc);"\` |
|    - |  726 | `   "    } else {"\` |
|    - |  727 | `   "      $ek = $key_prefix . '%5B' . __phl_hbq_enc($k, $enc) . '%5D';"\` |
|    - |  728 | `   "    }"\` |
|    - |  729 | `   "    if( is_array($v) ){"\` |
|    - |  730 | `   "      __phl_hbq($pairs, $v, $ek, $numeric_prefix, $sep, $enc);"\` |
|    - |  731 | `   "    } elseif( is_object($v) ){"\` |
|    - |  732 | `   "      __phl_hbq($pairs, get_object_vars($v), $ek, $numeric_prefix, $sep, $enc);"\` |
|    - |  733 | `   "    } else {"\` |
|    - |  734 | `   "      if( $v === true ){ $v = '1'; } elseif( $v === false ){ $v = '0'; }"\` |
|    - |  735 | `   "      $pairs[] = $ek . '=' . __phl_hbq_enc($v, $enc);"\` |
|    - |  736 | `   "    }"\` |
|    - |  737 | `   "  }"\` |
|    - |  738 | `   "}"\` |
|    - |  739 | `   "function http_build_query($data, $numeric_prefix = '', $arg_separator = null, $encoding_type = PHP_QUERY_RFC1738){"\` |
|    - |  740 | `   "  if( !is_array($data) && !is_object($data) ){"\` |
|    - |  741 | `   "    throw new TypeError('http_build_query(): Argument #1 ($data) must be of type array, ' . __php_zpp_type($data) . ' given');"\` |
|    - |  742 | `   "  }"\` |
|    - |  743 | `   "  if( $arg_separator === null ){ $arg_separator = '&'; }"\` |
|    - |  744 | `   "  $pairs = array();"\` |
|    - |  745 | `   "  __phl_hbq($pairs, is_object($data) ? get_object_vars($data) : $data, '', (string)$numeric_prefix, $arg_separator, $encoding_type);"\` |
|    - |  746 | `   "  return implode($arg_separator, $pairs);"\` |
|    - |  747 | `   "}"\` |
|    - |  748 | `   "/* php's parse_str() -- missing from PH7. Mangles the base name ('.'/' ' -> '_'),"\` |
|    - |  749 | `   " * parses [key] nesting and [] appends, urldecodes keys and values. */"\` |
|    - |  750 | `   "function __phl_parsestr_assign(&$arr, $segments, $i, $val){"\` |
|    - |  751 | `   "  $seg = $segments[$i];"\` |
|    - |  752 | `   "  $last = ($i === count($segments) - 1);"\` |
|    - |  753 | `   "  if( $seg === '' ){"\` |
|    - |  754 | `   "    if( $last ){ $arr[] = $val; return; }"\` |
|    - |  755 | `   "    $arr[] = array();"\` |
|    - |  756 | `   "    $k = array_key_last($arr);"\` |
|    - |  757 | `   "    __phl_parsestr_assign($arr[$k], $segments, $i + 1, $val);"\` |
|    - |  758 | `   "  } else {"\` |
|    - |  759 | `   "    if( $last ){ $arr[$seg] = $val; return; }"\` |
|    - |  760 | `   "    if( !isset($arr[$seg]) \|\| !is_array($arr[$seg]) ){ $arr[$seg] = array(); }"\` |
|    - |  761 | `   "    __phl_parsestr_assign($arr[$seg], $segments, $i + 1, $val);"\` |
|    - |  762 | `   "  }"\` |
|    - |  763 | `   "}"\` |
|    - |  764 | `   "function parse_str($string, &$result){"\` |
|    - |  765 | `   "  $result = array();"\` |
|    - |  766 | `   "  $string = (string)$string;"\` |
|    - |  767 | `   "  if( $string === '' ){ return; }"\` |
|    - |  768 | `   "  foreach( explode('&', $string) as $pair ){"\` |
|    - |  769 | `   "    if( $pair === '' ){ continue; }"\` |
|    - |  770 | `   "    $eq = strpos($pair, '=');"\` |
|    - |  771 | `   "    if( $eq === false ){ $rawkey = $pair; $val = ''; }"\` |
|    - |  772 | `   "    else { $rawkey = substr($pair, 0, $eq); $val = urldecode(substr($pair, $eq + 1)); }"\` |
|    - |  773 | `   "    if( $rawkey === '' ){ continue; }"\` |
|    - |  774 | `   "    $bpos = strpos($rawkey, '[');"\` |
|    - |  775 | `   "    if( $bpos === false ){ $base = $rawkey; $subs = array(); }"\` |
|    - |  776 | `   "    else {"\` |
|    - |  777 | `   "      $base = substr($rawkey, 0, $bpos);"\` |
|    - |  778 | `   "      preg_match_all('/\\[([^\\]]*)\\]/', substr($rawkey, $bpos), $m);"\` |
|    - |  779 | `   "      $subs = $m[1];"\` |
|    - |  780 | `   "    }"\` |
|    - |  781 | `   "    $base = str_replace(array(' ', '.'), '_', urldecode($base));"\` |
|    - |  782 | `   "    if( $base === '' ){ continue; }"\` |
|    - |  783 | `   "    $segs = array($base);"\` |
|    - |  784 | `   "    foreach( $subs as $s ){ $segs[] = urldecode($s); }"\` |
|    - |  785 | `   "    __phl_parsestr_assign($result, $segs, 0, $val);"\` |
|    - |  786 | `   "  }"\` |
|    - |  787 | `   "}"\` |
|    - |  788 | `   "/* php 8.3 str_increment(): Perl-style alphanumeric increment. */"\` |
|    - |  789 | `   "function str_increment($string){"\` |
|    - |  790 | `   "  $string = (string)$string;"\` |
|    - |  791 | `   "  if( $string === '' ){ throw new ValueError('str_increment(): Argument #1 ($string) must not be empty'); }"\` |
|    - |  792 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_increment(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|    - |  793 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|    - |  794 | `   "    $c = $string[$i];"\` |
|    - |  795 | `   "    if( $c === 'z' ){ $string[$i] = 'a'; }"\` |
|    - |  796 | `   "    elseif( $c === 'Z' ){ $string[$i] = 'A'; }"\` |
|    - |  797 | `   "    elseif( $c === '9' ){ $string[$i] = '0'; }"\` |
|    - |  798 | `   "    else { $string[$i] = chr(ord($c) + 1); return $string; }"\` |
|    - |  799 | `   "  }"\` |
|    - |  800 | `   "  $first = $string[0];"\` |
|    - |  801 | `   "  if( $first === '0' ){ return '1' . $string; }"\` |
|    - |  802 | `   "  if( $first === 'a' ){ return 'a' . $string; }"\` |
|    - |  803 | `   "  return 'A' . $string;"\` |
|    - |  804 | `   "}"\` |
|    - |  805 | `   "/* php 8.3 str_decrement(): inverse of str_increment(); throws out of range"\` |
|    - |  806 | `   " * at the bottom of the counting sequence. */"\` |
|    - |  807 | `   "function str_decrement($string){"\` |
|    - |  808 | `   "  $string = (string)$string;"\` |
|    - |  809 | `   "  if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) must not be empty'); }"\` |
|    - |  810 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_decrement(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|    - |  811 | `   "  $orig = $string;"\` |
|    - |  812 | `   "  $borrowed = false;"\` |
|    - |  813 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|    - |  814 | `   "    $c = $string[$i];"\` |
|    - |  815 | `   "    if( $c === 'a' ){ $string[$i] = 'z'; }"\` |
|    - |  816 | `   "    elseif( $c === 'A' ){ $string[$i] = 'Z'; }"\` |
|    - |  817 | `   "    elseif( $c === '0' ){ $string[$i] = '9'; }"\` |
|    - |  818 | `   "    else { $string[$i] = chr(ord($c) - 1); $borrowed = false; break; }"\` |
|    - |  819 | `   "    if( $i === 0 ){ $borrowed = true; }"\` |
|    - |  820 | `   "  }"\` |
|    - |  821 | `   "  if( $borrowed ){"\` |
|    - |  822 | `   "    if( $string[0] === '9' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|    - |  823 | `   "    $string = substr($string, 1);"\` |
|    - |  824 | `   "    if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|    - |  825 | `   "  } elseif( strlen($string) > 1 && $string[0] === '0' ){"\` |
|    - |  826 | `   "    $string = substr($string, 1);"\` |
|    - |  827 | `   "  }"\` |
|    - |  828 | `   "  return $string;"\` |
|    - |  829 | `   "}"\` |
|    - |  830 | `   "/* Permission bits via stat(); false + warning when stat fails, like php. */"\` |
|    - |  831 | `   "function fileperms($filename){"\` |
|    - |  832 | `   "  $s = @stat($filename);"\` |
|    - |  833 | `   "  if( $s === false ){"\` |
|    - |  834 | `   "    trigger_error('fileperms(): stat failed for ' . $filename, E_USER_WARNING);"\` |
|    - |  835 | `   "    return false;"\` |
|    - |  836 | `   "  }"\` |
|    - |  837 | `   "  return $s['mode'];"\` |
|    - |  838 | `   "}"\` |
|    - |  839 | `   "/* PH7 keeps no stat cache, so this is a no-op like php on a clean cache. */"\` |
|    - |  840 | `   "function clearstatcache($clear_realpath_cache = false, $filename = ''){}"\` |
|    - |  841 | `   "/* php 8.4 mb_ucfirst/mb_lcfirst: case-map only the first multibyte char. */"\` |
|    - |  842 | `   "function mb_ucfirst($string, $encoding = null){"\` |
|    - |  843 | `   "  $string = (string)$string;"\` |
|    - |  844 | `   "  if( $string === '' ){ return ''; }"\` |
|    - |  845 | `   "  return mb_strtoupper(mb_substr($string, 0, 1)) . mb_substr($string, 1);"\` |
|    - |  846 | `   "}"\` |
|    - |  847 | `   "function mb_lcfirst($string, $encoding = null){"\` |
|    - |  848 | `   "  $string = (string)$string;"\` |
|    - |  849 | `   "  if( $string === '' ){ return ''; }"\` |
|    - |  850 | `   "  return mb_strtolower(mb_substr($string, 0, 1)) . mb_substr($string, 1);"\` |
|    - |  851 | `   "}"\` |
|    - |  852 | `   "/* php 8.4 mb_trim family: strip leading/trailing characters (whole"\` |
|    - |  853 | `   " * multibyte chars, NO range syntax), defaulting to php's Unicode"\` |
|    - |  854 | `   " * whitespace set. */"\` |
|    - |  855 | `   "function __phl_mb_ws(){"\` |
|    - |  856 | `   "  static $set = null;"\` |
|    - |  857 | `   "  if( $set === null ){"\` |
|    - |  858 | `   "    $set = array();"\` |
|    - |  859 | `   "    foreach( array(0x00,0x09,0x0A,0x0B,0x0C,0x0D,0x20,0x85,0xA0,0x1680,"\` |
|    - |  860 | `   "      0x180E,0x2000,0x2001,0x2002,0x2003,0x2004,0x2005,0x2006,0x2007,0x2008,"\` |
|    - |  861 | `   "      0x2009,0x200A,0x2028,0x2029,0x202F,0x205F,0x3000) as $cp ){"\` |
|    - |  862 | `   "      $set[mb_chr($cp)] = true;"\` |
|    - |  863 | `   "    }"\` |
|    - |  864 | `   "  }"\` |
|    - |  865 | `   "  return $set;"\` |
|    - |  866 | `   "}"\` |
|    - |  867 | `   "function __phl_mb_trim($string, $characters, $left, $right){"\` |
|    - |  868 | `   "  $string = (string)$string;"\` |
|    - |  869 | `   "  if( $string === '' ){ return ''; }"\` |
|    - |  870 | `   "  if( $characters === null ){"\` |
|    - |  871 | `   "    $set = __phl_mb_ws();"\` |
|    - |  872 | `   "  } else {"\` |
|    - |  873 | `   "    $set = array();"\` |
|    - |  874 | `   "    foreach( mb_str_split((string)$characters) as $c ){ $set[$c] = true; }"\` |
|    - |  875 | `   "  }"\` |
|    - |  876 | `   "  $chars = mb_str_split($string);"\` |
|    - |  877 | `   "  $n = count($chars);"\` |
|    - |  878 | `   "  $i = 0; $j = $n;"\` |
|    - |  879 | `   "  if( $left ){ while( $i < $j && isset($set[$chars[$i]]) ){ $i++; } }"\` |
|    - |  880 | `   "  if( $right ){ while( $j > $i && isset($set[$chars[$j - 1]]) ){ $j--; } }"\` |
|    - |  881 | `   "  return implode('', array_slice($chars, $i, $j - $i));"\` |
|    - |  882 | `   "}"\` |
|    - |  883 | `   "function mb_trim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, true, true); }"\` |
|    - |  884 | `   "function mb_ltrim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, true, false); }"\` |
|    - |  885 | `   "function mb_rtrim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, false, true); }"\` |
|    - |  886 | `   "/* Creates a temporary file and returns its name */"\` |
|    - |  887 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|    - |  888 | `   "{"\` |
|    - |  889 | `   "   /* php CREATES the file (empty, mode 0600) and guarantees the name is unique --"\` |
|    - |  890 | `   "    * returning a bare name left the caller with a path that does not exist, so"\` |
|    - |  891 | `   "    * file_exists() was false and unlink() failed on it. */"\` |
|    - |  892 | `   "   $zDir = rtrim($zDir, DIRECTORY_SEPARATOR);"\` |
|    - |  893 | `   "   for( $i = 0 ; $i < 64 ; ++$i ){"\` |
|    - |  894 | `   "     $zPath = $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|    - |  895 | `   "     if( file_exists($zPath) ){ continue; }"\` |
|    - |  896 | `   "     $pHandle = @fopen($zPath,'x');"\` |
|    - |  897 | `   "     if( $pHandle === false ){ continue; }"\` |
|    - |  898 | `   "     fclose($pHandle);"\` |
|    - |  899 | `   "     @chmod($zPath, 0600);"\` |
|    - |  900 | `   "     return $zPath;"\` |
|    - |  901 | `   "   }"\` |
|    - |  902 | `   "   return false;"\` |
|    - |  903 | `   "}"\` |
|    - |  904 | `   "function array_unshift(&$pArray ){"\` |
|    - |  905 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|    - |  906 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . __php_zpp_type($pArray) . ' given'); }"\` |
|    - |  907 | `   "/* Copy arguments */"\` |
|    - |  908 | `   "$nArgs = func_num_args();"\` |
|    - |  909 | `   "$pNew = array();"\` |
|    - |  910 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|    - |  911 | `    " $pNew[] = func_get_arg($i);"\` |
|    - |  912 | `    "}"\` |
|    - |  913 | `   	"/* Make a copy of the old entries */"\` |
|    - |  914 | `	"$pOld = array_copy($pArray);"\` |
|    - |  915 | `	"/* Erase */"\` |
|    - |  916 | `	"array_erase($pArray);"\` |
|    - |  917 | `	"/* Unshift */"\` |
|    - |  918 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|    - |  919 | `	"return sizeof($pArray);"\` |
|    - |  920 | `    "}"\` |
|    - |  921 | `	"function array_merge_recursive(){"\` |
|    - |  922 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|    - |  923 | `    "$arrays = func_get_args();"\` |
|    - |  924 | `    "$narrays = count($arrays);"\` |
|    - |  925 | `    "$ret = array();"\` |
|    - |  926 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|    - |  927 | `	 " if( !is_array($arrays[$i]) ){"\` |
|    - |  928 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.__php_zpp_type($arrays[$i]).' given');"\` |
|    - |  929 | `	 " }"\` |
|    - |  930 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|    - |  931 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|    - |  932 | `     "  if( $keyIsInt ) {"\` |
|    - |  933 | `     "   $ret[] = $value;"\` |
|    - |  934 | `     "  } else {"\` |
|    - |  935 | `     "   if (array_key_exists($key, $ret)) {"\` |
|    - |  936 | `     "    $cur = $ret[$key];"\` |
|    - |  937 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|    - |  938 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|    - |  939 | `     "    } elseif (is_array($cur)) {"\` |
|    - |  940 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|    - |  941 | `     "    } elseif (is_array($value)) {"\` |
|    - |  942 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|    - |  943 | `     "    } else {"\` |
|    - |  944 | `     "     $ret[$key] = array($cur, $value);"\` |
|    - |  945 | `     "    }"\` |
|    - |  946 | `     "   } else {"\` |
|    - |  947 | `     "    $ret[$key] = $value;"\` |
|    - |  948 | `     "   }"\` |
|    - |  949 | `     "  }"\` |
|    - |  950 | `     " }"\` |
|    - |  951 | `	 " }"\` |
|    - |  952 | `	 " return $ret;"\` |
|    - |  953 | `    "}"\` |
|    - |  954 | `	/* __php_zpp_type: php's ZPP value-name for TypeError messages */\` |
|    - |  955 | `	"function __php_zpp_type($v){"\` |
|    - |  956 | `	" if( is_object($v) ){ return get_class($v); }"\` |
|    - |  957 | `	" if( is_int($v) ){ return 'int'; }"\` |
|    - |  958 | `	" if( is_float($v) ){ return 'float'; }"\` |
|    - |  959 | `	" if( is_string($v) ){ return 'string'; }"\` |
|    - |  960 | `	" if( is_bool($v) ){ return $v ? 'true' : 'false'; }"\` |
|    - |  961 | `	" if( is_null($v) ){ return 'null'; }"\` |
|    - |  962 | `	" if( is_array($v) ){ return 'array'; }"\` |
|    - |  963 | `	" if( is_resource($v) ){ return 'resource'; }"\` |
|    - |  964 | `	" return 'mixed';"\` |
|    - |  965 | `	"}"\` |
|    - |  966 | `	"function max(){"\` |
|    - |  967 | `    "  $pArgs = func_get_args();"\` |
|    - |  968 | `    " if( sizeof($pArgs) < 1 ){"\` |
|    - |  969 | `	"  throw new ArgumentCountError('max() expects at least 1 argument, 0 given');"\` |
|    - |  970 | `    " }"\` |
|    - |  971 | `    " if( sizeof($pArgs) < 2 ){"\` |
|    - |  972 | `    " $pArg = $pArgs[0];"\` |
|    - |  973 | `	" if( !is_array($pArg) ){"\` |
|    - |  974 | `	"   throw new TypeError('max(): Argument #1 ($value) must be of type array, ' . __php_zpp_type($pArg) . ' given');"\` |
|    - |  975 | `	" }"\` |
|    - |  976 | `	" if( sizeof($pArg) < 1 ){"\` |
|    - |  977 | `	"   throw new ValueError('max(): Argument #1 ($value) must contain at least one element');"\` |
|    - |  978 | `	" }"\` |
|    - |  979 | `	" $max = null; $first = true;"\` |
|    - |  980 | `	" foreach( $pArgs[0] as $val ){"\` |
|    - |  981 | `	"   if( $first ){ $max = $val; $first = false; }"\` |
|    - |  982 | `	"   else if( $val > $max ){ $max = $val; }"\` |
|    - |  983 | `	" }"\` |
|    - |  984 | `	" return $max;"\` |
|    - |  985 | `    " }"\` |
|    - |  986 | `    " $max = $pArgs[0];"\` |
|    - |  987 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|    - |  988 | `    " $val = $pArgs[$i];"\` |
|    - |  989 | `	"if( $val > $max ){"\` |
|    - |  990 | `	" $max = $val;"\` |
|    - |  991 | `	"}"\` |
|    - |  992 | `    " }"\` |
|    - |  993 | `	" return $max;"\` |
|    - |  994 | `    "}"\` |
|    - |  995 | `	"function min(){"\` |
|    - |  996 | `    "  $pArgs = func_get_args();"\` |
|    - |  997 | `    " if( sizeof($pArgs) < 1 ){"\` |
|    - |  998 | `	"  throw new ArgumentCountError('min() expects at least 1 argument, 0 given');"\` |
|    - |  999 | `    " }"\` |
|    - | 1000 | `    " if( sizeof($pArgs) < 2 ){"\` |
|    - | 1001 | `    " $pArg = $pArgs[0];"\` |
|    - | 1002 | `	" if( !is_array($pArg) ){"\` |
|    - | 1003 | `	"   throw new TypeError('min(): Argument #1 ($value) must be of type array, ' . __php_zpp_type($pArg) . ' given');"\` |
|    - | 1004 | `	" }"\` |
|    - | 1005 | `	" if( sizeof($pArg) < 1 ){"\` |
|    - | 1006 | `	"   throw new ValueError('min(): Argument #1 ($value) must contain at least one element');"\` |
|    - | 1007 | `	" }"\` |
|    - | 1008 | `	" $min = null; $first = true;"\` |
|    - | 1009 | `	" foreach( $pArgs[0] as $val ){"\` |
|    - | 1010 | `	"   if( $first ){ $min = $val; $first = false; }"\` |
|    - | 1011 | `	"   else if( $val < $min ){ $min = $val; }"\` |
|    - | 1012 | `	" }"\` |
|    - | 1013 | `	" return $min;"\` |
|    - | 1014 | `    " }"\` |
|    - | 1015 | `    " $min = $pArgs[0];"\` |
|    - | 1016 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|    - | 1017 | `    " $val = $pArgs[$i];"\` |
|    - | 1018 | `	"if( $val < $min ){"\` |
|    - | 1019 | `	" $min = $val;"\` |
|    - | 1020 | `	" }"\` |
|    - | 1021 | `    " }"\` |
|    - | 1022 | `	" return $min;"\` |
|    - | 1023 | `	"}"\` |
|    - | 1024 | `	"function fileowner(string $file){"\` |
|    - | 1025 | `    " $a = stat($file);"\` |
|    - | 1026 | `	" if( !is_array($a) ){"\` |
|    - | 1027 | `	"	return false;"\` |
|    - | 1028 | `	" }"\` |
|    - | 1029 | `	" return $a['uid'];"\` |
|    - | 1030 | `    "}"\` |
|    - | 1031 | `    "function filegroup(string $file){"\` |
|    - | 1032 | `	" $a = stat($file);"\` |
|    - | 1033 | `	" if( !is_array($a) ){"\` |
|    - | 1034 | `	"	return false;"\` |
|    - | 1035 | `	" }"\` |
|    - | 1036 | `	" return $a['gid'];"\` |
|    - | 1037 | `    "}"\` |
|    - | 1038 | `	 "function fileinode(string $file){"\` |
|    - | 1039 | `	" $a = stat($file);"\` |
|    - | 1040 | `	" if( !is_array($a) ){"\` |
|    - | 1041 | `	"	return false;"\` |
|    - | 1042 | `	" }"\` |
|    - | 1043 | `	" return $a['ino'];"\` |
|    - | 1044 | `    "}"` |
|    - | 1045 |  |
| 4140 | 1046 | `PH7_PRIVATE sxi32 PH7_VmInstallBuiltinLib(ph7_vm *pVm)` |
|    5 | 1047 | `{` |
|    - | 1048 | `	SyString sBuiltin;` |
|    - | 1049 | `	SyString sRandom;` |
| 4145 | 1050 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|    - | 1051 | `	/* Compile the built-in library */` |
| 4145 | 1052 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|    - | 1053 | `	/* Register the Random\RandomException namespaced class (PHP 8.2+).` |
|    - | 1054 | `	 * Kept in its own VmEvalChunk (not appended to PH7_BUILTIN_LIB): a namespace` |
|    - | 1055 | `	 * declaration is NOT reset at the block's closing brace in this engine, so` |
|    - | 1056 | `	 * anything following it in the same chunk would leak into the Random` |
|    - | 1057 | `	 * namespace. Isolation instead comes from VmEvalChunk saving/restoring` |
|    - | 1058 | `	 * pVm->sNamespace (and PH7_ResetCodeGenerator clearing the compiler` |
|    - | 1059 | `	 * namespace) per chunk, so this lands as Random\RandomException while later` |
|    - | 1060 | `	 * user code still compiles in the global namespace. */` |
|    - | 1061 | `	{` |
|    - | 1062 | `		static const char zRandomLib[] =` |
|    - | 1063 | `			"namespace Random { class RandomException extends \\Exception { } }";` |
| 4145 | 1064 | `		SyStringInitFromBuf(&sRandom,zRandomLib,sizeof(zRandomLib)-1);` |
| 4145 | 1065 | `		VmEvalChunk(&(*pVm),0,&sRandom,PH7_PHP_ONLY,FALSE);` |
|    - | 1066 | `	}` |
| 4145 | 1067 | `	return SXRET_OK;` |
|    5 | 1068 | `}` |
|    - | 1069 |  |
