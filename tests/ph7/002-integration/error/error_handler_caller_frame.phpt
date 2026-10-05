--TEST--
An error handler is called from the frame that raised the diagnostic, which is userland unless a builtin with a frame of its own raised it
--FILE--
<?php
declare(strict_types=1);
/* php calls the handler from the running frame. An opcode's own diagnostic, and
 * one raised by a call php's compiler folds into opcodes (sprintf() over a
 * literal format of nothing but %s/%d/%%, strval(), intval(), floatval()),
 * have no internal frame: the handler's frame carries the call site, no
 * builtin sits above it, its arguments bind in the calling file's strict mode,
 * and a too-few error says where they were "passed in". A builtin that keeps
 * its frame (a %5s format, a count mismatch) still calls it as a callback. */
function frames() {
    $o = [];
    foreach (debug_backtrace() as $f) {
        $n = str_starts_with($f['function'], '{closure') ? '{closure}' : $f['function'];
        $o[] = (isset($f['class']) ? $f['class'] . $f['type'] : '') . $n . '@' . ($f['line'] ?? '-');
    }
    echo '  ', implode(' < ', $o), "\n";
}
class S { function __toString(): string { frames(); return 's'; } }
function run($label, $f) {
    echo $label, "\n";
    try { $f(); } catch (Throwable $e) { echo '  ', get_class($e), ': ', str_replace(__FILE__, 'FILE', $e->getMessage()), "\n"; }
}
set_error_handler(function ($no, $str) { frames(); echo "  $str\n"; return true; });
run('opcode', function () { $x = [] . ''; });
run('opcode in a callback', function () { array_map(function ($v) { $x = [] . ''; }, [1]); });
run('sprintf folded', function () { sprintf('%s-%d', [], 1); });
run('sprintf fully qualified', function () { \sprintf('100%% %s', []); });
run('sprintf width', function () { sprintf('%5s', []); });
run('sprintf count mismatch', function () { sprintf('%s', [], 2); });
run('sprintf spread', function () { sprintf('%s', ...[[]]); });
run('%d of a float', function () { sprintf('%d', NAN); });
run('strval', function () { strval([]); });
run('intval', function () { intval(new stdClass); });
run('floatval', function () { floatval(new stdClass); });
run('implode', function () { implode(',', [[1]]); });
run('__toString from strval', function () { strval(new S); });
run('__toString from sprintf', function () { sprintf('%s', new S); });
run('__toString from sprintf width', function () { sprintf('%5s', new S); });

set_error_handler(function (string $no, $str) { echo "  bound\n"; return true; });
run('strict opcode', function () { $x = [] . ''; });
run('strict folded', function () { sprintf('%s', []); });
run('strict builtin', function () { sprintf('%5s', []); });
set_error_handler(function ($a, $b, $c, $d, $e, $f) { return true; });
run('few folded', function () { strval([]); });
run('few builtin', function () { sprintf('%5s', []); });
--EXPECT--
opcode
  frames@23 < {closure}@24 < {closure}@21 < run@24
  Array to string conversion
opcode in a callback
  frames@23 < {closure}@25 < {closure}@- < array_map@25 < {closure}@21 < run@25
  Array to string conversion
sprintf folded
  frames@23 < {closure}@26 < {closure}@21 < run@26
  Array to string conversion
sprintf fully qualified
  frames@23 < {closure}@27 < {closure}@21 < run@27
  Array to string conversion
sprintf width
  frames@23 < {closure}@- < sprintf@28 < {closure}@21 < run@28
  Array to string conversion
sprintf count mismatch
  frames@23 < {closure}@- < sprintf@29 < {closure}@21 < run@29
  Array to string conversion
sprintf spread
  frames@23 < {closure}@- < sprintf@30 < {closure}@21 < run@30
  Array to string conversion
%d of a float
  frames@23 < {closure}@31 < {closure}@21 < run@31
  The float NAN is not representable as an int, cast occurred
strval
  frames@23 < {closure}@32 < {closure}@21 < run@32
  Array to string conversion
intval
  frames@23 < {closure}@33 < {closure}@21 < run@33
  Object of class stdClass could not be converted to int
floatval
  frames@23 < {closure}@34 < {closure}@21 < run@34
  Object of class stdClass could not be converted to float
implode
  frames@23 < {closure}@- < implode@35 < {closure}@21 < run@35
  Array to string conversion
__toString from strval
  frames@18 < S->__toString@36 < {closure}@21 < run@36
__toString from sprintf
  frames@18 < S->__toString@37 < {closure}@21 < run@37
__toString from sprintf width
  frames@18 < S->__toString@- < sprintf@38 < {closure}@21 < run@38
strict opcode
  TypeError: {closure:FILE:40}(): Argument #1 ($no) must be of type string, int given, called in FILE on line 41
strict folded
  TypeError: {closure:FILE:40}(): Argument #1 ($no) must be of type string, int given, called in FILE on line 42
strict builtin
  bound
few folded
  ArgumentCountError: Too few arguments to function {closure:FILE:44}(), 4 passed in FILE on line 45 and exactly 6 expected
few builtin
  ArgumentCountError: Too few arguments to function {closure:FILE:44}(), 4 passed and exactly 6 expected
