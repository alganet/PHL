--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Trait with abstract method implemented by class
--FILE--
<?php
trait Formatter {
    abstract public function format($value);
    public function display($value) {
        echo $this->format($value), "\n";
    }
}
class UpperFormatter {
    use Formatter;
    public function format($value) {
        return strtoupper($value);
    }
}
$f = new UpperFormatter();
$f->display("hello");
echo $f->format("world"), "\n";
?>
--EXPECT--
HELLO
WORLD
--CLEAN--
<?php
