--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count returns 0 for an empty array
--FILE--
<?php
echo count(array());
?>
--EXPECT--
0
--CLEAN--
<?php

