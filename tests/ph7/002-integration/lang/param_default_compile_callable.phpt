--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed parameter: a callable parameter holds no folded default, not even a function name
--FILE--
<?php
echo "never printed\n";
function pdcc(callable $f = "strlen") {}
--EXPECTF--
PHP Fatal error:  Cannot use string as default value for parameter $f of type callable in %s on line 3%A
--CLEAN--
<?php
