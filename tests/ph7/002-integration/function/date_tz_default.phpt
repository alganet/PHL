--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The script default zone is an IDENTIFIER, and gm* ignore it
--FILE--
<?php
/* date_default_timezone_set() does NOT share the zone grammar DateTimeZone's
 * constructor has. It takes a tz-database IDENTIFIER and nothing else: no
 * leading blank is skipped, no `±HH:MM` is read, and the abbreviation table is
 * not consulted. So `+05:00`, ` Europe/Paris` and `CEST` are all "invalid" here
 * while `new DateTimeZone` takes all three.
 *
 * The ten names that are BOTH an abbreviation and a zone file are where that
 * shows: a `CET` DEFAULT is the FILE, and switches to CEST every summer, where
 * `new DateTimeZone('CET')` is the fixed +01:00 abbreviation that never does.
 *
 * The lookup folds case and the stored name is the caller's own bytes, so
 * get() echoes back what was written.
 *
 * gmdate() and gmmktime() ignore all of it -- and php's zone FIELDS for them
 * are not the default's either: the zone is named `UTC` and abbreviated `GMT`
 * under every default. */
foreach (['UTC','GMT','Europe/Paris','europe/paris','EUROPE/PARIS','CET','GMT0',
          'America/Argentina/Buenos_Aires','+05:00','T','CEST',' Europe/Paris',
          'Europe/Paris ','Nowhere/X',''] as $n) {
    $err = null;
    set_error_handler(function ($no, $msg) use (&$err) { $err = "[$no] $msg"; return true; });
    $r = date_default_timezone_set($n);
    restore_error_handler();
    printf("set[%s] -> %-5s %-70s get=%s\n", $n, var_export($r, true), $err ?? '-',
        date_default_timezone_get());
}

$f = 'Y-m-d H:i:s P T I Z e';
foreach (['UTC', 'CET', 'Europe/Paris', 'America/New_York', 'Australia/Sydney'] as $n) {
    date_default_timezone_set($n);
    echo "== $n\n";
    foreach ([0, 1263546000, 1279180800, 2140668000] as $ts) {
        echo '  date     ', date($f, $ts), "\n";
        echo '  gmdate   ', gmdate($f, $ts), "\n";
        echo '  idate    Z=', idate('Z', $ts), ' I=', idate('I', $ts), "\n";
        $l = localtime($ts, true);
        echo '  localtime ', $l['tm_hour'], ':', $l['tm_min'], ' isdst=', $l['tm_isdst'], "\n";
        $g = getdate($ts);
        echo '  getdate  ', $g['hours'], ':', $g['minutes'], ' ', $g[0], "\n";
    }
    /* Readings no daylight switch touches, so the answer does not depend on the
     * day this test RUNS, which mktime()'s seed would otherwise make it do. */
    echo '  mktime   ', mktime(12, 0, 0, 7, 15, 2010), ' ', mktime(12, 0, 0, 1, 15, 2010), "\n";
    echo '  gmmktime ', gmmktime(12, 0, 0, 7, 15, 2010), "\n";
    foreach (['2010-07-15 12:00:00', '2010-01-15 12:00:00',
              '2010-07-15 12:00:00 +03:00', '@1279180800'] as $s) {
        echo '  strtotime ', $s, ' = ', var_export(strtotime($s), true), "\n";
    }
    echo '  new DateTime ', (new DateTime('2010-07-15 12:00:00'))->format($f), "\n";
    echo '  date_create  ', date_create('2010-01-15 12:00:00')->format($f), "\n";
}
date_default_timezone_set('UTC');
?>
--EXPECT--
set[UTC] -> true  -                                                                      get=UTC
set[GMT] -> true  -                                                                      get=GMT
set[Europe/Paris] -> true  -                                                                      get=Europe/Paris
set[europe/paris] -> true  -                                                                      get=europe/paris
set[EUROPE/PARIS] -> true  -                                                                      get=EUROPE/PARIS
set[CET] -> true  -                                                                      get=CET
set[GMT0] -> true  -                                                                      get=GMT0
set[America/Argentina/Buenos_Aires] -> true  -                                                                      get=America/Argentina/Buenos_Aires
set[+05:00] -> false [8] date_default_timezone_set(): Timezone ID '+05:00' is invalid       get=America/Argentina/Buenos_Aires
set[T] -> false [8] date_default_timezone_set(): Timezone ID 'T' is invalid            get=America/Argentina/Buenos_Aires
set[CEST] -> false [8] date_default_timezone_set(): Timezone ID 'CEST' is invalid         get=America/Argentina/Buenos_Aires
set[ Europe/Paris] -> false [8] date_default_timezone_set(): Timezone ID ' Europe/Paris' is invalid get=America/Argentina/Buenos_Aires
set[Europe/Paris ] -> false [8] date_default_timezone_set(): Timezone ID 'Europe/Paris ' is invalid get=America/Argentina/Buenos_Aires
set[Nowhere/X] -> false [8] date_default_timezone_set(): Timezone ID 'Nowhere/X' is invalid    get=America/Argentina/Buenos_Aires
set[] -> false [8] date_default_timezone_set(): Timezone ID '' is invalid             get=America/Argentina/Buenos_Aires
== UTC
  date     1970-01-01 00:00:00 +00:00 UTC 0 0 UTC
  gmdate   1970-01-01 00:00:00 +00:00 GMT 0 0 UTC
  idate    Z=0 I=0
  localtime 0:0 isdst=0
  getdate  0:0 0
  date     2010-01-15 09:00:00 +00:00 UTC 0 0 UTC
  gmdate   2010-01-15 09:00:00 +00:00 GMT 0 0 UTC
  idate    Z=0 I=0
  localtime 9:0 isdst=0
  getdate  9:0 1263546000
  date     2010-07-15 08:00:00 +00:00 UTC 0 0 UTC
  gmdate   2010-07-15 08:00:00 +00:00 GMT 0 0 UTC
  idate    Z=0 I=0
  localtime 8:0 isdst=0
  getdate  8:0 1279180800
  date     2037-11-01 06:00:00 +00:00 UTC 0 0 UTC
  gmdate   2037-11-01 06:00:00 +00:00 GMT 0 0 UTC
  idate    Z=0 I=0
  localtime 6:0 isdst=0
  getdate  6:0 2140668000
  mktime   1279195200 1263556800
  gmmktime 1279195200
  strtotime 2010-07-15 12:00:00 = 1279195200
  strtotime 2010-01-15 12:00:00 = 1263556800
  strtotime 2010-07-15 12:00:00 +03:00 = 1279184400
  strtotime @1279180800 = 1279180800
  new DateTime 2010-07-15 12:00:00 +00:00 UTC 0 0 UTC
  date_create  2010-01-15 12:00:00 +00:00 UTC 0 0 UTC
