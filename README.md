## Purdy plugin
This linux only Geany plugin provides 

* an SVN integrated tree browser on the sidebar. 
* The SVN revision of the files listed in the sidebar.
* SVN revisions are tracked by a separate thread. Responsiveness is top priority.
* SVN decorations are overlayed on the files (modeled after Eclipse's decorators).
* Multi select on the sidebar and activate Meld for diff.
* A local history is tracked after every Save.

Modeled after the original treebrowser and filebackup plugins.

## Build and Install

Build the dynamic link library

```
make
```

Copy the generated .so to Geany's plugin location.

```
make install
``` 
