# Packaging the plugin as a self-contained vst installer

--------------

> **Note:**
A guide can be found [here](https://docs.juce.com/master/tutorial_app_plugin_packaging.html)


## Windows

> **Note:**
> There are two ways to create installers. In the first case, torch dlls are included in the installer. 
> In the second case, only the plugin is installed. In the second case, it assumes the dlls are already available 
> in C:\Windows\System32. A libtorch installer can also be created for cases where you will be preparing multiple plugins
> that use libtorch.

1. Make sure you have `Innosetup` installed. You can download it [here](http://www.jrsoftware.org/isdl.php)
2. Compile the plugin in `Release` mode
3. If you'd like to have presets, create them in the plugin, or move pre-existing ones to the plugin's folder (C:\Program Files\{PluginName})
4. Open the `Windows.iss` script file in the `Packaging` folder
5. Compile the script with `Innosetup`
6. The installer will be created in the `Output` folder

