--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto with nonexistent label
--FILE--
<?php
goto nonexistent;
echo "this should not be reached\n";
?>
--EXPECTF--
%AFatal error:%A'goto' to undefined label 'nonexistent'%A
--CLEAN--
<?php

