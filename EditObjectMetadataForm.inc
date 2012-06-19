<?php

/*
 * @file
 *
 */
module_load_include('inc', 'fedora_repository', 'api/fedora_item');
module_load_include('inc', 'fedora_repository', 'ContentModel');
module_load_include('inc', 'xml_form_api', 'XMLForm');

/**
 * FIXME:  This is just a collection of functions; there is no object state.
 *   All functions should be made static and so accessed.
 */
class EditObjectMetadataForm {

  /**
   * Get Content Models for the form.
   * 
   * @param string $collection_pid
   * 
   * @return array
   */
  public function getPotentialContentModels($collection_pid) {
    if (($collection_policy = CollectionPolicy::loadFromCollection($collection_pid)) === FALSE) {
      drupal_set_message(t('Unable to load collection policy \'' . $collection_pid . '\'.'));
      return FALSE;
    }
    if (!($content_models = $collection_policy->getContentModels())) {
      drupal_set_message(t('No content models associated with this collection: !collection_label. Please contact your administrator.', array('!collection_label' => $collection_label)), 'error');
      return FALSE;
    }
    $potential_models = array();
    foreach ($content_models as $content_model) {
      $identifier = $content_model->getIdentifier();
      $name = $content_model->name;
      $potential_models["$identifier"] = "$name";
    }
    return $potential_models;
  }

  /**
   * Create page one.
   * 
   * @param string $pid
   * @param string $dsid
   * @param SoapClient $client
   * @param array $form_state
   * 
   * @return array
   */
  public function createPageOne($pid, $client, array &$form_state) {
    $content_model = ContentModel::loadFromObject($pid);
    $form_names = islandora_content_model_get_form_names($content_model->pid);
    if (empty($form_names)) {
      throw new Exception('Couldn\'t Create Page One of the Edit Form.');
    }
    return array(
      'indicator' => array(
        '#type' => 'fieldset',
        '#title' => t('Choose edit form.'),
        'forms' => array(
          '#type' => 'select',
          '#title' => t('Forms'),
          '#options' => $form_names,
          '#description' => t('Select the form to edit.')
        )
      ),
      'submit' => array(
        '#type' => 'submit',
        '#submit' => array('fedora_repository_edit_qdc_form_submit'),
        '#value' => 'Next'
      )
    );
  }

  /**
   * Create page two.
   * 
   * @param string $pid
   * @param string $dsid
   * @param SoapClient $client
   * @param array $form_state
   * 
   * @return array
   */
  public function createPageTwo($pid, $client, array &$form_state) {
    module_load_include('inc', 'fedora_repository', 'api/fedora_item');
    $item = new Fedora_Item($pid);
    $content_model = ContentModel::loadFromObject($pid);
    $content_model_pid = $content_model->pid;
    $form_name = $form_state['values']['forms'];

    $dsid = islandora_content_model_get_dsid($content_model->pid, $form_name);
    $xml = NULL;
    if (array_key_exists($dsid, $item->datastreams)) {
      $xml = $this->getMetadata($pid, $dsid, $client);
    }

    $form = xml_form_builder_get_form($form_state, $form_name, $xml);
    $form['forms'] = array(
      '#type' => 'hidden',
      '#value' => $form_state['values']['forms'],
    );
    $form['content_model_pid'] = array(
      '#type' => 'hidden',
      '#value' => $content_model_pid,
    );
    $form['pid'] = array(
      '#type' => 'hidden',
      '#value' => $pid,
    );
    $form['dsid'] = array(
      '#type' => 'hidden',
      '#value' => isset($form_state['values']['dsid']) ? $form_state['values']['dsid'] : $dsid,
    );
    return $form;
  }

  private function getMetadata($pid, $dsid, $client) {
    $params = array('pid' => $pid, 'dsID' => $dsid, 'asOfDateTime' => "");
    try {
      $object = $client->__soapCAll('getDatastreamDissemination', array('parameters' => $params));
    } catch (Exception $e) {
      return array();
    }
    return trim($object->dissemination->stream);
  }

  /**
   * Create forms.
   * 
   * @param string $pid
   * @param string $dsid
   * @param SoapClient $client
   * @param array $form_state
   * 
   * @return array
   */
  public function create($pid, $client, array &$form_state) {
    $page = $form_state['storage']['step'] = empty($form_state['storage']['step']) ? 1 : $form_state['storage']['step'];
    if ($page == 1) {
      $form_state['storage']['xml'] = TRUE;
      return $this->createPageOne($pid, $client, $form_state);
    }
    else if ($form_state['storage']['xml']) {
      return $this->createPageTwo($pid, $client, $form_state);
    }
    else {
      throw new Exception('Couldn\'t Create the Edit Form.');
    }
  }

