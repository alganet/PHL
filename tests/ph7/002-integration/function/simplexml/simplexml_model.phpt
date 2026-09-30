--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/simplexml: the four questions an element object asks
--FILE--
<?php
/* What a SimpleXMLElement IS: a node plus one of php's four questions about it,
 * and what every read surface answers for each. */
function sxShow(string $label, $v): void {
    if (!$v instanceof SimpleXMLElement) { printf("%-12s %s\n", $label, var_export($v, true)); return; }
    printf("%-12s name=%-6s count=%d string=%-12s bool=%s\n", $label, $v->getName(), count($v),
        var_export((string) $v, true), var_export((bool) $v, true));
    printf("             table=%s\n", str_replace("\n", '', var_export((array) $v, true)));
    $keys = [];
    foreach ($v as $k => $w) { $keys[] = $k . ':' . trim((string) $w); }
    printf("             iter=[%s] json=%s\n", implode('|', $keys), json_encode($v));
}

echo "-- the four questions\n";
$doc = '<r a="1" b="2"><c x="9">one</c><c>two</c><d><e>ee</e></d>tail</r>';
$x = simplexml_load_string($doc);
sxShow('this node', $x);
sxShow('->c', $x->c);
sxShow('->c[1]', $x->c[1]);
sxShow('->d', $x->d);
sxShow('->missing', $x->missing);
sxShow('children()', $x->children());
sxShow('attributes()', $x->attributes());
sxShow("['a']", $x['a']);
sxShow("['zz']", $x['zz']);

echo "-- the table's rules, one document shape at a time\n";
$shapes = [
    'empty'          => '<r/>',
    'text only'      => '<r>only text</r>',
    'blank only'     => '<r>  </r>',
    'text + element' => '<r>a<c/>b</r>',
    'one text kid'   => '<r><s>x</s></r>',
    'two text kids'  => '<r><s>1</s><s>2</s></r>',
    'two empty kids' => '<r><s/><s/></r>',
    'deep kid'       => '<r><s><t/></s></r>',
    'mixed kid'      => '<r><s>a<u/>b</s></r>',
    'blank kid'      => '<r><s>  </s></r>',
    'cdata'          => '<r><![CDATA[cd]]></r>',
    'comment + pi'   => '<r>a<!--x--><?pi z?></r>',
];
foreach ($shapes as $name => $src) {
    $s = simplexml_load_string($src);
    printf("%-15s self=%-42s ->s=%-28s children()=%s\n", $name,
        str_replace("\n", '', var_export((array) $s, true)),
        str_replace("\n", '', var_export((array) $s->s, true)),
        str_replace("\n", '', var_export((array) $s->children(), true)));
}

echo "-- namespaces are a FILTER, and it travels through navigation\n";
$ns = '<root xmlns="urn:d" xmlns:a="urn:a" a:at="AV" plain="PV">'
    . '<kid>k1</kid><a:kid><a:in>AI</a:in><p>PP</p></a:kid><b/></root>';
$n = simplexml_load_string($ns);
foreach ([null, '', 'urn:d', 'urn:a', 'a'] as $filter) {
    foreach ([false, true] as $isPrefix) {
        $ch = $n->children($filter, $isPrefix);
        $at = $n->attributes($filter, $isPrefix);
        $names = []; foreach ($ch as $k => $v) { $names[] = $k; }
        $attrs = []; foreach ($at as $k => $v) { $attrs[] = "$k=$v"; }
        printf("  %-7s prefix=%d children=[%s] attrs=[%s] ->kid=%s\n",
            var_export($filter, true), (int) $isPrefix, implode(',', $names),
            implode(',', $attrs), var_export(trim((string) $ch->kid), true));
    }
}
var_dump($n->getNamespaces(), $n->getNamespaces(true), $n->getDocNamespaces());
var_dump($n->kid->getNamespaces(), $n->kid->getDocNamespaces(false, false));

