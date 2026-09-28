--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A hooked property may not be readonly (PHP 8.4)
--DESCRIPTION--
`readonly` promises one write and a hook decides what a write MEANS, so php will
not have both. The declaration screen never had this rule, so the pair compiled
here -- in a readonly CLASS too, where the modifier is implied rather than
written.
--FILE--
<?php
class HprC { public readonly int $p { get => 1; } }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Hooked properties cannot be readonly %s
--CLEAN--
<?php
