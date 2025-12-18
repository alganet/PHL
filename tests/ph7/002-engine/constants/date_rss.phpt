--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: DATE_RSS constant expands to RSS date format
--FILE--
<?php
echo "DATE_RSS=" . DATE_RSS . "\n";
?>
--EXPECT--
DATE_RSS=D, d M Y H:i:s O