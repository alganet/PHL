--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A generator's finally that runs during exception unwind may itself contain an inline try/catch that swallows a locally-thrown exception (Face 2)
--FILE--
<?php
function g(){
    try {
        yield 1;
        throw new Exception("outer");   // finally runs during this unwind, on resume
    } finally {
        try {
            throw new Exception("inner");   // constructor call inside the inner try
        } catch (Exception $e) {
            echo "swallowed: ", $e->getMessage(), "\n";
        }
    }
}
try {
    foreach (g() as $v) { echo "got ", $v, "\n"; }
} catch (Exception $e) {
    echo "caller caught: ", $e->getMessage(), "\n";
}
echo "done\n";
?>
--EXPECT--
got 1
swallowed: inner
caller caught: outer
done
--CLEAN--
<?php
