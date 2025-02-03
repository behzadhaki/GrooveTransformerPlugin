# Packaging the plugin as a self-contained vst installer

--------------

> **Note:**
A guide can be found [here](https://docs.juce.com/master/tutorial_app_plugin_packaging.html)


## Windows

1. Make sure you have `Innosetup` installed. You can download it [here](http://www.jrsoftware.org/isdl.php)
2. Compile the plugin in `Release` mode
3. If you'd like to have presets, create them in the plugin, or move pre-existing ones to the plugin's folder (C:\Program Files\{PluginName})
4. Open the `Windows.iss` script file in the `Packaging` folder
5. Compile the script with `Innosetup`
6. The installer will be created in the `Output` folder