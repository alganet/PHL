--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
list() assignment compiled correctly
--FILE--
<?php
list($a,$b) = array(1,2);
echo $a . ',' . $b . "\n";
?>
--EXPECT--
1,2
--CLEAN--
<?php

