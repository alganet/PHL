--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: __FUNCTION__ magic constant
--FILE--
<?php
echo "__FUNCTION__ in global scope: '" . __FUNCTION__ . "'\n";
function test_func() {
    echo "__FUNCTION__ in function: '" . __FUNCTION__ . "'\n";
}
test_func();
?>
--EXPECT--
__FUNCTION__ in global scope: ''
__FUNCTION__ in function: 'test_func'