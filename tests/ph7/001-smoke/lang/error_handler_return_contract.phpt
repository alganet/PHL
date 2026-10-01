--TEST--
set_error_handler(): only a BOOLEAN false falls through to the engine reporter
--FILE--
<?php
// php falls through to its own reporter ONLY when the handler returns the
// BOOLEAN false. Every other answer -- a bare `return;`, no return at all,
// null, 0, 0.0, "", "0", [], an object, true -- means handled. This engine
// coerced the answer to bool, so seven of those fourteen printed anyway; the
// two commonest of them are `function () {}` and `return;`, which is the idiom
// a library uses to silence a call it expects to fail.
$ehqCases = [
    'void'      => static function () {},
    'null'      => static function () { return null; },
    'false'     => static function () { return false; },
    'true'      => static function () { return true; },
    'int0'      => static function () { return 0; },
    'int1'      => static function () { return 1; },
    'float0'    => static function () { return 0.0; },
    'str-empty' => static function () { return ""; },
    'str-zero'  => static function () { return "0"; },
    'str-x'     => static function () { return "x"; },
    'arr-empty' => static function () { return []; },
    'arr-one'   => static function () { return [1]; },
    'obj'       => static function () { return new stdClass(); },
];
foreach ($ehqCases as $ehqK => $ehqH) {
    set_error_handler($ehqH);
    $ehqU = @$ehqUndefinedProbe;   // '@' so only the HANDLER decision is under test
    restore_error_handler();
    echo $ehqK, " ";
}
echo "\n";
// The handler RUNS whatever it answers -- the answer only decides who PRINTS.
foreach (['silent' => static function () {}, 'fallthrough' => static function () { return false; }] as $ehqK2 => $ehqH2) {
    set_error_handler(static function ($n, $s) use ($ehqH2, $ehqK2) { echo "[ran:$ehqK2]"; return $ehqH2(); });
    $ehqU2 = @$ehqUndefined2;
    restore_error_handler();
}
echo "\n";
--EXPECT--
void null false true int0 int1 float0 str-empty str-zero str-x arr-empty arr-one obj 
[ran:silent][ran:fallthrough]
