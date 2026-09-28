--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An interface method may not contain a body
--DESCRIPTION--
The interface noun for the same rule.
--FILE--
<?php
interface ImbI { public function f() {} }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Interface function ImbI::f() cannot contain body %s
--CLEAN--
<?php
