--TEST--
A generator's argument refusal traces the generator as called from the creating line
--FILE--
<?php
// A generator function binds its arguments at the call that creates the Generator,
// so a refusal raised there has the generator's frame on its trace -- called from
// THAT line, whatever spelling made the call. Its frame is built detached from the
// frame chain and only a start or resume stamped it, so it read line 1.
class C {
    function m(int $a) { yield $a; }
    static function s(int ...$a) { yield 1; }
}
function gen(int $a, int $b = 2) { yield $a; }
function outer() {
    return gen([]);
}
$f = function (int $a) { yield $a; };
$cases = [
    'direct'  => fn() => gen("x"),
    'nested'  => fn() => outer(),
    'method'  => fn() => (new C)->m("x"),
    'static'  => fn() => C::s(1, "x"),
    'closure' => fn() => $f("x"),
    'cuf'     => fn() => call_user_func('gen', "x"),
    'named'   => fn() => gen(b: "x", a: 1),
    'few'     => fn() => gen(),
];
foreach ($cases as $k => $c) {
    try {
        $c();
    } catch (Error $e) {
        echo $k, ': ', get_class($e), "\n";
        foreach ($e->getTrace() as $i => $fr) {
            if (($fr['file'] ?? __FILE__) !== __FILE__) {
                break; // the harness's own frames, when it runs the case in-process
            }
            echo "  #$i ", str_replace(__FILE__, 'F', $fr['function']), ' ', $fr['line'] ?? '-', "\n";
        }
    }
}
// Stamping at creation does not pin the frame there: a later resume re-stamps it.
function g2() { yield 1; throw new Exception("in"); }
$g = g2();
try {
    foreach ($g as $v) {
    }
} catch (Exception $e) {
    echo 'resume: ', $e->getTrace()[0]['line'], "\n";
}
--EXPECT--
direct: TypeError
  #0 gen 16
  #1 {closure:F:16} 27
nested: TypeError
  #0 gen 12
  #1 outer 17
  #2 {closure:F:17} 27
method: TypeError
  #0 m 18
  #1 {closure:F:18} 27
static: TypeError
  #0 s 19
  #1 {closure:F:19} 27
closure: TypeError
  #0 {closure:F:14} 20
  #1 {closure:F:20} 27
cuf: TypeError
  #0 gen 21
  #1 {closure:F:21} 27
named: TypeError
  #0 gen 22
  #1 {closure:F:22} 27
few: ArgumentCountError
  #0 gen 23
  #1 {closure:F:23} 27
resume: 42
