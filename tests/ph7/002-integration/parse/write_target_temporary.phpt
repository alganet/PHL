--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A write THROUGH a temporary is php's compile fatal
--FILE--
<?php
// php refuses the whole chain when its base cannot outlive the statement: the
// object `new` just built, a literal, a computed value. PHL used to run it and
// answer with PH7's own "Cannot perform assignment on a constant class
// attribute", so the write silently went nowhere.
class WriteTargetTemp { public $p; }
(new WriteTargetTemp)->p = 1;
?>
--EXPECTF--
%ACannot use temporary expression in write context%A
