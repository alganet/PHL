--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: DATE_W3C constant expands to W3C date format
--FILE--
<?php
echo "DATE_W3C=" . DATE_W3C . "\n";
?>
--EXPECT--
DATE_W3C=Y-m-d\TH:i:sP