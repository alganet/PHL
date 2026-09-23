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
/* The eleven core INTERFACES and the whole Exception/Error family are declared
 * entirely from C (VmInstallCoreInterfaces / VmInstallExceptions below) -- they
 * have no presence in this chunk at all. A chunk cannot express what php declares
 * on them: a TENTATIVE return type (php marks nearly every interface method),
 * Throwable's `extends Stringable`, the exception family's FINAL getters and
 * private __clone, or a typed slot with no default (Error::$line). */
#define PH7_BUILTIN_LIB \
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
 * ---------------------------------------------------------------------------
 * The Exception / Error family, declared from C.
 *
 * php's two roots are one implementation twice over (its stub says
 * `@implementation-alias Exception::__construct` for every one of Error's
 * methods), so the bodies below are shared by both spec tables and the
 * ~20 subclasses are declaration-only rows.
 *
 * php's seven slots, in php's own declaration order. `string` is php's cache of
 * the __toString rendering -- unused by the engine but PRESENT on every
 * presentation surface, which is why it is declared here rather than skipped:
 * var_dump/print_r/(array)/serialize all show it, and PHL was one property short
 * of php on every exception ever printed.
 * ---------------------------------------------------------------------------
 */
#define EXC_MESSAGE  "message"
#define EXC_STRING   "string"
#define EXC_CODE     "code"
#define EXC_FILE     "file"
#define EXC_LINE     "line"
#define EXC_TRACE    "trace"
#define EXC_PREVIOUS "previous"
#define EXC_SEVERITY "severity"
/*
 * Answer a declared slot the way php's getter does. Three of the seven CONVERT
 * rather than copy — getMessage()/getFile() answer a string and getLine() an int,
 * whatever the slot holds — and that shows twice: a subclass assigning
 * `$this->message = 5` reads back "5", and a slot __wakeup has DROPPED reads as
 * "" rather than null. The other four are verbatim copies (getCode() of that same
 * subclass really is the int).
 */
#define EXC_READ_RAW 0
#define EXC_READ_STR 1
#define EXC_READ_INT 2
static int VmExcReadSlot(ph7_context *pCtx,const char *zSlot,int iAs)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,zSlot) : 0;
	ph7_value sTmp;
	if( iAs == EXC_READ_RAW ){
		if( pVal ){
			ph7_result_value(pCtx,pVal);
		}else{
			ph7_result_null(pCtx);
		}
		return PH7_OK;
	}
	/* Through a COPY: converting the slot would rewrite the exception's state. */
	PH7_MemObjInit(pCtx->pVm,&sTmp);
	if( pVal ){
		PH7_MemObjStore(pVal,&sTmp);
	}
	if( iAs == EXC_READ_INT ){
		PH7_MemObjToInteger(&sTmp);
	}else{
		PH7_MemObjToString(&sTmp);
	}
	ph7_result_value(pCtx,&sTmp);
	PH7_MemObjRelease(&sTmp);
	return PH7_OK;
}
static int vm_builtin_Exception_getMessage(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return VmExcReadSlot(pCtx,EXC_MESSAGE,EXC_READ_STR);
}
static int vm_builtin_Exception_getCode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return VmExcReadSlot(pCtx,EXC_CODE,EXC_READ_RAW);
}
static int vm_builtin_Exception_getFile(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return VmExcReadSlot(pCtx,EXC_FILE,EXC_READ_STR);
}
static int vm_builtin_Exception_getLine(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return VmExcReadSlot(pCtx,EXC_LINE,EXC_READ_INT);
}
static int vm_builtin_Exception_getTrace(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return VmExcReadSlot(pCtx,EXC_TRACE,EXC_READ_RAW);
}
static int vm_builtin_Exception_getPrevious(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return VmExcReadSlot(pCtx,EXC_PREVIOUS,EXC_READ_RAW);
}
static int vm_builtin_ErrorException_getSeverity(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return VmExcReadSlot(pCtx,EXC_SEVERITY,EXC_READ_RAW);
}
/*
 * php's zend_update_exception_properties: each of the three is written only when
 * the caller actually supplied it — a message when the argument was PASSED (`""`
 * included), a code when it is NON-ZERO, a previous when it is an object. That is
 * not the same as writing the defaults: a subclass may redeclare
 * `protected $message = 'default'`, and php keeps it for `new Sub()`.
 */
