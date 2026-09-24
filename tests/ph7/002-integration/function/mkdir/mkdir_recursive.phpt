--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: mkdir()'s $recursive builds the whole tree
--FILE--
<?php
/* error_reporting-aware: the cleanup below is '@'-suppressed and a handler that
 * prints regardless would make the expectation depend on what is left over. */
set_error_handler(function ($n, $s) {
    if (error_reporting() & $n) { echo "  ERR[$n] $s\n"; }
    return true;
});
$mk_base = sys_get_temp_dir() . '/phl_mkr_' . getmypid();

/* Every missing ancestor is created, and the mode is applied to each of them. */
var_dump(mkdir($mk_base . '/x/y/z', 0777, true), is_dir($mk_base . '/x/y/z'));

/* An ancestor already there is skipped in silence; the LEAF is always
 * attempted, so an existing one is "File exists" with the flag as without it. */
var_dump(mkdir($mk_base . '/x/y/w', 0777, true));
var_dump(mkdir($mk_base . '/x/y/z', 0777, true));
var_dump(mkdir($mk_base . '/x/y/z'));

/* A trailing separator names the same directory, a doubled one is one level,
 * and ".." is resolved on the way through. */
var_dump(mkdir($mk_base . '/t1/t2/', 0777, true), is_dir($mk_base . '/t1/t2'));
var_dump(mkdir($mk_base . '/s1//s2', 0777, true), is_dir($mk_base . '/s1/s2'));
var_dump(mkdir($mk_base . '/x/../q/r', 0777, true), is_dir($mk_base . '/q/r'));

/* An ancestor that is a FILE stops the walk where the system does. The WORD for
 * it is the system's, and php itself reports a different one per platform
 * (ENOTDIR on POSIX, ERROR_PATH_NOT_FOUND on Windows), so only the verdict is
 * asserted here. */
file_put_contents($mk_base . '/afile', 'x');
var_dump(@mkdir($mk_base . '/afile/sub', 0777, true));

/* The two names that are not paths, and the one that already is the root. */
var_dump(mkdir('', 0777, true));
var_dump(mkdir(''));
var_dump(mkdir('.', 0777, true));

/* The mode reaches every level it creates (asserted where a mode means
 * something; the umask applies to both engines identically). */
if (DIRECTORY_SEPARATOR === '/') {
    mkdir($mk_base . '/m/inner', 0700, true);
    printf("m=%04o inner=%04o\n",
        fileperms($mk_base . '/m') & 0777, fileperms($mk_base . '/m/inner') & 0777);
} else {
    echo "m=0700 inner=0700\n";
}

foreach (['/m/inner', '/m', '/q/r', '/q', '/s1/s2', '/s1', '/t1/t2', '/t1',
          '/x/y/w', '/x/y/z', '/x/y', '/x'] as $mk_d) {
    @rmdir($mk_base . $mk_d);
}
@unlink($mk_base . '/afile');
@rmdir($mk_base);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
  ERR[2] mkdir(): File exists
bool(false)
  ERR[2] mkdir(): File exists
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
  ERR[2] mkdir(): Invalid path
bool(false)
  ERR[2] mkdir(): No such file or directory
bool(false)
  ERR[2] mkdir(): File exists
bool(false)
m=0700 inner=0700
--CLEAN--
<?php
unset($mk_base, $mk_d);