  /**
   * Submit.
   * 
   * @param array $form
   * @param array $form_state 
   */
  public function submit(array &$form, array &$form_state) {
    global $base_url;
    $pid = $form_state['values']['pid'];
    $dsid = $form_state['values']['dsid'];
    $form_name = $form_state['values']['forms'];
    $content_model_pid = $form_state['values']['content_model_pid'];
    $label_field = islandora_content_model_get_title_field($content_model_pid, $form_name);
    eval('$label = $form_state[\'values\']' . $label_field . ';');
    $this->setObjectLabel($pid, $label);
    $document = $this->modifyMetadata($pid, $dsid, $form, $form_state);
    $transform = islandora_content_model_get_transform($content_model_pid, $form_name);
    $this->transform($document, $transform, $pid, $label);
    $form_state['storage'] = NULL;
    $form_state['redirect'] = $base_url . '/fedora/repository/' . $pid;
  }

  /**
   *
   * @param string $pid
   * @param string $dsid
   * @param array $form
   * @param array $form_state 
   */
  private function modifyMetadata($pid, $dsid, array &$form, array &$form_state) {
    $xml_form = new XMLForm($form_state);
    $doc = $xml_form->submit($form, $form_state);
    $document = $doc->document;
    $this->modifyDatastream($document, $pid, $dsid, "$dsid Record", 'text/xml');
    return $document;
  }

  /**
   *
   * @param string $label 
   */
  private function setObjectLabel($pid, $label) {
    // Because of FCREPO-1059 we need to make sure that any label we create doesn't
    // contain any newlines. This is valid for at least version 3.5 of Fedora.
    $label = str_replace(array("\r", "\r\n", "\n"), '', $label);

    $object = new Fedora_Item($pid);
    $object->modify_object($label); // Set the label
  }

  /**
   *
   * @param DOMDocument $document
   * @param string $transform
   * @param string $pid
   * @param string $label 
   */
  private function transform(DOMDocument $document, $transform, $pid, $label) {
    $dublin_core = $this->transformDocumentIntoDublinCore($document, $transform);
    $xpath = new DOMXPath($dublin_core);
    // Set the Label
    $results = $xpath->query("*[local-name()='title']");
    $results->item(0)->nodeValue = htmlspecialchars(html_entity_decode($label, ENT_QUOTES, 'UTF-8'), ENT_QUOTES, 'UTF-8');
    // Set the Pid
    $results = $xpath->query("*[local-name()='identifier']");
    $results->item(0)->nodeValue = htmlspecialchars(html_entity_decode($pid, ENT_QUOTES, 'UTF-8'), ENT_QUOTES, 'UTF-8');
    $dublin_core->version = "1.0";
    $dublin_core->encoding = "UTF-8";
    $dublin_core->formatOutput = TRUE;
    $this->modifyDatastream($dublin_core, $pid, 'DC', "Dublin Core Record", 'text/xml');
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
  private function isDatastreamManaged($pid, $dsid) {
    $fedora_item = new Fedora_Item($pid);
    $datastream = $fedora_item->get_datastream($dsid);
    if($datastream) {
      return $datastream->controlGroup == 'M';
    }
    else {
      return FALSE;
    }
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
  private function modifyManagedDatastream(DOMDocument $dom, $pid, $dsid, $label, $mime_type) {
    $temp_file_dir = file_directory_path();
    $temp_file_path = file_create_filename("datastream.xml", $temp_file_dir);
    if ($dom->save($temp_file_path)) {
      $temp_file_url = file_create_url($temp_file_path);
      $fedora_item = new Fedora_Item($pid);
      if ($fedora_item->modify_datastream_by_reference($temp_file_url, $dsid, $label, $mime_type) !== NULL) {
        drupal_set_message(t('Successfully updated %dsid datastream for object %pid', array('%pid' => $pid, '%dsid' => $dsid)));
      }
    }
    file_delete($temp_file_path);
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
  private function modifyInlineDatastream(DOMDocument $dom, $pid, $dsid, $label, $mime_type) {
    $fedora_item = new Fedora_Item($pid);
    if (!array_key_exists($dsid, $fedora_item->datastreams)) {
      $xml = $dom->saveXML();
      $fedora_item->add_datastream_from_string($dom->saveXML(), $dsid, $label, $mime_type, 'X');
      return;
    }
    if ($fedora_item->modify_datastream_by_value($dom->saveXML(), $dsid, $label, $mime_type) !== NULL) {
      drupal_set_message(t('Successfully updated %dsid datastream for object %pid', array('%pid' => $pid, '%dsid' => $dsid)));
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
  private function modifyDatastream($dom, $pid, $dsid, $label, $mime_type) {

    if ($this->isDatastreamManaged($pid, $dsid)) {
      $this->modifyManagedDatastream($dom, $pid, $dsid, $label, $mime_type);
    }
    else {
      $this->modifyInlineDatastream($dom, $pid, $dsid, $label, $mime_type);
    }
  }

}
