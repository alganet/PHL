--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
phl interpreter CLI inline code run (-r): $argv[0] is "Standard input code"
--SKIPIF--
<?php if (PHP_OS == 'WINNT') { echo "skip"; } ?>
--FILE--
<?php
/* Matches PHP: under -r, $argv[0] is the literal "Standard input code" and the
 * script's own arguments follow (so "foo" is $argv[1]). */
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
Standard input code
--CLEAN--
<?php
unset($phl, $fp, $out);
