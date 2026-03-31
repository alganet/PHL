--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Closure captures defining namespace context
--FILE--
<?php
namespace App;
$f = function() { return __NAMESPACE__; };
echo $f() . "\n";
?>
--EXPECT--
App
--CLEAN--
<?php
