--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Unterminated heredoc must produce a compiler parse error (PH7 only)
--FILE--
<?php
if (true) {
<<<EOT
unterminated
// missing EOT marker here
}
?>
--EXPECTF--
%AParse error:%AUnclosed '{' on line 2%A
--CLEAN--
<?php

