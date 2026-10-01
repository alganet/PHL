--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Both single-answer sun functions warn that the nine-answer one replaced them
--DESCRIPTION--
date_sunrise() and date_sunset() were deprecated in favour of date_sun_info(),
and the notice belongs to the CALL rather than to what the body does: php warns
first and then goes on to answer. The stock corpus runs one display_errors
combination, so the stream and the number of copies are pinned here rather than
left to it -- log_errors is off, so the lines on stderr are the display copies,
and error_reporting is pinned because a stock php.ini may mask E_DEPRECATED.
date_sun_info() itself is not deprecated and must stay silent.
--INI--
display_errors=stderr
error_reporting=E_ALL
log_errors=0
date.timezone=UTC
--FILE--
<?php
$ts = mktime(0, 0, 0, 6, 21, 2026);
var_dump(date_sunrise($ts, 0, 52.37, 4.90));
var_dump(date_sunset($ts, 0, 52.37, 4.90));
/* The notice is raised once per call, so a second call warns again. */
var_dump(date_sunrise($ts, 0, 52.37, 4.90));
/* The replacement is silent. */
var_dump(date_sun_info($ts, 52.37, 4.90)['transit']);
/* Muted at the call site, the value still arrives. */
var_dump(@date_sunrise($ts, 0, 52.37, 4.90));
?>
--EXPECT--
int(1782011746)
int(1782072520)
int(1782011746)
int(1782042133)
int(1782011746)
--EXPECT_STDERR--
Deprecated: Function date_sunrise() is deprecated since 8.1, use date_sun_info() instead in %s on line %d
Deprecated: Function date_sunset() is deprecated since 8.1, use date_sun_info() instead in %s on line %d
Deprecated: Function date_sunrise() is deprecated since 8.1, use date_sun_info() instead in %s on line %d
