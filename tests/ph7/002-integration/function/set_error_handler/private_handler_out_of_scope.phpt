--TEST--
set_error_handler(): a private or protected method handler throws "Invalid callback" when a diagnostic is raised outside its scope
--FILE--
<?php
// php keeps set_error_handler()'s callable as a bare value and resolves it again
// each time a diagnostic reaches it, in the scope the diagnostic was raised in.
// A private or protected method registered from inside its class is callable
// there and nowhere else: outside, php throws "Invalid callback" in place of the
// diagnostic, and the statement that raised it goes no further.
class A {
    private function priv($no, $str) { echo "A::priv: $str\n"; return true; }
    protected function prot($no, $str) { echo "A::prot: $str\n"; return true; }
    function usePriv() { set_error_handler([$this, 'priv']); }
    function useProt() { set_error_handler([$this, 'prot']); }
    function inside() { echo $insideA; }
}
class B extends A {
    function inB() { echo $insideB; }
}
function lookup() { $x = []; return $x['k']; }
function attempt(callable $f) {
    try {
        $f();
        echo "  statement finished\n";
    } catch (Error $e) {
        echo "  ", get_class($e), ": ", $e->getMessage(), " @", $e->getLine(), "\n";
    }
}

$a = new A;
$a->usePriv();
echo "private, raised inside the class:\n";
$a->inside();
echo "the handler is still reported as installed outside it:\n";
var_dump(get_error_handler() === [$a, 'priv']);
echo "private, raised outside the class:\n";
attempt(function () { echo $undefinedVar; });
attempt(function () { lookup(); });
attempt(function () { trigger_error("user notice"); });
attempt(function () { $r = @$suppressed; });
attempt(function () { $r = hex2bin("abc"); });
echo "a subclass cannot reach the parent's private method either:\n";
$b = new B;
attempt(function () use ($b) { $b->inB(); });
var_dump(set_error_handler(null) === [$a, 'priv']);
restore_error_handler();
restore_error_handler();

$b->useProt();
echo "protected, raised in a subclass:\n";
$b->inB();
echo "protected, raised outside the hierarchy:\n";
attempt(function () { echo $outside; });
restore_error_handler();

echo "no handler left:\n";
echo @$quiet, "done\n";
--EXPECT--
private, raised inside the class:
A::priv: Undefined variable $insideA
the handler is still reported as installed outside it:
bool(true)
private, raised outside the class:
  Error: Invalid callback A::priv, cannot access private method A::priv() @34
  Error: Invalid callback A::priv, cannot access private method A::priv() @17
  Error: Invalid callback A::priv, cannot access private method A::priv() @36
  Error: Invalid callback A::priv, cannot access private method A::priv() @37
  Error: Invalid callback A::priv, cannot access private method A::priv() @38
a subclass cannot reach the parent's private method either:
  Error: Invalid callback A::priv, cannot access private method A::priv() @15
bool(true)
protected, raised in a subclass:
A::prot: Undefined variable $insideB
protected, raised outside the hierarchy:
  Error: Invalid callback B::prot, cannot access protected method B::prot() @50
no handler left:
done
