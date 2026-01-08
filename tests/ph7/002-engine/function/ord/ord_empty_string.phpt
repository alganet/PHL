--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: ord with empty string returns -1
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = ord('');
if ($result === -1) {
    echo "PASS";
} else {
    echo "FAIL";
}
?>
--EXPECT--
PASS