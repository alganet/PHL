--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert_options query ASSERT_ACTIVE returns 1 by default
--SKIPIF--
<?php if (function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
error_reporting(E_ALL & ~E_DEPRECATED);
echo assert_options(ASSERT_ACTIVE) === 1 ? "OK" : "FAIL";
?>
--EXPECT--
OK
--CLEAN--
<?php

