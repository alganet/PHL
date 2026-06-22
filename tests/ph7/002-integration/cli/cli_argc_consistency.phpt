--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
$argc stays equal to count($argv) even when an argument is the empty string
--SKIPIF--
<?php if (PHP_OS == 'WINNT') { echo "skip"; } ?>
--FILE--
<?php
/* Regression: registering an argv entry can skip an empty string, so $argc must
 * be counted from what was actually registered — not bumped unconditionally —
 * or the $argc == count($argv) invariant (which `for ($i=0;$i<$argc;$i++)`
 * relies on) breaks. Holds under both phl and PHP regardless of how each treats
 * the empty argument. */
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$script = sys_get_temp_dir() . '/phl_argc_' . getmypid() . '.php';
file_put_contents($script,
    "<?php echo (\$argc === count(\$argv)) ? \"consistent\\n\" : \"BROKEN argc=\$argc count=\" . count(\$argv) . \"\\n\";\n");
$fp = popen("\"$phl\" \"$script\" \"\" beta", 'r');
$out = '';
while (!feof($fp)) { $out .= fgets($fp); }
fclose($fp);
echo $out;
?>
--EXPECT--
consistent
--CLEAN--
<?php
@unlink(sys_get_temp_dir() . '/phl_argc_' . getmypid() . '.php');
