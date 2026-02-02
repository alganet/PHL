--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
float precision handling
--FILE--
<?php
$val = abs(-0.0001);
echo "abs=" . $val . "\n";
?>
--EXPECT--
abs=0.0001
--CLEAN--
<?php
unset($val);
?>