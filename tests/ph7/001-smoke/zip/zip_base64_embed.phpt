--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test embedded base64 zip using zip_open/zip_entry functions
--SKIPIF--
<?php if (!function_exists('zip_open')) { echo 'skip zip_open not available'; } ?>
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
--EXPECTF--
%Aexists=1%Alen=113%AFunction zip_open() is deprecated since 8.0, use ZipArchive::open() instead%Azip_open=ok%AFunction zip_read() is deprecated since 8.0, use ZipArchive::statIndex() instead%AFunction zip_entry_name() is deprecated since 8.0, use ZipArchive::statIndex() instead%Azip_entry_name=file1%AFunction zip_entry_open() is deprecated since 8.0%AFunction zip_entry_filesize() is deprecated since 8.0, use ZipArchive::statIndex() instead%Azip_entry_filesize=5%AFunction zip_entry_read() is deprecated since 8.0, use ZipArchive::getFromIndex() instead%Azip_entry_read=hello%AFunction zip_entry_close() is deprecated since 8.0%AFunction zip_close() is deprecated since 8.0, use ZipArchive::close() instead%A
--CLEAN--
<?php
unset($fn, $zip_b64, $zip, $entry);
