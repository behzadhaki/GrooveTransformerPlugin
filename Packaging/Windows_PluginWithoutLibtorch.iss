[Setup]
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
AppName=GrooveTransformerPlugin
AppVersion=0.0.1
DefaultDirName={cf}
DefaultGroupName=GrooveTransformerPlugin
OutputBaseFilename=GrooveTransformerPlugin-Partial-0.0.1



[Files]
Source: "..\cmake-build-release\PluginCode\GrooveTransformerPlugin_artefacts\Release\VST3\*.*"; DestDir: "C:\Program Files\Common Files\VST3"; Flags: recursesubdirs createallsubdirs
Source: "C:\ProgramData\GrooveTransformerPlugin\*.*"; DestDir: "C:\ProgramData\GrooveTransformerPlugin"; Flags: recursesubdirs createallsubdirs