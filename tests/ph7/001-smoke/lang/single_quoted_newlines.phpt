--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test single quoted strings with literal newlines
--FILE--
<?php
echo 'line1
line2' . "\n";
?>
--EXPECT--
line1
line2
--CLEAN--
<?php

