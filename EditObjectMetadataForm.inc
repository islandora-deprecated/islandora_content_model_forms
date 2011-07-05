<?php

// $Id$

/*
 * @file
 *
 */
module_load_include('inc', 'fedora_repository', 'api/fedora_item');
module_load_include('inc', 'fedora_repository', 'ContentModel');

/**
 *
 */
class EditObjectMetadataForm {

  /**
   * The pid of the object whose datastream will be modify.
   *
   * @var string
   */
  protected $pid;
  /**
   * The datatstrean ID for the datastream we will modify.
   *
   * @var string
   */
  protected $dsid;
  /**
   * The name of the form to create.
   * 
   * @var string 
   */
  protected $formName;
  /**
   * The fedora object to edit.
   *
   * @var Fedora_Item
   */
  protected $fedoraObject;

  /**
   * Create the Edit Form Builder.
   *
   * @param array $form
   * @param array $form_state
   */
  public function __construct(array &$form_state) {
    $this->formState = &$form_state;
    
    $this->pid = $this->storage['pid'];
    $this->dsid = $this->storage['dsid'];
    $this->formName = $this->storage['form_name'];
    
  }

  /**
   * Submit the form. Modify the datastream identified by $this->dsid.
   *
   * @param array $form
   *   Drupal form to be validated.
   */
  public function submitForm(&$form) {
    global $base_url;
    
    

    $object = new Fedora_Item($this->pid);
    $object->modify_object($form['#post']['form_builder'][0]); // set label
    $content_model = ContentModel::loadFromObject($this->pid);
    
    // Get the document...
    list($class) = $this->loadDocumentClass($content_model);
    $document = new $class($form, $this->formState);
    
    //
    
    $this->modifyDatastream($document, "{$this->dsid}", "{$this->dsid} Record", 'text/xml');
    $transform = drupal_get_path('module', 'islandora_form_builder') . '/transforms/mods_to_dc.xsl'; // TODO add transform to content modeler ...
    $dublin_core = $this->transformDocumentIntoDublinCore($document, $transform);
    $dublin_core->version = "1.0";
    $dublin_core->encoding = "UTF-8";
    $dublin_core->formatOutput = TRUE;
    $this->modifyDatastream($dublin_core, 'DC', "Dublin Core Record", 'text/xml');
    $this->formState['storage'] = NULL;
    $this->formState['redirect'] = $base_url . '/fedora/repository/' . $this->pid;
  }

  /**
   * Transforms $document in to a dublin core document via xsl transform.
   *
   * @param DOMDocument $document
   * @param string $transform
   *
   * @return DOMDocument
   */
  private function transformDocumentIntoDublinCore(DOMDocument $document, $transform) {
    $xsl = new DOMDocument();
    $xsl->load($transform);
    $xslt = new XSLTProcessor();
    $xslt->importStyleSheet($xsl);
    $doc = new DOMDocument();
    $doc->loadXML($document->saveXML());
    return $xslt->transformToDoc($doc->documentElement);
  }

  /**
   * Test if a data stream is managed or not.
   *
   * @param $dsid
   *   Data stream identifier.
   *
   * @return
   *  TRUE if the data stream specified by $disd is managed, FALSE otherwise.
   */
  private function isDatastreamManaged($dsid) {
    $datastream = $this->fedoraItem->get_datastream($dsid);
    return $datastream->controlGroup == 'M';
  }

  /**
   * Overwrites a managed data stream by reference.
   *
   * Only works for managed data streams. Creates a temp file to be used as the
   * reference
   *
   * @param $dom
   *   XML document that will replace the data stream.
   * @param $dsid
   *   Data stream identifier.
   * @param $label
   *   Data stream label.
   * @param $mime_type
   *   Data stream mime_type.
   */
  private function modifyManagedDatastream(DOMDocument $dom, $dsid, $label, $mime_type) {
    $temp_file_dir = file_directory_temp();
    $temp_file_path = file_create_filename("modsdatastream.xml", $temp_file_dir);
    if ($dom->save($temp_file_path)) {
      $temp_file_url = file_create_url($temp_file_path);
      if ($this->fedoraItem->modify_datastream_by_reference($temp_file_url, $dsid, $label, $mime_type) !== NULL) {
        drupal_set_message(t('Successfully updated %dsid datastream for object %pid', array('%pid' => $this->pid, '%dsid' => $dsid)));
      }
    }
  }

  /**
   * Overwrites a inlined data stream by value.
   *
   * Only works for inlined data streams.
   *
   * @param $dom
   *   XML document that will replace the data stream.
   * @param $dsid
   *   Data stream identifier.
   * @param $label
   *   Data stream label.
   * @param $mime_type
   *   Data stream mime_type.
   */
  private function modifyInlineDatastream($dom, $dsid, $label, $mime_type) {
    if ($this->fedoraItem->modify_datastream_by_value($dom->saveXML(), $dsid, $label, $mime_type) !== NULL) {
      drupal_set_message(t('Successfully updated %dsid datastream for object %pid', array('%pid' => $this->pid, '%dsid' => $dsid)));
    }
  }

  /**
   * Overwrites a data stream.
   *
   * Overwrites the given datastream by value for inline datastreams and by
   * reference for managed datastreams.
   *
   * @param $dom
   *   XML document that will replace the data stream.
   * @param $dsid
   *   Data stream identifier.
   * @param $label
   *   Data stream label.
   * @param $mime_type
   *   Data stream mime_type.
   */
  private function modifyDatastream($dom, $dsid, $label, $mime_type) {
    if ($this->isDatastreamManaged($dsid)) {
      $this->modifyManagedDatastream($dom, $dsid, $label, $mime_type);
    }
    else {
      $this->modifyInlineDatastream($dom, $dsid, $label, $mime_type);
    }
  }

}
