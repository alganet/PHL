--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An interface method must be public, and php names it with its parentheses
--DESCRIPTION--
The method half of the same rule; PHL used to drop the `()` php prints.
--FILE--
<?php
interface ImvI { protected function f(); }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Access type for interface method ImvI::f() must be public %s
--CLEAN--
<?php
