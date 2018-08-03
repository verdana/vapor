--TEST--
Check for vapor insert()
--SKIPIF--
<?php if (!extension_loaded("vapor")) {
    print "skip";
}
?>
--FILE--
<?php
is_dir("/tmp/views/layout") or mkdir("/tmp/views/layout", 0777, true);
is_dir("/tmp/views/shared") or mkdir("/tmp/views/shared", 0777, true);

// header file
$header = <<<'EOF'
<header>vapor-test-007</header>
EOF;
file_put_contents('/tmp/views/shared/header.php', $header);

// layout file
$layout = <<<'EOF'
<?php $this->insert('shared::header') ?>
<div class="layout-container"><?= $this->section('content') ?></div>
EOF;
file_put_contents('/tmp/views/layout/default.php', $layout);

// template file
$index = <<<'EOF'
<?php $this->layout('layout::default') ?>
<span>The quick <?= $animal ?> jumped over the lazy dog</span>
EOF;
file_put_contents('/tmp/views/index.php', $index);

// vapor
$v1 = new Vapor('/tmp/views', 'php');
$v1->addFolder('layout', '/tmp/views/layout');
$v1->addFolder('shared', '/tmp/views/shared');
$content = $v1->render('index', ['animal' => 'brown fox']);
var_dump($content);

// cleanup
unlink("/tmp/views/layout/default.php");
unlink("/tmp/views/shared/header.php");
unlink("/tmp/views/index.php");
rmdir("/tmp/views/layout");
rmdir("/tmp/views/shared");
rmdir("/tmp/views");
?>
--EXPECT--
string(124) "<header>vapor-test-007</header><div class="layout-container"><span>The quick brown fox jumped over the lazy dog</span></div>"
