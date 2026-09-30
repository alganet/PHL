--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A property at the SOURCE end of a reference holds its own value
--DESCRIPTION--
`$q->p =& $o->p` makes BOTH ends references in php. PHL's reference table
cannot NAME a property as a holder — a bound property holds its slot with a
counted pin instead — and only the TARGET end ever took one. So the source
property's slot had exactly one recorded holder, and when the target's object
died the unpin freed the value out from under the source, which then read NULL.
A variable at either end was always safe, because a variable IS a name the table
records; a property was not.
Found by nette/di's Compiler::processSchema(), which binds
`$context->dynamics =& $this->extensions[...]->dynamicValidators` once per
context — so the second context's source was already gone, and phpstan died on
`Cannot use object of type Context as array`.
--FILE--
<?php
class RsplHolder { public $dv = ['seed']; public static $sv = ['seed']; }
class RsplCtx    { public $dynamics = []; public static $st = []; }

function rspl_show($label, $v) {
    echo str_pad($label, 30), gettype($v), is_array($v) ? '(' . count($v) . ')' : '', "\n";
}

// The target end dies; the source property keeps its value.
$rspl_h = new RsplHolder;
(function () use ($rspl_h) { $c = new RsplCtx; $c->dynamics =& $rspl_h->dv; })();
rspl_show('obj prop <= obj prop:', $rspl_h->dv);

RsplHolder::$sv = ['seed'];
(function () { $c = new RsplCtx; $c->dynamics =& RsplHolder::$sv; })();
rspl_show('obj prop <= static prop:', RsplHolder::$sv);

$rspl_h2 = new RsplHolder;
(function () use ($rspl_h2) { $o = new stdClass; $o->x =& $rspl_h2->dv; })();
rspl_show('dynamic prop <= obj prop:', $rspl_h2->dv);

// ...and the shapes that were already right stay right.
$rspl_arr = ['seed'];
(function () use (&$rspl_arr) { $c = new RsplCtx; $c->dynamics =& $rspl_arr[0]; })();
rspl_show('obj prop <= array elem:', $rspl_arr);

$rspl_h3 = new RsplHolder;
(function () use ($rspl_h3) { $a = []; $a[] =& $rspl_h3->dv; })();
rspl_show('array elem <= obj prop:', $rspl_h3->dv);

$rspl_h4 = new RsplHolder;
(function () use ($rspl_h4) { $v =& $rspl_h4->dv; $v[] = 'q'; })();
rspl_show('variable <= obj prop:', $rspl_h4->dv);

// Binding it twice, the way a per-context bind does, still writes through.
$rspl_h5 = new RsplHolder;
$rspl_h5->dv = [];
foreach ([1, 2, 3] as $rspl_i) {
    $c = new RsplCtx;
    $c->dynamics =& $rspl_h5->dv;
    $c->dynamics[] = $rspl_i;
    unset($c);
}
rspl_show('rebound three times:', $rspl_h5->dv);
var_dump($rspl_h5->dv);

// A source re-bound to somewhere else lets the first slot go.
$rspl_h6 = new RsplHolder;
$rspl_r =& $rspl_h6->dv;
$rspl_other = ['other'];
$rspl_h6->dv =& $rspl_other;
$rspl_other[] = 'more';
rspl_show('source rebound:', $rspl_h6->dv);
rspl_show('...its old value:', $rspl_r);
?>
--EXPECT--
obj prop <= obj prop:         array(1)
obj prop <= static prop:      array(1)
dynamic prop <= obj prop:     array(1)
obj prop <= array elem:       array(1)
array elem <= obj prop:       array(1)
variable <= obj prop:         array(2)
rebound three times:          array(3)
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
source rebound:               array(2)
...its old value:             array(1)
--CLEAN--
<?php
unset($rspl_h, $rspl_h2, $rspl_h3, $rspl_h4, $rspl_h5, $rspl_h6, $rspl_arr, $rspl_i, $rspl_r, $rspl_other, $c);
