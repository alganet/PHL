--TEST--
#[\Deprecated] runtime E_USER_DEPRECATED notices for functions, methods, constants, enum cases
--FILE--
<?php
set_error_handler(function ($no, $msg) { echo "[", $no, "] ", $msg, "\n"; return true; });
#[\Deprecated] function depFnA() { return 1; }
#[\Deprecated(message: "use depFnC() instead", since: "2.0")] function depFnB() { return 2; }
#[\Deprecated("plain positional")] function depFnC() { return 3; }
#[\Deprecated(since: "8.4")] function depFnD() { return 4; }
echo depFnA(), depFnB(), depFnC(), depFnD(), "\n";
class DepCls {
    #[\Deprecated] function m() { return "m"; }
    #[\Deprecated(message: "gone")] static function s() { return "s"; }
    #[\Deprecated] const K = 11;
    #[\Deprecated(message: "use L", since: "3.1")] const M = 12;
    const PLAIN = 13;
}
echo (new DepCls())->m(), DepCls::s(), "\n";
echo DepCls::K, DepCls::M, DepCls::PLAIN, "\n";
// generators warn at the call site, before iteration
#[\Deprecated] function depGen() { yield 5; }
$depG = depGen();
echo "created;";
foreach ($depG as $v) { echo $v; }
echo "\n";
// first-class-callable creation is quiet; the call warns
#[\Deprecated] function depFnE() { return 6; }
$depC = depFnE(...);
echo "made;", $depC(), "\n";
// a non-Deprecated attribute does not trigger
#[Attribute] class DepOther {}
#[DepOther] function depFnF() { return 7; }
echo depFnF(), "\n";
// every call and every constant access re-warns
echo depFnA(), depFnA(), "\n";
echo DepCls::K, "\n";
// deprecated enum case
enum DepEnum { #[\Deprecated] case A; }
$depX = DepEnum::A;
echo $depX->name, "\n";
restore_error_handler();
--EXPECT--
[16384] Function depFnA() is deprecated
1[16384] Function depFnB() is deprecated since 2.0, use depFnC() instead
2[16384] Function depFnC() is deprecated, plain positional
3[16384] Function depFnD() is deprecated since 8.4
4
[16384] Method DepCls::m() is deprecated
m[16384] Method DepCls::s() is deprecated, gone
s
[16384] Constant DepCls::K is deprecated
11[16384] Constant DepCls::M is deprecated since 3.1, use L
1213
[16384] Function depGen() is deprecated
created;5
made;[16384] Function depFnE() is deprecated
6
7
[16384] Function depFnA() is deprecated
1[16384] Function depFnA() is deprecated
1
[16384] Constant DepCls::K is deprecated
11
[16384] Enum case DepEnum::A is deprecated
A
