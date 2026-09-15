--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
idate format tokens: month, hour, ISO week/year, Swatch beat and $timestamp handling
--FILE--
<?php
date_default_timezone_set('UTC');
// 2021-07-15, 2025-01-01, 2026-01-01 and 2021-01-01 (which is ISO 2020-W53).
foreach ([1626307200, 1735689600, 1767225600, 1609459200] as $ts) {
    echo "ts=$ts:";
    foreach (['Y','y','m','n','d','j','H','G','h','g','i','s','w','z','t','L','W','o','B','U'] as $f) {
        echo " $f=", idate($f, $ts);
    }
    echo "\n";
}
// An unrecognized token is FALSE, distinguishable from a legitimate 0.
var_dump(@idate('Q', 1626307200));
?>
--EXPECT--
ts=1626307200: Y=2021 y=21 m=7 n=7 d=15 j=15 H=0 G=0 h=12 g=12 i=0 s=0 w=4 z=195 t=31 L=0 W=28 o=2021 B=41 U=1626307200
ts=1735689600: Y=2025 y=25 m=1 n=1 d=1 j=1 H=0 G=0 h=12 g=12 i=0 s=0 w=3 z=0 t=31 L=0 W=1 o=2025 B=41 U=1735689600
ts=1767225600: Y=2026 y=26 m=1 n=1 d=1 j=1 H=0 G=0 h=12 g=12 i=0 s=0 w=4 z=0 t=31 L=0 W=1 o=2026 B=41 U=1767225600
ts=1609459200: Y=2021 y=21 m=1 n=1 d=1 j=1 H=0 G=0 h=12 g=12 i=0 s=0 w=5 z=0 t=31 L=0 W=53 o=2020 B=41 U=1609459200
bool(false)
--CLEAN--
<?php
