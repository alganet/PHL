--TEST--
A no-capture long-form static closure declared in a method carries its class scope (self::, private property/method); global and free-function lambdas are unaffected
--FILE--
<?php
class Ncs {
    private static $s = 'PS';
    private $i = 'PI';
    const C = 'CC';
    private function priv() { return 'PRIV'; }
    public function selfRef() { return static function() { return self::$s . '|' . self::C; }; }
    public function instRef() { return static function(Ncs $p) { return $p->i . '|' . $p->priv(); }; }
}
$ncsP = new Ncs();
echo ($ncsP->selfRef())(), "\n";          // PS|CC
echo ($ncsP->instRef())(new Ncs()), "\n"; // PI|PRIV
// A global no-capture static lambda needs no scope and is unchanged
$ncsG = static function ($x) { return $x * 2; };
echo $ncsG(21), "\n";                     // 42
// A no-capture static closure in a free function has no class scope but must not crash
function ncsMk() { return static function () { return 'freefn'; }; }
echo (ncsMk())(), "\n";                   // freefn
--EXPECT--
PS|CC
PI|PRIV
42
freefn
