--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: max(1,2,3) returns 3
--FILE--
<?php
$val = max(1,2,3);
echo "max=" . (int)$val . "\n";
?>
--EXPECT--
max=3
--CLEAN--
<?php
unset($val);
