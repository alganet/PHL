--TEST--
A generator's trace frames name what resumed it
--FILE--
<?php
// A generator's body runs under whatever resumed it. A Generator method called by
// user code (current, send, next, throw, key, valid) is an internal frame there, so
// the body's own frame has no file or line and the method has a frame of its own
// at the caller's line -- and so does an internal function that walks it
// (iterator_to_array). The engine's OWN iteration makes no call at all: foreach,
// an IteratorAggregate's generator, a spread, `yield from`, IteratorIterator's
// inner walk and the close of an abandoned generator leave no Generator frame.
function frames(array $t) {
    $out = [];
    foreach ($t as $fr) {
        $out[] = (isset($fr['file']) ? 'line ' . $fr['line'] : '[internal]') . ' '
            . (isset($fr['class']) ? $fr['class'] . $fr['type'] : '') . str_replace(__FILE__, 'FILE', $fr['function']);
    }
    return implode(' | ', $out);
}
function tr() { return frames(array_slice((new Exception)->getTrace(), 1)); }
function bt() { return frames(array_slice(debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS), 1)); }
function g() {
    $x = yield tr();
    echo "send: ", tr(), "\n";
    yield 2;
    echo "next: ", tr(), "\n";
    try { yield 3; } catch (Exception $e) { echo "throw: ", tr(), "\n"; }
    yield 4;
    echo "key: ", bt(), "\n";
    yield 5 => 5;
    try { yield 6; } finally { echo "close: ", tr(), "\n"; }
}
$g = g();
echo "current: ", $g->current(), "\n";
$g->send(1);
$g->next();
$g->throw(new Exception);
$g->next();
$g->key();
$g->next();
unset($g);
function h() { yield tr(); yield bt(); }
foreach (h() as $v) echo "foreach: $v\n";
class Agg implements IteratorAggregate { function getIterator(): Generator { yield tr(); } }
foreach (new Agg as $v) echo "aggregate: $v\n";
echo "spread: ", implode(' // ', [...h()]), "\n";
echo "iterator_to_array: ", implode(' // ', iterator_to_array(h())), "\n";
array_map(function ($x) { foreach (h() as $v) echo "foreach in callback: $v\n"; }, [1]);
array_map(function ($x) { echo "current in callback: ", h()->current(), "\n"; }, [1]);
function vg() { echo "valid: ", tr(), "\n"; yield 1; }
vg()->valid();
$it = new IteratorIterator(h());
$it->rewind();
echo "IteratorIterator: ", $it->current(), "\n";
function w() { yield from h(); }
$g = w();
echo "yield from, current: ", $g->current(), "\n";
foreach (w() as $v) echo "yield from, foreach: $v\n";
function u() { yield 1; throw new LogicException('u'); }
foreach ([function () { $g = u(); $g->current(); $g->next(); },
          function () { foreach (u() as $v) {} }] as $c) {
    try { $c(); } catch (Exception $e) { echo get_class($e), ": ", frames($e->getTrace()), "\n"; }
}
$g = g();
$g->next();
try { foreach ($g as $v) {} } catch (Exception $e) {
    echo $e->getMessage(), ": ", frames($e->getTrace()), "\n";
}
--EXPECT--
current: [internal] g | line 31 Generator->current
send: [internal] g | line 32 Generator->send
next: [internal] g | line 33 Generator->next
throw: [internal] g | line 34 Generator->throw
key: [internal] g | line 35 Generator->next
close: line 38 g
foreach: line 40 h
foreach: line 40 h
aggregate: line 42 Agg->getIterator
spread: line 43 h // line 43 h
iterator_to_array: [internal] h | line 44 iterator_to_array // [internal] h | line 44 iterator_to_array
foreach in callback: line 45 h | [internal] {closure:FILE:45} | line 45 array_map
foreach in callback: line 45 h | [internal] {closure:FILE:45} | line 45 array_map
current in callback: [internal] h | line 46 Generator->current | [internal] {closure:FILE:46} | line 46 array_map
valid: [internal] vg | line 48 Generator->valid
IteratorIterator: [internal] h | line 50 IteratorIterator->rewind
yield from, current: line 52 h | [internal] w | line 54 Generator->current
yield from, foreach: line 52 h | line 55 w
yield from, foreach: line 52 h | line 55 w
LogicException: [internal] u | line 57 Generator->next | line 59 {closure:FILE:57}
LogicException: line 58 u | line 59 {closure:FILE:58}
send: [internal] g | line 62 Generator->next
Cannot rewind a generator that was already run: 
