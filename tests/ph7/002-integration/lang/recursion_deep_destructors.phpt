--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Deep recursion (under the cap): per-frame destructor cascade fires in unwind order
--FILE--
<?php
class Node {
    public function __construct(private int $id) {}
    public function __destruct() {
        echo "~", $this->id;
    }
}
function build(int $n) {
    $local = new Node($n);
    if ($n > 0) {
        build($n - 1);
    }
}
build(6);
echo "\n";
?>
--EXPECT--
~0~1~2~3~4~5~6
--CLEAN--
<?php
