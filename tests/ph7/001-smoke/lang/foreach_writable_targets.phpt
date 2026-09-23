--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
foreach binds any writable target, on the key side as much as the value side
--FILE--
<?php
class FeTgtHolder {
    public $k;
    public $v;
    public $arr = [];
    public static $s;
}

$h = new FeTgtHolder;
foreach (["a" => 1, "b" => 2] as $h->k => $h->v) {
    echo "$h->k=$h->v\n";
}

foreach ([10, 20] as FeTgtHolder::$s) {
}
echo "static ", FeTgtHolder::$s, "\n";

$slot = [];
foreach (["x" => "one", "y" => "two"] as $slot["key"] => $slot["value"]) {
}
echo $slot["key"], " ", $slot["value"], "\n";

$appended = [];
foreach ([7, 8, 9] as $appended[]) {
}
echo implode(",", $appended), "\n";

$h->arr = [];
foreach (["deep" => 5] as $h->arr["k"] => $h->arr["v"]) {
}
echo $h->arr["k"], " ", $h->arr["v"], "\n";

$name = "fetgt_dyn";
foreach ([1, 2, 3] as $$name) {
}
echo $fetgt_dyn, "\n";

class FeTgtSelf {
    public $p;
    public function run(array $in) {
        foreach ($in as $this->p) {
        }
        return $this->p;
    }
}
echo (new FeTgtSelf)->run(["last"]), "\n";
?>
--EXPECT--
a=1
b=2
static 20
y two
7,8,9
deep 5
3
last
--CLEAN--
<?php
unset($h, $slot, $appended, $name, $fetgt_dyn);
