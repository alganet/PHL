--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
readonly class (PHP 8.2): promoted constructor properties become readonly and read back
--FILE--
<?php
readonly class Money {
    public function __construct(
        public int $amount,
        public string $currency,
    ) {}
}
$m = new Money(100, "EUR");
echo $m->amount, "\n";
echo $m->currency, "\n";

// a non-readonly typed property of a non-readonly class still increments normally
class RoClassPromotedCounter {
    public int $n = 0;
    public function bump(): void { $this->n++; }
}
$c = new RoClassPromotedCounter;
$c->bump();
$c->bump();
echo $c->n, "\n";
?>
--EXPECT--
100
EUR
2
--CLEAN--
<?php
