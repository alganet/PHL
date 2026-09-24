--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PREG_UNMATCHED_AS_NULL reports EVERY group of the pattern, not only the ones pcre reached, and an unmatched group's offset pair is (NULL, -1)
--FILE--
<?php
// pcre answers only as many groups as the LAST one that participated, so a
// pattern's TRAILING optional groups are simply absent from $matches -- which is
// php's default shape too. PREG_UNMATCHED_AS_NULL is the flag that says "report
// them all"; reading it only for the groups pcre did report left the array a
// different LENGTH than php's, with the entries the caller asked for missing.
echo "preg_match\n";
preg_match('/(a)(x)?/', 'a', $pun_m, PREG_UNMATCHED_AS_NULL);
var_dump($pun_m);
preg_match('/(a)(x)?(y)?/', 'a', $pun_m, PREG_UNMATCHED_AS_NULL);
var_dump($pun_m);
// A MIDDLE unmatched group was reported all along; both must agree now.
preg_match('/(a)(x)?(b)/', 'ab', $pun_m, PREG_UNMATCHED_AS_NULL);
var_dump($pun_m);
// Without the flag the tail stays absent and a middle group is "".
preg_match('/(a)(x)?(b)/', 'ab', $pun_m);
var_dump($pun_m);
preg_match('/(a)(x)?/', 'a', $pun_m);
var_dump($pun_m);

echo "named groups in the tail\n";
preg_match('/(a)(?<nm>x)?/', 'a', $pun_m, PREG_UNMATCHED_AS_NULL);
var_dump($pun_m);

echo "with PREG_OFFSET_CAPTURE\n";
preg_match('/(a)(x)?/', 'a', $pun_m, PREG_OFFSET_CAPTURE | PREG_UNMATCHED_AS_NULL);
var_dump($pun_m);
preg_match('/(a)(x)?(b)/', 'ab', $pun_m, PREG_OFFSET_CAPTURE);
var_dump($pun_m);

echo "preg_match_all PATTERN_ORDER\n";
preg_match_all('/(a)(x)?/', 'aa', $pun_m, PREG_PATTERN_ORDER | PREG_UNMATCHED_AS_NULL);
var_dump($pun_m);
// The unmatched arm of PATTERN_ORDER ignored PREG_OFFSET_CAPTURE too: php pairs
// it with -1 rather than answering a bare string.
preg_match_all('/(a)(x)?/', 'aa', $pun_m, PREG_PATTERN_ORDER | PREG_OFFSET_CAPTURE);
var_dump($pun_m);
preg_match_all('/(a)(x)?/', 'aa', $pun_m, PREG_PATTERN_ORDER | PREG_OFFSET_CAPTURE | PREG_UNMATCHED_AS_NULL);
var_dump($pun_m);
preg_match_all('/(a)(?<nm>x)?/', 'a', $pun_m, PREG_PATTERN_ORDER | PREG_UNMATCHED_AS_NULL);
var_dump($pun_m);

echo "preg_match_all SET_ORDER\n";
preg_match_all('/(a)(x)?/', 'aa', $pun_m, PREG_SET_ORDER | PREG_UNMATCHED_AS_NULL);
var_dump($pun_m);

echo "preg_replace_callback\n";
preg_replace_callback('/(a)(x)?(y)?/', static function ($pun_c) {
    var_dump($pun_c);
    return 'Q';
}, 'a', -1, $pun_n, PREG_UNMATCHED_AS_NULL);
var_dump($pun_n);

// Duplicate group names -- `(?J)` -- give two numbered groups ONE key, and only
// one of them can have participated. A padded NULL must not land on top of the
// value the other one stored: php writes the name key from a non-participating
// group only when nothing is there yet.
echo "duplicate group names\n";
preg_match('/(?J)(?<d>a)|(?<d>b)/', 'a', $pun_m, PREG_UNMATCHED_AS_NULL);
var_dump($pun_m);
preg_match('/(?J)(?<d>a)|(?<d>b)/', 'b', $pun_m, PREG_UNMATCHED_AS_NULL);
var_dump($pun_m);
preg_match('/(?J)(?<d>a)(?<d>x)?/', 'a', $pun_m, PREG_UNMATCHED_AS_NULL);
var_dump($pun_m);
// The other order: a group that DID participate always wins, even over a NULL
// already written by an earlier duplicate.
preg_match('/(?J)(?<d>x)?(?<d>a)/', 'a', $pun_m, PREG_UNMATCHED_AS_NULL);
var_dump($pun_m);
// Neither participated: the first write stands.
preg_match('/(?J)(?<d>x)?(?<d>y)?/', '', $pun_m, PREG_UNMATCHED_AS_NULL);
var_dump($pun_m);
preg_match('/(?J)(?<d>a)|(?<d>b)/', 'a', $pun_m, PREG_OFFSET_CAPTURE | PREG_UNMATCHED_AS_NULL);
var_dump($pun_m);

