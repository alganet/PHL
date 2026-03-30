--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Self-referencing trait use produces error
--FILE--
<?php
trait A {
    use A;
    public function hello() { return "A"; }
}
class C {
    use A;
}
?>
--EXPECTF--
%s %s %s  %s
--CLEAN--
<?php
