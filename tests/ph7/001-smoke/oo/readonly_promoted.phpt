--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
readonly promoted constructor properties (incl. implicit-public `readonly T $x`)
--FILE--
<?php
class ReadonlyPromoted {
    public function __construct(
        public readonly string $name,
        private readonly int $age,
        readonly float $score
    ) {}
    public function age(): int { return $this->age; }
}
$o = new ReadonlyPromoted("Ada", 36, 9.5);
echo $o->name, "\n";
echo $o->age(), "\n";
echo $o->score, "\n";
?>
--EXPECT--
Ada
36
9.5
--CLEAN--
<?php