static void VmExcInitProps(ph7_context *pCtx,ph7_class_instance *pThis,int nArg,
	ph7_value **apArg,int iPrev)
{
	if( nArg > 0 ){
		int nMsg = 0;
		const char *zMsg = ph7_value_to_string(apArg[0],&nMsg);
		PH7_NativeSetAttrStr(pCtx->pVm,pThis,EXC_MESSAGE,zMsg,nMsg);
	}
	if( nArg > 1 ){
		ph7_value sCode;
		PH7_MemObjInit(pCtx->pVm,&sCode);
		PH7_MemObjStore(apArg[1],&sCode);
		PH7_MemObjToInteger(&sCode);
		if( sCode.x.iVal != 0 ){
			PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_CODE,sCode.x.iVal);
		}
		PH7_MemObjRelease(&sCode);
	}
	/* php's `previous` is the LAST parameter of each constructor, and
	 * ErrorException's is #5 rather than #2. */
	if( nArg > iPrev && (apArg[iPrev]->iFlags & MEMOBJ_OBJ) && apArg[iPrev]->x.pOther ){
		PH7_NativeSetAttrObj(pCtx->pVm,pThis,EXC_PREVIOUS,
			(ph7_class_instance *)apArg[iPrev]->x.pOther);
	}
}
static int vm_builtin_Exception_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	if( pThis ){
		VmExcInitProps(pCtx,pThis,nArg,apArg,2);
	}
	return PH7_OK;
}
/*
 * ErrorException's own constructor: php's Exception three, then severity, then
 * the OPTIONAL file/line overrides. php's `?string $filename = null` /
 * `?int $line = null` mean "keep the creation site" — the chunk defaulted them to
 * __FILE__/__LINE__, which resolved against the EMBEDDED chunk and reported
 * `:MEMORY:` line 1 for every ErrorException that did not pass them. php's one
 * asymmetry: a filename WITHOUT a line resets the line to 0.
 */
static int vm_builtin_ErrorException_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	if( pThis == 0 ){
		return PH7_OK;
	}
	VmExcInitProps(pCtx,pThis,nArg,apArg,5);
	if( nArg > 2 ){
		PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_SEVERITY,ph7_value_to_int64(apArg[2]));
	}
	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){
		int nFile = 0;
		const char *zFile = ph7_value_to_string(apArg[3],&nFile);
		PH7_NativeSetAttrStr(pCtx->pVm,pThis,EXC_FILE,zFile,nFile);
		if( nArg < 5 || ph7_value_is_null(apArg[4]) ){
			PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_LINE,0);
		}
	}
	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){
		PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_LINE,ph7_value_to_int64(apArg[4]));
	}
	return PH7_OK;
}
/*
 * php's private __clone. It has an empty body and is never reached: the class
 * carries php's own clone refusal (PH7_CLASS_NOCLONE, answered before any body
 * runs), which is what `clone $e` reports — "Trying to clone an uncloneable
 * object of class X", not a visibility error. Declaring it is still php-visible:
 * Reflection lists it, and `$e->__clone()` from inside the class works.
 */
static int vm_builtin_Exception_clone(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_null(pCtx);
	return PH7_OK;
}
/*
 * php's __wakeup: the two UNTYPED slots are the only ones a serialized payload
 * can lie about (the other five are typed and the store enforces them), so php
 * DROPS a message that is not a string and a code that is not an int rather than
 * letting a method read one.
 */
