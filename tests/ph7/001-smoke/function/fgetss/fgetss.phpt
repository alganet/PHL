--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fgetss should strip HTML tags from file content when reading
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_getss_');
file_put_contents($fname, '<b>Bold</b>Text');
$fp = fopen($fname, 'r');
if ($fp) {
    $s = fgetss($fp);
    echo trim($s) . PHP_EOL;
    fclose($fp);
}
?>
--EXPECT--
BoldText
--CLEAN--
<?php
@unlink($fname);
unset($fname, $fp, $s);
