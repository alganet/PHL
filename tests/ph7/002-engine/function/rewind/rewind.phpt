--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
rewind resets file pointer to beginning (ftell shows 0 afterwards)
--SKIPIF--
<?php if (!function_exists('rewind')) { echo 'skip'; } ?>
--FILE--
<?php
$fn = tempnam(sys_get_temp_dir(), 'ph7_rewind');
file_put_contents($fn, 'Hello World');
$fp = fopen($fn, 'r');
$buf = fread($fp, 5);
echo ftell($fp) . PHP_EOL; // expecting 5
rewind($fp);
echo ftell($fp) . PHP_EOL; // expecting 0
fclose($fp);
unlink($fn);
?>
--EXPECT--
5
0
--CLEAN--
<?php
unset($fn);
?>
