--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The visibility WARNING is php's first line even when the same declaration is also fatal
--DESCRIPTION--
The magic-method rules are decided with the signature in hand but raised at the
end of the declaration, so that the fatals php decides FIRST — a redeclaration,
an abstract method in a non-abstract class, a parse error in the body — win.
`__toString`'s return-type rule is the one php decides AFTER them, so it is the
one case where the parked diagnostic must still come out first.

It matters because that diagnostic is a WARNING, not a fatal: a declaration can
be both non-public and wrongly typed, and php reports both lines. Ordered the
other way the warning would simply be lost, since a declaration that has already
reported a fatal stays quiet.
--FILE--
<?php
class Both { private function __toString(): int { return 1; } }
echo "not reached\n";
?>
--EXPECTF--
%AWarning:%AThe magic method Both::__toString() must have public visibility in %s on line 2
%ABoth::__toString(): Return type must be string when declared in %s on line 2%A
