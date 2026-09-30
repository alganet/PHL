--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A type NAME is not a reserved word: `Integer::m();` at statement position
--DESCRIPTION--
php reserves none of `int`/`integer`/`bool`/`boolean`/`float`/`string`/`object`:
its scanner hands every one of them back as a plain identifier, and only a TYPE
position gives them a meaning. PHL lexed all seven as KEYWORDS, and its statement
dispatcher refused a statement that begins with one -- `Integer::setModulo($id,
$m);`, which is how phpseclib's BinaryField and PrimeField spell the class they
imported under that name. The same word after `$x = ` already compiled, so only
the statement HEAD and the global `const` name were refusing it.

`integer` and `boolean` are not even reserved CLASS names, so a class may carry
one; the other five are, and only their use as an ordinary name is at issue here.
--FILE--
<?php
class integer { public static $seen = []; public static function z($n) { self::$seen[] = $n; return $n; } }
class boolean { public static function z($n) { return "b$n"; } }
function integer($n) { return "f$n"; }

const integer = 'CI';
const boolean = 'CB';

// Statement position: an expression statement, not a keyword.
integer::z(1);
boolean::z(2);
integer(3);
var_dump(integer::$seen);

// Expression position kept working.
var_dump(integer::z(4), boolean::z(5), integer(6));

// The five reserved class names still PARSE as names; the class simply is not there.
foreach (['int', 'float', 'string', 'bool', 'object'] as $stknName) {
    try {
        $stknName::z(1);
    } catch (Error $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}

// The global constants.
var_dump(integer, boolean);

// The words still MEAN types where a type is expected.
function stknTyped(int $i, float $f, string $s, bool $b, object $o): string {
    return gettype($i) . '|' . gettype($f) . '|' . gettype($s) . '|' . gettype($b) . '|' . get_class($o);
}
var_dump(stknTyped(1, 1.5, 'x', true, new stdClass));
?>
--EXPECT--
array(1) {
  [0]=>
  int(1)
}
int(4)
string(2) "b5"
string(2) "f6"
Error: Class "int" not found
Error: Class "float" not found
Error: Class "string" not found
Error: Class "bool" not found
Error: Class "object" not found
string(2) "CI"
string(2) "CB"
string(38) "integer|double|string|boolean|stdClass"
