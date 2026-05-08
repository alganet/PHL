--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Stringable interface is auto-implemented when class declares __toString
--FILE--
<?php
class Plain {}
class Talker {
    public function __toString(): string { return "hi"; }
}
class TalkerChild extends Talker {}
echo "Plain: ", (new Plain()) instanceof Stringable ? "yes" : "no", "\n";
echo "Talker: ", (new Talker()) instanceof Stringable ? "yes" : "no", "\n";
echo "TalkerChild: ", (new TalkerChild()) instanceof Stringable ? "yes" : "no", "\n";
echo (string)(new Talker()), "\n";
?>
--EXPECT--
Plain: no
Talker: yes
TalkerChild: yes
hi
--CLEAN--
<?php
