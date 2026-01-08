--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert with null input
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = base_convert(null, 10, 10);
if ($result === "0") {
    echo "PASS";
} else {
    echo "FAIL";
}
?>
--EXPECT--
PASS