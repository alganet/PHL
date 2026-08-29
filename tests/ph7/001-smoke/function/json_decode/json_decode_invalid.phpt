--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: json_decode with invalid JSON

--FILE--
<?php
// Test json_decode with invalid JSON
$result = json_decode('{invalid');
if ($result === null) {
    echo "PASS\n";
} else {
    echo "FAIL\n";
}
?>
--EXPECT--
PASS
--CLEAN--
<?php
unset($result);