== CET
  date     1970-01-01 01:00:00 +01:00 CET 0 3600 CET
  gmdate   1970-01-01 00:00:00 +00:00 GMT 0 0 UTC
  idate    Z=3600 I=0
  localtime 1:0 isdst=0
  getdate  1:0 0
  date     2010-01-15 10:00:00 +01:00 CET 0 3600 CET
  gmdate   2010-01-15 09:00:00 +00:00 GMT 0 0 UTC
  idate    Z=3600 I=0
  localtime 10:0 isdst=0
  getdate  10:0 1263546000
  date     2010-07-15 10:00:00 +02:00 CEST 1 7200 CET
  gmdate   2010-07-15 08:00:00 +00:00 GMT 0 0 UTC
  idate    Z=7200 I=1
  localtime 10:0 isdst=1
  getdate  10:0 1279180800
  date     2037-11-01 07:00:00 +01:00 CET 0 3600 CET
  gmdate   2037-11-01 06:00:00 +00:00 GMT 0 0 UTC
  idate    Z=3600 I=0
  localtime 7:0 isdst=0
  getdate  7:0 2140668000
  mktime   1279188000 1263553200
  gmmktime 1279195200
  strtotime 2010-07-15 12:00:00 = 1279188000
  strtotime 2010-01-15 12:00:00 = 1263553200
  strtotime 2010-07-15 12:00:00 +03:00 = 1279184400
  strtotime @1279180800 = 1279180800
  new DateTime 2010-07-15 12:00:00 +02:00 CEST 1 7200 CET
  date_create  2010-01-15 12:00:00 +01:00 CET 0 3600 CET
