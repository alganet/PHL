--TEST--
Attribute capture on closures/arrow-fns/anonymous classes in expressions and on const statements
--FILE--
<?php
set_error_handler(function ($no, $msg) { echo "[", $no, "] ", $msg, "\n"; return true; });
#[Attribute] class AecMark { function __construct(public string $v = "d", public int $n = 0) {} }
// closure expression
$aecC1 = #[AecMark("x", 5)] function () { return 1; };
$aecAt = (new ReflectionFunction($aecC1))->getAttributes();
echo count($aecAt), $aecAt[0]->getName(), $aecAt[0]->newInstance()->v, $aecAt[0]->newInstance()->n, "\n";
// arrow-fn expression
$aecC2 = #[AecMark] fn() => 2;
echo count((new ReflectionFunction($aecC2))->getAttributes()), "\n";
// anonymous-class expression
$aecO = new #[AecMark("k")] class { public $z = 3; };
$aecCat = (new ReflectionClass($aecO))->getAttributes();
echo count($aecCat), $aecCat[0]->newInstance()->v, "\n";
// attributes on a global const statement (php 8.5) + ReflectionConstant
#[AecMark("c")] const AEC_C = 41;
$aecR = new ReflectionConstant("AEC_C");
$aecGat = $aecR->getAttributes();
echo count($aecGat), $aecGat[0]->getName(), $aecGat[0]->newInstance()->v, AEC_C, "\n";
// #[\Deprecated] on a global const warns on access and through constant()
#[\Deprecated(since: "1.2")] const AEC_D = 42;
echo AEC_D, "\n";
echo constant("AEC_D"), "\n";
// ... and a deprecated class constant through constant()
class AecK { #[\Deprecated] const KK = 43; }
echo constant("AecK::KK"), "\n";
restore_error_handler();
--EXPECT--
1AecMarkx5
1
1k
1AecMarkc41
[16384] Constant AEC_D is deprecated since 1.2
42
[16384] Constant AEC_D is deprecated since 1.2
42
[16384] Constant AecK::KK is deprecated
43
