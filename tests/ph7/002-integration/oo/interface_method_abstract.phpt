--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An interface method may not be written `abstract`
--DESCRIPTION--
The other half of the interface-method rule.
--FILE--
<?php
interface ImaI { abstract public function f(); }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Interface method ImaI::f() must not be abstract %s
--CLEAN--
<?php
