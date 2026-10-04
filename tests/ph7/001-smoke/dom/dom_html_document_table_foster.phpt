--TEST--
Content a table cannot hold is moved out in front of it
--FILE--
<?php
/* A table's own insertion modes accept only table furniture.  Anything else
 * written while a table, a row group or a row is the current node is FOSTER
 * PARENTED: moved out to just before the table, while the stack keeps it
 * nested so what follows goes inside it.  Whitespace is the one run that
 * stays, a `<form>` is inserted and popped at once, and a `<table>` inside a
 * table closes the one it was written in. */
$cases = [
	/* The fostered node leaves, the formatting around it stays around it. */
	'text'          => '<b>a<table>b</b>c</table>',
	'text-lone'     => '<table>x</table>',
	'space-only'    => '<table>  </table>',
	'space-mixed'   => '<table> x </table>',
	'in-group'      => '<table><tbody>q<tr><td>a',
	'in-row'        => '<table><tr> z <td>a</td></tr></table>',
	'after-cell'    => '<table><td>c</td>q</table>',
	'whole-run'     => '<table>a<b>bb</b>c</table>',
	'reconstructed' => '<table><tr><b>bb</b></tr></table>',
	'nested-fmt'    => '<b><i><table>t</table></i></b>',
	'in-div'        => '<div><table>x</table></div>',

	/* A start tag leaves as an element and keeps collecting its children. */
	'element'       => '<table><div>d</div></table>',
	'foreign'       => '<table><svg>s</svg></table>',
	'select'        => '<table><select><option>o</select></table>',
	'input'         => '<table><input></table>',
	'input-hidden'  => '<table><input type=hidden></table>',
	'script-stays'  => '<table><script>s</script>q</table>',
	'caption-stays' => '<table><caption>c</caption>q</table>',

	/* A cell or a row written after something left goes back to the table. */
	'cell-after'    => '<table><p><th>c</th></table>',
	'row-after'     => '<table><h1>h<tr><td>d</td></tr></table>',

	/* The column group: implied by a `<col>`, closed by anything else. */
	'col-implied'   => '<table><col></table>',
	'col-group'     => '<table><colgroup><col><col></colgroup></table>',
	'col-then-text' => '<table><colgroup><col></colgroup>t</table>',
	'col-then-el'   => '<table><colgroup><span>s</span></colgroup></table>',

	/* A form is kept but never fills, and a table closes a table. */
	'form'          => '<table><form><b>q</b></table>',
	'form-in-cell'  => '<table><tr><td><form><b>q</b></td></tr></table>',
	'table-in-table'=> '<table>1<table>2</table>3</table>',
	'table-in-cell' => '<table><td>1<table>2</table>3</td></table>',
];
foreach ($cases as $zName => $zSrc) {
	$oDoc = \Dom\HTMLDocument::createFromString($zSrc, LIBXML_NOERROR);
	printf("%-15s %s\n", $zName, $oDoc->saveHtml($oDoc->body));
}
--EXPECT--
text            <body><b>abc<table></table></b></body>
text-lone       <body>x<table></table></body>
space-only      <body><table>  </table></body>
space-mixed     <body> x <table></table></body>
in-group        <body>q<table><tbody><tr><td>a</td></tr></tbody></table></body>
in-row          <body> z <table><tbody><tr><td>a</td></tr></tbody></table></body>
after-cell      <body>q<table><tbody><tr><td>c</td></tr></tbody></table></body>
whole-run       <body>a<b>bb</b>c<table></table></body>
reconstructed   <body><b>bb</b><table><tbody><tr></tr></tbody></table></body>
nested-fmt      <body><b><i>t<table></table></i></b></body>
in-div          <body><div>x<table></table></div></body>
element         <body><div>d</div><table></table></body>
foreign         <body><svg>s</svg><table></table></body>
select          <body><select><option>o</option></select><table></table></body>
input           <body><input><table></table></body>
input-hidden    <body><table><input type="hidden"></table></body>
script-stays    <body>q<table><script>s</script></table></body>
caption-stays   <body>q<table><caption>c</caption></table></body>
cell-after      <body><p></p><table><tbody><tr><th>c</th></tr></tbody></table></body>
row-after       <body><h1>h</h1><table><tbody><tr><td>d</td></tr></tbody></table></body>
col-implied     <body><table><colgroup><col></colgroup></table></body>
col-group       <body><table><colgroup><col><col></colgroup></table></body>
col-then-text   <body>t<table><colgroup><col></colgroup></table></body>
col-then-el     <body><span>s</span><table><colgroup></colgroup></table></body>
form            <body><b>q</b><table><form></form></table></body>
form-in-cell    <body><table><tbody><tr><td><form><b>q</b></form></td></tr></tbody></table></body>
table-in-table  <body>1<table></table>2<table></table>3</body>
table-in-cell   <body><table><tbody><tr><td>12<table></table>3</td></tr></tbody></table></body>
