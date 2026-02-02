--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
large negative integer returns positive
--FILE--
<?php
$val = abs(-2147483648);
echo "abs=" . $val . "\n";
?>
--EXPECT--
abs=2147483648
--CLEAN--
<?php
unset($val);
?>