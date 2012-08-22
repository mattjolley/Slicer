import os
import glob
import tempfile
import zipfile
from __main__ import qt
from __main__ import vtk
from __main__ import ctk
from __main__ import slicer

import DICOMLib

#########################################################
#
# 
comment = """

DICOMDataExchange supports moving data between slicer
data structures and dicom datastructures (using ctkDICOMDatabase
and related code).

This code is slicer-specific and relies on the slicer python module
for elements like slicer.dicomDatatabase and slicer.mrmlScene

# TODO : 
"""
#
#########################################################

class DICOMLoader(object):
  """Code to load dicom files into slicer
  """

  def __init__(self,files=None,name=None,method='archetype'):
    self.files = files
    self.name = name
    self.method = method
    if self.files:
      self.loadFiles()

  def loadFiles(self):
    if self.method == 'auto':
        self.method = 'archetype'
    elif self.method == 'archetype':
      self.loadFilesWithArchetype()
    else:
      raise RuntimeError("Unknown dicom load method '%s'" % self.method)

    #
    # add list of DICOM instance UIDs to the volume node
    # corresponding to the loaded files
    #
    instanceUIDs = ""
    instanceUIDTag = "0008,0018"
    for file in self.files:
      slicer.app.dicomDatabase().loadFileHeader(file)
      d = slicer.app.dicomDatabase().headerValue(instanceUIDTag)
      print('header for %s is %s' % (instanceUIDTag, d))
      try:
        uid = d[d.index('[')+1:d.index(']')]
      except ValueError:
        uid = "Unknown"
      instanceUIDs += uid + " "
    instanceUIDs = instanceUIDs[:-1]  # strip last space
    self.volumeNode.SetAttribute("DICOM.instanceUIDs", instanceUIDs)

    # automatically select the volume to display
    appLogic = slicer.app.applicationLogic()
    selNode = appLogic.GetSelectionNode()
    if self.volumeNode:
      selNode.SetReferenceActiveVolumeID(self.volumeNode.GetID())
      appLogic.PropagateVolumeSelection()

  def loadFilesWithArchetype(self):
    """Load files in the traditional Slicer manner
    using the volume logic helper class
    and the vtkITK archetype helper code
    """
    fileList = vtk.vtkStringArray()
    for f in self.files:
      fileList.InsertNextValue(f)
    vl = slicer.modules.volumes.logic()
    self.volumeNode = vl.AddArchetypeVolume( self.files[0], self.name, 0, fileList )

class DICOMVolume(object):
  """Handy container for things related
  to a slicer volume created from a list
  of dicom files
  """

  def __init__(self,files=[],name="NoName",warning=None,selected=True):
    self.files = files
    self.name = name
    self.warning = warning
    self.selected = selected

