--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: ASSERT_QUIET_EVAL constant
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
echo "ASSERT_QUIET_EVAL=" . ASSERT_QUIET_EVAL . "\n";
?>
--EXPECTF--
ASSERT_QUIET_EVAL=%d
--CLEAN--
<?php

