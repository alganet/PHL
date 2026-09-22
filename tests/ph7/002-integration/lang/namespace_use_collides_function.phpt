--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A function declaration whose short name is a local function import is a compile fatal
--FILE--
<?php
namespace B;
use function A\eff;
function eff() {}
echo "unreachable\n";
?>
--EXPECTF--
%s %s %s  Cannot redeclare function B\eff() (previously declared as local import) %s
--CLEAN--
<?php
