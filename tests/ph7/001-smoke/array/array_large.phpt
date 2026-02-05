--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test large array operations
--FILE--
<?php
/* Create a large array to trigger hashmap growth */
$a = array();
for ($i = 0; $i < 1000; $i++) {
    $a["key$i"] = $i;
}
/* Perform operations that may trigger rehashing */
unset($a['key500']);
$a['newkey'] = 'value';
echo 'count=' . count($a) . "\n";
echo 'isset=' . (isset($a['key999']) ? '1' : '0') . "\n";
echo 'value=' . $a['newkey'] . "\n";
?>
--EXPECT--
count=1000
isset=1
value=value
--CLEAN--
<?php
unset($a);
