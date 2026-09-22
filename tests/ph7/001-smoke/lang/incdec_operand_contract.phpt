--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
++/-- operand contract: array/object/resource TypeError, bool and null warned no-ops
--DESCRIPTION--
The mirror of arith_operand_contract.phpt for the in-place mutation opcodes.
php's ++/-- reject an array, object or resource operand with a catchable
TypeError ("Cannot increment array", "Cannot decrement P") and leave the
variable untouched; a bool or null operand is a no-op php WARNS about. PHL
performed all of these silently: `$o++` left the object alone with no
diagnostic, and `true++` answered int(2).
--FILE--
<?php
function idocTry($label, $fn) {
    // Build the whole line before echoing it: a warning raised inside $fn must
    // land on its own line, not in the middle of this one.
    try {
        $out = var_export($fn(), true);
    } catch (TypeError $e) {
        $out = "TypeError: " . $e->getMessage();
    }
    echo $label, " => ", $out, "\n";
}
set_error_handler(function ($no, $msg) { echo "  Warning[$no]: $msg\n"; return true; });

class IdocP {}
class IdocS { public function __toString(): string { return "5"; } }

// Objects: the class name is part of the message, and __toString() does not help.
idocTry('$o++', function () { $o = new IdocP(); $o++; return get_class($o); });
idocTry('$o--', function () { $o = new IdocP(); $o--; return get_class($o); });
idocTry('++$o', function () { $o = new IdocP(); ++$o; return get_class($o); });
idocTry('--$o', function () { $o = new IdocP(); --$o; return get_class($o); });
idocTry('$str_obj++', function () { $o = new IdocS(); $o++; return get_class($o); });

// Arrays and resources answer the same TypeError, named by type.
idocTry('$a++', function () { $a = [1]; $a++; return $a; });
idocTry('$a--', function () { $a = [1]; $a--; return $a; });
idocTry('++$a', function () { $a = [1]; ++$a; return $a; });
idocTry('$res++', function () { $r = fopen("php://memory", "r"); $r++; return "unreached"; });
idocTry('$res--', function () { $r = fopen("php://memory", "r"); $r--; return "unreached"; });

// The rejection reaches through an element, a property and a static property.
idocTry('$a["k"]++', function () { $a = ["k" => new IdocP()]; $a["k"]++; return "unreached"; });
idocTry('$a[0]++ (array)', function () { $a = [[1]]; $a[0]++; return "unreached"; });
idocTry('$o->p++', function () { $o = new stdClass(); $o->p = [1]; $o->p++; return "unreached"; });

// The variable is left untouched, and execution carries on after the catch.
$idocO = new IdocP();
try { $idocO++; } catch (TypeError $e) { echo "caught: ", $e->getMessage(), "\n"; }
echo "still ", get_class($idocO), "\n";
$idocA = [1, 2];
try { --$idocA; } catch (TypeError $e) { echo "caught: ", $e->getMessage(), "\n"; }
echo "still ", count($idocA), " element(s)\n";

// bool: a warned no-op, both directions and both fixities (PHL answered 2 / -1).
idocTry('true++',  function () { $b = true;  $b++; return $b; });
idocTry('false++', function () { $b = false; $b++; return $b; });
idocTry('true--',  function () { $b = true;  $b--; return $b; });
idocTry('false--', function () { $b = false; $b--; return $b; });
idocTry('++true',  function () { $b = true;  return ++$b; });
idocTry('--false', function () { $b = false; return --$b; });

// null: ++ counts up from zero, -- is the warned no-op.
idocTry('null++', function () { $n = null; $n++; return $n; });
idocTry('null--', function () { $n = null; $n--; return $n; });
idocTry('++null', function () { $n = null; return ++$n; });

// Numbers and numeric strings are unaffected by the contract.
idocTry('int++',   function () { $i = 5; $i++; return $i; });
idocTry('float--', function () { $f = 1.5; $f--; return $f; });
idocTry('"5"++',   function () { $s = "5"; $s++; return $s; });
restore_error_handler();
?>
--EXPECT--
$o++ => TypeError: Cannot increment IdocP
$o-- => TypeError: Cannot decrement IdocP
++$o => TypeError: Cannot increment IdocP
--$o => TypeError: Cannot decrement IdocP
$str_obj++ => TypeError: Cannot increment IdocS
$a++ => TypeError: Cannot increment array
$a-- => TypeError: Cannot decrement array
++$a => TypeError: Cannot increment array
$res++ => TypeError: Cannot increment resource
$res-- => TypeError: Cannot decrement resource
$a["k"]++ => TypeError: Cannot increment IdocP
$a[0]++ (array) => TypeError: Cannot increment array
$o->p++ => TypeError: Cannot increment array
caught: Cannot increment IdocP
still IdocP
caught: Cannot decrement array
still 2 element(s)
  Warning[2]: Increment on type bool has no effect, this will change in the next major version of PHP
true++ => true
  Warning[2]: Increment on type bool has no effect, this will change in the next major version of PHP
false++ => false
  Warning[2]: Decrement on type bool has no effect, this will change in the next major version of PHP
true-- => true
  Warning[2]: Decrement on type bool has no effect, this will change in the next major version of PHP
false-- => false
  Warning[2]: Increment on type bool has no effect, this will change in the next major version of PHP
++true => true
  Warning[2]: Decrement on type bool has no effect, this will change in the next major version of PHP
--false => false
null++ => 1
  Warning[2]: Decrement on type null has no effect, this will change in the next major version of PHP
null-- => NULL
++null => 1
int++ => 6
float-- => 0.5
"5"++ => 6
--CLEAN--
<?php
?>
