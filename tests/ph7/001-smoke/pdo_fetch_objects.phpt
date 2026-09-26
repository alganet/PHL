--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
FETCH_CLASS, fetchObject() and FETCH_INTO build or fill an object from a row
--DESCRIPTION--
FETCH_CLASS writes the columns as properties and runs the CONSTRUCTOR after
them, so a constructor that assigns a property wins over the column of the same
name. FETCH_PROPS_LATE reverses that order, which is the whole reason the flag
exists. FETCH_CLASSTYPE takes the class name from the FIRST column, which then
leaves the row.

The write ignores visibility: a private or protected property named like a
column is filled just the same, which is why this cannot go through an ordinary
property store. A class that declares nothing -- stdClass, which is also what
FETCH_CLASS falls back to with no name -- gets dynamic properties instead, the
ones an `(object)` cast would create.

fetchObject() is FETCH_CLASS for one row with its own refusal wording:
fetchAll() names the argument POSITION for an unusable class, fetchObject()
names the class it was given.

FETCH_INTO fills an object the script already has, once per fetch, and asking
for it without one is php's "No fetch-into object specified."
--FILE--
<?php
class PdoFetchObjRow {
    public $id;
    public $name;
    public $seen = 'no-ctor';
    public function __construct($tag = 'default') { $this->seen = $tag; }
}
class PdoFetchObjPlain { public $id; public $name; }
class PdoFetchObjPriv {
    private $id;
    protected $name;
    public function peek() { return [$this->id, $this->name]; }
}
$show = function ($label, $fn) {
    try { echo str_pad($label, 24), ' => ', json_encode($fn()), "\n"; }
    catch (Throwable $e) { echo str_pad($label, 24), ' => ', get_class($e), ': ', $e->getMessage(), "\n"; }
};

$db = new PDO('sqlite::memory:');
$db->exec('CREATE TABLE t (id INTEGER, name TEXT)');
$db->exec("INSERT INTO t VALUES (1,'a'),(2,'b')");
$q = fn () => $db->query('SELECT id, name FROM t ORDER BY rowid');

$show('CLASS',        fn () => $q()->fetchAll(PDO::FETCH_CLASS, 'PdoFetchObjRow'));
$show('CLASS + args', fn () => $q()->fetchAll(PDO::FETCH_CLASS, 'PdoFetchObjRow', ['tag']));
$show('CLASS late',   fn () => $q()->fetchAll(PDO::FETCH_CLASS|PDO::FETCH_PROPS_LATE, 'PdoFetchObjRow', ['tag']));
$show('CLASS no ctor',fn () => $q()->fetchAll(PDO::FETCH_CLASS, 'PdoFetchObjPlain'));
$show('CLASS stdClass',fn () => $q()->fetchAll(PDO::FETCH_CLASS, 'stdClass'));
$show('CLASS no name',fn () => $q()->fetchAll(PDO::FETCH_CLASS));
$show('CLASS missing',fn () => $q()->fetchAll(PDO::FETCH_CLASS, 'PdoFetchObjNoSuchClass'));
$show('CLASS private',function () use ($q) { $r = $q()->fetchAll(PDO::FETCH_CLASS, 'PdoFetchObjPriv'); return $r[0]->peek(); });
$show('CLASSTYPE',    fn () => $db->query("SELECT 'PdoFetchObjRow' AS c, id, name FROM t")
                                  ->fetchAll(PDO::FETCH_CLASS|PDO::FETCH_CLASSTYPE));
$show('setFetchMode CLASS', function () use ($q) {
    $s = $q(); $s->setFetchMode(PDO::FETCH_CLASS, 'PdoFetchObjRow'); return $s->fetch(); });

$show('fetchObject',      fn () => $q()->fetchObject());
$show('fetchObject Row',  fn () => $q()->fetchObject('PdoFetchObjRow'));
$show('fetchObject args', fn () => $q()->fetchObject('PdoFetchObjRow', ['tag']));
$show('fetchObject bad',  fn () => $q()->fetchObject('PdoFetchObjNoSuchClass'));
$show('fetchObject none', fn () => $db->query('SELECT id FROM t WHERE id = 99')->fetchObject());

$show('INTO', function () use ($q) {
    $o = new PdoFetchObjRow('into');
    $s = $q(); $s->setFetchMode(PDO::FETCH_INTO, $o); $s->fetch();
    return get_object_vars($o); });
$show('INTO twice', function () use ($q) {
    $o = new PdoFetchObjRow('into');
    $s = $q(); $s->setFetchMode(PDO::FETCH_INTO, $o); $s->fetch(); $s->fetch();
    return get_object_vars($o); });
$show('INTO no object', function () use ($q) { return $q()->setFetchMode(PDO::FETCH_INTO); });
$show('INTO unset',     function () use ($q) { return $q()->fetch(PDO::FETCH_INTO); });
?>
--EXPECT--
CLASS                    => [{"id":1,"name":"a","seen":"default"},{"id":2,"name":"b","seen":"default"}]
CLASS + args             => [{"id":1,"name":"a","seen":"tag"},{"id":2,"name":"b","seen":"tag"}]
CLASS late               => [{"id":1,"name":"a","seen":"tag"},{"id":2,"name":"b","seen":"tag"}]
CLASS no ctor            => [{"id":1,"name":"a"},{"id":2,"name":"b"}]
CLASS stdClass           => [{"id":1,"name":"a"},{"id":2,"name":"b"}]
CLASS no name            => [{"id":1,"name":"a"},{"id":2,"name":"b"}]
CLASS missing            => CLASS missing            => TypeError: PDOStatement::fetchAll(): Argument #2 must be a valid class
CLASS private            => [1,"a"]
CLASSTYPE                => [{"id":1,"name":"a","seen":"default"},{"id":2,"name":"b","seen":"default"}]
setFetchMode CLASS       => {"id":1,"name":"a","seen":"default"}
fetchObject              => {"id":1,"name":"a"}
fetchObject Row          => {"id":1,"name":"a","seen":"default"}
fetchObject args         => {"id":1,"name":"a","seen":"tag"}
fetchObject bad          => fetchObject bad          => TypeError: PDOStatement::fetchObject(): Argument #1 ($class) must be a valid class name, PdoFetchObjNoSuchClass given
fetchObject none         => false
INTO                     => {"id":1,"name":"a","seen":"into"}
INTO twice               => {"id":2,"name":"b","seen":"into"}
INTO no object           => INTO no object           => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 2 arguments for the fetch mode provided, 1 given
INTO unset               => INTO unset               => PDOException: SQLSTATE[HY000]: General error: No fetch-into object specified.
