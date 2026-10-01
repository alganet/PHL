--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Every zone one abbreviation stands for, and the four rules that pick one back
--FILE--
<?php
/* Every row here was read off /usr/bin/php. The abbreviation table is
 * compiled into the engine rather than read from the zone files, so it is the
 * same table on a build that reads the system's zoneinfo and on one that does
 * not. */
$a = DateTimeZone::listAbbreviations();
printf("groups %d, triples %d, identical to the function %s\n",
    count($a), array_sum(array_map('count', $a)),
    var_export($a === timezone_abbreviations_list(), true));

/* The keys are the LOWER-CASE spellings, in the flat table's own order --
 * which is neither alphabetical nor the order the engine stores them in. */
$k = array_keys($a);
printf("keys %s ... %s\n", implode(' ', array_slice($k, 0, 4)), implode(' ', array_slice($k, -4)));
printf("sorted %s\n", var_export($k === (function (array $v) { sort($v); return $v; })($k), true));

/* A whole group. `acdt` is small enough to print and carries the shape: the
 * first triple is the fixed offset `new DateTimeZone('ACDT')` becomes. */
foreach ($a['acdt'] as $i => $r) {
    printf("acdt[%d] %s %6d %s\n", $i, $r['dst'] ? 'dst' : '   ', $r['offset'],
        $r['timezone_id'] === null ? 'NULL' : $r['timezone_id']);
}
/* A group whose triples name no zone at all -- php prints null, it does not
 * leave the row out. */
foreach ($a['z'] as $i => $r) {
    printf("z[%d] %s %6d %s\n", $i, $r['dst'] ? 'dst' : '   ', $r['offset'],
        $r['timezone_id'] === null ? 'NULL' : $r['timezone_id']);
}
/* The first triple of a group IS the abbreviation's own fixed offset. */
$bad = 0;
foreach ($a as $name => $rows) {
    $z = new DateTimeZone($name);
    $d = new DateTime('@0');
    $d->setTimezone($z);
    if ($z->getOffset(new DateTime('@0')) !== $rows[0]['offset']
        || ($d->format('I') === '1') !== $rows[0]['dst']) {
        $bad++;
        echo "first triple disagrees for $name\n";
    }
}
printf("groups whose first triple is not the fixed offset: %d\n", $bad);

echo "== the reverse lookup\n";
/* Rule 1 -- `utc` and `gmt` short-circuit the table, either case, and the
 * offset argument is ignored. */
foreach (['utc', 'UTC', 'gmt', 'GmT'] as $w) {
    printf("%-4s        %-16s %s\n", $w,
        var_export(timezone_name_from_abbr($w), true),
        var_export(timezone_name_from_abbr($w, 7200, 1), true));
}
/* Rules 2 and 3 -- a named offset picks a triple; -1 means "none given", and
 * an offset no triple carries falls back to the first triple anyway. */
foreach ([['cet', -1], ['cet', 3600], ['cet', 7200], ['cet', 999],
          ['CET', -1], ['est', -1], ['est', -18000], ['a', -1]] as [$w, $o]) {
    printf("%-4s %7d %s\n", $w, $o, var_export(timezone_name_from_abbr($w, $o), true));
}
/* Rule 4 -- an abbreviation that matches nothing is decided by the (offset,
 * daylight) pair alone, and this is the only rule $isDST reaches. */
foreach ([[-1, -1], [0, -1], [0, 0], [0, 1], [3600, 0], [3600, 1],
          [-18000, 0], [-18000, 1], [20700, 0], [20700, 1], [999, 0]] as [$o, $d]) {
    printf("zzz %7d %2d %s\n", $o, $d, var_export(timezone_name_from_abbr('zzz', $o, $d), true));
}
/* A triple that names no zone answers false -- the search succeeded, it just
 * has nothing to give, so rule 4 is never reached. */
printf("z %s\n", var_export(timezone_name_from_abbr('z'), true));
printf("'' %s\n", var_export(timezone_name_from_abbr(''), true));
--EXPECT--
groups 144, triples 1127, identical to the function true
keys acdt acst addt adt ... w x y z
sorted false
acdt[0] dst  37800 Australia/Adelaide
acdt[1] dst  37800 Australia/Broken_Hill
acdt[2] dst  37800 Australia/Darwin
acdt[3] dst  37800 Australia/North
acdt[4] dst  37800 Australia/South
acdt[5] dst  37800 Australia/Yancowinna
z[0]          0 NULL
groups whose first triple is not the fixed offset: 0
== the reverse lookup
utc         'UTC'            'UTC'
UTC         'UTC'            'UTC'
gmt         'UTC'            'UTC'
GmT         'UTC'            'UTC'
cet       -1 'Europe/Berlin'
cet     3600 'Europe/Berlin'
cet     7200 'Europe/Kaliningrad'
cet      999 'Europe/Berlin'
CET       -1 'Europe/Berlin'
est       -1 'America/New_York'
est   -18000 'America/New_York'
a         -1 false
zzz      -1 -1 false
zzz       0 -1 false
zzz       0  0 'Europe/London'
zzz       0  1 'Atlantic/Azores'
zzz    3600  0 'Europe/Paris'
zzz    3600  1 'Europe/London'
zzz  -18000  0 'America/New_York'
zzz  -18000  1 'America/Chicago'
zzz   20700  0 'Asia/Katmandu'
zzz   20700  1 false
zzz     999  0 false
z false
'' false
