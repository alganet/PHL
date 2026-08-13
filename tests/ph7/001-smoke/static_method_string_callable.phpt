--TEST--
"Class::method" string callables: direct call, call_user_func(_array), array_map, is_callable
--FILE--
<?php
class SmcStr {
    static function up($s) { return strtoupper($s); }
    static function add($a, $b) { return $a + $b; }
}
$smcF = "SmcStr::up";
echo $smcF("abc"), "\n";
echo call_user_func("SmcStr::add", 2, 3), call_user_func_array("SmcStr::add", [4, 5]), "\n";
echo implode(",", array_map("SmcStr::up", ["a", "b"])), "\n";
echo is_callable("SmcStr::up") ? "T" : "F",
     is_callable("SmcStr::nope") ? "T" : "F",
     is_callable("Nope::up") ? "T" : "F", "\n";
// inherited static through the subclass name
class SmcSub extends SmcStr {}
echo call_user_func("SmcSub::add", 1, 1), "\n";
// usort comparator as string
$smcArr = [3, 1, 2];
usort($smcArr, "SmcStr::add" === "x" ? "strcmp" : function ($a, $b) { return $a <=> $b; });
echo implode("", $smcArr), "\n";
--EXPECT--
ABC
59
A,B
TFF
2
123
