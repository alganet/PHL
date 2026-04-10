--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 7.3 indented nowdoc as function argument
--FILE--
<?php
function pair($a, $b) {
    return "[$a][$b]";
}
echo pair(<<<'EOT'
    raw $var text
    EOT, "second") . "\n";
--EXPECT--
[raw $var text][second]
--CLEAN--
<?php

