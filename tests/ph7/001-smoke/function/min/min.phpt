--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: min(1,2,3) returns 1
--FILE--
<?php
$val = min(1,2,3);
echo "min=" . (int)$val . "\n";
?>
--EXPECT--
min=1
--CLEAN--
<?php
unset($val);
