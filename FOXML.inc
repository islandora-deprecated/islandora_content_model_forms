<?php

/**
 * @file
 *
 */
module_load_include('inc', 'islandora_form_builder', 'Utilities');

class FOXML extends DOMDocument {
  const foxml = 'info:fedora/fedora-system:def/foxml#';
  const xlink = 'http://www.w3.org/1999/xlink';
  const xsi = 'http://www.w3.org/2001/XMLSchema-instance';
  const xmlns = 'http://www.w3.org/2000/xmlns/';
  const rdf = 'http://www.w3.org/1999/02/22-rdf-syntax-ns#';
  const rdfs = 'http://www.w3.org/2000/01/rdf-schema#';
  const fedora = 'info:fedora/fedora-system:def/relations-external#';
  const dc = 'http://purl.org/dc/elements/1.1/';
  const oai_dc = 'http://www.openarchives.org/OAI/2.0/oai_dc/';
  const fedora_model = 'info:fedora/fedora-system:def/model#';

  /**
   *
   * @var <type>
   */
  protected $root;
  protected $pid;
  protected $dsid;
  protected $collectionPid;
  protected $contentModelPid;
  protected $ingestFileLocations;
  protected $label;
  protected $user;
  protected $document;
  protected $transform;
  protected $state;

  /**
   *
   * @global <type> $user
   * @param <type> $label
   * @param <type> $pid
   * @param <type> $dsid
   * @param <type> $content_model_pid
   * @param <type> $collection_pid
   * @param <type> $relationship
   * @param <type> $ingest_file_location
   * @param <type> $document 
   */
  public function __construct($label, $pid, $dsid, $content_model_pid, $collection_pid, $relationship, $ingest_file_locations, $document, $transform, $state = 'A') {
    global $user;
    parent::__construct("1.0", "UTF-8"); // DomDocument
    $this->formatOutput = TRUE;
    $this->preserveWhiteSpace = FALSE;
    $this->label = substr($label, 0, 256);
    $this->user = $user->name;
    $this->pid = $pid;
    $this->dsid = $dsid;
    $this->state = $state;
    $this->contentModelPid = $content_model_pid;
    $this->collectionPid = $collection_pid;
    $this->relationship = isset($relationship) ? $relationship : 'isMemberOfCollection';
    $this->ingestFileLocations = $ingest_file_locations;
    $this->document = $document;
    $this->transform = $transform;
    $this->root = $this->createRootElement();
    $this->createDocument();
  }

  private function createRootElement() {
    $root = $this->createElementNS(self::foxml, 'foxml:digitalObject');
    $root->setAttribute('VERSION', '1.1');
    $root->setAttribute('PID', "{$this->pid}");
    $root->setAttribute('xmlns', self::foxml);
    $root->setAttributeNS(self::xmlns, 'xmlns:foxml', self::foxml);
    $root->setAttributeNS(self::xmlns, 'xmlns:xsi', self::xsi);
    $root->setAttributeNS(self::xsi, 'xsi:schemaLocation', self::foxml . " http://www.fedora.info/definitions/1/0/foxml1-1.xsd");
    $this->appendChild($root);
    return $root;
  }

  private function createDocument() {
    /**
     * If DOMNodes are not appended in the corrected order root -> leaf, namespaces may break...
     * So be be cautious, add DOMNode's to thier parent element before adding child elements to them.
     */
    $this->createObjectProperties();
    $this->createRelationships();
    $this->createIngestFileDatastreams();
    $this->createPolicy();
    $this->createDocumentDatastream();
    $this->createDublinCoreDatastream();
    $this->createCollectionPolicy();
    $this->createWorkflowStream();
  }

  /**
   *
   * @param DOMElement $root
   * @return DOMElement
   */
  private function createObjectProperties() {
    global $user;
    $object_properties = $this->createElementNS(self::foxml, 'foxml:objectProperties');
    $this->root->appendChild($object_properties);

    $property = $this->createElementNS(self::foxml, 'foxml:property');
    $property->setAttribute('NAME', 'info:fedora/fedora-system:def/model#state');
    $property->setAttribute('VALUE', $this->state);
    $object_properties->appendChild($property);

    // Because of FCREPO-1059 we need to make sure that any label we create doesn't
    // contain any newlines. This is valid for at least version 3.5 of Fedora.
    $label = str_replace(array("\r", "\r\n", "\n"), '', $this->label);

    $property = $this->createElementNS(self::foxml, 'foxml:property');
    $property->setAttribute('NAME', 'info:fedora/fedora-system:def/model#label');
    $property->setAttribute('VALUE', $label);
    $object_properties->appendChild($property);

    $property = $this->createElementNS(self::foxml, 'foxml:property');
    $property->setAttribute('NAME', 'info:fedora/fedora-system:def/model#ownerId');
    $property->setAttribute('VALUE', $user->name);
    $object_properties->appendChild($property);
    return $object_properties;
  }

