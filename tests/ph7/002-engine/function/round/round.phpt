--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: round(2.3) returns 2
--FILE--
<?php
$val = round(2.3);
echo "round=" . (int)$val . "\n";
?>
--EXPECT--
round=2
--CLEAN--
<?php
unset($val);
?>
