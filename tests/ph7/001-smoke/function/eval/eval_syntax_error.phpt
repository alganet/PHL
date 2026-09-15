--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
eval() raises a catchable ParseError for invalid code
--DESCRIPTION--
php throws ParseError (extends CompileError extends Error) when the eval'd chunk does not
compile; PHL returned FALSE, so a syntax error silently produced a value. The test was
guarded on function_exists('eval') -- false in php -- so the divergence never ran under the
oracle.
--FILE--
<?php
try {
    eval('invalid syntax here');
    echo "no-throw\n";
} catch (\ParseError $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
var_dump(class_exists('ParseError'), get_parent_class('ParseError'), get_parent_class('CompileError'));
try { eval('function f( {'); } catch (\Error $e) { echo 'as Error: ', get_class($e), "\n"; }
echo "eval completed\n";
?>
--EXPECT--
ParseError: syntax error, unexpected identifier "syntax"
bool(true)
string(12) "CompileError"
string(5) "Error"
as Error: ParseError
eval completed
