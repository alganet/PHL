--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
zip_open should return false on invalid ZIP file
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip"; // Deprecated on Zend
}
?>
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_zip_');
file_put_contents($fname, 'NOTAZIP');
$res = zip_open($fname);
echo "zip_open_ok=" . ($res ? 'true' : 'false') . PHP_EOL;
if (is_resource($res)) { zip_close($res); }
@unlink($fname);
?>
--EXPECT--
zip_open_ok=false
--CLEAN--
<?php
@unlink($fname);
?>
