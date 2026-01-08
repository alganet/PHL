--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
round() returns 0 when called with no arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = round();
if ($result === 0) {
    echo "round with no args returns 0\n";
} else {
    echo "unexpected result\n";
}
?>
--EXPECT--
round with no args returns 0