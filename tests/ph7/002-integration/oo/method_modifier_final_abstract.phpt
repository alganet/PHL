--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`final` and `abstract` on one method
--DESCRIPTION--
PHL used to answer "Unexpected token 'abstract'" from its `final` branch, which
described the ladder it fell out of rather than the declaration.
--FILE--
<?php
abstract class MmfaC { final abstract function f(); }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Cannot use the final modifier on an abstract method %s
--CLEAN--
<?php
