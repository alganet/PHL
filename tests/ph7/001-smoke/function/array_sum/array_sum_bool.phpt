--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum treats booleans as integers (true=1, false=0)
--FILE--
<?php
echo array_sum(array(true, false, true));
?>
--EXPECT--
2
--CLEAN--
<?php

