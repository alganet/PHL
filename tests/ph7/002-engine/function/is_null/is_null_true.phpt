--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: is_null(null) returns true
--FILE--
<?php
$val = null;
echo "is_null_true=" . (is_null($val) ? 'true' : 'false') . "\n";
?>
--EXPECT--
is_null_true=true
--CLEAN--
<?php
unset($val);
?>
