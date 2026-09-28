--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Argument unpacking after a named argument is a compile error (anon form)
--DESCRIPTION--
php refuses `f(x: 1, ...$a)` where it is WRITTEN, the mirror of the positional-after-named rule
it already enforced. PHL compiled it and let the runtime binder report whatever it made of the
flattened list -- a different sentence, raised too late, on a program php never starts. The
anonymous-class form parses its argument list separately and had none of the four order rules.
--FILE--
<?php
class AuacnB { function __construct($a=0,$b=0){} } new class(a: 1, ...[2]) extends AuacnB {};
?>
--EXPECTF--
%s Fatal error:  Cannot use argument unpacking after named arguments%A
--CLEAN--
<?php
