--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A `??=` through a temporary is refused too
--FILE--
<?php
class WtCoalTemp { public $p; }
(new WtCoalTemp)->p ??= 3;
?>
--EXPECTF--
%ACannot use temporary expression in write context%A
