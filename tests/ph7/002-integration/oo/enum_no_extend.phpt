--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A class cannot extend an enum (compile fatal)
--FILE--
<?php
enum EnumSealed { case A; }
class EnumSubclass extends EnumSealed {}
echo "unreached\n";
?>
--EXPECTF--
%AClass EnumSubclass cannot extend enum EnumSealed%A
--CLEAN--
<?php
