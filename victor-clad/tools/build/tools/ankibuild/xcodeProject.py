#!/usr/bin/python

import sys
import io
import re
import json
from collections import OrderedDict
import hashlib

from os import listdir
import os.path

class XcodeProjectParser(object):

  def __init__(self, string):
    self.words = re.split('\s', string)
    self.index = 0

  def printLocalWords(self, index):
      print " ".join(self.words[max(0,self.index - 5) : min(self.index + 5, len(self.words))])

  def DeserializeProject(self):
    result = None
    # first line is // !$*UTF8*$!
    self.index = 2;
    try:
      result = self.Deserialize(XcodeProject)
    except:
      print("Exception Thrown Parsing around line:")
      self.printLocalWords(self.index);
      raise

    #reset index
    self.index = 0
    return result  

  def Deserialize(self, t):
    self.chompWhitespace()
    result = None;
    if (t is XcodeProjectObjectsBody):
      result = self.DeserializeXcodeProjectObjectsBody()
    elif t is not None and issubclass(t, IXcodeObject):
      result = self.DeserializeXcodeObject(t)
    elif t is not None and (issubclass(t,XcodeString) or (t is XcodeString)):
      result = self.DeserializeXcodeString(t)
    elif (t is float) or (t is int):
      result = self.DeserializeNumber(t)
    elif (t is list):
      result = self.DeserializeList()
    elif (t is OrderedDict):
      result = self.DeserializeDictionary()        
    else:
      result = self.TryParseObject()

    self.chompWhitespace()
    return result

  def chompWhitespace(self):
    wordLen = len(self.words)
    while self.index < wordLen and len(self.words[self.index]) == 0:
      self.index += 1


  def isFloat(self, value):
    try:
      float(value)
      return True
    except:
      return False

  def typeSwitch(self, t):
    return {
      "PBXBuildFile": PBXBuildFile,
      "PBXContainerItemProxy": PBXContainerItemProxy,
      "PBXCopyFilesBuildPhase": PBXCopyFilesBuildPhase,
      "PBXFileReference": PBXFileReference,
      "PBXFrameworksBuildPhase": PBXFrameworksBuildPhase,
      "PBXGroup": PBXGroup,
      "PBXNativeTarget": PBXNativeTarget,
      "PBXProject": PBXProject,
      "PBXResourcesBuildPhase": PBXResourcesBuildPhase,
      "PBXShellScriptBuildPhase": PBXShellScriptBuildPhase,
      "PBXSourcesBuildPhase": PBXSourcesBuildPhase,
      "PBXTargetDependency": PBXTargetDependency,
      "PBXVariantGroup": PBXVariantGroup,
      "XCBuildConfiguration": XCBuildConfiguration,
      "XCConfigurationList": XCConfigurationList
    }[t];    

  def DeserializeXcodeProjectObjectsBody(self):
    #first char is {
    self.words[self.index] = self.words[self.index][1:]

    self.chompWhitespace()

    body = XcodeProjectObjectsBody()

    currentList = None
    currentType = None

    self.chompWhitespace()

    while not self.words[self.index].startswith('}'):

      if self.words[self.index].startswith("/*"):
        if self.words[self.index + 1] == "Begin":
          typeName = self.words[self.index + 2]

          fieldName = '{0}Section'.format(typeName)

          currentType = self.typeSwitch(typeName)

          currentList = getattr(body,fieldName)        
        else:

          currentList = None
          currentType = None
        self.index += 5

        self.chompWhitespace()
        continue

      currentList.append(self.DeserializeNamedVariable(FileId, currentType))

      self.chompWhitespace()
    
    self.index += 1

    return body

  def DeserializeXcodeString(self, t):

    string = t()

    value = ""
    if self.words[self.index].startswith('"'):
      word = ""
      first = True
      while True:
        word = self.words[self.index]
        self.index += 1

        if len(word) == 0:
          value = "{0} ".format(value)
          continue

        closeQuote = -1
        if first:
          closeQuote = 0
        lastCloseQuote = -1

        while True:
          closeQuote = word.find('"', closeQuote + 1)
          if closeQuote == -1:
            break
          escaped = False
          i = closeQuote - 1
          while i >= 0 and word[i] == '\\':
            escaped = not escaped
            i -= 1

          if not escaped:
            lastCloseQuote = closeQuote

        first = False

        if lastCloseQuote != -1:
          value = '{0} {1}'.format(value, word[0:lastCloseQuote + 1])
          break
        else:
          value = '{0} {1}'.format(value, word)

      value = json.loads(value)
    else:
      value = self.words[self.index].rstrip(',').rstrip(';')
      self.index += 1

    string.Value = value

    #check if this string has a comment
    if self.words[self.index][:2] == "/*":
      comment = ''
      self.index += 1

      while True:
        word = self.words[self.index]
        self.index += 1

        if word[:2] == "*/":
          break

        comment = '{0} {1}'.format(comment, word)

      #we put an extra space on the start we should strip off
      string.SetComment(comment[1:])

    return string


  def DeserializeNumber(self, t):
    word = self.words[self.index].rstrip(',').rstrip(';')
    self.index += 1

    return t(word)

  def TryParseObject(self):
    word = self.words[self.index]
    if self.isFloat(word):
      t = int
      if '.' in word:
        t = float
      return self.DeserializeNumber(t)
    elif word.startswith('('):
      return self.DeserializeList()
    elif word.startswith('{'):
      return self.DeserializeDictionary()
    else:
      return self.DeserializeXcodeString(FileId)

  def DeserializeList(self, elementType = None):
    l = []

    self.words[self.index] = self.words[self.index][1:]

    self.chompWhitespace()

    while not self.words[self.index].startswith(')'):
      l.append(self.Deserialize(elementType))

    # the last char is )
    self.index += 1

    return l;

  def DeserializeDictionary(self, keyType = None, elementType = None):
    d = OrderedDict()

    #first char is {
    self.words[self.index] = self.words[self.index][1:]

    self.chompWhitespace()

    if keyType is None:
      keyType = XcodeString

    while not self.words[self.index].startswith('}'):

      k = self.Deserialize(keyType)

      # next is "="
      self.index += 1

      val = self.Deserialize(elementType)

      d[k] = val

      # the last char is }
    self.index += 1

    return d;

  def DeserializeNamedVariable(self, keyType = None, elementType = None):
    obj = XcodeNamedVariable()    

    self.chompWhitespace()

    if keyType is None:
      keyType = XcodeString

    k = self.Deserialize(keyType)

    # next is "="
    self.index += 1

    val = self.Deserialize(elementType)

    obj.Name = k
    obj.Value = val

    return obj


  def DeserializeXcodeObject(self, t):
    obj = t();

    #first char is {
    self.words[self.index] = self.words[self.index][1:]

    self.chompWhitespace()

    while not self.words[self.index].startswith('}'):

      fieldName = self.words[self.index]
      self.index+=1

      fieldVal = getattr(obj,fieldName)

      fieldType = type(fieldVal)

      # next is "="
      self.index += 1

      val = self.Deserialize(fieldType)

      setattr(obj, fieldName, val)

    # the last char is }
    self.index += 1

    return obj



