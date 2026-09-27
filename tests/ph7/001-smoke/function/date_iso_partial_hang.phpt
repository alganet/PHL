--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a date string whose ISO head is incomplete answers instead of spinning
--FILE--
<?php
/* `new DateTime('2020-1-1 12:00')` HUNG this engine: the ISO rule saw a 4-digit
 * year with enough bytes behind it, found the rest was not `-DD-DD`, and refused
 * by returning the offending TOKEN's position -- which for a token at position 0
 * encodes as the same 1 the caller reads as "matched". The cursor stayed where it
 * was and the parse loop went round forever, on a spelling php answers.
 *
 * The rule hands the text on now (the day-first numeric rule is php's own owner of
 * `2020-1-1`), and the loop refuses outright if a pass ever consumes nothing, so
 * no shape rule can spin it again. */
date_default_timezone_set('UTC');

foreach (['2020-1-1 12:00', '2020-01-1 12:00', '2020-1-01 12:00', '2020-11-1 12:00',
          '2020-1-1 12:00:00', '2020-1-1', '2020-1-1x',
          '2020-12-31 23:59:59', '2020-13-01', '2020-12-32', '+12345-1-1'] as $spec) {
    try {
        $d = new DateTime($spec);
        printf("%-22s %s\n", $spec, $d->format('Y-m-d H:i:s'));
    } catch (Throwable $e) {
        printf("%-22s %s\n", $spec, $e->getMessage());
    }
}
var_dump(strtotime('2020-1-1 12:00'));
?>
--EXPECT--
2020-1-1 12:00         2020-01-01 12:00:00
2020-01-1 12:00        2020-01-01 12:00:00
2020-1-01 12:00        2020-01-01 12:00:00
2020-11-1 12:00        2020-11-01 12:00:00
2020-1-1 12:00:00      2020-01-01 12:00:00
2020-1-1               2020-01-01 00:00:00
2020-1-1x              2020-01-01 00:00:00
2020-12-31 23:59:59    2020-12-31 23:59:59
2020-13-01             Failed to parse time string (2020-13-01) at position 6 (3): Unexpected character
2020-12-32             Failed to parse time string (2020-12-32) at position 9 (2): Unexpected character
+12345-1-1             2005-01-01 00:00:00
int(1577880000)
