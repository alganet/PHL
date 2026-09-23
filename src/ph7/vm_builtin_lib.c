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
/* The eleven core INTERFACES are declared entirely from C by
 * VmInstallCoreInterfaces() below -- they have no presence in this chunk at all.
 * A chunk cannot express what php declares on them: a TENTATIVE return type
 * (php marks nearly every one), and Throwable's `extends Stringable`. */
#define PH7_BUILTIN_LIB \
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
	/* Closure is declared ENTIRELY in C by PH7_VmInstallClosureNative() (vm_exec_ctx.c):
	 * class, the three engine slots (hidden, as php presents no property) and all five
	 * methods. It cannot live here — a chunk-declared property is on every presentation
	 * surface, and `call()` in PHP leaked get_class()'s own TypeError text. */\
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
   "function is_nan($num){ $num = (float)$num; return $num != $num; }"\
   "function is_infinite($num){ $num = (float)$num; return $num == INF || $num == -INF; }"\
   "function is_finite($num){ $num = (float)$num; return !is_nan($num) && !is_infinite($num); }"\
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

/*
 * The eleven core interfaces, declared from C.
 *
 * They are contracts -- no method here has a body, every row is
 * PH7_MOD_ABSTRACT -- so the conversion is entirely about what the DECLARATION
 * says, which is where a chunk fell short in four php-visible ways:
 *
 *  - php's `interface Throwable extends Stringable`: the chunk redeclared
 *    __toString() on Throwable instead, so no Exception was ever Stringable
 *    (`$e instanceof Stringable` was false, and Reflection attributed the
 *    method to Throwable rather than printing php's `inherits Stringable`);
 *  - php declares a RETURN TYPE on all but three of these methods and marks
 *    nearly all of them TENTATIVE (the leading `@`, rule 45) -- a chunk has no
 *    way to say tentative at all;
 *  - php's `mixed` on ArrayAccess's offsets, which the chunk left untyped;
 *  - method ORDER, which Reflection prints: php lists Throwable's getPrevious
 *    before getTraceAsString, and Iterator's as current/next/key/valid/rewind.
 *
 * Order within the table is php's stub order too; the declare-then-link phases
 * of PH7_InstallNativeClasses let Throwable name Stringable and Iterator name
 * Traversable regardless of row order. An interface's parent is `zParent`, not
 * `zImplements` (Reflection walks pBase to attribute an inherited method).
 */
