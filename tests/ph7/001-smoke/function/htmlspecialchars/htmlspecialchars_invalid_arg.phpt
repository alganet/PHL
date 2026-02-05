--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
htmlspecialchars with invalid argument
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = htmlspecialchars(123);
if ($result === null) {
    echo "PASS";
} else {
    echo "FAIL: expected null, got " . var_export($result, true);
}
?>
--EXPECT--
PASS
--CLEAN--
<?php
unset($result);
