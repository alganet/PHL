# src/ph7/vm_builtin_session.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4/4 lines (100.00%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    4 | ` */` |
|    - |    5 | `#include "ph7int.h"` |
|    - |    6 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |    7 | `#ifndef PH7_DISABLE_DISK_IO` |
|    - |    8 | `/*` |
|    - |    9 | ` * Sessions (NEWPLAN band D): file-backed session_* functions over the` |
|    - |   10 | ` * existing $_SESSION superglobal. Embedded-PHP chunk, no C thunks — the` |
|    - |   11 | ` * needed primitives (file IO, random_bytes, serialize, headers_sent,` |
|    - |   12 | ` * register_shutdown_function) are all hosted builtins. Session files use` |
|    - |   13 | `` * php's DEFAULT "php" serialize handler format (`key\|<serialized>` runs),`` |
|    - |   14 | ` * decoded by a chunk-level fragment scanner, so files interoperate with a` |
|    - |   15 | ` * stock php install both ways.` |
|    - |   16 | ` */` |
|    - |   17 |  |
|    - |   18 | `static const char zSessionLib[] =` |
|    - |   19 | `"class __SessS {"` |
|    - |   20 | `" public static $status = 1;"` |
|    - |   21 | `" public static $id = '';"` |
|    - |   22 | `" public static $name = 'PHPSESSID';"` |
|    - |   23 | `" public static $path = '';"` |
|    - |   24 | `" public static $wired = false;"` |
|    - |   25 | `"}"` |
|    - |   26 | `"function session_status(){ return __SessS::$status; }"` |
|    - |   27 | `"function session_id($id = null){"` |
|    - |   28 | `" $old = __SessS::$id;"` |
|    - |   29 | `" if( $id === null ){ return $old; }"` |
|    - |   30 | `" if( __SessS::$status === PHP_SESSION_ACTIVE ){"` |
|    - |   31 | `"  trigger_error('session_id(): Session ID cannot be changed when a session is"` |
|    - |   32 | `" active', E_USER_WARNING);"` |
|    - |   33 | `"  return false;"` |
|    - |   34 | `" }"` |
|    - |   35 | `" if( headers_sent() ){"` |
|    - |   36 | `"  trigger_error('session_id(): Session ID cannot be changed after headers have"` |
|    - |   37 | `" already been sent', E_USER_WARNING);"` |
|    - |   38 | `"  return false;"` |
|    - |   39 | `" }"` |
|    - |   40 | `" __SessS::$id = (string)$id;"` |
|    - |   41 | `" return $old;"` |
|    - |   42 | `"}"` |
|    - |   43 | `"function session_name($name = null){"` |
|    - |   44 | `" $old = __SessS::$name;"` |
|    - |   45 | `" if( $name === null ){ return $old; }"` |
|    - |   46 | `" if( __SessS::$status === PHP_SESSION_ACTIVE ){"` |
|    - |   47 | `"  trigger_error('session_name(): Session name cannot be changed when a session"` |
|    - |   48 | `" is active', E_USER_WARNING);"` |
|    - |   49 | `"  return false;"` |
|    - |   50 | `" }"` |
|    - |   51 | `" if( headers_sent() ){"` |
|    - |   52 | `"  trigger_error('session_name(): Session name cannot be changed after headers"` |
|    - |   53 | `" have already been sent', E_USER_WARNING);"` |
|    - |   54 | `"  return false;"` |
|    - |   55 | `" }"` |
|    - |   56 | `" __SessS::$name = (string)$name;"` |
|    - |   57 | `" return $old;"` |
|    - |   58 | `"}"` |
|    - |   59 | `"function session_save_path($path = null){"` |
|    - |   60 | `" if( __SessS::$path === '' ){"` |
|    - |   61 | `"  __SessS::$path = rtrim(sys_get_temp_dir(), '/');"` |
|    - |   62 | `" }"` |
|    - |   63 | `" $old = __SessS::$path;"` |
|    - |   64 | `" if( $path === null ){ return $old; }"` |
|    - |   65 | `" if( __SessS::$status === PHP_SESSION_ACTIVE ){"` |
|    - |   66 | `"  trigger_error('session_save_path(): Session save path cannot be changed when"` |
|    - |   67 | `" a session is active', E_USER_WARNING);"` |
|    - |   68 | `"  return false;"` |
|    - |   69 | `" }"` |
|    - |   70 | `" __SessS::$path = rtrim((string)$path, '/');"` |
|    - |   71 | `" return $old;"` |
|    - |   72 | `"}"` |
|    - |   73 | `"function __sess_file(){"` |
|    - |   74 | `" return session_save_path() . '/sess_' . __SessS::$id;"` |
|    - |   75 | `"}"` |
|    - |   76 | `"function __sess_genid(){"` |
|    - |   77 | `" $chars = '0123456789abcdefghijklmnopqrstuv';"` |
|    - |   78 | `" $raw = random_bytes(32);"` |
|    - |   79 | `" $out = '';"` |
|    - |   80 | `" for( $i = 0; $i < 32; $i++ ){ $out .= $chars[ord($raw[$i]) & 31]; }"` |
|    - |   81 | `" return $out;"` |
|    - |   82 | `"}"` |
|    - |   83 | `"function __sess_scan_fragment($s, $p){"` |
|    - |   84 | `" /* Return the position just past ONE serialized value starting at $p"` |
|    - |   85 | `"  * (enough of php's serialize grammar for session payloads). */"` |
|    - |   86 | `" $c = $s[$p] ?? '';"` |
|    - |   87 | `" if( $c === 'N' ){ return $p + 2; }"` |
|    - |   88 | `" if( $c === 'i' \|\| $c === 'd' \|\| $c === 'b' ){"` |
|    - |   89 | `"  $e = strpos($s, ';', $p);"` |
|    - |   90 | `"  return $e === false ? strlen($s) : $e + 1;"` |
|    - |   91 | `" }"` |
|    - |   92 | `" if( $c === 's' ){"` |
|    - |   93 | `"  $q = strpos($s, ':', $p + 2);"` |
|    - |   94 | `"  $len = (int)substr($s, $p + 2, $q - ($p + 2));"` |
|    - |   95 | `"  return $q + 1 + 1 + $len + 2;"` |
|    - |   96 | `" }"` |
|    - |   97 | `" if( $c === 'a' \|\| $c === 'O' ){"` |
|    - |   98 | `"  $open = strpos($s, '{', $p);"` |
|    - |   99 | `"  if( $open === false ){ return strlen($s); }"` |
|    - |  100 | `"  $depth = 1;"` |
|    - |  101 | `"  $i = $open + 1;"` |
|    - |  102 | `"  $n = strlen($s);"` |
|    - |  103 | `"  while( $i < $n && $depth > 0 ){"` |
|    - |  104 | `"   $ch = $s[$i];"` |
|    - |  105 | `"   if( $ch === 's' && substr($s, $i + 1, 1) === ':' ){"` |
|    - |  106 | `"    /* skip strings wholesale so braces inside them don't count */"` |
|    - |  107 | `"    $i = __sess_scan_fragment($s, $i);"` |
|    - |  108 | `"    continue;"` |
|    - |  109 | `"   }"` |
|    - |  110 | `"   if( $ch === '{' ){ $depth++; }"` |
|    - |  111 | `"   elseif( $ch === '}' ){ $depth--; }"` |
|    - |  112 | `"   $i++;"` |
|    - |  113 | `"  }"` |
|    - |  114 | `"  return $i;"` |
|    - |  115 | `" }"` |
|    - |  116 | `" $e = strpos($s, ';', $p);"` |
|    - |  117 | `" return $e === false ? strlen($s) : $e + 1;"` |
|    - |  118 | `"}"` |
|    - |  119 | `"function __sess_decode($s){"` |
|    - |  120 | `" $out = [];"` |
|    - |  121 | `" $p = 0;"` |
|    - |  122 | `" $n = strlen($s);"` |
|    - |  123 | `" while( $p < $n ){"` |
|    - |  124 | `"  $bar = strpos($s, '\|', $p);"` |
|    - |  125 | `"  if( $bar === false ){ break; }"` |
|    - |  126 | `"  $key = substr($s, $p, $bar - $p);"` |
|    - |  127 | `"  $end = __sess_scan_fragment($s, $bar + 1);"` |
|    - |  128 | `"  $frag = substr($s, $bar + 1, $end - ($bar + 1));"` |
|    - |  129 | `"  $val = unserialize($frag);"` |
|    - |  130 | `"  $out[$key] = $val;"` |
|    - |  131 | `"  $p = $end;"` |
|    - |  132 | `" }"` |
|    - |  133 | `" return $out;"` |
|    - |  134 | `"}"` |
|    - |  135 | `"function __sess_encode($data){"` |
|    - |  136 | `" $out = '';"` |
|    - |  137 | `" foreach( $data as $k => $v ){"` |
|    - |  138 | `"  $out .= $k . '\|' . serialize($v);"` |
|    - |  139 | `" }"` |
|    - |  140 | `" return $out;"` |
|    - |  141 | `"}"` |
|    - |  142 | `"function session_start($options = []){"` |
|    - |  143 | `" if( __SessS::$status === PHP_SESSION_ACTIVE ){"` |
|    - |  144 | `"  trigger_error('session_start(): Ignoring session_start() because a session"` |
|    - |  145 | `" is already active', E_USER_NOTICE);"` |
|    - |  146 | `"  return true;"` |
|    - |  147 | `" }"` |
|    - |  148 | `" if( headers_sent() ){"` |
|    - |  149 | `"  trigger_error('session_start(): Session cannot be started after headers have"` |
|    - |  150 | `" already been sent', E_USER_WARNING);"` |
|    - |  151 | `"  return false;"` |
|    - |  152 | `" }"` |
|    - |  153 | `" if( __SessS::$id === '' ){"` |
|    - |  154 | `"  $n = __SessS::$name;"` |
|    - |  155 | `"  if( isset($_COOKIE[$n]) && preg_match('/^[0-9a-zA-Z,-]{1,128}$/', $_COOKIE[$n]) ){"` |
|    - |  156 | `"   __SessS::$id = $_COOKIE[$n];"` |
|    - |  157 | `"  }else{"` |
|    - |  158 | `"   __SessS::$id = __sess_genid();"` |
|    - |  159 | `"  }"` |
|    - |  160 | `" }"` |
|    - |  161 | `" $file = __sess_file();"` |
|    - |  162 | `" if( file_exists($file) ){"` |
|    - |  163 | `"  $raw = file_get_contents($file);"` |
|    - |  164 | `"  $_SESSION = is_string($raw) ? __sess_decode($raw) : [];"` |
|    - |  165 | `" }else{"` |
|    - |  166 | `"  $_SESSION = [];"` |
|    - |  167 | `" }"` |
|    - |  168 | `" __SessS::$status = PHP_SESSION_ACTIVE;"` |
|    - |  169 | `" if( !__SessS::$wired ){"` |
|    - |  170 | `"  __SessS::$wired = true;"` |
|    - |  171 | `"  register_shutdown_function('session_write_close');"` |
|    - |  172 | `"  if( function_exists('setcookie') ){"` |
|    - |  173 | `"   setcookie(__SessS::$name, __SessS::$id);"` |
|    - |  174 | `"  }"` |
|    - |  175 | `" }"` |
|    - |  176 | `" return true;"` |
|    - |  177 | `"}"` |
|    - |  178 | `"function session_write_close(){"` |
|    - |  179 | `" if( __SessS::$status !== PHP_SESSION_ACTIVE ){ return false; }"` |
|    - |  180 | `" file_put_contents(__sess_file(), __sess_encode($_SESSION));"` |
|    - |  181 | `" __SessS::$status = PHP_SESSION_NONE;"` |
|    - |  182 | `" return true;"` |
|    - |  183 | `"}"` |
|    - |  184 | `"function session_commit(){ return session_write_close(); }"` |
|    - |  185 | `"function session_abort(){"` |
|    - |  186 | `" if( __SessS::$status !== PHP_SESSION_ACTIVE ){ return false; }"` |
|    - |  187 | `" __SessS::$status = PHP_SESSION_NONE;"` |
|    - |  188 | `" return true;"` |
|    - |  189 | `"}"` |
|    - |  190 | `"function session_reset(){"` |
|    - |  191 | `" if( __SessS::$status !== PHP_SESSION_ACTIVE ){ return false; }"` |
|    - |  192 | `" $file = __sess_file();"` |
|    - |  193 | `" $_SESSION = file_exists($file) ? __sess_decode(file_get_contents($file)) : [];"` |
|    - |  194 | `" return true;"` |
|    - |  195 | `"}"` |
|    - |  196 | `"function session_unset(){"` |
|    - |  197 | `" if( __SessS::$status !== PHP_SESSION_ACTIVE ){ return false; }"` |
|    - |  198 | `" $_SESSION = [];"` |
|    - |  199 | `" return true;"` |
|    - |  200 | `"}"` |
|    - |  201 | `"function session_destroy(){"` |
|    - |  202 | `" if( __SessS::$status !== PHP_SESSION_ACTIVE ){"` |
|    - |  203 | `"  trigger_error('session_destroy(): Trying to destroy uninitialized session',"` |
|    - |  204 | `"   E_USER_WARNING);"` |
|    - |  205 | `"  return false;"` |
|    - |  206 | `" }"` |
|    - |  207 | `" $file = __sess_file();"` |
|    - |  208 | `" if( file_exists($file) ){ unlink($file); }"` |
|    - |  209 | `" __SessS::$status = PHP_SESSION_NONE;"` |
|    - |  210 | `" __SessS::$id = '';"` |
|    - |  211 | `" return true;"` |
|    - |  212 | `"}"` |
|    - |  213 | `"function session_regenerate_id($deleteOldSession = false){"` |
|    - |  214 | `" if( __SessS::$status !== PHP_SESSION_ACTIVE ){"` |
|    - |  215 | `"  trigger_error('session_regenerate_id(): Session ID cannot be regenerated when"` |
|    - |  216 | `" there is no active session', E_USER_WARNING);"` |
|    - |  217 | `"  return false;"` |
|    - |  218 | `" }"` |
|    - |  219 | `" $oldFile = __sess_file();"` |
|    - |  220 | `" if( $deleteOldSession && file_exists($oldFile) ){ unlink($oldFile); }"` |
|    - |  221 | `" __SessS::$id = __sess_genid();"` |
|    - |  222 | `" return true;"` |
|    - |  223 | `"}"` |
|    - |  224 | `;` |
|    - |  225 |  |
| 3840 |  226 | `PH7_PRIVATE sxi32 PH7_VmInstallSession(ph7_vm *pVm)` |
|    5 |  227 | `{` |
| 3845 |  228 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zSessionLib,sizeof(zSessionLib)-1);` |
|    5 |  229 | `}` |
|    - |  230 |  |
|    - |  231 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    - |  232 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - |  233 |  |
|    - |  234 | `#if defined(PH7_DISABLE_BUILTIN_FUNC) \|\| defined(PH7_DISABLE_DISK_IO)` |
|    - |  235 | `/* Tiny build: no sessions (builtin funcs / disk IO disabled) */` |
|    - |  236 | `PH7_PRIVATE sxi32 PH7_VmInstallSession(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|    - |  237 | `#endif` |
|    - |  238 |  |
