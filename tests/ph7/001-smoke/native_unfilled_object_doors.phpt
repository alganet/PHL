--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A native object nothing has filled: StreamBucket's typed slots and an unbuilt PDOStatement
--DESCRIPTION--
Two doors on an object whose C state does not exist yet. php declares StreamBucket's
`data`/`datalen`/`dataLength` TYPED and with no default, so a bucket the stream layer has
not filled carries them UNINITIALIZED — absent from the (array) cast, get_object_vars()
and json_encode(), named but uncounted by var_dump, and a read before the first write is
php's "must not be accessed before initialization" Error rather than an empty string;
PHL gave all three a zero default. And a PDOStatement no driver built refuses every door
with `PDOStatement object is uninitialized` — `foreach` and `getIterator()` included,
where PHL walked an empty set.
--FILE--
<?php
// php declares StreamBucket's three data fields TYPED and without a default, so a
// bucket nothing has filled carries them UNINITIALIZED: absent from the cast,
// get_object_vars() and json_encode(), named but uncounted by var_dump, and a read
// before the first write is an Error rather than an empty string.
$b = new StreamBucket();
$r = new ReflectionClass('StreamBucket');
foreach ($r->getProperties() as $p) {
    echo $p->getName(), ' type=', ($p->hasType() ? (string) $p->getType() : '-'),
         ' default=', var_export($p->hasDefaultValue(), true),
         ' init=', var_export($p->isInitialized($b), true), "\n";
}
ob_start(); var_dump($b); echo preg_replace('/#\d+ \(/', '#N (', ob_get_clean());
echo 'cast=', json_encode((array) $b), ' vars=', json_encode(get_object_vars($b)),
     ' json=', json_encode($b), "\n";
try { var_dump($b->data); } catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }
$b->data = 'x';
ob_start(); var_dump($b); echo preg_replace('/#\d+ \(/', '#N (', ob_get_clean());
echo 'cast=', json_encode((array) $b), "\n";
// A statement no driver built refuses every door, and `foreach` is one of them.
$s = new PDOStatement();
foreach ([['foreach', function () use ($s) { foreach ($s as $row) {} return 'walked'; }],
          ['getIterator', fn () => get_class($s->getIterator())],
          ['fetch', fn () => $s->fetch()],
          ['queryString', fn () => $s->queryString]] as [$label, $f]) {
    try { echo $label, ' = ', var_export($f(), true), "\n"; }
    catch (Throwable $t) { echo $label, ': ', get_class($t), ': ', $t->getMessage(), "\n"; }
}
--EXPECT--
bucket type=- default=true init=true
data type=string default=false init=false
datalen type=int default=false init=false
dataLength type=int default=false init=false
object(StreamBucket)#N (1) {
  ["bucket"]=>
  NULL
  ["data"]=>
  uninitialized(string)
  ["datalen"]=>
  uninitialized(int)
  ["dataLength"]=>
  uninitialized(int)
}
cast={"bucket":null} vars={"bucket":null} json={"bucket":null}
Error: Typed property StreamBucket::$data must not be accessed before initialization
object(StreamBucket)#N (2) {
  ["bucket"]=>
  NULL
  ["data"]=>
  string(1) "x"
  ["datalen"]=>
  uninitialized(int)
  ["dataLength"]=>
  uninitialized(int)
}
cast={"bucket":null,"data":"x"}
foreach = foreach: Error: PDOStatement object is uninitialized
getIterator = getIterator: Error: PDOStatement object is uninitialized
fetch = fetch: Error: PDOStatement object is uninitialized
queryString = queryString: Error: Typed property PDOStatement::$queryString must not be accessed before initialization
