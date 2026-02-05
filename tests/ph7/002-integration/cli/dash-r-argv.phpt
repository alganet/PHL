--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
phl interpreter CLI inline code run (-r) with arguments
--SKIPIF--
<?php if (PHP_OS == 'WINNT') { echo "skip"; } ?>
<?php if (function_exists('zend_version')) { echo "skip"; } ?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$fp = popen("\"$phl\" -r \"echo \\\"\\\$argv[0]\\n\\\";\" foo", "r");
$out = '';
while (!feof($fp)) {
	$out .= fgets($fp);
}
fclose($fp);
echo $out;
?>
--EXPECT--
foo
--CLEAN--
<?php
unset($phl, $fp, $out, $out .);
