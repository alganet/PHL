--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: fmod with insufficient arguments returns 0
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test fmod with insufficient arguments
$result = fmod(5.0);
if ($result === 0.0) {
    echo "PASS";
} else {
    echo "FAIL";
}
?>
--EXPECT--
PASS