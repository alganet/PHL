--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
htmlspecialchars_decode with non-string argument returns null
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = htmlspecialchars_decode(123);
if ($result === null) {
    echo "PASS";
} else {
    echo "FAIL";
}
?>
--EXPECT--
PASS