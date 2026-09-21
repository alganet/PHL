--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
"Array to string conversion" E_WARNING fires at every user-visible coercion site
--DESCRIPTION--
php emits an E_WARNING "Array to string conversion" whenever an ARRAY is coerced to a string
FOR THE USER — echo/print, concatenation and `.=`, the (string) cast, string interpolation,
a variable-variable NAME, printf/sprintf %s, implode(), settype($x,'string'), and a string
offset write — but stays SILENT for the internal coercions that format a value for inspection
(print_r/var_export/serialize/json_encode) or compare it (sort). PHL used to emit none,
silently producing "Array". The handler captures the warning so the prefix/stream difference
across engines does not matter; the value still renders as "Array".
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "[W$no] $str\n"; return true; });
$a = [1, 2];

echo "== user-visible sites ==\n";
echo $a; echo "\n";
print $a; echo "\n";
$x = "p" . $a;
$x = "$a";
$x = (string)$a;
printf("%s\n", $a);
echo sprintf("%s", $a), "\n";
echo implode(",", [[1], [2]]), "\n";   // two inner arrays -> two warnings
$b = [1]; $b .= "x";                    // lhs array -> one
$c = [1]; $dd = [2]; $c .= $dd;         // both arrays -> two
$e = [1]; echo $e, $e, "\n";            // two echo args -> two
$n = "$a" . $a;                         // interp + concat -> two
$Array = "vv"; $f = ["k" => 1]; echo $$f, "\n";  // var-var NAME -> one, reads $Array
$g = "abc"; $g[0] = $a; echo $g, "\n";  // string offset write: A2S + first-byte
$h = [1]; settype($h, "string"); var_dump($h);

echo "== internal coercions stay SILENT ==\n";
print_r([1]);
echo var_export([1], true), "\n";
echo serialize([1]), "\n";
echo json_encode([1]), "\n";
$s = [[2], [1]]; sort($s); echo count($s), "\n";  // sort comparison: no A2S
echo "done\n";
?>
--EXPECT--
== user-visible sites ==
[W2] Array to string conversion
Array
[W2] Array to string conversion
Array
[W2] Array to string conversion
[W2] Array to string conversion
[W2] Array to string conversion
[W2] Array to string conversion
Array
[W2] Array to string conversion
Array
[W2] Array to string conversion
[W2] Array to string conversion
Array,Array
[W2] Array to string conversion
[W2] Array to string conversion
[W2] Array to string conversion
[W2] Array to string conversion
Array[W2] Array to string conversion
Array
[W2] Array to string conversion
[W2] Array to string conversion
[W2] Array to string conversion
vv
[W2] Array to string conversion
[W2] Only the first byte will be assigned to the string offset
Abc
[W2] Array to string conversion
string(5) "Array"
== internal coercions stay SILENT ==
Array
(
    [0] => 1
)
array (
  0 => 1,
)
a:1:{i:0;i:1;}
[1]
2
done
