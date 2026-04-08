--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: boolval("hello") returns true
--FILE--
<?php
echo "boolval=" . (boolval("hello") ? 'true' : 'false') . "\n";
?>
--EXPECT--
boolval=true
--CLEAN--
<?php

