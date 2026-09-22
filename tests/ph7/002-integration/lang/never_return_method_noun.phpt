--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a never-returning METHOD is named a method, not a function
--FILE--
<?php
class C {
    public function bad(): never { return; }
}
?>
--EXPECTF--
%s Fatal error:  A never-returning method must not return in %s
--CLEAN--
<?php
