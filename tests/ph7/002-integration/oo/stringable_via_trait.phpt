--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Stringable is auto-implemented when __toString comes from a trait
--FILE--
<?php
trait HasToString {
    public function __toString(): string { return "from-trait"; }
}
class Greeter {
    use HasToString;
}
$g = new Greeter();
echo $g instanceof Stringable ? "yes" : "no", "\n";
echo (string)$g, "\n";
?>
--EXPECT--
yes
from-trait
--CLEAN--
<?php
