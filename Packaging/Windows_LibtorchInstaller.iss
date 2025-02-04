[Setup]
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
AppName=LibtorchInstaller
AppVersion=2.0.1
DefaultDirName={cf}
DefaultGroupName=LibtorchInstaller
OutputBaseFilename=LibtorchInstaller-2.0.1



[Files]
Source: "C:\ProgramData\libtorch-2.0.1-Release\lib\asmjit.dll"; DestDir: "{sys}"
Source: "C:\ProgramData\libtorch-2.0.1-Release\lib\c10.dll"; DestDir: "{sys}"
Source: "C:\ProgramData\libtorch-2.0.1-Release\lib\fbgemm.dll"; DestDir: "{sys}"
Source: "C:\ProgramData\libtorch-2.0.1-Release\lib\libiomp5md.dll"; DestDir: "{sys}"
Source: "C:\ProgramData\libtorch-2.0.1-Release\lib\libiompstubs5md.dll"; DestDir: "{sys}"
Source: "C:\ProgramData\libtorch-2.0.1-Release\lib\pytorch_jni.dll"; DestDir: "{sys}"
Source: "C:\ProgramData\libtorch-2.0.1-Release\lib\torch.dll"; DestDir: "{sys}"
Source: "C:\ProgramData\libtorch-2.0.1-Release\lib\torch_cpu.dll"; DestDir: "{sys}"
Source: "C:\ProgramData\libtorch-2.0.1-Release\lib\torch_global_deps.dll"; DestDir: "{sys}"
Source: "C:\ProgramData\libtorch-2.0.1-Release\lib\uv.dll"; DestDir: "{sys}"
