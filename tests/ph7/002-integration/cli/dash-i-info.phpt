--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
phl interpreter CLI -i prints interpreter information (PHL plain-text subset)
--SKIPIF--
<?php if (PHP_OS == 'WINNT') { echo "skip"; } ?>
<?php if (function_exists('zend_version')) { echo "skip"; } ?>
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
