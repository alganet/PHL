--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
class_alias creates an alias for a class
--FILE--
<?php
class ClassAlias_Test_2025 {}
class_alias('ClassAlias_Test_2025', 'ClassAlias_Alias_2025');
echo (class_exists('ClassAlias_Alias_2025') ? "ok\n" : "fail\n");
?>
--EXPECT--
ok
--CLEAN--
<?php

