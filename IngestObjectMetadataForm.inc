<?php

// $Id$

/**
 * @file
 *
 */
module_load_include('inc', 'fedora_repository', 'api/fedora_item');
module_load_include('inc', 'fedora_repository', 'ContentModel');
module_load_include('inc', 'fedora_repository', 'CollectionPolicy');
module_load_include('inc', 'xml_form_api', 'XMLForm');

/**
 * Create an ingest form.
 */
class IngestObjectMetadataForm {

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
      drupal_set_message(t('No content models associated with this collection: !collection_pid. Please contact your administrator.', array('!collection_pid' => $collection_pid)), 'error');
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
   * Create the first page of the Ingest Object Metadata form.
   *
   * @param string $collection_pid
   * @param string $collection_label
   * @param array $form_state
   *
   * @return array
   */
  public function createPageOne($collection_pid, $collection_label, array &$form_state) {
    $potential_models = $this->getPotentialContentModels($collection_pid);
    list($identifier, $name) = array_peek($potential_models);
    $selected_model = isset($form_state['values']['models']) ? $form_state['values']['models'] : $identifier;
    $content_model_pid = ContentModel::getPidFromIdentifier($identifier);
    $form_names = islandora_content_model_get_form_names($content_model_pid);
    if (empty($form_names)) {
      throw new Exception('Couldn\'t Create Page One of the Ingest Form.');
    }
    return array(
      'indicator' => array(
        '#type' => 'fieldset',
        '#title' => t('Ingest digital object into collection_pid !collection_label Step #1', array('collection_pid' => $collection_pid, '!collection_label' => $collection_label)),
        'models' => array(
          '#type' => 'select',
          '#title' => t('Content models available'),
          '#options' => $potential_models,
          '#default_value' => $selected_model,
          '#description' => t('Content models define datastream composition, relationships between this and other content models, and the mandatory behaviors associated with each digital object.<br /> Additional information may be found <a href="https://wiki.duraspace.org/display/FEDORACREATE/Content+Models+Overview">here.</a> '),
        ),
        'forms' => array(
          '#type' => 'select',
          '#title' => t('Select form'),
          '#options' => $form_names,
          '#description' => t('Select the form to populate the metadata of the new object.')
        ),
      ),
      'collection_pid' => array(
        '#type' => 'hidden',
        '#value' => $collection_pid
      ),
      'submit' => array(
        '#type' => 'submit',
        '#submit' => array('fedora_repository_ingest_form_submit'),
        '#value' => t('Next')
      ),
    );
  }

  /**
   * Create the second page of the Ingest Object Metadata form.
   *
   * @param string $collection_pid
   * @param string $collection_label
   * @param array $form_state
   *
   * @return array
   */
  public function createPageTwo($collection_pid, $collection_label, array &$form_state) {
    $path = drupal_get_path('module', 'islandora_content_model_forms');
    drupal_add_js("$path/js/fix-jquery-update.js", 'theme'); // Hacktacular, fixes issues between jquery update file uploads and AHAH.
    $content_model_pid = ContentModel::getPidFromIdentifier($form_state['values']['models']);
    $content_model_dsid = ContentModel::getDSIDFromIdentifier($form_state['values']['models']);
    $dsid = islandora_content_model_get_dsid($content_model_pid, $form_state['values']['forms']);
    $form = xml_form_builder_get_form($form_state, $form_name);
    $form['collection_pid'] = array(
      '#type' => 'hidden',
      '#value' => $collection_pid
    );
    $form['content_model_pid'] = array(
      '#type' => 'hidden',
      '#value' => $content_model_pid
    );
    $form['content_model_dsid'] = array(
      '#type' => 'hidden',
      '#value' => $content_model_dsid
    );
    $form['dsid'] = array(
      '#type' => 'hidden',
      '#value' => $dsid
    );
    $form['models'] = array(
      '#type' => 'hidden',
      '#value' => $form_state['values']['models']
    );
    $form['forms'] = array(
      '#type' => 'hidden',
      '#value' => $form_state['values']['forms']
    );

    return $form;
  }

  /**
   * Create the Ingest Object Metadata form.
   *
   * @param string $collection_pid
   * @param string $collection_label
   * @param array $form_state
   *
   * @return array
   */
  public function create($collection_pid, $collection_label, array &$form_state) {
    $page = $form_state['storage']['step'] = empty($form_state['storage']['step']) ? 1 : $form_state['storage']['step'];
    if ($page == 1) {
      $form_state['storage']['xml'] = true;
      return $this->createPageOne($collection_pid, $collection_label, $form_state);
    }
    else if ($form_state['storage']['xml']) {
      return $this->createPageTwo($collection_pid, $collection_label, $form_state);
    }
    else {
      throw new Exception("Couldn't Create the Ingest Form.");
    }
  }

  /**
   * Ingest the object.
   *
   * @global string $base_url
   *
   * @param array $form
   * @param array $form_state
   */
  public function submit(array &$form, array &$form_state) {
    global $base_url;
    if (isset($_FILES['files']['name']['file_upload']) && $_FILES['files']['name']['file_upload'] == 'text/xml') {
      $tmpDir = file_directory_path();
      if (!$file = file_save_upload('file_upload', NULL, $tmpDir)) {
        drupal_set_message('No file loaded', 'warning');
        return;
      }
      $file = "$tmpDir/$file->filename";
      $mods_xml = file_get_contents($file);
      $document = DOMDocument::loadXml($mods_xml);
      file_delete($file);
      if (!$document) {
        drupal_set_message('This does not appear to be a valid XML file', 'warning');
        return;
      }
    }
    else {
      $xml_form = new XMLForm($form_state);
      $doc = $xml_form->submit($form, $form_state);
      $document = $doc->document;
    }

    $collection_pid = $form_state['values']['collection_pid'];
    $content_model_pid = $form_state['values']['content_model_pid'];
    $content_model_dsid = $form_state['values']['content_model_dsid'];
    $dsid = $form_state['values']['dsid'];
    $form_name = $form_state['values']['forms'];
    $should_redirect = TRUE;
    $collection_policy = CollectionPolicy::loadFromCollection($collection_pid);
    if ($collection_policy !== FALSE) {
      $relationship = $collection_policy->getRelationship();
      module_load_include('inc', 'islandora_content_model_forms', 'FOXML');
      $label_field = islandora_content_model_get_title_field($content_model_pid, $form_name);
      eval('$label = $form_state[\'values\']' . $label_field . ';');
      if (empty($form['#post']['active-toggle'])) {
        $state = 'A';
      }
      else {
        $state = $form['#post']['active-toggle'] ? 'A' : 'I';
      }
      $pid = $collection_policy->getNextPid($content_model_dsid);
      $ingest_file_location = isset($form_state['values']['ingest-file-location']) ? $form_state['values']['ingest-file-location'] : NULL;
      $ingest_file_location = is_array($ingest_file_location) ? $ingest_file_location : array($ingest_file_location);
      $transform = islandora_content_model_get_transform($content_model_pid, $form_name);
      $transform = drupal_get_path('module', 'islandora_content_model_forms') . "/transforms/$transform";
      $foxml = new FOXML($label, $pid, $dsid, $content_model_pid, $collection_pid, $relationship, $ingest_file_location, $document, $transform, $state);
      $foxml->ingest();
      $_SESSION['fedora_ingest_files'] = '';
      $form_state['storage'] = NULL;
    }
    $form_state['redirect'] = $base_url . "/fedora/repository/{$collection_pid}";
  }

}

