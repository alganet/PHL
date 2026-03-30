--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Duplicate use import alias produces error
--FILE--
<?php
namespace App;
use Foo\Bar;
use Baz\Bar;
?>
--EXPECTF--
%s %s %s  Cannot use Baz\Bar as Bar because the name is already in use %s
--CLEAN--
<?php
