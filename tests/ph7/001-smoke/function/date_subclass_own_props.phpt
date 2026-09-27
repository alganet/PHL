--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A date subclass's own properties show beside the shape its state builds
--FILE--
<?php
/* php's presentation for a date class starts from the object's OWN property table
 * and writes the struct's keys INTO it, so a subclass's properties come first and
 * the internal ones follow. PHL built the internal keys alone, so every property a
 * subclass of DateTime or DateTimeZone declared was missing from var_dump,
 * print_r, var_export, the (array) cast and json_encode — the object showed as
 * though it had none.
 *
 * __serialize() is the other way round: php builds a fresh array from the struct
 * and appends the object's own table to the END of it, so serialize() and
 * var_dump disagree about where a subclass's property sits — in both engines.
 * The append is an ADD and not an update, which is what makes a subclass property
 * named like one of the internal keys lose to the internal value there while
 * winning nothing in the presentation.
 *
 * The keys are MANGLED, the way php mangles a non-public property everywhere it
 * hands an object's own table out, and each surface then does its own thing with
 * that: the (array) cast and serialize() keep the mangling, var_dump prints the
 * visibility beside the name, var_export prints the plain name, and json_encode
 * emits public properties only — the mangling is exactly what tells it which.
 *
 * get_object_vars() is not one of these surfaces in either engine: it walks the
 * object's REAL table with the caller's scope applied, and answers `pub` alone
 * from out here. */
date_default_timezone_set('UTC');

class DtOwnDT extends DateTime { public $pub = 'P'; protected $prot = 'R'; private $priv = 'V'; }
class DtOwnZ extends DateTimeZone { public $pub = 'P'; protected $prot = 'R'; private $priv = 'V'; }
class DtOwnIv extends DateInterval { public $pub = 'P'; protected $prot = 'R'; private $priv = 'V'; }
class DtOwnDp extends DatePeriod { public $pub = 'P'; protected $prot = 'R'; private $priv = 'V'; }
class DtOwnClash extends DateTime { public $date = 'CLASH'; }

function dtown_keys($a) {
    return implode('|', array_map(fn ($k) => str_replace("\0", '~', (string) $k), array_keys($a)));
}
function dtown_show($label, $fn) {
    printf("%-26s %s\n", $label, $fn());
}

$d = new DtOwnDT('@0');
$z = new DtOwnZ('UTC');
$i = new DtOwnIv('P1D');
$p = new DtOwnDp(new DateTime('@0'), new DateInterval('P1D'), 1);

foreach (['datetime' => $d, 'timezone' => $z, 'interval' => $i, 'period' => $p] as $n => $o) {
    dtown_show("$n cast", fn () => dtown_keys((array) $o));
    dtown_show("$n json", fn () => json_encode($o));
    dtown_show("$n serialize", fn () => preg_replace('/\0/', '~', serialize($o)));
    dtown_show("$n __serialize", fn () => dtown_keys($o->__serialize()));
    dtown_show("$n export", fn () => preg_replace('/\s+/', ' ', var_export($o, true)));
}
dtown_show('datetime var_dump', function () use ($d) {
    ob_start();
    var_dump($d);
    return "\n" . preg_replace('/#\d+ /', '#N ', trim(ob_get_clean()));
});
dtown_show('timezone var_dump', function () use ($z) {
    ob_start();
    var_dump($z);
    return "\n" . preg_replace('/#\d+ /', '#N ', trim(ob_get_clean()));
});

/* A subclass property named like an internal key: the presentation gives it the
 * internal VALUE at its own position, while __serialize keeps the internal one
 * and drops the object's. The property itself still reads what it holds. */
$c = new DtOwnClash('@0');
dtown_show('clash cast', fn () => dtown_keys((array) $c));
dtown_show('clash cast date', function () use ($c) { $a = (array) $c; return $a['date']; });
dtown_show('clash serialize', fn () => preg_replace('/\0/', '~', serialize($c)));
dtown_show('clash read', fn () => $c->date);

/* An unconstructed one contributes no internal keys at all, so what shows is the
 * object's own table and nothing else. */
$u = (new ReflectionClass('DtOwnDT'))->newInstanceWithoutConstructor();
dtown_show('unbuilt cast', fn () => dtown_keys((array) $u));
dtown_show('unbuilt json', fn () => json_encode($u));

