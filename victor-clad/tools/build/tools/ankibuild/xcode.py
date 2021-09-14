
import getpass
import shutil
import subprocess
import os
import plistlib
import util

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

RAW_OUTPUT = True

class XcodeWorkspace(object):
    def __init__(self, name):
        self.name = name
        self.project_name = None
        self.projects = []
        self.schemes = []
    
    @classmethod
    def generate_self(cls, project_path, derived_data_path):
        project_name = os.path.basename(project_path)
        workspace_path = os.path.join(project_path, 'project.xcworkspace')
        workspace = cls(project_name)
        workspace.add_self_project(project_name)
        workspace.generate(workspace_path, derived_data_path)
    
    def add_project(self, project_path):
        self.projects.append(project_path)
    
    def add_self_project(self, project_name):
        self.project_name = project_name
    
    def add_scheme_cmake(self, name, project_path):
    	scheme = dict(template=SCHEME_TEMPLATE_CMAKE, name=name, project_path=project_path)
    	self.schemes.append(scheme)
    
    def add_scheme_gyp(self, name, project_path):
    	scheme = dict(template=SCHEME_TEMPLATE_GYP, name=name, project_path=project_path)
    	self.schemes.append(scheme)
    
    def add_scheme_ios(self, scheme_name, project_path, mode='auto'):
        scheme = dict(template=SCHEME_TEMPLATE_IOS, name=scheme_name, mode=mode, project_path=project_path)
        self.schemes.append(scheme)
    
    def generate(self, path, derived_data_path):
        header = '<?xml version="1.0" encoding="UTF-8"?>'
        workspace_begin = '<Workspace version="1.0">'
        workspace_end = '</Workspace>'

        output = []
        output.append(header)
        output.append(workspace_begin)
        if self.project_name is not None:
            fileref = self.generate_file_ref(self.project_name, tag='self')
            output.append(fileref)
        for project in self.projects:
            fileref = self.generate_file_ref(project)
            output.append(fileref)
        output.append(workspace_end)

        # make workspace bundle path
        util.File.mkdir_p(path)

        # generate contents
        xc_contents = os.path.join(path, 'contents.xcworkspacedata')
        util.File.write(xc_contents, "\n".join(output) + "\n")
        
        # generate settings if necessary
        if derived_data_path is not None:
            self.generate_workspace_settings(path, derived_data_path)
        
        if self.schemes:
        	scheme_dir = os.path.join(path, 'xcshareddata', 'xcschemes')
        	util.File.mkdir_p(scheme_dir)
        	for scheme in self.schemes:
        		scheme_path = os.path.join(scheme_dir, '{0}.xcscheme'.format(scheme['name']))
        		scheme_data = scheme['template'].format(**scheme)
        		util.File.write(scheme_path, scheme_data)
	
    def generate_file_ref(self, path, tag='group'):
        location = "%s:%s" % (tag, path)
        xml = "<FileRef location = \"%s\"></FileRef>" % location
        return xml
    
    @classmethod
    def generate_workspace_settings(cls, workspace_path, derived_data_path):
        xc_settings = {
            'BuildLocationStyle': 'UseAppPreferences',
            'CustomBuildIntermediatesPath': 'Build/Intermediates',
            'CustomBuildLocationType': 'RelativeToDerivedData',
            'CustomBuildProductsPath': 'Build/Products',
            'DerivedDataCustomLocation': derived_data_path,
            'DerivedDataLocationStyle': 'AbsolutePath',
            'IssueFilterStyle': 'ShowActiveSchemeOnly',
            'LiveSourceIssuesEnabled': True,
            'SnapshotAutomaticallyBeforeSignificantChanges': False,
            'SnapshotLocationStyle': 'Default',
        }
        username = getpass.getuser()
        xc_user_settings = os.path.join(workspace_path, 'xcuserdata', username + '.xcuserdatad', 'WorkspaceSettings.xcsettings')
        util.File.mkdir_p(os.path.dirname(xc_user_settings))
        plistlib.writePlist(xc_settings, xc_user_settings)

