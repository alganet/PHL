--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ceil(5) should return the integer unchanged
--FILE--
<?php
$val = ceil(5);
echo "ceil=" . (int)$val . "\n";
?>
--EXPECT--
ceil=5
--CLEAN--
<?php
unset($val);
?>
