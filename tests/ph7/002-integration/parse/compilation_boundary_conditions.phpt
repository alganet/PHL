--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
compilation boundary conditions and syntax error edge cases
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test various compilation boundary conditions and syntax errors
// to cover additional uncovered lines in compile.c

// Invalid function declarations
function invalid_func( {
    echo "invalid";
}

// Function with invalid argument syntax
function bad_args($var = ) {
    return $var;
}

// Invalid class syntax
class BadClass extends {
    public function method() {
        echo "bad";
    }
}

// Invalid interface syntax
interface BadInterface implements {
    public function method();
}

// Try without catch
try {
    echo "trying";
}

// Invalid constant declaration
const INVALID_CONST = function() { return 1; }();

// Invalid static declaration
static $invalid_static = new stdClass();

// Invalid namespace syntax
namespace invalid\namespace\with\spaces;

// Invalid use syntax
use invalid\use\statement as;

// Invalid declare syntax
declare(invalid_syntax) {
    echo "declare";
}

// Empty heredoc
$empty = <<<EMPTY
EMPTY;

// Invalid heredoc syntax
$bad = <<<BAD
content
BAD

// Function with invalid return type
function bad_return() : {
    return 1;
}

// Class with invalid inheritance
class Child extends Parent implements {
    // empty
}

?>
--EXPECTF--
%s Error:  Missing ')' after function 'invalid_func' signature %s
--CLEAN--
<?php
unset($empty, $bad);
