--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
$this is accessible inside a method's catch block
--FILE--
<?php
class CatchScopeThisBox {
    public $v = 7;
    public function run() {
        try {
            throw new Exception("e");
        } catch (Exception $e) {
            echo $this->v . "\n";
            $this->v = 9;
            echo $this->v . "\n";
        }
    }
}
$b = new CatchScopeThisBox();
$b->run();
echo $b->v . "\n";
?>
--EXPECT--
7
9
9
--CLEAN--
<?php
