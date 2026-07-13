--TEST--
func_num_args/func_get_args arity, json control chars, get_class_methods order, constant('C::K'), ctor visibility
--DESCRIPTION--
Band A #4 batch, all surfaced by the Reflection work: func_num_args()
returned the FORMAL count for defaulted params (and func_get_args() listed
defaults and the packed variadic array); json_encode() emitted control
characters raw and tied backslash-escaping to JSON_UNESCAPED_SLASHES;
get_class_methods() returned reverse declaration order; constant('C::K')
did not resolve class constants (and missing constants were a notice+NULL,
not php 8's Error); non-public constructors were forced public so `new` on
a private-ctor class succeeded.
--FILE--
<?php
// --- func_num_args / func_get_args
function ebFn($a, $b = 2) { return func_num_args(); }
echo ebFn(1), ebFn(1, 2), "\n";
function ebGa($a, $b = 2, ...$c) { echo implode(",", func_get_args()), "\n"; }
ebGa(1);
ebGa(1, 2, 3, 4);
function ebEx($a, $b) { echo implode(",", func_get_args()), "\n"; }
ebEx(1, 2, 3, 4, 5);
class EbM { public function m($a = 1) { return func_num_args(); } }
echo (new EbM())->m(), (new EbM())->m(9), "\n";

// --- json_encode escapes
echo json_encode("a\nb\tc\x01"), "\n";
echo json_encode("http://x/y"), "\n";
echo json_encode("a/b", JSON_UNESCAPED_SLASHES), "\n";
echo json_encode("q\\w"), "\n";

// --- get_class_methods declaration order (own then parent)
class EbP {
    public function pa() {}
    public function pb() {}
}
class EbC extends EbP {
    public function ca() {}
    public function cb() {}
}
echo implode(",", get_class_methods("EbC")), "\n";

// --- constant() with class constants + php 8 Errors
class EbK { const K = 7; }
interface EbI { const J = "i"; }
var_export(constant("EbK::K")); echo "\n";
echo constant("EbI::J"), "\n";
try { constant("EB_NOPE"); } catch (Error $e) { echo $e->getMessage(), "\n"; }
try { constant("EbK::MISS"); } catch (Error $e) { echo $e->getMessage(), "\n"; }
try { constant("EbNoCls::K"); } catch (Error $e) { echo $e->getMessage(), "\n"; }

// --- constructor visibility
class EbPriv {
    private function __construct() { echo "ctor\n"; }
    public static function make() { return new EbPriv(); }
}
try { new EbPriv(); echo "made\n"; } catch (Error $e) { echo "caught:", $e->getMessage(), "\n"; }
echo get_class(EbPriv::make()), "\n";
class EbProt { protected function __construct() {} }
class EbSub extends EbProt { public static function make() { return new EbSub(); } }
echo get_class(EbSub::make()), "\n";
$r = new ReflectionClass("EbPriv");
var_export($r->isInstantiable()); echo "\n";
try { $r->newInstance(); } catch (ReflectionException $e) { echo "RE\n"; }
echo "done\n";
?>
--EXPECT--
12
1
1,2,3,4
1,2,3,4,5
01
"a\nb\tc\u0001"
"http:\/\/x\/y"
"a/b"
"q\\w"
ca,cb,pa,pb
7
i
Undefined constant "EB_NOPE"
Undefined constant EbK::MISS
Class "EbNoCls" not found
caught:Call to private EbPriv::__construct() from global scope
ctor
EbPriv
EbSub
false
RE
done
