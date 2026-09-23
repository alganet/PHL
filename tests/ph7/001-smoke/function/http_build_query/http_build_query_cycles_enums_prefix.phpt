--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
http_build_query stops on a cycle, and an enum is its backing value
--FILE--
<?php
// A container that is its own ancestor contributes NOTHING: php protects the
// hashtable it is walking, so the self-reference is skipped rather than
// recursed into. PHL had no guard at all here and ran out of memory.
$a = ['a' => 1];
$a['self'] = &$a;
echo "cycle=", http_build_query($a), "\n";
$o = new stdClass();
$o->a = 1;
$o->self = $o;
echo "objcycle=", http_build_query(['o' => $o]), "\n";
// Deep but acyclic still walks the whole way down.
$d = 'v';
for ($i = 0; $i < 300; $i++) { $d = ['k' => $d]; }
echo "deep=", strlen(http_build_query($d)), "\n";

// The numeric prefix is written RAW -- php never url-encodes it -- and only a
// TOP-LEVEL integer key ever carries it.
foreach (['a b', 'a&b', '%', ''] as $p) {
    echo "pfx[$p]=", http_build_query([1, 2], $p), " | ",
        http_build_query(['k' => [1, 2]], $p), " | ",
        http_build_query([5 => [7 => 'x']], $p), "\n";
}

// A backed enum is a SCALAR here, valued by its backing value; an unbacked one
// cannot be one, and an enum can never be the $data itself.
enum HbqSuit: string { case H = 'h'; case S = 's'; }
enum HbqPure { case A; }
echo "enum=", http_build_query(['s' => HbqSuit::H, 'n' => ['x' => HbqSuit::S]]), "\n";
try {
    http_build_query(['a' => HbqPure::A]);
} catch (ValueError $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
foreach ([HbqSuit::H, HbqPure::A] as $e) {
    try {
        http_build_query($e);
    } catch (TypeError $t) {
        echo get_class($t), ": ", $t->getMessage(), "\n";
    }
}

// Resources are skipped exactly like nulls.
$fp = fopen('php://memory', 'r');
echo "res=", http_build_query(['a' => 1, 'r' => $fp, 'n' => null, 'b' => 2]), "\n";
fclose($fp);

// Visibility is the CALLER's scope, a virtual hooked property has no value to
// write, and the get hook is not dispatched.
class HbqVis {
    public $p = 1;
    private $q = 2;
    protected $r = 3;
    public int $v { get => 9; }
    public $w = 4;
    public function inside() { return http_build_query($this); }
}
echo "outside=", http_build_query(new HbqVis()), "\n";
echo "inside=", (new HbqVis())->inside(), "\n";

// php declares `object|array $data` and reports only "array" -- one ZPP macro,
// one name -- and the other three parameters are screened normally.
try {
    http_build_query(null);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
foreach ([[['a' => 1], [1]], [['a' => 1], '', [1]], [['a' => 1], '', '&', [1]]] as $args) {
    try {
        http_build_query(...$args);
    } catch (TypeError $e) {
        echo $e->getMessage(), "\n";
    }
}
// The same wording rule reaches every `object|array` parameter.
$null = null;
try {
    array_walk($null, 'strlen');
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
try {
    current($null);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
--EXPECT--
cycle=a=1
objcycle=o%5Ba%5D=1
deep=2096
pfx[a b]=a b0=1&a b1=2 | k%5B0%5D=1&k%5B1%5D=2 | a b5%5B7%5D=x
pfx[a&b]=a&b0=1&a&b1=2 | k%5B0%5D=1&k%5B1%5D=2 | a&b5%5B7%5D=x
pfx[%]=%0=1&%1=2 | k%5B0%5D=1&k%5B1%5D=2 | %5%5B7%5D=x
pfx[]=0=1&1=2 | k%5B0%5D=1&k%5B1%5D=2 | 5%5B7%5D=x
enum=s=h&n%5Bx%5D=s
ValueError: Unbacked enum HbqPure cannot be converted to a string
TypeError: http_build_query(): Argument #1 ($data) must not be an enum, HbqSuit given
TypeError: http_build_query(): Argument #1 ($data) must not be an enum, HbqPure given
res=a=1&b=2
outside=p=1&w=4
inside=p=1&q=2&r=3&w=4
http_build_query(): Argument #1 ($data) must be of type array, null given
http_build_query(): Argument #2 ($numeric_prefix) must be of type string, array given
http_build_query(): Argument #3 ($arg_separator) must be of type ?string, array given
http_build_query(): Argument #4 ($encoding_type) must be of type int, array given
array_walk(): Argument #1 ($array) must be of type array, null given
current(): Argument #1 ($array) must be of type array, null given
--CLEAN--
<?php
