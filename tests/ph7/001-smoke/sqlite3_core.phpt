--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SQLite3 opens, closes and reports a failure the way ext/sqlite3 does
--DESCRIPTION--
ext/sqlite3 is php's OTHER sqlite surface and it shares nothing with ext/pdo but
the library underneath. Its object carries two pieces of state a script can tell
apart, and the whole "is this usable" surface follows from the pair: an
`initialised` flag a successful open raises and close() never lowers, and the
handle itself, which close() drops.

So there are THREE states, not two. A never-opened object refuses every verb
with an Error; a CLOSED one refuses the same verbs but still answers the three
error readers (0, "" and 0, because there is no library state left to ask) and
may be open()ed again; a live one answers everything. enableExceptions() asks
about neither and works on all three, and enableExtendedResultCodes() splits the
way the readers do -- Error for a blank object, false for a closed one.

A failed OPEN is a plain Exception rather than SQLite3Exception, whatever the
object was later told: nothing can have told it yet. Every other failure is one
routine with two destinations -- an E_WARNING naming the method, or a
SQLite3Exception carrying sqlite's own integer code and no prefix at all.

Extended result codes move BOTH readers: with them on lastErrorCode() reports
1555 for a UNIQUE violation and not 19, while lastExtendedErrorCode() reports
the extended one either way. And php 8.3 deprecated the warning mode itself, so
asking for it says so every time, whatever the setting already was.
--FILE--
<?php
$sq3show = function ($label, $fn) {
    $notes = [];
    set_error_handler(function ($no, $str) use (&$notes) { $notes[] = "[$no] $str"; return true; });
    try { $out = json_encode($fn()); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    restore_error_handler();
    foreach ($notes as $n) { echo '  ', $n, "\n"; }
    echo str_pad($label, 30), ' => ', $out, "\n";
};
$sq3blank = fn () => (new ReflectionClass('SQLite3'))->newInstanceWithoutConstructor();
$sq3closed = function () { $d = new SQLite3(':memory:'); $d->close(); return $d; };

/* what the library says about itself */
$v = SQLite3::version();
$sq3show('version keys', fn () => array_keys($v));
$sq3show('version shapes', fn () => [is_string($v['versionString']), is_int($v['versionNumber'])]);
$sq3show('escapeString', fn () => SQLite3::escapeString("it's a 'test'"));
$sq3show('escapeString stops at NUL', fn () => SQLite3::escapeString("ab\0cd"));
$sq3show('escapeString empty', fn () => SQLite3::escapeString(''));
$sq3show('escapeString coerces', fn () => SQLite3::escapeString(42));

/* three states, and what each verb makes of them */
$sq3show('blank lastErrorCode', fn () => $sq3blank()->lastErrorCode());
$sq3show('blank exec', fn () => $sq3blank()->exec('SELECT 1'));
$sq3show('blank close', fn () => $sq3blank()->close());
$sq3show('blank enableExceptions', fn () => $sq3blank()->enableExceptions(true));
$sq3show('blank enableExtended', fn () => $sq3blank()->enableExtendedResultCodes(true));
$sq3show('closed lastErrorCode', fn () => $sq3closed()->lastErrorCode());
$sq3show('closed lastErrorMsg', fn () => $sq3closed()->lastErrorMsg());
$sq3show('closed lastExtended', fn () => $sq3closed()->lastExtendedErrorCode());
$sq3show('closed exec', fn () => $sq3closed()->exec('SELECT 1'));
$sq3show('closed changes', fn () => $sq3closed()->changes());
$sq3show('closed busyTimeout', fn () => $sq3closed()->busyTimeout(5));
$sq3show('closed enableExceptions', fn () => $sq3closed()->enableExceptions(true));
$sq3show('closed enableExtended', fn () => $sq3closed()->enableExtendedResultCodes(true));
$sq3show('close twice', function () { $d = new SQLite3(':memory:'); $d->close(); return $d->close(); });
$sq3show('reopen after close', function () {
    $d = new SQLite3(':memory:'); $d->close(); $d->open(':memory:');
    return $d->exec('CREATE TABLE t (a)'); });
$sq3show('open a live handle', function () { $d = new SQLite3(':memory:'); $d->open(':memory:'); return 'no'; });
$sq3show('open a blank one', function () { $d = (new ReflectionClass('SQLite3'))->newInstanceWithoutConstructor();
    $d->open(':memory:'); return $d->exec('CREATE TABLE t (a)'); });

/* a failed open is a plain Exception whatever the object was told */
$sq3show('open nowhere', fn () => (bool)new SQLite3('/no-such-dir-sq3/x.db'));
$sq3show('open no flags', fn () => (bool)new SQLite3(':memory:', 0));
$sq3show('open NUL name', fn () => (bool)new SQLite3("/tmp/a\0b"));

/* running SQL, and the two shapes a failure takes */
$db = new SQLite3(':memory:');
$sq3show('exec create', fn () => $db->exec('CREATE TABLE sq3t (a INTEGER PRIMARY KEY, b TEXT)'));
$sq3show('exec insert', fn () => $db->exec("INSERT INTO sq3t VALUES (1,'x'),(2,'y')"));
$sq3show('changes', fn () => $db->changes());
$sq3show('lastInsertRowID', fn () => $db->lastInsertRowID());
$sq3show('exec a select', fn () => $db->exec('SELECT * FROM sq3t'));
$sq3show('changes after select', fn () => $db->changes());
$sq3show('exec empty', fn () => $db->exec(''));
$sq3show('exec broken', fn () => $db->exec('NOT SQL'));
$sq3show('error after failure', fn () => [$db->lastErrorCode(), $db->lastErrorMsg()]);
$sq3show('error after success', function () use ($db) {
    $db->exec('SELECT 1'); return [$db->lastErrorCode(), $db->lastErrorMsg()]; });
$sq3show('busyTimeout', fn () => $db->busyTimeout(250));

/* the extended codes, which move BOTH readers */
$sq3show('extended off', function () use ($db) {
    $db->exec('INSERT INTO sq3t VALUES (1,\'dup\')');
    return [$db->lastErrorCode(), $db->lastExtendedErrorCode()]; });
$sq3show('extended on', function () use ($db) {
    $db->enableExtendedResultCodes(true);
    $db->exec('INSERT INTO sq3t VALUES (1,\'dup\')');
    return [$db->lastErrorCode(), $db->lastExtendedErrorCode()]; });
$sq3show('extended off again', function () use ($db) {
    $db->enableExtendedResultCodes(false);
    $db->exec('INSERT INTO sq3t VALUES (1,\'dup\')');
    return [$db->lastErrorCode(), $db->lastExtendedErrorCode()]; });

/* the error mode: a switch that answers what it REPLACED */
$sq3show('exceptions on', fn () => $db->enableExceptions(true));
$sq3show('exec broken, throwing', fn () => $db->exec('NOT SQL'));
$sq3show('exceptions off', fn () => $db->enableExceptions(false));
$sq3show('exec broken, warning', fn () => $db->exec('NOT SQL'));

/* the shut door loadExtension stands behind */
$sq3show('loadExtension', fn () => $db->loadExtension('nope'));
$sq3show('loadExtension, throwing', function () use ($db) {
    $db->enableExceptions(true);
    try { return $db->loadExtension('nope'); } finally { $db->enableExceptions(false); } });

/* the object shows nothing at all */
ob_start(); var_dump(new SQLite3(':memory:')); $sq3dump = ob_get_clean();
$sq3show('presentation', fn () => preg_replace('/#\d+/', '#N', $sq3dump));
$sq3show('clone', fn () => (bool)clone $db);
$sq3show('serialize', fn () => serialize($db));
--EXPECT--
version keys                   => ["versionString","versionNumber"]
version shapes                 => [true,true]
escapeString                   => "it''s a ''test''"
escapeString stops at NUL      => "ab"
escapeString empty             => ""
escapeString coerces           => "42"
blank lastErrorCode            => Error: The SQLite3 object has not been correctly initialised or is already closed
blank exec                     => Error: The SQLite3 object has not been correctly initialised or is already closed
blank close                    => true
blank enableExceptions         => false
blank enableExtended           => Error: The SQLite3 object has not been correctly initialised or is already closed
closed lastErrorCode           => 0
closed lastErrorMsg            => ""
closed lastExtended            => 0
closed exec                    => Error: The SQLite3 object has not been correctly initialised or is already closed
closed changes                 => Error: The SQLite3 object has not been correctly initialised or is already closed
closed busyTimeout             => Error: The SQLite3 object has not been correctly initialised or is already closed
closed enableExceptions        => false
closed enableExtended          => false
close twice                    => true
reopen after close             => true
open a live handle             => Exception: Already initialised DB Object
open a blank one               => true
open nowhere                   => Exception: Unable to open database: unable to open database file
open no flags                  => Exception: Unable to open database: bad parameter or other API misuse
open NUL name                  => ValueError: SQLite3::__construct(): Argument #1 ($filename) must not contain any null bytes
exec create                    => true
exec insert                    => true
changes                        => 2
lastInsertRowID                => 2
exec a select                  => true
changes after select           => 2
exec empty                     => true
  [2] SQLite3::exec(): near "NOT": syntax error
exec broken                    => false
error after failure            => [1,"near \"NOT\": syntax error"]
error after success            => [0,"not an error"]
busyTimeout                    => true
  [2] SQLite3::exec(): UNIQUE constraint failed: sq3t.a
extended off                   => [19,1555]
  [2] SQLite3::exec(): UNIQUE constraint failed: sq3t.a
extended on                    => [1555,1555]
  [2] SQLite3::exec(): UNIQUE constraint failed: sq3t.a
extended off again             => [19,1555]
exceptions on                  => false
exec broken, throwing          => SQLite3Exception: near "NOT": syntax error
  [8192] SQLite3::enableExceptions(): Use of warnings for SQLite3 is deprecated
exceptions off                 => true
  [2] SQLite3::exec(): near "NOT": syntax error
exec broken, warning           => false
  [2] SQLite3::loadExtension(): SQLite Extensions are disabled
loadExtension                  => false
  [8192] SQLite3::enableExceptions(): Use of warnings for SQLite3 is deprecated
loadExtension, throwing        => SQLite3Exception: SQLite Extensions are disabled
presentation                   => "object(SQLite3)#N (0) {\n}\n"
clone                          => Error: Trying to clone an uncloneable object of class SQLite3
serialize                      => Exception: Serialization of 'SQLite3' is not allowed
