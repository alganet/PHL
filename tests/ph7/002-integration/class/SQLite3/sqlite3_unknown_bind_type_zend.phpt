--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--TEST--
php: an unknown bind type is ACCEPTED and cannot be reported (zend half of the twin pair)
--DESCRIPTION--
php validates a bind $type nowhere: bindValue() answers true for any integer,
and the type is looked at only when the binding is applied. What happens there
is not pinned here and cannot be -- php's sentence for an unknown type spells
its two numbers with a printf modifier its engine no longer supports, so
applying such a binding ends the request with an uncatchable
`printf "p" modifier is no longer supported` and prints nothing of what it meant
to say. This half therefore stops at the last thing php can answer.
--FILE--
<?php
$db = new SQLite3(':memory:');
$st = $db->prepare('SELECT ?');
var_dump($st->bindValue(1, 5, 77));
var_dump($st->bindValue(1, 5, -3));
/* getSQL() would apply them too, so even ASKING for the SQL back is the fatal:
   there is nothing further to pin. */
var_dump($st->paramCount());
?>
--EXPECT--
bool(true)
bool(true)
int(1)
