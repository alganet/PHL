--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
parse_ini_file parses a small ini file and returns values
--SKIPIF--
<?php
if (!function_exists('parse_ini_file')) { echo 'skip: parse_ini_file not available'; }
?>
--FILE--
<?php
$fn = tempnam(sys_get_temp_dir(), 'ph7_ini');
file_put_contents($fn, "a=1\nb=foo\n");
$parsed = parse_ini_file($fn, true);
if (is_array($parsed) && isset($parsed['a'])) {
    echo $parsed['a'] . PHP_EOL;
    echo $parsed['b'] . PHP_EOL;
} else {
    echo "parse_failed" . PHP_EOL;
}
unlink($fn);
?>
--EXPECT--
1
foo
--CLEAN--
<?php
unset($fn);
?>
