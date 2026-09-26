--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
isset()/empty() chains read intermediate links through the accessors
--FILE--
<?php
// php reads an isset() chain's INTERMEDIATE links in BP_VAR_IS mode: silent,
// but a real read -- __isset() and then __get() (or offsetExists() and then
// offsetGet()). Only the LAST link answers the isset question.
class IssetChainModel {
    public $plain;
    public function __construct() { $this->plain = new IssetChainLeaf; }
    public function __get($n) {
        echo "get($n) ";
        return $n === 'rel' ? new IssetChainLeaf : ($n === 'bag' ? ['k' => new IssetChainLeaf] : null);
    }
    public function __isset($n) { echo "isset($n) "; return $n === 'rel' || $n === 'bag'; }
}
class IssetChainGetOnly {
    public function __get($n) { echo "getonly($n) "; return new IssetChainLeaf; }
}
class IssetChainLeaf { public $id = 7; public $nil = null; }
class IssetChainStore implements ArrayAccess {
    private $slots;
    public function __construct($v) { $this->slots = ['k' => $v]; }
    public function offsetExists($o): bool { echo "exists($o) "; return isset($this->slots[$o]); }
    public function offsetGet($o): mixed { echo "get[$o] "; return $this->slots[$o] ?? null; }
    public function offsetSet($o, $v): void {}
    public function offsetUnset($o): void {}
}
$m = new IssetChainModel;
$g = new IssetChainGetOnly;
$leaf = new IssetChainStore(new IssetChainLeaf);
$nested = new IssetChainStore($leaf);
$run = function (string $label, callable $f) { echo str_pad($label, 34), '=> '; var_dump($f()); };
$run('isset($m->rel->id)',        fn() => isset($m->rel->id));
$run('isset($m->rel->nil)',       fn() => isset($m->rel->nil));
$run('isset($m->rel->nope)',      fn() => isset($m->rel->nope));
$run('isset($m->bag["k"])',       fn() => isset($m->bag['k']));
$run('isset($m->bag["k"]->id)',   fn() => isset($m->bag['k']->id));
$run('isset($m->plain->id)',      fn() => isset($m->plain->id));
$run('isset($g->rel->id)',        fn() => isset($g->rel->id));
$run('isset($g->rel)',            fn() => isset($g->rel));
$run('empty($g->rel->id)',        fn() => empty($g->rel->id));
$run('isset($leaf["k"]->id)',     fn() => isset($leaf['k']->id));
$run('isset($nested["k"]["k"])',  fn() => isset($nested['k']['k']));
$run('isset($nested["z"]["k"])',  fn() => isset($nested['z']['k']));
$run('empty($nested["k"]["k"])',  fn() => empty($nested['k']['k']));
$run('$m->rel->id ?? "d"',        fn() => $m->rel->id ?? 'd');
$run('isset($undef->a->b)',       function () { return isset($undef->a->b); });
--EXPECT--
isset($m->rel->id)                => isset(rel) get(rel) bool(true)
isset($m->rel->nil)               => isset(rel) get(rel) bool(false)
isset($m->rel->nope)              => isset(rel) get(rel) bool(false)
isset($m->bag["k"])               => isset(bag) get(bag) bool(true)
isset($m->bag["k"]->id)           => isset(bag) get(bag) bool(true)
isset($m->plain->id)              => bool(true)
isset($g->rel->id)                => getonly(rel) bool(true)
isset($g->rel)                    => bool(false)
empty($g->rel->id)                => getonly(rel) bool(false)
isset($leaf["k"]->id)             => exists(k) get[k] bool(true)
isset($nested["k"]["k"])          => exists(k) get[k] exists(k) bool(true)
isset($nested["z"]["k"])          => exists(z) bool(false)
empty($nested["k"]["k"])          => exists(k) get[k] exists(k) get[k] bool(false)
$m->rel->id ?? "d"                => isset(rel) get(rel) int(7)
isset($undef->a->b)               => bool(false)
