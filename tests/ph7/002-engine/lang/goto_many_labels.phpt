--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto with many labels to test label lookup loop
--FILE--
<?php
goto label10;
label1:
echo "label1\n";
goto end;
label2:
echo "label2\n";
goto label1;
label3:
echo "label3\n";
goto label2;
label4:
echo "label4\n";
goto label3;
label5:
echo "label5\n";
goto label4;
label6:
echo "label6\n";
goto label5;
label7:
echo "label7\n";
goto label6;
label8:
echo "label8\n";
goto label7;
label9:
echo "label9\n";
goto label8;
label10:
echo "label10\n";
goto label9;
end:
echo "end\n";
?>
--EXPECT--
label10
label9
label8
label7
label6
label5
label4
label3
label2
label1
end