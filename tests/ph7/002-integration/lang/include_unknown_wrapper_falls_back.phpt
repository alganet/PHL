--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An unregistered scheme falls back to the plain-files wrapper and the file RUNS
--DESCRIPTION--
The warning is not a refusal. php drops the protocol it could not resolve and
hands the whole uri to the plain-files wrapper, so a file that really sits at
that name is included -- and because the resolve SUCCEEDED, everything after it
works on an absolute plain path and the script reads exactly one sentence about
the wrapper whichever construct asked. This engine reported "Invalid argument"
and included nothing.
--SKIPIF--
<?php
if (DIRECTORY_SEPARATOR === '\\') {
    die("skip a directory named 'zzz:' cannot exist on Windows");
}
?>
--FILE--
<?php
set_error_handler(function ($no, $msg) { echo "W: $msg\n"; return true; });

/* The plain-files wrapper the fallback lands on resolves a relative name
 * against the CWD, so the fixture has to sit there. */
$cwd = getcwd();
$dir = sys_get_temp_dir() . '/phl_unknown_wrapper_' . getmypid();
@mkdir($dir . '/zzz:', 0777, true);
file_put_contents($dir . '/zzz:/hit.php', "<?php echo \"fell back\\n\";\n");
chdir($dir);
set_include_path('.');

var_dump(include 'zzz://hit.php');
var_dump(include_once 'zzz://hit.php');
var_dump(require 'zzz://hit.php');

chdir($cwd);
unlink($dir . '/zzz:/hit.php');
rmdir($dir . '/zzz:');
rmdir($dir);
echo "done\n";
?>
--EXPECT--
W: include(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
fell back
int(1)
W: include_once(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
bool(true)
W: require(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
fell back
int(1)
done
