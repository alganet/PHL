--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`abstract` written twice on one member
--DESCRIPTION--
One sentence per modifier; this is the `abstract` one.
--FILE--
<?php
abstract class MmdabC { abstract abstract function f(); }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Multiple abstract modifiers are not allowed %s
--CLEAN--
<?php
