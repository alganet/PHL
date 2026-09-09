--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strtotime parses absolute, epoch, time-only and relative forms against a base timestamp
--FILE--
<?php
function strtotimeCases(): array {
    date_default_timezone_set("UTC");
    $base = 1600000000;
    $inputs = [
        "", "   ", "now", "@1500000000", "@0", "@-100",
        "2020-01-15 14:30:45", "2020-01-15", "2020-01-15T14:30:45+02:00", "2020-01-15T14:30:45Z",
        "14:30:45", "14:30", "garbage", "2020-13-01", "2020-01-32",
        "+1 day", "+1 week", "-3 hours", "+1 month", "+1 year", "-2 weeks",
        "tomorrow", "yesterday", "midnight", "noon", "today",
        "2020-01-31 +1 month", "+90 minutes", "+2 fortnights",
    ];
    $out = [];
    foreach ($inputs as $in) { $out[] = $in . " => " . var_export(strtotime($in, $base), true); }
    return $out;
}
echo implode("\n", strtotimeCases()), "\n";
--EXPECT--
 => false
    => 1600000000
now => 1600000000
@1500000000 => 1500000000
@0 => 0
@-100 => -100
2020-01-15 14:30:45 => 1579098645
2020-01-15 => 1579046400
2020-01-15T14:30:45+02:00 => 1579091445
2020-01-15T14:30:45Z => 1579098645
14:30:45 => 1600007445
14:30 => 1600007400
garbage => false
2020-13-01 => false
2020-01-32 => false
+1 day => 1600086400
+1 week => 1600604800
-3 hours => 1599989200
+1 month => 1602592000
+1 year => 1631536000
-2 weeks => 1598790400
tomorrow => 1600041600
yesterday => 1599868800
midnight => 1599955200
noon => 1599998400
today => 1599955200
2020-01-31 +1 month => 1583107200
+90 minutes => 1600005400
+2 fortnights => 1602419200
--CLEAN--
<?php
