--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The switches a zone makes, and the 32-bit horizon the walk stops at
--FILE--
<?php
/* Every row here was read off /usr/bin/php. The two bounds are EXCLUSIVE at
 * both ends and the end DEFAULTS to 2147483647 -- timelib's 32-bit horizon --
 * which is what decides whether the walk extrapolates past the data. */
function show(string $label, $t): void {
    if (!is_array($t)) { printf("%-34s %s\n", $label, var_export($t, true)); return; }
    printf("%-34s %d row(s)\n", $label, count($t));
    foreach ($t as $r) {
        printf("    %14d %s %7d %s %s\n",
            $r['ts'], $r['time'], $r['offset'], $r['isdst'] ? 'dst' : '   ', $r['abbr']);
    }
}

$ny = new DateTimeZone('America/New_York');

echo "== a plain window ==\n";
show('NY 2001..2004', $ny->getTransitions(1000000000, 1010000000));

echo "\n== both bounds are exclusive ==\n";
/* 1004248800 is a switch. As the BEGIN it is the synthesized row and is not
 * repeated; as the END it is left out entirely. */
show('begin ON the switch', $ny->getTransitions(1004248800, 1004248801));
show('begin one second before', $ny->getTransitions(1004248799, 1004248801));
show('end ON the switch', $ny->getTransitions(1004248799, 1004248800));

echo "\n== a reversed range is just the synthesized row ==\n";
show('begin after end', $ny->getTransitions(1100000000, 1000000000));

echo "\n== past the data, the footer rule generates ==\n";
/* The files stop in 2037. A window in 2040 is answered from `EST5EDT,M3.2.0,
 * M11.1.0` read as a rule, not from a row. */
show('NY 2040', $ny->getTransitions(2216160000, 2247696000));

echo "\n== ...but only as far as the end allows ==\n";
/* Defaulted, the end is 2147483647, and NY's next switch after the last row is
 * 2038-03-14, which is past it. So the walk stops with the data. */
$all = $ny->getTransitions();
printf("%-34s %d row(s), first ts %d, last ts %d\n",
    'NY defaulted end', count($all), $all[0]['ts'], $all[count($all) - 1]['ts']);
$one = $ny->getTransitions(1000000000);
printf("%-34s %d row(s), last ts %d\n", 'NY begin only', count($one), $one[count($one) - 1]['ts']);
$past = $ny->getTransitions(1000000000, 2152162801);
printf("%-34s %d row(s), last ts %d\n", 'NY end past 2038', count($past), $past[count($past) - 1]['ts']);

echo "\n== a southern zone, whose DST spans the new year ==\n";
show('Sydney 2040', (new DateTimeZone('Australia/Sydney'))->getTransitions(2216160000, 2247696000));

echo "\n== a zone that stopped switching ==\n";
show('Tokyo 2040', (new DateTimeZone('Asia/Tokyo'))->getTransitions(2216160000, 2247696000));

echo "\n== what is not a database zone ==\n";
/* An identifier answers; a fixed offset and an abbreviation do not. `CET` is
 * the abbreviation, a fixed +01:00, not the file of that name. */
show('UTC', (new DateTimeZone('UTC'))->getTransitions(0, 100000000));
show('+02:00', (new DateTimeZone('+02:00'))->getTransitions(0, 100000000));
show('CET', (new DateTimeZone('CET'))->getTransitions(0, 100000000));

echo "\n== the procedural twin ==\n";
show('Lisbon 2001', timezone_transitions_get(timezone_open('Europe/Lisbon'), 1000000000, 1010000000));
printf("%-34s %d row(s)\n", 'Lisbon defaulted end',
    count(timezone_transitions_get(timezone_open('Europe/Lisbon'))));
?>
--EXPECT--
== a plain window ==
NY 2001..2004                      2 row(s)
        1000000000 2001-09-09T01:46:40+00:00  -14400 dst EDT
        1004248800 2001-10-28T06:00:00+00:00  -18000     EST

== both bounds are exclusive ==
begin ON the switch                1 row(s)
        1004248800 2001-10-28T06:00:00+00:00  -18000     EST
begin one second before            2 row(s)
        1004248799 2001-10-28T05:59:59+00:00  -14400 dst EDT
        1004248800 2001-10-28T06:00:00+00:00  -18000     EST
end ON the switch                  1 row(s)
        1004248799 2001-10-28T05:59:59+00:00  -14400 dst EDT

== a reversed range is just the synthesized row ==
begin after end                    1 row(s)
        1100000000 2004-11-09T11:33:20+00:00  -18000     EST

== past the data, the footer rule generates ==
NY 2040                            3 row(s)
        2216160000 2040-03-24T00:00:00+00:00  -14400 dst EDT
        2235621600 2040-11-04T06:00:00+00:00  -18000     EST
        2246511600 2041-03-10T07:00:00+00:00  -14400 dst EDT

== ...but only as far as the end allows ==
NY defaulted end                   237 row(s), first ts -9223372036854775808, last ts 2140668000
NY begin only                      74 row(s), last ts 2140668000
NY end past 2038                   75 row(s), last ts 2152162800

== a southern zone, whose DST spans the new year ==
Sydney 2040                        3 row(s)
        2216160000 2040-03-24T00:00:00+00:00   39600 dst AEDT
        2216822400 2040-03-31T16:00:00+00:00   36000     AEST
        2233152000 2040-10-06T16:00:00+00:00   39600 dst AEDT

== a zone that stopped switching ==
Tokyo 2040                         1 row(s)
        2216160000 2040-03-24T00:00:00+00:00   32400     JST

== what is not a database zone ==
UTC                                1 row(s)
                 0 1970-01-01T00:00:00+00:00       0     UTC
+02:00                             false
CET                                false

== the procedural twin ==
Lisbon 2001                        2 row(s)
        1000000000 2001-09-09T01:46:40+00:00    3600 dst WEST
        1004230800 2001-10-28T01:00:00+00:00       0     WET
Lisbon defaulted end               226 row(s)
