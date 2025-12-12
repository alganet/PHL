--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
opendir/readdir/closedir basic iteration
--SKIPIF--
<?php
if (!function_exists('opendir') || !function_exists('readdir') || !function_exists('closedir')) {
    die('skip');
}
?>
--FILE--
<?php
$dir = sys_get_temp_dir() . '/ph7_test_opendir_' . getmypid();
@mkdir($dir);
file_put_contents($dir . '/a.txt', 'a');
file_put_contents($dir . '/b.txt', 'b');

$dh = opendir($dir);
$found = array();
while (($entry = readdir($dh)) !== false) {
    if ($entry === '.' || $entry === '..') continue;
    $found[] = $entry;
}
closedir($dh);
sort($found);
echo implode(",", $found) . "\n";

// Cleanup
@unlink($dir . '/a.txt');
@unlink($dir . '/b.txt');
@rmdir($dir);
?>
--CLEAN--
<?php
@unlink($dir . '/a.txt');
@unlink($dir . '/b.txt');
@rmdir($dir);
unset($dir, $dh, $found, $entry);
?>
--EXPECT--
a.txt,b.txt
