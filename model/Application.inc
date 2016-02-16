<?php

namespace model;
require_once('Helper.inc');
/**
 * View model content based on requested type.
 *
 * @author Iurii Prudius <hardwork.mouse@gmail.com>
 */
class Application
        extends \Controller {
  public function __invoke( $args = null ) {
    return($this);  // must be registered
  }
  /**
   * Wrapper for requested types.
   * Send headers and buffered output to client.
   * 
   * @param DOMNode $model 
   * @return bool true on binary sent or false on text sent
   */
  public function view( $model = null ) {
    $viewname = __NAMESPACE__.'\\'.$this->type;
    if( class_exists($viewname, false) ) {
      $viewer = new $viewname;
    } else {
      $viewer = new data;
    }
    return $viewer->view($model);
  }
}

/**
 * Process data requests
 *
 * @table `data` (
 * `id` int(11) NOT NULL AUTO_INCREMENT,
 * `page` varchar(64) NOT NULL,
 * `path` varchar(255) NOT NULL,
 * `action` varchar(32) NOT NULL,
 * `type` varchar(32) NOT NULL,
 * `mimetype` varchar(255) NOT NULL DEFAULT 'text/plain',
 * `filename` varchar(255) NOT NULL,
 * `data` blob NOT NULL,
 * PRIMARY KEY (`id`),
 * UNIQUE KEY `page` (`page`,`path`,`action`,`type`) ) DEFAULT CHARSET=utf8;
 */
class data
        extends \Controller {
  function __invoke( $args = null ) {
    return(null);   // off
  }
  /**
   *  Send processed data
   *
   *  @param DOMNode $view
   *  @return bool FALSE (w/o profiler) | TRUE on error (with profiler)
   */
  public function view( $model = null ) {
    if( $this->security->checkRights()
            && ($table = $this->config['table'])
            && ($rec = $this->database->getRow('SELECT * FROM ?n WHERE `page`=?s AND `path`=?s AND `action`=?s AND `type`=?s', $table, $this->page, $this->path, $this->action, $this->type)) ) {
      $this->sendHeader('Content-Type: '.$rec['mimetype']);
      if( $rec['filename'] && ($rfile = realpath($rec['filename'])) ) {
        readfile($rfile);
      } else {
        $this->sendHeader('Length: '.strlen($rec['data']));
        echo $rec['data'];
      }
      return(true);
    } else {
      $this->sendHeader('Not Found', 404);
      echo "Data '{$this->page}".($this->path ? '/'.$this->path : '')."/{$this->action}.{$this->type}' not found";
    }
    return(false);
  }
}

/**
 * Process '.json' requests.
 */
class json
        extends \Controller {
  public function __invoke( $args = null ) {
    return(null); // not register
  }
  /**
   * Send processed 'json'
   *
   * @param DOMNode $model
   * @return bool true
   */
  public function view( $model = null ) {
    $xml = $this->merge($model, 'json');

    if( $xml && ($sxml = \simplexml_import_dom($xml)) ) {
      var_dump($arr = \Helper::obj2arr($sxml,'@attributes'));
      $json = \json_encode(\Helper::obj2arr($sxml,'@attributes'));
      $this->sendHeader('Content-Type: application/json');
      echo $json;
      return(true);
    }
    return(false);
  }
}

/**
 * Process '.xml' requests
 */
class xml
        extends \Controller {
  function __invoke( $args = null ) {
    return(null);   // off
  }
  /**
   * Send processed 'xml'
   *
   * @param DOMNode $model
   * @return bool FALSE (w/o profiler) | TRUE (with profiler)
   */
  public function view( $model = null ) {
    $xml = $this->merge($model, 'xml');

    $this->sendHeader('Content-Type: text/xml');
    echo $xml->ownerDocument->saveXML($xml);
    return(false); // false for comments
  }
}

/**
 * Process '.html' requests
 */
