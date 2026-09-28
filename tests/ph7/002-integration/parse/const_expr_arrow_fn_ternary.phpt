--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An arrow function whose body is a ternary is still refused: the ternary belongs to the arrow, not to the constant expression
--DESCRIPTION--
An arrow function carries no braces, so its body's `?` sits at the same bracket
depth as the initializer's own. Read as the initializer's ternary it folded the
whole arrow away -- and the arrow is exactly what a constant expression may not
hold. The ternary split skips a closure / arrow construct whole for that reason.
--FILE--
<?php
class CeaT {
    const C = fn() => true ? 1 : 2;
}
echo "unreachable\n";
?>
--EXPECTF--
%AConstant expression contains invalid operations%A
--CLEAN--
<?php
