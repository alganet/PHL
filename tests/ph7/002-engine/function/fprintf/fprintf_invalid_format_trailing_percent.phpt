--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
fprintf with invalid format string ending with '%'
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_fprintf_');
$fp = fopen($fname, 'w');
if ($fp) {
    $result = fprintf($fp, "test %");
    fclose($fp);
    $content = file_get_contents($fname);
    echo "Result: $result\n";
    echo "Content: " . trim($content) . "\n";
}
?>
--EXPECT--
Result: 5
Content: test
--CLEAN--
<?php
@unlink($fname);
?>