--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
fprintf should handle hexadecimal formatting with alternate form
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_fprintf_hex_');
$fp = fopen($fname, 'w');
if ($fp) {
    fprintf($fp, "Lower: %#x Upper: %#X", 255, 255);
    fclose($fp);
    echo trim(file_get_contents($fname)) . PHP_EOL;
}
?>
--EXPECT--
Lower: 0xff Upper: 0XFF
--CLEAN--
<?php
@unlink($fname);
unset($fname, $fp);
