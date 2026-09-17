--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
die with integer status code

--FILE--
<?php
echo "before_die\n";
die(42);
echo "after_die\n";
?>
--EXPECT--
before_die
--CLEAN--
<?php

