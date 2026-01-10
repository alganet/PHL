--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test open corrupted zip (bad local and central headers, no recovery)
--SKIPIF--
<?php
/* Only run on PH7/PHL */
if (function_exists('zend_version')) { die("skip: not PH7\n"); }
if (!function_exists('zip_open')) {
    die("skip zip_open not available\n");
}
?>
--FILE--
<?php
$fn = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'phl_test_zip_corrupt_central.zip';
$zip_b64 = 'UEsDBBQAAAAAADtSjFuGphA2BQAAAAUAAAAFAAAAZmlsZTFoZWxsb1BLAQIUAxQAAAAAADtSjFuGphA2BQAAAAUAAAAFAAAAAAAAAAAAAACAAQAAAABmaWxlMVBLBQYAAAAAAQABADMAAAAoAAAAAAA=';
file_put_contents($fn, base64_decode($zip_b64));
$data = file_get_contents($fn);
/* Corrupt the local header signature (first 4 bytes) */
$data[0] = 'Z';
$data[1] = 'Z';
$data[2] = 'Z';
$data[3] = 'Z';
/* Corrupt the central directory signature (last 4 bytes of central) */
$len = strlen($data);
$data[$len - 4] = 'Z';
$data[$len - 3] = 'Z';
$data[$len - 2] = 'Z';
$data[$len - 1] = 'Z';
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
--EXPECT--
exists=1
len=113
zip_open=failed