  /**
   *
   * @return DOMElement
   */
  private function createRelationships() {
    $datastream = $this->createDatastreamElement('RELS-EXT', NULL, 'X');
    $version = $this->createDatastreamVersionElement('RELS-EXT.0', 'RDF Statements about this Object', 'application/rdf+xml', 'info:fedora/fedora-system:FedoraRELSExt-1.0');
    $content = $this->createDatastreamContentElement();

    $rdf = $this->createElementNS(self::rdf, 'rdf:RDF');
    $rdf->setAttributeNS(self::xmlns, 'xmlns:rdf', self::rdf);
    $rdf->setAttributeNS(self::xmlns, 'xmlns:rdfs', self::rdfs);
    $rdf->setAttributeNS(self::xmlns, 'xmlns:fedora', self::fedora);
    $rdf->setAttributeNS(self::xmlns, 'xmlns:dc', self::dc);
    $rdf->setAttributeNS(self::xmlns, 'xmlns:oai_dc', self::oai_dc);
    $rdf->setAttributeNS(self::xmlns, 'xmlns:fedora-model', self::fedora_model);

    $rdf_description = $this->createElementNS(self::rdf, 'rdf:Description');
    $rdf_description->setAttributeNS(self::rdf, 'rdf:about', "info:fedora/{$this->pid}");

    $member = $this->createElementNS(self::fedora, "fedora:{$this->relationship}");
    $member->setAttributeNS(self::rdf, 'rdf:resource', "info:fedora/{$this->collectionPid}");

    $has_rdf_model = $this->createElementNS(self::fedora_model, 'fedora-model:hasModel');
    $has_rdf_model->setAttributeNS(self::rdf, "rdf:resource", "info:fedora/{$this->contentModelPid}");

    $this->root->appendChild($datastream)->appendChild($version)->appendChild($content);
    $content->appendChild($rdf)->appendChild($rdf_description);
    $rdf_description->appendChild($member);
    $rdf_description->appendChild($has_rdf_model);
  }

  private function createIngestFileDatastreams() {
    module_load_include('inc', 'fedora_repository', 'api/fedora_util');
    if (empty($this->ingestFileLocations) || count($this->ingestFileLocations) === 0)
      return;
    foreach ($this->ingestFileLocations as $dsid => $file) {
      $dsid = fix_dsid($dsid);
      list($label, $mime_type, $file_url) = $this->getFileAttributes($file);
      $datastream = $this->createDatastreamElement($dsid, 'A', 'M');
      $version = $this->createDatastreamVersionElement($dsid . '.0', $label, $mime_type);
      $content = $this->createDatastreamContentLocationElement('URL', $file_url);
      $this->root->appendChild($datastream)->appendChild($version)->appendChild($content);
    }
  }

  private function getFileAttributes($file) {
    global $base_url;
    module_load_include('inc', 'fedora_repository', 'MimeClass');
    module_load_include('inc', 'fedora_repository', 'api/fedora_util');
    $mime = new MimeClass();
    $mime_type = $mime->getType($file);
    $parts = explode('/', $file);
    foreach ($parts as $n => $part) {
      $parts[$n] = rawurlencode($part);
    }
    $path = implode('/', $parts);
    $file_url = $base_url . '/' . $path;
    $beginIndex = strrpos($file_url, '/');
    $dtitle = substr($file_url, $beginIndex + 1);
    $dtitle = urldecode($dtitle);
    $dtitle = fix_dsid($dtitle);
    return array($dtitle, $mime_type, $file_url);
  }

  private function createPolicy() {
    $policy_element = $this->getPolicyStreamElement();
    if ($policy_element) {
      $datastream = $this->createDatastreamElement('POLICY', 'A', 'X');
      $version = $this->createDatastreamVersionElement('POLICY.0', 'POLICY', 'text/xml');
      $content = $this->createDatastreamContentElement();
      $this->root->appendChild($datastream)->appendChild($version)->appendChild($content)->appendChild($policy_element);
    }
  }

