/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * The embedded PHP source of the core built-in class library (Exception and
 * friends, ArrayAccess/Countable/..., Closure, Fiber, Generator shells),
 * compiled at VM init by PH7_VmInstallBuiltinLib() — the bootstrap step
 * PH7_VmInit runs right after the code generator comes up. Split from vm.c
 * so the chunk string and its sizeof stay in one translation unit (the
 * vm_builtin_reflection_lib.c pattern).
 */
/* The libxml-backed extensions (libxml/dom/xmlwriter) are reported by
 * extension_loaded()/get_loaded_extensions() only when compiled in. These
 * string fragments splice into the prelude arrays via adjacent-literal
 * concatenation; they are leading-comma so they append cleanly and vanish
 * to "" in tiny/non-libxml builds. */
#ifdef PH7_ENABLE_LIBXML
#define PHL_EXT_LOADED_LIBXML ", 'libxml' => 1, 'dom' => 1, 'xmlwriter' => 1"
#define PHL_EXT_LIST_LIBXML   ",'libxml','dom','xmlwriter'"
#else
#define PHL_EXT_LOADED_LIBXML ""
#define PHL_EXT_LIST_LIBXML   ""
#endif
#define PH7_BUILTIN_LIB \
	"interface Throwable {"\
	"public function getMessage();"\
	"public function getCode();"\
	"public function getFile();"\
	"public function getLine();"\
	"public function getTrace();"\
	"public function getTraceAsString();"\
	"public function getPrevious();"\
	"public function __toString();"\
	"}"\
	"interface Traversable {}"\
	"interface ArrayAccess {"\
	"public function offsetExists($offset);"\
	"public function offsetGet($offset);"\
	"public function offsetSet($offset, $value);"\
	"public function offsetUnset($offset);"\
	"}"\
	"interface Countable {"\
	"public function count();"\
	"}"\
	"interface Stringable {"\
	"public function __toString();"\
	"}"\
	"interface JsonSerializable {"\
	"public function jsonSerialize();"\
	"}"\
	"interface UnitEnum {"\
	"public static function cases();"\
	"}"\
	"interface BackedEnum extends UnitEnum {"\
	"public static function from($value);"\
	"public static function tryFrom($value);"\
	"}"\
	"class Exception implements Throwable { "\
    "protected $message = '';"\
    "protected $code = 0;"\
    "protected $file;"\
    "protected $line;"\
    "private $trace;"\
    "private $previous;"\
	"public function __construct($message = null, $code = 0, ?Throwable $previous = null){"\
	"   if( isset($message) ){"\
	"	  $this->message = $message;"\
	"   }"\
	"   $this->code = $code;"\
	"   if( isset($previous) ){"\
	"     $this->previous = $previous;"\
	"   }"\
	"}"\
	"public function getMessage(){"\
	"   return $this->message;"\
	"}"\
	" public function getCode(){"\
	"  return $this->code;"\
	"}"\
	"public function getFile(){"\
	"  return $this->file;"\
	"}"\
	"public function getLine(){"\
	"  return $this->line;"\
	"}"\
	"public function getTrace(){"\
	"   return $this->trace;"\
	"}"\
	"public function getTraceAsString(){"\
	"  $s = ''; $i = 0;"\
	"  if( is_array($this->trace) ){"\
	"    foreach( $this->trace as $f ){"\
	"      $a = array();"\
	"      if( isset($f['args']) && is_array($f['args']) ){"\
	"        foreach( $f['args'] as $v ){"\
	"          if( is_string($v) ){"\
	"            $a[] = strlen($v) > 15 ? \"'\" . substr($v, 0, 15) . \"...'\" : \"'\" . $v . \"'\";"\
	"          } elseif( is_array($v) ){ $a[] = 'Array'; }"\
	"          elseif( is_object($v) ){ $a[] = 'Object(' . get_class($v) . ')'; }"\
	"          elseif( is_null($v) ){ $a[] = 'NULL'; }"\
	"          elseif( is_bool($v) ){ $a[] = $v ? 'true' : 'false'; }"\
	"          else { $a[] = (string)$v; }"\
	"        }"\
	"      }"\
	"      $s .= '#' . $i . ' ' . $f['file'] . '(' . $f['line'] . '): '"\
	"         . (isset($f['class']) ? $f['class'] . $f['type'] : '') . $f['function']"\
	"         . '(' . implode(', ', $a) . \")\\n\";"\
	"      $i++;"\
	"    }"\
	"  }"\
	"  return $s . '#' . $i . ' {main}';"\
	"}"\
	"public function getPrevious(){"\
	"    return $this->previous;"\
	"}"\
	"public function __toString(){"\
	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\
    "}"\
	"}"\
	"class Error implements Throwable { "\
    "protected $message = '';"\
    "protected $code = 0;"\
    "protected $file;"\
    "protected $line;"\
    "private $trace;"\
    "private $previous;"\
	"public function __construct($message = null, $code = 0, ?Throwable $previous = null){"\
	"   if( isset($message) ){"\
	"	  $this->message = $message;"\
	"   }"\
	"   $this->code = $code;"\
	"   if( isset($previous) ){"\
	"     $this->previous = $previous;"\
	"   }"\
	"}"\
	"public function getMessage(){"\
	"   return $this->message;"\
	"}"\
	"public function getCode(){"\
	"  return $this->code;"\
	"}"\
	"public function getFile(){"\
	"  return $this->file;"\
	"}"\
	"public function getLine(){"\
	"  return $this->line;"\
	"}"\
	"public function getTrace(){"\
	"   return $this->trace;"\
	"}"\
	"public function getTraceAsString(){"\
	"  $s = ''; $i = 0;"\
	"  if( is_array($this->trace) ){"\
	"    foreach( $this->trace as $f ){"\
	"      $a = array();"\
	"      if( isset($f['args']) && is_array($f['args']) ){"\
	"        foreach( $f['args'] as $v ){"\
	"          if( is_string($v) ){"\
	"            $a[] = strlen($v) > 15 ? \"'\" . substr($v, 0, 15) . \"...'\" : \"'\" . $v . \"'\";"\
	"          } elseif( is_array($v) ){ $a[] = 'Array'; }"\
	"          elseif( is_object($v) ){ $a[] = 'Object(' . get_class($v) . ')'; }"\
	"          elseif( is_null($v) ){ $a[] = 'NULL'; }"\
	"          elseif( is_bool($v) ){ $a[] = $v ? 'true' : 'false'; }"\
	"          else { $a[] = (string)$v; }"\
	"        }"\
	"      }"\
	"      $s .= '#' . $i . ' ' . $f['file'] . '(' . $f['line'] . '): '"\
	"         . (isset($f['class']) ? $f['class'] . $f['type'] : '') . $f['function']"\
	"         . '(' . implode(', ', $a) . \")\\n\";"\
	"      $i++;"\
	"    }"\
	"  }"\
	"  return $s . '#' . $i . ' {main}';"\
	"}"\
	"public function getPrevious(){"\
	"    return $this->previous;"\
	"}"\
	"public function __toString(){"\
	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\
	"}"\
	"}"\
	"class TypeError extends Error { }"\
	"class ArgumentCountError extends TypeError { }"\
	"class ValueError extends Error { }"\
	"class FiberError extends Error { }"\
	"class AssertionError extends Error { }"\
	"class ArithmeticError extends Error { }"\
	"class DivisionByZeroError extends ArithmeticError { }"\
	"class UnhandledMatchError extends Error { }"\
	"class CompileError extends Error { }"\
	"class ParseError extends CompileError { }"\
	"class ErrorException extends Exception { "\
	"protected $severity;"\
	"public function __construct(?string $message = null,"\
	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,?Throwable $previous = null){"\
	"   /* message/code/previous belong to Exception (trace/previous are private"\
	"    * to it); delegate, then set our own severity plus the caller-supplied"\
	"    * file/line, which are protected and stay writable here. */"\
	"   parent::__construct($message, $code, $previous);"\
	"   $this->severity = $severity;"\
	"   $this->file = $filename;"\
	"   $this->line = $lineno;"\
	"}"\
	"public function getSeverity(){"\
	"   return $this->severity;"\
    "}"\
	"}"\
	"/* SPL exceptions: thin tree, inherit Exception's ctor+getters. Roots first. */"\
	"class LogicException extends Exception { }"\
	"class RuntimeException extends Exception { }"\
	"class BadFunctionCallException extends LogicException { }"\
	"class BadMethodCallException extends BadFunctionCallException { }"\
	"class DomainException extends LogicException { }"\
	"class InvalidArgumentException extends LogicException { }"\
	"class LengthException extends LogicException { }"\
	"class OutOfRangeException extends LogicException { }"\
	"class OutOfBoundsException extends RuntimeException { }"\
	"class OverflowException extends RuntimeException { }"\
	"class RangeException extends RuntimeException { }"\
	"class UnderflowException extends RuntimeException { }"\
	"class UnexpectedValueException extends RuntimeException { }"\
	"class JsonException extends Exception { }"\
	"interface Iterator extends Traversable {"\
	"public function current();"\
	"public function key();"\
	"public function next();"\
	"public function rewind();"\
	"public function valid();"\
	"}"\
	"interface IteratorAggregate extends Traversable {"\
	"public function getIterator();"\
	"}"\
	"interface Serializable {"\
	"public function serialize();"\
	"public function unserialize(string $serialized);"\
	"}"\
	"/* Directory releated IO */"\
	"class Directory {"\
	"public $handle = null;"\
	"public $path  = null;"\
	"public function __construct(string $path)"\
	"{"\
	"   $this->handle = opendir($path);"\
	"   if( $this->handle !== FALSE ){"\
	"      $this->path = $path;"\
	"   }"\
	"}"\
	"public function __destruct()"\
	"{"\
	"  if( $this->handle != null ){"\
	"       closedir($this->handle);"\
	"  }"\
	"}"\
	"public function read()"\
	"{"\
	"    return readdir($this->handle);"\
	"}"\
	"public function rewind()"\
	"{"\
	"    rewinddir($this->handle);"\
	"}"\
	"public function close()"\
	"{"\
	"    closedir($this->handle);"\
	"    $this->handle = null;"\
	"}"\
	"}"\
	/* Fiber and Generator are declared ENTIRELY from C — class, private slots and
	 * every method — by PH7_VmInstallFiberNative / PH7_VmInstallGeneratorNative.
	 * They are the first two builtin classes with no presence in this chunk at all.
	 * Generator's `implements Iterator` is attached there too, and has to be: it is
	 * applied AFTER its methods exist, because PH7_ClassImplement installs an
	 * ABSTRACT stub for every interface method a class does not already declare. */\
	"final class Closure {"\
	"  private $__fn;"\
	"  private $__this;"\
	"  private $__scope;"\
	"  public function __construct(){ throw new \\Error('Instantiation of class Closure is not allowed'); }"\
	/* bindTo()/bind()/fromCallable() are NATIVE methods installed by
	 * PH7_VmInstallClosureNative() — they used to be one-line forwards to the
	 * global __closure_bindTo/__closure_fromCallable thunks, which no longer
	 * exist. call() stays here because its body is genuinely PHP: it rebinds and
	 * then invokes with an argument unpack. */\
	"  public function call($newThis, ...$args){ $bound = $this->bindTo($newThis, get_class($newThis)); return $bound(...$args); }"\
	"}"\
	/* stdClass is empty (PHP-exact): holds only dynamic (runtime-added) properties. */\
	"#[Attribute(Attribute::TARGET_CLASS)]"\
	"final class Attribute {"\
	"  const TARGET_CLASS = 1;"\
	"  const TARGET_FUNCTION = 2;"\
	"  const TARGET_METHOD = 4;"\
	"  const TARGET_PROPERTY = 8;"\
	"  const TARGET_CLASS_CONSTANT = 16;"\
	"  const TARGET_PARAMETER = 32;"\
	"  const TARGET_CONSTANT = 64;"\
	"  const TARGET_ALL = 127;"\
	"  const IS_REPEATABLE = 128;"\
	"  public $flags;"\
	"  public function __construct($flags = 127){ $this->flags = $flags; }"\
	"}"\
	"#[Attribute(Attribute::TARGET_METHOD | Attribute::TARGET_FUNCTION | Attribute::TARGET_CLASS_CONSTANT | Attribute::TARGET_CONSTANT)]"\
	"final class Deprecated {"\
	"  public $message;"\
	"  public $since;"\
	"  public function __construct($message = null, $since = null){"\
	"    $this->message = $message;"\
	"    $this->since = $since;"\
	"  }"\
	"}"\
	"class stdClass{"\
	"}"\
	/* This one definition serves every spelling — function names are case-insensitive
	   (hFunction, vm.c). The second, byte-identical `Dir()` copy that used to sit here
	   was PH7's manual hack for that, the same one the keyword table had. */\
	"function dir(string $directory, $context = null){"\
	"   return new Directory($directory);"\
	"}"\
	"function scandir(string $directory,int $sorting_order = SCANDIR_SORT_ASCENDING, $context = null)"\
    "{"\
	"  $aDir = array();"\
	"  $pHandle = opendir($directory);"\
	"  if( $pHandle == FALSE ){ return FALSE; }"\
	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\
	"      $aDir[] = $pEntry;"\
	"   }"\
	"  closedir($pHandle);"\
	"  /* php's rule is a two-way split, not a three-value enum: SORT_NONE leaves the"\
	"     order alone and EVERY other value sorts -- ascending only for the exact"\
	"     SORT_ASCENDING, descending otherwise. PHL left an unknown value UNSORTED,"\
	"     which reads as SORT_NONE. */"\
	"  if( $sorting_order != SCANDIR_SORT_NONE ){"\
	"      if( $sorting_order == SCANDIR_SORT_ASCENDING ){ sort($aDir); }"\
	"      else { rsort($aDir); }"\
	"  }"\
	"  return $aDir;"\
	"}"\
	"function glob(string $pattern,int $flags = 0){"\
	"/* php rejects a mask holding any bit outside GLOB_AVAILABLE_FLAGS with a warning"\
	"   and FALSE. PHL accepted anything and just tested the bits it knew, so a stale"\
	"   script passing the OLD PHL glob values (1/2/4/...) silently got a plain glob."\
	"   The literal is GLOB_AVAILABLE_FLAGS; PHL does not define that constant yet. */"\
	"if( $flags & ~(GLOB_ERR|GLOB_MARK|GLOB_NOCHECK|GLOB_NOSORT|GLOB_BRACE|GLOB_NOESCAPE|GLOB_ONLYDIR) ){"\
	"  trigger_error('glob(): At least one of the passed flags is invalid or not supported on this platform', E_USER_WARNING);"\
	"  return FALSE;"\
	"}"\
	"/* php keeps the literal directory portion of the pattern in every result;"\
	"   split off everything up to and including the last '/' as the prefix. */"\
	"$slash = strrpos($pattern,'/');"\
	"if( $slash === false ){ $zDir = '.'; $prefix = ''; $pat = $pattern; }"\
	"else { $zDir = substr($pattern,0,$slash); if( $zDir === '' ){ $zDir = '/'; } $prefix = substr($pattern,0,$slash+1); $pat = substr($pattern,$slash+1); }"\
	"$pHandle = opendir($zDir);"\
	"if( $pHandle == FALSE ){"\
	"   /* IO error while opening the target directory,return FALSE */"\
	"	return FALSE;"\
	"}"\
	"$pArray = array(); /* Empty array */"\
	"/* Loop throw available entries */"\
	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\
	" /* php's glob() never matches a leading-dot entry (incl. '.' and '..') unless"\
	"    the pattern itself starts with a dot */"\
	"	if( strlen($pEntry) > 0 && $pEntry[0] === '.' && (strlen($pat) < 1 || $pat[0] !== '.') ){ continue; }"\
	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\
	"	$rc = strglob($pat,$pEntry);"\
	"	if( $rc ){"\
	"	   $zFull = $prefix . $pEntry;"\
	"	   if( is_dir($zDir . '/' . $pEntry) ){"\
	"	      if( $flags & GLOB_MARK ){"\
	"		     /* Adds a slash to each directory returned */"\
	"			 $zFull .= DIRECTORY_SEPARATOR;"\
	"		  }"\
	"	   }else if( $flags & GLOB_ONLYDIR ){"\
	"	     /* Not a directory,ignore */"\
	"		 continue;"\
	"	   }"\
	"	   /* Add the entry (with its literal directory prefix, php-style) */"\
	"	   $pArray[] = $zFull;"\
	"	}"\
	" }"\
	"/* Close the handle */"\
	"closedir($pHandle);"\
	"if( ($flags & GLOB_NOSORT) == 0 ){"\
	"  /* Sort the array */"\
	"  sort($pArray);"\
	"}"\
	"if( ($flags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\
	"  /* Return the search pattern if no files matching were found */"\
	"  $pArray[] = $pattern;"\
	"}"\
	"/* Return the created array */"\
	"return $pArray;"\
   "}"\
   "/* Creates a temporary file */"\
   "function tmpfile(){"\
   "  /* Extract the temp directory */"\
   "  $zTempDir = sys_get_temp_dir();"\
   "  if( strlen($zTempDir) < 1 ){"\
   "    /* Use the current dir */"\
   "    $zTempDir = '.';"\
   "  }"\
   "  /* Create the file */"\
   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\
   "  return $pHandle;"\
   "}"\
   "/* php's number_format(): missing entirely from PH7."\
   " * Signature: number_format(float $num, int $decimals = 0, ?string $decimal_separator = '.',"\
   " * ?string $thousands_separator = ','): string. Being a PRELUDE function it is out of reach"\
   " * of the aBuiltinSig[] ZPP screen (host builtins only), so the four rows are checked here in"\
   " * the shape php's ZPP reports them (__php_zpp_type, the pattern count_chars/max/min use)."\
   " * Without them an array, a not-stringable object, a resource and a non-numeric string all"\
   " * reached the (float) cast and ANSWERED -- number_format(new Bare()) warned and returned"\
   " * '1', number_format('abc') returned '0'. A leading-numeric string ('12abc') is php's"\
   " * TypeError too, not a silent 12. null and a lossy float stay PHL's TypeError rather than"\
   " * php's deprecate-and-coerce (§10), matching what str_repeat() already answers for the"\
   " * same two; the two SEPARATORS are declared ?string, so null is legal there. */"\
   "function number_format($num, $decimals = 0, $decimal_separator = '.', $thousands_separator = ','){"\
   "  if( is_array($num) || is_object($num) || is_resource($num) || $num === null"\
   "   || (is_string($num) && !is_numeric($num)) ){"\
   "    throw new TypeError('number_format(): Argument #1 ($num) must be of type int|float, '"\
   "      . __php_zpp_type($num) . ' given');"\
   "  }"\
   "  if( is_array($decimals) || is_object($decimals) || is_resource($decimals) || $decimals === null"\
   "   || (is_string($decimals) && !is_numeric($decimals)) ){"\
   "    throw new TypeError('number_format(): Argument #2 ($decimals) must be of type int, '"\
   "      . __php_zpp_type($decimals) . ' given');"\
   "  }"\
   "  if( is_float($decimals) && $decimals != (int)$decimals ){"\
   "    throw new TypeError('number_format(): Argument #2 ($decimals) must be of type int, float given');"\
   "  }"\
   "  if( is_array($decimal_separator) || is_resource($decimal_separator)"\
   "   || (is_object($decimal_separator) && !method_exists($decimal_separator, '__toString')) ){"\
   "    throw new TypeError('number_format(): Argument #3 ($decimal_separator) must be of type ?string, '"\
   "      . __php_zpp_type($decimal_separator) . ' given');"\
   "  }"\
   "  if( is_array($thousands_separator) || is_resource($thousands_separator)"\
   "   || (is_object($thousands_separator) && !method_exists($thousands_separator, '__toString')) ){"\
   "    throw new TypeError('number_format(): Argument #4 ($thousands_separator) must be of type ?string, '"\
   "      . __php_zpp_type($thousands_separator) . ' given');"\
   "  }"\
   "  $num = (float)$num;"\
   "  $decimals = (int)$decimals;"\
   "  if( $decimal_separator === null ){ $decimal_separator = '.'; }"\
   "  if( $thousands_separator === null ){ $thousands_separator = ','; }"\
   "  $decimal_separator = (string)$decimal_separator;"\
   "  $thousands_separator = (string)$thousands_separator;"\
   "  /* round() first: sprintf uses banker's rounding, php's number_format rounds"\
   "   * half AWAY FROM ZERO (number_format(0.5) is '1', not '0'). A NEGATIVE precision"\
   "   * rounds to tens/hundreds (php 8: number_format(1.5,-1) is '0'); the displayed"\
   "   * value never carries negative decimal places, so round with the real precision"\
   "   * but format/append with max(0, $decimals). */"\
   "  $num = round($num, $decimals);"\
   "  $fdec = $decimals < 0 ? 0 : $decimals;"\
   "  $s = sprintf('%.' . $fdec . 'f', $num);"\
   "  $neg = false;"\
   "  if( substr($s, 0, 1) === '-' ){ $neg = true; $s = substr($s, 1); }"\
   "  $parts = explode('.', $s);"\
   "  $int = $parts[0];"\
   "  $frac = count($parts) > 1 ? $parts[1] : '';"\
   "  $out = '';"\
   "  $len = strlen($int);"\
   "  $c = 0;"\
   "  for( $i = $len - 1 ; $i >= 0 ; $i-- ){"\
   "    $out = $int[$i] . $out;"\
   "    $c++;"\
   "    if( $c % 3 === 0 && $i > 0 ){ $out = $thousands_separator . $out; }"\
   "  }"\
   "  if( $fdec > 0 ){ $out = $out . $decimal_separator . $frac; }"\
   "  if( $neg ){ $out = '-' . $out; }"\
   "  return $out;"\
   "}"\
   "function is_nan($num){ $num = (float)$num; return $num != $num; }"\
   "function is_infinite($num){ $num = (float)$num; return $num == INF || $num == -INF; }"\
   "function is_finite($num){ $num = (float)$num; return !is_nan($num) && !is_infinite($num); }"\
   "/* phl.stub_extensions (a -d/php.ini list, comma-separated) declares extensions"\
   " * PHL does not implement as LOADED, backed by no-op behaviour, so software that"\
   " * only GATES on extension_loaded() (e.g. PHPUnit's dom/xmlwriter check) runs"\
   " * unmodified. It does NOT synthesize the extension's classes/functions. */"\
   "function __phl_stub_exts(){"\
   "  $s = ini_get('phl.stub_extensions');"\
   "  if( $s === false || $s === '' ){ return array(); }"\
   "  $out = array();"\
   "  foreach( explode(',', (string)$s) as $e ){ $e = trim($e); if( $e !== '' ){ $out[strtolower($e)] = $e; } }"\
   "  return $out;"\
   "}"\
   "function extension_loaded($extension){"\
   "  static $ext = array('core' => 1, 'standard' => 1, 'pcre' => 1, 'json' => 1,"\
   "   'ctype' => 1, 'date' => 1, 'spl' => 1, 'reflection' => 1, 'mbstring' => 1,"\
   "   'hash' => 1, 'filter' => 1, 'session' => 1" PHL_EXT_LOADED_LIBXML ");"\
   "  $n = strtolower((string)$extension);"\
   "  if( isset($ext[$n]) ){ return true; }"\
   "  $stub = __phl_stub_exts();"\
   "  return isset($stub[$n]);"\
   "}"\
   "function get_loaded_extensions($zend_extensions = false){"\
   "  if( $zend_extensions ){ return array(); }"\
   "  $base = array('Core','date','pcre','SPL','json','standard',"\
   "   'ctype','filter','hash','Reflection','session','mbstring'" PHL_EXT_LIST_LIBXML ");"\
   "  foreach( __phl_stub_exts() as $e ){ $base[] = $e; }"\
   "  return $base;"\
   "}"\
   "/* Inverse of bin2hex() */"\
   "function hex2bin($string){"\
   "  $string = (string)$string;"\
   "  $len = strlen($string);"\
   "  if( $len % 2 !== 0 ){"\
   "    trigger_error('hex2bin(): Hexadecimal input string must have an even length', E_USER_WARNING);"\
   "    return false;"\
   "  }"\
   "  $out = '';"\
   "  for( $i = 0 ; $i < $len ; $i += 2 ){"\
   "    $pair = substr($string, $i, 2);"\
   "    if( !ctype_xdigit($pair) ){"\
   "      trigger_error('hex2bin(): Input string must be hexadecimal string', E_USER_WARNING);"\
   "      return false;"\
   "    }"\
   "    $out = $out . chr(hexdec($pair));"\
   "  }"\
   "  return $out;"\
   "}"\
   "/* Division that never throws: INF/-INF/NAN like php */"\
   "function fdiv($num1, $num2){"\
   "  $num1 = (float)$num1;"\
   "  $num2 = (float)$num2;"\
   "  if( $num2 == 0.0 ){"\
   "    if( $num1 == 0.0 || is_nan($num1) ){ return NAN; }"\
   "    return $num1 > 0 ? INF : -INF;"\
   "  }"\
   "  return $num1 / $num2;"\
   "}"\
   "function checkdate($month, $day, $year){"\
   "  $month = (int)$month; $day = (int)$day; $year = (int)$year;"\
   "  if( $month < 1 || $month > 12 || $year < 1 || $year > 32767 || $day < 1 ){ return false; }"\
   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\
   "  $max = $days[$month - 1];"\
   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) || ($year % 400 === 0)) ){"\
   "    $max = 29;"\
   "  }"\
   "  return $day <= $max;"\
   "}"\
   "function is_iterable($value){ return is_array($value) || ($value instanceof Traversable); }"\
   "function is_countable($value){ return is_array($value) || ($value instanceof Countable); }"\
   "function doubleval($value){ return (float)$value; }"\
   "function array_count_values($array){"\
   "  $out = array();"\
   "  foreach( $array as $v ){"\
   "    if( !is_int($v) && !is_string($v) ){"\
   "      trigger_error('array_count_values(): Can only count string and integer values, entry skipped', E_USER_WARNING);"\
   "      continue;"\
   "    }"\
   "    if( isset($out[$v]) ){ $out[$v] = $out[$v] + 1; } else { $out[$v] = 1; }"\
   "  }"\
   "  return $out;"\
   "}"\
   "function array_change_key_case($array, $case = CASE_LOWER){"\
   "  $out = array();"\
   "  foreach( $array as $k => $v ){"\
   "    if( is_string($k) ){ $k = ($case == CASE_UPPER) ? strtoupper($k) : strtolower($k); }"\
   "    $out[$k] = $v;"\
   "  }"\
   "  return $out;"\
   "}"\
   "function array_replace_recursive($array, ...$replacements){"\
   "  foreach( $replacements as $o ){"\
   "    foreach( $o as $k => $v ){"\
   "      if( is_array($v) && isset($array[$k]) && is_array($array[$k]) ){"\
   "        $array[$k] = array_replace_recursive($array[$k], $v);"\
   "      }else{"\
   "        $array[$k] = $v;"\
   "      }"\
   "    }"\
   "  }"\
   "  return $array;"\
   "}"\
   "function class_uses($object_or_class, $autoload = true){"\
   "  $c = is_object($object_or_class) ? get_class($object_or_class) : (string)$object_or_class;"\
   "  if( !class_exists($c) ){ return false; }"\
   "  return array();  /* PHL has no traits yet -- always the empty set */"\
   "}"\
   /* count_chars: php's five modes are a 2x2 split plus mode 0 -- 1/2 answer an \
    * ARRAY, 3/4 a STRING, and the ODD modes report the bytes that WERE used \
    * where the EVEN ones report the bytes that were NOT. PH7 implemented 0/1/3 \
    * and let 2 and 4 fall through to mode 0's full table: the exact complement \
    * of the answer asked for, silently. Any other mode is php's ValueError. */\
   "function count_chars($string, $mode = 0){"\
   "  if( is_array($mode) || is_object($mode) || is_resource($mode)"\
   "   || (is_string($mode) && !is_numeric($mode)) ){"\
   "    throw new TypeError('count_chars(): Argument #2 ($mode) must be of type int, ' . __php_zpp_type($mode) . ' given');"\
   "  }"\
   "  $mode = (int)$mode;"\
   "  if( $mode < 0 || $mode > 4 ){"\
   "    throw new ValueError('count_chars(): Argument #2 ($mode) must be between 0 and 4 (inclusive)');"\
   "  }"\
   "  $string = (string)$string;"\
   "  $counts = array();"\
   "  for( $i = 0 ; $i < 256 ; $i++ ){ $counts[$i] = 0; }"\
   "  $len = strlen($string);"\
   "  for( $i = 0 ; $i < $len ; $i++ ){ $b = ord($string[$i]); $counts[$b] = $counts[$b] + 1; }"\
   "  if( $mode == 0 ){ return $counts; }"\
   "  $used = ($mode == 1 || $mode == 3);"\
   "  $asStr = ($mode == 3 || $mode == 4);"\
   "  $out = $asStr ? '' : array();"\
   "  foreach( $counts as $b => $n ){"\
   "    if( ($n > 0) !== $used ){ continue; }"\
   "    if( $asStr ){ $out = $out . chr($b); } else { $out[$b] = $n; }"\
   "  }"\
   "  return $out;"\
   "}"\
   "function ip2long($ip){"\
   "  $p = explode('.', (string)$ip);"\
   "  if( count($p) !== 4 ){ return false; }"\
   "  $n = 0;"\
   "  foreach( $p as $o ){"\
   "    if( !ctype_digit($o) || (int)$o < 0 || (int)$o > 255 ){ return false; }"\
   "    $n = $n * 256 + (int)$o;"\
   "  }"\
   "  return $n;"\
   "}"\
   "function long2ip($ip){"\
   "  $n = (int)$ip;"\
   "  return (($n >> 24) & 255) . '.' . (($n >> 16) & 255) . '.' . (($n >> 8) & 255) . '.' . ($n & 255);"\
   "}"\
   "function preg_filter($pattern, $replacement, $subject, $limit = -1, &$count = null){"\
   "  /* php declares &$count and always writes it -- the total number of"\
   "   * replacements across every subject, 0 when nothing matched. PHL never"\
   "   * declared the parameter, so a caller reading it got its previous value. */"\
   "  if( is_array($subject) ){"\
   "    $total = 0;"\
   "    $out = array();"\
   "    foreach( $subject as $k => $v ){"\
   "      $r = preg_replace($pattern, $replacement, (string)$v, $limit, $cnt);"\
   "      $total = $total + $cnt;"\
   "      if( $cnt > 0 ){ $out[$k] = $r; }"\
   "    }"\
   "    $count = $total;"\
   "    return $out;"\
   "  }"\
   "  $r = preg_replace($pattern, $replacement, (string)$subject, $limit, $cnt);"\
   "  $count = $cnt;"\
   "  return $cnt > 0 ? $r : null;"\
   "}"\
   "function preg_replace_callback_array($pattern, $subject, $limit = -1, &$count = null, $flags = 0){"\
   "  /* &$count is the total across every pattern; $flags shapes each callback's"\
   "   * match array. php writes &$count only when the whole run SUCCEEDED -- a"\
   "   * pattern that fails to compile answers null and leaves it untouched (an"\
   "   * array subject is not a failure: it degrades to the empty array, count 0). */"\
   "  $total = 0;"\
   "  foreach( $pattern as $pat => $cb ){"\
   "    $subject = preg_replace_callback($pat, $cb, $subject, $limit, $cnt, $flags);"\
   "    if( $subject === null ){ return null; }"\
   "    $total = $total + $cnt;"\
   "  }"\
   "  $count = $total;"\
   "  return $subject;"\
   "}"\
   "function cal_days_in_month($calendar, $month, $year){"\
   "  $month = (int)$month; $year = (int)$year;"\
   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\
   "  if( $month < 1 || $month > 12 ){"\
   "    throw new ValueError('cal_days_in_month(): Argument #2 ($month) must be a valid month');"\
   "  }"\
   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) || ($year % 400 === 0)) ){"\
   "    return 29;"\
   "  }"\
   "  return $days[$month - 1];"\
   "}"\
   "function preg_grep($pattern, $array, $flags = 0){"\
   "  $out = array();"\
   "  foreach( $array as $k => $v ){"\
   "    $m = preg_match($pattern, (string)$v);"\
   "    if( $flags & PREG_GREP_INVERT ){ $m = !$m; }"\
   "    if( $m ){ $out[$k] = $v; }"\
   "  }"\
   "  return $out;"\
   "}"\
   "function class_implements($object_or_class, $autoload = true){"\
   "  $c = is_object($object_or_class) ? get_class($object_or_class) : (string)$object_or_class;"\
   "  if( !class_exists($c) && !interface_exists($c) ){ return false; }"\
   "  $out = array();"\
   "  $r = new ReflectionClass($c);"\
   "  foreach( $r->getInterfaceNames() as $i ){ $out[$i] = $i; }"\
   "  return $out;"\
   "}"\
   "function class_parents($object_or_class, $autoload = true){"\
   "  $c = is_object($object_or_class) ? get_class($object_or_class) : (string)$object_or_class;"\
   "  if( !class_exists($c) ){ return false; }"\
   "  $out = array();"\
   "  $r = new ReflectionClass($c);"\
   "  while( ($p = $r->getParentClass()) ){"\
   "    $n = $p->getName();"\
   "    $out[$n] = $n;"\
   "    $r = $p;"\
   "  }"\
   "  return $out;"\
   "}"\
   "/* php's http_build_query() -- missing from PH7. Skips null values, casts"\
   " * bool to 1/0, prefixes numeric top-level keys, urlencodes per RFC. */"\
   "function __phl_hbq_enc($s, $enc){"\
   "  return $enc == PHP_QUERY_RFC3986 ? rawurlencode((string)$s) : urlencode((string)$s);"\
   "}"\
   "function __phl_hbq(&$pairs, $data, $key_prefix, $numeric_prefix, $sep, $enc){"\
   "  foreach( $data as $k => $v ){"\
   "    if( $v === null ){ continue; }"\
   "    if( $key_prefix === '' ){"\
   "      $ek = is_int($k) ? __phl_hbq_enc($numeric_prefix . $k, $enc) : __phl_hbq_enc($k, $enc);"\
   "    } else {"\
   "      $ek = $key_prefix . '%5B' . __phl_hbq_enc($k, $enc) . '%5D';"\
   "    }"\
   "    if( is_array($v) ){"\
   "      __phl_hbq($pairs, $v, $ek, $numeric_prefix, $sep, $enc);"\
   "    } elseif( is_object($v) ){"\
   "      __phl_hbq($pairs, get_object_vars($v), $ek, $numeric_prefix, $sep, $enc);"\
   "    } else {"\
   "      if( $v === true ){ $v = '1'; } elseif( $v === false ){ $v = '0'; }"\
   "      $pairs[] = $ek . '=' . __phl_hbq_enc($v, $enc);"\
   "    }"\
   "  }"\
   "}"\
   "function http_build_query($data, $numeric_prefix = '', $arg_separator = null, $encoding_type = PHP_QUERY_RFC1738){"\
   "  if( !is_array($data) && !is_object($data) ){"\
   "    throw new TypeError('http_build_query(): Argument #1 ($data) must be of type array, ' . __php_zpp_type($data) . ' given');"\
   "  }"\
   "  if( $arg_separator === null ){ $arg_separator = '&'; }"\
   "  $pairs = array();"\
   "  __phl_hbq($pairs, is_object($data) ? get_object_vars($data) : $data, '', (string)$numeric_prefix, $arg_separator, $encoding_type);"\
   "  return implode($arg_separator, $pairs);"\
   "}"\
   "/* php's parse_str() -- missing from PH7. Mangles the base name ('.'/' ' -> '_'),"\
   " * parses [key] nesting and [] appends, urldecodes keys and values. */"\
   "function __phl_parsestr_assign(&$arr, $segments, $i, $val){"\
   "  $seg = $segments[$i];"\
   "  $last = ($i === count($segments) - 1);"\
   "  if( $seg === '' ){"\
   "    if( $last ){ $arr[] = $val; return; }"\
   "    $arr[] = array();"\
   "    $k = array_key_last($arr);"\
   "    __phl_parsestr_assign($arr[$k], $segments, $i + 1, $val);"\
   "  } else {"\
   "    if( $last ){ $arr[$seg] = $val; return; }"\
   "    if( !isset($arr[$seg]) || !is_array($arr[$seg]) ){ $arr[$seg] = array(); }"\
   "    __phl_parsestr_assign($arr[$seg], $segments, $i + 1, $val);"\
   "  }"\
   "}"\
   "function parse_str($string, &$result){"\
   "  $result = array();"\
   "  $string = (string)$string;"\
   "  if( $string === '' ){ return; }"\
   "  foreach( explode('&', $string) as $pair ){"\
   "    if( $pair === '' ){ continue; }"\
   "    $eq = strpos($pair, '=');"\
   "    if( $eq === false ){ $rawkey = $pair; $val = ''; }"\
   "    else { $rawkey = substr($pair, 0, $eq); $val = urldecode(substr($pair, $eq + 1)); }"\
   "    if( $rawkey === '' ){ continue; }"\
   "    $bpos = strpos($rawkey, '[');"\
   "    if( $bpos === false ){ $base = $rawkey; $subs = array(); }"\
   "    else {"\
   "      $base = substr($rawkey, 0, $bpos);"\
   "      preg_match_all('/\\[([^\\]]*)\\]/', substr($rawkey, $bpos), $m);"\
   "      $subs = $m[1];"\
   "    }"\
   "    $base = str_replace(array(' ', '.'), '_', urldecode($base));"\
   "    if( $base === '' ){ continue; }"\
   "    $segs = array($base);"\
   "    foreach( $subs as $s ){ $segs[] = urldecode($s); }"\
   "    __phl_parsestr_assign($result, $segs, 0, $val);"\
   "  }"\
   "}"\
   "/* php 8.3 str_increment(): Perl-style alphanumeric increment. */"\
   "function str_increment($string){"\
   "  $string = (string)$string;"\
   "  if( $string === '' ){ throw new ValueError('str_increment(): Argument #1 ($string) must not be empty'); }"\
   "  if( !ctype_alnum($string) ){ throw new ValueError('str_increment(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\
   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\
   "    $c = $string[$i];"\
   "    if( $c === 'z' ){ $string[$i] = 'a'; }"\
   "    elseif( $c === 'Z' ){ $string[$i] = 'A'; }"\
   "    elseif( $c === '9' ){ $string[$i] = '0'; }"\
   "    else { $string[$i] = chr(ord($c) + 1); return $string; }"\
   "  }"\
   "  $first = $string[0];"\
   "  if( $first === '0' ){ return '1' . $string; }"\
   "  if( $first === 'a' ){ return 'a' . $string; }"\
   "  return 'A' . $string;"\
   "}"\
   "/* php 8.3 str_decrement(): inverse of str_increment(); throws out of range"\
   " * at the bottom of the counting sequence. */"\
   "function str_decrement($string){"\
   "  $string = (string)$string;"\
   "  if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) must not be empty'); }"\
   "  if( !ctype_alnum($string) ){ throw new ValueError('str_decrement(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\
   "  $orig = $string;"\
   "  $borrowed = false;"\
   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\
   "    $c = $string[$i];"\
   "    if( $c === 'a' ){ $string[$i] = 'z'; }"\
   "    elseif( $c === 'A' ){ $string[$i] = 'Z'; }"\
   "    elseif( $c === '0' ){ $string[$i] = '9'; }"\
   "    else { $string[$i] = chr(ord($c) - 1); $borrowed = false; break; }"\
   "    if( $i === 0 ){ $borrowed = true; }"\
   "  }"\
   "  if( $borrowed ){"\
   "    if( $string[0] === '9' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\
   "    $string = substr($string, 1);"\
   "    if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\
   "  } elseif( strlen($string) > 1 && $string[0] === '0' ){"\
   "    $string = substr($string, 1);"\
   "  }"\
   "  return $string;"\
   "}"\
   "/* Permission bits via stat(); false + warning when stat fails, like php. */"\
   "function fileperms($filename){"\
   "  $s = @stat($filename);"\
   "  if( $s === false ){"\
   "    trigger_error('fileperms(): stat failed for ' . $filename, E_USER_WARNING);"\
   "    return false;"\
   "  }"\
   "  return $s['mode'];"\
   "}"\
   "/* PH7 keeps no stat cache, so this is a no-op like php on a clean cache. */"\
   "function clearstatcache($clear_realpath_cache = false, $filename = ''){}"\
   "/* php 8.4 mb_ucfirst/mb_lcfirst: case-map only the first multibyte char. */"\
   "function mb_ucfirst($string, $encoding = null){"\
   "  $string = (string)$string;"\
   "  if( $string === '' ){ return ''; }"\
   "  return mb_strtoupper(mb_substr($string, 0, 1)) . mb_substr($string, 1);"\
   "}"\
   "function mb_lcfirst($string, $encoding = null){"\
   "  $string = (string)$string;"\
   "  if( $string === '' ){ return ''; }"\
   "  return mb_strtolower(mb_substr($string, 0, 1)) . mb_substr($string, 1);"\
   "}"\
   "/* php 8.4 mb_trim family: strip leading/trailing characters (whole"\
   " * multibyte chars, NO range syntax), defaulting to php's Unicode"\
   " * whitespace set. */"\
   "function __phl_mb_ws(){"\
   "  static $set = null;"\
   "  if( $set === null ){"\
   "    $set = array();"\
   "    foreach( array(0x00,0x09,0x0A,0x0B,0x0C,0x0D,0x20,0x85,0xA0,0x1680,"\
   "      0x180E,0x2000,0x2001,0x2002,0x2003,0x2004,0x2005,0x2006,0x2007,0x2008,"\
   "      0x2009,0x200A,0x2028,0x2029,0x202F,0x205F,0x3000) as $cp ){"\
   "      $set[mb_chr($cp)] = true;"\
   "    }"\
   "  }"\
   "  return $set;"\
   "}"\
   "function __phl_mb_trim($string, $characters, $left, $right){"\
   "  $string = (string)$string;"\
   "  if( $string === '' ){ return ''; }"\
   "  if( $characters === null ){"\
   "    $set = __phl_mb_ws();"\
   "  } else {"\
   "    $set = array();"\
   "    foreach( mb_str_split((string)$characters) as $c ){ $set[$c] = true; }"\
   "  }"\
   "  $chars = mb_str_split($string);"\
   "  $n = count($chars);"\
   "  $i = 0; $j = $n;"\
   "  if( $left ){ while( $i < $j && isset($set[$chars[$i]]) ){ $i++; } }"\
   "  if( $right ){ while( $j > $i && isset($set[$chars[$j - 1]]) ){ $j--; } }"\
   "  return implode('', array_slice($chars, $i, $j - $i));"\
   "}"\
   "function mb_trim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, true, true); }"\
   "function mb_ltrim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, true, false); }"\
   "function mb_rtrim($string, $characters = null, $encoding = null){ return __phl_mb_trim($string, $characters, false, true); }"\
   "/* Creates a temporary file and returns its name */"\
   "function tempnam(string $directory,string $prefix)"\
   "{"\
   "   /* php CREATES the file (empty, mode 0600) and guarantees the name is unique --"\
   "    * returning a bare name left the caller with a path that does not exist, so"\
   "    * file_exists() was false and unlink() failed on it. */"\
   "   $directory = rtrim($directory, DIRECTORY_SEPARATOR);"\
   "   for( $i = 0 ; $i < 64 ; ++$i ){"\
   "     $zPath = $directory.DIRECTORY_SEPARATOR.$prefix.rand_str(12);"\
   "     if( file_exists($zPath) ){ continue; }"\
   "     $pHandle = @fopen($zPath,'x');"\
   "     if( $pHandle === false ){ continue; }"\
   "     fclose($pHandle);"\
   "     @chmod($zPath, 0600);"\
   "     return $zPath;"\
   "   }"\
   "   return false;"\
   "}"\
   "function array_unshift(&$array, ...$values){"\
   " if( !is_array($array) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . __php_zpp_type($array) . ' given'); }"\
   "/* Copy arguments */"\
   "$pNew = $values;"\
   	"/* Make a copy of the old entries */"\
	"$pOld = array_copy($array);"\
	"/* Erase */"\
	"array_erase($array);"\
	"/* Unshift */"\
	"$array = array_merge($pNew,$pOld);"\
	"return sizeof($array);"\
    "}"\
	"function array_merge_recursive(...$arrays){"\
    "$narrays = count($arrays);"\
    "$ret = array();"\
    "for( $i = 0; $i < $narrays; $i++ ){"\
	 " if( !is_array($arrays[$i]) ){"\
	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.__php_zpp_type($arrays[$i]).' given');"\
	 " }"\
     " foreach ($arrays[$i] as $key => $value) {"\
     "  $keyIsInt = is_int($key) || (is_string($key) && (string)intval($key) === $key);"\
     "  if( $keyIsInt ) {"\
     "   $ret[] = $value;"\
     "  } else {"\
     "   if (array_key_exists($key, $ret)) {"\
     "    $cur = $ret[$key];"\
     "    if (is_array($cur) && is_array($value)) {"\
     "     $ret[$key] = array_merge_recursive($cur, $value);"\
     "    } elseif (is_array($cur)) {"\
     "     $ret[$key] = array_merge_recursive($cur, array($value));"\
     "    } elseif (is_array($value)) {"\
     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\
     "    } else {"\
     "     $ret[$key] = array($cur, $value);"\
     "    }"\
     "   } else {"\
     "    $ret[$key] = $value;"\
     "   }"\
     "  }"\
     " }"\
	 " }"\
	 " return $ret;"\
    "}"\
	/* __php_zpp_type: php's ZPP value-name for TypeError messages */\
	"function __php_zpp_type($v){"\
	" if( is_object($v) ){ return get_class($v); }"\
	" if( is_int($v) ){ return 'int'; }"\
	" if( is_float($v) ){ return 'float'; }"\
	" if( is_string($v) ){ return 'string'; }"\
	" if( is_bool($v) ){ return $v ? 'true' : 'false'; }"\
	" if( is_null($v) ){ return 'null'; }"\
	" if( is_array($v) ){ return 'array'; }"\
	" if( is_resource($v) ){ return 'resource'; }"\
	" return 'mixed';"\
	"}"\
	"function max($value, ...$values){"\
    "  $pArgs = func_get_args();"\
    " if( sizeof($pArgs) < 2 ){"\
    " $pArg = $pArgs[0];"\
	" if( !is_array($pArg) ){"\
	"   throw new TypeError('max(): Argument #1 ($value) must be of type array, ' . __php_zpp_type($pArg) . ' given');"\
	" }"\
	" if( sizeof($pArg) < 1 ){"\
	"   throw new ValueError('max(): Argument #1 ($value) must contain at least one element');"\
	" }"\
	" $max = null; $first = true;"\
	" foreach( $pArgs[0] as $val ){"\
	"   if( $first ){ $max = $val; $first = false; }"\
	"   else if( $val > $max ){ $max = $val; }"\
	" }"\
	" return $max;"\
    " }"\
    " $max = $pArgs[0];"\
    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\
    " $val = $pArgs[$i];"\
	"if( $val > $max ){"\
	" $max = $val;"\
	"}"\
    " }"\
	" return $max;"\
    "}"\
	"function min($value, ...$values){"\
    "  $pArgs = func_get_args();"\
    " if( sizeof($pArgs) < 2 ){"\
    " $pArg = $pArgs[0];"\
	" if( !is_array($pArg) ){"\
	"   throw new TypeError('min(): Argument #1 ($value) must be of type array, ' . __php_zpp_type($pArg) . ' given');"\
	" }"\
	" if( sizeof($pArg) < 1 ){"\
	"   throw new ValueError('min(): Argument #1 ($value) must contain at least one element');"\
	" }"\
	" $min = null; $first = true;"\
	" foreach( $pArgs[0] as $val ){"\
	"   if( $first ){ $min = $val; $first = false; }"\
	"   else if( $val < $min ){ $min = $val; }"\
	" }"\
	" return $min;"\
    " }"\
    " $min = $pArgs[0];"\
    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\
    " $val = $pArgs[$i];"\
	"if( $val < $min ){"\
	" $min = $val;"\
	" }"\
    " }"\
	" return $min;"\
	"}"\
	"function fileowner(string $filename){"\
    " $a = stat($filename);"\
	" if( !is_array($a) ){"\
	"	return false;"\
	" }"\
	" return $a['uid'];"\
    "}"\
    "function filegroup(string $filename){"\
	" $a = stat($filename);"\
	" if( !is_array($a) ){"\
	"	return false;"\
	" }"\
	" return $a['gid'];"\
    "}"\
	 "function fileinode(string $filename){"\
	" $a = stat($filename);"\
	" if( !is_array($a) ){"\
	"	return false;"\
	" }"\
	" return $a['ino'];"\
    "}"

PH7_PRIVATE sxi32 PH7_VmInstallBuiltinLib(ph7_vm *pVm)
{
	SyString sBuiltin;
	SyString sRandom;
	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);
	/* Compile the built-in library */
	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);
	/* Register the Random\RandomException namespaced class (PHP 8.2+).
	 * Kept in its own VmEvalChunk (not appended to PH7_BUILTIN_LIB): a namespace
	 * declaration is NOT reset at the block's closing brace in this engine, so
	 * anything following it in the same chunk would leak into the Random
	 * namespace. Isolation instead comes from the compile state being per CHUNK
	 * (PH7_ResetCodeGenerator/PH7_CompilerSaveState clear the compiler namespace),
	 * so this lands as Random\RandomException while later user code still compiles
	 * in the global namespace. */
	{
		static const char zRandomLib[] =
			"namespace Random { class RandomException extends \\Exception { } }";
		SyStringInitFromBuf(&sRandom,zRandomLib,sizeof(zRandomLib)-1);
		VmEvalChunk(&(*pVm),0,&sRandom,PH7_PHP_ONLY,FALSE);
	}
	return SXRET_OK;
}
