--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: substr_compare with invalid adjusted length returns 1
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Negative length that becomes invalid after adjustment
$result = substr_compare('a', 'b', 0, -1);
if ($result === 1) {
    echo "PASS";
} else {
    echo "FAIL";
}
?>
--EXPECT--
PASS