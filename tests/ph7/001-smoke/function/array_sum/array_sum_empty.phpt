--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum with empty array returns 0
--FILE--
<?php
echo array_sum(array());
?>
--EXPECT--
0
--CLEAN--
<?php

