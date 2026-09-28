--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--TEST--
PHL: an unknown bind type is the Error php means to raise (PHL half of the twin pair)
--DESCRIPTION--
php's ext/sqlite3 accepts any integer as a bind $type and only looks at it when
the binding is applied, where a type it has no case for is meant to raise
`Unknown parameter type: N for parameter K`. That sentence is the one answer in
this family that CANNOT be checked against the oracle: php's own format string
spells both numbers with a printf modifier its engine no longer supports, so
asking php to print it ends the request with
`printf "p" modifier is no longer supported` before any of it is produced -- an
uncatchable fatal, and plainly a php bug rather than a contract.

PHL raises php's sentence with the two numbers filled in, as a catchable Error:
it is a programming error rather than a database one, so it stops the run
instead of being reported and carried past the way a failed bind is, and it does
not depend on the error mode. The zend half of this pair pins what php CAN
answer -- that the bind itself is accepted.
--FILE--
<?php
$db = new SQLite3(':memory:');
$st = $db->prepare('SELECT ?');
var_dump($st->bindValue(1, 5, 77));
try { $st->execute(); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
/* getSQL() applies the bindings too, so it meets the same refusal */
try { $st->getSQL(); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
/* the statement itself is untouched: the refusal is about the binding */
var_dump($st->paramCount());
var_dump($db->lastErrorCode());
?>
--EXPECT--
bool(true)
Error: Unknown parameter type: 77 for parameter 1
Error: Unknown parameter type: 77 for parameter 1
int(1)
int(0)
