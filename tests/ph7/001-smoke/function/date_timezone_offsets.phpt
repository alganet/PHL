--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A DateTimeZone offset: php's whole grammar, its range, and its KIND
--FILE--
<?php
/* PHL knew two offset spellings -- `±HH:MM` and `±HHMM` -- and nothing else, so
 * `+01`, `+1:30`, `+01:00:59`, `+010059` and `GMT+01:00` were all "Unknown or bad
 * timezone" for a zone php builds without complaint. Under the missing spellings
 * sat three wrong answers:
 *  - NO RANGE CHECK: `+99:99` was accepted here and used as +100:39, where php
 *    refuses anything at or past 100 hours with a DIFFERENT sentence;
 *  - the SECONDS field had nowhere to go, and php shows it in the zone's NAME
 *    (and in format('e')) while still stopping at the minute for P/p/O/T;
 *  - the zone's KIND was read back off its NAME, which cannot carry it: php calls
 *    `new DateTimeZone('UTC')` an IDENTIFIER and `new DateTimeZone('utc')` an
 *    ABBREVIATION, names them both "UTC", and refuses to compare the two. */
date_default_timezone_set('UTC');

echo "--- what parses, and as what\n";
foreach (['+01:00', '+0100', '+01', '+1', '+1:00', '+1:0', '+01:00:00', '+01:00:59',
          '+010059', '-01:00', '-0', '+0', '+00:00:00', '+99:00', '+24:00', '+23:59',
          '+23:59:59', '+24:00:00', '+99:59:59', '+000', '+100', '+0060', '+0099',
          '+002000', '+00:60', '+01:99', '+00:00:60', '+1234', '+123456', '+1:30',
          '+9959', '+995959', ' +01:00', "\t+01:00", 'GMT+01:00', 'GMT-0500', 'GMT+1',
          'UTC', 'utc', 'GMT', 'gmt', 'Z', 'z', '+00:00', '-00:00'] as $spec) {
    try {
        $z = new DateTimeZone($spec);
        $d = new DateTime('2020-06-15 12:00:00', $z);
        $shown = (array)$d;
        printf("%-12s name=%-12s type=%d off=%-7d P=%-7s p=%-7s O=%-6s T=%-10s e=%s\n",
            "'" . str_replace("\t", '\t', $spec) . "'", $z->getName(), $shown['timezone_type'],
            $z->getOffset($d), $d->format('P'), $d->format('p'), $d->format('O'),
            $d->format('T'), $d->format('e'));
    } catch (Throwable $e) {
        printf("%-12s %s: %s\n", "'" . str_replace("\t", '\t', $spec) . "'",
            get_class($e), $e->getMessage());
    }
}

echo "--- and what does not: php words the two refusals differently\n";
set_error_handler(function ($no, $msg) { echo "  WARN: $msg\n"; return true; });
foreach (['+99:99', '+9999', '+9960', '+996000', '+99:59:60', '+99:60:00', '-99:99',
          '-9999', '+12345', '+00000', '+0000000', '+1000000', '+100:00:00', '+1:2:3',
          '+0:0:0', '+1:00:00', '+0:00:00', '+00:00:0', '0', '00:00', '01:00', '',
          '+', '-', '+01:00 ', 'UT', 'UTC+1', 'gmt+1', 'GMT+', 'Z+01:00', '++01:00',
          '+ 01:00', '+01 :00'] as $spec) {
    try {
        $z = new DateTimeZone($spec);
        printf("%-14s BUILT %s\n", "'$spec'", $z->getName());
    } catch (Throwable $e) {
        printf("%-14s %s\n", "'$spec'", $e->getMessage());
    }
    /* timezone_open() answers the same two sentences as a WARNING and false */
    var_dump(timezone_open($spec));
}
restore_error_handler();

echo "--- the kind travels with the zone, and it is not the name\n";
foreach (['UTC', 'utc', 'GMT', 'gmt', 'Z', 'z', '+01:00', '+00:00:59'] as $spec) {
    $z = new DateTimeZone($spec);
    $d = new DateTime('2020-06-15 12:00:00', $z);
    $shownDate = (array)$d;
    $shownZone = (array)$z;
    $fromDate = (array)$d->getTimezone();
    printf("%-11s zone=%d date=%d getTimezone=%d ser=%s\n", "'$spec'",
        $shownZone['timezone_type'], $shownDate['timezone_type'], $fromDate['timezone_type'],
        serialize($z));
    /* ...but a payload carries only the NAME, so a round trip re-derives it --
     * which is what turns the abbreviation 'utc' into the identifier UTC */
    $back = (array)unserialize(serialize($z));
    printf("            round trip type=%d name=%s\n", $back['timezone_type'], $back['timezone']);
}
$mutable = new DateTime('@0');
$mutable->setTimezone(new DateTimeZone('utc'));
$afterSet = (array)$mutable;
printf("setTimezone('utc') type=%d name=%s\n", $afterSet['timezone_type'], $afterSet['timezone']);

