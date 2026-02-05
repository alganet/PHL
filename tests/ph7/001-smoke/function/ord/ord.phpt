--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ord returns ASCII code of first character
--FILE--
<?php
echo ord('A') . "\n";
?>
--EXPECT--
65
--CLEAN--
<?php

