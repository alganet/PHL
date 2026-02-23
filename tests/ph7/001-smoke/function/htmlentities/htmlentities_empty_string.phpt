--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: htmlentities with empty string input
--FILE--
<?php
// Test htmlentities with empty string to hit uncovered error paths
$result = htmlentities("", ENT_QUOTES);
if ($result === "") {
    echo "PASS\n";
} else {
    echo "FAIL: got '" . $result . "'\n";
}
?>
--EXPECT--
PASS
--CLEAN--
<?php
unset($result);
