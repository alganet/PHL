--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A case of a non-backed enum must not have a value (compile fatal)
--FILE--
<?php
enum EnumPureWithValue { case A = 5; }
echo "unreached\n";
?>
--EXPECTF--
%ACase A of non-backed enum EnumPureWithValue must not have a value%A
--CLEAN--
<?php