/* And a plain (unsubclassed) date is what it always was. */
dtown_show('plain cast', fn () => dtown_keys((array) new DateTime('@0')));
dtown_show('plain json', fn () => json_encode(new DateTimeZone('UTC')));
dtown_show('plain serialize', fn () => serialize(new DateInterval('PT1S')));
?>
--EXPECT--
datetime cast              pub|~*~prot|~DtOwnDT~priv|date|timezone_type|timezone
datetime json              {"pub":"P","date":"1970-01-01 00:00:00.000000","timezone_type":1,"timezone":"+00:00"}
datetime serialize         O:7:"DtOwnDT":6:{s:4:"date";s:26:"1970-01-01 00:00:00.000000";s:13:"timezone_type";i:1;s:8:"timezone";s:6:"+00:00";s:3:"pub";s:1:"P";s:7:"~*~prot";s:1:"R";s:13:"~DtOwnDT~priv";s:1:"V";}
datetime __serialize       date|timezone_type|timezone|pub|~*~prot|~DtOwnDT~priv
datetime export            \DtOwnDT::__set_state(array( 'pub' => 'P', 'prot' => 'R', 'priv' => 'V', 'date' => '1970-01-01 00:00:00.000000', 'timezone_type' => 1, 'timezone' => '+00:00', ))
timezone cast              pub|~*~prot|~DtOwnZ~priv|timezone_type|timezone
timezone json              {"pub":"P","timezone_type":3,"timezone":"UTC"}
timezone serialize         O:6:"DtOwnZ":5:{s:13:"timezone_type";i:3;s:8:"timezone";s:3:"UTC";s:3:"pub";s:1:"P";s:7:"~*~prot";s:1:"R";s:12:"~DtOwnZ~priv";s:1:"V";}
timezone __serialize       timezone_type|timezone|pub|~*~prot|~DtOwnZ~priv
timezone export            \DtOwnZ::__set_state(array( 'pub' => 'P', 'prot' => 'R', 'priv' => 'V', 'timezone_type' => 3, 'timezone' => 'UTC', ))
interval cast              pub|~*~prot|~DtOwnIv~priv|y|m|d|h|i|s|f|invert|days|from_string
interval json              {"pub":"P","y":0,"m":0,"d":1,"h":0,"i":0,"s":0,"f":0,"invert":0,"days":false,"from_string":false}
interval serialize         O:7:"DtOwnIv":13:{s:1:"y";i:0;s:1:"m";i:0;s:1:"d";i:1;s:1:"h";i:0;s:1:"i";i:0;s:1:"s";i:0;s:1:"f";d:0;s:6:"invert";i:0;s:4:"days";b:0;s:11:"from_string";b:0;s:3:"pub";s:1:"P";s:7:"~*~prot";s:1:"R";s:13:"~DtOwnIv~priv";s:1:"V";}
interval __serialize       y|m|d|h|i|s|f|invert|days|from_string|pub|~*~prot|~DtOwnIv~priv
interval export            \DtOwnIv::__set_state(array( 'pub' => 'P', 'prot' => 'R', 'priv' => 'V', 'y' => 0, 'm' => 0, 'd' => 1, 'h' => 0, 'i' => 0, 's' => 0, 'f' => 0.0, 'invert' => 0, 'days' => false, 'from_string' => false, ))
period cast                pub|~*~prot|~DtOwnDp~priv|start|current|end|interval|recurrences|include_start_date|include_end_date
period json                {"pub":"P","start":{"date":"1970-01-01 00:00:00.000000","timezone_type":1,"timezone":"+00:00"},"current":null,"end":null,"interval":{"y":0,"m":0,"d":1,"h":0,"i":0,"s":0,"f":0,"invert":0,"days":false,"from_string":false},"recurrences":2,"include_start_date":true,"include_end_date":false}
period serialize           O:7:"DtOwnDp":10:{s:5:"start";O:8:"DateTime":3:{s:4:"date";s:26:"1970-01-01 00:00:00.000000";s:13:"timezone_type";i:1;s:8:"timezone";s:6:"+00:00";}s:7:"current";N;s:3:"end";N;s:8:"interval";O:12:"DateInterval":10:{s:1:"y";i:0;s:1:"m";i:0;s:1:"d";i:1;s:1:"h";i:0;s:1:"i";i:0;s:1:"s";i:0;s:1:"f";d:0;s:6:"invert";i:0;s:4:"days";b:0;s:11:"from_string";b:0;}s:11:"recurrences";i:2;s:18:"include_start_date";b:1;s:16:"include_end_date";b:0;s:3:"pub";s:1:"P";s:7:"~*~prot";s:1:"R";s:13:"~DtOwnDp~priv";s:1:"V";}
period __serialize         start|current|end|interval|recurrences|include_start_date|include_end_date|pub|~*~prot|~DtOwnDp~priv
period export              \DtOwnDp::__set_state(array( 'pub' => 'P', 'prot' => 'R', 'priv' => 'V', 'start' => \DateTime::__set_state(array( 'date' => '1970-01-01 00:00:00.000000', 'timezone_type' => 1, 'timezone' => '+00:00', )), 'current' => NULL, 'end' => NULL, 'interval' => \DateInterval::__set_state(array( 'y' => 0, 'm' => 0, 'd' => 1, 'h' => 0, 'i' => 0, 's' => 0, 'f' => 0.0, 'invert' => 0, 'days' => false, 'from_string' => false, )), 'recurrences' => 2, 'include_start_date' => true, 'include_end_date' => false, ))
datetime var_dump          
object(DtOwnDT)#N (6) {
  ["pub"]=>
  string(1) "P"
  ["prot":protected]=>
  string(1) "R"
  ["priv":"DtOwnDT":private]=>
  string(1) "V"
  ["date"]=>
  string(26) "1970-01-01 00:00:00.000000"
  ["timezone_type"]=>
  int(1)
  ["timezone"]=>
  string(6) "+00:00"
}
timezone var_dump          
object(DtOwnZ)#N (5) {
  ["pub"]=>
  string(1) "P"
  ["prot":protected]=>
  string(1) "R"
  ["priv":"DtOwnZ":private]=>
  string(1) "V"
  ["timezone_type"]=>
  int(3)
  ["timezone"]=>
  string(3) "UTC"
}
clash cast                 date|timezone_type|timezone
clash cast date            1970-01-01 00:00:00.000000
clash serialize            O:10:"DtOwnClash":3:{s:4:"date";s:26:"1970-01-01 00:00:00.000000";s:13:"timezone_type";i:1;s:8:"timezone";s:6:"+00:00";}
clash read                 CLASH
unbuilt cast               pub|~*~prot|~DtOwnDT~priv
unbuilt json               {"pub":"P"}
plain cast                 date|timezone_type|timezone
plain json                 {"timezone_type":3,"timezone":"UTC"}
plain serialize            O:12:"DateInterval":10:{s:1:"y";i:0;s:1:"m";i:0;s:1:"d";i:0;s:1:"h";i:0;s:1:"i";i:0;s:1:"s";i:1;s:1:"f";d:0;s:6:"invert";i:0;s:4:"days";b:0;s:11:"from_string";b:0;}
