--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: DateInterval's two read-only properties refuse a write, and `f` == -1.0 has no sentinel (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* php answers `days` and `from_string` from its own struct and lets a write
 * create a DEPRECATED dynamic property beside them -- one that never reaches
 * the interval, so `$i->days = 5` leaves `$i->days` false there. The scope policy refuses a
 * deprecation and PHL refuses a dynamic property outright, so both writes meet
 * at the Error PHL already raises for any other name. */
$i = new DateInterval('PT0S');
foreach (['days', 'from_string', 'nope'] as $p) {
    try {
        $i->$p = 5;
        echo "$p written: ", var_export($i->$p, true), "\n";
    } catch (Error $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}
var_dump($i->days, $i->from_string);

/* php reserves the microsecond count -1000000 as an UNSET sentinel and answers
 * the int -1 for it -- through the property read alone: its own get_properties
 * view, and every surface built on one, still says float(-1). PHL has one slot
 * per property, so it keeps the float that six surfaces agree on. */
$j = new DateInterval('PT0S');
$j->f = -1.0;
var_dump($j->f);
echo serialize($j), "\n";
var_dump((array)$j === get_object_vars($j), get_object_vars($j)['f'], $j->format('%f'));
$k = new DateInterval('PT0S');
$k->f = -2.0;
var_dump($k->f);
?>
--EXPECT--
Error: Cannot create dynamic property DateInterval::$days
Error: Cannot create dynamic property DateInterval::$from_string
Error: Cannot create dynamic property DateInterval::$nope
bool(false)
bool(false)
float(-1)
O:12:"DateInterval":10:{s:1:"y";i:0;s:1:"m";i:0;s:1:"d";i:0;s:1:"h";i:0;s:1:"i";i:0;s:1:"s";i:0;s:1:"f";d:-1;s:6:"invert";i:0;s:4:"days";b:0;s:11:"from_string";b:0;}
bool(true)
float(-1)
string(8) "-1000000"
float(-2)
