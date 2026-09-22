--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Duplicate use import alias is detected case-insensitively
--FILE--
<?php
namespace App;
use Foo\Bar;
use Baz\BAR;
?>
--EXPECTF--
%s %s %s  Cannot use Baz\BAR as BAR because the name is already in use %s
--CLEAN--
<?php
