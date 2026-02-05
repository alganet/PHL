--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
quotemeta special characters
--FILE--
<?php
echo quotemeta(".+*?[^]($") . "\n";
?>
--EXPECT--
\.\+\*\?\[\^\]\(\$
--CLEAN--
<?php

