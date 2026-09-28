--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A dimension write cannot auto-initialize an array a type has no room for
--DESCRIPTION--
php decides by the declared type's MASK, so a class type is refused exactly like `int` --
`ArrayAccess` and `Traversable` included, which is the part a "does it behave like an array"
reading would get wrong. The sentence names the property and its type, and is a TypeError,
not the Error a plain read of an uninitialized typed property raises.
--FILE--
<?php
class AiarHolder { public int $t; }
$aiar = new AiarHolder;
$aiar->t['n'] = 'v';
?>
--EXPECTF--
PHP Fatal error:  Uncaught TypeError: Cannot auto-initialize an array inside property AiarHolder::$t of type int in %s:4
Stack trace:
#0 {main}
  thrown in %s on line 4
--CLEAN--
<?php
