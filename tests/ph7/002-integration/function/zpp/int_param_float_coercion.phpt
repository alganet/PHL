--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: builtin int parameters deprecate a lossy float (ZPP contract)
--SKIPIF--
skip: flaky
--FILE--
<?php
// a fractional float truncates, carrying php's precision deprecation
echo str_repeat('x', 2.7), "\n";
echo intdiv(7.9, 2), "\n";
echo substr('abcdef', 1.5), "\n";
echo str_pad('x', 3.5), "\n";
$a = [1, 2, 3];
echo array_slice($a, 1.5)[0], "\n";
// a float-STRING carries php's distinct float-string wording
echo str_repeat('y', '2.5'), "\n";
// an INTEGRAL float loses nothing, so it stays silent
echo str_repeat('z', 3.0), "\n";
echo substr('abcdef', 2.0), "\n";
echo intdiv(7, 2), "\n";
?>
--EXPECTF--
%AImplicit conversion from float 2.7 to int loses precision%A
xx
%AImplicit conversion from float 7.9 to int loses precision%A
3
%AImplicit conversion from float 1.5 to int loses precision%A
bcdef
%AImplicit conversion from float 3.5 to int loses precision%A
x  
%AImplicit conversion from float 1.5 to int loses precision%A
2
%AImplicit conversion from float-string "2.5" to int loses precision%A
yy
zzz
cdef
3
--CLEAN--
<?php
unset($a);