class XcodeProjectSerializer(object):
  def __init__(self, stream):
    self.stream = stream

  def SerializeProject(self, obj):
    self.stream.write(u'// !$*UTF8*$!\n')
    self.Serialize(u"", obj)
    self.stream.write(u'\n')

  def Serialize(self, indent, obj, oneLine = False):
    t = type(obj)
    if (t is XcodeProjectObjectsBody):
      self.SerializeXcodeProjectObjectsBody(indent, obj)
    elif (t is XcodeNamedVariable):
      self.SerializeNamedVariable(indent, obj, oneLine)
    elif t is not None and issubclass(t, IXcodeObject):      
      self.SerializeXcodeObject(indent, obj, oneLine)
    elif (t is XcodeNamedVariable):
      self.SerializeNamedVariable(indent, obj, oneLine)
    elif t is not None and (issubclass(t,XcodeString) or (t is XcodeString)): 
      self.stream.write(unicode(obj.ToString()))
    elif (t is list):
      self.SerializeList(indent, obj, oneLine)
    elif (t is OrderedDict):
      self.SerializeDictionary(indent, obj, oneLine)
    elif (t is float) or (t is int):
      self.stream.write(u'{0}'.format(obj))


  def SerializeXcodeProjectObjectsBody(self, indent, obj):
    fields = obj.getFields()

    self.stream.write(u"{\n")
    subIndent = u'\t{0}'.format(indent)
    for field in fields:

      sectionName = field[:-7] #strip off 'Section'

      l = getattr(obj,field)

      if (l and len(l) > 0):
        self.stream.write(u'\n')
        self.stream.write(u'/* Begin {0} section */\n'.format(sectionName))
        for item in l:
          self.Serialize(subIndent, item)
        self.stream.write(u'/* End {0} section */\n'.format(sectionName))

    self.stream.write(u'{0}{1}'.format(indent, '}'))

  def SerializeXcodeObject(self, indent, obj, oneLine):
    if (hasattr(obj, "isOneLine")):
      oneLine = oneLine or obj.isOneLine()

    fields = obj.getFields()
    subIndent = u'\t{0}'.format(indent)
    self.stream.write(u'{')

    separator = u"\n"
    if oneLine:
      separator = u" "
    else:
      self.stream.write(separator)

    for field in fields:
      value = getattr(obj, field)

      if value is None:
        continue

      if not oneLine:
        self.stream.write(subIndent)

      self.stream.write(unicode(field))
      self.stream.write(u" = ")
      self.Serialize(subIndent, value, oneLine)
      self.stream.write(u';')
      self.stream.write(separator)

    if not oneLine:
      self.stream.write(indent)
    self.stream.write(u'}')

  def SerializeDictionary(self, indent, obj, oneLine):    

    subIndent = u'\t{0}'.format(indent)
    self.stream.write(u'{')

    separator = u"\n"
    if oneLine:
      separator = u" "
    else:
      self.stream.write(separator)

    for field, value in obj.items():
      if value is None:
        continue

      if not oneLine:
        self.stream.write(subIndent)

      self.Serialize(subIndent, field, oneLine)
      self.stream.write(u" = ")
      self.Serialize(subIndent, value, oneLine)
      self.stream.write(u';')
      self.stream.write(separator)

    if not oneLine:
      self.stream.write(indent)
    self.stream.write(u'}')

  def SerializeList(self, indent, obj, oneLine):

    subIndent = u'\t{0}'.format(indent)
    self.stream.write(u'(')

    separator = u"\n"
    if oneLine:
      separator = u" "
    else:
      self.stream.write(separator)

    for item in obj:
      if not oneLine:
        self.stream.write(subIndent)
      self.Serialize(subIndent, item, oneLine)
      self.stream.write(u',')
      self.stream.write(separator)

    if not oneLine:
      self.stream.write(indent)

    self.stream.write(u')')

  def SerializeNamedVariable(self, indent, obj, oneLine):
    self.stream.write(indent)
    self.stream.write(unicode(obj.Name.ToString()))
    self.stream.write(u" = ")
    self.Serialize(indent, obj.Value, oneLine)
    self.stream.write(u";\n")



