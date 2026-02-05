--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr with insufficient arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = substr();
if ($result === false) {
    echo "PASS";
} else {
    echo "FAIL: expected false, got " . var_export($result, true);
}
?>
--EXPECT--
PASS
--CLEAN--
<?php
unset($result);
