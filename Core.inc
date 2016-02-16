<?php

require_once('Helper.inc');

/**
 * Abstract core class.
 * 
 * Provides access to:
 * - request parameters
 * - builtin profiler
 * - used plugins list (append, get clone of existed)
 * 
 * Magically access to any registered controller over static method or as property name.
 * Example:
 * Core::database()
 * Core::config()
 * $this->database
 * $this->config
 *
 * @property-read bool $cloned Is object original or clone
 * @property-read string $context Class name
 * @property-read array $query List of requested query elements
 * @property-read string $page Requested page element
 * @property-read string $action Requested action element
 * @property-read string $type Requested type element
 * @property-read string $path Requested path element
 * @property-read string $method Request method
 * @property-read string $theme Current theme
 * @property-read string $rootdir Realpath to root dir
 * @author Iurii Prudius <hardwork.mouse@gmail.com>
 * @version 0.1.2
 * 
 * @since 0.1.2 Added model caching 
 */
abstract class Core {
  /**
   * Current http return code.
   * @var int 
   */
  static public $exitcode = null;
  static private $started = null;
  static private $last = null;
  static private $log = null;
  static private $registered = null;
  static private $cache = null;
  static private $_rootdir = null;
  static private $_query = null;
  private $_cloned = null;

  /**
   * Checking for plugin accessibility.
   * 
   * @param string[]
   * @return Controller|null
   */
  abstract function __invoke( $args = null );

  /**
   * Clone object properties.
   * 
   * @param string[]
   * @return Controller|null
   */
  final function __clone() {
    foreach( $this as $name => $property ) {
      if( is_object($property) ) {
        $this->$name = clone $property;
      }
    }
    $this->_cloned = true;
  }

  /**
   * Read-only properties.
   * 
   * @param string $name
   * @link security
   * @link database
   * @link config
   */
  public function __get( $name ) {

    switch( $name = strtolower($name) ) {
      case 'context':
        return($this->context = get_class($this));
      case 'theme':
        return($this->theme = self::$_query['theme']);
      case 'page':
        return($this->page = self::$_query['page']);
      case 'action':
        return($this->action = self::$_query['action']);
      case 'type':
        return($this->type = self::$_query['type']);
      case 'path':
        return($this->path = self::$_query['path']);
      case 'method':
        return($this->method = self::$_query['method']);
      case 'query':
        return($this->query = self::$_query);
      case 'rootdir':
        return($this->rootdir = self::$_rootdir);
      case 'cloned':
        return($this->_cloned);
      default:
        if( \array_key_exists($name, (array) self::$registered) ) {
          return(self::uses($name, ['context' => get_class()]));
        }
    }
    return(null);
  }

  /**
   * Use already REGISTERED controller.
   * 
   * @param string $context object name
   * @param array $args object parameters
   * @return Controller|null Clone of registered object | NULL on unknown or disabled object
   */
  public function __callStatic( $name, $args ) {
    if( !\array_key_exists($name, (array) self::$registered) ) {
      return(null);
    }
    $args['context'] = get_called_class();
    return(self::uses($context, $args));
  }

  /**
   * Init internal static properties.
   * 
   * @param bool $onoff turn on/off the internal logging
   * @global array $query named query elements
   * @global string $page 'page' query element
   * @global string $action 'action' query element
   * @global string $type 'type' query element
   * @global string $path 'path' query element
   * @global string $method 'method' request method
   * @return void
   */
  final static function init( $onoff = null ) {
    if( $onoff ) {
      self::$log = array();
    } else {
      self::$log = null;
    }
    //  init properties
    if( !self::$started ) {
      self::$registered = array();
      //  init profiler
      self::$last = self::$started = microtime(true);
      self::$_rootdir = __DIR__;
      self::spy('Query: ' . str_replace('%', '\%', $_SERVER['QUERY_STRING']));
      //	store query address
      if( $GLOBALS['query'] = self::$_query = Helper::eliminate($_GET, 'query') ) {
        $GLOBALS['page'] = self::$_query['page'] = strtolower(Helper::getVar(self::$_query, 'page', 'main'));
        $GLOBALS['action'] = self::$_query['action'] = strtolower(Helper::getVar(self::$_query, 'action', 'main'));
        $GLOBALS['type'] = self::$_query['type'] = strtolower(Helper::getVar(self::$_query, 'type', 'html'));
        $GLOBALS['path'] = self::$_query['path'] = Helper::getVar(self::$_query, 'path', '');
        $GLOBALS['method'] = self::$_query['method'] = $_SERVER['REQUEST_METHOD'];
      }
      $GLOBALS['theme'] = self::$_query['theme'] = Helper::eliminate($_REQUEST, 'theme', 'default');
    }
    return(true);
  }

