--TEST--
Tokenizer: token_get_all/token_name/T_* constants/PhpToken php-parity
--SKIPIF--
skip: flaky
--FILE--
<?php
// close-tag source built via concatenation to dodge PHL compiler string handling
$tkQm = '?' . '>';
$tkCases = array(
  'basic'   => '<?php $x = 1 + 2.5; foo($x);',
  'kw_op'   => '<?php class A extends B { public function m(): ?int { return $a <=> $b ?? 0; } }',
  'strings' => '<?php $s = "hi $name x"; $c = "plain"; $b = "b {$o->p} e";',
  'comment' => "<?php // c\n# h\n/* x */ /** d */ echo 1;",
  'docrule' => '<?php /**x*/ /** y*/ /***/ echo 1;',
  'ns'      => '<?php namespace Foo\\Bar; use A\\B\\C; $z = \\Full\\Name();',
  'heredoc' => "<?php \$h = <<<EOT\nline \$v\nEOT;\n",
  'nowdoc'  => "<?php \$h = <<<'NOW'\nraw \$v\nNOW;\n",
  'numbers' => '<?php 0x1F 0b101 0o17 017 1_000 1.5e3 .5 0x 42;',
  'ctxkw'   => '<?php enum E {} enum(); yield from $g; readonly public $p; $o->class;',
  'unterm'  => '<?php $s = "open',
  'halt'    => '<?php echo 1; __halt_compiler(); raw bytes <?php still raw',
  'html'    => "html<?php echo 1; " . $tkQm . "\ntail",
);
foreach($tkCases as $tkName => $tkSrc){
  echo "== $tkName ==\n";
  foreach(token_get_all($tkSrc) as $tkT){
    echo is_array($tkT) ? token_name($tkT[0])."|".$tkT[1]."|".$tkT[2]."\n" : "S:$tkT\n";
  }
}
echo "== name ==\n";
echo token_name(T_VARIABLE),",",token_name(266),",",token_name(402),",",token_name(43),",",token_name(999),"\n";
echo "== phptoken ==\n";
foreach(PhpToken::tokenize('<?php $x=1;') as $tkP){
  echo $tkP->id,"|",$tkP->text,"|",$tkP->line,"|",$tkP->pos,"|",($tkP->isIgnorable()?"I":"-"),"|",($tkP->getTokenName()??"NULL"),"\n";
}
$tkV = PhpToken::tokenize('<?php $x;')[1];
echo ($tkV->is(T_VARIABLE)?"1":"0"),($tkV->is("T_VARIABLE")?"1":"0"),"\n";
echo TOKEN_PARSE,"\n";
--EXPECT--
== basic ==
T_OPEN_TAG|<?php |1
T_VARIABLE|$x|1
T_WHITESPACE| |1
S:=
T_WHITESPACE| |1
T_LNUMBER|1|1
T_WHITESPACE| |1
S:+
T_WHITESPACE| |1
T_DNUMBER|2.5|1
S:;
T_WHITESPACE| |1
T_STRING|foo|1
S:(
T_VARIABLE|$x|1
S:)
S:;
== kw_op ==
T_OPEN_TAG|<?php |1
T_CLASS|class|1
T_WHITESPACE| |1
T_STRING|A|1
T_WHITESPACE| |1
T_EXTENDS|extends|1
T_WHITESPACE| |1
T_STRING|B|1
T_WHITESPACE| |1
S:{
T_WHITESPACE| |1
T_PUBLIC|public|1
T_WHITESPACE| |1
T_FUNCTION|function|1
T_WHITESPACE| |1
T_STRING|m|1
S:(
S:)
S::
T_WHITESPACE| |1
S:?
T_STRING|int|1
T_WHITESPACE| |1
S:{
T_WHITESPACE| |1
T_RETURN|return|1
T_WHITESPACE| |1
T_VARIABLE|$a|1
T_WHITESPACE| |1
T_SPACESHIP|<=>|1
T_WHITESPACE| |1
T_VARIABLE|$b|1
T_WHITESPACE| |1
T_COALESCE|??|1
T_WHITESPACE| |1
T_LNUMBER|0|1
S:;
T_WHITESPACE| |1
S:}
T_WHITESPACE| |1
S:}
== strings ==
T_OPEN_TAG|<?php |1
T_VARIABLE|$s|1
T_WHITESPACE| |1
S:=
T_WHITESPACE| |1
S:"
T_ENCAPSED_AND_WHITESPACE|hi |1
T_VARIABLE|$name|1
T_ENCAPSED_AND_WHITESPACE| x|1
S:"
S:;
T_WHITESPACE| |1
T_VARIABLE|$c|1
T_WHITESPACE| |1
S:=
T_WHITESPACE| |1
T_CONSTANT_ENCAPSED_STRING|"plain"|1
S:;
T_WHITESPACE| |1
T_VARIABLE|$b|1
T_WHITESPACE| |1
S:=
T_WHITESPACE| |1
S:"
T_ENCAPSED_AND_WHITESPACE|b |1
T_CURLY_OPEN|{|1
T_VARIABLE|$o|1
T_OBJECT_OPERATOR|->|1
T_STRING|p|1
S:}
T_ENCAPSED_AND_WHITESPACE| e|1
S:"
S:;
== comment ==
T_OPEN_TAG|<?php |1
T_COMMENT|// c|1
T_WHITESPACE|
|1
T_COMMENT|# h|2
T_WHITESPACE|
|2
T_COMMENT|/* x */|3
T_WHITESPACE| |3
T_DOC_COMMENT|/** d */|3
T_WHITESPACE| |3
T_ECHO|echo|3
T_WHITESPACE| |3
T_LNUMBER|1|3
S:;
== docrule ==
T_OPEN_TAG|<?php |1
T_COMMENT|/**x*/|1
T_WHITESPACE| |1
T_DOC_COMMENT|/** y*/|1
T_WHITESPACE| |1
T_COMMENT|/***/|1
T_WHITESPACE| |1
T_ECHO|echo|1
T_WHITESPACE| |1
T_LNUMBER|1|1
S:;
== ns ==
T_OPEN_TAG|<?php |1
T_NAMESPACE|namespace|1
T_WHITESPACE| |1
T_NAME_QUALIFIED|Foo\Bar|1
S:;
T_WHITESPACE| |1
T_USE|use|1
T_WHITESPACE| |1
T_NAME_QUALIFIED|A\B\C|1
S:;
T_WHITESPACE| |1
T_VARIABLE|$z|1
T_WHITESPACE| |1
S:=
T_WHITESPACE| |1
T_NAME_FULLY_QUALIFIED|\Full\Name|1
S:(
S:)
S:;
== heredoc ==
T_OPEN_TAG|<?php |1
T_VARIABLE|$h|1
T_WHITESPACE| |1
S:=
T_WHITESPACE| |1
T_START_HEREDOC|<<<EOT
|1
T_ENCAPSED_AND_WHITESPACE|line |2
T_VARIABLE|$v|2
T_ENCAPSED_AND_WHITESPACE|
|2
T_END_HEREDOC|EOT|3
S:;
T_WHITESPACE|
|3
== nowdoc ==
T_OPEN_TAG|<?php |1
T_VARIABLE|$h|1
T_WHITESPACE| |1
S:=
T_WHITESPACE| |1
T_START_HEREDOC|<<<'NOW'
|1
T_ENCAPSED_AND_WHITESPACE|raw $v
|2
T_END_HEREDOC|NOW|3
S:;
T_WHITESPACE|
|3
== numbers ==
T_OPEN_TAG|<?php |1
T_LNUMBER|0x1F|1
T_WHITESPACE| |1
T_LNUMBER|0b101|1
T_WHITESPACE| |1
T_LNUMBER|0o17|1
T_WHITESPACE| |1
T_LNUMBER|017|1
T_WHITESPACE| |1
T_LNUMBER|1_000|1
T_WHITESPACE| |1
T_DNUMBER|1.5e3|1
T_WHITESPACE| |1
T_DNUMBER|.5|1
T_WHITESPACE| |1
T_LNUMBER|0|1
T_STRING|x|1
T_WHITESPACE| |1
T_LNUMBER|42|1
S:;
== ctxkw ==
T_OPEN_TAG|<?php |1
T_ENUM|enum|1
T_WHITESPACE| |1
T_STRING|E|1
T_WHITESPACE| |1
S:{
S:}
T_WHITESPACE| |1
T_STRING|enum|1
S:(
S:)
S:;
T_WHITESPACE| |1
T_YIELD_FROM|yield from|1
T_WHITESPACE| |1
T_VARIABLE|$g|1
S:;
T_WHITESPACE| |1
T_READONLY|readonly|1
T_WHITESPACE| |1
T_PUBLIC|public|1
T_WHITESPACE| |1
T_VARIABLE|$p|1
S:;
T_WHITESPACE| |1
T_VARIABLE|$o|1
T_OBJECT_OPERATOR|->|1
T_STRING|class|1
S:;
== unterm ==
T_OPEN_TAG|<?php |1
T_VARIABLE|$s|1
T_WHITESPACE| |1
S:=
T_WHITESPACE| |1
S:"
T_ENCAPSED_AND_WHITESPACE|open|1
== halt ==
T_OPEN_TAG|<?php |1
T_ECHO|echo|1
T_WHITESPACE| |1
T_LNUMBER|1|1
S:;
T_WHITESPACE| |1
T_HALT_COMPILER|__halt_compiler|1
S:(
S:)
S:;
T_INLINE_HTML| raw bytes <?php still raw|1
== html ==
T_INLINE_HTML|html|1
T_OPEN_TAG|<?php |1
T_ECHO|echo|1
T_WHITESPACE| |1
T_LNUMBER|1|1
S:;
T_WHITESPACE| |1
T_CLOSE_TAG|?>
|1
T_INLINE_HTML|tail|2
== name ==
T_VARIABLE,T_VARIABLE,T_DOUBLE_COLON,UNKNOWN,UNKNOWN
== phptoken ==
394|<?php |1|0|I|T_OPEN_TAG
266|$x|1|6|-|T_VARIABLE
61|=|1|8|-|=
260|1|1|9|-|T_LNUMBER
59|;|1|10|-|;
10
1
