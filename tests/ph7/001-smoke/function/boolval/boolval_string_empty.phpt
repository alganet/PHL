--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: boolval("") returns false
--FILE--
<?php
echo "boolval=" . (boolval("") ? 'true' : 'false') . "\n";
?>
--EXPECT--
boolval=false
--CLEAN--
<?php

