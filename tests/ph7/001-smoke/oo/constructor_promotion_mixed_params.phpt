--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Constructor property promotion: promoted and non-promoted params coexist
--FILE--
<?php
class CppMix {
    public int $tally = 0;
    public function __construct(public int $x, int $bonus) {
        $this->tally = $x + $bonus;
    }
}
$m = new CppMix(3, 4);
echo $m->x, "\n";
echo $m->tally, "\n";
?>
--EXPECT--
3
7
--CLEAN--
<?php
unset($m);
