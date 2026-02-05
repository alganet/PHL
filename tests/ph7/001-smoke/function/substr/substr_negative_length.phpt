--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr with negative length
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = substr("hello", 1, -1);
if ($result === "ell") {
    echo "PASS";
} else {
    echo "FAIL";
}
?>
--EXPECT--
PASS
--CLEAN--
<?php
unset($result);
