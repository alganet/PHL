--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Where a zone is: getLocation()'s zone.tab row, and the zones that have none
--SKIPIF--
<?php
// The rows are a --with-system-tzdata php's, whose location is read from
// zone.tab. A php over timelib's bundled database (Homebrew, Windows) stores
// each coordinate as a scaled integer and answers -90/-180 and '' for a zone
// the tab does not list, so only that oracle is skipped.
if (function_exists('zend_version') && timezone_version_get() !== '0.system') {
    echo 'skip this php has a bundled timezone database';
}
?>
--FILE--
<?php
/* Every row here was read off /usr/bin/php over the whole identifier list.
 * getLocation() answers only for a DATABASE zone: a fixed offset declines, and
 * so do the names that are ALSO abbreviations, because the abbreviation table is
 * resolved first and `CET` is a fixed +01:00 rather than the file of that name. */
function show(string $label, $l): void {
    if (!is_array($l)) { printf("%-22s %s\n", $label, var_export($l, true)); return; }
    printf("%-22s %s %11s %11s  %s\n", $label, $l['country_code'],
        var_export($l['latitude'], true), var_export($l['longitude'], true),
        var_export($l['comments'], true));
}

echo "== a listed zone, with and without a note ==\n";
foreach (['America/Sao_Paulo', 'Africa/Abidjan', 'Asia/Kathmandu', 'Europe/Paris'] as $id) {
    show($id, (new DateTimeZone($id))->getLocation());
}

echo "\n== a zone the tab does not list reads `??`, the origin and `?` ==\n";
/* Not the EMPTY comment a listed zone with no note carries -- a literal `?`. */
foreach (['UTC', 'Etc/GMT+5', 'US/Pacific', 'Africa/Timbuktu'] as $id) {
    show($id, (new DateTimeZone($id))->getLocation());
}

echo "\n== only a database zone answers at all ==\n";
foreach (['+02:00', '-05:30', 'CET', 'EST', 'GMT+0', 'MST'] as $id) {
    show($id, (new DateTimeZone($id))->getLocation());
}

echo "\n== the procedural twin is the same door ==\n";
$z = new DateTimeZone('Australia/Eucla');
var_dump(timezone_location_get($z) === $z->getLocation());
show('Australia/Eucla', timezone_location_get($z));

echo "\n== the key order is fixed ==\n";
var_dump(array_keys((new DateTimeZone('Pacific/Chatham'))->getLocation()));
?>
--EXPECT--
== a listed zone, with and without a note ==
America/Sao_Paulo      BR   -23.53333   -46.61666  'Brazil (southeast: GO, DF, MG, ES, RJ, SP, PR, SC, RS)'
Africa/Abidjan         CI     5.31666    -4.03333  ''
Asia/Kathmandu         NP    27.71666    85.31666  ''
Europe/Paris           FR    48.86666     2.33333  ''

== a zone the tab does not list reads `??`, the origin and `?` ==
UTC                    ??         0.0         0.0  '?'
Etc/GMT+5              ??         0.0         0.0  '?'
US/Pacific             ??         0.0         0.0  '?'
Africa/Timbuktu        ??         0.0         0.0  '?'

== only a database zone answers at all ==
+02:00                 false
-05:30                 false
CET                    false
EST                    false
GMT+0                  false
MST                    false

== the procedural twin is the same door ==
bool(true)
Australia/Eucla        AU   -31.71666   128.86666  'Western Australia (Eucla)'

== the key order is fixed ==
array(4) {
  [0]=>
  string(12) "country_code"
  [1]=>
  string(8) "latitude"
  [2]=>
  string(9) "longitude"
  [3]=>
  string(8) "comments"
}
