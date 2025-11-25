--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sleep should return 0 when sleeping 0 seconds
--FILE--
<?php
$r = sleep(0);
echo "sleep=" . $r . PHP_EOL;
?>
--EXPECT--
sleep=0
