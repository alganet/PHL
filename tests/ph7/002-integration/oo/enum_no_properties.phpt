--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Enums cannot declare properties (compile fatal)
--FILE--
<?php
enum EnumNoProps { case A; public $x = 1; }
echo "unreached\n";
?>
--EXPECTF--
%AEnum EnumNoProps cannot include properties%A
--CLEAN--
<?php
