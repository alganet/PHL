--CREDITS--
OpenAI (ChatGPT)
--TEST--
Testing strspn function
--FILE--
<?php
// basic usage
echo strspn("123abc456", "0123456789") . "\n"; // should return 3
// starting in the middle
echo strspn("abc123", "xyzabc") . "\n"; // should return 3
// empty mask
echo strspn("abc", "") . "\n"; // should return 0
// zero-length & others
echo strspn("", "abc") . "\n"; // should return 0

// with mask that contains none
echo strspn("abc", "def") . "\n"; // should return 0

// with long run at start
echo strspn("aaaaab", "a") . "\n"; // should return 5

// using mask with range (behaviour for hyphen is not documented in PH7, treat literally)
echo strspn("--a--", "-a") . "\n";

?>
--EXPECT--
3
3
0
0
0
5
5
--CLEAN--
<?php

