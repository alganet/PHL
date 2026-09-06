--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
undefined function call warning
--FILE--
<?php
nonexistent_function();
?>
--EXPECTF--
%AFatal error:%AUncaught Error: Call to undefined function nonexistent_function()%A
--CLEAN--
<?php

