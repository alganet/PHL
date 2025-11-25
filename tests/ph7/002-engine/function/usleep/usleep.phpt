--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
usleep should not raise and return nothing
--FILE--
<?php
usleep(1);
echo "ok" . PHP_EOL;
?>
--EXPECT--
ok
