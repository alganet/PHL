--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The write targets php DOES accept keep working next to the temporary rule
--FILE--
<?php
class WriteOkHolder {
    public $p = 0;
    public $arr = [];
    public static $s = 0;
    public static $sarr = [];
    public function me() { return $this; }
    public function run() {
        $this->p = 1;
        $this->arr["k"] = 2;
        $this->arr[] = 3;
        return $this->p + $this->arr["k"] + count($this->arr);
    }
}

// A userland CALL result is writable through — php's rule, and the reason the
// temporary check classifies the chain's BASE rather than refusing every
// non-variable.
$o = new WriteOkHolder;
$o->me()->p = 5;
$o->me()->arr[] = 6;
echo $o->p, " ", count($o->arr), "\n";

// Static storage outlives any temporary, including one used to name its class.
WriteOkHolder::$s = 7;
WriteOkHolder::$sarr["k"] = 8;
(new WriteOkHolder)::$s = 9;
echo WriteOkHolder::$s, " ", WriteOkHolder::$sarr["k"], "\n";

// A parenthesised VARIABLE is still that variable.
($o)->p = 10;
echo $o->p, "\n";

echo (new WriteOkHolder)->run(), "\n";
?>
--EXPECT--
5 1
9 8
10
5
--CLEAN--
<?php
unset($o);
