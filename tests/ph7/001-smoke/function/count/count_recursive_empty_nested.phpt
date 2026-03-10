--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count with COUNT_RECURSIVE counts empty nested arrays as elements
--FILE--
<?php
echo count(array(array(), array()), COUNT_RECURSIVE);
?>
--EXPECT--
2
--CLEAN--
<?php

