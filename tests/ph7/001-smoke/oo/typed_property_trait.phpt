--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: declared inside a trait
--FILE--
<?php
trait TptCounter {
    public int $count = 0;
    public ?string $label = null;
}
class TptWidget {
    use TptCounter;
}
$w = new TptWidget();
$w->count = 10;
$w->label = "clicks";
echo $w->count, " ", $w->label, "\n";
?>
--EXPECT--
10 clicks
--CLEAN--
<?php
unset($w);
