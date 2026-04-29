--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_replace_callback accepts an object with __invoke as its callback
--FILE--
<?php
class Upper {
    public function __invoke($m) {
        return strtoupper($m[0]);
    }
}
class Tagged {
    private string $tag;
    public function __construct(string $tag) { $this->tag = $tag; }
    public function __invoke($m) {
        return "<{$this->tag}>{$m[0]}</{$this->tag}>";
    }
}
echo preg_replace_callback("/[a-z]+/", new Upper(), "hello world"), "\n";
echo preg_replace_callback("/[0-9]+/", new Tagged("n"), "a1 b22 c333"), "\n";
?>
--EXPECT--
HELLO WORLD
a<n>1</n> b<n>22</n> c<n>333</n>
--CLEAN--
<?php