class XcodeString(object):
  def __init__(self, value = '', comment = None):
    self.Value = value
    self.SetComment(comment)

  def __eq__(self, other):
    if(other is None):
      return False;
    if (type(other) is XcodeString or issubclass(type(other), XcodeString)):
      return self.Value == other.Value;
    return False

  def __ne__(self, other):
    return not self.__eq__(other)

  def __lt__(self, other):
    if(other is None):
      return False;
    if (type(other) is XcodeString or issubclass(type(other), XcodeString)):
      return self.Value < other.Value;    
    return False

  def __hash__(self):
    return self.Value.__hash__()
    

  def GetComment(self):
    return self.Comment

  def SetComment(self, comment):
    self.Comment = comment

  def ToString(self):
    output = ""

    if self.Value is None:
      return output

    match = re.search('[ \\$\\(\\)\\+"\\\\;,\\*\\<\\>-]', self.Value)

    if len(self.Value) == 0 or match is not None:
      output = json.dumps(self.Value).replace("\n", "\\n").replace("\t", "\\t")
    else:
      output = self.Value

    comment = self.GetComment()
    if comment is not None and len(comment) > 0:
      output += " /* {0} */".format(comment)

    return output

class FileId(XcodeString):
  def __init__(self, value = '', comment = None):
    XcodeString.__init__(self, value, comment)

  def GetComment(self):
    if self.Group is None or len(self.Group) == 0:
      return self.FileName
    else:
      return "{0} in {1}".format(self.FileName, self.Group)

  def SetComment(self, comment):
    if comment is not None:
      m = re.match("(.*) in (.*)", comment)
      if m:
        self.FileName = m.group(1)
        self.Group = m.group(2)
      else:
        self.FileName = comment
        self.Group = None
    else:
      self.FileName = None
      self.Group = None

class IXcodeObject(object):
  
  def getFields(self):
    return [a for a in dir(self) if not a.startswith('__') and not callable(getattr(self,a))]


class XcodeProject(IXcodeObject):
  def __init__(self):
    self.archiveVersion = 0
    self.classes = OrderedDict()
    self.objectVersion = 0
    self.objects = XcodeProjectObjectsBody()
    self.rootObject = FileId()

