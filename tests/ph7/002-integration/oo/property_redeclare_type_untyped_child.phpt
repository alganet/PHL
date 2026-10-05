--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An untyped child property is refused under a typed parent, mixed included
--DESCRIPTION--
Leaving the type off in the child is not the top type: under a parent
declared `mixed` it is still a mismatch. The message names the class that
DECLARED the property, two levels up.
--FILE--
<?php
class PrdTuBase {
    public mixed $p;
}
class PrdTuMid extends PrdTuBase {}
class PrdTuKid extends PrdTuMid {
    public $p;
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Type of PrdTuKid::$p must be mixed (as in class PrdTuBase) %s
