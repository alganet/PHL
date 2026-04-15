--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Throwable: `extends \` with no identifier produces a fatal
--FILE--
<?php
class BareBsExt extends \ {}
?>
--EXPECTF--
%APHP %s error: %A
--CLEAN--
<?php
