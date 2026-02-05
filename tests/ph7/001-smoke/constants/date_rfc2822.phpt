--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: DATE_RFC2822 constant expands to RFC 2822 date format
--FILE--
<?php
echo "DATE_RFC2822=" . DATE_RFC2822 . "\n";
?>
--EXPECT--
DATE_RFC2822=D, d M Y H:i:s O
--CLEAN--
<?php