static void VmExcDropSlot(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot)
{
	SyHashEntry *pEntry = SyHashGet(&pThis->hAttr,(const void *)zSlot,SyStrlen(zSlot));
	if( pEntry ){
		PH7_VmReleaseInstanceAttr(&(*pVm),(VmClassAttr *)pEntry->pUserData);
		SyHashDeleteEntry2(pEntry);
	}
}
static int vm_builtin_Exception_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pVal;
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pThis == 0 ){
		return PH7_OK;
	}
	pVal = PH7_NativeAttr(pThis,EXC_MESSAGE);
	if( pVal && (pVal->iFlags & MEMOBJ_NULL) == 0 && (pVal->iFlags & MEMOBJ_STRING) == 0 ){
		VmExcDropSlot(pCtx->pVm,pThis,EXC_MESSAGE);
	}
	pVal = PH7_NativeAttr(pThis,EXC_CODE);
	if( pVal && (pVal->iFlags & MEMOBJ_NULL) == 0 && (pVal->iFlags & MEMOBJ_INT) == 0 ){
		VmExcDropSlot(pCtx->pVm,pThis,EXC_CODE);
	}
	ph7_result_null(pCtx);
	return PH7_OK;
}
/*
 * One argument of a trace frame, php's smart_str_append_scalar: a string is
 * single-quoted, ESCAPED (`\n`, `\xNN` for anything non-printable) and truncated
 * to 15 bytes with `...` inside the quotes; a float takes php's precision; an
 * enum case prints `Enum::Case`; and anything else is a bare word.
 */
#define EXC_ARG_MAX 15
static void VmExcTraceArg(ph7_vm *pVm,SyBlob *pOut,ph7_value *pArg)
{
	if( pArg == 0 || (pArg->iFlags & MEMOBJ_NULL) ){
		SyBlobAppend(pOut,"NULL",sizeof("NULL")-1);
		return;
	}
	if( pArg->iFlags & MEMOBJ_BOOL ){
		if( pArg->x.iVal ){
			SyBlobAppend(pOut,"true",sizeof("true")-1);
		}else{
			SyBlobAppend(pOut,"false",sizeof("false")-1);
		}
		return;
	}
	if( pArg->iFlags & MEMOBJ_HASHMAP ){
		SyBlobAppend(pOut,"Array",sizeof("Array")-1);
		return;
	}
	if( pArg->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pObj = (ph7_class_instance *)pArg->x.pOther;
		if( pObj && pObj->pClass && (pObj->pClass->iFlags & PH7_CLASS_ENUM) ){
			ph7_value *pName = PH7_NativeAttr(pObj,"name");
			SyBlobFormat(pOut,"%z::",&pObj->pClass->sName);
			if( pName ){
				SyBlobAppend(pOut,SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));
			}
			return;
		}
		SyBlobAppend(pOut,"Object(",sizeof("Object(")-1);
		if( pObj && pObj->pClass ){
			SyBlobFormat(pOut,"%z",&pObj->pClass->sName);
		}
		SyBlobAppend(pOut,")",sizeof(")")-1);
		return;
	}
	if( pArg->iFlags & MEMOBJ_STRING ){
		const char *z = (const char *)SyBlobData(&pArg->sBlob);
		sxu32 n = SyBlobLength(&pArg->sBlob);
		sxu32 nKeep = n > EXC_ARG_MAX ? EXC_ARG_MAX : n;
		sxu32 i;
		SyBlobAppend(pOut,"'",sizeof("'")-1);
		for( i = 0 ; i < nKeep ; i++ ){
			unsigned char c = (unsigned char)z[i];
			if( c >= 32 && c <= 126 && c != '\\' ){
				SyBlobAppend(pOut,(const void *)&z[i],sizeof(char));
				continue;
			}
			switch( c ){
				case '\n': SyBlobAppend(pOut,"\\n",2); break;
				case '\r': SyBlobAppend(pOut,"\\r",2); break;
				case '\t': SyBlobAppend(pOut,"\\t",2); break;
				case '\f': SyBlobAppend(pOut,"\\f",2); break;
				case '\v': SyBlobAppend(pOut,"\\v",2); break;
				case '\\': SyBlobAppend(pOut,"\\\\",2); break;
				case 27:   SyBlobAppend(pOut,"\\e",2); break;
				default:   SyBlobFormat(pOut,"\\x%02X",(int)c); break;
			}
		}
		if( n > nKeep ){
			SyBlobAppend(pOut,"...",sizeof("...")-1);
		}
		SyBlobAppend(pOut,"'",sizeof("'")-1);
		return;
	}
	{
		/* int / float / anything else: php prints the scalar itself. */
		ph7_value sTmp;
		PH7_MemObjInit(&(*pVm),&sTmp);
		PH7_MemObjStore(pArg,&sTmp);
		PH7_MemObjToString(&sTmp);
		SyBlobAppend(pOut,SyBlobData(&sTmp.sBlob),SyBlobLength(&sTmp.sBlob));
		PH7_MemObjRelease(&sTmp);
	}
}
/* An element of a trace frame, or NULL when the frame does not carry it. */
static ph7_value * VmExcFrameField(ph7_vm *pVm,ph7_hashmap *pFrame,const char *zField)
{
	ph7_hashmap_node *pNode = 0;
	ph7_value sKey;
	sxi32 rc;
	SyString sName;
	SyStringInitFromBuf(&sName,zField,SyStrlen(zField));
	PH7_MemObjInitFromString(&(*pVm),&sKey,&sName);
	rc = PH7_HashmapLookup(pFrame,&sKey,&pNode);
	PH7_MemObjRelease(&sKey);
	if( rc != SXRET_OK || pNode == 0 ){
		return 0;
	}
	return (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);
}
static void VmExcFrameStr(SyBlob *pOut,ph7_value *pVal)
{
	if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){
		SyBlobAppend(pOut,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));
	}
}
/* A slot's string form, taken through a COPY: converting the value in place
 * would rewrite the exception's own state. */
