--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: DATE_RFC822 constant expands to RFC 822 date format
--FILE--
<?php
echo "DATE_RFC822=" . DATE_RFC822 . "\n";
?>
--EXPECTF--
DATE_RFC822=D, d M y H:i:s O
--CLEAN--
<?php

