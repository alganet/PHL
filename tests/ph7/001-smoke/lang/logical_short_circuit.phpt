--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
logical AND and OR short-circuit evaluation
--FILE--
<?php
function foo() {
    echo "foo called\n";
    return true;
}

function bar() {
    echo "bar called\n";
    return false;
}

// Test AND short-circuit: false && anything -> anything not evaluated
$result1 = false && foo();
echo "AND result: " . ($result1 ? "true" : "false") . "\n";

// Test OR short-circuit: true || anything -> anything not evaluated
$result2 = true || bar();
echo "OR result: " . ($result2 ? "true" : "false") . "\n";

// Test non-short-circuit cases
$result3 = true && foo();
echo "AND true result: " . ($result3 ? "true" : "false") . "\n";

$result4 = false || bar();
echo "OR false result: " . ($result4 ? "true" : "false") . "\n";
?>
--EXPECT--
AND result: false
OR result: true
foo called
AND true result: true
bar called
OR false result: false
--CLEAN--
<?php
unset($result1, $result2, $result3, $result4);
