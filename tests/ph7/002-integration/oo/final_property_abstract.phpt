--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`final` and `abstract` on one property is php's own sentence
--DESCRIPTION--
An abstract (hooked) property is a requirement and a final one may not be
restated: php refuses the pair before either rule applies.
--FILE--
<?php
abstract class FpaC { final abstract public int $p { get; } }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Cannot use the final modifier on an abstract property %s
--CLEAN--
<?php
