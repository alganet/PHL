--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
positive integer returns same value
--FILE--
<?php
$val = abs(7);
echo "abs=" . $val . "\n";
?>
--EXPECT--
abs=7
--CLEAN--
<?php
unset($val);
?>