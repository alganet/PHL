--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
class var property compiles
--FILE--
<?php
class C { var $x = 5; }
$o = new C();
echo $o->x;
?>
--EXPECT--
5
