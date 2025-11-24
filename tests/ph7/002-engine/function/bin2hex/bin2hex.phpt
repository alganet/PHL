--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
bin2hex converts binary string to hex
--FILE--
<?php
echo bin2hex('a') . "\n";
?>
--EXPECT--
61
