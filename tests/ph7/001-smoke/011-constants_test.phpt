--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Constants
--FILE--
<?php
define('MY_CONST', 123);
echo MY_CONST;
echo "\n";
echo defined('MY_CONST') ? 'yes' : 'no';
?>
--EXPECT--
123
yes