static sxi32 VmInstallCoreInterfaces(ph7_vm *pVm)
{
	static const PH7_NativeMethodDef aStringable[] = {
		{ "__toString", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "string", 0 },
	};
	static const PH7_NativeMethodDef aThrowable[] = {
		/* Not one of these is tentative: php's Throwable is a real contract. */
		{ "getMessage",       PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "string", 0 },
		{ "getCode",          PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", 0, 0 },
		{ "getFile",          PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "string", 0 },
		{ "getLine",          PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "int", 0 },
		{ "getTrace",         PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "array", 0 },
		{ "getPrevious",      PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "?Throwable", 0 },
		{ "getTraceAsString", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "string", 0 },
	};
	static const PH7_NativeMethodDef aArrayAccess[] = {
		{ "offsetExists", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "mixed $offset", "@bool", 0 },
		{ "offsetGet",    PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "mixed $offset", "@mixed", 0 },
		{ "offsetSet",    PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "mixed $offset, mixed $value",
		  "@void", 0 },
		{ "offsetUnset",  PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "mixed $offset", "@void", 0 },
	};
	static const PH7_NativeMethodDef aCountable[] = {
		{ "count", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "@int", 0 },
	};
	static const PH7_NativeMethodDef aJsonSerializable[] = {
		{ "jsonSerialize", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "@mixed", 0 },
	};
	/* The concrete cases()/from()/tryFrom() an enum gets are native methods
	 * declared to match these (oo_native.c, PH7_InstallEnumInterfaceMethods). */
	static const PH7_NativeMethodDef aUnitEnum[] = {
		{ "cases", PH7_MOD_PUBLIC|PH7_MOD_STATIC|PH7_MOD_ABSTRACT, "", "array", 0 },
	};
	static const PH7_NativeMethodDef aBackedEnum[] = {
		{ "from",    PH7_MOD_PUBLIC|PH7_MOD_STATIC|PH7_MOD_ABSTRACT, "string|int $value",
		  "static", 0 },
		{ "tryFrom", PH7_MOD_PUBLIC|PH7_MOD_STATIC|PH7_MOD_ABSTRACT, "string|int $value",
		  "?static", 0 },
	};
	static const PH7_NativeMethodDef aIterator[] = {
		{ "current", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "@mixed", 0 },
		{ "next",    PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "@void", 0 },
		{ "key",     PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "@mixed", 0 },
		{ "valid",   PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "@bool", 0 },
		{ "rewind",  PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "@void", 0 },
	};
	static const PH7_NativeMethodDef aIteratorAggregate[] = {
		{ "getIterator", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "@Traversable", 0 },
	};
	/* php's legacy Serializable declares NO return type on either method. */
	static const PH7_NativeMethodDef aSerializable[] = {
		{ "serialize",   PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", 0, 0 },
		{ "unserialize", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "string $data", 0, 0 },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "Traversable", 0, 0, PH7_CLASS_INTERFACE,
		  0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "Stringable", 0, 0, PH7_CLASS_INTERFACE,
		  aStringable, SX_ARRAYSIZE(aStringable), 0, 0, 0, 0, 0, 0, 0 },
		{ "Throwable", "Stringable", 0, PH7_CLASS_INTERFACE,
		  aThrowable, SX_ARRAYSIZE(aThrowable), 0, 0, 0, 0, 0, 0, 0 },
		{ "ArrayAccess", 0, 0, PH7_CLASS_INTERFACE,
		  aArrayAccess, SX_ARRAYSIZE(aArrayAccess), 0, 0, 0, 0, 0, 0, 0 },
		{ "Countable", 0, 0, PH7_CLASS_INTERFACE,
		  aCountable, SX_ARRAYSIZE(aCountable), 0, 0, 0, 0, 0, 0, 0 },
		{ "JsonSerializable", 0, 0, PH7_CLASS_INTERFACE,
		  aJsonSerializable, SX_ARRAYSIZE(aJsonSerializable), 0, 0, 0, 0, 0, 0, 0 },
		{ "UnitEnum", 0, 0, PH7_CLASS_INTERFACE,
		  aUnitEnum, SX_ARRAYSIZE(aUnitEnum), 0, 0, 0, 0, 0, 0, 0 },
		{ "BackedEnum", "UnitEnum", 0, PH7_CLASS_INTERFACE,
		  aBackedEnum, SX_ARRAYSIZE(aBackedEnum), 0, 0, 0, 0, 0, 0, 0 },
		{ "Iterator", "Traversable", 0, PH7_CLASS_INTERFACE,
		  aIterator, SX_ARRAYSIZE(aIterator), 0, 0, 0, 0, 0, 0, 0 },
		{ "IteratorAggregate", "Traversable", 0, PH7_CLASS_INTERFACE,
		  aIteratorAggregate, SX_ARRAYSIZE(aIteratorAggregate), 0, 0, 0, 0, 0, 0, 0 },
		{ "Serializable", 0, 0, PH7_CLASS_INTERFACE,
		  aSerializable, SX_ARRAYSIZE(aSerializable), 0, 0, 0, 0, 0, 0, 0 },
	};
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}
PH7_PRIVATE sxi32 PH7_VmInstallBuiltinLib(ph7_vm *pVm)
{
	SyString sBuiltin;
	SyString sRandom;
	/* The interfaces first: the chunk below declares classes that implement
	 * them (Exception implements Throwable). */
	VmInstallCoreInterfaces(&(*pVm));
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
