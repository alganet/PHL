--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A native class presents the shape php shows, not the state it keeps
--DESCRIPTION--
php shows a DateTime as date/timezone_type/timezone and a WeakReference as
["object"]; the state underneath is a timestamp with an offset, and a C-side
cell. PHL kept those slots hidden and so showed NOTHING at all. ph7_class::
xPresent is php's get_properties/get_debug_info handler: it fills an array with
what php shows. The two handlers do not agree and the callback is told which is
asking — a WeakReference shows ["object"] to var_dump and nothing to the (array)
cast, while a DateTime shows the same three keys to both. Neither handler feeds
get_object_vars() or foreach, which php answers from the real properties with the
caller's scope applied — empty for every class here.
--FILE--
<?php
function nphShow($label, $o) {
    ob_start(); print_r($o); $pr = trim(ob_get_clean());
    ob_start(); var_dump($o); $vd = trim(ob_get_clean());
    /* var_dump prints an object id that counts allocations; the shape is what
     * this pins, so normalize it away. */
    $vd = preg_replace('/#\d+ /', '#N ', $vd);
    echo $label, "\n";
    echo '  dump   : ', str_replace("\n", ' ', $vd), "\n";
    echo '  print  : ', str_replace("\n", ' ', $pr), "\n";
    echo '  export : ', str_replace("\n", ' ', var_export($o, true)), "\n";
    echo '  cast   : ', json_encode((array)$o), "\n";
    echo '  vars   : ', json_encode(get_object_vars($o)), "\n";
    $keys = []; foreach ($o as $k => $v) { $keys[] = $k; }
    echo '  foreach: ', json_encode($keys), "\n";
}

/* php's timezone_type tag: 3 an identifier, 1 a fixed offset, 2 an abbreviation. */
nphShow('DateTime UTC', new DateTime('2020-01-02 03:04:05.123456', new DateTimeZone('UTC')));
nphShow('DateTime offset', new DateTime('2020-06-01 12:00:00', new DateTimeZone('+02:00')));
nphShow('DateTimeImmutable', new DateTimeImmutable('1999-12-31 23:59:59', new DateTimeZone('UTC')));
nphShow('DateTimeZone id', new DateTimeZone('UTC'));
nphShow('DateTimeZone offset', new DateTimeZone('-03:30'));
nphShow('DateTimeZone abbrev', new DateTimeZone('GMT'));

/* A DEBUG-only hook: ["object"] to var_dump/print_r, nothing to (array). */
$nphTarget = new stdClass;
$nphTarget->a = 1;
nphShow('WeakReference live', WeakReference::create($nphTarget));

/* The presented shape is a VIEW: the engine state stays reachable and writable
 * through the class's own methods, and the slots behind it stay invisible. */
$nphDt = new DateTime('2020-01-02 03:04:05', new DateTimeZone('UTC'));
echo 'still works => ', $nphDt->format('Y-m-d H:i:s'), ' / ', $nphDt->getTimestamp(), "\n";
echo 'no declared props => ', count((new ReflectionClass('DateTime'))->getProperties()), "\n";
echo 'zone name => ', (new DateTimeZone('+05:45'))->getName(), "\n";
--EXPECT--
DateTime UTC
  dump   : object(DateTime)#N (3) {   ["date"]=>   string(26) "2020-01-02 03:04:05.123456"   ["timezone_type"]=>   int(3)   ["timezone"]=>   string(3) "UTC" }
  print  : DateTime Object (     [date] => 2020-01-02 03:04:05.123456     [timezone_type] => 3     [timezone] => UTC )
  export : \DateTime::__set_state(array(    'date' => '2020-01-02 03:04:05.123456',    'timezone_type' => 3,    'timezone' => 'UTC', ))
  cast   : {"date":"2020-01-02 03:04:05.123456","timezone_type":3,"timezone":"UTC"}
  vars   : []
  foreach: []
DateTime offset
  dump   : object(DateTime)#N (3) {   ["date"]=>   string(26) "2020-06-01 12:00:00.000000"   ["timezone_type"]=>   int(1)   ["timezone"]=>   string(6) "+02:00" }
  print  : DateTime Object (     [date] => 2020-06-01 12:00:00.000000     [timezone_type] => 1     [timezone] => +02:00 )
  export : \DateTime::__set_state(array(    'date' => '2020-06-01 12:00:00.000000',    'timezone_type' => 1,    'timezone' => '+02:00', ))
  cast   : {"date":"2020-06-01 12:00:00.000000","timezone_type":1,"timezone":"+02:00"}
  vars   : []
  foreach: []
DateTimeImmutable
  dump   : object(DateTimeImmutable)#N (3) {   ["date"]=>   string(26) "1999-12-31 23:59:59.000000"   ["timezone_type"]=>   int(3)   ["timezone"]=>   string(3) "UTC" }
  print  : DateTimeImmutable Object (     [date] => 1999-12-31 23:59:59.000000     [timezone_type] => 3     [timezone] => UTC )
  export : \DateTimeImmutable::__set_state(array(    'date' => '1999-12-31 23:59:59.000000',    'timezone_type' => 3,    'timezone' => 'UTC', ))
  cast   : {"date":"1999-12-31 23:59:59.000000","timezone_type":3,"timezone":"UTC"}
  vars   : []
  foreach: []
DateTimeZone id
  dump   : object(DateTimeZone)#N (2) {   ["timezone_type"]=>   int(3)   ["timezone"]=>   string(3) "UTC" }
  print  : DateTimeZone Object (     [timezone_type] => 3     [timezone] => UTC )
  export : \DateTimeZone::__set_state(array(    'timezone_type' => 3,    'timezone' => 'UTC', ))
  cast   : {"timezone_type":3,"timezone":"UTC"}
  vars   : []
  foreach: []
DateTimeZone offset
  dump   : object(DateTimeZone)#N (2) {   ["timezone_type"]=>   int(1)   ["timezone"]=>   string(6) "-03:30" }
  print  : DateTimeZone Object (     [timezone_type] => 1     [timezone] => -03:30 )
  export : \DateTimeZone::__set_state(array(    'timezone_type' => 1,    'timezone' => '-03:30', ))
  cast   : {"timezone_type":1,"timezone":"-03:30"}
  vars   : []
  foreach: []
DateTimeZone abbrev
  dump   : object(DateTimeZone)#N (2) {   ["timezone_type"]=>   int(2)   ["timezone"]=>   string(3) "GMT" }
  print  : DateTimeZone Object (     [timezone_type] => 2     [timezone] => GMT )
  export : \DateTimeZone::__set_state(array(    'timezone_type' => 2,    'timezone' => 'GMT', ))
  cast   : {"timezone_type":2,"timezone":"GMT"}
  vars   : []
  foreach: []
WeakReference live
  dump   : object(WeakReference)#N (1) {   ["object"]=>   object(stdClass)#N (1) {     ["a"]=>     int(1)   } }
  print  : WeakReference Object (     [object] => stdClass Object         (             [a] => 1         )  )
  export : \WeakReference::__set_state(array( ))
  cast   : []
  vars   : []
  foreach: []
still works => 2020-01-02 03:04:05 / 1577934245
no declared props => 0
zone name => +05:45
