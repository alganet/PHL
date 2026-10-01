--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
date.timezone and date_default_timezone_set() are one rule, and php latches on the function
--FILE--
<?php
/* `date.timezone` and date_default_timezone_set() are ONE rule, enforced where
 * php enforces it -- on the write -- so a name neither table resolves is
 * REFUSED with a warning naming the value it kept instead, and the directive is
 * left alone. `+05:00` is such a name here: the directive takes an IDENTIFIER,
 * where `new DateTimeZone` takes the offset grammar as well.
 *
 * They are not symmetric, though. php LATCHES on date_default_timezone_set():
 * after a script has named a zone outright, a later ini_set() still records the
 * DIRECTIVE and the default no longer follows it. And the function never writes
 * the directive, so ini_get() goes on reading whatever ini_set() last put
 * there. */
function dateTzIniShow(string $tag): void
{
    printf("%-10s ini=%-16s def=%s\n", $tag, ini_get('date.timezone'), date_default_timezone_get());
}
dateTzIniShow('start');
var_dump(ini_set('date.timezone', 'Europe/Paris'));  dateTzIniShow('ini1');
var_dump(ini_set('date.timezone', 'Asia/Tokyo'));    dateTzIniShow('ini2');
var_dump(date_default_timezone_set('Europe/Berlin')); dateTzIniShow('set');
var_dump(ini_set('date.timezone', 'Europe/Rome'));   dateTzIniShow('ini3');
var_dump(@ini_set('date.timezone', 'nowhere/x'));    dateTzIniShow('bad-ini');
var_dump(@date_default_timezone_set('nowhere/x'));   dateTzIniShow('bad-set');
var_dump(@ini_set('date.timezone', '+05:00'));       dateTzIniShow('off-ini');
var_dump(@ini_set('date.timezone', 'CET'));          dateTzIniShow('cet-ini');
date_default_timezone_set('UTC');
?>
--EXPECT--
start      ini=UTC              def=UTC
string(3) "UTC"
ini1       ini=Europe/Paris     def=Europe/Paris
string(12) "Europe/Paris"
ini2       ini=Asia/Tokyo       def=Asia/Tokyo
bool(true)
set        ini=Asia/Tokyo       def=Europe/Berlin
string(10) "Asia/Tokyo"
ini3       ini=Europe/Rome      def=Europe/Berlin
bool(false)
bad-ini    ini=Europe/Rome      def=Europe/Berlin
bool(false)
bad-set    ini=Europe/Rome      def=Europe/Berlin
bool(false)
off-ini    ini=Europe/Rome      def=Europe/Berlin
string(11) "Europe/Rome"
cet-ini    ini=CET              def=Europe/Berlin