echo "-- the iterator, and the recursive one\n";
$it = simplexml_load_string($doc);
for ($it->rewind(); $it->valid(); $it->next()) {
    printf("  key=%-3s current=%-6s hasChildren=%d getChildren=%s\n", $it->key(),
        trim((string) $it->current()), (int) $it->hasChildren(),
        $it->getChildren() === null ? 'NULL' : $it->getChildren()->getName());
}
try { $it->current(); } catch (Throwable $e) { echo '  ', get_class($e), ': ', $e->getMessage(), "\n"; }
$rec = new RecursiveIteratorIterator(new SimpleXMLIterator($doc), RecursiveIteratorIterator::SELF_FIRST);
foreach ($rec as $k => $v) { printf("  depth=%d %s=%s\n", $rec->getDepth(), $k, var_export(trim((string) $v), true)); }

echo "-- casts, comparison and the presentation surfaces\n";
$num = simplexml_load_string('<r><i>5</i><f>2.5</f><s>abc</s></r>');
var_dump((int) $num->i, (float) $num->f, (int) $num->s, $num->i + 1, $num->f * 2,
         $num->i == 5, $num->i == '5', $num->i > 4, intdiv((int) $num->i, 1));
$a = simplexml_load_string($doc);
$b = simplexml_load_string($doc);
var_dump($a == $b, $a === $b, $a->c == $a->c, $a->c === $a->c, $a->c == $a->d);
var_dump(get_object_vars($a->d));
var_export($a->d); echo "\n";
print_r($a->d);
/* Object HANDLES never match between engines, so the debug tables are
 * rendered without them. */
echo str_replace("\n", '', var_export(array_map(
    fn($v) => $v instanceof SimpleXMLElement ? 'SXE:' . var_export((array) $v, true) : $v,
    $a->d->__debugInfo()), true)), "\n";
echo str_replace("\n", '', var_export(array_map(
    fn($v) => $v instanceof SimpleXMLElement ? 'SXE:' . var_export((array) $v, true) : $v,
    $a->children()->__debugInfo()), true)), "\n";
--EXPECT--
-- the four questions
this node    name=r      count=3 string='tail'       bool=true
             table=array (  '@attributes' =>   array (    'a' => '1',    'b' => '2',  ),  'c' =>   array (    0 => 'one',    1 => 'two',  ),  'd' =>   \SimpleXMLElement::__set_state(array(     'e' => 'ee',  )),)
             iter=[c:one|c:two|d:] json={"@attributes":{"a":"1","b":"2"},"c":["one","two"],"d":{"e":"ee"}}
->c          name=c      count=2 string='one'        bool=true
             table=array (  '@attributes' =>   array (    'x' => '9',  ),  0 => 'one',  1 => 'two',)
             iter=[c:one|c:two] json={"@attributes":{"x":"9"},"0":"one","1":"two"}
->c[1]       name=c      count=0 string='two'        bool=true
             table=array (  0 => 'two',)
             iter=[] json={"0":"two"}
->d          name=d      count=1 string=''           bool=true
             table=array (  'e' => 'ee',)
             iter=[d:] json={"e":"ee"}
->missing    name=       count=0 string=''           bool=false
             table=array ()
             iter=[] json={}
children()   name=c      count=3 string='one'        bool=true
             table=array (  'c' =>   array (    0 => 'one',    1 => 'two',  ),  'd' =>   \SimpleXMLElement::__set_state(array(     'e' => 'ee',  )),)
             iter=[c:one|c:two|d:] json={"c":["one","two"],"d":{"e":"ee"}}
attributes() name=a      count=2 string='1'          bool=true
             table=array (  '@attributes' =>   array (    'a' => '1',    'b' => '2',  ),)
             iter=[a:1|b:2] json={"@attributes":{"a":"1","b":"2"}}
['a']        name=a      count=0 string='1'          bool=true
             table=array (  0 => '1',)
             iter=[] json={"0":"1"}
