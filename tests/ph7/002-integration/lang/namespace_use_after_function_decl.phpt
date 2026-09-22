--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A function import whose name a function declared earlier in the file took is a compile fatal
--FILE--
<?php
namespace B;
function eff() {}
use function A\eff;
echo "unreachable\n";
?>
--EXPECTF--
%s %s %s  Cannot use function A\eff as eff because the name is already in use %s
--CLEAN--
<?php
