--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
$this as a parameter name is a compile fatal, before any variadic complaint
--FILE--
<?php
function q(...$this = 1) {}
?>
--EXPECTF--
%s Fatal error:  Cannot use $this as parameter in %s
--CLEAN--
<?php
