--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: htmlspecialchars empty string with ENT_QUOTES
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test htmlspecialchars with empty string and ENT_QUOTES flag
// This tests lines 1909-1910 in builtin.c
$result = htmlspecialchars("", ENT_QUOTES);
if ($result === "") {
    echo "PASS: empty string returned\n";
} else {
    echo "FAIL: got '" . $result . "'\n";
}
?>
--EXPECT--
PASS: empty string returned
--CLEAN--
<?php
unset($result);
