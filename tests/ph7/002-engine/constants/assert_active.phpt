--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: ASSERT_ACTIVE constant
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
--FILE--
<?php
echo "ASSERT_ACTIVE=" . ASSERT_ACTIVE . "\n";
?>
--EXPECTF--
ASSERT_ACTIVE=%d
