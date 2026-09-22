--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Declaring __toString() with any return type other than string is a compile fatal
--DESCRIPTION--
The other side of __toString()'s implicit `string` return type: php refuses any
declared type that is not exactly `string` — `?string`, a union, `mixed`,
`static`, `self`, `array` and `iterable` all report
`C::__toString(): Return type must be string when declared`, at the method's
line, before any of the file runs. PHL used to compile them and then fall back
to the "Object" placeholder at the cast, because the declared type kept the
return value from ever being a string. `?string` is pinned here as the shape
most likely to be written by mistake; the check is one comparison, shared by
every rejected type.

`: void` and `: never` are the two php answers with a DIFFERENT message
(`A void method must not return a value` / `A never-returning method must not
return`), because php checks the return STATEMENT against those types first;
PHL has no such check yet, so it reports this one. Both engines reject, so they
are left out rather than pinned to a divergence.
--FILE--
<?php
class Bad { public function __toString(): ?string { return "x"; } }
echo "not reached\n";
?>
--EXPECTF--
%ABad::__toString(): Return type must be string when declared in %s on line 2%A
