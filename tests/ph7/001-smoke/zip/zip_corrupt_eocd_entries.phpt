--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test open corrupted zip (end-of-central with too many entries)
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
$fn = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'phl_test_zip_corrupt_eocd_entries.zip';
$zip_b64 = 'UEsDBBQAAAAAADtSjFuGphA2BQAAAAUAAAAFAAAAZmlsZTFoZWxsb1BLAQIUAxQAAAAAADtSjFuGphA2BQAAAAUAAAAFAAAAAAAAAAAAAACAAQAAAABmaWxlMVBLBQYAAAAAAQABADMAAAAoAAAAAAA=';
file_put_contents($fn, base64_decode($zip_b64));
$data = file_get_contents($fn);
/* Corrupt the number of entries in end-of-central (offset 88-89, set to 0xFFFF > 32767) */
$data[88] = "\xFF";
$data[89] = "\xFF";
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
zip_open=ok
--CLEAN--
<?php
unset($fn, $zip_b64, $data, $zip);
