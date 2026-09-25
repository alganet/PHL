# src/ph7/vm_builtin_lib.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 401/463 lines (86.61%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `/*` |
|       - |    8 | ` * The embedded PHP source of the core built-in class library (Exception and` |
|       - |    9 | ` * friends, ArrayAccess/Countable/..., Closure, Fiber, Generator shells),` |
|       - |   10 | ` * compiled at VM init by PH7_VmInstallBuiltinLib() — the bootstrap step` |
|       - |   11 | ` * PH7_VmInit runs right after the code generator comes up. Split from vm.c` |
|       - |   12 | ` * so the chunk string and its sizeof stay in one translation unit (the` |
|       - |   13 | ` * vm_builtin_reflection_lib.c pattern).` |
|       - |   14 | ` */` |
|       - |   15 | `/* The eleven core INTERFACES and the whole Exception/Error family are declared` |
|       - |   16 | ` * entirely from C (VmInstallCoreInterfaces / VmInstallExceptions below) -- they` |
|       - |   17 | ` * have no presence in this chunk at all. A chunk cannot express what php declares` |
|       - |   18 | ` * on them: a TENTATIVE return type (php marks nearly every interface method),` |
|       - |   19 | `` * Throwable's `extends Stringable`, the exception family's FINAL getters and`` |
|       - |   20 | ` * private __clone, or a typed slot with no default (Error::$line). */` |
|       - |   21 | `#define PH7_BUILTIN_LIB \` |
|       - |   22 | `	/* Fiber and Generator are declared ENTIRELY from C — class, private slots and` |
|       - |   23 | `	 * every method — by PH7_VmInstallFiberNative / PH7_VmInstallGeneratorNative.` |
|       - |   24 | `	 * They are the first two builtin classes with no presence in this chunk at all.` |
|       - |   25 | ``	 * Generator's `implements Iterator` is attached there too, and has to be: it is`` |
|       - |   26 | `	 * applied AFTER its methods exist, because PH7_ClassImplement installs an` |
|       - |   27 | `	 * ABSTRACT stub for every interface method a class does not already declare. */\` |
|       - |   28 | `	/* Closure is declared ENTIRELY in C by PH7_VmInstallClosureNative() (vm_exec_ctx.c):` |
|       - |   29 | `	 * class, the three engine slots (hidden, as php presents no property) and all five` |
|       - |   30 | `	 * methods. It cannot live here — a chunk-declared property is on every presentation` |
|       - |   31 | ``	 * surface, and `call()` in PHP leaked get_class()'s own TypeError text. */\`` |
|       - |   32 | `	/* stdClass is empty (PHP-exact): holds only dynamic (runtime-added) properties. */\` |
|       - |   33 | `	"function scandir(string $directory,int $sorting_order = SCANDIR_SORT_ASCENDING, $context = null)"\` |
|       - |   34 | `    "{"\` |
|       - |   35 | `	"  /* php's Z_PARAM_PATH refusal, spelled here because this builtin is prelude PHP:"\` |
|       - |   36 | `	"     the C screen (VmBuiltinPathMask) reaches host builtins only, so without this"\` |
|       - |   37 | `	"     the NUL reached opendir() and the ValueError named opendir(), not scandir(). */"\` |
|       - |   38 | `	"  if( strpos($directory, chr(0)) !== false ){"\` |
|       - |   39 | `	"    throw new ValueError('scandir(): Argument #1 ($directory) must not contain any null bytes');"\` |
|       - |   40 | `	"  }"\` |
|       - |   41 | ``	"  /* php's `?resource $context` refusal, spelled here for the same reason the"\`` |
|       - |   42 | `	"     NUL check above is: forwarding an invalid one to opendir() would name"\` |
|       - |   43 | `	"     opendir() in a message php raises against scandir(). */"\` |
|       - |   44 | `	"  if( $context !== null ){"\` |
|       - |   45 | `	"    if( !is_resource($context) ){"\` |
|       - |   46 | `	"      throw new TypeError('scandir(): Argument #3 ($context) must be of type resource or null, '"\` |
|       - |   47 | `	"        . get_debug_type($context) . ' given');"\` |
|       - |   48 | `	"    }"\` |
|       - |   49 | `	"    if( get_resource_type($context) !== 'stream-context' ){"\` |
|       - |   50 | `	"      throw new TypeError('scandir(): supplied resource is not a valid Stream-Context resource');"\` |
|       - |   51 | `	"    }"\` |
|       - |   52 | `	"  }"\` |
|       - |   53 | `	"  $aDir = array();"\` |
|       - |   54 | `	"  /* php hands the context straight to the open it performs; dropping it here"\` |
|       - |   55 | `	"     was the same unread argument every C opener had. */"\` |
|       - |   56 | `	"  $pHandle = opendir($directory, $context);"\` |
|       - |   57 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|       - |   58 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|       - |   59 | `	"      $aDir[] = $pEntry;"\` |
|       - |   60 | `	"   }"\` |
|       - |   61 | `	"  closedir($pHandle);"\` |
|       - |   62 | `	"  /* php's rule is a two-way split, not a three-value enum: SORT_NONE leaves the"\` |
|       - |   63 | `	"     order alone and EVERY other value sorts -- ascending only for the exact"\` |
|       - |   64 | `	"     SORT_ASCENDING, descending otherwise. PHL left an unknown value UNSORTED,"\` |
|       - |   65 | `	"     which reads as SORT_NONE. */"\` |
|       - |   66 | `	"  if( $sorting_order != SCANDIR_SORT_NONE ){"\` |
|       - |   67 | `	"      if( $sorting_order == SCANDIR_SORT_ASCENDING ){ sort($aDir); }"\` |
|       - |   68 | `	"      else { rsort($aDir); }"\` |
|       - |   69 | `	"  }"\` |
|       - |   70 | `	"  return $aDir;"\` |
|       - |   71 | `	"}"\` |
|       - |   72 | `	"function glob(string $pattern,int $flags = 0){"\` |
|       - |   73 | `	"/* php's Z_PARAM_PATH refusal (see scandir above). It precedes the flag check:"\` |
|       - |   74 | `	"   php's ZPP runs before the function body. Without it the NUL was simply the end"\` |
|       - |   75 | `	"   of the pattern and glob() answered for the truncated one. */"\` |
|       - |   76 | `	"if( strpos($pattern, chr(0)) !== false ){"\` |
|       - |   77 | `	"  throw new ValueError('glob(): Argument #1 ($pattern) must not contain any null bytes');"\` |
|       - |   78 | `	"}"\` |
|       - |   79 | `	"/* php rejects a mask holding any bit outside GLOB_AVAILABLE_FLAGS with a warning"\` |
|       - |   80 | `	"   and FALSE. PHL accepted anything and just tested the bits it knew, so a stale"\` |
|       - |   81 | `	"   script passing the OLD PHL glob values (1/2/4/...) silently got a plain glob. */"\` |
|       - |   82 | `	"if( $flags & ~GLOB_AVAILABLE_FLAGS ){"\` |
|       - |   83 | `	"  trigger_error('glob(): At least one of the passed flags is invalid or not supported on this platform', E_USER_WARNING);"\` |
|       - |   84 | `	"  return FALSE;"\` |
|       - |   85 | `	"}"\` |
|       - |   86 | `	"/* GLOB_BRACE: expand the FIRST top-level {a,b,...} group and glob each"\` |
|       - |   87 | `	"   alternative IN ORDER, concatenating the answers (each sub-glob sorts its"\` |
|       - |   88 | `	"   own results; php never re-sorts across alternatives). Nested groups are"\` |
|       - |   89 | `	"   handled by the recursion, and GLOB_NOCHECK applies per EXPANDED pattern,"\` |
|       - |   90 | `	"   which is php's answer too. The flag used to be accepted and IGNORED, so"\` |
|       - |   91 | `	"   any braced pattern answered [] in silence. */"\` |
|       - |   92 | `	"if( $flags & GLOB_BRACE ){"\` |
|       - |   93 | `	"  $nLen = strlen($pattern); $iOpen = -1; $iClose = -1; $iDepth = 0;"\` |
|       - |   94 | `	"  for( $i = 0 ; $i < $nLen ; $i++ ){"\` |
|       - |   95 | `	"    $ch = $pattern[$i];"\` |
|       - |   96 | `	"    if( $ch === '{' ){ if( $iDepth === 0 ){ $iOpen = $i; } $iDepth++; }"\` |
|       - |   97 | `	"    else if( $ch === '}' && $iDepth > 0 ){ $iDepth--; if( $iDepth === 0 ){ $iClose = $i; break; } }"\` |
|       - |   98 | `	"  }"\` |
|       - |   99 | `	"  if( $iOpen >= 0 && $iClose > $iOpen ){"\` |
|       - |  100 | `	"    $zHead = substr($pattern,0,$iOpen);"\` |
|       - |  101 | `	"    $zBody = (string)substr($pattern,$iOpen+1,$iClose-$iOpen-1);"\` |
|       - |  102 | `	"    $zTail = (string)substr($pattern,$iClose+1);"\` |
|       - |  103 | `	"    $aAlt = array(); $zCur = ''; $iDepth = 0;"\` |
|       - |  104 | `	"    for( $i = 0 ; $i < strlen($zBody) ; $i++ ){"\` |
|       - |  105 | `	"      $ch = $zBody[$i];"\` |
|       - |  106 | `	"      if( $ch === '{' ){ $iDepth++; }"\` |
|       - |  107 | `	"      else if( $ch === '}' ){ $iDepth--; }"\` |
|       - |  108 | `	"      if( $ch === ',' && $iDepth === 0 ){ $aAlt[] = $zCur; $zCur = ''; continue; }"\` |
|       - |  109 | `	"      $zCur .= $ch;"\` |
|       - |  110 | `	"    }"\` |
|       - |  111 | `	"    $aAlt[] = $zCur;"\` |
|       - |  112 | `	"    $pArray = array();"\` |
|       - |  113 | `	"    foreach( $aAlt as $zAlt ){"\` |
|       - |  114 | `	"      $aSub = glob($zHead . $zAlt . $zTail,$flags);"\` |
|       - |  115 | `	"      if( $aSub !== false ){ foreach( $aSub as $zHit ){ $pArray[] = $zHit; } }"\` |
|       - |  116 | `	"    }"\` |
|       - |  117 | `	"    return $pArray;"\` |
|       - |  118 | `	"  }"\` |
|       - |  119 | `	"}"\` |
|       - |  120 | `	"/* php keeps the literal directory portion of the pattern in every result;"\` |
|       - |  121 | `	"   split off everything up to and including the last '/' as the prefix. */"\` |
|       - |  122 | `	"$slash = strrpos($pattern,'/');"\` |
|       - |  123 | `	"if( $slash === false ){ $zDir = '.'; $prefix = ''; $pat = $pattern; }"\` |
|       - |  124 | `	"else { $zDir = substr($pattern,0,$slash); if( $zDir === '' ){ $zDir = '/'; } $prefix = substr($pattern,0,$slash+1); $pat = substr($pattern,$slash+1); }"\` |
|       - |  125 | `	"$pArray = array(); /* Empty array */"\` |
|       - |  126 | `	"/* php answers [] in SILENCE for a directory that cannot be opened — a"\` |
|       - |  127 | `	"   nonexistent path is simply zero matches (GLOB_ERR included; that flag is"\` |
|       - |  128 | `	"   about errors during the walk, not about the path). PHL used to let"\` |
|       - |  129 | `	"   opendir() warn and answered FALSE. */"\` |
|       - |  130 | `	"$pHandle = @opendir($zDir);"\` |
|       - |  131 | `	"if( $pHandle != FALSE ){"\` |
|       - |  132 | `	"/* Loop throw available entries */"\` |
|       - |  133 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|       - |  134 | `	" /* php's glob() never matches a leading-dot entry (incl. '.' and '..') unless"\` |
|       - |  135 | `	"    the pattern itself starts with a dot */"\` |
|       - |  136 | `	"	if( strlen($pEntry) > 0 && $pEntry[0] === '.' && (strlen($pat) < 1 \|\| $pat[0] !== '.') ){ continue; }"\` |
|       - |  137 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|       - |  138 | `	"	$rc = strglob($pat,$pEntry);"\` |
|       - |  139 | `	"	if( $rc ){"\` |
|       - |  140 | `	"	   $zFull = $prefix . $pEntry;"\` |
|       - |  141 | `	"	   if( is_dir($zDir . '/' . $pEntry) ){"\` |
|       - |  142 | `	"	      if( $flags & GLOB_MARK ){"\` |
|       - |  143 | `	"		     /* Adds a slash to each directory returned */"\` |
|       - |  144 | `	"			 $zFull .= DIRECTORY_SEPARATOR;"\` |
|       - |  145 | `	"		  }"\` |
|       - |  146 | `	"	   }else if( $flags & GLOB_ONLYDIR ){"\` |
|       - |  147 | `	"	     /* Not a directory,ignore */"\` |
|       - |  148 | `	"		 continue;"\` |
|       - |  149 | `	"	   }"\` |
|       - |  150 | `	"	   /* Add the entry (with its literal directory prefix, php-style) */"\` |
|       - |  151 | `	"	   $pArray[] = $zFull;"\` |
|       - |  152 | `	"	}"\` |
|       - |  153 | `	" }"\` |
|       - |  154 | `	"/* Close the handle */"\` |
|       - |  155 | `	"closedir($pHandle);"\` |
|       - |  156 | `	"}"\` |
|       - |  157 | `	"if( ($flags & GLOB_NOSORT) == 0 ){"\` |
|       - |  158 | `	"  /* Sort the array */"\` |
|       - |  159 | `	"  sort($pArray);"\` |
|       - |  160 | `	"}"\` |
|       - |  161 | `	"if( ($flags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|       - |  162 | `	"  /* Return the search pattern if no files matching were found */"\` |
|       - |  163 | `	"  $pArray[] = $pattern;"\` |
|       - |  164 | `	"}"\` |
|       - |  165 | `	"/* Return the created array */"\` |
|       - |  166 | `	"return $pArray;"\` |
|       - |  167 | `   "}"\` |
|       - |  168 | `   "/* Creates a temporary file */"\` |
|       - |  169 | `   "function tmpfile(){"\` |
|       - |  170 | `   "  /* Extract the temp directory */"\` |
|       - |  171 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|       - |  172 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|       - |  173 | `   "    /* Use the current dir */"\` |
|       - |  174 | `   "    $zTempDir = '.';"\` |
|       - |  175 | `   "  }"\` |
|       - |  176 | `   "  /* Create the file */"\` |
|       - |  177 | `   "  $zPath = $zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12);"\` |
|       - |  178 | `   "  /* php CREATES the file and then opens it r+b, which is the mode"\` |
|       - |  179 | `   "   * stream_get_meta_data() reports back for it. */"\` |
|       - |  180 | `   "  fclose(fopen($zPath,'w'));"\` |
|       - |  181 | `   "  $pHandle = fopen($zPath,'r+b');"\` |
|       - |  182 | `   "  return $pHandle;"\` |
|       - |  183 | `   "}"\` |
|       - |  184 | `   "function is_nan($num){ $num = (float)$num; return $num != $num; }"\` |
|       - |  185 | `   "function is_infinite($num){ $num = (float)$num; return $num == INF \|\| $num == -INF; }"\` |
|       - |  186 | `   "function is_finite($num){ $num = (float)$num; return !is_nan($num) && !is_infinite($num); }"\` |
|       - |  187 | `   "/* Inverse of bin2hex() */"\` |
|       - |  188 | `   "function hex2bin($string){"\` |
|       - |  189 | `   "  $string = (string)$string;"\` |
|       - |  190 | `   "  $len = strlen($string);"\` |
|       - |  191 | `   "  if( $len % 2 !== 0 ){"\` |
|       - |  192 | `   "    trigger_error('hex2bin(): Hexadecimal input string must have an even length', E_USER_WARNING);"\` |
|       - |  193 | `   "    return false;"\` |
|       - |  194 | `   "  }"\` |
|       - |  195 | `   "  $out = '';"\` |
|       - |  196 | `   "  for( $i = 0 ; $i < $len ; $i += 2 ){"\` |
|       - |  197 | `   "    $pair = substr($string, $i, 2);"\` |
|       - |  198 | `   "    if( !ctype_xdigit($pair) ){"\` |
|       - |  199 | `   "      trigger_error('hex2bin(): Input string must be hexadecimal string', E_USER_WARNING);"\` |
|       - |  200 | `   "      return false;"\` |
|       - |  201 | `   "    }"\` |
|       - |  202 | `   "    $out = $out . chr(hexdec($pair));"\` |
|       - |  203 | `   "  }"\` |
|       - |  204 | `   "  return $out;"\` |
|       - |  205 | `   "}"\` |
|       - |  206 | ``   "/* Division that never throws: INF/-INF/NAN like php. The two `float`"\`` |
|       - |  207 | `   " * declarations are php's own: they are what refuses a non-numeric string"\` |
|       - |  208 | `   " * (an untyped $num1 cast to 0.0 and DIVIDED, so fdiv('abc',2) answered"\` |
|       - |  209 | `   " * float(0)), and what ReflectionFunction prints. */"\` |
|       - |  210 | `   "function fdiv(float $num1, float $num2): float {"\` |
|       - |  211 | `   "  if( $num2 == 0.0 ){"\` |
|       - |  212 | `   "    if( $num1 == 0.0 \|\| is_nan($num1) ){ return NAN; }"\` |
|       - |  213 | `   "    return $num1 > 0 ? INF : -INF;"\` |
|       - |  214 | `   "  }"\` |
|       - |  215 | `   "  return $num1 / $num2;"\` |
|       - |  216 | `   "}"\` |
|       - |  217 | `   "function checkdate($month, $day, $year){"\` |
|       - |  218 | `   "  $month = (int)$month; $day = (int)$day; $year = (int)$year;"\` |
|       - |  219 | `   "  if( $month < 1 \|\| $month > 12 \|\| $year < 1 \|\| $year > 32767 \|\| $day < 1 ){ return false; }"\` |
|       - |  220 | `   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\` |
|       - |  221 | `   "  $max = $days[$month - 1];"\` |
|       - |  222 | `   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) \|\| ($year % 400 === 0)) ){"\` |
|       - |  223 | `   "    $max = 29;"\` |
|       - |  224 | `   "  }"\` |
|       - |  225 | `   "  return $day <= $max;"\` |
|       - |  226 | `   "}"\` |
|       - |  227 | `   "function is_iterable($value){ return is_array($value) \|\| ($value instanceof Traversable); }"\` |
|       - |  228 | `   "function is_countable($value){ return is_array($value) \|\| ($value instanceof Countable); }"\` |
|       - |  229 | `   "function doubleval($value){ return (float)$value; }"\` |
|       - |  230 | `   "function array_count_values($array){"\` |
|       - |  231 | `   "  $out = array();"\` |
|       - |  232 | `   "  foreach( $array as $v ){"\` |
|       - |  233 | `   "    if( !is_int($v) && !is_string($v) ){"\` |
|       - |  234 | `   "      trigger_error('array_count_values(): Can only count string and integer values, entry skipped', E_USER_WARNING);"\` |
|       - |  235 | `   "      continue;"\` |
|       - |  236 | `   "    }"\` |
|       - |  237 | `   "    if( isset($out[$v]) ){ $out[$v] = $out[$v] + 1; } else { $out[$v] = 1; }"\` |
|       - |  238 | `   "  }"\` |
|       - |  239 | `   "  return $out;"\` |
|       - |  240 | `   "}"\` |
|       - |  241 | `   "function array_change_key_case($array, $case = CASE_LOWER){"\` |
|       - |  242 | `   "  $out = array();"\` |
|       - |  243 | `   "  foreach( $array as $k => $v ){"\` |
|       - |  244 | `   "    if( is_string($k) ){ $k = ($case == CASE_UPPER) ? strtoupper($k) : strtolower($k); }"\` |
|       - |  245 | `   "    $out[$k] = $v;"\` |
|       - |  246 | `   "  }"\` |
|       - |  247 | `   "  return $out;"\` |
|       - |  248 | `   "}"\` |
|       - |  249 | `   "function array_replace_recursive($array, ...$replacements){"\` |
|       - |  250 | `   "  foreach( $replacements as $o ){"\` |
|       - |  251 | `   "    foreach( $o as $k => $v ){"\` |
|       - |  252 | `   "      if( is_array($v) && isset($array[$k]) && is_array($array[$k]) ){"\` |
|       - |  253 | `   "        $array[$k] = array_replace_recursive($array[$k], $v);"\` |
|       - |  254 | `   "      }else{"\` |
|       - |  255 | `   "        $array[$k] = $v;"\` |
|       - |  256 | `   "      }"\` |
|       - |  257 | `   "    }"\` |
|       - |  258 | `   "  }"\` |
|       - |  259 | `   "  return $array;"\` |
|       - |  260 | `   "}"\` |
|       - |  261 | `   "function class_uses($object_or_class, $autoload = true){"\` |
|       - |  262 | `   "  $c = is_object($object_or_class) ? get_class($object_or_class) : (string)$object_or_class;"\` |
|       - |  263 | `   "  if( !class_exists($c) ){ return false; }"\` |
|       - |  264 | `   "  return array();  /* PHL has no traits yet -- always the empty set */"\` |
|       - |  265 | `   "}"\` |
|       - |  266 | `   "function ip2long($ip){"\` |
|       - |  267 | `   "  $p = explode('.', (string)$ip);"\` |
|       - |  268 | `   "  if( count($p) !== 4 ){ return false; }"\` |
|       - |  269 | `   "  $n = 0;"\` |
|       - |  270 | `   "  foreach( $p as $o ){"\` |
|       - |  271 | `   "    if( !ctype_digit($o) \|\| (int)$o < 0 \|\| (int)$o > 255 ){ return false; }"\` |
|       - |  272 | `   "    $n = $n * 256 + (int)$o;"\` |
|       - |  273 | `   "  }"\` |
|       - |  274 | `   "  return $n;"\` |
|       - |  275 | `   "}"\` |
|       - |  276 | `   "function long2ip($ip){"\` |
|       - |  277 | `   "  $n = (int)$ip;"\` |
|       - |  278 | `   "  return (($n >> 24) & 255) . '.' . (($n >> 16) & 255) . '.' . (($n >> 8) & 255) . '.' . ($n & 255);"\` |
|       - |  279 | `   "}"\` |
|       - |  280 | `   "function preg_filter($pattern, $replacement, $subject, $limit = -1, &$count = null){"\` |
|       - |  281 | `   "  /* php declares &$count and always writes it -- the total number of"\` |
|       - |  282 | `   "   * replacements across every subject, 0 when nothing matched. PHL never"\` |
|       - |  283 | `   "   * declared the parameter, so a caller reading it got its previous value. */"\` |
|       - |  284 | `   "  if( is_array($subject) ){"\` |
|       - |  285 | `   "    $total = 0;"\` |
|       - |  286 | `   "    $out = array();"\` |
|       - |  287 | `   "    foreach( $subject as $k => $v ){"\` |
|       - |  288 | `   "      $r = preg_replace($pattern, $replacement, (string)$v, $limit, $cnt);"\` |
|       - |  289 | `   "      $total = $total + $cnt;"\` |
|       - |  290 | `   "      if( $cnt > 0 ){ $out[$k] = $r; }"\` |
|       - |  291 | `   "    }"\` |
|       - |  292 | `   "    $count = $total;"\` |
|       - |  293 | `   "    return $out;"\` |
|       - |  294 | `   "  }"\` |
|       - |  295 | `   "  $r = preg_replace($pattern, $replacement, (string)$subject, $limit, $cnt);"\` |
|       - |  296 | `   "  $count = $cnt;"\` |
|       - |  297 | `   "  return $cnt > 0 ? $r : null;"\` |
|       - |  298 | `   "}"\` |
|       - |  299 | `   "function preg_replace_callback_array($pattern, $subject, $limit = -1, &$count = null, $flags = 0){"\` |
|       - |  300 | `   "  /* &$count is the total across every pattern; $flags shapes each callback's"\` |
|       - |  301 | `   "   * match array. php writes &$count only when the whole run SUCCEEDED -- a"\` |
|       - |  302 | `   "   * pattern that fails to compile answers null and leaves it untouched (an"\` |
|       - |  303 | `   "   * array subject is not a failure: it degrades to the empty array, count 0). */"\` |
|       - |  304 | `   "  $total = 0;"\` |
|       - |  305 | `   "  foreach( $pattern as $pat => $cb ){"\` |
|       - |  306 | `   "    $subject = preg_replace_callback($pat, $cb, $subject, $limit, $cnt, $flags);"\` |
|       - |  307 | `   "    if( $subject === null ){ return null; }"\` |
|       - |  308 | `   "    $total = $total + $cnt;"\` |
|       - |  309 | `   "  }"\` |
|       - |  310 | `   "  $count = $total;"\` |
|       - |  311 | `   "  return $subject;"\` |
|       - |  312 | `   "}"\` |
|       - |  313 | `   "function cal_days_in_month($calendar, $month, $year){"\` |
|       - |  314 | `   "  $month = (int)$month; $year = (int)$year;"\` |
|       - |  315 | `   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\` |
|       - |  316 | `   "  if( $month < 1 \|\| $month > 12 ){"\` |
|       - |  317 | `   "    throw new ValueError('cal_days_in_month(): Argument #2 ($month) must be a valid month');"\` |
|       - |  318 | `   "  }"\` |
|       - |  319 | `   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) \|\| ($year % 400 === 0)) ){"\` |
|       - |  320 | `   "    return 29;"\` |
|       - |  321 | `   "  }"\` |
|       - |  322 | `   "  return $days[$month - 1];"\` |
|       - |  323 | `   "}"\` |
|       - |  324 | `   "function preg_grep($pattern, $array, $flags = 0){"\` |
|       - |  325 | `   "  $out = array();"\` |
|       - |  326 | `   "  foreach( $array as $k => $v ){"\` |
|       - |  327 | `   "    $m = preg_match($pattern, (string)$v);"\` |
|       - |  328 | `   "    if( $flags & PREG_GREP_INVERT ){ $m = !$m; }"\` |
|       - |  329 | `   "    if( $m ){ $out[$k] = $v; }"\` |
|       - |  330 | `   "  }"\` |
|       - |  331 | `   "  return $out;"\` |
|       - |  332 | `   "}"\` |
|       - |  333 | `   "function class_implements($object_or_class, $autoload = true){"\` |
|       - |  334 | `   "  $c = is_object($object_or_class) ? get_class($object_or_class) : (string)$object_or_class;"\` |
|       - |  335 | `   "  if( !class_exists($c) && !interface_exists($c) ){ return false; }"\` |
|       - |  336 | `   "  $out = array();"\` |
|       - |  337 | `   "  $r = new ReflectionClass($c);"\` |
|       - |  338 | `   "  foreach( $r->getInterfaceNames() as $i ){ $out[$i] = $i; }"\` |
|       - |  339 | `   "  return $out;"\` |
|       - |  340 | `   "}"\` |
|       - |  341 | `   "function class_parents($object_or_class, $autoload = true){"\` |
|       - |  342 | `   "  $c = is_object($object_or_class) ? get_class($object_or_class) : (string)$object_or_class;"\` |
|       - |  343 | `   "  if( !class_exists($c) ){ return false; }"\` |
|       - |  344 | `   "  $out = array();"\` |
|       - |  345 | `   "  $r = new ReflectionClass($c);"\` |
|       - |  346 | `   "  while( ($p = $r->getParentClass()) ){"\` |
|       - |  347 | `   "    $n = $p->getName();"\` |
|       - |  348 | `   "    $out[$n] = $n;"\` |
|       - |  349 | `   "    $r = $p;"\` |
|       - |  350 | `   "  }"\` |
|       - |  351 | `   "  return $out;"\` |
|       - |  352 | `   "}"\` |
|       - |  353 | `   "/* php 8.3 str_increment(): Perl-style alphanumeric increment. */"\` |
|       - |  354 | `   "function str_increment($string){"\` |
|       - |  355 | `   "  $string = (string)$string;"\` |
|       - |  356 | `   "  if( $string === '' ){ throw new ValueError('str_increment(): Argument #1 ($string) must not be empty'); }"\` |
|       - |  357 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_increment(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|       - |  358 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|       - |  359 | `   "    $c = $string[$i];"\` |
|       - |  360 | `   "    if( $c === 'z' ){ $string[$i] = 'a'; }"\` |
|       - |  361 | `   "    elseif( $c === 'Z' ){ $string[$i] = 'A'; }"\` |
|       - |  362 | `   "    elseif( $c === '9' ){ $string[$i] = '0'; }"\` |
|       - |  363 | `   "    else { $string[$i] = chr(ord($c) + 1); return $string; }"\` |
|       - |  364 | `   "  }"\` |
|       - |  365 | `   "  $first = $string[0];"\` |
|       - |  366 | `   "  if( $first === '0' ){ return '1' . $string; }"\` |
|       - |  367 | `   "  if( $first === 'a' ){ return 'a' . $string; }"\` |
|       - |  368 | `   "  return 'A' . $string;"\` |
|       - |  369 | `   "}"\` |
|       - |  370 | `   "/* php 8.3 str_decrement(): inverse of str_increment(); throws out of range"\` |
|       - |  371 | `   " * at the bottom of the counting sequence. */"\` |
|       - |  372 | `   "function str_decrement($string){"\` |
|       - |  373 | `   "  $string = (string)$string;"\` |
|       - |  374 | `   "  if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) must not be empty'); }"\` |
|       - |  375 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_decrement(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|       - |  376 | `   "  $orig = $string;"\` |
|       - |  377 | `   "  $borrowed = false;"\` |
|       - |  378 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|       - |  379 | `   "    $c = $string[$i];"\` |
|       - |  380 | `   "    if( $c === 'a' ){ $string[$i] = 'z'; }"\` |
|       - |  381 | `   "    elseif( $c === 'A' ){ $string[$i] = 'Z'; }"\` |
|       - |  382 | `   "    elseif( $c === '0' ){ $string[$i] = '9'; }"\` |
|       - |  383 | `   "    else { $string[$i] = chr(ord($c) - 1); $borrowed = false; break; }"\` |
|       - |  384 | `   "    if( $i === 0 ){ $borrowed = true; }"\` |
|       - |  385 | `   "  }"\` |
|       - |  386 | `   "  if( $borrowed ){"\` |
|       - |  387 | `   "    if( $string[0] === '9' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|       - |  388 | `   "    $string = substr($string, 1);"\` |
|       - |  389 | `   "    if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|       - |  390 | `   "  } elseif( strlen($string) > 1 && $string[0] === '0' ){"\` |
|       - |  391 | `   "    $string = substr($string, 1);"\` |
|       - |  392 | `   "  }"\` |
|       - |  393 | `   "  return $string;"\` |
|       - |  394 | `   "}"\` |
|       - |  395 | `   /* fileperms/fileowner/filegroup/fileinode moved to C (vfs.c, VfsStatField):` |
|       - |  396 | `    * as prelude wrappers over stat() three of them said nothing on a failed stat` |
|       - |  397 | `    * and the fourth raised trigger_error, whose errno is E_USER_WARNING's 512 and` |
|       - |  398 | `    * whose line is this chunk's rather than the caller's. */\` |
|       - |  399 | `   "/* PH7 keeps no stat cache, so this is a no-op like php on a clean cache. */"\` |
|       - |  400 | `   "function clearstatcache($clear_realpath_cache = false, $filename = ''){}"\` |
|       - |  401 | `   /* mb_ucfirst/mb_lcfirst moved to C (builtin_mb.c): as prelude wrappers they` |
|       - |  402 | `    * dropped $encoding, UPPER-cased where php title-cases ('ß' -> 'SS' for php's` |
|       - |  403 | `    * 'Ss') and lowered a leading Σ with nothing after it, which is php's FINAL` |
|       - |  404 | `    * sigma and not what a first character gets. */\` |
|       - |  405 | `   "/* Creates a temporary file and returns its name */"\` |
|       - |  406 | `   "function tempnam(string $directory,string $prefix)"\` |
|       - |  407 | `   "{"\` |
|       - |  408 | `   "   /* php's Z_PARAM_PATH refusal on BOTH parameters (see scandir above); the prefix"\` |
|       - |  409 | `   "    * is a path fragment there too, and PHL used to build a filename with the NUL"\` |
|       - |  410 | `   "    * still in it. */"\` |
|       - |  411 | `   "   if( strpos($directory, chr(0)) !== false ){"\` |
|       - |  412 | `   "     throw new ValueError('tempnam(): Argument #1 ($directory) must not contain any null bytes');"\` |
|       - |  413 | `   "   }"\` |
|       - |  414 | `   "   if( strpos($prefix, chr(0)) !== false ){"\` |
|       - |  415 | `   "     throw new ValueError('tempnam(): Argument #2 ($prefix) must not contain any null bytes');"\` |
|       - |  416 | `   "   }"\` |
|       - |  417 | `   "   /* php CREATES the file (empty, mode 0600) and guarantees the name is unique --"\` |
|       - |  418 | `   "    * returning a bare name left the caller with a path that does not exist, so"\` |
|       - |  419 | `   "    * file_exists() was false and unlink() failed on it. */"\` |
|       - |  420 | `   "   $directory = rtrim($directory, DIRECTORY_SEPARATOR);"\` |
|       - |  421 | `   "   for( $i = 0 ; $i < 64 ; ++$i ){"\` |
|       - |  422 | `   "     $zPath = $directory.DIRECTORY_SEPARATOR.$prefix.rand_str(12);"\` |
|       - |  423 | `   "     if( file_exists($zPath) ){ continue; }"\` |
|       - |  424 | `   "     $pHandle = @fopen($zPath,'x');"\` |
|       - |  425 | `   "     if( $pHandle === false ){ continue; }"\` |
|       - |  426 | `   "     fclose($pHandle);"\` |
|       - |  427 | `   "     @chmod($zPath, 0600);"\` |
|       - |  428 | `   "     return $zPath;"\` |
|       - |  429 | `   "   }"\` |
|       - |  430 | `   "   return false;"\` |
|       - |  431 | `   "}"\` |
|       - |  432 | `	/* fileowner/filegroup/fileinode: see the note beside fileperms above. */\` |
|       - |  433 | `	""` |
|       - |  434 |  |
|       - |  435 | `/*` |
|       - |  436 | ` * ---------------------------------------------------------------------------` |
|       - |  437 | ` * The Exception / Error family, declared from C.` |
|       - |  438 | ` *` |
|       - |  439 | ` * php's two roots are one implementation twice over (its stub says` |
|       - |  440 | `` * `@implementation-alias Exception::__construct` for every one of Error's`` |
|       - |  441 | ` * methods), so the bodies below are shared by both spec tables and the` |
|       - |  442 | ` * ~20 subclasses are declaration-only rows.` |
|       - |  443 | ` *` |
|       - |  444 | `` * php's seven slots, in php's own declaration order. `string` is php's cache of`` |
|       - |  445 | ` * the __toString rendering -- unused by the engine but PRESENT on every` |
|       - |  446 | ` * presentation surface, which is why it is declared here rather than skipped:` |
|       - |  447 | ` * var_dump/print_r/(array)/serialize all show it, and PHL was one property short` |
|       - |  448 | ` * of php on every exception ever printed.` |
|       - |  449 | ` * ---------------------------------------------------------------------------` |
|       - |  450 | ` */` |
|       - |  451 | `#define EXC_MESSAGE  "message"` |
|       - |  452 | `#define EXC_STRING   "string"` |
|       - |  453 | `#define EXC_CODE     "code"` |
|       - |  454 | `#define EXC_FILE     "file"` |
|       - |  455 | `#define EXC_LINE     "line"` |
|       - |  456 | `#define EXC_TRACE    "trace"` |
|       - |  457 | `#define EXC_PREVIOUS "previous"` |
|       - |  458 | `#define EXC_SEVERITY "severity"` |
|       - |  459 | `/*` |
|       - |  460 | ` * Answer a declared slot the way php's getter does. Three of the seven CONVERT` |
|       - |  461 | ` * rather than copy — getMessage()/getFile() answer a string and getLine() an int,` |
|       - |  462 | ` * whatever the slot holds — and that shows twice: a subclass assigning` |
|       - |  463 | `` * `$this->message = 5` reads back "5", and a slot __wakeup has DROPPED reads as`` |
|       - |  464 | ` * "" rather than null. The other four are verbatim copies (getCode() of that same` |
|       - |  465 | ` * subclass really is the int).` |
|       - |  466 | ` */` |
|       - |  467 | `#define EXC_READ_RAW 0` |
|       - |  468 | `#define EXC_READ_STR 1` |
|       - |  469 | `#define EXC_READ_INT 2` |
|    7426 |  470 | `static int VmExcReadSlot(ph7_context *pCtx,const char *zSlot,int iAs)` |
|       5 |  471 | `{` |
|    7431 |  472 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    7431 |  473 | `	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,zSlot) : 0;` |
|       - |  474 | `	ph7_value sTmp;` |
|    7431 |  475 | `	if( iAs == EXC_READ_RAW ){` |
|      54 |  476 | `		if( pVal ){` |
|      54 |  477 | `			ph7_result_value(pCtx,pVal);` |
|      29 |  478 | `		}else{` |
|     ! 0 |  479 | `			ph7_result_null(pCtx);` |
|       - |  480 | `		}` |
|      54 |  481 | `		return PH7_OK;` |
|       - |  482 | `	}` |
|       - |  483 | `	/* Through a COPY: converting the slot would rewrite the exception's state. */` |
|    7381 |  484 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|    7381 |  485 | `	if( pVal ){` |
|    7381 |  486 | `		PH7_MemObjStore(pVal,&sTmp);` |
|    3688 |  487 | `	}` |
|    7381 |  488 | `	if( iAs == EXC_READ_INT ){` |
|     629 |  489 | `		PH7_MemObjToInteger(&sTmp);` |
|     317 |  490 | `	}else{` |
|    6757 |  491 | `		PH7_MemObjToString(&sTmp);` |
|       - |  492 | `	}` |
|    7381 |  493 | `	ph7_result_value(pCtx,&sTmp);` |
|    7381 |  494 | `	PH7_MemObjRelease(&sTmp);` |
|    7381 |  495 | `	return PH7_OK;` |
|    3718 |  496 | `}` |
|    6738 |  497 | `static int vm_builtin_Exception_getMessage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  498 | `{` |
|    3369 |  499 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    6743 |  500 | `	return VmExcReadSlot(pCtx,EXC_MESSAGE,EXC_READ_STR);` |
|       5 |  501 | `}` |
|      16 |  502 | `static int vm_builtin_Exception_getCode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  503 | `{` |
|       8 |  504 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      18 |  505 | `	return VmExcReadSlot(pCtx,EXC_CODE,EXC_READ_RAW);` |
|       2 |  506 | `}` |
|      14 |  507 | `static int vm_builtin_Exception_getFile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  508 | `{` |
|       7 |  509 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      17 |  510 | `	return VmExcReadSlot(pCtx,EXC_FILE,EXC_READ_STR);` |
|       3 |  511 | `}` |
|     624 |  512 | `static int vm_builtin_Exception_getLine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  513 | `{` |
|     312 |  514 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     629 |  515 | `	return VmExcReadSlot(pCtx,EXC_LINE,EXC_READ_INT);` |
|       5 |  516 | `}` |
|       8 |  517 | `static int vm_builtin_Exception_getTrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  518 | `{` |
|       4 |  519 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      11 |  520 | `	return VmExcReadSlot(pCtx,EXC_TRACE,EXC_READ_RAW);` |
|       3 |  521 | `}` |
|      22 |  522 | `static int vm_builtin_Exception_getPrevious(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  523 | `{` |
|      11 |  524 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      24 |  525 | `	return VmExcReadSlot(pCtx,EXC_PREVIOUS,EXC_READ_RAW);` |
|       2 |  526 | `}` |
|       4 |  527 | `static int vm_builtin_ErrorException_getSeverity(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  528 | `{` |
|       2 |  529 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       5 |  530 | `	return VmExcReadSlot(pCtx,EXC_SEVERITY,EXC_READ_RAW);` |
|       1 |  531 | `}` |
|       - |  532 | `/*` |
|       - |  533 | ` * php's zend_update_exception_properties: each of the three is written only when` |
|       - |  534 | ``  * the caller actually supplied it — a message when the argument was PASSED (`""` `` |
|       - |  535 | ` * included), a code when it is NON-ZERO, a previous when it is an object. That is` |
|       - |  536 | ` * not the same as writing the defaults: a subclass may redeclare` |
|       - |  537 | `` * `protected $message = 'default'`, and php keeps it for `new Sub()`.`` |
|       - |  538 | ` */` |
| 1456020 |  539 | `static void VmExcInitProps(ph7_context *pCtx,ph7_class_instance *pThis,int nArg,` |
|       - |  540 | `	ph7_value **apArg,int iPrev)` |
|       5 |  541 | `{` |
| 1456025 |  542 | `	if( nArg > 0 ){` |
| 1455951 |  543 | `		int nMsg = 0;` |
| 1455951 |  544 | `		const char *zMsg = ph7_value_to_string(apArg[0],&nMsg);` |
| 1455951 |  545 | `		PH7_NativeSetAttrStr(pCtx->pVm,pThis,EXC_MESSAGE,zMsg,nMsg);` |
|  727973 |  546 | `	}` |
| 1456025 |  547 | `	if( nArg > 1 ){` |
|       - |  548 | `		ph7_value sCode;` |
|      49 |  549 | `		PH7_MemObjInit(pCtx->pVm,&sCode);` |
|      49 |  550 | `		PH7_MemObjStore(apArg[1],&sCode);` |
|      49 |  551 | `		PH7_MemObjToInteger(&sCode);` |
|      49 |  552 | `		if( sCode.x.iVal != 0 ){` |
|      28 |  553 | `			PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_CODE,sCode.x.iVal);` |
|      13 |  554 | `		}` |
|      49 |  555 | `		PH7_MemObjRelease(&sCode);` |
|      23 |  556 | `	}` |
|       - |  557 | ``	/* php's `previous` is the LAST parameter of each constructor, and`` |
|       - |  558 | `	 * ErrorException's is #5 rather than #2. */` |
| 1456025 |  559 | `	if( nArg > iPrev && (apArg[iPrev]->iFlags & MEMOBJ_OBJ) && apArg[iPrev]->x.pOther ){` |
|      24 |  560 | `		PH7_NativeSetAttrObj(pCtx->pVm,pThis,EXC_PREVIOUS,` |
|      14 |  561 | `			(ph7_class_instance *)apArg[iPrev]->x.pOther);` |
|       7 |  562 | `	}` |
| 1456025 |  563 | `}` |
| 1456006 |  564 | `static int vm_builtin_Exception_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  565 | `{` |
| 1456011 |  566 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
| 1456011 |  567 | `	if( pThis ){` |
| 1456011 |  568 | `		VmExcInitProps(pCtx,pThis,nArg,apArg,2);` |
|  728003 |  569 | `	}` |
| 1456011 |  570 | `	return PH7_OK;` |
|       5 |  571 | `}` |
|       - |  572 | `/*` |
|       - |  573 | ` * ErrorException's own constructor: php's Exception three, then severity, then` |
|       - |  574 | `` * the OPTIONAL file/line overrides. php's `?string $filename = null` /`` |
|       - |  575 | `` * `?int $line = null` mean "keep the creation site" — the chunk defaulted them to`` |
|       - |  576 | ` * __FILE__/__LINE__, which resolved against the EMBEDDED chunk and reported` |
|       - |  577 | `` * `:MEMORY:` line 1 for every ErrorException that did not pass them. php's one`` |
|       - |  578 | ` * asymmetry: a filename WITHOUT a line resets the line to 0.` |
|       - |  579 | ` */` |
|      14 |  580 | `static int vm_builtin_ErrorException_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  581 | `{` |
|      15 |  582 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      15 |  583 | `	if( pThis == 0 ){` |
|     ! 0 |  584 | `		return PH7_OK;` |
|       - |  585 | `	}` |
|      15 |  586 | `	VmExcInitProps(pCtx,pThis,nArg,apArg,5);` |
|      15 |  587 | `	if( nArg > 2 ){` |
|      11 |  588 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_SEVERITY,ph7_value_to_int64(apArg[2]));` |
|       5 |  589 | `	}` |
|      15 |  590 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|       9 |  591 | `		int nFile = 0;` |
|       9 |  592 | `		const char *zFile = ph7_value_to_string(apArg[3],&nFile);` |
|       9 |  593 | `		PH7_NativeSetAttrStr(pCtx->pVm,pThis,EXC_FILE,zFile,nFile);` |
|       9 |  594 | `		if( nArg < 5 \|\| ph7_value_is_null(apArg[4]) ){` |
|       3 |  595 | `			PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_LINE,0);` |
|       1 |  596 | `		}` |
|       4 |  597 | `	}` |
|      15 |  598 | `	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|       7 |  599 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_LINE,ph7_value_to_int64(apArg[4]));` |
|       3 |  600 | `	}` |
|      15 |  601 | `	return PH7_OK;` |
|       8 |  602 | `}` |
|       - |  603 | `/*` |
|       - |  604 | ` * php's private __clone. It has an empty body and is never reached: the class` |
|       - |  605 | ` * carries php's own clone refusal (PH7_CLASS_NOCLONE, answered before any body` |
|       - |  606 | `` * runs), which is what `clone $e` reports — "Trying to clone an uncloneable`` |
|       - |  607 | ` * object of class X", not a visibility error. Declaring it is still php-visible:` |
|       - |  608 | `` * Reflection lists it, and `$e->__clone()` from inside the class works.`` |
|       - |  609 | ` */` |
|     ! 0 |  610 | `static int vm_builtin_Exception_clone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  611 | `{` |
|     ! 0 |  612 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  613 | `	ph7_result_null(pCtx);` |
|     ! 0 |  614 | `	return PH7_OK;` |
|     ! 0 |  615 | `}` |
|       - |  616 | `/*` |
|       - |  617 | ` * php's __wakeup: the two UNTYPED slots are the only ones a serialized payload` |
|       - |  618 | ` * can lie about (the other five are typed and the store enforces them), so php` |
|       - |  619 | ` * DROPS a message that is not a string and a code that is not an int rather than` |
|       - |  620 | ` * letting a method read one.` |
|       - |  621 | ` */` |
|     ! 0 |  622 | `static void VmExcDropSlot(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot)` |
|     ! 0 |  623 | `{` |
|     ! 0 |  624 | `	SyHashEntry *pEntry = SyHashGet(&pThis->hAttr,(const void *)zSlot,SyStrlen(zSlot));` |
|     ! 0 |  625 | `	if( pEntry ){` |
|     ! 0 |  626 | `		PH7_VmReleaseInstanceAttr(&(*pVm),(VmClassAttr *)pEntry->pUserData);` |
|     ! 0 |  627 | `		SyHashDeleteEntry2(pEntry);` |
|     ! 0 |  628 | `	}` |
|     ! 0 |  629 | `}` |
|     ! 0 |  630 | `static int vm_builtin_Exception_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  631 | `{` |
|     ! 0 |  632 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - |  633 | `	ph7_value *pVal;` |
|     ! 0 |  634 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  635 | `	if( pThis == 0 ){` |
|     ! 0 |  636 | `		return PH7_OK;` |
|       - |  637 | `	}` |
|     ! 0 |  638 | `	pVal = PH7_NativeAttr(pThis,EXC_MESSAGE);` |
|     ! 0 |  639 | `	if( pVal && (pVal->iFlags & MEMOBJ_NULL) == 0 && (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 |  640 | `		VmExcDropSlot(pCtx->pVm,pThis,EXC_MESSAGE);` |
|     ! 0 |  641 | `	}` |
|     ! 0 |  642 | `	pVal = PH7_NativeAttr(pThis,EXC_CODE);` |
|     ! 0 |  643 | `	if( pVal && (pVal->iFlags & MEMOBJ_NULL) == 0 && (pVal->iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 |  644 | `		VmExcDropSlot(pCtx->pVm,pThis,EXC_CODE);` |
|     ! 0 |  645 | `	}` |
|     ! 0 |  646 | `	ph7_result_null(pCtx);` |
|     ! 0 |  647 | `	return PH7_OK;` |
|     ! 0 |  648 | `}` |
|       - |  649 | `/*` |
|       - |  650 | ` * One argument of a trace frame, php's smart_str_append_scalar: a string is` |
|       - |  651 | `` * single-quoted, ESCAPED (`\n`, `\xNN` for anything non-printable) and truncated`` |
|       - |  652 | `` * to 15 bytes with `...` inside the quotes; a float takes php's precision; an`` |
|       - |  653 | `` * enum case prints `Enum::Case`; and anything else is a bare word.`` |
|       - |  654 | ` */` |
|       - |  655 | `#define EXC_ARG_MAX 15` |
|      62 |  656 | `static void VmExcTraceArg(ph7_vm *pVm,SyBlob *pOut,ph7_value *pArg)` |
|       1 |  657 | `{` |
|      63 |  658 | `	if( pArg == 0 \|\| (pArg->iFlags & MEMOBJ_NULL) ){` |
|       3 |  659 | `		SyBlobAppend(pOut,"NULL",sizeof("NULL")-1);` |
|       3 |  660 | `		return;` |
|       - |  661 | `	}` |
|      61 |  662 | `	if( pArg->iFlags & MEMOBJ_BOOL ){` |
|       5 |  663 | `		if( pArg->x.iVal ){` |
|       3 |  664 | `			SyBlobAppend(pOut,"true",sizeof("true")-1);` |
|       2 |  665 | `		}else{` |
|       3 |  666 | `			SyBlobAppend(pOut,"false",sizeof("false")-1);` |
|       - |  667 | `		}` |
|       5 |  668 | `		return;` |
|       - |  669 | `	}` |
|      57 |  670 | `	if( pArg->iFlags & MEMOBJ_HASHMAP ){` |
|       3 |  671 | `		SyBlobAppend(pOut,"Array",sizeof("Array")-1);` |
|       3 |  672 | `		return;` |
|       - |  673 | `	}` |
|      55 |  674 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|       3 |  675 | `		ph7_class_instance *pObj = (ph7_class_instance *)pArg->x.pOther;` |
|       3 |  676 | `		if( pObj && pObj->pClass && (pObj->pClass->iFlags & PH7_CLASS_ENUM) ){` |
|     ! 0 |  677 | `			ph7_value *pName = PH7_NativeAttr(pObj,"name");` |
|     ! 0 |  678 | `			SyBlobFormat(pOut,"%z::",&pObj->pClass->sName);` |
|     ! 0 |  679 | `			if( pName ){` |
|     ! 0 |  680 | `				SyBlobAppend(pOut,SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|     ! 0 |  681 | `			}` |
|     ! 0 |  682 | `			return;` |
|       - |  683 | `		}` |
|       3 |  684 | `		SyBlobAppend(pOut,"Object(",sizeof("Object(")-1);` |
|       3 |  685 | `		if( pObj && pObj->pClass ){` |
|       3 |  686 | `			SyBlobFormat(pOut,"%z",&pObj->pClass->sName);` |
|       1 |  687 | `		}` |
|       3 |  688 | `		SyBlobAppend(pOut,")",sizeof(")")-1);` |
|       3 |  689 | `		return;` |
|       - |  690 | `	}` |
|      53 |  691 | `	if( pArg->iFlags & MEMOBJ_STRING ){` |
|       - |  692 | `		/* php 8.5 does not put string CONTENT in a trace at all: every non-empty` |
|       - |  693 | `		 * one renders as '...' (the empty one still shows as ''), so a password` |
|       - |  694 | `		 * or a token passed to the function that threw cannot reach a log through` |
|       - |  695 | `		 * the trace. The truncate-at-15-and-escape shape here was php 8.4's. */` |
|       7 |  696 | `		if( SyBlobLength(&pArg->sBlob) < 1 ){` |
|       3 |  697 | `			SyBlobAppend(pOut,"''",sizeof("''")-1);` |
|       2 |  698 | `		}else{` |
|       5 |  699 | `			SyBlobAppend(pOut,"'...'",sizeof("'...'")-1);` |
|       - |  700 | `		}` |
|       7 |  701 | `		return;` |
|       - |  702 | `	}` |
|       - |  703 | `	{` |
|       - |  704 | `		/* int / float / anything else: php prints the scalar itself -- but a` |
|       - |  705 | `		 * trace FLOAT always shows its fraction (1.0, not the "1" the ordinary` |
|       - |  706 | `		 * string cast produces), which is what tells a float argument apart from` |
|       - |  707 | `		 * an int one. INF/NAN and the exponent forms already carry a marker. */` |
|       - |  708 | `		ph7_value sTmp;` |
|       - |  709 | `		const char *z;` |
|       - |  710 | `		sxu32 n,i;` |
|      47 |  711 | `		int bMarked = 0;` |
|      47 |  712 | `		PH7_MemObjInit(&(*pVm),&sTmp);` |
|      47 |  713 | `		PH7_MemObjStore(pArg,&sTmp);` |
|      47 |  714 | `		PH7_MemObjToString(&sTmp);` |
|      47 |  715 | `		z = (const char *)SyBlobData(&sTmp.sBlob);` |
|      47 |  716 | `		n = SyBlobLength(&sTmp.sBlob);` |
|      47 |  717 | `		SyBlobAppend(pOut,z,n);` |
|      47 |  718 | `		if( pArg->iFlags & MEMOBJ_REAL ){` |
|      23 |  719 | `			for( i = 0 ; i < n ; ++i ){` |
|      19 |  720 | `				if( z[i] < '0' \|\| z[i] > '9' ){` |
|      11 |  721 | `					if( z[i] != '-' && z[i] != '+' ){` |
|       9 |  722 | `						bMarked = 1;` |
|       9 |  723 | `						break;` |
|       - |  724 | `					}` |
|       1 |  725 | `				}` |
|       6 |  726 | `			}` |
|      13 |  727 | `			if( !bMarked ){` |
|       5 |  728 | `				SyBlobAppend(pOut,".0",sizeof(".0")-1);` |
|       2 |  729 | `			}` |
|       6 |  730 | `		}` |
|      47 |  731 | `		PH7_MemObjRelease(&sTmp);` |
|       - |  732 | `	}` |
|      32 |  733 | `}` |
|       - |  734 | `/* An element of a trace frame, or NULL when the frame does not carry it. */` |
|     612 |  735 | `static ph7_value * VmExcFrameField(ph7_vm *pVm,ph7_hashmap *pFrame,const char *zField)` |
|       5 |  736 | `{` |
|     617 |  737 | `	ph7_hashmap_node *pNode = 0;` |
|       - |  738 | `	ph7_value sKey;` |
|       - |  739 | `	sxi32 rc;` |
|       - |  740 | `	SyString sName;` |
|     617 |  741 | `	SyStringInitFromBuf(&sName,zField,SyStrlen(zField));` |
|     617 |  742 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,&sName);` |
|     617 |  743 | `	rc = PH7_HashmapLookup(pFrame,&sKey,&pNode);` |
|     617 |  744 | `	PH7_MemObjRelease(&sKey);` |
|     617 |  745 | `	if( rc != SXRET_OK \|\| pNode == 0 ){` |
|     223 |  746 | `		return 0;` |
|       - |  747 | `	}` |
|     399 |  748 | `	return (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|     311 |  749 | `}` |
|     414 |  750 | `static void VmExcFrameStr(SyBlob *pOut,ph7_value *pVal)` |
|       5 |  751 | `{` |
|     419 |  752 | `	if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){` |
|     255 |  753 | `		SyBlobAppend(pOut,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|     125 |  754 | `	}` |
|     419 |  755 | `}` |
|       - |  756 | `/* A slot's string form, taken through a COPY: converting the value in place` |
|       - |  757 | ` * would rewrite the exception's own state. */` |
|       6 |  758 | `static void VmExcValueStr(ph7_vm *pVm,ph7_value *pVal,SyBlob *pOut)` |
|       1 |  759 | `{` |
|       - |  760 | `	ph7_value sTmp;` |
|       7 |  761 | `	if( pVal == 0 ){` |
|     ! 0 |  762 | `		return;` |
|       - |  763 | `	}` |
|       7 |  764 | `	PH7_MemObjInit(&(*pVm),&sTmp);` |
|       7 |  765 | `	PH7_MemObjStore(pVal,&sTmp);` |
|       7 |  766 | `	PH7_MemObjToString(&sTmp);` |
|       7 |  767 | `	SyBlobAppend(pOut,SyBlobData(&sTmp.sBlob),SyBlobLength(&sTmp.sBlob));` |
|       7 |  768 | `	PH7_MemObjRelease(&sTmp);` |
|       4 |  769 | `}` |
|       - |  770 | ``/* Does the blob contain this literal? SyBlobSearch() is `#ifndef`` |
|       - |  771 | `` * PH7_DISABLE_BUILTIN_FUNC`, and the exception family exists in the tiny build`` |
|       - |  772 | ` * too, so the one search this file needs is spelled out. */` |
|     ! 0 |  773 | `static int VmExcBlobHas(SyBlob *pBlob,const char *zPat,sxu32 nPat)` |
|     ! 0 |  774 | `{` |
|     ! 0 |  775 | `	const char *z = (const char *)SyBlobData(pBlob);` |
|     ! 0 |  776 | `	sxu32 n = SyBlobLength(pBlob);` |
|       - |  777 | `	sxu32 i;` |
|     ! 0 |  778 | `	if( nPat == 0 \|\| n < nPat ){` |
|     ! 0 |  779 | `		return 0;` |
|       - |  780 | `	}` |
|     ! 0 |  781 | `	for( i = 0 ; i + nPat <= n ; i++ ){` |
|     ! 0 |  782 | `		if( SyMemcmp((const void *)&z[i],(const void *)zPat,nPat) == 0 ){` |
|     ! 0 |  783 | `			return 1;` |
|       - |  784 | `		}` |
|     ! 0 |  785 | `	}` |
|     ! 0 |  786 | `	return 0;` |
|     ! 0 |  787 | `}` |
|       - |  788 | ``/* php's `Z_OBJCE_P == zend_ce_type_error \|\| == zend_ce_argument_count_error`:`` |
|       - |  789 | ` * the two classes whose message __toString finishes with " and defined". */` |
|       6 |  790 | `static int VmExcIsArgError(ph7_vm *pVm,ph7_class_instance *pExc)` |
|       1 |  791 | `{` |
|       7 |  792 | `	ph7_class *pClass = pExc ? pExc->pClass : 0;` |
|       - |  793 | `	ph7_class *pType;` |
|       7 |  794 | `	if( pClass == 0 ){` |
|     ! 0 |  795 | `		return 0;` |
|       - |  796 | `	}` |
|       7 |  797 | `	pType = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,FALSE,0);` |
|       7 |  798 | `	if( pType && pClass == pType ){` |
|     ! 0 |  799 | `		return 1;` |
|       - |  800 | `	}` |
|       7 |  801 | `	pType = PH7_VmExtractClass(&(*pVm),"ArgumentCountError",sizeof("ArgumentCountError")-1,FALSE,0);` |
|       7 |  802 | `	return pType != 0 && pClass == pType;` |
|       4 |  803 | `}` |
|       - |  804 | `/*` |
|       - |  805 | `` * php's zend_trace_to_string: one `#N file(line): Class->method(args)` line per`` |
|       - |  806 | `` * frame, then `#N {main}` with NO trailing newline. A frame with no `file` is`` |
|       - |  807 | `` * php's `[internal function]: `.`` |
|       - |  808 | ` */` |
|     648 |  809 | `PH7_PRIVATE void PH7_VmTraceToString(ph7_vm *pVm,ph7_value *pTrace,int bMainMarker,SyBlob *pOut)` |
|       5 |  810 | `{` |
|       - |  811 | `	ph7_hashmap *pMap;` |
|       - |  812 | `	ph7_hashmap_node *pEntry;` |
|     653 |  813 | `	sxu32 nFrame = 0;` |
|     653 |  814 | `	if( pTrace && (pTrace->iFlags & MEMOBJ_HASHMAP) && pTrace->x.pOther ){` |
|     653 |  815 | `		pMap = (ph7_hashmap *)pTrace->x.pOther;` |
|       - |  816 | `		/* Insertion order is pFirst then the pPrev chain (rule 12). */` |
|     755 |  817 | `		for( pEntry = pMap->pFirst ; pEntry ; pEntry = pEntry->pPrev ){` |
|     107 |  818 | `			ph7_value *pFrameVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|       - |  819 | `			ph7_hashmap *pFrame;` |
|       - |  820 | `			ph7_value *pFile;` |
|     107 |  821 | `			if( pFrameVal == 0 \|\| (pFrameVal->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 |  822 | `				continue;` |
|       - |  823 | `			}` |
|     107 |  824 | `			pFrame = (ph7_hashmap *)pFrameVal->x.pOther;` |
|     107 |  825 | `			SyBlobFormat(pOut,"#%u ",nFrame);` |
|     107 |  826 | `			pFile = VmExcFrameField(&(*pVm),pFrame,"file");` |
|     158 |  827 | `			if( pFile && (pFile->iFlags & MEMOBJ_STRING) ){` |
|     107 |  828 | `				ph7_value *pLine = VmExcFrameField(&(*pVm),pFrame,"line");` |
|     107 |  829 | `				VmExcFrameStr(pOut,pFile);` |
|     209 |  830 | `				SyBlobFormat(pOut,"(%qd): ",` |
|     102 |  831 | `					(pLine && (pLine->iFlags & MEMOBJ_INT)) ? pLine->x.iVal : (sxi64)0);` |
|      56 |  832 | `			}else{` |
|     ! 0 |  833 | `				SyBlobAppend(pOut,"[internal function]: ",sizeof("[internal function]: ")-1);` |
|       - |  834 | `			}` |
|     107 |  835 | `			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"class"));` |
|     107 |  836 | `			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"type"));` |
|     107 |  837 | `			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"function"));` |
|     107 |  838 | `			SyBlobAppend(pOut,"(",sizeof("(")-1);` |
|       - |  839 | `			{` |
|     107 |  840 | `				ph7_value *pArgs = VmExcFrameField(&(*pVm),pFrame,"args");` |
|     107 |  841 | `				if( pArgs && (pArgs->iFlags & MEMOBJ_HASHMAP) && pArgs->x.pOther ){` |
|      49 |  842 | `					ph7_hashmap *pArgMap = (ph7_hashmap *)pArgs->x.pOther;` |
|       - |  843 | `					ph7_hashmap_node *pArg;` |
|      49 |  844 | `					int bFirst = 1;` |
|     111 |  845 | `					for( pArg = pArgMap->pFirst ; pArg ; pArg = pArg->pPrev ){` |
|      63 |  846 | `						if( !bFirst ){` |
|      17 |  847 | `							SyBlobAppend(pOut,", ",sizeof(", ")-1);` |
|       8 |  848 | `						}` |
|      63 |  849 | `						bFirst = 0;` |
|      94 |  850 | `						VmExcTraceArg(&(*pVm),pOut,` |
|      62 |  851 | `							(ph7_value *)SySetAt(&pVm->aMemObj,pArg->nValIdx));` |
|      32 |  852 | `					}` |
|      24 |  853 | `				}` |
|       - |  854 | `			}` |
|     107 |  855 | `			SyBlobAppend(pOut,")\n",sizeof(")\n")-1);` |
|     107 |  856 | `			nFrame++;` |
|      56 |  857 | `		}` |
|     324 |  858 | `	}` |
|     653 |  859 | `	if( bMainMarker ){` |
|       - |  860 | `		/* getTraceAsString() ends on the bottom marker; debug_print_backtrace()` |
|       - |  861 | `		 * does not print one -- it stops after the last real frame. */` |
|     611 |  862 | `		SyBlobFormat(pOut,"#%u {main}",nFrame);` |
|     303 |  863 | `	}` |
|     653 |  864 | `}` |
|     600 |  865 | `static int vm_builtin_Exception_getTraceAsString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  866 | `{` |
|     605 |  867 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - |  868 | `	SyBlob sOut;` |
|     300 |  869 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     605 |  870 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     605 |  871 | `	PH7_VmTraceToString(pCtx->pVm,pThis ? PH7_NativeAttr(pThis,EXC_TRACE) : 0,TRUE,&sOut);` |
|     605 |  872 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     605 |  873 | `	SyBlobRelease(&sOut);` |
|     605 |  874 | `	return PH7_OK;` |
|       5 |  875 | `}` |
|       - |  876 | `/*` |
|       - |  877 | ` * php's Exception::__toString.` |
|       - |  878 | ` *` |
|       - |  879 | ` *    C: message in file:line` |
|       - |  880 | ` *    Stack trace:` |
|       - |  881 | ` *    <trace>` |
|       - |  882 | ` *` |
|       - |  883 | ` * The PREVIOUS chain is part of the format and the ORDER is inverted: php builds` |
|       - |  884 | `` * the string innermost-first and joins the shallower ones after `\n\nNext `, so`` |
|       - |  885 | ` * the root cause is printed first. The chunk answered a four-field space-joined` |
|       - |  886 | `` * line instead — `file line code message` — which no php ever produced, and it is`` |
|       - |  887 | `` * what an uncaught exception, `echo $e` and `(string)$e` all show.`` |
|       - |  888 | ` *` |
|       - |  889 | ` * The walk carries its ancestors on the C stack (rule 31): php protects each` |
|       - |  890 | `` * object it visits and stops when it comes back round, and a `$a->previous = $b;`` |
|       - |  891 | `` * $b->previous = $a` pair must not spin.`` |
|       - |  892 | ` */` |
|       - |  893 | `#define EXC_CHAIN_MAX 256` |
|       4 |  894 | `static int vm_builtin_Exception_toString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  895 | `{` |
|       - |  896 | `	ph7_class_instance *apChain[EXC_CHAIN_MAX];` |
|       5 |  897 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       5 |  898 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - |  899 | `	SyBlob sOut;` |
|       5 |  900 | `	int nChain = 0;` |
|       - |  901 | `	int i,j;` |
|       2 |  902 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      11 |  903 | `	while( pThis && nChain < EXC_CHAIN_MAX ){` |
|       - |  904 | `		ph7_class_instance *pPrev;` |
|       9 |  905 | `		for( j = 0 ; j < nChain ; j++ ){` |
|       3 |  906 | `			if( apChain[j] == pThis ){` |
|     ! 0 |  907 | `				pThis = 0;    /* already on the chain: php's recursion protection */` |
|     ! 0 |  908 | `				break;` |
|       - |  909 | `			}` |
|       2 |  910 | `		}` |
|       7 |  911 | `		if( pThis == 0 ){` |
|     ! 0 |  912 | `			break;` |
|       - |  913 | `		}` |
|       7 |  914 | `		apChain[nChain++] = pThis;` |
|       7 |  915 | `		pPrev = PH7_NativeAttrObj(pThis,EXC_PREVIOUS);` |
|       7 |  916 | `		pThis = pPrev;` |
|       1 |  917 | `	}` |
|       - |  918 | `	/* php formats the SHALLOWEST first and pushes each one it has already built` |
|       - |  919 | `	 * behind the next, so the printed order is inverted: the ROOT CAUSE leads and` |
|       - |  920 | ``	 * every caller follows it after `\n\nNext `. */`` |
|       5 |  921 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      11 |  922 | `	for( i = 0 ; i < nChain ; i++ ){` |
|       7 |  923 | `		ph7_class_instance *pExc = apChain[i];` |
|       7 |  924 | `		ph7_value *pLine = PH7_NativeAttr(pExc,EXC_LINE);` |
|       - |  925 | `		SyBlob sMsg;` |
|       - |  926 | `		SyBlob sThis;` |
|       7 |  927 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       7 |  928 | `		SyBlobInit(&sThis,&pVm->sAllocator);` |
|       7 |  929 | `		VmExcValueStr(pVm,PH7_NativeAttr(pExc,EXC_MESSAGE),&sMsg);` |
|       - |  930 | `		/* php's one message rewrite: a TypeError/ArgumentCountError raised at a` |
|       - |  931 | `		 * CALL SITE says "..., called in F on line N", and __toString finishes the` |
|       - |  932 | `		 * sentence with " and defined". */` |
|       6 |  933 | `		if( VmExcIsArgError(pVm,pExc)` |
|       4 |  934 | `		 && VmExcBlobHas(&sMsg,", called in ",sizeof(", called in ")-1) ){` |
|     ! 0 |  935 | `			SyBlobAppend(&sMsg," and defined",sizeof(" and defined")-1);` |
|     ! 0 |  936 | `		}` |
|       7 |  937 | `		SyBlobFormat(&sThis,"%z",&pExc->pClass->sName);` |
|       7 |  938 | `		if( SyBlobLength(&sMsg) > 0 ){` |
|       5 |  939 | `			SyBlobAppend(&sThis,": ",sizeof(": ")-1);` |
|       5 |  940 | `			SyBlobAppend(&sThis,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       2 |  941 | `		}` |
|       7 |  942 | `		SyBlobAppend(&sThis," in ",sizeof(" in ")-1);` |
|       7 |  943 | `		VmExcFrameStr(&sThis,PH7_NativeAttr(pExc,EXC_FILE));` |
|      10 |  944 | `		SyBlobFormat(&sThis,":%qd\nStack trace:\n",` |
|       6 |  945 | `			(pLine && (pLine->iFlags & MEMOBJ_INT)) ? pLine->x.iVal : (sxi64)0);` |
|       7 |  946 | `		PH7_VmTraceToString(pVm,PH7_NativeAttr(pExc,EXC_TRACE),TRUE,&sThis);` |
|       7 |  947 | `		if( SyBlobLength(&sOut) > 0 ){` |
|       3 |  948 | `			SyBlobAppend(&sThis,"\n\nNext ",sizeof("\n\nNext ")-1);` |
|       3 |  949 | `			SyBlobAppend(&sThis,SyBlobData(&sOut),SyBlobLength(&sOut));` |
|       1 |  950 | `		}` |
|       7 |  951 | `		SyBlobReset(&sOut);` |
|       7 |  952 | `		SyBlobAppend(&sOut,SyBlobData(&sThis),SyBlobLength(&sThis));` |
|       7 |  953 | `		SyBlobRelease(&sMsg);` |
|       7 |  954 | `		SyBlobRelease(&sThis);` |
|       4 |  955 | `	}` |
|       5 |  956 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|       5 |  957 | `	SyBlobRelease(&sOut);` |
|       5 |  958 | `	return PH7_OK;` |
|       1 |  959 | `}` |
|       - |  960 | `/*` |
|       - |  961 | ` * The declaration. php's two roots carry the same eleven methods and the same` |
|       - |  962 | `` * seven slots; the only difference php's stub records is `Error::$line`, which`` |
|       - |  963 | ` * has NO default where Exception's is 0.` |
|       - |  964 | ` *` |
|       - |  965 | `` * PH7_CLASS_NOCLONE on EVERY row: php refuses `clone $e` outright, and a native`` |
|       - |  966 | ` * subclass does not inherit its parent's class flags (rule 29).` |
|       - |  967 | ` */` |
|       - |  968 | `#define EXC_METHODS(zCtor,xCtor) \` |
|       - |  969 | `	{ "__clone",          PH7_MOD_PRIVATE, "", "void", vm_builtin_Exception_clone }, \` |
|       - |  970 | `	{ "__construct",      PH7_MOD_PUBLIC, zCtor, 0, xCtor }, \` |
|       - |  971 | `	{ "__wakeup",         PH7_MOD_PUBLIC, "", "@void", vm_builtin_Exception_wakeup }, \` |
|       - |  972 | `	{ "getMessage",       PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "string", \` |
|       - |  973 | `	  vm_builtin_Exception_getMessage }, \` |
|       - |  974 | `	{ "getCode",          PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", 0, \` |
|       - |  975 | `	  vm_builtin_Exception_getCode }, \` |
|       - |  976 | `	{ "getFile",          PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "string", \` |
|       - |  977 | `	  vm_builtin_Exception_getFile }, \` |
|       - |  978 | `	{ "getLine",          PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "int", \` |
|       - |  979 | `	  vm_builtin_Exception_getLine }, \` |
|       - |  980 | `	{ "getTrace",         PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "array", \` |
|       - |  981 | `	  vm_builtin_Exception_getTrace }, \` |
|       - |  982 | `	{ "getPrevious",      PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "?Throwable", \` |
|       - |  983 | `	  vm_builtin_Exception_getPrevious }, \` |
|       - |  984 | `	{ "getTraceAsString", PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "string", \` |
|       - |  985 | `	  vm_builtin_Exception_getTraceAsString }, \` |
|       - |  986 | `	{ "__toString",       PH7_MOD_PUBLIC, "", "string", vm_builtin_Exception_toString }` |
|       - |  987 | `#define EXC_CTOR_SIG "string $message = \"\", int $code = 0, ?Throwable $previous = null"` |
|       - |  988 | `/* php's seven slots, twice: the only difference between the two roots is` |
|       - |  989 | `` * `Error::$line`, which php's stub declares with NO default where Exception's is`` |
|       - |  990 | `` * 0 (`PH7_NATIVE_VAL_NONE` — its hasDefaultValue() is false and the export`` |
|       - |  991 | `` * prints `protected int $line` bare). `message` and `code` are the two php leaves`` |
|       - |  992 | ` * UNTYPED, and its stub says why: BC, since a subclass may have assigned` |
|       - |  993 | ` * anything to them. */` |
|       - |  994 | `#define EXC_PROP_HEAD \` |
|       - |  995 | `	{ EXC_MESSAGE,  PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 }, \` |
|       - |  996 | `	{ EXC_STRING,   PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" }, \` |
|       - |  997 | `	{ EXC_CODE,     PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 }, \` |
|       - |  998 | `	{ EXC_FILE,     PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" }` |
|       - |  999 | `#define EXC_PROP_TAIL \` |
|       - | 1000 | `	{ EXC_TRACE,    PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_ARRAY, 0, 0, 0.0 }, "array" }, \` |
|       - | 1001 | `	{ EXC_PREVIOUS, PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?Throwable" }` |
|    5146 | 1002 | `static sxi32 VmInstallExceptions(ph7_vm *pVm)` |
|       5 | 1003 | `{` |
|       - | 1004 | `	static const PH7_NativeMethodDef aExcMethod[] = {` |
|       - | 1005 | `		EXC_METHODS(EXC_CTOR_SIG,vm_builtin_Exception_construct)` |
|       - | 1006 | `	};` |
|       - | 1007 | `	static const PH7_NativePropDef aExcProp[] = {` |
|       - | 1008 | `		EXC_PROP_HEAD,` |
|       - | 1009 | `		{ EXC_LINE, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, "int" },` |
|       - | 1010 | `		EXC_PROP_TAIL` |
|       - | 1011 | `	};` |
|       - | 1012 | `	static const PH7_NativePropDef aErrProp[] = {` |
|       - | 1013 | `		EXC_PROP_HEAD,` |
|       - | 1014 | `		{ EXC_LINE, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|       - | 1015 | `		EXC_PROP_TAIL` |
|       - | 1016 | `	};` |
|       - | 1017 | `	static const PH7_NativePropDef aErrExcProp[] = {` |
|       - | 1018 | `		{ EXC_SEVERITY, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 1, 0, 0.0 }, "int" },` |
|       - | 1019 | `	};` |
|       - | 1020 | `	static const PH7_NativeMethodDef aErrExcMethod[] = {` |
|       - | 1021 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|       - | 1022 | `		  "string $message = \"\", int $code = 0, int $severity = E_ERROR, "` |
|       - | 1023 | `		  "?string $filename = null, ?int $line = null, ?Throwable $previous = null", 0,` |
|       - | 1024 | `		  vm_builtin_ErrorException_construct },` |
|       - | 1025 | `		{ "getSeverity", PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "int",` |
|       - | 1026 | `		  vm_builtin_ErrorException_getSeverity },` |
|       - | 1027 | `	};` |
|       - | 1028 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1029 | `		{ "Exception", 0, "Throwable", PH7_CLASS_NOCLONE,` |
|       - | 1030 | `		  aExcMethod, SX_ARRAYSIZE(aExcMethod), 0, 0, aExcProp, SX_ARRAYSIZE(aExcProp), 0, 0, 0 },` |
|       - | 1031 | `		{ "Error", 0, "Throwable", PH7_CLASS_NOCLONE,` |
|       - | 1032 | `		  aExcMethod, SX_ARRAYSIZE(aExcMethod), 0, 0, aErrProp, SX_ARRAYSIZE(aErrProp), 0, 0, 0 },` |
|       - | 1033 | `		/* Zend's own subclasses, then ErrorException, then SPL's tree. Every row is` |
|       - | 1034 | `		 * declaration-only in php too. */` |
|       - | 1035 | `		{ "TypeError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1036 | `		{ "ArgumentCountError", "TypeError", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1037 | `		{ "ValueError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1038 | `		{ "FiberError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1039 | `		{ "AssertionError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1040 | `		{ "ArithmeticError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1041 | `		{ "DivisionByZeroError", "ArithmeticError", 0, PH7_CLASS_NOCLONE,` |
|       - | 1042 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1043 | `		{ "UnhandledMatchError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1044 | `		{ "CompileError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1045 | `		{ "ParseError", "CompileError", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1046 | `		{ "ErrorException", "Exception", 0, PH7_CLASS_NOCLONE,` |
|       - | 1047 | `		  aErrExcMethod, SX_ARRAYSIZE(aErrExcMethod), 0, 0,` |
|       - | 1048 | `		  aErrExcProp, SX_ARRAYSIZE(aErrExcProp), 0, 0, 0 },` |
|       - | 1049 | `		{ "LogicException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1050 | `		{ "RuntimeException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1051 | `		{ "BadFunctionCallException", "LogicException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1052 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1053 | `		{ "BadMethodCallException", "BadFunctionCallException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1054 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1055 | `		{ "DomainException", "LogicException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1056 | `		{ "InvalidArgumentException", "LogicException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1057 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1058 | `		{ "LengthException", "LogicException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1059 | `		{ "OutOfRangeException", "LogicException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1060 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1061 | `		{ "OutOfBoundsException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1062 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1063 | `		{ "OverflowException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1064 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1065 | `		{ "RangeException", "RuntimeException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1066 | `		{ "UnderflowException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1067 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1068 | `		{ "UnexpectedValueException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1069 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1070 | `		{ "JsonException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1071 | `	};` |
|    5151 | 1072 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|       5 | 1073 | `}` |
|       - | 1074 | `/*` |
|       - | 1075 | ` * The eleven core interfaces, declared from C.` |
|       - | 1076 | ` *` |
|       - | 1077 | ` * They are contracts -- no method here has a body, every row is` |
|       - | 1078 | ` * PH7_MOD_ABSTRACT -- so the conversion is entirely about what the DECLARATION` |
|       - | 1079 | ` * says, which is where a chunk fell short in four php-visible ways:` |
|       - | 1080 | ` *` |
|       - | 1081 | `` *  - php's `interface Throwable extends Stringable`: the chunk redeclared`` |
|       - | 1082 | ` *    __toString() on Throwable instead, so no Exception was ever Stringable` |
|       - | 1083 | `` *    (`$e instanceof Stringable` was false, and Reflection attributed the`` |
|       - | 1084 | `` *    method to Throwable rather than printing php's `inherits Stringable`);`` |
|       - | 1085 | ` *  - php declares a RETURN TYPE on all but three of these methods and marks` |
|       - | 1086 | `` *    nearly all of them TENTATIVE (the leading `@`, rule 45) -- a chunk has no`` |
|       - | 1087 | ` *    way to say tentative at all;` |
|       - | 1088 | `` *  - php's `mixed` on ArrayAccess's offsets, which the chunk left untyped;`` |
|       - | 1089 | ` *  - method ORDER, which Reflection prints: php lists Throwable's getPrevious` |
|       - | 1090 | ` *    before getTraceAsString, and Iterator's as current/next/key/valid/rewind.` |
|       - | 1091 | ` *` |
|       - | 1092 | ` * Order within the table is php's stub order too; the declare-then-link phases` |
|       - | 1093 | ` * of PH7_InstallNativeClasses let Throwable name Stringable and Iterator name` |
|       - | 1094 | `` * Traversable regardless of row order. An interface's parent is `zParent`, not`` |
|       - | 1095 | `` * `zImplements` (Reflection walks pBase to attribute an inherited method).`` |
|       - | 1096 | ` */` |
|    5146 | 1097 | `static sxi32 VmInstallCoreInterfaces(ph7_vm *pVm)` |
|       5 | 1098 | `{` |
|       - | 1099 | `	static const PH7_NativeMethodDef aStringable[] = {` |
|       - | 1100 | `		{ "__toString", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1101 | `	};` |
|       - | 1102 | `	static const PH7_NativeMethodDef aThrowable[] = {` |
|       - | 1103 | `		/* Not one of these is tentative: php's Throwable is a real contract. */` |
|       - | 1104 | `		{ "getMessage",       PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1105 | `		{ "getCode",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", 0, 0 },` |
|       - | 1106 | `		{ "getFile",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1107 | `		{ "getLine",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "int", 0 },` |
|       - | 1108 | `		{ "getTrace",         PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "array", 0 },` |
|       - | 1109 | `		{ "getPrevious",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "?Throwable", 0 },` |
|       - | 1110 | `		{ "getTraceAsString", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1111 | `	};` |
|       - | 1112 | `	static const PH7_NativeMethodDef aArrayAccess[] = {` |
|       - | 1113 | `		{ "offsetExists", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset", "@bool", 0 },` |
|       - | 1114 | `		{ "offsetGet",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset", "@mixed", 0 },` |
|       - | 1115 | `		{ "offsetSet",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset, mixed $value",` |
|       - | 1116 | `		  "@void", 0 },` |
|       - | 1117 | `		{ "offsetUnset",  PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset", "@void", 0 },` |
|       - | 1118 | `	};` |
|       - | 1119 | `	static const PH7_NativeMethodDef aCountable[] = {` |
|       - | 1120 | `		{ "count", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@int", 0 },` |
|       - | 1121 | `	};` |
|       - | 1122 | `	static const PH7_NativeMethodDef aJsonSerializable[] = {` |
|       - | 1123 | `		{ "jsonSerialize", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@mixed", 0 },` |
|       - | 1124 | `	};` |
|       - | 1125 | `	/* The concrete cases()/from()/tryFrom() an enum gets are native methods` |
|       - | 1126 | `	 * declared to match these (oo_native.c, PH7_InstallEnumInterfaceMethods). */` |
|       - | 1127 | `	static const PH7_NativeMethodDef aUnitEnum[] = {` |
|       - | 1128 | `		{ "cases", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_ABSTRACT, "", "array", 0 },` |
|       - | 1129 | `	};` |
|       - | 1130 | `	static const PH7_NativeMethodDef aBackedEnum[] = {` |
|       - | 1131 | `		{ "from",    PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_ABSTRACT, "string\|int $value",` |
|       - | 1132 | `		  "static", 0 },` |
|       - | 1133 | `		{ "tryFrom", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_ABSTRACT, "string\|int $value",` |
|       - | 1134 | `		  "?static", 0 },` |
|       - | 1135 | `	};` |
|       - | 1136 | `	static const PH7_NativeMethodDef aIterator[] = {` |
|       - | 1137 | `		{ "current", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@mixed", 0 },` |
|       - | 1138 | `		{ "next",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@void", 0 },` |
|       - | 1139 | `		{ "key",     PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@mixed", 0 },` |
|       - | 1140 | `		{ "valid",   PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@bool", 0 },` |
|       - | 1141 | `		{ "rewind",  PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@void", 0 },` |
|       - | 1142 | `	};` |
|       - | 1143 | `	static const PH7_NativeMethodDef aIteratorAggregate[] = {` |
|       - | 1144 | `		{ "getIterator", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@Traversable", 0 },` |
|       - | 1145 | `	};` |
|       - | 1146 | `	/* php's legacy Serializable declares NO return type on either method. */` |
|       - | 1147 | `	static const PH7_NativeMethodDef aSerializable[] = {` |
|       - | 1148 | `		{ "serialize",   PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", 0, 0 },` |
|       - | 1149 | `		{ "unserialize", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "string $data", 0, 0 },` |
|       - | 1150 | `	};` |
|       - | 1151 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1152 | `		{ "Traversable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1153 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1154 | `		{ "Stringable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1155 | `		  aStringable, SX_ARRAYSIZE(aStringable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1156 | `		{ "Throwable", "Stringable", 0, PH7_CLASS_INTERFACE,` |
|       - | 1157 | `		  aThrowable, SX_ARRAYSIZE(aThrowable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1158 | `		{ "ArrayAccess", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1159 | `		  aArrayAccess, SX_ARRAYSIZE(aArrayAccess), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1160 | `		{ "Countable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1161 | `		  aCountable, SX_ARRAYSIZE(aCountable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1162 | `		{ "JsonSerializable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1163 | `		  aJsonSerializable, SX_ARRAYSIZE(aJsonSerializable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1164 | `		{ "UnitEnum", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1165 | `		  aUnitEnum, SX_ARRAYSIZE(aUnitEnum), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1166 | `		{ "BackedEnum", "UnitEnum", 0, PH7_CLASS_INTERFACE,` |
|       - | 1167 | `		  aBackedEnum, SX_ARRAYSIZE(aBackedEnum), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1168 | `		{ "Iterator", "Traversable", 0, PH7_CLASS_INTERFACE,` |
|       - | 1169 | `		  aIterator, SX_ARRAYSIZE(aIterator), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1170 | `		{ "IteratorAggregate", "Traversable", 0, PH7_CLASS_INTERFACE,` |
|       - | 1171 | `		  aIteratorAggregate, SX_ARRAYSIZE(aIteratorAggregate), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1172 | `		{ "Serializable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1173 | `		  aSerializable, SX_ARRAYSIZE(aSerializable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1174 | `	};` |
|    5151 | 1175 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|       5 | 1176 | `}` |
|       - | 1177 | `/*` |
|       - | 1178 | ` * ---------------------------------------------------------------------------` |
|       - | 1179 | `` * php's Directory — the object `dir()` answers.`` |
|       - | 1180 | ` *` |
|       - | 1181 | ` * php declares it FINAL with **no constructor at all**: the class is created by` |
|       - | 1182 | `` * `dir()` and `new Directory` is refused in the create_object handler, with a`` |
|       - | 1183 | ` * sentence that names dir() as the way to get one. Its two slots are` |
|       - | 1184 | `` * `public protected(set) readonly`, so a script can read `$d->path` and never`` |
|       - | 1185 | `` * write it, and its three methods declare return types (`read(): string\|false`).`` |
|       - | 1186 | ` * The chunk had a public constructor, a __destruct php does not declare, no` |
|       - | 1187 | ` * types anywhere and writable slots.` |
|       - | 1188 | ` * ---------------------------------------------------------------------------` |
|       - | 1189 | ` */` |
|       - | 1190 | `#define DIR_HANDLE "handle"` |
|       - | 1191 | `#define DIR_PATH   "path"` |
|       - | 1192 | `/*` |
|       - | 1193 | ` * Forward one method to the engine's own directory builtin (rule 7: call, don't` |
|       - | 1194 | `` * reimplement). php's Directory methods are `php_stream_readdir(...)` on the very`` |
|       - | 1195 | `` * stream `readdir()` uses, and a CLOSED handle is a TypeError there — the one`` |
|       - | 1196 | ` * place php's wording names the class rather than the function.` |
|       - | 1197 | ` */` |
|      22 | 1198 | `static int VmDirClosed(ph7_value *pHandle)` |
|       1 | 1199 | `{` |
|      23 | 1200 | `	io_private *pDev = (io_private *)pHandle->x.pOther;` |
|      23 | 1201 | `	return IO_PRIVATE_INVALID(pDev);` |
|       1 | 1202 | `}` |
|      22 | 1203 | `static int VmDirForward(ph7_context *pCtx,const char *zFunc,const char *zMethod)` |
|       1 | 1204 | `{` |
|      23 | 1205 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      23 | 1206 | `	ph7_value *pHandle = pThis ? PH7_NativeAttr(pThis,DIR_HANDLE) : 0;` |
|       - | 1207 | `	ph7_value *apArg[1];` |
|       - | 1208 | `	ph7_value sResult;` |
|       - | 1209 | `	ph7_value sName;` |
|       - | 1210 | `	SyString sStr;` |
|       - | 1211 | `	sxi32 rc;` |
|       - | 1212 | ``	/* php's check is `php_stream_from_zval` on a stream it CLOSED: closedir()`` |
|       - | 1213 | `	 * keeps the resource alive and marks it (gettype() answers` |
|       - | 1214 | `	 * "resource (closed)"), so the test is the magic, not the type. */` |
|      22 | 1215 | `	if( pHandle == 0 \|\| (pHandle->iFlags & MEMOBJ_RES) == 0` |
|      23 | 1216 | `	 \|\| VmDirClosed(pHandle) ){` |
|      10 | 1217 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1218 | `			"Directory::%s(): cannot use Directory resource after it has been closed",` |
|       3 | 1219 | `			zMethod);` |
|       - | 1220 | `	}` |
|      17 | 1221 | `	SyStringInitFromBuf(&sStr,zFunc,SyStrlen(zFunc));` |
|      17 | 1222 | `	PH7_MemObjInit(pCtx->pVm,&sName);` |
|      17 | 1223 | `	PH7_MemObjInitFromString(pCtx->pVm,&sName,&sStr);` |
|      17 | 1224 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      17 | 1225 | `	apArg[0] = pHandle;` |
|      17 | 1226 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,&sName,1,apArg,&sResult);` |
|      17 | 1227 | `	PH7_MemObjRelease(&sName);` |
|      17 | 1228 | `	if( rc == SXRET_OK ){` |
|      17 | 1229 | `		ph7_result_value(pCtx,&sResult);` |
|       8 | 1230 | `	}` |
|      17 | 1231 | `	PH7_MemObjRelease(&sResult);` |
|      17 | 1232 | `	return PH7_OK;` |
|      12 | 1233 | `}` |
|      14 | 1234 | `static int vm_builtin_Directory_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1235 | `{` |
|       7 | 1236 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      15 | 1237 | `	return VmDirForward(pCtx,"readdir","read");` |
|       1 | 1238 | `}` |
|       4 | 1239 | `static int vm_builtin_Directory_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1240 | `{` |
|       2 | 1241 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       5 | 1242 | `	return VmDirForward(pCtx,"rewinddir","rewind");` |
|       1 | 1243 | `}` |
|       4 | 1244 | `static int vm_builtin_Directory_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1245 | `{` |
|       2 | 1246 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       5 | 1247 | `	return VmDirForward(pCtx,"closedir","close");` |
|       1 | 1248 | `}` |
|    5146 | 1249 | `static sxi32 VmInstallDirectory(ph7_vm *pVm)` |
|       5 | 1250 | `{` |
|       - | 1251 | `	static const PH7_NativePropDef aDirProp[] = {` |
|       - | 1252 | `		{ DIR_PATH,   PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1253 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|       - | 1254 | `		{ DIR_HANDLE, PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1255 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "mixed" },` |
|       - | 1256 | `	};` |
|       - | 1257 | `	static const PH7_NativeMethodDef aDirMethod[] = {` |
|       - | 1258 | `		{ "close",  PH7_MOD_PUBLIC, "", "void", vm_builtin_Directory_close },` |
|       - | 1259 | `		{ "rewind", PH7_MOD_PUBLIC, "", "void", vm_builtin_Directory_rewind },` |
|       - | 1260 | `		{ "read",   PH7_MOD_PUBLIC, "", "string\|false", vm_builtin_Directory_read },` |
|       - | 1261 | `	};` |
|       - | 1262 | `	static const PH7_NativeClassSpec sSpec = {` |
|       - | 1263 | `		"Directory", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE,` |
|       - | 1264 | `		aDirMethod, SX_ARRAYSIZE(aDirMethod), 0, 0,` |
|       - | 1265 | `		aDirProp, SX_ARRAYSIZE(aDirProp), 0, 0, 0` |
|       - | 1266 | `	};` |
|    5151 | 1267 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|    5151 | 1268 | `	if( rc == SXRET_OK ){` |
|    5151 | 1269 | `		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"Directory",sizeof("Directory")-1,FALSE,0);` |
|    5151 | 1270 | `		if( pClass ){` |
|       - | 1271 | `			/* php words this refusal per class rather than with the generic` |
|       - | 1272 | `			 * "Instantiation of class %s is not allowed". */` |
|    5151 | 1273 | `			pClass->zNewRefusal = "Cannot directly construct Directory, use dir() instead";` |
|    2573 | 1274 | `		}` |
|    2573 | 1275 | `	}` |
|    5151 | 1276 | `	return rc;` |
|       5 | 1277 | `}` |
|       - | 1278 | `/*` |
|       - | 1279 | ` * ---------------------------------------------------------------------------` |
|       - | 1280 | ` * php's two attribute classes.` |
|       - | 1281 | ` *` |
|       - | 1282 | ``  * Both carry an ATTRIBUTE of their own — `#[Attribute(Attribute::TARGET_CLASS)]` `` |
|       - | 1283 | ` * on Attribute, a target mask on Deprecated — and those records are load-bearing` |
|       - | 1284 | ` * rather than decorative: the engine reads them to decide whether a user's` |
|       - | 1285 | `` * `#[Deprecated]` may sit where it does, and ReflectionAttribute answers them.`` |
|       - | 1286 | ` * A compiled attribute holds its argument as byte-code, so this is what` |
|       - | 1287 | `` * `PH7_NativeClassAddAttribute()` exists for (rule 11's next unused corner,`` |
|       - | 1288 | ` * exercised here): the argument rides as a literal.` |
|       - | 1289 | ` *` |
|       - | 1290 | ` * php's Deprecated mask is 87 — TARGET_CLASS\|FUNCTION\|METHOD\|CLASS_CONSTANT\|` |
|       - | 1291 | ` * CONSTANT — where the chunk wrote 86 and left the CLASS bit out.` |
|       - | 1292 | ` * ---------------------------------------------------------------------------` |
|       - | 1293 | ` */` |
|       8 | 1294 | `static int vm_builtin_Attribute_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1295 | `{` |
|       9 | 1296 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       9 | 1297 | `	if( pThis ){` |
|      16 | 1298 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,"flags",` |
|       7 | 1299 | `			nArg > 0 ? ph7_value_to_int64(apArg[0]) : 127);` |
|       4 | 1300 | `	}` |
|       9 | 1301 | `	return PH7_OK;` |
|       1 | 1302 | `}` |
|      10 | 1303 | `static int vm_builtin_Deprecated_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1304 | `{` |
|      11 | 1305 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - | 1306 | `	static const char *const azSlot[] = { "message", "since" };` |
|       - | 1307 | `	int n;` |
|      11 | 1308 | `	if( pThis == 0 ){` |
|     ! 0 | 1309 | `		return PH7_OK;` |
|       - | 1310 | `	}` |
|      31 | 1311 | `	for( n = 0 ; n < 2 ; n++ ){` |
|       - | 1312 | `		ph7_value sVal;` |
|      21 | 1313 | `		PH7_MemObjInit(pCtx->pVm,&sVal);` |
|      21 | 1314 | `		if( n < nArg ){` |
|      15 | 1315 | `			PH7_MemObjStore(apArg[n],&sVal);` |
|       7 | 1316 | `		}` |
|      21 | 1317 | `		PH7_NativeSetProp(pCtx->pVm,pThis,azSlot[n],SyStrlen(azSlot[n]),&sVal);` |
|      21 | 1318 | `		PH7_MemObjRelease(&sVal);` |
|      11 | 1319 | `	}` |
|      11 | 1320 | `	return PH7_OK;` |
|       6 | 1321 | `}` |
|    5146 | 1322 | `static sxi32 VmInstallAttributes(ph7_vm *pVm)` |
|       5 | 1323 | `{` |
|       - | 1324 | `	static const PH7_NativeConstDef aAttrConst[] = {` |
|       - | 1325 | `		{ "TARGET_CLASS",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },` |
|       - | 1326 | `		{ "TARGET_FUNCTION",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|       - | 1327 | `		{ "TARGET_METHOD",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4, 0, 0.0 },` |
|       - | 1328 | `		{ "TARGET_PROPERTY",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 8, 0, 0.0 },` |
|       - | 1329 | `		{ "TARGET_CLASS_CONSTANT", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16, 0, 0.0 },` |
|       - | 1330 | `		{ "TARGET_PARAMETER",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32, 0, 0.0 },` |
|       - | 1331 | `		{ "TARGET_CONSTANT",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64, 0, 0.0 },` |
|       - | 1332 | `		{ "TARGET_ALL",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 127, 0, 0.0 },` |
|       - | 1333 | `		{ "IS_REPEATABLE",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 128, 0, 0.0 },` |
|       - | 1334 | `	};` |
|       - | 1335 | ``	/* php declares `public int $flags;` — typed, NO default (the constructor is`` |
|       - | 1336 | ``	 * the only writer), which is what the chunk's `public $flags;` could not say. */`` |
|       - | 1337 | `	static const PH7_NativePropDef aAttrProp[] = {` |
|       - | 1338 | `		{ "flags", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|       - | 1339 | `	};` |
|       - | 1340 | `	static const PH7_NativeMethodDef aAttrMethod[] = {` |
|       - | 1341 | `		{ "__construct", PH7_MOD_PUBLIC, "int $flags = Attribute::TARGET_ALL", 0,` |
|       - | 1342 | `		  vm_builtin_Attribute_construct },` |
|       - | 1343 | `	};` |
|       - | 1344 | `	static const PH7_NativePropDef aDepProp[] = {` |
|       - | 1345 | `		{ "message", PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1346 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "?string" },` |
|       - | 1347 | `		{ "since",   PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1348 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "?string" },` |
|       - | 1349 | `	};` |
|       - | 1350 | `	static const PH7_NativeMethodDef aDepMethod[] = {` |
|       - | 1351 | `		{ "__construct", PH7_MOD_PUBLIC, "?string $message = null, ?string $since = null", 0,` |
|       - | 1352 | `		  vm_builtin_Deprecated_construct },` |
|       - | 1353 | `	};` |
|       - | 1354 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1355 | `		{ "Attribute", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1356 | `		  aAttrMethod, SX_ARRAYSIZE(aAttrMethod), aAttrConst, SX_ARRAYSIZE(aAttrConst),` |
|       - | 1357 | `		  aAttrProp, SX_ARRAYSIZE(aAttrProp), 0, 0, 0 },` |
|       - | 1358 | `		{ "Deprecated", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1359 | `		  aDepMethod, SX_ARRAYSIZE(aDepMethod), 0, 0,` |
|       - | 1360 | `		  aDepProp, SX_ARRAYSIZE(aDepProp), 0, 0, 0 },` |
|       - | 1361 | `	};` |
|       - | 1362 | `	static const PH7_NativeAttrArg aOnAttribute[] = {` |
|       - | 1363 | `		{ 0, { 0, 0, PH7_NATIVE_VAL_INT, 1, 0, 0.0 } },   /* TARGET_CLASS */` |
|       - | 1364 | `	};` |
|       - | 1365 | `	static const PH7_NativeAttrArg aOnDeprecated[] = {` |
|       - | 1366 | `		{ 0, { 0, 0, PH7_NATIVE_VAL_INT, 87, 0, 0.0 } },  /* php's own mask */` |
|       - | 1367 | `	};` |
|    5151 | 1368 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|    5151 | 1369 | `	if( rc == SXRET_OK ){` |
|    7724 | 1370 | `		rc = PH7_NativeClassAddAttribute(&(*pVm),` |
|    2573 | 1371 | `			PH7_VmExtractClass(&(*pVm),"Attribute",sizeof("Attribute")-1,FALSE,0),` |
|       - | 1372 | `			"Attribute",aOnAttribute,SX_ARRAYSIZE(aOnAttribute));` |
|    2573 | 1373 | `	}` |
|    5151 | 1374 | `	if( rc == SXRET_OK ){` |
|    7724 | 1375 | `		rc = PH7_NativeClassAddAttribute(&(*pVm),` |
|    2573 | 1376 | `			PH7_VmExtractClass(&(*pVm),"Deprecated",sizeof("Deprecated")-1,FALSE,0),` |
|       - | 1377 | `			"Attribute",aOnDeprecated,SX_ARRAYSIZE(aOnDeprecated));` |
|    2573 | 1378 | `	}` |
|    5151 | 1379 | `	return rc;` |
|       5 | 1380 | `}` |
|       - | 1381 | `/*` |
|       - | 1382 | ` * stdClass and Random\RandomException.` |
|       - | 1383 | ` *` |
|       - | 1384 | ` * stdClass is EMPTY in php too — it holds only dynamic properties — so the whole` |
|       - | 1385 | `` * declaration is the row. `Random\RandomException` is the first NAMESPACED class`` |
|       - | 1386 | ` * declared from C: the engine keys its class table by the FULLY QUALIFIED name` |
|       - | 1387 | `` * (the compiler resolves `namespace Random { class RandomException }` to exactly`` |
|       - | 1388 | ` * this string before installing), so a spec row spells the FQN and needs no` |
|       - | 1389 | ` * namespace machinery at all. It also retires the chunk this file kept ALONE for` |
|       - | 1390 | `` * it, whose comment explains why: a `namespace` declaration is not reset at its`` |
|       - | 1391 | ` * closing brace here, so anything following it in the same chunk would have` |
|       - | 1392 | ` * leaked into the Random namespace.` |
|       - | 1393 | ` */` |
|    5146 | 1394 | `static sxi32 VmInstallStdClasses(ph7_vm *pVm)` |
|       5 | 1395 | `{` |
|       - | 1396 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1397 | `		{ "stdClass", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1398 | `		/* unserialize()'s carrier for a disallowed or unknown class: as empty as` |
|       - | 1399 | `		 * stdClass (its properties are the payload's, created dynamically); what` |
|       - | 1400 | `		 * makes it special is the pVm->pIncClass checks at the access sites. */` |
|       - | 1401 | `		{ "__PHP_Incomplete_Class", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1402 | `		{ "Random\\RandomException", "Exception", 0, PH7_CLASS_NOCLONE,` |
|       - | 1403 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1404 | `		/* php 8.5's filter exceptions: FILTER_THROW_ON_FAILURE raises the second,` |
|       - | 1405 | `		 * and the first is the base a caller catches to mean "any filter error". */` |
|       - | 1406 | `		{ "Filter\\FilterException", "Exception", 0, 0,` |
|       - | 1407 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1408 | `		{ "Filter\\FilterFailedException", "Filter\\FilterException", 0, 0,` |
|       - | 1409 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1410 | `	};` |
|    5151 | 1411 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|       5 | 1412 | `}` |
|    5146 | 1413 | `PH7_PRIVATE sxi32 PH7_VmInstallBuiltinLib(ph7_vm *pVm)` |
|       5 | 1414 | `{` |
|       - | 1415 | `	SyString sBuiltin;` |
|       - | 1416 | `	/* The interfaces first: everything below implements one of them` |
|       - | 1417 | `	 * (Exception implements Throwable). */` |
|    5151 | 1418 | `	VmInstallCoreInterfaces(&(*pVm));` |
|    5151 | 1419 | `	VmInstallExceptions(&(*pVm));` |
|    5151 | 1420 | `	VmInstallStdClasses(&(*pVm));` |
|    5151 | 1421 | `	VmInstallDirectory(&(*pVm));` |
|    5151 | 1422 | `	VmInstallAttributes(&(*pVm));` |
|    5151 | 1423 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|       - | 1424 | `	/* Compile the built-in library */` |
|    5151 | 1425 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|    5151 | 1426 | `	return SXRET_OK;` |
|       5 | 1427 | `}` |
|       - | 1428 |  |
