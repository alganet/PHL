--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto in function with multiple labels
--FILE--
<?php
function test() {
    goto label2;
    label1:
    echo "label1\n";
    goto end;
    label2:
    echo "label2\n";
    goto label1;
    end:
    echo "end\n";
}
test();
?>
--EXPECT--
label2
label1
end
--CLEAN--
<?php

