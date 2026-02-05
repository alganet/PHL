--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
quotemeta basic
--FILE--
<?php
echo quotemeta("hello.world") . "\n";
?>
--EXPECT--
hello\.world
--CLEAN--
<?php