static void VmExcValueStr(ph7_vm *pVm,ph7_value *pVal,SyBlob *pOut)
{
	ph7_value sTmp;
	if( pVal == 0 ){
		return;
	}
	PH7_MemObjInit(&(*pVm),&sTmp);
	PH7_MemObjStore(pVal,&sTmp);
	PH7_MemObjToString(&sTmp);
	SyBlobAppend(pOut,SyBlobData(&sTmp.sBlob),SyBlobLength(&sTmp.sBlob));
	PH7_MemObjRelease(&sTmp);
}
/* Does the blob contain this literal? SyBlobSearch() is `#ifndef
 * PH7_DISABLE_BUILTIN_FUNC`, and the exception family exists in the tiny build
 * too, so the one search this file needs is spelled out. */
static int VmExcBlobHas(SyBlob *pBlob,const char *zPat,sxu32 nPat)
{
	const char *z = (const char *)SyBlobData(pBlob);
	sxu32 n = SyBlobLength(pBlob);
	sxu32 i;
	if( nPat == 0 || n < nPat ){
		return 0;
	}
	for( i = 0 ; i + nPat <= n ; i++ ){
		if( SyMemcmp((const void *)&z[i],(const void *)zPat,nPat) == 0 ){
			return 1;
		}
	}
	return 0;
}
/* php's `Z_OBJCE_P == zend_ce_type_error || == zend_ce_argument_count_error`:
 * the two classes whose message __toString finishes with " and defined". */