class html
        extends \Controller {
  public function __invoke( $args = null ) {
    return(null);   // off
  }
  /**
   * Prepare processed 'html' and send it.
   *
   * @param DOMNode $model
   * @return bool TRUE
   */
  public function view( $model = null ) {
    self::spy('view');

    $html = self::merge($model, 'html');
    $head = self::merge($html, 'head', $html->firstChild);
    $body = self::merge($html, 'body');
    $title = self::merge($head, 'title', $head->firstChild);
//    $title = self::merge($html, 'title', $head->firstChild);

    $this->reflectBefore($title, $this->collectAll($html, 'meta'), 'meta');
    $this->reflectBefore($title, $this->collectAll($html, 'link'), 'link');
    $this->reflectBefore($title, $this->collectSome($html, 'script'), 'script');

    $this->sendHeader('Content-Type: text/html');

    if( !empty($this->config['tidy']) ) {
      $xml = $html->ownerDocument->saveXML($html, LIBXML_NOEMPTYTAG);
      $regex = array(
          '~'.preg_quote('<![CDATA[', '~').'~' => '',
          '~'.preg_quote(']]>', '~').'~' => '',
          '~></(?:area|base(?:font)?|br|col|command|embed|frame|hr|img|input|keygen|link|meta|param|source|track|wbr)>~' => ' />',
      );
      echo '<!DOCTYPE html>'."\n".preg_replace(array_keys($regex), $regex, $xml);
    } else {
      echo $html->ownerDocument->saveHTML($html);
    }
    return(false);
  }
  /**
   * User function for collect 'meta' over collectAll.
   *
   * @param DOMElement $el
   * @return array
   */
  protected function metaCollect( $el ) {
    $arr = array();
    if( $content = $el->getAttribute('content') ) {
      $el->removeAttribute('content');
      foreach( $el->attributes as $attr ) {
        $arr[$attr->name][$attr->value][] = $content;
      }
    }
    return($arr);
  }
  /**
   * User function for reflect collected 'meta'
   *
   * @param DOMElement $el
   * @param string $key
   * @param array $list
   * @return bool FALSE (for removing autocreated $el)
   */
  protected function metaReflect( $el, $key, $list ) {
    foreach( $list as $attr => $content ) {
      $elm = $el->ownerDocument->createElement('meta');
      $elm->setAttribute($key, $attr);
      $elm->setAttribute('content', implode('; ', $content));
      $el->parentNode->insertBefore($elm, $el);
    }
    return(false);
  }
  /**
   * User function for collect 'link' over collectAll
   *
   * @param' DOMElement $el
   * @return array
   */
  protected function linkCollect( $el ) {   // href, rel, type, media
    $arr = array();
    if( $href = $el->getAttribute('href') ) {
      $el->removeAttribute('href');
      foreach( $el->attributes as $attr ) {
        $arr[$href][$attr->name][$attr->value] = true;
      }
    }
    return($arr);
  }
  /**
   * User function for reflect collected 'link'
   *
   * @param DOMElement $el
   * @param string $key
   * @param array $list
   * @return bool TRUE (for saving autocreated $el)
   */
  protected function linkReflect( $el, $key, $list ) {   // href, rel, type, media
    $el->setAttribute('href', $key);
    foreach( $list as $attr => $content ) {
      $el->setAttribute($attr, implode(', ', array_keys($content)));
    }
    return(true);
  }
  /**
   * User function for collect 'script' over collectSome
   *
   * @param DOMElement $el
   * @return array|bool FALSE on inline script
   */
  protected function scriptCollect( $el ) {   // src, ...
    $arr = array();
    if( $src = $el->getAttribute('src') ) {
      $el->removeAttribute('src');
      $arr[$src] = \Helper::attr2arr($el);
      return($arr);
    }
    return(false);
  }
  /**
   * User function for reflect collected 'script'
   *
   * @param DOMElement $el
   * @param string $key
   * @param array $list
   * @return bool TRUE (for saving autocreated $el)
   */
  protected function scriptReflect( $el, $key, $list ) {   // src, ...
    $el->setAttribute('src', $key);
    foreach( $list as $attr => $value ) {
      $el->setAttribute($attr, $value);
    }
    return(true);
  }
}

?>
