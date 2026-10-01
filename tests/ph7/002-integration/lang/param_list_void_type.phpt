--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
void as a parameter type is a compile fatal naming the position
--FILE--
<?php
function q(void $x) {}
?>
--EXPECTF--
%s Fatal error:  void cannot be used as a parameter type in %s
--CLEAN--
<?php
