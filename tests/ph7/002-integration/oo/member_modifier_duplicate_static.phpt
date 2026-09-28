--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`static` written twice on one member
--DESCRIPTION--
One sentence per modifier; this is the `static` one.
--FILE--
<?php
class MmdsC { public static static $p = 1; }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Multiple static modifiers are not allowed %s
--CLEAN--
<?php
