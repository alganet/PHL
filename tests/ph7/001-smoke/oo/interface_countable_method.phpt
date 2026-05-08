--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count() dispatches to Countable::count() on objects
--FILE--
<?php
class IfaceCountableBox implements Countable {
    public function count(): int { return 42; }
}
echo count(new IfaceCountableBox()), "\n";
?>
--EXPECT--
42
--CLEAN--
<?php