  /**
   *
   * @return DOMElement
   */
  private function getPolicyStreamElement() {
    module_load_include('inc', 'fedora_repository', 'ObjectHelper');
    $object_helper = new ObjectHelper();
    $policy_stream = $object_helper->getStream($this->collectionPid, 'CHILD_SECURITY', FALSE);
    if (!isset($policy_stream)) {
      return NULL; //there is no policy stream so object will not have a policy stream
    }
    try {
      $xml = new SimpleXMLElement($policy_stream);
    } catch (Exception $exception) {
      watchdog(t("Fedora_Repository"), t("Problem getting security policy."), NULL, WATCHDOG_ERROR);
      drupal_set_message(t('Problem getting security policy: @e', array('@e' => $exception->getMessage())), 'error');
      return FALSE;
    }
    $policy_element = $this->createDocumentFragment();
    if (!$policy_element) {
      drupal_set_message(t('Error parsing security policy stream.'));
      watchdog(t("Fedora_Repository"), t("Error parsing security policy stream, could not parse policy stream."), NULL, WATCHDOG_NOTICE);
      return FALSE;
    }
    $this->importNode($policy_element, TRUE);
    $value = $policy_element->appendXML($policy_stream);
    if (!$value) {
      drupal_set_message(t('Error creating security policy stream.'));
      watchdog(t("Fedora_Repository"), t("Error creating security policy stream, could not parse collection policy template file."), NULL, WATCHDOG_NOTICE);
      return FALSE;
    }
    return $policy_element;
  }

  /**
   * Creates Collection policy data stream from a template stored within the
   * Content Model.
   *
   * @return DOMElement
   */
  private function createCollectionPolicy() {
    module_load_include('inc', 'fedora_repository', 'api/fedora_item');
    $fedora_item = new fedora_item($this->contentModelPid);
    $datastreams = $fedora_item->get_datastreams_list_as_array();
    if (isset($datastreams['COLLECTION_POLICY_TMPL'])) {
      $collection_policy_template = $fedora_item->get_datastream_dissemination('COLLECTION_POLICY_TMPL');
      $collection_policy_template_dom = DOMDocument::loadXML($collection_policy_template);
      $collection_policy_template_root = $collection_policy_template_dom->getElementsByTagName('collection_policy');
      if ($collection_policy_template_root->length > 0) {
        $collection_policy_template_root = $collection_policy_template_root->item(0);
        $node = $this->importNode($collection_policy_template_root, TRUE);
        $datastream = $this->createDatastreamElement('COLLECTION_POLICY', 'A', 'X');
        $version = $this->createDatastreamVersionElement('COLLECTION_POLICY.0', 'Collection Policy', 'text/xml');
        $content = $this->createDatastreamContentElement();
        $this->root->appendChild($datastream)->appendChild($version)->appendChild($content)->appendChild($node);
      }
    }
  }

  private function createDatastreamElement($id = NULL, $state = NULL, $control_group = NULL) {
    $datastream = $this->createElementNS(self::foxml, 'foxml:datastream');
    if (isset($id)) {
      $datastream->setAttribute('ID', $id);
    }
    if (isset($state)) {
      $datastream->setAttribute('STATE', $state);
    }
    if (isset($control_group)) {
      $datastream->setAttribute('CONTROL_GROUP', $control_group);
    }
    return $datastream;
  }

  private function createDatastreamVersionElement($id = NULL, $label = NULL, $mime_type = NULL, $format_uri = NULL) {
    $version = $this->createElementNS(self::foxml, 'foxml:datastreamVersion');
    if (isset($id)) {
      $version->setAttribute('ID', $id);
    }
    if (isset($label)) {
      $version->setAttribute('LABEL', $label);
    }
    if (isset($mime_type)) {
      $version->setAttribute('MIMETYPE', $mime_type);
    }
    if (isset($format_uri)) {
      $version->setAttribute('FORMAT_URI', $format_uri);
    }
    return $version;
  }

  private function createDatastreamContentElement() {
    $content = $this->createElementNS(self::foxml, 'foxml:xmlContent');
    return $content;
  }

  private function createDatastreamContentLocationElement($type = NULL, $ref = NULL) {
    $location = $this->createElementNS(self::foxml, 'foxml:contentLocation');
    if (isset($type)) {
      $location->setAttribute('TYPE', $type);
    }
    if (isset($ref)) {
      $location->setAttribute('REF', $ref);
    }
    return $location;
  }

