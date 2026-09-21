--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: attributes cannot decorate a comma-separated const list
--FILE--
<?php
#[Attribute]
class Marker {}
#[Marker] const A = 1, B = 2;
echo A, B, "\n";
?>
--EXPECTF--
%ACannot apply attributes to multiple constants at once%A
--CLEAN--
<?php

