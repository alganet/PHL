--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Constructor property promotion: visibility modifier on non-constructor method is a fatal
--FILE--
<?php
class CppOutside {
    public function notCtor(public int $x) {}
}
?>
--EXPECTF--
%s Fatal error:  Cannot declare promoted property outside a constructor %s
--CLEAN--
<?php
