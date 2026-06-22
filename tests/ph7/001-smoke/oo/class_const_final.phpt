--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Final class constants (PHP 8.1): modifier order and combination with a type
--FILE--
<?php
class FinalConstModifiers {
    final const X = 1;
    public final const Y = 2;
    final public const Z = 3;
    final const int T = 4;        // final + typed together
}
echo FinalConstModifiers::X, FinalConstModifiers::Y, FinalConstModifiers::Z, FinalConstModifiers::T, "\n";
?>
--EXPECT--
1234
--CLEAN--
<?php
