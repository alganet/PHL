--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
exit with integer status code

--FILE--
<?php
echo "before_exit\n";
exit(0);
echo "after_exit\n";
?>
--EXPECT--
before_exit
--CLEAN--
<?php

