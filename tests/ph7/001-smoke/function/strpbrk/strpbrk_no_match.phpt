--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: strpbrk with no match
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test strpbrk with no matching characters
$result = strpbrk("hello", "xyz");
if ($result === false) {
    echo "PASS\n";
} else {
    echo "FAIL\n";
}
?>
--EXPECT--
PASS
--CLEAN--
<?php
unset($result);
