--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
$expr::class refuses every operand that is not an object, a class name string included
--FILE--
<?php
class CcnoA { static function names() { return [self::class, static::class]; } }
class CcnoB extends CcnoA { static function up() { return parent::class; } }
enum CcnoE { case X; }
$ccnoRes = fopen('php://memory', 'r');
$ccnoClosed = fopen('php://memory', 'r');
fclose($ccnoClosed);
$ccnoVals = ['CcnoA', 'stdClass', 'nope', '', 'self', 'static', 0, -1, 1.5, true, false, null, [], [1], $ccnoRes, $ccnoClosed];
foreach ($ccnoVals as $ccnoV) {
    try {
        var_dump($ccnoV::class);
    } catch (Throwable $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}
fclose($ccnoRes);
function ccnoName() { return 'CcnoA'; }
try {
    var_dump(ccnoName()::class);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
$ccnoS = 'CcnoA';
try {
    var_dump($ccnoS::CLASS);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
try {
    $ccnoOut = $ccnoS::class;
    echo "not reached\n";
} catch (TypeError $e) {
    var_dump(isset($ccnoOut));
}
// what still answers: an object of any kind, a literal name, and the three keywords
var_dump((new CcnoB)::class, CcnoE::X::class, (function () {})::class, [new CcnoA][0]::class);
var_dump(('CcnoA')::class, CcnoA::class, CcnoB::names(), CcnoB::up());
?>
--EXPECT--
TypeError: Cannot use "::class" on string
TypeError: Cannot use "::class" on string
TypeError: Cannot use "::class" on string
TypeError: Cannot use "::class" on string
TypeError: Cannot use "::class" on string
TypeError: Cannot use "::class" on string
TypeError: Cannot use "::class" on int
TypeError: Cannot use "::class" on int
TypeError: Cannot use "::class" on float
TypeError: Cannot use "::class" on true
TypeError: Cannot use "::class" on false
TypeError: Cannot use "::class" on null
TypeError: Cannot use "::class" on array
TypeError: Cannot use "::class" on array
TypeError: Cannot use "::class" on resource
TypeError: Cannot use "::class" on resource
Cannot use "::class" on string
Cannot use "::class" on string
bool(false)
string(5) "CcnoB"
string(5) "CcnoE"
string(7) "Closure"
string(5) "CcnoA"
string(5) "CcnoA"
string(5) "CcnoA"
array(2) {
  [0]=>
  string(5) "CcnoA"
  [1]=>
  string(5) "CcnoB"
}
string(5) "CcnoA"
--CLEAN--
<?php
unset($ccnoRes, $ccnoClosed, $ccnoVals, $ccnoV, $ccnoS, $ccnoOut);
