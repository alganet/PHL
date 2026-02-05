--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: fmod returns remainder of division
--FILE--
<?php
$val = fmod(5.5, 2.0);
echo (string)$val . "\n";
?>
--EXPECT--
1.5
--CLEAN--
<?php
unset($val);
