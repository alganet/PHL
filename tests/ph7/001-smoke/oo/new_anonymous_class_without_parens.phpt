--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An anonymous class is dereferencable without parentheses (PHP 8.4)
--FILE--
<?php
interface NacwpI { function m(); }
class NacwpP { public $q = 'q'; }
echo new class { function m(){return 'anon';} }->m(), "\n";
echo new class(3) { function __construct(public $v){} }->v, "\n";
$f = new class { function __invoke(){return 'inv';} }(); echo $f, "\n";
echo new class { const K = 'K'; }::K, "\n";
echo new class { static function s(){return 'S';} }::s(), "\n";
echo new class { public $a = [4,5]; }->a[1], "\n";
echo new class extends NacwpP {}->q, "\n";
echo new class implements NacwpI { function m(){return 'I';} }?->m(), "\n";
echo new readonly class(9) { function __construct(public int $r){} }->r, "\n";
$o = new class { public $z = 1; }; var_dump($o instanceof stdClass, $o->z);
$c = clone new class { public $w = 'w'; }; echo $c->w, "\n";
echo get_class(new class {}) === get_class(new class {}) ? "same\n" : "diff\n";
echo (new class { function m(){return 'paren';} })->m(), "\n";
var_dump(new class { function m(){return [1];} }->m()[0]);
echo strlen(new class { function __toString(){return 'abc';} }), "\n";
$x = [new class { public $k = 'arr'; }->k]; echo $x[0], "\n";
echo new class { function m(){ return new class { public $in = 'nested'; }->in; } }->m(), "\n";
--EXPECT--
anon
3
inv
K
S
5
q
I
9
bool(false)
int(1)
w
diff
paren
int(1)
3
arr
nested
