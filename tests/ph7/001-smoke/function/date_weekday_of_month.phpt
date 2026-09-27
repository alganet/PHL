--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php's `first monday of` -- a weekday inside a month
--FILE--
<?php
/* `first monday of next month` and `last sunday of february 2020` -- the way php
 * names the Nth weekday of a month -- did not parse at all: the `of` came back
 * as an unknown TIMEZONE.
 *
 * It is not a rule of its own so much as a way in to the weekday hunt the
 * relative vector already carries: the MONTH is settled first (the relative
 * months are applied and consumed, though not the years), the day becomes that
 * month's 1st, and the ordinary hunt walks forward from there. The count rides
 * the relative DAYS, a week per count past the first, so the days a string
 * spells beside it simply add on -- which is what makes `second monday of
 * tomorrow` the day after the FIRST one, since `tomorrow` sets that field
 * rather than adding to it. `last` and `previous` are the same rule one month
 * on with a week taken off, and `this` is that month shift with nothing taken
 * off. Only the WORD counts reach it: `1 monday of` is a refusal in php too. */
date_default_timezone_set('UTC');
$rows = ["first monday of","last monday of","second monday of","fifth monday of","sixth monday of",
         "twelfth monday of","next monday of","this monday of","previous monday of","first sunday of",
         "last sunday of","first mon of","first mondays of","first monday of next month",
         "last monday of next month","last sunday of february 2020","third wednesday of july 2021",
         "first monday of this month","first monday of last month","last thursday of december 2019",
         "first monday of january","first monday of 2021-03","first monday of 12:00",
         "first monday of +3 days","first monday of -1 month","first monday of tomorrow",
         "second monday of tomorrow","first sunday of tomorrow","first monday of +1 day",
         "last monday of +1 day","last monday of today","last monday of yesterday",
         "first monday of next week","this monday of next week","this monday of +7 days",
         "next friday of +1 year","tenth tuesday of +1 year","1 monday of","-1 monday of",
         "first weekday of","monday of","first monday of of"];
foreach ($rows as $s) {
    $d = new DateTime('2020-06-15 08:09:10');   /* a Monday */
    try { $d->modify($s); $r = $d->format('Y-m-d D H:i:s'); }
    catch (Throwable $e) { $r = $e->getMessage(); }
    printf("%-32s %s\n", $s, $r);
}
?>
--EXPECT--
first monday of                  2020-06-01 Mon 00:00:00
last monday of                   2020-06-29 Mon 00:00:00
second monday of                 2020-06-08 Mon 00:00:00
fifth monday of                  2020-06-29 Mon 00:00:00
sixth monday of                  2020-07-06 Mon 00:00:00
twelfth monday of                2020-08-17 Mon 00:00:00
next monday of                   2020-06-01 Mon 00:00:00
this monday of                   2020-07-06 Mon 00:00:00
previous monday of               2020-06-29 Mon 00:00:00
first sunday of                  2020-06-07 Sun 00:00:00
last sunday of                   2020-06-28 Sun 00:00:00
first mon of                     2020-06-01 Mon 00:00:00
first mondays of                 2020-06-01 Mon 00:00:00
first monday of next month       2020-07-06 Mon 00:00:00
last monday of next month        2020-07-27 Mon 00:00:00
last sunday of february 2020     2020-02-23 Sun 00:00:00
third wednesday of july 2021     2021-07-21 Wed 00:00:00
first monday of this month       2020-06-01 Mon 00:00:00
first monday of last month       2020-05-04 Mon 00:00:00
last thursday of december 2019   2019-12-26 Thu 00:00:00
first monday of january          2020-01-06 Mon 00:00:00
first monday of 2021-03          2021-03-01 Mon 00:00:00
first monday of 12:00            2020-06-01 Mon 12:00:00
first monday of +3 days          2020-06-04 Thu 00:00:00
first monday of -1 month         2020-05-04 Mon 00:00:00
first monday of tomorrow         2020-06-02 Tue 00:00:00
second monday of tomorrow        2020-06-02 Tue 00:00:00
first sunday of tomorrow         2020-06-08 Mon 00:00:00
first monday of +1 day           2020-06-02 Tue 00:00:00
last monday of +1 day            2020-06-30 Tue 00:00:00
last monday of today             2020-06-29 Mon 00:00:00
last monday of yesterday         2020-07-05 Sun 00:00:00
first monday of next week        2020-06-08 Mon 00:00:00
this monday of next week         2020-07-06 Mon 00:00:00
this monday of +7 days           2020-07-13 Mon 00:00:00
next friday of +1 year           2021-06-05 Sat 00:00:00
tenth tuesday of +1 year         2021-08-04 Wed 00:00:00
1 monday of                      DateTime::modify(): Failed to parse time string (1 monday of) at position 9 (o): The timezone could not be found in the database
-1 monday of                     DateTime::modify(): Failed to parse time string (-1 monday of) at position 10 (o): The timezone could not be found in the database
first weekday of                 DateTime::modify(): Failed to parse time string (first weekday of) at position 14 (o): The timezone could not be found in the database
monday of                        DateTime::modify(): Failed to parse time string (monday of) at position 7 (o): The timezone could not be found in the database
first monday of of               DateTime::modify(): Failed to parse time string (first monday of of) at position 16 (o): The timezone could not be found in the database
