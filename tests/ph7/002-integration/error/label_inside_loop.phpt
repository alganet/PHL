--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Label inside loop construct error
--FILE--
<?php
// This should trigger a compile error: "Label 'label' inside loop or try/catch block is disallowed"
for ($i = 0; $i < 1; $i++) {
    label:
    echo "test";
}
?>
--EXPECTF--
%Atest%A
--CLEAN--
<?php

