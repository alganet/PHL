--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Expression dereferencing: method chaining on function return
--FILE--
<?php
class Builder {
    private $val;
    function __construct($v) { $this->val = $v; }
    function add($n) { $this->val += $n; return $this; }
    function get() { return $this->val; }
}
function makeBuilder() {
    return new Builder(10);
}
echo makeBuilder()->add(5)->get(), "\n";
echo makeBuilder()->get(), "\n";
?>
--EXPECT--
15
10
--CLEAN--
<?php
