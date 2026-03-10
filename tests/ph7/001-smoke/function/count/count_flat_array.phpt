--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count returns number of elements in a flat array
--FILE--
<?php
echo count(array(1, 2, 3));
?>
--EXPECT--
3
--CLEAN--
<?php

