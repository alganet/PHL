--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_flip should warn and skip sub-array values
--FILE--
<?php
$array = array('a' => array(1,2,3), 'b' => 'ok');
$flipped = array_flip($array);
$pass = (count($flipped) === 1 && isset($flipped['ok']) && $flipped['ok'] === 'b');
echo $pass ? "PASS" : "FAIL";
?>
--EXPECTF--
Error [2]: array_flip(): Can only flip string and integer values, entry skipped in %s on line %d
PASS
--CLEAN--
<?php
unset($array, $flipped, $pass);
