--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed parameter: a trait method keeps self as written in the refusal
--FILE--
<?php
echo "never printed\n";
trait PdctsTrait {
    function m(iterable|self $p = 1.5) {}
}
--EXPECTF--
PHP Fatal error:  Cannot use float as default value for parameter $p of type Traversable|self|array in %s on line 4%A
--CLEAN--
<?php
