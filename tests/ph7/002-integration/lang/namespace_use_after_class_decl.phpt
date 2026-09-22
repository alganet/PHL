--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An import whose name a class declared earlier in the file already took is a compile fatal
--FILE--
<?php
namespace B;
class Cee {}
use A\CEE;
echo "unreachable\n";
?>
--EXPECTF--
%s %s %s  Cannot use A\CEE as CEE because the name is already in use %s
--CLEAN--
<?php