  /**
   * Creates WorkFlow datastream from a template stored within the Content Model.
   *
   * @return DOMElement
   */
  private function createWorkflowStream() {
    module_load_include('inc', 'fedora_repository', 'api/fedora_item');
    $fedora_item = new fedora_item($this->contentModelPid);
    $datastreams = $fedora_item->get_datastreams_list_as_array();
    if (isset($datastreams['WORKFLOW_TMPL'])) {
      $work_flow_template = $fedora_item->get_datastream_dissemination('WORKFLOW_TMPL');
      $work_flow_template_dom = DOMDocument::loadXML($work_flow_template);
      $work_flow_template_root = $work_flow_template_dom->getElementsByTagName('workflow');
      if ($work_flow_template_root->length > 0) {
        $work_flow_template_root = $work_flow_template_root->item(0);
        $node = $this->importNode($work_flow_template_root, TRUE);
        $datastream = $this->createDatastreamElement('WORKFLOW', 'A', 'X');
        $version = $this->createDatastreamVersionElement("{$this->dsid}.0", "{$this->dsid} Record", 'text/xml');
        $content = $this->createDatastreamContentElement();
        $this->root->appendChild($datastream)->appendChild($version)->appendChild($content)->appendChild($node);
      }
    }
  }

  /**
   * Creates an xml based datastream by importing the root node from $this->document.
   */
  public function createDocumentDatastream() {
    if($this->document !== NULL) {
      $datastream = $this->createDatastreamElement($this->dsid, 'A', 'X');
      $version = $this->createDatastreamVersionElement("{$this->dsid}.0", "{$this->dsid} Record", 'text/xml');
      $content = $this->createDatastreamContentElement();
      $node = $this->importNode($this->document->documentElement, TRUE);
      $this->root->appendChild($datastream)->appendChild($version)->appendChild($content)->appendChild($node);
      // Once again god damn you libxml...
      $class = get_class($this->document);
      // FIX THIS!
      /*
        $namespaces = call_user_func(array($class, 'getRequiredNamespaces'));
        foreach ($namespaces as $prefix => $uri) {
        $node->setAttributeNS(self::xmlns, "xmlns:$prefix", $uri);
        } */
    }
  }

  /**
   * Creates a Dublin Core document by transforming $this->document.
   */
  private function createDublinCoreDatastream() {
    $datastream = $this->createDatastreamElement('DC', 'A', 'X');
    $version = $this->createDatastreamVersionElement('DC.0', 'Dublin Core Record', 'text/xml');
    $content = $this->createDatastreamContentElement();
    $dublin_core = $this->applyTransformation();
    if (isset($dublin_core)) {
      $this->root->appendChild($datastream)->appendChild($version)->appendChild($content)->appendChild($dublin_core);
      $dublin_core->setAttributeNS(self::xmlns, 'xmlns:xsi', self::xsi); // GOD Damn you libxml!
    }
  }

  /**
   * Transforms a document via an XSL.
   *
   * @return DOMElement
   *   A DOMDocumentFragment that contains the transformed Dublin Core
   *   Document.
   */
  private function applyTransformation() {
    if($this->transform !== NULL) {
      $xsl = new DOMDocument();
      $xsl->load($this->transform);
      $xslt = new XSLTProcessor();
      $xslt->importStyleSheet($xsl);
      $document = $xslt->transformToDoc($this->document);
      if ($document) {
        $xpath = new DOMXPath($document);
        // Set the Label
        $results = $xpath->query("*[local-name()='title']");
        $results->item(0)->nodeValue = $this->label;
        // Set the Pid
        $results = $xpath->query("*[local-name()='identifier']");
        $results->item(0)->nodeValue = $this->pid;
        if (isset($document->documentElement)) {
          return $this->importNode($document->documentElement, TRUE);
        }
      }
    }
    return NULL;
  }

  /**
   * Ingests this Foxml document into fedora.
   */
  public function ingest() {
    $ret = FALSE;
    try {
      $object = Fedora_Item::ingest_from_FOXML($this);

      if (!empty($object->pid)) {
        drupal_set_message(t("Item !pid created successfully.", array('!pid' => l($object->pid, 'fedora/repository/' . $object->pid))), "status");
        $ret = TRUE;
      }
      if (is_array($this->ingestFileLocations)) {
        foreach ($this->ingestFileLocations as $file) {
          file_delete($file);
        }
      }
    } catch (exception $exception) {
      drupal_set_message(t("Error ingesting object: @e", array('@e' => $exception->getMessage())), 'error');
      watchdog(t("Fedora_Repository"), t("Error ingesting object: @e", array('@e' => $exception->getMessage())), NULL, WATCHDOG_ERROR);
      // TODO is this necessary?
      foreach ($this->ingestFileLocations as $file)
        file_delete($file);
    }
    return $ret;
  }

}
