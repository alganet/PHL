--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
foreach with key and list() unpacking
--FILE--
<?php
$data = [
    'first' => ['Alice', 30],
    'second' => ['Bob', 25],
];
foreach ($data as $key => list($name, $age)) {
    echo "$key: $name is $age\n";
}
?>
--EXPECT--
first: Alice is 30
second: Bob is 25
--CLEAN--
<?php