def build(
    project = None,
    workspace = None,
    target = None,
    scheme = None,
    configuration = None,
    platform = None,
    simulator = False,
    scriptengine = 'il2cpp',
    other_code_sign_flags = None,
    code_sign_identity = None,
    provision_profile = None,
    buildaction = 'build'):
    
    if not project and not workspace:
        raise ValueError('You must specify either a project or workspace to xcodebuild.')

    arguments = ['xcodebuild']
    if RAW_OUTPUT:
        use_xcpretty = False
    elif util.File.evaluate(['xcpretty', '--version'], ignore_result=True):
        use_xcpretty = True
    else:
        use_xcpretty = False
        print('NOTE: For better output, install xcpretty:')
        print('`sudo gem install xcpretty`')
    
    if project:
        arguments += ['-project', os.path.abspath(project)]
    if workspace:
        arguments += ['-workspace', os.path.abspath(workspace)]
    if target:
        arguments += ['-target', target]
    if scheme:
        arguments += ['-scheme', scheme]
    if configuration:
        arguments += ['-configuration', configuration]
    if other_code_sign_flags:
        arguments += ['OTHER_CODE_SIGN_FLAGS=' + other_code_sign_flags]
    if code_sign_identity:
        arguments += ['CODE_SIGN_IDENTITY=' + code_sign_identity]
    if provision_profile:
        arguments += ['PROVISION_PROFILE=' + provision_profile]

    if platform == 'ios':
        if simulator:
            arguments += ['-sdk', 'iphonesimulator']
    
    arguments += ['-parallelizeTargets']

    arguments += ['SCRIPT_ENGINE=' + scriptengine]

    arguments += [buildaction]
    
    if use_xcpretty:
        util.File.execute(arguments, ['xcpretty', '-c'])
    else:
        util.File.execute(arguments)

SCHEME_TEMPLATE_CMAKE = '''\
<?xml version="1.0" encoding="UTF-8"?>
<Scheme
   LastUpgradeVersion = "0600"
   version = "1.3">
   <BuildAction
      parallelizeBuildables = "YES"
      buildImplicitDependencies = "YES">
      <BuildActionEntries>
         <BuildActionEntry
            buildForTesting = "YES"
            buildForRunning = "YES"
            buildForProfiling = "YES"
            buildForArchiving = "YES"
            buildForAnalyzing = "YES">
            <BuildableReference
               BuildableIdentifier = "primary"
               BlueprintIdentifier = "6F942CD0023C40FDBB818758"
               BuildableName = "ALL_BUILD"
               BlueprintName = "ALL_BUILD"
               ReferencedContainer = "container:{project_path}">
            </BuildableReference>
         </BuildActionEntry>
      </BuildActionEntries>
   </BuildAction>
   <TestAction
      selectedDebuggerIdentifier = "Xcode.DebuggerFoundation.Debugger.LLDB"
      selectedLauncherIdentifier = "Xcode.DebuggerFoundation.Launcher.LLDB"
      shouldUseLaunchSchemeArgsEnv = "YES"
      buildConfiguration = "Debug">
      <Testables>
      </Testables>
   </TestAction>
   <LaunchAction
      selectedDebuggerIdentifier = "Xcode.DebuggerFoundation.Debugger.LLDB"
      selectedLauncherIdentifier = "Xcode.DebuggerFoundation.Launcher.LLDB"
      launchStyle = "0"
      useCustomWorkingDirectory = "NO"
      buildConfiguration = "Debug"
      ignoresPersistentStateOnLaunch = "NO"
      debugDocumentVersioning = "YES"
      allowLocationSimulation = "YES">
      <MacroExpansion>
         <BuildableReference
            BuildableIdentifier = "primary"
            BlueprintIdentifier = "6F942CD0023C40FDBB818758"
            BuildableName = "ALL_BUILD"
            BlueprintName = "ALL_BUILD"
            ReferencedContainer = "container:{project_path}">
         </BuildableReference>
      </MacroExpansion>
      <AdditionalOptions>
      </AdditionalOptions>
   </LaunchAction>
   <ProfileAction
      shouldUseLaunchSchemeArgsEnv = "YES"
      savedToolIdentifier = ""
      useCustomWorkingDirectory = "NO"
      buildConfiguration = "Release"
      debugDocumentVersioning = "YES">
      <MacroExpansion>
         <BuildableReference
            BuildableIdentifier = "primary"
            BlueprintIdentifier = "6F942CD0023C40FDBB818758"
            BuildableName = "ALL_BUILD"
            BlueprintName = "ALL_BUILD"
            ReferencedContainer = "container:{project_path}">
         </BuildableReference>
      </MacroExpansion>
   </ProfileAction>
   <AnalyzeAction
      buildConfiguration = "Debug">
   </AnalyzeAction>
   <ArchiveAction
      buildConfiguration = "Release"
      revealArchiveInOrganizer = "YES">
   </ArchiveAction>
</Scheme>
'''


