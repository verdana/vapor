--TEST--
Check for vapor Template::__constructor()
--SKIPIF--
<?php if (!extension_loaded("vapor")) {
    print "skip";
}
?>
--FILE--
<?php
$v = new Vapor\Engine('/tmp', 'php');
$t = new Vapor\Template($v, 'home');

var_dump($v);
var_dump($t);
?>
--EXPECT--
object(Vapor\Engine)#1 (2) {
  ["basepath"]=>
  string(4) "/tmp"
  ["extension"]=>
  string(3) "php"
}
object(Vapor\Template)#2 (0) {
}
