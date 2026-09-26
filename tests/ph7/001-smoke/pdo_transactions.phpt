--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
beginTransaction()/commit()/rollBack(), and what counts as "in a transaction"
--DESCRIPTION--
Whether a transaction is open is sqlite's OWN autocommit flag, not a count the
driver keeps -- so a BEGIN the script sent through exec() is indistinguishable
from beginTransaction(): inTransaction() answers true for it, and a second
beginTransaction() refuses.

The three refusals are bare sentences with no SQLSTATE in front of them, unlike
every other PDOException this driver raises. And the four transaction verbs are
the ones that do NOT clear the handle's error state on entry, which is why a
failure inside a transaction is still readable after a rollBack().

Pdo\Sqlite::ATTR_TRANSACTION_MODE chooses WHICH BEGIN is emitted -- deferred,
immediate or exclusive -- and anything that is not one of those three answers
false in silence, like every other unusable attribute value.
--FILE--
<?php
$mk = function () {
    $d = new PDO('sqlite::memory:');
    $d->exec('CREATE TABLE t (a INTEGER)');
    return $d;
};
$show = function ($label, $fn) {
    try { echo str_pad($label, 26), ' => ', json_encode($fn()), "\n"; }
    catch (Throwable $e) { echo str_pad($label, 26), ' => ', get_class($e), ': ', $e->getMessage(), "\n"; }
};

$db = $mk();
$show('fresh',        fn () => $db->inTransaction());
$show('begin',        fn () => $db->beginTransaction());
$show('in one',       fn () => $db->inTransaction());
$show('begin twice',  fn () => $db->beginTransaction());
$show('commit',       fn () => $db->commit());
$show('after commit', fn () => $db->inTransaction());
$show('commit again', fn () => $db->commit());
$show('rollBack cold',fn () => $db->rollBack());

$show('rollBack undoes', function () use ($mk) {
    $d = $mk();
    $d->beginTransaction();
    $d->exec('INSERT INTO t VALUES (1)');
    $mid = $d->query('SELECT count(*) FROM t')->fetch(PDO::FETCH_NUM)[0];
    $d->rollBack();
    return [$mid, $d->query('SELECT count(*) FROM t')->fetch(PDO::FETCH_NUM)[0]]; });
$show('commit persists', function () use ($mk) {
    $d = $mk();
    $d->beginTransaction();
    $d->exec('INSERT INTO t VALUES (1)');
    $d->commit();
    return $d->query('SELECT count(*) FROM t')->fetch(PDO::FETCH_NUM)[0]; });
$show('a raw BEGIN counts', function () use ($mk) {
    $d = $mk(); $d->exec('BEGIN'); $r = $d->inTransaction(); $d->exec('COMMIT'); return $r; });
$show('failure inside one', function () use ($mk) {
    $d = $mk(); $d->beginTransaction();
    try { $d->exec('NOT SQL'); } catch (Throwable $e) {}
    return [$d->inTransaction(), $d->rollBack()]; });

$show('mode default', fn () => $db->getAttribute(Pdo\Sqlite::ATTR_TRANSACTION_MODE));
foreach ([Pdo\Sqlite::TRANSACTION_MODE_DEFERRED, Pdo\Sqlite::TRANSACTION_MODE_IMMEDIATE,
          Pdo\Sqlite::TRANSACTION_MODE_EXCLUSIVE] as $m) {
    $show("mode $m", function () use ($mk, $m) {
        $d = $mk();
        $set = $d->setAttribute(Pdo\Sqlite::ATTR_TRANSACTION_MODE, $m);
        $b = $d->beginTransaction();
        $d->exec('INSERT INTO t VALUES (1)');
        return [$set, $b, $d->commit(), $d->getAttribute(Pdo\Sqlite::ATTR_TRANSACTION_MODE)]; });
}
$show('mode bogus', function () use ($mk) {
    $d = $mk(); return [$d->setAttribute(Pdo\Sqlite::ATTR_TRANSACTION_MODE, 99),
                        $d->getAttribute(Pdo\Sqlite::ATTR_TRANSACTION_MODE)]; });
?>
--EXPECT--
fresh                      => false
begin                      => true
in one                     => true
begin twice                => begin twice                => PDOException: There is already an active transaction
commit                     => true
after commit               => false
commit again               => commit again               => PDOException: There is no active transaction
rollBack cold              => rollBack cold              => PDOException: There is no active transaction
rollBack undoes            => [1,0]
commit persists            => 1
a raw BEGIN counts         => true
failure inside one         => [true,true]
mode default               => 0
mode 0                     => [true,true,true,0]
mode 1                     => [true,true,true,1]
mode 2                     => [true,true,true,2]
mode bogus                 => [false,0]
