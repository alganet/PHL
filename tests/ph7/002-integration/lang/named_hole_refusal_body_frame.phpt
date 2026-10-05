--TEST--
A named call's unbound parameter is refused with the callee's own frame on the trace
--FILE--
<?php
// A named call that leaves a required parameter unbound below a bound one is
// refused `Argument #N ($x) not passed` from INSIDE the callee, so the trace has
// the callee's own frame -- for a generator (whose call binds its arguments) and
// for a fiber body started by Fiber::start() as well. Both are bound before
// their frame is on the chain, and the refusal was thrown there, frameless.
class C {
    function m(int $a, int $b) { yield $a; }
    static function s(int $a, int $b) { yield $a; }
    function body(int $a, int $b) { return $a; }
}
function gen(int $a, int $b) { yield $a; }
function body(int $a, int $b) { return $a; }
$gc = 'gen';
$cases = [
    'gen'        => fn() => gen(b: 1),
    'gen-method' => fn() => (new C)->m(b: 1),
    'gen-static' => fn() => C::s(b: 1),
    'gen-var'    => fn() => $gc(b: 1),
    'gen-clos'   => fn() => (function (int $a, int $b) { yield $a; })(b: 1),
    'fiber-fn'   => fn() => (new Fiber('body'))->start(b: 1),
    'fiber-meth' => fn() => (new Fiber([new C, 'body']))->start(b: 1),
    'fiber-clos' => fn() => (new Fiber(function (int $a, int $b) {}))->start(b: 1),
    // An unknown name is the CALL's refusal: no callee frame, in php too.
    'unknown'    => fn() => gen(c: 1),
    'fib-unk'    => fn() => (new Fiber('body'))->start(c: 1),
];
foreach ($cases as $k => $c) {
    try {
        $c();
    } catch (Error $e) {
        echo $k, ': ', get_class($e), ' @', $e->getLine(), ' ',
            str_replace(__FILE__, 'F', $e->getMessage()), "\n";
        foreach ($e->getTrace() as $i => $fr) {
            if (($fr['file'] ?? __FILE__) !== __FILE__) {
                break; // the harness's own frames, when it runs the case in-process
            }
            echo "  #$i ", ($fr['class'] ?? ''), ($fr['type'] ?? ''),
                str_replace(__FILE__, 'F', $fr['function']), ' ', $fr['line'] ?? '-', "\n";
        }
    }
}
--EXPECT--
gen: ArgumentCountError @12 gen(): Argument #1 ($a) not passed
  #0 gen 16
  #1 {closure:F:16} 30
gen-method: ArgumentCountError @8 C::m(): Argument #1 ($a) not passed
  #0 C->m 17
  #1 {closure:F:17} 30
gen-static: ArgumentCountError @9 C::s(): Argument #1 ($a) not passed
  #0 C::s 18
  #1 {closure:F:18} 30
gen-var: ArgumentCountError @12 gen(): Argument #1 ($a) not passed
  #0 gen 19
  #1 {closure:F:19} 30
gen-clos: ArgumentCountError @20 {closure:{closure:F:20}:20}(): Argument #1 ($a) not passed
  #0 {closure:{closure:F:20}:20} 20
  #1 {closure:F:20} 30
fiber-fn: ArgumentCountError @13 body(): Argument #1 ($a) not passed
  #0 body -
  #1 Fiber->start 21
  #2 {closure:F:21} 30
fiber-meth: ArgumentCountError @10 C::body(): Argument #1 ($a) not passed
  #0 C->body -
  #1 Fiber->start 22
  #2 {closure:F:22} 30
fiber-clos: ArgumentCountError @23 {closure:{closure:F:23}:23}(): Argument #1 ($a) not passed
  #0 {closure:{closure:F:23}:23} -
  #1 Fiber->start 23
  #2 {closure:F:23} 30
unknown: Error @25 Unknown named parameter $c
  #0 {closure:F:25} 30
fib-unk: Error @26 Unknown named parameter $c
  #0 Fiber->start 26
  #1 {closure:F:26} 30
