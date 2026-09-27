--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
createFromFormat's textual day is a relative weekday, not decoration
--FILE--
<?php
/* php's `D` and `l` format characters do not merely CONSUME the day name they
 * match: they set a relative weekday, and it MOVES the date read beside them.
 * PHL ate the name and threw it away, so `createFromFormat('D, d M Y',
 * 'Tue, 02 Jan 2020')` answered the 2nd -- a Thursday -- where php answers the
 * 7th, the Tuesday on or after it. Every RFC-2822-shaped parse a program writes
 * goes through that character.
 *
 * The hunt is the bare NAME's: forward to that weekday, and a day that already
 * matches counts. The clock is left standing, unlike the string parser's, which
 * zeroes it. */
date_default_timezone_set('UTC');
$rows = [['D, d M Y', 'Thu, 02 Jan 2020'], ['D, d M Y', 'Fri, 02 Jan 2020'],
         ['D, d M Y', 'Tue, 02 Jan 2020'], ['D, d M Y', 'Wed, 02 Jan 2020'],
         ['l d M Y', 'Sunday 02 Jan 2020'], ['D d M Y H:i:s', 'Tue 02 Jan 2020 05:06:07'],
         ['Y-m-d', '2020-01-02'], ['D, d M Y', 'Mon, 29 Feb 2020']];
foreach ($rows as $r) {
    $d = DateTime::createFromFormat($r[0], $r[1]);
    /* the clock only where the format spelled one: everything else would be
     * the moment this test ran */
    $out = $d ? $d->format('Y-m-d D') . (strpos($r[0], 'H') === false ? '' : ' ' . $d->format('H:i:s'))
              : 'false';
    printf("%-16s %-26s %s\n", $r[0], $r[1], $out);
}
?>
--EXPECT--
D, d M Y         Thu, 02 Jan 2020           2020-01-02 Thu
D, d M Y         Fri, 02 Jan 2020           2020-01-03 Fri
D, d M Y         Tue, 02 Jan 2020           2020-01-07 Tue
D, d M Y         Wed, 02 Jan 2020           2020-01-08 Wed
l d M Y          Sunday 02 Jan 2020         2020-01-05 Sun
D d M Y H:i:s    Tue 02 Jan 2020 05:06:07   2020-01-07 Tue 05:06:07
Y-m-d            2020-01-02                 2020-01-02 Thu
D, d M Y         Mon, 29 Feb 2020           2020-03-02 Mon
