## Purdy plugin
This linux only Geany plugin puts a tree browser on the sidebar. 

## Features 
* The SVN revision of the files listed in the sidebar
* SVN revisions are tracked by a separate thread and updated whenever there is a change.
* SVN decorations are overlayed on the files (modeled after Eclipse's decorators)
* Multi select on the sidebar and activate Meld for diff.
* A local history is tracked after every Save.

## Build and Install

```
gcc -c purdy.c -fPIC `pkg-config --cflags geany`
gcc purdy.o -o purdy.so -shared `pkg-config --libs geany` 
sudo cp purdy.so /usr/lib64/geany/
```
