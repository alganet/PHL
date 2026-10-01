--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Where the sun is on a given day, and the nine answers date_sun_info() gives
--INI--
date.timezone=UTC
--FILE--
<?php
/* Every row was read off /usr/bin/php. The day asked about is the LOCAL
 * calendar date but the answers are timestamps, so the same instant asked in
 * two zones a date apart is answered for two different days. */
function show($label, $a) {
	echo $label, "\n";
	foreach ($a as $k => $v) {
		printf("  %-28s %s\n", $k, var_export($v, true));
	}
}
$ts = mktime(0, 0, 0, 6, 21, 2026);
show('Amsterdam, midsummer, UTC', date_sun_info($ts, 52.37, 4.90));

/* Tokyo is on the same calendar date as UTC at this instant and answers
 * identically; Los Angeles is still on the day before and answers for it. */
date_default_timezone_set('Asia/Tokyo');
$tokyo = date_sun_info($ts, 52.37, 4.90);
date_default_timezone_set('America/Los_Angeles');
$la = date_sun_info($ts, 52.37, 4.90);
date_default_timezone_set('UTC');
var_dump($tokyo === date_sun_info($ts, 52.37, 4.90));
var_dump($la['transit'] < $tokyo['transit'], $tokyo['transit'] - $la['transit']);

/* A polar summer: the sun never sets, and php says so with TRUE in both
 * halves of every pair it never crossed. The transit happens regardless. */
show('Longyearbyen, midsummer', date_sun_info($ts, 78.22, 15.63));
/* A polar winter is FALSE, and the twilights come back one band at a time. */
show('Longyearbyen, midwinter', date_sun_info(mktime(0, 0, 0, 12, 21, 2026), 78.22, 15.63));

/* A rise east of the meridian is a negative offset from the base day, and php
 * truncates the SUM rather than the offset -- Sydney reads one second earlier
 * than truncating the offset alone would give. */
show('Sydney, new year', date_sun_info(mktime(0, 0, 0, 1, 1, 2026), -33.87, 151.21));

/* Both coordinates are screened, and only these two: the sunrise/sunset pair
 * answers false for the same value instead. */
foreach ([[NAN, 0.0], [INF, 0.0], [0.0, NAN], [0.0, -INF]] as [$lat, $lon]) {
	try { date_sun_info($ts, $lat, $lon); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
}
try { date_sun_info($ts, 52.37); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
try { date_sun_info('x', 52.37, 4.90); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
/* A latitude past the pole is not screened and simply never resolves. */
var_dump(date_sun_info($ts, 91.0, 0.0)['sunrise']);
?>
--EXPECT--
Amsterdam, midsummer, UTC
  sunrise                      1782011879
  sunset                       1782072387
  transit                      1782042133
  civil_twilight_begin         1782008895
  civil_twilight_end           1782075371
  nautical_twilight_begin      1782003979
  nautical_twilight_end        1782080287
  astronomical_twilight_begin  true
  astronomical_twilight_end    true
bool(true)
bool(true)
int(86413)
Longyearbyen, midsummer
  sunrise                      true
  sunset                       true
  transit                      1782039557
  civil_twilight_begin         true
  civil_twilight_end           true
  nautical_twilight_begin      true
  nautical_twilight_end        true
  astronomical_twilight_begin  true
  astronomical_twilight_end    true
Longyearbyen, midwinter
  sunrise                      false
  sunset                       false
  transit                      1797850531
  civil_twilight_begin         false
  civil_twilight_end           false
  nautical_twilight_begin      1797847073
  nautical_twilight_end        1797853989
  astronomical_twilight_begin  1797835025
  astronomical_twilight_end    1797866037
Sydney, new year
  sunrise                      1767206856
  sunset                       1767258567
  transit                      1767232711
  civil_twilight_begin         1767205124
  civil_twilight_end           1767260299
  nautical_twilight_begin      1767202987
  nautical_twilight_end        1767262436
  astronomical_twilight_begin  1767200652
  astronomical_twilight_end    1767264771
date_sun_info(): Argument #2 ($latitude) must be finite
date_sun_info(): Argument #2 ($latitude) must be finite
date_sun_info(): Argument #3 ($longitude) must be finite
date_sun_info(): Argument #3 ($longitude) must be finite
date_sun_info() expects exactly 3 arguments, 2 given
date_sun_info(): Argument #1 ($timestamp) must be of type int, string given
bool(false)
