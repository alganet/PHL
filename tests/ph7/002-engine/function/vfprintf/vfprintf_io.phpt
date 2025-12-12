--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
vfprintf invalid resource returns 0 and valid stream writes formatted text
--SKIPIF--
<?php
if (!function_exists('vfprintf')) { echo 'skip: vfprintf not available'; }
?>
--FILE--
<?php
error_reporting(0);
// invalid resource: some runtimes throw TypeError, others return 0
$bad = 12345;
try {
    $ok = @vfprintf($bad, '%d %s', array(42, 'ok'));
    echo 'first=' . (int)$ok . PHP_EOL;
} catch (TypeError $e) {
    echo 'first=typeerror' . PHP_EOL;
}
// valid stream (temp file)
$fn = tempnam(sys_get_temp_dir(), 'ph7_vfprintf');
$fp = fopen($fn, 'w+');
$n = vfprintf($fp, '%d %s', array(42, 'ok'));
rewind($fp);
echo (int)$n . PHP_EOL;
echo rtrim(fread($fp, 64)) . PHP_EOL;
fclose($fp);
unlink($fn);
?>
--EXPECTF--
first=%s
%d
42 ok
--CLEAN--
<?php
unset($fn);
?>