SCHEME_TEMPLATE_GYP = '''\
<?xml version="1.0" encoding="UTF-8"?>
<Scheme
   LastUpgradeVersion = "0620"
   version = "1.3">
   <BuildAction
      parallelizeBuildables = "YES"
      buildImplicitDependencies = "YES">
      <BuildActionEntries>
         <BuildActionEntry
            buildForTesting = "YES"
            buildForRunning = "YES"
            buildForProfiling = "YES"
            buildForArchiving = "YES"
            buildForAnalyzing = "YES">
            <BuildableReference
               BuildableIdentifier = "primary"
               BlueprintIdentifier = "6671842B2D322E791E2BFD7F"
               BuildableName = "All"
               BlueprintName = "All"
               ReferencedContainer = "container:{project_path}">
            </BuildableReference>
         </BuildActionEntry>
      </BuildActionEntries>
   </BuildAction>
   <TestAction
      selectedDebuggerIdentifier = "Xcode.DebuggerFoundation.Debugger.LLDB"
      selectedLauncherIdentifier = "Xcode.DebuggerFoundation.Launcher.LLDB"
      shouldUseLaunchSchemeArgsEnv = "YES"
      buildConfiguration = "Debug">
      <Testables>
      </Testables>
   </TestAction>
   <LaunchAction
      selectedDebuggerIdentifier = "Xcode.DebuggerFoundation.Debugger.LLDB"
      selectedLauncherIdentifier = "Xcode.DebuggerFoundation.Launcher.LLDB"
      launchStyle = "0"
      useCustomWorkingDirectory = "NO"
      buildConfiguration = "Debug"
      ignoresPersistentStateOnLaunch = "NO"
      debugDocumentVersioning = "YES"
      allowLocationSimulation = "YES">
      <MacroExpansion>
         <BuildableReference
            BuildableIdentifier = "primary"
            BlueprintIdentifier = "6671842B2D322E791E2BFD7F"
            BuildableName = "All"
            BlueprintName = "All"
            ReferencedContainer = "container:{project_path}">
         </BuildableReference>
      </MacroExpansion>
      <AdditionalOptions>
      </AdditionalOptions>
   </LaunchAction>
   <ProfileAction
      shouldUseLaunchSchemeArgsEnv = "YES"
      savedToolIdentifier = ""
      useCustomWorkingDirectory = "NO"
      buildConfiguration = "Release"
      debugDocumentVersioning = "YES">
      <MacroExpansion>
         <BuildableReference
            BuildableIdentifier = "primary"
            BlueprintIdentifier = "6671842B2D322E791E2BFD7F"
            BuildableName = "All"
            BlueprintName = "All"
            ReferencedContainer = "container:{project_path}">
         </BuildableReference>
      </MacroExpansion>
   </ProfileAction>
   <AnalyzeAction
      buildConfiguration = "Debug">
   </AnalyzeAction>
   <ArchiveAction
      buildConfiguration = "Release"
      revealArchiveInOrganizer = "YES">
   </ArchiveAction>
</Scheme>
'''

