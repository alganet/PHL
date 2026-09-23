--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Removing an element keeps the value for whoever still holds it
--FILE--
<?php
// unset($a[k]) removes the ELEMENT; a reference to it keeps reading the value.
$rda = ['k' => 1];
$rdr = &$rda['k'];
unset($rda['k']);
var_dump($rdr, $rda);

// Same when the whole array goes, in each of its three ways.
$rdb = [7];
$rds = &$rdb[0];
unset($rdb);
var_dump($rds);

$rdc = [7];
$rdt = &$rdc[0];
$rdc = "gone";
var_dump($rdt, $rdc);

$rdd = [1];
$rde = [&$rdd[0]];
unset($rdd);
var_dump($rde);

// A builtin that drops an element follows the same rule.
$rdf = [1, 2];
$rdu = &$rdf[1];
array_pop($rdf);
var_dump($rdu, $rdf);

// $GLOBALS['x'] is the global $x: unsetting it drops the NAME, not the value
// another holder still refers to.
$rdg = 2;
$rdv = &$rdg;
unset($GLOBALS['rdg']);
var_dump(isset($rdg), $rdv);

// An unset chain only removes its OUTERMOST subscript.
$rdh = ['k' => ['n' => 1]];
unset($rdh['k']['n']);
var_dump($rdh);

// ...and an ArrayAccess base gets offsetUnset() for that outermost subscript alone.
class RefDeathBag implements ArrayAccess {
    public $d = [];
    public $log = '';
    public function __construct($d) { $this->d = $d; }
    public function offsetExists($o): bool { return isset($this->d[$o]); }
    #[\ReturnTypeWillChange] public function offsetGet($o) { $this->log .= "get($o) "; return $this->d[$o] ?? null; }
    public function offsetSet($o, $v): void { $this->d[$o] = $v; }
    public function offsetUnset($o): void { $this->log .= "unset($o) "; unset($this->d[$o]); }
}
$rdj = new RefDeathBag(['a' => 1, 'b' => 2]);
unset($rdj['a']);
echo $rdj->log, "\n";
var_dump($rdj->d);
?>
--EXPECT--
int(1)
array(0) {
}
int(7)
int(7)
string(4) "gone"
array(1) {
  [0]=>
  int(1)
}
int(2)
array(1) {
  [0]=>
  int(1)
}
bool(false)
int(2)
array(1) {
  ["k"]=>
  array(0) {
  }
}
unset(a) 
array(1) {
  ["b"]=>
  int(2)
}
--CLEAN--
<?php
unset($rda, $rdr, $rdb, $rds, $rdc, $rdt, $rdd, $rde, $rdf, $rdu, $rdv, $rdh, $rdj);
