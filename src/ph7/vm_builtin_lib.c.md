# src/ph7/vm_builtin_lib.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 338/465 lines (72.69%)

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
|       - |   41 | `	"  $aDir = array();"\` |
|       - |   42 | `	"  $pHandle = opendir($directory);"\` |
|       - |   43 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|       - |   44 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|       - |   45 | `	"      $aDir[] = $pEntry;"\` |
|       - |   46 | `	"   }"\` |
|       - |   47 | `	"  closedir($pHandle);"\` |
|       - |   48 | `	"  /* php's rule is a two-way split, not a three-value enum: SORT_NONE leaves the"\` |
|       - |   49 | `	"     order alone and EVERY other value sorts -- ascending only for the exact"\` |
|       - |   50 | `	"     SORT_ASCENDING, descending otherwise. PHL left an unknown value UNSORTED,"\` |
|       - |   51 | `	"     which reads as SORT_NONE. */"\` |
|       - |   52 | `	"  if( $sorting_order != SCANDIR_SORT_NONE ){"\` |
|       - |   53 | `	"      if( $sorting_order == SCANDIR_SORT_ASCENDING ){ sort($aDir); }"\` |
|       - |   54 | `	"      else { rsort($aDir); }"\` |
|       - |   55 | `	"  }"\` |
|       - |   56 | `	"  return $aDir;"\` |
|       - |   57 | `	"}"\` |
|       - |   58 | `	"function glob(string $pattern,int $flags = 0){"\` |
|       - |   59 | `	"/* php's Z_PARAM_PATH refusal (see scandir above). It precedes the flag check:"\` |
|       - |   60 | `	"   php's ZPP runs before the function body. Without it the NUL was simply the end"\` |
|       - |   61 | `	"   of the pattern and glob() answered for the truncated one. */"\` |
|       - |   62 | `	"if( strpos($pattern, chr(0)) !== false ){"\` |
|       - |   63 | `	"  throw new ValueError('glob(): Argument #1 ($pattern) must not contain any null bytes');"\` |
|       - |   64 | `	"}"\` |
|       - |   65 | `	"/* php rejects a mask holding any bit outside GLOB_AVAILABLE_FLAGS with a warning"\` |
|       - |   66 | `	"   and FALSE. PHL accepted anything and just tested the bits it knew, so a stale"\` |
|       - |   67 | `	"   script passing the OLD PHL glob values (1/2/4/...) silently got a plain glob. */"\` |
|       - |   68 | `	"if( $flags & ~GLOB_AVAILABLE_FLAGS ){"\` |
|       - |   69 | `	"  trigger_error('glob(): At least one of the passed flags is invalid or not supported on this platform', E_USER_WARNING);"\` |
|       - |   70 | `	"  return FALSE;"\` |
|       - |   71 | `	"}"\` |
|       - |   72 | `	"/* GLOB_BRACE: expand the FIRST top-level {a,b,...} group and glob each"\` |
|       - |   73 | `	"   alternative IN ORDER, concatenating the answers (each sub-glob sorts its"\` |
|       - |   74 | `	"   own results; php never re-sorts across alternatives). Nested groups are"\` |
|       - |   75 | `	"   handled by the recursion, and GLOB_NOCHECK applies per EXPANDED pattern,"\` |
|       - |   76 | `	"   which is php's answer too. The flag used to be accepted and IGNORED, so"\` |
|       - |   77 | `	"   any braced pattern answered [] in silence. */"\` |
|       - |   78 | `	"if( $flags & GLOB_BRACE ){"\` |
|       - |   79 | `	"  $nLen = strlen($pattern); $iOpen = -1; $iClose = -1; $iDepth = 0;"\` |
|       - |   80 | `	"  for( $i = 0 ; $i < $nLen ; $i++ ){"\` |
|       - |   81 | `	"    $ch = $pattern[$i];"\` |
|       - |   82 | `	"    if( $ch === '{' ){ if( $iDepth === 0 ){ $iOpen = $i; } $iDepth++; }"\` |
|       - |   83 | `	"    else if( $ch === '}' && $iDepth > 0 ){ $iDepth--; if( $iDepth === 0 ){ $iClose = $i; break; } }"\` |
|       - |   84 | `	"  }"\` |
|       - |   85 | `	"  if( $iOpen >= 0 && $iClose > $iOpen ){"\` |
|       - |   86 | `	"    $zHead = substr($pattern,0,$iOpen);"\` |
|       - |   87 | `	"    $zBody = (string)substr($pattern,$iOpen+1,$iClose-$iOpen-1);"\` |
|       - |   88 | `	"    $zTail = (string)substr($pattern,$iClose+1);"\` |
|       - |   89 | `	"    $aAlt = array(); $zCur = ''; $iDepth = 0;"\` |
|       - |   90 | `	"    for( $i = 0 ; $i < strlen($zBody) ; $i++ ){"\` |
|       - |   91 | `	"      $ch = $zBody[$i];"\` |
|       - |   92 | `	"      if( $ch === '{' ){ $iDepth++; }"\` |
|       - |   93 | `	"      else if( $ch === '}' ){ $iDepth--; }"\` |
|       - |   94 | `	"      if( $ch === ',' && $iDepth === 0 ){ $aAlt[] = $zCur; $zCur = ''; continue; }"\` |
|       - |   95 | `	"      $zCur .= $ch;"\` |
|       - |   96 | `	"    }"\` |
|       - |   97 | `	"    $aAlt[] = $zCur;"\` |
|       - |   98 | `	"    $pArray = array();"\` |
|       - |   99 | `	"    foreach( $aAlt as $zAlt ){"\` |
|       - |  100 | `	"      $aSub = glob($zHead . $zAlt . $zTail,$flags);"\` |
|       - |  101 | `	"      if( $aSub !== false ){ foreach( $aSub as $zHit ){ $pArray[] = $zHit; } }"\` |
|       - |  102 | `	"    }"\` |
|       - |  103 | `	"    return $pArray;"\` |
|       - |  104 | `	"  }"\` |
|       - |  105 | `	"}"\` |
|       - |  106 | `	"/* php keeps the literal directory portion of the pattern in every result;"\` |
|       - |  107 | `	"   split off everything up to and including the last '/' as the prefix. */"\` |
|       - |  108 | `	"$slash = strrpos($pattern,'/');"\` |
|       - |  109 | `	"if( $slash === false ){ $zDir = '.'; $prefix = ''; $pat = $pattern; }"\` |
|       - |  110 | `	"else { $zDir = substr($pattern,0,$slash); if( $zDir === '' ){ $zDir = '/'; } $prefix = substr($pattern,0,$slash+1); $pat = substr($pattern,$slash+1); }"\` |
|       - |  111 | `	"$pArray = array(); /* Empty array */"\` |
|       - |  112 | `	"/* php answers [] in SILENCE for a directory that cannot be opened — a"\` |
|       - |  113 | `	"   nonexistent path is simply zero matches (GLOB_ERR included; that flag is"\` |
|       - |  114 | `	"   about errors during the walk, not about the path). PHL used to let"\` |
|       - |  115 | `	"   opendir() warn and answered FALSE. */"\` |
|       - |  116 | `	"$pHandle = @opendir($zDir);"\` |
|       - |  117 | `	"if( $pHandle != FALSE ){"\` |
|       - |  118 | `	"/* Loop throw available entries */"\` |
|       - |  119 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|       - |  120 | `	" /* php's glob() never matches a leading-dot entry (incl. '.' and '..') unless"\` |
|       - |  121 | `	"    the pattern itself starts with a dot */"\` |
|       - |  122 | `	"	if( strlen($pEntry) > 0 && $pEntry[0] === '.' && (strlen($pat) < 1 \|\| $pat[0] !== '.') ){ continue; }"\` |
|       - |  123 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|       - |  124 | `	"	$rc = strglob($pat,$pEntry);"\` |
|       - |  125 | `	"	if( $rc ){"\` |
|       - |  126 | `	"	   $zFull = $prefix . $pEntry;"\` |
|       - |  127 | `	"	   if( is_dir($zDir . '/' . $pEntry) ){"\` |
|       - |  128 | `	"	      if( $flags & GLOB_MARK ){"\` |
|       - |  129 | `	"		     /* Adds a slash to each directory returned */"\` |
|       - |  130 | `	"			 $zFull .= DIRECTORY_SEPARATOR;"\` |
|       - |  131 | `	"		  }"\` |
|       - |  132 | `	"	   }else if( $flags & GLOB_ONLYDIR ){"\` |
|       - |  133 | `	"	     /* Not a directory,ignore */"\` |
|       - |  134 | `	"		 continue;"\` |
|       - |  135 | `	"	   }"\` |
|       - |  136 | `	"	   /* Add the entry (with its literal directory prefix, php-style) */"\` |
|       - |  137 | `	"	   $pArray[] = $zFull;"\` |
|       - |  138 | `	"	}"\` |
|       - |  139 | `	" }"\` |
|       - |  140 | `	"/* Close the handle */"\` |
|       - |  141 | `	"closedir($pHandle);"\` |
|       - |  142 | `	"}"\` |
|       - |  143 | `	"if( ($flags & GLOB_NOSORT) == 0 ){"\` |
|       - |  144 | `	"  /* Sort the array */"\` |
|       - |  145 | `	"  sort($pArray);"\` |
|       - |  146 | `	"}"\` |
|       - |  147 | `	"if( ($flags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|       - |  148 | `	"  /* Return the search pattern if no files matching were found */"\` |
|       - |  149 | `	"  $pArray[] = $pattern;"\` |
|       - |  150 | `	"}"\` |
|       - |  151 | `	"/* Return the created array */"\` |
|       - |  152 | `	"return $pArray;"\` |
|       - |  153 | `   "}"\` |
|       - |  154 | `   "/* Creates a temporary file */"\` |
|       - |  155 | `   "function tmpfile(){"\` |
|       - |  156 | `   "  /* Extract the temp directory */"\` |
|       - |  157 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|       - |  158 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|       - |  159 | `   "    /* Use the current dir */"\` |
|       - |  160 | `   "    $zTempDir = '.';"\` |
|       - |  161 | `   "  }"\` |
|       - |  162 | `   "  /* Create the file */"\` |
|       - |  163 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|       - |  164 | `   "  return $pHandle;"\` |
|       - |  165 | `   "}"\` |
|       - |  166 | `   "function is_nan($num){ $num = (float)$num; return $num != $num; }"\` |
|       - |  167 | `   "function is_infinite($num){ $num = (float)$num; return $num == INF \|\| $num == -INF; }"\` |
|       - |  168 | `   "function is_finite($num){ $num = (float)$num; return !is_nan($num) && !is_infinite($num); }"\` |
|       - |  169 | `   "/* Inverse of bin2hex() */"\` |
|       - |  170 | `   "function hex2bin($string){"\` |
|       - |  171 | `   "  $string = (string)$string;"\` |
|       - |  172 | `   "  $len = strlen($string);"\` |
|       - |  173 | `   "  if( $len % 2 !== 0 ){"\` |
|       - |  174 | `   "    trigger_error('hex2bin(): Hexadecimal input string must have an even length', E_USER_WARNING);"\` |
|       - |  175 | `   "    return false;"\` |
|       - |  176 | `   "  }"\` |
|       - |  177 | `   "  $out = '';"\` |
|       - |  178 | `   "  for( $i = 0 ; $i < $len ; $i += 2 ){"\` |
|       - |  179 | `   "    $pair = substr($string, $i, 2);"\` |
|       - |  180 | `   "    if( !ctype_xdigit($pair) ){"\` |
|       - |  181 | `   "      trigger_error('hex2bin(): Input string must be hexadecimal string', E_USER_WARNING);"\` |
|       - |  182 | `   "      return false;"\` |
|       - |  183 | `   "    }"\` |
|       - |  184 | `   "    $out = $out . chr(hexdec($pair));"\` |
|       - |  185 | `   "  }"\` |
|       - |  186 | `   "  return $out;"\` |
|       - |  187 | `   "}"\` |
|       - |  188 | ``   "/* Division that never throws: INF/-INF/NAN like php. The two `float`"\`` |
|       - |  189 | `   " * declarations are php's own: they are what refuses a non-numeric string"\` |
|       - |  190 | `   " * (an untyped $num1 cast to 0.0 and DIVIDED, so fdiv('abc',2) answered"\` |
|       - |  191 | `   " * float(0)), and what ReflectionFunction prints. */"\` |
|       - |  192 | `   "function fdiv(float $num1, float $num2): float {"\` |
|       - |  193 | `   "  if( $num2 == 0.0 ){"\` |
|       - |  194 | `   "    if( $num1 == 0.0 \|\| is_nan($num1) ){ return NAN; }"\` |
|       - |  195 | `   "    return $num1 > 0 ? INF : -INF;"\` |
|       - |  196 | `   "  }"\` |
|       - |  197 | `   "  return $num1 / $num2;"\` |
|       - |  198 | `   "}"\` |
|       - |  199 | `   "function checkdate($month, $day, $year){"\` |
|       - |  200 | `   "  $month = (int)$month; $day = (int)$day; $year = (int)$year;"\` |
|       - |  201 | `   "  if( $month < 1 \|\| $month > 12 \|\| $year < 1 \|\| $year > 32767 \|\| $day < 1 ){ return false; }"\` |
|       - |  202 | `   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\` |
|       - |  203 | `   "  $max = $days[$month - 1];"\` |
|       - |  204 | `   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) \|\| ($year % 400 === 0)) ){"\` |
|       - |  205 | `   "    $max = 29;"\` |
|       - |  206 | `   "  }"\` |
|       - |  207 | `   "  return $day <= $max;"\` |
|       - |  208 | `   "}"\` |
|       - |  209 | `   "function is_iterable($value){ return is_array($value) \|\| ($value instanceof Traversable); }"\` |
|       - |  210 | `   "function is_countable($value){ return is_array($value) \|\| ($value instanceof Countable); }"\` |
|       - |  211 | `   "function doubleval($value){ return (float)$value; }"\` |
|       - |  212 | `   "function array_count_values($array){"\` |
|       - |  213 | `   "  $out = array();"\` |
|       - |  214 | `   "  foreach( $array as $v ){"\` |
|       - |  215 | `   "    if( !is_int($v) && !is_string($v) ){"\` |
|       - |  216 | `   "      trigger_error('array_count_values(): Can only count string and integer values, entry skipped', E_USER_WARNING);"\` |
|       - |  217 | `   "      continue;"\` |
|       - |  218 | `   "    }"\` |
|       - |  219 | `   "    if( isset($out[$v]) ){ $out[$v] = $out[$v] + 1; } else { $out[$v] = 1; }"\` |
|       - |  220 | `   "  }"\` |
|       - |  221 | `   "  return $out;"\` |
|       - |  222 | `   "}"\` |
|       - |  223 | `   "function array_change_key_case($array, $case = CASE_LOWER){"\` |
|       - |  224 | `   "  $out = array();"\` |
|       - |  225 | `   "  foreach( $array as $k => $v ){"\` |
|       - |  226 | `   "    if( is_string($k) ){ $k = ($case == CASE_UPPER) ? strtoupper($k) : strtolower($k); }"\` |
|       - |  227 | `   "    $out[$k] = $v;"\` |
|       - |  228 | `   "  }"\` |
|       - |  229 | `   "  return $out;"\` |
|       - |  230 | `   "}"\` |
|       - |  231 | `   "function array_replace_recursive($array, ...$replacements){"\` |
|       - |  232 | `   "  foreach( $replacements as $o ){"\` |
|       - |  233 | `   "    foreach( $o as $k => $v ){"\` |
|       - |  234 | `   "      if( is_array($v) && isset($array[$k]) && is_array($array[$k]) ){"\` |
|       - |  235 | `   "        $array[$k] = array_replace_recursive($array[$k], $v);"\` |
|       - |  236 | `   "      }else{"\` |
|       - |  237 | `   "        $array[$k] = $v;"\` |
|       - |  238 | `   "      }"\` |
|       - |  239 | `   "    }"\` |
|       - |  240 | `   "  }"\` |
|       - |  241 | `   "  return $array;"\` |
|       - |  242 | `   "}"\` |
|       - |  243 | `   "function class_uses($object_or_class, $autoload = true){"\` |
|       - |  244 | `   "  $c = is_object($object_or_class) ? get_class($object_or_class) : (string)$object_or_class;"\` |
|       - |  245 | `   "  if( !class_exists($c) ){ return false; }"\` |
|       - |  246 | `   "  return array();  /* PHL has no traits yet -- always the empty set */"\` |
|       - |  247 | `   "}"\` |
|       - |  248 | `   "function ip2long($ip){"\` |
|       - |  249 | `   "  $p = explode('.', (string)$ip);"\` |
|       - |  250 | `   "  if( count($p) !== 4 ){ return false; }"\` |
|       - |  251 | `   "  $n = 0;"\` |
|       - |  252 | `   "  foreach( $p as $o ){"\` |
|       - |  253 | `   "    if( !ctype_digit($o) \|\| (int)$o < 0 \|\| (int)$o > 255 ){ return false; }"\` |
|       - |  254 | `   "    $n = $n * 256 + (int)$o;"\` |
|       - |  255 | `   "  }"\` |
|       - |  256 | `   "  return $n;"\` |
|       - |  257 | `   "}"\` |
|       - |  258 | `   "function long2ip($ip){"\` |
|       - |  259 | `   "  $n = (int)$ip;"\` |
|       - |  260 | `   "  return (($n >> 24) & 255) . '.' . (($n >> 16) & 255) . '.' . (($n >> 8) & 255) . '.' . ($n & 255);"\` |
|       - |  261 | `   "}"\` |
|       - |  262 | `   "function preg_filter($pattern, $replacement, $subject, $limit = -1, &$count = null){"\` |
|       - |  263 | `   "  /* php declares &$count and always writes it -- the total number of"\` |
|       - |  264 | `   "   * replacements across every subject, 0 when nothing matched. PHL never"\` |
|       - |  265 | `   "   * declared the parameter, so a caller reading it got its previous value. */"\` |
|       - |  266 | `   "  if( is_array($subject) ){"\` |
|       - |  267 | `   "    $total = 0;"\` |
|       - |  268 | `   "    $out = array();"\` |
|       - |  269 | `   "    foreach( $subject as $k => $v ){"\` |
|       - |  270 | `   "      $r = preg_replace($pattern, $replacement, (string)$v, $limit, $cnt);"\` |
|       - |  271 | `   "      $total = $total + $cnt;"\` |
|       - |  272 | `   "      if( $cnt > 0 ){ $out[$k] = $r; }"\` |
|       - |  273 | `   "    }"\` |
|       - |  274 | `   "    $count = $total;"\` |
|       - |  275 | `   "    return $out;"\` |
|       - |  276 | `   "  }"\` |
|       - |  277 | `   "  $r = preg_replace($pattern, $replacement, (string)$subject, $limit, $cnt);"\` |
|       - |  278 | `   "  $count = $cnt;"\` |
|       - |  279 | `   "  return $cnt > 0 ? $r : null;"\` |
|       - |  280 | `   "}"\` |
|       - |  281 | `   "function preg_replace_callback_array($pattern, $subject, $limit = -1, &$count = null, $flags = 0){"\` |
|       - |  282 | `   "  /* &$count is the total across every pattern; $flags shapes each callback's"\` |
|       - |  283 | `   "   * match array. php writes &$count only when the whole run SUCCEEDED -- a"\` |
|       - |  284 | `   "   * pattern that fails to compile answers null and leaves it untouched (an"\` |
|       - |  285 | `   "   * array subject is not a failure: it degrades to the empty array, count 0). */"\` |
|       - |  286 | `   "  $total = 0;"\` |
|       - |  287 | `   "  foreach( $pattern as $pat => $cb ){"\` |
|       - |  288 | `   "    $subject = preg_replace_callback($pat, $cb, $subject, $limit, $cnt, $flags);"\` |
|       - |  289 | `   "    if( $subject === null ){ return null; }"\` |
|       - |  290 | `   "    $total = $total + $cnt;"\` |
|       - |  291 | `   "  }"\` |
|       - |  292 | `   "  $count = $total;"\` |
|       - |  293 | `   "  return $subject;"\` |
|       - |  294 | `   "}"\` |
|       - |  295 | `   "function cal_days_in_month($calendar, $month, $year){"\` |
|       - |  296 | `   "  $month = (int)$month; $year = (int)$year;"\` |
|       - |  297 | `   "  $days = array(31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31);"\` |
|       - |  298 | `   "  if( $month < 1 \|\| $month > 12 ){"\` |
|       - |  299 | `   "    throw new ValueError('cal_days_in_month(): Argument #2 ($month) must be a valid month');"\` |
|       - |  300 | `   "  }"\` |
|       - |  301 | `   "  if( $month === 2 && ((($year % 4 === 0) && ($year % 100 !== 0)) \|\| ($year % 400 === 0)) ){"\` |
|       - |  302 | `   "    return 29;"\` |
|       - |  303 | `   "  }"\` |
|       - |  304 | `   "  return $days[$month - 1];"\` |
|       - |  305 | `   "}"\` |
|       - |  306 | `   "function preg_grep($pattern, $array, $flags = 0){"\` |
|       - |  307 | `   "  $out = array();"\` |
|       - |  308 | `   "  foreach( $array as $k => $v ){"\` |
|       - |  309 | `   "    $m = preg_match($pattern, (string)$v);"\` |
|       - |  310 | `   "    if( $flags & PREG_GREP_INVERT ){ $m = !$m; }"\` |
|       - |  311 | `   "    if( $m ){ $out[$k] = $v; }"\` |
|       - |  312 | `   "  }"\` |
|       - |  313 | `   "  return $out;"\` |
|       - |  314 | `   "}"\` |
|       - |  315 | `   "function class_implements($object_or_class, $autoload = true){"\` |
|       - |  316 | `   "  $c = is_object($object_or_class) ? get_class($object_or_class) : (string)$object_or_class;"\` |
|       - |  317 | `   "  if( !class_exists($c) && !interface_exists($c) ){ return false; }"\` |
|       - |  318 | `   "  $out = array();"\` |
|       - |  319 | `   "  $r = new ReflectionClass($c);"\` |
|       - |  320 | `   "  foreach( $r->getInterfaceNames() as $i ){ $out[$i] = $i; }"\` |
|       - |  321 | `   "  return $out;"\` |
|       - |  322 | `   "}"\` |
|       - |  323 | `   "function class_parents($object_or_class, $autoload = true){"\` |
|       - |  324 | `   "  $c = is_object($object_or_class) ? get_class($object_or_class) : (string)$object_or_class;"\` |
|       - |  325 | `   "  if( !class_exists($c) ){ return false; }"\` |
|       - |  326 | `   "  $out = array();"\` |
|       - |  327 | `   "  $r = new ReflectionClass($c);"\` |
|       - |  328 | `   "  while( ($p = $r->getParentClass()) ){"\` |
|       - |  329 | `   "    $n = $p->getName();"\` |
|       - |  330 | `   "    $out[$n] = $n;"\` |
|       - |  331 | `   "    $r = $p;"\` |
|       - |  332 | `   "  }"\` |
|       - |  333 | `   "  return $out;"\` |
|       - |  334 | `   "}"\` |
|       - |  335 | `   "/* php 8.3 str_increment(): Perl-style alphanumeric increment. */"\` |
|       - |  336 | `   "function str_increment($string){"\` |
|       - |  337 | `   "  $string = (string)$string;"\` |
|       - |  338 | `   "  if( $string === '' ){ throw new ValueError('str_increment(): Argument #1 ($string) must not be empty'); }"\` |
|       - |  339 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_increment(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|       - |  340 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|       - |  341 | `   "    $c = $string[$i];"\` |
|       - |  342 | `   "    if( $c === 'z' ){ $string[$i] = 'a'; }"\` |
|       - |  343 | `   "    elseif( $c === 'Z' ){ $string[$i] = 'A'; }"\` |
|       - |  344 | `   "    elseif( $c === '9' ){ $string[$i] = '0'; }"\` |
|       - |  345 | `   "    else { $string[$i] = chr(ord($c) + 1); return $string; }"\` |
|       - |  346 | `   "  }"\` |
|       - |  347 | `   "  $first = $string[0];"\` |
|       - |  348 | `   "  if( $first === '0' ){ return '1' . $string; }"\` |
|       - |  349 | `   "  if( $first === 'a' ){ return 'a' . $string; }"\` |
|       - |  350 | `   "  return 'A' . $string;"\` |
|       - |  351 | `   "}"\` |
|       - |  352 | `   "/* php 8.3 str_decrement(): inverse of str_increment(); throws out of range"\` |
|       - |  353 | `   " * at the bottom of the counting sequence. */"\` |
|       - |  354 | `   "function str_decrement($string){"\` |
|       - |  355 | `   "  $string = (string)$string;"\` |
|       - |  356 | `   "  if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) must not be empty'); }"\` |
|       - |  357 | `   "  if( !ctype_alnum($string) ){ throw new ValueError('str_decrement(): Argument #1 ($string) must be composed only of alphanumeric ASCII characters'); }"\` |
|       - |  358 | `   "  $orig = $string;"\` |
|       - |  359 | `   "  $borrowed = false;"\` |
|       - |  360 | `   "  for( $i = strlen($string) - 1 ; $i >= 0 ; $i-- ){"\` |
|       - |  361 | `   "    $c = $string[$i];"\` |
|       - |  362 | `   "    if( $c === 'a' ){ $string[$i] = 'z'; }"\` |
|       - |  363 | `   "    elseif( $c === 'A' ){ $string[$i] = 'Z'; }"\` |
|       - |  364 | `   "    elseif( $c === '0' ){ $string[$i] = '9'; }"\` |
|       - |  365 | `   "    else { $string[$i] = chr(ord($c) - 1); $borrowed = false; break; }"\` |
|       - |  366 | `   "    if( $i === 0 ){ $borrowed = true; }"\` |
|       - |  367 | `   "  }"\` |
|       - |  368 | `   "  if( $borrowed ){"\` |
|       - |  369 | `   "    if( $string[0] === '9' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|       - |  370 | `   "    $string = substr($string, 1);"\` |
|       - |  371 | `   "    if( $string === '' ){ throw new ValueError('str_decrement(): Argument #1 ($string) \"' . $orig . '\" is out of decrement range'); }"\` |
|       - |  372 | `   "  } elseif( strlen($string) > 1 && $string[0] === '0' ){"\` |
|       - |  373 | `   "    $string = substr($string, 1);"\` |
|       - |  374 | `   "  }"\` |
|       - |  375 | `   "  return $string;"\` |
|       - |  376 | `   "}"\` |
|       - |  377 | `   /* fileperms/fileowner/filegroup/fileinode moved to C (vfs.c, VfsStatField):` |
|       - |  378 | `    * as prelude wrappers over stat() three of them said nothing on a failed stat` |
|       - |  379 | `    * and the fourth raised trigger_error, whose errno is E_USER_WARNING's 512 and` |
|       - |  380 | `    * whose line is this chunk's rather than the caller's. */\` |
|       - |  381 | `   "/* PH7 keeps no stat cache, so this is a no-op like php on a clean cache. */"\` |
|       - |  382 | `   "function clearstatcache($clear_realpath_cache = false, $filename = ''){}"\` |
|       - |  383 | `   "/* php 8.4 mb_ucfirst/mb_lcfirst: case-map only the first multibyte char. */"\` |
|       - |  384 | `   "function mb_ucfirst($string, $encoding = null){"\` |
|       - |  385 | `   "  $string = (string)$string;"\` |
|       - |  386 | `   "  if( $string === '' ){ return ''; }"\` |
|       - |  387 | `   "  return mb_strtoupper(mb_substr($string, 0, 1)) . mb_substr($string, 1);"\` |
|       - |  388 | `   "}"\` |
|       - |  389 | `   "function mb_lcfirst($string, $encoding = null){"\` |
|       - |  390 | `   "  $string = (string)$string;"\` |
|       - |  391 | `   "  if( $string === '' ){ return ''; }"\` |
|       - |  392 | `   "  return mb_strtolower(mb_substr($string, 0, 1)) . mb_substr($string, 1);"\` |
|       - |  393 | `   "}"\` |
|       - |  394 | `   "/* Creates a temporary file and returns its name */"\` |
|       - |  395 | `   "function tempnam(string $directory,string $prefix)"\` |
|       - |  396 | `   "{"\` |
|       - |  397 | `   "   /* php's Z_PARAM_PATH refusal on BOTH parameters (see scandir above); the prefix"\` |
|       - |  398 | `   "    * is a path fragment there too, and PHL used to build a filename with the NUL"\` |
|       - |  399 | `   "    * still in it. */"\` |
|       - |  400 | `   "   if( strpos($directory, chr(0)) !== false ){"\` |
|       - |  401 | `   "     throw new ValueError('tempnam(): Argument #1 ($directory) must not contain any null bytes');"\` |
|       - |  402 | `   "   }"\` |
|       - |  403 | `   "   if( strpos($prefix, chr(0)) !== false ){"\` |
|       - |  404 | `   "     throw new ValueError('tempnam(): Argument #2 ($prefix) must not contain any null bytes');"\` |
|       - |  405 | `   "   }"\` |
|       - |  406 | `   "   /* php CREATES the file (empty, mode 0600) and guarantees the name is unique --"\` |
|       - |  407 | `   "    * returning a bare name left the caller with a path that does not exist, so"\` |
|       - |  408 | `   "    * file_exists() was false and unlink() failed on it. */"\` |
|       - |  409 | `   "   $directory = rtrim($directory, DIRECTORY_SEPARATOR);"\` |
|       - |  410 | `   "   for( $i = 0 ; $i < 64 ; ++$i ){"\` |
|       - |  411 | `   "     $zPath = $directory.DIRECTORY_SEPARATOR.$prefix.rand_str(12);"\` |
|       - |  412 | `   "     if( file_exists($zPath) ){ continue; }"\` |
|       - |  413 | `   "     $pHandle = @fopen($zPath,'x');"\` |
|       - |  414 | `   "     if( $pHandle === false ){ continue; }"\` |
|       - |  415 | `   "     fclose($pHandle);"\` |
|       - |  416 | `   "     @chmod($zPath, 0600);"\` |
|       - |  417 | `   "     return $zPath;"\` |
|       - |  418 | `   "   }"\` |
|       - |  419 | `   "   return false;"\` |
|       - |  420 | `   "}"\` |
|       - |  421 | `	/* fileowner/filegroup/fileinode: see the note beside fileperms above. */\` |
|       - |  422 | `	""` |
|       - |  423 |  |
|       - |  424 | `/*` |
|       - |  425 | ` * ---------------------------------------------------------------------------` |
|       - |  426 | ` * The Exception / Error family, declared from C.` |
|       - |  427 | ` *` |
|       - |  428 | ` * php's two roots are one implementation twice over (its stub says` |
|       - |  429 | `` * `@implementation-alias Exception::__construct` for every one of Error's`` |
|       - |  430 | ` * methods), so the bodies below are shared by both spec tables and the` |
|       - |  431 | ` * ~20 subclasses are declaration-only rows.` |
|       - |  432 | ` *` |
|       - |  433 | `` * php's seven slots, in php's own declaration order. `string` is php's cache of`` |
|       - |  434 | ` * the __toString rendering -- unused by the engine but PRESENT on every` |
|       - |  435 | ` * presentation surface, which is why it is declared here rather than skipped:` |
|       - |  436 | ` * var_dump/print_r/(array)/serialize all show it, and PHL was one property short` |
|       - |  437 | ` * of php on every exception ever printed.` |
|       - |  438 | ` * ---------------------------------------------------------------------------` |
|       - |  439 | ` */` |
|       - |  440 | `#define EXC_MESSAGE  "message"` |
|       - |  441 | `#define EXC_STRING   "string"` |
|       - |  442 | `#define EXC_CODE     "code"` |
|       - |  443 | `#define EXC_FILE     "file"` |
|       - |  444 | `#define EXC_LINE     "line"` |
|       - |  445 | `#define EXC_TRACE    "trace"` |
|       - |  446 | `#define EXC_PREVIOUS "previous"` |
|       - |  447 | `#define EXC_SEVERITY "severity"` |
|       - |  448 | `/*` |
|       - |  449 | ` * Answer a declared slot the way php's getter does. Three of the seven CONVERT` |
|       - |  450 | ` * rather than copy — getMessage()/getFile() answer a string and getLine() an int,` |
|       - |  451 | ` * whatever the slot holds — and that shows twice: a subclass assigning` |
|       - |  452 | `` * `$this->message = 5` reads back "5", and a slot __wakeup has DROPPED reads as`` |
|       - |  453 | ` * "" rather than null. The other four are verbatim copies (getCode() of that same` |
|       - |  454 | ` * subclass really is the int).` |
|       - |  455 | ` */` |
|       - |  456 | `#define EXC_READ_RAW 0` |
|       - |  457 | `#define EXC_READ_STR 1` |
|       - |  458 | `#define EXC_READ_INT 2` |
|    6812 |  459 | `static int VmExcReadSlot(ph7_context *pCtx,const char *zSlot,int iAs)` |
|       5 |  460 | `{` |
|    6817 |  461 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    6817 |  462 | `	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,zSlot) : 0;` |
|       - |  463 | `	ph7_value sTmp;` |
|    6817 |  464 | `	if( iAs == EXC_READ_RAW ){` |
|      54 |  465 | `		if( pVal ){` |
|      54 |  466 | `			ph7_result_value(pCtx,pVal);` |
|      29 |  467 | `		}else{` |
|     ! 0 |  468 | `			ph7_result_null(pCtx);` |
|       - |  469 | `		}` |
|      54 |  470 | `		return PH7_OK;` |
|       - |  471 | `	}` |
|       - |  472 | `	/* Through a COPY: converting the slot would rewrite the exception's state. */` |
|    6767 |  473 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|    6767 |  474 | `	if( pVal ){` |
|    6767 |  475 | `		PH7_MemObjStore(pVal,&sTmp);` |
|    3381 |  476 | `	}` |
|    6767 |  477 | `	if( iAs == EXC_READ_INT ){` |
|     617 |  478 | `		PH7_MemObjToInteger(&sTmp);` |
|     311 |  479 | `	}else{` |
|    6155 |  480 | `		PH7_MemObjToString(&sTmp);` |
|       - |  481 | `	}` |
|    6767 |  482 | `	ph7_result_value(pCtx,&sTmp);` |
|    6767 |  483 | `	PH7_MemObjRelease(&sTmp);` |
|    6767 |  484 | `	return PH7_OK;` |
|    3411 |  485 | `}` |
|    6136 |  486 | `static int vm_builtin_Exception_getMessage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  487 | `{` |
|    3068 |  488 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    6141 |  489 | `	return VmExcReadSlot(pCtx,EXC_MESSAGE,EXC_READ_STR);` |
|       5 |  490 | `}` |
|      16 |  491 | `static int vm_builtin_Exception_getCode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  492 | `{` |
|       8 |  493 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      18 |  494 | `	return VmExcReadSlot(pCtx,EXC_CODE,EXC_READ_RAW);` |
|       2 |  495 | `}` |
|      14 |  496 | `static int vm_builtin_Exception_getFile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  497 | `{` |
|       7 |  498 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      16 |  499 | `	return VmExcReadSlot(pCtx,EXC_FILE,EXC_READ_STR);` |
|       2 |  500 | `}` |
|     612 |  501 | `static int vm_builtin_Exception_getLine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  502 | `{` |
|     306 |  503 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     617 |  504 | `	return VmExcReadSlot(pCtx,EXC_LINE,EXC_READ_INT);` |
|       5 |  505 | `}` |
|       8 |  506 | `static int vm_builtin_Exception_getTrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  507 | `{` |
|       4 |  508 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      11 |  509 | `	return VmExcReadSlot(pCtx,EXC_TRACE,EXC_READ_RAW);` |
|       3 |  510 | `}` |
|      22 |  511 | `static int vm_builtin_Exception_getPrevious(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  512 | `{` |
|      11 |  513 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      25 |  514 | `	return VmExcReadSlot(pCtx,EXC_PREVIOUS,EXC_READ_RAW);` |
|       3 |  515 | `}` |
|       4 |  516 | `static int vm_builtin_ErrorException_getSeverity(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  517 | `{` |
|       2 |  518 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       5 |  519 | `	return VmExcReadSlot(pCtx,EXC_SEVERITY,EXC_READ_RAW);` |
|       1 |  520 | `}` |
|       - |  521 | `/*` |
|       - |  522 | ` * php's zend_update_exception_properties: each of the three is written only when` |
|       - |  523 | ``  * the caller actually supplied it — a message when the argument was PASSED (`""` `` |
|       - |  524 | ` * included), a code when it is NON-ZERO, a previous when it is an object. That is` |
|       - |  525 | ` * not the same as writing the defaults: a subclass may redeclare` |
|       - |  526 | `` * `protected $message = 'default'`, and php keeps it for `new Sub()`.`` |
|       - |  527 | ` */` |
| 1455380 |  528 | `static void VmExcInitProps(ph7_context *pCtx,ph7_class_instance *pThis,int nArg,` |
|       - |  529 | `	ph7_value **apArg,int iPrev)` |
|       5 |  530 | `{` |
| 1455385 |  531 | `	if( nArg > 0 ){` |
| 1455311 |  532 | `		int nMsg = 0;` |
| 1455311 |  533 | `		const char *zMsg = ph7_value_to_string(apArg[0],&nMsg);` |
| 1455311 |  534 | `		PH7_NativeSetAttrStr(pCtx->pVm,pThis,EXC_MESSAGE,zMsg,nMsg);` |
|  727653 |  535 | `	}` |
| 1455385 |  536 | `	if( nArg > 1 ){` |
|       - |  537 | `		ph7_value sCode;` |
|      50 |  538 | `		PH7_MemObjInit(pCtx->pVm,&sCode);` |
|      50 |  539 | `		PH7_MemObjStore(apArg[1],&sCode);` |
|      50 |  540 | `		PH7_MemObjToInteger(&sCode);` |
|      50 |  541 | `		if( sCode.x.iVal != 0 ){` |
|      28 |  542 | `			PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_CODE,sCode.x.iVal);` |
|      13 |  543 | `		}` |
|      50 |  544 | `		PH7_MemObjRelease(&sCode);` |
|      23 |  545 | `	}` |
|       - |  546 | ``	/* php's `previous` is the LAST parameter of each constructor, and`` |
|       - |  547 | `	 * ErrorException's is #5 rather than #2. */` |
| 1455385 |  548 | `	if( nArg > iPrev && (apArg[iPrev]->iFlags & MEMOBJ_OBJ) && apArg[iPrev]->x.pOther ){` |
|      24 |  549 | `		PH7_NativeSetAttrObj(pCtx->pVm,pThis,EXC_PREVIOUS,` |
|      14 |  550 | `			(ph7_class_instance *)apArg[iPrev]->x.pOther);` |
|       7 |  551 | `	}` |
| 1455385 |  552 | `}` |
| 1455366 |  553 | `static int vm_builtin_Exception_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  554 | `{` |
| 1455371 |  555 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
| 1455371 |  556 | `	if( pThis ){` |
| 1455371 |  557 | `		VmExcInitProps(pCtx,pThis,nArg,apArg,2);` |
|  727683 |  558 | `	}` |
| 1455371 |  559 | `	return PH7_OK;` |
|       5 |  560 | `}` |
|       - |  561 | `/*` |
|       - |  562 | ` * ErrorException's own constructor: php's Exception three, then severity, then` |
|       - |  563 | `` * the OPTIONAL file/line overrides. php's `?string $filename = null` /`` |
|       - |  564 | `` * `?int $line = null` mean "keep the creation site" — the chunk defaulted them to`` |
|       - |  565 | ` * __FILE__/__LINE__, which resolved against the EMBEDDED chunk and reported` |
|       - |  566 | `` * `:MEMORY:` line 1 for every ErrorException that did not pass them. php's one`` |
|       - |  567 | ` * asymmetry: a filename WITHOUT a line resets the line to 0.` |
|       - |  568 | ` */` |
|      14 |  569 | `static int vm_builtin_ErrorException_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  570 | `{` |
|      15 |  571 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      15 |  572 | `	if( pThis == 0 ){` |
|     ! 0 |  573 | `		return PH7_OK;` |
|       - |  574 | `	}` |
|      15 |  575 | `	VmExcInitProps(pCtx,pThis,nArg,apArg,5);` |
|      15 |  576 | `	if( nArg > 2 ){` |
|      11 |  577 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_SEVERITY,ph7_value_to_int64(apArg[2]));` |
|       5 |  578 | `	}` |
|      15 |  579 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|       9 |  580 | `		int nFile = 0;` |
|       9 |  581 | `		const char *zFile = ph7_value_to_string(apArg[3],&nFile);` |
|       9 |  582 | `		PH7_NativeSetAttrStr(pCtx->pVm,pThis,EXC_FILE,zFile,nFile);` |
|       9 |  583 | `		if( nArg < 5 \|\| ph7_value_is_null(apArg[4]) ){` |
|       3 |  584 | `			PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_LINE,0);` |
|       1 |  585 | `		}` |
|       4 |  586 | `	}` |
|      15 |  587 | `	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|       7 |  588 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,EXC_LINE,ph7_value_to_int64(apArg[4]));` |
|       3 |  589 | `	}` |
|      15 |  590 | `	return PH7_OK;` |
|       8 |  591 | `}` |
|       - |  592 | `/*` |
|       - |  593 | ` * php's private __clone. It has an empty body and is never reached: the class` |
|       - |  594 | ` * carries php's own clone refusal (PH7_CLASS_NOCLONE, answered before any body` |
|       - |  595 | `` * runs), which is what `clone $e` reports — "Trying to clone an uncloneable`` |
|       - |  596 | ` * object of class X", not a visibility error. Declaring it is still php-visible:` |
|       - |  597 | `` * Reflection lists it, and `$e->__clone()` from inside the class works.`` |
|       - |  598 | ` */` |
|     ! 0 |  599 | `static int vm_builtin_Exception_clone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  600 | `{` |
|     ! 0 |  601 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  602 | `	ph7_result_null(pCtx);` |
|     ! 0 |  603 | `	return PH7_OK;` |
|     ! 0 |  604 | `}` |
|       - |  605 | `/*` |
|       - |  606 | ` * php's __wakeup: the two UNTYPED slots are the only ones a serialized payload` |
|       - |  607 | ` * can lie about (the other five are typed and the store enforces them), so php` |
|       - |  608 | ` * DROPS a message that is not a string and a code that is not an int rather than` |
|       - |  609 | ` * letting a method read one.` |
|       - |  610 | ` */` |
|     ! 0 |  611 | `static void VmExcDropSlot(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot)` |
|     ! 0 |  612 | `{` |
|     ! 0 |  613 | `	SyHashEntry *pEntry = SyHashGet(&pThis->hAttr,(const void *)zSlot,SyStrlen(zSlot));` |
|     ! 0 |  614 | `	if( pEntry ){` |
|     ! 0 |  615 | `		PH7_VmReleaseInstanceAttr(&(*pVm),(VmClassAttr *)pEntry->pUserData);` |
|     ! 0 |  616 | `		SyHashDeleteEntry2(pEntry);` |
|     ! 0 |  617 | `	}` |
|     ! 0 |  618 | `}` |
|     ! 0 |  619 | `static int vm_builtin_Exception_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  620 | `{` |
|     ! 0 |  621 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - |  622 | `	ph7_value *pVal;` |
|     ! 0 |  623 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  624 | `	if( pThis == 0 ){` |
|     ! 0 |  625 | `		return PH7_OK;` |
|       - |  626 | `	}` |
|     ! 0 |  627 | `	pVal = PH7_NativeAttr(pThis,EXC_MESSAGE);` |
|     ! 0 |  628 | `	if( pVal && (pVal->iFlags & MEMOBJ_NULL) == 0 && (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 |  629 | `		VmExcDropSlot(pCtx->pVm,pThis,EXC_MESSAGE);` |
|     ! 0 |  630 | `	}` |
|     ! 0 |  631 | `	pVal = PH7_NativeAttr(pThis,EXC_CODE);` |
|     ! 0 |  632 | `	if( pVal && (pVal->iFlags & MEMOBJ_NULL) == 0 && (pVal->iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 |  633 | `		VmExcDropSlot(pCtx->pVm,pThis,EXC_CODE);` |
|     ! 0 |  634 | `	}` |
|     ! 0 |  635 | `	ph7_result_null(pCtx);` |
|     ! 0 |  636 | `	return PH7_OK;` |
|     ! 0 |  637 | `}` |
|       - |  638 | `/*` |
|       - |  639 | ` * One argument of a trace frame, php's smart_str_append_scalar: a string is` |
|       - |  640 | `` * single-quoted, ESCAPED (`\n`, `\xNN` for anything non-printable) and truncated`` |
|       - |  641 | `` * to 15 bytes with `...` inside the quotes; a float takes php's precision; an`` |
|       - |  642 | `` * enum case prints `Enum::Case`; and anything else is a bare word.`` |
|       - |  643 | ` */` |
|       - |  644 | `#define EXC_ARG_MAX 15` |
|     ! 0 |  645 | `static void VmExcTraceArg(ph7_vm *pVm,SyBlob *pOut,ph7_value *pArg)` |
|     ! 0 |  646 | `{` |
|     ! 0 |  647 | `	if( pArg == 0 \|\| (pArg->iFlags & MEMOBJ_NULL) ){` |
|     ! 0 |  648 | `		SyBlobAppend(pOut,"NULL",sizeof("NULL")-1);` |
|     ! 0 |  649 | `		return;` |
|       - |  650 | `	}` |
|     ! 0 |  651 | `	if( pArg->iFlags & MEMOBJ_BOOL ){` |
|     ! 0 |  652 | `		if( pArg->x.iVal ){` |
|     ! 0 |  653 | `			SyBlobAppend(pOut,"true",sizeof("true")-1);` |
|     ! 0 |  654 | `		}else{` |
|     ! 0 |  655 | `			SyBlobAppend(pOut,"false",sizeof("false")-1);` |
|       - |  656 | `		}` |
|     ! 0 |  657 | `		return;` |
|       - |  658 | `	}` |
|     ! 0 |  659 | `	if( pArg->iFlags & MEMOBJ_HASHMAP ){` |
|     ! 0 |  660 | `		SyBlobAppend(pOut,"Array",sizeof("Array")-1);` |
|     ! 0 |  661 | `		return;` |
|       - |  662 | `	}` |
|     ! 0 |  663 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 |  664 | `		ph7_class_instance *pObj = (ph7_class_instance *)pArg->x.pOther;` |
|     ! 0 |  665 | `		if( pObj && pObj->pClass && (pObj->pClass->iFlags & PH7_CLASS_ENUM) ){` |
|     ! 0 |  666 | `			ph7_value *pName = PH7_NativeAttr(pObj,"name");` |
|     ! 0 |  667 | `			SyBlobFormat(pOut,"%z::",&pObj->pClass->sName);` |
|     ! 0 |  668 | `			if( pName ){` |
|     ! 0 |  669 | `				SyBlobAppend(pOut,SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|     ! 0 |  670 | `			}` |
|     ! 0 |  671 | `			return;` |
|       - |  672 | `		}` |
|     ! 0 |  673 | `		SyBlobAppend(pOut,"Object(",sizeof("Object(")-1);` |
|     ! 0 |  674 | `		if( pObj && pObj->pClass ){` |
|     ! 0 |  675 | `			SyBlobFormat(pOut,"%z",&pObj->pClass->sName);` |
|     ! 0 |  676 | `		}` |
|     ! 0 |  677 | `		SyBlobAppend(pOut,")",sizeof(")")-1);` |
|     ! 0 |  678 | `		return;` |
|       - |  679 | `	}` |
|     ! 0 |  680 | `	if( pArg->iFlags & MEMOBJ_STRING ){` |
|     ! 0 |  681 | `		const char *z = (const char *)SyBlobData(&pArg->sBlob);` |
|     ! 0 |  682 | `		sxu32 n = SyBlobLength(&pArg->sBlob);` |
|     ! 0 |  683 | `		sxu32 nKeep = n > EXC_ARG_MAX ? EXC_ARG_MAX : n;` |
|       - |  684 | `		sxu32 i;` |
|     ! 0 |  685 | `		SyBlobAppend(pOut,"'",sizeof("'")-1);` |
|     ! 0 |  686 | `		for( i = 0 ; i < nKeep ; i++ ){` |
|     ! 0 |  687 | `			unsigned char c = (unsigned char)z[i];` |
|     ! 0 |  688 | `			if( c >= 32 && c <= 126 && c != '\\' ){` |
|     ! 0 |  689 | `				SyBlobAppend(pOut,(const void *)&z[i],sizeof(char));` |
|     ! 0 |  690 | `				continue;` |
|       - |  691 | `			}` |
|     ! 0 |  692 | `			switch( c ){` |
|     ! 0 |  693 | `				case '\n': SyBlobAppend(pOut,"\\n",2); break;` |
|     ! 0 |  694 | `				case '\r': SyBlobAppend(pOut,"\\r",2); break;` |
|     ! 0 |  695 | `				case '\t': SyBlobAppend(pOut,"\\t",2); break;` |
|     ! 0 |  696 | `				case '\f': SyBlobAppend(pOut,"\\f",2); break;` |
|     ! 0 |  697 | `				case '\v': SyBlobAppend(pOut,"\\v",2); break;` |
|     ! 0 |  698 | `				case '\\': SyBlobAppend(pOut,"\\\\",2); break;` |
|     ! 0 |  699 | `				case 27:   SyBlobAppend(pOut,"\\e",2); break;` |
|     ! 0 |  700 | `				default:   SyBlobFormat(pOut,"\\x%02X",(int)c); break;` |
|       - |  701 | `			}` |
|     ! 0 |  702 | `		}` |
|     ! 0 |  703 | `		if( n > nKeep ){` |
|     ! 0 |  704 | `			SyBlobAppend(pOut,"...",sizeof("...")-1);` |
|     ! 0 |  705 | `		}` |
|     ! 0 |  706 | `		SyBlobAppend(pOut,"'",sizeof("'")-1);` |
|     ! 0 |  707 | `		return;` |
|       - |  708 | `	}` |
|       - |  709 | `	{` |
|       - |  710 | `		/* int / float / anything else: php prints the scalar itself. */` |
|       - |  711 | `		ph7_value sTmp;` |
|     ! 0 |  712 | `		PH7_MemObjInit(&(*pVm),&sTmp);` |
|     ! 0 |  713 | `		PH7_MemObjStore(pArg,&sTmp);` |
|     ! 0 |  714 | `		PH7_MemObjToString(&sTmp);` |
|     ! 0 |  715 | `		SyBlobAppend(pOut,SyBlobData(&sTmp.sBlob),SyBlobLength(&sTmp.sBlob));` |
|     ! 0 |  716 | `		PH7_MemObjRelease(&sTmp);` |
|       - |  717 | `	}` |
|     ! 0 |  718 | `}` |
|       - |  719 | `/* An element of a trace frame, or NULL when the frame does not carry it. */` |
|     216 |  720 | `static ph7_value * VmExcFrameField(ph7_vm *pVm,ph7_hashmap *pFrame,const char *zField)` |
|       5 |  721 | `{` |
|     221 |  722 | `	ph7_hashmap_node *pNode = 0;` |
|       - |  723 | `	ph7_value sKey;` |
|       - |  724 | `	sxi32 rc;` |
|       - |  725 | `	SyString sName;` |
|     221 |  726 | `	SyStringInitFromBuf(&sName,zField,SyStrlen(zField));` |
|     221 |  727 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,&sName);` |
|     221 |  728 | `	rc = PH7_HashmapLookup(pFrame,&sKey,&pNode);` |
|     221 |  729 | `	PH7_MemObjRelease(&sKey);` |
|     221 |  730 | `	if( rc != SXRET_OK \|\| pNode == 0 ){` |
|      93 |  731 | `		return 0;` |
|       - |  732 | `	}` |
|     133 |  733 | `	return (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|     113 |  734 | `}` |
|     150 |  735 | `static void VmExcFrameStr(SyBlob *pOut,ph7_value *pVal)` |
|       5 |  736 | `{` |
|     155 |  737 | `	if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){` |
|     103 |  738 | `		SyBlobAppend(pOut,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|      49 |  739 | `	}` |
|     155 |  740 | `}` |
|       - |  741 | `/* A slot's string form, taken through a COPY: converting the value in place` |
|       - |  742 | ` * would rewrite the exception's own state. */` |
|       6 |  743 | `static void VmExcValueStr(ph7_vm *pVm,ph7_value *pVal,SyBlob *pOut)` |
|       1 |  744 | `{` |
|       - |  745 | `	ph7_value sTmp;` |
|       7 |  746 | `	if( pVal == 0 ){` |
|     ! 0 |  747 | `		return;` |
|       - |  748 | `	}` |
|       7 |  749 | `	PH7_MemObjInit(&(*pVm),&sTmp);` |
|       7 |  750 | `	PH7_MemObjStore(pVal,&sTmp);` |
|       7 |  751 | `	PH7_MemObjToString(&sTmp);` |
|       7 |  752 | `	SyBlobAppend(pOut,SyBlobData(&sTmp.sBlob),SyBlobLength(&sTmp.sBlob));` |
|       7 |  753 | `	PH7_MemObjRelease(&sTmp);` |
|       4 |  754 | `}` |
|       - |  755 | ``/* Does the blob contain this literal? SyBlobSearch() is `#ifndef`` |
|       - |  756 | `` * PH7_DISABLE_BUILTIN_FUNC`, and the exception family exists in the tiny build`` |
|       - |  757 | ` * too, so the one search this file needs is spelled out. */` |
|     ! 0 |  758 | `static int VmExcBlobHas(SyBlob *pBlob,const char *zPat,sxu32 nPat)` |
|     ! 0 |  759 | `{` |
|     ! 0 |  760 | `	const char *z = (const char *)SyBlobData(pBlob);` |
|     ! 0 |  761 | `	sxu32 n = SyBlobLength(pBlob);` |
|       - |  762 | `	sxu32 i;` |
|     ! 0 |  763 | `	if( nPat == 0 \|\| n < nPat ){` |
|     ! 0 |  764 | `		return 0;` |
|       - |  765 | `	}` |
|     ! 0 |  766 | `	for( i = 0 ; i + nPat <= n ; i++ ){` |
|     ! 0 |  767 | `		if( SyMemcmp((const void *)&z[i],(const void *)zPat,nPat) == 0 ){` |
|     ! 0 |  768 | `			return 1;` |
|       - |  769 | `		}` |
|     ! 0 |  770 | `	}` |
|     ! 0 |  771 | `	return 0;` |
|     ! 0 |  772 | `}` |
|       - |  773 | ``/* php's `Z_OBJCE_P == zend_ce_type_error \|\| == zend_ce_argument_count_error`:`` |
|       - |  774 | ` * the two classes whose message __toString finishes with " and defined". */` |
|       6 |  775 | `static int VmExcIsArgError(ph7_vm *pVm,ph7_class_instance *pExc)` |
|       1 |  776 | `{` |
|       7 |  777 | `	ph7_class *pClass = pExc ? pExc->pClass : 0;` |
|       - |  778 | `	ph7_class *pType;` |
|       7 |  779 | `	if( pClass == 0 ){` |
|     ! 0 |  780 | `		return 0;` |
|       - |  781 | `	}` |
|       7 |  782 | `	pType = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,FALSE,0);` |
|       7 |  783 | `	if( pType && pClass == pType ){` |
|     ! 0 |  784 | `		return 1;` |
|       - |  785 | `	}` |
|       7 |  786 | `	pType = PH7_VmExtractClass(&(*pVm),"ArgumentCountError",sizeof("ArgumentCountError")-1,FALSE,0);` |
|       7 |  787 | `	return pType != 0 && pClass == pType;` |
|       4 |  788 | `}` |
|       - |  789 | `/*` |
|       - |  790 | `` * php's zend_trace_to_string: one `#N file(line): Class->method(args)` line per`` |
|       - |  791 | `` * frame, then `#N {main}` with NO trailing newline. A frame with no `file` is`` |
|       - |  792 | `` * php's `[internal function]: `.`` |
|       - |  793 | ` */` |
|     594 |  794 | `static void VmExcTraceString(ph7_vm *pVm,ph7_value *pTrace,SyBlob *pOut)` |
|       5 |  795 | `{` |
|       - |  796 | `	ph7_hashmap *pMap;` |
|       - |  797 | `	ph7_hashmap_node *pEntry;` |
|     599 |  798 | `	sxu32 nFrame = 0;` |
|     599 |  799 | `	if( pTrace && (pTrace->iFlags & MEMOBJ_HASHMAP) && pTrace->x.pOther ){` |
|     599 |  800 | `		pMap = (ph7_hashmap *)pTrace->x.pOther;` |
|       - |  801 | `		/* Insertion order is pFirst then the pPrev chain (rule 12). */` |
|     635 |  802 | `		for( pEntry = pMap->pFirst ; pEntry ; pEntry = pEntry->pPrev ){` |
|      41 |  803 | `			ph7_value *pFrameVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|       - |  804 | `			ph7_hashmap *pFrame;` |
|       - |  805 | `			ph7_value *pFile;` |
|      41 |  806 | `			if( pFrameVal == 0 \|\| (pFrameVal->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     ! 0 |  807 | `				continue;` |
|       - |  808 | `			}` |
|      41 |  809 | `			pFrame = (ph7_hashmap *)pFrameVal->x.pOther;` |
|      41 |  810 | `			SyBlobFormat(pOut,"#%u ",nFrame);` |
|      41 |  811 | `			pFile = VmExcFrameField(&(*pVm),pFrame,"file");` |
|      59 |  812 | `			if( pFile && (pFile->iFlags & MEMOBJ_STRING) ){` |
|      41 |  813 | `				ph7_value *pLine = VmExcFrameField(&(*pVm),pFrame,"line");` |
|      41 |  814 | `				VmExcFrameStr(pOut,pFile);` |
|      77 |  815 | `				SyBlobFormat(pOut,"(%qd): ",` |
|      36 |  816 | `					(pLine && (pLine->iFlags & MEMOBJ_INT)) ? pLine->x.iVal : (sxi64)0);` |
|      23 |  817 | `			}else{` |
|     ! 0 |  818 | `				SyBlobAppend(pOut,"[internal function]: ",sizeof("[internal function]: ")-1);` |
|       - |  819 | `			}` |
|      41 |  820 | `			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"class"));` |
|      41 |  821 | `			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"type"));` |
|      41 |  822 | `			VmExcFrameStr(pOut,VmExcFrameField(&(*pVm),pFrame,"function"));` |
|      41 |  823 | `			SyBlobAppend(pOut,"(",sizeof("(")-1);` |
|       - |  824 | `			{` |
|      41 |  825 | `				ph7_value *pArgs = VmExcFrameField(&(*pVm),pFrame,"args");` |
|      41 |  826 | `				if( pArgs && (pArgs->iFlags & MEMOBJ_HASHMAP) && pArgs->x.pOther ){` |
|     ! 0 |  827 | `					ph7_hashmap *pArgMap = (ph7_hashmap *)pArgs->x.pOther;` |
|       - |  828 | `					ph7_hashmap_node *pArg;` |
|     ! 0 |  829 | `					int bFirst = 1;` |
|     ! 0 |  830 | `					for( pArg = pArgMap->pFirst ; pArg ; pArg = pArg->pPrev ){` |
|     ! 0 |  831 | `						if( !bFirst ){` |
|     ! 0 |  832 | `							SyBlobAppend(pOut,", ",sizeof(", ")-1);` |
|     ! 0 |  833 | `						}` |
|     ! 0 |  834 | `						bFirst = 0;` |
|     ! 0 |  835 | `						VmExcTraceArg(&(*pVm),pOut,` |
|     ! 0 |  836 | `							(ph7_value *)SySetAt(&pVm->aMemObj,pArg->nValIdx));` |
|     ! 0 |  837 | `					}` |
|     ! 0 |  838 | `				}` |
|       - |  839 | `			}` |
|      41 |  840 | `			SyBlobAppend(pOut,")\n",sizeof(")\n")-1);` |
|      41 |  841 | `			nFrame++;` |
|      23 |  842 | `		}` |
|     297 |  843 | `	}` |
|     599 |  844 | `	SyBlobFormat(pOut,"#%u {main}",nFrame);` |
|     599 |  845 | `}` |
|     588 |  846 | `static int vm_builtin_Exception_getTraceAsString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  847 | `{` |
|     593 |  848 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - |  849 | `	SyBlob sOut;` |
|     294 |  850 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     593 |  851 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     593 |  852 | `	VmExcTraceString(pCtx->pVm,pThis ? PH7_NativeAttr(pThis,EXC_TRACE) : 0,&sOut);` |
|     593 |  853 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     593 |  854 | `	SyBlobRelease(&sOut);` |
|     593 |  855 | `	return PH7_OK;` |
|       5 |  856 | `}` |
|       - |  857 | `/*` |
|       - |  858 | ` * php's Exception::__toString.` |
|       - |  859 | ` *` |
|       - |  860 | ` *    C: message in file:line` |
|       - |  861 | ` *    Stack trace:` |
|       - |  862 | ` *    <trace>` |
|       - |  863 | ` *` |
|       - |  864 | ` * The PREVIOUS chain is part of the format and the ORDER is inverted: php builds` |
|       - |  865 | `` * the string innermost-first and joins the shallower ones after `\n\nNext `, so`` |
|       - |  866 | ` * the root cause is printed first. The chunk answered a four-field space-joined` |
|       - |  867 | `` * line instead — `file line code message` — which no php ever produced, and it is`` |
|       - |  868 | `` * what an uncaught exception, `echo $e` and `(string)$e` all show.`` |
|       - |  869 | ` *` |
|       - |  870 | ` * The walk carries its ancestors on the C stack (rule 31): php protects each` |
|       - |  871 | `` * object it visits and stops when it comes back round, and a `$a->previous = $b;`` |
|       - |  872 | `` * $b->previous = $a` pair must not spin.`` |
|       - |  873 | ` */` |
|       - |  874 | `#define EXC_CHAIN_MAX 256` |
|       4 |  875 | `static int vm_builtin_Exception_toString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  876 | `{` |
|       - |  877 | `	ph7_class_instance *apChain[EXC_CHAIN_MAX];` |
|       5 |  878 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       5 |  879 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - |  880 | `	SyBlob sOut;` |
|       5 |  881 | `	int nChain = 0;` |
|       - |  882 | `	int i,j;` |
|       2 |  883 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      11 |  884 | `	while( pThis && nChain < EXC_CHAIN_MAX ){` |
|       - |  885 | `		ph7_class_instance *pPrev;` |
|       9 |  886 | `		for( j = 0 ; j < nChain ; j++ ){` |
|       3 |  887 | `			if( apChain[j] == pThis ){` |
|     ! 0 |  888 | `				pThis = 0;    /* already on the chain: php's recursion protection */` |
|     ! 0 |  889 | `				break;` |
|       - |  890 | `			}` |
|       2 |  891 | `		}` |
|       7 |  892 | `		if( pThis == 0 ){` |
|     ! 0 |  893 | `			break;` |
|       - |  894 | `		}` |
|       7 |  895 | `		apChain[nChain++] = pThis;` |
|       7 |  896 | `		pPrev = PH7_NativeAttrObj(pThis,EXC_PREVIOUS);` |
|       7 |  897 | `		pThis = pPrev;` |
|       1 |  898 | `	}` |
|       - |  899 | `	/* php formats the SHALLOWEST first and pushes each one it has already built` |
|       - |  900 | `	 * behind the next, so the printed order is inverted: the ROOT CAUSE leads and` |
|       - |  901 | ``	 * every caller follows it after `\n\nNext `. */`` |
|       5 |  902 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      11 |  903 | `	for( i = 0 ; i < nChain ; i++ ){` |
|       7 |  904 | `		ph7_class_instance *pExc = apChain[i];` |
|       7 |  905 | `		ph7_value *pLine = PH7_NativeAttr(pExc,EXC_LINE);` |
|       - |  906 | `		SyBlob sMsg;` |
|       - |  907 | `		SyBlob sThis;` |
|       7 |  908 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       7 |  909 | `		SyBlobInit(&sThis,&pVm->sAllocator);` |
|       7 |  910 | `		VmExcValueStr(pVm,PH7_NativeAttr(pExc,EXC_MESSAGE),&sMsg);` |
|       - |  911 | `		/* php's one message rewrite: a TypeError/ArgumentCountError raised at a` |
|       - |  912 | `		 * CALL SITE says "..., called in F on line N", and __toString finishes the` |
|       - |  913 | `		 * sentence with " and defined". */` |
|       6 |  914 | `		if( VmExcIsArgError(pVm,pExc)` |
|       4 |  915 | `		 && VmExcBlobHas(&sMsg,", called in ",sizeof(", called in ")-1) ){` |
|     ! 0 |  916 | `			SyBlobAppend(&sMsg," and defined",sizeof(" and defined")-1);` |
|     ! 0 |  917 | `		}` |
|       7 |  918 | `		SyBlobFormat(&sThis,"%z",&pExc->pClass->sName);` |
|       7 |  919 | `		if( SyBlobLength(&sMsg) > 0 ){` |
|       5 |  920 | `			SyBlobAppend(&sThis,": ",sizeof(": ")-1);` |
|       5 |  921 | `			SyBlobAppend(&sThis,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       2 |  922 | `		}` |
|       7 |  923 | `		SyBlobAppend(&sThis," in ",sizeof(" in ")-1);` |
|       7 |  924 | `		VmExcFrameStr(&sThis,PH7_NativeAttr(pExc,EXC_FILE));` |
|      10 |  925 | `		SyBlobFormat(&sThis,":%qd\nStack trace:\n",` |
|       6 |  926 | `			(pLine && (pLine->iFlags & MEMOBJ_INT)) ? pLine->x.iVal : (sxi64)0);` |
|       7 |  927 | `		VmExcTraceString(pVm,PH7_NativeAttr(pExc,EXC_TRACE),&sThis);` |
|       7 |  928 | `		if( SyBlobLength(&sOut) > 0 ){` |
|       3 |  929 | `			SyBlobAppend(&sThis,"\n\nNext ",sizeof("\n\nNext ")-1);` |
|       3 |  930 | `			SyBlobAppend(&sThis,SyBlobData(&sOut),SyBlobLength(&sOut));` |
|       1 |  931 | `		}` |
|       7 |  932 | `		SyBlobReset(&sOut);` |
|       7 |  933 | `		SyBlobAppend(&sOut,SyBlobData(&sThis),SyBlobLength(&sThis));` |
|       7 |  934 | `		SyBlobRelease(&sMsg);` |
|       7 |  935 | `		SyBlobRelease(&sThis);` |
|       4 |  936 | `	}` |
|       5 |  937 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|       5 |  938 | `	SyBlobRelease(&sOut);` |
|       5 |  939 | `	return PH7_OK;` |
|       1 |  940 | `}` |
|       - |  941 | `/*` |
|       - |  942 | ` * The declaration. php's two roots carry the same eleven methods and the same` |
|       - |  943 | `` * seven slots; the only difference php's stub records is `Error::$line`, which`` |
|       - |  944 | ` * has NO default where Exception's is 0.` |
|       - |  945 | ` *` |
|       - |  946 | `` * PH7_CLASS_NOCLONE on EVERY row: php refuses `clone $e` outright, and a native`` |
|       - |  947 | ` * subclass does not inherit its parent's class flags (rule 29).` |
|       - |  948 | ` */` |
|       - |  949 | `#define EXC_METHODS(zCtor,xCtor) \` |
|       - |  950 | `	{ "__clone",          PH7_MOD_PRIVATE, "", "void", vm_builtin_Exception_clone }, \` |
|       - |  951 | `	{ "__construct",      PH7_MOD_PUBLIC, zCtor, 0, xCtor }, \` |
|       - |  952 | `	{ "__wakeup",         PH7_MOD_PUBLIC, "", "@void", vm_builtin_Exception_wakeup }, \` |
|       - |  953 | `	{ "getMessage",       PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "string", \` |
|       - |  954 | `	  vm_builtin_Exception_getMessage }, \` |
|       - |  955 | `	{ "getCode",          PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", 0, \` |
|       - |  956 | `	  vm_builtin_Exception_getCode }, \` |
|       - |  957 | `	{ "getFile",          PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "string", \` |
|       - |  958 | `	  vm_builtin_Exception_getFile }, \` |
|       - |  959 | `	{ "getLine",          PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "int", \` |
|       - |  960 | `	  vm_builtin_Exception_getLine }, \` |
|       - |  961 | `	{ "getTrace",         PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "array", \` |
|       - |  962 | `	  vm_builtin_Exception_getTrace }, \` |
|       - |  963 | `	{ "getPrevious",      PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "?Throwable", \` |
|       - |  964 | `	  vm_builtin_Exception_getPrevious }, \` |
|       - |  965 | `	{ "getTraceAsString", PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "string", \` |
|       - |  966 | `	  vm_builtin_Exception_getTraceAsString }, \` |
|       - |  967 | `	{ "__toString",       PH7_MOD_PUBLIC, "", "string", vm_builtin_Exception_toString }` |
|       - |  968 | `#define EXC_CTOR_SIG "string $message = \"\", int $code = 0, ?Throwable $previous = null"` |
|       - |  969 | `/* php's seven slots, twice: the only difference between the two roots is` |
|       - |  970 | `` * `Error::$line`, which php's stub declares with NO default where Exception's is`` |
|       - |  971 | `` * 0 (`PH7_NATIVE_VAL_NONE` — its hasDefaultValue() is false and the export`` |
|       - |  972 | `` * prints `protected int $line` bare). `message` and `code` are the two php leaves`` |
|       - |  973 | ` * UNTYPED, and its stub says why: BC, since a subclass may have assigned` |
|       - |  974 | ` * anything to them. */` |
|       - |  975 | `#define EXC_PROP_HEAD \` |
|       - |  976 | `	{ EXC_MESSAGE,  PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 }, \` |
|       - |  977 | `	{ EXC_STRING,   PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" }, \` |
|       - |  978 | `	{ EXC_CODE,     PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 }, \` |
|       - |  979 | `	{ EXC_FILE,     PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, "string" }` |
|       - |  980 | `#define EXC_PROP_TAIL \` |
|       - |  981 | `	{ EXC_TRACE,    PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_ARRAY, 0, 0, 0.0 }, "array" }, \` |
|       - |  982 | `	{ EXC_PREVIOUS, PH7_MOD_PRIVATE,   { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?Throwable" }` |
|    4670 |  983 | `static sxi32 VmInstallExceptions(ph7_vm *pVm)` |
|       5 |  984 | `{` |
|       - |  985 | `	static const PH7_NativeMethodDef aExcMethod[] = {` |
|       - |  986 | `		EXC_METHODS(EXC_CTOR_SIG,vm_builtin_Exception_construct)` |
|       - |  987 | `	};` |
|       - |  988 | `	static const PH7_NativePropDef aExcProp[] = {` |
|       - |  989 | `		EXC_PROP_HEAD,` |
|       - |  990 | `		{ EXC_LINE, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, "int" },` |
|       - |  991 | `		EXC_PROP_TAIL` |
|       - |  992 | `	};` |
|       - |  993 | `	static const PH7_NativePropDef aErrProp[] = {` |
|       - |  994 | `		EXC_PROP_HEAD,` |
|       - |  995 | `		{ EXC_LINE, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|       - |  996 | `		EXC_PROP_TAIL` |
|       - |  997 | `	};` |
|       - |  998 | `	static const PH7_NativePropDef aErrExcProp[] = {` |
|       - |  999 | `		{ EXC_SEVERITY, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 1, 0, 0.0 }, "int" },` |
|       - | 1000 | `	};` |
|       - | 1001 | `	static const PH7_NativeMethodDef aErrExcMethod[] = {` |
|       - | 1002 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|       - | 1003 | `		  "string $message = \"\", int $code = 0, int $severity = E_ERROR, "` |
|       - | 1004 | `		  "?string $filename = null, ?int $line = null, ?Throwable $previous = null", 0,` |
|       - | 1005 | `		  vm_builtin_ErrorException_construct },` |
|       - | 1006 | `		{ "getSeverity", PH7_MOD_PUBLIC\|PH7_MOD_FINAL, "", "int",` |
|       - | 1007 | `		  vm_builtin_ErrorException_getSeverity },` |
|       - | 1008 | `	};` |
|       - | 1009 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1010 | `		{ "Exception", 0, "Throwable", PH7_CLASS_NOCLONE,` |
|       - | 1011 | `		  aExcMethod, SX_ARRAYSIZE(aExcMethod), 0, 0, aExcProp, SX_ARRAYSIZE(aExcProp), 0, 0, 0 },` |
|       - | 1012 | `		{ "Error", 0, "Throwable", PH7_CLASS_NOCLONE,` |
|       - | 1013 | `		  aExcMethod, SX_ARRAYSIZE(aExcMethod), 0, 0, aErrProp, SX_ARRAYSIZE(aErrProp), 0, 0, 0 },` |
|       - | 1014 | `		/* Zend's own subclasses, then ErrorException, then SPL's tree. Every row is` |
|       - | 1015 | `		 * declaration-only in php too. */` |
|       - | 1016 | `		{ "TypeError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1017 | `		{ "ArgumentCountError", "TypeError", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1018 | `		{ "ValueError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1019 | `		{ "FiberError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1020 | `		{ "AssertionError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1021 | `		{ "ArithmeticError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1022 | `		{ "DivisionByZeroError", "ArithmeticError", 0, PH7_CLASS_NOCLONE,` |
|       - | 1023 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1024 | `		{ "UnhandledMatchError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1025 | `		{ "CompileError", "Error", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1026 | `		{ "ParseError", "CompileError", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1027 | `		{ "ErrorException", "Exception", 0, PH7_CLASS_NOCLONE,` |
|       - | 1028 | `		  aErrExcMethod, SX_ARRAYSIZE(aErrExcMethod), 0, 0,` |
|       - | 1029 | `		  aErrExcProp, SX_ARRAYSIZE(aErrExcProp), 0, 0, 0 },` |
|       - | 1030 | `		{ "LogicException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1031 | `		{ "RuntimeException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1032 | `		{ "BadFunctionCallException", "LogicException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1033 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1034 | `		{ "BadMethodCallException", "BadFunctionCallException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1035 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1036 | `		{ "DomainException", "LogicException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1037 | `		{ "InvalidArgumentException", "LogicException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1038 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1039 | `		{ "LengthException", "LogicException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1040 | `		{ "OutOfRangeException", "LogicException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1041 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1042 | `		{ "OutOfBoundsException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1043 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1044 | `		{ "OverflowException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1045 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1046 | `		{ "RangeException", "RuntimeException", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1047 | `		{ "UnderflowException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1048 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1049 | `		{ "UnexpectedValueException", "RuntimeException", 0, PH7_CLASS_NOCLONE,` |
|       - | 1050 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1051 | `		{ "JsonException", "Exception", 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1052 | `	};` |
|    4675 | 1053 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|       5 | 1054 | `}` |
|       - | 1055 | `/*` |
|       - | 1056 | ` * The eleven core interfaces, declared from C.` |
|       - | 1057 | ` *` |
|       - | 1058 | ` * They are contracts -- no method here has a body, every row is` |
|       - | 1059 | ` * PH7_MOD_ABSTRACT -- so the conversion is entirely about what the DECLARATION` |
|       - | 1060 | ` * says, which is where a chunk fell short in four php-visible ways:` |
|       - | 1061 | ` *` |
|       - | 1062 | `` *  - php's `interface Throwable extends Stringable`: the chunk redeclared`` |
|       - | 1063 | ` *    __toString() on Throwable instead, so no Exception was ever Stringable` |
|       - | 1064 | `` *    (`$e instanceof Stringable` was false, and Reflection attributed the`` |
|       - | 1065 | `` *    method to Throwable rather than printing php's `inherits Stringable`);`` |
|       - | 1066 | ` *  - php declares a RETURN TYPE on all but three of these methods and marks` |
|       - | 1067 | `` *    nearly all of them TENTATIVE (the leading `@`, rule 45) -- a chunk has no`` |
|       - | 1068 | ` *    way to say tentative at all;` |
|       - | 1069 | `` *  - php's `mixed` on ArrayAccess's offsets, which the chunk left untyped;`` |
|       - | 1070 | ` *  - method ORDER, which Reflection prints: php lists Throwable's getPrevious` |
|       - | 1071 | ` *    before getTraceAsString, and Iterator's as current/next/key/valid/rewind.` |
|       - | 1072 | ` *` |
|       - | 1073 | ` * Order within the table is php's stub order too; the declare-then-link phases` |
|       - | 1074 | ` * of PH7_InstallNativeClasses let Throwable name Stringable and Iterator name` |
|       - | 1075 | `` * Traversable regardless of row order. An interface's parent is `zParent`, not`` |
|       - | 1076 | `` * `zImplements` (Reflection walks pBase to attribute an inherited method).`` |
|       - | 1077 | ` */` |
|    4670 | 1078 | `static sxi32 VmInstallCoreInterfaces(ph7_vm *pVm)` |
|       5 | 1079 | `{` |
|       - | 1080 | `	static const PH7_NativeMethodDef aStringable[] = {` |
|       - | 1081 | `		{ "__toString", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1082 | `	};` |
|       - | 1083 | `	static const PH7_NativeMethodDef aThrowable[] = {` |
|       - | 1084 | `		/* Not one of these is tentative: php's Throwable is a real contract. */` |
|       - | 1085 | `		{ "getMessage",       PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1086 | `		{ "getCode",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", 0, 0 },` |
|       - | 1087 | `		{ "getFile",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1088 | `		{ "getLine",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "int", 0 },` |
|       - | 1089 | `		{ "getTrace",         PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "array", 0 },` |
|       - | 1090 | `		{ "getPrevious",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "?Throwable", 0 },` |
|       - | 1091 | `		{ "getTraceAsString", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "string", 0 },` |
|       - | 1092 | `	};` |
|       - | 1093 | `	static const PH7_NativeMethodDef aArrayAccess[] = {` |
|       - | 1094 | `		{ "offsetExists", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset", "@bool", 0 },` |
|       - | 1095 | `		{ "offsetGet",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset", "@mixed", 0 },` |
|       - | 1096 | `		{ "offsetSet",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset, mixed $value",` |
|       - | 1097 | `		  "@void", 0 },` |
|       - | 1098 | `		{ "offsetUnset",  PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "mixed $offset", "@void", 0 },` |
|       - | 1099 | `	};` |
|       - | 1100 | `	static const PH7_NativeMethodDef aCountable[] = {` |
|       - | 1101 | `		{ "count", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@int", 0 },` |
|       - | 1102 | `	};` |
|       - | 1103 | `	static const PH7_NativeMethodDef aJsonSerializable[] = {` |
|       - | 1104 | `		{ "jsonSerialize", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@mixed", 0 },` |
|       - | 1105 | `	};` |
|       - | 1106 | `	/* The concrete cases()/from()/tryFrom() an enum gets are native methods` |
|       - | 1107 | `	 * declared to match these (oo_native.c, PH7_InstallEnumInterfaceMethods). */` |
|       - | 1108 | `	static const PH7_NativeMethodDef aUnitEnum[] = {` |
|       - | 1109 | `		{ "cases", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_ABSTRACT, "", "array", 0 },` |
|       - | 1110 | `	};` |
|       - | 1111 | `	static const PH7_NativeMethodDef aBackedEnum[] = {` |
|       - | 1112 | `		{ "from",    PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_ABSTRACT, "string\|int $value",` |
|       - | 1113 | `		  "static", 0 },` |
|       - | 1114 | `		{ "tryFrom", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_ABSTRACT, "string\|int $value",` |
|       - | 1115 | `		  "?static", 0 },` |
|       - | 1116 | `	};` |
|       - | 1117 | `	static const PH7_NativeMethodDef aIterator[] = {` |
|       - | 1118 | `		{ "current", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@mixed", 0 },` |
|       - | 1119 | `		{ "next",    PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@void", 0 },` |
|       - | 1120 | `		{ "key",     PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@mixed", 0 },` |
|       - | 1121 | `		{ "valid",   PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@bool", 0 },` |
|       - | 1122 | `		{ "rewind",  PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@void", 0 },` |
|       - | 1123 | `	};` |
|       - | 1124 | `	static const PH7_NativeMethodDef aIteratorAggregate[] = {` |
|       - | 1125 | `		{ "getIterator", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "@Traversable", 0 },` |
|       - | 1126 | `	};` |
|       - | 1127 | `	/* php's legacy Serializable declares NO return type on either method. */` |
|       - | 1128 | `	static const PH7_NativeMethodDef aSerializable[] = {` |
|       - | 1129 | `		{ "serialize",   PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", 0, 0 },` |
|       - | 1130 | `		{ "unserialize", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "string $data", 0, 0 },` |
|       - | 1131 | `	};` |
|       - | 1132 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1133 | `		{ "Traversable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1134 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1135 | `		{ "Stringable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1136 | `		  aStringable, SX_ARRAYSIZE(aStringable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1137 | `		{ "Throwable", "Stringable", 0, PH7_CLASS_INTERFACE,` |
|       - | 1138 | `		  aThrowable, SX_ARRAYSIZE(aThrowable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1139 | `		{ "ArrayAccess", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1140 | `		  aArrayAccess, SX_ARRAYSIZE(aArrayAccess), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1141 | `		{ "Countable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1142 | `		  aCountable, SX_ARRAYSIZE(aCountable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1143 | `		{ "JsonSerializable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1144 | `		  aJsonSerializable, SX_ARRAYSIZE(aJsonSerializable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1145 | `		{ "UnitEnum", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1146 | `		  aUnitEnum, SX_ARRAYSIZE(aUnitEnum), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1147 | `		{ "BackedEnum", "UnitEnum", 0, PH7_CLASS_INTERFACE,` |
|       - | 1148 | `		  aBackedEnum, SX_ARRAYSIZE(aBackedEnum), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1149 | `		{ "Iterator", "Traversable", 0, PH7_CLASS_INTERFACE,` |
|       - | 1150 | `		  aIterator, SX_ARRAYSIZE(aIterator), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1151 | `		{ "IteratorAggregate", "Traversable", 0, PH7_CLASS_INTERFACE,` |
|       - | 1152 | `		  aIteratorAggregate, SX_ARRAYSIZE(aIteratorAggregate), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1153 | `		{ "Serializable", 0, 0, PH7_CLASS_INTERFACE,` |
|       - | 1154 | `		  aSerializable, SX_ARRAYSIZE(aSerializable), 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1155 | `	};` |
|    4675 | 1156 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|       5 | 1157 | `}` |
|       - | 1158 | `/*` |
|       - | 1159 | ` * ---------------------------------------------------------------------------` |
|       - | 1160 | `` * php's Directory — the object `dir()` answers.`` |
|       - | 1161 | ` *` |
|       - | 1162 | ` * php declares it FINAL with **no constructor at all**: the class is created by` |
|       - | 1163 | `` * `dir()` and `new Directory` is refused in the create_object handler, with a`` |
|       - | 1164 | ` * sentence that names dir() as the way to get one. Its two slots are` |
|       - | 1165 | `` * `public protected(set) readonly`, so a script can read `$d->path` and never`` |
|       - | 1166 | `` * write it, and its three methods declare return types (`read(): string\|false`).`` |
|       - | 1167 | ` * The chunk had a public constructor, a __destruct php does not declare, no` |
|       - | 1168 | ` * types anywhere and writable slots.` |
|       - | 1169 | ` * ---------------------------------------------------------------------------` |
|       - | 1170 | ` */` |
|       - | 1171 | `#define DIR_HANDLE "handle"` |
|       - | 1172 | `#define DIR_PATH   "path"` |
|       - | 1173 | `/*` |
|       - | 1174 | ` * Forward one method to the engine's own directory builtin (rule 7: call, don't` |
|       - | 1175 | `` * reimplement). php's Directory methods are `php_stream_readdir(...)` on the very`` |
|       - | 1176 | `` * stream `readdir()` uses, and a CLOSED handle is a TypeError there — the one`` |
|       - | 1177 | ` * place php's wording names the class rather than the function.` |
|       - | 1178 | ` */` |
|      22 | 1179 | `static int VmDirClosed(ph7_value *pHandle)` |
|       1 | 1180 | `{` |
|      23 | 1181 | `	io_private *pDev = (io_private *)pHandle->x.pOther;` |
|      23 | 1182 | `	return IO_PRIVATE_INVALID(pDev);` |
|       1 | 1183 | `}` |
|      22 | 1184 | `static int VmDirForward(ph7_context *pCtx,const char *zFunc,const char *zMethod)` |
|       1 | 1185 | `{` |
|      23 | 1186 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      23 | 1187 | `	ph7_value *pHandle = pThis ? PH7_NativeAttr(pThis,DIR_HANDLE) : 0;` |
|       - | 1188 | `	ph7_value *apArg[1];` |
|       - | 1189 | `	ph7_value sResult;` |
|       - | 1190 | `	ph7_value sName;` |
|       - | 1191 | `	SyString sStr;` |
|       - | 1192 | `	sxi32 rc;` |
|       - | 1193 | ``	/* php's check is `php_stream_from_zval` on a stream it CLOSED: closedir()`` |
|       - | 1194 | `	 * keeps the resource alive and marks it (gettype() answers` |
|       - | 1195 | `	 * "resource (closed)"), so the test is the magic, not the type. */` |
|      22 | 1196 | `	if( pHandle == 0 \|\| (pHandle->iFlags & MEMOBJ_RES) == 0` |
|      23 | 1197 | `	 \|\| VmDirClosed(pHandle) ){` |
|      10 | 1198 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1199 | `			"Directory::%s(): cannot use Directory resource after it has been closed",` |
|       3 | 1200 | `			zMethod);` |
|       - | 1201 | `	}` |
|      17 | 1202 | `	SyStringInitFromBuf(&sStr,zFunc,SyStrlen(zFunc));` |
|      17 | 1203 | `	PH7_MemObjInit(pCtx->pVm,&sName);` |
|      17 | 1204 | `	PH7_MemObjInitFromString(pCtx->pVm,&sName,&sStr);` |
|      17 | 1205 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      17 | 1206 | `	apArg[0] = pHandle;` |
|      17 | 1207 | `	rc = PH7_VmCallUserFunction(pCtx->pVm,&sName,1,apArg,&sResult);` |
|      17 | 1208 | `	PH7_MemObjRelease(&sName);` |
|      17 | 1209 | `	if( rc == SXRET_OK ){` |
|      17 | 1210 | `		ph7_result_value(pCtx,&sResult);` |
|       8 | 1211 | `	}` |
|      17 | 1212 | `	PH7_MemObjRelease(&sResult);` |
|      17 | 1213 | `	return PH7_OK;` |
|      12 | 1214 | `}` |
|      14 | 1215 | `static int vm_builtin_Directory_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1216 | `{` |
|       7 | 1217 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|      15 | 1218 | `	return VmDirForward(pCtx,"readdir","read");` |
|       1 | 1219 | `}` |
|       4 | 1220 | `static int vm_builtin_Directory_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1221 | `{` |
|       2 | 1222 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       5 | 1223 | `	return VmDirForward(pCtx,"rewinddir","rewind");` |
|       1 | 1224 | `}` |
|       4 | 1225 | `static int vm_builtin_Directory_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1226 | `{` |
|       2 | 1227 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       5 | 1228 | `	return VmDirForward(pCtx,"closedir","close");` |
|       1 | 1229 | `}` |
|    4670 | 1230 | `static sxi32 VmInstallDirectory(ph7_vm *pVm)` |
|       5 | 1231 | `{` |
|       - | 1232 | `	static const PH7_NativePropDef aDirProp[] = {` |
|       - | 1233 | `		{ DIR_PATH,   PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1234 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|       - | 1235 | `		{ DIR_HANDLE, PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1236 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "mixed" },` |
|       - | 1237 | `	};` |
|       - | 1238 | `	static const PH7_NativeMethodDef aDirMethod[] = {` |
|       - | 1239 | `		{ "close",  PH7_MOD_PUBLIC, "", "void", vm_builtin_Directory_close },` |
|       - | 1240 | `		{ "rewind", PH7_MOD_PUBLIC, "", "void", vm_builtin_Directory_rewind },` |
|       - | 1241 | `		{ "read",   PH7_MOD_PUBLIC, "", "string\|false", vm_builtin_Directory_read },` |
|       - | 1242 | `	};` |
|       - | 1243 | `	static const PH7_NativeClassSpec sSpec = {` |
|       - | 1244 | `		"Directory", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE,` |
|       - | 1245 | `		aDirMethod, SX_ARRAYSIZE(aDirMethod), 0, 0,` |
|       - | 1246 | `		aDirProp, SX_ARRAYSIZE(aDirProp), 0, 0, 0` |
|       - | 1247 | `	};` |
|    4675 | 1248 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|    4675 | 1249 | `	if( rc == SXRET_OK ){` |
|    4675 | 1250 | `		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"Directory",sizeof("Directory")-1,FALSE,0);` |
|    4675 | 1251 | `		if( pClass ){` |
|       - | 1252 | `			/* php words this refusal per class rather than with the generic` |
|       - | 1253 | `			 * "Instantiation of class %s is not allowed". */` |
|    4675 | 1254 | `			pClass->zNewRefusal = "Cannot directly construct Directory, use dir() instead";` |
|    2335 | 1255 | `		}` |
|    2335 | 1256 | `	}` |
|    4675 | 1257 | `	return rc;` |
|       5 | 1258 | `}` |
|       - | 1259 | `/*` |
|       - | 1260 | ` * ---------------------------------------------------------------------------` |
|       - | 1261 | ` * php's two attribute classes.` |
|       - | 1262 | ` *` |
|       - | 1263 | ``  * Both carry an ATTRIBUTE of their own — `#[Attribute(Attribute::TARGET_CLASS)]` `` |
|       - | 1264 | ` * on Attribute, a target mask on Deprecated — and those records are load-bearing` |
|       - | 1265 | ` * rather than decorative: the engine reads them to decide whether a user's` |
|       - | 1266 | `` * `#[Deprecated]` may sit where it does, and ReflectionAttribute answers them.`` |
|       - | 1267 | ` * A compiled attribute holds its argument as byte-code, so this is what` |
|       - | 1268 | `` * `PH7_NativeClassAddAttribute()` exists for (rule 11's next unused corner,`` |
|       - | 1269 | ` * exercised here): the argument rides as a literal.` |
|       - | 1270 | ` *` |
|       - | 1271 | ` * php's Deprecated mask is 87 — TARGET_CLASS\|FUNCTION\|METHOD\|CLASS_CONSTANT\|` |
|       - | 1272 | ` * CONSTANT — where the chunk wrote 86 and left the CLASS bit out.` |
|       - | 1273 | ` * ---------------------------------------------------------------------------` |
|       - | 1274 | ` */` |
|       8 | 1275 | `static int vm_builtin_Attribute_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1276 | `{` |
|       9 | 1277 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       9 | 1278 | `	if( pThis ){` |
|      16 | 1279 | `		PH7_NativeSetAttrInt(pCtx->pVm,pThis,"flags",` |
|       7 | 1280 | `			nArg > 0 ? ph7_value_to_int64(apArg[0]) : 127);` |
|       4 | 1281 | `	}` |
|       9 | 1282 | `	return PH7_OK;` |
|       1 | 1283 | `}` |
|      10 | 1284 | `static int vm_builtin_Deprecated_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1285 | `{` |
|      11 | 1286 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - | 1287 | `	static const char *const azSlot[] = { "message", "since" };` |
|       - | 1288 | `	int n;` |
|      11 | 1289 | `	if( pThis == 0 ){` |
|     ! 0 | 1290 | `		return PH7_OK;` |
|       - | 1291 | `	}` |
|      31 | 1292 | `	for( n = 0 ; n < 2 ; n++ ){` |
|       - | 1293 | `		ph7_value sVal;` |
|      21 | 1294 | `		PH7_MemObjInit(pCtx->pVm,&sVal);` |
|      21 | 1295 | `		if( n < nArg ){` |
|      15 | 1296 | `			PH7_MemObjStore(apArg[n],&sVal);` |
|       7 | 1297 | `		}` |
|      21 | 1298 | `		PH7_NativeSetProp(pCtx->pVm,pThis,azSlot[n],SyStrlen(azSlot[n]),&sVal);` |
|      21 | 1299 | `		PH7_MemObjRelease(&sVal);` |
|      11 | 1300 | `	}` |
|      11 | 1301 | `	return PH7_OK;` |
|       6 | 1302 | `}` |
|    4670 | 1303 | `static sxi32 VmInstallAttributes(ph7_vm *pVm)` |
|       5 | 1304 | `{` |
|       - | 1305 | `	static const PH7_NativeConstDef aAttrConst[] = {` |
|       - | 1306 | `		{ "TARGET_CLASS",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },` |
|       - | 1307 | `		{ "TARGET_FUNCTION",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|       - | 1308 | `		{ "TARGET_METHOD",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4, 0, 0.0 },` |
|       - | 1309 | `		{ "TARGET_PROPERTY",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 8, 0, 0.0 },` |
|       - | 1310 | `		{ "TARGET_CLASS_CONSTANT", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16, 0, 0.0 },` |
|       - | 1311 | `		{ "TARGET_PARAMETER",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32, 0, 0.0 },` |
|       - | 1312 | `		{ "TARGET_CONSTANT",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64, 0, 0.0 },` |
|       - | 1313 | `		{ "TARGET_ALL",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 127, 0, 0.0 },` |
|       - | 1314 | `		{ "IS_REPEATABLE",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 128, 0, 0.0 },` |
|       - | 1315 | `	};` |
|       - | 1316 | ``	/* php declares `public int $flags;` — typed, NO default (the constructor is`` |
|       - | 1317 | ``	 * the only writer), which is what the chunk's `public $flags;` could not say. */`` |
|       - | 1318 | `	static const PH7_NativePropDef aAttrProp[] = {` |
|       - | 1319 | `		{ "flags", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|       - | 1320 | `	};` |
|       - | 1321 | `	static const PH7_NativeMethodDef aAttrMethod[] = {` |
|       - | 1322 | `		{ "__construct", PH7_MOD_PUBLIC, "int $flags = Attribute::TARGET_ALL", 0,` |
|       - | 1323 | `		  vm_builtin_Attribute_construct },` |
|       - | 1324 | `	};` |
|       - | 1325 | `	static const PH7_NativePropDef aDepProp[] = {` |
|       - | 1326 | `		{ "message", PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1327 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "?string" },` |
|       - | 1328 | `		{ "since",   PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|       - | 1329 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "?string" },` |
|       - | 1330 | `	};` |
|       - | 1331 | `	static const PH7_NativeMethodDef aDepMethod[] = {` |
|       - | 1332 | `		{ "__construct", PH7_MOD_PUBLIC, "?string $message = null, ?string $since = null", 0,` |
|       - | 1333 | `		  vm_builtin_Deprecated_construct },` |
|       - | 1334 | `	};` |
|       - | 1335 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1336 | `		{ "Attribute", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1337 | `		  aAttrMethod, SX_ARRAYSIZE(aAttrMethod), aAttrConst, SX_ARRAYSIZE(aAttrConst),` |
|       - | 1338 | `		  aAttrProp, SX_ARRAYSIZE(aAttrProp), 0, 0, 0 },` |
|       - | 1339 | `		{ "Deprecated", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1340 | `		  aDepMethod, SX_ARRAYSIZE(aDepMethod), 0, 0,` |
|       - | 1341 | `		  aDepProp, SX_ARRAYSIZE(aDepProp), 0, 0, 0 },` |
|       - | 1342 | `	};` |
|       - | 1343 | `	static const PH7_NativeAttrArg aOnAttribute[] = {` |
|       - | 1344 | `		{ 0, { 0, 0, PH7_NATIVE_VAL_INT, 1, 0, 0.0 } },   /* TARGET_CLASS */` |
|       - | 1345 | `	};` |
|       - | 1346 | `	static const PH7_NativeAttrArg aOnDeprecated[] = {` |
|       - | 1347 | `		{ 0, { 0, 0, PH7_NATIVE_VAL_INT, 87, 0, 0.0 } },  /* php's own mask */` |
|       - | 1348 | `	};` |
|    4675 | 1349 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|    4675 | 1350 | `	if( rc == SXRET_OK ){` |
|    7010 | 1351 | `		rc = PH7_NativeClassAddAttribute(&(*pVm),` |
|    2335 | 1352 | `			PH7_VmExtractClass(&(*pVm),"Attribute",sizeof("Attribute")-1,FALSE,0),` |
|       - | 1353 | `			"Attribute",aOnAttribute,SX_ARRAYSIZE(aOnAttribute));` |
|    2335 | 1354 | `	}` |
|    4675 | 1355 | `	if( rc == SXRET_OK ){` |
|    7010 | 1356 | `		rc = PH7_NativeClassAddAttribute(&(*pVm),` |
|    2335 | 1357 | `			PH7_VmExtractClass(&(*pVm),"Deprecated",sizeof("Deprecated")-1,FALSE,0),` |
|       - | 1358 | `			"Attribute",aOnDeprecated,SX_ARRAYSIZE(aOnDeprecated));` |
|    2335 | 1359 | `	}` |
|    4675 | 1360 | `	return rc;` |
|       5 | 1361 | `}` |
|       - | 1362 | `/*` |
|       - | 1363 | ` * stdClass and Random\RandomException.` |
|       - | 1364 | ` *` |
|       - | 1365 | ` * stdClass is EMPTY in php too — it holds only dynamic properties — so the whole` |
|       - | 1366 | `` * declaration is the row. `Random\RandomException` is the first NAMESPACED class`` |
|       - | 1367 | ` * declared from C: the engine keys its class table by the FULLY QUALIFIED name` |
|       - | 1368 | `` * (the compiler resolves `namespace Random { class RandomException }` to exactly`` |
|       - | 1369 | ` * this string before installing), so a spec row spells the FQN and needs no` |
|       - | 1370 | ` * namespace machinery at all. It also retires the chunk this file kept ALONE for` |
|       - | 1371 | `` * it, whose comment explains why: a `namespace` declaration is not reset at its`` |
|       - | 1372 | ` * closing brace here, so anything following it in the same chunk would have` |
|       - | 1373 | ` * leaked into the Random namespace.` |
|       - | 1374 | ` */` |
|    4670 | 1375 | `static sxi32 VmInstallStdClasses(ph7_vm *pVm)` |
|       5 | 1376 | `{` |
|       - | 1377 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1378 | `		{ "stdClass", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1379 | `		/* unserialize()'s carrier for a disallowed or unknown class: as empty as` |
|       - | 1380 | `		 * stdClass (its properties are the payload's, created dynamically); what` |
|       - | 1381 | `		 * makes it special is the pVm->pIncClass checks at the access sites. */` |
|       - | 1382 | `		{ "__PHP_Incomplete_Class", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1383 | `		{ "Random\\RandomException", "Exception", 0, PH7_CLASS_NOCLONE,` |
|       - | 1384 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 1385 | `	};` |
|    4675 | 1386 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|       5 | 1387 | `}` |
|    4670 | 1388 | `PH7_PRIVATE sxi32 PH7_VmInstallBuiltinLib(ph7_vm *pVm)` |
|       5 | 1389 | `{` |
|       - | 1390 | `	SyString sBuiltin;` |
|       - | 1391 | `	/* The interfaces first: everything below implements one of them` |
|       - | 1392 | `	 * (Exception implements Throwable). */` |
|    4675 | 1393 | `	VmInstallCoreInterfaces(&(*pVm));` |
|    4675 | 1394 | `	VmInstallExceptions(&(*pVm));` |
|    4675 | 1395 | `	VmInstallStdClasses(&(*pVm));` |
|    4675 | 1396 | `	VmInstallDirectory(&(*pVm));` |
|    4675 | 1397 | `	VmInstallAttributes(&(*pVm));` |
|    4675 | 1398 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|       - | 1399 | `	/* Compile the built-in library */` |
|    4675 | 1400 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|    4675 | 1401 | `	return SXRET_OK;` |
|       5 | 1402 | `}` |
|       - | 1403 |  |
