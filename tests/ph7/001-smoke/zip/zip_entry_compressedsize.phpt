--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
zip_entry_compressedsize returns compressed size of entry
--SKIPIF--
<?php if (!function_exists('zip_open')) { echo 'skip zip_open not available'; } ?>
--FILE--
<?php
$fn = tempnam(sys_get_temp_dir(), 'ph7_zip');
$zip_b64 = "UEsDBBQAAAAAADtSjFuGphA2BQAAAAUAAAAFAAAAZmlsZTFoZWxsb1BLAQIUAxQAAAAAADtSjFuGphA2BQAAAAUAAAAFAAAAAAAAAAAAAACAAQAAAABmaWxlMVBLBQYAAAAAAQABADMAAAAoAAAAAAA=";
$bin = base64_decode($zip_b64);
file_put_contents($fn, $bin);
$res = zip_open($fn);
if (!is_resource($res)) { echo "failed\n"; unlink($fn); return; }
$entry = zip_read($res);
if (!is_resource($entry)) { echo "failed\n"; zip_close($res); unlink($fn); return; }
$compressed = zip_entry_compressedsize($entry);
if ($compressed >= 0) {
    echo "ok\n";
} else {
    echo "failed\n";
}
zip_entry_close($entry);
zip_close($res);
unlink($fn);
?>
--EXPECTF--
%AFunction zip_open() is deprecated since 8.0, use ZipArchive::open() instead%AFunction zip_read() is deprecated since 8.0, use ZipArchive::statIndex() instead%AFunction zip_entry_compressedsize() is deprecated since 8.0, use ZipArchive::statIndex() instead%Aok%AFunction zip_entry_close() is deprecated since 8.0%AFunction zip_close() is deprecated since 8.0, use ZipArchive::close() instead%A
--CLEAN--
<?php
unset($fn, $zip_b64, $bin, $res, $entry, $compressed);
