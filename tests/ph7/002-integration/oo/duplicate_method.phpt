--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Two methods of the same name in one class is a fatal (was a bare skip titled "triggers overloading path" — the PH7 overloading extension, removed for functions in Jul but still live for methods)
--FILE--
<?php
class DuplicateMethodTest {
    function foo() { return 'first'; }
    function foo() { return 'second'; }
}
echo "unreachable\n";
?>
--EXPECTF--
%AFatal error:%ACannot redeclare DuplicateMethodTest::foo()%A
--CLEAN--
<?php
