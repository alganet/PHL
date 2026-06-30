--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unset($o->prop) removes the property from all iteration (declared + dynamic)
--FILE--
<?php
/* dynamic (stdClass) properties */
$o = new stdClass; $o->a = 1; $o->b = 2; $o->c = 3;
unset($o->b);
echo json_encode($o), "\n";                                   // {"a":1,"c":3}
echo implode(",", array_keys(get_object_vars($o))), "\n";     // a,c
echo count((array) $o), "\n";                                 // 2
foreach ($o as $k => $v) echo "$k=$v;"; echo "\n";            // a=1;c=3;
echo var_export(isset($o->b), true), "\n";                    // false

/* declared properties are removed too */
class UnsetPropC { public $x = 1; public $y = 2; public $z = 3; }
$c = new UnsetPropC(); unset($c->y);
echo json_encode($c), "\n";                                   // {"x":1,"z":3}
echo var_export(isset($c->y), true), "\n";                    // false

/* unset then re-add moves the key to the end (PHP creation order) */
$o = new stdClass; $o->a = 1; $o->b = 2;
unset($o->a); $o->a = 9;
echo json_encode($o), "\n";                                   // {"b":2,"a":9}

/* nested: unset a key inside a property-held array must NOT destroy the property */
$n = new stdClass; $n->data = ["a" => 1, "b" => 2];
unset($n->data["a"]);
echo json_encode($n), "\n";                                   // {"data":{"b":2}}

/* nested: unset a sub-object's property must NOT destroy the sub-object */
$m = new stdClass; $m->inner = new stdClass; $m->inner->p = 1; $m->inner->q = 2;
unset($m->inner->p);
echo json_encode($m), "\n";                                   // {"inner":{"q":2}}

/* clone must NOT resurrect a property unset on the source (declared or dynamic) */
class CloneUnsetC { public $p = 1; public $q = 2; }
$s = new CloneUnsetC(); unset($s->p);
echo json_encode(clone $s), "\n";                             // {"q":2}
$sd = new stdClass; $sd->x = 1; $sd->y = 2; unset($sd->x);
echo json_encode(clone $sd), "\n";                            // {"y":2}
?>
--EXPECT--
{"a":1,"c":3}
a,c
2
a=1;c=3;
false
{"x":1,"z":3}
false
{"b":2,"a":9}
{"data":{"b":2}}
{"inner":{"q":2}}
{"q":2}
{"y":2}
--CLEAN--
<?php
