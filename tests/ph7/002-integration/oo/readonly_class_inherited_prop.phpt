--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
readonly class: an inherited readonly property is immutable; the error names its declaring class
--FILE--
<?php
readonly class RoInhBase {
    public int $x;
    public function __construct() { $this->x = 1; }
}
readonly class RoInhChild extends RoInhBase {}
$o = new RoInhChild();
echo $o->x, "\n";
$o->x = 9;
?>
--EXPECTF--
1
%s Fatal error:  Uncaught Error: Cannot modify readonly property RoInhBase::$x%A
--CLEAN--
<?php
