function Component()
{
    // Default constructor
}

Component.prototype.createOperations = function()
{
    // Create shortcut
    if (installer.value("os") === "win") {

        component.addOperation("CreateShortcut",
                               installer.value("TargetDir") + "/doc/cmake-3.6/cmake.org.html",
                               installer.value("StartMenuDir") + "/CMake Web Site.lnk");

        component.addOperation("CreateShortcut",
                               installer.value("TargetDir") + "/cmake-maintenance.exe",
                               installer.value("StartMenuDir") + "/CMake Maintenance Tool.lnk");
    }

    // Call default implementation
    component.createOperations();
}