static int VmExcIsArgError(ph7_vm *pVm,ph7_class_instance *pExc)
{
	ph7_class *pClass = pExc ? pExc->pClass : 0;
	ph7_class *pType;
	if( pClass == 0 ){
		return 0;
	}
	pType = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,FALSE,0);
	if( pType && pClass == pType ){
		return 1;
	}
	pType = PH7_VmExtractClass(&(*pVm),"ArgumentCountError",sizeof("ArgumentCountError")-1,FALSE,0);
	return pType != 0 && pClass == pType;
}
/*
 * php's zend_trace_to_string: one `#N file(line): Class->method(args)` line per
 * frame, then `#N {main}` with NO trailing newline. A frame with no `file` is
 * php's `[internal function]: `.
 */
static void VmExcTraceString(ph7_vm *pVm,ph7_value *pTrace,SyBlob *pOut)
{
	ph7_hashmap *pMap;
	ph7_hashmap_node *pEntry;
	sxu32 nFrame = 0;
	if( pTrace && (pTrace->iFlags & MEMOBJ_HASHMAP) && pTrace->x.pOther ){
		pMap = (ph7_hashmap *)pTrace->x.pOther;
		/* Insertion order is pFirst then the pPrev chain (rule 12). */
		for( pEntry = pMap->pFirst ; pEntry ; pEntry = pEntry->pPrev ){
			ph7_value *pFrameVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);
			ph7_hashmap *pFrame;
			ph7_value *pFile;
			if( pFrameVal == 0 || (pFrameVal->iFlags & MEMOBJ_HASHMAP) == 0 ){
				continue;
			}
			pFrame = (ph7_hashmap *)pFrameVal->x.pOther;
			SyBlobFormat(pOut,"#%u ",nFrame);
			pFile = VmExcFrameField(&(*pVm),pFrame,"file");
			if( pFile && (pFile->iFlags & MEMOBJ_STRING) ){
				ph7_value *pLine = VmExcFrameField(&(*pVm),pFrame,"line");
				VmExcFrameStr(pOut,pFile);
				SyBlobFormat(pOut,"(%qd): ",
					(pLine && (pLine->iFlags & MEMOBJ_INT)) ? pLine->x.iVal : (sxi64)0);
			}else{
				SyBlobAppend(pOut,"[internal function]: ",sizeof("[internal function]: ")-1);
			}
			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"class"));
			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"type"));
			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"function"));
			SyBlobAppend(pOut,"(",sizeof("(")-1);
			{
				ph7_value *pArgs = VmExcFrameField(&(*pVm),pFrame,"args");
				if( pArgs && (pArgs->iFlags & MEMOBJ_HASHMAP) && pArgs->x.pOther ){
					ph7_hashmap *pArgMap = (ph7_hashmap *)pArgs->x.pOther;
					ph7_hashmap_node *pArg;
					int bFirst = 1;
					for( pArg = pArgMap->pFirst ; pArg ; pArg = pArg->pPrev ){
						if( !bFirst ){
							SyBlobAppend(pOut,", ",sizeof(", ")-1);
						}
						bFirst = 0;
						VmExcTraceArg(&(*pVm),pOut,
							(ph7_value *)SySetAt(&pVm->aMemObj,pArg->nValIdx));
					}
				}
			}
			SyBlobAppend(pOut,")\n",sizeof(")\n")-1);
			nFrame++;
		}
	}
	SyBlobFormat(pOut,"#%u {main}",nFrame);
}
static int vm_builtin_Exception_getTraceAsString(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SyBlob sOut;
	SXUNUSED(nArg); SXUNUSED(apArg);
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	VmExcTraceString(pCtx->pVm,pThis ? PH7_NativeAttr(pThis,EXC_TRACE) : 0,&sOut);
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/*
 * php's Exception::__toString.
 *
 *    C: message in file:line
 *    Stack trace:
 *    <trace>
 *
 * The PREVIOUS chain is part of the format and the ORDER is inverted: php builds
 * the string innermost-first and joins the shallower ones after `\n\nNext `, so
 * the root cause is printed first. The chunk answered a four-field space-joined
 * line instead — `file line code message` — which no php ever produced, and it is
 * what an uncaught exception, `echo $e` and `(string)$e` all show.
 *
 * The walk carries its ancestors on the C stack (rule 31): php protects each
 * object it visits and stops when it comes back round, and a `$a->previous = $b;
 * $b->previous = $a` pair must not spin.
 */
#define EXC_CHAIN_MAX 256
static int vm_builtin_Exception_toString(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *apChain[EXC_CHAIN_MAX];
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_vm *pVm = pCtx->pVm;
	SyBlob sOut;
	int nChain = 0;
	int i,j;
	SXUNUSED(nArg); SXUNUSED(apArg);
	while( pThis && nChain < EXC_CHAIN_MAX ){
		ph7_class_instance *pPrev;
		for( j = 0 ; j < nChain ; j++ ){
			if( apChain[j] == pThis ){
				pThis = 0;    /* already on the chain: php's recursion protection */
				break;
			}
		}
		if( pThis == 0 ){
			break;
		}
		apChain[nChain++] = pThis;
		pPrev = PH7_NativeAttrObj(pThis,EXC_PREVIOUS);
		pThis = pPrev;
	}
	/* php formats the SHALLOWEST first and pushes each one it has already built
	 * behind the next, so the printed order is inverted: the ROOT CAUSE leads and
	 * every caller follows it after `\n\nNext `. */
	SyBlobInit(&sOut,&pVm->sAllocator);
	for( i = 0 ; i < nChain ; i++ ){
		ph7_class_instance *pExc = apChain[i];
		ph7_value *pLine = PH7_NativeAttr(pExc,EXC_LINE);
		SyBlob sMsg;
		SyBlob sThis;
		SyBlobInit(&sMsg,&pVm->sAllocator);
		SyBlobInit(&sThis,&pVm->sAllocator);
		VmExcValueStr(pVm,PH7_NativeAttr(pExc,EXC_MESSAGE),&sMsg);
		/* php's one message rewrite: a TypeError/ArgumentCountError raised at a
		 * CALL SITE says "..., called in F on line N", and __toString finishes the
		 * sentence with " and defined". */
		if( VmExcIsArgError(pVm,pExc)
		 && VmExcBlobHas(&sMsg,", called in ",sizeof(", called in ")-1) ){
			SyBlobAppend(&sMsg," and defined",sizeof(" and defined")-1);
		}
		SyBlobFormat(&sThis,"%z",&pExc->pClass->sName);
		if( SyBlobLength(&sMsg) > 0 ){
			SyBlobAppend(&sThis,": ",sizeof(": ")-1);
			SyBlobAppend(&sThis,SyBlobData(&sMsg),SyBlobLength(&sMsg));
		}
		SyBlobAppend(&sThis," in ",sizeof(" in ")-1);
		VmExcFrameStr(&sThis,PH7_NativeAttr(pExc,EXC_FILE));
		SyBlobFormat(&sThis,":%qd\nStack trace:\n",
			(pLine && (pLine->iFlags & MEMOBJ_INT)) ? pLine->x.iVal : (sxi64)0);
		VmExcTraceString(pVm,PH7_NativeAttr(pExc,EXC_TRACE),&sThis);
		if( SyBlobLength(&sOut) > 0 ){
			SyBlobAppend(&sThis,"\n\nNext ",sizeof("\n\nNext ")-1);
			SyBlobAppend(&sThis,SyBlobData(&sOut),SyBlobLength(&sOut));
		}
		SyBlobReset(&sOut);
		SyBlobAppend(&sOut,SyBlobData(&sThis),SyBlobLength(&sThis));
		SyBlobRelease(&sMsg);
		SyBlobRelease(&sThis);
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/*
 * The declaration. php's two roots carry the same eleven methods and the same
 * seven slots; the only difference php's stub records is `Error::$line`, which
 * has NO default where Exception's is 0.
 *
 * PH7_CLASS_NOCLONE on EVERY row: php refuses `clone $e` outright, and a native
 * subclass does not inherit its parent's class flags (rule 29).
 */
#define EXC_METHODS(zCtor,xCtor) \
	{ "__clone",          PH7_MOD_PRIVATE, "", "void", vm_builtin_Exception_clone }, \
	{ "__construct",      PH7_MOD_PUBLIC, zCtor, 0, xCtor }, \
	{ "__wakeup",         PH7_MOD_PUBLIC, "", "@void", vm_builtin_Exception_wakeup }, \
	{ "getMessage",       PH7_MOD_PUBLIC|PH7_MOD_FINAL, "", "string", \
	  vm_builtin_Exception_getMessage }, \
	{ "getCode",          PH7_MOD_PUBLIC|PH7_MOD_FINAL, "", 0, \
	  vm_builtin_Exception_getCode }, \
	{ "getFile",          PH7_MOD_PUBLIC|PH7_MOD_FINAL, "", "string", \
	  vm_builtin_Exception_getFile }, \
	{ "getLine",          PH7_MOD_PUBLIC|PH7_MOD_FINAL, "", "int", \
	  vm_builtin_Exception_getLine }, \
	{ "getTrace",         PH7_MOD_PUBLIC|PH7_MOD_FINAL, "", "array", \
	  vm_builtin_Exception_getTrace }, \
	{ "getPrevious",      PH7_MOD_PUBLIC|PH7_MOD_FINAL, "", "?Throwable", \
	  vm_builtin_Exception_getPrevious }, \
	{ "getTraceAsString", PH7_MOD_PUBLIC|PH7_MOD_FINAL, "", "string", \
	  vm_builtin_Exception_getTraceAsString }, \
	{ "__toString",       PH7_MOD_PUBLIC, "", "string", vm_builtin_Exception_toString }
#define EXC_CTOR_SIG "string $message = \"\", int $code = 0, ?Throwable $previous = null"
/* php's seven slots, twice: the only difference between the two roots is
 * `Error::$line`, which php's stub declares with NO default where Exception's is
 * 0 (`PH7_NATIVE_VAL_NONE` — its hasDefaultValue() is false and the export
 * prints `protected int $line` bare). `message` and `code` are the two php leaves
 * UNTYPED, and its stub says why: BC, since a subclass may have assigned
 * anything to them. */
#define EXC_PROP_HEAD \
	{ EXC_MESSAGE,  PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 }, \
	{ EXC_STRING,   PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" }, \
	{ EXC_CODE,     PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 }, \
	{ EXC_FILE,     PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" }
#define EXC_PROP_TAIL \
	{ EXC_TRACE,    PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_ARRAY, 0, 0, 0.0 }, "array" }, \
	{ EXC_PREVIOUS, PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?Throwable" }
static sxi32 VmInstallExceptions(ph7_vm *pVm)
{
	static const PH7_NativeMethodDef aExcMethod[] = {
		EXC_METHODS(EXC_CTOR_SIG,vm_builtin_Exception_construct)
	};
	static const PH7_NativePropDef aExcProp[] = {
		EXC_PROP_HEAD,
		{ EXC_LINE, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, "int" },
		EXC_PROP_TAIL
	};
	static const PH7_NativePropDef aErrProp[] = {
		EXC_PROP_HEAD,
		{ EXC_LINE, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },
		EXC_PROP_TAIL
	};
	static const PH7_NativePropDef aErrExcProp[] = {
		{ EXC_SEVERITY, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 1, 0, 0.0 }, "int" },
	};
	static const PH7_NativeMethodDef aErrExcMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC,
		  "string $message = \"\", int $code = 0, int $severity = E_ERROR, "
		  "?string $filename = null, ?int $line = null, ?Throwable $previous = null", 0,
		  vm_builtin_ErrorException_construct },
		{ "getSeverity", PH7_MOD_PUBLIC|PH7_MOD_FINAL, "", "int",
		  vm_builtin_ErrorException_getSeverity },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "Exception", 0, "Throwable", PH7_CLASS_NOCLONE,
		  aExcMethod, SX_ARRAYSIZE(aExcMethod), 0, 0, aExcProp, SX_ARRAYSIZE(aExcProp), 0, 0, 0 },
		{ "Error", 0, "Throwable", PH7_CLASS_NOCLONE,
		  aExcMethod, SX_ARRAYSIZE(aExcMethod), 0, 0, aErrProp, SX_ARRAYSIZE(aErrProp), 0, 0, 0 },
		/* Zend's own subclasses, then ErrorException, then SPL's tree. Every row is
		 * declaration-only in php too. */
		{ "TypeError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "ArgumentCountError", "TypeError", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "ValueError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "FiberError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "AssertionError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "ArithmeticError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DivisionByZeroError", "ArithmeticError", 0, PH7_CLASS_NOCLONE,
		  0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "UnhandledMatchError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "CompileError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "ParseError", "CompileError", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "ErrorException", "Exception", 0, PH7_CLASS_NOCLONE,
		  aErrExcMethod, SX_ARRAYSIZE(aErrExcMethod), 0, 0,
		  aErrExcProp, SX_ARRAYSIZE(aErrExcProp), 0, 0, 0 },
		{ "LogicException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "RuntimeException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "BadFunctionCallException", "LogicException", 0, PH7_CLASS_NOCLONE,
		  0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "BadMethodCallException", "BadFunctionCallException", 0, PH7_CLASS_NOCLONE,
		  0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DomainException", "LogicException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "InvalidArgumentException", "LogicException", 0, PH7_CLASS_NOCLONE,
		  0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "LengthException", "LogicException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "OutOfRangeException", "LogicException", 0, PH7_CLASS_NOCLONE,
		  0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "OutOfBoundsException", "RuntimeException", 0, PH7_CLASS_NOCLONE,
		  0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "OverflowException", "RuntimeException", 0, PH7_CLASS_NOCLONE,
		  0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "RangeException", "RuntimeException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "UnderflowException", "RuntimeException", 0, PH7_CLASS_NOCLONE,
		  0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "UnexpectedValueException", "RuntimeException", 0, PH7_CLASS_NOCLONE,
		  0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "JsonException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	};
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}
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
/*
 * stdClass and Random\RandomException.
 *
 * stdClass is EMPTY in php too — it holds only dynamic properties — so the whole
 * declaration is the row. `Random\RandomException` is the first NAMESPACED class
 * declared from C: the engine keys its class table by the FULLY QUALIFIED name
 * (the compiler resolves `namespace Random { class RandomException }` to exactly
 * this string before installing), so a spec row spells the FQN and needs no
 * namespace machinery at all. It also retires the chunk this file kept ALONE for
 * it, whose comment explains why: a `namespace` declaration is not reset at its
 * closing brace here, so anything following it in the same chunk would have
 * leaked into the Random namespace.
 */
static sxi32 VmInstallStdClasses(ph7_vm *pVm)
{
	static const PH7_NativeClassSpec aSpec[] = {
		{ "stdClass", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "Random\\RandomException", "Exception", 0, PH7_CLASS_NOCLONE,
		  0, 0, 0, 0, 0, 0, 0, 0, 0 },
	};
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}
PH7_PRIVATE sxi32 PH7_VmInstallBuiltinLib(ph7_vm *pVm)
{
	SyString sBuiltin;
	/* The interfaces first: everything below implements one of them
	 * (Exception implements Throwable). */
	VmInstallCoreInterfaces(&(*pVm));
	VmInstallExceptions(&(*pVm));
	VmInstallStdClasses(&(*pVm));
	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);
	/* Compile the built-in library */
	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);
	return SXRET_OK;
}
