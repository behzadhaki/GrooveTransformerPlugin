
sudo bash install_libtorch.sh


then run cmake


----

Paths:

- Torch:
  - Win: C:\Windows\System32\*.dll copied via install_libtorch.bat script (which extracts and moves the content of torch_win_2.0.1.zip in repo)
  - Linux: /opt/libtorch/libtorch-x.y.z-Debug (or Release) --> .so files should be copied to /usr/lib
  - Mac: /Library/Application Support/libtorch/libtorch-x.y.z-Debug (or Release)
- Resources:
  - Linux: /home/{usrname}/.local/share/{plugin_name}/
  - Mac: ~/Library/Application Support/{plugin_name}
  - Win: C:\ProgramData\{plugin_name}\

To do:
  - Move dylib files to mac lib source
  - Remove the dev torch folder to check it still works
  - rn the plugin can't be automatically moved to VST3 folder (moved manually)

