--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: date ISO 8601 format
--FILE--
<?php
// Test ISO 8601 format (php shape: 2024-01-15T10:30:45+00:00 = 25 chars)
$result = date("c");
echo strlen($result) === 25 ? "ISO8601_OK\n" : "ISO8601_FAIL: '$result'\n";
?>
--EXPECT--
ISO8601_OK
--CLEAN--
<?php
unset($result);
