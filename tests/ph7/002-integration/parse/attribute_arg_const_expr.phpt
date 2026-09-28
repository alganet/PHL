--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An attribute argument is a constant expression: a call in one is a compile-time fatal
--FILE--
<?php
#[Attribute]
class CesAttr { public function __construct($x) {} }
#[CesAttr(strlen('ab'))]
class CesTarget {}
echo "unreachable\n";
?>
--EXPECTF--
%AConstant expression contains invalid operations%A
--CLEAN--
<?php