  /**
   * Use caching for models.
   * 
   * @param string $path Path to cache folder
   * @return void
   */
  final static function cache( $path = null ) {
    if( ($path != self::$cache) && (self::$cache) ) {
      \Helper::rmDir(self::$cache);
      self::spy('cache cleared');
    }
    if( $path && (self::$cache = \realpath($path)) ) {
      if( !\file_exists(self::$cache) ) {
        if( !\mkdir(self::$cache) ) {
          self::spy('cache not created: ' . $path);
          self::$cache = null;
          return;
        }
      }
      $c_time = filemtime($path);
      if( ($c_time < filemtime('./plugins')) || ($c_time < filemtime('./model')) ) {
        \Helper::rmDir(self::$cache, true);
        self::spy('cache freshed: ' . $path);
      } else {
        self::spy('cache used: ' . $path);
      }
    } else {
      self::spy('no caching: ' . $path);
      self::$cache = null;
    }
  }

  private static function cachePath( $model ) {
    if( !self::$cache ) {
      return(false);
    }
    if( !$model ) {
      $model = '_.xml';
    }
    $theme = self::$_query['theme'];
    $context = get_called_class();
    $page = self::$_query['page'];
    $action = self::$_query['action'];
    return self::$cache . "/_{$theme}.{$context}.{$page}.{$action}._{$model}";
  }

  /**
   * Get cached model.
   * 
   * @param string $model Model name
   * @return string|false Path to cached model or false
   */
  final static function getCached( $model = null ) {
    if( \file_exists($path = self::cachePath($model)) ) {
      return($path);
    }
    return(false);
  }

  /**
   * Add model to cache.
   * 
   * @param string $fname File for caching
   * @param string $model Model name
   * @return bool Is cached
   */
  final static function addCached( $fname, $model = null ) {
    $path = self::cachePath($model);
    if( $fname ) {
      return \copy($fname, $path);
    }
    return touch($path);
  }

  /**
   * Total microtime.
   * 
   * @return float Microtime from init
   */
  final static function total() {
    return((self::$last = microtime(true)) - self::$started);
  }

  /**
   * Local microtime.
   * 
   * @return float microtime from last call local or total
   */
  final static function local() {
    $last = self::$last;
    return((self::$last = microtime(true)) - $last);
  }

  /**
   * Append identified record to current log.
   * 
   * @param string $text
   */
  final static function spy( $text ) {
    if( is_array(self::$log) && $text ) {
      self::$log[] = sprintf('%s:: %s (%f) %f', get_called_class(), htmlentities($text), self::local(), self::total());
    }
  }

  /**
   * Get current log.
   * 
   * @return array Internal log
   */
  final static function observe() {
    return(self::$log);
  }

  /**
   * Get registered controller or create and register new.
   * 
   * @param string $context object name
   * @param array $args object parameters
   * @return Controller|null Original of new object | clone of registered object | NULL on disabled object
   */
  final static function uses( $context, $args = [] ) {
    ob_start();
    $object = null;
    if( !\array_key_exists($context, (array) self::$registered) ) { // first
      self::spy('create: ' . $context);
      if( class_exists($context, true) ) {
        $object = new $context($args);
        $object = self::$registered[$context] = $object($args);
      }
    } elseif( isset(self::$registered[$context]) ) { // clone
      self::spy('clone: ' . $context);
      $object = clone self::$registered[$context];
      $object = $object($args);
    }
    if( $object ) {
      self::spy(ob_get_clean());
    } else {
      self::spy('skipped: ' . $context . ': ' . ob_get_clean());
    }
    return($object);
  }

  /**
   * Conventional reset SESSION and COOKIEs for all registered controllers
   * via method ::forget().
   * 
   * @return bool TRUE for chaining
   */
  final static function reset() {
    foreach( self::$registered as $plugin ) {
      if( method_exists($plugin, 'forget') ) {
        $plugin->forget();
      }
    }
    return true;
  }

  /**
   * Send http header.
   * 
   * @param string $hdr formatted header or http header text (if $cod)
   * @param numeric $cod	http error code
   * @return bool TRUE on sucess | FALSE on headers already sent
   */
  final static function sendHeader( $hdr, $cod = 0 ) {
    if( !\headers_sent($filename, $linenum) ) {
      if( $cod && is_numeric($cod) ) {
        $cod = self::$exitcode = intval($cod);
        \header($_SERVER['SERVER_PROTOCOL'] . " {$cod} {$hdr}", true, $cod);
        if( ($cod >= 300) && ($cod < 400) ) {
          exit;
        }
      } else {
        \header($hdr, true);
      }
      return(true);
    } else {
      self::spy(sprintf('headers already sent by "%s" at line %s', $filename, $linenum));
    }
    return(false);
  }

