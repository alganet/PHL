--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
var_dump marks a property whose value is a REFERENCE, exactly as it marks an element
--FILE--
<?php
/* php marks a property whose value IS a reference with `&`, in var_dump and
 * nowhere else — the same mark the array renderer puts on an element something
 * else holds. Both ends of the bind earn it (`$o->p =& $x` and `$r =& $o->p`),
 * it goes away with the last other holder, and print_r never shows it. */
function rpd(string $label, $o): void {
    ob_start(); var_dump($o);
    printf("%-30s %s\n", $label, preg_replace('/#\d+/', '#N', str_replace("\n", ' ', trim(ob_get_clean()))));
}
class RpdDecl { public $a = 1; }
class RpdVis { private $p = 1; protected $q = 2; }

$rpdA = new stdClass; $rpdA->q = 5;                      rpd('a plain dynamic property', $rpdA);
$rpdB = new RpdDecl;                                     rpd('a plain declared property', $rpdB);
$rpdC = new stdClass; $rpdC->q = 5; $rpdR =& $rpdC->q;   rpd('a dynamic one, bound', $rpdC);
$rpdD = new RpdDecl; $rpdS =& $rpdD->a;                  rpd('a declared one, bound', $rpdD);
$rpdE = new stdClass; $rpdE->q = 5; $rpdT =& $rpdE->q; unset($rpdT);
                                                         rpd('...and unbound again', $rpdE);
$rpdX = 1; $rpdF = new stdClass; $rpdF->p =& $rpdX;      rpd('the TARGET end of a bind', $rpdF);
$rpdG = new stdClass; $rpdH = [];  $rpdH[] =& $rpdG->q;  rpd('an array element holds it', $rpdG);
$rpdI = new stdClass; $rpdJ =& $rpdI->m;                 rpd('a property the bind created', $rpdI);
$rpdK = new RpdDecl; unset($rpdK->a); $rpdK->a = 2;      rpd('a declared one, re-created', $rpdK);
                                                         rpd('private and protected', new RpdVis);
$rpdL = unserialize('O:8:"stdClass":1:{s:1:"z";i:7;}');  rpd('an unserialized property', $rpdL);
$rpdM = json_decode('{"j":1}');                          rpd('a json_decode()d one', $rpdM);
$rpdN = (object)['c' => 3];                              rpd('a cast one', $rpdN);
$rpdO = new stdClass; $rpdO->a = 1;                      rpd('a clone', clone $rpdO);
echo "print_r marks nothing: ";
$rpdP = new stdClass; $rpdP->q = 5; $rpdQ =& $rpdP->q;
echo str_replace("\n", ' ', trim(print_r($rpdP, true))), "\n";
--EXPECT--
a plain dynamic property       object(stdClass)#N (1) {   ["q"]=>   int(5) }
a plain declared property      object(RpdDecl)#N (1) {   ["a"]=>   int(1) }
a dynamic one, bound           object(stdClass)#N (1) {   ["q"]=>   &int(5) }
a declared one, bound          object(RpdDecl)#N (1) {   ["a"]=>   &int(1) }
...and unbound again           object(stdClass)#N (1) {   ["q"]=>   int(5) }
the TARGET end of a bind       object(stdClass)#N (1) {   ["p"]=>   &int(1) }
an array element holds it      object(stdClass)#N (1) {   ["q"]=>   &NULL }
a property the bind created    object(stdClass)#N (1) {   ["m"]=>   &NULL }
a declared one, re-created     object(RpdDecl)#N (1) {   ["a"]=>   int(2) }
private and protected          object(RpdVis)#N (2) {   ["p":"RpdVis":private]=>   int(1)   ["q":protected]=>   int(2) }
an unserialized property       object(stdClass)#N (1) {   ["z"]=>   int(7) }
a json_decode()d one           object(stdClass)#N (1) {   ["j"]=>   int(1) }
a cast one                     object(stdClass)#N (1) {   ["c"]=>   int(3) }
a clone                        object(stdClass)#N (1) {   ["a"]=>   int(1) }
print_r marks nothing: stdClass Object (     [q] => 5 )
