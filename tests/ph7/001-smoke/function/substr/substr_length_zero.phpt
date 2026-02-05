--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr returns empty string when length is zero
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = substr("abc", 1, 0);
if ($result == "") {
    echo "PASS: length 0 returns empty string\n";
} else {
    echo "FAIL: expected empty string, got: ";
    var_dump($result);
}
?>
--EXPECT--
PASS: length 0 returns empty string
--CLEAN--
<?php
unset($result);
