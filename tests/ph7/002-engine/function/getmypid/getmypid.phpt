--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: getmypid returns an integer > 0
--FILE--
<?php
$pid = getmypid();
echo "getmypid_isint=" . (is_int($pid) ? 'true' : 'false') . "\n";
echo "getmypid_positive=" . ($pid > 0 ? 'true' : 'false') . "\n";
?>
--EXPECT--
getmypid_isint=true
getmypid_positive=true
--CLEAN--
<?php
unset($pid);
?>
