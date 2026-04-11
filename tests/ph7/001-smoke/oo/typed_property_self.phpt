--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: self (linked list node)
--FILE--
<?php
class TpNode {
    public int $value;
    public ?self $next = null;
    public function __construct(int $v) { $this->value = $v; }
}
$head = new TpNode(1);
$head->next = new TpNode(2);
$head->next->next = new TpNode(3);
for ($n = $head; $n !== null; $n = $n->next) {
    echo $n->value, "\n";
}
?>
--EXPECT--
1
2
3
--CLEAN--
<?php
unset($head, $n);
