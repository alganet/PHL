--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
object input returns zero
--FILE--
<?php
$val = abs(new stdClass());
echo "abs=" . $val . "\n";
?>
--EXPECT--
abs=0
--CLEAN--
<?php
unset($val);
?>