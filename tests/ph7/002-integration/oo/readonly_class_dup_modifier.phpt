--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a repeated class modifier (readonly readonly) is rejected
--FILE--
<?php
readonly readonly class RoDup {}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Multiple readonly modifiers are not allowed%A
--CLEAN--
<?php
