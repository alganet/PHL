--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: DateInterval's two read-only properties take a DEPRECATED dynamic one, and `f` == -1.0 reads the int sentinel (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* the deprecated dynamic property the write really creates here: `days` still
 * reads false from the struct behind it, while `from_string` and any other name
 * answer the new one. The deprecation itself is muted -- the scope policy keeps E_DEPRECATED
 * off this corpus, and the notice is not what the pair is pinning. */
error_reporting(E_ALL & ~E_DEPRECATED);
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

/* the sentinel: the microsecond count -1000000 reads back as the INT -1
 * through the property door, while get_properties (and the surfaces built on
 * it) still say float(-1). */
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
days written: false
from_string written: 5
nope written: 5
bool(false)
int(5)
int(-1)
O:12:"DateInterval":10:{s:1:"y";i:0;s:1:"m";i:0;s:1:"d";i:0;s:1:"h";i:0;s:1:"i";i:0;s:1:"s";i:0;s:1:"f";d:-1;s:6:"invert";i:0;s:4:"days";b:0;s:11:"from_string";b:0;}
bool(true)
float(-1)
string(8) "-1000000"
float(-2)
