--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: base64_decode with empty string returns FALSE
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test base64_decode with empty string - covers nLen < 1 branch
$result = base64_decode('');
if ($result === false) {
    echo "PASS: Empty string returns FALSE\n";
} else {
    echo "FAIL: Expected FALSE, got " . var_export($result, true) . "\n";
}
?>
--EXPECT--
PASS: Empty string returns FALSE
--CLEAN--
<?php
unset($result);
