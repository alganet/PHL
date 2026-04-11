--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Union return type: void cannot appear inside a union
--FILE--
<?php
function f(): int|void {}
--EXPECTF--
PHP Fatal error:  Void can only be used as a standalone type in %s on line %d%A
--CLEAN--
<?php
