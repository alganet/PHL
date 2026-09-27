--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An expanded year no int64 holds is php's Number out of range
--FILE--
<?php
/* php's EXPANDED year -- the signed form ISO 8601 gives years outside four
 * digits -- is kept in an int64, and a magnitude past it is php's `Number out
 * of range` reported at the sign. PHL saturated the field instead and answered
 * a date for it, silently.
 *
 * The bound is the int64's own, so it is one larger on the negative side, and
 * the refusal belongs to the WHOLE token: `+9999999999999999999-1-1` is no
 * expanded date at all in php (its month and day carry both digits), so that
 * one reports the byte php's re-reading trips on instead. Nineteen digits is
 * also where the token stops: a wider run is not this rule at all and the
 * string re-reads it with whatever else fits, which is why twenty nines are a
 * date in the year 1999 in both engines. */
date_default_timezone_set('UTC');
$rows = [];
foreach ([15,16,17,18,19,20,21] as $n) { foreach (['+','-'] as $sg) { $rows[] = $sg . str_repeat('9', $n) . '-01-01'; } }
foreach (["+9223372036854775806-01-01","+9223372036854775807-01-01","+9223372036854775808-01-01",
          "-9223372036854775807-01-01","-9223372036854775808-01-01","-9223372036854775809-01-01",
          "+9999999999999999999-12-31T23:59:59Z","+9999999999999999999-1-1","+9999999999999999999-01",
          "999999999999999999-01-01","9999999999999999999-01-01","99999999999999999999-01-01",
          "+1234567890123456789-01-01","+12345678901234567890-01-01","+11111111111111111111-01-01"] as $s) { $rows[] = $s; }
foreach ($rows as $s) {
    try { $r = (new DateTime($s))->format('Y-m-d H:i:s'); }
    catch (Throwable $e) { $r = $e->getMessage(); }
    printf("%-40s %s\n", $s, $r);
}
?>
--EXPECT--
+999999999999999-01-01                   -171978273348-05-11 15:23:44
-999999999999999-01-01                   171978273347-08-23 08:36:16
+9999999999999999-01-01                  33879414294-02-28 06:58:08
-9999999999999999-01-01                  -33879414295-11-03 17:01:52
+99999999999999999-01-01                 -245759906304-10-01 14:41:04
-99999999999999999-01-01                 245759906303-04-02 09:18:56
+999999999999999999-01-01                -119382866009-12-15 06:51:44
-999999999999999999-01-01                119382866008-01-18 17:08:16
+9999999999999999999-01-01               Failed to parse time string (+9999999999999999999-01-01) at position 0 (+): Number out of range
-9999999999999999999-01-01               Failed to parse time string (-9999999999999999999-01-01) at position 0 (-): Number out of range
+99999999999999999999-01-01              1999-01-01 00:00:00
-99999999999999999999-01-01              1999-01-01 00:00:00
+999999999999999999999-01-01             0999-01-01 00:00:00
-999999999999999999999-01-01             0999-01-01 00:00:00
+9223372036854775806-01-01               -0003-12-31 13:26:24
+9223372036854775807-01-01               -0002-12-31 13:26:24
+9223372036854775808-01-01               Failed to parse time string (+9223372036854775808-01-01) at position 0 (+): Number out of range
-9223372036854775807-01-01               0000-12-30 13:26:24
-9223372036854775808-01-01               -0001-12-31 13:26:24
-9223372036854775809-01-01               Failed to parse time string (-9223372036854775809-01-01) at position 0 (-): Number out of range
+9999999999999999999-12-31T23:59:59Z     Failed to parse time string (+9999999999999999999-12-31T23:59:59Z) at position 0 (+): Number out of range
+9999999999999999999-1-1                 2009-01-01 00:00:00
+9999999999999999999-01                  Failed to parse time string (+9999999999999999999-01) at position 19 (9): Unexpected character
999999999999999999-01-01                 1999-01-01 00:00:00
9999999999999999999-01-01                0999-01-01 00:00:00
99999999999999999999-01-01               9999-01-01 00:00:00
+1234567890123456789-01-01               260072200690-06-21 03:24:48
+12345678901234567890-01-01              Failed to parse time string (+12345678901234567890-01-01) at position 18 (8): Double date specification
+11111111111111111111-01-01              Failed to parse time string (+11111111111111111111-01-01) at position 24 (-): Double timezone specification
