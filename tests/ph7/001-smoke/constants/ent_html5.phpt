--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: ENT_HTML5 constant value (compat: 48 or 512)
--FILE--
<?php
$val = (int)ENT_HTML5;
echo "ENT_HTML5=" . (($val === 48 || $val === 512) ? 'OK' : 'FAIL') . "\n";
?>
--EXPECT--
ENT_HTML5=OK
--CLEAN--
<?php
unset($val);
