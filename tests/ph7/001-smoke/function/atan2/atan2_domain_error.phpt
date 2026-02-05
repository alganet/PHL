--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: atan2 with domain error (0,0 arguments)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test atan2 with (0,0) arguments which produces NaN
$result = atan2(0.0, 0.0);
if ($result == 0.0) {
    echo "PASS\n";
} else {
    echo "FAIL\n";
}
?>
--EXPECT--
PASS
--CLEAN--
<?php
unset($result);
