--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fgetss should strip HTML tags from file content when reading
--SKIPIF--
<?php
// PHL extension: `fgetss()` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: fgetss() is not a php symbol'; }
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
