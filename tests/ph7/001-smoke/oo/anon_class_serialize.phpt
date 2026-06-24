--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Anonymous class cannot be serialized (throws Exception)
--FILE--
<?php
try {
    serialize(new class { public $a = 1; });
} catch (\Exception $e) {
    echo $e->getMessage(), "\n";
}
// also when nested inside a container
try {
    serialize([1, new class {}, 2]);
} catch (\Exception $e) {
    echo "nested: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
Serialization of 'class@anonymous' is not allowed
nested: Serialization of 'class@anonymous' is not allowed
--CLEAN--
<?php
