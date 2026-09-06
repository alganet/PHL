--CREDITS--
SPDX-FileCopyrightText: 2025 Test PH7 Engine
SPDX-License-Identifier: BSD-3-Clause
--TEST--
zip_entry_reset_read_cursor function coverage
--SKIPIF--
<?php
/* Skip on Zend PHP (this test targets the PH7/PHL implementation) */
if (function_exists('zend_version')) { echo "skip: not PH7\n"; }
if (!function_exists('zip_open')) { echo 'skip: zip not available'; }
?>
--FILE--
<?php
$fn = tempnam(sys_get_temp_dir(), 'ph7_zip');
$zip_b64 = "UEsDBBQAAAAAADtSjFuGphA2BQAAAAUAAAAFAAAAZmlsZTFoZWxsb1BLAQIUAxQAAAAAADtSjFuGphA2BQAAAAUAAAAFAAAAAAAAAAAAAACAAQAAAABmaWxlMVBLBQYAAAAAAQABADMAAAAoAAAAAAA=";
$bin = base64_decode($zip_b64);
if ($bin === false) {
    echo "skip: cannot decode zip archive\n";
    unlink($fn);
    return;
}
file_put_contents($fn, $bin);
$res = @zip_open($fn);
if (!is_resource($res)) { 
    echo "open failed\n";
    unlink($fn);
    return;
}
$entry = zip_read($res);
if (!is_resource($entry)) {
    echo "read failed\n";
    zip_close($res);
    unlink($fn);
    return;
}
// Test with valid resource
$result1 = zip_entry_reset_read_cursor($entry);
var_dump($result1);
// Test with invalid argument
$result2 = zip_entry_reset_read_cursor("invalid");
var_dump($result2);
// Test with no argument
$result3 = zip_entry_reset_read_cursor();
var_dump($result3);
zip_close($res);
unlink($fn);
?>
--EXPECTF--
Deprecated: Function zip_open() is deprecated since 8.0, use ZipArchive::open() instead in %s on line %d
Deprecated: Function zip_read() is deprecated since 8.0, use ZipArchive::statIndex() instead in %s on line %d
bool(true)
Error: zip_entry_reset_read_cursor(): Expecting a ZIP archive entry in %s on line %d
bool(false)
Error: zip_entry_reset_read_cursor(): Expecting a ZIP archive entry in %s on line %d
bool(false)
Deprecated: Function zip_close() is deprecated since 8.0, use ZipArchive::close() instead in %s on line %d
--CLEAN--
<?php
unset($fn, $zip_b64, $bin, $res, $entry, $result1, $result2, $result3);
