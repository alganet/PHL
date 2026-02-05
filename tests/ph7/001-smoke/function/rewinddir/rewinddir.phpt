--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
rewinddir resets directory stream pointer (read same entry twice)
--SKIPIF--
<?php if (!function_exists('rewinddir')) { echo 'skip'; } ?>
--FILE--
<?php
$dir = sys_get_temp_dir() . '/ph7_rewinddir_'.uniqid();
mkdir($dir);
file_put_contents($dir . '/file1', 'x');
$dh = opendir($dir);
$first = null;
while (($e = readdir($dh)) !== false) {
    if ($e === '.' || $e === '..') continue;
    $first = $e;
    break;
}
rewinddir($dh);
$first2 = null;
while (($e = readdir($dh)) !== false) {
    if ($e === '.' || $e === '..') continue;
    $first2 = $e;
    break;
}
closedir($dh);
echo $first . PHP_EOL;
echo $first2 . PHP_EOL;
// cleanup
unlink($dir . '/file1');
rmdir($dir);
?>
--EXPECT--
file1
file1
--CLEAN--
<?php
unset($dir, $dh, $first, $first2);
