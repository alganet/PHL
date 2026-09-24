--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: a LOSSY float-string operand and a lossy typed int store deprecate and truncate (zend half)
--DESCRIPTION--
php 8.1 DEPRECATES an implicit float->int conversion that loses precision and performs it
anyway, in two spellings: `Implicit conversion from float 1.9 to int loses precision` and
`... from float-string "1.9" to int ...`. PHL targets php's NON-deprecated surface (§10) and
rejects it. It already did so for a real FLOAT operand of the integer-only operators and for a
typed int parameter and return; the two sites this pins had been left out, so the same event
was refused in one place and silently truncated in another. php's half is the `_zend` twin.
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
class LossyInt { public int $i = 0; }
set_error_handler(function ($no, $msg) { echo '  D: ', $msg, "\n"; return true; });
function lossy_show(string $label, callable $fn): void {
    try {
        $out = var_export($fn(), true);
    } catch (\Throwable $e) {
        $out = get_class($e) . ': ' . $e->getMessage();
    }
    echo $label, ' => ', $out, "\n";
}
echo "## the seven integer-only operators, with a float-string operand\n";
lossy_show('"1.9" % 2', fn() => "1.9" % 2);
lossy_show('2 % "1.9"', fn() => 2 % "1.9");
lossy_show('"1.5" | 1', fn() => "1.5" | 1);
lossy_show('"1.5" & 3', fn() => "1.5" & 3);
lossy_show('"1.5" ^ 3', fn() => "1.5" ^ 3);
lossy_show('"1.5" << 1', fn() => "1.5" << 1);
lossy_show('"1.5" >> 1', fn() => "1.5" >> 1);
lossy_show('" 1.5 " % 2', fn() => " 1.5 " % 2);
lossy_show('"1.5abc" % 2', fn() => "1.5abc" % 2);
lossy_show('"-1.5" % 2', fn() => "-1.5" % 2);

echo "## a float-string the int cannot hold takes the same treatment\n";
lossy_show('"1e30" % 7', fn() => "1e30" % 7);
lossy_show('"1e400" % 7', fn() => "1e400" % 7);
lossy_show('"99999999999999999999" % 7', fn() => "99999999999999999999" % 7);

echo "## a lossy store into a typed int property\n";
lossy_show('$o->i = 1.9', function () { $o = new LossyInt(); $o->i = 1.9; return $o->i; });
lossy_show('$o->i = "1.9"', function () { $o = new LossyInt(); $o->i = "1.9"; return $o->i; });
lossy_show('$o->i += 0.9', function () { $o = new LossyInt(); $o->i = 1; $o->i += 0.9; return $o->i; });

echo "## what is NOT lossy still goes through, in both engines\n";
lossy_show('"2.0" % 2', fn() => "2.0" % 2);
lossy_show('"1e3" % 7', fn() => "1e3" % 7);
lossy_show('2.0 % 2', fn() => 2.0 % 2);
lossy_show('"5abc" % 2', fn() => "5abc" % 2);
lossy_show('$o->i = 5.0', function () { $o = new LossyInt(); $o->i = 5.0; return $o->i; });
lossy_show('$o->i = "2e1"', function () { $o = new LossyInt(); $o->i = "2e1"; return $o->i; });
?>
--EXPECT--
## the seven integer-only operators, with a float-string operand
  D: Implicit conversion from float-string "1.9" to int loses precision
"1.9" % 2 => 1
  D: Implicit conversion from float-string "1.9" to int loses precision
2 % "1.9" => 0
  D: Implicit conversion from float-string "1.5" to int loses precision
"1.5" | 1 => 1
  D: Implicit conversion from float-string "1.5" to int loses precision
"1.5" & 3 => 1
  D: Implicit conversion from float-string "1.5" to int loses precision
"1.5" ^ 3 => 2
  D: Implicit conversion from float-string "1.5" to int loses precision
"1.5" << 1 => 2
  D: Implicit conversion from float-string "1.5" to int loses precision
"1.5" >> 1 => 0
  D: Implicit conversion from float-string " 1.5 " to int loses precision
" 1.5 " % 2 => 1
  D: A non-numeric value encountered
  D: Implicit conversion from float-string "1.5abc" to int loses precision
"1.5abc" % 2 => 1
  D: Implicit conversion from float-string "-1.5" to int loses precision
"-1.5" % 2 => -1
## a float-string the int cannot hold takes the same treatment
  D: Implicit conversion from float-string "1e30" to int loses precision
"1e30" % 7 => 0
  D: Implicit conversion from float-string "1e400" to int loses precision
"1e400" % 7 => 0
  D: Implicit conversion from float-string "99999999999999999999" to int loses precision
"99999999999999999999" % 7 => 0
## a lossy store into a typed int property
  D: Implicit conversion from float 1.9 to int loses precision
$o->i = 1.9 => 1
  D: Implicit conversion from float-string "1.9" to int loses precision
$o->i = "1.9" => 1
  D: Implicit conversion from float 1.9 to int loses precision
$o->i += 0.9 => 1
## what is NOT lossy still goes through, in both engines
"2.0" % 2 => 0
"1e3" % 7 => 6
2.0 % 2 => 0
  D: A non-numeric value encountered
"5abc" % 2 => 1
$o->i = 5.0 => 5
$o->i = "2e1" => 20
