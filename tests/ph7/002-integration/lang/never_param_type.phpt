--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
never cannot be used as a parameter type (return-only)
--FILE--
<?php
function h(never $x) {}
?>
--EXPECTF--
%s Fatal error:  never cannot be used as a parameter type in %s
--CLEAN--
<?php
