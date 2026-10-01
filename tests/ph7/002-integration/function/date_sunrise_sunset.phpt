--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The three shapes date_sunrise() answers in, and the clock face it puts them on
--INI--
date.timezone=UTC
error_reporting=E_ALL & ~E_DEPRECATED
--FILE--
<?php
/* Every row was read off /usr/bin/php. The deprecation both functions raise is
 * masked here so the values can be read; date_sunrise_deprecated.phpt pins it. */
$ts = mktime(0, 0, 0, 6, 21, 2026);
$lat = 52.37; $lon = 4.90;

var_dump(SUNFUNCS_RET_TIMESTAMP, SUNFUNCS_RET_STRING, SUNFUNCS_RET_DOUBLE);

/* STRING is the default shape, which is why it is 1 and not 0. */
var_dump(date_sunrise($ts, SUNFUNCS_RET_STRING, $lat, $lon) === date_sunrise($ts, 1, $lat, $lon));
foreach ([0, 1, 2] as $f) {
	var_dump(date_sunrise($ts, $f, $lat, $lon), date_sunset($ts, $f, $lat, $lon));
}

/* This pair and date_sun_info() both ask for the sun's upper limb, but at
 * different altitudes -- the stock zenith is 15 arcminutes lower -- so they
 * disagree by a little over two minutes at the same place on the same day. */
$info = date_sun_info($ts, $lat, $lon);
var_dump($info['sunrise'] - date_sunrise($ts, 0, $lat, $lon));

echo "-- zenith\n";
foreach ([90.0, 90.833333, 96.0, 102.0, 108.0] as $z) {
	printf("%9.6f %s %s\n", $z,
		var_export(date_sunrise($ts, 2, $lat, $lon, $z), true),
		var_export(date_sunset($ts, 2, $lat, $lon, $z), true));
}

echo "-- utcOffset folds onto a clock face, and TIMESTAMP ignores it\n";
foreach ([null, 0, 2, -8, 5.5, 26, -30] as $u) {
	printf("%-6s %-7s %-20s %d\n", var_export($u, true),
		var_export(date_sunset($ts, 1, $lat, $lon, 90.833333, $u), true),
		var_export(date_sunset($ts, 2, $lat, $lon, 90.833333, $u), true),
		date_sunset($ts, 0, $lat, $lon, 90.833333, $u));
}

echo "-- the ini defaults every null argument falls back to\n";
var_dump(ini_get('date.default_latitude'), ini_get('date.default_longitude'),
	ini_get('date.sunrise_zenith'), ini_get('date.sunset_zenith'));
var_dump(date_sunrise($ts, 2) === date_sunrise($ts, 2, 31.7667, 35.2333, 90.833333, 0));
/* Each end of the arc reads its OWN zenith directive. */
ini_set('date.sunrise_zenith', '90.0');
ini_set('date.sunset_zenith', '100.0');
var_dump(date_sunrise($ts, 2, $lat, $lon) === date_sunrise($ts, 2, $lat, $lon, 90.0),
         date_sunset($ts, 2, $lat, $lon) === date_sunset($ts, 2, $lat, $lon, 100.0));

echo "-- a day the sun never crosses the altitude is false, either way round\n";
var_dump(date_sunrise($ts, 0, 78.22, 15.63), date_sunset($ts, 0, 78.22, 15.63),
         date_sunrise(mktime(0, 0, 0, 12, 21, 2026), 0, 78.22, 15.63));

echo "-- a non-finite argument is false here and a ValueError in date_sun_info()\n";
var_dump(date_sunrise($ts, 2, NAN, $lon), date_sunrise($ts, 2, $lat, INF),
         date_sunrise($ts, 2, $lat, $lon, NAN), date_sunrise($ts, 2, $lat, $lon, 90.83, INF));

foreach ([3, -1, 99] as $f) {
	try { date_sunrise($ts, $f); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
	try { date_sunset($ts, $f); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
}
?>
--EXPECT--
int(0)
int(1)
int(2)
bool(true)
int(1782011746)
int(1782072520)
string(5) "03:15"
string(5) "20:08"
float(3.262875121213634)
float(20.144646921063966)
int(133)
-- zenith
90.000000 3.384987441738872 20.022534600538727
90.833333 3.262875121213634 20.144646921063966
96.000000 2.4237500077247525 20.983772034552846
102.000000 1.0177735009765883 22.38974854130101
108.000000 false false
-- utcOffset folds onto a clock face, and TIMESTAMP ignores it
NULL   '20:08' 20.144646921063966   1782072520
0      '20:08' 20.144646921063966   1782072520
2      '22:08' 22.144646921063966   1782072520
-8     '12:08' 12.144646921063966   1782072520
5.5    '01:38' 1.6446469210639663   1782072520
26     '22:08' 22.144646921063966   1782072520
-30    '14:08' 14.144646921063966   1782072520
-- the ini defaults every null argument falls back to
string(7) "31.7667"
string(7) "35.2333"
string(9) "90.833333"
string(9) "90.833333"
bool(true)
bool(true)
bool(true)
-- a day the sun never crosses the altitude is false, either way round
bool(false)
bool(false)
bool(false)
-- a non-finite argument is false here and a ValueError in date_sun_info()
bool(false)
bool(false)
bool(false)
bool(false)
date_sunrise(): Argument #2 ($returnFormat) must be one of SUNFUNCS_RET_TIMESTAMP, SUNFUNCS_RET_STRING, or SUNFUNCS_RET_DOUBLE
date_sunset(): Argument #2 ($returnFormat) must be one of SUNFUNCS_RET_TIMESTAMP, SUNFUNCS_RET_STRING, or SUNFUNCS_RET_DOUBLE
date_sunrise(): Argument #2 ($returnFormat) must be one of SUNFUNCS_RET_TIMESTAMP, SUNFUNCS_RET_STRING, or SUNFUNCS_RET_DOUBLE
date_sunset(): Argument #2 ($returnFormat) must be one of SUNFUNCS_RET_TIMESTAMP, SUNFUNCS_RET_STRING, or SUNFUNCS_RET_DOUBLE
date_sunrise(): Argument #2 ($returnFormat) must be one of SUNFUNCS_RET_TIMESTAMP, SUNFUNCS_RET_STRING, or SUNFUNCS_RET_DOUBLE
date_sunset(): Argument #2 ($returnFormat) must be one of SUNFUNCS_RET_TIMESTAMP, SUNFUNCS_RET_STRING, or SUNFUNCS_RET_DOUBLE
