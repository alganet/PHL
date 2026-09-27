--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PDOStatement::$queryString is read only — to a store and an unset, and to nothing else
--DESCRIPTION--
php's PDOStatement declares `queryString` and its write_property handler refuses:
`$stmt->queryString = 'x'` and `unset($stmt->queryString)` are both
`Error: Property queryString is read only`, a sentence that names neither the
class nor the `$`. Reflection's setValue() goes through the same handler and
takes the same refusal.

Everything that reaches the property by POINTER instead bypasses that handler in
php and is left alone: a compound assign, `??=` and a reference bind all write.
And the refusal belongs to a statement a driver BUILT — a bare
`new PDOStatement()` has no cursor for the property to describe, and takes the
store like any other typed property.
--FILE--
<?php
$qsDb = new PDO('sqlite::memory:');
$qsDb->exec('CREATE TABLE qs (a TEXT)');
$qsDb->exec("INSERT INTO qs VALUES ('v')");
$qsSql = 'SELECT * FROM qs';
$qsSt = $qsDb->query($qsSql);
var_dump($qsSt->queryString);
try { $qsSt->queryString = 'REPLACED'; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
var_dump($qsSt->queryString);
try { unset($qsSt->queryString); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
var_dump($qsSt->queryString);
try { (new ReflectionProperty('PDOStatement', 'queryString'))->setValue($qsSt, 'R'); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
var_dump($qsSt->queryString);
/* a destructuring target is a store too */
try { [$qsSt->queryString] = ['L']; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
var_dump($qsSt->queryString);

/* the paths that take a pointer are php's own exception to it */
$qsSt->queryString .= '!';
var_dump($qsSt->queryString);
$qsSt->queryString ??= 'never';
var_dump($qsSt->queryString);
$qsRef = &$qsSt->queryString;
$qsRef = 'THROUGH THE REFERENCE';
var_dump($qsSt->queryString);

/* the property is still an ordinary declared one to everything that ASKS */
var_dump(isset($qsSt->queryString), property_exists($qsSt, 'queryString'),
    (new ReflectionProperty('PDOStatement', 'queryString'))->isReadOnly(),
    (new ReflectionProperty('PDOStatement', 'queryString'))->getValue($qsSt));

/* a statement nobody built a cursor for takes the write */
$qsBare = new PDOStatement();
$qsBare->queryString = 'MINE';
var_dump($qsBare->queryString);
/* and one from prepare() refuses like one from query() */
$qsPrep = $qsDb->prepare($qsSql);
try { $qsPrep->queryString = 'x'; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
var_dump($qsPrep->queryString);
?>
--EXPECT--
string(16) "SELECT * FROM qs"
Error: Property queryString is read only
string(16) "SELECT * FROM qs"
Error: Property queryString is read only
string(16) "SELECT * FROM qs"
Error: Property queryString is read only
string(16) "SELECT * FROM qs"
Error: Property queryString is read only
string(16) "SELECT * FROM qs"
string(17) "SELECT * FROM qs!"
string(17) "SELECT * FROM qs!"
string(21) "THROUGH THE REFERENCE"
bool(true)
bool(true)
bool(false)
string(21) "THROUGH THE REFERENCE"
string(4) "MINE"
Error: Property queryString is read only
string(16) "SELECT * FROM qs"
