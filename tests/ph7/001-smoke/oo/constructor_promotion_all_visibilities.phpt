--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Constructor property promotion: public, protected, private
--FILE--
<?php
class CppVis {
    public function __construct(
        public int $pub,
        protected int $pro,
        private int $priv
    ) {}
    public function dump() {
        echo $this->pub, "/", $this->pro, "/", $this->priv, "\n";
    }
}
$v = new CppVis(1, 2, 3);
$v->dump();
echo $v->pub, "\n";
?>
--EXPECT--
1/2/3
1
--CLEAN--
<?php
unset($v);
