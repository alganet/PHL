--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
never in a union (or ?never) is rejected as non-standalone
--FILE--
<?php
function u(): int|never {}
?>
--EXPECTF--
%s Fatal error:  never can only be used as a standalone type in %s
--CLEAN--
<?php
