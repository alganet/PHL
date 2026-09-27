--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/pdo declares PDO, PDOStatement, PDOException and Pdo\Sqlite, and reports one driver
--DESCRIPTION--
The DECLARED surface of the PDO family: the class names, their parents and
interfaces, the constants and the method signatures — everything Reflection and
class_exists() can see before a connection exists. php's own surface is the
source (band E), and the whole table is diffed against the 8.5
oracle rather than written from the manual.

One row here is deliberately NOT php's: §10's non-deprecated rule removes the
seven PDO::SQLITE_* constants php 8.5 still declares and marks deprecated —
their unprefixed successors live on Pdo\Sqlite. It is twinned in
002-integration/class/PDO/.
--FILE--
<?php
var_dump(extension_loaded('PDO'), extension_loaded('pdo_sqlite'));

/* The driver list is read through the drivers PHL SHIPS, because the oracle's
 * own set is not pinned and moves under us: this box's php gained pdo_mysql
 * from its container's packaging, and enumerating the raw list here measured
 * the BOX rather than either engine. Shipping sqlite alone among the drivers
 * is a standing decision, so a driver php has and we do not is not a gap
 * anyone intends to close. A driver we are supposed to have going missing
 * still shows, which is what this row is for. */
$ours = static fn (array $d): array => array_values(array_intersect($d, ['sqlite']));
var_dump($ours(PDO::getAvailableDrivers()));

/* the driver list has a PROCEDURAL spelling too — ext/pdo's only function —
 * and both spellings build the same array from the same routine (compared
 * UNFILTERED: that they agree is true whatever the set happens to hold) */
var_dump($ours(pdo_drivers()), pdo_drivers() === PDO::getAvailableDrivers());
$drv = new ReflectionFunction('pdo_drivers');
var_dump($drv->getNumberOfParameters(), (string)$drv->getReturnType());
try { pdo_drivers(1); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

$r = new ReflectionClass('Pdo\Sqlite');
var_dump($r->getParentClass()->getName());

/* the two spellings of a return type php's stubs use: PDO's verbs are
 * tentative, the driver subclass's are declared */
$exec = new ReflectionMethod('PDO', 'exec');
var_dump($exec->hasReturnType(), (string)$exec->getTentativeReturnType());
$cf = new ReflectionMethod('Pdo\Sqlite', 'createFunction');
var_dump((string)$cf->getReturnType(), $cf->hasTentativeReturnType());

/* a by-reference parameter, a union type and a class-constant default all
 * come out of the one signature string */
$bp = new ReflectionMethod('PDOStatement', 'bindParam');
foreach ($bp->getParameters() as $p) {
    printf("%s %s%s%s\n", (string)$p->getType(), $p->isPassedByReference() ? '&' : '',
        '$' . $p->getName(),
        $p->isDefaultValueAvailable() ? ' = ' . var_export($p->getDefaultValue(), true) : '');
}
var_dump(PDO::PARAM_STR, PDO::FETCH_BOTH, PDO::ERRMODE_EXCEPTION, PDO::ERR_NONE);
var_dump(Pdo\Sqlite::OPEN_READONLY, Pdo\Sqlite::DETERMINISTIC, Pdo\Sqlite::DENY);

/* both handles refuse clone and serialize, and a statement presents exactly
 * one property */
$st = new PDOStatement();
try { clone $st; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { serialize($st); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
var_dump(array_keys(get_class_vars('PDOStatement')));
var_dump((new ReflectionClass('PDOException'))->getParentClass()->getName());
?>
--EXPECT--
bool(true)
bool(true)
array(1) {
  [0]=>
  string(6) "sqlite"
}
array(1) {
  [0]=>
  string(6) "sqlite"
}
bool(true)
int(0)
string(5) "array"
ArgumentCountError: pdo_drivers() expects exactly 0 arguments, 1 given
string(3) "PDO"
bool(false)
string(9) "int|false"
string(4) "bool"
bool(false)
string|int $param
mixed &$var
int $type = 2
int $maxLength = 0
mixed $driverOptions = NULL
int(2)
int(4)
int(2)
string(5) "00000"
int(1)
int(2048)
int(1)
Error: Trying to clone an uncloneable object of class PDOStatement
Exception: Serialization of 'PDOStatement' is not allowed
array(1) {
  [0]=>
  string(11) "queryString"
}
string(16) "RuntimeException"
