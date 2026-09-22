--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A const declaration whose name is a local const import is a compile fatal
--FILE--
<?php
namespace B;
use const A\KAY;
const KAY = 1;
echo "unreachable\n";
?>
--EXPECTF--
%s %s %s  Cannot declare const B\KAY because the name is already in use %s
--CLEAN--
<?php
