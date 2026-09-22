--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ksort()/krsort() compare two string keys as VALUES, not bytewise
--FILE--
<?php
// php compares two array KEYS exactly the way it compares two VALUES, so two
// NUMERIC string keys compare numerically. PHL compared blob keys bytewise, so
// '10.0' sorted before '9.0' and '1e3' (1000) before '20'; keys that compare
// EQUAL also lost php's stable order.
$sets = [
    'numeric strings'   => ['10.0' => 1, '9.0' => 2],
    'exponent'          => ['1e3' => 1, '20' => 2],
    'leading space'     => [' 10' => 1, ' 9' => 2],
    'negative floats'   => ['-1.5' => 1, '-2.5' => 2],
    'fraction'          => ['0.1' => 1, '0.09' => 2],
    'all equal'         => ['1.0' => 1, 1 => 2, '01' => 3, '1e0' => 4],
    'mixed non-numeric' => ['b' => 1, '10.0' => 2, 'a' => 3, '9.0' => 4],
    'int vs word'       => ['x' => 1, 10 => 2],
    'plain words'       => ['b' => 1, 'a' => 2, 'B' => 3],
    'empty key'         => ['' => 1, '0' => 2, 'a' => 3],
    'leading zeros'     => ['01' => 1, '00' => 2, '0.5' => 3],
];
foreach ($sets as $name => $set) {
    $a = $set; ksort($a);
    $b = $set; krsort($b);
    $c = $set; ksort($c, SORT_STRING);
    $d = $set; ksort($d, SORT_NUMERIC);
    echo str_pad($name, 20), "\n";
    echo '  k: ', json_encode(array_keys($a)), "\n";
    echo '  r: ', json_encode(array_keys($b)), "\n";
    echo '  s: ', json_encode(array_keys($c)), "\n";
    echo '  n: ', json_encode(array_keys($d)), "\n";
}
?>
--EXPECT--
numeric strings     
  k: ["9.0","10.0"]
  r: ["10.0","9.0"]
  s: ["10.0","9.0"]
  n: ["9.0","10.0"]
exponent            
  k: [20,"1e3"]
  r: ["1e3",20]
  s: ["1e3",20]
  n: [20,"1e3"]
leading space       
  k: [" 9"," 10"]
  r: [" 10"," 9"]
  s: [" 10"," 9"]
  n: [" 9"," 10"]
negative floats     
  k: ["-2.5","-1.5"]
  r: ["-1.5","-2.5"]
  s: ["-1.5","-2.5"]
  n: ["-2.5","-1.5"]
fraction            
  k: ["0.09","0.1"]
  r: ["0.1","0.09"]
  s: ["0.09","0.1"]
  n: ["0.09","0.1"]
all equal           
  k: ["1.0",1,"01","1e0"]
  r: ["1.0",1,"01","1e0"]
  s: ["01",1,"1.0","1e0"]
  n: ["1.0",1,"01","1e0"]
mixed non-numeric   
  k: ["9.0","10.0","a","b"]
  r: ["b","a","10.0","9.0"]
  s: ["10.0","9.0","a","b"]
  n: ["b","a","9.0","10.0"]
int vs word         
  k: [10,"x"]
  r: ["x",10]
  s: [10,"x"]
  n: ["x",10]
plain words         
  k: ["B","a","b"]
  r: ["b","a","B"]
  s: ["B","a","b"]
  n: ["b","a","B"]
empty key           
  k: ["",0,"a"]
  r: ["a",0,""]
  s: ["",0,"a"]
  n: ["",0,"a"]
leading zeros       
  k: ["00","0.5","01"]
  r: ["01","0.5","00"]
  s: ["0.5","00","01"]
  n: ["00","0.5","01"]
--CLEAN--
<?php
unset($sets, $name, $set, $a, $b, $c, $d);
