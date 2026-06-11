--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An exception thrown inside jsonSerialize() propagates out of json_encode
--SKIPIF--
<?php if (!function_exists('json_encode') || !interface_exists('JsonSerializable')) { die('skip'); } ?>
--FILE--
<?php
class JsonSerBoom implements JsonSerializable {
    public function jsonSerialize(): mixed { throw new Exception("boom"); }
}
class JsonSerInner implements JsonSerializable {
    public function jsonSerialize(): mixed { throw new Exception("inner"); }
}

try {
    json_encode(new JsonSerBoom());
    echo "no-throw\n";
} catch (Exception $e) {
    echo "caught: ", $e->getMessage(), "\n";
}

// Thrown while nested inside an array being encoded.
try {
    json_encode(['k' => new JsonSerInner()]);
    echo "no-throw\n";
} catch (Exception $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
caught: boom
caught: inner
--CLEAN--
<?php
