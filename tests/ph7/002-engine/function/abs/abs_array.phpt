--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array input returns zero
--FILE--
<?php
$val = abs(array());
echo "abs=" . $val . "\n";
?>
--EXPECT--
abs=0
--CLEAN--
<?php
unset($val);
?>