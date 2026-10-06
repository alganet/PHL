--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A name taken above a run-time class declaration makes it a redeclaration
--DESCRIPTION--
An autoloader asked for the name before the statement ran declares its own
class under it, and the statement is then php's redeclaration fatal.
--INI--
display_errors=0
log_errors=0
--FILE--
<?php
register_shutdown_function(function () {
    $e = error_get_last();
    echo $e['type'], " ", str_replace(__FILE__, 'FILE', $e['message']), " @", $e['line'], "\n";
});
spl_autoload_register(function ($c) {
    echo "load $c\n";
    eval("class $c {}");
});
var_dump(class_exists('DwrTaken'));
echo "before\n";
class DwrTaken implements Countable { function count(): int { return 0; } }
echo "unreached\n";
?>
--EXPECT--
load DwrTaken
bool(true)
before
64 Cannot redeclare class DwrTaken (previously declared in FILE(8) : eval()'d code:1) @12
