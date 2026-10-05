--TEST--
An ArrayObject or ArrayIterator sort has a frame for the array function it runs
--FILE--
<?php
// php runs an ArrayObject / ArrayIterator sort as a real call to the array
// function of the same name, so that function has a frame of its own between
// the method and whatever the sort reaches for: a comparator, an exception it
// throws, an error handler. The frame has no file or line, and its args are the
// ones php passes: the array and the callback for uasort/uksort, the array and
// the flags for asort/ksort, the array alone for natsort/natcasesort.
function frames() {
    $out = [];
    foreach (debug_backtrace() as $f) {
        if ($f['function'] === 'frames') continue;
        $out[] = (isset($f['file']) ? 'line ' . $f['line'] : '[internal]') . ' '
            . (isset($f['class']) ? $f['class'] . $f['type'] : '')
            . str_replace(__FILE__, 'FILE', $f['function'])
            . ' args=' . (isset($f['args']) ? count($f['args']) : '-');
    }
    return $out;
}
foreach (['ArrayObject', 'ArrayIterator'] as $cls) {
    foreach (['uasort', 'uksort'] as $m) {
        $seen = null;
        (new $cls([3 => 'c', 1 => 'a', 2 => 'b']))->$m(function ($x, $y) use (&$seen) {
            $seen ??= frames();
            return $x <=> $y;
        });
        echo "$cls->$m\n  ", implode("\n  ", $seen), "\n";
    }
}
$cmp = function ($x, $y) { throw new LogicException('cmp'); };
foreach (['uasort', 'uksort'] as $m) {
    try {
        (new ArrayIterator([2, 1]))->$m($cmp);
    } catch (LogicException $e) {
        echo str_replace(__FILE__, 'FILE', $e->getTraceAsString()), "\n";
    }
}
set_error_handler(function ($n, $s) {
    echo "$s\n  ", implode("\n  ", array_slice(frames(), 1)), "\n";
    return true;
});
(new ArrayObject([['x'], 'a']))->asort(SORT_STRING);
(new ArrayIterator([['x'], 'a']))->natsort();
(new ArrayObject([['x'], 'a']))->natcasesort();
$trace = null;
set_error_handler(function () use (&$trace) { $trace ??= debug_backtrace(); return true; });
(new ArrayObject([['x'], 'a']))->asort(SORT_STRING);
restore_error_handler();
var_dump($trace[1]['function'], $trace[1]['args'][1], $trace[2]['args']);
--EXPECT--
ArrayObject->uasort
  [internal] {closure:FILE:22} args=2
  [internal] uasort args=2
  line 22 ArrayObject->uasort args=1
ArrayObject->uksort
  [internal] {closure:FILE:22} args=2
  [internal] uksort args=2
  line 22 ArrayObject->uksort args=1
ArrayIterator->uasort
  [internal] {closure:FILE:22} args=2
  [internal] uasort args=2
  line 22 ArrayIterator->uasort args=1
ArrayIterator->uksort
  [internal] {closure:FILE:22} args=2
  [internal] uksort args=2
  line 22 ArrayIterator->uksort args=1
#0 [internal function]: {closure:FILE:29}()
#1 [internal function]: uasort()
#2 FILE(32): ArrayIterator->uasort()
#3 {main}
#0 [internal function]: {closure:FILE:29}()
#1 [internal function]: uksort()
#2 FILE(32): ArrayIterator->uksort()
#3 {main}
Array to string conversion
  [internal] asort args=2
  line 41 ArrayObject->asort args=1
Array to string conversion
  [internal] natsort args=1
  line 42 ArrayIterator->natsort args=0
Array to string conversion
  [internal] natcasesort args=1
  line 43 ArrayObject->natcasesort args=0
string(5) "asort"
int(2)
array(1) {
  [0]=>
  int(2)
}