  /**
   * Copy attributes from one DOMElement to other exclude listed.
   *    
   * @param DOMElement $node Source node
   * @param DOMElement $target Target node (if same - just remove attributes)
   * @param string $exclude Space delimited list of excluded attributes
   * @return DOMElement Target node (for chaining)
   */
  final static function copyAttr( $node, $target, $exclude = null ) {
    $exclude = explode(' ', $exclude);
    if( $node->isSameNode($target) ) {
      foreach( $exclude as $attr ) {
        $target->removeAttribute($attr);
      }
    } else {
      foreach( $node->attributes as $attr ) {
        if( !$exclude || !in_array($attr->name, $exclude) ) {
          $target->setAttribute($attr->name, $attr->value);
        }
      }
    }
    return($target);
  }

  /**
   * Remove all children from node.
   * 
   * @param DOMNode $node
   * @return DOMNode|false $node or false on error
   */
  final static function removeChildren( $node ) {
    if( $node && ($node instanceof DOMNode) ) {
      while( $node->hasChildNodes() ) {
        $node->removeChild($node->firstChild);
      }
      return($node);
    }
    return(false);
  }

  /**
   * Add comment node to DOMDocument before anchor node.
   * 
   * @param string $text comment text
   * @param DOMElement $before anchor node
   * @return @a DOMElement anchor node
   * @return @bool FALSE on error
   */
  final static function addComment( $text, $before ) {
    if( $cmt = $before->ownerDocument->createComment($text) ) {
      return( $before->parentNode->insertBefore($cmt, $before) );
    }
    return(false);
  }

  /**
   * Create new DOMElement
   * with tag name from DOMElement attribute 'as'
   * and copy all attributes exclude 'as' to it.
   * 
   * @param DOMElement $node Processed node
   * @return DOMElement new if has attribute 'as' or unchanged $node
   */
  final static function makeAs( $node ) {
    $target = $node;
    if( $as = $node->getAttribute('as') ) {
      $target = $node->ownerDocument->createElement($as);
      $node->parentNode->insertBefore($target, $node);
      self::copyAttr($node, $target, 'as');
    }
    return($target);
  }

  /**
   * Move all $source children.
   * 
   * - before $source if $target is null or same node $source
   * - into $target elsewhere.
   * 
   * @param DOMElement $source source node
   * @param DOMElement $target target node
   * @return DOMElement|null $target or null on same nodes
   */
  final static function moveTo( $source, $target = null ) {
    if( $target && !$source->isSameNode($target) ) {
      while( $source->childNodes->length > 0 ) {
        $target->appendChild($source->childNodes->item(0));
      }
    } else {
      while( $source->childNodes->length > 0 ) {
        $source->parentNode->insertBefore($source->childNodes->item(0), $source);
      }
      $target = null;
    }
    return($target);
  }

  /**
   * Unshell (shuck) nested this tag nodes.
   *
   * @param DOMNode $node 
   * @return int Count of processed nested nodes
   */
  final static function shuck( $node ) {
    $count = 0;
    $tag = $node->tagName;
    if( $elms = $node->getElementsByTagName($tag) ) {
      while( $elms->length > 0 ) {
        $el = $elms->item(0);
        self::shuck($el);
        self::moveTo($el);
        $el->parentNode->removeChild($el);
        $count++;
      }
    }
    return($count);
  }

