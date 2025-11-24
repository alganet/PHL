--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: is_array([]) returns true
--FILE--
<?php
$val = array();
echo "is_array_true=" . (is_array($val) ? 'true' : 'false') . "\n";
?>
--EXPECT--
is_array_true=true
--CLEAN--
<?php
unset($val);
?>
