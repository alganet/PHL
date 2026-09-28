--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`readonly` written twice on one member
--DESCRIPTION--
One sentence per modifier; this is the `readonly` one. It is read as a modifier
at this position however many times it appears, which is why php has a duplicate
rule for it rather than reading the second one as a type.
--FILE--
<?php
class MmdrC { public readonly readonly int $p; }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Multiple readonly modifiers are not allowed %s
--CLEAN--
<?php
