--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
OO constructor visibility (private constructor from outside is a catchable Error)
--DESCRIPTION--
Rewritten cross-engine with the band A #4 fix: __construct keeps its declared
visibility (the engine previously forced it public, so `new` on a
private-ctor class succeeded from any scope — this test used to enshrine
that). Construction from inside the class still works; destructors are
engine-invoked so a private __destruct always runs.
--FILE--
<?php
class ConstructorVisibilityTest {
    private function __construct() {
        echo "Private constructor called\n";
    }

    public function __destruct() {
        echo "Destructor called\n";
    }

    public static function make(): ConstructorVisibilityTest {
        return new ConstructorVisibilityTest();
    }
}

echo "Testing constructor visibility...\n";

try {
    $test = new ConstructorVisibilityTest();
    echo "constructed from global scope\n";
} catch (Error $e) {
    echo "Caught: ", $e->getMessage(), "\n";
}

$test = ConstructorVisibilityTest::make();
unset($test);

echo "Constructor visibility test completed\n";
?>
--EXPECT--
Testing constructor visibility...
Caught: Call to private ConstructorVisibilityTest::__construct() from global scope
Private constructor called
Destructor called
Constructor visibility test completed
