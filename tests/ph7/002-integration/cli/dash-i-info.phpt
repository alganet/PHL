--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
phl interpreter CLI -i prints interpreter information (PHL plain-text subset)
--SKIPIF--
<?php
// Engine identity: this can never be cross-engine.
if (function_exists('zend_version')) { echo 'skip PHL-only by construction: asserts the -i output, a plain-text subset php does not emit'; }
// Windows emits a different -i body (paths, line endings and the build banner all
// differ), so the plain-text subset asserted below is POSIX-only.
elseif (PHP_OS == 'WINNT') { echo 'skip -i output differs on Windows (paths and build banner)'; }
?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$fp = popen("\"$phl\" -i", 'r');
$out = '';
while (!feof($fp)) { $out .= fgets($fp); }
fclose($fp);
echo $out;
?>
--EXPECTF--
phpinfo()
PHP Version => %d.%d.%d

System => %s
Build Date => %A
PHL Version => %d.%d.%d
PHP SAPI => cli
--CLEAN--
<?php
unset($phl, $fp, $out);
