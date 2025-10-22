

## Compiling a header binary for ESP

Inside the root folder of the frontend server run:

```
npx svelteesp32 -e espidf -s ./dist -o ./esp32/svelteesp32.h --etag=true 
```