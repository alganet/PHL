--TEST--
A magic method an internal function runs implicitly has that function's frame under it
--FILE--
<?php
// When an internal function reaches for a magic method on its own -- asort()
// casting an element with SORT_STRING, serialize() asking for __serialize --
// php traces the method with no file or line and gives the function a frame of
// its own at the call site, exactly as it does for a callback it was handed.
// The same method run from userland bytecode (echo, a string cast, strlen(),
// which php compiles into an opcode) keeps the caller's line, and so does one
// reached from a sprintf() php's compiler rewrites into concatenation: a
// literal format of only %s, %d and %% with one argument per placeholder.
function frames() {
    $out = [];
    foreach (debug_backtrace() as $f) {
        if ($f['function'] === 'frames') continue;
        $out[] = (isset($f['file']) ? 'line ' . $f['line'] : '[internal]') . ' '
            . (isset($f['class']) ? $f['class'] . $f['type'] : '') . $f['function'];
    }
    return implode(' < ', $out);
}
class S {
    public static $log = [];
    function __toString(): string { self::$log[] = frames(); return 'x'; }
    function __serialize(): array { self::$log[] = frames(); return []; }
    function __unserialize(array $a): void { self::$log[] = frames(); }
}
class Boom { function __toString(): string { throw new Exception('boom'); } }
function show($what, $fn) {
    S::$log = [];
    $fn();
    echo $what, "\n  ", implode("\n  ", array_unique(S::$log)), "\n";
}
function run() {
    $o = new S;
    $y = 'y';
    $fmt = '%s';
    show('asort', function () use ($o) { $a = [$o, $o]; asort($a, SORT_STRING); });
    show('implode', function () use ($o) { implode(',', [$o]); });
    show('str_repeat', function () use ($o) { str_repeat($o, 1); });
    show('in_array', function () use ($o) { in_array('x', [$o]); });
    show('serialize', function () use ($o) { serialize($o); });
    show('unserialize', function () { unserialize('O:1:"S":0:{}'); });
    show('ArrayObject->asort', function () use ($o) { (new ArrayObject([$o, $o]))->asort(SORT_STRING); });
    show('array_map strval', function () use ($o) { array_map('strval', [$o]); });
    show('echo', function () use ($o) { ob_start(); echo $o; ob_end_clean(); });
    show('cast', function () use ($o) { (string)$o; });
    show('strlen', function () use ($o) { strlen($o); });
    show('sprintf folded', function () use ($o) { sprintf('%s', $o); });
    show('sprintf folded, double quotes', function () use ($o) { sprintf("%s\n", $o); });
    show('sprintf interpolated format', function () use ($o, $y) { sprintf("%s-$y", $o); });
    show('sprintf variable format', function () use ($o, $fmt) { sprintf($fmt, $o); });
    show('sprintf width', function () use ($o) { sprintf('%5s', $o); });
    show('sprintf extra argument', function () use ($o) { sprintf('%s', $o, 1); });
    show('vsprintf', function () use ($o) { vsprintf('%s', [$o]); });
    try {
        implode(',', [new Boom]);
    } catch (Exception $e) {
        $t = [];
        foreach ($e->getTrace() as $f) {
            $t[] = (isset($f['file']) ? 'line ' . $f['line'] : '[internal]') . ' '
                . (isset($f['class']) ? $f['class'] . $f['type'] : '') . $f['function'];
        }
        echo "thrown\n  ", implode(' < ', $t), "\n";
    }
}
run();
--EXPECT--
asort
  [internal] S->__toString < line 35 asort < line 28 {closure:run():35} < line 35 show < line 64 run
implode
  [internal] S->__toString < line 36 implode < line 28 {closure:run():36} < line 36 show < line 64 run
str_repeat
  [internal] S->__toString < line 37 str_repeat < line 28 {closure:run():37} < line 37 show < line 64 run
in_array
  [internal] S->__toString < line 38 in_array < line 28 {closure:run():38} < line 38 show < line 64 run
serialize
  [internal] S->__serialize < line 39 serialize < line 28 {closure:run():39} < line 39 show < line 64 run
unserialize
  [internal] S->__unserialize < line 40 unserialize < line 28 {closure:run():40} < line 40 show < line 64 run
ArrayObject->asort
  [internal] S->__toString < [internal] asort < line 41 ArrayObject->asort < line 28 {closure:run():41} < line 41 show < line 64 run
array_map strval
  [internal] S->__toString < [internal] strval < line 42 array_map < line 28 {closure:run():42} < line 42 show < line 64 run
echo
  line 43 S->__toString < line 28 {closure:run():43} < line 43 show < line 64 run
cast
  line 44 S->__toString < line 28 {closure:run():44} < line 44 show < line 64 run
strlen
  line 45 S->__toString < line 28 {closure:run():45} < line 45 show < line 64 run
sprintf folded
  line 46 S->__toString < line 28 {closure:run():46} < line 46 show < line 64 run
sprintf folded, double quotes
  line 47 S->__toString < line 28 {closure:run():47} < line 47 show < line 64 run
sprintf interpolated format
  [internal] S->__toString < line 48 sprintf < line 28 {closure:run():48} < line 48 show < line 64 run
sprintf variable format
  [internal] S->__toString < line 49 sprintf < line 28 {closure:run():49} < line 49 show < line 64 run
sprintf width
  [internal] S->__toString < line 50 sprintf < line 28 {closure:run():50} < line 50 show < line 64 run
sprintf extra argument
  [internal] S->__toString < line 51 sprintf < line 28 {closure:run():51} < line 51 show < line 64 run
vsprintf
  [internal] S->__toString < line 52 vsprintf < line 28 {closure:run():52} < line 52 show < line 64 run
thrown
  [internal] Boom->__toString < line 54 implode < line 64 run
