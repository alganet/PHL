--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
use function works outside any namespace
--FILE--
<?php
use function strlen as myStrlen;
echo myStrlen("hello") . "\n";
echo "done\n";
?>
--EXPECT--
5
done
--CLEAN--
<?php

