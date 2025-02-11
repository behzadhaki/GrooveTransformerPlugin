
sudo bash install_libtorch.sh


then run cmake


----

Paths:

- Torch:
  - Win: C:\Windows\System32\*.dll copied via install_libtorch.bat script (which extracts and moves the content of torch_win_2.0.1.zip in repo)
  - Linux: /opt/libtorch/libtorch-x.y.z-Debug (or Release) --> .so files should be copied to /usr/lib
  - Mac: /Library/Application Support/libtorch/libtorch-x.y.z-Debug (or Release) --> for dev!
        - torch .dylib files are copied to Resources folder within VST3 Post-Build
- Resources:
  - Linux: /home/{usrname}/.local/share/{plugin_name}/
  - Mac: ~/Library/Application Support/{plugin_name}
  - Win: C:\ProgramData\{plugin_name}\


----

After build, vst3 is copied to:
- Linux: /usr/lib/vst3/ ???? <== Double check this!
- Mac: ~/Library/Audio/Plug-Ins/VST3/
- Win: C:\Program Files\Common Files\VST3\