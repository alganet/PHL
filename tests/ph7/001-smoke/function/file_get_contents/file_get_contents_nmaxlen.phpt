--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
file_get_contents respects max length parameter
--SKIPIF--
<?php
if (!function_exists('file_get_contents')) { echo 'skip: file_get_contents not available'; }
?>
--FILE--
<?php
$fn = tempnam(sys_get_temp_dir(), 'ph7_fgc');
file_put_contents($fn, 'Hello World');
$partial = file_get_contents($fn, false, null, 0, 5);
echo $partial . PHP_EOL;
$full = file_get_contents($fn, false, null, 0, 20);
echo strlen($full) . PHP_EOL;
?>
--EXPECT--
Hello
11
--CLEAN--
<?php
unlink($fn);
unset($fn, $partial, $full);
