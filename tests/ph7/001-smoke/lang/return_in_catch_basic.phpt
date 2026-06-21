--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
return inside a catch block returns from the enclosing function
--FILE--
<?php
function rcfBasic() {
    try {
        throw new Exception("e");
    } catch (Exception $e) {
        return "C";
    }
    return "FALLTHROUGH";
}
echo rcfBasic() . "\n";
?>
--EXPECT--
C
--CLEAN--
<?php
