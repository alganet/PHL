--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test open truncated zip (too short)
--SKIPIF--
<?php
if (!function_exists('zip_open')) {
    print("skip zip_open not available\n");
}
if (function_exists('zend_version')) echo "skip";
?>
--FILE--
<?php
$fn = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'phl_test_zip_trunc.zip';
$zip_b64 = 'UEsDBBQAAAAAADtSjFuGphA2BQAAAAUAAAAFAAAAZmlsZTFoZWxsb1BLAQIUAxQAAAAAADtSjFuGphA2BQAAAAUAAAAFAAAAAAAAAAAAAACAAQAAAABmaWxlMVBLBQYAAAAAAQABADMAAAAoAAAAAAA=';
file_put_contents($fn, base64_decode($zip_b64));
$fp = fopen($fn,'r+');
ftruncate($fp, 10);
fclose($fp);

echo 'exists='.(file_exists($fn)?'1':'0')."\n";
echo 'len='.filesize($fn)."\n";

$zip = @zip_open($fn);
echo 'zip_open='.(is_resource($zip)?'ok':'failed')."\n";
if (is_resource($zip)){
    zip_close($zip);
}
unlink($fn);
?>
--EXPECTF--
exists=1
len=10
Error [8192]: Function zip_open() is deprecated since 8.0, use ZipArchive::open() instead in %s on line %d
zip_open=failed
--CLEAN--
<?php
unset($fn, $zip_b64, $fp, $zip);
