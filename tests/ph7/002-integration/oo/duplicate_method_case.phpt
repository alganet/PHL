--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Method names are case-insensitive, so foo() and FOO() collide; php names the second spelling
--FILE--
<?php
class DupCase {
    function foo() {}
    function FOO() {}
}
echo "unreachable\n";
?>
--EXPECTF--
%AFatal error:%ACannot redeclare DupCase::FOO()%A
--CLEAN--
<?php
