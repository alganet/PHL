--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A STRING container applies php's own offset rules: a non-integer offset throws, an integer-then-garbage one warns, a lookup stays silent (PHL cast every offset, so $s["p"] answered $s[0])
--FILE--
<?php
function attempt($label, callable $fn) {
    echo str_pad($label, 22);
    try {
        $line = 'ok ' . var_export($fn(), true);
    } catch (Error $e) {
        $line = get_class($e) . ': ' . $e->getMessage();
    }
    echo $line, "\n";
}

$obj = new stdClass();
$arr = [1, 2];
$res = fopen('php://memory', 'r');

// A string offset php refuses is a TypeError naming the OFFSET's type -- and, for
// a string offset, php's is_numeric_string verdict is what decides: only IS_LONG
// is an offset, so every float-shaped or 64-bit-overflowing string is refused too.
attempt('read empty',   function () { $s = "abcdef"; return $s[""]; });
attempt('read dash',    function () { $s = "abcdef"; return $s["-"]; });
attempt('read alpha',   function () { $s = "abcdef"; return $s["p"]; });
attempt('read spaces',  function () { $s = "abcdef"; return $s["  "]; });
attempt('read float-s', function () { $s = "abcdef"; return $s["1.5"]; });
attempt('read dot-s',   function () { $s = "abcdef"; return $s["1."]; });
attempt('read exp-s',   function () { $s = "abcdef"; return $s["1e2"]; });
attempt('read overflow',function () { $s = "abcdef"; return $s["9223372036854775808"]; });
attempt('read array',   function () use ($arr) { $s = "abcdef"; return $s[$arr]; });
attempt('read object',  function () use ($obj) { $s = "abcdef"; return $s[$obj]; });
attempt('read resource',function () use ($res) { $s = "abcdef"; return $s[$res]; });
attempt('write alpha',  function () { $s = "abcdef"; $s["p"] = "X"; return $s; });
attempt('write arr key',function () use ($arr) { $s = "abcdef"; $s[$arr] = "X"; return $s; });
// The offset is judged BEFORE the right-hand side is cast, so a rejected offset is
// the TypeError alone -- no "Array to string conversion" in front of it.
attempt('write arr val',function () use ($arr) { $s = "abcdef"; $s["p"] = $arr; return $s; });
attempt('append array', function () use ($arr) { $s = "abcdef"; $s[] = $arr; return $s; });
// An offset that IS an integer, whatever it is written as.
attempt('int-ish str',  function () { $s = "abcdef"; return $s[" 2 "]; });
attempt('signed str',   function () { $s = "abcdef"; return $s["+1"]; });
attempt('padded str',   function () { $s = "abcdef"; return $s["003"]; });
// A LOOKUP raises nothing at all and answers "not set" for every refused shape --
// isset("0x1") is false even though READING it warns and yields $s[0].
attempt('isset alpha',  function () { $s = "abcdef"; return isset($s["p"]); });
attempt('isset empty',  function () { $s = "abcdef"; return isset($s[""]); });
attempt('isset array',  function () use ($arr) { $s = "abcdef"; return isset($s[$arr]); });
attempt('isset hex',    function () { $s = "abcdef"; return isset($s["0x1"]); });
attempt('isset int-ish',function () { $s = "abcdef"; return isset($s[" 1 "]); });
attempt('isset float',  function () { $s = "abcdef"; return isset($s[1.5]); });
attempt('empty alpha',  function () { $s = "abcdef"; return empty($s["p"]); });
attempt('coalesce miss',function () { $s = "abcdef"; return $s["p"] ?? "dflt"; });
attempt('coalesce hit', function () { $s = "abcdef"; return $s["1"] ?? "dflt"; });
// A by-ref argument over a string offset is php's reference Error -- but only once
// the offset itself passes the same rules.
attempt('byref bad',    function () { $s = "abcdef"; $f = function (&$x) {}; $f($s["p"]); return $s; });
attempt('byref ok',     function () { $s = "abcdef"; $f = function (&$x) {}; $f($s[1]); return $s; });
// A rejected offset abandons the rest of the expression, as php abandons it: the
// builtin below is never reached with a null argument.
attempt('mid-expr',     function () { $s = "abcdef"; return str_repeat($s["p"], 2); });

// The two shapes php ACCEPTS with a diagnostic. Captured rather than printed so the
// assertions match the message BODY (php's log copy prefixes "PHP ").
$warn = [];
set_error_handler(function ($no, $msg) use (&$warn) { $warn[] = $msg; return true; });
$s = "abcdef";
$hex    = $s["0x1"];
$trail  = $s["5abc"];
$float  = $s[1.5];
$null   = $s[null];
$bool   = $s[true];
$minInt = $s["-9223372036854775808"];
$w = "abcdef";
$w[" 2 "]  = "!";
$w["3abc"] = "?";
$w[null]   = "^";
restore_error_handler();
fclose($res);

echo 'hex read:    ', var_export($hex, true), "\n";
echo 'trail read:  ', var_export($trail, true), "\n";
echo 'float read:  ', var_export($float, true), "\n";
echo 'null read:   ', var_export($null, true), "\n";
echo 'bool read:   ', var_export($bool, true), "\n";
echo 'min-int read:', var_export($minInt, true), "\n";
echo 'written:     ', $w, "\n";
echo 'warnings:', "\n  ", implode("\n  ", $warn), "\n";
?>
--EXPECT--
read empty            TypeError: Cannot access offset of type string on string
read dash             TypeError: Cannot access offset of type string on string
read alpha            TypeError: Cannot access offset of type string on string
read spaces           TypeError: Cannot access offset of type string on string
read float-s          TypeError: Cannot access offset of type string on string
read dot-s            TypeError: Cannot access offset of type string on string
read exp-s            TypeError: Cannot access offset of type string on string
read overflow         TypeError: Cannot access offset of type string on string
read array            TypeError: Cannot access offset of type array on string
read object           TypeError: Cannot access offset of type stdClass on string
read resource         TypeError: Cannot access offset of type resource on string
write alpha           TypeError: Cannot access offset of type string on string
write arr key         TypeError: Cannot access offset of type array on string
write arr val         TypeError: Cannot access offset of type string on string
append array          Error: [] operator not supported for strings
int-ish str           ok 'c'
signed str            ok 'b'
padded str            ok 'd'
isset alpha           ok false
isset empty           ok false
isset array           ok false
isset hex             ok false
isset int-ish         ok true
isset float           ok true
empty alpha           ok true
coalesce miss         ok 'dflt'
coalesce hit          ok 'b'
byref bad             TypeError: Cannot access offset of type string on string
byref ok              Error: Cannot create references to/from string offsets
mid-expr              TypeError: Cannot access offset of type string on string
hex read:    'a'
trail read:  'f'
float read:  'b'
null read:   'a'
bool read:   'b'
min-int read:''
written:     ^b!?ef
warnings:
  Illegal string offset "0x1"
  Illegal string offset "5abc"
  String offset cast occurred
  String offset cast occurred
  String offset cast occurred
  Uninitialized string offset -9223372036854775808
  Illegal string offset "3abc"
  String offset cast occurred