class DICOMVolumeOrganizer(object):
  """Code to organize sets of dicom files into
  chunks that translate into well formed
  slicer volumes (splitting based on things like the
  slice orientation, spacing, etc)
  """

  def __init__(self,files=None,name=None,epsilon=0.01):
    self.files = files
    self.name = name
    self.volumes = []
    self.epsilon = epsilon
    if self.files:
      self.organizeFiles()

  def organizeFiles(self,files=None):
    """ Returns a list of DICOMVolume instances
    corresponding to ways of interpreting the 
    files parameter.
    """
    if not files:
      files = self.files

    # get the series description to use as base for volume name
    slicer.app.dicomDatabase().loadFileHeader(files[0])
    seriesDescription = "0008,103e"
    d = slicer.app.dicomDatabase().headerValue(seriesDescription)
    try:
      name = d[d.index('[')+1:d.index(']')]
    except ValueError:
      name = "Unknown"

    # default volume includes all files for series
    volume = DICOMVolume()
    volume.files = files
    volume.name = name
    volume.selected = True
    self.volumes.append(volume)

    # while looping through files, keep track of their
    # position and orientation for later use
    POSITION = "0020,0032"
    ORIENTATION = "0020,0037"
    positions = {}
    orientations = {}


    # make subseries volumes based on tag differences
    subseriesSpecs = {
        "SeriesInstanceUID":              "0020,000E",
        "ContentTime":                    "0008,0033",
        "TriggerTime":                    "0018,1060",
        "DiffusionGradientOrientation":   "0018,9089 ",
        "ImageOrientationPatient":        "0020,0037"
    }

    #
    # first, look for subseries within this series
    # - build a list of files for each unique value
    #   of each spec
    #
    subseriesFiles = {}
    subseriesValues = {}
    for file in volume.files:
      slicer.app.dicomDatabase().loadFileHeader(file)
      # save position and orientation
      v = slicer.app.dicomDatabase().headerValue(POSITION)
      try:
        positions[file] = v[v.index('[')+1:v.index(']')]
      except ValueError:
        positions[file] = None
      v = slicer.app.dicomDatabase().headerValue(ORIENTATION)
      try:
        orientations[file] = v[v.index('[')+1:v.index(']')]
      except ValueError:
        orientations[file] = None
      # check for subseries values
      for spec in subseriesSpecs.keys():
        v = slicer.app.dicomDatabase().headerValue(subseriesSpecs[spec])
        try:
          value = v[v.index('[')+1:v.index(']')]
        except ValueError:
          value = "Unknown"
        if not subseriesValues.has_key(spec):
          subseriesValues[spec] = []
        if not subseriesValues[spec].__contains__(value):
          subseriesValues[spec].append(value)
        if not subseriesFiles.has_key((spec,value)):
          subseriesFiles[spec,value] = []
        subseriesFiles[spec,value].append(file)


    #
    # second, for any specs that have more than one value, create a new
    # virtual series
    #
    for spec in subseriesSpecs.keys():
      if len(subseriesValues[spec]) > 1:
        for value in subseriesValues[spec]:
          # default volume includes all files for series
          volume = DICOMVolume()
          volume.files = subseriesFiles[spec,value]
          volume.name = name + " for %s of %s" % (spec,value)
          volume.selected = False
          self.volumes.append(volume)


    #
    # now for each series and subseries, sort the images
    # by position and check for consistency
    #

    # TODO: more consistency checks:
    # - is there gantry tilt?
    # - are the orientations the same for all slices?
    NUMBER_OF_FRAMES = "0028,0008"
    for volume in self.volumes:
      #
      # use the first file to get the ImageOrientationPatient for the 
      # series and calculate the scan direction (assumed to be perpendicular
      # to the acquisition plane)
      #
      slicer.app.dicomDatabase().loadFileHeader(volume.files[0])
      v = slicer.app.dicomDatabase().headerValue(NUMBER_OF_FRAMES)
      try:
        value = v[v.index('[')+1:v.index(']')]
      except ValueError:
        value = "Unknown"
      if value != "Unknown":
        volume.warning = "Multi-frame image. If slice orientation or spacing is non-uniform then the image may be displayed incorrectly. Use with caution."

      validGeometry = True
      ref = {}
      for tag in [POSITION, ORIENTATION]:
        v = slicer.app.dicomDatabase().headerValue(tag)
        try:
          value = v[v.index('[')+1:v.index(']')]
        except ValueError:
          value = "Unknown"
        if value == "Unknown":
          volume.warning = "Reference image in series does not contain geometry information.  Please use caution."
          validGeometry = False
          break
        ref[tag] = value

      if not validGeometry:
        continue

      # get the geometry of the scan
      # with respect to an arbitrary slice
      sliceAxes = [float(zz) for zz in ref[ORIENTATION].split('\\')]
      x = sliceAxes[:3]
      y = sliceAxes[3:]
      scanAxis = self.cross(x,y)
      scanOrigin = [float(zz) for zz in ref[POSITION].split('\\')]

      #
      # for each file in series, calculate the distance along
      # the scan axis, sort files by this
      #
      sortList = []
      for file in volume.files:
        position = [float(zz) for zz in positions[file].split('\\')]
        vec = self.difference(position, scanOrigin)
        dist = self.dot(vec, scanAxis)
        sortList.append((file, dist))

      sortedFiles = sorted(sortList, key=lambda x: x[1])
      distances = {}
      volume.files = []
      for file,dist in sortedFiles:
        volume.files.append(file)
        distances[file] = dist

      #
      # confirm equal spacing between slices
      # - use variable 'epsilon' to determine the tolerance
      #
      spaceWarnings = 0
      if len(volume.files) > 1:
        file0 = volume.files[0]
        file1 = volume.files[1]
        dist0 = distances[file0]
        dist1 = distances[file1]
        spacing0 = dist1 - dist0
        n = 1
        for fileN in volume.files[1:]:
          fileNminus1 = volume.files[n-1]
          distN = distances[fileN]
          distNminus1 = distances[fileNminus1]
          spacingN = distN - distNminus1
          spaceError = spacingN - spacing0
          if abs(spaceError) > self.epsilon:
            spaceWarnings += 1
            volume.warning = "Images are not equally spaced (a difference of %g in spacings was detected).  Slicer will load this series as if it had a spacing of %g.  Please use caution." % (spaceError, spacing0)
            break
          n += 1

      # TODO: issue space warnings dialog?
      if spaceWarnings != 0:
        print("Geometric issues were found with %d of the series.  Please use caution." % spaceWarnings)
      return self.volumes

  # 
  # math utilities for processing dicom volumes
  #
  def cross(self, x, y):
    return [x[1] * y[2] - x[2] * y[1],
            x[2] * y[0] - x[0] * y[2],
            x[0] * y[1] - x[1] * y[0]]

  def difference(self, x, y):
    return [x[0] - y[0], x[1] - y[1], x[2] - y[2]]

  def dot(self, x, y):
    return x[0] * y[0] + x[1] * y[1] + x[2] * y[2]