SCHEME_TEMPLATE_IOS = '''\
<?xml version="1.0" encoding="UTF-8"?>
<Scheme
   LastUpgradeVersion = "0600"
   version = "1.3">
   <BuildAction
      parallelizeBuildables = "YES"
      buildImplicitDependencies = "YES">
      <PreActions>
         <ExecutionAction
            ActionType = "Xcode.IDEStandardExecutionActionsCore.ExecutionActionType.ShellScriptAction">
            <ActionContent
               title = "Run Script"
               scriptText = "export ANKI_BUILD_MODE=&quot;{mode}&quot;"
               shellToInvoke = "/bin/bash">
            </ActionContent>
         </ExecutionAction>
      </PreActions>
      <BuildActionEntries>
         <BuildActionEntry
            buildForTesting = "YES"
            buildForRunning = "YES"
            buildForProfiling = "YES"
            buildForArchiving = "YES"
            buildForAnalyzing = "YES">
            <BuildableReference
               BuildableIdentifier = "primary"
               BlueprintIdentifier = "1D6058900D05DD3D006BFB54"
               BuildableName = "cozmo.app"
               BlueprintName = "Unity-iPhone"
               ReferencedContainer = "container:{project_path}">
            </BuildableReference>
         </BuildActionEntry>
         <BuildActionEntry
            buildForTesting = "YES"
            buildForRunning = "YES"
            buildForProfiling = "NO"
            buildForArchiving = "NO"
            buildForAnalyzing = "YES">
            <BuildableReference
               BuildableIdentifier = "primary"
               BlueprintIdentifier = "5623C57217FDCB0800090B9E"
               BuildableName = "cozmo.octest"
               BlueprintName = "Unity-iPhone Tests"
               ReferencedContainer = "container:{project_path}">
            </BuildableReference>
         </BuildActionEntry>
      </BuildActionEntries>
   </BuildAction>
   <TestAction
      selectedDebuggerIdentifier = "Xcode.DebuggerFoundation.Debugger.LLDB"
      selectedLauncherIdentifier = "Xcode.DebuggerFoundation.Launcher.LLDB"
      shouldUseLaunchSchemeArgsEnv = "YES"
      buildConfiguration = "Debug">
      <Testables>
         <TestableReference
            skipped = "NO">
            <BuildableReference
               BuildableIdentifier = "primary"
               BlueprintIdentifier = "5623C57217FDCB0800090B9E"
               BuildableName = "cozmo.octest"
               BlueprintName = "Unity-iPhone Tests"
               ReferencedContainer = "container:{project_path}">
            </BuildableReference>
         </TestableReference>
      </Testables>
      <MacroExpansion>
         <BuildableReference
            BuildableIdentifier = "primary"
            BlueprintIdentifier = "1D6058900D05DD3D006BFB54"
            BuildableName = "cozmo.app"
            BlueprintName = "Unity-iPhone"
            ReferencedContainer = "container:{project_path}">
         </BuildableReference>
      </MacroExpansion>
   </TestAction>
   <LaunchAction
      selectedDebuggerIdentifier = "Xcode.DebuggerFoundation.Debugger.LLDB"
      selectedLauncherIdentifier = "Xcode.DebuggerFoundation.Launcher.LLDB"
      launchStyle = "0"
      useCustomWorkingDirectory = "NO"
      buildConfiguration = "Debug"
      ignoresPersistentStateOnLaunch = "NO"
      debugDocumentVersioning = "YES"
      allowLocationSimulation = "YES">
      <BuildableProductRunnable>
         <BuildableReference
            BuildableIdentifier = "primary"
            BlueprintIdentifier = "1D6058900D05DD3D006BFB54"
            BuildableName = "cozmo.app"
            BlueprintName = "Unity-iPhone"
            ReferencedContainer = "container:{project_path}">
         </BuildableReference>
      </BuildableProductRunnable>
      <AdditionalOptions>
      </AdditionalOptions>
   </LaunchAction>
   <ProfileAction
      shouldUseLaunchSchemeArgsEnv = "YES"
      savedToolIdentifier = ""
      useCustomWorkingDirectory = "NO"
      buildConfiguration = "Release"
      debugDocumentVersioning = "YES">
      <BuildableProductRunnable>
         <BuildableReference
            BuildableIdentifier = "primary"
            BlueprintIdentifier = "1D6058900D05DD3D006BFB54"
            BuildableName = "cozmo.app"
            BlueprintName = "Unity-iPhone"
            ReferencedContainer = "container:{project_path}">
         </BuildableReference>
      </BuildableProductRunnable>
   </ProfileAction>
   <AnalyzeAction
      buildConfiguration = "Debug">
   </AnalyzeAction>
   <ArchiveAction
      buildConfiguration = "Release"
      revealArchiveInOrganizer = "YES">
   </ArchiveAction>
</Scheme>
'''
