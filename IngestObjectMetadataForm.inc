<?php

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
 * FIXME:  This is just a collection of functions; there is no object state.
 *   All functions should be made static and so accessed.
 */
class IngestObjectMetadataForm {

  /**
   * Get Content Models for the form.
   *
   * @param string $collection_pid
   *
   * @return array
   */
  public function getPotentialContentModels($collection_pid, array &$form_state) {
    $potential_models = array();
    
    //Add models from collection policy.
    if (($collection_policy = CollectionPolicy::loadFromCollection($collection_pid)) !== FALSE && ($content_models = $collection_policy->getContentModels())) {
      $potential_models = array();
      foreach ($content_models as $content_model) {
        $identifier = $content_model->getIdentifier();
        $name = $content_model->name;
        $potential_models["$identifier"] = "$name";
      }
    }

    //Add cmodels to programatically include...
    if ($CMs = $form_state['storage']['content_model']) {
      $CMs = (array)$CMs;
      module_load_include('inc', 'fedora_repository', 'ContentModel');
      foreach ($CMs as $content_model) {
        $content_model = ContentModel::loadFromModel($content_model);
        $identifier = $content_model->getIdentifier();
        $name = $content_model->name;
        $potential_models["$identifier"] = "$name";
      }
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
  // this code contains a temporary fix and should be replaced as soon as a more permananent solution is developed
  // currently if a collection has more than one content model listed in the collection policy the user
  // will only have access to the first form associated with the content type.


  public function createPageOne($collection_pid, $collection_label, array &$form_state) {
    //ddebug_backtrace();
    $potential_models = $this->getPotentialContentModels($collection_pid, $form_state);
    unset($potential_models['islandora:collectionCModel/ISLANDORACM']);
    list($identifier, $name) = array_peek($potential_models);
    $selected_model = isset($form_state['values']['models']) ? $form_state['values']['models'] : $identifier;
    $identifier = isset($form_state['post']['models']) ? $form_state['post']['models'] : $identifier;
    $content_model_pid = ContentModel::getPidFromIdentifier($identifier);
    $form_names = islandora_content_model_get_form_names($content_model_pid);

    $form = array();
    
    if (count($potential_models) == 0) {
      if (!user_access('manage collections')) {
        $message = t('This collection cannot receive additions.  Please contact the site administrator if you believe this message to be in error.');
      }
      else {
        if (module_exists('islandora_collection_manager')) {
          $message = t('Select the %manage tab to add content models or child collections to this collection', array('%manage' => 'Manage This Collection')); //XXX: Should get the value from the collection manager?
        }
        else {
          $collection_link = l("Islandora Collection Manager", "https://wiki.duraspace.org/display/ISLANDORA6121/Collection+Manager");
          $message = t('This collection requires either one or more content models in its COLLECTION_POLICY, or one or more child collections.<br />
                      We strongly recommend enabling the !collection_link module for managing collection objects.', array('!collection_link' => $collection_link));
        }
        $form['message'] = array(
          '#type' => 'item',
          '#value' => $message,
        );
        return $form;
      }
    }
    
    $form['indicator'] = array(
      '#type' => 'fieldset',
      '#title' => t('Ingest digital object into %collection_label (@collection_pid), Step #1:', array('@collection_pid' => $collection_pid, '%collection_label' => $collection_label)),
    );

    $form['indicator']['models'] = array(
      '#type' => 'select',
      '#title' => t('Content models available'),
      '#options' => $potential_models,
      '#default_value' => $selected_model,
      '#description' => t('Content models define datastream composition, relationships between this and other content models, and the mandatory behaviors associated with each digital object.<br /> Additional information may be found <a href="https://wiki.duraspace.org/display/FEDORACREATE/Content+Models+Overview">here.</a> '),
      '#ahah' => array(
        'path' => 'islandora/form/update',
        'wrapper' => 'content-form-choice',
        'effect' => 'fade',
      ),
    );

    $form['indicator']['content_form_ahah_wrapper'] = array(
      '#prefix' => '<div id="content-form-choice">',
      '#suffix' => '</div>',
      '#type' => 'item',
    );

    if (count($form_names) == 1) {
      $form['indicator']['content_form_ahah_wrapper']['forms'] = array(
        '#type' => 'hidden',
        '#value' => array_shift(array_keys($form_names))
      );
    }
    elseif (count($form_names) > 0) {
      $form['indicator']['content_form_ahah_wrapper']['forms'] = array(
        '#type' => 'select',
        '#title' => t('Select form'),
        '#options' => $form_names,
        '#description' => t('Select the form to populate the metadata of the new object.'),
      );
    }
    
    $form['collection_pid'] = array(
      '#type' => 'hidden',
      '#value' => $collection_pid
    );
    $form['submit'] = array(
      '#type' => 'submit',
      '#submit' => array('fedora_repository_ingest_form_submit'),
      '#value' => t('Next'),
    );
    return $form;
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
    module_load_include('inc', 'fedora_repository', 'formClass');
    $path = drupal_get_path('module', 'islandora_content_model_forms');
    $content_model_pid = ContentModel::getPidFromIdentifier($form_state['values']['models']);
    $content_model_dsid = ContentModel::getDSIDFromIdentifier($form_state['values']['models']);

    $form_name = NULL;
    if (isset($form_state['values']['forms'])) {
      $form_name = $form_state['values']['forms'];
    }
    else {
      $form_names = islandora_content_model_get_form_names($content_model_pid);
      reset($form_names);
      $form_name = key($form_names);
      $form_state['values']['forms'] = $form_name;
    }
    if (empty($form_name)) {
      $ingestForm = new formClass();
      $form_state['storage']['content_model'] = $content_model;
      $form_state['storage']['collection_pid'] = $collection_pid;
      return $ingestForm->createQDCIngestForm($collection_pid, $collection_label, $form_state);
    }
    $dsid = islandora_content_model_get_dsid($content_model_pid, $form_state['values']['forms']);
    
    $xml = NULL;
    if (!empty($form_state['storage']['xml_data'])) {
      $xml = $form_state['storage']['xml_data'];
    }
    
    $form = xml_form_builder_get_form($form_state, $form_name, $xml);
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
      $form_state['storage']['xml'] = TRUE;
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
      $file = "$tmpDir/{$file->filename}";
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
    
    $relationship = NULL;
    $pid = NULL;
    
    @$collection_policy = CollectionPolicy::loadFromCollection($collection_pid);
    if ($collection_policy !== FALSE) { //Collection policy exists; use it!
      $relationship = $collection_policy->getRelationship();
      $pid = $collection_policy->getNextPid($content_model_dsid, $content_model_pid);
    }
    
    if (empty($pid)) { //No PID yet, try to get it some other way...
      if (!empty($form_state['storage']['pid_namespace'])) {
        module_load_include('inc', 'fedora_repository', 'api/fedora_item');
        $pid = Fedora_Item::get_next_PID_in_namespace($form_state['storage']['pid_namespace']);
      }
    }
    
    if (empty($relationship)) {
      if (!empty($form_state['storage']['parent_relation'])) {
        $relationship = $form_state['storage']['parent_relation'];
      }
    }
      
    if (!empty($relationship) && !empty($pid)) {
      module_load_include('inc', 'islandora_content_model_forms', 'FOXML');
      $label_field = islandora_content_model_get_title_field($content_model_pid, $form_name);
      eval('$label = $form_state[\'values\']' . $label_field . ';'); //FIXME:  This doesn't really work in many cases (building the title from multiple fields, as for a person's last and first name fields.  Also:
      //FIXME:  'eval' is terrible... Get rid of it!
      
      if (empty($form['#post']['active-toggle'])) {
        $state = 'A';
      }
      else {
        $state = $form['#post']['active-toggle'] ? 'A' : 'I';
      }
      
      $form_state['values']['pid'] = $pid;
      // added ne functionality to static thumbnails to objects if values are provided in forms
      $module = isset($form_state['values']['module']) ? $form_state['values']['module'] : NULL;
      $image_dir = isset($form_state['values']['image_directory']) ? $form_state['values']['image_directory'] : NULL;
      $thumbnail_file = isset($form_state['values']['thumbnail_file']) ? $form_state['values']['thumbnail_file'] : NULL;

      if ($module && $thumbnail_file) {
        $stored_thumbnail = drupal_get_path('module', $module) . "/$image_dir/$thumbnail_file";
        $stored_thumbnail = preg_replace('/\/\//', '/', $stored_thumbnail);
        file_copy($stored_thumbnail, file_directory_path());
        $thumbnail = file_directory_path() . '/' . $thumbnail_file;
        $_SESSION['fedora_ingest_files']['TN'] = $thumbnail;
      }

      $uploaded_file = isset($form_state['values']['ingest-file-location']) && trim($form_state['values']['ingest-file-location']) !== '';
      $ingest_file_location = $uploaded_file ? $form_state['values']['ingest-file-location'] : NULL;
      if (isset($ingest_file_location)) {
        $ingest_file_location = is_array($ingest_file_location) ? $ingest_file_location : array('OBJ' => $ingest_file_location);
      }
      if (isset($_SESSION['fedora_ingest_files']) && is_array($_SESSION['fedora_ingest_files'])) {
        foreach ($_SESSION['fedora_ingest_files'] as $key => $value) {
          $ingest_file_location[$key] = $value;
        }
      }

      $transform = islandora_content_model_get_transform($content_model_pid, $form_name);

      $foxml = new FOXML($label, $pid, $dsid, $content_model_pid, $collection_pid, $relationship, $ingest_file_location, $document, $transform, $state);
      $foxml->ingest();
      $_SESSION['fedora_ingest_files'] = '';
      $form_state['storage'] = NULL;
    }
    $form_state['redirect'] = "fedora/repository/{$collection_pid}";
  }

}

function islandora_content_model_update_form_choice() {
  // prep only
  $form_state = array('storage' => NULL, 'submitted' => FALSE);
  $form_build_id = $_POST['form_build_id'];
  $form = form_get_cache($form_build_id, $form_state);
  $args = $form['#parameters'];
  $form_id = array_shift($args);
  $form_state['post'] = $form['#post'] = $_POST;
  // Enable the submit/validate handlers to determine whether AHAH-submittted.
  $form_state['ahah_submission'] = TRUE;
  $form['#programmed'] = $form['#redirect'] = FALSE;
  drupal_process_form($form_id, $form, $form_state);
  $form = drupal_rebuild_form($form_id, $form_state, $args, $form_build_id);

  $changed_elements = $form['indicator']['content_form_ahah_wrapper'];
  unset($changed_elements['#prefix'], $changed_elements['#suffix']);
  $output = theme('status_messages') . drupal_render($changed_elements);
  drupal_json(array(
    'status' => TRUE,
    'data' => $output,
  ));
}