echo "no match at all\n";
preg_match('/(a)(x)?/', 'z', $pun_m, PREG_UNMATCHED_AS_NULL);
var_dump($pun_m);
?>
--EXPECT--
preg_match
array(3) {
  [0]=>
  string(1) "a"
  [1]=>
  string(1) "a"
  [2]=>
  NULL
}
array(4) {
  [0]=>
  string(1) "a"
  [1]=>
  string(1) "a"
  [2]=>
  NULL
  [3]=>
  NULL
}
array(4) {
  [0]=>
  string(2) "ab"
  [1]=>
  string(1) "a"
  [2]=>
  NULL
  [3]=>
  string(1) "b"
}
array(4) {
  [0]=>
  string(2) "ab"
  [1]=>
  string(1) "a"
  [2]=>
  string(0) ""
  [3]=>
  string(1) "b"
}
array(2) {
  [0]=>
  string(1) "a"
  [1]=>
  string(1) "a"
}
named groups in the tail
array(4) {
  [0]=>
  string(1) "a"
  [1]=>
  string(1) "a"
  ["nm"]=>
  NULL
  [2]=>
  NULL
}
with PREG_OFFSET_CAPTURE
array(3) {
  [0]=>
  array(2) {
    [0]=>
    string(1) "a"
    [1]=>
    int(0)
  }
  [1]=>
  array(2) {
    [0]=>
    string(1) "a"
    [1]=>
    int(0)
  }
  [2]=>
  array(2) {
    [0]=>
    NULL
    [1]=>
    int(-1)
  }
}
array(4) {
  [0]=>
  array(2) {
    [0]=>
    string(2) "ab"
    [1]=>
    int(0)
  }
  [1]=>
  array(2) {
    [0]=>
    string(1) "a"
    [1]=>
    int(0)
  }
  [2]=>
  array(2) {
    [0]=>
    string(0) ""
    [1]=>
    int(-1)
  }
  [3]=>
  array(2) {
    [0]=>
    string(1) "b"
    [1]=>
    int(1)
  }
}
preg_match_all PATTERN_ORDER
array(3) {
  [0]=>
  array(2) {
    [0]=>
    string(1) "a"
    [1]=>
    string(1) "a"
  }
  [1]=>
  array(2) {
    [0]=>
    string(1) "a"
    [1]=>
    string(1) "a"
  }
  [2]=>
  array(2) {
    [0]=>
    NULL
    [1]=>
    NULL
  }
}
array(3) {
  [0]=>
  array(2) {
    [0]=>
    array(2) {
      [0]=>
      string(1) "a"
      [1]=>
      int(0)
    }
    [1]=>
    array(2) {
      [0]=>
      string(1) "a"
      [1]=>
      int(1)
    }
  }
  [1]=>
  array(2) {
    [0]=>
    array(2) {
      [0]=>
      string(1) "a"
      [1]=>
      int(0)
    }
    [1]=>
    array(2) {
      [0]=>
      string(1) "a"
      [1]=>
      int(1)
    }
  }
  [2]=>
  array(2) {
    [0]=>
    array(2) {
      [0]=>
      string(0) ""
      [1]=>
      int(-1)
    }
    [1]=>
    array(2) {
      [0]=>
      string(0) ""
      [1]=>
      int(-1)
    }
  }
}
array(3) {
  [0]=>
  array(2) {
    [0]=>
    array(2) {
      [0]=>
      string(1) "a"
      [1]=>
      int(0)
    }
    [1]=>
    array(2) {
      [0]=>
      string(1) "a"
      [1]=>
      int(1)
    }
  }
  [1]=>
  array(2) {
    [0]=>
    array(2) {
      [0]=>
      string(1) "a"
      [1]=>
      int(0)
    }
    [1]=>
    array(2) {
      [0]=>
      string(1) "a"
      [1]=>
      int(1)
    }
  }
  [2]=>
  array(2) {
    [0]=>
    array(2) {
      [0]=>
      NULL
      [1]=>
      int(-1)
    }
    [1]=>
    array(2) {
      [0]=>
      NULL
      [1]=>
      int(-1)
    }
  }
}
array(4) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "a"
  }
  [1]=>
  array(1) {
    [0]=>
    string(1) "a"
  }
  ["nm"]=>
  array(1) {
    [0]=>
    NULL
  }
  [2]=>
  array(1) {
    [0]=>
    NULL
  }
}
preg_match_all SET_ORDER
array(2) {
  [0]=>
  array(3) {
    [0]=>
    string(1) "a"
    [1]=>
    string(1) "a"
    [2]=>
    NULL
  }
  [1]=>
  array(3) {
    [0]=>
    string(1) "a"
    [1]=>
    string(1) "a"
    [2]=>
    NULL
  }
}
preg_replace_callback
array(4) {
  [0]=>
  string(1) "a"
  [1]=>
  string(1) "a"
  [2]=>
  NULL
  [3]=>
  NULL
}
int(1)
duplicate group names
array(4) {
  [0]=>
  string(1) "a"
  ["d"]=>
  string(1) "a"
  [1]=>
  string(1) "a"
  [2]=>
  NULL
}
array(4) {
  [0]=>
  string(1) "b"
  ["d"]=>
  string(1) "b"
  [1]=>
  NULL
  [2]=>
  string(1) "b"
}
array(4) {
  [0]=>
  string(1) "a"
  ["d"]=>
  string(1) "a"
  [1]=>
  string(1) "a"
  [2]=>
  NULL
}
array(4) {
  [0]=>
  string(1) "a"
  ["d"]=>
  string(1) "a"
  [1]=>
  NULL
  [2]=>
  string(1) "a"
}
array(4) {
  [0]=>
  string(0) ""
  ["d"]=>
  NULL
  [1]=>
  NULL
  [2]=>
  NULL
}
array(4) {
  [0]=>
  array(2) {
    [0]=>
    string(1) "a"
    [1]=>
    int(0)
  }
  ["d"]=>
  array(2) {
    [0]=>
    string(1) "a"
    [1]=>
    int(0)
  }
  [1]=>
  array(2) {
    [0]=>
    string(1) "a"
    [1]=>
    int(0)
  }
  [2]=>
  array(2) {
    [0]=>
    NULL
    [1]=>
    int(-1)
  }
}
no match at all
array(0) {
}
--CLEAN--
<?php
unset($pun_m, $pun_n);
