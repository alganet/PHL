--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Enum backing type must be int or string (compile fatal)
--FILE--
<?php
enum EnumFloatBacked: float { case A = 1.5; }
echo "unreached\n";
?>
--EXPECTF--
%AEnum backing type must be int or string, float given%A
--CLEAN--
<?php
