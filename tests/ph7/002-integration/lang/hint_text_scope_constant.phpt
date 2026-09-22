--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a typed class CONSTANT names its resolved type in the fatal
--FILE--
<?php
class Base {}
// The mismatch is a non-catchable fatal raised when the class is mounted, so it
// is the only thing this file can assert. `self` has to read as the class name.
class R extends Base { const self CST = 1; }
echo "never reached\n";
?>
--EXPECTF--
%ACannot use int as value for class constant R::CST of type R in %s on line %d%A
--CLEAN--
<?php
