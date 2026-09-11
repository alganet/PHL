--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
isset()/empty()/?? treat an uninitialized typed property as not-set (no Error)
--FILE--
<?php
class CutpBox {
    private readonly int $x;
    public int $y = 7;
    public function tIsset(): bool { return isset($this->x); }
    public function tCoalesce(): string { return $this->x ?? 'def'; }
    public function tPresent(): int { return $this->y ?? 99; }
}
$b = new CutpBox();
echo $b->tIsset() ? "set\n" : "unset\n";
echo $b->tCoalesce(), "\n";
echo $b->tPresent(), "\n";
?>
--EXPECT--
unset
def
7
