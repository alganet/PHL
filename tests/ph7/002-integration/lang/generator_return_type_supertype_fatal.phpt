--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A generator declaring a non-supertype-of-Generator return type is a compile-time fatal (php 8)
--FILE--
<?php
function g(): int {
    yield 1;
}
echo "never\n";
?>
--EXPECTF--
%AGenerator return type must be a supertype of Generator, int given%A
--CLEAN--
<?php
