--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A child class cannot override a final class constant (PHP 8.1)
--FILE--
<?php
class FinalConstBase {
    final const X = 1;
}
class FinalConstChild extends FinalConstBase {
    const X = 2;
}
echo FinalConstChild::X;
?>
--EXPECTF--
%s Fatal error:  FinalConstChild::X cannot override final constant FinalConstBase::X %s
--CLEAN--
<?php
