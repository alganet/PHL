--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A NUL byte in a path or command argument is a ValueError, never a truncation
--FILE--
<?php
/* php parses these parameters with Z_PARAM_PATH, which refuses a NUL byte
 * outright. Truncating at it instead makes the builtin operate on a DIFFERENT
 * path than the one written -- the poison-NUL-byte shape. Every row below used
 * to truncate silently. */
$dir = sys_get_temp_dir();
$p   = $dir . DIRECTORY_SEPARATOR . "phl_nul_aa\0bb";

$cases = [
	'fopen'             => fn() => fopen($p, 'r'),
	'file_get_contents' => fn() => file_get_contents($p),
	'file_put_contents' => fn() => file_put_contents($p, 'x'),
	'file'              => fn() => file($p),
	'readfile'          => fn() => readfile($p),
	'parse_ini_file'    => fn() => parse_ini_file($p),
	'md5_file'          => fn() => md5_file($p),
	'sha1_file'         => fn() => sha1_file($p),
	'unlink'            => fn() => unlink($p),
	'touch'             => fn() => touch($p),
	'chmod'             => fn() => chmod($p, 0644),
	'rename #1'         => fn() => rename($p, $dir . DIRECTORY_SEPARATOR . 'phl_nul_z'),
	'rename #2'         => fn() => rename($dir . DIRECTORY_SEPARATOR . 'phl_nul_z', $p),
	'copy #1'           => fn() => copy($p, $dir . DIRECTORY_SEPARATOR . 'phl_nul_z'),
	'copy #2'           => fn() => copy($dir . DIRECTORY_SEPARATOR . 'phl_nul_z', $p),
	'readlink'          => fn() => readlink($p),
	'realpath'          => fn() => realpath($p),
	'mkdir'             => fn() => mkdir($p),
	'rmdir'             => fn() => rmdir($p),
	'opendir'           => fn() => opendir($p),
	'dir'               => fn() => dir($p),
	'scandir'           => fn() => scandir($p),
	'chdir'             => fn() => chdir($p),
	'glob'              => fn() => glob($p),
	'tempnam #1'        => fn() => tempnam($p, 'pfx'),
	'tempnam #2'        => fn() => tempnam($dir, "pf\0x"),
	'disk_free_space'   => fn() => disk_free_space($p),
	'disk_total_space'  => fn() => disk_total_space($p),
	'diskfreespace'     => fn() => diskfreespace($p),
	'fnmatch #1'        => fn() => fnmatch($p, 'x'),
	'fnmatch #2'        => fn() => fnmatch('*', "aa\0bb"),
	'set_include_path'  => fn() => set_include_path($p),
	'session_save_path' => fn() => session_save_path($p),
	'error_log #3'      => fn() => error_log('m', 3, $p),
	'shell_exec'        => fn() => shell_exec("echo\0hi"),
	'popen'             => fn() => popen("echo\0hi", 'r'),
];
foreach ($cases as $name => $fn) {
	echo str_pad($name, 20);
	try {
		$r = $fn();
		echo 'NO THROW: ', var_export($r, true);
	} catch (ValueError $e) {
		echo $e->getMessage();
	}
	echo "\n";
}

/* Nothing was created, deleted or written on the way. */
echo "aa exists: ", var_export(file_exists($dir . DIRECTORY_SEPARATOR . 'phl_nul_aa'), true), "\n";

/* The screen sits at the native-call boundary, so it does not care HOW the
 * builtin was reached: a named argument (bound to its position first), the two
 * call_user_func forwards, a callback and a first-class callable all refuse. */
$routes = [
	'named'      => fn() => fopen(filename: $p, mode: 'r'),
	'cuf'        => fn() => call_user_func('unlink', $p),
	'cufa'       => fn() => call_user_func_array('touch', [$p]),
	'array_map'  => fn() => array_map('realpath', [$p]),
	'fcc'        => function () use ($p) { $f = realpath(...); return $f($p); },
];
foreach ($routes as $name => $fn) {
	echo str_pad($name, 20);
	try {
		$r = $fn();
		echo 'NO THROW: ', var_export($r, true);
	} catch (ValueError $e) {
		echo $e->getMessage();
	}
	echo "\n";
}

/* A Stringable is coerced FIRST and its result asked, which is php's ZPP order.
 * The accessor runs exactly once (one [TS] per call) and the caller's object is
 * not retyped on the way. */
