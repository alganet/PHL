--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: an empty const initializer is rejected, not silently defined as null (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
const A = ;
echo "unreachable\n";
?>
--EXPECTF--
%AEmpty constant 'A' value in %s on line 2%A
--CLEAN--
<?php

