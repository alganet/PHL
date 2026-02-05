--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
fprintf should handle octal formatting with alternate form
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_fprintf_octal_');
$fp = fopen($fname, 'w');
if ($fp) {
    fprintf($fp, "Octal: %#o", 63);
    fclose($fp);
    echo trim(file_get_contents($fname)) . PHP_EOL;
}
?>
--EXPECT--
Octal: 077
--CLEAN--
<?php
@unlink($fname);
unset($fname, $fp);
