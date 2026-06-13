--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match populates named groups into an undefined $matches
--FILE--
<?php
preg_match('/(?<year>\d{4})-(?<month>\d{2})/', '2026-06', $m);
echo $m['year'] . "\n";
echo $m['month'] . "\n";
echo $m[1] . "\n";
echo $m[2] . "\n";
?>
--EXPECT--
2026
06
2026
06
--CLEAN--
<?php
