--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
explode with a negative limit drops the last -limit components (PHP-exact)
--FILE--
<?php
function show($r){ echo json_encode($r), "\n"; }
show(explode(",", "a,b,c,d", -1));   // ["a","b","c"]
show(explode(",", "a,b,c,d", -2));   // ["a","b"]
show(explode(",", "a,b,c,d", -3));   // ["a"]
show(explode(",", "a,b,c,d", -4));   // []  (drops all)
show(explode(",", "a,b,c,d", -5));   // []  (drops more than exist)
show(explode(",", "abc", -1));       // []  (no delimiter -> 1 component, dropped)
show(explode(",", "", -1));          // []  (empty string -> sole component dropped)
show(explode(",", "a,,c", -1));      // ["a",""]  (empty components preserved)
show(explode("::", "a::b::c::d", -1)); // ["a","b","c"] (multi-byte delimiter)
// Controls: positive / zero / empty stay unchanged
show(explode(",", "a,b,c,d", 2));    // ["a","b,c,d"]
show(explode(",", "a,b,c,d", 0));    // ["a,b,c,d"]
show(explode(",", "", 5));           // [""]
?>
--EXPECT--
["a","b","c"]
["a","b"]
["a"]
[]
[]
[]
[]
["a",""]
["a","b","c"]
["a","b,c,d"]
["a,b,c,d"]
[""]
--CLEAN--
<?php
