--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count with explicit COUNT_NORMAL mode does not count nested elements
--FILE--
<?php
echo count(array(1, array(2, 3), 4), COUNT_NORMAL);
?>
--EXPECT--
3
--CLEAN--
<?php

