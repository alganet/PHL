--TEST--
Late static binding (static::) resolves inside closures, static closures, and arrow functions; Closure::bind rebinds the called-class
--FILE--
<?php
class LsbBase {
    public static function who() { return static::class; }
    public function makeClosure() { return function() { return static::who() . '|' . static::class; }; }
    public function makeStatic()  { return static function() { return static::who(); }; }
    public function makeArrow()   { return fn() => static::who(); }
    public function makeNew()     { return static function() { return get_class(new static()); }; }
}
class LsbDerived extends LsbBase {
    public static function who() { return 'D:' . static::class; }
}
$lsbD = new LsbDerived();
echo ($lsbD->makeClosure())(), "\n";  // D:LsbDerived|LsbDerived
echo ($lsbD->makeStatic())(), "\n";   // D:LsbDerived
echo ($lsbD->makeArrow())(), "\n";    // D:LsbDerived
echo ($lsbD->makeNew())(), "\n";      // LsbDerived
// Closure::bind rebinds the called-class used by static::
$lsbC = (new LsbBase())->makeClosure();
echo Closure::bind($lsbC, null, LsbDerived::class)(), "\n"; // D:LsbDerived|LsbDerived
--EXPECT--
D:LsbDerived|LsbDerived
D:LsbDerived
D:LsbDerived
LsbDerived
D:LsbDerived|LsbDerived
