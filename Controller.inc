<?php

require_once('Core.inc');
require_once('Helper.inc');

/**
 * Abstract basic class for all controller classes.
 *
 * Define basic methods for DOM processing.
 * Process basic tags:
 * - set
 * - block
 * - use
 * - include
 * - plugin
 * - event
 *
 * @author Iurii Prudius hardwork.mouse@gmail.com
 * @version 0.1.1
 * @property-read mixed $set Static settings from model
 * @see Core
 * @see Helper
 */
abstract class Controller
        extends Core {
  static private $_set = null;
  static private $_blocks = null;
  /**
   * @var string Controller files base path.
   */
  public $basepath = null;

  /**
   * Build view on specified model.
   * 
   * @param mixed $model DOMNode or XML file name to load. Find model file from context if omitted.
   * @return DOMNode|false view from model or false on error
   */
  public function view( $model = null ) {
    if( !($model instanceof DOMNode)
            && ($path = $this->findModel($model))
            && ($dom = self::loadXMLfile($path)) ) {
      $model = $dom->documentElement;
      $path = self::makeURL($path);
      self::spy("used model '{$path}'");
    }
    if( $model instanceof DOMNode ) {
      return($this->process($model));
    }
    return(false);
  }
  /**
   * Process predefined tags.
   * 
   * - 'set' eliminate into array self::$_set
   * - 'block' eliminate into array self::$_block
   * - 'plugin', 'include', 'use' process tags
   *
   * @param DOMNode $node    
   * @return DOMNode $node (for chaining)
   * @see self::setCollect()
   * @see self::blockCollect()
   * @see self::pluginProcess()
   * @see self::includeProcess()
   * @see self::useProcess()
   */
  public function process( $node ) {
    if( $node ) {
      do {
        if( $sets = $this->collectAll($node, 'set') ) {
          foreach( $sets as $key => $el ) {
            $var = Helper::obj2arr(simplexml_import_dom($el));
            unset($var['@attributes']); // remove attributes
            $sets[$key] = $var;
          }
          $this->set = self::$_set = array_merge_recursive((array) self::$_set, $sets);
        }
        $this->collectAll($node, 'block');
        $this->processNodes($node, 'plugin');
      } while( $this->processNodes($node, 'include') || $this->processNodes($node, 'use') );
    }
    return($node);
  }
  /**
   * @param string $name
   * @return mixed
   */
  public function __get( $name ) {
    switch( $name ) {
      case 'set':
        return($this->set = self::$_set);
      default:
        return( parent::__get($name) );
    }
  }
  /**
   * Define name of DOMElement.
   * 
   * - from attribute 'name'
   * - from named parent if attribute 'name' not set
   * - eval name if setted $set['names']['eval'] (! experimental )
   * modify attribute 'name' if need.
   * 
   * @param DOMElement $node Processed node
   * @param DOMElement $dflt Default name
   * @return string Value of new attribute 'name'
   */
  final protected function useName( $node, $dflt = null ) {
    $vnam = $node->getAttribute('name');
    if( !$vnam ) {
      $elp = $node;
      while( !$dflt && ($elp = $elp->parentNode) && ($elp->nodeType != XML_DOCUMENT_NODE) ) {
        $dflt = $elp->getAttribute('name');
      }
      $vnam = $dflt;
    } elseif( isset($this->set['names']['eval']) && (strpos($vnam, '$') !== false) ) {
      extract($this->query);
      $vnam = eval("return(\"$vnam\");");
    } else {
      return($vnam);
    }
    $node->setAttribute('name', $vnam);
    return($vnam);
  }
  /**
   * Eliminate all tagged children to array.
   * 
   * Seek defined method named $tag.'Collect' as user function.
   * User function receive current DOMElement
   * and returns:
   * - array(key=>value) for named element of result array;
   * - value for unnamed element of result array;
   * - null to not include element to result array and remove it.
   * 
   * If user function not found use internal function:
   * - if is current DOMElement has attribute 'name' then returns array(name=>DOMElement);
   * - else returns DOMElement.
   * 
   * @param DOMElement $root Processed node
   * @param string $tag Tag name
   * @return array Named nodes
   * @see self::reflectSmart()
   * @see self::reflectAppend()
   * @see self::reflectBefore()
   */
  final protected function collectAll( $root, $tag ) {
    $arr = array();
    if( ($els = $root->getElementsByTagName($tag)) && $els->length ) {
      $func = null;
      if( method_exists($this, $tag.'Collect') ) {
        $func = array( $this, $tag.'Collect' );
      } else {
        $func = function($el) {
          return ($name = $el->getAttribute('name')) ? array( $name => $el ) : $el;
        };
      }
      while( $els->length ) {
        $el = $els->item(0);
        if( $res = call_user_func($func, $el) ) {
          if( is_array($res) ) {
            $arr = array_merge_recursive($arr, $res);
          } else {
            $arr[] = $res;
          }
        }
        $el->parentNode->removeChild($el);
      }
    }
    return($arr);
  }
  /**
   * Eliminate some tagged children to array.
   * 
   * Seek defined method named $tag.'Collect' as user function.
   * User function receive current DOMElement
   * and returns:
   * - array(key=>value) for named element of result array;
   * - null to not include element to result array and save it.
   *
   * If user function not found use internal function:
   * - if current DOMElement has attribute 'name' then returns array(name=>DOMElement)
   * - else returns null.
   * 
   * @param DOMElement $root processed node
   * @param string $tag tag name
   * @return @a array of named nodes
   * @see self::reflectSmart()
   * @see self::reflectAppend()
   * @see self::reflectBefore()
   */
  final protected function collectSome( $root, $tag ) {
    $arr = array();
    if( ($els = $root->getElementsByTagName($tag)) && $els->length ) {
      $func = null;
      if( method_exists($this, $tag.'Collect') ) {
        $func = array( $this, $tag.'Collect' );
      } else {
        $func = function($el) {
          return ($name = $el->getAttribute('name')) ? array( $name => $el ) : null;
        };
      }
      for( $i = $els->length; --$i >= 0; ) {
        $el = $els->item($i);
        if( ($res = call_user_func($func, $el)) && is_array($res) ) {
          $arr = array_merge_recursive($arr, $res);
          $el->parentNode->removeChild($el);
        }
      }
    }
    return(array_reverse($arr));
  }
  /**
   * Universal private reflect function.
   * 
   * Wrapped by public functions:
   * - reflectAppend()
   * - reflectBefore().
   *
   * Insert tagged elements from array
   * before|append as child $target.
   * Seek defined method named $tag.'Reflect' as user function.
   * User function receive: new DOMElement, element key, element value
   * and returns:
   * - true on OK;
   * - false for remove new DOMElement.
   *
   * If user function not found then use internal function:
   * - if element is a DOMElement - copy attributes and children
   *   to new DOMElement and returns true;
   * - else returns false.
   * 
   * @param DOMElement $target
   * @param array $arr elements
   * @param string $tag tag name for new DOMElement
   * @param bool $before `TRUE` insert before, `FALSE` (default) append as child
   * @return int count of reflected new DOMElements
   * @see self::collectAll()
   * @see self::collectSome()
   * @see self::reflectAppend()
   * @see self::reflectBefore()
   */
  private function reflectSmart( $target, $arr, $tag, $before = false ) {
    $count = 0;
    if( $arr ) {
      foreach( $arr as $key => $list ) {
        $el = $target->ownerDocument->createElement($tag);
        if( $before ) {
          $target->parentNode->insertBefore($el, $target);
        } else {
          $target->appendChild($el);
        }
        $func = null;
        if( method_exists($this, $tag.'Reflect') ) {
          $func = array( $this, $tag.'Reflect' );
        } else {
          $func = function($el, $key, $list) {
            if( $list instanceof DOMElement ) {
              self::copyAttr($list, $el);
              self::moveTo($list, $el);
              return(true);
            }
            return(false);
          };
        }
        if( !call_user_func($func, $el, $key, $list) ) {
          $el->parentNode->removeChild($el);
        } else {
          $count++;
        }
      }
    }
    return($count);
  }
  /**
   * Insert tags from array before $anchor as child DOMElement.
   *
   * @param DOMElement $anchor 
   * @param array $arr Elements
   * @param string $tag Tag name for new DOMElement
   * @return int Count of reflected new DOMElements
   * @see self::reflectSmart()
   * @see self::collectAll()
   * @see self::collectSome()
   */
  final protected function reflectBefore( $anchor, $arr, $tag ) {
    return $this->reflectSmart($anchor, $arr, $tag, true);
  }
  /**
   * Append tags from array as children.
   *
   * @param DOMElement $parent 
   * @param array $arr Elements
   * @param string $tag Tag name for new DOMElement
   * @return int Count of reflected new DOMElements
   * @see self::reflectSmart()
   * @see self::collectAll()
   * @see self::collectSome()
   */
  final protected function reflectAppend( $parent, $arr, $tag ) {
    return $this->reflectSmart($parent, $arr, $tag, false);
  }
  /**
   * User function for process 'block' element by $this->collectAll().
   *
   * If exists named in private array self::$_blocks
   * - if is set attribute 'replace' - replace existing
   * - if is set attribute 'first' - insert before first child
   * - else append to childrens.
   * 
   * Process attribute 'as' {@see $this->makeAs()} to refactoring content.
   * If 'as' not exists - create new DOMDocument from DOMElement
   * and register it in private array self::$_blocks
   * with key = attribute 'name'.
   *
   * @param DOMElement $el 'block' element
   * @return null For remove this element from DOM
   * @see self::collectAll()
   * @see self::collectSome()
   * @see self::useProcess()
   */
  final protected function blockCollect( $el ) {
    $vnam = $this->useName($el, $this->action);
    if( !self::$_blocks )
      self::$_blocks = array();

    if( !isset(self::$_blocks[$vnam]) || $el->hasAttribute('replace') ) {
      $el->removeAttribute('replace');
      $el->removeAttribute('first');
      self::$_blocks[$vnam] = $this->createDOM($el);
    } else {
      $doc = self::$_blocks[$vnam];
      try {
        if( $nel = $doc->insertBefore($doc->importNode($el, true), $el->hasAttribute('first')
                          ? $doc->documentElement->firstChild : null ) ) {
          $nel->removeAttribute('first');
          self::moveTo($nel, $this->makeAs($nel));
          $nel->parentNode->removeChild($nel);
        }
      } catch( DOMException $e ) {
        self::spy($e->getMessage());
      }
    }
    return(null);
  }
  /**
   * User function for process 'use' element by $this->processNodes().
   *
   * If exists named in private array self::$_blocks
   * - clear 'use' children
   * - merge attributes ('use' over 'block')
   * - if is set attribute 'as' - create new DOMElement and copy children
   * - else copy attributes and children to 'use'
   * - if is set attribute 'once' - remove named from private array self::$_blocks
   *
   * @param  $el DOMElement <use>
   * @return DOMElement|bool
   * @see self::processNodes()
   * @see self::blockCollect()
   */
  final protected function useProcess( $el ) {
    $vnam = $this->useName($el, $this->action);
    if( self::$_blocks && isset(self::$_blocks[$vnam]) ) {
      while( $el->hasChildNodes() ) {
        $el->removeChild($el->childNodes->item(0));
      }
      $node = $el->ownerDocument->importNode(self::$_blocks[$vnam]->documentElement, true);
      $node = $el->parentNode->insertBefore($node, $el);
      self::copyAttr($el, $node);
      $target = self::moveTo($node, $node->hasAttribute('as') ? self::makeAs($node)
                                : self::copyAttr($node, $el) );
      $node->parentNode->removeChild($node);
      if( $target->hasAttribute('once') ) {
        $target->removeAttribute('once');
        unset(self::$_blocks[$vnam]);
      }
      return( $target->isSameNode($el) ? true : $target );
    }
    return(true);
  }
  /**
   * User function for process 'include' element by $this->processNodes().
   *
   * Include file content as child node.
   * Root tag may be
   * - 'body' for include more then one elements in top 'body'
   * - 'head' for include more then one elements in top 'head'
   * - 'html' for include both 'head' and 'body' in top 'html'
   * - any other one tag
   *
   * @param DOMElement $el
   * @return bool true for included content using | false on error
   * @see self::processNodes()
   */
  final protected function includeProcess( $el ) {
    $fnam = $this->useName($el, $this->page.'.'.$this->action.'.xml');
    if( ($fnam = $this->makePath($el->getAttribute('baseURI'), $el->getAttribute('path'), $fnam)) &&
            ($xml = $this->loadXMLfile($fnam)) ) {
      self::includeXML($xml, $el);
      return(true);
    }
    return(false);
  }
  /**
   * User function for process 'plugin' element by $this->processNodes().
   *
   * Activate named controller and process content of element.
   *
   * @param DOMElement $el
   * @return bool true for processed content using | false on controller not ready
   * @see self::processNodes()
   */
  final protected function pluginProcess( $el ) {
    $vnam = $this->useName($el, $this->page);
    $attrs = Helper::attr2arr($el);
    $xpath = new DOMXPath($el->ownerDocument);
    $skip = $xpath->query('skip', $el);
    if( $controller = $this->uses($vnam, $attrs) ) {
      if( $skip->length ) {
        foreach( $skip as $delme ) {
          $delme->parentNode->removeChild($delme);
        }
      }
      if( !($baseURI = $el->getAttribute('baseURI') ) ) {
        $baseURI = $this->basepath;
      }
      $controller->basepath = $this->makePath($baseURI, $el->getAttribute('path'));
      $controller->process($el);
      $controller->processNodes($el, 'event');
      return(true);
    } else {
      if( $skip->length ) {
        foreach( $skip as $useme ) {
          $el->parentNode->insertBefore($useme, $el);
          self::moveTo($useme, self::makeAs($useme));
          $useme->parentNode->removeChild($useme);
        }
      }
    }
    return(false);
  }
  /**
   * User function for process <event> element by $this->processNodes().
   *
   * Fire named event for active plugin.
   *
   * @param DOMElement $el
   * @return bool true for processed content using | false on event not exists
   * @see self::processNodes()
   */
  final protected function eventProcess( $el ) {
    $name = $this->useName($el, $this->action);
    $enam = 'event_'.$name;
    ob_start();
    if( method_exists($this, $enam) ) {
      if( $ret = $this->$enam($el) ) {
        if( is_string($ret) ) {
          self::spy($enam.': '.$ret);
        }
        if( self::includeXML(ob_get_clean(), $el) ) {
          $this->process($el);
        }
        return(true);
      }
    } elseif( $path = $this->findModel("{$name}.xml") ) {
      $this->spy('evented: '.$this->makeURL($path));
      if( self::includeXML(file_get_contents($path), $el) ) {
        $this->process($el);
      }
      $this->spy(ob_get_clean());
      return(true);
    }
    ob_end_clean();
    return(false);
  }
  /**
   * Process specified tags of root.
   * 
   * Need defined user function {$tag}.'Process'. If not defined remove tag.
   * If submethod returns:
   * - false - remove tag
   * - true - remake tag with 'as' attribute and move children into new
   * - DOMNode - append children into new node
   *
   * @param DOMNode $root    
   * @param string $tag		
   * @param mixed $param
   * @return int Count of processed tags
   * @see self::addComment()
   * @see self::makeAs()
   * @see self::moveTo()
   */
  final protected function processNodes( $root, $tag, $param = null ) {
    $count = 0;
    if( ($els = $root->getElementsByTagName($tag)) && $els->length ) {
      $addcmt = isset($this->set['comment'][$tag]);
      $method = $tag.'Process';
    }
    while( $els->length > 0 ) {
      $el = $els->item(0);
      if( $target = $this->{$method}($el, $param) ) {
        if( !is_object($target) ) {
          $target = self::makeAs($el);
        }
        if( $addcmt ) {
          self::addComment($tag.' "'.$el->getAttribute('name').'" {', $target);
        }
        self::moveTo($el, $target);
        if( $addcmt ) {
          self::addComment('}'.$tag, $el);
        }
        $count++;
      } else {
        if( $addcmt ) {
          self::addComment($tag.' "'.$el->getAttribute('name').'"', $el);
        }
      }
      $el->parentNode->removeChild($el);
    }
    return($count);
  }
  /**
   * Event 'title'
   */
  final function event_title( $node ) {
    echo "<language name=\"{$this->context}\" />";
    return(true);
  }
  /**
   * Event "head".
   * 
   * Link self .css and .js files for html.
   *
   * @param DOMElement $el
   * @return string Error text
   * @see Controller::findSelf()
   */
  final function event_head( $el ) {
    echo '<html>';
    if( $ret[] = $css = $this->findSelf('css', 'html') ) {
      echo '<link rel="stylesheet" type="text/css" href="'.$css.'" />';
    }
    if( $ret[] = $js = $this->findSelf('js', 'html') ) {
      echo '<script language="javascript" type="text/javascript" src="'.$js.'" />';
    }
    echo '</html>';
    return implode('+', array_filter($ret));
  }
  /**
   * Event 'view'
   * 
   * Include specified model element by requested type and replace 'id' attribute if exists.
   * 
   * @param DOMNode $node Event node
   */
  final function event_view( $node ) {
    if( $node->hasAttribute('id') ) {
      $node->setAttribute('id', $this->context.'_view');
    }
    if( $view = $this->view($node->getAttribute('model')) ) {
      self::includeXML($view, $node, $this->type);
      return(true);
    }
    return(false);
  }
}

?>