class XcodeProjectObjectsBody(IXcodeObject):
  def __init__(self):
    self.PBXBuildFileSection = []
    self.PBXContainerItemProxySection = []
    self.PBXCopyFilesBuildPhaseSection = []
    self.PBXFileReferenceSection = []
    self.PBXFrameworksBuildPhaseSection = []
    self.PBXGroupSection = []
    self.PBXNativeTargetSection = []
    self.PBXProjectSection = []
    self.PBXResourcesBuildPhaseSection = []
    self.PBXShellScriptBuildPhaseSection = []
    self.PBXSourcesBuildPhaseSection = []
    self.PBXTargetDependencySection = []
    self.PBXVariantGroupSection = []
    self.XCBuildConfigurationSection = []
    self.XCConfigurationListSection = []

class XcodeNamedVariable(IXcodeObject):
  def __init__(self, Name = XcodeString(), Value = None):
    self.Name = Name
    self.Value = Value

class PBXObject(IXcodeObject):
  def __init__(self):
    self.isa = XcodeString(self.__class__.__name__)

  def getFields(self):
    fields = IXcodeObject.getFields(self)
    # move isa to the front
    fields.remove("isa")
    fields.insert(0, "isa")
    return fields

  def isOneLine(self):
    return False

class PBXBuildFile(PBXObject):
  def __init__(self):
    PBXObject.__init__(self)
    self.fileRef = FileId()
    self.settings = None

  def isOneLine(self):
    return True

class PBXFileReference(PBXObject):
  def __init__(self):
    PBXObject.__init__(self)
    self.fileEncoding = None
    self.lastKnownFileType = None
    self.explicitFileType = None
    self.includeInIndex = None
    self.name = None
    self.path = None
    self.sourceTree = None

  def isOneLine(self):
    return True

class PBXContainerItemProxy(PBXObject):
  def __init__(self):
    PBXObject.__init__(self)
    self.containerPortal = FileId()
    self.proxyType = 0
    self.remoteGlobalIDString = XcodeString()
    self.remoteInfo = XcodeString()

class PBXBuildPhase(PBXObject):
  def __init__(self):
    PBXObject.__init__(self)
    self.buildActionMask = 0
    self.files = []
    self.runOnlyForDeploymentPostprocessing = 0

class PBXCopyFilesBuildPhase(PBXBuildPhase):
  def __init__(self):
    PBXBuildPhase.__init__(self)
    self.dstPath = XcodeString()
    self.dstSubfolderSpec = 0
    self.name = XcodeString()

class PBXFrameworksBuildPhase(PBXBuildPhase):
  def __init__(self):
    PBXBuildPhase.__init__(self)

class PBXGroup(PBXObject):
  def __init__(self):
    PBXObject.__init__(self)
    self.children = []
    self.name = XcodeString()
    self.path = None
    self.sourceTree = XcodeString()

class PBXNativeTarget(PBXObject):
  def __init__(self):
    PBXObject.__init__(self)
    self.buildConfigurationList = FileId()
    self.buildPhases = []
    self.buildRules = []
    self.dependencies = []
    self.name = XcodeString()
    self.productName = XcodeString()
    self.productReference = FileId()
    self.productType = XcodeString()

class PBXProject(PBXObject):
  def __init__(self):
    PBXObject.__init__(self)
    self.attributes = OrderedDict()
    self.buildConfigurationList = FileId()
    self.compatibilityVersion = XcodeString()
    self.developmentRegion = XcodeString()
    self.hasScannedForEncodings = 0
    self.knownRegions = []
    self.mainGroup = FileId()
    self.projectDirPath = XcodeString()
    self.projectRoot = XcodeString()
    self.targets = []

class PBXResourcesBuildPhase(PBXBuildPhase):
  def __init__(self):
    PBXBuildPhase.__init__(self)

class PBXShellScriptBuildPhase(PBXBuildPhase):
  def __init__(self):
    PBXBuildPhase.__init__(self)
    self.inputPaths = []
    self.name = XcodeString()
    self.outputPaths = []
    self.shellPath = XcodeString()
    self.shellScript = XcodeString()
    self.showEnvVarsInLog = 0

class PBXSourcesBuildPhase(PBXBuildPhase):
  def __init__(self):
    PBXBuildPhase.__init__(self)

class PBXTargetDependency(PBXObject):
  def __init__(self):
    PBXObject.__init__(self)
    self.target = FileId()
    self.targetProxy = FileId()

class PBXVariantGroup(PBXObject):
  def __init__(self):
    PBXObject.__init__(self)
    self.children = []
    self.name = XcodeString()
    self.sourceTree = XcodeString()

class XCBuildConfiguration(PBXObject):
  def __init__(self):
    PBXObject.__init__(self)
    self.baseConfigurationReference = FileId()
    self.buildSettings = OrderedDict()
    self.name = XcodeString()

