--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
readonly property (PHP 8.1): initialized once in the constructor, read back
--FILE--
<?php
class ReadonlyBasic {
    public readonly int $x;
    public readonly string $name;
    public function __construct(int $v, string $n) {
        $this->x = $v;
        $this->name = $n;
    }
}
$o = new ReadonlyBasic(42, "Ada");
echo $o->x, "\n";
echo $o->name, "\n";
?>
--EXPECT--
42
Ada
--CLEAN--
<?php
