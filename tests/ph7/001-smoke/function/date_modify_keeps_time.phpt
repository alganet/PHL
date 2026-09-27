--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
modify() writes only the fields its string really set
--FILE--
<?php
/* php's modify() copies from the parsed vector only what the modifier SPELLED,
 * so a string naming a date and no time of day leaves the receiver's clock
 * where it was: `(new DateTime('2020-06-15 12:30:45'))->modify('2020-01-31')`
 * is 12:30:45 there and was midnight here. A fresh parse is the other rule --
 * a date with no time ZEROES the clock -- so the constructor and strtotime()
 * keep answering midnight for the same string.
 *
 * The sub-second clock rides the same rule: a time of day sets the whole clock
 * including the microseconds (zero unless the string carried a fraction), and
 * anything that names no time at all leaves them alone. */
date_default_timezone_set('UTC');

$base  = '2020-06-15 12:30:45';
$baseU = '2020-06-15 12:30:45.123456';

echo "--- a modifier with no time of day keeps the receiver's\n";
foreach (['2020-01-31', '2020-01-31 +1 month', '1 january 2020', 'january',
          'first day of next month', 'last day of february 2021', '+1 day',
          '-21 hour first day of january', '2020-01-31 08:00', 'noon',
          'tomorrow', '15:00', '2020-01-31T00:00:00Z'] as $spec) {
    $d = new DateTime($base);
    $d->modify($spec);
    $i = (new DateTimeImmutable($base))->modify($spec);
    printf("%-32s mod=%s imm=%s\n", "'$spec'", $d->format('Y-m-d H:i:s'),
        $i->format('Y-m-d H:i:s'));
}

echo "--- a fresh parse zeroes it instead\n";
foreach (['2020-01-31', '1 january 2020', 'january 2020'] as $spec) {
    printf("%-18s new=%s strtotime=%s\n", "'$spec'",
        (new DateTime($spec))->format('Y-m-d H:i:s'),
        date('Y-m-d H:i:s', strtotime($spec, strtotime($base))));
}

echo "--- and the sub-second clock rides the same rule\n";
foreach (['2020-01-01 08:00', '2020-01-01', '+1 day', 'noon',
          '05:06:07.000009', '@1600000000.5'] as $spec) {
    $d = new DateTime($baseU);
    $d->modify($spec);
    printf("%-22s %s\n", "'$spec'", $d->format('Y-m-d H:i:s.u'));
}

echo "--- the procedural door answers the same\n";
foreach (['2020-01-31', '2020-01-31 08:00'] as $spec) {
    $d = date_create($baseU);
    date_modify($d, $spec);
    printf("%-20s %s\n", "'$spec'", date_format($d, 'Y-m-d H:i:s.u'));
}
?>
--EXPECT--
--- a modifier with no time of day keeps the receiver's
'2020-01-31'                     mod=2020-01-31 12:30:45 imm=2020-01-31 12:30:45
'2020-01-31 +1 month'            mod=2020-03-02 12:30:45 imm=2020-03-02 12:30:45
'1 january 2020'                 mod=2020-01-01 12:30:45 imm=2020-01-01 12:30:45
'january'                        mod=2020-01-15 12:30:45 imm=2020-01-15 12:30:45
'first day of next month'        mod=2020-07-01 12:30:45 imm=2020-07-01 12:30:45
'last day of february 2021'      mod=2021-02-28 12:30:45 imm=2021-02-28 12:30:45
'+1 day'                         mod=2020-06-16 12:30:45 imm=2020-06-16 12:30:45
'-21 hour first day of january'  mod=2019-12-31 15:30:45 imm=2019-12-31 15:30:45
'2020-01-31 08:00'               mod=2020-01-31 08:00:00 imm=2020-01-31 08:00:00
'noon'                           mod=2020-06-15 12:00:00 imm=2020-06-15 12:00:00
'tomorrow'                       mod=2020-06-16 00:00:00 imm=2020-06-16 00:00:00
'15:00'                          mod=2020-06-15 15:00:00 imm=2020-06-15 15:00:00
'2020-01-31T00:00:00Z'           mod=2020-01-31 00:00:00 imm=2020-01-31 00:00:00
--- a fresh parse zeroes it instead
'2020-01-31'       new=2020-01-31 00:00:00 strtotime=2020-01-31 00:00:00
'1 january 2020'   new=2020-01-01 00:00:00 strtotime=2020-01-01 00:00:00
'january 2020'     new=2020-01-01 00:00:00 strtotime=2020-01-01 00:00:00
--- and the sub-second clock rides the same rule
'2020-01-01 08:00'     2020-01-01 08:00:00.000000
'2020-01-01'           2020-01-01 12:30:45.123456
'+1 day'               2020-06-16 12:30:45.123456
'noon'                 2020-06-15 12:00:00.000000
'05:06:07.000009'      2020-06-15 05:06:07.000009
'@1600000000.5'        2020-09-13 12:26:40.500000
--- the procedural door answers the same
'2020-01-31'         2020-01-31 12:30:45.123456
'2020-01-31 08:00'   2020-01-31 08:00:00.000000
