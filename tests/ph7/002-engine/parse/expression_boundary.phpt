--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: Expression parsing boundary with complex nesting
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test expression with complex nesting to trigger boundary parsing
$result = (1 + (2 * (3 / (4 - 5))));
echo "result=" . $result . "\n";
?>
--EXPECT--
result=-5