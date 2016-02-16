<?php

require_once('security.inc');

/**
 * Interface plugin to class 'security'
 *
 * plugin name="user" path="user"
 */
class user
        extends \Controller {
    private $atts = null;

    /**
     * Check security
     * 
     * @param array $args
     * @return Controller
     */
    function __invoke( $args = null ) {
        $this->atts = $args;
        if( $this->security = self::uses('security') ) {
            return($this);
        }
        return(null);
    }
    /**
     * Event "logged_out"
     * 
     * Check if user logged out
     * @param DOMElement $node Event node
     * @return bool
     */
    function event_logged_out( $node ) {
        return(!$this->security->uid );
    }
    /**
     * Event "logged_in"
     * 
     * Check if user logged in
     * @param DOMElement $node Event node
     * @return bool
     */
    function event_logged_in( $node ) {
        return(!!$this->security->uid );
    }
    /**
     * Event "security"
     * 
     * Change 'name' and 'href' attributes of current $node by attribute 'element'
     * @param DOMElement $node Event node
     * @return bool
     */
    function event_security( $node ) {
        if( $element = $node->getAttribute('element') ) {
            $node->removeAttribute('element');
            $node->setAttribute('name', "security[{$element}]");
            if( $href = $node->getAttribute('href') ) {
                $node->setAttribute('href', $href."security[{$element}]");
            }
            return true;
        }
        return(false);
    }
    /**
     * Event "accessname"
     * 
     * Get access name
     * @param DOMElement $node Event node
     * @return bool
     */
    function event_accessname( $node ) {
        switch( $this->security->aid ) {
            case DB_USER_ACCESS_MASTER: echo '<language name="Master" />';
                break;
            case DB_USER_ACCESS_GUEST: echo '<language name="Guest" />';
                break;
            case DB_USER_ACCESS_USER: echo '<language name="Registered" />';
                break;
            case DB_USER_ACCESS_KNOWN: echo '<language name="Verified" />';
                break;
            default: echo '<language>'.$this->security->access.'</language>';
                break;
        }
        return(true);
    }
    /**
     * Event "username"
     * 
     * Get user name
     * @param DOMElement $node Event node
     * @return bool
     */
    function event_username( $node ) {
        if( $this->security->uid ) {
            $node->nodeValue = $this->security->username;
        } else {
            echo '<language name="Not logged" />';
        }
        return(true);
    }
    /**
     * Event "prifile"
     * 
     * Show user profile
     * @param DOMElement $node Event node
     * @return bool
     */
    function event_profile( $node ) {
        $fragment = $this->moveTo($node, $node->ownerDocument->createDocumentFragment());
        if( $this->security->uid ) {
            foreach( $this->config->getExpand($this->security->uid) as $rec ) {
                $node->appendChild($fragment->cloneNode(true));
                $this->processNodes($node, 'field', $rec);
            }
            return(true);
        }
        return(false);
    }
    /**
     * Process child nodes 'field' by records.
     * 
     * @param DOMElement $el
     * @param array $rec
     * @return bool
     */
    protected function fieldProcess( $el, $rec ) {
        if( ($name = $this->useName($el)) && array_key_exists($name, $rec) ) {
            $attr = \Helper::attr2arr($el);
            if( isset($attr['editable']) ) {
                $attr['name'] = "user[{$rec['key']}][{$name}]";
            } else {
                $attr['name'] = '';
            }
            $this->includeXML('<html>'.implode('', (array) \Helper::showTyped($rec[$name], $attr)).'</html>', $el);
            return(true);
        }
        return(false);
    }
    /**
     * Event "json"
     * 
     * Check if user logged out
     * @param DOMElement $node Event node
     * @return bool
     */
    function event_json( $node ) {
        return(!$this->security->uid );
    }
}

?>