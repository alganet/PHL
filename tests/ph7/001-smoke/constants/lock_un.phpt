--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
LOCK_UN constant expands to 3
--FILE--
<?php
echo LOCK_UN . "\n";
?>
--EXPECTF--
%A3%A
--CLEAN--
<?php

