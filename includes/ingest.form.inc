<?php

function islandora_content_model_forms_get_ingest_form($form, &$form_state, $collection_pid) {
  $page = $form_state['storage']['step'] = empty($form_state['storage']['step']) ? 1 : $form_state['storage']['step'];
  $collection_label = 'test'; //@TODO: collection label
    if ($page == 1) {
      $form_state['storage']['xml'] = TRUE;
      $form_state['storage']['collection_pid'] = $collection_pid;
      $form = islandora_content_model_forms_ingest_page_1($form_state, $collection_pid, $collection_label);
    }
    else if ($form_state['storage']['xml']) {
      $form_state['storage']['content_model'] = $form_state['values']['models'];
      $form = islandora_content_model_forms_ingest_page_2($form_state, $collection_pid, $collection_label);
    }
    else {
      throw new Exception("Couldn't Create the Ingest Form.");
    }
  
  return $form;
}

function islandora_content_model_forms_get_ingest_form_validate($form, &$form_state) {
  //only validate the form if the submit button was pressed (other buttons may be used for AHAH)
  if (!$form_state['submitted']) {
    return;
  }
 
  if (strpos($form_state['clicked_button']['#id'], 'edit-submit') === 0) {
    switch ($form_state['storage']['step']) {
      case 1:
        $form_state['storage']['step']++;
        $form_state['rebuild'] = TRUE;
        break;

      case 2:
        // XML based form.
        if ($form_state['storage']['xml']) {
          module_load_include('inc', 'xml_form_api', 'XMLForm');
          $xml_form = new XMLForm($form_state);
          $xml_form->validate($form, $form_state);
        }
        // Get the uploaded file.
        
        $arr = array();
        if (!empty($types_allowed)) {
 	      foreach ($types_allowed as $type) {
            $arr[] = $mimetype->getExtension($type);
 	      }
        }
         
        if (!empty($_FILES['files']['name']['ingest-file-location'])) {
          $fileObject = file_save_upload('ingest-file-location', array('file_validate_extensions' => $arr));

          if ($fileObject) {
            $form_state['values']['ingest-file-location'] = $fileObject->uri;
            $form_state['values']['ingest-file-location-id'] = $fileObject->fid;
          }
        }

        $form_state['rebuild'] = FALSE;
        break;
    }
  }
}