class NulPathStringable {
	public function __construct(private string $s) {}
	public function __toString(): string { echo '[TS]'; return $this->s; }
}
$obj = new NulPathStringable($p);
try {
	unlink($obj);
} catch (ValueError $e) {
	echo $e->getMessage(), "\n";
}
echo 'still an object: ', get_debug_type($obj), "\n";

/* The rule is the NUL, not the argument: the same calls with a clean path
 * still work, and the PATH-STRING functions keep php's answer -- they never
 * touch the filesystem, so php lets the NUL through untouched. */
$ok = $dir . DIRECTORY_SEPARATOR . 'phl_nul_ok';
var_dump(file_put_contents($ok, 'data'));
var_dump(file_get_contents($ok));
var_dump(unlink($ok));
var_dump(basename("aa\0bb") === "aa\0bb");
var_dump(dirname("/x/aa\0bb"));
var_dump(pathinfo("/x/aa\0bb")['basename'] === "aa\0bb");
?>
--EXPECT--
fopen               fopen(): Argument #1 ($filename) must not contain any null bytes
file_get_contents   file_get_contents(): Argument #1 ($filename) must not contain any null bytes
file_put_contents   file_put_contents(): Argument #1 ($filename) must not contain any null bytes
file                file(): Argument #1 ($filename) must not contain any null bytes
readfile            readfile(): Argument #1 ($filename) must not contain any null bytes
parse_ini_file      parse_ini_file(): Argument #1 ($filename) must not contain any null bytes
md5_file            md5_file(): Argument #1 ($filename) must not contain any null bytes
sha1_file           sha1_file(): Argument #1 ($filename) must not contain any null bytes
unlink              unlink(): Argument #1 ($filename) must not contain any null bytes
touch               touch(): Argument #1 ($filename) must not contain any null bytes
chmod               chmod(): Argument #1 ($filename) must not contain any null bytes
rename #1           rename(): Argument #1 ($from) must not contain any null bytes
rename #2           rename(): Argument #2 ($to) must not contain any null bytes
copy #1             copy(): Argument #1 ($from) must not contain any null bytes
copy #2             copy(): Argument #2 ($to) must not contain any null bytes
readlink            readlink(): Argument #1 ($path) must not contain any null bytes
realpath            realpath(): Argument #1 ($path) must not contain any null bytes
mkdir               mkdir(): Argument #1 ($directory) must not contain any null bytes
rmdir               rmdir(): Argument #1 ($directory) must not contain any null bytes
opendir             opendir(): Argument #1 ($directory) must not contain any null bytes
dir                 dir(): Argument #1 ($directory) must not contain any null bytes
scandir             scandir(): Argument #1 ($directory) must not contain any null bytes
chdir               chdir(): Argument #1 ($directory) must not contain any null bytes
glob                glob(): Argument #1 ($pattern) must not contain any null bytes
tempnam #1          tempnam(): Argument #1 ($directory) must not contain any null bytes
tempnam #2          tempnam(): Argument #2 ($prefix) must not contain any null bytes
disk_free_space     disk_free_space(): Argument #1 ($directory) must not contain any null bytes
disk_total_space    disk_total_space(): Argument #1 ($directory) must not contain any null bytes
diskfreespace       diskfreespace(): Argument #1 ($directory) must not contain any null bytes
fnmatch #1          fnmatch(): Argument #1 ($pattern) must not contain any null bytes
fnmatch #2          fnmatch(): Argument #2 ($filename) must not contain any null bytes
set_include_path    set_include_path(): Argument #1 ($include_path) must not contain any null bytes
session_save_path   session_save_path(): Argument #1 ($path) must not contain any null bytes
error_log #3        error_log(): Argument #3 ($destination) must not contain any null bytes
shell_exec          shell_exec(): Argument #1 ($command) must not contain any null bytes
popen               popen(): Argument #1 ($command) must not contain any null bytes
aa exists: false
named               fopen(): Argument #1 ($filename) must not contain any null bytes
cuf                 unlink(): Argument #1 ($filename) must not contain any null bytes
cufa                touch(): Argument #1 ($filename) must not contain any null bytes
array_map           realpath(): Argument #1 ($path) must not contain any null bytes
fcc                 realpath(): Argument #1 ($path) must not contain any null bytes
[TS]unlink(): Argument #1 ($filename) must not contain any null bytes
still an object: NulPathStringable
int(4)
string(4) "data"
bool(true)
bool(true)
string(2) "/x"
bool(true)
