# geany-plugin-purdy

Putting this here for now

```
gcc -c purdy.c -fPIC `pkg-config --cflags geany`
gcc purdy.o -o purdy.so -shared `pkg-config --libs geany` 
sudo cp purdy.so /usr/lib64/geany/
```
