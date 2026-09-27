--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php's MILITARY timezones: one letter is a whole-hour offset
--FILE--
<?php
/* A single letter is a zone to php: `A`..`I` are +1..+9, `K`..`M` +10..+12 and
 * `N`..`Y` -1..-12, with `Z` the zero ISO 8601 spells and no `J` at all (which
 * is why `J` alone is a name php looks up and does not find). The name is the
 * UPPERCASE letter whatever case the string used, and its timezone_type is 2.
 *
 * No tz database is involved -- the whole set is arithmetic -- so this engine
 * answers it exactly, at both doors: the date STRING (`1234t` is 12:34 at -07:00)
 * and `new DateTimeZone('t')`, which is what lets a date carrying one survive
 * serialize()/unserialize(). */
date_default_timezone_set('UTC');

echo "--- every letter, in a date string\n";
foreach (str_split('ABCDEFGHIJKLMNOPQRSTUVWXYZ') as $c) {
    $spec = "2020-01-01 12:00 $c";
    try {
        $d = new DateTime($spec);
        $a = (array)$d;
        printf("%s %s tt=%d tz=%s\n", $c, $d->format('P'), $a['timezone_type'], $a['timezone']);
    } catch (Throwable $e) {
        printf("%s %s\n", $c, $e->getMessage());
    }
}

echo "--- the lowercase half answers the same zone\n";
foreach (['a', 'j', 'm', 'n', 'y', 'z'] as $c) {
    $spec = "2020-01-01 12:00 $c";
    try {
        $d = new DateTime($spec);
        $a = (array)$d;
        printf("%s %s tz=%s\n", $c, $d->format('P'), $a['timezone']);
    } catch (Throwable $e) {
        printf("%s %s\n", $c, $e->getMessage());
    }
}

echo "--- attached to a clock, and beside one\n";
foreach (['1234t', '1234 t', '12:00 t', '2020-01-01T12:00:00T', '2020-01-01(A)',
          'a1', 'a 1'] as $spec) {
    try {
        $d = new DateTime($spec);
        $a = (array)$d;
        printf("%-22s %s tz=%s\n", $spec, $d->format('H:i:s P'), $a['timezone']);
    } catch (Throwable $e) {
        printf("%-22s %s\n", $spec, $e->getMessage());
    }
}

echo "--- new DateTimeZone(), and the round trip through it\n";
foreach (['T', 't', 'A', 'M', 'Y', 'K', 'Z', 'J', 'j'] as $name) {
    try {
        $z = new DateTimeZone($name);
        printf("%-3s %s %d\n", $name, $z->getName(), $z->getOffset(new DateTime('2020-01-01')));
    } catch (Throwable $e) {
        printf("%-3s %s\n", $name, $e->getMessage());
    }
}
$d = new DateTime('2020-01-01 12:00 T');
echo serialize($d), "\n";
$r = unserialize(serialize($d));
echo $r->format('Y-m-d H:i:s e P'), "\n";
var_dump($d == $r);
echo json_encode((array)$d), "\n";
echo $d->setTimezone(new DateTimeZone('UTC'))->format('Y-m-d H:i:s P'), "\n";
?>
--EXPECT--
--- every letter, in a date string
A +01:00 tt=2 tz=A
B +02:00 tt=2 tz=B
C +03:00 tt=2 tz=C
D +04:00 tt=2 tz=D
E +05:00 tt=2 tz=E
F +06:00 tt=2 tz=F
G +07:00 tt=2 tz=G
H +08:00 tt=2 tz=H
I +09:00 tt=2 tz=I
J Failed to parse time string (2020-01-01 12:00 J) at position 17 (J): The timezone could not be found in the database
K +10:00 tt=2 tz=K
L +11:00 tt=2 tz=L
M +12:00 tt=2 tz=M
N -01:00 tt=2 tz=N
O -02:00 tt=2 tz=O
P -03:00 tt=2 tz=P
Q -04:00 tt=2 tz=Q
R -05:00 tt=2 tz=R
S -06:00 tt=2 tz=S
T -07:00 tt=2 tz=T
U -08:00 tt=2 tz=U
V -09:00 tt=2 tz=V
W -10:00 tt=2 tz=W
X -11:00 tt=2 tz=X
Y -12:00 tt=2 tz=Y
Z +00:00 tt=2 tz=Z
--- the lowercase half answers the same zone
a +01:00 tz=A
j Failed to parse time string (2020-01-01 12:00 j) at position 17 (j): The timezone could not be found in the database
m +12:00 tz=M
n -01:00 tz=N
y -12:00 tz=Y
z +00:00 tz=Z
--- attached to a clock, and beside one
1234t                  12:34:00 -07:00 tz=T
1234 t                 12:34:00 -07:00 tz=T
12:00 t                12:00:00 -07:00 tz=T
2020-01-01T12:00:00T   12:00:00 -07:00 tz=T
2020-01-01(A)          00:00:00 +01:00 tz=A
a1                     Failed to parse time string (a1) at position 1 (1): Unexpected character
a 1                    Failed to parse time string (a 1) at position 2 (1): Unexpected character
--- new DateTimeZone(), and the round trip through it
T   T -25200
t   T -25200
A   A 3600
M   M 43200
Y   Y -43200
K   K 36000
Z   Z 0
J   DateTimeZone::__construct(): Unknown or bad timezone (J)
j   DateTimeZone::__construct(): Unknown or bad timezone (j)
O:8:"DateTime":3:{s:4:"date";s:26:"2020-01-01 12:00:00.000000";s:13:"timezone_type";i:2;s:8:"timezone";s:1:"T";}
2020-01-01 12:00:00 T -07:00
bool(true)
{"date":"2020-01-01 12:00:00.000000","timezone_type":2,"timezone":"T"}
2020-01-01 19:00:00 +00:00