echo "--- a DATE reads its zone back through a narrower grammar than the zone did\n";
foreach (['+00:00:59', '+23:59:59', '+24:00:00', '+24:00:01', '+24:59', '+25:00',
          '+99:59:59', '-25:00:00'] as $spec) {
    $z = new DateTimeZone($spec);
    $d = new DateTime('2020-01-01 00:00:00', $z);
    printf("%-12s off=%-7d zone round trip=%s | date round trip=", $spec, $z->getOffset($d),
        unserialize(serialize($z))->getName());
    try {
        $shown = (array)unserialize(serialize($d));
        echo $shown['timezone'], "\n";
    } catch (Throwable $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}

echo "--- and it decides comparability\n";
foreach ([['UTC', 'utc'], ['utc', 'GMT'], ['UTC', 'UTC'], ['utc', 'utc'],
          ['+01:00', '+0100'], ['+01:00', 'GMT+01:00'], ['Z', 'z']] as $pair) {
    try {
        $r = (new DateTimeZone($pair[0])) <=> (new DateTimeZone($pair[1]));
        printf("%-10s <=> %-10s %d\n", $pair[0], $pair[1], $r);
    } catch (Throwable $e) {
        printf("%-10s <=> %-10s %s: %s\n", $pair[0], $pair[1], get_class($e), $e->getMessage());
    }
}
?>
--EXPECT--
--- what parses, and as what
'+01:00'     name=+01:00       type=1 off=3600    P=+01:00  p=+01:00  O=+0100  T=GMT+0100   e=+01:00
'+0100'      name=+01:00       type=1 off=3600    P=+01:00  p=+01:00  O=+0100  T=GMT+0100   e=+01:00
'+01'        name=+01:00       type=1 off=3600    P=+01:00  p=+01:00  O=+0100  T=GMT+0100   e=+01:00
'+1'         name=+01:00       type=1 off=3600    P=+01:00  p=+01:00  O=+0100  T=GMT+0100   e=+01:00
'+1:00'      name=+01:00       type=1 off=3600    P=+01:00  p=+01:00  O=+0100  T=GMT+0100   e=+01:00
'+1:0'       name=+01:00       type=1 off=3600    P=+01:00  p=+01:00  O=+0100  T=GMT+0100   e=+01:00
'+01:00:00'  name=+01:00       type=1 off=3600    P=+01:00  p=+01:00  O=+0100  T=GMT+0100   e=+01:00
'+01:00:59'  name=+01:00:59    type=1 off=3659    P=+01:00  p=+01:00  O=+0100  T=GMT+0100   e=+01:00:59
'+010059'    name=+01:00:59    type=1 off=3659    P=+01:00  p=+01:00  O=+0100  T=GMT+0100   e=+01:00:59
'-01:00'     name=-01:00       type=1 off=-3600   P=-01:00  p=-01:00  O=-0100  T=GMT-0100   e=-01:00
'-0'         name=+00:00       type=1 off=0       P=+00:00  p=Z       O=+0000  T=GMT+0000   e=+00:00
'+0'         name=+00:00       type=1 off=0       P=+00:00  p=Z       O=+0000  T=GMT+0000   e=+00:00
'+00:00:00'  name=+00:00       type=1 off=0       P=+00:00  p=Z       O=+0000  T=GMT+0000   e=+00:00
'+99:00'     name=+99:00       type=1 off=356400  P=+99:00  p=+99:00  O=+9900  T=GMT+9900   e=+99:00
'+24:00'     name=+24:00       type=1 off=86400   P=+24:00  p=+24:00  O=+2400  T=GMT+2400   e=+24:00
'+23:59'     name=+23:59       type=1 off=86340   P=+23:59  p=+23:59  O=+2359  T=GMT+2359   e=+23:59
'+23:59:59'  name=+23:59:59    type=1 off=86399   P=+23:59  p=+23:59  O=+2359  T=GMT+2359   e=+23:59:59
'+24:00:00'  name=+24:00       type=1 off=86400   P=+24:00  p=+24:00  O=+2400  T=GMT+2400   e=+24:00
'+99:59:59'  name=+99:59:59    type=1 off=359999  P=+99:59  p=+99:59  O=+9959  T=GMT+9959   e=+99:59:59
'+000'       name=+00:00       type=1 off=0       P=+00:00  p=Z       O=+0000  T=GMT+0000   e=+00:00
'+100'       name=+01:00       type=1 off=3600    P=+01:00  p=+01:00  O=+0100  T=GMT+0100   e=+01:00
'+0060'      name=+01:00       type=1 off=3600    P=+01:00  p=+01:00  O=+0100  T=GMT+0100   e=+01:00
'+0099'      name=+01:39       type=1 off=5940    P=+01:39  p=+01:39  O=+0139  T=GMT+0139   e=+01:39
'+002000'    name=+00:20       type=1 off=1200    P=+00:20  p=+00:20  O=+0020  T=GMT+0020   e=+00:20
'+00:60'     name=+01:00       type=1 off=3600    P=+01:00  p=+01:00  O=+0100  T=GMT+0100   e=+01:00
'+01:99'     name=+02:39       type=1 off=9540    P=+02:39  p=+02:39  O=+0239  T=GMT+0239   e=+02:39
'+00:00:60'  name=+00:01       type=1 off=60      P=+00:01  p=+00:01  O=+0001  T=GMT+0001   e=+00:01
'+1234'      name=+12:34       type=1 off=45240   P=+12:34  p=+12:34  O=+1234  T=GMT+1234   e=+12:34
'+123456'    name=+12:34:56    type=1 off=45296   P=+12:34  p=+12:34  O=+1234  T=GMT+1234   e=+12:34:56
'+1:30'      name=+01:30       type=1 off=5400    P=+01:30  p=+01:30  O=+0130  T=GMT+0130   e=+01:30
'+9959'      name=+99:59       type=1 off=359940  P=+99:59  p=+99:59  O=+9959  T=GMT+9959   e=+99:59
'+995959'    name=+99:59:59    type=1 off=359999  P=+99:59  p=+99:59  O=+9959  T=GMT+9959   e=+99:59:59
' +01:00'    name=+01:00       type=1 off=3600    P=+01:00  p=+01:00  O=+0100  T=GMT+0100   e=+01:00
'\t+01:00'   name=+01:00       type=1 off=3600    P=+01:00  p=+01:00  O=+0100  T=GMT+0100   e=+01:00
'GMT+01:00'  name=+01:00       type=1 off=3600    P=+01:00  p=+01:00  O=+0100  T=GMT+0100   e=+01:00
'GMT-0500'   name=-05:00       type=1 off=-18000  P=-05:00  p=-05:00  O=-0500  T=GMT-0500   e=-05:00
'GMT+1'      name=+01:00       type=1 off=3600    P=+01:00  p=+01:00  O=+0100  T=GMT+0100   e=+01:00
'UTC'        name=UTC          type=3 off=0       P=+00:00  p=Z       O=+0000  T=UTC        e=UTC
'utc'        name=UTC          type=2 off=0       P=+00:00  p=Z       O=+0000  T=UTC        e=UTC
'GMT'        name=GMT          type=2 off=0       P=+00:00  p=+00:00  O=+0000  T=GMT        e=GMT
'gmt'        name=GMT          type=2 off=0       P=+00:00  p=+00:00  O=+0000  T=GMT        e=GMT
'Z'          name=Z            type=2 off=0       P=+00:00  p=Z       O=+0000  T=Z          e=Z
'z'          name=Z            type=2 off=0       P=+00:00  p=Z       O=+0000  T=Z          e=Z
'+00:00'     name=+00:00       type=1 off=0       P=+00:00  p=Z       O=+0000  T=GMT+0000   e=+00:00
'-00:00'     name=+00:00       type=1 off=0       P=+00:00  p=Z       O=+0000  T=GMT+0000   e=+00:00
--- and what does not: php words the two refusals differently
'+99:99'       DateTimeZone::__construct(): Timezone offset is out of range (+99:99)
  WARN: timezone_open(): Timezone offset is out of range (+99:99)
bool(false)
'+9999'        DateTimeZone::__construct(): Timezone offset is out of range (+9999)
  WARN: timezone_open(): Timezone offset is out of range (+9999)
bool(false)
'+9960'        DateTimeZone::__construct(): Timezone offset is out of range (+9960)
  WARN: timezone_open(): Timezone offset is out of range (+9960)
bool(false)
'+996000'      DateTimeZone::__construct(): Timezone offset is out of range (+996000)
  WARN: timezone_open(): Timezone offset is out of range (+996000)
bool(false)
'+99:59:60'    DateTimeZone::__construct(): Timezone offset is out of range (+99:59:60)
  WARN: timezone_open(): Timezone offset is out of range (+99:59:60)
bool(false)
'+99:60:00'    DateTimeZone::__construct(): Timezone offset is out of range (+99:60:00)
  WARN: timezone_open(): Timezone offset is out of range (+99:60:00)
bool(false)
'-99:99'       DateTimeZone::__construct(): Timezone offset is out of range (-99:99)
  WARN: timezone_open(): Timezone offset is out of range (-99:99)
bool(false)
'-9999'        DateTimeZone::__construct(): Timezone offset is out of range (-9999)
  WARN: timezone_open(): Timezone offset is out of range (-9999)
bool(false)
'+12345'       DateTimeZone::__construct(): Unknown or bad timezone (+12345)
  WARN: timezone_open(): Unknown or bad timezone (+12345)
bool(false)
'+00000'       DateTimeZone::__construct(): Unknown or bad timezone (+00000)
  WARN: timezone_open(): Unknown or bad timezone (+00000)
bool(false)
'+0000000'     DateTimeZone::__construct(): Unknown or bad timezone (+0000000)
  WARN: timezone_open(): Unknown or bad timezone (+0000000)
bool(false)
'+1000000'     DateTimeZone::__construct(): Unknown or bad timezone (+1000000)
  WARN: timezone_open(): Unknown or bad timezone (+1000000)
bool(false)
'+100:00:00'   DateTimeZone::__construct(): Unknown or bad timezone (+100:00:00)
  WARN: timezone_open(): Unknown or bad timezone (+100:00:00)
bool(false)
'+1:2:3'       DateTimeZone::__construct(): Unknown or bad timezone (+1:2:3)
  WARN: timezone_open(): Unknown or bad timezone (+1:2:3)
bool(false)
'+0:0:0'       DateTimeZone::__construct(): Unknown or bad timezone (+0:0:0)
  WARN: timezone_open(): Unknown or bad timezone (+0:0:0)
bool(false)
'+1:00:00'     DateTimeZone::__construct(): Unknown or bad timezone (+1:00:00)
  WARN: timezone_open(): Unknown or bad timezone (+1:00:00)
bool(false)
'+0:00:00'     DateTimeZone::__construct(): Unknown or bad timezone (+0:00:00)
  WARN: timezone_open(): Unknown or bad timezone (+0:00:00)
bool(false)
'+00:00:0'     DateTimeZone::__construct(): Unknown or bad timezone (+00:00:0)
  WARN: timezone_open(): Unknown or bad timezone (+00:00:0)
bool(false)
'0'            DateTimeZone::__construct(): Unknown or bad timezone (0)
  WARN: timezone_open(): Unknown or bad timezone (0)
bool(false)
'00:00'        DateTimeZone::__construct(): Unknown or bad timezone (00:00)
  WARN: timezone_open(): Unknown or bad timezone (00:00)
bool(false)
'01:00'        DateTimeZone::__construct(): Unknown or bad timezone (01:00)
  WARN: timezone_open(): Unknown or bad timezone (01:00)
bool(false)
''             DateTimeZone::__construct(): Unknown or bad timezone ()
  WARN: timezone_open(): Unknown or bad timezone ()
bool(false)
'+'            DateTimeZone::__construct(): Unknown or bad timezone (+)
  WARN: timezone_open(): Unknown or bad timezone (+)
bool(false)
'-'            DateTimeZone::__construct(): Unknown or bad timezone (-)
  WARN: timezone_open(): Unknown or bad timezone (-)
bool(false)
'+01:00 '      DateTimeZone::__construct(): Unknown or bad timezone (+01:00 )
  WARN: timezone_open(): Unknown or bad timezone (+01:00 )
bool(false)
'UT'           DateTimeZone::__construct(): Unknown or bad timezone (UT)
  WARN: timezone_open(): Unknown or bad timezone (UT)
bool(false)
'UTC+1'        DateTimeZone::__construct(): Unknown or bad timezone (UTC+1)
  WARN: timezone_open(): Unknown or bad timezone (UTC+1)
bool(false)
'gmt+1'        DateTimeZone::__construct(): Unknown or bad timezone (gmt+1)
  WARN: timezone_open(): Unknown or bad timezone (gmt+1)
bool(false)
'GMT+'         DateTimeZone::__construct(): Unknown or bad timezone (GMT+)
  WARN: timezone_open(): Unknown or bad timezone (GMT+)
bool(false)
'Z+01:00'      DateTimeZone::__construct(): Unknown or bad timezone (Z+01:00)
  WARN: timezone_open(): Unknown or bad timezone (Z+01:00)
bool(false)
'++01:00'      DateTimeZone::__construct(): Unknown or bad timezone (++01:00)
  WARN: timezone_open(): Unknown or bad timezone (++01:00)
bool(false)
'+ 01:00'      DateTimeZone::__construct(): Unknown or bad timezone (+ 01:00)
  WARN: timezone_open(): Unknown or bad timezone (+ 01:00)
bool(false)
'+01 :00'      DateTimeZone::__construct(): Unknown or bad timezone (+01 :00)
  WARN: timezone_open(): Unknown or bad timezone (+01 :00)
bool(false)
--- the kind travels with the zone, and it is not the name
'UTC'       zone=3 date=3 getTimezone=3 ser=O:12:"DateTimeZone":2:{s:13:"timezone_type";i:3;s:8:"timezone";s:3:"UTC";}
            round trip type=3 name=UTC
'utc'       zone=2 date=2 getTimezone=2 ser=O:12:"DateTimeZone":2:{s:13:"timezone_type";i:2;s:8:"timezone";s:3:"UTC";}
            round trip type=3 name=UTC
'GMT'       zone=2 date=2 getTimezone=2 ser=O:12:"DateTimeZone":2:{s:13:"timezone_type";i:2;s:8:"timezone";s:3:"GMT";}
            round trip type=2 name=GMT
'gmt'       zone=2 date=2 getTimezone=2 ser=O:12:"DateTimeZone":2:{s:13:"timezone_type";i:2;s:8:"timezone";s:3:"GMT";}
            round trip type=2 name=GMT
'Z'         zone=2 date=2 getTimezone=2 ser=O:12:"DateTimeZone":2:{s:13:"timezone_type";i:2;s:8:"timezone";s:1:"Z";}
            round trip type=2 name=Z
'z'         zone=2 date=2 getTimezone=2 ser=O:12:"DateTimeZone":2:{s:13:"timezone_type";i:2;s:8:"timezone";s:1:"Z";}
            round trip type=2 name=Z
'+01:00'    zone=1 date=1 getTimezone=1 ser=O:12:"DateTimeZone":2:{s:13:"timezone_type";i:1;s:8:"timezone";s:6:"+01:00";}
            round trip type=1 name=+01:00
'+00:00:59' zone=1 date=1 getTimezone=1 ser=O:12:"DateTimeZone":2:{s:13:"timezone_type";i:1;s:8:"timezone";s:9:"+00:00:59";}
            round trip type=1 name=+00:00:59
setTimezone('utc') type=2 name=UTC
--- a DATE reads its zone back through a narrower grammar than the zone did
+00:00:59    off=59      zone round trip=+00:00:59 | date round trip=+00:00:59
+23:59:59    off=86399   zone round trip=+23:59:59 | date round trip=+23:59:59
+24:00:00    off=86400   zone round trip=+24:00 | date round trip=+24:00
+24:00:01    off=86401   zone round trip=+24:00:01 | date round trip=+24:00:01
+24:59       off=89940   zone round trip=+24:59 | date round trip=+24:59
+25:00       off=90000   zone round trip=+25:00 | date round trip=Error: Invalid serialization data for DateTime object
+99:59:59    off=359999  zone round trip=+99:59:59 | date round trip=Error: Invalid serialization data for DateTime object
-25:00:00    off=-90000  zone round trip=-25:00 | date round trip=Error: Invalid serialization data for DateTime object
--- and it decides comparability
UTC        <=> utc        DateException: Cannot compare two different kinds of DateTimeZone objects
utc        <=> GMT        1
UTC        <=> UTC        0
utc        <=> utc        0
+01:00     <=> +0100      0
+01:00     <=> GMT+01:00  0
Z          <=> z          0
