--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto with three labels
--FILE--
<?php
goto label2;
label2:
echo "label2\n";
goto label3;
label3:
echo "label3\n";
goto end;
end:
echo "end\n";
?>
--EXPECT--
label2
label3
end
--CLEAN--
<?php

