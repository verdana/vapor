--TEST--
Check for vapor Template::__constructor()
--SKIPIF--
<?php if (!extension_loaded("vapor")) {
    print "skip";
}
?>
--FILE--
<?php
$fp = tmpfile();
fwrite($fp, '<h1>The quick brown fox jumped over the lazy dog.</h1>');
$filename = basename(stream_get_meta_data($fp)['uri']);

$v = new Vapor\Engine('/tmp');
$t = new Vapor\Template($v, $filename);

var_dump($v);
var_dump($t);
?>
--EXPECT--
object(Vapor\Engine)#1 (3) {
  ["basepath"]=>
  string(4) "/tmp"
  ["extension"]=>
  NULL
  ["exception"]=>
  bool(false)
}
object(Vapor\Template)#2 (0) {
}
