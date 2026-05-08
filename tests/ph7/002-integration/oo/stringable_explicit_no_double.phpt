--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Class explicitly implementing Stringable plus declaring __toString does not double-implement
--FILE--
<?php
class ExplicitStr implements Stringable {
    public function __toString(): string { return "ok"; }
}
$x = new ExplicitStr();
echo $x instanceof Stringable ? "yes" : "no", "\n";
echo (string)$x, "\n";
?>
--EXPECT--
yes
ok
--CLEAN--
<?php
