--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Interface method implemented with fewer parameters produces error
--FILE--
<?php
interface Calculable {
    public function compute($a, $b);
}
class Calculator implements Calculable {
    public function compute($a) {
        return $a;
    }
}
?>
--EXPECTF--
%s %s %s  Declaration of Calculator::compute($a) must be compatible with Calculable::compute($a, $b) %s
--CLEAN--
<?php
