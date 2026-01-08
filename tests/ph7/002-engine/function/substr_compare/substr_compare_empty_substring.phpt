--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: substr_compare with empty substring returns FALSE
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = substr_compare('abcdef', '', 0);
if ($result === false) {
    echo "PASS";
} else {
    echo "FAIL";
}
?>
--EXPECT--
PASS