  /**
   * Merge specified siblings to first tag.
   * 
   * For root element (move before specified element if need)
   * unshell (shuck) nested tags and
   * process attributes:
   * - 'first' place tag content in the top
   * - 'replace' replace previous content
   * - 'separator' (for text nodes only) concatenate values using this string
   *
   * @param DOMNode $dom    
   * @param string $tag    
   * @param DOMNode $createBefore    
   * @return DOMNode root tagged element | new root
   */
  final static function merge( $dom, $tag, $createBefore = null ) {
    if( XML_DOCUMENT_NODE == $dom->nodeType ) {
      $doc = $dom;
      $dom = $doc->documentElement;
    } else {
      $doc = $dom->ownerDocument;
    }
    if( $dom->nodeName == $tag ) {
      $root = $dom;
    } elseif( ($elms = $dom->getElementsByTagName($tag)) && ($elms->length > 0) ) {
      $root = $elms->item(0);
    } else {
      $root = $doc->createElement($tag);
      if( $createBefore ) {
        $createBefore->parentNode->insertBefore($root, $createBefore);
      } else {
        $dom->appendChild($root);
      }
    }
    self::copyAttr($root, $root, 'first replace');
    // shuck internal
    self::shuck($root);
    // append external
    $elms = $dom->getElementsByTagName($tag);
    while( $elms->length > 1 ) {
      $item = $elms->item(1);
      if( $item->hasAttribute('first') ) {
        $root->parentNode->insertBefore($item, $root);
        $root = $item;
        self::copyAttr($root, $root, 'first replace');
        self::shuck($root);
      } elseif( $item->hasAttribute('replace') ) {
        $root->parentNode->insertBefore($item, $root);
        $root->parentNode->removeChild($root);
        $root = $item;
        self::copyAttr($root, $root, 'first replace');
        self::shuck($root);
      } else {
        self::shuck($item);
        self::moveTo($item, $root);
        $item->parentNode->removeChild($item);
      }
    }
    return($root);
  }

  /**
   * Make real path to existing file/directory.
   * 
   * @param string $base Base path for relative path
   * @param string $path Path
   * @param string $filename File name
   * @return string|bool Real path | FALSE if file not exists
   */
  final static function makePath( $base, $path, $filename = '' ) {
    $base = rtrim($base, '/\\');
    $path = rtrim($path, '/\\');
    if( $path ) {
      if( strpos('\\/', $path[0]) !== false ) {
        $base = self::$_rootdir;
      } elseif( $base ) {
        $base .= '/';
      }
    }
    if( $filename ) {
      return realpath($base . $path . '/' . $filename);
    }
    return realpath($base . $path);
  }

  /**
   * Make correct external link for resource.
   * 
   * @param string $name Absolute real file name
   * @return string URL path from root
   */
  final static function makeURL( $name ) {
    return str_ireplace(str_ireplace('\\', '/', self::$_rootdir), '', str_ireplace('\\', '/', realpath($name)));
  }

  /**
   * Find model file for view.
   * 
   * @param mixed $model XML file name. Use context if omitted.
   * @return string Real path to existing file or empty string.
   */
  final static function findModel( $model = null ) {
    if( $path = self::getCached($model) ) {
      return($path);
    }

    $rootdir = self::$_rootdir;
    $context = get_called_class();
    $action = self::$_query['action'];
    $page = self::$_query['page'];
    $theme = self::$_query['theme'];

    if( empty($model) ) {

      if( !($path = self::makePath($rootdir, "/plugins/{$context}/model", "{$action}.xml")) && !($path = self::makePath($rootdir, "/plugins/{$context}/model", "{$page}.xml")) && !($path = self::makePath($rootdir, "/model/{$theme}/{$context}", "{$page}.xml")) && !($path = self::makePath($rootdir, "/model/{$theme}", "{$context}.xml")) && !($path = self::makePath($rootdir, '/model', "{$context}.xml"))
      ) {
        self::spy('not found default model');
      }
    } elseif( is_string($model) ) {
      if( !($path = self::makePath($rootdir, "/plugins/{$context}/model", $model)) && !($path = self::makePath($rootdir, "/model/{$theme}/{$page}", $model)) && !($path = self::makePath($rootdir, "/model/{$theme}", $model)) && !($path = self::makePath($rootdir, "/model", $model))
      ) {
        self::spy("not found model '{$model}' for '{$context}'");
      }
    }
    self::addCached($path, $model);
    return($path);
  }

  /**
   * Find file name of context specific resource.
   * 
   * By type on path:
   * - /$rootdir/$type/$theme/$context.$type
   * - /$rootdir/$type/$context.$type
   * 
   * @global string $theme Used theme (if set)
   * @param string $type File extension
   * @param string $name File name (use context if omitted)
   * @return string|null Relative path to existing file | null on not found
   */
  final static function findSelf( $type, $name = null ) {
    $rootdir = self::$_rootdir;
    $context = get_called_class();
    $theme = self::$_query['theme'];

    $first = is_dir($type) ? $type . '/' : 'image/';
    $last = ( $name ? $name : $context ) . '.' . $type;
    if( ($theme && ($path = self::makePath($rootdir, $first . $theme, $last))) ||
      ($path = self::makePath($rootdir, $first, $last)) ) {
      $path = self::makeURL($path);
      return( $path );
    }
    return(null);
  }

