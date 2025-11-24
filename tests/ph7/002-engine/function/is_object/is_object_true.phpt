--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: is_object((object)[]) returns true
--FILE--
<?php
$val = (object) array();
echo "is_object_true=" . (is_object($val) ? 'true' : 'false') . "\n";
?>
--EXPECT--
is_object_true=true
--CLEAN--
<?php
unset($val);
?>
