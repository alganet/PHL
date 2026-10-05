--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A property untyped in the parent must stay untyped in the child
--DESCRIPTION--
The ABSENT type is not mixed: php refuses a declared type over an untyped
parent property with its own sentence.
--FILE--
<?php
class PrdToParent {
    public $p;
}
class PrdToKid extends PrdToParent {
    public int $p;
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Type of PrdToKid::$p must be omitted to match the parent definition in class PrdToParent %s
