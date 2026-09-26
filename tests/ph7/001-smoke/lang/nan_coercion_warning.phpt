--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A NaN coerced to bool or string says so, once per site
--FILE--
<?php
/* php 8.5 warns when a NaN is COERCED -- `unexpected NAN value was coerced to
 * bool` at every truthiness site, `unexpected NAN value was coerced to string`
 * at every string one -- and answers true and "NAN" as it always did. PHL
 * answered both values and said nothing.
 *
 * Neither warning belongs to a COMPARISON (`NAN == true` reaches the same bool
 * conversion in silence) nor to the debug and serialization renderers, which
 * are not coercions at all. And it is a NaN only: INF and -INF spell
 * themselves out without a word. */
set_error_handler(function ($n, $s) { echo "  [$n] $s\n"; return true; });
$nan = NAN;
$fn = function (bool $b) { return $b; };
$sf = function (string $s) { return strlen($s); };
echo "-- bool sites\n";
var_dump((bool)$nan);
if ($nan) { echo "if taken\n"; }
var_dump(!$nan, $nan ? 'y' : 'n', $nan && false, $nan || false, empty($nan), boolval($nan));
$t = $nan; settype($t, 'bool'); var_dump($t);
var_dump($fn($nan), count(array_filter([$nan])), strlen(md5('x', $nan)));
echo "-- string sites\n";
var_dump((string)$nan, strval($nan), 'x' . $nan, "$nan");
$t = $nan; settype($t, 'string'); var_dump($t);
var_dump($sf($nan), sprintf('%s', $nan), trim($nan), implode(',', [$nan, 1]));
print_r($nan); echo "\n";
$a = []; $a["$nan"] = 1; var_dump(array_key_first($a));
echo "-- silent: comparisons, renderers, and the infinities\n";
var_dump($nan == true, $nan == 'NAN', $nan < 1.5, [$nan] == [$nan], in_array($nan, [true]));
var_dump($nan, var_export($nan, true), serialize($nan), json_encode([$nan]), sprintf('%f', $nan));
var_dump((bool)INF, (string)INF, (string)-INF, (bool)-INF);
var_dump(is_nan($nan), $nan + 1);
/* One truthiness rule, not two: `empty()` and `array_filter()`'s default test
 * used to answer with a set of rules of their own, and they disagreed with the
 * bool cast about a string of MORE THAN ONE zero -- php calls only "" and the
 * single byte "0" false. */
foreach (['' , '0', '00', '000', '0.0', ' ', 'false', '0x0'] as $s) {
    printf("%-7s empty=%-5s bool=%-5s filter=%d\n", "'$s'", var_export(empty($s), true),
        var_export((bool)$s, true), count(array_filter([$s])));
}
restore_error_handler();
?>
--EXPECT--
-- bool sites
  [2] unexpected NAN value was coerced to bool
bool(true)
  [2] unexpected NAN value was coerced to bool
if taken
  [2] unexpected NAN value was coerced to bool
  [2] unexpected NAN value was coerced to bool
  [2] unexpected NAN value was coerced to bool
  [2] unexpected NAN value was coerced to bool
  [2] unexpected NAN value was coerced to bool
  [2] unexpected NAN value was coerced to bool
bool(false)
string(1) "y"
bool(false)
bool(true)
bool(false)
bool(true)
  [2] unexpected NAN value was coerced to bool
bool(true)
  [2] unexpected NAN value was coerced to bool
  [2] unexpected NAN value was coerced to bool
  [2] unexpected NAN value was coerced to bool
bool(true)
int(1)
int(16)
-- string sites
  [2] unexpected NAN value was coerced to string
  [2] unexpected NAN value was coerced to string
  [2] unexpected NAN value was coerced to string
  [2] unexpected NAN value was coerced to string
string(3) "NAN"
string(3) "NAN"
string(4) "xNAN"
string(3) "NAN"
  [2] unexpected NAN value was coerced to string
string(3) "NAN"
  [2] unexpected NAN value was coerced to string
  [2] unexpected NAN value was coerced to string
  [2] unexpected NAN value was coerced to string
  [2] unexpected NAN value was coerced to string
int(3)
string(3) "NAN"
string(3) "NAN"
string(5) "NAN,1"
  [2] unexpected NAN value was coerced to string
NAN
  [2] unexpected NAN value was coerced to string
string(3) "NAN"
-- silent: comparisons, renderers, and the infinities
bool(true)
bool(false)
bool(false)
bool(false)
bool(true)
float(NAN)
string(3) "NAN"
string(6) "d:NAN;"
bool(false)
string(3) "NaN"
bool(true)
string(3) "INF"
string(4) "-INF"
bool(true)
bool(true)
float(NAN)
''      empty=true  bool=false filter=0
'0'     empty=true  bool=false filter=0
'00'    empty=false bool=true  filter=1
'000'   empty=false bool=true  filter=1
'0.0'   empty=false bool=true  filter=1
' '     empty=false bool=true  filter=1
'false' empty=false bool=true  filter=1
'0x0'   empty=false bool=true  filter=1