class DICOMExporter(object):
  """Code to export slicer data to dicom database
  TODO: delete temp directories and files
  """

  def __init__(self,studyUID,volumeNode=None,parameters=None):
    self.studyUID = studyUID
    self.volumeNode = volumeNode
    self.parameters = parameters
    self.referenceFile = None
    self.sdbFile = None

  def parametersFromStudy(self,studyUID=None):
    """Return a dictionary of the required conversion parameters
    based on the studyUID found in the dicom dictionary (empty if
    not well defined"""
    if not studyUID:
      studyUID = self.studyUID

    # TODO: we should install dicom.dic with slicer and use it to 
    # define the tag to name mapping
    tags = {
        "0010,0010": "Patient Name",
        "0010,0020": "Patient ID",
        "0010,4000": "Patient Comments",
        "0020,0010": "Study ID",
        "0008,0020": "Study Date",
        "0008,1030": "Study Description",
        "0008,0060": "Modality",
        "0008,0070": "Manufacturer",
        "0008,1090": "Model",
    }
    seriesNumbers = []
    p = {}
    if studyUID:
      series = slicer.app.dicomDatabase().seriesForStudy(studyUID)
      # first find a unique series number
      for serie in series:
        files = slicer.app.dicomDatabase().filesForSeries(serie)
        if len(files):
          slicer.app.dicomDatabase().loadFileHeader(files[0])
          dump = slicer.app.dicomDatabase().headerValue('0020,0011')
          try:
            value = dump[dump.index('[')+1:dump.index(']')]
            seriesNumbers.append(int(value))
          except ValueError:
            pass
      for i in xrange(len(series)+1):
        if not i in seriesNumbers:
          p['Series Number'] = i
          break

      # now find the other values from any file (use first file in first series)
      if len(series):
        p['Series Number'] = str(len(series)+1) # doesn't need to be unique, but we try
        files = slicer.app.dicomDatabase().filesForSeries(series[0])
        if len(files):
          self.referenceFile = files[0]
          slicer.app.dicomDatabase().loadFileHeader(self.referenceFile)
          for tag in tags.keys():
            dump = slicer.app.dicomDatabase().headerValue(tag)
            try:
              value = dump[dump.index('[')+1:dump.index(']')]
            except ValueError:
              value = "Unknown"
            p[tags[tag]] = value
    return p

  def progress(self,string):
    # TODO: make this a callback for a gui progress dialog
    print(string)

  def export(self, parameters=None):
    if not parameters:
      parameters = self.parameters
    if not parameters:
      parameters = self.parametersFromStudy()
    if self.volumeNode:
      success = self.createDICOMFilesForVolume(parameters)
    else:
      success = self.createDICOMFileForScene(parameters)
    if success:
      self.addFilesToDatabase()
    return success

  def createDICOMFilesForVolume(self, parameters):
    """
    Export the volume data using the ITK-based utility
    TODO: confirm that resulting file is valid - may need to change the CLI
    to include more parameters or do a new implementation ctk/DCMTK
    See:
    http://sourceforge.net/apps/mediawiki/gdcm/index.php?title=Writing_DICOM
    TODO: add more parameters to the CLI and/or find a different
    mechanism for creating the DICOM files
    """
    cliparameters = {}
    cliparameters['patientName'] = parameters['Patient Name']
    cliparameters['patientID'] = parameters['Patient ID']
    cliparameters['patientComments'] = parameters['Patient Comments']
    cliparameters['studyID'] = parameters['Study ID']
    cliparameters['studyDate'] = parameters['Study Date']
    cliparameters['studyDescription'] = parameters['Study Description']
    cliparameters['modality'] = parameters['Modality']
    cliparameters['manufacturer'] = parameters['Manufacturer']
    cliparameters['model'] = parameters['Model']
    cliparameters['seriesDescription'] = parameters['Series Description']
    cliparameters['seriesNumber'] = parameters['Series Number']

    cliparameters['inputVolume'] = self.volumeNode.GetID()

    self.dicomDirectory = tempfile.mkdtemp('', 'dicomExport', slicer.app.temporaryPath)
    cliparameters['dicomDirectory'] = self.dicomDirectory

    # 
    # run the task (in the background)
    # - use the GUI to provide progress feedback
    # - use the GUI's Logic to invoke the task
    #
    if not hasattr(slicer.modules, 'createdicomseries'):
      return False
    dicomWrite = slicer.modules.createdicomseries
    cliNode = slicer.cli.run(dicomWrite, None, cliparameters, wait_for_completion=True)
    return cliNode != None


  def createDICOMFileForScene(self, parameters):
    """
    Export the scene data:
    - first to a directory using the utility in the mrmlScene
    - create a zip file using python utility
    - create secondary capture based on the sample dataset
    - add the zip file as a private creator tag
    TODO: confirm that resulting file is valid - may need to change the CLI
    to include more parameters or do a new implementation ctk/DCMTK
    See:
    http://sourceforge.net/apps/mediawiki/gdcm/index.php?title=Writing_DICOM
    """

    # set up temp directories and files
    self.dicomDirectory = tempfile.mkdtemp('', 'dicomExport', slicer.app.temporaryPath)
    self.sceneDirectory = os.path.join(self.dicomDirectory,'scene')
    os.mkdir(self.sceneDirectory) # known to be unique
    self.imageFile = os.path.join(self.dicomDirectory, "scene.jpg")
    self.zipFile = os.path.join(self.dicomDirectory, "scene.zip")
    self.dumpFile = os.path.join(self.dicomDirectory, "dicom.dump")
    self.sdbFile = os.path.join(self.dicomDirectory, "SlicerDataBundle.dcm")

    # get the screen image
    self.progress('Saving Image...')
    pixmap = qt.QPixmap.grabWidget(slicer.util.mainWindow())
    pixmap.save(self.imageFile)
    imageReader = vtk.vtkJPEGReader()
    imageReader.SetFileName(self.imageFile)
    imageReader.Update()

    # save the scene to the temp dir
    self.progress('Saving Scene...')
    appLogic = slicer.app.applicationLogic()
    appLogic.SaveSceneToSlicerDataBundleDirectory(self.sceneDirectory, imageReader.GetOutput())

    # make the zip file
    self.progress('Making zip...')
    zip = zipfile.ZipFile( self.zipFile, "w", zipfile.ZIP_DEFLATED )
    start = len(self.sceneDirectory) + 1
    for root, subdirs, files in os.walk(self.sceneDirectory):
      for f in files:
        filePath = os.path.join(root,f)
        archiveName = filePath[start:]
        zip.write(filePath, archiveName)
    zip.close()
    zipSize = os.path.getsize(self.zipFile)

    # now create the dicom file 
    # - create the dump (capture stdout)
    # cmd = "dcmdump --print-all --write-pixel %s %s" % (self.dicomDirectory, self.referenceFile)
    self.progress('Making dicom reference file...')
    if not self.referenceFile:
      self.parametersFromStudy()
    args = ['--print-all', '--write-pixel', self.dicomDirectory, self.referenceFile]
    dump = DICOMLib.DICOMCommand('dcmdump', args).start()

    # append this to the dumped output and save the result as self.dicomDirectory/dcm.dump
    #with %s as self.zipFile and %d being its size in bytes
    zipSizeString = "%d" % zipSize
    candygram = """(cadb,0010) LO [3D Slicer Candygram]                    #  20, 1 PrivateCreator
(cadb,1008) IS %s                                       #  %d, 1 Unknown Tag & Data
(cadb,1010) OB =%s                                      #  %d, 1 Unknown Tag & Data
""" % (zipSizeString, len(zipSizeString), self.zipFile, zipSize)

    dump = dump + candygram

    fp = open('%s/dump.dcm' % self.dicomDirectory, 'w')
    fp.write(dump)
    fp.close()

    # cmd = "dump2dcm %s/dump.dcm %s/template.dcm" % (self.dicomDirectory, self.dicomDirectory)
    self.progress('Encapsulating Scene in DICOM Dump...')
    args = ['%s/dump.dcm' % self.dicomDirectory, '%s/template.dcm' % self.dicomDirectory]
    DICOMLib.DICOMCommand('dump2dcm', args).start()

    # now create the SC data set
    # cmd = "img2dcm -k 'InstanceNumber=1' -k 'SeriesDescription=Slicer Data Bundle' -df %s/template.dcm %s %s" % (self.dicomDirectory, self.imageFile, self.sdbFile)
    args = ['-k', 'InstanceNumber=1', '-k', 'SeriesDescription=Slicer Data Bundle',
      '-df', '%s/template.dcm' % self.dicomDirectory,
      self.imageFile, self.sdbFile]
    self.progress('Creating DICOM Binary File...')
    DICOMLib.DICOMCommand('img2dcm', args).start()
    self.progress('Done')
    return True


  def addFilesToDatabase(self):
    indexer = ctk.ctkDICOMIndexer()
    destinationDir = os.path.dirname(slicer.app.dicomDatabase().databaseFilename)
    if self.sdbFile:
      files = [self.sdbFile]
    else:
      files = glob.glob('%s/*' % self.dicomDirectory)
    for file in files: 
      indexer.addFile( slicer.app.dicomDatabase(), file, destinationDir )
      slicer.util.showStatusMessage("Loaded: %s" % file, 1000)

# TODO: turn these into unit tests
tests = """
  dump = DICOMLib.DICOMCommand('dcmdump', ['/media/extra650/data/CTC/JANCT000/series_2/instance_706.dcm']).start()
  
  id = slicer.app.dicomDatabase().studiesForPatient('2')[0]
  e = DICOMLib.DICOMExporter(id)
  e.export()
"""
