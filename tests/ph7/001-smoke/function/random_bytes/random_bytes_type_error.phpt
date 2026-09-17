--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
random_bytes throws TypeError on non-int, non-numeric arguments

--FILE--
<?php
$arr = array();
try { random_bytes($arr); echo "n_arr\n"; } catch (TypeError $e) { echo "te_arr\n"; }
try { random_bytes("abc"); echo "n_str\n"; } catch (TypeError $e) { echo "te_str\n"; }
?>
--EXPECT--
te_arr
te_str
--CLEAN--
<?php
