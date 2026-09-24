--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
debug_backtrace()'s $limit stops the walk, and debug_print_backtrace() prints php's own "#N file(line): func(args)" frames
--FILE--
<?php
function dbl3($dbl_o, $dbl_l) { return debug_backtrace($dbl_o, $dbl_l); }
function dbl2($dbl_o, $dbl_l) { return dbl3($dbl_o, $dbl_l); }
function dbl1($dbl_o, $dbl_l) { return dbl2($dbl_o, $dbl_l); }
// Only this file's own frames are asserted: whatever ran the file sits under
// them and differs between a direct run and an include.
function dbl_shape(array $dbl_t) {
    $dbl_out = [];
    foreach ($dbl_t as $dbl_f) {
        $dbl_n = $dbl_f['function'] ?? '?';
        if (strncmp($dbl_n, 'dbl', 3) !== 0) { continue; }
        $dbl_out[] = $dbl_n
            . '/' . (isset($dbl_f['args']) ? 'args' : 'noargs')
            . '/' . (isset($dbl_f['object']) ? 'obj' : 'noobj');
    }
    return $dbl_out;
}

// $limit is the number of FRAMES to report, innermost first; 0 is all of them.
// It was declared, screened as an int and never read, so asking for the caller
// alone answered the whole stack.
foreach ([[0, 0], [0, 1], [0, 2], [0, 3], [0, 10]] as [$dbl_o, $dbl_l]) {
    echo json_encode(dbl_shape(dbl1($dbl_o, $dbl_l))), "\n";
}

// It composes with the $options bits rather than replacing them: bit 1 attaches
// the frame's $this, bit 2 (IGNORE_ARGS) drops the argument list.
foreach ([[1, 2], [2, 2], [3, 1], [DEBUG_BACKTRACE_IGNORE_ARGS, 2], [DEBUG_BACKTRACE_PROVIDE_OBJECT, 2]] as [$dbl_o, $dbl_l]) {
    echo json_encode(dbl_shape(dbl1($dbl_o, $dbl_l))), "\n";
}

// A limit no int can hold must not truncate into a small one.
echo count(dbl_shape(dbl1(0, PHP_INT_MAX))), count(dbl_shape(dbl1(0, 4294967296))), "\n";

// A frame's args are the ones the caller actually PASSED: never a defaulted
// parameter, and a variadic callee's extras spread rather than the packed array
// once per slot.
function dbl_def($dbl_x = null, $dbl_y = 2) { return debug_backtrace(0, 1)[0]['args']; }
function dbl_var(...$dbl_a) { return debug_backtrace(0, 1)[0]['args']; }
var_dump(dbl_def(), dbl_def(1), dbl_var(1, 2), dbl_var());

// A trace ARGUMENT is rendered by kind, not by value: php 8.5 keeps string
// CONTENT out of a trace entirely (only the empty one is distinguishable), and
// a float always shows its fraction so it cannot be read as an int.
function dbl_arg($dbl_v) { debug_print_backtrace(); }
foreach (['', 'a', 'a longer one', 1.0, -0.0, 2.5, 1e100, INF, NAN, 7, true, false, null, [1], new stdClass] as $dbl_v) {
    ob_start();
    dbl_arg($dbl_v);
    $dbl_l = explode("\n", trim(str_replace(__FILE__, 'FILE', ob_get_clean())));
    echo $dbl_l[0], "\n";
}

// The separator says what the CALLEE is, not how the caller reached it: a
// static method is "::" even when the calling frame has a $this bound.
class DblK {
    public static function s() { return debug_backtrace(1, 1)[0]; }
    public function m() { return debug_backtrace(1, 1)[0]; }
    public function callsStatic() { return self::s(); }
}
foreach ([DblK::s(), (new DblK)->m(), (new DblK)->callsStatic()] as $dbl_f) {
    echo $dbl_f['class'], $dbl_f['type'], $dbl_f['function'],
        '/', isset($dbl_f['object']) ? 'obj' : 'noobj', "\n";
}

// debug_print_backtrace() prints php's frames -- "#N file(line): func(args)",
// with no "#N {main}" marker (that is the bottom of an exception's trace, not a
// frame) -- and honours the same two arguments.
function dbp3($dbp_o, $dbp_l) { debug_print_backtrace($dbp_o, $dbp_l); }
function dbp2($dbp_o, $dbp_l) { dbp3($dbp_o, $dbp_l); }
function dbp1($dbp_o, $dbp_l) { dbp2($dbp_o, $dbp_l); }
foreach ([[0, 0], [0, 1], [0, 2], [1, 2], [2, 0]] as [$dbp_o, $dbp_l]) {
    echo "-- options=$dbp_o limit=$dbp_l\n";
    ob_start();
    dbp1($dbp_o, $dbp_l);
    // The file path is the runner's; only its basename is stable.
    $dbp_lines = explode("\n", trim(str_replace(__FILE__, 'FILE', ob_get_clean())));
    foreach ($dbp_lines as $dbp_line) {
        if (strpos($dbp_line, ': dbp') !== false) { echo $dbp_line, "\n"; }
    }
}
?>
--EXPECT--
["dbl3\/args\/noobj","dbl2\/args\/noobj","dbl1\/args\/noobj"]
["dbl3\/args\/noobj"]
["dbl3\/args\/noobj","dbl2\/args\/noobj"]
["dbl3\/args\/noobj","dbl2\/args\/noobj","dbl1\/args\/noobj"]
["dbl3\/args\/noobj","dbl2\/args\/noobj","dbl1\/args\/noobj"]
["dbl3\/args\/noobj","dbl2\/args\/noobj"]
["dbl3\/noargs\/noobj","dbl2\/noargs\/noobj"]
["dbl3\/noargs\/noobj"]
["dbl3\/noargs\/noobj","dbl2\/noargs\/noobj"]
["dbl3\/args\/noobj","dbl2\/args\/noobj"]
03
array(0) {
}
array(1) {
  [0]=>
  int(1)
}
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(2)
}
array(0) {
}
#0 FILE(48): dbl_arg('')
#0 FILE(48): dbl_arg('...')
#0 FILE(48): dbl_arg('...')
#0 FILE(48): dbl_arg(1.0)
#0 FILE(48): dbl_arg(-0.0)
#0 FILE(48): dbl_arg(2.5)
#0 FILE(48): dbl_arg(1.0E+100)
#0 FILE(48): dbl_arg(INF)
#0 FILE(48): dbl_arg(NAN)
#0 FILE(48): dbl_arg(7)
#0 FILE(48): dbl_arg(true)
#0 FILE(48): dbl_arg(false)
#0 FILE(48): dbl_arg(NULL)
#0 FILE(48): dbl_arg(Array)
#0 FILE(48): dbl_arg(Object(stdClass))
DblK::s/noobj
DblK->m/obj
DblK::s/noobj
-- options=0 limit=0
#0 FILE(69): dbp3(0, 0)
#1 FILE(70): dbp2(0, 0)
#2 FILE(74): dbp1(0, 0)
-- options=0 limit=1
#0 FILE(69): dbp3(0, 1)
-- options=0 limit=2
#0 FILE(69): dbp3(0, 2)
#1 FILE(70): dbp2(0, 2)
-- options=1 limit=2
#0 FILE(69): dbp3(1, 2)
#1 FILE(70): dbp2(1, 2)
-- options=2 limit=0
#0 FILE(69): dbp3()
#1 FILE(70): dbp2()
#2 FILE(74): dbp1()
