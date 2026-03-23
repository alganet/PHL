--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert silently ignores invalid characters
--FILE--
<?php
echo @base_convert("1g2", 10, 10) . "\n";
echo @base_convert("-12", 10, 10) . "\n";
?>
--EXPECT--
12
12
--CLEAN--
<?php

