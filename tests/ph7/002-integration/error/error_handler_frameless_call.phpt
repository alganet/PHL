--TEST--
A frameless builtin calls the error handler and an autoloader from the user frame: its trace frame stays, the callee binds in the caller's strict mode and names the call site
--FILE--
<?php
declare(strict_types=1);
namespace N;
/* php 8.4 compiles a direct call of a few builtins (implode, str_replace, trim,
 * class_exists, ...) at the arities their stubs list into a FRAMELESS call: no
 * frame is pushed, so what the builtin calls back -- the error handler, an
 * autoloader -- is called from the user frame. The builtin keeps its trace
 * frame, but the callee binds in the calling file's strict mode and its
 * diagnostics name that call site. A dynamic name, a spread, a named argument,
 * a first-class callable or another arity is an ordinary internal call. */
function fr() {
    $o = [];
    foreach (debug_backtrace() as $f) {
        $n = str_starts_with($f['function'], '{closure') ? '{closure}' : $f['function'];
        $o[] = (isset($f['class']) ? $f['class'] . $f['type'] : '') . $n . '@' . ($f['line'] ?? '-') . '#' . count($f['args'] ?? []);
    }
    echo '  ', \implode(' < ', $o), "\n";
}
function run($l, $f) {
    echo $l, "\n";
    try { $f(); } catch (\Throwable $e) { echo '  ', get_class($e), ': ', \str_replace(__FILE__, 'F', $e->getMessage()), "\n"; }
}
set_error_handler(function ($no, $str) { fr(); echo "  $str\n"; return true; });
run('trace: implode', function () { implode(',', [[1]]); });
run('trace: dynamic implode', function () { $f = 'implode'; $f(',', [[1]]); });
run('trace: preg_match', function () { \preg_match('/(/', 'x'); });
set_error_handler(function (string $no, string $str) { echo "  typed $str\n"; return true; });
run('implode', function () { implode(',', [[1]]); });
run('implode, one argument', function () { implode([[1]]); });
run('\\implode', function () { \implode(',', [[1]]); });
run('str_replace', function () { str_replace('a', 'b', [[1]]); });
run('strtr', function () { strtr('abc', [[1]]); });
run('preg_match', function () { preg_match('/(/', 'x'); });
run('preg_replace', function () { preg_replace('/(/', 'x', 'y'); });
run('in a ternary', function () { $x = true ? implode(',', [[1]]) : 1; });
run('in an arrow fn', function () { $g = fn() => implode(',', [[1]]); $g(); });
run('dynamic', function () { $f = 'implode'; $f(',', [[1]]); });
run('spread', function () { implode(',', ...[[[1]]]); });
run('named', function () { implode(separator: ',', array: [[1]]); });
run('first-class callable', function () { $f = implode(...); $f(',', [[1]]); });
run('preg_match, three arguments', function () { preg_match('/(/', 'x', $m); });
run('array_map is no frameless call', function () { array_map('implode', [[',', [[1]]]]); });
set_error_handler(function ($no, $str, $a, $b, $c) { echo "  five $str\n"; return true; });
run('too few, implode', function () { implode(',', [[1]]); });
run('too few, dynamic', function () { $f = 'implode'; $f(',', [[1]]); });
restore_error_handler();
restore_error_handler();
spl_autoload_register(function (int $c) { echo "  loader $c\n"; });
run('class_exists', function () { class_exists('Zz'); });
run('class_exists, two arguments', function () { class_exists('Zz', true); });
run('\\class_exists', function () { \class_exists('Zz'); });
run('class_exists, dynamic', function () { $f = 'class_exists'; $f('Zz'); });
--EXPECT--
trace: implode
  N\fr@23#0 < {closure}@-#4 < implode@24#2 < {closure}@21#0 < N\run@24#2
  Array to string conversion
trace: dynamic implode
  N\fr@23#0 < {closure}@-#4 < implode@25#2 < {closure}@21#0 < N\run@25#2
  Array to string conversion
trace: preg_match
  N\fr@23#0 < {closure}@-#4 < preg_match@26#2 < {closure}@21#0 < N\run@26#2
  preg_match(): Compilation failed: missing closing parenthesis at offset 1
implode
  TypeError: {closure:F:27}(): Argument #1 ($no) must be of type string, int given, called in F on line 28
implode, one argument
  TypeError: {closure:F:27}(): Argument #1 ($no) must be of type string, int given, called in F on line 29
\implode
  TypeError: {closure:F:27}(): Argument #1 ($no) must be of type string, int given, called in F on line 30
str_replace
  TypeError: {closure:F:27}(): Argument #1 ($no) must be of type string, int given, called in F on line 31
strtr
  TypeError: {closure:F:27}(): Argument #1 ($no) must be of type string, int given, called in F on line 32
preg_match
  TypeError: {closure:F:27}(): Argument #1 ($no) must be of type string, int given, called in F on line 33
preg_replace
  TypeError: {closure:F:27}(): Argument #1 ($no) must be of type string, int given, called in F on line 34
in a ternary
  TypeError: {closure:F:27}(): Argument #1 ($no) must be of type string, int given, called in F on line 35
in an arrow fn
  TypeError: {closure:F:27}(): Argument #1 ($no) must be of type string, int given, called in F on line 36
dynamic
  typed Array to string conversion
spread
  typed Array to string conversion
named
  typed Array to string conversion
first-class callable
  typed Array to string conversion
preg_match, three arguments
  typed preg_match(): Compilation failed: missing closing parenthesis at offset 1
array_map is no frameless call
  typed Array to string conversion
too few, implode
  ArgumentCountError: Too few arguments to function {closure:F:43}(), 4 passed in F on line 44 and exactly 5 expected
too few, dynamic
  ArgumentCountError: Too few arguments to function {closure:F:43}(), 4 passed and exactly 5 expected
class_exists
  TypeError: {closure:F:48}(): Argument #1 ($c) must be of type int, string given, called in F on line 49
class_exists, two arguments
  TypeError: {closure:F:48}(): Argument #1 ($c) must be of type int, string given, called in F on line 50
\class_exists
  TypeError: {closure:F:48}(): Argument #1 ($c) must be of type int, string given, called in F on line 51
class_exists, dynamic
  TypeError: {closure:F:48}(): Argument #1 ($c) must be of type int, string given
