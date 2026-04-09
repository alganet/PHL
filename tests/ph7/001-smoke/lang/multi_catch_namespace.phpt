--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Multi-catch with namespace-qualified and fully-qualified types
--FILE--
<?php
namespace McNs;

use Exception;

class McExOne extends Exception {}
class McExTwo extends Exception {}

try {
    throw new McExOne("namespaced");
} catch (McExOne | McExTwo $e) {
    echo $e->getMessage() . "\n";
}

try {
    throw new McExTwo("fqn");
} catch (\McNs\McExOne | \McNs\McExTwo $e) {
    echo $e->getMessage() . "\n";
}

try {
    throw new McExOne("backslash");
} catch (\Exception $e) {
    echo $e->getMessage() . "\n";
}
?>
--EXPECT--
namespaced
fqn
backslash
--CLEAN--
<?php
