--TEST--
A builtin written as embedded PHP raises its diagnostics at the engine level, from its own frame
--FILE--
<?php
/* hex2bin(), array_count_values() and tempnam() raise their diagnostics at
 * php's ENGINE levels -- E_WARNING and E_NOTICE, not the E_USER_* ones -- and
 * from their own internal frame, with no trigger_error() frame above it. The
 * level is what a handler receives, what error_reporting() masks and what
 * error_get_last() and an ErrorException's severity report. */
error_reporting(E_ALL);
set_error_handler(function ($no, $str, $file, $line) {
    echo "handler: $no $str ", ($file === __FILE__ ? "self" : $file), ":$line\n";
    foreach (debug_backtrace() as $f) {
        echo "  ", str_starts_with($f["function"], "{closure") ? "{closure}" : $f["function"], " ", $f["line"] ?? "-", "\n";
    }
    return true;
});
var_dump(hex2bin('abc'));
var_dump(hex2bin('zz'));
var_dump(array_count_values([1, 1.5, 'a']));
$d = sys_get_temp_dir() . '/phl-no-such-dir-' . getmypid();
$f = tempnam($d, 'x');
var_dump(is_string($f));
@unlink($f);
restore_error_handler();

error_clear_last();
var_dump(@hex2bin('q1'));
var_dump(error_get_last()['type'], error_get_last()['line']);

set_error_handler(function ($no, $str) { echo "handler: $no $str\n"; return true; }, E_USER_WARNING);
error_reporting(E_ALL & ~E_WARNING);
var_dump(hex2bin('abc'));
error_reporting(E_ALL);
restore_error_handler();

set_error_handler(function ($no, $str, $file, $line) {
    throw new ErrorException($str, 0, $no, $file, $line);
});
try {
    hex2bin('abc');
} catch (ErrorException $e) {
    echo get_class($e), " severity=", $e->getSeverity(), " line=", $e->getLine(), "\n";
    foreach ($e->getTrace() as $t) {
        echo "  ", str_starts_with($t["function"], "{closure") ? "{closure}" : $t["function"], " ", $t["line"] ?? "-", "\n";
    }
}
--EXPECT--
handler: 2 hex2bin(): Hexadecimal input string must have an even length self:15
  {closure} -
  hex2bin 15
bool(false)
handler: 2 hex2bin(): Input string must be hexadecimal string self:16
  {closure} -
  hex2bin 16
bool(false)
handler: 2 array_count_values(): Can only count string and integer values, entry skipped self:17
  {closure} -
  array_count_values 17
array(2) {
  [1]=>
  int(1)
  ["a"]=>
  int(1)
}
handler: 8 tempnam(): file created in the system's temporary directory self:19
  {closure} -
  tempnam 19
bool(true)
bool(false)
int(2)
int(25)
bool(false)
ErrorException severity=2 line=38
  {closure} -
  hex2bin 38
