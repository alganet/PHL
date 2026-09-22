--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A class declaration whose short name is a local import is a compile fatal
--FILE--
<?php
namespace B;
use A\Cee;
class Cee {}
echo "unreachable\n";
?>
--EXPECTF--
%s %s %s  Cannot redeclare class B\Cee (previously declared as local import) %s
--CLEAN--
<?php