function islandora_content_model_forms_ingest_form_submit($form, &$form_state) {
  //only validate the form if the submit button was pressed (other buttons may be used for AHAH)
  if (!$form_state['submitted']) {
    return;
  }
 
    module_load_include('inc', 'xml_form_api', 'XMLForm');
    module_load_include('inc', 'islandora', 'includes/islandora.ingest');
    
    global $base_url;
    if (isset($_FILES['files']['name']['file_upload']) && $_FILES['files']['type']['file_upload'] == 'text/xml') {
      $tmpDir = file_directory_path();
      if (!$file = file_save_upload('file_upload', array('file_validate_extensions' => array()), $tmpDir)) {
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
    
    
    global $user;
    module_load_include('inc', 'islandora', 'RestConnection');
    $restConnection = new RestConnection($user);
    $fedora_object = new FedoraObject($collection_pid, $restConnection->repository);
    $collection_policy_info = islandora_ingest_get_information($fedora_object);
    
    $relationship = $collection_policy_info['relationship'];
    
    $object = islandora_ingest_get_object($collection_policy_info['models'], $collection_pid, $collection_policy_info['relationship'], 'islandora');
      $transform = islandora_content_model_get_transform($content_model_pid, $form_name);
      $label_field = islandora_content_model_get_title_field($content_model_pid, $form_name);
      eval('$label = $form_state[\'values\']' . $label_field . ';');
     
    $object->label = substr($label, 0, 256);
      $object->owner = $user->name;
      
      if (!isset($form['#post']['active-toggle'])) {
        $state = 'A';
      }
      else {
        $state = $form['#post']['active-toggle'] ? 'A' : 'I';
      }
      $object->state = $state;
      $object = islandora_ingest_add_object($object);
      
      $ds = $object->constructDatastream($dsid, 'M');
      $ds->setContentFromString($document->saveXML());
      $ds->label = $dsid . ' Record';
      $object->ingestDatastream($ds);
      
      $dc_ds = new FedoraDatastream('DC', $object, $restConnection->repository);
      $dc_doc = islandora_content_model_forms_transform_document($transform, $document, $object);
      $dc_ds->setContentFromString($dc_doc->saveXML());
     
    if (!empty($relationship) && !empty($object->id)) {
     
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
    
      islandora_content_model_forms_ingest_files($ingest_file_location, $object);
      
      if (isset($form_state['values']['ingest-file-location-id'])) {
        file_delete(file_load($form_state['values']['ingest-file-location-id']));
      }
      
      $form_state['storage'] = NULL;
    }
      
    $form_state['redirect'] = "islandora/object/{$collection_pid}";
  
}

function islandora_content_model_forms_ingest_page_1(&$form_state, $collection_pid, $collection_label) {
    $potential_models = islandora_content_model_forms_get_potential_content_models($collection_pid, $form_state);
    unset($potential_models['islandora:collectionCModel/ISLANDORACM']);
    list($identifier, $name) = array_peek($potential_models);
    $selected_model = isset($form_state['values']['models']) ? $form_state['values']['models'] : $identifier;
    $content_model_pid = $selected_model;
    
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
      
      '#ajax' => array(
        'callback' => 'islandora_content_model_forms_update_form_choice',
        'wrapper' => 'content-form-choice',
        'effect' => 'fade',
        'method' => 'replace',
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
        '#value' => array_shift(array_keys($form_names)),
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
      '#value' => $collection_pid,
    );
    
    $form['submit'] = array(
      '#type' => 'submit',
      '#validate' => array('islandora_content_model_forms_get_ingest_form_validate'),
      '#value' => t('Next'),
    );
    
    return $form;
}

function islandora_content_model_forms_ingest_page_2(&$form_state, $collection_pid, $collection_label) {
    $path = drupal_get_path('module', 'islandora_content_model_forms');
   
    $content_model_pid = $form_state['values']['models'];
    $content_model_dsid = $content_model_pid; //@TODO: check what this should be

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
      //@TODO: what should happen here
    }
    $dsid = islandora_content_model_get_dsid($content_model_pid, $form_state['values']['forms']);

    $xml = NULL;
    if (!empty($form_state['storage']['xml_data'])) {
      $xml = $form_state['storage']['xml_data'];
    }

    $form = xml_form_builder_get_form($form_state, $form_name, $xml);
    $form['collection_pid'] = array(
      '#type' => 'hidden',
      '#value' => $collection_pid,
    );
    $form['content_model_pid'] = array(
      '#type' => 'hidden',
      '#value' => $content_model_pid,
    );
    $form['content_model_dsid'] = array(
      '#type' => 'hidden',
      '#value' => $content_model_dsid,
    );
    $form['dsid'] = array(
      '#type' => 'hidden',
      '#value' => $dsid,
    );
    $form['models'] = array(
      '#type' => 'hidden',
      '#value' => $form_state['values']['models'],
    );
    $form['forms'] = array(
      '#type' => 'hidden',
      '#value' => $form_state['values']['forms'],
    );
    
    $form['#submit'] = array('islandora_content_model_forms_ingest_form_submit');
    $form['#validate'] = array('islandora_content_model_forms_get_ingest_form_validate');
    return $form;
}

function islandora_content_model_forms_update_form_choice($form, $form_state) {
  return $form['indicator']['content_form_ahah_wrapper'];
}

function islandora_content_model_forms_get_potential_content_models($collection_pid, $form_state = array()) {
  module_load_include('inc', 'islandora', 'RestConnection');
  module_load_include('inc', 'islandora', 'includes/islandora.ingest');
  
  global $user;
  $restConnection = new RestConnection($user);
  $fedora_object = new FedoraObject($collection_pid, $restConnection->repository);
  $info = islandora_ingest_get_information($fedora_object);
   
  $potential_models = array();
  foreach ($info['models'] AS $model) {
    $potential_models[$model['pid']] = $model['name'];
  }
  
  if (isset($form_state['storage']['content_model']) && $content_models = $form_state['storage']['content_model']) {
    $content_models = (array) $content_models;
    foreach ($content_models as $content_model_pid) {
      $content_model = new FedoraObject($content_model_pid, $restConnection->repository);
      $potential_models["$content_model_pid"] = $content_model->label;
    }
  }
  
  return $potential_models;
}

function islandora_content_model_forms_transform_document($transform, $source_document, $object) {
  if ($transform !== NULL) {
      $xsl = new DOMDocument();
      $xsl->load($transform);
      $xslt = new XSLTProcessor();
      $xslt->importStyleSheet($xsl);
      $document = $xslt->transformToDoc($source_document);
      if ($document) {
        $xpath = new DOMXPath($document);
        // Set the Label
        $results = $xpath->query("*[local-name()='title']");
        $results->item(0)->nodeValue = $object->label;
        // Set the Pid
        $results = $xpath->query("*[local-name()='identifier']");
        $results->item(0)->nodeValue = $object->id;
        if (isset($document->documentElement)) {
          return $document;
        }
      }
    }
    return NULL;
}

function islandora_content_model_forms_ingest_files($ingest_file_locations, $object) {
  if (empty($ingest_file_locations) || count($ingest_file_locations) === 0) {
    return;
  }
  foreach ($ingest_file_locations as $dsid => $file) {
    list($label, $mime_type) = _islandora_content_model_forms_get_file_attributes($file);
    
    $ds = $object->constructDatastream($dsid, 'M');
    $file = drupal_realpath($file);
    $ds->setContentFromFile($file);
    $ds->label = $label;
    $ds->mimetype = $mime_type;
    $object->ingestDatastream($ds);
  }
}

function _islandora_content_model_forms_get_file_attributes($file) {
    global $base_url;
    module_load_include('inc', 'islandora', 'includes/MimeClass');
    
    $mime = new MimeClass();
    $mime_type = $mime->getMimetype($file);
    $parts = explode('/', $file);
    foreach ($parts as $n => $part) {
      $parts[$n] = rawurlencode($part);
    }
    $path = implode('/', $parts);
    $file_url = $base_url . '/' . $path;
    $beginIndex = strrpos($file_url, '/');
    $dtitle = substr($file_url, $beginIndex + 1);
    $dtitle = urldecode($dtitle);
    return array($dtitle, $mime_type);
}

/*
function islandora_content_model_forms_content_model_get_mimetypes($pid) {
  global $user;
    module_load_include('inc', 'islandora', 'RestConnection');
    $restConnection = new RestConnection($user);
    $fedora_object = new FedoraObject($collection_pid, $restConnection->repository);
    
        $result = $this->xml->getElementsByTagName('mimetypes');
        $result = $result->item(0)->getElementsByTagName('type');
        $mimes = array();
        for ($i = 0; $i < $result->length; $i++) {
          $mimes[] = trim($result->item($i)->nodeValue);
        }
        return $mimes;
}
*/