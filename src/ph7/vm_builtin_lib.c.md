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
|    - |  271 | `	/* Fiber and Generator are declared ENTIRELY from C — class, private slots and` |
|    - |  272 | `	 * every method — by PH7_VmInstallFiberNative / PH7_VmInstallGeneratorNative.` |
|    - |  273 | `	 * They are the first two builtin classes with no presence in this chunk at all.` |
|    - |  274 | ``	 * Generator's `implements Iterator` is attached there too, and has to be: it is`` |
|    - |  275 | `	 * applied AFTER its methods exist, because PH7_ClassImplement installs an` |
|    - |  276 | `	 * ABSTRACT stub for every interface method a class does not already declare. */\` |
|    - |  277 | `	"final class Closure {"\` |
|    - |  278 | `	"  private $__fn;"\` |
|    - |  279 | `	"  private $__this;"\` |
|    - |  280 | `	"  private $__scope;"\` |
|    - |  281 | `	"  public function __construct(){ throw new \\Error('Instantiation of class Closure is not allowed'); }"\` |
|    - |  282 | `	/* bindTo()/bind()/fromCallable() are NATIVE methods installed by` |
|    - |  283 | `	 * PH7_VmInstallClosureNative() — they used to be one-line forwards to the` |
|    - |  284 | `	 * global __closure_bindTo/__closure_fromCallable thunks, which no longer` |
|    - |  285 | `	 * exist. call() stays here because its body is genuinely PHP: it rebinds and` |
|    - |  286 | `	 * then invokes with an argument unpack. */\` |
|    - |  287 | `	"  public function call($newThis, ...$args){ $bound = $this->bindTo($newThis, get_class($newThis)); return $bound(...$args); }"\` |
|    - |  288 | `	"}"\` |
|    - |  289 | `	/* stdClass is empty (PHP-exact): holds only dynamic (runtime-added) properties. */\` |
|    - |  290 | `	"#[Attribute(Attribute::TARGET_CLASS)]"\` |
|    - |  291 | `	"final class Attribute {"\` |
|    - |  292 | `	"  const TARGET_CLASS = 1;"\` |
|    - |  293 | `	"  const TARGET_FUNCTION = 2;"\` |
|    - |  294 | `	"  const TARGET_METHOD = 4;"\` |
|    - |  295 | `	"  const TARGET_PROPERTY = 8;"\` |
|    - |  296 | `	"  const TARGET_CLASS_CONSTANT = 16;"\` |
|    - |  297 | `	"  const TARGET_PARAMETER = 32;"\` |
|    - |  298 | `	"  const TARGET_CONSTANT = 64;"\` |
|    - |  299 | `	"  const TARGET_ALL = 127;"\` |
|    - |  300 | `	"  const IS_REPEATABLE = 128;"\` |
|    - |  301 | `	"  public $flags;"\` |
|    - |  302 | `	"  public function __construct($flags = 127){ $this->flags = $flags; }"\` |
|    - |  303 | `	"}"\` |
|    - |  304 | `	"#[Attribute(Attribute::TARGET_METHOD \| Attribute::TARGET_FUNCTION \| Attribute::TARGET_CLASS_CONSTANT \| Attribute::TARGET_CONSTANT)]"\` |
|    - |  305 | `	"final class Deprecated {"\` |
|    - |  306 | `	"  public $message;"\` |
|    - |  307 | `	"  public $since;"\` |
|    - |  308 | `	"  public function __construct($message = null, $since = null){"\` |
|    - |  309 | `	"    $this->message = $message;"\` |
|    - |  310 | `	"    $this->since = $since;"\` |
|    - |  311 | `	"  }"\` |
|    - |  312 | `	"}"\` |
|    - |  313 | `	"class stdClass{"\` |
|    - |  314 | `	"}"\` |
|    - |  315 | `	/* This one definition serves every spelling — function names are case-insensitive` |
|    - |  316 | ``	   (hFunction, vm.c). The second, byte-identical `Dir()` copy that used to sit here`` |
|    - |  317 | `	   was PH7's manual hack for that, the same one the keyword table had. */\` |
|    - |  318 | `	"function dir(string $directory, $context = null){"\` |
|    - |  319 | `	"   return new Directory($directory);"\` |
|    - |  320 | `	"}"\` |
|    - |  321 | `	"function scandir(string $directory,int $sorting_order = SCANDIR_SORT_ASCENDING, $context = null)"\` |
|    - |  322 | `    "{"\` |
|    - |  323 | `	"  $aDir = array();"\` |
|    - |  324 | `	"  $pHandle = opendir($directory);"\` |
|    - |  325 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|    - |  326 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|    - |  327 | `	"      $aDir[] = $pEntry;"\` |
|    - |  328 | `	"   }"\` |
|    - |  329 | `	"  closedir($pHandle);"\` |
|    - |  330 | `	"  /* php's rule is a two-way split, not a three-value enum: SORT_NONE leaves the"\` |
|    - |  331 | `	"     order alone and EVERY other value sorts -- ascending only for the exact"\` |
|    - |  332 | `	"     SORT_ASCENDING, descending otherwise. PHL left an unknown value UNSORTED,"\` |
|    - |  333 | `	"     which reads as SORT_NONE. */"\` |
|    - |  334 | `	"  if( $sorting_order != SCANDIR_SORT_NONE ){"\` |
|    - |  335 | `	"      if( $sorting_order == SCANDIR_SORT_ASCENDING ){ sort($aDir); }"\` |
|    - |  336 | `	"      else { rsort($aDir); }"\` |
|    - |  337 | `	"  }"\` |
|    - |  338 | `	"  return $aDir;"\` |
|    - |  339 | `	"}"\` |
|    - |  340 | `	"function glob(string $pattern,int $flags = 0){"\` |
|    - |  341 | `	"/* php rejects a mask holding any bit outside GLOB_AVAILABLE_FLAGS with a warning"\` |
|    - |  342 | `	"   and FALSE. PHL accepted anything and just tested the bits it knew, so a stale"\` |
|    - |  343 | `	"   script passing the OLD PHL glob values (1/2/4/...) silently got a plain glob."\` |
|    - |  344 | `	"   The literal is GLOB_AVAILABLE_FLAGS; PHL does not define that constant yet. */"\` |
|    - |  345 | `	"if( $flags & ~(GLOB_ERR\|GLOB_MARK\|GLOB_NOCHECK\|GLOB_NOSORT\|GLOB_BRACE\|GLOB_NOESCAPE\|GLOB_ONLYDIR) ){"\` |
|    - |  346 | `	"  trigger_error('glob(): At least one of the passed flags is invalid or not supported on this platform', E_USER_WARNING);"\` |
|    - |  347 | `	"  return FALSE;"\` |
|    - |  348 | `	"}"\` |
|    - |  349 | `	"/* php keeps the literal directory portion of the pattern in every result;"\` |
|    - |  350 | `	"   split off everything up to and including the last '/' as the prefix. */"\` |
|    - |  351 | `	"$slash = strrpos($pattern,'/');"\` |
|    - |  352 | `	"if( $slash === false ){ $zDir = '.'; $prefix = ''; $pat = $pattern; }"\` |
|    - |  353 | `	"else { $zDir = substr($pattern,0,$slash); if( $zDir === '' ){ $zDir = '/'; } $prefix = substr($pattern,0,$slash+1); $pat = substr($pattern,$slash+1); }"\` |
|    - |  354 | `	"$pHandle = opendir($zDir);"\` |
|    - |  355 | `	"if( $pHandle == FALSE ){"\` |
|    - |  356 | `	"   /* IO error while opening the target directory,return FALSE */"\` |
|    - |  357 | `	"	return FALSE;"\` |
|    - |  358 | `	"}"\` |
|    - |  359 | `	"$pArray = array(); /* Empty array */"\` |
|    - |  360 | `	"/* Loop throw available entries */"\` |
|    - |  361 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|    - |  362 | `	" /* php's glob() never matches a leading-dot entry (incl. '.' and '..') unless"\` |
|    - |  363 | `	"    the pattern itself starts with a dot */"\` |
|    - |  364 | `	"	if( strlen($pEntry) > 0 && $pEntry[0] === '.' && (strlen($pat) < 1 \|\| $pat[0] !== '.') ){ continue; }"\` |
|    - |  365 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|    - |  366 | `	"	$rc = strglob($pat,$pEntry);"\` |
|    - |  367 | `	"	if( $rc ){"\` |
|    - |  368 | `	"	   $zFull = $prefix . $pEntry;"\` |
|    - |  369 | `	"	   if( is_dir($zDir . '/' . $pEntry) ){"\` |
|    - |  370 | `	"	      if( $flags & GLOB_MARK ){"\` |
|    - |  371 | `	"		     /* Adds a slash to each directory returned */"\` |
|    - |  372 | `	"			 $zFull .= DIRECTORY_SEPARATOR;"\` |
|    - |  373 | `	"		  }"\` |
|    - |  374 | `	"	   }else if( $flags & GLOB_ONLYDIR ){"\` |
|    - |  375 | `	"	     /* Not a directory,ignore */"\` |
|    - |  376 | `	"		 continue;"\` |
|    - |  377 | `	"	   }"\` |
|    - |  378 | `	"	   /* Add the entry (with its literal directory prefix, php-style) */"\` |
|    - |  379 | `	"	   $pArray[] = $zFull;"\` |
|    - |  380 | `	"	}"\` |
|    - |  381 | `	" }"\` |
|    - |  382 | `	"/* Close the handle */"\` |
|    - |  383 | `	"closedir($pHandle);"\` |
|    - |  384 | `	"if( ($flags & GLOB_NOSORT) == 0 ){"\` |
|    - |  385 | `	"  /* Sort the array */"\` |
|    - |  386 | `	"  sort($pArray);"\` |
|    - |  387 | `	"}"\` |
|    - |  388 | `	"if( ($flags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|    - |  389 | `	"  /* Return the search pattern if no files matching were found */"\` |
|    - |  390 | `	"  $pArray[] = $pattern;"\` |
|    - |  391 | `	"}"\` |
|    - |  392 | `	"/* Return the created array */"\` |
|    - |  393 | `	"return $pArray;"\` |
|    - |  394 | `   "}"\` |
|    - |  395 | `   "/* Creates a temporary file */"\` |
|    - |  396 | `   "function tmpfile(){"\` |
|    - |  397 | `   "  /* Extract the temp directory */"\` |
|    - |  398 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|    - |  399 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|    - |  400 | `   "    /* Use the current dir */"\` |
|    - |  401 | `   "    $zTempDir = '.';"\` |
|    - |  402 | `   "  }"\` |
|    - |  403 | `   "  /* Create the file */"\` |
|    - |  404 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|    - |  405 | `   "  return $pHandle;"\` |
|    - |  406 | `   "}"\` |
|    - |  407 | `   "/* php's number_format(): missing entirely from PH7."\` |
|    - |  408 | `   " * Signature: number_format(float $num, int $decimals = 0, ?string $decimal_separator = '.',"\` |
|    - |  409 | `   " * ?string $thousands_separator = ','): string. Being a PRELUDE function it is out of reach"\` |
|    - |  410 | `   " * of the aBuiltinSig[] ZPP screen (host builtins only), so the four rows are checked here in"\` |
|    - |  411 | `   " * the shape php's ZPP reports them (__php_zpp_type, the pattern count_chars/max/min use)."\` |
|    - |  412 | `   " * Without them an array, a not-stringable object, a resource and a non-numeric string all"\` |
|    - |  413 | `   " * reached the (float) cast and ANSWERED -- number_format(new Bare()) warned and returned"\` |
|    - |  414 | `   " * '1', number_format('abc') returned '0'. A leading-numeric string ('12abc') is php's"\` |
|    - |  415 | `   " * TypeError too, not a silent 12. null and a lossy float stay PHL's TypeError rather than"\` |
|    - |  416 | `   " * php's deprecate-and-coerce (§10), matching what str_repeat() already answers for the"\` |
|    - |  417 | `   " * same two; the two SEPARATORS are declared ?string, so null is legal there. */"\` |
|    - |  418 | `   "function number_format($num, $decimals = 0, $decimal_separator = '.', $thousands_separator = ','){"\` |
|    - |  419 | `   "  if( is_array($num) \|\| is_object($num) \|\| is_resource($num) \|\| $num === null"\` |
|    - |  420 | `   "   \|\| (is_string($num) && !is_numeric($num)) ){"\` |
|    - |  421 | `   "    throw new TypeError('number_format(): Argument #1 ($num) must be of type int\|float, '"\` |
|    - |  422 | `   "      . __php_zpp_type($num) . ' given');"\` |
|    - |  423 | `   "  }"\` |
|    - |  424 | `   "  if( is_array($decimals) \|\| is_object($decimals) \|\| is_resource($decimals) \|\| $decimals === null"\` |
|    - |  425 | `   "   \|\| (is_string($decimals) && !is_numeric($decimals)) ){"\` |
|    - |  426 | `   "    throw new TypeError('number_format(): Argument #2 ($decimals) must be of type int, '"\` |
|    - |  427 | `   "      . __php_zpp_type($decimals) . ' given');"\` |
|    - |  428 | `   "  }"\` |
|    - |  429 | `   "  if( is_float($decimals) && $decimals != (int)$decimals ){"\` |
|    - |  430 | `   "    throw new TypeError('number_format(): Argument #2 ($decimals) must be of type int, float given');"\` |
|    - |  431 | `   "  }"\` |
|    - |  432 | `   "  if( is_array($decimal_separator) \|\| is_resource($decimal_separator)"\` |
|    - |  433 | `   "   \|\| (is_object($decimal_separator) && !method_exists($decimal_separator, '__toString')) ){"\` |
|    - |  434 | `   "    throw new TypeError('number_format(): Argument #3 ($decimal_separator) must be of type ?string, '"\` |
|    - |  435 | `   "      . __php_zpp_type($decimal_separator) . ' given');"\` |
|    - |  436 | `   "  }"\` |
|    - |  437 | `   "  if( is_array($thousands_separator) \|\| is_resource($thousands_separator)"\` |
|    - |  438 | `   "   \|\| (is_object($thousands_separator) && !method_exists($thousands_separator, '__toString')) ){"\` |
|    - |  439 | `   "    throw new TypeError('number_format(): Argument #4 ($thousands_separator) must be of type ?string, '"\` |
|    - |  440 | `   "      . __php_zpp_type($thousands_separator) . ' given');"\` |
|    - |  441 | `   "  }"\` |
|    - |  442 | `   "  $num = (float)$num;"\` |
|    - |  443 | `   "  $decimals = (int)$decimals;"\` |
|    - |  444 | `   "  if( $decimal_separator === null ){ $decimal_separator = '.'; }"\` |
|    - |  445 | `   "  if( $thousands_separator === null ){ $thousands_separator = ','; }"\` |
|    - |  446 | `   "  $decimal_separator = (string)$decimal_separator;"\` |
|    - |  447 | `   "  $thousands_separator = (string)$thousands_separator;"\` |
|    - |  448 | `   "  /* round() first: sprintf uses banker's rounding, php's number_format rounds"\` |
|    - |  449 | `   "   * half AWAY FROM ZERO (number_format(0.5) is '1', not '0'). A NEGATIVE precision"\` |
|    - |  450 | `   "   * rounds to tens/hundreds (php 8: number_format(1.5,-1) is '0'); the displayed"\` |
|    - |  451 | `   "   * value never carries negative decimal places, so round with the real precision"\` |
|    - |  452 | `   "   * but format/append with max(0, $decimals). */"\` |
|    - |  453 | `   "  $num = round($num, $decimals);"\` |
|    - |  454 | `   "  $fdec = $decimals < 0 ? 0 : $decimals;"\` |
|    - |  455 | `   "  $s = sprintf('%.' . $fdec . 'f', $num);"\` |
|    - |  456 | `   "  $neg = false;"\` |
|    - |  457 | `   "  if( substr($s, 0, 1) === '-' ){ $neg = true; $s = substr($s, 1); }"\` |
|    - |  458 | `   "  $parts = explode('.', $s);"\` |
|    - |  459 | `   "  $int = $parts[0];"\` |
|    - |  460 | `   "  $frac = count($parts) > 1 ? $parts[1] : '';"\` |
|    - |  461 | `   "  $out = '';"\` |
|    - |  462 | `   "  $len = strlen($int);"\` |
|    - |  463 | `   "  $c = 0;"\` |
|    - |  464 | `   "  for( $i = $len - 1 ; $i >= 0 ; $i-- ){"\` |
|    - |  465 | `   "    $out = $int[$i] . $out;"\` |
|    - |  466 | `   "    $c++;"\` |
|    - |  467 | `   "    if( $c % 3 === 0 && $i > 0 ){ $out = $thousands_separator . $out; }"\` |
|    - |  468 | `   "  }"\` |
|    - |  469 | `   "  if( $fdec > 0 ){ $out = $out . $decimal_separator . $frac; }"\` |
|    - |  470 | `   "  if( $neg ){ $out = '-' . $out; }"\` |
|    - |  471 | `   "  return $out;"\` |
|    - |  472 | `   "}"\` |
|    - |  473 | `   "function is_nan($num){ $num = (float)$num; return $num != $num; }"\` |
|    - |  474 | `   "function is_infinite($num){ $num = (float)$num; return $num == INF \|\| $num == -INF; }"\` |
|    - |  475 | `   "function is_finite($num){ $num = (float)$num; return !is_nan($num) && !is_infinite($num); }"\` |
|    - |  476 | `   "/* php's version_compare: canonicalise (separators + digit/alpha boundaries all"\` |
|    - |  477 | `   " * become '.'), then compare parts with the special dev<alpha<beta<RC<#<pl ordering. */"\` |
|    - |  478 | `   "function __phl_vcanon($v){"\` |
|    - |  479 | `   "  $v = (string)$v; $len = strlen($v); $out = '';"\` |
|    - |  480 | `   "  for( $i = 0; $i < $len; $i++ ){"\` |
|    - |  481 | `   "   $c = $v[$i]; $rp = $i + 1 < $len ? $v[$i + 1] : '';"\` |
|    - |  482 | `   "   $cd = ($c >= '0' && $c <= '9');"\` |
|    - |  483 | `   "   $ca = $cd \|\| ($c >= 'a' && $c <= 'z') \|\| ($c >= 'A' && $c <= 'Z');"\` |
|    - |  484 | `   "   if( !$ca ){"\` |
|    - |  485 | `   "    /* any non-alphanumeric (., -, _, +, ...) is a separator: emit one '.' */"\` |
|    - |  486 | `   "    if( $out !== '' && substr($out, -1) !== '.' ){ $out .= '.'; }"\` |
|    - |  487 | `   "   }else{"\` |
|    - |  488 | `   "    $out .= $c;"\` |
|    - |  489 | `   "    $rd = ($rp >= '0' && $rp <= '9');"\` |
|    - |  490 | `   "    $ra = $rd \|\| ($rp >= 'a' && $rp <= 'z') \|\| ($rp >= 'A' && $rp <= 'Z');"\` |
|    - |  491 | `   "    if( $rp !== '' && $ra && ($cd !== $rd) ){ $out .= '.'; }"\` |
|    - |  492 | `   "   }"\` |
|    - |  493 | `   "  }"\` |
|    - |  494 | `   "  return explode('.', $out);"\` |
|    - |  495 | `   "}"\` |
|    - |  496 | `   "function __phl_vform($s){"\` |
|    - |  497 | `   "  if( $s === '' ){ return -1; }"\` |
|    - |  498 | `   "  if( ctype_digit($s) ){ return 4; }"\` |
|    - |  499 | `   "  $f = array('dev' => 0, 'alpha' => 1, 'a' => 1, 'beta' => 2, 'b' => 2, 'RC' => 3, 'rc' => 3, 'pl' => 5, 'p' => 5);"\` |
|    - |  500 | `   "  foreach( $f as $name => $ord ){ if( strncmp($s, $name, strlen($name)) === 0 ){ return $ord; } }"\` |
|    - |  501 | `   "  return -1;"\` |
|    - |  502 | `   "}"\` |
|    - |  503 | `   "function version_compare($version1, $version2, $operator = null){"\` |
|    - |  504 | `   "  $v1 = __phl_vcanon($version1); $v2 = __phl_vcanon($version2);"\` |
|    - |  505 | `   "  $n1 = count($v1); $n2 = count($v2); $n = $n1 > $n2 ? $n1 : $n2; $cmp = 0;"\` |
|    - |  506 | `   "  for( $i = 0; $i < $n; $i++ ){"\` |
|    - |  507 | `   "   $a = $i < $n1 ? $v1[$i] : null; $b = $i < $n2 ? $v2[$i] : null;"\` |
|    - |  508 | `   "   if( $a === null ){ $cmp = ctype_digit($b) ? -1 : (4 <=> __phl_vform($b)); }"\` |
|    - |  509 | `   "   elseif( $b === null ){ $cmp = ctype_digit($a) ? 1 : (__phl_vform($a) <=> 4); }"\` |
|    - |  510 | `   "   elseif( ctype_digit($a) && ctype_digit($b) ){ $cmp = (int)$a <=> (int)$b; }"\` |
|    - |  511 | `   "   else{ $cmp = __phl_vform($a) <=> __phl_vform($b); }"\` |
|    - |  512 | `   "   if( $cmp !== 0 ){ break; }"\` |
|    - |  513 | `   "  }"\` |
|    - |  514 | `   "  if( $operator === null ){ return $cmp; }"\` |
|    - |  515 | `   "  switch( (string)$operator ){"\` |
|    - |  516 | `   "   case '<': case 'lt': return $cmp < 0;"\` |
|    - |  517 | `   "   case '<=': case 'le': return $cmp <= 0;"\` |
|    - |  518 | `   "   case '>': case 'gt': return $cmp > 0;"\` |
|    - |  519 | `   "   case '>=': case 'ge': return $cmp >= 0;"\` |
|    - |  520 | `   "   case '==': case '=': case 'eq': return $cmp === 0;"\` |
|    - |  521 | `   "   case '!=': case '<>': case 'ne': return $cmp !== 0;"\` |
|    - |  522 | `   "  }"\` |
|    - |  523 | `   "  return null;"\` |
|    - |  524 | `   "}"\` |
|    - |  525 | `   "/* phl.stub_extensions (a -d/php.ini list, comma-separated) declares extensions"\` |
|    - |  526 | `   " * PHL does not implement as LOADED, backed by no-op behaviour, so software that"\` |
|    - |  527 | `   " * only GATES on extension_loaded() (e.g. PHPUnit's dom/xmlwriter check) runs"\` |
|    - |  528 | `   " * unmodified. It does NOT synthesize the extension's classes/functions. */"\` |
|    - |  529 | `   "function __phl_stub_exts(){"\` |
|    - |  530 | `   "  $s = ini_get('phl.stub_extensions');"\` |
|    - |  531 | `   "  if( $s === false \|\| $s === '' ){ return array(); }"\` |
|    - |  532 | `   "  $out = array();"\` |
|    - |  533 | `   "  foreach( explode(',', (string)$s) as $e ){ $e = trim($e); if( $e !== '' ){ $out[strtolower($e)] = $e; } }"\` |
|    - |  534 | `   "  return $out;"\` |
|    - |  535 | `   "}"\` |
|    - |  536 | `   "function extension_loaded($extension){"\` |
|    - |  537 | `   "  static $ext = array('core' => 1, 'standard' => 1, 'pcre' => 1, 'json' => 1,"\` |
|    - |  538 | `   "   'ctype' => 1, 'date' => 1, 'spl' => 1, 'reflection' => 1, 'mbstring' => 1,"\` |
|    - |  539 | `   "   'hash' => 1, 'filter' => 1, 'session' => 1" PHL_EXT_LOADED_LIBXML ");"\` |
|    - |  540 | `   "  $n = strtolower((string)$extension);"\` |
|    - |  541 | `   "  if( isset($ext[$n]) ){ return true; }"\` |
|    - |  542 | `   "  $stub = __phl_stub_exts();"\` |
|    - |  543 | `   "  return isset($stub[$n]);"\` |
|    - |  544 | `   "}"\` |
|    - |  545 | `   "function get_loaded_extensions($zend_extensions = false){"\` |
|    - |  546 | `   "  if( $zend_extensions ){ return array(); }"\` |
|    - |  547 | `   "  $base = array('Core','date','pcre','SPL','json','standard',"\` |
|    - |  548 | `   "   'ctype','filter','hash','Reflection','session','mbstring'" PHL_EXT_LIST_LIBXML ");"\` |
|    - |  549 | `   "  foreach( __phl_stub_exts() as $e ){ $base[] = $e; }"\` |
|    - |  550 | `   "  return $base;"\` |
|    - |  551 | `   "}"\` |
|    - |  552 | `   "/* Inverse of bin2hex() */"\` |
|    - |  553 | `   "function hex2bin($string){"\` |
|    - |  554 | `   "  $string = (string)$string;"\` |
|    - |  555 | `   "  $len = strlen($string);"\` |
|    - |  556 | `   "  if( $len % 2 !== 0 ){"\` |
|    - |  557 | `   "    trigger_error('hex2bin(): Hexadecimal input string must have an even length', E_USER_WARNING);"\` |
|    - |  558 | `   "    return false;"\` |
|    - |  559 | `   "  }"\` |
|    - |  560 | `   "  $out = '';"\` |
|    - |  561 | `   "  for( $i = 0 ; $i < $len ; $i += 2 ){"\` |
|    - |  562 | `   "    $pair = substr($string, $i, 2);"\` |
|    - |  563 | `   "    if( !ctype_xdigit($pair) ){"\` |
|    - |  564 | `   "      trigger_error('hex2bin(): Input string must be hexadecimal string', E_USER_WARNING);"\` |
|    - |  565 | `   "      return false;"\` |
|    - |  566 | `   "    }"\` |
|    - |  567 | `   "    $out = $out . chr(hexdec($pair));"\` |
|    - |  568 | `   "  }"\` |
|    - |  569 | `   "  return $out;"\` |
|    - |  570 | `   "}"\` |
|    - |  571 | `   "/* Division that never throws: INF/-INF/NAN like php */"\` |
|    - |  572 | `   "function fdiv($num1, $num2){"\` |
|    - |  573 | `   "  $num1 = (float)$num1;"\` |
|    - |  574 | `   "  $num2 = (float)$num2;"\` |
|    - |  575 | `   "  if( $num2 == 0.0 ){"\` |
|    - |  576 | `   "    if( $num1 == 0.0 \|\| is_nan($num1) ){ return NAN; }"\` |
|    - |  577 | `   "    return $num1 > 0 ? INF : -INF;"\` |
|    - |  578 | `   "  }"\` |
|    - |  579 | `   "  return $num1 / $num2;"\` |
|    - |  580 | `   "}"\` |
|    - |  581 | `   "function checkdate($month, $day, $year){"\` |
|    - |  582 | `   "  $month = (int)$month; $day = (int)$day; $year = (int)$year;"\` |
|    - |  583 | `   "  if( $month < 1 \|\| $month > 12 \|\| $year < 1 \|\| $year > 32767 \|\| $day < 1 ){ return false; }"\` |
|    - |  584 | `   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\` |
|    - |  585 | `   "  $max = $days[$month - 1];"\` |
|    - |  586 | `   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) \|\| ($year % 400 === 0)) ){"\` |
|    - |  587 | `   "    $max = 29;"\` |
|    - |  588 | `   "  }"\` |
|    - |  589 | `   "  return $day <= $max;"\` |
|    - |  590 | `   "}"\` |
|    - |  591 | `   "function is_iterable($value){ return is_array($value) \|\| ($value instanceof Traversable); }"\` |
|    - |  592 | `   "function is_countable($value){ return is_array($value) \|\| ($value instanceof Countable); }"\` |
|    - |  593 | `   "function doubleval($value){ return (float)$value; }"\` |
|    - |  594 | `   "function array_count_values($array){"\` |
|    - |  595 | `   "  $out = array();"\` |
|    - |  596 | `   "  foreach( $array as $v ){"\` |
|    - |  597 | `   "    if( !is_int($v) && !is_string($v) ){"\` |
|    - |  598 | `   "      trigger_error('array_count_values(): Can only count string and integer values, entry skipped', E_USER_WARNING);"\` |
|    - |  599 | `   "      continue;"\` |
|    - |  600 | `   "    }"\` |
|    - |  601 | `   "    if( isset($out[$v]) ){ $out[$v] = $out[$v] + 1; } else { $out[$v] = 1; }"\` |
|    - |  602 | `   "  }"\` |
|    - |  603 | `   "  return $out;"\` |
|    - |  604 | `   "}"\` |
|    - |  605 | `   "function array_change_key_case($array, $case = CASE_LOWER){"\` |
|    - |  606 | `   "  $out = array();"\` |
|    - |  607 | `   "  foreach( $array as $k => $v ){"\` |
|    - |  608 | `   "    if( is_string($k) ){ $k = ($case == CASE_UPPER) ? strtoupper($k) : strtolower($k); }"\` |
|    - |  609 | `   "    $out[$k] = $v;"\` |
|    - |  610 | `   "  }"\` |
|    - |  611 | `   "  return $out;"\` |
|    - |  612 | `   "}"\` |
|    - |  613 | `   "function array_replace_recursive($array, ...$replacements){"\` |
|    - |  614 | `   "  foreach( $replacements as $o ){"\` |
|    - |  615 | `   "    foreach( $o as $k => $v ){"\` |
|    - |  616 | `   "      if( is_array($v) && isset($array[$k]) && is_array($array[$k]) ){"\` |
|    - |  617 | `   "        $array[$k] = array_replace_recursive($array[$k], $v);"\` |
|    - |  618 | `   "      }else{"\` |
|    - |  619 | `   "        $array[$k] = $v;"\` |
|    - |  620 | `   "      }"\` |
|    - |  621 | `   "    }"\` |
|    - |  622 | `   "  }"\` |
|    - |  623 | `   "  return $array;"\` |
|    - |  624 | `   "}"\` |
|    - |  625 | `   "function class_uses($object_or_class, $autoload = true){"\` |
|    - |  626 | `   "  $c = is_object($object_or_class) ? get_class($object_or_class) : (string)$object_or_class;"\` |
|    - |  627 | `   "  if( !class_exists($c) ){ return false; }"\` |
|    - |  628 | `   "  return array();  /* PHL has no traits yet -- always the empty set */"\` |
|    - |  629 | `   "}"\` |
|    - |  630 | `   /* count_chars: php's five modes are a 2x2 split plus mode 0 -- 1/2 answer an \` |
|    - |  631 | `    * ARRAY, 3/4 a STRING, and the ODD modes report the bytes that WERE used \` |
|    - |  632 | `    * where the EVEN ones report the bytes that were NOT. PH7 implemented 0/1/3 \` |
|    - |  633 | `    * and let 2 and 4 fall through to mode 0's full table: the exact complement \` |
|    - |  634 | `    * of the answer asked for, silently. Any other mode is php's ValueError. */\` |
|    - |  635 | `   "function count_chars($string, $mode = 0){"\` |
|    - |  636 | `   "  if( is_array($mode) \|\| is_object($mode) \|\| is_resource($mode)"\` |
|    - |  637 | `   "   \|\| (is_string($mode) && !is_numeric($mode)) ){"\` |
|    - |  638 | `   "    throw new TypeError('count_chars(): Argument #2 ($mode) must be of type int, ' . __php_zpp_type($mode) . ' given');"\` |
|    - |  639 | `   "  }"\` |
|    - |  640 | `   "  $mode = (int)$mode;"\` |
|    - |  641 | `   "  if( $mode < 0 \|\| $mode > 4 ){"\` |
|    - |  642 | `   "    throw new ValueError('count_chars(): Argument #2 ($mode) must be between 0 and 4 (inclusive)');"\` |
|    - |  643 | `   "  }"\` |
|    - |  644 | `   "  $string = (string)$string;"\` |
|    - |  645 | `   "  $counts = array();"\` |
|    - |  646 | `   "  for( $i = 0 ; $i < 256 ; $i++ ){ $counts[$i] = 0; }"\` |
|    - |  647 | `   "  $len = strlen($string);"\` |
|    - |  648 | `   "  for( $i = 0 ; $i < $len ; $i++ ){ $b = ord($string[$i]); $counts[$b] = $counts[$b] + 1; }"\` |
|    - |  649 | `   "  if( $mode == 0 ){ return $counts; }"\` |
|    - |  650 | `   "  $used = ($mode == 1 \|\| $mode == 3);"\` |
|    - |  651 | `   "  $asStr = ($mode == 3 \|\| $mode == 4);"\` |
|    - |  652 | `   "  $out = $asStr ? '' : array();"\` |
|    - |  653 | `   "  foreach( $counts as $b => $n ){"\` |
|    - |  654 | `   "    if( ($n > 0) !== $used ){ continue; }"\` |
|    - |  655 | `   "    if( $asStr ){ $out = $out . chr($b); } else { $out[$b] = $n; }"\` |
|    - |  656 | `   "  }"\` |
|    - |  657 | `   "  return $out;"\` |
|    - |  658 | `   "}"\` |
|    - |  659 | `   "function ip2long($ip){"\` |
|    - |  660 | `   "  $p = explode('.', (string)$ip);"\` |
|    - |  661 | `   "  if( count($p) !== 4 ){ return false; }"\` |
|    - |  662 | `   "  $n = 0;"\` |
|    - |  663 | `   "  foreach( $p as $o ){"\` |
|    - |  664 | `   "    if( !ctype_digit($o) \|\| (int)$o < 0 \|\| (int)$o > 255 ){ return false; }"\` |
|    - |  665 | `   "    $n = $n * 256 + (int)$o;"\` |
|    - |  666 | `   "  }"\` |
|    - |  667 | `   "  return $n;"\` |
|    - |  668 | `   "}"\` |
|    - |  669 | `   "function long2ip($ip){"\` |
|    - |  670 | `   "  $n = (int)$ip;"\` |
|    - |  671 | `   "  return (($n >> 24) & 255) . '.' . (($n >> 16) & 255) . '.' . (($n >> 8) & 255) . '.' . ($n & 255);"\` |
|    - |  672 | `   "}"\` |
|    - |  673 | `   "function preg_filter($pattern, $replacement, $subject, $limit = -1, &$count = null){"\` |
|    - |  674 | `   "  /* php declares &$count and always writes it -- the total number of"\` |
|    - |  675 | `   "   * replacements across every subject, 0 when nothing matched. PHL never"\` |
|    - |  676 | `   "   * declared the parameter, so a caller reading it got its previous value. */"\` |
|    - |  677 | `   "  if( is_array($subject) ){"\` |
|    - |  678 | `   "    $total = 0;"\` |
|    - |  679 | `   "    $out = array();"\` |
|    - |  680 | `   "    foreach( $subject as $k => $v ){"\` |
|    - |  681 | `   "      $r = preg_replace($pattern, $replacement, (string)$v, $limit, $cnt);"\` |
|    - |  682 | `   "      $total = $total + $cnt;"\` |
|    - |  683 | `   "      if( $cnt > 0 ){ $out[$k] = $r; }"\` |
|    - |  684 | `   "    }"\` |
|    - |  685 | `   "    $count = $total;"\` |
|    - |  686 | `   "    return $out;"\` |
|    - |  687 | `   "  }"\` |
|    - |  688 | `   "  $r = preg_replace($pattern, $replacement, (string)$subject, $limit, $cnt);"\` |
|    - |  689 | `   "  $count = $cnt;"\` |
|    - |  690 | `   "  return $cnt > 0 ? $r : null;"\` |
|    - |  691 | `   "}"\` |
|    - |  692 | `   "function preg_replace_callback_array($pattern, $subject, $limit = -1, &$count = null, $flags = 0){"\` |
|    - |  693 | `   "  /* &$count is the total across every pattern; $flags shapes each callback's"\` |
|    - |  694 | `   "   * match array. php writes &$count only when the whole run SUCCEEDED -- a"\` |
|    - |  695 | `   "   * pattern that fails to compile answers null and leaves it untouched (an"\` |
|    - |  696 | `   "   * array subject is not a failure: it degrades to the empty array, count 0). */"\` |
|    - |  697 | `   "  $total = 0;"\` |
|    - |  698 | `   "  foreach( $pattern as $pat => $cb ){"\` |
|    - |  699 | `   "    $subject = preg_replace_callback($pat, $cb, $subject, $limit, $cnt, $flags);"\` |
|    - |  700 | `   "    if( $subject === null ){ return null; }"\` |
|    - |  701 | `   "    $total = $total + $cnt;"\` |
|    - |  702 | `   "  }"\` |
|    - |  703 | `   "  $count = $total;"\` |
|    - |  704 | `   "  return $subject;"\` |
|    - |  705 | `   "}"\` |
|    - |  706 | `   "function cal_days_in_month($calendar, $month, $year){"\` |
|    - |  707 | `   "  $month = (int)$month; $year = (int)$year;"\` |
|    - |  708 | `   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\` |
|    - |  709 | `   "  if( $month < 1 \|\| $month > 12 ){"\` |
|    - |  710 | `   "    throw new ValueError('cal_days_in_month(): Argument #2 ($month) must be a valid month');"\` |
|    - |  711 | `   "  }"\` |
|    - |  712 | `   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) \|\| ($year % 400 === 0)) ){"\` |
|    - |  713 | `   "    return 29;"\` |
|    - |  714 | `   "  }"\` |
|    - |  715 | `   "  return $days[$month - 1];"\` |
|    - |  716 | `   "}"\` |
|    - |  717 | `   "function preg_grep($pattern, $array, $flags = 0){"\` |
|    - |  718 | `   "  $out = array();"\` |
|    - |  719 | `   "  foreach( $array as $k => $v ){"\` |
|    - |  720 | `   "    $m = preg_match($pattern, (string)$v);"\` |
|    - |  721 | `   "    if( $flags & PREG_GREP_INVERT ){ $m = !$m; }"\` |
|    - |  722 | `   "    if( $m ){ $out[$k] = $v; }"\` |
|    - |  723 | `   "  }"\` |
|    - |  724 | `   "  return $out;"\` |
|    - |  725 | `   "}"\` |
|    - |  726 | `   "function class_implements($object_or_class, $autoload = true){"\` |
|    - |  727 | `   "  $c = is_object($object_or_class) ? get_class($object_or_class) : (string)$object_or_class;"\` |
|    - |  728 | `   "  if( !class_exists($c) && !interface_exists($c) ){ return false; }"\` |
|    - |  729 | `   "  $out = array();"\` |
|    - |  730 | `   "  $r = new ReflectionClass($c);"\` |
|    - |  731 | `   "  foreach( $r->getInterfaceNames() as $i ){ $out[$i] = $i; }"\` |
|    - |  732 | `   "  return $out;"\` |
|    - |  733 | `   "}"\` |
|    - |  734 | `   "function class_parents($object_or_class, $autoload = true){"\` |
|    - |  735 | `   "  $c = is_object($object_or_class) ? get_class($object_or_class) : (string)$object_or_class;"\` |
|    - |  736 | `   "  if( !class_exists($c) ){ return false; }"\` |
|    - |  737 | `   "  $out = array();"\` |
|    - |  738 | `   "  $r = new ReflectionClass($c);"\` |
|    - |  739 | `   "  while( ($p = $r->getParentClass()) ){"\` |
|    - |  740 | `   "    $n = $p->getName();"\` |
|    - |  741 | `   "    $out[$n] = $n;"\` |
|    - |  742 | `   "    $r = $p;"\` |
|    - |  743 | `   "  }"\` |
|    - |  744 | `   "  return $out;"\` |
|    - |  745 | `   "}"\` |
|    - |  746 | `   "/* php's http_build_query() -- missing from PH7. Skips null values, casts"\` |
|    - |  747 | `   " * bool to 1/0, prefixes numeric top-level keys, urlencodes per RFC. */"\` |
|    - |  748 | `   "function __phl_hbq_enc($s, $enc){"\` |
|    - |  749 | `   "  return $enc == PHP_QUERY_RFC3986 ? rawurlencode((string)$s) : urlencode((string)$s);"\` |
|    - |  750 | `   "}"\` |
|    - |  751 | `   "function __phl_hbq(&$pairs, $data, $key_prefix, $numeric_prefix, $sep, $enc){"\` |
|    - |  752 | `   "  foreach( $data as $k => $v ){"\` |
|    - |  753 | `   "    if( $v === null ){ continue; }"\` |
|    - |  754 | `   "    if( $key_prefix === '' ){"\` |
|    - |  755 | `   "      $ek = is_int($k) ? __phl_hbq_enc($numeric_prefix . $k, $enc) : __phl_hbq_enc($k, $enc);"\` |
|    - |  756 | `   "    } else {"\` |
|    - |  757 | `   "      $ek = $key_prefix . '%5B' . __phl_hbq_enc($k, $enc) . '%5D';"\` |
|    - |  758 | `   "    }"\` |
|    - |  759 | `   "    if( is_array($v) ){"\` |
|    - |  760 | `   "      __phl_hbq($pairs, $v, $ek, $numeric_prefix, $sep, $enc);"\` |
|    - |  761 | `   "    } elseif( is_object($v) ){"\` |
|    - |  762 | `   "      __phl_hbq($pairs, get_object_vars($v), $ek, $numeric_prefix, $sep, $enc);"\` |
|    - |  763 | `   "    } else {"\` |
|    - |  764 | `   "      if( $v === true ){ $v = '1'; } elseif( $v === false ){ $v = '0'; }"\` |
|    - |  765 | `   "      $pairs[] = $ek . '=' . __phl_hbq_enc($v, $enc);"\` |
|    - |  766 | `   "    }"\` |
|    - |  767 | `   "  }"\` |
|    - |  768 | `   "}"\` |
|    - |  769 | `   "function http_build_query($data, $numeric_prefix = '', $arg_separator = null, $encoding_type = PHP_QUERY_RFC1738){"\` |
|    - |  770 | `   "  if( !is_array($data) && !is_object($data) ){"\` |
|    - |  771 | `   "    throw new TypeError('http_build_query(): Argument #1 ($data) must be of type array, ' . __php_zpp_type($data) . ' given');"\` |
|    - |  772 | `   "  }"\` |
|    - |  773 | `   "  if( $arg_separator === null ){ $arg_separator = '&'; }"\` |
|    - |  774 | `   "  $pairs = array();"\` |
|    - |  775 | `   "  __phl_hbq($pairs, is_object($data) ? get_object_vars($data) : $data, '', (string)$numeric_prefix, $arg_separator, $encoding_type);"\` |
|    - |  776 | `   "  return implode($arg_separator, $pairs);"\` |
|    - |  777 | `   "}"\` |
|    - |  778 | `   "/* php's parse_str() -- missing from PH7. Mangles the base name ('.'/' ' -> '_'),"\` |
|    - |  779 | `   " * parses [key] nesting and [] appends, urldecodes keys and values. */"\` |
|    - |  780 | `   "function __phl_parsestr_assign(&$arr, $segments, $i, $val){"\` |
|    - |  781 | `   "  $seg = $segments[$i];"\` |
|    - |  782 | `   "  $last = ($i === count($segments) - 1);"\` |
|    - |  783 | `   "  if( $seg === '' ){"\` |
|    - |  784 | `   "    if( $last ){ $arr[] = $val; return; }"\` |
|    - |  785 | `   "    $arr[] = array();"\` |
|    - |  786 | `   "    $k = array_key_last($arr);"\` |
|    - |  787 | `   "    __phl_parsestr_assign($arr[$k], $segments, $i + 1, $val);"\` |
|    - |  788 | `   "  } else {"\` |
|    - |  789 | `   "    if( $last ){ $arr[$seg] = $val; return; }"\` |
|    - |  790 | `   "    if( !isset($arr[$seg]) \|\| !is_array($arr[$seg]) ){ $arr[$seg] = array(); }"\` |
|    - |  791 | `   "    __phl_parsestr_assign($arr[$seg], $segments, $i + 1, $val);"\` |
|    - |  792 | `   "  }"\` |
|    - |  793 | `   "}"\` |
|    - |  794 | `   "function parse_str($string, &$result){"\` |
|    - |  795 | `   "  $result = array();"\` |
|    - |  796 | `   "  $string = (string)$string;"\` |
|    - |  797 | `   "  if( $string === '' ){ return; }"\` |
|    - |  798 | `   "  foreach( explode('&', $string) as $pair ){"\` |
|    - |  799 | `   "    if( $pair === '' ){ continue; }"\` |
|    - |  800 | `   "    $eq = strpos($pair, '=');"\` |
|    - |  801 | `   "    if( $eq === false ){ $rawkey = $pair; $val = ''; }"\` |
|    - |  802 | `   "    else { $rawkey = substr($pair, 0, $eq); $val = urldecode(substr($pair, $eq + 1)); }"\` |
|    - |  803 | `   "    if( $rawkey === '' ){ continue; }"\` |
|    - |  804 | `   "    $bpos = strpos($rawkey, '[');"\` |
|    - |  805 | `   "    if( $bpos === false ){ $base = $rawkey; $subs = array(); }"\` |
|    - |  806 | `   "    else {"\` |
|    - |  807 | `   "      $base = substr($rawkey, 0, $bpos);"\` |
|    - |  808 | `   "      preg_match_all('/\\[([^\\]]*)\\]/', substr($rawkey, $bpos), $m);"\` |
|    - |  809 | `   "      $subs = $m[1];"\` |
|    - |  810 | `   "    }"\` |
|    - |  811 | `   "    $base = str_replace(array(' ', '.'), '_', urldecode($base));"\` |
|    - |  812 | `   "    if( $base === '' ){ continue; }"\` |
|    - |  813 | `   "    $segs = array($base);"\` |
|    - |  814 | `   "    foreach( $subs as $s ){ $segs[] = urldecode($s); }"\` |
|    - |  815 | `   "    __phl_parsestr_assign($result, $segs, 0, $val);"\` |
|    - |  816 | `   "  }"\` |
|    - |  817 | `   "}"\` |
|    - |  818 | `   "/* php 8.3 str_increment(): Perl-style alphanumeric increment. */"\` |
|    - |  819 | `   "function str_increment($string){"\` |
|    - |  820 | `   "  $string = (string)$string;"\` |
|    - |  821 | `   "  if( $string === '' ){ throw new ValueError('str_increment(): Argument #1 ($string) must not be empty'); }"\` |
|    - |  822 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_increment(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|    - |  823 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|    - |  824 | `   "    $c = $string[$i];"\` |
|    - |  825 | `   "    if( $c === 'z' ){ $string[$i] = 'a'; }"\` |
|    - |  826 | `   "    elseif( $c === 'Z' ){ $string[$i] = 'A'; }"\` |
|    - |  827 | `   "    elseif( $c === '9' ){ $string[$i] = '0'; }"\` |
|    - |  828 | `   "    else { $string[$i] = chr(ord($c) + 1); return $string; }"\` |
|    - |  829 | `   "  }"\` |
|    - |  830 | `   "  $first = $string[0];"\` |
|    - |  831 | `   "  if( $first === '0' ){ return '1' . $string; }"\` |
|    - |  832 | `   "  if( $first === 'a' ){ return 'a' . $string; }"\` |
|    - |  833 | `   "  return 'A' . $string;"\` |
|    - |  834 | `   "}"\` |
|    - |  835 | `   "/* php 8.3 str_decrement(): inverse of str_increment(); throws out of range"\` |
|    - |  836 | `   " * at the bottom of the counting sequence. */"\` |
|    - |  837 | `   "function str_decrement($string){"\` |
|    - |  838 | `   "  $string = (string)$string;"\` |
|    - |  839 | `   "  if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) must not be empty'); }"\` |
|    - |  840 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_decrement(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|    - |  841 | `   "  $orig = $string;"\` |
|    - |  842 | `   "  $borrowed = false;"\` |
|    - |  843 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|    - |  844 | `   "    $c = $string[$i];"\` |
|    - |  845 | `   "    if( $c === 'a' ){ $string[$i] = 'z'; }"\` |
|    - |  846 | `   "    elseif( $c === 'A' ){ $string[$i] = 'Z'; }"\` |
|    - |  847 | `   "    elseif( $c === '0' ){ $string[$i] = '9'; }"\` |
|    - |  848 | `   "    else { $string[$i] = chr(ord($c) - 1); $borrowed = false; break; }"\` |
|    - |  849 | `   "    if( $i === 0 ){ $borrowed = true; }"\` |
|    - |  850 | `   "  }"\` |
|    - |  851 | `   "  if( $borrowed ){"\` |
|    - |  852 | `   "    if( $string[0] === '9' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|    - |  853 | `   "    $string = substr($string, 1);"\` |
|    - |  854 | `   "    if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|    - |  855 | `   "  } elseif( strlen($string) > 1 && $string[0] === '0' ){"\` |
|    - |  856 | `   "    $string = substr($string, 1);"\` |
|    - |  857 | `   "  }"\` |
|    - |  858 | `   "  return $string;"\` |
|    - |  859 | `   "}"\` |
|    - |  860 | `   "/* Permission bits via stat(); false + warning when stat fails, like php. */"\` |
|    - |  861 | `   "function fileperms($filename){"\` |
|    - |  862 | `   "  $s = @stat($filename);"\` |
|    - |  863 | `   "  if( $s === false ){"\` |
|    - |  864 | `   "    trigger_error('fileperms(): stat failed for ' . $filename, E_USER_WARNING);"\` |
|    - |  865 | `   "    return false;"\` |
|    - |  866 | `   "  }"\` |
|    - |  867 | `   "  return $s['mode'];"\` |
|    - |  868 | `   "}"\` |
|    - |  869 | `   "/* PH7 keeps no stat cache, so this is a no-op like php on a clean cache. */"\` |
|    - |  870 | `   "function clearstatcache($clear_realpath_cache = false, $filename = ''){}"\` |
|    - |  871 | `   "/* php 8.4 mb_ucfirst/mb_lcfirst: case-map only the first multibyte char. */"\` |
|    - |  872 | `   "function mb_ucfirst($string, $encoding = null){"\` |
|    - |  873 | `   "  $string = (string)$string;"\` |
|    - |  874 | `   "  if( $string === '' ){ return ''; }"\` |
|    - |  875 | `   "  return mb_strtoupper(mb_substr($string, 0, 1)) . mb_substr($string, 1);"\` |
|    - |  876 | `   "}"\` |
|    - |  877 | `   "function mb_lcfirst($string, $encoding = null){"\` |
|    - |  878 | `   "  $string = (string)$string;"\` |
|    - |  879 | `   "  if( $string === '' ){ return ''; }"\` |
|    - |  880 | `   "  return mb_strtolower(mb_substr($string, 0, 1)) . mb_substr($string, 1);"\` |
|    - |  881 | `   "}"\` |
|    - |  882 | `   "/* php 8.4 mb_trim family: strip leading/trailing characters (whole"\` |
|    - |  883 | `   " * multibyte chars, NO range syntax), defaulting to php's Unicode"\` |
|    - |  884 | `   " * whitespace set. */"\` |
|    - |  885 | `   "function __phl_mb_ws(){"\` |
|    - |  886 | `   "  static $set = null;"\` |
|    - |  887 | `   "  if( $set === null ){"\` |
|    - |  888 | `   "    $set = array();"\` |
|    - |  889 | `   "    foreach( array(0x00,0x09,0x0A,0x0B,0x0C,0x0D,0x20,0x85,0xA0,0x1680,"\` |
|    - |  890 | `   "      0x180E,0x2000,0x2001,0x2002,0x2003,0x2004,0x2005,0x2006,0x2007,0x2008,"\` |
|    - |  891 | `   "      0x2009,0x200A,0x2028,0x2029,0x202F,0x205F,0x3000) as $cp ){"\` |
|    - |  892 | `   "      $set[mb_chr($cp)] = true;"\` |
|    - |  893 | `   "    }"\` |
|    - |  894 | `   "  }"\` |
|    - |  895 | `   "  return $set;"\` |
|    - |  896 | `   "}"\` |
|    - |  897 | `   "function __phl_mb_trim($string, $characters, $left, $right){"\` |
|    - |  898 | `   "  $string = (string)$string;"\` |
|    - |  899 | `   "  if( $string === '' ){ return ''; }"\` |
|    - |  900 | `   "  if( $characters === null ){"\` |
|    - |  901 | `   "    $set = __phl_mb_ws();"\` |
|    - |  902 | `   "  } else {"\` |
|    - |  903 | `   "    $set = array();"\` |
|    - |  904 | `   "    foreach( mb_str_split((string)$characters) as $c ){ $set[$c] = true; }"\` |
|    - |  905 | `   "  }"\` |
|    - |  906 | `   "  $chars = mb_str_split($string);"\` |
|    - |  907 | `   "  $n = count($chars);"\` |
|    - |  908 | `   "  $i = 0; $j = $n;"\` |
|    - |  909 | `   "  if( $left ){ while( $i < $j && isset($set[$chars[$i]]) ){ $i++; } }"\` |
|    - |  910 | `   "  if( $right ){ while( $j > $i && isset($set[$chars[$j - 1]]) ){ $j--; } }"\` |
|    - |  911 | `   "  return implode('', array_slice($chars, $i, $j - $i));"\` |
|    - |  912 | `   "}"\` |
|    - |  913 | `   "function mb_trim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, true, true); }"\` |
|    - |  914 | `   "function mb_ltrim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, true, false); }"\` |
|    - |  915 | `   "function mb_rtrim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, false, true); }"\` |
|    - |  916 | `   "/* Creates a temporary file and returns its name */"\` |
|    - |  917 | `   "function tempnam(string $directory,string $prefix)"\` |
|    - |  918 | `   "{"\` |
|    - |  919 | `   "   /* php CREATES the file (empty, mode 0600) and guarantees the name is unique --"\` |
|    - |  920 | `   "    * returning a bare name left the caller with a path that does not exist, so"\` |
|    - |  921 | `   "    * file_exists() was false and unlink() failed on it. */"\` |
|    - |  922 | `   "   $directory = rtrim($directory, DIRECTORY_SEPARATOR);"\` |
|    - |  923 | `   "   for( $i = 0 ; $i < 64 ; ++$i ){"\` |
|    - |  924 | `   "     $zPath = $directory.DIRECTORY_SEPARATOR.$prefix.rand_str(12);"\` |
|    - |  925 | `   "     if( file_exists($zPath) ){ continue; }"\` |
|    - |  926 | `   "     $pHandle = @fopen($zPath,'x');"\` |
|    - |  927 | `   "     if( $pHandle === false ){ continue; }"\` |
|    - |  928 | `   "     fclose($pHandle);"\` |
|    - |  929 | `   "     @chmod($zPath, 0600);"\` |
|    - |  930 | `   "     return $zPath;"\` |
|    - |  931 | `   "   }"\` |
|    - |  932 | `   "   return false;"\` |
|    - |  933 | `   "}"\` |
|    - |  934 | `   "function array_unshift(&$array, ...$values){"\` |
|    - |  935 | `   " if( !is_array($array) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . __php_zpp_type($array) . ' given'); }"\` |
|    - |  936 | `   "/* Copy arguments */"\` |
|    - |  937 | `   "$pNew = $values;"\` |
|    - |  938 | `   	"/* Make a copy of the old entries */"\` |
|    - |  939 | `	"$pOld = array_copy($array);"\` |
|    - |  940 | `	"/* Erase */"\` |
|    - |  941 | `	"array_erase($array);"\` |
|    - |  942 | `	"/* Unshift */"\` |
|    - |  943 | `	"$array = array_merge($pNew,$pOld);"\` |
|    - |  944 | `	"return sizeof($array);"\` |
|    - |  945 | `    "}"\` |
|    - |  946 | `	"function array_merge_recursive(...$arrays){"\` |
|    - |  947 | `    "$narrays = count($arrays);"\` |
|    - |  948 | `    "$ret = array();"\` |
|    - |  949 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|    - |  950 | `	 " if( !is_array($arrays[$i]) ){"\` |
|    - |  951 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.__php_zpp_type($arrays[$i]).' given');"\` |
|    - |  952 | `	 " }"\` |
|    - |  953 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|    - |  954 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|    - |  955 | `     "  if( $keyIsInt ) {"\` |
|    - |  956 | `     "   $ret[] = $value;"\` |
|    - |  957 | `     "  } else {"\` |
|    - |  958 | `     "   if (array_key_exists($key, $ret)) {"\` |
|    - |  959 | `     "    $cur = $ret[$key];"\` |
|    - |  960 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|    - |  961 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|    - |  962 | `     "    } elseif (is_array($cur)) {"\` |
|    - |  963 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|    - |  964 | `     "    } elseif (is_array($value)) {"\` |
|    - |  965 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|    - |  966 | `     "    } else {"\` |
|    - |  967 | `     "     $ret[$key] = array($cur, $value);"\` |
|    - |  968 | `     "    }"\` |
|    - |  969 | `     "   } else {"\` |
|    - |  970 | `     "    $ret[$key] = $value;"\` |
|    - |  971 | `     "   }"\` |
|    - |  972 | `     "  }"\` |
|    - |  973 | `     " }"\` |
|    - |  974 | `	 " }"\` |
|    - |  975 | `	 " return $ret;"\` |
|    - |  976 | `    "}"\` |
|    - |  977 | `	/* __php_zpp_type: php's ZPP value-name for TypeError messages */\` |
|    - |  978 | `	"function __php_zpp_type($v){"\` |
|    - |  979 | `	" if( is_object($v) ){ return get_class($v); }"\` |
|    - |  980 | `	" if( is_int($v) ){ return 'int'; }"\` |
|    - |  981 | `	" if( is_float($v) ){ return 'float'; }"\` |
|    - |  982 | `	" if( is_string($v) ){ return 'string'; }"\` |
|    - |  983 | `	" if( is_bool($v) ){ return $v ? 'true' : 'false'; }"\` |
|    - |  984 | `	" if( is_null($v) ){ return 'null'; }"\` |
|    - |  985 | `	" if( is_array($v) ){ return 'array'; }"\` |
|    - |  986 | `	" if( is_resource($v) ){ return 'resource'; }"\` |
|    - |  987 | `	" return 'mixed';"\` |
|    - |  988 | `	"}"\` |
|    - |  989 | `	"function max($value, ...$values){"\` |
|    - |  990 | `    "  $pArgs = func_get_args();"\` |
|    - |  991 | `    " if( sizeof($pArgs) < 2 ){"\` |
|    - |  992 | `    " $pArg = $pArgs[0];"\` |
|    - |  993 | `	" if( !is_array($pArg) ){"\` |
|    - |  994 | `	"   throw new TypeError('max(): Argument #1 ($value) must be of type array, ' . __php_zpp_type($pArg) . ' given');"\` |
|    - |  995 | `	" }"\` |
|    - |  996 | `	" if( sizeof($pArg) < 1 ){"\` |
|    - |  997 | `	"   throw new ValueError('max(): Argument #1 ($value) must contain at least one element');"\` |
|    - |  998 | `	" }"\` |
|    - |  999 | `	" $max = null; $first = true;"\` |
|    - | 1000 | `	" foreach( $pArgs[0] as $val ){"\` |
|    - | 1001 | `	"   if( $first ){ $max = $val; $first = false; }"\` |
|    - | 1002 | `	"   else if( $val > $max ){ $max = $val; }"\` |
|    - | 1003 | `	" }"\` |
|    - | 1004 | `	" return $max;"\` |
|    - | 1005 | `    " }"\` |
|    - | 1006 | `    " $max = $pArgs[0];"\` |
|    - | 1007 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|    - | 1008 | `    " $val = $pArgs[$i];"\` |
|    - | 1009 | `	"if( $val > $max ){"\` |
|    - | 1010 | `	" $max = $val;"\` |
|    - | 1011 | `	"}"\` |
|    - | 1012 | `    " }"\` |
|    - | 1013 | `	" return $max;"\` |
|    - | 1014 | `    "}"\` |
|    - | 1015 | `	"function min($value, ...$values){"\` |
|    - | 1016 | `    "  $pArgs = func_get_args();"\` |
|    - | 1017 | `    " if( sizeof($pArgs) < 2 ){"\` |
|    - | 1018 | `    " $pArg = $pArgs[0];"\` |
|    - | 1019 | `	" if( !is_array($pArg) ){"\` |
|    - | 1020 | `	"   throw new TypeError('min(): Argument #1 ($value) must be of type array, ' . __php_zpp_type($pArg) . ' given');"\` |
|    - | 1021 | `	" }"\` |
|    - | 1022 | `	" if( sizeof($pArg) < 1 ){"\` |
|    - | 1023 | `	"   throw new ValueError('min(): Argument #1 ($value) must contain at least one element');"\` |
|    - | 1024 | `	" }"\` |
|    - | 1025 | `	" $min = null; $first = true;"\` |
|    - | 1026 | `	" foreach( $pArgs[0] as $val ){"\` |
|    - | 1027 | `	"   if( $first ){ $min = $val; $first = false; }"\` |
|    - | 1028 | `	"   else if( $val < $min ){ $min = $val; }"\` |
|    - | 1029 | `	" }"\` |
|    - | 1030 | `	" return $min;"\` |
|    - | 1031 | `    " }"\` |
|    - | 1032 | `    " $min = $pArgs[0];"\` |
|    - | 1033 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|    - | 1034 | `    " $val = $pArgs[$i];"\` |
|    - | 1035 | `	"if( $val < $min ){"\` |
|    - | 1036 | `	" $min = $val;"\` |
|    - | 1037 | `	" }"\` |
|    - | 1038 | `    " }"\` |
|    - | 1039 | `	" return $min;"\` |
|    - | 1040 | `	"}"\` |
|    - | 1041 | `	"function fileowner(string $filename){"\` |
|    - | 1042 | `    " $a = stat($filename);"\` |
|    - | 1043 | `	" if( !is_array($a) ){"\` |
|    - | 1044 | `	"	return false;"\` |
|    - | 1045 | `	" }"\` |
|    - | 1046 | `	" return $a['uid'];"\` |
|    - | 1047 | `    "}"\` |
|    - | 1048 | `    "function filegroup(string $filename){"\` |
|    - | 1049 | `	" $a = stat($filename);"\` |
|    - | 1050 | `	" if( !is_array($a) ){"\` |
|    - | 1051 | `	"	return false;"\` |
|    - | 1052 | `	" }"\` |
|    - | 1053 | `	" return $a['gid'];"\` |
|    - | 1054 | `    "}"\` |
|    - | 1055 | `	 "function fileinode(string $filename){"\` |
|    - | 1056 | `	" $a = stat($filename);"\` |
|    - | 1057 | `	" if( !is_array($a) ){"\` |
|    - | 1058 | `	"	return false;"\` |
|    - | 1059 | `	" }"\` |
|    - | 1060 | `	" return $a['ino'];"\` |
|    - | 1061 | `    "}"` |
|    - | 1062 |  |
| 4528 | 1063 | `PH7_PRIVATE sxi32 PH7_VmInstallBuiltinLib(ph7_vm *pVm)` |
|    5 | 1064 | `{` |
|    - | 1065 | `	SyString sBuiltin;` |
|    - | 1066 | `	SyString sRandom;` |
| 4533 | 1067 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|    - | 1068 | `	/* Compile the built-in library */` |
| 4533 | 1069 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|    - | 1070 | `	/* Register the Random\RandomException namespaced class (PHP 8.2+).` |
|    - | 1071 | `	 * Kept in its own VmEvalChunk (not appended to PH7_BUILTIN_LIB): a namespace` |
|    - | 1072 | `	 * declaration is NOT reset at the block's closing brace in this engine, so` |
|    - | 1073 | `	 * anything following it in the same chunk would leak into the Random` |
|    - | 1074 | `	 * namespace. Isolation instead comes from the compile state being per CHUNK` |
|    - | 1075 | `	 * (PH7_ResetCodeGenerator/PH7_CompilerSaveState clear the compiler namespace),` |
|    - | 1076 | `	 * so this lands as Random\RandomException while later user code still compiles` |
|    - | 1077 | `	 * in the global namespace. */` |
|    - | 1078 | `	{` |
|    - | 1079 | `		static const char zRandomLib[] =` |
|    - | 1080 | `			"namespace Random { class RandomException extends \\Exception { } }";` |
| 4533 | 1081 | `		SyStringInitFromBuf(&sRandom,zRandomLib,sizeof(zRandomLib)-1);` |
| 4533 | 1082 | `		VmEvalChunk(&(*pVm),0,&sRandom,PH7_PHP_ONLY,FALSE);` |
|    - | 1083 | `	}` |
| 4533 | 1084 | `	return SXRET_OK;` |
|    5 | 1085 | `}` |
|    - | 1086 |  |
