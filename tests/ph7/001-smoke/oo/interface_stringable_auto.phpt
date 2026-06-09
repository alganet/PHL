--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Stringable is auto-implemented when class declares __toString
--FILE--
<?php
class IfaceStringableYes {
    public function __toString(): string { return "x"; }
}
class IfaceStringableNo {}
echo (new IfaceStringableYes()) instanceof Stringable ? "IfaceStringableYes:yes" : "IfaceStringableYes:no", "\n";
echo (new IfaceStringableNo()) instanceof Stringable ? "IfaceStringableNo:yes" : "IfaceStringableNo:no", "\n";
?>
--EXPECT--
IfaceStringableYes:yes
IfaceStringableNo:no
--CLEAN--
<?php
