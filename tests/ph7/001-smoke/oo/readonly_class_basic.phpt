--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
readonly class (PHP 8.2): all declared properties become readonly; modifier combos
--FILE--
<?php
readonly class RoClassPoint {
    public int $x;
    public string $label;
    public function __construct(int $x, string $label) {
        $this->x = $x;
        $this->label = $label;
    }
}
$p = new RoClassPoint(3, "origin");
echo $p->x, "\n";
echo $p->label, "\n";

// final/abstract combine with readonly, in either order
final readonly class RoClassFr { public int $n; public function __construct() { $this->n = 1; } }
echo (new RoClassFr)->n, "\n";

readonly final class RoClassRf { public int $n; public function __construct() { $this->n = 2; } }
echo (new RoClassRf)->n, "\n";

// readonly stays usable as an ordinary identifier
function readonly() { return 42; }
echo readonly(), "\n";
?>
--EXPECT--
3
origin
1
2
42
--CLEAN--
<?php