class XCConfigurationList(PBXObject):
  def __init__(self):
    PBXObject.__init__(self)
    self.buildConfigurations = []
    self.defaultConfigurationIsVisible = 0
    self.defaultConfigurationName = XcodeString()

class FileTypeDefinition(object):
  def __init__(self, Extension = None, DefaultSourceTree = None, Category = None, LastKnownFileType = None, FileEncoding = None, ExcludeFromBuildPhase = False):
    self.Extension = Extension
    self.DefaultSourceTree = DefaultSourceTree
    self.Category  = Category
    self.LastKnownFileType = LastKnownFileType
    self.FileEncoding = FileEncoding
    self.ExcludeFromBuildPhase = ExcludeFromBuildPhase

class XcodeProjectUtility(object):

  def __init__(self):
    self.XcodeCategorySources = "Sources"
    self.XcodeCategoryResources = "Resources"
    self.XcodeCategoryFrameworks = "Frameworks"

    self.FileTypeList = [
      # Frameworks
      FileTypeDefinition(
        Extension = "a",
        DefaultSourceTree = "SOURCE_ROOT",
        Category = self.XcodeCategoryFrameworks,
        LastKnownFileType = "archive.ar",
      ),
      FileTypeDefinition(
        Extension = "framework",
        DefaultSourceTree = "SDKROOT",
        Category = self.XcodeCategoryFrameworks,
        LastKnownFileType = "wrapper.framework"
      ),
      FileTypeDefinition(
        Extension = "dylib",
        DefaultSourceTree = "SDKROOT",
        Category = self.XcodeCategoryFrameworks,
        LastKnownFileType = "compiled.mach-o.dylib"
      ),
      # Sources
      FileTypeDefinition(
        Extension = "s",
        DefaultSourceTree = "<group>",
        Category = self.XcodeCategorySources,
        LastKnownFileType = "sourcecode.asm",
        FileEncoding = 4
      ),
      FileTypeDefinition(
        Extension = "pch",
        DefaultSourceTree = "<group>",
        Category = self.XcodeCategorySources,
        LastKnownFileType = "sourcecode.c.h",
        FileEncoding = 4,
        ExcludeFromBuildPhase = True
      ),
      FileTypeDefinition(
        Extension = "h",
        DefaultSourceTree = "<group>",
        Category = self.XcodeCategorySources,
        LastKnownFileType = "sourcecode.c.h",
        FileEncoding = 4,
        ExcludeFromBuildPhase = True
      ),
      FileTypeDefinition(
        Extension = "m",
        DefaultSourceTree = "<group>",
        Category = self.XcodeCategorySources,
        LastKnownFileType = "sourcecode.c.objc",
        FileEncoding = 4
      ),
      FileTypeDefinition(
        Extension = "mm",
        DefaultSourceTree = "<group>",
        Category = self.XcodeCategorySources,
        LastKnownFileType = "sourcecode.cpp.objc",
        FileEncoding = 4
      ),
      FileTypeDefinition(
        Extension = "cpp",
        DefaultSourceTree = "<group>",
        Category = self.XcodeCategorySources,
        LastKnownFileType = "sourcecode.cpp.cpp",
        FileEncoding = 4
      ),
      # Resources
      FileTypeDefinition(
        Extension = "png",
        DefaultSourceTree = "<group>",
        Category = self.XcodeCategoryResources,
        LastKnownFileType = "image.png",
      ),
      FileTypeDefinition(
        Extension = "xib",
        DefaultSourceTree = "<group>",
        Category = self.XcodeCategoryResources,
        LastKnownFileType = "file.xib",
        FileEncoding = 4
      ),
      FileTypeDefinition(
        Extension = "plist",
        DefaultSourceTree = "<group>",
        Category = self.XcodeCategoryResources,
        LastKnownFileType = "text.plist.xml",
        FileEncoding = 4
      ),
      FileTypeDefinition(
        Extension = "config",
        DefaultSourceTree = "<group>",
        Category = self.XcodeCategoryResources,
        LastKnownFileType = "text.xml",
      ),
      FileTypeDefinition(
        Extension = "xml",
        DefaultSourceTree = "<group>",
        Category = self.XcodeCategoryResources,
        LastKnownFileType = "text.xml",
      ),
      FileTypeDefinition(
        Extension = "dat",
        DefaultSourceTree = "<group>",
        Category = self.XcodeCategoryResources,
        LastKnownFileType = "file",
      ),
      FileTypeDefinition(
        Extension = "map",
        DefaultSourceTree = "<group>",
        Category = self.XcodeCategoryResources,
        LastKnownFileType = "sourcecode.module-map",
      ),
      FileTypeDefinition(
        Extension = "strings",
        DefaultSourceTree = "<group>",
        Category = self.XcodeCategoryResources,
        LastKnownFileType = "text.plist.strings",
        FileEncoding = 4
      ),
      FileTypeDefinition(
        Extension = "xcassets",
        DefaultSourceTree = "<group>",
        Category = self.XcodeCategoryResources,
        LastKnownFileType = "folder.assetcatalog"
      ),
      FileTypeDefinition(
        Extension = "xcconfig",
        DefaultSourceTree = "<group>",
        Category = self.XcodeCategoryResources,
        LastKnownFileType = "text.xcconfig"
      )          
      # TODO: Add any FileTypes that I've missed
    ]

  def CalculateMD5Hash(self, string):
    m = hashlib.md5()
    m.update(string.encode('utf-8'))
    return m.hexdigest()

  def NewGuid(self, key):
    return self.CalculateMD5Hash(key)[0:24].upper()

  def NewFileId(self, path, name, group = None):

    comment = "{0} in {1}".format(name, group)
    if group is None:
      comment = name
      group = ''

    if name is None:
      comment = None
      name = ''

    return FileId(self.NewGuid('{0}%{1}%{2}'.format(path,name,group)), comment)
  def XcodeStringOrNone(self, string):
    if string is None:
      return None
    else:
      return XcodeString('{0}'.format(string))

  def EnsureFolderExists(self, project, groupId, folderPath, relativePath):
    groupPath = folderPath.split('/')

    index = 0
    lastFolderId = groupId
    while index < len(groupPath):
      folder = groupPath[index]
      index += 1
      if len(folder) == 0:
        continue

      group = self.GetEntry(project.objects.PBXGroupSection, lastFolderId)

      fileId = self.GetFileId(group.children, folder)

      if fileId is None:

        fileId = self.NewFileId(relativePath, folder)
        newGroup = PBXGroup()
        newGroup.name = self.XcodeStringOrNone(folder)
        newGroup.sourceTree = XcodeString("<group>")

        self.AppendNamedValue(project.objects.PBXGroupSection, XcodeNamedVariable(fileId, newGroup))
        group.children.append(fileId)

      lastFolderId = fileId.Value

    return self.GetEntry(project.objects.PBXGroupSection, lastFolderId)

  def GetExistingFolder(self, project, groupId, relativePath):
    groupPath = relativePath.split('/')

    index = 0
    lastFolderId = groupId
    while index < len(groupPath):
      folder = groupPath[index]
      index += 1
      if len(folder) == 0:
        continue

      group = self.GetEntry(project.objects.PBXGroupSection, lastFolderId)

      fileId = self.GetFileId(group.children, folder)

      if fileId is None:
        return None

      lastFolderId = fileId.Value

    return self.GetEntry(project.objects.PBXGroupSection, lastFolderId)

  def AddFolder(self, project, projectRootPath, fileSystemFolderPath, projectFolderPath, exclusions = None):
    projectRoot = self.GetOnly(project.objects.PBXProjectSection).mainGroup

    relativePath = os.path.relpath(fileSystemFolderPath, projectRootPath)

    folderGroup = self.EnsureFolderExists(project, projectRoot.Value, projectFolderPath, relativePath)

    # this should be the folder which will get the contents of our folder

    for fileName in os.listdir(fileSystemFolderPath):
      f = os.path.join(fileSystemFolderPath, fileName)
      if exclusions is not None and exclusions.search(f):
        continue

      if os.path.isfile(f):
        self.AddFileToGroup(project, projectRootPath, folderGroup, f)
      else:
        fileType, extension = self.GetFileType(f)
        if fileType is not None:
          self.AddFileToGroup(project, projectRootPath, folderGroup, f)
        else:
          folderName = os.path.basename(f)

          if not folderName.startswith('.'):
            self.AddFolder(project, projectRootPath, f, os.path.join(projectFolderPath, folderName), exclusions)

  def AddFile(self, project, projectRootPath, fileSystemPath, projectPath):
    projectRoot = self.GetOnly(project.objects.PBXProjectSection).mainGroup

    relativePath = os.path.relpath(fileSystemPath, projectRootPath)

    folderGroup = self.EnsureFolderExists(project, projectRoot.Value, os.path.dirname(projectPath), relativePath)

    self.AddFileToGroup(project, projectRootPath, folderGroup, fileSystemPath)

  def AddFileToGroup(self, project, projectRootPath, group, path):
    name = os.path.basename(path)

    if name.startswith('.'):
      return

    (fileType, extension) = self.GetFileType(name)

    if fileType is None:
      fileType = FileTypeDefinition(Category = self.XcodeCategoryResources, Extension = extension, LastKnownFileType="text")

    relativePath = os.path.relpath(path, projectRootPath)
    fileId = self.NewFileId(relativePath, name)

    group.children.append(fileId)

    fileReference = PBXFileReference()
    fileReference.fileEncoding = self.XcodeStringOrNone(fileType.FileEncoding)
    fileReference.lastKnownFileType = self.XcodeStringOrNone(fileType.LastKnownFileType)
    fileReference.path = self.XcodeStringOrNone(relativePath)
    fileReference.name = self.XcodeStringOrNone(name)
    fileReference.sourceTree = self.XcodeStringOrNone("<group>")

    self.AppendNamedValue(project.objects.PBXFileReferenceSection, XcodeNamedVariable(fileId, fileReference))

    buildFileId = self.NewFileId(relativePath, name, fileType.Category)
    buildFile = PBXBuildFile()
    buildFile.fileRef = fileId

    # special case exclude headers
    if extension != 'h' and extension != 'pch':
      self.AppendNamedValue(project.objects.PBXBuildFileSection, XcodeNamedVariable(buildFileId, buildFile))

      if not fileType.ExcludeFromBuildPhase:
        if fileType.Category == self.XcodeCategoryFrameworks:
          self.GetOnly(project.objects.PBXFrameworksBuildPhaseSection).files.append(buildFileId)
        elif fileType.Category == self.XcodeCategoryResources:
          self.GetOnly(project.objects.PBXResourcesBuildPhaseSection).files.append(buildFileId)
        elif fileType.Category == self.XcodeCategorySources:
          self.GetOnly(project.objects.PBXSourcesBuildPhaseSection).files.append(buildFileId)

  def RemoveFile(self, project, projectRootPath, path):
    relativePath = os.path.relpath(path, projectRootPath)

    frs = [x for x in project.objects.PBXFileReferenceSection if os.path.samefile(x.Value.path.Value, relativePath)]

    if(len(frs) == 0):
      print("Could not find file {0} in project".format(path))
      return

    fileReference = frs[0]

    grps = [x for x in project.objects.PBXGroupSection if fileReference.Name in x.Value.children]

    if len(grps) > 0:
      print ("File {0} not in any groups".format(fileReference.ToString()))
      group = grps[0]
      group.Value.children.remove(fileReference.Name)

    bfs = [x for x in project.objects.PBXBuildFileSection if x.Value.fileRef == fileReference.Name]

    if len(bfs) > 0:
      buildFile = bfs[0]

      project.objects.PBXBuildFileSection.remove(buildFile)
      for files in [
        self.GetOnly(project.objects.PBXResourcesBuildPhaseSection).files, 
        self.GetOnly(project.objects.PBXSourcesBuildPhaseSection).files,
        self.GetOnly(project.objects.PBXFrameworksBuildPhaseSection).files]:

        if buildFile.Name in files: 
          files.remove(buildFile.Name)



  def RemoveFileOrFolder(self, project, group, fileId):
    if fileId is None:
      return;

    group.children.remove(fileId)

    subFolder = self.GetNamedValue(project.objects.PBXGroupSection, fileId)

    if (subFolder is not None):

      i = len(subFolder.Value.children) - 1

      while i >= 0:
        child = subFolder.Value.children[i]
        self.RemoveFileOrFolder(project, subFolder.Value, child)        
        i -= 1

      project.objects.PBXGroupSection.remove(subFolder)
      return;

    fileRef = self.GetNamedValue(project.objects.PBXFileReferenceSection, fileId)

    if(fileRef is not None):
      project.objects.PBXFileReferenceSection.remove(fileRef)
    else:
      varGroup = self.GetNamedValue(project.objects.PBXVariantGroupSection, fileId)

      if(varGroup is not None):
        project.objects.PBXVariantGroupSection.remove(vgs[0])
      else:
        print("Could not find any reference to file id {0}".format(fileId.ToString()))

    bfs = [x for x in project.objects.PBXBuildFileSection if x.Value.fileRef == fileRef.Name]

    if len(bfs) > 0:
      buildFile = bfs[0]

      project.objects.PBXBuildFileSection.remove(buildFile)
      for files in [
        self.GetOnly(project.objects.PBXResourcesBuildPhaseSection).files, 
        self.GetOnly(project.objects.PBXSourcesBuildPhaseSection).files,
        self.GetOnly(project.objects.PBXFrameworksBuildPhaseSection).files]:

        if buildFile.Name in files: 
          files.remove(buildFile.Name)


  def RemoveFolder(self, project, projectFolderPath):
    projectRoot = self.GetOnly(project.objects.PBXProjectSection).mainGroup

    folderPath = projectFolderPath.rstrip('/')

    parentFolder = self.GetExistingFolder(project, projectRoot.Value, os.path.dirname(folderPath))

    if parentFolder is None:
      print("Could not find parent folder in project for {0}".format(projectFolderPath))
      return

    fileId = self.GetFileId(parentFolder.children, folderPath)

    if fileId is None:
      print ("Could not find file id for {0}".format(folderPath))

    self.RemoveFileOrFolder(project, parentFolder, fileId)


  def GetFileType(self, name):
    extension = os.path.splitext(name)[1]
    if extension is not None:
      extension = extension.lstrip('.')

      ft = [x for x in self.FileTypeList if x.Extension == extension]
      if(len(ft) > 0):
        return (ft[0], extension)
    return (None, extension)


  def GetEntry(self, list, key):
    for item in list:
      if(item.Name.Value == key or item.Name == key):
        return item.Value
    return None

  def GetNamedValue(self, list, key):
    for item in list:
      if(item.Name.Value == key or item.Name == key):
        return item
    return None

  def AppendNamedValue(self, list, item):
    i = 0;
    count = len(list)
    while(i < count and list[i].Name < item.Name): i += 1
    list.insert(i, item)

  def GetOnly(self, list):
    return list[0].Value

  def GetFileId(self, list, fileName):
    for item in list:
      if item.FileName == fileName:
        return item
    return None

  def FixupIosUnityProject(self, xcodeProjectPath, unityBuildPath, folderName = None):  
    if xcodeProjectPath is None or unityBuildPath is None:
      return
    print( "Reading {0}".format(xcodeProjectPath))
    f = open(xcodeProjectPath, 'r')
    initialString = f.read()
    f.close()


    print( "Parsing {0}".format(xcodeProjectPath))
    parser = XcodeProjectParser(initialString)

    proj = parser.DeserializeProject()

    projectName = os.path.basename(unityBuildPath)
    rootPath = os.path.dirname(os.path.dirname(xcodeProjectPath))

    if(folderName is None):
      folderName = projectName

    print("Modifying {0} by updating {1} from {2}".format(xcodeProjectPath, folderName, unityBuildPath))

    self.RemoveFolder(proj, folderName)

    self.AddFolder(proj, rootPath, os.path.join(unityBuildPath, "Classes"), "{0}/Classes".format(folderName), re.compile(".*/Native/.*\\.h"))
    self.AddFolder(proj, rootPath, os.path.join(unityBuildPath, "Libraries"), "{0}/Libraries".format(folderName), re.compile(".*/libil2cpp(/.*)?"))
    self.AddFolder(proj, rootPath, os.path.join(unityBuildPath, "Unity-iPhone"), "{0}/Unity-iPhone".format(folderName))

    self.AddFile(proj, rootPath, os.path.join(unityBuildPath, "LaunchScreen-iPhoneLandscape.png"), "{0}/LaunchScreen-iPhoneLandscape.png".format(folderName))
    self.AddFile(proj, rootPath, os.path.join(unityBuildPath, "LaunchScreen-iPhonePortrait.png"), "{0}/LaunchScreen-iPhonePortrait.png".format(folderName))
    self.AddFile(proj, rootPath, os.path.join(unityBuildPath, "LaunchScreen-iPhone.xib"), "{0}/LaunchScreen-iPhone.xib".format(folderName))

    stream = io.StringIO()

    print( "Serializing {0}".format(xcodeProjectPath))
    serializer = XcodeProjectSerializer(stream)

    serializer.SerializeProject(proj)

    finalString = stream.getvalue()

    print( "Writing {0}".format(xcodeProjectPath))
    f = open(xcodeProjectPath, 'w')    
    f.write(unicode(finalString))
    f.close()

if __name__ == "__main__":
  xcodeProjectPath = None
  unityBuildPath = None
  folderName = None

  if(len(sys.argv) == 1):
    print("Usage: ./xcodeProject.py path/to/xcode/project.pbxproj path/to/unity/build [folderName]")
    print("  deletes the unity folder from the given project file path,")
    print("  then readds it from the given unity project path")

  if (len(sys.argv) > 1):
    xcodeProjectPath = sys.argv[1]

  if(len(sys.argv) > 2):
    unityBuildPath = sys.argv[2]

  if(len(sys.argv) > 3):
    folderName = sys.argv[3]

    utility = XcodeProjectUtility()
    utility.FixupIosUnityProject(xcodeProjectPath, unityBuildPath, folderName)
