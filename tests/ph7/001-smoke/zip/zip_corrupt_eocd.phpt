--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test open corrupted zip (bad end-of-central directory)
--SKIPIF--
<?php
/* Only run on PH7/PHL (behavior differs on Zend PHP) */
if (function_exists('zend_version')) {
    print("skip: not PH7\n");
}
if (!function_exists('zip_open')) {
    print("skip zip_open not available\n");
}
?>
--FILE--
<?php
$fn = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'phl_test_zip_corrupt.zip';
$zip_b64 = 'UEsDBBQAAAAAADtSjFuGphA2BQAAAAUAAAAFAAAAZmlsZTFoZWxsb1BLAQIUAxQAAAAAADtSjFuGphA2BQAAAAUAAAAFAAAAAAAAAAAAAACAAQAAAABmaWxlMVBLBQYAAAAAAQABADMAAAAoAAAAAAA=';
file_put_contents($fn, base64_decode($zip_b64));
$data = file_get_contents($fn);
/* Corrupt the end of central directory signature (last bytes) */
$data[strlen($data)-4] = 'Z';
$data[strlen($data)-3] = 'Z';
file_put_contents($fn, $data);

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
len=113
Error [8192]: Function zip_open() is deprecated since 8.0, use ZipArchive::open() instead in %s on line %d
zip_open=ok
Error [8192]: Function zip_close() is deprecated since 8.0, use ZipArchive::close() instead in %s on line %d
--CLEAN--
<?php
unset($fn, $zip_b64, $data, $zip);