['zz']       NULL
-- the table's rules, one document shape at a time
empty           self=array ()                                   ->s=array ()                     children()=array ()
text only       self=array (  0 => 'only text',)                ->s=array ()                     children()=array ()
blank only      self=array ()                                   ->s=array ()                     children()=array ()
text + element  self=array (  'c' =>   \SimpleXMLElement::__set_state(array(  )),) ->s=array ()                     children()=array (  'c' =>   \SimpleXMLElement::__set_state(array(  )),)
one text kid    self=array (  's' => 'x',)                      ->s=array (  0 => 'x',)          children()=array (  's' => 'x',)
two text kids   self=array (  's' =>   array (    0 => '1',    1 => '2',  ),) ->s=array (  0 => '1',  1 => '2',) children()=array (  's' =>   array (    0 => '1',    1 => '2',  ),)
two empty kids  self=array (  's' =>   array (    0 =>     \SimpleXMLElement::__set_state(array(    )),    1 =>     \SimpleXMLElement::__set_state(array(    )),  ),) ->s=array ()                     children()=array (  's' =>   array (    0 =>     \SimpleXMLElement::__set_state(array(    )),    1 =>     \SimpleXMLElement::__set_state(array(    )),  ),)
deep kid        self=array (  's' =>   \SimpleXMLElement::__set_state(array(     't' =>     \SimpleXMLElement::__set_state(array(    )),  )),) ->s=array (  't' =>   \SimpleXMLElement::__set_state(array(  )),) children()=array (  's' =>   \SimpleXMLElement::__set_state(array(     't' =>     \SimpleXMLElement::__set_state(array(    )),  )),)
mixed kid       self=array (  's' => 'ab',)                     ->s=array (  'u' =>   \SimpleXMLElement::__set_state(array(  )),) children()=array (  's' => 'ab',)
blank kid       self=array (  's' =>   \SimpleXMLElement::__set_state(array(  )),) ->s=array ()                     children()=array (  's' =>   \SimpleXMLElement::__set_state(array(  )),)
cdata           self=array ()                                   ->s=array ()                     children()=array ()
comment + pi    self=array (  'comment' =>   \SimpleXMLElement::__set_state(array(  )),  'pi' =>   \SimpleXMLElement::__set_state(array(  )),) ->s=array ()                     children()=array ()
-- namespaces are a FILTER, and it travels through navigation
  NULL    prefix=0 children=[kid,b] attrs=[plain=PV] ->kid='k1'
  NULL    prefix=1 children=[kid,b] attrs=[plain=PV] ->kid='k1'
  ''      prefix=0 children=[kid,b] attrs=[plain=PV] ->kid='k1'
  ''      prefix=1 children=[kid,b] attrs=[plain=PV] ->kid='k1'
  'urn:d' prefix=0 children=[kid,b] attrs=[] ->kid='k1'
  'urn:d' prefix=1 children=[] attrs=[] ->kid=''
  'urn:a' prefix=0 children=[kid] attrs=[at=AV] ->kid=''
  'urn:a' prefix=1 children=[] attrs=[] ->kid=''
  'a'     prefix=0 children=[] attrs=[] ->kid=''
  'a'     prefix=1 children=[kid] attrs=[at=AV] ->kid=''
array(2) {
  [""]=>
  string(5) "urn:d"
  ["a"]=>
  string(5) "urn:a"
}
array(2) {
  [""]=>
  string(5) "urn:d"
  ["a"]=>
  string(5) "urn:a"
}
array(2) {
  [""]=>
  string(5) "urn:d"
  ["a"]=>
  string(5) "urn:a"
}
array(1) {
  [""]=>
  string(5) "urn:d"
}
array(2) {
  [""]=>
  string(5) "urn:d"
  ["a"]=>
  string(5) "urn:a"
}
-- the iterator, and the recursive one
  key=c   current=one    hasChildren=0 getChildren=c
  key=c   current=two    hasChildren=0 getChildren=c
  key=d   current=       hasChildren=1 getChildren=d
  Error: Iterator not initialized or already consumed
  depth=0 c='one'
  depth=0 c='two'
  depth=0 d=''
  depth=1 e='ee'
-- casts, comparison and the presentation surfaces
int(5)
float(2.5)
int(0)
int(6)
float(5)
bool(true)
bool(true)
bool(true)
int(5)
bool(false)
bool(false)
bool(true)
bool(false)
bool(true)
array(1) {
  ["e"]=>
  string(2) "ee"
}
\SimpleXMLElement::__set_state(array(
   'e' => 'ee',
))
SimpleXMLElement Object
(
    [e] => ee
)
array (  'e' => 'ee',)
array (  '@attributes' =>   array (    'a' => '1',    'b' => '2',  ),  'c' =>   array (    0 => 'one',    1 => 'two',  ),  'd' => 'SXE:array (  \'e\' => \'ee\',)',)
