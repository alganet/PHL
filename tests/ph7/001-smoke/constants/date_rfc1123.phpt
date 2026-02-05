--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: DATE_RFC1123 constant expands to RFC 1123 date format
--FILE--
<?php
echo "DATE_RFC1123=" . DATE_RFC1123 . "\n";
?>
--EXPECT--
DATE_RFC1123=D, d M Y H:i:s O
--CLEAN--
<?php

