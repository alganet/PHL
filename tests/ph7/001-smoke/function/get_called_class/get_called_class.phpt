--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_called_class builtin basic checks

--FILE--
<?php
class A {
    public static function who(){ echo get_called_class() . "\n"; }
}
class B extends A {}
// Calling via B should return 'B' and via A should return 'A'
B::who();
A::who();
?>
--EXPECT--
B
A
--CLEAN--
<?php

