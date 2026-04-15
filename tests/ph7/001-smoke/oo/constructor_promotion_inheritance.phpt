--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Constructor property promotion: child ctor calls parent with its own promoted props
--FILE--
<?php
class CppBase {
    public function __construct(public int $id) {}
}
class CppChild extends CppBase {
    public function __construct(int $id, public string $name) {
        parent::__construct($id);
    }
}
$c = new CppChild(7, "alice");
echo $c->id, "/", $c->name, "\n";
?>
--EXPECT--
7/alice
--CLEAN--
<?php
unset($c);