== Europe/Paris
  date     1970-01-01 01:00:00 +01:00 CET 0 3600 Europe/Paris
  gmdate   1970-01-01 00:00:00 +00:00 GMT 0 0 UTC
  idate    Z=3600 I=0
  localtime 1:0 isdst=0
  getdate  1:0 0
  date     2010-01-15 10:00:00 +01:00 CET 0 3600 Europe/Paris
  gmdate   2010-01-15 09:00:00 +00:00 GMT 0 0 UTC
  idate    Z=3600 I=0
  localtime 10:0 isdst=0
  getdate  10:0 1263546000
  date     2010-07-15 10:00:00 +02:00 CEST 1 7200 Europe/Paris
  gmdate   2010-07-15 08:00:00 +00:00 GMT 0 0 UTC
  idate    Z=7200 I=1
  localtime 10:0 isdst=1
  getdate  10:0 1279180800
  date     2037-11-01 07:00:00 +01:00 CET 0 3600 Europe/Paris
  gmdate   2037-11-01 06:00:00 +00:00 GMT 0 0 UTC
  idate    Z=3600 I=0
  localtime 7:0 isdst=0
  getdate  7:0 2140668000
  mktime   1279188000 1263553200
  gmmktime 1279195200
  strtotime 2010-07-15 12:00:00 = 1279188000
  strtotime 2010-01-15 12:00:00 = 1263553200
  strtotime 2010-07-15 12:00:00 +03:00 = 1279184400
  strtotime @1279180800 = 1279180800
  new DateTime 2010-07-15 12:00:00 +02:00 CEST 1 7200 Europe/Paris
  date_create  2010-01-15 12:00:00 +01:00 CET 0 3600 Europe/Paris
== America/New_York
  date     1969-12-31 19:00:00 -05:00 EST 0 -18000 America/New_York
  gmdate   1970-01-01 00:00:00 +00:00 GMT 0 0 UTC
  idate    Z=-18000 I=0
  localtime 19:0 isdst=0
  getdate  19:0 0
  date     2010-01-15 04:00:00 -05:00 EST 0 -18000 America/New_York
  gmdate   2010-01-15 09:00:00 +00:00 GMT 0 0 UTC
  idate    Z=-18000 I=0
  localtime 4:0 isdst=0
  getdate  4:0 1263546000
  date     2010-07-15 04:00:00 -04:00 EDT 1 -14400 America/New_York
  gmdate   2010-07-15 08:00:00 +00:00 GMT 0 0 UTC
  idate    Z=-14400 I=1
  localtime 4:0 isdst=1
  getdate  4:0 1279180800
  date     2037-11-01 01:00:00 -05:00 EST 0 -18000 America/New_York
  gmdate   2037-11-01 06:00:00 +00:00 GMT 0 0 UTC
  idate    Z=-18000 I=0
  localtime 1:0 isdst=0
  getdate  1:0 2140668000
  mktime   1279209600 1263574800
  gmmktime 1279195200
  strtotime 2010-07-15 12:00:00 = 1279209600
  strtotime 2010-01-15 12:00:00 = 1263574800
  strtotime 2010-07-15 12:00:00 +03:00 = 1279184400
  strtotime @1279180800 = 1279180800
  new DateTime 2010-07-15 12:00:00 -04:00 EDT 1 -14400 America/New_York
  date_create  2010-01-15 12:00:00 -05:00 EST 0 -18000 America/New_York
== Australia/Sydney
  date     1970-01-01 10:00:00 +10:00 AEST 0 36000 Australia/Sydney
  gmdate   1970-01-01 00:00:00 +00:00 GMT 0 0 UTC
  idate    Z=36000 I=0
  localtime 10:0 isdst=0
  getdate  10:0 0
  date     2010-01-15 20:00:00 +11:00 AEDT 1 39600 Australia/Sydney
  gmdate   2010-01-15 09:00:00 +00:00 GMT 0 0 UTC
  idate    Z=39600 I=1
  localtime 20:0 isdst=1
  getdate  20:0 1263546000
  date     2010-07-15 18:00:00 +10:00 AEST 0 36000 Australia/Sydney
  gmdate   2010-07-15 08:00:00 +00:00 GMT 0 0 UTC
  idate    Z=36000 I=0
  localtime 18:0 isdst=0
  getdate  18:0 1279180800
  date     2037-11-01 17:00:00 +11:00 AEDT 1 39600 Australia/Sydney
  gmdate   2037-11-01 06:00:00 +00:00 GMT 0 0 UTC
  idate    Z=39600 I=1
  localtime 17:0 isdst=1
  getdate  17:0 2140668000
  mktime   1279159200 1263517200
  gmmktime 1279195200
  strtotime 2010-07-15 12:00:00 = 1279159200
  strtotime 2010-01-15 12:00:00 = 1263517200
  strtotime 2010-07-15 12:00:00 +03:00 = 1279184400
  strtotime @1279180800 = 1279180800
  new DateTime 2010-07-15 12:00:00 +10:00 AEST 0 36000 Australia/Sydney
  date_create  2010-01-15 12:00:00 +11:00 AEDT 1 39600 Australia/Sydney
