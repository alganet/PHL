--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
negative float returns positive float
--FILE--
<?php
$val = abs(-3.14);
echo "abs=" . $val . "\n";
?>
--EXPECT--
abs=3.14
--CLEAN--
<?php
unset($val);
?>