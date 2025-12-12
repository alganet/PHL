--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test embedded base64 zip using zip_open/zip_entry functions
--SKIPIF--
<?php
/* Skip on Zend PHP (this test targets the PH7/PHL zip implementation) */
if (function_exists('zend_version')) { echo "skip: not PH7\n"; }
if (!function_exists('zip_open')) {
    die("skip zip_open not available\n");
}
?>
--FILE--
<?php
$fn = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'phl_test_zip_base64.zip';
$zip_b64 = 'UEsDBBQAAAAAADtSjFuGphA2BQAAAAUAAAAFAAAAZmlsZTFoZWxsb1BLAQIUAxQAAAAAADtSjFuGphA2BQAAAAUAAAAFAAAAAAAAAAAAAACAAQAAAABmaWxlMVBLBQYAAAAAAQABADMAAAAoAAAAAAA=';
file_put_contents($fn, base64_decode($zip_b64));

echo 'exists='.(file_exists($fn)?'1':'0')."\n";
echo 'len='.filesize($fn)."\n";

$zip = zip_open($fn);
if (!$zip) {
    echo "zip_open=failed\n";
    unlink($fn);
    exit(0);
}
echo "zip_open=ok\n";

$entry = zip_read($zip);
if (!$entry) {
    echo "zip_read=failed\n";
    zip_close($zip);
    unlink($fn);
    exit(0);
}

echo 'zip_entry_name='.zip_entry_name($entry)."\n";
zip_entry_open($zip, $entry);
echo 'zip_entry_filesize='.zip_entry_filesize($entry)."\n";
echo "zip_entry_read=".zip_entry_read($entry, zip_entry_filesize($entry))."\n";
zip_entry_close($entry);
zip_close($zip);
unlink($fn);
?>
--EXPECT--
exists=1
len=113
zip_open=ok
zip_entry_name=file1
zip_entry_filesize=5
zip_entry_read=hello
