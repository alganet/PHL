--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Unreferenced label emits warning
--FILE--
<?php
my_label:
echo "test\n";
?>
--EXPECTF--
%Atest%A
--CLEAN--
<?php

