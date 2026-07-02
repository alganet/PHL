--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: htmlspecialchars single quote with ENT_QUOTES flag
--FILE--
<?php
// Test htmlspecialchars with single quote and ENT_QUOTES to hit line 1772
$result = htmlspecialchars("'", ENT_QUOTES);
if ($result === "&#039;") {
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
