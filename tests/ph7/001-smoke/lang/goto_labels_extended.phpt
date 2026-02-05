--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Extended goto label functionality testing
--FILE--
<?php
// Test extended goto/label scenarios to cover GenStateGetLabel function

echo "Testing goto with multiple labels:\n";

// Test 1: Multiple labels pointing to same location
goto label1;
echo "This should not print\n";

label1:
echo "Reached label1\n";
goto label2;

label2:
echo "Reached label2\n";

// Test 2: Goto to label in different scope (simulated)
$i = 0;
start_loop:
if ($i < 3) {
    echo "Loop iteration: $i\n";
    $i++;
    goto start_loop;
}
echo "Loop ended\n";

// Test 3: Nested goto with labels
goto outer;
inner:
echo "Inner label reached\n";
goto end;

outer:
echo "Outer label reached\n";
goto inner;

end:
echo "Test completed\n";
?>
--EXPECT--
Testing goto with multiple labels:
Reached label1
Reached label2
Loop iteration: 0
Loop iteration: 1
Loop iteration: 2
Loop ended
Outer label reached
Inner label reached
Test completed
--CLEAN--
<?php
unset($i);