  /**
   * Get content of context specific error file by code.
   * 
   * @global string $theme Used theme (if set)
   * @param numeric $code Error code
   * @return string|null File content | null on not exists
   */
  final static function showError( $code ) {
    $theme = self::$_query['theme'];
    if( ($theme && ($path = self::makePath(self::$_rootdir, "/error/{$theme}", "{$code}.xml"))) || ($path = self::makePath(self::$_rootdir, '/error', "{$code}.xml")) ) {
      return file_get_contents($path);
    }
    return('');
  }

  /**
   * Create formatted DOMDocument.
   * 
   * @param mixed $root string root tag |DOMNode root element
   * @return DOMDocument
   */
  final static function createDOM( $root = null ) {
    $doc = new DOMDocument();
    $doc->preserveWhiteSpace = false;
    $doc->resolveExternals = false;
    $doc->formatOutput = true;

//		if( libxml_use_internal_errors(true) ) {
//			libxml_clear_errors();
//		}

    if( $root ) {
      if( $root instanceof DOMNode ) {
        $doc->appendChild($doc->importNode($root, true));
      } elseif( is_string($root) ) {
        $doc->loadXML("<{$root} />"
          , LIBXML_COMPACT | LIBXML_NOXMLDECL);
      }
    }
    return($doc);
  }

  /**
   * Create formatted DOMDocument and load from file.
   * 
   * @param string $fname file name
   * @return DOMDocument|NULL on error loading
   */
  final static function loadXMLfile( $fname ) {
    if( $rname = realpath($fname) ) {
      if( $xml = file_get_contents($rname) ) {
        return self::loadXML($xml);
      } else {
        self::spy('empty file ' . self::makeURL($fname));
      }
    } else {
      self::spy('not found file ' . self::makeURL($fname));
    }
    return(null);
  }

  /**
   * Create formatted DOMDocument and load from string.
   * 
   * @param string $string XML string
   * @return DOMDocument|NULL on error loading
   */
  final static function loadXML( $string ) {
    $doc = self::createDOM();
    if( $doc->loadXML($string
        , LIBXML_COMPACT | LIBXML_NONET | LIBXML_NOWARNING | LIBXML_NOERROR | LIBXML_NOXMLDECL | LIBXML_XINCLUDE) ) {
      return($doc);
    }
    self::spy('error load XML: ' . $string);
    return(null);
  }

  /**
   * Append DOMDocument or XML string as node into target.
   *
   * @param mixed $xml XML string or DOMNode 
   * @param DOMNode $target Node to append content
   * @param string $usetag Use this tags content only
   * @return bool true if contains 'set', 'block', 'plugin' | false otherwise
   */
  final static function includeXML( $xml, $target, $usetag = null ) {
    $ret = false;
    ob_start();

    if( $xml && ( ($xml instanceof DOMNode) || ($xml = self::loadXML($xml))) ) {
      if( $usetag ) {
        $root = self::merge($xml, $usetag);
      } elseif( XML_DOCUMENT_NODE == $xml->nodeType ) {
        $root = $xml->documentElement;
      } else {
        $root = $xml;
      }
      $doc = $root->ownerDocument;
      $addbase = array('//*[@path]', '//include');
      $countes = array('set', 'block', 'plugin');
      $xdom = new DOMXPath($doc);
      $base = $doc->documentURI ? $doc->documentURI : $target->ownerDocument->documentURI;
      $base = dirname(realpath(str_replace($base, 'file:///', '')));
      foreach( $addbase as $xpath ) {
        $incs = $xdom->query($xpath);
        foreach( $incs as $inc ) {
          $inc->setAttribute('baseURI', $base);
        }
      }
      $node = $target->ownerDocument->importNode($root, true);
      $target->appendChild($node);
      foreach( $countes as $tag ) {
        $ret = $ret || $node->getElementsByTagName($tag)->length;
      }
    }
    self::spy(ob_get_clean());
    return($ret);
  }

}

/**
 * Extended syntax for accesing to the methods of core elements.
 * 
 * Just use:
 * Core\Database::method();
 * Core\Security::method();
 * Core\Config::method();
 */
namespace Core;

/**
 * Trait for namespace Core\
 */
trait AsStatic {

  function __callStatic( $name, $arguments ) {
    $class = strtolower(substr(strrpos(__CLASS__, '\\'), 1));
    if( ($object = \Core::{$class}()) && method_exists($object, $name) ) {
      return $object->{$name}($arguments);
    }
    throw new ErrorException("Unknown method '$name' for " . __CLASS__);
  }

}

class Database {

  use AsStatic;
}

class Security {

  use AsStatic;
}

class Config {

  use AsStatic;
}
