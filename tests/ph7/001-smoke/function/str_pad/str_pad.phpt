--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_pad behavior
--FILE--
<?php
echo "left='" . str_pad('hi', 4, ' ', STR_PAD_LEFT) . "'\n";
echo "right='" . str_pad('hi', 4, ' ', STR_PAD_RIGHT) . "'\n";
?>
--EXPECT--
left='  hi'
right='hi  '
--CLEAN--
<?php

