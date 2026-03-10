--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count with COUNT_RECURSIVE on a flat array equals normal count
--FILE--
<?php
echo count(array(1, 2, 3), COUNT_RECURSIVE);
?>
--EXPECT--
3
--CLEAN--
<?php

