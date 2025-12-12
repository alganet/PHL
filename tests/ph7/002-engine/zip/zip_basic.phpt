--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
zip_open/zip_read/zip_entry_* basic flow using dynamically created archive
--SKIPIF--
<?php
/* Skip on Zend PHP (this test targets the PH7/PHL zip implementation) */
if (function_exists('zend_version')) { echo "skip: not PH7\n"; }
if (!function_exists('zip_open')) { echo 'skip: zip not available'; }
?>
--FILE--
<?php
$fn = tempnam(sys_get_temp_dir(), 'ph7_zip');
// Create the zip file from an embedded base64 representation so the test
// does not depend on ZipArchive or platform-specific tooling.
$zip_b64 = "UEsDBBQAAAAAADtSjFuGphA2BQAAAAUAAAAFAAAAZmlsZTFoZWxsb1BLAQIUAxQAAAAAADtSjFuGphA2BQ\
AAAAUAAAAFAAAAAAAAAAAAAACAAQAAAABmaWxlMVBLBQYAAAAAAQABADMAAAAoAAAAAAA=";
$bin = base64_decode($zip_b64);
if ($bin === false) {
    echo "skip: cannot decode zip archive\n";
    unlink($fn);
    return;
}
file_put_contents($fn, $bin);
$created = true;
$res = @zip_open($fn);
if (!is_resource($res)) { echo "status=failed\n"; unlink($fn); return; }
$entry = zip_read($res);
if (!is_resource($entry)) { echo "status=failed\n"; zip_close($res); unlink($fn); return; }
$name = zip_entry_name($entry);
$size = zip_entry_filesize($entry);
$content = zip_entry_read($entry, $size);
zip_entry_close($entry);
zip_close($res);
echo "status=ok\n";
echo $name . PHP_EOL;
echo $size . PHP_EOL;
echo $content . PHP_EOL;
unlink($fn);
?>
--EXPECTF--
status=%s

