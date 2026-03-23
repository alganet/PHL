--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_defined_vars returns variables in current scope
--FILE--
<?php
function test_get_defined_vars() {
    $foo = 'bar';
    $vars = get_defined_vars();
    if (isset($vars['foo']) && $vars['foo'] === 'bar') echo "OK\n"; else echo "FAIL\n";
}
test_get_defined_vars();
?>
--EXPECT--
OK
--CLEAN--
<?php
