--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
gregoriantojd() answers the serial day number, 0 for no such date
--FILE--
<?php
/* The serial day number (SDN) is a plain day counter: SDN 1 is 25 November
 * 4714 B.C. Two rules of the conversion are only visible from outside: ZERO is
 * the "no such date" answer rather than an exception, and a POSITIVE answer
 * does not mean the input was valid -- the package screens month 1-12 and day
 * 1-31 and nothing else, so 31 February converts happily. php's own validity
 * test is to convert back and compare. */
function cal_g2j(int $m, int $d, int $y): void {
    printf("%4d/%-3d/%-6d -> %d\n", $m, $d, $y, gregoriantojd($m, $d, $y));
}
function cal_gregoriantojd_rules(): void {
    echo "## ordinary dates\n";
    cal_g2j(1, 1, 1970);
    cal_g2j(1, 1, 1990);
    cal_g2j(10, 15, 1582);
    cal_g2j(2, 29, 2000);
    cal_g2j(12, 31, 9999);

    echo "## the epoch of the counter, and the day before it\n";
    cal_g2j(11, 25, -4714);
    cal_g2j(11, 24, -4714);
    cal_g2j(10, 31, -4714);

    echo "## there is no year 0, and no month 0 or 13\n";
    cal_g2j(1, 1, 0);
    cal_g2j(1, 1, -1);
    cal_g2j(0, 1, 2000);
    cal_g2j(13, 1, 2000);
    cal_g2j(1, 0, 2000);
    cal_g2j(1, 32, 2000);

    echo "## a positive answer is not a valid date\n";
    cal_g2j(2, 31, 2000);
    printf("2/31/2000 converts back to %s\n", jdtogregorian(gregoriantojd(2, 31, 2000)));

    echo "## php reads the arguments as int and NARROWS them, so 2^32 wraps\n";
    cal_g2j(1, 1, 4294967297);
    printf("%d\n", gregoriantojd(1, 1, PHP_INT_MAX));
    printf("%d\n", gregoriantojd(1, 1, PHP_INT_MIN));
    printf("%d\n", gregoriantojd(4294967297, 1, 2000));
}
cal_gregoriantojd_rules();
--EXPECT--
## ordinary dates
   1/1  /1970   -> 2440588
   1/1  /1990   -> 2447893
  10/15 /1582   -> 2299161
   2/29 /2000   -> 2451604
  12/31 /9999   -> 5373484
## the epoch of the counter, and the day before it
  11/25 /-4714  -> 1
  11/24 /-4714  -> 0
  10/31 /-4714  -> 0
## there is no year 0, and no month 0 or 13
   1/1  /0      -> 0
   1/1  /-1     -> 1721060
   0/1  /2000   -> 0
  13/1  /2000   -> 0
   1/0  /2000   -> 0
   1/32 /2000   -> 0
## a positive answer is not a valid date
   2/31 /2000   -> 2451606
2/31/2000 converts back to 3/2/2000
## php reads the arguments as int and NARROWS them, so 2^32 wraps
   1/1  /4294967297 -> 1721426
1721060
0
2451545
