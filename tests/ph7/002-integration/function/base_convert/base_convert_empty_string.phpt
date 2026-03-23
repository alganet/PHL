--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert with empty string returns "0"
--FILE--
<?php
echo @base_convert("", 10, 16) . "\n";
?>
--EXPECT--
0
--CLEAN--
<?php

