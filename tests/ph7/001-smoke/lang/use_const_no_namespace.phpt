--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
use const works outside any namespace
--FILE--
<?php
define('UseConstNoNs\UCNN_VAL', 77);
use const UseConstNoNs\UCNN_VAL as MY_BAR;
echo MY_BAR . "\n";
echo "done\n";
?>
--EXPECT--
77
done
--CLEAN--
<?php

